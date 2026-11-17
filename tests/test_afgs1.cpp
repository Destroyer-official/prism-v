#include "test_harness.hpp"
#include "prism/afgs1.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_afgs1_noise_extraction_and_synthesis) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<uint8_t> noisy_frame(W * H);
    std::vector<uint8_t> clean_frame(W * H);
    std::vector<uint8_t> synthesized_frame(W * H);

    for (size_t i = 0; i < W * H; ++i) {
        noisy_frame[i] = static_cast<uint8_t>((128 + ((i % 17) - 8)));
    }

    // Extract noise & get clean frame
    Afgs1Params params = Afgs1Engine::extract_noise_and_denoise(noisy_frame.data(),
                                                               clean_frame.data(),
                                                               W, H);
    PRISM_ASSERT(params.apply_grain);
    PRISM_ASSERT(params.num_y_points >= 2);

    // Synthesize grain on clean frame
    Afgs1Engine::synthesize_grain(clean_frame.data(), synthesized_frame.data(), W, H, params);

    // Serialization & deserialization check
    std::vector<uint8_t> bytes = Afgs1Engine::serialize_params(params);
    Afgs1Params restored_params = Afgs1Engine::deserialize_params(bytes.data(), bytes.size());
    PRISM_ASSERT_EQ(restored_params.grain_scale, params.grain_scale);
    PRISM_ASSERT_EQ(restored_params.random_seed, params.random_seed);
    PRISM_ASSERT_EQ(restored_params.ar_coeff_lag, params.ar_coeff_lag);
}

PRISM_TEST(test_afgs1_edge_preservation) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<uint8_t> noisy_step(W * H, 50);
    std::vector<uint8_t> clean_step(W * H, 0);

    // Create a high-contrast vertical edge at x = 32 (50 on left, 200 on right)
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 32; x < W; ++x) {
            noisy_step[y * W + x] = 200;
        }
    }

    // Add small noise to flat regions
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            if (x != 31 && x != 32) {
                noisy_step[y * W + x] += static_cast<uint8_t>((x + y) % 5);
            }
        }
    }

    Afgs1Engine::extract_noise_and_denoise(noisy_step.data(), clean_step.data(), W, H);

    // Check that edge step height is strictly preserved (left is ~50, right is ~200)
    for (uint32_t y = 10; y < H - 10; ++y) {
        PRISM_ASSERT(clean_step[y * W + 20] < 60);
        PRISM_ASSERT(clean_step[y * W + 45] > 190);
    }
}

PRISM_TEST(test_afgs1_ar_colored_noise_synthesis) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;

    std::vector<uint8_t> base_frame(W * H, 128);
    std::vector<uint8_t> out_ar(W * H, 0);
    std::vector<uint8_t> out_white(W * H, 0);

    Afgs1Params params_ar;
    params_ar.apply_grain = true;
    params_ar.random_seed = 0x1234;
    params_ar.grain_scale = 10;
    params_ar.ar_coeff_lag = 1;
    params_ar.ar_coeffs_y[0] = 8;
    params_ar.ar_coeffs_y[1] = 8;

    Afgs1Params params_white = params_ar;
    params_white.ar_coeff_lag = 0; // Pure white noise

    Afgs1Engine::synthesize_grain(base_frame.data(), out_ar.data(), W, H, params_ar);
    Afgs1Engine::synthesize_grain(base_frame.data(), out_white.data(), W, H, params_white);

    // AR noise must produce correlated patterns distinct from pure white noise
    bool any_diff = false;
    for (size_t i = 0; i < W * H; ++i) {
        if (out_ar[i] != out_white[i]) any_diff = true;
        PRISM_ASSERT(out_ar[i] >= 0 && out_ar[i] <= 255);
    }
    PRISM_ASSERT(any_diff);
}

PRISM_TEST(test_afgs1_lfsr_determinism) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;

    std::vector<uint8_t> base_frame(W * H, 128);
    std::vector<uint8_t> out1(W * H, 0);
    std::vector<uint8_t> out2(W * H, 0);

    Afgs1Params params;
    params.apply_grain = true;
    params.random_seed = 0xBEEF;
    params.grain_scale = 12;

    Afgs1Engine::synthesize_grain(base_frame.data(), out1.data(), W, H, params);
    Afgs1Engine::synthesize_grain(base_frame.data(), out2.data(), W, H, params);

    // Exact bit-for-bit identity across repeated synthesis with identical seed
    for (size_t i = 0; i < W * H; ++i) {
        PRISM_ASSERT_EQ(out1[i], out2[i]);
        PRISM_ASSERT(out1[i] >= 0 && out1[i] <= 255);
    }
}

PRISM_TEST(test_afgs1_bypass_and_serialization) {
    constexpr uint32_t W = 16;
    constexpr uint32_t H = 16;

    std::vector<uint8_t> in(W * H, 77);
    std::vector<uint8_t> out(W * H, 0);

    Afgs1Params params;
    params.apply_grain = false; // Bypass mode

    Afgs1Engine::synthesize_grain(in.data(), out.data(), W, H, params);
    for (size_t i = 0; i < W * H; ++i) {
        PRISM_ASSERT_EQ(out[i], 77);
    }

    // Serialization verification
    params.apply_grain = true;
    params.random_seed = 0xCAFE;
    params.grain_scale = 14;
    params.ar_coeff_lag = 2;
    for (size_t i = 0; i < 24; ++i) {
        params.ar_coeffs_y[i] = static_cast<int8_t>(i - 12);
    }

    auto bytes = Afgs1Engine::serialize_params(params);
    PRISM_ASSERT(bytes.size() >= 30);

    auto restored = Afgs1Engine::deserialize_params(bytes.data(), bytes.size());
    PRISM_ASSERT(restored.apply_grain);
    PRISM_ASSERT_EQ(restored.random_seed, 0xCAFE);
    PRISM_ASSERT_EQ(restored.grain_scale, 14);
    PRISM_ASSERT_EQ(restored.ar_coeff_lag, 2);
    for (size_t i = 0; i < 24; ++i) {
        PRISM_ASSERT_EQ(restored.ar_coeffs_y[i], static_cast<int8_t>(i - 12));
    }
}

PRISM_TEST(test_afgs1_yuv420_grain_synthesis) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    constexpr uint32_t UV_W = W / 2;
    constexpr uint32_t UV_H = H / 2;

    std::vector<uint8_t> in_y(W * H, 128);
    std::vector<uint8_t> in_u(UV_W * UV_H, 128);
    std::vector<uint8_t> in_v(UV_W * UV_H, 128);

    std::vector<uint8_t> out_y(W * H, 0);
    std::vector<uint8_t> out_u(UV_W * UV_H, 0);
    std::vector<uint8_t> out_v(UV_W * UV_H, 0);

    Afgs1Params params;
    params.apply_grain = true;
    params.random_seed = 0x5432;
    params.grain_scale = 10;
    params.ar_coeff_lag = 1;
    params.ar_coeffs_y[0] = 6;
    params.ar_coeffs_cb[0] = 4;
    params.ar_coeffs_cr[0] = 4;

    Afgs1Engine::synthesize_grain_yuv420(in_y.data(), in_u.data(), in_v.data(),
                                         out_y.data(), out_u.data(), out_v.data(),
                                         W, H, params);

    // Verify all planes have non-trivial grain variations
    bool y_has_grain = false;
    bool u_has_grain = false;
    bool v_has_grain = false;

    for (size_t i = 0; i < W * H; ++i) {
        if (out_y[i] != 128) y_has_grain = true;
        PRISM_ASSERT(out_y[i] >= 0 && out_y[i] <= 255);
    }
    for (size_t i = 0; i < UV_W * UV_H; ++i) {
        if (out_u[i] != 128) u_has_grain = true;
        if (out_v[i] != 128) v_has_grain = true;
        PRISM_ASSERT(out_u[i] >= 0 && out_u[i] <= 255);
        PRISM_ASSERT(out_v[i] >= 0 && out_v[i] <= 255);
    }

    PRISM_ASSERT(y_has_grain);
    PRISM_ASSERT(u_has_grain);
    PRISM_ASSERT(v_has_grain);

    // Chroma decorrelation check: Cb and Cr should not be identical
    bool chroma_decorrelated = false;
    for (size_t i = 0; i < UV_W * UV_H; ++i) {
        if (out_u[i] != out_v[i]) chroma_decorrelated = true;
    }
    PRISM_ASSERT(chroma_decorrelated);

    // Disabled grain check
    params.apply_grain = false;
    Afgs1Engine::synthesize_grain_yuv420(in_y.data(), in_u.data(), in_v.data(),
                                         out_y.data(), out_u.data(), out_v.data(),
                                         W, H, params);
    for (size_t i = 0; i < UV_W * UV_H; ++i) {
        PRISM_ASSERT_EQ(out_u[i], 128);
        PRISM_ASSERT_EQ(out_v[i], 128);
    }
}

PRISM_TEST(test_afgs1_temporal_grain_flicker_free) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<uint8_t> frame_t0(W * H, 120);
    std::vector<uint8_t> frame_t1(W * H, 122);
    std::vector<uint8_t> out_t0(W * H, 0);
    std::vector<uint8_t> out_t1(W * H, 0);

    // Initial frame params
    Afgs1Params params_t0;
    params_t0.apply_grain = true;
    params_t0.random_seed = 0x1A2B;
    params_t0.grain_scale = 10;
    params_t0.ar_coeff_lag = 1;
    params_t0.ar_coeffs_y[0] = 6;
    params_t0.ar_coeffs_y[1] = 4;

    // Next frame raw params (hypothetically noisy fluctuation)
    Afgs1Params params_t1 = params_t0;
    params_t1.grain_scale = 14; // Fluctuation jump
    params_t1.ar_coeffs_y[0] = 12;

    // Apply temporal smoothing to guarantee flicker-free playback
    Afgs1Engine::smooth_temporal_grain(params_t1, params_t0, false);

    // Assert that scale was smoothed (no abrupt jump)
    PRISM_ASSERT(params_t1.grain_scale == 11 || params_t1.grain_scale == 10);
    // Assert coherent seed progression
    PRISM_ASSERT(params_t1.random_seed != params_t0.random_seed);

    Afgs1Engine::synthesize_grain(frame_t0.data(), out_t0.data(), W, H, params_t0);
    Afgs1Engine::synthesize_grain(frame_t1.data(), out_t1.data(), W, H, params_t1);

    // Compute temporal grain delta variance
    int64_t diff_sum = 0;
    for (size_t i = 0; i < W * H; ++i) {
        int32_t d = static_cast<int32_t>(out_t1[i]) - static_cast<int32_t>(out_t0[i]);
        diff_sum += std::abs(d);
    }
    double mean_temporal_diff = static_cast<double>(diff_sum) / (W * H);
    // Smooth grain transition: mean delta should be gentle (< 15 intensity levels)
    PRISM_ASSERT(mean_temporal_diff < 15.0);
}
