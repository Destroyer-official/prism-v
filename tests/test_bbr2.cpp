#include "test_harness.hpp"
#include "prism/bbr2.hpp"
#include <vector>
#include <random>

using namespace prism;

PRISM_TEST(test_bbr2_ssg_cr4_determinism) {
    SsgCr4Predictor pred1;
    SsgCr4Predictor pred2;

    for (int i = 0; i < 64; ++i) {
        pred1.step(45, (i % 8 == 0), (i % 16 == 0),
                   static_cast<int16_t>(i - 32),
                   static_cast<uint8_t>(i % 8),
                   static_cast<uint8_t>(i),
                   static_cast<uint8_t>(26 + (i % 4)));

        pred2.step(45, (i % 8 == 0), (i % 16 == 0),
                   static_cast<int16_t>(i - 32),
                   static_cast<uint8_t>(i % 8),
                   static_cast<uint8_t>(i),
                   static_cast<uint8_t>(26 + (i % 4)));
    }

    auto ctx1 = pred1.get_context();
    auto ctx2 = pred2.get_context();

    for (size_t k = 0; k < SsgCr4Predictor::OUTPUT_CTX_DIM; ++k) {
        PRISM_ASSERT_EQ(ctx1[k], ctx2[k]);
    }
}

PRISM_TEST(test_bbr2_cmavq_tokenization_bijective) {
    // Generate synthetic byte stream with zero runs, small levels, and escapes
    std::vector<uint8_t> original(4096);
    std::mt19937 rng(42);

    for (size_t i = 0; i < original.size(); ++i) {
        uint32_t r = rng() % 100;
        if (r < 70) {
            original[i] = 0; // High zero sparsity typical of DCT
        } else if (r < 90) {
            original[i] = static_cast<uint8_t>((rng() % 5) + 1);
        } else {
            original[i] = static_cast<uint8_t>(rng() % 256);
        }
    }

    std::vector<CmavqTokenizer::Token> tokens = CmavqTokenizer::tokenize(original.data(), original.size());
    PRISM_ASSERT(!tokens.empty());

    std::vector<uint8_t> reconstructed = CmavqTokenizer::detokenize(tokens, original.size());
    PRISM_ASSERT_EQ(reconstructed.size(), original.size());

    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(reconstructed[i], original[i]);
    }
}

PRISM_TEST(test_bbr2_bit_exact_sha256_roundtrip) {
    Bbr2Engine engine;

    std::vector<uint8_t> bitstream(8192);
    std::mt19937 rng(999);
    for (size_t i = 0; i < bitstream.size(); ++i) {
        uint32_t r = rng() % 100;
        if (r < 65) {
            bitstream[i] = 0;
        } else if (r < 85) {
            bitstream[i] = static_cast<uint8_t>((rng() % 4) + 1);
        } else {
            bitstream[i] = static_cast<uint8_t>(rng() % 256);
        }
    }

    Sha256Digest orig_sha = Bbr2Engine::compute_sha256(bitstream.data(), bitstream.size());

    // Compress with BBR 2.0 (Frontier 1)
    Bbr2Chunk chunk = engine.compress(bitstream.data(), bitstream.size(), SourceCodecId::CODEC_H264);
    PRISM_ASSERT_EQ(chunk.orig_sha256, orig_sha);
    PRISM_ASSERT(!chunk.rans_payload.empty());

    // Decompress with BBR 2.0
    std::vector<uint8_t> restored = engine.decompress(chunk);
    PRISM_ASSERT_EQ(restored.size(), bitstream.size());

    Sha256Digest recon_sha = Bbr2Engine::compute_sha256(restored.data(), restored.size());
    PRISM_ASSERT_EQ(recon_sha, orig_sha);

    for (size_t i = 0; i < bitstream.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], bitstream[i]);
    }
}
