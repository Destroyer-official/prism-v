#pragma once
#ifndef PRISM_COOLCHIC_HPP
#define PRISM_COOLCHIC_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include <cstdint>
#include <vector>
#include <array>
#include <cmath>

namespace prism {

/**
 * @brief Cool-chic 5.0 Micro-Residual Synthesis Parameters & Model.
 * Complexity ceiling: strictly <= 600 MACs / decoded pixel.
 * Quantized to INT8 weights and INT16 activations.
 */
class CoolchicResidualModel {
public:
    static constexpr size_t LATENT_CHANNELS = 8;
    static constexpr size_t BASE_FEATURE_CHANNELS = 4;
    static constexpr size_t INPUT_DIM = LATENT_CHANNELS + BASE_FEATURE_CHANNELS; // 12
    static constexpr size_t HIDDEN_DIM = 12;
    static constexpr size_t OUTPUT_DIM = 4; // 3 residual channels (Y, U, V) + 1 alpha gating mask

    CoolchicResidualModel();

    /**
     * @brief Synthesize residual and alpha-gating mask for a single pixel.
     * Guaranteed <= 600 MACs/pixel.
     * (12x12 = 144 MACs + 12x12 = 144 MACs + 12x4 = 48 MACs = 336 MACs/pixel!).
     *
     * @param latents 8 INT16 latent features from upsampled pyramid
     * @param base_features 4 INT16 spatial edge/gradient features from base frame
     * @param out_residual Array of 3 INT16 residual deltas [-128, +127]
     * @param out_alpha Soft gating mask in [0, 256] where 256 = 1.0 (pure base) and 0 = 0.0 (block suppressed)
     */
    void synthesize_pixel(const int16_t* latents,
                          const int16_t* base_features,
                          int16_t* out_residual,
                          uint8_t& out_alpha) const;

    /**
     * @brief Decoupled Masked Soft-Gating Blending:
     * f_out = alpha * f_base + (1 - alpha) * f_local + residual
     */
    static uint8_t blend_pixel(uint8_t base_pixel,
                               uint8_t local_avg,
                               int16_t residual,
                               uint8_t alpha);

private:
    std::array<int8_t, HIDDEN_DIM * INPUT_DIM> w1_{};  // 12 x 12
    std::array<int32_t, HIDDEN_DIM> b1_{};             // 12
    std::array<int8_t, HIDDEN_DIM * HIDDEN_DIM> w2_{}; // 12 x 12
    std::array<int32_t, HIDDEN_DIM> b2_{};             // 12
    std::array<int8_t, OUTPUT_DIM * HIDDEN_DIM> w3_{}; // 4 x 12
    std::array<int32_t, OUTPUT_DIM> b3_{};             // 4

    void init_default_weights();
};

/**
 * @brief 4-Target Pareto Dial Rate-Distortion Solver.
 */
class ParetoDialSolver {
public:
    struct DialResult {
        uint32_t target_bitrate_kbps;
        uint8_t base_qp;
        bool enable_micro_residual;
        bool enable_afgs1_grain;
        float predicted_vmaf;
        float predicted_energy_joules_per_frame;
    };

    static DialResult solve(const ParetoTarget& target,
                            uint32_t width,
                            uint32_t height,
                            uint32_t fps,
                            float duration_sec);

    /**
     * @brief 0.3-second Preflight Pareto Probe:
     * Samples up to 5 patches of 256x256 to adjust a heuristic VMAF estimate.
     * The estimate is not a measured VMAF value and has no accuracy guarantee.
     */
    static DialResult run_preflight_probe(const uint8_t* frame_y,
                                         uint32_t width,
                                         uint32_t height,
                                         const ParetoTarget& target);
};

} // namespace prism

#endif // PRISM_COOLCHIC_HPP
