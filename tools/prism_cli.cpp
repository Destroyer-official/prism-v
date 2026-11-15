#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/tile_pool.hpp"
#include "prism/bbr.hpp"
#include "prism/psc.hpp"
#include "prism/coolchic.hpp"
#include "prism/afgs1.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/dedup_rabin.hpp"
#include "prism/transaction.hpp"
#include "prism/remux.hpp"
#include "prism/router.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <chrono>

namespace fs = std::filesystem;
using namespace prism;

void print_banner() {
    std::cout << "===============================================================\n";
    std::cout << " Prism-V command-line interface\n";
    std::cout << " Lossless bitstream, image, and container tooling              \n";
    std::cout << "===============================================================\n\n";
}

void print_usage() {
    std::cout << "Usage: prism <command> [options]\n\n"
              << "Commands:\n"
              << "  compress   <input> <output> [--contract bbr]\n"
              << "  decompress <input.pvloss> <output_file>\n"
              << "  vfs-mount  <file.pvloss>\n"
              << "  dedup      <input_video> <vault_dir> <manifest_out>\n"
              << "  restore    <manifest_in> <vault_dir> <output_video>\n"
              << "  route      <input_file> [--lossless|--lossy]\n"
              << "  remux      <input_mp4> <output_mp4>\n"
              << "  audit      Check integer bounds, page size, tile budget, and CDF LUT\n"
              << "  help       Show this help message\n";
}

int cmd_audit() {
    std::cout << "[AUDIT] Local build and invariant checks\n";
    bool all_ok = true;

    // 1. Integer determinism bound check
    std::vector<int16_t> lhs(512, 32767);
    std::vector<int8_t> rhs(512, 127);
    int32_t dot = DetInt::dot_product(lhs.data(), rhs.data(), 512);
    bool dot_ok = dot >= 0;
    all_ok = all_ok && dot_ok;
    std::cout << "  [1/4] Max dot product: " << dot
              << " (INT32_MAX " << INT32_MAX << ") -> "
              << (dot_ok ? "PASS" : "FAIL") << "\n";

    // 2. Page size check
    size_t ps = DetInt::query_system_page_size();
    bool page_ok = ps >= PAGE_SIZE_16KB;
    all_ok = all_ok && page_ok;
    std::cout << "  [2/4] System page size: " << ps << " bytes -> "
              << (page_ok ? "PASS" : "FAIL (below 16 KB)") << "\n";

    // 3. Configured tile ring budget (not measured RSS)
    size_t tile_mem = MAX_TILE_SLOTS * SpatialTile::PIXEL_COUNT * sizeof(int16_t);
    bool tile_ok = tile_mem <= 64u * 1024u * 1024u;
    all_ok = all_ok && tile_ok;
    std::cout << "  [3/4] Configured tile ring: " << (tile_mem / (1024.0 * 1024.0))
              << " MB (limit 64 MB) -> " << (tile_ok ? "PASS" : "FAIL") << "\n";

    // 4. CDF LUT monotonicity check
    bool lut_ok = true;
    for (size_t i = 1; i < DetInt::CDF_SIGMOID_LUT.size(); ++i) {
        if (DetInt::CDF_SIGMOID_LUT[i] < DetInt::CDF_SIGMOID_LUT[i - 1]) {
            lut_ok = false;
            break;
        }
    }
    lut_ok = lut_ok && DetInt::CDF_SIGMOID_LUT.front() == 0 &&
             DetInt::CDF_SIGMOID_LUT.back() == 32768;
    all_ok = all_ok && lut_ok;
    std::cout << "  [4/4] Sigmoid CDF LUT monotonic on [0, 32768] -> "
              << (lut_ok ? "PASS" : "FAIL") << "\n";

    std::cout << "[AUDIT] Result: " << (all_ok ? "all checks passed" : "one or more checks failed") << "\n";
    return all_ok ? 0 : 1;
}

int cmd_compress(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string input_path = argv[2];
    std::string output_path = argv[3];
    std::string contract_str = "bbr";

    for (int i = 4; i < argc; ++i) {
        if (std::string(argv[i]) == "--contract" && i + 1 < argc) {
            contract_str = argv[++i];
        }
    }

    if (!fs::exists(input_path)) {
        std::cerr << "Error: Input file does not exist: " << input_path << "\n";
        return 1;
    }
    if (!fs::is_regular_file(input_path)) {
        std::cerr << "Error: Input path is not a regular file: " << input_path << "\n";
        return 1;
    }
    if (contract_str != "bbr") {
        std::cerr << "Error: Unsupported contract '" << contract_str
                  << "'. This build implements only 'bbr' for file compression.\n";
        return 1;
    }

    std::cout << "[COMPRESS] Ingesting: " << input_path << "\n";
    std::cout << "[COMPRESS] Selected Contract: " << contract_str << "\n";

    std::ifstream in(input_path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Failed to open input file: " << input_path << "\n";
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();

    Sha256Digest orig_sha = BbrEngine::compute_sha256(data.data(), data.size());
    std::cout << "  Original Size: " << data.size() << " bytes\n";

    BbrEngine engine;
    PvlossWriter writer;

    PvlossHeader hdr;
    hdr.lossless_mode = LosslessContract::CONTRACT_A_BBR;
    hdr.source_codec_id = SourceCodecId::CODEC_H264;
    hdr.orig_file_size = data.size();
    hdr.orig_sha256 = orig_sha;

    writer.open(output_path, hdr);

    // Partition into 256 KB virtual chunks
    constexpr size_t chunk_size = 256 * 1024;
    for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
        size_t len = std::min(chunk_size, data.size() - offset);
        BbrChunk bchunk = engine.compress(data.data() + offset, len, SourceCodecId::CODEC_H264);
        writer.write_chunk(bchunk.rans_payload.data(),
                           static_cast<uint32_t>(bchunk.rans_payload.size()),
                           static_cast<uint32_t>(len),
                           offset);
    }
    writer.close();

    uintmax_t out_size = fs::file_size(output_path);
    if (data.empty()) {
        std::cout << "  Output .pvloss: " << out_size << " bytes (empty input)\n";
    } else {
        double ratio = (1.0 - (static_cast<double>(out_size) / static_cast<double>(data.size()))) * 100.0;
        std::cout << "  Output .pvloss: " << out_size << " bytes (Saved: "
                  << std::fixed << std::setprecision(2) << ratio << "%)\n";
    }

    return 0;
}

int cmd_decompress(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: prism decompress <input.pvloss> <output_file>\n";
        return 1;
    }
    std::string in_path = argv[2];
    std::string out_path = argv[3];

    if (!fs::exists(in_path)) {
        std::cerr << "Error: File not found: " << in_path << "\n";
        return 1;
    }

    PvlossReader reader;
    if (!reader.open(in_path)) {
        std::cerr << "Error: Failed to open .pvloss container: " << in_path << "\n";
        return 1;
    }

    const auto& hdr = reader.header();
    const char* mode_name = "Unknown";
    if (hdr.lossless_mode == LosslessContract::CONTRACT_A_BBR) mode_name = "Contract A (L-BBR)";
    else if (hdr.lossless_mode == LosslessContract::CONTRACT_C_PSC) mode_name = "Contract C (L-PSC)";
    std::cout << "[DECOMPRESS] Ingesting .pvloss container: " << in_path << "\n"
              << "  Lossless Mode: " << mode_name << "\n"
              << "  Source Codec: " << static_cast<int>(hdr.source_codec_id) << "\n"
              << "  Target Virtual Size: " << hdr.orig_file_size << " bytes\n"
              << "  Chunks: " << reader.index().size() << "\n";

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to create output file: " << out_path << "\n";
        return 1;
    }

    BbrEngine bbr;
    std::vector<uint8_t> all_restored;
    if (hdr.orig_file_size <= std::numeric_limits<size_t>::max()) {
        all_restored.reserve(static_cast<size_t>(hdr.orig_file_size));
    }

    for (const auto& entry : reader.index()) {
        std::vector<uint8_t> payload = reader.read_chunk_payload(entry);
        BbrChunk chunk;
        chunk.uncompressed_size = entry.uncompressed_size;
        chunk.rans_payload = std::move(payload);

        std::vector<uint8_t> decomp = bbr.decompress(chunk);
        all_restored.insert(all_restored.end(), decomp.begin(), decomp.end());
    }

    out.write(reinterpret_cast<const char*>(all_restored.data()), all_restored.size());
    out.close();
    if (!out) {
        std::cerr << "Error: Failed to write output file: " << out_path << "\n";
        return 1;
    }

    Sha256Digest restored_sha = BbrEngine::compute_sha256(all_restored.data(), all_restored.size());
    bool bit_exact = (restored_sha == hdr.orig_sha256) &&
                     (all_restored.size() == hdr.orig_file_size);

    std::cout << "  Restored Size: " << all_restored.size() << " bytes\n"
              << "  Expected SHA-256: " << RabinCdcEngine::hash_to_hex(hdr.orig_sha256) << "\n"
              << "  Restored SHA-256: " << RabinCdcEngine::hash_to_hex(restored_sha) << "\n"
              << "  SHA-256 match: " << (bit_exact ? "YES" : "NO") << "\n";

    return bit_exact ? 0 : 1;
}

int cmd_vfs_mount(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: prism vfs-mount <file.pvloss>\n";
        return 1;
    }
    std::string in_path = argv[2];
    if (!fs::exists(in_path)) {
        std::cerr << "Error: File not found: " << in_path << "\n";
        return 1;
    }

    PrismVfsProxy vfs;
    if (!vfs.mount(in_path)) {
        std::cerr << "Error: Failed to mount .pvloss in Prism-VFS proxy.\n";
        return 1;
    }

    uint64_t virtual_size = vfs.on_get_size();
    std::cout << "[VFS] Mounted .pvloss in the Prism-VFS proxy:\n"
              << "  File: " << in_path << "\n"
              << "  Virtual MP4 Size: " << virtual_size << " bytes ("
              << (virtual_size / (1024.0 * 1024.0)) << " MB)\n"
              << "  Running 100 random 64 KB reads...\n";

    std::vector<uint8_t> buf(64 * 1024);
    auto t_start = std::chrono::high_resolution_clock::now();
    constexpr size_t iterations = 100;
    size_t completed_reads = 0;
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t range = (virtual_size > buf.size()) ? (virtual_size - buf.size()) : 1;
        uint64_t offset = (i * 262147ULL) % range;
        if (vfs.on_read(offset, static_cast<uint32_t>(buf.size()), buf.data()) > 0) {
            ++completed_reads;
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
    double avg_latency_us = total_us / static_cast<double>(iterations);

    auto stats = vfs.get_stats();
    uint64_t total_ops = stats.cache_hits + stats.cache_misses;
    double hit_pct = (total_ops > 0) ? (static_cast<double>(stats.cache_hits) / total_ops * 100.0) : 0.0;
    std::cout << "  Completed Reads: " << completed_reads << "/" << iterations << "\n"
              << "  Mean read/seek latency: " << std::fixed << std::setprecision(2)
              << avg_latency_us << " us\n"
              << "  Cache Hits: " << stats.cache_hits << ", Misses: " << stats.cache_misses
              << " (Hit Ratio: " << std::fixed << std::setprecision(1) << hit_pct << "%)\n";

    vfs.unmount();
    return 0;
}

int cmd_dedup(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: prism dedup <input_video> <vault_dir> <manifest_out>\n";
        return 1;
    }
    std::string in_path = argv[2];
    std::string vault_dir = argv[3];
    std::string manifest_out = argv[4];

    if (!fs::exists(in_path) || !fs::is_regular_file(in_path)) {
        std::cerr << "Error: Input file not found: " << in_path << "\n";
        return 1;
    }

    RabinCdcEngine engine;
    ChunkManifest manifest;
    uint64_t new_bytes = engine.ingest_video(in_path, vault_dir, manifest);

    std::ofstream out(manifest_out, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to create manifest: " << manifest_out << "\n";
        return 1;
    }

    const char magic[8] = "PVREF01";
    out.write(magic, 7);
    out.write(reinterpret_cast<const char*>(&manifest.original_file_size), sizeof(uint64_t));
    out.write(reinterpret_cast<const char*>(manifest.original_file_sha256.data()), 32);
    uint32_t count = static_cast<uint32_t>(manifest.chunks.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
    for (const auto& chk : manifest.chunks) {
        out.write(reinterpret_cast<const char*>(&chk.offset), sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(&chk.length), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(chk.hash.data()), 32);
    }
    out.close();

    uint64_t orig_size = manifest.original_file_size;
    uint64_t saved_bytes = orig_size > new_bytes ? (orig_size - new_bytes) : 0;
    double saved_pct = orig_size > 0 ? (static_cast<double>(saved_bytes) / orig_size) * 100.0 : 0.0;

    std::cout << "[DEDUP] Content-Defined Chunking Ingestion Complete:\n"
              << "  Original Size: " << orig_size << " bytes\n"
              << "  Total Chunks: " << manifest.chunks.size() << "\n"
              << "  New Vault Storage: " << new_bytes << " bytes\n"
              << "  Reclaimed Duplicate Storage: " << saved_bytes << " bytes ("
              << std::fixed << std::setprecision(1) << saved_pct << "%)\n"
              << "  Manifest: " << manifest_out << "\n";
    return 0;
}

int cmd_restore(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: prism restore <manifest_in> <vault_dir> <output_video>\n";
        return 1;
    }
    std::string manifest_in = argv[2];
    std::string vault_dir = argv[3];
    std::string out_video = argv[4];

    if (!fs::exists(manifest_in)) {
        std::cerr << "Error: Manifest not found: " << manifest_in << "\n";
        return 1;
    }
    if (!fs::exists(vault_dir) || !fs::is_directory(vault_dir)) {
        std::cerr << "Error: Vault directory not found: " << vault_dir << "\n";
        return 1;
    }

    std::ifstream in(manifest_in, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        std::cerr << "Error: Failed to open manifest: " << manifest_in << "\n";
        return 1;
    }
    std::streamoff manifest_size = in.tellg();
    if (manifest_size < 51) {
        std::cerr << "Error: Manifest is too small to be valid.\n";
        return 1;
    }
    in.seekg(0, std::ios::beg);

    char magic[8] = {0};
    in.read(magic, 7);
    if (std::string(magic, 7) != "PVREF01") {
        std::cerr << "Error: Invalid manifest format (magic mismatch).\n";
        return 1;
    }

    ChunkManifest manifest;
    in.read(reinterpret_cast<char*>(&manifest.original_file_size), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(manifest.original_file_sha256.data()), 32);
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    if (!in) {
        std::cerr << "Error: Manifest header is truncated.\n";
        return 1;
    }

    constexpr uint64_t kManifestHeader = 7 + 8 + 32 + 4;
    constexpr uint64_t kChunkRecord = 8 + 4 + 32;
    uint64_t available = static_cast<uint64_t>(manifest_size) - kManifestHeader;
    if (static_cast<uint64_t>(count) > available / kChunkRecord) {
        std::cerr << "Error: Manifest chunk count exceeds the file size.\n";
        return 1;
    }

    manifest.chunks.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        in.read(reinterpret_cast<char*>(&manifest.chunks[i].offset), sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(&manifest.chunks[i].length), sizeof(uint32_t));
        in.read(reinterpret_cast<char*>(manifest.chunks[i].hash.data()), 32);
    }
    if (!in) {
        std::cerr << "Error: Manifest chunk records are truncated.\n";
        return 1;
    }
    in.close();

    RabinCdcEngine engine;
    if (!engine.restore_video(manifest, vault_dir, out_video)) {
        std::cerr << "Error: Failed to restore video from vault blocks.\n";
        return 1;
    }

    std::ifstream out_file(out_video, std::ios::binary);
    std::vector<uint8_t> restored_data((std::istreambuf_iterator<char>(out_file)),
                                       std::istreambuf_iterator<char>());
    out_file.close();

    Sha256Digest restored_sha = BbrEngine::compute_sha256(restored_data.data(), restored_data.size());
    bool bit_exact = (restored_sha == manifest.original_file_sha256) &&
                     (restored_data.size() == manifest.original_file_size);

    std::cout << "[RESTORE] Chunk reconstruction complete:\n"
              << "  Restored File: " << out_video << " (" << restored_data.size() << " bytes)\n"
              << "  Expected SHA-256: " << RabinCdcEngine::hash_to_hex(manifest.original_file_sha256) << "\n"
              << "  Restored SHA-256: " << RabinCdcEngine::hash_to_hex(restored_sha) << "\n"
              << "  SHA-256 match: " << (bit_exact ? "YES" : "NO") << "\n";

    return bit_exact ? 0 : 1;
}

int cmd_route(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: prism route <input_file> [--lossless|--lossy]\n";
        return 1;
    }
    std::string input_path = argv[2];
    bool lossless = true;
    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--lossy") lossless = false;
        if (std::string(argv[i]) == "--lossless") lossless = true;
    }

    if (!fs::exists(input_path)) {
        std::cerr << "Error: File not found: " << input_path << "\n";
        return 1;
    }

    uint64_t file_size = fs::file_size(input_path);
    std::ifstream in(input_path, std::ios::binary);
    std::vector<uint8_t> header(std::min<size_t>(4096, static_cast<size_t>(file_size)));
    in.read(reinterpret_cast<char*>(header.data()), header.size());
    in.close();

    IngestMediaInfo info = IngestRouter::probe_media(header.data(), header.size(), file_size);
    RoutingDecision dec = IngestRouter::route_media(info, lossless);

    std::cout << "[ROUTE] Ingest Probe Results:\n"
              << "  File: " << input_path << " (" << file_size << " bytes)\n"
              << "  Container Standard: " << (info.is_container_standard ? "YES (ISOBMFF)" : "NO / RAW") << "\n"
              << "  Est. BBR Savings: " << (info.estimated_bbr_savings_ratio * 100.0) << "%\n"
              << "  Recommended Path: " << dec.mode_name << "\n"
              << "  Rationale: " << dec.rationale << "\n";
    return 0;
}

int cmd_remux(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: prism remux <input_mp4> <output_mp4>\n";
        return 1;
    }
    std::string in_path = argv[2];
    std::string out_path = argv[3];

    if (!fs::exists(in_path)) {
        std::cerr << "Error: File not found: " << in_path << "\n";
        return 1;
    }

    std::ifstream in(in_path, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();

    RemuxEngine::RemuxPackage pkg;
    if (!RemuxEngine::parse_and_verify_isobmff(data.data(), data.size(), pkg)) {
        std::cerr << "Error: Failed to parse input ISOBMFF.\n";
        return 1;
    }

    std::vector<uint8_t> remuxed = RemuxEngine::remux_to_clean_isobmff(pkg);
    std::ofstream out(out_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(remuxed.data()), remuxed.size());
    out.close();

    std::cout << "[REMUX] Mode L-Remux (Contract B) Complete:\n"
              << "  Preserved Packets: " << pkg.packets.size() << " (Video: "
              << pkg.video_packet_count << ", Audio: " << pkg.audio_packet_count << ")\n"
              << "  Output: " << out_path << " (" << remuxed.size() << " bytes)\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (!std::cout) return 1;

    std::string cmd = (argc > 1) ? argv[1] : "";
    if (cmd == "help" || cmd == "--help" || cmd == "-h" || cmd.empty()) {
        print_banner();
        print_usage();
        return cmd.empty() ? 1 : 0;
    }

    try {
        if (cmd == "audit") {
            print_banner();
            return cmd_audit();
        } else if (cmd == "compress") {
            print_banner();
            return cmd_compress(argc, argv);
        } else if (cmd == "decompress") {
            print_banner();
            return cmd_decompress(argc, argv);
        } else if (cmd == "vfs-mount") {
            print_banner();
            return cmd_vfs_mount(argc, argv);
        } else if (cmd == "dedup") {
            print_banner();
            return cmd_dedup(argc, argv);
        } else if (cmd == "restore") {
            print_banner();
            return cmd_restore(argc, argv);
        } else if (cmd == "route") {
            print_banner();
            return cmd_route(argc, argv);
        } else if (cmd == "remux") {
            print_banner();
            return cmd_remux(argc, argv);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Error: unknown failure\n";
        return 1;
    }

    std::cerr << "Error: Unknown command '" << cmd << "'.\n\n";
    print_usage();
    return 1;
}
