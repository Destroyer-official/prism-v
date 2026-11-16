#pragma once
#ifndef PRISM_RABIN_MESH_HPP
#define PRISM_RABIN_MESH_HPP

#include "prism/types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

namespace prism {

/**
 * @brief Level 1: 256-bit Perceptual Hash (vPDQ) and 128-D Quantized Neural Descriptor.
 */
struct PerceptualKeyframe {
    uint64_t timestamp_ms = 0;
    uint32_t sample_index = 0;
    std::array<uint8_t, 32> vpdq_hash{}; // 256-bit binary perceptual hash
    uint8_t quality_score = 0;          // Contrast/sharpness confidence (0-100)
    std::vector<int8_t> neural_embedding; // 128-D quantized INT8 SSCD descriptor
};

/**
 * @brief Level 2: Video Group of Pictures (GOP) Descriptor.
 */
struct GopDescriptor {
    uint32_t gop_index = 0;
    uint64_t start_offset = 0;
    uint32_t byte_length = 0;
    Sha256Digest gop_hash{};            // Cryptographic digest of GOP bitstream
    PerceptualKeyframe keyframe{};       // Fingerprint of the leading IDR frame
    bool is_closed_gop = true;
};

/**
 * @brief Level 3: Sub-Block Content-Defined Chunk.
 */
struct MeshChunk {
    uint64_t virtual_offset = 0;
    uint32_t length = 0;
    Sha256Digest blake3_hash{};
    bool is_residual = false;
};

/**
 * @brief Virtual Reconstruction File Graph Manifest (.pvmesh).
 */
struct MeshDeltaManifest {
    static constexpr uint32_t MAGIC = 0x50564D48; // 'PVMH'
    static constexpr uint16_t VERSION = 0x0200;

    uint64_t original_file_size = 0;
    Sha256Digest original_sha256{};
    std::string master_cluster_uuid;
    bool is_master_node = false;

    // Structural container headers (ftyp, moov, etc.)
    std::vector<uint8_t> container_preamble;

    // References to shared chunks in the vault
    std::vector<MeshChunk> chunk_references;

    // Small compressed residual patch stream
    std::vector<uint8_t> residual_stream;
};

/**
 * @brief Frontier 6: Hierarchical Rabin-Mesh Vault & Deduplication Engine.
 * 
 * Implements 3-Level Spatio-Temporal Chunking:
 * - Level 1: Keyframe scene fingerprinting (vPDQ + Neural Embeddings)
 * - Level 2: Inter-video GOP deduplication (burst takes, HDR brackets, social shares)
 * - Level 3: Sub-block byte-exact chunk deduplication (SIMD VectorCDC)
 * - Virtual Reconstruction File Graph: Transparent Delta Manifests with Android VFS proxy.
 */
class RabinMeshEngine {
public:
    static constexpr size_t VECTOR_CDC_MIN_CHUNK = 16 * 1024;   // 16 KiB
    static constexpr size_t VECTOR_CDC_AVG_CHUNK = 64 * 1024;   // 64 KiB
    static constexpr size_t VECTOR_CDC_MAX_CHUNK = 256 * 1024;  // 256 KiB
    static constexpr uint64_t VECTOR_CDC_MASK = 0x0000FFFFULL;  // 65,535 (avg 64 KiB)
    static constexpr uint64_t RABIN_POLYNOMIAL = 0x0000000000163351ULL;

    RabinMeshEngine();
    ~RabinMeshEngine();

    // Level 1: Keyframe Scene Fingerprinting
    static uint32_t compute_hamming_distance(const std::array<uint8_t, 32>& h1,
                                            const std::array<uint8_t, 32>& h2);
    static float compute_cosine_similarity(const std::vector<int8_t>& e1,
                                          const std::vector<int8_t>& e2);
    PerceptualKeyframe fingerprint_frame(const uint8_t* luma_plane,
                                         uint32_t width,
                                         uint32_t height,
                                         uint64_t timestamp_ms);

    // Level 2: Inter-Video GOP Slicing & Burst Detection
    std::vector<GopDescriptor> slice_stream_gops(const uint8_t* data, size_t size);
    bool is_burst_duplicate(const std::vector<PerceptualKeyframe>& clip_a,
                            const std::vector<PerceptualKeyframe>& clip_b,
                            float similarity_threshold = 0.85f);

    // Level 3: SIMD VectorCDC Sub-Block Partitioning
    std::vector<MeshChunk> partition_stream_simd(const uint8_t* data, size_t size);

    // Vault Ingest: Takes input stream, stores blocks in vault, returns new bytes written
    uint64_t ingest_video(const uint8_t* data,
                          size_t size,
                          const std::string& vault_dir,
                          const std::string& cluster_uuid,
                          bool as_master,
                          MeshDeltaManifest& out_manifest);

    // Virtual Reconstruction: Reconstructs exact bytes from manifest + vault
    bool reconstruct_video(const MeshDeltaManifest& manifest,
                           const std::string& vault_dir,
                           std::vector<uint8_t>& out_stream);

    // VFS Proxy: Read arbitrary byte ranges without full-file materialization
    bool read_virtual_range(const MeshDeltaManifest& manifest,
                            const std::string& vault_dir,
                            uint64_t offset,
                            size_t length,
                            uint8_t* destination);

    // Manifest File Persistence (.pvmesh)
    bool save_manifest(const MeshDeltaManifest& manifest, const std::string& manifest_path);
    bool load_manifest(const std::string& manifest_path, MeshDeltaManifest& out_manifest);

    // High-Level File APIs
    uint64_t ingest_video_file(const std::string& input_video_path,
                               const std::string& vault_dir,
                               const std::string& manifest_output_path,
                               const std::string& cluster_uuid,
                               bool as_master);

    bool restore_video_file(const std::string& manifest_path,
                            const std::string& vault_dir,
                            const std::string& destination_path);

    // Master Re-Election & Pruning
    bool elect_new_master(const std::vector<MeshDeltaManifest>& cluster_manifests,
                          size_t deleted_master_idx,
                          MeshDeltaManifest& out_promoted_master);

    static std::string hash_to_hex(const Sha256Digest& hash);

private:
    std::array<uint64_t, 256> rabin_table_{};
    void init_table();
};

} // namespace prism

#endif // PRISM_RABIN_MESH_HPP
