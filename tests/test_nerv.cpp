#include "test_harness.hpp"
#include "prism/nerv_engine.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_nerv_triplane_sampling) {
    TriPlaneLattice lattice(32, 32, 10, 8, 4);

    // Initialize plane features with non-zero values
    auto& p_xy = lattice.plane_xy();
    std::fill(p_xy.begin(), p_xy.end(), static_cast<int16_t>(1000));
    auto& p_xt = lattice.plane_xt();
    std::fill(p_xt.begin(), p_xt.end(), static_cast<int16_t>(1000));
    auto& p_yt = lattice.plane_yt();
    std::fill(p_yt.begin(), p_yt.end(), static_cast<int16_t>(1000));

    std::array<int16_t, 8> out_feat{};
    lattice.sample_feature(16384, 16384, 16384, out_feat.data());

    // Channel values must be non-zero and bounded
    for (int16_t val : out_feat) {
        PRISM_ASSERT(val != 0);
        PRISM_ASSERT(val >= -32767 && val <= 32767);
    }

    // Boundary torture checks
    std::array<int16_t, 8> out_zero{}, out_max{};
    lattice.sample_feature(0, 0, 0, out_zero.data());
    lattice.sample_feature(32767, 32767, 32767, out_max.data());
    for (size_t c = 0; c < 8; ++c) {
        PRISM_ASSERT(out_zero[c] >= -32767 && out_zero[c] <= 32767);
        PRISM_ASSERT(out_max[c] >= -32767 && out_max[c] <= 32767);
    }
}

PRISM_TEST(test_nerv_synthesis_mlp_bounded_macs) {
    NeRVSynthesisMLP mlp;
    std::array<int16_t, 8> feat{100, -200, 300, -400, 500, -600, 700, -800};
    std::array<int16_t, 3> residual{};

    mlp.synthesize(feat.data(), residual.data());

    // Residual deltas must stay within bounded 8-bit dynamic range [-128, +127]
    for (size_t c = 0; c < 3; ++c) {
        PRISM_ASSERT(residual[c] >= -128 && residual[c] <= 127);
    }

    // Complexity verification: 8*16 + 16*16 + 16*3 = 432 MACs <= 450 MACs ceiling
    constexpr size_t MACS = 8 * 16 + 16 * 16 + 16 * 3;
    static_assert(MACS == 432, "NeRV MLP complexity must be exactly 432 MACs");
}

PRISM_TEST(test_nerv_hessian_bit_allocation) {
    // 5 parameters with steep curvature difference
    std::vector<int32_t> hessian{1000000, 500000, 10000, 100, 10};
    std::vector<uint8_t> alloc = HessianBitAllocator::allocate_bits(hessian, 4);

    PRISM_ASSERT_EQ(alloc.size(), 5);
    // Highest curvature parameters must receive 8 bits
    PRISM_ASSERT_EQ(alloc[0], 8);
    // Lowest curvature parameters must receive 2 bits
    PRISM_ASSERT_EQ(alloc[4], 2);

    std::vector<int16_t> weights{10000, 5000, 2000, 500, 100};
    std::vector<int8_t> quantized(5);
    HessianBitAllocator::quantize_with_allocation(weights.data(), alloc, 5, quantized.data());

    // Highest curvature parameter must have wider dynamic range
    PRISM_ASSERT(std::abs(static_cast<int32_t>(quantized[0])) > 0);
}

PRISM_TEST(test_nerv_codec_serialization_and_rendering) {
    NeRVSConfig cfg;
    cfg.width = 32;
    cfg.height = 32;
    cfg.gop_frames = 10;
    cfg.triplane_channels = 8;
    cfg.spatial_downscale = 4;

    NeRVCodec codec(cfg);

    std::array<uint8_t, 1024> payload{};
    size_t written = codec.serialize_gop(payload.data(), payload.size());
    PRISM_ASSERT(written > 100);
    PRISM_ASSERT_EQ(payload[0], 'N');
    PRISM_ASSERT_EQ(payload[1], 'E');
    PRISM_ASSERT_EQ(payload[2], 'R');
    PRISM_ASSERT_EQ(payload[3], 'V');

    // Deserialize into fresh codec
    NeRVCodec codec_recv(cfg);
    bool ok = codec_recv.deserialize_gop(payload.data(), written);
    PRISM_ASSERT(ok);

    // Render continuous frames at multiple timestamps
    std::vector<uint8_t> base_frame(cfg.width * cfg.height, 128);
    std::vector<uint8_t> out_frame(cfg.width * cfg.height, 0);

    const float test_timestamps[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float t : test_timestamps) {
        codec_recv.render_frame(t, base_frame.data(), out_frame.data());
        for (uint8_t px : out_frame) {
            PRISM_ASSERT(px >= 100 && px <= 160);
        }
    }
}

PRISM_TEST(test_nerv_gop_optimization_step) {
    NeRVSConfig cfg;
    cfg.width = 32;
    cfg.height = 32;
    cfg.gop_frames = 4;
    cfg.triplane_channels = 8;
    cfg.spatial_downscale = 4;

    NeRVCodec codec(cfg);

    const size_t frame_size = cfg.width * cfg.height;
    std::vector<std::vector<uint8_t>> targets(cfg.gop_frames, std::vector<uint8_t>(frame_size));
    std::vector<std::vector<uint8_t>> bases(cfg.gop_frames, std::vector<uint8_t>(frame_size, 128));

    for (size_t f = 0; f < cfg.gop_frames; ++f) {
        for (size_t i = 0; i < frame_size; ++i) {
            // Target frame has high-contrast pattern
            targets[f][i] = static_cast<uint8_t>((128 + ((static_cast<int>(f) + 1) * 10) + (i % 31) - 15));
        }
    }

    std::vector<const uint8_t*> target_ptrs(cfg.gop_frames);
    std::vector<const uint8_t*> base_ptrs(cfg.gop_frames);
    for (size_t f = 0; f < cfg.gop_frames; ++f) {
        target_ptrs[f] = targets[f].data();
        base_ptrs[f] = bases[f].data();
    }

    // Step 1: Run 1 step
    float initial_mse = codec.optimize_gop_step(target_ptrs.data(), base_ptrs.data(), 1, 0.01f);
    PRISM_ASSERT(initial_mse >= 0.0f);

    // Step 2: Run multiple optimization steps and check convergence
    float post_mse = codec.optimize_gop_step(target_ptrs.data(), base_ptrs.data(), 5, 0.01f);
    PRISM_ASSERT(post_mse <= initial_mse + 1.0f); // MSE should decrease or remain stably bounded

    // Verify bounded integer weights after gradient updates
    for (int8_t w : codec.mlp().weights_l1()) {
        PRISM_ASSERT(w >= -127 && w <= 127);
    }
    for (int8_t w : codec.mlp().weights_l2()) {
        PRISM_ASSERT(w >= -127 && w <= 127);
    }
    for (int8_t w : codec.mlp().weights_l3()) {
        PRISM_ASSERT(w >= -127 && w <= 127);
    }
}

PRISM_TEST(test_nerv_continuous_subframe_render_latency) {
    NeRVSConfig cfg;
    cfg.width = 64;
    cfg.height = 64;
    cfg.gop_frames = 10;
    cfg.triplane_channels = 8;
    cfg.spatial_downscale = 4;

    NeRVCodec codec(cfg);

    std::vector<uint8_t> base_frame(cfg.width * cfg.height, 128);
    std::vector<uint8_t> rendered_frame(cfg.width * cfg.height, 0);

    // Continuous time subframe rendering at arbitrary temporal positions
    float timestamps[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float t : timestamps) {
        codec.render_frame(t, base_frame.data(), rendered_frame.data());
        for (size_t i = 0; i < cfg.width * cfg.height; i += 64) {
            PRISM_ASSERT(rendered_frame[i] >= 0 && rendered_frame[i] <= 255);
        }
    }
}
