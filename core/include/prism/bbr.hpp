#pragma once
#ifndef PRISM_BBR_HPP
#define PRISM_BBR_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/rans8.hpp"
#include <cstdint>
#include <vector>
#include <array>
#include <string>

namespace prism {

// Syntax symbol types for CABAC syntax deconstruction
enum class BbrSyntaxType : uint8_t {
    NAL_HEADER = 0,
    SLICE_HEADER = 1,
    MB_SKIP_FLAG = 2,
    MB_TYPE = 3,
    PRED_MODE = 4,
    MVD_X = 5,
    MVD_Y = 6,
    CBF = 7,
    COEFF_SIG = 8,
    COEFF_SIGN = 9,
    COEFF_MAG = 10,
    EXP_GOLOMB_MISC = 11
};

struct BbrSyntaxSymbol {
    BbrSyntaxType type;
    uint16_t ctx_idx;
    int32_t val;
};

/**
 * @brief Position-Indexed Alphabet (HuffmanPos / ECB) for transform coefficients.
 * Maps transform coefficient (i, j) within 4x4, 8x8, or 16x16 blocks into 16 position-indexed buckets.
 */
class HuffmanPos {
public:
    static constexpr size_t NUM_BUCKETS = 16;

    static uint8_t get_position_bucket(uint8_t x, uint8_t y, uint8_t block_size = 8) {
        // Frequency distance along diagonal: x + y
        uint32_t diag = static_cast<uint32_t>(x) + static_cast<uint32_t>(y);
        uint32_t max_diag = (block_size * 2) - 2;
        if (max_diag == 0) return 0;
        uint32_t bucket = (diag * (NUM_BUCKETS - 1) + (max_diag / 2)) / max_diag;
        return static_cast<uint8_t>(bucket >= NUM_BUCKETS ? (NUM_BUCKETS - 1) : bucket);
    }
};

/**
 * @brief 1.1 KB INT8 Micro-MLP Conditioning Model.
 * Architecture: 16 -> 32 -> 16 (512 + 512 = 1,024 weights + 48 biases = 1,072 params = 1.1 KB).
 * Weights in INT8, biases in INT32, dyadic shift >> 7.
 * Fits entirely inside L1 data cache (<32 KB).
 */
class MicroMlp1K {
public:
    static constexpr size_t INPUT_DIM = 16;
    static constexpr size_t HIDDEN_DIM = 32;
    static constexpr size_t OUTPUT_DIM = 16;

    MicroMlp1K();

    /**
     * @brief Run forward pass on 16-element integer context vector.
     * Guaranteed deterministic and zero floating point.
     * @param input_context 16 causal INT16 features
     * @param output_cdf Array of 16 cumulative probabilities normalized to 32,768
     */
    void forward(const std::array<int16_t, INPUT_DIM>& input_context,
                 std::array<RansSymbol, OUTPUT_DIM>& output_symbols) const;

private:
    std::array<int8_t, HIDDEN_DIM * INPUT_DIM> w1_{};  // 32 x 16
    std::array<int32_t, HIDDEN_DIM> b1_{};             // 32
    std::array<int8_t, OUTPUT_DIM * HIDDEN_DIM> w2_{}; // 16 x 32
    std::array<int32_t, OUTPUT_DIM> b2_{};             // 16

    void initialize_normative_weights();
};

/**
 * @brief Sparse Discrepancy Patch Stream (Delta_patch).
 * Records exact byte differences when non-canonical OEM encoders emit non-standard padding.
 */
struct DiscrepancyPatch {
    uint32_t byte_offset = 0;
    std::vector<uint8_t> patch_bytes;
};

/**
 * @brief Compressed BBR Chunk representing a 256 KB slice of virtual bitstream.
 */
struct BbrChunk {
    uint32_t chunk_id = 0;
    uint32_t uncompressed_size = 0;
    std::vector<uint8_t> rans_payload;
    std::vector<DiscrepancyPatch> delta_patches;
    Sha256Digest orig_sha256{};
};

/**
 * @brief Mode L-BBR (Bitstream-Domain Syntax Recompression) Engine.
 * 100% bit-exact restoration guaranteed via SHA-256 verification.
 */
class BbrEngine {
public:
    BbrEngine();

    /**
     * @brief Compress raw H.264 or HEVC bitstream NAL units into a BbrChunk.
     */
    BbrChunk compress(const uint8_t* bitstream_data, size_t size, SourceCodecId codec_id);

    /**
     * @brief Decompress a BbrChunk back into the exact original bitstream.
     * @return 100.000% bit-exact reconstructed original byte stream.
     */
    std::vector<uint8_t> decompress(const BbrChunk& chunk);

    /**
     * @brief Compute cryptographic SHA-256 digest of a byte buffer.
     */
    static Sha256Digest compute_sha256(const uint8_t* data, size_t size);

private:
    MicroMlp1K mlp_;
};

} // namespace prism

#endif // PRISM_BBR_HPP
