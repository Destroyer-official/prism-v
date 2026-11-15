#pragma once
#ifndef PRISM_PSC_HPP
#define PRISM_PSC_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/rans8.hpp"
#include <cstdint>
#include <vector>
#include <array>

namespace prism {

struct RgbPixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct YCoCgPixel {
    int16_t y;
    int16_t co;
    int16_t cg;
};

/**
 * @brief Mode L-PSC: Bijective Subband-Tokenized Wavelet Codec.
 * Decoded-Sample-Exact Archival (PSNR = infinity).
 *
 * Replaces heavy neural transformers with:
 * 1. Reversible Color Transform (YCoCg-R).
 * 2. 2D Reversible Integer LeGall 5/3 Lifting Wavelet.
 * 3. 4-Pass Checkerboard Causal Integer ConvNet (0.34 GMACs/frame, ~110 MACs/pixel).
 * 4. 8-Way Interleaved SIMD rANS Entropy Coding.
 */
class PscEngine {
public:
    struct FrameGeometry {
        uint32_t width = 0;
        uint32_t height = 0;
        uint8_t bit_depth = 8;
    };

    struct WaveletSubbands {
        std::vector<int16_t> ll;
        std::vector<int16_t> lh;
        std::vector<int16_t> hl;
        std::vector<int16_t> hh;
        uint32_t sub_w = 0;
        uint32_t sub_h = 0;
    };

    struct CompressedPscFrame {
        FrameGeometry geometry;
        std::vector<uint8_t> rans_payload;
        Sha256Digest original_pixel_sha256{};
    };

    PscEngine();

    // Bayer CFA 4-Plane Splitting & Sensor Preservation (Section 3.7 & P09/P18)
    struct BayerCfaSubplanes {
        uint32_t sub_w = 0;
        uint32_t sub_h = 0;
        uint8_t bit_depth = 10; // 10, 12, 14, 16 bits
        std::vector<uint16_t> r;
        std::vector<uint16_t> g1; // Green-Red
        std::vector<uint16_t> g2; // Green-Blue
        std::vector<uint16_t> b;
    };

    struct CompressedBayerFrame {
        uint32_t width = 0;
        uint32_t height = 0;
        uint8_t bit_depth = 10;
        bool is_delta = false;
        std::vector<uint8_t> rans_payload;
        Sha256Digest original_bayer_sha256{};
    };

    static BayerCfaSubplanes splice_bayer_cfa(const uint16_t* bayer_raw, uint32_t width, uint32_t height, uint8_t bit_depth);
    static void merge_bayer_cfa(const BayerCfaSubplanes& planes, uint16_t* out_bayer_raw, uint32_t width, uint32_t height);
    static BayerCfaSubplanes compute_bayer_temporal_delta(const BayerCfaSubplanes& curr, const BayerCfaSubplanes& ref);
    static BayerCfaSubplanes apply_bayer_temporal_delta(const BayerCfaSubplanes& delta, const BayerCfaSubplanes& ref);

    CompressedBayerFrame compress_bayer_frame(const uint16_t* bayer_raw, uint32_t width, uint32_t height, uint8_t bit_depth, const BayerCfaSubplanes* ref_planes = nullptr);
    std::vector<uint16_t> decompress_bayer_frame(const CompressedBayerFrame& frame, const BayerCfaSubplanes* ref_planes = nullptr);
    static bool verify_bayer_parity(const uint16_t* orig, const uint16_t* recon, size_t count);

    // Reversible Color Transforms (YCoCg-R)
    static YCoCgPixel rgb_to_ycocgr(RgbPixel rgb);
    static RgbPixel ycocgr_to_rgb(YCoCgPixel ycocg);

    // 1D Reversible LeGall 5/3 Lifting Wavelet
    static void forward_legall53_1d(const int16_t* in, int16_t* low, int16_t* high, size_t n);
    static void inverse_legall53_1d(const int16_t* low, const int16_t* high, int16_t* out, size_t n);

    // 2D Reversible LeGall 5/3 Lifting Wavelet
    static WaveletSubbands forward_wavelet_2d(const int16_t* plane, uint32_t width, uint32_t height);
    static void inverse_wavelet_2d(const WaveletSubbands& bands, int16_t* plane, uint32_t width, uint32_t height);

    // Median Edge Detection (MED) predictor for LL subband
    static int16_t med_predict(int16_t left, int16_t top, int16_t topleft);

    /**
     * @brief Compress raw planar image into a lossless CompressedPscFrame.
     */
    CompressedPscFrame compress_frame(const uint8_t* raw_rgb_or_yuv, const FrameGeometry& geom);

    /**
     * @brief Decompress a CompressedPscFrame back to the original pixel buffers.
     * The decode path checks geometry and the stored payload hash.
     */
    std::vector<uint8_t> decompress_frame(const CompressedPscFrame& frame);

    /**
     * @brief Compare two sample buffers for exact equality.
     * @return true when every byte matches.
     */
    static bool verify_sample_parity(const uint8_t* original, const uint8_t* reconstructed, size_t size);
};

} // namespace prism

#endif // PRISM_PSC_HPP
