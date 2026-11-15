#pragma once
#ifndef PRISM_DETINT_HPP
#define PRISM_DETINT_HPP

#include "prism/types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <stdexcept>

namespace prism {

/**
 * @brief Prism-DetInt: Cross-Silicon Strict Class A Integer Determinism Specification.
 *
 * Enforces Zero Floating-Point operations in normative entropy priors,
 * autoregressive loops, and coordinate latents.
 *
 * Mathematical Guarantee:
 * With input vector x in [-32767, +32767]^K (int16_t),
 * weight matrix W in [-127, +127]^(M x K) (int8_t),
 * and channel depth K <= 512:
 *
 * max |a_m| <= 512 * 127 * 32,767 = 2,130,469,376 < 2^31 - 1 (2,147,483,647).
 *
 * Thus, signed 32-bit integer accumulation is mathematically guaranteed
 * to NEVER overflow.
 */
class DetInt {
public:
    static constexpr size_t MAX_K_CHANNELS = 512;
    static constexpr int16_t ACT_MIN = -32767;
    static constexpr int16_t ACT_MAX = 32767;
    static constexpr int8_t WEIGHT_MIN = -127;
    static constexpr int8_t WEIGHT_MAX = 127;
    static constexpr uint16_t CDF_NORMALIZATION_MASS = 32768; // 2^15

    // Static 512-byte Sigmoid/Laplace CDF lookup table
    static const std::array<uint16_t, 512> CDF_SIGMOID_LUT;

    /**
     * @brief Computes deterministic dot product of INT16 activation and INT8 weight.
     * Guaranteed overflow-free in int32_t accumulator for K <= 512.
     */
    static int32_t dot_product(const int16_t* x, const int8_t* w, size_t k);

    /**
     * @brief Deterministic Dyadic Rescaling with round-to-nearest and clamp:
     * z = clamp([-32767, 32767], floor((acc * M_scale + 2^(s-1)) / 2^s))
     */
    static int16_t dyadic_rescale(int32_t acc, int32_t m_scale, int32_t s_shift);

    /**
     * @brief Fully-Connected Deterministic Linear Layer:
     * y = dyadic_rescale(W * x + bias, M_scale, s_shift)
     */
    static void linear_layer(const int16_t* x,
                             const int8_t* weights, // M rows, K cols
                             const int32_t* bias,   // M elements
                             int16_t* y,            // M output elements
                             size_t k_in,
                             size_t m_out,
                             int32_t m_scale,
                             int32_t s_shift);

    /**
     * @brief Element-wise integer ReLU activation.
     */
    static void relu(int16_t* data, size_t count);

    /**
     * @brief Map a delta/difference value to a normative CDF value using the static 512-byte LUT.
     * @param diff Input difference (-255 to +255).
     * @param index Symbol index in alphabet.
     * @return Cumulative distribution function mass in [0, 32768].
     */
    static uint16_t lookup_cdf(int32_t diff, uint16_t index);

    /**
     * @brief Dynamic query for system memory page size.
     * Compatible with Android 15+ 16 KB pages, Linux, and Windows.
     */
    static size_t query_system_page_size();

    /**
     * @brief Allocate 16 KB page-aligned memory buffer.
     */
    static void* allocate_page_aligned(size_t bytes, size_t alignment = PAGE_SIZE_16KB);

    /**
     * @brief Free page-aligned memory buffer.
     */
    static void free_page_aligned(void* ptr);
};

// RAII Page-Aligned Buffer Helper
template <typename T>
class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t count, size_t alignment = PAGE_SIZE_16KB)
        : count_(count), alignment_(alignment) {
        size_t bytes = count * sizeof(T);
        ptr_ = static_cast<T*>(DetInt::allocate_page_aligned(bytes, alignment));
    }

    ~AlignedBuffer() {
        if (ptr_) {
            DetInt::free_page_aligned(ptr_);
        }
    }

    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    AlignedBuffer(AlignedBuffer&& other) noexcept
        : ptr_(other.ptr_), count_(other.count_), alignment_(other.alignment_) {
        other.ptr_ = nullptr;
        other.count_ = 0;
    }

    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
        if (this != &other) {
            if (ptr_) DetInt::free_page_aligned(ptr_);
            ptr_ = other.ptr_;
            count_ = other.count_;
            alignment_ = other.alignment_;
            other.ptr_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }

    T* data() { return ptr_; }
    const T* data() const { return ptr_; }
    size_t size() const { return count_; }
    T& operator[](size_t idx) { return ptr_[idx]; }
    const T& operator[](size_t idx) const { return ptr_[idx]; }

private:
    T* ptr_ = nullptr;
    size_t count_ = 0;
    size_t alignment_ = PAGE_SIZE_16KB;
};

} // namespace prism

#endif // PRISM_DETINT_HPP
