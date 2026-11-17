#include "test_harness.hpp"
#include "prism/rans16.hpp"
#include <vector>
#include <random>

using namespace prism;

PRISM_TEST(test_rans16_interleaved_exact_roundtrip) {
    constexpr size_t NUM_SYMBOLS = 8192;
    constexpr size_t ALPHABET_SIZE = 64;

    // Create non-uniform frequency distribution summing to 32768
    std::vector<RansSymbol> stats(ALPHABET_SIZE);
    uint32_t remaining = Rans16Codec::RANS_M; // 32768
    uint16_t running = 0;

    for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
        uint16_t freq = (i == ALPHABET_SIZE - 1)
            ? static_cast<uint16_t>(remaining)
            : static_cast<uint16_t>(std::max(8U, static_cast<uint32_t>(remaining / (ALPHABET_SIZE - i))));
        
        stats[i].start = running;
        stats[i].freq = freq;
        running += freq;
        remaining -= freq;
    }
    PRISM_ASSERT_EQ(running, 32768);

    std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(stats);

    // Generate pseudo-random symbols conforming to distribution
    std::vector<uint16_t> original(NUM_SYMBOLS);
    std::mt19937 rng(1337);
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        uint32_t slot = rng() % 32768;
        original[i] = cdf_lut[slot];
    }

    // Encode via 16-way SIMD interleaved rANS
    Rans16Codec::EncodedChunk chunk = Rans16Codec::encode(original, stats);
    PRISM_ASSERT(!chunk.byte_stream.empty());
    PRISM_ASSERT_EQ(chunk.symbol_count, NUM_SYMBOLS);

    // Serialize & Deserialize
    std::vector<uint8_t> serialized = Rans16Codec::serialize(chunk);
    Rans16Codec::EncodedChunk restored_chunk = Rans16Codec::deserialize(serialized.data(), serialized.size());
    PRISM_ASSERT_EQ(restored_chunk.symbol_count, NUM_SYMBOLS);

    // Decode via 16-way SIMD interleaved rANS
    std::vector<uint16_t> decoded = Rans16Codec::decode(restored_chunk, cdf_lut, stats);
    PRISM_ASSERT_EQ(decoded.size(), NUM_SYMBOLS);

    // Verify 100.000% symbol identity across all 16 lanes
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}

PRISM_TEST(test_rans16_lane_independence_and_tail) {
    constexpr size_t ALPHABET_SIZE = 16;
    std::vector<RansSymbol> stats(ALPHABET_SIZE);
    uint16_t freq = 32768 / ALPHABET_SIZE; // 2048 each
    uint16_t running = 0;
    for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
        stats[i].start = running;
        stats[i].freq = freq;
        running += freq;
    }
    std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(stats);

    // Test non-multiple of 16 length (e.g. 37 symbols) to verify tail loop handling
    std::vector<uint16_t> original(37);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint16_t>(i % ALPHABET_SIZE);
    }

    Rans16Codec::EncodedChunk chunk = Rans16Codec::encode(original, stats);
    std::vector<uint16_t> decoded = Rans16Codec::decode(chunk, cdf_lut, stats);

    PRISM_ASSERT_EQ(decoded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}

PRISM_TEST(test_rans16_statistics_validation) {
    // 1. Non-contiguous starts (gap between symbols)
    std::vector<RansSymbol> bad_starts = {
        RansSymbol{0, 16384},
        RansSymbol{16385, 16383} // Gap at 16384!
    };
    bool threw1 = false;
    try {
        Rans16Codec::encode({0, 1}, bad_starts);
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
        Rans16Codec::encode({0}, zero_freq);
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
        Rans16Codec::encode({0, 1}, wrong_sum);
    } catch (const std::invalid_argument&) {
        threw3 = true;
    }
    PRISM_ASSERT(threw3);
}

PRISM_TEST(test_rans16_truncated_stream_rejected) {
    std::vector<RansSymbol> stats = {
        RansSymbol{0, 16384},
        RansSymbol{16384, 16384}
    };
    std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(stats);
    std::vector<uint16_t> symbols = {0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0};

    Rans16Codec::EncodedChunk chunk = Rans16Codec::encode(symbols, stats);

    // Truncate chunk byte stream
    Rans16Codec::EncodedChunk truncated = chunk;
    truncated.byte_stream.resize(10); // Less than header size (16 * 4 = 64 bytes)
    bool threw = false;
    try {
        Rans16Codec::decode(truncated, cdf_lut, stats);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    PRISM_ASSERT(threw);
}

PRISM_TEST(test_rans16_skewed_distribution_and_boundary_conditions) {
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

    std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(stats);

    // Mostly symbol 0 with rare boundary symbols
    std::vector<uint16_t> original(4096, 0);
    for (size_t i = 0; i < original.size(); i += 50) {
        original[i] = static_cast<uint16_t>(i % ALPHABET_SIZE);
    }

    Rans16Codec::EncodedChunk chunk = Rans16Codec::encode(original, stats);
    std::vector<uint16_t> decoded = Rans16Codec::decode(chunk, cdf_lut, stats);

    PRISM_ASSERT_EQ(decoded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}

PRISM_TEST(test_rans16_sme2_query) {
    // Calling has_sme2_support should return without throwing on any target architecture
    bool sme2 = Rans16Codec::has_sme2_support();
    (void)sme2; // Value depends on target hardware (ARMv9 SME2 vs x86-64)
}

PRISM_TEST(test_rans16_high_volume_64k_stress) {
    constexpr size_t NUM_SYMBOLS = 65536; // 64K symbols stress test
    constexpr size_t ALPHABET_SIZE = 32;

    std::vector<RansSymbol> stats(ALPHABET_SIZE);
    uint16_t freq_per_sym = Rans16Codec::RANS_M / ALPHABET_SIZE; // 1024 each
    uint16_t running = 0;
    for (size_t s = 0; s < ALPHABET_SIZE; ++s) {
        stats[s].start = running;
        stats[s].freq = freq_per_sym;
        running += freq_per_sym;
    }
    PRISM_ASSERT_EQ(running, 32768);

    std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(stats);

    std::vector<uint16_t> original(NUM_SYMBOLS);
    std::mt19937 rng(987654321);
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        original[i] = static_cast<uint16_t>(rng() % ALPHABET_SIZE);
    }

    Rans16Codec::EncodedChunk chunk = Rans16Codec::encode(original, stats);
    PRISM_ASSERT_EQ(chunk.symbol_count, NUM_SYMBOLS);

    std::vector<uint8_t> bytes = Rans16Codec::serialize(chunk);
    Rans16Codec::EncodedChunk deser = Rans16Codec::deserialize(bytes.data(), bytes.size());
    PRISM_ASSERT_EQ(deser.symbol_count, NUM_SYMBOLS);

    std::vector<uint16_t> decoded = Rans16Codec::decode(deser, cdf_lut, stats);
    PRISM_ASSERT_EQ(decoded.size(), NUM_SYMBOLS);

    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}

