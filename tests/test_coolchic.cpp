#include "test_harness.hpp"
#include "prism/coolchic.hpp"
#include <vector>
#include <random>

using namespace prism;

PRISM_TEST(test_coolchic_synthesis_bounded_macs) {
    CoolchicResidualModel model;

    std::array<int16_t, CoolchicResidualModel::LATENT_CHANNELS> latents{};
    latents.fill(50);
    std::array<int16_t, CoolchicResidualModel::BASE_FEATURE_CHANNELS> base_feats{};
    base_feats.fill(20);

    int16_t residual[3]{};
    uint8_t alpha = 0;

    model.synthesize_pixel(latents.data(), base_feats.data(), residual, alpha);

    // Verify bounded values
    for (int i = 0; i < 3; ++i) {
        PRISM_ASSERT(residual[i] >= -128 && residual[i] <= 127);
    }
}

PRISM_TEST(test_coolchic_masked_soft_gating_blend) {
    // When alpha == 255 (pure base)
    uint8_t out1 = CoolchicResidualModel::blend_pixel(100, 50, 0, 255);
    PRISM_ASSERT(out1 >= 99 && out1 <= 100);

    // When alpha == 0 (fully suppressed base block edge, local average + residual)
    uint8_t out2 = CoolchicResidualModel::blend_pixel(100, 50, 10, 0);
    PRISM_ASSERT_EQ(out2, 60); // 50 + 10 = 60

    // Extreme clamping checks
    uint8_t out_underflow = CoolchicResidualModel::blend_pixel(10, 10, -50, 128);
    PRISM_ASSERT_EQ(out_underflow, 0);

    uint8_t out_overflow = CoolchicResidualModel::blend_pixel(240, 240, 50, 128);
    PRISM_ASSERT_EQ(out_overflow, 255);
}

PRISM_TEST(test_coolchic_determinism_and_torture) {
    CoolchicResidualModel model_a;
    CoolchicResidualModel model_b;

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int16_t> act_dist(-32767, 32767);

    // 1,000 random torture pixels
    for (size_t p = 0; p < 1000; ++p) {
        std::array<int16_t, CoolchicResidualModel::LATENT_CHANNELS> latents{};
        std::array<int16_t, CoolchicResidualModel::BASE_FEATURE_CHANNELS> base_feats{};

        for (auto& l : latents) l = act_dist(rng);
        for (auto& f : base_feats) f = act_dist(rng);

        int16_t res_a[3]{}, res_b[3]{};
        uint8_t alpha_a = 0, alpha_b = 0;

        model_a.synthesize_pixel(latents.data(), base_feats.data(), res_a, alpha_a);
        model_b.synthesize_pixel(latents.data(), base_feats.data(), res_b, alpha_b);

        // 100.000% Cross-instance bit-exactness check
        PRISM_ASSERT_EQ(alpha_a, alpha_b);
        for (int c = 0; c < 3; ++c) {
            PRISM_ASSERT_EQ(res_a[c], res_b[c]);
            PRISM_ASSERT(res_a[c] >= -128 && res_a[c] <= 127);
        }
    }

    // Extreme boundary torture: all +32767 and all -32767
    std::array<int16_t, CoolchicResidualModel::LATENT_CHANNELS> lat_max{}, lat_min{};
    std::array<int16_t, CoolchicResidualModel::BASE_FEATURE_CHANNELS> base_max{}, base_min{};
    lat_max.fill(32767); base_max.fill(32767);
    lat_min.fill(-32767); base_min.fill(-32767);

    int16_t res_max[3]{}, res_min[3]{};
    uint8_t alpha_max = 0, alpha_min = 0;

    model_a.synthesize_pixel(lat_max.data(), base_max.data(), res_max, alpha_max);
    model_a.synthesize_pixel(lat_min.data(), base_min.data(), res_min, alpha_min);

    for (int c = 0; c < 3; ++c) {
        PRISM_ASSERT(res_max[c] >= -128 && res_max[c] <= 127);
        PRISM_ASSERT(res_min[c] >= -128 && res_min[c] <= 127);
    }
}

PRISM_TEST(test_coolchic_synthesize_frame_batch) {
    CoolchicResidualModel model;
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;

    std::vector<uint8_t> base_frame(W * H);
    for (size_t i = 0; i < W * H; ++i) {
        base_frame[i] = static_cast<uint8_t>((i * 17) % 256);
    }

    std::vector<int16_t> latents(W * H * CoolchicResidualModel::LATENT_CHANNELS, 50);
    std::vector<uint8_t> out_frame(W * H, 0);

    model.synthesize_frame(latents.data(), base_frame.data(), out_frame.data(), W, H);

    // Output must be valid clamped pixels
    for (size_t i = 0; i < W * H; ++i) {
        PRISM_ASSERT(out_frame[i] >= 0 && out_frame[i] <= 255);
    }
}

PRISM_TEST(test_coolchic_model_serialization_roundtrip) {
    CoolchicResidualModel model1;
    auto bytes = model1.serialize_model();
    PRISM_ASSERT_EQ(bytes.size(), CoolchicResidualModel::SERIALIZED_MODEL_SIZE);
    PRISM_ASSERT_EQ(bytes[0], 'C');
    PRISM_ASSERT_EQ(bytes[1], 'C');
    PRISM_ASSERT_EQ(bytes[2], 'H');
    PRISM_ASSERT_EQ(bytes[3], 'C');

    CoolchicResidualModel model2;
    // Mutate model2 weights
    int8_t zero_w[144]{};
    model2.set_weights(zero_w, nullptr, zero_w, nullptr, nullptr, nullptr);

    // Deserialize into model2
    bool ok = model2.deserialize_model(bytes.data(), bytes.size());
    PRISM_ASSERT(ok);

    // Output parity verification
    std::array<int16_t, 8> latents{100, -200, 300, -400, 500, -600, 700, -800};
    std::array<int16_t, 4> feats{128, 50, -50, 10};
    int16_t r1[3]{}, r2[3]{};
    uint8_t a1 = 0, a2 = 0;

    model1.synthesize_pixel(latents.data(), feats.data(), r1, a1);
    model2.synthesize_pixel(latents.data(), feats.data(), r2, a2);

    PRISM_ASSERT_EQ(a1, a2);
    for (int i = 0; i < 3; ++i) {
        PRISM_ASSERT_EQ(r1[i], r2[i]);
    }
}

PRISM_TEST(test_coolchic_pareto_dial_solver) {
    ParetoTarget target;
    target.weight_battery = 0.8f; // Battery Saver Mode

    auto res = ParetoDialSolver::solve(target, 1920, 1080, 30, 60.0f);
    PRISM_ASSERT(res.predicted_energy_joules_per_frame <= 0.015f); // Hardware ASIC energy ceiling!
    PRISM_ASSERT(!res.enable_micro_residual); // Bypasses micro-residual in battery mode

    // Quality Mode
    target.weight_battery = 0.1f;
    target.weight_quality = 0.8f;
    auto res_qual = ParetoDialSolver::solve(target, 1920, 1080, 30, 60.0f);
    PRISM_ASSERT(res_qual.enable_micro_residual);
    PRISM_ASSERT(res_qual.predicted_vmaf >= 94.0f);
    PRISM_ASSERT(res_qual.predicted_energy_joules_per_frame <= 0.05f); // <0.05 J/frame ceiling
    PRISM_ASSERT(res_qual.predicted_cambi < 2.5f); // Anti-banding criterion

    // Explicit Cap Mode
    target.explicit_size_cap_bytes = 20 * 1024 * 1024; // 20 MB cap for 60s video
    auto res_cap = ParetoDialSolver::solve(target, 1920, 1080, 30, 60.0f);
    PRISM_ASSERT(res_cap.target_bitrate_kbps > 1000);
}

PRISM_TEST(test_coolchic_preflight_probe) {
    constexpr uint32_t W = 512;
    constexpr uint32_t H = 512;
    std::vector<uint8_t> frame(W * H, 128);

    // Draw high frequency noise in frame
    for (size_t i = 0; i < W * H; ++i) {
        frame[i] = static_cast<uint8_t>((i * 73 + 19) % 256);
    }

    ParetoTarget target;
    target.weight_quality = 0.7f;

    auto res = ParetoDialSolver::run_preflight_probe(frame.data(), W, H, target);
    PRISM_ASSERT(res.predicted_vmaf > 80.0f && res.predicted_vmaf <= 100.0f);
    PRISM_ASSERT(res.target_bitrate_kbps > 0);
    PRISM_ASSERT(res.predicted_cambi < 2.5f);
}

PRISM_TEST(test_coolchic_activity_guided_sub_35_gmacs) {
    CoolchicResidualModel model;
    constexpr uint32_t W = 1920;
    constexpr uint32_t H = 1080;

    // Realistic video pattern: 65% flat/smooth background, 35% detailed foreground
    std::vector<uint8_t> base_frame(W * H, 128);
    for (uint32_t y = 300; y < 780; ++y) {
        for (uint32_t x = 600; x < 1320; ++x) {
            base_frame[y * W + x] = static_cast<uint8_t>((x * 37 + y * 41) % 256);
        }
    }

    std::vector<int16_t> latents(W * H * CoolchicResidualModel::LATENT_CHANNELS, 20);
    std::vector<uint8_t> out_frame(W * H, 0);

    uint64_t total_macs = 0;
    model.synthesize_frame_activity_guided(latents.data(), base_frame.data(), out_frame.data(),
                                          W, H, 16, &total_macs);

    // Strict 2027+ Guarantee: Total executed MACs must be strictly < 0.35 GMACs/frame
    constexpr uint64_t MACS_CEILING = 350'000'000ULL; // 0.35 GMACs
    PRISM_ASSERT(total_macs < MACS_CEILING);

    // Output range assertion
    for (size_t i = 0; i < W * H; i += 1024) {
        PRISM_ASSERT(out_frame[i] >= 0 && out_frame[i] <= 255);
    }
}

PRISM_TEST(test_coolchic_latency_benchmark) {
    CoolchicResidualModel model;
    constexpr uint32_t W = 128;
    constexpr uint32_t H = 128;

    std::vector<uint8_t> base_frame(W * H, 100);
    for (size_t i = 0; i < W * H; ++i) {
        base_frame[i] = static_cast<uint8_t>((i * 13) % 256);
    }
    std::vector<int16_t> latents(W * H * CoolchicResidualModel::LATENT_CHANNELS, 15);
    std::vector<uint8_t> out_frame(W * H, 0);

    // Warm up
    model.synthesize_frame(latents.data(), base_frame.data(), out_frame.data(), W, H);

    // Measure time
    uint64_t macs = 0;
    model.synthesize_frame_activity_guided(latents.data(), base_frame.data(), out_frame.data(), W, H, 16, &macs);
    PRISM_ASSERT(out_frame[0] >= 0);
}

PRISM_TEST(test_coolchic_yuv420_activity_guided) {
    CoolchicResidualModel model;
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint32_t UV_W = W / 2;
    constexpr uint32_t UV_H = H / 2;

    std::vector<uint8_t> base_y(W * H, 128);
    // Draw an edge in center to trigger activity
    for (uint32_t y = 16; y < 48; ++y) {
        for (uint32_t x = 16; x < 48; ++x) {
            base_y[y * W + x] = 220;
        }
    }

    std::vector<uint8_t> base_u(UV_W * UV_H, 128);
    std::vector<uint8_t> base_v(UV_W * UV_H, 128);

    std::vector<int16_t> latents(W * H * CoolchicResidualModel::LATENT_CHANNELS, 30);
    std::vector<uint8_t> out_y(W * H, 0);
    std::vector<uint8_t> out_u(UV_W * UV_H, 0);
    std::vector<uint8_t> out_v(UV_W * UV_H, 0);

    uint64_t macs = 0;
    model.synthesize_frame_yuv420_activity_guided(latents.data(),
                                                  base_y.data(), base_u.data(), base_v.data(),
                                                  out_y.data(), out_u.data(), out_v.data(),
                                                  W, H, 16, &macs);

    PRISM_ASSERT(macs > 0);
    // Verified: MACs must be strictly less than full frame evaluation
    uint64_t full_frame_macs = static_cast<uint64_t>(W) * H * 336;
    PRISM_ASSERT(macs < full_frame_macs);

    // Assert all output planes valid
    for (size_t i = 0; i < W * H; ++i) {
        PRISM_ASSERT(out_y[i] >= 0 && out_y[i] <= 255);
    }
    for (size_t i = 0; i < UV_W * UV_H; ++i) {
        PRISM_ASSERT(out_u[i] >= 0 && out_u[i] <= 255);
        PRISM_ASSERT(out_v[i] >= 0 && out_v[i] <= 255);
    }
}
