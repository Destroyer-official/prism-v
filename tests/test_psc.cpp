#include "test_harness.hpp"
#include "prism/psc.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_psc_ycocgr_reversible_identity) {
    // Test across entire color spectrum
    for (int r = 0; r < 256; r += 17) {
        for (int g = 0; g < 256; g += 17) {
            for (int b = 0; b < 256; b += 17) {
                RgbPixel orig{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
                YCoCgPixel ycocg = PscEngine::rgb_to_ycocgr(orig);
                RgbPixel recon = PscEngine::ycocgr_to_rgb(ycocg);

                PRISM_ASSERT_EQ(recon.r, orig.r);
                PRISM_ASSERT_EQ(recon.g, orig.g);
                PRISM_ASSERT_EQ(recon.b, orig.b);
            }
        }
    }
}

PRISM_TEST(test_psc_legall53_1d_wavelet_reversibility) {
    std::vector<int16_t> signal = {12, 45, 78, 23, 90, 110, 56, 89, 34, 67, 101, 205};
    size_t n = signal.size();

    std::vector<int16_t> low(n / 2);
    std::vector<int16_t> high(n / 2);
    std::vector<int16_t> recon(n);

    PscEngine::forward_legall53_1d(signal.data(), low.data(), high.data(), n);
    PscEngine::inverse_legall53_1d(low.data(), high.data(), recon.data(), n);

    for (size_t i = 0; i < n; ++i) {
        PRISM_ASSERT_EQ(recon[i], signal[i]);
    }
}

PRISM_TEST(test_psc_2d_wavelet_reversibility) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    std::vector<int16_t> plane(W * H);
    for (size_t i = 0; i < plane.size(); ++i) {
        plane[i] = static_cast<int16_t>((i * 37 + 11) % 512 - 256);
    }

    auto bands = PscEngine::forward_wavelet_2d(plane.data(), W, H);
    std::vector<int16_t> recon(W * H);
    PscEngine::inverse_wavelet_2d(bands, recon.data(), W, H);

    for (size_t i = 0; i < plane.size(); ++i) {
        PRISM_ASSERT_EQ(recon[i], plane[i]);
    }
}

PRISM_TEST(test_psc_frame_compression_psnr_infinity) {
    PscEngine engine;
    PscEngine::FrameGeometry geom;
    geom.width = 64;
    geom.height = 64;
    geom.bit_depth = 8;

    // Generate test image (synthetic gradients + textures)
    std::vector<uint8_t> raw_rgb(geom.width * geom.height * 3);
    for (uint32_t y = 0; y < geom.height; ++y) {
        for (uint32_t x = 0; x < geom.width; ++x) {
            size_t idx = (y * geom.width + x) * 3;
            raw_rgb[idx + 0] = static_cast<uint8_t>((x * 4) % 256);
            raw_rgb[idx + 1] = static_cast<uint8_t>((y * 4) % 256);
            raw_rgb[idx + 2] = static_cast<uint8_t>((x * y) % 256);
        }
    }

    auto comp_frame = engine.compress_frame(raw_rgb.data(), geom);
    std::vector<uint8_t> decomp_rgb = engine.decompress_frame(comp_frame);

    PRISM_ASSERT_EQ(decomp_rgb.size(), raw_rgb.size());
    // Verify PSNR = infinity: 100% sample parity
    bool parity = PscEngine::verify_sample_parity(raw_rgb.data(), decomp_rgb.data(), raw_rgb.size());
    PRISM_ASSERT(parity);
}

PRISM_TEST(test_psc_bayer_splice_and_merge) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint8_t BIT_DEPTH = 10;

    std::vector<uint16_t> orig_bayer(W * H);
    for (size_t i = 0; i < orig_bayer.size(); ++i) {
        orig_bayer[i] = static_cast<uint16_t>((i * 19 + 7) & 0x3FF); // 10-bit samples
    }

    auto planes = PscEngine::splice_bayer_cfa(orig_bayer.data(), W, H, BIT_DEPTH);
    PRISM_ASSERT_EQ(planes.sub_w, W / 2);
    PRISM_ASSERT_EQ(planes.sub_h, H / 2);

    std::vector<uint16_t> recon_bayer(W * H);
    PscEngine::merge_bayer_cfa(planes, recon_bayer.data(), W, H);

    bool parity = PscEngine::verify_bayer_parity(orig_bayer.data(), recon_bayer.data(), orig_bayer.size());
    PRISM_ASSERT(parity);
}

PRISM_TEST(test_psc_bayer_compression_roundtrip_psnr_infinity) {
    PscEngine engine;
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint8_t BIT_DEPTH = 10;

    std::vector<uint16_t> bayer_frame0(W * H);
    std::vector<uint16_t> bayer_frame1(W * H);

    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            size_t idx = y * W + x;
            uint16_t val0 = static_cast<uint16_t>(((x * 8) ^ (y * 8)) & 0x3FF);
            bayer_frame0[idx] = val0;
            // Frame 1 with small motion/noise for burst temporal delta test
            bayer_frame1[idx] = static_cast<uint16_t>((val0 + ((x + y) % 5)) & 0x3FF);
        }
    }

    // Compress Frame 0 (Intra)
    auto comp0 = engine.compress_bayer_frame(bayer_frame0.data(), W, H, BIT_DEPTH);
    std::vector<uint16_t> recon0 = engine.decompress_bayer_frame(comp0);
    PRISM_ASSERT(PscEngine::verify_bayer_parity(bayer_frame0.data(), recon0.data(), W * H));

    // Compress Frame 1 (Inter-burst temporal delta)
    auto ref_planes = PscEngine::splice_bayer_cfa(recon0.data(), W, H, BIT_DEPTH);
    auto comp1 = engine.compress_bayer_frame(bayer_frame1.data(), W, H, BIT_DEPTH, &ref_planes);
    PRISM_ASSERT(comp1.is_delta);

    std::vector<uint16_t> recon1 = engine.decompress_bayer_frame(comp1, &ref_planes);
    PRISM_ASSERT(PscEngine::verify_bayer_parity(bayer_frame1.data(), recon1.data(), W * H));
}
