#include "test_harness.hpp"
#include "prism/coolchic.hpp"
#include <vector>

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
    // When alpha == 256 (pure base)
    uint8_t out1 = CoolchicResidualModel::blend_pixel(100, 50, 0, 255);
    PRISM_ASSERT(out1 >= 99 && out1 <= 100);

    // When alpha == 0 (fully suppressed base block edge, local average + residual)
    uint8_t out2 = CoolchicResidualModel::blend_pixel(100, 50, 10, 0);
    PRISM_ASSERT_EQ(out2, 60); // 50 + 10 = 60
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
}
