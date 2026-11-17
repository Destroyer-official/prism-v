#include "test_harness.hpp"
#include "prism/rans8.hpp"
#include <vector>
#include <random>

using namespace prism;

PRISM_TEST(test_rans8_interleaved_exact_roundtrip) {
    constexpr size_t NUM_SYMBOLS = 4096;
    constexpr size_t ALPHABET_SIZE = 16;

    // Create non-uniform frequency distribution summing to 32768
    std::vector<RansSymbol> stats(ALPHABET_SIZE);
    uint16_t freqs[ALPHABET_SIZE] = {
        8192, 4096, 4096, 2048, 2048, 2048, 1024, 1024,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
    };
    uint16_t run = 0;
    for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
        stats[i].start = run;
        stats[i].freq = freqs[i];
        run += freqs[i];
    }
    PRISM_ASSERT_EQ(run, 32768);

    std::vector<uint16_t> cdf_lut = Rans8Codec::build_cdf_lut(stats);

    // Generate pseudo-random symbols conforming to distribution
    std::vector<uint16_t> original(NUM_SYMBOLS);
    std::mt19937 rng(42);
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        uint32_t slot = rng() % 32768;
        original[i] = cdf_lut[slot];
    }

    // Encode
    Rans8Codec::EncodedChunk chunk = Rans8Codec::encode(original, stats);
    PRISM_ASSERT(!chunk.byte_stream.empty());
    PRISM_ASSERT_EQ(chunk.symbol_count, NUM_SYMBOLS);

    // Serialize & Deserialize
    std::vector<uint8_t> serialized = Rans8Codec::serialize(chunk);
    Rans8Codec::EncodedChunk restored_chunk = Rans8Codec::deserialize(serialized.data(), serialized.size());
    PRISM_ASSERT_EQ(restored_chunk.symbol_count, NUM_SYMBOLS);

    // Decode
    std::vector<uint16_t> decoded = Rans8Codec::decode(restored_chunk, cdf_lut, stats);
    PRISM_ASSERT_EQ(decoded.size(), NUM_SYMBOLS);

    // Verify 100.000% symbol identity across all 8 lanes
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        if (original[i] != decoded[i]) {
            throw std::runtime_error("Symbol mismatch at index " + std::to_string(i));
        }
    }
}

PRISM_TEST(test_rans8_statistics_validation) {
    // 1. Non-contiguous starts (gap between symbols)
    std::vector<RansSymbol> bad_starts = {
        RansSymbol{0, 16384},
        RansSymbol{16385, 16383} // Gap at 16384!
    };
    bool threw1 = false;
    try {
        Rans8Codec::encode({0, 1}, bad_starts);
    } catch (const std::invalid_argument&) {
        threw1 = true;
    }
    PRISM_ASSERT(threw1);

    // 2. Zero frequency
    std::vector<RansSymbol> zero_freq = {
        RansSymbol{0, 32768},
        RansSymbol{32768, 0}
    };
    bool threw2 = false;
    try {
        Rans8Codec::encode({0}, zero_freq);
    } catch (const std::invalid_argument&) {
        threw2 = true;
    }
    PRISM_ASSERT(threw2);

    // 3. Frequency sum != 32768
    std::vector<RansSymbol> wrong_sum = {
        RansSymbol{0, 10000},
        RansSymbol{10000, 10000}
    };
    bool threw3 = false;
    try {
        Rans8Codec::encode({0, 1}, wrong_sum);
    } catch (const std::invalid_argument&) {
        threw3 = true;
    }
    PRISM_ASSERT(threw3);
}

PRISM_TEST(test_rans8_truncated_stream_rejected) {
    std::vector<RansSymbol> stats = {
        RansSymbol{0, 16384},
        RansSymbol{16384, 16384}
    };
    std::vector<uint16_t> cdf_lut = Rans8Codec::build_cdf_lut(stats);
    std::vector<uint16_t> symbols = {0, 1, 0, 1, 1, 0, 1, 0};

    Rans8Codec::EncodedChunk chunk = Rans8Codec::encode(symbols, stats);

    // Truncate chunk byte stream
    Rans8Codec::EncodedChunk truncated = chunk;
    truncated.byte_stream.resize(10); // Less than header size (8 * 4 = 32 bytes)
    bool threw = false;
    try {
        Rans8Codec::decode(truncated, cdf_lut, stats);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    PRISM_ASSERT(threw);
}

PRISM_TEST(test_rans8_skewed_distribution_and_boundary_conditions) {
    constexpr size_t ALPHABET_SIZE = 64;
    std::vector<RansSymbol> stats(ALPHABET_SIZE);

    // Symbol 0 dominant (32768 - 63 * 8 = 32264)
    constexpr uint16_t MIN_F = 8;
    constexpr uint16_t DOM_F = 32768 - (ALPHABET_SIZE - 1) * MIN_F;
    stats[0].freq = DOM_F;
    stats[0].start = 0;
    uint16_t run = DOM_F;
    for (size_t i = 1; i < ALPHABET_SIZE; ++i) {
        stats[i].freq = MIN_F;
        stats[i].start = run;
        run += MIN_F;
    }
    PRISM_ASSERT_EQ(run, 32768);

    std::vector<uint16_t> cdf_lut = Rans8Codec::build_cdf_lut(stats);

    // Mostly symbol 0 with rare boundary symbols
    std::vector<uint16_t> original(4096, 0);
    for (size_t i = 0; i < original.size(); i += 50) {
        original[i] = static_cast<uint16_t>(i % ALPHABET_SIZE);
    }

    Rans8Codec::EncodedChunk chunk = Rans8Codec::encode(original, stats);
    std::vector<uint16_t> decoded = Rans8Codec::decode(chunk, cdf_lut, stats);

    PRISM_ASSERT_EQ(decoded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}
