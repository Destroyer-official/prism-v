#pragma once
#ifndef PRISM_DEDUP_RABIN_HPP
#define PRISM_DEDUP_RABIN_HPP

#include "prism/types.hpp"
#include "prism/bbr.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace prism {

/**
 * @brief Content-Defined Chunk descriptor.
 */
struct CdcChunk {
    uint64_t offset = 0;
    uint32_t length = 0;
    Sha256Digest hash{}; // Content-addressed cryptographic hash
};

/**
 * @brief Chunk Manifest (.pvref) representing a deduplicated video file.
 */
struct ChunkManifest {
    uint64_t original_file_size = 0;
    Sha256Digest original_file_sha256{};
    std::vector<CdcChunk> chunks;
};

/**
 * @brief Rabin Content-Defined Chunking (CDC) & Gallery Deduplication Engine.
 * Average chunk size: 512 KB, Min: 128 KB, Max: 2 MB.
 * Reclaims 20-50% of gallery storage across duplicate videos and burst recordings.
 */
class RabinCdcEngine {
public:
    static constexpr size_t MIN_CHUNK_SIZE = 128 * 1024;  // 128 KB
    static constexpr size_t AVG_CHUNK_SIZE = 512 * 1024;  // 512 KB (mask 0x7FFFF)
    static constexpr size_t MAX_CHUNK_SIZE = 2048 * 1024; // 2 MB
    static constexpr uint64_t RABIN_POLYNOMIAL = 0x0000000000163351ULL;
    static constexpr uint64_t CHUNK_SPLIT_MASK = 0x0007FFFFULL; // 524,287 (avg 512 KB)

    RabinCdcEngine();

    /**
     * @brief Partition a bitstream into content-defined chunks using rolling Rabin fingerprint.
     */
    std::vector<CdcChunk> partition_stream(const uint8_t* data, size_t size);

    /**
     * @brief Ingest a video file into the content-addressed block vault.
     * @param video_path Path to input video file
     * @param vault_blocks_dir Directory where unique chunk blocks are stored
     * @param out_manifest Generated .pvref manifest
     * @return Number of unique new bytes written to vault (savings = orig_size - new_bytes)
     */
    uint64_t ingest_video(const std::string& video_path,
                         const std::string& vault_blocks_dir,
                         ChunkManifest& out_manifest);

    /**
     * @brief Reconstruct original video from .pvref manifest and vault blocks.
     */
    bool restore_video(const ChunkManifest& manifest,
                       const std::string& vault_blocks_dir,
                       const std::string& destination_path);

    /**
     * @brief Convert 32-byte hash to hex string for file naming.
     */
    static std::string hash_to_hex(const Sha256Digest& hash);

private:
    std::array<uint64_t, 256> table_{};
    void init_rabin_table();
};

} // namespace prism

#endif // PRISM_DEDUP_RABIN_HPP
