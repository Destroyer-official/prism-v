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
