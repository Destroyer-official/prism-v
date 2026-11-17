#include "prism/afgs1.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace prism {

Afgs1Params Afgs1Engine::extract_noise_and_denoise(const uint8_t* frame_y,
                                                  uint8_t* clean_y,
                                                  uint32_t width,
                                                  uint32_t height) {
    Afgs1Params params;
    params.apply_grain = true;
    params.random_seed = 0xABCD;
    params.grain_scale = 8;
    params.ar_coeff_lag = 1;

    // Spatial noise extraction: 3x3 bilateral/guided filter
    // Separates temporally uncorrelated sensor noise from sharp structural edges
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                clean_y[y * width + x] = frame_y[y * width + x];
                continue;
            }

            uint32_t center = frame_y[y * width + x];
            uint32_t sum = 0;
            uint32_t weight_sum = 0;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    uint32_t neighbor = frame_y[(y + dy) * width + (x + dx)];
                    int diff = static_cast<int>(neighbor) - static_cast<int>(center);
                    int abs_diff = diff < 0 ? -diff : diff;

                    // Edge-preserving photometric weight: if diff > threshold, do not smooth (preserve edge)
                    uint32_t weight = (abs_diff < 12) ? 4 : (abs_diff < 24 ? 1 : 0);
                    if (dx == 0 && dy == 0) weight += 8;

                    sum += neighbor * weight;
                    weight_sum += weight;
                }
            }

            clean_y[y * width + x] = static_cast<uint8_t>((sum + (weight_sum / 2)) / weight_sum);
        }
    }

    // Estimate noise statistics: variance across intensity ranges
    int64_t noise_var_shadow = 0, count_shadow = 0;
    int64_t noise_var_mid = 0, count_mid = 0;
    int64_t noise_var_high = 0, count_high = 0;

    int64_t r_dx_sum = 0, r_dy_sum = 0, r_center_sum = 0;

    for (uint32_t y = 1; y < height - 1; ++y) {
        for (uint32_t x = 1; x < width - 1; ++x) {
            int32_t noise = static_cast<int32_t>(frame_y[y * width + x]) - static_cast<int32_t>(clean_y[y * width + x]);
            int32_t noise_sq = noise * noise;
            uint8_t intensity = clean_y[y * width + x];

            if (intensity < 85) {
                noise_var_shadow += noise_sq;
                ++count_shadow;
            } else if (intensity < 170) {
                noise_var_mid += noise_sq;
                ++count_mid;
            } else {
                noise_var_high += noise_sq;
                ++count_high;
            }

            // Spatial autocorrelation for AR coefficients
            int32_t noise_left = static_cast<int32_t>(frame_y[y * width + (x - 1)]) - static_cast<int32_t>(clean_y[y * width + (x - 1)]);
            int32_t noise_up = static_cast<int32_t>(frame_y[(y - 1) * width + x]) - static_cast<int32_t>(clean_y[(y - 1) * width + x]);
            r_center_sum += noise_sq;
            r_dx_sum += noise * noise_left;
            r_dy_sum += noise * noise_up;
        }
    }

    // Populate AR coefficients from spatial correlation
    if (r_center_sum > 100) {
        int32_t a0 = static_cast<int32_t>((r_dx_sum * 16) / r_center_sum);
        int32_t a1 = static_cast<int32_t>((r_dy_sum * 16) / r_center_sum);
        params.ar_coeffs_y[0] = static_cast<int8_t>(std::clamp(a0, -16, 16));
        params.ar_coeffs_y[1] = static_cast<int8_t>(std::clamp(a1, -16, 16));
    } else {
        params.ar_coeffs_y[0] = 4;
        params.ar_coeffs_y[1] = -2;
    }
    params.ar_coeffs_cb[0] = 3;
    params.ar_coeffs_cb[1] = -1;
    params.ar_coeffs_cr[0] = 3;
    params.ar_coeffs_cr[1] = -1;

    // Populate piecewise scaling points
    params.num_y_points = 3;
    uint8_t scale_shadow = static_cast<uint8_t>(std::clamp(count_shadow > 0 ? (noise_var_shadow / count_shadow) : 8, int64_t(0), int64_t(64)));
    uint8_t scale_mid = static_cast<uint8_t>(std::clamp(count_mid > 0 ? (noise_var_mid / count_mid) : 6, int64_t(0), int64_t(64)));
    uint8_t scale_high = static_cast<uint8_t>(std::clamp(count_high > 0 ? (noise_var_high / count_high) : 4, int64_t(0), int64_t(64)));

    params.scaling_points_y[0] = 32;  // x coordinate 1
    params.scaling_points_y[1] = scale_shadow; // y value 1
    params.scaling_points_y[2] = 128; // x coordinate 2
    params.scaling_points_y[3] = scale_mid;    // y value 2
    params.scaling_points_y[4] = 224; // x coordinate 3
    params.scaling_points_y[5] = scale_high;   // y value 3

    return params;
}

void Afgs1Engine::synthesize_grain(const uint8_t* in_frame,
                                  uint8_t* out_frame,
                                  uint32_t width,
                                  uint32_t height,
                                  const Afgs1Params& params) {
    if (!params.apply_grain || params.grain_scale == 0) {
        if (in_frame != out_frame) {
            std::memcpy(out_frame, in_frame, width * height);
        }
        return;
    }

    // Deterministic 16-bit Galois LFSR pseudo-random generator
    // Polynomial: x^16 + x^14 + x^13 + x^11 + 1 (0xB400)
    uint16_t lfsr = (params.random_seed != 0) ? params.random_seed : 0xACE1;

    auto next_lfsr = [&lfsr]() -> int16_t {
        uint16_t lsb = lfsr & 1;
        lfsr >>= 1;
        if (lsb) {
            lfsr ^= 0xB400u;
        }
        return static_cast<int16_t>((lfsr & 0xFF) - 128);
    };

    int32_t global_scale = static_cast<int32_t>(params.grain_scale);
    int8_t ar0 = (params.ar_coeff_lag > 0) ? params.ar_coeffs_y[0] : 0;
    int8_t ar1 = (params.ar_coeff_lag > 0) ? params.ar_coeffs_y[1] : 0;

    std::vector<int16_t> prev_row(width, 0);

    for (uint32_t y = 0; y < height; ++y) {
        int16_t prev_grain_left = 0;

        for (uint32_t x = 0; x < width; ++x) {
            int16_t raw_noise = next_lfsr();

            // 2D Autoregressive (AR) colored noise synthesis
            int32_t ar_filter = (static_cast<int32_t>(ar0) * prev_grain_left + 
                                 static_cast<int32_t>(ar1) * prev_row[x]) >> 5;
            int16_t grain_sample = static_cast<int16_t>(std::clamp(raw_noise + ar_filter, -256, 255));

            prev_grain_left = grain_sample;
            prev_row[x] = grain_sample;

            // Intensity-dependent piecewise scaling
            uint8_t px = in_frame[y * width + x];
            int32_t local_scale = global_scale;

            if (params.num_y_points >= 2) {
                // Piecewise linear interpolation
                if (px <= params.scaling_points_y[0]) {
                    local_scale = (global_scale * params.scaling_points_y[1] + 8) >> 4;
                } else if (px >= params.scaling_points_y[4]) {
                    local_scale = (global_scale * params.scaling_points_y[5] + 8) >> 4;
                } else {
                    local_scale = (global_scale * params.scaling_points_y[3] + 8) >> 4;
                }
                if (local_scale < 1) local_scale = 1;
            }

            int32_t scaled_noise = (static_cast<int32_t>(grain_sample) * local_scale) >> 6;
            int32_t pixel = static_cast<int32_t>(px) + scaled_noise;
            out_frame[y * width + x] = static_cast<uint8_t>(std::clamp(pixel, 0, 255));
        }
    }
}

void Afgs1Engine::synthesize_grain_yuv420(const uint8_t* in_y, const uint8_t* in_u, const uint8_t* in_v,
                                         uint8_t* out_y, uint8_t* out_u, uint8_t* out_v,
                                         uint32_t width, uint32_t height,
                                         const Afgs1Params& params) {
    if (!in_y || !in_u || !in_v || !out_y || !out_u || !out_v) return;

    // 1. Synthesize Luma (Y)
    synthesize_grain(in_y, out_y, width, height, params);

    const uint32_t uv_w = width / 2;
    const uint32_t uv_h = height / 2;

    if (!params.apply_grain || params.grain_scale == 0 || uv_w == 0 || uv_h == 0) {
        if (in_u != out_u) std::memcpy(out_u, in_u, uv_w * uv_h);
        if (in_v != out_v) std::memcpy(out_v, in_v, uv_w * uv_h);
        return;
    }

    // 2. Synthesize Chroma Cb (U) with seed decorrelation
    uint16_t lfsr_cb = static_cast<uint16_t>(params.random_seed ^ 0x3C6Eu);
    if (lfsr_cb == 0) lfsr_cb = 0x5A5A;
    auto next_lfsr_cb = [&lfsr_cb]() -> int16_t {
        uint16_t lsb = lfsr_cb & 1;
        lfsr_cb >>= 1;
        if (lsb) lfsr_cb ^= 0xB400u;
        return static_cast<int16_t>((lfsr_cb & 0xFF) - 128);
    };

    int32_t global_scale = static_cast<int32_t>(params.grain_scale);
    int8_t ar0_cb = (params.ar_coeff_lag > 0) ? params.ar_coeffs_cb[0] : 0;
    int8_t ar1_cb = (params.ar_coeff_lag > 0) ? params.ar_coeffs_cb[1] : 0;

    std::vector<int16_t> prev_row_cb(uv_w, 0);
    for (uint32_t y = 0; y < uv_h; ++y) {
        int16_t prev_grain_left = 0;
        for (uint32_t x = 0; x < uv_w; ++x) {
            int16_t raw_noise = next_lfsr_cb();
            int32_t ar_filter = (static_cast<int32_t>(ar0_cb) * prev_grain_left +
                                 static_cast<int32_t>(ar1_cb) * prev_row_cb[x]) >> 5;
            int16_t grain_sample = static_cast<int16_t>(std::clamp(raw_noise + ar_filter, -256, 255));
            prev_grain_left = grain_sample;
            prev_row_cb[x] = grain_sample;

            uint8_t px = in_u[y * uv_w + x];
            int32_t dist_neutral = std::abs(static_cast<int32_t>(px) - 128);
            int32_t local_scale = (global_scale * std::max(0, 128 - dist_neutral) + 64) >> 7;
            int32_t scaled_noise = (static_cast<int32_t>(grain_sample) * local_scale) >> 6;
            out_u[y * uv_w + x] = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(px) + scaled_noise, 0, 255));
        }
    }

    // 3. Synthesize Chroma Cr (V) with seed decorrelation
    uint16_t lfsr_cr = static_cast<uint16_t>(params.random_seed ^ 0xC391u);
    if (lfsr_cr == 0) lfsr_cr = 0x9696;
    auto next_lfsr_cr = [&lfsr_cr]() -> int16_t {
        uint16_t lsb = lfsr_cr & 1;
        lfsr_cr >>= 1;
        if (lsb) lfsr_cr ^= 0xB400u;
        return static_cast<int16_t>((lfsr_cr & 0xFF) - 128);
    };

    int8_t ar0_cr = (params.ar_coeff_lag > 0) ? params.ar_coeffs_cr[0] : 0;
    int8_t ar1_cr = (params.ar_coeff_lag > 0) ? params.ar_coeffs_cr[1] : 0;

    std::vector<int16_t> prev_row_cr(uv_w, 0);
    for (uint32_t y = 0; y < uv_h; ++y) {
        int16_t prev_grain_left = 0;
        for (uint32_t x = 0; x < uv_w; ++x) {
            int16_t raw_noise = next_lfsr_cr();
            int32_t ar_filter = (static_cast<int32_t>(ar0_cr) * prev_grain_left +
                                 static_cast<int32_t>(ar1_cr) * prev_row_cr[x]) >> 5;
            int16_t grain_sample = static_cast<int16_t>(std::clamp(raw_noise + ar_filter, -256, 255));
            prev_grain_left = grain_sample;
            prev_row_cr[x] = grain_sample;

            uint8_t px = in_v[y * uv_w + x];
            int32_t dist_neutral = std::abs(static_cast<int32_t>(px) - 128);
            int32_t local_scale = (global_scale * std::max(0, 128 - dist_neutral) + 64) >> 7;
            int32_t scaled_noise = (static_cast<int32_t>(grain_sample) * local_scale) >> 6;
            out_v[y * uv_w + x] = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(px) + scaled_noise, 0, 255));
        }
    }
}

void Afgs1Engine::smooth_temporal_grain(Afgs1Params& current_params,
                                        const Afgs1Params& prev_params,
                                        bool is_scene_cut) {
    if (is_scene_cut || !prev_params.apply_grain) {
        return;
    }

    // 1. Smooth grain scale via integer IIR filter: (3 * prev + 1 * curr + 2) / 4
    uint32_t smoothed_scale = (static_cast<uint32_t>(prev_params.grain_scale) * 3 +
                               static_cast<uint32_t>(current_params.grain_scale) + 2) >> 2;
    current_params.grain_scale = static_cast<uint8_t>(std::clamp<uint32_t>(smoothed_scale, 1, 15));

    // 2. Deterministic continuous pseudo-random seed progression:
    // Coherent Galois step across frames eliminates boiling/strobing grain flicker
    uint16_t seed = prev_params.random_seed;
    uint16_t lsb = seed & 1;
    seed >>= 1;
    if (lsb) seed ^= 0xB400u;
    current_params.random_seed = (seed != 0) ? seed : 0xACE1;

    // 3. Smooth primary AR coefficients to prevent sudden spatial grain direction shifts
    for (size_t i = 0; i < 2; ++i) {
        int32_t ar = (static_cast<int32_t>(prev_params.ar_coeffs_y[i]) * 3 +
                      static_cast<int32_t>(current_params.ar_coeffs_y[i]) + 2) >> 2;
        current_params.ar_coeffs_y[i] = static_cast<int8_t>(std::clamp(ar, -16, 16));
    }
}

std::vector<uint8_t> Afgs1Engine::serialize_params(const Afgs1Params& params) {
    // 32-byte compact binary representation
    std::vector<uint8_t> out(32, 0);
    out[0] = params.apply_grain ? 1 : 0;
    out[1] = static_cast<uint8_t>((params.random_seed >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>(params.random_seed & 0xFF);
    out[3] = params.grain_scale;
    out[4] = params.ar_coeff_lag;
    for (size_t i = 0; i < 24; ++i) {
        out[5 + i] = static_cast<uint8_t>(params.ar_coeffs_y[i]);
    }
    out[29] = params.num_y_points;
    out[30] = params.scaling_points_y[0];
    out[31] = params.scaling_points_y[1];
    return out;
}

Afgs1Params Afgs1Engine::deserialize_params(const uint8_t* data, size_t size) {
    Afgs1Params params;
    if (size < 30) return params;

    params.apply_grain = (data[0] != 0);
    params.random_seed = (static_cast<uint16_t>(data[1]) << 8) | data[2];
    params.grain_scale = data[3];
    params.ar_coeff_lag = data[4];
    for (size_t i = 0; i < 24; ++i) {
        params.ar_coeffs_y[i] = static_cast<int8_t>(data[5 + i]);
    }
    if (size >= 32) {
        params.num_y_points = data[29];
        params.scaling_points_y[0] = data[30];
        params.scaling_points_y[1] = data[31];
    }
    return params;
}

} // namespace prism
