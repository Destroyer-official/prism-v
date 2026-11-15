#include "prism/dedup_rabin.hpp"
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace prism {

RabinCdcEngine::RabinCdcEngine() {
    init_rabin_table();
}

void RabinCdcEngine::init_rabin_table() {
    for (uint64_t b = 0; b < 256; ++b) {
        uint64_t fp = b;
        for (int i = 0; i < 8; ++i) {
            if (fp & 1) {
                fp = (fp >> 1) ^ RABIN_POLYNOMIAL;
            } else {
                fp >>= 1;
            }
        }
        table_[b] = fp;
    }
}

std::string RabinCdcEngine::hash_to_hex(const Sha256Digest& hash) {
    std::ostringstream ss;
    for (uint8_t byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<CdcChunk> RabinCdcEngine::partition_stream(const uint8_t* data, size_t size) {
    std::vector<CdcChunk> chunks;
    if (size == 0) return chunks;

    uint64_t chunk_start = 0;
    uint64_t rolling_fp = 0;

    for (uint64_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        rolling_fp = (rolling_fp >> 8) ^ table_[(rolling_fp ^ byte) & 0xFF];

        uint64_t current_chunk_len = (i - chunk_start) + 1;

        bool boundary_detected = ((rolling_fp & CHUNK_SPLIT_MASK) == 0 &&
                                  current_chunk_len >= MIN_CHUNK_SIZE);
        bool max_size_reached = (current_chunk_len >= MAX_CHUNK_SIZE);
        bool end_of_stream = (i + 1 == size);

        if (boundary_detected || max_size_reached || end_of_stream) {
            CdcChunk chunk;
            chunk.offset = chunk_start;
            chunk.length = static_cast<uint32_t>(current_chunk_len);
            chunk.hash = BbrEngine::compute_sha256(data + chunk_start, current_chunk_len);
            chunks.push_back(chunk);

            chunk_start = i + 1;
            rolling_fp = 0;
        }
    }

    return chunks;
}

uint64_t RabinCdcEngine::ingest_video(const std::string& video_path,
                                      const std::string& vault_blocks_dir,
                                      ChunkManifest& out_manifest) {
    std::ifstream in(video_path, std::ios::binary);
    if (!in.is_open()) return 0;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();

    out_manifest.original_file_size = data.size();
    out_manifest.original_file_sha256 = BbrEngine::compute_sha256(data.data(), data.size());
    out_manifest.chunks = partition_stream(data.data(), data.size());

    fs::create_directories(vault_blocks_dir);
    uint64_t new_bytes_written = 0;

    for (const auto& chunk : out_manifest.chunks) {
        std::string hex_hash = hash_to_hex(chunk.hash);
        fs::path block_path = fs::path(vault_blocks_dir) / (hex_hash + ".blk");

        if (!fs::exists(block_path)) {
            // Write unique chunk to block store
            std::ofstream out(block_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data() + chunk.offset), chunk.length);
            new_bytes_written += chunk.length;
        }
    }

    return new_bytes_written;
}

bool RabinCdcEngine::restore_video(const ChunkManifest& manifest,
                                  const std::string& vault_blocks_dir,
                                  const std::string& destination_path) {
    std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    for (const auto& chunk : manifest.chunks) {
        std::string hex_hash = hash_to_hex(chunk.hash);
        fs::path block_path = fs::path(vault_blocks_dir) / (hex_hash + ".blk");

        if (!fs::exists(block_path)) {
            out.close();
            fs::remove(destination_path);
            return false;
        }

        std::ifstream block_in(block_path, std::ios::binary);
        std::vector<uint8_t> block_data((std::istreambuf_iterator<char>(block_in)),
                                        std::istreambuf_iterator<char>());
        out.write(reinterpret_cast<const char*>(block_data.data()), block_data.size());
    }

    out.close();
    return true;
}

} // namespace prism
