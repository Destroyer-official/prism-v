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
    int32_t levels = 15;
    int32_t k = (levels - 1) / 2; // 7

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
