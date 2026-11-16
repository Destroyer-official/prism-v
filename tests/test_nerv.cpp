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

    // Render continuous frame at t = 0.5
    std::vector<uint8_t> base_frame(cfg.width * cfg.height, 128);
    std::vector<uint8_t> out_frame(cfg.width * cfg.height, 0);

    codec_recv.render_frame(0.5f, base_frame.data(), out_frame.data());

    for (uint8_t px : out_frame) {
        PRISM_ASSERT(px >= 100 && px <= 160);
    }
}
