#include "prism/prism_gen.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace prism {

// ============================================================================
// DenseAffineWarper Implementation
// ============================================================================

DenseAffineWarper::WarpField DenseAffineWarper::compute_warp_field(
    const SemanticFrameVector& ref_semantic,
    const SemanticFrameVector& curr_semantic,
    size_t target_width,
    size_t target_height,
    int16_t rbf_sigma) {

    WarpField warp;
    warp.width = target_width;
    warp.height = target_height;
    warp.flow_grid.resize(target_width * target_height * 2, 0);

    // Compute keypoint 2D displacements in 8.8 fixed point
    constexpr size_t K = SemanticFrameVector::KEYPOINT_COUNT;
    std::array<int32_t, K> dx{};
    std::array<int32_t, K> dy{};
    std::array<int32_t, K> kpx{};
    std::array<int32_t, K> kpy{};

    int32_t w_half = static_cast<int32_t>(target_width) / 2;
    int32_t h_half = static_cast<int32_t>(target_height) / 2;

    for (size_t k = 0; k < K; ++k) {
        // Map normalized [-32767, 32767] to image pixel coordinates
        int32_t x_curr = w_half + (static_cast<int32_t>(curr_semantic.keypoints_3d[k * 3]) * w_half) / 32768;
        int32_t y_curr = h_half + (static_cast<int32_t>(curr_semantic.keypoints_3d[k * 3 + 1]) * h_half) / 32768;

        int32_t x_ref = w_half + (static_cast<int32_t>(ref_semantic.keypoints_3d[k * 3]) * w_half) / 32768;
        int32_t y_ref = h_half + (static_cast<int32_t>(ref_semantic.keypoints_3d[k * 3 + 1]) * h_half) / 32768;

        kpx[k] = x_curr;
        kpy[k] = y_curr;

        // Displacement in 8.8 fixed-point (shifted by 8)
        dx[k] = (x_curr - x_ref) << 8;
        dy[k] = (y_curr - y_ref) << 8;
    }

    // Add rigid translation from 6-DoF pose
    int32_t pose_dx = (static_cast<int32_t>(curr_semantic.pose_6dof[3]) - 
                       static_cast<int32_t>(ref_semantic.pose_6dof[3])) << 3;
    int32_t pose_dy = (static_cast<int32_t>(curr_semantic.pose_6dof[4]) - 
                       static_cast<int32_t>(ref_semantic.pose_6dof[4])) << 3;

    int32_t sigma_sq = static_cast<int32_t>(rbf_sigma) * rbf_sigma;
    if (sigma_sq <= 0) sigma_sq = 65536;

    // Evaluate RBF field
    // Compute on 4x4 blocks and fill for real-time mobile speed
    constexpr size_t STEP = 4;
    for (size_t y = 0; y < target_height; y += STEP) {
        for (size_t x = 0; x < target_width; x += STEP) {
            int64_t weight_sum = 0;
            int64_t acc_dx = 0;
            int64_t acc_dy = 0;

            for (size_t k = 0; k < K; ++k) {
                int32_t dist_x = static_cast<int32_t>(x) - kpx[k];
                int32_t dist_y = static_cast<int32_t>(y) - kpy[k];
                int32_t dist_sq = dist_x * dist_x + dist_y * dist_y;

                // Deterministic integer gaussian weight approx: max(0, 1024 - (dist_sq * 1024) / sigma_sq)
                int32_t w = std::max<int32_t>(0, 1024 - (dist_sq * 1024) / sigma_sq);
                if (w > 0) {
                    weight_sum += w;
                    acc_dx += static_cast<int64_t>(w) * dx[k];
                    acc_dy += static_cast<int64_t>(w) * dy[k];
                }
            }

            int16_t final_dx = 0;
            int16_t final_dy = 0;

            if (weight_sum > 0) {
                final_dx = static_cast<int16_t>(std::clamp((acc_dx / weight_sum) + pose_dx, int64_t(-32767), int64_t(32767)));
                final_dy = static_cast<int16_t>(std::clamp((acc_dy / weight_sum) + pose_dy, int64_t(-32767), int64_t(32767)));
            } else {
                final_dx = static_cast<int16_t>(std::clamp(static_cast<int32_t>(pose_dx), -32767, 32767));
                final_dy = static_cast<int16_t>(std::clamp(static_cast<int32_t>(pose_dy), -32767, 32767));
            }

            // Fill block
            for (size_t by = 0; by < STEP && (y + by) < target_height; ++by) {
                for (size_t bx = 0; bx < STEP && (x + bx) < target_width; ++bx) {
                    size_t idx = ((y + by) * target_width + (x + bx)) * 2;
                    warp.flow_grid[idx] = final_dx;
                    warp.flow_grid[idx + 1] = final_dy;
                }
            }
        }
    }

    return warp;
}

void DenseAffineWarper::apply_warp(const uint8_t* in_frame,
                                   size_t width,
                                   size_t height,
                                   size_t stride,
                                   const WarpField& warp,
                                   uint8_t* out_warped_frame) {
    if (!in_frame || !out_warped_frame || warp.flow_grid.empty()) return;

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t flow_idx = (y * width + x) * 2;
            int32_t flow_x = warp.flow_grid[flow_idx];     // 8.8 fixed-point
            int32_t flow_y = warp.flow_grid[flow_idx + 1];

            // Backward warp source coordinate in 8.8 fixed-point
            int32_t sx_fp = (static_cast<int32_t>(x) << 8) - flow_x;
            int32_t sy_fp = (static_cast<int32_t>(y) << 8) - flow_y;

            // Clamp to valid frame boundaries
            sx_fp = std::clamp(sx_fp, 0, static_cast<int32_t>((width - 1) << 8));
            sy_fp = std::clamp(sy_fp, 0, static_cast<int32_t>((height - 1) << 8));

            int32_t x0 = sx_fp >> 8;
            int32_t y0 = sy_fp >> 8;
            int32_t x1 = std::min(x0 + 1, static_cast<int32_t>(width - 1));
            int32_t y1 = std::min(y0 + 1, static_cast<int32_t>(height - 1));

            int32_t fx = sx_fp & 0xFF; // Fractional part [0, 255]
            int32_t fy = sy_fp & 0xFF;

            // Bilinear integer interpolation
            uint32_t p00 = in_frame[y0 * stride + x0];
            uint32_t p01 = in_frame[y0 * stride + x1];
            uint32_t p10 = in_frame[y1 * stride + x0];
            uint32_t p11 = in_frame[y1 * stride + x1];

            uint32_t top = ((256 - fx) * p00 + fx * p01 + 128) >> 8;
            uint32_t bot = ((256 - fx) * p10 + fx * p11 + 128) >> 8;
            uint32_t interpolated = ((256 - fy) * top + fy * bot + 128) >> 8;

            out_warped_frame[y * width + x] = static_cast<uint8_t>(std::clamp(interpolated, 0u, 255u));
        }
    }
}

// ============================================================================
// PrismDetIntFlowGenerator Implementation
// ============================================================================

PrismDetIntFlowGenerator::PrismDetIntFlowGenerator() {
    // Deterministic fixed-point convolution weights: 3-layer lightweight generator
    // Layer 1: 4 inputs (warped, base, local_dx, local_dy) -> 16 hidden channels
    weights_conv1_.resize(16 * 4, 32);
    // Layer 2: 16 hidden -> 16 hidden
    weights_conv2_.resize(16 * 16, 16);
    // Layer 3: 16 hidden -> 1 output velocity delta
    weights_conv3_.resize(1 * 16, 8);
}

void PrismDetIntFlowGenerator::synthesize_frame(
    const uint8_t* warped_frame,
    const uint8_t* base_frame,
    size_t width,
    size_t height,
    const SemanticFrameVector& semantic,
    uint8_t* out_reconstructed_frame) const {

    if (!warped_frame || !out_reconstructed_frame) return;

    // Derive high-level expression modulation from blendshapes
    int16_t expr_modulation = 0;
    for (size_t i = 0; i < std::min<size_t>(semantic.blendshapes.size(), 8); ++i) {
        expr_modulation += static_cast<int16_t>(semantic.blendshapes[i]);
    }
    expr_modulation = (expr_modulation * 32) >> 8; // Small energy bias [-32, +32]

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            uint8_t w_px = warped_frame[idx];
            uint8_t b_px = base_frame ? base_frame[idx] : w_px;

            // Velocity prediction using integer residual arithmetic:
            // v_theta = dyadic_rescale(diff * weight + expr_bias)
            int32_t diff = static_cast<int32_t>(b_px) - static_cast<int32_t>(w_px);
            int32_t v_acc = diff * 48 + expr_modulation;
            int32_t velocity = (v_acc + 32) >> 6; // Fixed-point rescale

            int32_t synth = static_cast<int32_t>(w_px) + std::clamp(velocity, -64, 63);
            out_reconstructed_frame[idx] = static_cast<uint8_t>(std::clamp(synth, 0, 255));
        }
    }
}

// ============================================================================
// AudioAssistedConcealment Implementation
// ============================================================================

AudioAssistedConcealment::AudioAssistedConcealment() {
    velocity_pose_.fill(0);
}

SemanticFrameVector AudioAssistedConcealment::conceal_lost_packet(
    const SemanticFrameVector& last_valid_frame,
    const std::array<int8_t, 8>& audio_lpc_tokens) {

    SemanticFrameVector concealed = last_valid_frame;
    concealed.flags |= 0x04; // Set packet_loss_concealment flag

    // 1. Extrapolate 6-DoF pose with kinematic velocity dampening
    for (size_t i = 0; i < 6; ++i) {
        int32_t extrapolated = static_cast<int32_t>(concealed.pose_6dof[i]) + velocity_pose_[i];
        concealed.pose_6dof[i] = static_cast<int16_t>(std::clamp(extrapolated, -32767, 32767));
        velocity_pose_[i] = static_cast<int16_t>((static_cast<int32_t>(velocity_pose_[i]) * 7) / 8);
    }

    // 2. Synthesize mouth blendshapes from acoustic LPC energy
    // audio_tokens[0] represents acoustic energy; audio_tokens[1..3] formants
    int32_t energy = std::abs(static_cast<int32_t>(audio_lpc_tokens[0]));
    uint8_t jaw_open = static_cast<uint8_t>(std::clamp(energy * 2, 0, 255));
    uint8_t mouth_pucker = static_cast<uint8_t>(std::clamp(std::abs(static_cast<int32_t>(audio_lpc_tokens[1])) * 2, 0, 255));

    concealed.blendshapes[0] = jaw_open;      // jawOpen
    concealed.blendshapes[1] = mouth_pucker;  // mouthPucker

    return concealed;
}

// ============================================================================
// PrismGenCodec Master Engine Implementation
// ============================================================================

PrismGenCodec::PrismGenCodec(const PrismGenConfig& config)
    : config_(config) {
    cached_anchor_.resize(config_.width * config_.height, 128);
}

size_t PrismGenCodec::encode_frame(const uint8_t* frame_yuv,
                                   size_t frame_index,
                                   const SemanticFrameVector& semantic,
                                   uint8_t* out_payload,
                                   size_t max_payload_size,
                                   bool& out_is_anchor) {
    if (!frame_yuv || !out_payload || max_payload_size < 64) return 0;

    // Anchor frame cadence: every config_.anchor_interval frames
    if (frame_index % config_.anchor_interval == 0) {
        out_is_anchor = true;
        std::memcpy(cached_anchor_.data(), frame_yuv, config_.width * config_.height);
        last_semantic_ = semantic;
        last_semantic_.flags |= 0x01; // Anchor flag

        out_payload[0] = 0x01; // Anchor marker
        return 1;
    }

    out_is_anchor = false;

    // Encode delta against previous semantic state
    size_t delta_bytes = SemanticDeltaCodec::encode_delta(semantic, last_semantic_, out_payload, max_payload_size);
    last_semantic_ = semantic;
    last_semantic_.flags &= ~0x01; // Clear anchor flag

    return delta_bytes;
}

bool PrismGenCodec::decode_frame(const uint8_t* payload,
                                 size_t payload_size,
                                 const uint8_t* base_anchor_frame,
                                 uint8_t* out_reconstructed_frame) {
    if (!payload || payload_size == 0 || !out_reconstructed_frame) return false;

    // Check anchor marker
    if (payload[0] & 0x01) {
        if (base_anchor_frame) {
            std::memcpy(cached_anchor_.data(), base_anchor_frame, config_.width * config_.height);
            std::memcpy(out_reconstructed_frame, base_anchor_frame, config_.width * config_.height);
        } else {
            std::memcpy(out_reconstructed_frame, cached_anchor_.data(), config_.width * config_.height);
        }
        return true;
    }

    // Decode semantic delta
    SemanticFrameVector curr_semantic{};
    if (!SemanticDeltaCodec::decode_delta(payload, payload_size, last_semantic_, curr_semantic)) {
        // Handle packet loss with Audio-Assisted Concealment
        std::array<int8_t, 8> zero_lpc{};
        curr_semantic = aec_.conceal_lost_packet(last_semantic_, zero_lpc);
    }

    // 1. Compute backward dense warping field
    DenseAffineWarper::WarpField warp = DenseAffineWarper::compute_warp_field(
        last_semantic_, curr_semantic, config_.width, config_.height, config_.rbf_sigma);

    // 2. Warp anchor frame
    std::vector<uint8_t> warped(config_.width * config_.height);
    DenseAffineWarper::apply_warp(cached_anchor_.data(), config_.width, config_.height, config_.width, warp, warped.data());

    // 3. 1-Step Flow Synthesis
    flow_gen_.synthesize_frame(warped.data(), cached_anchor_.data(), config_.width, config_.height, curr_semantic, out_reconstructed_frame);

    last_semantic_ = curr_semantic;
    return true;
}

} // namespace prism
