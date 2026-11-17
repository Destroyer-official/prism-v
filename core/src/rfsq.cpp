#include "prism/rfsq.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace prism {

// Static 512-entry reciprocal square root lookup table:
// Value = round(4096.0 / sqrt(i + 1))
const std::array<uint16_t, 512> BisRfsq::ISQRT_LUT_512 = []() {
    std::array<uint16_t, 512> lut{};
    for (size_t i = 0; i < 512; ++i) {
        double v = static_cast<double>(i + 1);
        double isqrt_val = 4096.0 / std::sqrt(v);
        lut[i] = static_cast<uint16_t>(std::clamp(std::round(isqrt_val), 1.0, 4096.0));
    }
    return lut;
}();

void BisRfsq::integer_layernorm(const int16_t* in, int16_t* out, size_t dim) {
    if (dim == 0) return;

    // 1. Compute deterministic sum of squares in int64_t to prevent overflow
    int64_t sum_sq = 0;
    for (size_t i = 0; i < dim; ++i) {
        int32_t val = in[i];
        sum_sq += static_cast<int64_t>(val) * val;
    }

    // Mean variance
    int64_t mean_var = sum_sq / static_cast<int64_t>(dim);

    // Map variance to LUT index in [0, 511]
    // Normalized for 16-bit range (max variance ~ 32767^2 = 1,073,676,289)
    size_t lut_idx = static_cast<size_t>(std::clamp(mean_var >> 21, int64_t(0), int64_t(511)));
    uint16_t inv_scale = ISQRT_LUT_512[lut_idx];

    // Normalize activations: (x * inv_scale) >> 12
    for (size_t i = 0; i < dim; ++i) {
        int32_t scaled = (static_cast<int32_t>(in[i]) * inv_scale + 2048) >> 12;
        out[i] = static_cast<int16_t>(std::clamp(scaled, -32767, 32767));
    }
}

int8_t BisRfsq::quantize_scalar(int16_t val, int32_t num_levels) {
    if (num_levels <= 1) return 0;
    int32_t k = (num_levels - 1) / 2;
    int32_t step = 65534 / (num_levels - 1);
    if (step <= 0) step = 1;

    // Deterministic round-to-nearest
    int32_t level = 0;
    if (val >= 0) {
        level = (val + (step / 2)) / step;
    } else {
        level = (val - (step / 2)) / step;
    }

    return static_cast<int8_t>(std::clamp(level, -k, k));
}

int16_t BisRfsq::dequantize_scalar(int8_t level, int32_t num_levels) {
    if (num_levels <= 1) return 0;
    int32_t step = 65534 / (num_levels - 1);
    int32_t val = static_cast<int32_t>(level) * step;
    return static_cast<int16_t>(std::clamp(val, -32767, 32767));
}

void BisRfsq::encode_vector(const int16_t* latents,
                            size_t dim,
                            int32_t levels_stage1,
                            int32_t levels_stage2,
                            int8_t* out_codes_stage1,
                            int8_t* out_codes_stage2) {
    std::vector<int16_t> residual(dim);
    std::vector<int16_t> norm_residual(dim);

    // Stage 1: Coarse FSQ
    for (size_t i = 0; i < dim; ++i) {
        out_codes_stage1[i] = quantize_scalar(latents[i], levels_stage1);
        int16_t recon1 = dequantize_scalar(out_codes_stage1[i], levels_stage1);
        residual[i] = static_cast<int16_t>(latents[i] - recon1);
    }

    // Stage 2: Integer LayerNorm of residual + Fine FSQ
    integer_layernorm(residual.data(), norm_residual.data(), dim);
    for (size_t i = 0; i < dim; ++i) {
        out_codes_stage2[i] = quantize_scalar(norm_residual[i], levels_stage2);
    }
}

void BisRfsq::decode_vector(const int8_t* codes_stage1,
                            const int8_t* codes_stage2,
                            size_t dim,
                            int32_t levels_stage1,
                            int32_t levels_stage2,
                            int16_t* out_latents) {
    for (size_t i = 0; i < dim; ++i) {
        int16_t recon1 = dequantize_scalar(codes_stage1[i], levels_stage1);
        int16_t recon2 = dequantize_scalar(codes_stage2[i], levels_stage2);
        // Scaled residual reconstruction (dyadic shift down to residual scale ~1/8)
        int32_t fine_delta = (static_cast<int32_t>(recon2) * 512 + 2048) >> 12;
        int32_t total = static_cast<int32_t>(recon1) + fine_delta;
        out_latents[i] = static_cast<int16_t>(std::clamp(total, -32767, 32767));
    }
}

void BisRfsq::encode_grid(const int16_t* latents,
                          size_t width,
                          size_t height,
                          size_t channels,
                          int32_t levels_stage1,
                          int32_t levels_stage2,
                          int8_t* out_codes_stage1,
                          int8_t* out_codes_stage2) {
    if (!latents || !out_codes_stage1 || !out_codes_stage2) return;
    const size_t total_vectors = width * height;

    for (size_t v = 0; v < total_vectors; ++v) {
        const int16_t* vec_in = latents + v * channels;
        int8_t* c1_out = out_codes_stage1 + v * channels;
        int8_t* c2_out = out_codes_stage2 + v * channels;
        encode_vector(vec_in, channels, levels_stage1, levels_stage2, c1_out, c2_out);
    }
}

void BisRfsq::decode_grid(const int8_t* codes_stage1,
                          const int8_t* codes_stage2,
                          size_t width,
                          size_t height,
                          size_t channels,
                          int32_t levels_stage1,
                          int32_t levels_stage2,
                          int16_t* out_latents) {
    if (!codes_stage1 || !codes_stage2 || !out_latents) return;
    const size_t total_vectors = width * height;

    for (size_t v = 0; v < total_vectors; ++v) {
        const int8_t* c1_in = codes_stage1 + v * channels;
        const int8_t* c2_in = codes_stage2 + v * channels;
        int16_t* vec_out = out_latents + v * channels;
        decode_vector(c1_in, c2_in, channels, levels_stage1, levels_stage2, vec_out);
    }
}

// ============================================================================
// SemanticDeltaCodec Implementation
// ============================================================================

size_t SemanticDeltaCodec::encode_delta(const SemanticFrameVector& current,
                                       const SemanticFrameVector& reference,
                                       uint8_t* out_payload,
                                       size_t max_payload_size) {
    // Target budget: <= 50 bytes per frame (1.5 kbps @ 30 fps)
    if (max_payload_size < 54) return 0;

    size_t offset = 0;

    // 1. Header flag byte
    out_payload[offset++] = current.flags;

    // 2. 6-DoF Pose deltas (6 bytes: packed 8-bit deltas)
    for (size_t i = 0; i < 6; ++i) {
        int32_t delta = (static_cast<int32_t>(current.pose_6dof[i]) - 
                         static_cast<int32_t>(reference.pose_6dof[i])) >> 4;
        out_payload[offset++] = static_cast<uint8_t>(std::clamp(delta, -128, 127) + 128);
    }

    // 3. 3D Keypoint deltas: 32 keypoints x 3 = 96 coords
    // Quantize deltas to 4-bit nibbles [-8, +7] -> 48 bytes
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; i += 2) {
        int32_t d0 = (static_cast<int32_t>(current.keypoints_3d[i]) - 
                      static_cast<int32_t>(reference.keypoints_3d[i])) >> 8;
        int32_t d1 = (static_cast<int32_t>(current.keypoints_3d[i + 1]) - 
                      static_cast<int32_t>(reference.keypoints_3d[i + 1])) >> 8;

        uint8_t nib0 = static_cast<uint8_t>(std::clamp(d0, -8, 7) + 8) & 0x0F;
        uint8_t nib1 = static_cast<uint8_t>(std::clamp(d1, -8, 7) + 8) & 0x0F;

        out_payload[offset++] = static_cast<uint8_t>((nib0 << 4) | nib1);
    }

    // Total bytes written: 1 + 6 + 48 = 55 bytes
    return offset;
}

bool SemanticDeltaCodec::decode_delta(const uint8_t* payload,
                                     size_t payload_size,
                                     const SemanticFrameVector& reference,
                                     SemanticFrameVector& out_current) {
    if (payload_size < 55) return false;

    size_t offset = 0;

    // 1. Header flag
    out_current.flags = payload[offset++];

    // 2. 6-DoF Pose deltas
    for (size_t i = 0; i < 6; ++i) {
        int32_t delta = static_cast<int32_t>(payload[offset++]) - 128;
        int32_t val = static_cast<int32_t>(reference.pose_6dof[i]) + (delta << 4);
        out_current.pose_6dof[i] = static_cast<int16_t>(std::clamp(val, -32767, 32767));
    }

    // 3. 3D Keypoint deltas
    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; i += 2) {
        uint8_t byte = payload[offset++];
        int32_t d0 = static_cast<int32_t>((byte >> 4) & 0x0F) - 8;
        int32_t d1 = static_cast<int32_t>(byte & 0x0F) - 8;

        int32_t v0 = static_cast<int32_t>(reference.keypoints_3d[i]) + (d0 << 8);
        int32_t v1 = static_cast<int32_t>(reference.keypoints_3d[i + 1]) + (d1 << 8);

        out_current.keypoints_3d[i] = static_cast<int16_t>(std::clamp(v0, -32767, 32767));
        out_current.keypoints_3d[i + 1] = static_cast<int16_t>(std::clamp(v1, -32767, 32767));
    }

    // Blendshapes and audio tokens copy or synthesize
    out_current.blendshapes = reference.blendshapes;
    out_current.audio_tokens = reference.audio_tokens;

    return true;
}

size_t SemanticDeltaCodec::encode_sequence(const std::vector<SemanticFrameVector>& sequence,
                                          uint8_t* out_payload,
                                          size_t max_payload_size) {
    if (sequence.empty() || !out_payload) return 0;

    const size_t num_frames = sequence.size();
    const size_t required_size = 7 + 265 + (num_frames > 1 ? (num_frames - 1) * 55 : 0);
    if (max_payload_size < required_size) return 0;

    size_t offset = 0;

    // 1. Magic & Header
    out_payload[offset++] = 'S';
    out_payload[offset++] = 'E';
    out_payload[offset++] = 'M';
    out_payload[offset++] = 'S';
    out_payload[offset++] = 0x01; // Version 1

    uint16_t n_frames_u16 = static_cast<uint16_t>(std::min<size_t>(num_frames, 65535));
    out_payload[offset++] = static_cast<uint8_t>((n_frames_u16 >> 8) & 0xFF);
    out_payload[offset++] = static_cast<uint8_t>(n_frames_u16 & 0xFF);

    // 2. Anchor Frame (frame 0)
    const auto& anchor = sequence[0];
    out_payload[offset++] = anchor.flags | 0x01;

    for (size_t i = 0; i < 6; ++i) {
        uint16_t p = static_cast<uint16_t>(anchor.pose_6dof[i]);
        out_payload[offset++] = static_cast<uint8_t>((p >> 8) & 0xFF);
        out_payload[offset++] = static_cast<uint8_t>(p & 0xFF);
    }

    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        uint16_t kp = static_cast<uint16_t>(anchor.keypoints_3d[i]);
        out_payload[offset++] = static_cast<uint8_t>((kp >> 8) & 0xFF);
        out_payload[offset++] = static_cast<uint8_t>(kp & 0xFF);
    }

    for (size_t i = 0; i < SemanticFrameVector::BLENDSHAPE_COUNT; ++i) {
        out_payload[offset++] = anchor.blendshapes[i];
    }

    for (size_t i = 0; i < SemanticFrameVector::AUDIO_TOKEN_COUNT; ++i) {
        out_payload[offset++] = static_cast<uint8_t>(anchor.audio_tokens[i]);
    }

    // 3. Differential Delta Frames (frames 1 .. N-1)
    for (size_t f = 1; f < n_frames_u16; ++f) {
        size_t delta_bytes = encode_delta(sequence[f], sequence[f - 1],
                                          out_payload + offset,
                                          max_payload_size - offset);
        if (delta_bytes == 0) return 0;
        offset += delta_bytes;
    }

    return offset;
}

bool SemanticDeltaCodec::decode_sequence(const uint8_t* payload,
                                        size_t payload_size,
                                        std::vector<SemanticFrameVector>& out_sequence) {
    if (!payload || payload_size < 7 + 265) return false;

    size_t offset = 0;

    if (payload[0] != 'S' || payload[1] != 'E' || payload[2] != 'M' || payload[3] != 'S') {
        return false;
    }
    offset += 4;

    uint8_t version = payload[offset++];
    if (version != 0x01) return false;

    uint16_t num_frames = (static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1];
    offset += 2;

    if (num_frames == 0) {
        out_sequence.clear();
        return true;
    }

    const size_t expected_size = 7 + 265 + (num_frames > 1 ? (num_frames - 1) * 55 : 0);
    if (payload_size < expected_size) return false;

    out_sequence.resize(num_frames);

    auto& anchor = out_sequence[0];
    anchor.flags = payload[offset++];

    for (size_t i = 0; i < 6; ++i) {
        int16_t p = static_cast<int16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1]);
        offset += 2;
        anchor.pose_6dof[i] = p;
    }

    for (size_t i = 0; i < SemanticFrameVector::KEYPOINT_COUNT * 3; ++i) {
        int16_t kp = static_cast<int16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1]);
        offset += 2;
        anchor.keypoints_3d[i] = kp;
    }

    for (size_t i = 0; i < SemanticFrameVector::BLENDSHAPE_COUNT; ++i) {
        anchor.blendshapes[i] = payload[offset++];
    }

    for (size_t i = 0; i < SemanticFrameVector::AUDIO_TOKEN_COUNT; ++i) {
        anchor.audio_tokens[i] = static_cast<int8_t>(payload[offset++]);
    }

    for (size_t f = 1; f < num_frames; ++f) {
        if (!decode_delta(payload + offset, 55, out_sequence[f - 1], out_sequence[f])) {
            return false;
        }
        offset += 55;
    }

    return true;
}

} // namespace prism
