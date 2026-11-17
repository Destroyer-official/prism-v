#include "test_harness.hpp"
#include "prism/spatial.hpp"
#include "prism/isobmff.hpp"
#include <vector>
#include <cmath>
#include <random>
#include <fstream>
#include <filesystem>

#ifndef PRISM_ASSERT_NEAR
#define PRISM_ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::abs((a) - (b)) > (eps)) { \
            throw std::runtime_error(std::string("Assertion failed: |") + #a + " - " + #b + "| <= " + #eps); \
        } \
    } while (0)
#endif

using namespace prism::spatial;

PRISM_TEST(test_chebyshev_polynomial_trajectory_evaluation) {
    ChebyshevTrajectory traj;
    traj.p0[0] = 1.0f; traj.p0[1] = 2.0f; traj.p0[2] = 3.0f;
    traj.c[0][0] = 0.5f; // T1 = tau
    traj.c[1][1] = 0.2f; // T2 = 2*tau^2 - 1
    traj.c[2][2] = 0.1f; // T3 = 4*tau^3 - 3*tau
    traj.is_static = false;

    // Test at tau = 0
    // T1(0) = 0, T2(0) = -1, T3(0) = 0
    float p_mid[3];
    traj.evaluate_position(0.0f, p_mid);
    PRISM_ASSERT_NEAR(p_mid[0], 1.0f, 1e-4f);
    PRISM_ASSERT_NEAR(p_mid[1], 2.0f - 0.2f, 1e-4f);
    PRISM_ASSERT_NEAR(p_mid[2], 3.0f, 1e-4f);

    // Test at tau = +1
    // T1(1) = 1, T2(1) = 1, T3(1) = 1
    float p_end[3];
    traj.evaluate_position(1.0f, p_end);
    PRISM_ASSERT_NEAR(p_end[0], 1.5f, 1e-4f);
    PRISM_ASSERT_NEAR(p_end[1], 2.2f, 1e-4f);
    PRISM_ASSERT_NEAR(p_end[2], 3.1f, 1e-4f);

    // Test at tau = -1
    // T1(-1) = -1, T2(-1) = 1, T3(-1) = -1
    float p_start[3];
    traj.evaluate_position(-1.0f, p_start);
    PRISM_ASSERT_NEAR(p_start[0], 0.5f, 1e-4f);
    PRISM_ASSERT_NEAR(p_start[1], 2.2f, 1e-4f);
    PRISM_ASSERT_NEAR(p_start[2], 2.9f, 1e-4f);
}

PRISM_TEST(test_chebyshev_analytical_velocity_derivatives) {
    ChebyshevTrajectory traj;
    traj.p0[0] = 1.0f; traj.p0[1] = 2.0f; traj.p0[2] = 3.0f;
    traj.c[0][0] = 0.5f; // T1 = tau
    traj.c[1][1] = 0.2f; // T2 = 2*tau^2 - 1
    traj.c[2][2] = 0.1f; // T3 = 4*tau^3 - 3*tau
    traj.is_static = false;

    constexpr float dt_sec = 1.0f;

    // At tau = 0:
    // T1'(0) = 1, T2'(0) = 0, T3'(0) = -3
    // v_x = (0.5 * 1) * (2/1) = 1.0
    // v_y = (0.2 * 0) * (2/1) = 0.0
    // v_z = (0.1 * -3) * (2/1) = -0.6
    float v_mid[3];
    traj.evaluate_velocity(0.0f, dt_sec, v_mid);
    PRISM_ASSERT_NEAR(v_mid[0], 1.0f, 1e-4f);
    PRISM_ASSERT_NEAR(v_mid[1], 0.0f, 1e-4f);
    PRISM_ASSERT_NEAR(v_mid[2], -0.6f, 1e-4f);

    // Compare analytical derivative against central finite difference
    constexpr float delta = 1e-4f;
    float p_plus[3], p_minus[3];
    traj.evaluate_position(delta, p_plus);
    traj.evaluate_position(-delta, p_minus);

    float num_v[3];
    for (int i = 0; i < 3; ++i) {
        num_v[i] = ((p_plus[i] - p_minus[i]) / (2.0f * delta)) * (2.0f / dt_sec);
        PRISM_ASSERT_NEAR(v_mid[i], num_v[i], 1e-3f);
    }
}

PRISM_TEST(test_chebyshev_unit_quaternion_normalization) {
    ChebyshevTrajectory traj;
    traj.q0[0] = 1.0f; traj.q0[1] = 0.0f; traj.q0[2] = 0.0f; traj.q0[3] = 0.0f;
    traj.delta_q[0] = 0.1f; traj.delta_q[1] = 0.2f; traj.delta_q[2] = -0.1f; traj.delta_q[3] = 0.3f;
    traj.is_static = false;

    // Test across 21 time slices in [-1, +1]
    for (int step = -10; step <= 10; ++step) {
        float tau = static_cast<float>(step) / 10.0f;
        float q[4];
        traj.evaluate_rotation(tau, q);
        float norm_sq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
        PRISM_ASSERT_NEAR(std::sqrt(norm_sq), 1.0f, 1e-5f);
    }
}

PRISM_TEST(test_chebyshev_static_anchor_classification) {
    ChebyshevTrajectory static_traj;
    static_traj.p0[0] = 5.0f;
    static_traj.c[0][0] = 0.0001f;
    static_traj.c[1][1] = 0.0002f;
    static_traj.c[2][2] = 0.0001f;

    PRISM_ASSERT(static_traj.kinematic_energy() < 0.0015f);
    PRISM_ASSERT(static_traj.check_static(0.0015f));

    ChebyshevTrajectory moving_traj;
    moving_traj.c[0][0] = 0.1f; // Large movement
    PRISM_ASSERT(moving_traj.kinematic_energy() > 0.0015f);
    PRISM_ASSERT(!moving_traj.check_static(0.0015f));
}

PRISM_TEST(test_dead_zone_vector_quantizer_sparsity) {
    DeadZoneVectorQuantizer quantizer(0.002f, 1.8f);
    const float tau_dead = quantizer.dead_zone_radius();
    PRISM_ASSERT(tau_dead > 0.0035f);

    // Vector with norm < tau_dead must map to exactly {0, 0, 0}
    float micro_noise[3] = {0.001f, -0.001f, 0.0005f};
    auto q_zero = quantizer.quantize(micro_noise);
    PRISM_ASSERT_EQ(q_zero[0], 0);
    PRISM_ASSERT_EQ(q_zero[1], 0);
    PRISM_ASSERT_EQ(q_zero[2], 0);

    // Vector with norm >= tau_dead must preserve direction
    float significant_motion[3] = {0.02f, 0.00f, -0.04f};
    auto q_motion = quantizer.quantize(significant_motion);
    PRISM_ASSERT(q_motion[0] != 0 || q_motion[1] != 0 || q_motion[2] != 0);

    float deq_v[3];
    quantizer.dequantize(q_motion, deq_v);
    PRISM_ASSERT_NEAR(deq_v[0], 0.02f, 0.002f);
    PRISM_ASSERT_NEAR(deq_v[2], -0.04f, 0.002f);

    // Batch Sparsity Check: 80% micro-noise, 20% significant motion
    std::vector<std::array<float, 3>> batch(100);
    for (size_t i = 0; i < 80; ++i) {
        batch[i] = {0.0005f, 0.0002f, -0.0004f};
    }
    for (size_t i = 80; i < 100; ++i) {
        batch[i] = {0.05f, 0.02f, -0.03f};
    }

    auto q_batch = quantizer.quantize_batch(batch);
    float sparsity = DeadZoneVectorQuantizer::calculate_sparsity(q_batch);
    PRISM_ASSERT(sparsity >= 0.80f);
}

PRISM_TEST(test_spherical_harmonics_dc_yuv_roundtrip) {
    float rgb_in[3] = {0.8f, 0.4f, 0.2f};
    uint8_t yuv[3];
    SphericalHarmonicsCodec::rgb_to_yuv8(rgb_in, yuv);

    float rgb_out[3];
    SphericalHarmonicsCodec::yuv8_to_rgb(yuv, rgb_out);

    // 8-bit YUV roundtrip precision is within 1/255 = 0.004
    PRISM_ASSERT_NEAR(rgb_out[0], rgb_in[0], 0.015f);
    PRISM_ASSERT_NEAR(rgb_out[1], rgb_in[1], 0.015f);
    PRISM_ASSERT_NEAR(rgb_out[2], rgb_in[2], 0.015f);
}

PRISM_TEST(test_dynamic_gaussian_field_4k60_evaluation_and_roundtrip) {
    DynamicGaussianField field;
    DeadZoneVectorQuantizer dz_quantizer(0.002f, 1.8f);

    // Populate 64 test splats (48 static background, 16 dynamic foreground)
    for (size_t i = 0; i < 64; ++i) {
        ChebyshevTrajectory traj;
        traj.p0[0] = static_cast<float>(i) * 0.1f;
        traj.p0[1] = 1.0f;
        traj.p0[2] = 2.0f;
        traj.alpha = 0.95f;
        traj.dc_yuv[0] = static_cast<uint8_t>(100 + (i % 100));
        traj.dc_yuv[1] = 128;
        traj.dc_yuv[2] = 128;

        if (i < 48) {
            traj.is_static = true;
        } else {
            traj.is_static = false;
            traj.c[0][0] = 0.05f;
            traj.c[1][1] = 0.02f;
        }
        field.add_trajectory(traj);
    }

    PRISM_ASSERT_EQ(field.count(), 64);
    PRISM_ASSERT_EQ(field.static_anchor_count(), 48);
    PRISM_ASSERT_EQ(field.dynamic_splat_count(), 16);

    // Continuous time evaluation at 60 fps (e.g. t = 0.25 s on interval [0.0, 1.0])
    std::vector<Gaussian3D> frame_splats;
    field.evaluate_at_timestamp(0.25f, 0.0f, 1.0f, frame_splats);
    PRISM_ASSERT_EQ(frame_splats.size(), 64);
    PRISM_ASSERT_NEAR(frame_splats[0].p[0], 0.0f, 1e-4f); // static splat 0 remains at p0

    // Compress with rANS and Dead-Zone VQ
    auto bitstream = field.compress_to_bitstream(dz_quantizer);
    PRISM_ASSERT(!bitstream.empty());

    // Decompress and verify
    auto restored_field = DynamicGaussianField::decompress_from_bitstream(
        bitstream.data(), bitstream.size(), dz_quantizer);

    PRISM_ASSERT_EQ(restored_field.count(), 64);
    PRISM_ASSERT_EQ(restored_field.static_anchor_count(), 48);
    PRISM_ASSERT_EQ(restored_field.dynamic_splat_count(), 16);

    // Compare positions at t = 0.25 s
    std::vector<Gaussian3D> restored_frame;
    restored_field.evaluate_at_timestamp(0.25f, 0.0f, 1.0f, restored_frame);
    PRISM_ASSERT_EQ(restored_frame.size(), 64);

    for (size_t i = 0; i < 64; ++i) {
        PRISM_ASSERT_NEAR(restored_frame[i].p[0], frame_splats[i].p[0], 0.01f);
        PRISM_ASSERT_NEAR(restored_frame[i].p[1], frame_splats[i].p[1], 0.01f);
        PRISM_ASSERT_NEAR(restored_frame[i].p[2], frame_splats[i].p[2], 0.01f);
        PRISM_ASSERT_EQ(restored_frame[i].dc_yuv[0], frame_splats[i].dc_yuv[0]);
    }
}

PRISM_TEST(test_epipolar_flow_lifting) {
    EpipolarFlowLifter lifter(1500.0f, 1500.0f, 960.0f, 540.0f, 0.02f);
    PRISM_ASSERT_NEAR(lifter.baseline(), 0.02f, 1e-4f);

    float u = 960.0f; // optical axis center
    float v = 540.0f;
    float z = 2.0f;   // 2 meters depth
    float dot_z = 0.1f; // moving backward at 0.1 m/s
    float flow_u = 15.0f; // 15 pixels/s horizontal flow
    float flow_v = 0.0f;

    float v_3d[3];
    lifter.lift_2d_flow_to_3d_velocity(u, v, z, dot_z, flow_u, flow_v, v_3d);

    // v_x = (0/1500)*0.1 + (2.0/1500)*15.0 = 2.0 * 0.01 = 0.02 m/s
    PRISM_ASSERT_NEAR(v_3d[0], 0.02f, 1e-4f);
    PRISM_ASSERT_NEAR(v_3d[1], 0.0f, 1e-4f);
    PRISM_ASSERT_NEAR(v_3d[2], 0.1f, 1e-4f);
}

PRISM_TEST(test_low_rank_subspace_factorization) {
    // 9-D trajectory polynomial
    std::array<float, 9> traj = {0.1f, 0.2f, 0.0f, -0.1f, 0.05f, 0.0f, 0.02f, -0.01f, 0.0f};

    std::vector<std::array<float, 9>> dataset = {traj};
    auto model = LowRankSubspaceFactorizer::fit_subspace(dataset);

    float w[4];
    LowRankSubspaceFactorizer::project_to_subspace(traj.data(), model, w);

    float recon[9];
    LowRankSubspaceFactorizer::reconstruct_from_subspace(w, model, recon);

    // Because dataset has 1 trajectory, fit_subspace aligns first basis vector exactly with traj
    for (size_t d = 0; d < 9; ++d) {
        PRISM_ASSERT_NEAR(recon[d], traj[d], 1e-3f);
    }
}

PRISM_TEST(test_spatial_camera_pose_and_stereo_matrices) {
    SpatialCameraPose pose;
    pose.pitch = 0.0f;
    pose.yaw = 0.0f;
    pose.roll = 0.0f;
    pose.tx = 0.0f;
    pose.ty = 1.6f; // Human eye height
    pose.tz = -2.0f;
    pose.virtual_ipd_mm = 63.0f; // 63 mm

    float view_left[16];
    float view_right[16];
    pose.compute_view_matrix(view_left, 0);  // Left eye
    pose.compute_view_matrix(view_right, 1); // Right eye

    // Left eye shifted by -31.5 mm, Right eye shifted by +31.5 mm -> Delta is 63 mm = 0.063 m
    float left_cam_x = view_left[12];
    float right_cam_x = view_right[12];
    float delta_x = right_cam_x - left_cam_x;
    PRISM_ASSERT_NEAR(delta_x, -0.063f, 1e-3f); // V = [R^T | -R^T * C]

    float proj[16];
    pose.compute_proj_matrix(16.0f / 9.0f, 0.1f, 100.0f, proj);
    PRISM_ASSERT(proj[0] > 0.0f);
    PRISM_ASSERT(proj[5] < 0.0f); // Vulkan Y-flip
    PRISM_ASSERT_EQ(proj[11], -1.0f);
}

PRISM_TEST(test_isobmff_spatial_track_mux_and_demux) {
    // 1. Synthesize a 4D Gaussian field with 120 Gaussians (mix of static and dynamic)
    DynamicGaussianField field;
    DeadZoneVectorQuantizer dz_quantizer(0.002f, 1.8f);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist_pos(-5.0f, 5.0f);
    std::uniform_real_distribution<float> dist_coeff(-0.05f, 0.05f);

    for (size_t i = 0; i < 120; ++i) {
        ChebyshevTrajectory traj;
        traj.p0[0] = dist_pos(rng);
        traj.p0[1] = dist_pos(rng);
        traj.p0[2] = dist_pos(rng);
        traj.q0[0] = 1.0f; traj.q0[1] = 0.0f; traj.q0[2] = 0.0f; traj.q0[3] = 0.0f;
        traj.s[0] = 0.05f; traj.s[1] = 0.05f; traj.s[2] = 0.05f;
        traj.alpha = 0.9f;
        traj.dc_yuv[0] = static_cast<uint8_t>(100 + (i % 100));
        traj.dc_yuv[1] = 128;
        traj.dc_yuv[2] = 128;

        if (i < 80) {
            // Static background anchor
            traj.is_static = true;
        } else {
            // Dynamic splat
            traj.is_static = false;
            for (int k = 0; k < 3; ++k) {
                traj.c[k][0] = dist_coeff(rng);
                traj.c[k][1] = dist_coeff(rng);
                traj.c[k][2] = dist_coeff(rng);
            }
        }
        field.add_trajectory(traj);
    }

    // 2. Compress field into rANS bitstream
    std::vector<uint8_t> spatial_bitstream = field.compress_to_bitstream(dz_quantizer);
    PRISM_ASSERT(!spatial_bitstream.empty());

    // 3. Prepare ISOBMFF Track 1 (base video) and Track 2 (4DGS spatial) samples
    std::vector<prism::IsoMp4Engine::Track1Sample> track1;
    prism::IsoMp4Engine::Track1Sample t1;
    t1.pts_ms = 0;
    t1.dts_ms = 0;
    t1.is_keyframe = true;
    t1.nal_data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E}; // Mock NAL
    track1.push_back(t1);

    std::vector<prism::IsoMp4Engine::TrackSpatialSample> track2_spatial;
    prism::IsoMp4Engine::TrackSpatialSample sp_sample;
    sp_sample.pts_ms = 0;
    sp_sample.is_anchor = true;
    sp_sample.bitstream = spatial_bitstream;
    track2_spatial.push_back(sp_sample);

    // 4. Configure PrismSpatialBox
    prism::PrismSpatialBox spatial_box;
    spatial_box.prism_version = 1;
    spatial_box.num_gaussians = static_cast<uint32_t>(field.count());
    spatial_box.gop_size = 30;
    spatial_box.max_poly_degree = 3;
    spatial_box.sh_degree = 2;
    spatial_box.quant_bits = 8;
    spatial_box.has_epipolar_flow = true;
    spatial_box.has_subspace_factorization = true;
    spatial_box.fps = 60.0f;

    prism::Afgs1Params afgs;
    prism::IsoMp4Engine::MetadataLedger meta;
    meta.has_colr = true;
    meta.colr.colour_primaries = 1; // BT.709

    // 5. Mux dual-track spatial package
    auto pkg = prism::IsoMp4Engine::mux_dual_track_spatial(track1, track2_spatial, spatial_box, afgs, &meta, nullptr);
    PRISM_ASSERT(pkg.has_spatial);
    PRISM_ASSERT_EQ(pkg.track1_sample_count, 1u);
    PRISM_ASSERT_EQ(pkg.spatial_sample_count, 1u);
    PRISM_ASSERT(!pkg.raw_mp4_bytes.empty());

    // 6. Demux dual-track spatial package
    std::vector<prism::IsoMp4Engine::Track1Sample> demux_t1;
    std::vector<prism::IsoMp4Engine::TrackSpatialSample> demux_sp;
    prism::PrismSpatialBox demux_spatial_box;
    prism::Afgs1Params demux_afgs;
    prism::IsoMp4Engine::MetadataLedger demux_meta;

    bool demux_ok = prism::IsoMp4Engine::demux_dual_track_spatial(
        pkg.raw_mp4_bytes.data(),
        pkg.raw_mp4_bytes.size(),
        demux_t1,
        demux_sp,
        demux_spatial_box,
        demux_afgs,
        &demux_meta,
        nullptr
    );

    PRISM_ASSERT(demux_ok);
    PRISM_ASSERT_EQ(demux_t1.size(), 1u);
    PRISM_ASSERT_EQ(demux_sp.size(), 1u);
    PRISM_ASSERT_EQ(demux_spatial_box.num_gaussians, 120u);
    PRISM_ASSERT_EQ(demux_spatial_box.gop_size, 30u);
    PRISM_ASSERT_NEAR(demux_spatial_box.fps, 60.0f, 1e-3f);
    PRISM_ASSERT(demux_meta.has_colr);
    PRISM_ASSERT_EQ(demux_meta.colr.colour_primaries, 1u);

    // Verify bitstream match
    PRISM_ASSERT_EQ(demux_sp[0].bitstream.size(), spatial_bitstream.size());
    PRISM_ASSERT(demux_sp[0].bitstream == spatial_bitstream);

    // 7. Decompress from demuxed spatial bitstream and verify full field fidelity
    auto demux_field = DynamicGaussianField::decompress_from_bitstream(
        demux_sp[0].bitstream.data(), demux_sp[0].bitstream.size(), dz_quantizer);
    PRISM_ASSERT_EQ(demux_field.count(), 120u);
    PRISM_ASSERT_EQ(demux_field.static_anchor_count(), 80u);
    PRISM_ASSERT_EQ(demux_field.dynamic_splat_count(), 40u);

    // Verify static anchor flags and centroids match exactly
    const auto& src_trajs = field.trajectories();
    const auto& dst_trajs = demux_field.trajectories();
    for (size_t i = 0; i < 120; ++i) {
        PRISM_ASSERT_EQ(dst_trajs[i].is_static, src_trajs[i].is_static);
        PRISM_ASSERT_NEAR(dst_trajs[i].p0[0], src_trajs[i].p0[0], 1e-4f);
        PRISM_ASSERT_NEAR(dst_trajs[i].p0[1], src_trajs[i].p0[1], 1e-4f);
        PRISM_ASSERT_NEAR(dst_trajs[i].p0[2], src_trajs[i].p0[2], 1e-4f);
    }
}

PRISM_TEST(test_vulkan_compute_rasterizer_spirv_validity) {
    // Check candidate locations for compiled shader binary
    std::vector<std::string> candidate_paths = {
        "core/shaders/spatial_rasterize.spv",
        "../core/shaders/spatial_rasterize.spv",
        "../../core/shaders/spatial_rasterize.spv"
    };

    std::string spv_path;
    for (const auto& path : candidate_paths) {
        if (std::filesystem::exists(path)) {
            spv_path = path;
            break;
        }
    }

    PRISM_ASSERT(!spv_path.empty());

    std::ifstream spv_file(spv_path, std::ios::binary | std::ios::ate);
    PRISM_ASSERT(spv_file.is_open());

    std::streamsize file_size = spv_file.tellg();
    PRISM_ASSERT(file_size > 0);
    PRISM_ASSERT_EQ(file_size % 4, 0); // SPIR-V is a sequence of 32-bit words

    spv_file.seekg(0, std::ios::beg);
    std::vector<uint32_t> words(file_size / 4);
    spv_file.read(reinterpret_cast<char*>(words.data()), file_size);
    PRISM_ASSERT(words.size() >= 5);

    // SPIR-V Header Invariants:
    // Word 0: Magic Number 0x07230203
    PRISM_ASSERT_EQ(words[0], 0x07230203u);

    // Word 1: Version >= 0x00010300 (Vulkan 1.3 / SPIR-V 1.3)
    PRISM_ASSERT(words[1] >= 0x00010300u);

    // Word 2: Generator Magic (non-zero)
    PRISM_ASSERT(words[2] != 0u);

    // Word 3: Bound ID (upper limit on IDs used in module)
    PRISM_ASSERT(words[3] > 0u);

    // Word 4: Schema (must be 0 in standard SPIR-V)
    PRISM_ASSERT_EQ(words[4], 0u);

    // Verify linear instruction stream parsing without truncation
    size_t idx = 5;
    size_t instruction_count = 0;
    while (idx < words.size()) {
        uint32_t word = words[idx];
        uint16_t word_count = static_cast<uint16_t>(word >> 16);
        PRISM_ASSERT(word_count > 0);
        PRISM_ASSERT(idx + word_count <= words.size());
        idx += word_count;
        instruction_count++;
    }
    PRISM_ASSERT(instruction_count > 50); // Comprehensive rasterizer shader
}

PRISM_TEST(test_spatial_extreme_edge_cases_and_robustness) {
    // 1. Degenerate zero quaternion recovery
    ChebyshevTrajectory traj;
    traj.q0[0] = 0.0f; traj.q0[1] = 0.0f; traj.q0[2] = 0.0f; traj.q0[3] = 0.0f;
    traj.delta_q[0] = 0.0f; traj.delta_q[1] = 0.0f; traj.delta_q[2] = 0.0f; traj.delta_q[3] = 0.0f;
    traj.is_static = false;

    float q_rec[4];
    traj.evaluate_rotation(0.5f, q_rec);
    // Degenerate zero quaternion must fall back to valid unit identity [1, 0, 0, 0]
    PRISM_ASSERT_EQ(q_rec[0], 1.0f);
    PRISM_ASSERT_EQ(q_rec[1], 0.0f);
    PRISM_ASSERT_EQ(q_rec[2], 0.0f);
    PRISM_ASSERT_EQ(q_rec[3], 0.0f);

    // 2. Extreme out-of-bounds tau values [-999.0f, +999.0f] clamped safely without NaN or Inf
    traj.p0[0] = 10.0f;
    traj.c[0][0] = 1.0f;
    float p_out[3];
    traj.evaluate_position(999.0f, p_out);
    PRISM_ASSERT(!std::isnan(p_out[0]) && !std::isinf(p_out[0]));
    traj.evaluate_position(-999.0f, p_out);
    PRISM_ASSERT(!std::isnan(p_out[0]) && !std::isinf(p_out[0]));

    // 3. Negative or near-zero dt_sec in velocity evaluation
    float v_out[3];
    traj.evaluate_velocity(0.0f, 0.0f, v_out);
    PRISM_ASSERT_EQ(v_out[0], 0.0f);
    PRISM_ASSERT_EQ(v_out[1], 0.0f);
    PRISM_ASSERT_EQ(v_out[2], 0.0f);

    traj.evaluate_velocity(0.0f, -1.0f, v_out);
    PRISM_ASSERT_EQ(v_out[0], 0.0f);

    // 4. Corrupted / truncated bitstream rejection
    DeadZoneVectorQuantizer dz;
    const uint8_t garbage_data[] = {0x12, 0x34, 0x56, 0x78};
    bool threw_exception = false;
    try {
        DynamicGaussianField::decompress_from_bitstream(garbage_data, sizeof(garbage_data), dz);
    } catch (const std::exception&) {
        threw_exception = true;
    }
    PRISM_ASSERT(threw_exception);
}

PRISM_TEST(test_mobile_tile_binning_and_depth_sorting) {
    // 1. Synthesize 50 Gaussians at different positions and depths
    std::vector<Gaussian3D> prims(50);
    for (size_t i = 0; i < prims.size(); ++i) {
        auto& g = prims[i];
        g.p[0] = static_cast<float>(static_cast<int>(i % 5) - 2) * 0.2f;
        g.p[1] = static_cast<float>(static_cast<int>((i / 5) % 5) - 2) * 0.2f;
        // Standard right-handed camera looks down -Z:
        // i=0 is behind camera (+5.0f) -> must be culled
        // i >= 1 is in front of camera (-1.0f - i * 0.1f)
        g.p[2] = (i == 0) ? 5.0f : (-1.0f - static_cast<float>(i) * 0.1f);
        g.q[0] = 1.0f; g.q[1] = 0.0f; g.q[2] = 0.0f; g.q[3] = 0.0f;
        g.s[0] = -3.0f; g.s[1] = -3.0f; g.s[2] = -3.0f; // Small radius
        g.alpha = 0.8f;
        g.dc_yuv[0] = 180; g.dc_yuv[1] = 128; g.dc_yuv[2] = 128;
        g.sh_codebook_idx = 0;
        g.flags = 0x04;
    }

    // 2. Set up standard camera view-projection matrix
    SpatialCameraPose cam;
    cam.tx = 0.0f; cam.ty = 0.0f; cam.tz = 0.0f;
    float view_mat[16], proj_mat[16], vp[16];
    cam.compute_view_matrix(view_mat, 0);
    cam.compute_proj_matrix(16.0f / 9.0f, 0.1f, 100.0f, proj_mat);

    // Compute vp = proj * view (column-major)
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += proj_mat[k * 4 + row] * view_mat[col * 4 + k];
            }
            vp[col * 4 + row] = sum;
        }
    }

    // 3. Bin and sort for 1920x1080 resolution
    constexpr uint32_t W = 1920;
    constexpr uint32_t H = 1080;
    auto binned = MobileTileBinningEngine::bin_and_sort(prims, vp, W, H, 0.1f);

    PRISM_ASSERT_EQ(binned.tiles_x, 120u); // (1920 + 15) / 16
    PRISM_ASSERT_EQ(binned.tiles_y, 68u);  // (1080 + 15) / 16
    PRISM_ASSERT_EQ(binned.total_tiles, 120u * 68u);
    PRISM_ASSERT_EQ(binned.gpu_gaussians.size(), prims.size());

    // 4. Verify behind-camera primitive i=0 was never binned into any tile
    for (uint32_t t = 0; t < binned.total_tiles; ++t) {
        uint32_t count = binned.tile_splat_counts[t];
        uint32_t base_idx = t * MobileTileBinningEngine::MAX_SPLATS_PER_TILE;
        for (uint32_t s = 0; s < count; ++s) {
            PRISM_ASSERT(binned.tile_splat_indices[base_idx + s] != 0u);
        }
    }

    // 5. Verify that in all tiles with >= 2 splats, splat depths are non-decreasing (front-to-back)
    size_t multi_splat_tiles = 0;
    for (uint32_t t = 0; t < binned.total_tiles; ++t) {
        uint32_t count = binned.tile_splat_counts[t];
        if (count >= 2) {
            multi_splat_tiles++;
            uint32_t base_idx = t * MobileTileBinningEngine::MAX_SPLATS_PER_TILE;
            for (uint32_t s = 0; s < count - 1; ++s) {
                uint32_t idx_curr = binned.tile_splat_indices[base_idx + s];
                uint32_t idx_next = binned.tile_splat_indices[base_idx + s + 1];
                // Distance from camera is strictly non-decreasing (front-to-back ordering)
                PRISM_ASSERT(std::abs(prims[idx_curr].p[2]) <= std::abs(prims[idx_next].p[2]));
            }
        }
    }
    PRISM_ASSERT(multi_splat_tiles > 0);
}
