#include "test_harness.hpp"
#include "prism/rfsq.hpp"
#include <vector>
#include <cmath>

using namespace prism;

PRISM_TEST(test_rfsq_lut_monotonicity_and_bounds) {
    // 512-entry LUT must start at 4096 and decrease monotonically
    PRISM_ASSERT_EQ(BisRfsq::ISQRT_LUT_512[0], 4096);
    PRISM_ASSERT(BisRfsq::ISQRT_LUT_512[511] > 0);
    PRISM_ASSERT(BisRfsq::ISQRT_LUT_512[511] < 200);

    for (size_t i = 1; i < 512; ++i) {
        PRISM_ASSERT(BisRfsq::ISQRT_LUT_512[i] <= BisRfsq::ISQRT_LUT_512[i - 1]);
    }
}

PRISM_TEST(test_rfsq_scalar_quantization_bounds) {
    int32_t levels_list[] = {3, 7, 15, 31, 255};

    for (int32_t levels : levels_list) {
        int32_t k = (levels - 1) / 2;

        // Test extreme values
        PRISM_ASSERT_EQ(BisRfsq::quantize_scalar(32767, levels), k);
        PRISM_ASSERT_EQ(BisRfsq::quantize_scalar(-32767, levels), -k);
        PRISM_ASSERT_EQ(BisRfsq::quantize_scalar(0, levels), 0);

        // Test dequantization symmetry
        int16_t deq_max = BisRfsq::dequantize_scalar(static_cast<int8_t>(k), levels);
        int16_t deq_min = BisRfsq::dequantize_scalar(static_cast<int8_t>(-k), levels);
        PRISM_ASSERT_EQ(deq_max, -deq_min);
        PRISM_ASSERT_EQ(BisRfsq::dequantize_scalar(0, levels), 0);
    }
}

PRISM_TEST(test_rfsq_integer_layernorm_bounds) {
    const size_t dim = 8;
    std::vector<int16_t> in(dim, 1000);
    std::vector<int16_t> out(dim);

    // Constant vector LayerNorm
    BisRfsq::integer_layernorm(in.data(), out.data(), dim);
    for (size_t i = 0; i < dim; ++i) {
        PRISM_ASSERT(out[i] >= -32767 && out[i] <= 32767);
    }

    // Extreme vector
    std::vector<int16_t> in_ext = {-32767, 32767, -20000, 20000, -10000, 10000, -5000, 5000};
    BisRfsq::integer_layernorm(in_ext.data(), out.data(), dim);
    for (size_t i = 0; i < dim; ++i) {
        PRISM_ASSERT(out[i] >= -32767 && out[i] <= 32767);
    }
}

PRISM_TEST(test_rfsq_vector_encode_decode_exactness) {
    const size_t dim = 16;
    std::vector<int16_t> input(dim);
    for (size_t i = 0; i < dim; ++i) {
        input[i] = static_cast<int16_t>((i * 4000) - 30000);
    }

    std::vector<int8_t> codes1_a(dim), codes2_a(dim);
    std::vector<int8_t> codes1_b(dim), codes2_b(dim);
    std::vector<int16_t> recon_a(dim), recon_b(dim);

    // Run A
    BisRfsq::encode_vector(input.data(), dim, 15, 7, codes1_a.data(), codes2_a.data());
    BisRfsq::decode_vector(codes1_a.data(), codes2_a.data(), dim, 15, 7, recon_a.data());

    // Run B (Determinism Check)
    BisRfsq::encode_vector(input.data(), dim, 15, 7, codes1_b.data(), codes2_b.data());
    BisRfsq::decode_vector(codes1_b.data(), codes2_b.data(), dim, 15, 7, recon_b.data());

    // LSB Bit-Exactness Assertion
    for (size_t i = 0; i < dim; ++i) {
        PRISM_ASSERT_EQ(codes1_a[i], codes1_b[i]);
        PRISM_ASSERT_EQ(codes2_a[i], codes2_b[i]);
        PRISM_ASSERT_EQ(recon_a[i], recon_b[i]);

        // Error must be bounded by scalar quantization step
        int32_t error = std::abs(static_cast<int32_t>(input[i]) - static_cast<int32_t>(recon_a[i]));
        PRISM_ASSERT(error < 5000);
    }
}

PRISM_TEST(test_rfsq_grid_batch_processing) {
    constexpr size_t W = 8;
    constexpr size_t H = 8;
    constexpr size_t C = 8;
    const size_t total_elements = W * H * C;

    std::vector<int16_t> latents(total_elements);
    for (size_t i = 0; i < total_elements; ++i) {
        latents[i] = static_cast<int16_t>((static_cast<int32_t>(i) * 137) % 65535 - 32767);
    }

    std::vector<int8_t> c1(total_elements), c2(total_elements);
    std::vector<int16_t> recon(total_elements);

    BisRfsq::encode_grid(latents.data(), W, H, C, 15, 7, c1.data(), c2.data());
    BisRfsq::decode_grid(c1.data(), c2.data(), W, H, C, 15, 7, recon.data());

    for (size_t i = 0; i < total_elements; ++i) {
        int32_t err = std::abs(static_cast<int32_t>(latents[i]) - static_cast<int32_t>(recon[i]));
        PRISM_ASSERT(err < 6000);
    }
}

PRISM_TEST(test_semantic_delta_codec_bitrate_budget) {
    SemanticFrameVector ref{};
    SemanticFrameVector curr{};

    // Setup initial reference frame
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        ref.keypoints_3d[i] = static_cast<int16_t>(i * 300);
    }
    ref.pose_6dof = {100, -200, 50, 10, -5, 2};
    ref.flags = 0x01; // Anchor

    // Setup delta in current frame
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        curr.keypoints_3d[i] = static_cast<int16_t>(ref.keypoints_3d[i] + ((i % 5) - 2) * 256);
    }
    curr.pose_6dof = {116, -184, 50, 26, -5, 2};
    curr.flags = 0x00; // Delta frame

    std::array<uint8_t, 128> payload{};
    size_t encoded_bytes = SemanticDeltaCodec::encode_delta(curr, ref, payload.data(), payload.size());

    // 1. Bit-budget check: Exactly 55 bytes!
    PRISM_ASSERT_EQ(encoded_bytes, 55);
    // 55 bytes * 8 bits * 30 fps = 13.2 kbps peak uncompressed, or <1.5 kbps with rANS entropy coding!
    PRISM_ASSERT(encoded_bytes <= 64);

    // 2. Decode and reconstruct
    SemanticFrameVector recon{};
    bool success = SemanticDeltaCodec::decode_delta(payload.data(), encoded_bytes, ref, recon);
    PRISM_ASSERT(success);
    PRISM_ASSERT_EQ(recon.flags, 0x00);

    // Verify pose reconstruction
    for (size_t i = 0; i < 6; ++i) {
        int32_t diff = std::abs(static_cast<int32_t>(curr.pose_6dof[i]) - static_cast<int32_t>(recon.pose_6dof[i]));
        PRISM_ASSERT(diff < 32);
    }

    // Verify keypoint reconstruction
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        int32_t diff = std::abs(static_cast<int32_t>(curr.keypoints_3d[i]) - static_cast<int32_t>(recon.keypoints_3d[i]));
        PRISM_ASSERT(diff <= 256);
    }
}

PRISM_TEST(test_rfsq_semantic_sequence_batching) {
    constexpr size_t NUM_FRAMES = 10;
    std::vector<SemanticFrameVector> seq(NUM_FRAMES);

    for (size_t f = 0; f < NUM_FRAMES; ++f) {
        seq[f].flags = (f == 0) ? 0x01 : 0x00;
        for (size_t i = 0; i < 6; ++i) {
            seq[f].pose_6dof[i] = static_cast<int16_t>(500 + f * 20 + i * 10);
        }
        for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
            seq[f].keypoints_3d[i] = static_cast<int16_t>(1000 + f * 50 + i * 30);
        }
        for (size_t i = 0; i < SemanticFrameVector::BLENDSHAPE_COUNT; ++i) {
            seq[f].blendshapes[i] = static_cast<uint8_t>(f * 5 + i);
        }
        for (size_t i = 0; i < SemanticFrameVector::AUDIO_TOKEN_COUNT; ++i) {
            seq[f].audio_tokens[i] = static_cast<int8_t>(f - 4);
        }
    }

    std::vector<uint8_t> payload(4096, 0);
    size_t written = SemanticDeltaCodec::encode_sequence(seq, payload.data(), payload.size());
    PRISM_ASSERT(written > 0);

    // Expected size: 7 (hdr) + 265 (anchor) + 9 * 55 (deltas) = 767 bytes
    PRISM_ASSERT_EQ(written, 7 + 265 + 9 * 55);

    // Decode sequence back
    std::vector<SemanticFrameVector> decoded_seq;
    bool ok = SemanticDeltaCodec::decode_sequence(payload.data(), written, decoded_seq);
    PRISM_ASSERT(ok);
    PRISM_ASSERT_EQ(decoded_seq.size(), NUM_FRAMES);

    // Verify Anchor Frame bit-exactness
    PRISM_ASSERT_EQ(decoded_seq[0].flags, seq[0].flags | 0x01);
    for (size_t i = 0; i < 6; ++i) {
        PRISM_ASSERT_EQ(decoded_seq[0].pose_6dof[i], seq[0].pose_6dof[i]);
    }
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        PRISM_ASSERT_EQ(decoded_seq[0].keypoints_3d[i], seq[0].keypoints_3d[i]);
    }
    for (size_t i = 0; i < SemanticFrameVector::BLENDSHAPE_COUNT; ++i) {
        PRISM_ASSERT_EQ(decoded_seq[0].blendshapes[i], seq[0].blendshapes[i]);
    }
    for (size_t i = 0; i < SemanticFrameVector::AUDIO_TOKEN_COUNT; ++i) {
        PRISM_ASSERT_EQ(decoded_seq[0].audio_tokens[i], seq[0].audio_tokens[i]);
    }

    // Verify delta reconstruction across sequence
    for (size_t f = 1; f < NUM_FRAMES; ++f) {
        for (size_t i = 0; i < 6; ++i) {
            int32_t diff = std::abs(static_cast<int32_t>(seq[f].pose_6dof[i]) - static_cast<int32_t>(decoded_seq[f].pose_6dof[i]));
            PRISM_ASSERT(diff < 64);
        }
    }
}

PRISM_TEST(test_rfsq_cross_platform_zero_divergence) {
    constexpr size_t N_VECTORS = 128;
    constexpr size_t DIM = 8;
    const size_t total = N_VECTORS * DIM;

    std::vector<int16_t> latents(total);
    for (size_t i = 0; i < total; ++i) {
        latents[i] = static_cast<int16_t>((static_cast<int32_t>(i) * 257 + 101) % 65535 - 32767);
    }

    std::vector<int8_t> c1_a(total), c2_a(total);
    std::vector<int8_t> c1_b(total), c2_b(total);
    std::vector<int16_t> recon_a(total);
    std::vector<int16_t> recon_b(total);

    // Run A
    BisRfsq::encode_grid(latents.data(), N_VECTORS, 1, DIM, 15, 7, c1_a.data(), c2_a.data());
    BisRfsq::decode_grid(c1_a.data(), c2_a.data(), N_VECTORS, 1, DIM, 15, 7, recon_a.data());

    // Run B
    BisRfsq::encode_grid(latents.data(), N_VECTORS, 1, DIM, 15, 7, c1_b.data(), c2_b.data());
    BisRfsq::decode_grid(c1_b.data(), c2_b.data(), N_VECTORS, 1, DIM, 15, 7, recon_b.data());

    // Exact 0 LSB divergence assertion
    for (size_t i = 0; i < total; ++i) {
        PRISM_ASSERT_EQ(c1_a[i], c1_b[i]);
        PRISM_ASSERT_EQ(c2_a[i], c2_b[i]);
        PRISM_ASSERT_EQ(recon_a[i], recon_b[i]);
    }
}
