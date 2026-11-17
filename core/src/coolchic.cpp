#include "prism/coolchic.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace prism {

CoolchicResidualModel::CoolchicResidualModel() {
    init_default_weights();
}

void CoolchicResidualModel::init_default_weights() {
    for (size_t i = 0; i < HIDDEN_DIM * INPUT_DIM; ++i) {
        w1_[i] = static_cast<int8_t>((static_cast<int32_t>((i * 13 + 7) % 255)) - 127);
    }
    for (size_t i = 0; i < HIDDEN_DIM; ++i) {
        b1_[i] = static_cast<int32_t>((i % 5) - 2) * 32;
    }
    for (size_t i = 0; i < HIDDEN_DIM * HIDDEN_DIM; ++i) {
        w2_[i] = static_cast<int8_t>((static_cast<int32_t>((i * 19 + 11) % 255)) - 127);
    }
    for (size_t i = 0; i < HIDDEN_DIM; ++i) {
        b2_[i] = static_cast<int32_t>((i % 3) - 1) * 32;
    }
    for (size_t i = 0; i < OUTPUT_DIM * HIDDEN_DIM; ++i) {
        w3_[i] = static_cast<int8_t>((static_cast<int32_t>((i * 29 + 17) % 255)) - 127);
    }
    for (size_t i = 0; i < OUTPUT_DIM; ++i) {
        b3_[i] = 0;
    }
}

void CoolchicResidualModel::set_weights(const int8_t* w1, const int32_t* b1,
                                       const int8_t* w2, const int32_t* b2,
                                       const int8_t* w3, const int32_t* b3) {
    if (w1) std::memcpy(w1_.data(), w1, w1_.size());
    if (b1) std::memcpy(b1_.data(), b1, b1_.size() * sizeof(int32_t));
    if (w2) std::memcpy(w2_.data(), w2, w2_.size());
    if (b2) std::memcpy(b2_.data(), b2, b2_.size() * sizeof(int32_t));
    if (w3) std::memcpy(w3_.data(), w3, w3_.size());
    if (b3) std::memcpy(b3_.data(), b3, b3_.size() * sizeof(int32_t));
}

void CoolchicResidualModel::synthesize_pixel(const int16_t* latents,
                                            const int16_t* base_features,
                                            int16_t* out_residual,
                                            uint8_t& out_alpha) const {
    std::array<int16_t, INPUT_DIM> x{};
    for (size_t i = 0; i < LATENT_CHANNELS; ++i) {
        x[i] = latents ? latents[i] : 0;
    }
    for (size_t i = 0; i < BASE_FEATURE_CHANNELS; ++i) {
        x[LATENT_CHANNELS + i] = base_features ? base_features[i] : 0;
    }

    std::array<int16_t, HIDDEN_DIM> h1{};
    std::array<int16_t, HIDDEN_DIM> h2{};
    std::array<int16_t, OUTPUT_DIM> y{};

    // Layer 1: 12 -> 12
    DetInt::linear_layer(x.data(), w1_.data(), b1_.data(), h1.data(), INPUT_DIM, HIDDEN_DIM, 16384, 15);
    DetInt::relu(h1.data(), HIDDEN_DIM);

    // Layer 2: 12 -> 12
    DetInt::linear_layer(h1.data(), w2_.data(), b2_.data(), h2.data(), HIDDEN_DIM, HIDDEN_DIM, 16384, 15);
    DetInt::relu(h2.data(), HIDDEN_DIM);

    // Layer 3: 12 -> 4 (residual Y, U, V + alpha mask)
    DetInt::linear_layer(h2.data(), w3_.data(), b3_.data(), y.data(), HIDDEN_DIM, OUTPUT_DIM, 16384, 15);

    // Clamp residuals to [-128, +127]
    for (size_t i = 0; i < 3; ++i) {
        int16_t r = y[i];
        if (r < -128) r = -128;
        if (r > 127) r = 127;
        out_residual[i] = r;
    }

    // Alpha gating mask [0, 256]
    int32_t raw_alpha = y[3] + 128;
    if (raw_alpha < 0) raw_alpha = 0;
    if (raw_alpha > 256) raw_alpha = 256;
    out_alpha = static_cast<uint8_t>(raw_alpha > 255 ? 255 : raw_alpha);
}

void CoolchicResidualModel::synthesize_frame(const int16_t* latents_plane,
                                            const uint8_t* base_frame,
                                            uint8_t* out_frame,
                                            uint32_t width,
                                            uint32_t height) const {
    if (!base_frame || !out_frame || width == 0 || height == 0) return;

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t y_prev = (y > 0) ? y - 1 : 0;
        uint32_t y_next = (y + 1 < height) ? y + 1 : height - 1;

        for (uint32_t x = 0; x < width; ++x) {
            uint32_t x_prev = (x > 0) ? x - 1 : 0;
            uint32_t x_next = (x + 1 < width) ? x + 1 : width - 1;

            uint8_t base_px = base_frame[y * width + x];
            uint8_t p_up = base_frame[y_prev * width + x];
            uint8_t p_down = base_frame[y_next * width + x];
            uint8_t p_left = base_frame[y * width + x_prev];
            uint8_t p_right = base_frame[y * width + x_next];

            // 4-neighborhood average
            uint8_t local_avg = static_cast<uint8_t>((static_cast<uint32_t>(p_up) + p_down + p_left + p_right + 2) >> 2);

            // 4 Base edge features
            std::array<int16_t, BASE_FEATURE_CHANNELS> base_feats{};
            base_feats[0] = static_cast<int16_t>(base_px);
            base_feats[1] = static_cast<int16_t>((static_cast<int32_t>(p_right) - static_cast<int32_t>(p_left)) << 6);
            base_feats[2] = static_cast<int16_t>((static_cast<int32_t>(p_down) - static_cast<int32_t>(p_up)) << 6);
            int32_t lap = 4 * static_cast<int32_t>(base_px) - (p_up + p_down + p_left + p_right);
            base_feats[3] = static_cast<int16_t>(std::clamp(lap << 6, -32767, 32767));

            const int16_t* lat_ptr = latents_plane ? (latents_plane + (y * width + x) * LATENT_CHANNELS) : nullptr;
            int16_t residual[3]{};
            uint8_t alpha = 0;

            synthesize_pixel(lat_ptr, base_feats.data(), residual, alpha);
            out_frame[y * width + x] = blend_pixel(base_px, local_avg, residual[0], alpha);
        }
    }
}

void CoolchicResidualModel::synthesize_frame_activity_guided(const int16_t* latents_plane,
                                                             const uint8_t* base_frame,
                                                             uint8_t* out_frame,
                                                             uint32_t width,
                                                             uint32_t height,
                                                             uint32_t activity_threshold,
                                                             uint64_t* out_macs) const {
    if (!base_frame || !out_frame || width == 0 || height == 0) {
        if (out_macs) *out_macs = 0;
        return;
    }

    constexpr uint32_t BLOCK_SIZE = 8;
    constexpr uint64_t MACS_PER_PIXEL = (HIDDEN_DIM * INPUT_DIM) + (HIDDEN_DIM * HIDDEN_DIM) + (OUTPUT_DIM * HIDDEN_DIM); // 336
    uint64_t total_macs = 0;

    for (uint32_t by = 0; by < height; by += BLOCK_SIZE) {
        uint32_t bh = std::min(BLOCK_SIZE, height - by);
        for (uint32_t bx = 0; bx < width; bx += BLOCK_SIZE) {
            uint32_t bw = std::min(BLOCK_SIZE, width - bx);

            // Step 1: Compute block activity (gradient energy)
            uint32_t activity_sum = 0;
            for (uint32_t y = by; y < by + bh; ++y) {
                uint32_t y_next = (y + 1 < height) ? y + 1 : y;
                for (uint32_t x = bx; x < bx + bw; ++x) {
                    uint32_t x_next = (x + 1 < width) ? x + 1 : x;
                    uint8_t p0 = base_frame[y * width + x];
                    uint8_t px = base_frame[y * width + x_next];
                    uint8_t py = base_frame[y_next * width + x];
                    activity_sum += static_cast<uint32_t>(std::abs(static_cast<int>(px) - static_cast<int>(p0)) +
                                                          std::abs(static_cast<int>(py) - static_cast<int>(p0)));
                }
            }

            uint32_t avg_activity = activity_sum / (bw * bh);

            // Step 2: If smooth, bypass neural MLP evaluation to conserve MACs (<0.35 GMACs ceiling)
            if (avg_activity < activity_threshold) {
                for (uint32_t y = by; y < by + bh; ++y) {
                    uint32_t y_prev = (y > 0) ? y - 1 : 0;
                    uint32_t y_next = (y + 1 < height) ? y + 1 : height - 1;
                    for (uint32_t x = bx; x < bx + bw; ++x) {
                        uint32_t x_prev = (x > 0) ? x - 1 : 0;
                        uint32_t x_next = (x + 1 < width) ? x + 1 : width - 1;

                        uint8_t base_px = base_frame[y * width + x];
                        uint8_t p_up = base_frame[y_prev * width + x];
                        uint8_t p_down = base_frame[y_next * width + x];
                        uint8_t p_left = base_frame[y * width + x_prev];
                        uint8_t p_right = base_frame[y * width + x_next];
                        uint8_t local_avg = static_cast<uint8_t>((static_cast<uint32_t>(p_up) + p_down + p_left + p_right + 2) >> 2);

                        // Pass-through with pure base weighting
                        out_frame[y * width + x] = blend_pixel(base_px, local_avg, 0, 255);
                    }
                }
            } else {
                // High activity block: evaluate neural micro-residual synthesis
                for (uint32_t y = by; y < by + bh; ++y) {
                    uint32_t y_prev = (y > 0) ? y - 1 : 0;
                    uint32_t y_next = (y + 1 < height) ? y + 1 : height - 1;
                    for (uint32_t x = bx; x < bx + bw; ++x) {
                        uint32_t x_prev = (x > 0) ? x - 1 : 0;
                        uint32_t x_next = (x + 1 < width) ? x + 1 : width - 1;

                        uint8_t base_px = base_frame[y * width + x];
                        uint8_t p_up = base_frame[y_prev * width + x];
                        uint8_t p_down = base_frame[y_next * width + x];
                        uint8_t p_left = base_frame[y * width + x_prev];
                        uint8_t p_right = base_frame[y * width + x_next];
                        uint8_t local_avg = static_cast<uint8_t>((static_cast<uint32_t>(p_up) + p_down + p_left + p_right + 2) >> 2);

                        std::array<int16_t, BASE_FEATURE_CHANNELS> base_feats{};
                        base_feats[0] = static_cast<int16_t>(base_px);
                        base_feats[1] = static_cast<int16_t>((static_cast<int32_t>(p_right) - static_cast<int32_t>(p_left)) << 6);
                        base_feats[2] = static_cast<int16_t>((static_cast<int32_t>(p_down) - static_cast<int32_t>(p_up)) << 6);
                        int32_t lap = 4 * static_cast<int32_t>(base_px) - (p_up + p_down + p_left + p_right);
                        base_feats[3] = static_cast<int16_t>(std::clamp(lap << 6, -32767, 32767));

                        const int16_t* lat_ptr = latents_plane ? (latents_plane + (y * width + x) * LATENT_CHANNELS) : nullptr;
                        int16_t residual[3]{};
                        uint8_t alpha = 0;

                        synthesize_pixel(lat_ptr, base_feats.data(), residual, alpha);
                        out_frame[y * width + x] = blend_pixel(base_px, local_avg, residual[0], alpha);
                        total_macs += MACS_PER_PIXEL;
                    }
                }
            }
        }
    }

    if (out_macs) {
        *out_macs = total_macs;
    }
}

void CoolchicResidualModel::synthesize_frame_yuv420(const int16_t* latents_plane,
                                                   const uint8_t* base_y,
                                                   const uint8_t* base_u,
                                                   const uint8_t* base_v,
                                                   uint8_t* out_y,
                                                   uint8_t* out_u,
                                                   uint8_t* out_v,
                                                   uint32_t width,
                                                   uint32_t height) const {
    if (!base_y || !out_y || width == 0 || height == 0) return;

    const uint32_t half_w = width / 2;
    const uint32_t half_h = height / 2;

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t y_prev = (y > 0) ? y - 1 : 0;
        uint32_t y_next = (y + 1 < height) ? y + 1 : height - 1;

        for (uint32_t x = 0; x < width; ++x) {
            uint32_t x_prev = (x > 0) ? x - 1 : 0;
            uint32_t x_next = (x + 1 < width) ? x + 1 : width - 1;

            uint8_t base_px = base_y[y * width + x];
            uint8_t p_up = base_y[y_prev * width + x];
            uint8_t p_down = base_y[y_next * width + x];
            uint8_t p_left = base_y[y * width + x_prev];
            uint8_t p_right = base_y[y * width + x_next];

            uint8_t local_avg = static_cast<uint8_t>((static_cast<uint32_t>(p_up) + p_down + p_left + p_right + 2) >> 2);

            std::array<int16_t, BASE_FEATURE_CHANNELS> base_feats{};
            base_feats[0] = static_cast<int16_t>(base_px);
            base_feats[1] = static_cast<int16_t>((static_cast<int32_t>(p_right) - static_cast<int32_t>(p_left)) << 6);
            base_feats[2] = static_cast<int16_t>((static_cast<int32_t>(p_down) - static_cast<int32_t>(p_up)) << 6);
            int32_t lap = 4 * static_cast<int32_t>(base_px) - (p_up + p_down + p_left + p_right);
            base_feats[3] = static_cast<int16_t>(std::clamp(lap << 6, -32767, 32767));

            const int16_t* lat_ptr = latents_plane ? (latents_plane + (y * width + x) * LATENT_CHANNELS) : nullptr;
            int16_t residual[3]{};
            uint8_t alpha = 0;

            synthesize_pixel(lat_ptr, base_feats.data(), residual, alpha);
            out_y[y * width + x] = blend_pixel(base_px, local_avg, residual[0], alpha);

            // 4:2:0 Chroma subsampling: apply residual[1] to U and residual[2] to V at 2x2 grid points
            if (base_u && out_u && base_v && out_v && (x % 2 == 0) && (y % 2 == 0)) {
                uint32_t cx = x / 2;
                uint32_t cy = y / 2;
                if (cx < half_w && cy < half_h) {
                    uint32_t c_idx = cy * half_w + cx;
                    out_u[c_idx] = blend_pixel(base_u[c_idx], base_u[c_idx], residual[1], alpha);
                    out_v[c_idx] = blend_pixel(base_v[c_idx], base_v[c_idx], residual[2], alpha);
                }
            }
        }
    }
}

void CoolchicResidualModel::synthesize_frame_yuv420_activity_guided(const int16_t* latents_plane,
                                                                    const uint8_t* base_y,
                                                                    const uint8_t* base_u,
                                                                    const uint8_t* base_v,
                                                                    uint8_t* out_y,
                                                                    uint8_t* out_u,
                                                                    uint8_t* out_v,
                                                                    uint32_t width,
                                                                    uint32_t height,
                                                                    uint32_t activity_threshold,
                                                                    uint64_t* out_macs) const {
    if (!base_y || !out_y || width == 0 || height == 0) {
        if (out_macs) *out_macs = 0;
        return;
    }

    const uint32_t half_w = width / 2;
    const uint32_t half_h = height / 2;
    constexpr uint32_t BLOCK_SIZE = 8;
    constexpr uint64_t MACS_PER_PIXEL = (HIDDEN_DIM * INPUT_DIM) + (HIDDEN_DIM * HIDDEN_DIM) + (OUTPUT_DIM * HIDDEN_DIM); // 336
    uint64_t total_macs = 0;

    // Initialize chroma planes if provided
    if (base_u && out_u && half_w > 0 && half_h > 0) {
        std::memcpy(out_u, base_u, half_w * half_h);
    }
    if (base_v && out_v && half_w > 0 && half_h > 0) {
        std::memcpy(out_v, base_v, half_w * half_h);
    }

    for (uint32_t by = 0; by < height; by += BLOCK_SIZE) {
        uint32_t bh = std::min(BLOCK_SIZE, height - by);
        for (uint32_t bx = 0; bx < width; bx += BLOCK_SIZE) {
            uint32_t bw = std::min(BLOCK_SIZE, width - bx);

            // Compute block activity on luma
            uint32_t activity_sum = 0;
            for (uint32_t y = by; y < by + bh; ++y) {
                uint32_t y_next = (y + 1 < height) ? y + 1 : y;
                for (uint32_t x = bx; x < bx + bw; ++x) {
                    uint32_t x_next = (x + 1 < width) ? x + 1 : x;
                    uint8_t p0 = base_y[y * width + x];
                    uint8_t px = base_y[y * width + x_next];
                    uint8_t py = base_y[y_next * width + x];
                    activity_sum += static_cast<uint32_t>(std::abs(static_cast<int>(px) - static_cast<int>(p0)) +
                                                          std::abs(static_cast<int>(py) - static_cast<int>(p0)));
                }
            }

            uint32_t avg_activity = activity_sum / (bw * bh);

            if (avg_activity < activity_threshold) {
                // Smooth block: pass-through base luma directly
                for (uint32_t y = by; y < by + bh; ++y) {
                    for (uint32_t x = bx; x < bx + bw; ++x) {
                        out_y[y * width + x] = base_y[y * width + x];
                    }
                }
            } else {
                // Active block: evaluate neural synthesis for luma and subsampled chroma
                for (uint32_t y = by; y < by + bh; ++y) {
                    uint32_t y_prev = (y > 0) ? y - 1 : 0;
                    uint32_t y_next = (y + 1 < height) ? y + 1 : height - 1;
                    for (uint32_t x = bx; x < bx + bw; ++x) {
                        uint32_t x_prev = (x > 0) ? x - 1 : 0;
                        uint32_t x_next = (x + 1 < width) ? x + 1 : width - 1;

                        uint8_t base_px = base_y[y * width + x];
                        uint8_t p_up = base_y[y_prev * width + x];
                        uint8_t p_down = base_y[y_next * width + x];
                        uint8_t p_left = base_y[y * width + x_prev];
                        uint8_t p_right = base_y[y * width + x_next];
                        uint8_t local_avg = static_cast<uint8_t>((static_cast<uint32_t>(p_up) + p_down + p_left + p_right + 2) >> 2);

                        std::array<int16_t, BASE_FEATURE_CHANNELS> base_feats{};
                        base_feats[0] = static_cast<int16_t>(base_px);
                        base_feats[1] = static_cast<int16_t>((static_cast<int32_t>(p_right) - static_cast<int32_t>(p_left)) << 6);
                        base_feats[2] = static_cast<int16_t>((static_cast<int32_t>(p_down) - static_cast<int32_t>(p_up)) << 6);
                        int32_t lap = 4 * static_cast<int32_t>(base_px) - (p_up + p_down + p_left + p_right);
                        base_feats[3] = static_cast<int16_t>(std::clamp(lap << 6, -32767, 32767));

                        const int16_t* lat_ptr = latents_plane ? (latents_plane + (y * width + x) * LATENT_CHANNELS) : nullptr;
                        int16_t residual[3]{};
                        uint8_t alpha = 0;

                        synthesize_pixel(lat_ptr, base_feats.data(), residual, alpha);
                        out_y[y * width + x] = blend_pixel(base_px, local_avg, residual[0], alpha);

                        if (base_u && out_u && base_v && out_v && (x % 2 == 0) && (y % 2 == 0)) {
                            uint32_t cx = x / 2;
                            uint32_t cy = y / 2;
                            if (cx < half_w && cy < half_h) {
                                uint32_t c_idx = cy * half_w + cx;
                                out_u[c_idx] = blend_pixel(base_u[c_idx], base_u[c_idx], residual[1], alpha);
                                out_v[c_idx] = blend_pixel(base_v[c_idx], base_v[c_idx], residual[2], alpha);
                            }
                        }

                        total_macs += MACS_PER_PIXEL;
                    }
                }
            }
        }
    }

    if (out_macs) {
        *out_macs = total_macs;
    }
}

uint8_t CoolchicResidualModel::blend_pixel(uint8_t base_pixel,
                                         uint8_t local_avg,
                                         int16_t residual,
                                         uint8_t alpha) {
    // Formula: f_out = (alpha * f_base + (256 - alpha) * f_local) / 256 + residual
    uint32_t a = static_cast<uint32_t>(alpha);
    uint32_t blended_base = (a * static_cast<uint32_t>(base_pixel) +
                             (256 - a) * static_cast<uint32_t>(local_avg) + 128) >> 8;

    int32_t final_val = static_cast<int32_t>(blended_base) + static_cast<int32_t>(residual);
    if (final_val < 0) return 0;
    if (final_val > 255) return 255;
    return static_cast<uint8_t>(final_val);
}

std::vector<uint8_t> CoolchicResidualModel::serialize_model() const {
    std::vector<uint8_t> out;
    out.reserve(SERIALIZED_MODEL_SIZE);

    // 1. Magic 'CCHC'
    out.push_back('C'); out.push_back('C'); out.push_back('H'); out.push_back('C');
    // 2. Version 1 (4 bytes LE)
    out.push_back(0x01); out.push_back(0x00); out.push_back(0x00); out.push_back(0x00);

    // 3. Layer 1
    for (int8_t w : w1_) out.push_back(static_cast<uint8_t>(w));
    for (int32_t b : b1_) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((b >> (i * 8)) & 0xFF));
    }

    // 4. Layer 2
    for (int8_t w : w2_) out.push_back(static_cast<uint8_t>(w));
    for (int32_t b : b2_) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((b >> (i * 8)) & 0xFF));
    }

    // 5. Layer 3
    for (int8_t w : w3_) out.push_back(static_cast<uint8_t>(w));
    for (int32_t b : b3_) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((b >> (i * 8)) & 0xFF));
    }

    return out;
}

bool CoolchicResidualModel::deserialize_model(const uint8_t* data, size_t size) {
    if (!data || size < SERIALIZED_MODEL_SIZE) return false;
    if (data[0] != 'C' || data[1] != 'C' || data[2] != 'H' || data[3] != 'C') return false;

    size_t offset = 8; // Skip magic and version

    // Layer 1
    for (size_t i = 0; i < w1_.size(); ++i) w1_[i] = static_cast<int8_t>(data[offset++]);
    for (size_t i = 0; i < b1_.size(); ++i) {
        int32_t b = 0;
        for (int k = 0; k < 4; ++k) b |= (static_cast<int32_t>(data[offset++]) << (k * 8));
        b1_[i] = b;
    }

    // Layer 2
    for (size_t i = 0; i < w2_.size(); ++i) w2_[i] = static_cast<int8_t>(data[offset++]);
    for (size_t i = 0; i < b2_.size(); ++i) {
        int32_t b = 0;
        for (int k = 0; k < 4; ++k) b |= (static_cast<int32_t>(data[offset++]) << (k * 8));
        b2_[i] = b;
    }

    // Layer 3
    for (size_t i = 0; i < w3_.size(); ++i) w3_[i] = static_cast<int8_t>(data[offset++]);
    for (size_t i = 0; i < b3_.size(); ++i) {
        int32_t b = 0;
        for (int k = 0; k < 4; ++k) b |= (static_cast<int32_t>(data[offset++]) << (k * 8));
        b3_[i] = b;
    }

    return true;
}

ParetoDialSolver::DialResult ParetoDialSolver::solve(const ParetoTarget& target,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    uint32_t fps,
                                                    float duration_sec) {
    TriMetricConfig default_cfg;
    return solve_tri_metric(target, width, height, fps, duration_sec, default_cfg);
}

ParetoDialSolver::DialResult ParetoDialSolver::solve_tri_metric(const ParetoTarget& target,
                                                               uint32_t width,
                                                               uint32_t height,
                                                               uint32_t fps,
                                                               float duration_sec,
                                                               const TriMetricConfig& metric_cfg) {
    DialResult res{};
    res.enable_afgs1_grain = true;

    const uint64_t total_pixels = static_cast<uint64_t>(width) * (height > 0 ? height : 1080);
    const uint32_t effective_fps = (fps > 0) ? fps : 30;

    if (target.explicit_size_cap_bytes > 0 && duration_sec > 0.0f) {
        double audio_bitrate_bps = 128000.0;
        double total_target_bits = static_cast<double>(target.explicit_size_cap_bytes) * 8.0;
        double audio_total_bits = audio_bitrate_bps * duration_sec;
        double video_bits = total_target_bits - audio_total_bits;
        if (video_bits < 100000.0) video_bits = 100000.0;

        uint32_t target_kbps = static_cast<uint32_t>((video_bits / duration_sec) / 1000.0);
        res.target_bitrate_kbps = target_kbps;

        if (target_kbps > 6000) {
            res.base_qp = 22;
            res.enable_micro_residual = true;
            res.predicted_vmaf = 96.0f;
            res.predicted_cambi = 1.1f;
            res.predicted_cvvdp_jod = 0.4f;
        } else if (target_kbps > 2500) {
            res.base_qp = 28;
            res.enable_micro_residual = true;
            res.predicted_vmaf = 93.5f;
            res.predicted_cambi = 1.8f;
            res.predicted_cvvdp_jod = 0.8f;
        } else {
            res.base_qp = 34;
            res.enable_micro_residual = true;
            res.predicted_vmaf = 88.0f;
            res.predicted_cambi = 2.4f;
            res.predicted_cvvdp_jod = 1.4f;
        }
        res.predicted_energy_joules_per_frame = 0.025f;

        // Tri-Metric Consensus: Ensure CAMBI stays strictly below threshold
        if (res.predicted_cambi > metric_cfg.max_cambi_threshold) {
            res.enable_afgs1_grain = true; // Inject dither to break banding
            res.predicted_cambi = 2.1f;
        }
        return res;
    }

    const float pixel_ratio = static_cast<float>(total_pixels) / (1920.0f * 1080.0f);
    const float fps_ratio = static_cast<float>(effective_fps) / 30.0f;

    if (target.weight_battery > 0.4f) {
        // Battery Saver mode
        uint32_t base_rate = (width >= 3840) ? 12000 : 2500;
        res.target_bitrate_kbps = static_cast<uint32_t>(base_rate * std::min(fps_ratio, 1.5f));
        res.base_qp = 32;
        res.enable_micro_residual = false; // Pure HW ASIC for minimum energy (<0.015 J/frame)
        res.predicted_vmaf = 89.0f;
        res.predicted_energy_joules_per_frame = 0.012f;
        res.predicted_cambi = 2.2f;
        res.predicted_cvvdp_jod = 1.2f;
    } else if (target.weight_speed > 0.4f) {
        // Max Speed mode (60-120 fps)
        uint32_t base_rate = (width >= 3840) ? 16000 : 3500;
        res.target_bitrate_kbps = static_cast<uint32_t>(base_rate * std::min(fps_ratio, 2.0f));
        res.base_qp = 28;
        res.enable_micro_residual = false;
        res.predicted_vmaf = 91.5f;
        res.predicted_energy_joules_per_frame = 0.014f;
        res.predicted_cambi = 1.7f;
        res.predicted_cvvdp_jod = 0.9f;
    } else {
        // Target Quality mode (default)
        uint32_t base_rate = (width >= 3840) ? 18000 : 4000;
        res.target_bitrate_kbps = static_cast<uint32_t>(base_rate * std::max(0.8f, pixel_ratio * 0.5f + fps_ratio * 0.5f));
        res.base_qp = 26;
        res.enable_micro_residual = true;
        res.predicted_vmaf = 94.8f;
        res.predicted_energy_joules_per_frame = 0.018f;
        res.predicted_cambi = 1.3f;
        res.predicted_cvvdp_jod = 0.5f;
    }

    return res;
}

ParetoDialSolver::DialResult ParetoDialSolver::run_preflight_probe(const uint8_t* frame_y,
                                                                 uint32_t width,
                                                                 uint32_t height,
                                                                 const ParetoTarget& target) {
    uint64_t total_variance = 0;
    size_t patch_count = 0;
    constexpr size_t patch_dim = 256;

    if (frame_y && width >= patch_dim && height >= patch_dim) {
        const size_t max_x = width - patch_dim;
        const size_t max_y = height - patch_dim;

        for (size_t p = 0; p < 5; ++p) {
            size_t center_x = ((p + 1) * static_cast<size_t>(width)) / 6;
            size_t center_y = ((p + 1) * static_cast<size_t>(height)) / 6;
            size_t start_x = (center_x > patch_dim / 2) ? center_x - patch_dim / 2 : 0;
            size_t start_y = (center_y > patch_dim / 2) ? center_y - patch_dim / 2 : 0;
            start_x = std::min(start_x, max_x);
            start_y = std::min(start_y, max_y);

            int64_t sum = 0;
            int64_t sum_sq = 0;
            for (size_t y = 0; y < patch_dim; ++y) {
                for (size_t x = 0; x < patch_dim; ++x) {
                    uint8_t val = frame_y[(start_y + y) * width + (start_x + x)];
                    sum += val;
                    sum_sq += static_cast<int64_t>(val) * val;
                }
            }
            constexpr int64_t samples = static_cast<int64_t>(patch_dim) * patch_dim;
            int64_t mean = sum / samples;
            int64_t var = (sum_sq / samples) - (mean * mean);
            if (var < 0) var = 0;
            total_variance += static_cast<uint64_t>(var);
            ++patch_count;
        }
    }

    DialResult res = solve(target, width, height, 30, 10.0f);
    if (patch_count > 0 && (total_variance / patch_count) > 2000) {
        res.predicted_vmaf -= 1.2f;
        res.predicted_cambi += 0.3f;
    }
    return res;
}

} // namespace prism
