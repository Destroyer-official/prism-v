#include "prism/nerv_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace prism {

// ============================================================================
// TriPlaneLattice Implementation
// ============================================================================

TriPlaneLattice::TriPlaneLattice(size_t width, size_t height, size_t frames, size_t channels, size_t downscale)
    : frames_(std::max<size_t>(1, frames)), channels_(std::max<size_t>(1, channels)) {
    pw_ = std::max<size_t>(1, width / downscale);
    ph_ = std::max<size_t>(1, height / downscale);

    p_xy_.resize(pw_ * ph_ * channels_, 0);
    p_xt_.resize(pw_ * frames_ * channels_, 0);
    p_yt_.resize(ph_ * frames_ * channels_, 0);
}

void TriPlaneLattice::sample_feature(int32_t t_norm,
                                     int32_t x_norm,
                                     int32_t y_norm,
                                     int16_t* out_feature) const {
    if (!out_feature) return;

    // Map normalized [0, 32767] to fixed-point grid coordinates (shifted by 8)
    int32_t gx_fp = (x_norm * static_cast<int32_t>(pw_ - 1) * 256) / 32768;
    int32_t gy_fp = (y_norm * static_cast<int32_t>(ph_ - 1) * 256) / 32768;
    int32_t gt_fp = (t_norm * static_cast<int32_t>(frames_ - 1) * 256) / 32768;

    int32_t x0 = std::clamp(gx_fp >> 8, 0, static_cast<int32_t>(pw_ - 1));
    int32_t y0 = std::clamp(gy_fp >> 8, 0, static_cast<int32_t>(ph_ - 1));
    int32_t t0 = std::clamp(gt_fp >> 8, 0, static_cast<int32_t>(frames_ - 1));

    int32_t x1 = std::min<int32_t>(x0 + 1, static_cast<int32_t>(pw_ - 1));
    int32_t y1 = std::min<int32_t>(y0 + 1, static_cast<int32_t>(ph_ - 1));
    int32_t t1 = std::min<int32_t>(t0 + 1, static_cast<int32_t>(frames_ - 1));

    int32_t fx = gx_fp & 0xFF;
    int32_t fy = gy_fp & 0xFF;
    int32_t ft = gt_fp & 0xFF;

    for (size_t c = 0; c < channels_; ++c) {
        // 1. Bilinear sample P_xy
        int32_t xy_00 = p_xy_[(y0 * pw_ + x0) * channels_ + c];
        int32_t xy_01 = p_xy_[(y0 * pw_ + x1) * channels_ + c];
        int32_t xy_10 = p_xy_[(y1 * pw_ + x0) * channels_ + c];
        int32_t xy_11 = p_xy_[(y1 * pw_ + x1) * channels_ + c];

        int32_t top_xy = ((256 - fx) * xy_00 + fx * xy_01 + 128) >> 8;
        int32_t bot_xy = ((256 - fx) * xy_10 + fx * xy_11 + 128) >> 8;
        int32_t feat_xy = ((256 - fy) * top_xy + fy * bot_xy + 128) >> 8;

        // 2. Bilinear sample P_xt
        int32_t xt_00 = p_xt_[(t0 * pw_ + x0) * channels_ + c];
        int32_t xt_01 = p_xt_[(t0 * pw_ + x1) * channels_ + c];
        int32_t xt_10 = p_xt_[(t1 * pw_ + x0) * channels_ + c];
        int32_t xt_11 = p_xt_[(t1 * pw_ + x1) * channels_ + c];

        int32_t top_xt = ((256 - fx) * xt_00 + fx * xt_01 + 128) >> 8;
        int32_t bot_xt = ((256 - fx) * xt_10 + fx * xt_11 + 128) >> 8;
        int32_t feat_xt = ((256 - ft) * top_xt + ft * bot_xt + 128) >> 8;

        // 3. Bilinear sample P_yt
        int32_t yt_00 = p_yt_[(t0 * ph_ + y0) * channels_ + c];
        int32_t yt_01 = p_yt_[(t0 * ph_ + y1) * channels_ + c];
        int32_t yt_10 = p_yt_[(t1 * ph_ + y0) * channels_ + c];
        int32_t yt_11 = p_yt_[(t1 * ph_ + y1) * channels_ + c];

        int32_t top_yt = ((256 - fy) * yt_00 + fy * yt_01 + 128) >> 8;
        int32_t bot_yt = ((256 - fy) * yt_10 + fy * yt_11 + 128) >> 8;
        int32_t feat_yt = ((256 - ft) * top_yt + ft * bot_yt + 128) >> 8;

        // 4. Hadamard channel product in fixed-point 16-bit
        int64_t prod = (static_cast<int64_t>(feat_xy) * feat_xt + 16384) >> 15;
        int64_t final_val = (prod * feat_yt + 16384) >> 15;

        out_feature[c] = static_cast<int16_t>(std::clamp(final_val, int64_t(-32767), int64_t(32767)));
    }
}

// ============================================================================
// NeRVSynthesisMLP Implementation
// ============================================================================

NeRVSynthesisMLP::NeRVSynthesisMLP() {
    w1_.fill(16);
    b1_.fill(0);
    w2_.fill(12);
    b2_.fill(0);
    w3_.fill(8);
    b3_.fill(0);
}

void NeRVSynthesisMLP::synthesize(const int16_t* feature, int16_t* out_yuv_residual) const {
    if (!feature || !out_yuv_residual) return;

    // Layer 1: 8 inputs -> 16 hidden
    std::array<int16_t, HIDDEN_DIM> h1{};
    for (size_t m = 0; m < HIDDEN_DIM; ++m) {
        int32_t acc = b1_[m];
        for (size_t k = 0; k < INPUT_DIM; ++k) {
            acc += static_cast<int32_t>(feature[k]) * w1_[m * INPUT_DIM + k];
        }
        // Dyadic rescale + integer PReLU: f(x) = x > 0 ? x : (x * 32) >> 8
        int16_t val = DetInt::dyadic_rescale(acc, 16384, 15);
        if (val < 0) val = static_cast<int16_t>((static_cast<int32_t>(val) * 32) >> 8);
        h1[m] = val;
    }

    // Layer 2: 16 hidden -> 16 hidden
    std::array<int16_t, HIDDEN_DIM> h2{};
    for (size_t m = 0; m < HIDDEN_DIM; ++m) {
        int32_t acc = b2_[m];
        for (size_t k = 0; k < HIDDEN_DIM; ++k) {
            acc += static_cast<int32_t>(h1[k]) * w2_[m * HIDDEN_DIM + k];
        }
        int16_t val = DetInt::dyadic_rescale(acc, 16384, 15);
        if (val < 0) val = static_cast<int16_t>((static_cast<int32_t>(val) * 32) >> 8);
        h2[m] = val;
    }

    // Layer 3: 16 hidden -> 3 Y, U, V residuals
    for (size_t m = 0; m < OUTPUT_DIM; ++m) {
        int32_t acc = b3_[m];
        for (size_t k = 0; k < HIDDEN_DIM; ++k) {
            acc += static_cast<int32_t>(h2[k]) * w3_[m * HIDDEN_DIM + k];
        }
        // Rescale into [-128, +127] residual delta
        int16_t res = DetInt::dyadic_rescale(acc, 4096, 15);
        out_yuv_residual[m] = static_cast<int16_t>(std::clamp<int32_t>(res, -128, 127));
    }
}

// ============================================================================
// HessianBitAllocator Implementation
// ============================================================================

std::vector<uint8_t> HessianBitAllocator::allocate_bits(
    const std::vector<int32_t>& diagonal_hessian,
    uint8_t target_avg_bits) {

    size_t count = diagonal_hessian.size();
    if (count == 0) return {};

    std::vector<uint8_t> alloc(count, target_avg_bits);

    // Compute log-sensitivity: s_i = log2(H_ii + 1)
    std::vector<double> log_s(count);
    double sum_s = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double v = std::max<double>(1.0, static_cast<double>(std::abs(diagonal_hessian[i])));
        log_s[i] = std::log2(v);
        sum_s += log_s[i];
    }
    double mean_s = sum_s / static_cast<double>(count);

    // Optimal water-filling bit allocation: b_i = target + 0.5 * (s_i - mean_s)
    for (size_t i = 0; i < count; ++i) {
        double b = static_cast<double>(target_avg_bits) + 0.5 * (log_s[i] - mean_s);
        int32_t b_int = static_cast<int32_t>(std::round(b));
        // Quantize bit-width to discrete support: 2, 4, 8 bits
        if (b_int <= 3) alloc[i] = 2;
        else if (b_int <= 6) alloc[i] = 4;
        else alloc[i] = 8;
    }

    return alloc;
}

void HessianBitAllocator::quantize_with_allocation(
    const int16_t* weights,
    const std::vector<uint8_t>& bit_alloc,
    size_t count,
    int8_t* out_quantized) {

    if (!weights || !out_quantized) return;

    for (size_t i = 0; i < count; ++i) {
        uint8_t bits = (i < bit_alloc.size()) ? bit_alloc[i] : 4;
        int32_t levels = (1 << bits) - 1; // 2 bits -> 3 levels, 4 bits -> 15 levels, 8 bits -> 255 levels
        out_quantized[i] = BisRfsq::quantize_scalar(weights[i], levels);
    }
}

// ============================================================================
// NeRVCodec Implementation
// ============================================================================

NeRVCodec::NeRVCodec(const NeRVSConfig& config)
    : config_(config),
      lattice_(config.width, config.height, config.gop_frames, config.triplane_channels, config.spatial_downscale) {
}

size_t NeRVCodec::serialize_gop(uint8_t* out_payload, size_t max_payload_size) {
    if (!out_payload || max_payload_size < 128) return 0;

    size_t offset = 0;

    // 1. Header: Magic 'NERV' (4 bytes) + version (1 byte)
    out_payload[offset++] = 'N';
    out_payload[offset++] = 'E';
    out_payload[offset++] = 'R';
    out_payload[offset++] = 'V';
    out_payload[offset++] = 0x01; // Version 1

    // 2. GOP dimensions
    out_payload[offset++] = static_cast<uint8_t>(config_.gop_frames);
    out_payload[offset++] = static_cast<uint8_t>(config_.triplane_channels);
    out_payload[offset++] = static_cast<uint8_t>(config_.spatial_downscale);

    // 3. Serialize quantized MLP weights (16*8 + 16*16 + 3*16 = 432 bytes)
    const auto& w1 = mlp_.weights_l1();
    const auto& w2 = mlp_.weights_l2();
    const auto& w3 = mlp_.weights_l3();

    for (int8_t w : w1) out_payload[offset++] = static_cast<uint8_t>(w);
    for (int8_t w : w2) out_payload[offset++] = static_cast<uint8_t>(w);
    for (int8_t w : w3) out_payload[offset++] = static_cast<uint8_t>(w);

    return offset;
}

bool NeRVCodec::deserialize_gop(const uint8_t* payload, size_t payload_size) {
    if (!payload || payload_size < 128) return false;

    size_t offset = 0;
    if (payload[0] != 'N' || payload[1] != 'E' || payload[2] != 'R' || payload[3] != 'V') {
        return false;
    }
    offset += 5; // Skip magic and version

    size_t gop_frames = payload[offset++];
    size_t channels = payload[offset++];
    size_t downscale = payload[offset++];
    (void)gop_frames;
    (void)downscale;

    if (channels != config_.triplane_channels) return false;

    // Restore MLP weights
    auto& w1 = mlp_.weights_l1();
    auto& w2 = mlp_.weights_l2();
    auto& w3 = mlp_.weights_l3();

    for (size_t i = 0; i < w1.size(); ++i) w1[i] = static_cast<int8_t>(payload[offset++]);
    for (size_t i = 0; i < w2.size(); ++i) w2[i] = static_cast<int8_t>(payload[offset++]);
    for (size_t i = 0; i < w3.size(); ++i) w3[i] = static_cast<int8_t>(payload[offset++]);

    return true;
}

void NeRVCodec::render_frame(float t_continuous,
                             const uint8_t* base_frame,
                             uint8_t* out_frame) {
    if (!out_frame) return;

    int32_t t_norm = static_cast<int32_t>(std::clamp(t_continuous, 0.0f, 1.0f) * 32767.0f);
    std::vector<int16_t> feat(config_.triplane_channels);
    std::array<int16_t, 3> yuv_residual{};

    for (size_t y = 0; y < config_.height; ++y) {
        int32_t y_norm = (static_cast<int32_t>(y) * 32767) / static_cast<int32_t>(config_.height - 1);
        for (size_t x = 0; x < config_.width; ++x) {
            int32_t x_norm = (static_cast<int32_t>(x) * 32767) / static_cast<int32_t>(config_.width - 1);

            // Sample continuous triplane feature
            lattice_.sample_feature(t_norm, x_norm, y_norm, feat.data());

            // Synthesize residual delta via coordinate MLP
            mlp_.synthesize(feat.data(), yuv_residual.data());

            size_t idx = y * config_.width + x;
            uint8_t base_px = base_frame ? base_frame[idx] : 128;
            int32_t rendered = static_cast<int32_t>(base_px) + yuv_residual[0];

            out_frame[idx] = static_cast<uint8_t>(std::clamp(rendered, 0, 255));
        }
    }
}

} // namespace prism
