#pragma once
#ifndef PRISM_AFGS1_HPP
#define PRISM_AFGS1_HPP

#include "prism/types.hpp"
#include <cstdint>
#include <vector>
#include <array>

namespace prism {

/**
 * @brief AOMedia Film Grain Synthesis 1 (AFGS1) 14-parameter Autoregressive Model.
 * Represents parametric high-frequency noise (~160 bytes/frame) to save 12-25% bitrate.
 */
struct Afgs1Params {
    bool apply_grain = true;
    uint16_t random_seed = 0x5A5A;
    uint8_t grain_scale = 10; // Overall amplitude [0..15]
    uint8_t ar_coeff_lag = 3;  // AR lag: 0, 1, 2, or 3
    std::array<int8_t, 24> ar_coeffs_y{};  // Autoregressive coefficients for Luma
    std::array<int8_t, 25> ar_coeffs_cb{}; // Autoregressive coefficients for Cb
    std::array<int8_t, 25> ar_coeffs_cr{}; // Autoregressive coefficients for Cr
    uint8_t num_y_points = 2;
    std::array<uint8_t, 14> scaling_points_y{};
};

/**
 * @brief AFGS1 Noise Extraction & Synthesis Engine.
 */
class Afgs1Engine {
public:
    Afgs1Engine() = default;

    /**
     * @brief Estimate noise characteristics from input frame and extract structural frame.
     * @param frame_y Input noisy luma plane
     * @param clean_y Output denoised structural luma plane
     * @param width Plane width
     * @param height Plane height
     * @return 14-parameter AFGS1 model
     */
    static Afgs1Params extract_noise_and_denoise(const uint8_t* frame_y,
                                                uint8_t* clean_y,
                                                uint32_t width,
                                                uint32_t height);

    /**
     * @brief Synthesize deterministic grain on top of reconstructed frame at decoder.
     * Uses deterministic 16-bit Galois Linear Feedback Shift Register (LFSR).
     */
    static void synthesize_grain(const uint8_t* in_frame,
                                uint8_t* out_frame,
                                uint32_t width,
                                uint32_t height,
                                const Afgs1Params& params);

    /**
     * @brief Synthesize deterministic grain on full YUV420 planes (Y: WxH, Cb: W/2xH/2, Cr: W/2xH/2).
     */
    static void synthesize_grain_yuv420(const uint8_t* in_y, const uint8_t* in_u, const uint8_t* in_v,
                                        uint8_t* out_y, uint8_t* out_u, uint8_t* out_v,
                                        uint32_t width, uint32_t height,
                                        const Afgs1Params& params);

    /**
     * @brief Smooth AFGS1 parameters across adjacent temporal frames to eliminate temporal grain flickering.
     * Locks LFSR random seed for continuous shot sequences and applies IIR low-pass smoothing to grain scale.
     */
    static void smooth_temporal_grain(Afgs1Params& current_params,
                                      const Afgs1Params& prev_params,
                                      bool is_scene_cut = false);

    /**
     * @brief Serialize AFGS1 parameters to binary box payload ('afgs').
     */
    static std::vector<uint8_t> serialize_params(const Afgs1Params& params);

    /**
     * @brief Deserialize AFGS1 parameters from binary box payload.
     */
    static Afgs1Params deserialize_params(const uint8_t* data, size_t size);
};

} // namespace prism

#endif // PRISM_AFGS1_HPP
