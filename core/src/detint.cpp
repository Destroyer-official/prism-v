#include "prism/detint.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__SSE4_1__) || defined(__AVX2__) || (defined(_M_X64) && !defined(__arm64ec__))
#include <immintrin.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace prism {

// Static 512-byte (256-word) Sigmoid/Laplace lookup table compiled into binary
// Monotonically non-decreasing, normalized to max value 32768
const std::array<uint16_t, 512> DetInt::CDF_SIGMOID_LUT = []() {
    std::array<uint16_t, 512> lut{};
    for (size_t i = 0; i < 512; ++i) {
        // Pre-computed discrete sigmoid curve centered at index 256
        double x = (static_cast<double>(i) - 256.0) / 32.0;
        double s = 1.0 / (1.0 + std::exp(-x));
        uint32_t val = static_cast<uint32_t>(s * 32768.0);
        if (val > 32768) val = 32768;
        lut[i] = static_cast<uint16_t>(val);
    }
    // Guarantee boundary values
    lut[0] = 0;
    lut[511] = 32768;
    return lut;
}();

int32_t DetInt::dot_product(const int16_t* x, const int8_t* w, size_t k) {
    if (k > MAX_K_CHANNELS) {
        throw std::invalid_argument("Channel depth K exceeds Prism-DetInt contract bound of 512");
    }

    int32_t acc = 0;
    size_t i = 0;

#if defined(__ARM_NEON) || defined(__aarch64__)
    int32x4_t v_acc = vdupq_n_s32(0);
    for (; i + 7 < k; i += 8) {
        int16x8_t vx = vld1q_s16(x + i);
        int8x8_t vw8 = vld1_s8(w + i);
        int16x8_t vw = vmovl_s8(vw8);
        int32x4_t low = vmull_s16(vget_low_s16(vx), vget_low_s16(vw));
        v_acc = vmlal_s16(v_acc, vget_high_s16(vx), vget_high_s16(vw));
        v_acc = vaddq_s32(v_acc, low);
    }
#if defined(__aarch64__)
    acc = vaddvq_s32(v_acc);
#else
    int32x2_t sum2 = vadd_s32(vget_low_s32(v_acc), vget_high_s32(v_acc));
    acc = vget_lane_s32(vpadd_s32(sum2, sum2), 0);
#endif

#elif defined(__SSE4_1__) || defined(_M_X64)
    __m128i v_acc = _mm_setzero_si128();
    for (; i + 7 < k; i += 8) {
        __m128i vx = _mm_loadu_si128(reinterpret_cast<const __m128i*>(x + i));
        __m128i vw8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(w + i));
        __m128i vw16 = _mm_cvtepi8_epi16(vw8);
        __m128i prod32 = _mm_madd_epi16(vx, vw16);
        v_acc = _mm_add_epi32(v_acc, prod32);
    }
    alignas(16) int32_t acc_arr[4];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(acc_arr), v_acc);
    acc = acc_arr[0] + acc_arr[1] + acc_arr[2] + acc_arr[3];

#else
    // Fallback vector unrolled loop: mathematically associative in two's complement without overflow
    for (; i + 7 < k; i += 8) {
        acc += static_cast<int32_t>(x[i + 0]) * static_cast<int32_t>(w[i + 0]);
        acc += static_cast<int32_t>(x[i + 1]) * static_cast<int32_t>(w[i + 1]);
        acc += static_cast<int32_t>(x[i + 2]) * static_cast<int32_t>(w[i + 2]);
        acc += static_cast<int32_t>(x[i + 3]) * static_cast<int32_t>(w[i + 3]);
        acc += static_cast<int32_t>(x[i + 4]) * static_cast<int32_t>(w[i + 4]);
        acc += static_cast<int32_t>(x[i + 5]) * static_cast<int32_t>(w[i + 5]);
        acc += static_cast<int32_t>(x[i + 6]) * static_cast<int32_t>(w[i + 6]);
        acc += static_cast<int32_t>(x[i + 7]) * static_cast<int32_t>(w[i + 7]);
    }
#endif

    for (; i < k; ++i) {
        acc += static_cast<int32_t>(x[i]) * static_cast<int32_t>(w[i]);
    }

    return acc;
}

int16_t DetInt::dyadic_rescale(int32_t acc, int32_t m_scale, int32_t s_shift) {
    // Dyadic formula: floor((acc * M_scale + 2^(s - 1)) / 2^s)
    int64_t scaled = static_cast<int64_t>(acc) * static_cast<int64_t>(m_scale);
    int64_t round_bias = 1LL << (s_shift - 1);
    int64_t shifted = (scaled + round_bias) >> s_shift;

    if (shifted > ACT_MAX) return ACT_MAX;
    if (shifted < ACT_MIN) return ACT_MIN;
    return static_cast<int16_t>(shifted);
}

void DetInt::linear_layer(const int16_t* x,
                          const int8_t* weights,
                          const int32_t* bias,
                          int16_t* y,
                          size_t k_in,
                          size_t m_out,
                          int32_t m_scale,
                          int32_t s_shift) {
    for (size_t m = 0; m < m_out; ++m) {
        const int8_t* row_w = weights + (m * k_in);
        int32_t acc = dot_product(x, row_w, k_in);
        if (bias) {
            acc += bias[m];
        }
        y[m] = dyadic_rescale(acc, m_scale, s_shift);
    }
}

void DetInt::relu(int16_t* data, size_t count) {
    size_t i = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    int16x8_t zero = vdupq_n_s16(0);
    for (; i + 7 < count; i += 8) {
        int16x8_t v = vld1q_s16(data + i);
        vst1q_s16(data + i, vmaxq_s16(v, zero));
    }
#elif defined(__SSE2__) || defined(_M_X64)
    __m128i zero = _mm_setzero_si128();
    for (; i + 7 < count; i += 8) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), _mm_max_epi16(v, zero));
    }
#endif
    for (; i < count; ++i) {
        if (data[i] < 0) {
            data[i] = 0;
        }
    }
}

uint16_t DetInt::lookup_cdf(int32_t diff, uint16_t index) {
    int32_t lut_idx = diff + 256;
    if (lut_idx < 0) lut_idx = 0;
    if (lut_idx > 511) lut_idx = 511;

    uint32_t base_prob = CDF_SIGMOID_LUT[static_cast<size_t>(lut_idx)];
    // Ensure strict monotonicity across symbol index
    uint32_t combined = base_prob + (static_cast<uint32_t>(index) << 1);
    if (combined > CDF_NORMALIZATION_MASS) {
        combined = CDF_NORMALIZATION_MASS;
    }
    return static_cast<uint16_t>(combined);
}

size_t DetInt::query_system_page_size() {
#if defined(_WIN32)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    size_t ps = sys_info.dwPageSize;
    return (ps < PAGE_SIZE_16KB) ? PAGE_SIZE_16KB : ps;
#elif defined(_SC_PAGESIZE)
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) return PAGE_SIZE_16KB;
    return static_cast<size_t>((ps < static_cast<long>(PAGE_SIZE_16KB)) ? PAGE_SIZE_16KB : ps);
#else
    return PAGE_SIZE_16KB;
#endif
}

void* DetInt::allocate_page_aligned(size_t bytes, size_t alignment) {
    if (bytes == 0) return nullptr;
    if (alignment < PAGE_SIZE_16KB) {
        alignment = PAGE_SIZE_16KB;
    }

#if defined(_WIN32)
    return _aligned_malloc(bytes, alignment);
#elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)
    void* ptr = nullptr;
    int res = posix_memalign(&ptr, alignment, bytes);
    if (res != 0) return nullptr;
    return ptr;
#else
    return std::aligned_alloc(alignment, bytes);
#endif
}

void DetInt::free_page_aligned(void* ptr) {
    if (!ptr) return;
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

} // namespace prism
