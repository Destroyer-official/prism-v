#pragma once
#ifndef PRISM_GEN_HPP
#define PRISM_GEN_HPP

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
 * @brief Configuration parameters for Prism-Gen 2.0 (Frontier 3).
 */
struct PrismGenConfig {
    size_t width = 1920;
    size_t height = 1080;
    size_t anchor_interval = 240; // Transmit base anchor frame every 240 frames (~8s @ 30fps)
    size_t keypoint_count = 32;   // 32 3D structural facial keypoints
    int32_t fsq_levels_stage1 = 15;
    int32_t fsq_levels_stage2 = 7;
    int16_t rbf_sigma = 256;      // RBF kernel width in fixed-point 8.8
    bool enable_audio_concealment = true;
};

/**
 * @brief Dense Affine Motion Warper for Prism-Gen 2.0.
 *
 * Computes a 2D backward warping field from 32 3D structural keypoints
 * using deterministic fixed-point Gaussian Radial Basis Functions (RBF).
 */
class DenseAffineWarper {
public:
    struct WarpField {
        size_t width = 0;
        size_t height = 0;
        // Interleaved dx, dy displacement vectors in fixed-point 8.8 (int16_t)
        std::vector<int16_t> flow_grid;
    };

    /**
     * @brief Compute dense motion warping field from reference keypoints to current keypoints.
     */
    static WarpField compute_warp_field(const SemanticFrameVector& ref_semantic,
                                        const SemanticFrameVector& curr_semantic,
                                        size_t target_width,
                                        size_t target_height,
                                        int16_t rbf_sigma);

    /**
     * @brief Apply dense warp field to base anchor frame using bilinear fixed-point interpolation.
     * Guaranteed LSB-deterministic across all silicon.
     */
    static void apply_warp(const uint8_t* in_frame,
                           size_t width,
                           size_t height,
                           size_t stride,
                           const WarpField& warp,
                           uint8_t* out_warped_frame);
};

/**
 * @brief 1-Step Distilled Flow-Matching Generator (Prism-DetInt-Flow).
 *
 * Evaluates the velocity vector in a single deterministic feed-forward pass (NFE = 1).
 * Executed purely in bounded integer arithmetic (INT8 weights, INT16 activations).
 */
class PrismDetIntFlowGenerator {
public:
    static constexpr size_t HIDDEN_CHANNELS = 16;

    PrismDetIntFlowGenerator();

    /**
     * @brief Synthesize high-frequency photorealistic residual and blend with warped anchor:
     * f_recon = clamp(0, 255, f_warp + v_theta(f_warp, semantic, base_features))
     */
    void synthesize_frame(const uint8_t* warped_frame,
                          const uint8_t* base_frame,
                          size_t width,
                          size_t height,
                          const SemanticFrameVector& semantic,
                          uint8_t* out_reconstructed_frame) const;

private:
    // Fixed deterministic integer weights (INT8)
    std::vector<int8_t> weights_conv1_;
    std::vector<int8_t> weights_conv2_;
    std::vector<int8_t> weights_conv3_;
};

/**
 * @brief Audio-Assisted Packet-Loss Error Concealment (A-EC).
 *
 * Synthesizes mouth blendshapes and extrapolates 6-DoF pose from audio LPC tokens
 * when UDP keypoint packets are dropped, preventing frozen video and desync.
 */
class AudioAssistedConcealment {
public:
    AudioAssistedConcealment();

    /**
     * @brief Conceal packet loss: extrapolate pose and predict mouth blendshapes.
     */
    SemanticFrameVector conceal_lost_packet(const SemanticFrameVector& last_valid_frame,
                                            const std::array<int8_t, 8>& audio_lpc_tokens);

private:
    std::array<int16_t, 6> velocity_pose_{};
};

/**
 * @brief Master Codec Engine for Prism-Gen 2.0 (Frontier 3).
 *
 * Implements end-to-end encoding and decoding of extreme low-bitrate semantic video streams.
 */
class PrismGenCodec {
public:
    explicit PrismGenCodec(const PrismGenConfig& config);

    /**
     * @brief Encode a frame into either a base anchor flag or a 1.5 kbps semantic payload.
     * @return Byte length of written payload in out_payload.
     */
    size_t encode_frame(const uint8_t* frame_yuv,
                        size_t frame_index,
                        const SemanticFrameVector& semantic,
                        uint8_t* out_payload,
                        size_t max_payload_size,
                        bool& out_is_anchor);

    /**
     * @brief Decode semantic payload and synthesize target video frame.
     */
    bool decode_frame(const uint8_t* payload,
                      size_t payload_size,
                      const uint8_t* base_anchor_frame,
                      uint8_t* out_reconstructed_frame);

private:
    PrismGenConfig config_;
    SemanticFrameVector last_semantic_{};
    PrismDetIntFlowGenerator flow_gen_;
    AudioAssistedConcealment aec_;
    std::vector<uint8_t> cached_anchor_;
};

} // namespace prism

#endif // PRISM_GEN_HPP
