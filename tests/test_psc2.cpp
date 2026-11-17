#include "test_harness.hpp"
#include "prism/psc2.hpp"
#include "prism/psc.hpp"
#include <vector>
#include <cmath>
#include <numeric>

using namespace prism;

PRISM_TEST(test_psc2_ycocgr2_reversibility) {
    constexpr size_t N = 256;
    std::vector<int16_t> orig_y(N), orig_co(N), orig_cg(N);
    std::vector<int16_t> y(N), co(N), cg(N);

    for (size_t i = 0; i < N; ++i) {
        orig_y[i]  = static_cast<int16_t>((i * 7 + 13) % 256);
        orig_co[i] = static_cast<int16_t>((i * 11 - 50) % 256);
        orig_cg[i] = static_cast<int16_t>((i * 17 - 100) % 256);
    }

    const int8_t test_shears[][2] = {
        {0, 0},
        {35, -42},
        {-64, 80},
        {127, -128},
        {-120, 115}
    };

    for (const auto& [alpha, beta] : test_shears) {
        y  = orig_y;
        co = orig_co;
        cg = orig_cg;

        Psc2Engine::forward_ycocgr2(y.data(), co.data(), cg.data(), N, alpha, beta);
        Psc2Engine::inverse_ycocgr2(y.data(), co.data(), cg.data(), N, alpha, beta);

        for (size_t i = 0; i < N; ++i) {
            PRISM_ASSERT_EQ(y[i], orig_y[i]);
            PRISM_ASSERT_EQ(co[i], orig_co[i]);
            PRISM_ASSERT_EQ(cg[i], orig_cg[i]);
        }
    }
}

PRISM_TEST(test_psc2_shear_parameter_estimator) {
    constexpr size_t N = 512;
    std::vector<int16_t> y(N), co(N), cg(N);

    // Synthesize data: Co = 0.5 * Y + residual; Cg = -0.25 * residual
    for (size_t i = 0; i < N; ++i) {
        y[i]  = static_cast<int16_t>((i % 200) - 100);
        int16_t res = static_cast<int16_t>((i * 13) % 100 - 50);
        co[i] = static_cast<int16_t>(((y[i] * 128) >> 8) + res); // slope ~ 0.5 (alpha ~ 128)
        cg[i] = static_cast<int16_t>((res * -64) >> 8);          // slope ~ -0.25 on residual (beta ~ -64)
    }

    auto [alpha, beta] = Psc2Engine::estimate_shear_parameters(y.data(), co.data(), cg.data(), N);

    PRISM_ASSERT(alpha >= 100 && alpha <= 135);
    PRISM_ASSERT(beta >= -75 && beta <= -50);
}

PRISM_TEST(test_psc2_adiw_53_directional_reversibility) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    std::vector<int16_t> orig_tile(W * H);
    for (size_t i = 0; i < orig_tile.size(); ++i) {
        orig_tile[i] = static_cast<int16_t>((i * 31 + 7) % 512 - 256);
    }

    const AdiwOrientation orientations[] = {
        AdiwOrientation::HORIZONTAL,
        AdiwOrientation::VERTICAL,
        AdiwOrientation::DIAGONAL,
        AdiwOrientation::ANTI_DIAG
    };

    for (auto ori : orientations) {
        std::vector<int16_t> tile = orig_tile;

        Psc2Engine::forward_adiw_2d(tile.data(), W, W, H, ori, WaveletFilterType::LEGALL_53);
        Psc2Engine::inverse_adiw_2d(tile.data(), W, W, H, ori, WaveletFilterType::LEGALL_53);

        for (size_t i = 0; i < orig_tile.size(); ++i) {
            PRISM_ASSERT_EQ(tile[i], orig_tile[i]);
        }
    }
}

PRISM_TEST(test_psc2_adiw_97_directional_reversibility) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    std::vector<int16_t> orig_tile(W * H);
    for (size_t i = 0; i < orig_tile.size(); ++i) {
        orig_tile[i] = static_cast<int16_t>((i * 43 + 19) % 512 - 256);
    }

    const AdiwOrientation orientations[] = {
        AdiwOrientation::HORIZONTAL,
        AdiwOrientation::VERTICAL,
        AdiwOrientation::DIAGONAL,
        AdiwOrientation::ANTI_DIAG
    };

    for (auto ori : orientations) {
        std::vector<int16_t> tile = orig_tile;

        Psc2Engine::forward_adiw_2d(tile.data(), W, W, H, ori, WaveletFilterType::CDF_97_INT);
        Psc2Engine::inverse_adiw_2d(tile.data(), W, W, H, ori, WaveletFilterType::CDF_97_INT);

        for (size_t i = 0; i < orig_tile.size(); ++i) {
            PRISM_ASSERT_EQ(tile[i], orig_tile[i]);
        }
    }
}

PRISM_TEST(test_psc2_orientation_detector) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    std::vector<int16_t> tile_diag(W * H);

    // Create a 45-degree diagonal ridge: f(x, y) = max(0, 100 - |x - y| * 10)
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            int dist = std::abs(static_cast<int>(x) - static_cast<int>(y));
            tile_diag[y * W + x] = static_cast<int16_t>(std::max(0, 100 - dist * 10));
        }
    }

    auto detected = Psc2Engine::detect_orientation_from_ll(tile_diag.data(), W, 0, 0, W, H);
    PRISM_ASSERT(detected == AdiwOrientation::ANTI_DIAG || detected == AdiwOrientation::DIAGONAL);
}

PRISM_TEST(test_psc2_quad_bayer_aiklt_reversibility) {
    constexpr uint32_t SW = 32;
    constexpr uint32_t SH = 32;
    constexpr uint8_t BIT_DEPTH = 10;
    const size_t count = SW * SH;

    Psc2Engine::QuadBayerPlanes orig_planes;
    orig_planes.sub_w = SW;
    orig_planes.sub_h = SH;
    orig_planes.bit_depth = BIT_DEPTH;
    orig_planes.r.resize(count);
    orig_planes.g1.resize(count);
    orig_planes.g2.resize(count);
    orig_planes.b.resize(count);

    for (size_t i = 0; i < count; ++i) {
        orig_planes.r[i]  = static_cast<uint16_t>((i * 13 + 5) & 0x3FF);
        orig_planes.g1[i] = static_cast<uint16_t>((i * 17 + 8) & 0x3FF);
        orig_planes.g2[i] = static_cast<uint16_t>((orig_planes.g1[i] + (i % 3)) & 0x3FF);
        orig_planes.b[i]  = static_cast<uint16_t>((i * 23 + 11) & 0x3FF);
    }

    auto [ar, ab, brb] = Psc2Engine::estimate_quad_bayer_shears(orig_planes);

    Psc2Engine::QuadBayerPlanes test_planes = orig_planes;
    Psc2Engine::forward_quad_bayer_aiklt(test_planes, ar, ab, brb);
    Psc2Engine::inverse_quad_bayer_aiklt(test_planes, ar, ab, brb);

    for (size_t i = 0; i < count; ++i) {
        PRISM_ASSERT_EQ(test_planes.r[i],  orig_planes.r[i]);
        PRISM_ASSERT_EQ(test_planes.g1[i], orig_planes.g1[i]);
        PRISM_ASSERT_EQ(test_planes.g2[i], orig_planes.g2[i]);
        PRISM_ASSERT_EQ(test_planes.b[i],  orig_planes.b[i]);
    }
}

PRISM_TEST(test_psc2_full_frame_compression_psnr_infinity) {
    Psc2Config cfg;
    cfg.width = 64;
    cfg.height = 64;
    cfg.tile_size = 32;
    cfg.bit_depth = 8;
    cfg.enable_directional_wavelet = true;
    cfg.enable_spectral_coupling = true;
    cfg.filter = WaveletFilterType::LEGALL_53;

    Psc2Engine engine(cfg);

    std::vector<uint8_t> raw_rgb(cfg.width * cfg.height * 3);
    for (uint32_t y = 0; y < cfg.height; ++y) {
        for (uint32_t x = 0; x < cfg.width; ++x) {
            size_t idx = (y * cfg.width + x) * 3;
            raw_rgb[idx + 0] = static_cast<uint8_t>((x * 7) % 256);
            raw_rgb[idx + 1] = static_cast<uint8_t>((y * 9) % 256);
            raw_rgb[idx + 2] = static_cast<uint8_t>((x + y) % 256);
        }
    }

    auto comp_frame = engine.compress_frame_rgb(raw_rgb.data(), raw_rgb.size());
    PRISM_ASSERT(!comp_frame.tile_headers.empty());
    PRISM_ASSERT(!comp_frame.bitstream_payload.empty());

    std::vector<uint8_t> decomp_rgb = engine.decompress_frame_rgb(comp_frame);
    PRISM_ASSERT_EQ(decomp_rgb.size(), raw_rgb.size());

    bool match = Psc2Engine::verify_pixel_parity(raw_rgb.data(), decomp_rgb.data(), raw_rgb.size());
    PRISM_ASSERT(match);
}

PRISM_TEST(test_psc2_quad_bayer_compression_psnr_infinity) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint8_t BIT_DEPTH = 10;

    Psc2Config cfg;
    cfg.width = W;
    cfg.height = H;
    cfg.bit_depth = BIT_DEPTH;
    cfg.enable_quad_bayer_coupling = true;

    Psc2Engine engine(cfg);

    std::vector<uint16_t> bayer_raw(W * H);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            bayer_raw[y * W + x] = static_cast<uint16_t>(((x * 13) ^ (y * 17)) & 0x3FF);
        }
    }

    auto comp = engine.compress_quad_bayer(bayer_raw.data(), W, H, BIT_DEPTH);
    PRISM_ASSERT(comp.is_quad_bayer);

    std::vector<uint16_t> decomp = engine.decompress_quad_bayer(comp);
    PRISM_ASSERT_EQ(decomp.size(), bayer_raw.size());

    bool match = Psc2Engine::verify_sample_parity_u16(bayer_raw.data(), decomp.data(), bayer_raw.size());
    PRISM_ASSERT(match);
}

PRISM_TEST(test_psc2_frame_serialization_roundtrip) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    Psc2Config cfg;
    cfg.width = W;
    cfg.height = H;
    cfg.tile_size = 32;
    Psc2Engine engine(cfg);

    std::vector<uint8_t> rgb(W * H * 3);
    for (size_t i = 0; i < rgb.size(); ++i) {
        rgb[i] = static_cast<uint8_t>((i * 17) ^ 0x5A);
    }

    auto comp = engine.compress_frame_rgb(rgb.data(), rgb.size());
    std::vector<uint8_t> bytes = Psc2Engine::serialize_frame(comp);
    PRISM_ASSERT(!bytes.empty());

    auto deser = Psc2Engine::deserialize_frame(bytes.data(), bytes.size());
    PRISM_ASSERT_EQ(deser.config.width, W);
    PRISM_ASSERT_EQ(deser.config.height, H);
    PRISM_ASSERT_EQ(deser.tile_headers.size(), comp.tile_headers.size());

    std::vector<uint8_t> recon = engine.decompress_frame_rgb(deser);
    PRISM_ASSERT_EQ(recon.size(), rgb.size());
    PRISM_ASSERT(Psc2Engine::verify_pixel_parity(rgb.data(), recon.data(), rgb.size()));
}

PRISM_TEST(test_psc2_chunk_payload_roundtrip) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;

    Psc2Config cfg;
    cfg.width = W;
    cfg.height = H;
    cfg.tile_size = 16;

    // 1. RGB chunk payload
    std::vector<uint8_t> raw_rgb(W * H * 3);
    for (size_t i = 0; i < raw_rgb.size(); ++i) {
        raw_rgb[i] = static_cast<uint8_t>((i * 7 + 23) & 0xFF);
    }

    std::vector<uint8_t> comp_rgb = Psc2Engine::compress_chunk_payload(raw_rgb.data(), raw_rgb.size(), cfg, false);
    PRISM_ASSERT(!comp_rgb.empty());

    std::vector<uint8_t> decomp_rgb = Psc2Engine::decompress_chunk_payload(comp_rgb.data(), comp_rgb.size(), raw_rgb.size());
    PRISM_ASSERT_EQ(decomp_rgb.size(), raw_rgb.size());
    PRISM_ASSERT(Psc2Engine::verify_pixel_parity(raw_rgb.data(), decomp_rgb.data(), raw_rgb.size()));

    // 2. Quad-Bayer chunk payload
    std::vector<uint16_t> raw_bayer(W * H);
    for (size_t i = 0; i < raw_bayer.size(); ++i) {
        raw_bayer[i] = static_cast<uint16_t>((i * 31) & 0x3FF);
    }
    const uint8_t* bayer_bytes = reinterpret_cast<const uint8_t*>(raw_bayer.data());
    size_t bayer_size = raw_bayer.size() * sizeof(uint16_t);

    std::vector<uint8_t> comp_bayer = Psc2Engine::compress_chunk_payload(bayer_bytes, bayer_size, cfg, true);
    PRISM_ASSERT(!comp_bayer.empty());

    std::vector<uint8_t> decomp_bayer = Psc2Engine::decompress_chunk_payload(comp_bayer.data(), comp_bayer.size(), bayer_size);
    PRISM_ASSERT_EQ(decomp_bayer.size(), bayer_size);
    PRISM_ASSERT(std::memcmp(bayer_bytes, decomp_bayer.data(), bayer_size) == 0);
}

PRISM_TEST(test_psc2_checkerboard_convnet_determinism_and_bounds) {
    const auto& convnet = Psc2Engine::get_convnet();

    // Verify parameter bounds across synthetic causal test patterns
    for (int p = 0; p < 4; ++p) {
        for (int16_t v = -1000; v <= 1000; v += 137) {
            auto params = convnet.predict(v, v + 10, v - 10, v + 20, v, static_cast<uint8_t>(p));
            PRISM_ASSERT(params.sigma >= 4 && params.sigma <= 127);
            PRISM_ASSERT(params.mu >= -32768 && params.mu <= 32767);
        }
    }

    // Assert determinism across repeated calls
    auto p1 = convnet.predict(150, 160, 145, 170, 155, 1);
    auto p2 = convnet.predict(150, 160, 145, 170, 155, 1);
    PRISM_ASSERT_EQ(p1.mu, p2.mu);
    PRISM_ASSERT_EQ(p1.sigma, p2.sigma);
}

PRISM_TEST(test_psc2_adiw_i32_high_bit_depth_reversibility) {
    constexpr uint32_t W = 32;
    constexpr uint32_t H = 32;
    std::vector<int32_t> orig_tile(W * H);

    // 16-bit sensor input dynamic range: samples up to 65535
    for (size_t i = 0; i < orig_tile.size(); ++i) {
        orig_tile[i] = static_cast<int32_t>((i * 1031 + 47) % 65536);
    }

    const AdiwOrientation orientations[] = {
        AdiwOrientation::HORIZONTAL,
        AdiwOrientation::VERTICAL,
        AdiwOrientation::DIAGONAL,
        AdiwOrientation::ANTI_DIAG
    };

    for (auto ori : orientations) {
        // Test 32-bit LeGall 5/3
        std::vector<int32_t> tile53 = orig_tile;
        Psc2Engine::forward_adiw_2d_i32(tile53.data(), W, W, H, ori, WaveletFilterType::LEGALL_53);
        Psc2Engine::inverse_adiw_2d_i32(tile53.data(), W, W, H, ori, WaveletFilterType::LEGALL_53);

        for (size_t i = 0; i < orig_tile.size(); ++i) {
            PRISM_ASSERT_EQ(tile53[i], orig_tile[i]);
        }

        // Test 32-bit CDF 9/7
        std::vector<int32_t> tile97 = orig_tile;
        Psc2Engine::forward_adiw_2d_i32(tile97.data(), W, W, H, ori, WaveletFilterType::CDF_97_INT);
        Psc2Engine::inverse_adiw_2d_i32(tile97.data(), W, W, H, ori, WaveletFilterType::CDF_97_INT);

        for (size_t i = 0; i < orig_tile.size(); ++i) {
            PRISM_ASSERT_EQ(tile97[i], orig_tile[i]);
        }
    }
}

PRISM_TEST(test_psc2_checkerboard_convnet_compression_psnr_infinity) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    Psc2Config cfg;
    cfg.width = W;
    cfg.height = H;
    cfg.tile_size = 32;
    cfg.bit_depth = 8;
    cfg.enable_directional_wavelet = true;
    cfg.enable_spectral_coupling = true;
    cfg.enable_checkerboard_convnet = true; // 4-Pass Checkerboard ConvNet prior
    cfg.filter = WaveletFilterType::LEGALL_53;

    Psc2Engine engine(cfg);

    std::vector<uint8_t> raw_rgb(W * H * 3);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            size_t idx = (y * W + x) * 3;
            raw_rgb[idx + 0] = static_cast<uint8_t>((x * 9 + y * 5) % 256);
            raw_rgb[idx + 1] = static_cast<uint8_t>((x * 3 + y * 11) % 256);
            raw_rgb[idx + 2] = static_cast<uint8_t>((x * 7 ^ y * 13) % 256);
        }
    }

    auto comp = engine.compress_frame_rgb(raw_rgb.data(), raw_rgb.size());
    PRISM_ASSERT(!comp.bitstream_payload.empty());

    // Serialize and deserialize bitstream
    std::vector<uint8_t> serialized = Psc2Engine::serialize_frame(comp);
    auto deser = Psc2Engine::deserialize_frame(serialized.data(), serialized.size());
    PRISM_ASSERT(deser.config.enable_checkerboard_convnet);

    std::vector<uint8_t> decomp = engine.decompress_frame_rgb(deser);
    PRISM_ASSERT_EQ(decomp.size(), raw_rgb.size());
    PRISM_ASSERT(Psc2Engine::verify_pixel_parity(raw_rgb.data(), decomp.data(), raw_rgb.size()));
}

PRISM_TEST(test_psc2_quad_bayer_multi_bit_depth_parity) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    const uint8_t bit_depths[] = {10, 12, 14, 16};

    for (uint8_t bd : bit_depths) {
        Psc2Config cfg;
        cfg.width = W;
        cfg.height = H;
        cfg.bit_depth = bd;
        cfg.enable_quad_bayer_coupling = true;

        Psc2Engine engine(cfg);
        uint32_t max_val = (bd == 16) ? 65535 : ((1u << bd) - 1);

        std::vector<uint16_t> bayer_raw(W * H);
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                bayer_raw[y * W + x] = static_cast<uint16_t>(((x * 37) ^ (y * 59) + 11) % (max_val + 1));
            }
        }

        auto comp = engine.compress_quad_bayer(bayer_raw.data(), W, H, bd);
        std::vector<uint16_t> decomp = engine.decompress_quad_bayer(comp);

        PRISM_ASSERT_EQ(decomp.size(), bayer_raw.size());
        PRISM_ASSERT(Psc2Engine::verify_sample_parity_u16(bayer_raw.data(), decomp.data(), bayer_raw.size()));
    }
}

PRISM_TEST(test_psc2_multithreaded_wavefront_parity) {
    constexpr uint32_t W = 256;
    constexpr uint32_t H = 256;

    std::vector<uint8_t> raw_rgb(W * H * 3);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            size_t idx = (y * W + x) * 3;
            raw_rgb[idx + 0] = static_cast<uint8_t>((x * 11 + y * 7) % 256);
            raw_rgb[idx + 1] = static_cast<uint8_t>((x * 19 ^ y * 13) % 256);
            raw_rgb[idx + 2] = static_cast<uint8_t>((x * 3 + y * 23 + 47) % 256);
        }
    }

    const uint32_t thread_counts[] = {1, 2, 4, 8};
    for (uint32_t threads : thread_counts) {
        // Test with MED
        {
            Psc2Config cfg;
            cfg.width = W;
            cfg.height = H;
            cfg.tile_size = 64; // 4x4 = 16 tiles
            cfg.enable_directional_wavelet = true;
            cfg.enable_spectral_coupling = true;
            cfg.enable_checkerboard_convnet = false;
            cfg.num_worker_threads = threads;

            Psc2Engine engine(cfg);
            auto comp = engine.compress_frame_rgb(raw_rgb.data(), raw_rgb.size());
            PRISM_ASSERT_EQ(comp.tile_headers.size(), 16u);

            std::vector<uint8_t> decomp = engine.decompress_frame_rgb(comp);
            PRISM_ASSERT_EQ(decomp.size(), raw_rgb.size());
            PRISM_ASSERT(Psc2Engine::verify_pixel_parity(raw_rgb.data(), decomp.data(), raw_rgb.size()));
        }

        // Test with Checkerboard ConvNet
        {
            Psc2Config cfg;
            cfg.width = W;
            cfg.height = H;
            cfg.tile_size = 64; // 16 tiles
            cfg.enable_directional_wavelet = true;
            cfg.enable_spectral_coupling = true;
            cfg.enable_checkerboard_convnet = true;
            cfg.num_worker_threads = threads;

            Psc2Engine engine(cfg);
            auto comp = engine.compress_frame_rgb(raw_rgb.data(), raw_rgb.size());
            PRISM_ASSERT_EQ(comp.tile_headers.size(), 16u);

            std::vector<uint8_t> decomp = engine.decompress_frame_rgb(comp);
            PRISM_ASSERT_EQ(decomp.size(), raw_rgb.size());
            PRISM_ASSERT(Psc2Engine::verify_pixel_parity(raw_rgb.data(), decomp.data(), raw_rgb.size()));
        }
    }
}

PRISM_TEST(test_psc2_p010_sensor_format_psnr_infinity) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint8_t BIT_DEPTH = 10; // 10-bit P010 sensor samples

    Psc2Config cfg;
    cfg.width = W;
    cfg.height = H;
    cfg.bit_depth = BIT_DEPTH;
    cfg.tile_size = 32;
    cfg.enable_quad_bayer_coupling = true;

    Psc2Engine engine(cfg);

    // Create 10-bit HDR sensor pattern [0..1023]
    std::vector<uint16_t> p010_raw(W * H);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            p010_raw[y * W + x] = static_cast<uint16_t>(((x * 29 + y * 41) ^ 0x123) & 0x3FF);
        }
    }

    auto comp = engine.compress_quad_bayer(p010_raw.data(), W, H, BIT_DEPTH);
    PRISM_ASSERT(comp.is_quad_bayer);

    std::vector<uint16_t> decomp = engine.decompress_quad_bayer(comp);
    PRISM_ASSERT_EQ(decomp.size(), p010_raw.size());

    bool match = Psc2Engine::verify_sample_parity_u16(p010_raw.data(), decomp.data(), p010_raw.size());
    PRISM_ASSERT(match);
}

