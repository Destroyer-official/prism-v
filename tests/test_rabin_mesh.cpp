#include "test_harness.hpp"
#include "prism/rabin_mesh.hpp"
#include <filesystem>
#include <fstream>
#include <numeric>

namespace fs = std::filesystem;
using namespace prism;

PRISM_TEST(test_rabin_mesh_keyframe_fingerprint) {
    RabinMeshEngine engine;

    constexpr uint32_t W = 128;
    constexpr uint32_t H = 128;
    std::vector<uint8_t> frame1(W * H);
    std::vector<uint8_t> frame2(W * H); // Slight noise variation (near-duplicate)
    std::vector<uint8_t> frame3(W * H); // Inverted/different pattern

    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint8_t pat = static_cast<uint8_t>((x * 2) ^ (y * 2));
            frame1[y * W + x] = pat;
            frame2[y * W + x] = static_cast<uint8_t>(pat + (x % 3 == 0 ? 1 : 0)); // 99% similar
            frame3[y * W + x] = static_cast<uint8_t>(255 - pat); // Inverted
        }
    }

    PerceptualKeyframe kf1 = engine.fingerprint_frame(frame1.data(), W, H, 0);
    PerceptualKeyframe kf2 = engine.fingerprint_frame(frame2.data(), W, H, 33);
    PerceptualKeyframe kf3 = engine.fingerprint_frame(frame3.data(), W, H, 66);

    PRISM_ASSERT(kf1.quality_score > 0);
    PRISM_ASSERT_EQ(kf1.neural_embedding.size(), 128);

    uint32_t dist_near = RabinMeshEngine::compute_hamming_distance(kf1.vpdq_hash, kf2.vpdq_hash);
    uint32_t dist_far = RabinMeshEngine::compute_hamming_distance(kf1.vpdq_hash, kf3.vpdq_hash);

    PRISM_ASSERT(dist_near <= 15); // High similarity
    PRISM_ASSERT(dist_far > 50);   // Significant difference

    float sim_near = RabinMeshEngine::compute_cosine_similarity(kf1.neural_embedding, kf2.neural_embedding);
    PRISM_ASSERT(sim_near > 0.90f);
}

PRISM_TEST(test_rabin_mesh_burst_duplicate_detection) {
    RabinMeshEngine engine;

    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    std::vector<uint8_t> frame(W * H, 128);
    PerceptualKeyframe kf = engine.fingerprint_frame(frame.data(), W, H, 0);

    std::vector<PerceptualKeyframe> clip_a = {kf, kf, kf};
    std::vector<PerceptualKeyframe> clip_b = {kf, kf, kf};

    PRISM_ASSERT(engine.is_burst_duplicate(clip_a, clip_b, 0.85f));
}

PRISM_TEST(test_rabin_mesh_gop_slicing) {
    RabinMeshEngine engine;

    // Build synthetic NAL stream with two IDR frames: 00 00 00 01 05 ... 00 00 00 01 05 ...
    std::vector<uint8_t> stream(160 * 1024, 0xAA);
    // Insert IDR 1 at 0
    stream[0] = 0; stream[1] = 0; stream[2] = 0; stream[3] = 1; stream[4] = 0x05;
    // Insert IDR 2 at 80 KB
    size_t idr2_pos = 80 * 1024;
    stream[idr2_pos] = 0; stream[idr2_pos + 1] = 0; stream[idr2_pos + 2] = 0;
    stream[idr2_pos + 3] = 1; stream[idr2_pos + 4] = 0x05;

    auto gops = engine.slice_stream_gops(stream.data(), stream.size());
    PRISM_ASSERT(gops.size() >= 2);
    PRISM_ASSERT_EQ(gops[0].start_offset, 0);
    PRISM_ASSERT_EQ(gops[1].start_offset, idr2_pos);
}

PRISM_TEST(test_rabin_mesh_vector_cdc_and_virtual_reconstruction) {
    std::string vault_dir = "test_mesh_vault";
    if (fs::exists(vault_dir)) fs::remove_all(vault_dir);

    RabinMeshEngine engine;

    // Create 1 MB synthetic stream with repeated 256 KB pattern
    size_t stream_size = 1024 * 1024;
    std::vector<uint8_t> stream1(stream_size);
    for (size_t i = 0; i < stream_size; ++i) {
        stream1[i] = static_cast<uint8_t>((i % 256) ^ ((i / 65536) & 0xFF));
    }

    // Stream 2 is a burst take: identical except for the last 32 KB
    std::vector<uint8_t> stream2 = stream1;
    for (size_t i = stream_size - 32 * 1024; i < stream_size; ++i) {
        stream2[i] = static_cast<uint8_t>(stream1[i] ^ 0xFF);
    }

    MeshDeltaManifest manifest1;
    uint64_t new_bytes1 = engine.ingest_video(stream1.data(), stream1.size(),
                                              vault_dir, "cluster-101", true, manifest1);
    PRISM_ASSERT(new_bytes1 > 0);

    MeshDeltaManifest manifest2;
    uint64_t new_bytes2 = engine.ingest_video(stream2.data(), stream2.size(),
                                              vault_dir, "cluster-101", false, manifest2);

    // Take 2 reuses most chunks from Take 1, so new_bytes2 should be far smaller than stream_size!
    PRISM_ASSERT(new_bytes2 < stream_size);

    // Byte-exact reconstruction check
    std::vector<uint8_t> restored1;
    bool ok1 = engine.reconstruct_video(manifest1, vault_dir, restored1);
    PRISM_ASSERT(ok1);
    PRISM_ASSERT_EQ(restored1.size(), stream1.size());
    PRISM_ASSERT(restored1 == stream1);

    std::vector<uint8_t> restored2;
    bool ok2 = engine.reconstruct_video(manifest2, vault_dir, restored2);
    PRISM_ASSERT(ok2);
    PRISM_ASSERT_EQ(restored2.size(), stream2.size());
    PRISM_ASSERT(restored2 == stream2);

    // Test virtual range read (VFS Proxy simulation)
    std::vector<uint8_t> sub_read(4096);
    uint64_t test_offset = 200 * 1024; // Read 4 KB from offset 200 KB
    bool vfs_ok = engine.read_virtual_range(manifest1, vault_dir, test_offset, sub_read.size(), sub_read.data());
    PRISM_ASSERT(vfs_ok);
    for (size_t i = 0; i < sub_read.size(); ++i) {
        PRISM_ASSERT_EQ(sub_read[i], stream1[test_offset + i]);
    }

    // Test Master Re-Election
    std::vector<MeshDeltaManifest> cluster = {manifest1, manifest2};
    MeshDeltaManifest promoted;
    bool elect_ok = engine.elect_new_master(cluster, 0, promoted);
    PRISM_ASSERT(elect_ok);
    PRISM_ASSERT(promoted.is_master_node);

    // Clean up
    fs::remove_all(vault_dir);
}
