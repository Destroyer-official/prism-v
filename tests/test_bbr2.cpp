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

PRISM_TEST(test_bbr2_edge_cases_and_empty) {
    Bbr2Engine engine;

    // 1. Empty buffer
    {
        Bbr2Chunk empty_chunk = engine.compress(nullptr, 0, SourceCodecId::CODEC_H264);
        PRISM_ASSERT_EQ(empty_chunk.uncompressed_size, 0);
        std::vector<uint8_t> decomp = engine.decompress(empty_chunk);
        PRISM_ASSERT(decomp.empty());
    }

    // 2. 1-byte buffer
    {
        uint8_t single_byte = 0x42;
        Bbr2Chunk chunk = engine.compress(&single_byte, 1, SourceCodecId::CODEC_H264);
        PRISM_ASSERT_EQ(chunk.uncompressed_size, 1);
        std::vector<uint8_t> decomp = engine.decompress(chunk);
        PRISM_ASSERT_EQ(decomp.size(), 1);
        PRISM_ASSERT_EQ(decomp[0], 0x42);
    }

    // 3. All-zeros buffer (extreme DCT sparsity)
    {
        std::vector<uint8_t> zeros(4096, 0);
        Sha256Digest orig_sha = Bbr2Engine::compute_sha256(zeros.data(), zeros.size());
        Bbr2Chunk chunk = engine.compress(zeros.data(), zeros.size(), SourceCodecId::CODEC_H264);
        PRISM_ASSERT(chunk.rans_payload.size() < zeros.size() / 2); // Heavy compression on pure zeros
        std::vector<uint8_t> decomp = engine.decompress(chunk);
        PRISM_ASSERT_EQ(decomp.size(), zeros.size());
        Sha256Digest recon_sha = Bbr2Engine::compute_sha256(decomp.data(), decomp.size());
        PRISM_ASSERT_EQ(recon_sha, orig_sha);
    }

    // 4. Prime-sized buffer (1009 bytes)
    {
        std::vector<uint8_t> prime_buf(1009);
        for (size_t i = 0; i < prime_buf.size(); ++i) {
            prime_buf[i] = static_cast<uint8_t>((i * 67 + 13) % 256);
        }
        Sha256Digest orig_sha = Bbr2Engine::compute_sha256(prime_buf.data(), prime_buf.size());
        Bbr2Chunk chunk = engine.compress(prime_buf.data(), prime_buf.size(), SourceCodecId::CODEC_H264);
        std::vector<uint8_t> decomp = engine.decompress(chunk);
        PRISM_ASSERT_EQ(decomp.size(), prime_buf.size());
        Sha256Digest recon_sha = Bbr2Engine::compute_sha256(decomp.data(), decomp.size());
        PRISM_ASSERT_EQ(recon_sha, orig_sha);
    }
}

PRISM_TEST(test_bbr2_stream_detokenizer_equivalence) {
    std::vector<uint8_t> original(4096);
    std::mt19937 rng(777);
    for (size_t i = 0; i < original.size(); ++i) {
        uint32_t r = rng() % 100;
        if (r < 75) original[i] = 0;
        else if (r < 90) original[i] = static_cast<uint8_t>((rng() % 6) + 1);
        else original[i] = static_cast<uint8_t>(rng() % 256);
    }

    std::vector<CmavqTokenizer::Token> tokens = CmavqTokenizer::tokenize(original.data(), original.size());
    PRISM_ASSERT(!tokens.empty());

    // Build raw symbols and extra payload
    std::vector<uint16_t> symbols;
    symbols.reserve(tokens.size());
    std::vector<uint8_t> extra_bytes;

    for (const auto& tok : tokens) {
        symbols.push_back(tok.symbol);
        if (tok.symbol == 0 || tok.symbol == 62 || tok.symbol == 63) {
            uint16_t ev = static_cast<uint16_t>(tok.extra_val & 0xFFFF);
            extra_bytes.push_back(static_cast<uint8_t>(ev & 0xFF));
            extra_bytes.push_back(static_cast<uint8_t>((ev >> 8) & 0xFF));
        }
    }

    // Detokenize via Token array
    std::vector<uint8_t> detok_array = CmavqTokenizer::detokenize(tokens, original.size());

    // Detokenize via direct stream
    std::vector<uint8_t> detok_stream = CmavqTokenizer::detokenize_stream(
        symbols.data(), symbols.size(),
        extra_bytes.data(), extra_bytes.size(),
        original.size());

    PRISM_ASSERT_EQ(detok_array.size(), original.size());
    PRISM_ASSERT_EQ(detok_stream.size(), original.size());

    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(detok_array[i], original[i]);
        PRISM_ASSERT_EQ(detok_stream[i], original[i]);
    }
}

PRISM_TEST(test_bbr2_discrepancy_patch_stream) {
    Bbr2Engine engine;

    std::vector<uint8_t> bitstream(2048, 0);
    for (size_t i = 0; i < bitstream.size(); ++i) {
        bitstream[i] = static_cast<uint8_t>((i % 7 == 0) ? (i % 256) : 0);
    }

    Bbr2Chunk chunk = engine.compress(bitstream.data(), bitstream.size(), SourceCodecId::CODEC_H264);

    // Inject synthetic OEM encoder discrepancy patch at offset 100
    DiscrepancyPatch patch;
    patch.byte_offset = 100;
    patch.patch_bytes = {0xAA, 0xBB, 0xCC, 0xDD}; // Non-canonical RBSP padding bytes
    chunk.delta_patches.push_back(patch);

    // Compute expected hash after patch
    std::vector<uint8_t> expected = bitstream;
    for (size_t k = 0; k < patch.patch_bytes.size(); ++k) {
        expected[patch.byte_offset + k] = patch.patch_bytes[k];
    }
    chunk.orig_sha256 = Bbr2Engine::compute_sha256(expected.data(), expected.size());

    // Decompress and verify patch application & SHA-256
    std::vector<uint8_t> restored = engine.decompress(chunk);
    PRISM_ASSERT_EQ(restored.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], expected[i]);
    }
}

PRISM_TEST(test_bbr2_chunk_serialization_and_bypass) {
    Bbr2Engine engine;

    // Normal chunk
    {
        std::vector<uint8_t> bitstream(1024, 0);
        bitstream[10] = 5;
        Bbr2Chunk chunk = engine.compress(bitstream.data(), bitstream.size(), SourceCodecId::CODEC_H264);
        std::vector<uint8_t> serialized = Bbr2Engine::serialize_chunk_payload(chunk);
        PRISM_ASSERT(!serialized.empty());

        Bbr2Chunk restored_chunk = Bbr2Engine::deserialize_chunk_payload(
            serialized.data(), serialized.size(), chunk.uncompressed_size, chunk.orig_sha256);
        PRISM_ASSERT_EQ(restored_chunk.uncompressed_size, chunk.uncompressed_size);
        PRISM_ASSERT_EQ(restored_chunk.orig_sha256, chunk.orig_sha256);

        std::vector<uint8_t> restored_data = engine.decompress(restored_chunk);
        PRISM_ASSERT_EQ(restored_data.size(), bitstream.size());
        for (size_t i = 0; i < bitstream.size(); ++i) {
            PRISM_ASSERT_EQ(restored_data[i], bitstream[i]);
        }
    }

    // Bypass chunk (simulated expansion)
    {
        Bbr2Chunk bypass_chunk;
        bypass_chunk.uncompressed_size = 64;
        bypass_chunk.extra_payload.clear();
        bypass_chunk.rans_payload.assign(64, 0xEF);
        bypass_chunk.orig_sha256 = Bbr2Engine::compute_sha256(bypass_chunk.rans_payload.data(), 64);

        std::vector<uint8_t> serialized = Bbr2Engine::serialize_chunk_payload(bypass_chunk);
        Bbr2Chunk restored_bypass = Bbr2Engine::deserialize_chunk_payload(
            serialized.data(), serialized.size(), bypass_chunk.uncompressed_size, bypass_chunk.orig_sha256);

        PRISM_ASSERT_EQ(restored_bypass.uncompressed_size, 64);
        PRISM_ASSERT(restored_bypass.extra_payload.empty());
        PRISM_ASSERT_EQ(restored_bypass.rans_payload.size(), 64);

        std::vector<uint8_t> restored_data = engine.decompress(restored_bypass);
        PRISM_ASSERT_EQ(restored_data.size(), 64);
        for (size_t i = 0; i < 64; ++i) {
            PRISM_ASSERT_EQ(restored_data[i], 0xEF);
        }
    }
}

PRISM_TEST(test_bbr2_multi_codec_hevc_av1_bit_exact) {
    Bbr2Engine engine;

    const SourceCodecId codecs[] = {
        SourceCodecId::CODEC_HEVC,
        SourceCodecId::CODEC_AV1
    };

    for (auto codec : codecs) {
        std::vector<uint8_t> bitstream(8192);
        std::mt19937 rng(static_cast<uint32_t>(codec) * 31337 + 101);

        for (size_t i = 0; i < bitstream.size(); ++i) {
            uint32_t r = rng() % 100;
            if (r < 70) {
                bitstream[i] = 0; // high zero sparsity
            } else if (r < 88) {
                bitstream[i] = static_cast<uint8_t>((rng() % 4) + 1);
            } else {
                bitstream[i] = static_cast<uint8_t>(rng() % 256);
            }
        }

        Sha256Digest orig_sha = Bbr2Engine::compute_sha256(bitstream.data(), bitstream.size());

        // Compress
        Bbr2Chunk chunk = engine.compress(bitstream.data(), bitstream.size(), codec);
        PRISM_ASSERT_EQ(chunk.orig_sha256, orig_sha);
        PRISM_ASSERT(!chunk.rans_payload.empty());

        // Decompress
        std::vector<uint8_t> restored = engine.decompress(chunk);
        PRISM_ASSERT_EQ(restored.size(), bitstream.size());

        Sha256Digest recon_sha = Bbr2Engine::compute_sha256(restored.data(), restored.size());
        PRISM_ASSERT_EQ(recon_sha, orig_sha);

        for (size_t i = 0; i < bitstream.size(); ++i) {
            PRISM_ASSERT_EQ(restored[i], bitstream[i]);
        }
    }
}

PRISM_TEST(test_bbr2_zero_expansion_bypass_invariant) {
    Bbr2Engine engine;

    // Generate high-entropy incompressible pseudorandom noise
    std::vector<uint8_t> random_noise(4096);
    std::mt19937 rng(123456789);
    for (size_t i = 0; i < random_noise.size(); ++i) {
        random_noise[i] = static_cast<uint8_t>(rng() % 256);
    }

    Sha256Digest orig_sha = Bbr2Engine::compute_sha256(random_noise.data(), random_noise.size());

    // Compress
    Bbr2Chunk chunk = engine.compress(random_noise.data(), random_noise.size(), SourceCodecId::CODEC_H264);

    // Assert zero-expansion invariant: compressed size must never exceed uncompressed size
    size_t total_comp = chunk.rans_payload.size() + chunk.extra_payload.size();
    PRISM_ASSERT(total_comp <= random_noise.size());

    // Decompress and verify bit-exact SHA-256
    std::vector<uint8_t> restored = engine.decompress(chunk);
    PRISM_ASSERT_EQ(restored.size(), random_noise.size());

    Sha256Digest recon_sha = Bbr2Engine::compute_sha256(restored.data(), restored.size());
    PRISM_ASSERT_EQ(recon_sha, orig_sha);

    for (size_t i = 0; i < random_noise.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], random_noise[i]);
    }
}

