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
