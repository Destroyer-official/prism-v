#pragma once
#ifndef PRISM_NERV_ENGINE_HPP
#define PRISM_NERV_ENGINE_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/rfsq.hpp"
#include "prism/rans8.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <memory>

namespace prism {

/**
 * @brief Configuration for NeRV-S / WM-IVR (Wavelet-Modulated Implicit Video Representation).
 */
struct NeRVSConfig {
    size_t width = 1920;
    size_t height = 1080;
    size_t gop_frames = 60;          // Continuous temporal span per GOP
    size_t triplane_channels = 8;    // 8-channel latent tri-plane lattice
    size_t mlp_hidden_dim = 16;      // Hidden dimension of coordinate MLP
    size_t spatial_downscale = 4;    // 4x spatial subsampling for plane grids
    int32_t target_bpp_x1000 = 15;   // Target 0.015 bpp (ultra-low bitrate)
};

/**
 * @brief Continuous Tri-Plane Spatio-Temporal Feature Lattice.
 *
 * Decomposes continuous video GOP into orthogonal coordinate planes:
 * P_xy (spatial morphology), P_xt (horizontal motion), P_yt (vertical motion).
 */
class TriPlaneLattice {
public:
    TriPlaneLattice(size_t width, size_t height, size_t frames, size_t channels, size_t downscale);

    /**
     * @brief Sample continuous coordinate (t, x, y) via deterministic integer bilinear interpolation
     * and compute Hadamard channel-wise product: F = P_xy * P_xt * P_yt.
     */
    void sample_feature(int32_t t_norm, // [0, 32767] normalized time
                        int32_t x_norm, // [0, 32767] normalized x
                        int32_t y_norm, // [0, 32767] normalized y
                        int16_t* out_feature) const;

    // Direct access to plane buffers for quantization and entropy coding
    std::vector<int16_t>& plane_xy() { return p_xy_; }
    std::vector<int16_t>& plane_xt() { return p_xt_; }
    std::vector<int16_t>& plane_yt() { return p_yt_; }

    const std::vector<int16_t>& plane_xy() const { return p_xy_; }
    const std::vector<int16_t>& plane_xt() const { return p_xt_; }
    const std::vector<int16_t>& plane_yt() const { return p_yt_; }

    size_t plane_xy_size() const { return pw_ * ph_ * channels_; }
    size_t plane_xt_size() const { return pw_ * frames_ * channels_; }
    size_t plane_yt_size() const { return ph_ * frames_ * channels_; }

private:
    size_t pw_ = 0;
    size_t ph_ = 0;
    size_t frames_ = 0;
    size_t channels_ = 0;

    std::vector<int16_t> p_xy_;
    std::vector<int16_t> p_xt_;
    std::vector<int16_t> p_yt_;
};

/**
 * @brief Ultra-compact 3-Layer Continuous Coordinate Synthesis MLP.
 *
 * Complexity: exactly 432 MACs per synthesized pixel (8x16 + 16x16 + 16x3).
 * Bound to Prism-DetInt integer determinism (INT8 weights, INT16 activations).
 */
class NeRVSynthesisMLP {
public:
    static constexpr size_t INPUT_DIM = 8;
    static constexpr size_t HIDDEN_DIM = 16;
    static constexpr size_t OUTPUT_DIM = 3; // Y, U, V residual channels

    NeRVSynthesisMLP();

    /**
     * @brief Synthesize continuous Y, U, V residual for a coordinate feature.
     */
    void synthesize(const int16_t* feature, int16_t* out_yuv_residual) const;

    // Weight accessors for Hessian bit allocation
    std::array<int8_t, HIDDEN_DIM * INPUT_DIM>& weights_l1() { return w1_; }
    std::array<int8_t, HIDDEN_DIM * HIDDEN_DIM>& weights_l2() { return w2_; }
    std::array<int8_t, OUTPUT_DIM * HIDDEN_DIM>& weights_l3() { return w3_; }

    const std::array<int8_t, HIDDEN_DIM * INPUT_DIM>& weights_l1() const { return w1_; }
    const std::array<int8_t, HIDDEN_DIM * HIDDEN_DIM>& weights_l2() const { return w2_; }
    const std::array<int8_t, OUTPUT_DIM * HIDDEN_DIM>& weights_l3() const { return w3_; }

private:
    std::array<int8_t, HIDDEN_DIM * INPUT_DIM> w1_{};   // 16 x 8
    std::array<int32_t, HIDDEN_DIM> b1_{};              // 16
    std::array<int8_t, HIDDEN_DIM * HIDDEN_DIM> w2_{};  // 16 x 16
    std::array<int32_t, HIDDEN_DIM> b2_{};              // 16
    std::array<int8_t, OUTPUT_DIM * HIDDEN_DIM> w3_{};  // 3 x 16
    std::array<int32_t, OUTPUT_DIM> b3_{};              // 3
};

/**
 * @brief Diagonal Hessian-Weighted Bit Allocation Engine.
 *
 * Analyzes loss landscape curvature H_ii = E[(d L / d theta_i)^2]
 * and dynamically allocates mixed-precision FSQ bit-widths (2, 4, 8 bits).
 */
class HessianBitAllocator {
public:
    /**
     * @brief Compute optimal bit allocation vector across model parameters.
     */
    static std::vector<uint8_t> allocate_bits(const std::vector<int32_t>& diagonal_hessian,
                                              uint8_t target_avg_bits);

    /**
     * @brief Quantize a vector of weights according to per-parameter bit allocation.
     */
    static void quantize_with_allocation(const int16_t* weights,
                                         const std::vector<uint8_t>& bit_alloc,
                                         size_t count,
                                         int8_t* out_quantized);
};

/**
 * @brief Master NeRV-S Video Representation Engine.
 */
class NeRVCodec {
public:
    explicit NeRVCodec(const NeRVSConfig& config);

    /**
     * @brief Serialize overfitted TriPlane lattice and MLP weights into compressed payload
     * using 8-way SIMD interleaved rANS.
     */
    size_t serialize_gop(uint8_t* out_payload, size_t max_payload_size);

    /**
     * @brief Deserialize GOP payload and restore continuous coordinate fields.
     */
    bool deserialize_gop(const uint8_t* payload, size_t payload_size);

    /**
     * @brief Render continuous video frame at arbitrary temporal coordinate t in [0, 1].
     */
    void render_frame(float t_continuous,
                      const uint8_t* base_frame,
                      uint8_t* out_frame);

    TriPlaneLattice& lattice() { return lattice_; }
    NeRVSynthesisMLP& mlp() { return mlp_; }

private:
    NeRVSConfig config_;
    TriPlaneLattice lattice_;
    NeRVSynthesisMLP mlp_;
};

} // namespace prism

#endif // PRISM_NERV_ENGINE_HPP
