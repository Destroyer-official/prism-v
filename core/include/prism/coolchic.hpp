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
    static constexpr size_t SERIALIZED_MODEL_SIZE = 4 + 4 + (HIDDEN_DIM * INPUT_DIM) + (HIDDEN_DIM * 4) +
                                                   (HIDDEN_DIM * HIDDEN_DIM) + (HIDDEN_DIM * 4) +
                                                   (OUTPUT_DIM * HIDDEN_DIM) + (OUTPUT_DIM * 4); // 456 bytes

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
     * @brief Synthesize and blend an entire single plane.
     * Evaluates the 336 MACs/pixel network across all coordinates.
     */
    void synthesize_frame(const int16_t* latents_plane,
                          const uint8_t* base_frame,
                          uint8_t* out_frame,
                          uint32_t width,
                          uint32_t height) const;

    /**
     * @brief Synthesize and blend an entire single plane with activity-guided block gating.
     * Evaluates neural MLP only on active blocks with high spatial frequency/edges,
     * bypassing smooth flat regions to guarantee strictly < 0.35 GMACs/frame at 1080p.
     *
     * @param latents_plane 8 INT16 latent features per pixel
     * @param base_frame Base reconstructed plane
     * @param out_frame Output blended plane
     * @param width Frame width
     * @param height Frame height
     * @param activity_threshold Block variance threshold for neural activation
     * @param out_macs Optional pointer to receive total executed MACs
     */
    void synthesize_frame_activity_guided(const int16_t* latents_plane,
                                          const uint8_t* base_frame,
                                          uint8_t* out_frame,
                                          uint32_t width,
                                          uint32_t height,
                                          uint32_t activity_threshold = 16,
                                          uint64_t* out_macs = nullptr) const;

    /**
     * @brief Synthesize and blend an entire YUV420 frame (Luma WxH, Chroma U and V (W/2)x(H/2)).
     */
    void synthesize_frame_yuv420(const int16_t* latents_plane,
                                 const uint8_t* base_y,
                                 const uint8_t* base_u,
                                 const uint8_t* base_v,
                                 uint8_t* out_y,
                                 uint8_t* out_u,
                                 uint8_t* out_v,
                                 uint32_t width,
                                 uint32_t height) const;

    /**
     * @brief Synthesize and blend an entire YUV420 frame with activity-guided block gating.
     * Evaluates neural MLP only on active blocks, bypassing smooth flat regions to guarantee strictly < 0.35 GMACs/frame.
     */
    void synthesize_frame_yuv420_activity_guided(const int16_t* latents_plane,
                                                 const uint8_t* base_y,
                                                 const uint8_t* base_u,
                                                 const uint8_t* base_v,
                                                 uint8_t* out_y,
                                                 uint8_t* out_u,
                                                 uint8_t* out_v,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 uint32_t activity_threshold = 16,
                                                 uint64_t* out_macs = nullptr) const;

    /**
     * @brief Decoupled Masked Soft-Gating Blending:
     * f_out = alpha * f_base + (1 - alpha) * f_local + residual
     */
    static uint8_t blend_pixel(uint8_t base_pixel,
                               uint8_t local_avg,
                               int16_t residual,
                               uint8_t alpha);

    /**
     * @brief Serialize model weights and biases to binary payload.
     */
    std::vector<uint8_t> serialize_model() const;

    /**
     * @brief Deserialize model weights and biases from binary payload.
     */
    bool deserialize_model(const uint8_t* data, size_t size);

    /**
     * @brief Direct weight accessor and mutator.
     */
    void set_weights(const int8_t* w1, const int32_t* b1,
                     const int8_t* w2, const int32_t* b2,
                     const int8_t* w3, const int32_t* b3);

    const auto& weights_l1() const { return w1_; }
    const auto& bias_l1() const { return b1_; }
    const auto& weights_l2() const { return w2_; }
    const auto& bias_l2() const { return b2_; }
    const auto& weights_l3() const { return w3_; }
    const auto& bias_l3() const { return b3_; }

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
 * @brief 4-Target Pareto Dial Rate-Distortion Solver & Tri-Metric Consensus Controller.
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
        float predicted_cambi;
        float predicted_cvvdp_jod;
    };

    struct TriMetricConfig {
        float lambda_vmaf = 1.0f;
        float lambda_cvvdp = 1.5f;
        float lambda_cambi = 2.0f;
        float max_cambi_threshold = 2.5f;
    };

    static DialResult solve(const ParetoTarget& target,
                            uint32_t width,
                            uint32_t height,
                            uint32_t fps,
                            float duration_sec);

    static DialResult solve_tri_metric(const ParetoTarget& target,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t fps,
                                       float duration_sec,
                                       const TriMetricConfig& metric_cfg);

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
