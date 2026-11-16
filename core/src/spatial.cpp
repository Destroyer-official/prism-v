#include "prism/spatial.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace prism::spatial {

void ChebyshevTrajectory::evaluate_position(float tau, float out_p[3]) const {
    const float t = std::clamp(tau, -1.0f, 1.0f);
    if (is_static) {
        out_p[0] = p0[0];
        out_p[1] = p0[1];
        out_p[2] = p0[2];
        return;
    }

    // Chebyshev basis polynomials of the first kind
    const float t1 = t;
    const float t2 = 2.0f * t * t - 1.0f;
    const float t3 = 4.0f * t * t * t - 3.0f * t;

    for (int i = 0; i < 3; ++i) {
        out_p[i] = p0[i] + (c[0][i] * t1) + (c[1][i] * t2) + (c[2][i] * t3);
    }
}

void ChebyshevTrajectory::evaluate_rotation(float tau, float out_q[4]) const {
    const float t = std::clamp(tau, -1.0f, 1.0f);
    if (is_static) {
        out_q[0] = q0[0];
        out_q[1] = q0[1];
        out_q[2] = q0[2];
        out_q[3] = q0[3];
        return;
    }

    // SLERP approximation: linear interpolation + normalization
    float raw_q[4];
    for (int i = 0; i < 4; ++i) {
        raw_q[i] = q0[i] + delta_q[i] * t;
    }

    float norm = std::sqrt(raw_q[0] * raw_q[0] + raw_q[1] * raw_q[1] +
                           raw_q[2] * raw_q[2] + raw_q[3] * raw_q[3]);
    if (norm < 1e-6f) {
        out_q[0] = 1.0f;
        out_q[1] = 0.0f;
        out_q[2] = 0.0f;
        out_q[3] = 0.0f;
    } else {
        const float inv_norm = 1.0f / norm;
        out_q[0] = raw_q[0] * inv_norm;
        out_q[1] = raw_q[1] * inv_norm;
        out_q[2] = raw_q[2] * inv_norm;
        out_q[3] = raw_q[3] * inv_norm;
    }
}

float ChebyshevTrajectory::kinematic_energy() const {
    float total_energy = 0.0f;
    for (size_t k = 1; k <= MAX_DEGREE; ++k) {
        const float weight = static_cast<float>(k * k);
        const float sq_norm = c[k - 1][0] * c[k - 1][0] +
                              c[k - 1][1] * c[k - 1][1] +
                              c[k - 1][2] * c[k - 1][2];
        total_energy += weight * sq_norm;
    }
    return total_energy;
}

bool ChebyshevTrajectory::check_static(float threshold_energy) const {
    return kinematic_energy() < threshold_energy;
}

DeadZoneVectorQuantizer::DeadZoneVectorQuantizer(float step_size, float dead_zone_factor)
    : step_size_(step_size > 1e-6f ? step_size : 0.002f),
      dead_zone_radius_(dead_zone_factor * step_size_) {}

std::array<int16_t, 3> DeadZoneVectorQuantizer::quantize(const float v[3]) const {
    const float norm = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    // Dead-zone condition: vectors inside hypersphere map to exact {0, 0, 0}
    if (norm < dead_zone_radius_) {
        return {0, 0, 0};
    }

    std::array<int16_t, 3> code;
    for (int i = 0; i < 3; ++i) {
        float q = std::round(v[i] / step_size_);
        code[i] = static_cast<int16_t>(std::clamp(q, -32767.0f, 32767.0f));
    }
    return code;
}

void DeadZoneVectorQuantizer::dequantize(const std::array<int16_t, 3>& q, float out_v[3]) const {
    for (int i = 0; i < 3; ++i) {
        out_v[i] = static_cast<float>(q[i]) * step_size_;
    }
}

std::vector<std::array<int16_t, 3>> DeadZoneVectorQuantizer::quantize_batch(
    const std::vector<std::array<float, 3>>& vectors) const {
    std::vector<std::array<int16_t, 3>> result;
    result.reserve(vectors.size());
    for (const auto& v : vectors) {
        result.push_back(quantize(v.data()));
    }
    return result;
}

std::vector<std::array<float, 3>> DeadZoneVectorQuantizer::dequantize_batch(
    const std::vector<std::array<int16_t, 3>>& codes) const {
    std::vector<std::array<float, 3>> result;
    result.reserve(codes.size());
    for (const auto& q : codes) {
        std::array<float, 3> v;
        dequantize(q, v.data());
        result.push_back(v);
    }
    return result;
}

float DeadZoneVectorQuantizer::calculate_sparsity(
    const std::vector<std::array<int16_t, 3>>& codes) {
    if (codes.empty()) return 1.0f;
    size_t zero_count = 0;
    for (const auto& c : codes) {
        if (c[0] == 0 && c[1] == 0 && c[2] == 0) {
            zero_count++;
        }
    }
    return static_cast<float>(zero_count) / static_cast<float>(codes.size());
}

void SphericalHarmonicsCodec::rgb_to_yuv8(const float rgb[3], uint8_t yuv[3]) {
    const float r = std::clamp(rgb[0], 0.0f, 1.0f);
    const float g = std::clamp(rgb[1], 0.0f, 1.0f);
    const float b = std::clamp(rgb[2], 0.0f, 1.0f);

    // BT.709 Matrix
    const float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float u = -0.1146f * r - 0.3854f * g + 0.5000f * b + 0.5f;
    const float v = 0.5000f * r - 0.4542f * g - 0.0458f * b + 0.5f;

    yuv[0] = static_cast<uint8_t>(std::clamp(std::round(y * 255.0f), 0.0f, 255.0f));
    yuv[1] = static_cast<uint8_t>(std::clamp(std::round(u * 255.0f), 0.0f, 255.0f));
    yuv[2] = static_cast<uint8_t>(std::clamp(std::round(v * 255.0f), 0.0f, 255.0f));
}

void SphericalHarmonicsCodec::yuv8_to_rgb(const uint8_t yuv[3], float rgb[3]) {
    const float y = static_cast<float>(yuv[0]) / 255.0f;
    const float u = (static_cast<float>(yuv[1]) / 255.0f) - 0.5f;
    const float v = (static_cast<float>(yuv[2]) / 255.0f) - 0.5f;

    // BT.709 Inverse Matrix
    const float r = y + 1.5748f * v;
    const float g = y - 0.1873f * u - 0.4681f * v;
    const float b = y + 1.8556f * u;

    rgb[0] = std::clamp(r, 0.0f, 1.0f);
    rgb[1] = std::clamp(g, 0.0f, 1.0f);
    rgb[2] = std::clamp(b, 0.0f, 1.0f);
}

float SphericalHarmonicsCodec::compute_sensitivity(const float* grad_sh, size_t count) {
    if (!grad_sh || count == 0) return 0.0f;
    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum_sq += grad_sh[i] * grad_sh[i];
    }
    return std::sqrt(sum_sq);
}

uint8_t SphericalHarmonicsCodec::quantize_specular(
    const float ac_coeffs[AC_COEFFS_COUNT],
    const std::vector<std::array<float, AC_COEFFS_COUNT>>& codebook) {
    if (codebook.empty()) return 0;

    float min_dist_sq = 1e30f;
    uint8_t best_idx = 0;

    const size_t num_entries = std::min(codebook.size(), static_cast<size_t>(256));
    for (size_t i = 0; i < num_entries; ++i) {
        float dist_sq = 0.0f;
        for (size_t j = 0; j < AC_COEFFS_COUNT; ++j) {
            const float diff = ac_coeffs[j] - codebook[i][j];
            dist_sq += diff * diff;
        }
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            best_idx = static_cast<uint8_t>(i);
        }
    }
    return best_idx;
}

void SphericalHarmonicsCodec::dequantize_specular(
    uint8_t index,
    const std::vector<std::array<float, AC_COEFFS_COUNT>>& codebook,
    float out_coeffs[AC_COEFFS_COUNT]) {
    if (codebook.empty()) {
        std::memset(out_coeffs, 0, AC_COEFFS_COUNT * sizeof(float));
        return;
    }
    const size_t idx = std::min(static_cast<size_t>(index), codebook.size() - 1);
    std::memcpy(out_coeffs, codebook[idx].data(), AC_COEFFS_COUNT * sizeof(float));
}

void DynamicGaussianField::add_trajectory(const ChebyshevTrajectory& traj) {
    trajectories_.push_back(traj);
}

size_t DynamicGaussianField::static_anchor_count() const {
    size_t count = 0;
    for (const auto& t : trajectories_) {
        if (t.is_static) count++;
    }
    return count;
}

size_t DynamicGaussianField::dynamic_splat_count() const {
    return trajectories_.size() - static_anchor_count();
}

void DynamicGaussianField::evaluate_at_timestamp(
    float t, float t_start, float t_end,
    std::vector<Gaussian3D>& out_gaussians) const {

    const float duration = (t_end > t_start) ? (t_end - t_start) : 1.0f;
    const float tau = std::clamp((2.0f * (t - t_start) / duration) - 1.0f, -1.0f, 1.0f);

    out_gaussians.resize(trajectories_.size());
    for (size_t i = 0; i < trajectories_.size(); ++i) {
        const auto& traj = trajectories_[i];
        auto& g = out_gaussians[i];

        traj.evaluate_position(tau, g.p);
        traj.evaluate_rotation(tau, g.q);

        g.s[0] = traj.s[0];
        g.s[1] = traj.s[1];
        g.s[2] = traj.s[2];

        g.alpha = traj.alpha;
        g.dc_yuv[0] = traj.dc_yuv[0];
        g.dc_yuv[1] = traj.dc_yuv[1];
        g.dc_yuv[2] = traj.dc_yuv[2];

        g.sh_codebook_idx = traj.sh_idx;
        g.flags = (traj.is_static ? 0x01 : 0x00) | 0x04;
    }
}

std::vector<uint8_t> DynamicGaussianField::compress_to_bitstream(
    const DeadZoneVectorQuantizer& dz_quantizer) const {

    std::vector<uint8_t> bitstream;
    // Magic '4dgs' (0x34646773)
    const uint32_t magic = BOX_TYPE_4DGS;
    const uint32_t num_splats = static_cast<uint32_t>(trajectories_.size());
    const float step_size = dz_quantizer.step_size();

    // Stream 1: Static flag symbols (0 = dynamic, 1 = static)
    std::vector<uint16_t> static_symbols(num_splats);
    size_t static_count = 0;
    for (size_t i = 0; i < num_splats; ++i) {
        static_symbols[i] = trajectories_[i].is_static ? 1 : 0;
        if (trajectories_[i].is_static) static_count++;
    }

    // Build rANS stats for static flags (alphabet size 2)
    std::vector<RansSymbol> static_stats(2);
    uint16_t freq_static = static_cast<uint16_t>(
        std::clamp(static_cast<uint32_t>((static_count * Rans8Codec::RANS_M) / num_splats), 1U, Rans8Codec::RANS_M - 1));
    static_stats[0] = {0, static_cast<uint16_t>(Rans8Codec::RANS_M - freq_static)};
    static_stats[1] = {static_stats[0].freq, freq_static};

    auto static_chunk = Rans8Codec::encode(static_symbols, static_stats);
    auto static_bytes = Rans8Codec::serialize(static_chunk);

    // Serialize Header: Magic(4) + SplatCount(4) + StepSize(4) + FreqStatic(2) + Reserved(2)
    bitstream.resize(16);
    std::memcpy(bitstream.data() + 0, &magic, 4);
    std::memcpy(bitstream.data() + 4, &num_splats, 4);
    std::memcpy(bitstream.data() + 8, &step_size, 4);
    std::memcpy(bitstream.data() + 12, &freq_static, 2);
    uint16_t reserved = 0;
    std::memcpy(bitstream.data() + 14, &reserved, 2);

    // Stream 2: Quantized dynamic residuals
    // Pack trajectory centroids and polynomial coefficients
    std::vector<uint8_t> dynamic_payload;
    for (const auto& traj : trajectories_) {
        // Position reference
        for (int d = 0; d < 3; ++d) {
            float p_val = traj.p0[d];
            uint8_t raw[4];
            std::memcpy(raw, &p_val, 4);
            dynamic_payload.insert(dynamic_payload.end(), raw, raw + 4);
        }
        if (!traj.is_static) {
            // Quantize polynomial c coefficients with dead-zone quantizer
            for (int k = 0; k < 3; ++k) {
                auto q_vec = dz_quantizer.quantize(traj.c[k]);
                uint8_t q_raw[6];
                std::memcpy(q_raw, q_vec.data(), 6);
                dynamic_payload.insert(dynamic_payload.end(), q_raw, q_raw + 6);
            }
        }
        // DC color and specular index
        dynamic_payload.push_back(traj.dc_yuv[0]);
        dynamic_payload.push_back(traj.dc_yuv[1]);
        dynamic_payload.push_back(traj.dc_yuv[2]);
        dynamic_payload.push_back(traj.sh_idx);
    }

    // Mux into final bitstream: [Header: 16B][StaticLen: 4B][StaticData][DynLen: 4B][DynData]
    const uint32_t static_len = static_cast<uint32_t>(static_bytes.size());
    const uint32_t dyn_len = static_cast<uint32_t>(dynamic_payload.size());

    size_t cur_offset = bitstream.size(); // 16
    bitstream.resize(cur_offset + 8 + static_len + dyn_len);

    std::memcpy(bitstream.data() + cur_offset, &static_len, 4);
    std::memcpy(bitstream.data() + cur_offset + 4, static_bytes.data(), static_len);

    cur_offset += 4 + static_len;
    std::memcpy(bitstream.data() + cur_offset, &dyn_len, 4);
    std::memcpy(bitstream.data() + cur_offset + 4, dynamic_payload.data(), dyn_len);

    return bitstream;
}

DynamicGaussianField DynamicGaussianField::decompress_from_bitstream(
    const uint8_t* data, size_t size,
    const DeadZoneVectorQuantizer& dz_quantizer) {

    if (!data || size < 24) {
        throw std::runtime_error("Invalid Prism-Spatial 4DGS bitstream size");
    }

    uint32_t magic = 0, num_splats = 0;
    float step_size = 0.0f;
    uint16_t freq_static = 16384;
    std::memcpy(&magic, data + 0, 4);
    std::memcpy(&num_splats, data + 4, 4);
    std::memcpy(&step_size, data + 8, 4);
    std::memcpy(&freq_static, data + 12, 2);

    if (magic != BOX_TYPE_4DGS) {
        throw std::runtime_error("Invalid 4DGS box magic header");
    }

    DynamicGaussianField field;
    if (num_splats == 0) return field;

    uint32_t static_len = 0;
    std::memcpy(&static_len, data + 16, 4);
    if (20ULL + static_len + 4ULL > size) {
        throw std::runtime_error("Corrupted static chunk length: static_len=" + std::to_string(static_len) + " size=" + std::to_string(size));
    }

    // Deserialize and decode static flags with rANS
    auto static_chunk = Rans8Codec::deserialize(data + 20, static_len);

    // Exact frequency stats from header
    std::vector<RansSymbol> static_stats(2);
    static_stats[0] = {0, static_cast<uint16_t>(Rans8Codec::RANS_M - freq_static)};
    static_stats[1] = {static_stats[0].freq, freq_static};
    auto cdf_lut = Rans8Codec::build_cdf_lut(static_stats);
    auto static_symbols = Rans8Codec::decode(static_chunk, cdf_lut, static_stats);

    const size_t dyn_offset = 20 + static_len;
    uint32_t dyn_len = 0;
    std::memcpy(&dyn_len, data + dyn_offset, 4);
    if (dyn_offset + 4 + dyn_len > size) {
        throw std::runtime_error("Corrupted dynamic payload length");
    }

    const uint8_t* dyn_ptr = data + dyn_offset + 4;
    const uint8_t* dyn_end = dyn_ptr + dyn_len;

    field.trajectories_.reserve(num_splats);
    for (size_t i = 0; i < num_splats && dyn_ptr < dyn_end; ++i) {
        ChebyshevTrajectory traj;
        traj.is_static = (i < static_symbols.size() && static_symbols[i] == 1);

        // Position reference
        if (dyn_ptr + 12 > dyn_end) break;
        std::memcpy(traj.p0, dyn_ptr, 12);
        dyn_ptr += 12;

        if (!traj.is_static) {
            // Dequantize polynomial coefficients
            if (dyn_ptr + 18 > dyn_end) break;
            for (int k = 0; k < 3; ++k) {
                std::array<int16_t, 3> q_vec;
                std::memcpy(q_vec.data(), dyn_ptr, 6);
                dyn_ptr += 6;
                dz_quantizer.dequantize(q_vec, traj.c[k]);
            }
        } else {
            std::memset(traj.c, 0, sizeof(traj.c));
        }

        // DC color and specular index
        if (dyn_ptr + 4 > dyn_end) break;
        traj.dc_yuv[0] = dyn_ptr[0];
        traj.dc_yuv[1] = dyn_ptr[1];
        traj.dc_yuv[2] = dyn_ptr[2];
        traj.sh_idx = dyn_ptr[3];
        dyn_ptr += 4;

        field.add_trajectory(traj);
    }

    return field;
}

void SpatialCameraPose::compute_view_matrix(float out_mat4[16], int eye_idx) const {
    // Eye offset along local horizontal axis (IPD in meters)
    float ipd_m = (virtual_ipd_mm * 0.001f);
    float eye_shift_x = (eye_idx == 1) ? (+0.5f * ipd_m) : (-0.5f * ipd_m);

    // Compute rotation angles
    float cp = std::cos(pitch);
    float sp = std::sin(pitch);
    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float cr = std::cos(roll);
    float sr = std::sin(roll);

    // R = Ry * Rx * Rz (Y-up convention)
    float r00 = cy * cr + sy * sp * sr;
    float r01 = sr * cp;
    float r02 = -sy * cr + cy * sp * sr;

    float r10 = -cy * sr + sy * sp * cr;
    float r11 = cr * cp;
    float r12 = sr * sy + cy * sp * cr;

    float r20 = sy * cp;
    float r21 = -sp;
    float r22 = cy * cp;

    // Camera position including eye offset
    float cam_x = tx + eye_shift_x;
    float cam_y = ty;
    float cam_z = tz;

    // View matrix V = [R^T | -R^T * C] (column-major)
    out_mat4[0] = r00;
    out_mat4[1] = r10;
    out_mat4[2] = r20;
    out_mat4[3] = 0.0f;

    out_mat4[4] = r01;
    out_mat4[5] = r11;
    out_mat4[6] = r21;
    out_mat4[7] = 0.0f;

    out_mat4[8] = r02;
    out_mat4[9] = r12;
    out_mat4[10] = r22;
    out_mat4[11] = 0.0f;

    out_mat4[12] = -(r00 * cam_x + r01 * cam_y + r02 * cam_z);
    out_mat4[13] = -(r10 * cam_x + r11 * cam_y + r12 * cam_z);
    out_mat4[14] = -(r20 * cam_x + r21 * cam_y + r22 * cam_z);
    out_mat4[15] = 1.0f;
}

void SpatialCameraPose::compute_proj_matrix(float aspect_ratio, float near_z, float far_z, float out_mat4[16]) const {
    float tan_half_fov = std::tan(fov_y * 0.5f);
    float f = 1.0f / (tan_half_fov > 1e-4f ? tan_half_fov : 1.0f);
    float aspect = aspect_ratio > 1e-4f ? aspect_ratio : 1.0f;

    std::memset(out_mat4, 0, 16 * sizeof(float));

    // Vulkan NDC convention: depth Z in [0, 1]
    out_mat4[0] = f / aspect;
    out_mat4[5] = -f; // Vulkan Y-flip
    out_mat4[10] = far_z / (near_z - far_z);
    out_mat4[11] = -1.0f;
    out_mat4[14] = (near_z * far_z) / (near_z - far_z);
}

EpipolarFlowLifter::EpipolarFlowLifter(float fx, float fy, float cx, float cy, float baseline_m)
    : fx_(fx > 1.0f ? fx : 1500.0f),
      fy_(fy > 1.0f ? fy : 1500.0f),
      cx_(cx > 0.0f ? cx : 960.0f),
      cy_(cy > 0.0f ? cy : 540.0f),
      baseline_m_(baseline_m > 0.001f ? baseline_m : 0.02f) {}

void EpipolarFlowLifter::lift_2d_flow_to_3d_velocity(
    float u, float v, float z, float dot_z,
    float flow_u, float flow_v,
    float out_v[3]) const {

    float safe_z = (std::abs(z) > 1e-3f) ? z : 1.0f;
    float norm_u = (u - cx_) / fx_;
    float norm_v = (v - cy_) / fy_;

    out_v[0] = norm_u * dot_z + (safe_z / fx_) * flow_u;
    out_v[1] = norm_v * dot_z + (safe_z / fy_) * flow_v;
    out_v[2] = dot_z;
}

void LowRankSubspaceFactorizer::project_to_subspace(
    const float c[FULL_DIM],
    const SubspaceModel& model,
    float out_w[SUBSPACE_DIM]) {

    for (size_t m = 0; m < SUBSPACE_DIM; ++m) {
        float sum = 0.0f;
        for (size_t d = 0; d < FULL_DIM; ++d) {
            sum += c[d] * model.basis[d][m];
        }
        out_w[m] = sum;
    }
}

void LowRankSubspaceFactorizer::reconstruct_from_subspace(
    const float w[SUBSPACE_DIM],
    const SubspaceModel& model,
    float out_c[FULL_DIM]) {

    for (size_t d = 0; d < FULL_DIM; ++d) {
        float sum = 0.0f;
        for (size_t m = 0; m < SUBSPACE_DIM; ++m) {
            sum += w[m] * model.basis[d][m];
        }
        out_c[d] = sum;
    }
}

LowRankSubspaceFactorizer::SubspaceModel LowRankSubspaceFactorizer::fit_subspace(
    const std::vector<std::array<float, FULL_DIM>>& trajectories) {

    SubspaceModel model;
    // Default to DCT-style orthonormal basis if empty or small dataset
    if (trajectories.empty()) {
        for (size_t d = 0; d < FULL_DIM; ++d) {
            for (size_t m = 0; m < SUBSPACE_DIM; ++m) {
                constexpr float PI_CONST = 3.14159265358979323846f;
                float factor = (m == 0) ? std::sqrt(1.0f / FULL_DIM)
                                        : std::sqrt(2.0f / FULL_DIM) * std::cos((PI_CONST * (2 * d + 1) * m) / (2.0f * FULL_DIM));
                model.basis[d][m] = factor;
            }
        }
        return model;
    }

    // Gram-Schmidt orthonormalization on first 4 components of trajectories
    for (size_t m = 0; m < SUBSPACE_DIM; ++m) {
        std::array<float, FULL_DIM> v{};
        if (m < trajectories.size()) {
            v = trajectories[m];
        } else {
            v[m % FULL_DIM] = 1.0f;
        }

        // Subtract projections onto prior basis vectors
        for (size_t p = 0; p < m; ++p) {
            float dot = 0.0f;
            for (size_t d = 0; d < FULL_DIM; ++d) {
                dot += v[d] * model.basis[d][p];
            }
            for (size_t d = 0; d < FULL_DIM; ++d) {
                v[d] -= dot * model.basis[d][p];
            }
        }

        // Normalize
        float norm_sq = 0.0f;
        for (size_t d = 0; d < FULL_DIM; ++d) {
            norm_sq += v[d] * v[d];
        }
        float inv_norm = (norm_sq > 1e-6f) ? (1.0f / std::sqrt(norm_sq)) : 1.0f;
        for (size_t d = 0; d < FULL_DIM; ++d) {
            model.basis[d][m] = v[d] * inv_norm;
        }
    }

    return model;
}

} // namespace prism::spatial
