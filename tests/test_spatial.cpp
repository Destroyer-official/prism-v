#include "test_harness.hpp"
#include "prism/spatial.hpp"
#include "prism/isobmff.hpp"
#include <vector>
#include <cmath>
#include <random>

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
