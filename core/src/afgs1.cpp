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

    // Default AR coefficients
    params.ar_coeffs_y[0] = 4;
    params.ar_coeffs_y[1] = -2;

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
        // Centered noise sample [-128..+127]
        return static_cast<int16_t>((lfsr & 0xFF) - 128);
    };

    int32_t scale = static_cast<int32_t>(params.grain_scale);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            int16_t raw_noise = next_lfsr();
            int32_t scaled_noise = (raw_noise * scale) >> 6;

            int32_t pixel = static_cast<int32_t>(in_frame[y * width + x]) + scaled_noise;
            if (pixel < 0) pixel = 0;
            if (pixel > 255) pixel = 255;

            out_frame[y * width + x] = static_cast<uint8_t>(pixel);
        }
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
    for (size_t i = 0; i < 24 && i < 24; ++i) {
        out[5 + i] = static_cast<uint8_t>(params.ar_coeffs_y[i]);
    }
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
    return params;
}

} // namespace prism
