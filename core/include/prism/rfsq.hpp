#pragma once
#ifndef PRISM_RFSQ_HPP
#define PRISM_RFSQ_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace prism {

/**
 * @brief Frontier 3: Bounded Integer-Scaled Residual Finite Scalar Quantization (BIS-RFSQ).
 * 
 * Replaces floating-point latents with integer lattice grids, guaranteeing
 * mathematically bit-exact generative video reconstruction across all mobile NPUs/GPUs.
 */
class BisRfsq {
public:
    static constexpr size_t DEFAULT_CHANNELS = 8;
    static constexpr int32_t Q_SCALE_DYADIC = 16384; // 2^14 dyadic fixed-point base

    // Pre-computed 512-entry reciprocal square root table for integer LayerNorm
    static const std::array<uint16_t, 512> ISQRT_LUT_512;

    /**
     * @brief Computes deterministic integer LayerNorm over vector x.
     * Maps variance to reciprocal square root via static 512-entry LUT.
     */
    static void integer_layernorm(const int16_t* in, int16_t* out, size_t dim);

    /**
     * @brief Quantize a single scalar value in [-32767, 32767] to FSQ grid of L levels:
     * level in [-(L-1)/2, +(L-1)/2].
     */
    static int8_t quantize_scalar(int16_t val, int32_t num_levels);

    /**
     * @brief Dequantize a scalar level back to INT16 activation:
     * val = clamp([-32767, 32767], (level * step_m) >> shift).
     */
    static int16_t dequantize_scalar(int8_t level, int32_t num_levels);

    /**
     * @brief Two-stage Residual FSQ encoding of a latent vector.
     * Stage 1: Coarse FSQ.
     * Stage 2: Integer LayerNorm of residual + Fine FSQ.
     */
    static void encode_vector(const int16_t* latents,
                              size_t dim,
                              int32_t levels_stage1,
                              int32_t levels_stage2,
                              int8_t* out_codes_stage1,
                              int8_t* out_codes_stage2);

    /**
     * @brief Two-stage Residual FSQ decoding back to bit-exact INT16 latents.
     */
    static void decode_vector(const int8_t* codes_stage1,
                              const int8_t* codes_stage2,
                              size_t dim,
                              int32_t levels_stage1,
                              int32_t levels_stage2,
                              int16_t* out_latents);

    /**
     * @brief Batch encode an entire 2D grid of latent vectors.
     */
    static void encode_grid(const int16_t* latents,
                            size_t width,
                            size_t height,
                            size_t channels,
                            int32_t levels_stage1,
                            int32_t levels_stage2,
                            int8_t* out_codes_stage1,
                            int8_t* out_codes_stage2);

    /**
     * @brief Batch decode an entire 2D grid of latent vectors.
     */
    static void decode_grid(const int8_t* codes_stage1,
                            const int8_t* codes_stage2,
                            size_t width,
                            size_t height,
                            size_t channels,
                            int32_t levels_stage1,
                            int32_t levels_stage2,
                            int16_t* out_latents);
};

/**
 * @brief Semantic Frame Vector for Prism-Gen 2.0 (Frontier 3).
 * Represents 3D facial skeletal structure, rigid pose, blendshapes, and audio cues.
 */
struct SemanticFrameVector {
    static constexpr size_t KEYPOINT_COUNT = 32;
    static constexpr size_t BLENDSHAPE_COUNT = 52;
    static constexpr size_t AUDIO_TOKEN_COUNT = 8;

    // 32 3D structural keypoints (X, Y, Z coordinates in normalized [-32767, +32767])
    std::array<int16_t, KEYPOINT_COUNT * 3> keypoints_3d{};

    // 6-DoF rigid head pose: rx, ry, rz (Euler angles) + tx, ty, tz (translations)
    std::array<int16_t, 6> pose_6dof{};

    // 52 blendshape activation weights in [0, 255]
    std::array<uint8_t, BLENDSHAPE_COUNT> blendshapes{};

    // 8 audio LPC / acoustic formant alignment tokens
    std::array<int8_t, AUDIO_TOKEN_COUNT> audio_tokens{};

    // Flags: bit 0: is_anchor_frame, bit 1: gaze_valid, bit 2: packet_loss_concealment
    uint8_t flags{0};
};

/**
 * @brief Ultra-low bitrate differential predictive codec for semantic streams.
 * Compresses semantic vectors to ~1.5 kbps (<= 50 bytes/frame @ 30 fps).
 */
class SemanticDeltaCodec {
public:
    /**
     * @brief Encode delta between current frame and previous frame.
     * Returns compressed payload size in bytes (guaranteed <= 64 bytes).
     */
    static size_t encode_delta(const SemanticFrameVector& current,
                               const SemanticFrameVector& reference,
                               uint8_t* out_payload,
                               size_t max_payload_size);

    /**
     * @brief Decode delta payload and reconstruct current frame from reference.
     */
    static bool decode_delta(const uint8_t* payload,
                             size_t payload_size,
                             const SemanticFrameVector& reference,
                             SemanticFrameVector& out_current);

    /**
     * @brief Encode an entire GOP sequence of semantic frame vectors into a single binary payload.
     * Starts with an anchor keyframe, followed by predictive delta frames.
     */
    static size_t encode_sequence(const std::vector<SemanticFrameVector>& sequence,
                                  uint8_t* out_payload,
                                  size_t max_payload_size);

    /**
     * @brief Decode a binary sequence payload back into a sequence of semantic frame vectors.
     */
    static bool decode_sequence(const uint8_t* payload,
                                size_t payload_size,
                                std::vector<SemanticFrameVector>& out_sequence);
};

} // namespace prism

#endif // PRISM_RFSQ_HPP
