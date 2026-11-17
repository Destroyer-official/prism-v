#include "prism/types.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/bbr.hpp"
#include "prism/bbr2.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/dedup_rabin.hpp"
#include "prism/isobmff.hpp"
#include "prism/remux.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <filesystem>

using namespace prism;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: verify_real_video <original_mp4> [pvloss_file]\n";
        return 1;
    }

    std::string orig_path = argv[1];
    std::string pvloss_path = (argc >= 3) ? argv[2] : (orig_path + ".bbr2.pvloss");
    bool auto_generated = (argc < 3);

    std::cout << "===============================================================\n";
    std::cout << "  PRISM-V REAL VIDEO BBR 2.0 BIT-EXACT VERIFICATION          \n";
    std::cout << "===============================================================\n\n";

    // 1. Read original file
    std::ifstream orig_in(orig_path, std::ios::binary);
    if (!orig_in.is_open()) {
        std::cerr << "Cannot open original file: " << orig_path << "\n";
        return 1;
    }
    std::vector<uint8_t> orig_bytes((std::istreambuf_iterator<char>(orig_in)),
                                    std::istreambuf_iterator<char>());
    orig_in.close();

    Sha256Digest orig_sha = BbrEngine::compute_sha256(orig_bytes.data(), orig_bytes.size());
    std::cout << "[ORIGINAL] File: " << orig_path << " | Size: " << orig_bytes.size() << " bytes\n";
    std::cout << "[ORIGINAL] SHA-256: " << RabinCdcEngine::hash_to_hex(orig_sha) << "\n";

    // Native Container & Ground-Truth Duration Probe
    VideoMetadata meta;
    if (NativeContainerParser::parse_file(orig_path, meta)) {
        std::cout << "[NATIVE PROBE] Container: " << meta.container_format
                  << " | Ground-Truth Duration: " << std::fixed << std::setprecision(2)
                  << (meta.duration_us / 1000000.0) << "s (" << (meta.duration_us / 60000000.0) << " min)"
                  << " | Bitrate: " << (meta.exact_bitrate_bps / 1000.0) << " kbps"
                  << " | Dimensions: " << meta.width << "x" << meta.height
                  << " | FPS: " << meta.fps << " | Codec: " << static_cast<int>(meta.video_codec)
                  << " | Keyframes: " << meta.keyframe_pts_us.size() << "\n";
    }
    std::cout << "\n";

    if (auto_generated) {
        // Auto-compress with BBR 2.0
        std::cout << "[BBR 2.0] Compressing " << orig_bytes.size() << " bytes with Frontier 1 (SSG-CR4 + C-MAVQ + SIMD-16)...\n";
        Bbr2Engine bbr2;
        PvlossWriter writer;

        PvlossHeader hdr;
        hdr.lossless_mode = LosslessContract::CONTRACT_A_BBR2;
        hdr.source_codec_id = SourceCodecId::CODEC_H264;
        hdr.orig_file_size = orig_bytes.size();
        hdr.orig_sha256 = orig_sha;

        writer.open(pvloss_path, hdr);

        constexpr size_t CHUNK_SIZE = 256 * 1024;
        size_t offset = 0;
        uint64_t total_comp_bytes = 0;

        while (offset < orig_bytes.size()) {
            size_t len = std::min(CHUNK_SIZE, orig_bytes.size() - offset);
            Bbr2Chunk chunk = bbr2.compress(orig_bytes.data() + offset, len, SourceCodecId::CODEC_H264);
            std::vector<uint8_t> payload = Bbr2Engine::serialize_chunk_payload(chunk);
            writer.write_chunk(payload.data(), static_cast<uint32_t>(payload.size()),
                               static_cast<uint32_t>(len), offset);
            total_comp_bytes += payload.size();
            offset += len;
        }
        writer.close();

        double savings = 100.0 * (1.0 - static_cast<double>(total_comp_bytes) / orig_bytes.size());
        std::cout << "[BBR 2.0] Compressed size: " << total_comp_bytes << " bytes ("
                  << std::fixed << std::setprecision(2) << savings << "% savings!)\n\n";
    }

    // 2. Mount virtual MP4 via Prism-VFS
    PrismVfsProxy vfs;
    if (!vfs.mount(pvloss_path)) {
        std::cerr << "Failed to mount .pvloss file: " << pvloss_path << "\n";
        return 1;
    }

    uint64_t virtual_size = vfs.on_get_size();
    std::cout << "[VFS] Mounted: " << pvloss_path << "\n";
    std::cout << "[VFS] Virtual MP4 Size reported: " << virtual_size << " bytes\n";

    if (virtual_size != orig_bytes.size()) {
        std::cerr << "ERROR: Virtual size mismatch!\n";
        return 1;
    }
    if (virtual_size > std::numeric_limits<size_t>::max()) {
        std::cerr << "ERROR: Virtual size exceeds the addressable buffer size.\n";
        return 1;
    }

    // 3. Read entire file through VFS proxy in 256 KB increments
    std::vector<uint8_t> reconstructed(static_cast<size_t>(virtual_size));
    constexpr uint32_t READ_BLOCK = 256 * 1024;
    uint64_t bytes_read_total = 0;

    while (bytes_read_total < virtual_size) {
        uint32_t want = static_cast<uint32_t>(std::min<uint64_t>(READ_BLOCK, virtual_size - bytes_read_total));
        int32_t got = vfs.on_read(bytes_read_total, want, reconstructed.data() + bytes_read_total);
        if (got <= 0) break;
        bytes_read_total += got;
    }

    std::cout << "[VFS] Total bytes read through RAM proxy: " << bytes_read_total << " bytes\n";
    auto stats = vfs.get_stats();
    std::cout << "[VFS] Cache Hits: " << stats.cache_hits << " | Misses: " << stats.cache_misses
              << " | Avg Read Latency: " << std::fixed << std::setprecision(2)
              << stats.avg_read_latency_us << " us\n\n";

    if (bytes_read_total != virtual_size) {
        std::cerr << "[FAILURE] VFS returned fewer bytes than the virtual file size.\n";
        return 1;
    }

    // 4. Compute reconstructed SHA-256
    Sha256Digest recon_sha = BbrEngine::compute_sha256(reconstructed.data(), reconstructed.size());
    std::cout << "[RECONSTRUCTED] SHA-256: " << RabinCdcEngine::hash_to_hex(recon_sha) << "\n\n";

    bool exact_match = (orig_sha == recon_sha);
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "SHA-256 parity: " << (exact_match ? "MATCH" : "MISMATCH") << "\n";
    std::cout << "---------------------------------------------------------------\n";

    vfs.unmount();
    if (auto_generated) {
        std::filesystem::remove(pvloss_path);
    }

    if (exact_match) {
        std::cout << "[SUCCESS] Reconstructed file matches the original byte for byte.\n";
        return 0;
    } else {
        std::cerr << "[FAILURE] SHA-256 hash mismatch!\n";
        return 1;
    }
}
