#include "prism/types.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/bbr.hpp"
#include "prism/dedup_rabin.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <limits>
#include <algorithm>

using namespace prism;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: verify_real_video <original_mp4> <pvloss_file>\n";
        return 1;
    }

    std::string orig_path = argv[1];
    std::string pvloss_path = argv[2];

    std::cout << "===============================================================\n";
    std::cout << "  Pvloss round-trip verification against an original file     \n";
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
    std::cout << "[ORIGINAL] SHA-256: " << RabinCdcEngine::hash_to_hex(orig_sha) << "\n\n";

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

    if (exact_match) {
        std::cout << "[SUCCESS] Reconstructed file matches the original byte for byte.\n";
        return 0;
    } else {
        std::cerr << "[FAILURE] SHA-256 hash mismatch!\n";
        return 1;
    }
}
