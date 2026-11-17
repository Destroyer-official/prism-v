#include "test_harness.hpp"
#include "prism/detint.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_detint_overflow_free_proof) {
    // Torture test: set all activations to +32767 and all weights to +127 for maximum K=512
    std::vector<int16_t> x(DetInt::MAX_K_CHANNELS, DetInt::ACT_MAX);
    std::vector<int8_t> w(DetInt::MAX_K_CHANNELS, DetInt::WEIGHT_MAX);

    int32_t result = DetInt::dot_product(x.data(), w.data(), DetInt::MAX_K_CHANNELS);
    // Exact theoretical maximum: 512 * 127 * 32767 = 2,130,641,408
    int64_t expected = static_cast<int64_t>(DetInt::MAX_K_CHANNELS) *
                       static_cast<int64_t>(DetInt::WEIGHT_MAX) *
                       static_cast<int64_t>(DetInt::ACT_MAX);

    PRISM_ASSERT_EQ(static_cast<int64_t>(result), expected);
    PRISM_ASSERT(result > 0);
    PRISM_ASSERT(result < INT32_MAX);

    // Negative maximum torture test
    std::vector<int8_t> w_neg(DetInt::MAX_K_CHANNELS, -127);
    int32_t result_neg = DetInt::dot_product(x.data(), w_neg.data(), DetInt::MAX_K_CHANNELS);
    PRISM_ASSERT_EQ(static_cast<int64_t>(result_neg), -expected);
}

PRISM_TEST(test_detint_dyadic_rescaling) {
    // Test dyadic rounding
    int32_t acc = 65536; // 2^16
    int16_t res = DetInt::dyadic_rescale(acc, 16384, 15);
    PRISM_ASSERT_EQ(res, 32767); // Clamped to ACT_MAX

    int32_t acc2 = 1000;
    int16_t res2 = DetInt::dyadic_rescale(acc2, 16384, 15); // (1000 * 16384 + 16384) >> 15 = 500
    PRISM_ASSERT_EQ(res2, 500);

    // Test clamping bounds
    int16_t clamp_high = DetInt::dyadic_rescale(10000000, 32767, 10);
    PRISM_ASSERT_EQ(clamp_high, DetInt::ACT_MAX);

    int16_t clamp_low = DetInt::dyadic_rescale(-10000000, 32767, 10);
    PRISM_ASSERT_EQ(clamp_low, DetInt::ACT_MIN);
}

PRISM_TEST(test_detint_static_lut) {
    // 512-byte LUT monotonic boundary checks
    PRISM_ASSERT_EQ(DetInt::CDF_SIGMOID_LUT[0], 0);
    PRISM_ASSERT_EQ(DetInt::CDF_SIGMOID_LUT[511], 32768);

    // Check center point is approximately 16384
    uint16_t center = DetInt::CDF_SIGMOID_LUT[256];
    PRISM_ASSERT(center >= 16000 && center <= 16800);

    // Lookup function bounds
    uint16_t cdf1 = DetInt::lookup_cdf(-255, 0);
    uint16_t cdf2 = DetInt::lookup_cdf(255, 10);
    PRISM_ASSERT(cdf1 <= cdf2);
    PRISM_ASSERT(cdf2 <= DetInt::CDF_NORMALIZATION_MASS);
}

PRISM_TEST(test_detint_16kb_page_alignment) {
    size_t ps = DetInt::query_system_page_size();
    PRISM_ASSERT(ps >= PAGE_SIZE_16KB);

    void* ptr = DetInt::allocate_page_aligned(1024, PAGE_SIZE_16KB);
    PRISM_ASSERT(ptr != nullptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    PRISM_ASSERT((addr % PAGE_SIZE_16KB) == 0); // 16 KB page-size aligned!

    DetInt::free_page_aligned(ptr);
}

PRISM_TEST(test_detint_linear_layer_and_relu) {
    // Test linear_layer with identical test vector to Java DetInt
    int16_t lin_x[2] = {10, -20};
    int8_t lin_w[4] = {2, 1, -3, 4}; // row 0: [2, 1], row 1: [-3, 4]
    int32_t lin_bias[2] = {5, -5};
    int16_t lin_y[2] = {0, 0};

    DetInt::linear_layer(lin_x, lin_w, lin_bias, lin_y, 2, 2, 16384, 14);
    PRISM_ASSERT_EQ(lin_y[0], 5);
    PRISM_ASSERT_EQ(lin_y[1], -115);

    // Test relu
    int16_t relu_arr[5] = {500, -100, 0, -32767, 32767};
    DetInt::relu(relu_arr, 5);
    PRISM_ASSERT_EQ(relu_arr[0], 500);
    PRISM_ASSERT_EQ(relu_arr[1], 0);
    PRISM_ASSERT_EQ(relu_arr[2], 0);
    PRISM_ASSERT_EQ(relu_arr[3], 0);
    PRISM_ASSERT_EQ(relu_arr[4], 32767);
}

PRISM_TEST(test_detint_aligned_buffer_raii) {
    constexpr size_t ELEMENT_COUNT = 4096;
    AlignedBuffer<int32_t> buf(ELEMENT_COUNT, PAGE_SIZE_16KB);

    PRISM_ASSERT(buf.data() != nullptr);
    PRISM_ASSERT_EQ(buf.size(), ELEMENT_COUNT);

    uintptr_t addr = reinterpret_cast<uintptr_t>(buf.data());
    PRISM_ASSERT((addr % PAGE_SIZE_16KB) == 0);

    for (size_t i = 0; i < ELEMENT_COUNT; ++i) {
        buf[i] = static_cast<int32_t>(i * 17 - 100);
    }

    for (size_t i = 0; i < ELEMENT_COUNT; ++i) {
        PRISM_ASSERT_EQ(buf[i], static_cast<int32_t>(i * 17 - 100));
    }

    // Move semantics check
    AlignedBuffer<int32_t> moved = std::move(buf);
    PRISM_ASSERT(buf.data() == nullptr);
    PRISM_ASSERT(moved.data() != nullptr);
    PRISM_ASSERT_EQ(moved[10], static_cast<int32_t>(10 * 17 - 100));
}

PRISM_TEST(test_detint_dyadic_rescale_shift_bounds) {
    // Test s_shift = 1 (minimum power of two shift)
    int16_t r1 = DetInt::dyadic_rescale(100, 1, 1); // (100 * 1 + 1) >> 1 = 50
    PRISM_ASSERT_EQ(r1, 50);

    // Test s_shift = 30 (large shift)
    int16_t r30 = DetInt::dyadic_rescale(1073741824, 1, 30); // 2^30 / 2^30 = 1
    PRISM_ASSERT_EQ(r30, 1);

    // Negative rounding symmetry
    int16_t r_neg = DetInt::dyadic_rescale(-1000, 16384, 15);
    PRISM_ASSERT_EQ(r_neg, -500);
}

