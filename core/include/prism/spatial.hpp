#pragma once
#ifndef PRISM_SPATIAL_HPP
#define PRISM_SPATIAL_HPP

#include "prism/types.hpp"
#include "prism/rans8.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <cmath>
#include <string>

namespace prism::spatial {

/**
 * @brief Discrete 3D Gaussian primitive evaluated at a specific frame/time instant.
 */
struct Gaussian3D {
    float p[3];               ///< Position centroid (X, Y, Z) in meters
    float q[4];               ///< Unit quaternion rotation (w, x, y, z)
    float s[3];               ///< Log-scale semi-axes (ln(sigma_x), ln(sigma_y), ln(sigma_z))
    float alpha;              ///< Opacity in [0.0, 1.0]
    uint8_t dc_yuv[3];        ///< DC base diffuse color (Y, Cb, Cr) in [0, 255]
    uint8_t sh_codebook_idx;  ///< Quantized specular Spherical Harmonics codebook index
    uint8_t flags;            ///< Bitmask: 0x01 = is_static, 0x02 = has_specular, 0x04 = active
};

/**
 * @brief Continuous Low-Rank Chebyshev Trajectory over a GOP interval.
 *
 * Models p(tau) = p_0 + sum_{k=1}^K c_k * T_k(tau), where tau in [-1, +1]
 * T_0(tau) = 1
 * T_1(tau) = tau
 * T_2(tau) = 2*tau^2 - 1
 * T_3(tau) = 4*tau^3 - 3*tau
 */
struct ChebyshevTrajectory {
    static constexpr size_t MAX_DEGREE = 3;

    float p0[3] = {0.0f, 0.0f, 0.0f};           ///< Reference centroid at tau = 0
    float c[MAX_DEGREE][3] = {{0.0f}};           ///< Chebyshev polynomial coefficients for k=1..3
    float q0[4] = {1.0f, 0.0f, 0.0f, 0.0f};      ///< Base quaternion orientation
    float delta_q[4] = {0.0f, 0.0f, 0.0f, 0.0f}; ///< Angular velocity quaternion increment
    float s[3] = {0.0f, 0.0f, 0.0f};            ///< Scale
    float alpha = 1.0f;                          ///< Opacity
    uint8_t dc_yuv[3] = {128, 128, 128};         ///< Base color
    uint8_t sh_idx = 0;                          ///< Specular index
    bool is_static = false;                      ///< Static anchor classification

    /**
     * @brief Evaluate continuous centroid position at normalized time tau in [-1, +1].
     */
    void evaluate_position(float tau, float out_p[3]) const;

    /**
     * @brief Evaluate continuous unit quaternion orientation at normalized time tau in [-1, +1].
     */
    void evaluate_rotation(float tau, float out_q[4]) const;

    /**
     * @brief Compute kinematic motion energy: E = sum_{k=1}^K k^2 * ||c_k||^2.
     */
    float kinematic_energy() const;

    /**
     * @brief Classify whether this Gaussian is a motionless static anchor.
     */
    bool check_static(float threshold_energy = 0.0015f) const;
};

/**
 * @brief Dead-Zone Hyper-Lattice Vector Quantizer (DZ-HLVQ).
 *
 * Zeroes residual velocity and rotation vectors whose Euclidean norm is within
 * the dead-zone radius tau_dead, achieving massive sparsity (>80%) on micro-motion.
 */
class DeadZoneVectorQuantizer {
public:
    explicit DeadZoneVectorQuantizer(float step_size = 0.002f, float dead_zone_factor = 1.8f);

    /**
     * @brief Quantize a 3D residual vector. If ||v|| < tau_dead, returns {0, 0, 0}.
     */
    std::array<int16_t, 3> quantize(const float v[3]) const;

    /**
     * @brief Dequantize an integer residual code back into floating-point coordinates.
     */
    void dequantize(const std::array<int16_t, 3>& q, float out_v[3]) const;

    /**
     * @brief Quantize a batch of 3D velocity vectors.
     */
    std::vector<std::array<int16_t, 3>> quantize_batch(const std::vector<std::array<float, 3>>& vectors) const;

    /**
     * @brief Dequantize a batch of 3D integer codes back into floating-point vectors.
     */
    std::vector<std::array<float, 3>> dequantize_batch(const std::vector<std::array<int16_t, 3>>& codes) const;

    /**
     * @brief Calculate zero-sparsity fraction (count of exact zero vectors / total).
     */
    static float calculate_sparsity(const std::vector<std::array<int16_t, 3>>& codes);

    float step_size() const { return step_size_; }
    float dead_zone_radius() const { return dead_zone_radius_; }

private:
    float step_size_;
    float dead_zone_radius_;
};

/**
 * @brief Spherical Harmonics Codec (EASH-SAC).
 *
 * Handles DC YUV conversion, Hessian sensitivity-weighted specular pruning,
 * and vector codebook quantization for degrees 2 and 3.
 */
class SphericalHarmonicsCodec {
public:
    static constexpr size_t AC_COEFFS_COUNT = 36; // Degrees 2 & 3: 5 + 7 = 12 basis functions * 3 color channels

    /**
     * @brief Convert linear RGB [0..1] to 8-bit YUV [0..255] (BT.709 standard matrix).
     */
    static void rgb_to_yuv8(const float rgb[3], uint8_t yuv[3]);

    /**
     * @brief Convert 8-bit YUV [0..255] back to linear RGB [0..1].
     */
    static void yuv8_to_rgb(const uint8_t yuv[3], float rgb[3]);

    /**
     * @brief Compute viewing sensitivity from gradient energy.
     */
    static float compute_sensitivity(const float* grad_sh, size_t count);

    /**
     * @brief Quantize 36 AC specular coefficients to the nearest 8-bit codebook centroid.
     */
    static uint8_t quantize_specular(const float ac_coeffs[AC_COEFFS_COUNT],
                                    const std::vector<std::array<float, AC_COEFFS_COUNT>>& codebook);

    /**
     * @brief Dequantize 8-bit codebook index into 36 floating-point AC coefficients.
     */
    static void dequantize_specular(uint8_t index,
                                   const std::vector<std::array<float, AC_COEFFS_COUNT>>& codebook,
                                   float out_coeffs[AC_COEFFS_COUNT]);
};

/**
 * @brief Dynamic 4D Gaussian Splatting Field Sequence (Prism-Spatial Engine).
 *
 * Encapsulates static anchors, dynamic polynomial trajectories, dead-zone residuals,
 * and bit-exact rANS serialization.
 */
class DynamicGaussianField {
public:
    DynamicGaussianField() = default;

    /**
     * @brief Add a Gaussian trajectory to the field.
     */
    void add_trajectory(const ChebyshevTrajectory& traj);

    /**
     * @brief Total number of Gaussians in the field.
     */
    size_t count() const { return trajectories_.size(); }

    /**
     * @brief Count of motionless static anchors.
     */
    size_t static_anchor_count() const;

    /**
     * @brief Count of dynamic moving Gaussians.
     */
    size_t dynamic_splat_count() const;

    /**
     * @brief Evaluate the 4D field at continuous timestamp t in [t_start, t_end].
     * @param t Target timestamp in seconds.
     * @param t_start GOP start timestamp.
     * @param t_end GOP end timestamp.
     * @param out_gaussians Destination vector populated with 3D Gaussian states.
     */
    void evaluate_at_timestamp(float t, float t_start, float t_end,
                              std::vector<Gaussian3D>& out_gaussians) const;

    /**
     * @brief Compress the field's static masks and quantized dynamics using 8-way rANS.
     * @return Compressed byte stream with header.
     */
    std::vector<uint8_t> compress_to_bitstream(const DeadZoneVectorQuantizer& dz_quantizer) const;

    /**
     * @brief Decompress bitstream back into a DynamicGaussianField.
     */
    static DynamicGaussianField decompress_from_bitstream(const uint8_t* data, size_t size,
                                                         const DeadZoneVectorQuantizer& dz_quantizer);

    const std::vector<ChebyshevTrajectory>& trajectories() const { return trajectories_; }

private:
    std::vector<ChebyshevTrajectory> trajectories_;
};

/**
 * @brief 6-DoF Camera Pose Representation for Spatial Rendering.
 */
struct SpatialCameraPose {
    float pitch = 0.0f;          ///< Pitch Euler angle in radians
    float yaw = 0.0f;            ///< Yaw Euler angle in radians
    float roll = 0.0f;           ///< Roll Euler angle in radians
    float tx = 0.0f;             ///< Translation X in meters
    float ty = 0.0f;             ///< Translation Y in meters
    float tz = 0.0f;             ///< Translation Z in meters
    float fov_y = 1.04719755f;   ///< Vertical Field of View (60 degrees default)
    float virtual_ipd_mm = 63.0f;///< Inter-pupillary distance in millimeters

    /**
     * @brief Compute 4x4 View Matrix (column-major).
     * @param out_mat4 Destination 16-element array.
     * @param eye_idx 0 = Left / Center eye, 1 = Right eye (shifted by +IPD).
     */
    void compute_view_matrix(float out_mat4[16], int eye_idx = 0) const;

    /**
     * @brief Compute 4x4 Perspective Projection Matrix (column-major).
     */
    void compute_proj_matrix(float aspect_ratio, float near_z, float far_z, float out_mat4[16]) const;
};

/**
 * @brief Multi-Lens Epipolar Flow-Lifter (MEF-LAD).
 *
 * Ingests calibrated multi-lens smartphone parameters, 2D optical flow fields,
 * and metric depth deltas to lift 2D motion into continuous 3D velocity vectors.
 */
class EpipolarFlowLifter {
public:
    EpipolarFlowLifter(float fx = 1500.0f, float fy = 1500.0f,
                       float cx = 960.0f, float cy = 540.0f,
                       float baseline_m = 0.02f);

    /**
     * @brief Lift 2D pixel motion and depth delta into 3D metric velocity vector:
     * v_x = ((u - cx)/fx) * dot_z + (z / fx) * flow_u
     * v_y = ((v - cy)/fy) * dot_z + (z / fy) * flow_v
     * v_z = dot_z
     */
    void lift_2d_flow_to_3d_velocity(float u, float v, float z, float dot_z,
                                     float flow_u, float flow_v,
                                     float out_v[3]) const;

    float fx() const { return fx_; }
    float fy() const { return fy_; }
    float baseline() const { return baseline_m_; }

private:
    float fx_;
    float fy_;
    float cx_;
    float cy_;
    float baseline_m_;
};

/**
 * @brief Low-Rank Trajectory Subspace Factorizer (LR-HCP).
 *
 * Projects 9-dimensional dynamic Chebyshev trajectory polynomials into a
 * compact 4-dimensional orthonormal subspace basis (Phi in R^{9x4}).
 */
class LowRankSubspaceFactorizer {
public:
    static constexpr size_t FULL_DIM = 9;   // 3 coefficients * 3 coordinates (X, Y, Z)
    static constexpr size_t SUBSPACE_DIM = 4; // M = 4 latent channels

    struct SubspaceModel {
        std::array<std::array<float, SUBSPACE_DIM>, FULL_DIM> basis{};
    };

    /**
     * @brief Project 9-D polynomial coefficients onto 4-D latent subspace weights.
     */
    static void project_to_subspace(const float c[FULL_DIM],
                                   const SubspaceModel& model,
                                   float out_w[SUBSPACE_DIM]);

    /**
     * @brief Reconstruct 9-D polynomial coefficients from 4-D latent subspace weights.
     */
    static void reconstruct_from_subspace(const float w[SUBSPACE_DIM],
                                         const SubspaceModel& model,
                                         float out_c[FULL_DIM]);

    /**
     * @brief Fit an orthonormal subspace basis from a batch of dynamic trajectories.
     */
    static SubspaceModel fit_subspace(const std::vector<std::array<float, FULL_DIM>>& trajectories);
};

} // namespace prism::spatial

#endif // PRISM_SPATIAL_HPP
