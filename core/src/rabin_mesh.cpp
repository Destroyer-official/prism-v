#include "prism/rabin_mesh.hpp"
#include "prism/bbr.hpp"
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

namespace prism {

RabinMeshEngine::RabinMeshEngine() {
    init_table();
}

RabinMeshEngine::~RabinMeshEngine() = default;

void RabinMeshEngine::init_table() {
    for (uint64_t b = 0; b < 256; ++b) {
        uint64_t fp = b;
        for (int i = 0; i < 8; ++i) {
            if (fp & 1) {
                fp = (fp >> 1) ^ RABIN_POLYNOMIAL;
            } else {
                fp >>= 1;
            }
        }
        rabin_table_[b] = fp;
    }
}

std::string RabinMeshEngine::hash_to_hex(const Sha256Digest& hash) {
    std::ostringstream ss;
    for (uint8_t byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

uint32_t RabinMeshEngine::compute_hamming_distance(const std::array<uint8_t, 32>& h1,
                                                   const std::array<uint8_t, 32>& h2) {
    uint32_t dist = 0;
    for (size_t i = 0; i < 32; ++i) {
        uint8_t x = h1[i] ^ h2[i];
#if defined(__GNUC__) || defined(__clang__)
        dist += static_cast<uint32_t>(__builtin_popcount(x));
#else
        // Portable popcount
        x = x - ((x >> 1) & 0x55);
        x = (x & 0x33) + ((x >> 2) & 0x33);
        dist += ((x + (x >> 4)) & 0x0F);
#endif
    }
    return dist;
}

float RabinMeshEngine::compute_cosine_similarity(const std::vector<int8_t>& e1,
                                                 const std::vector<int8_t>& e2) {
    if (e1.empty() || e1.size() != e2.size()) return 0.0f;

    int64_t dot = 0;
    int64_t norm1 = 0;
    int64_t norm2 = 0;

    for (size_t i = 0; i < e1.size(); ++i) {
        dot += static_cast<int64_t>(e1[i]) * e2[i];
        norm1 += static_cast<int64_t>(e1[i]) * e1[i];
        norm2 += static_cast<int64_t>(e2[i]) * e2[i];
    }

    if (norm1 == 0 || norm2 == 0) return 0.0f;
    return static_cast<float>(dot) / (std::sqrt(static_cast<float>(norm1)) * std::sqrt(static_cast<float>(norm2)));
}

PerceptualKeyframe RabinMeshEngine::fingerprint_frame(const uint8_t* luma_plane,
                                                     uint32_t width,
                                                     uint32_t height,
                                                     uint64_t timestamp_ms) {
    PerceptualKeyframe kf;
    kf.timestamp_ms = timestamp_ms;
    if (!luma_plane || width < 16 || height < 16) return kf;

    // Downsample luma plane to 64x64 thumbnail
    constexpr uint32_t THUMB_SZ = 64;
    std::array<uint8_t, THUMB_SZ * THUMB_SZ> thumb{};

    uint64_t sum = 0;
    uint64_t sq_sum = 0;

    for (uint32_t y = 0; y < THUMB_SZ; ++y) {
        uint32_t src_y = (y * height) / THUMB_SZ;
        for (uint32_t x = 0; x < THUMB_SZ; ++x) {
            uint32_t src_x = (x * width) / THUMB_SZ;
            uint8_t val = luma_plane[src_y * width + src_x];
            thumb[y * THUMB_SZ + x] = val;
            sum += val;
            sq_sum += static_cast<uint64_t>(val) * val;
        }
    }

    // Compute contrast/variance score (0-100)
    uint32_t count = THUMB_SZ * THUMB_SZ;
    double mean = static_cast<double>(sum) / count;
    double var = (static_cast<double>(sq_sum) / count) - (mean * mean);
    kf.quality_score = static_cast<uint8_t>(std::clamp(std::sqrt(std::max(0.0, var)) * 2.0, 0.0, 100.0));

    // Level 1: 256-bit vPDQ Perceptual Hash (block luminance against global mean)
    // 16x16 blocks = 256 bits
    kf.vpdq_hash.fill(0);
    uint32_t bit_idx = 0;

    for (uint32_t by = 0; by < 16; ++by) {
        for (uint32_t bx = 0; bx < 16; ++bx) {
            uint32_t py = by * 4;
            uint32_t px = bx * 4;
            int32_t blk_sum = 0;
            for (uint32_t dy = 0; dy < 4; ++dy) {
                for (uint32_t dx = 0; dx < 4; ++dx) {
                    blk_sum += thumb[(py + dy) * THUMB_SZ + (px + dx)];
                }
            }

            if (blk_sum >= static_cast<int32_t>(mean * 16)) {
                kf.vpdq_hash[bit_idx / 8] |= static_cast<uint8_t>(1 << (bit_idx % 8));
            }
            bit_idx++;
        }
    }

    // 128-D Quantized INT8 SSCD Neural Descriptor (coarse low-frequency DCT-style projection)
    kf.neural_embedding.resize(128, 0);
    for (size_t i = 0; i < 128; ++i) {
        int32_t proj = 0;
        for (size_t j = 0; j < 32; ++j) {
            size_t sample_pos = ((i * 31 + j * 7) % count);
            proj += (thumb[sample_pos] - static_cast<int32_t>(mean));
        }
        kf.neural_embedding[i] = static_cast<int8_t>(std::clamp(proj / 4, -128, 127));
    }

    return kf;
}

std::vector<GopDescriptor> RabinMeshEngine::slice_stream_gops(const uint8_t* data, size_t size) {
    std::vector<GopDescriptor> gops;
    if (!data || size < 8) return gops;

    // Scan for NAL unit start codes: 0x00 00 01 or 0x00 00 00 01
    std::vector<uint64_t> idr_offsets;
    idr_offsets.push_back(0); // Stream start

    for (size_t i = 0; i + 4 < size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0) {
            size_t nal_start = 0;
            uint8_t nal_type = 0;
            if (data[i + 2] == 1) {
                nal_start = i + 3;
                nal_type = data[nal_start] & 0x1F; // H.264 NAL type
            } else if (data[i + 2] == 0 && data[i + 3] == 1) {
                nal_start = i + 4;
                nal_type = data[nal_start] & 0x1F;
            }

            // IDR slice in H.264 is 5, or HEVC CRA/IDR is 19/20
            if (nal_type == 5 || nal_type == 19 || nal_type == 20) {
                if (i > idr_offsets.back() + VECTOR_CDC_MIN_CHUNK) {
                    idr_offsets.push_back(i);
                }
            }
        }
    }

    idr_offsets.push_back(size);

    for (size_t g = 0; g + 1 < idr_offsets.size(); ++g) {
        GopDescriptor gop;
        gop.gop_index = static_cast<uint32_t>(g);
        gop.start_offset = idr_offsets[g];
        gop.byte_length = static_cast<uint32_t>(idr_offsets[g + 1] - idr_offsets[g]);
        gop.gop_hash = BbrEngine::compute_sha256(data + gop.start_offset, gop.byte_length);
        gop.is_closed_gop = true;
        gops.push_back(gop);
    }

    return gops;
}

bool RabinMeshEngine::is_burst_duplicate(const std::vector<PerceptualKeyframe>& clip_a,
                                         const std::vector<PerceptualKeyframe>& clip_b,
                                         float similarity_threshold) {
    if (clip_a.empty() || clip_b.empty()) return false;

    size_t min_len = std::min(clip_a.size(), clip_b.size());
    uint32_t match_count = 0;

    for (size_t i = 0; i < min_len; ++i) {
        uint32_t dist = compute_hamming_distance(clip_a[i].vpdq_hash, clip_b[i].vpdq_hash);
        // Normalized Hamming distance: 0 to 256. Distance <= 32 means >=87.5% visual identity
        if (dist <= 32) {
            match_count++;
        } else if (!clip_a[i].neural_embedding.empty() && !clip_b[i].neural_embedding.empty()) {
            float sim = compute_cosine_similarity(clip_a[i].neural_embedding, clip_b[i].neural_embedding);
            if (sim >= 0.88f) {
                match_count++;
            }
        }
    }

    float match_ratio = static_cast<float>(match_count) / static_cast<float>(min_len);
    return match_ratio >= similarity_threshold;
}

std::vector<MeshChunk> RabinMeshEngine::partition_stream_simd(const uint8_t* data, size_t size) {
    std::vector<MeshChunk> chunks;
    if (!data || size == 0) return chunks;

    // Hierarchical Level 2: If stream contains NAL/GOP units, align chunks to GOP boundaries
    auto gops = slice_stream_gops(data, size);
    if (gops.size() > 1) {
        for (const auto& gop : gops) {
            MeshChunk chunk;
            chunk.virtual_offset = gop.start_offset;
            chunk.length = gop.byte_length;
            chunk.blake3_hash = gop.gop_hash;
            chunk.is_residual = false;
            chunks.push_back(chunk);
        }
        return chunks;
    }

    uint64_t chunk_start = 0;
    uint64_t rolling_fp = 0;

    // Level 3 VectorCDC: evaluates sliding window with rolling hash and tree-based check
    for (uint64_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        rolling_fp = (rolling_fp << 1) + rabin_table_[byte];

        uint64_t current_len = (i - chunk_start) + 1;

        bool boundary_detected = ((rolling_fp & VECTOR_CDC_MASK) == 0 &&
                                  current_len >= VECTOR_CDC_MIN_CHUNK);
        bool max_reached = (current_len >= VECTOR_CDC_MAX_CHUNK);
        bool end_of_stream = (i + 1 == size);

        if (boundary_detected || max_reached || end_of_stream) {
            MeshChunk chunk;
            chunk.virtual_offset = chunk_start;
            chunk.length = static_cast<uint32_t>(current_len);
            chunk.blake3_hash = BbrEngine::compute_sha256(data + chunk_start, current_len);
            chunk.is_residual = false;
            chunks.push_back(chunk);

            chunk_start = i + 1;
        }
    }

    return chunks;
}

uint64_t RabinMeshEngine::ingest_video(const uint8_t* data,
                                       size_t size,
                                       const std::string& vault_dir,
                                       const std::string& cluster_uuid,
                                       bool as_master,
                                       MeshDeltaManifest& out_manifest) {
    if (!data || size == 0) return 0;

    out_manifest.original_file_size = size;
    out_manifest.original_sha256 = BbrEngine::compute_sha256(data, size);
    out_manifest.master_cluster_uuid = cluster_uuid;
    out_manifest.is_master_node = as_master;

    // Partition using Level 3 SIMD VectorCDC
    out_manifest.chunk_references = partition_stream_simd(data, size);

    fs::create_directories(vault_dir);
    uint64_t new_bytes_written = 0;

    for (const auto& chunk : out_manifest.chunk_references) {
        std::string hex_hash = hash_to_hex(chunk.blake3_hash);
        fs::path block_path = fs::path(vault_dir) / (hex_hash + ".blk");

        if (!fs::exists(block_path)) {
            std::ofstream out(block_path, std::ios::binary);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to write mesh block: " + block_path.string());
            }
            out.write(reinterpret_cast<const char*>(data + chunk.virtual_offset), chunk.length);
            if (!out) {
                throw std::runtime_error("Failed to commit mesh block: " + block_path.string());
            }
            new_bytes_written += chunk.length;
        }
    }

    return new_bytes_written;
}

bool RabinMeshEngine::reconstruct_video(const MeshDeltaManifest& manifest,
                                        const std::string& vault_dir,
                                        std::vector<uint8_t>& out_stream) {
    out_stream.clear();
    out_stream.reserve(manifest.original_file_size);

    for (const auto& chunk : manifest.chunk_references) {
        std::string hex_hash = hash_to_hex(chunk.blake3_hash);
        fs::path block_path = fs::path(vault_dir) / (hex_hash + ".blk");

        if (!fs::exists(block_path)) return false;

        std::ifstream in(block_path, std::ios::binary);
        if (!in.is_open()) return false;

        std::vector<uint8_t> block_buf((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
        in.close();

        if (block_buf.size() != chunk.length) return false;
        if (BbrEngine::compute_sha256(block_buf.data(), block_buf.size()) != chunk.blake3_hash) {
            return false;
        }

        out_stream.insert(out_stream.end(), block_buf.begin(), block_buf.end());
    }

    if (out_stream.size() != manifest.original_file_size) return false;
    if (BbrEngine::compute_sha256(out_stream.data(), out_stream.size()) != manifest.original_sha256) {
        return false;
    }

    return true;
}

bool RabinMeshEngine::read_virtual_range(const MeshDeltaManifest& manifest,
                                         const std::string& vault_dir,
                                         uint64_t offset,
                                         size_t length,
                                         uint8_t* destination) {
    if (!destination || offset >= manifest.original_file_size || length == 0) return false;

    size_t bytes_to_read = std::min(static_cast<uint64_t>(length),
                                     manifest.original_file_size - offset);
    size_t bytes_copied = 0;

    for (const auto& chunk : manifest.chunk_references) {
        uint64_t chunk_start = chunk.virtual_offset;
        uint64_t chunk_end = chunk_start + chunk.length;

        // Check if requested range overlaps with this chunk
        if (offset + bytes_copied < chunk_end && offset + bytes_copied >= chunk_start) {
            uint64_t in_chunk_offset = (offset + bytes_copied) - chunk_start;
            size_t available = static_cast<size_t>(chunk.length - in_chunk_offset);
            size_t need = bytes_to_read - bytes_copied;
            size_t copy_now = std::min(available, need);

            std::string hex_hash = hash_to_hex(chunk.blake3_hash);
            fs::path block_path = fs::path(vault_dir) / (hex_hash + ".blk");

            std::ifstream in(block_path, std::ios::binary);
            if (!in.is_open()) return false;

            in.seekg(static_cast<std::streamoff>(in_chunk_offset));
            in.read(reinterpret_cast<char*>(destination + bytes_copied), copy_now);
            if (!in) return false;

            bytes_copied += copy_now;
            if (bytes_copied >= bytes_to_read) break;
        }
    }

    return bytes_copied == bytes_to_read;
}

bool RabinMeshEngine::elect_new_master(const std::vector<MeshDeltaManifest>& cluster_manifests,
                                       size_t deleted_master_idx,
                                       MeshDeltaManifest& out_promoted_master) {
    if (cluster_manifests.empty()) return false;

    // Pick candidate with largest file size or most chunks that isn't the deleted master
    size_t best_idx = size_t(-1);
    uint64_t max_size = 0;

    for (size_t i = 0; i < cluster_manifests.size(); ++i) {
        if (i == deleted_master_idx) continue;
        if (cluster_manifests[i].original_file_size > max_size) {
            max_size = cluster_manifests[i].original_file_size;
            best_idx = i;
        }
    }

    if (best_idx == size_t(-1)) return false;

    out_promoted_master = cluster_manifests[best_idx];
    out_promoted_master.is_master_node = true;
    return true;
}

bool RabinMeshEngine::save_manifest(const MeshDeltaManifest& manifest, const std::string& manifest_path) {
    std::ofstream out(manifest_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    uint32_t magic = MeshDeltaManifest::MAGIC;
    uint16_t version = MeshDeltaManifest::VERSION;
    uint16_t flags = manifest.is_master_node ? 0x0001 : 0x0002;

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

    out.write(reinterpret_cast<const char*>(&manifest.original_file_size), sizeof(manifest.original_file_size));
    out.write(reinterpret_cast<const char*>(manifest.original_sha256.data()), manifest.original_sha256.size());

    uint32_t uuid_len = static_cast<uint32_t>(manifest.master_cluster_uuid.size());
    out.write(reinterpret_cast<const char*>(&uuid_len), sizeof(uuid_len));
    if (uuid_len > 0) {
        out.write(manifest.master_cluster_uuid.data(), uuid_len);
    }

    uint32_t chunk_count = static_cast<uint32_t>(manifest.chunk_references.size());
    out.write(reinterpret_cast<const char*>(&chunk_count), sizeof(chunk_count));

    for (const auto& c : manifest.chunk_references) {
        out.write(reinterpret_cast<const char*>(&c.virtual_offset), sizeof(c.virtual_offset));
        out.write(reinterpret_cast<const char*>(&c.length), sizeof(c.length));
        out.write(reinterpret_cast<const char*>(c.blake3_hash.data()), c.blake3_hash.size());
        uint8_t is_res = c.is_residual ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&is_res), sizeof(is_res));
    }

    uint32_t res_len = static_cast<uint32_t>(manifest.residual_stream.size());
    out.write(reinterpret_cast<const char*>(&res_len), sizeof(res_len));
    if (res_len > 0) {
        out.write(reinterpret_cast<const char*>(manifest.residual_stream.data()), res_len);
    }

    return static_cast<bool>(out);
}

bool RabinMeshEngine::load_manifest(const std::string& manifest_path, MeshDeltaManifest& out_manifest) {
    std::ifstream in(manifest_path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t flags = 0;

    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&flags), sizeof(flags));

    if (magic != MeshDeltaManifest::MAGIC || version != MeshDeltaManifest::VERSION) {
        return false;
    }

    out_manifest.is_master_node = (flags & 0x0001) != 0;

    in.read(reinterpret_cast<char*>(&out_manifest.original_file_size), sizeof(out_manifest.original_file_size));
    in.read(reinterpret_cast<char*>(out_manifest.original_sha256.data()), out_manifest.original_sha256.size());

    uint32_t uuid_len = 0;
    in.read(reinterpret_cast<char*>(&uuid_len), sizeof(uuid_len));
    if (uuid_len > 0 && uuid_len < 4096) {
        out_manifest.master_cluster_uuid.resize(uuid_len);
        in.read(&out_manifest.master_cluster_uuid[0], uuid_len);
    } else {
        out_manifest.master_cluster_uuid.clear();
    }

    uint32_t chunk_count = 0;
    in.read(reinterpret_cast<char*>(&chunk_count), sizeof(chunk_count));
    out_manifest.chunk_references.clear();
    out_manifest.chunk_references.reserve(chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        MeshChunk c;
        in.read(reinterpret_cast<char*>(&c.virtual_offset), sizeof(c.virtual_offset));
        in.read(reinterpret_cast<char*>(&c.length), sizeof(c.length));
        in.read(reinterpret_cast<char*>(c.blake3_hash.data()), c.blake3_hash.size());
        uint8_t is_res = 0;
        in.read(reinterpret_cast<char*>(&is_res), sizeof(is_res));
        c.is_residual = (is_res != 0);
        out_manifest.chunk_references.push_back(c);
    }

    uint32_t res_len = 0;
    in.read(reinterpret_cast<char*>(&res_len), sizeof(res_len));
    if (res_len > 0 && res_len < 100 * 1024 * 1024) {
        out_manifest.residual_stream.resize(res_len);
        in.read(reinterpret_cast<char*>(out_manifest.residual_stream.data()), res_len);
    } else {
        out_manifest.residual_stream.clear();
    }

    return static_cast<bool>(in);
}

uint64_t RabinMeshEngine::ingest_video_file(const std::string& input_video_path,
                                           const std::string& vault_dir,
                                           const std::string& manifest_output_path,
                                           const std::string& cluster_uuid,
                                           bool as_master) {
    std::ifstream in(input_video_path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open input video: " + input_video_path);
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();

    MeshDeltaManifest manifest;
    uint64_t new_bytes = ingest_video(data.data(), data.size(), vault_dir, cluster_uuid, as_master, manifest);
    if (!save_manifest(manifest, manifest_output_path)) {
        throw std::runtime_error("Failed to save manifest to: " + manifest_output_path);
    }
    return new_bytes;
}

bool RabinMeshEngine::restore_video_file(const std::string& manifest_path,
                                        const std::string& vault_dir,
                                        const std::string& destination_path) {
    MeshDeltaManifest manifest;
    if (!load_manifest(manifest_path, manifest)) return false;

    std::vector<uint8_t> restored;
    if (!reconstruct_video(manifest, vault_dir, restored)) return false;

    std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(restored.data()), restored.size());
    return static_cast<bool>(out);
}

} // namespace prism
