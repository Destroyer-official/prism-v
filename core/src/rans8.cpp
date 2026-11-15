#include "prism/rans8.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace prism {

std::vector<uint16_t> Rans8Codec::build_cdf_lut(const std::vector<RansSymbol>& symbol_stats) {
    std::vector<uint16_t> lut(RANS_M, 0);
    for (size_t s = 0; s < symbol_stats.size(); ++s) {
        uint16_t start = symbol_stats[s].start;
        uint16_t end = start + symbol_stats[s].freq;
        for (uint16_t m = start; m < end && m < RANS_M; ++m) {
            lut[m] = static_cast<uint16_t>(s);
        }
    }
    return lut;
}

Rans8Codec::EncodedChunk Rans8Codec::encode(const std::vector<uint16_t>& symbols,
                                          const std::vector<RansSymbol>& symbol_stats) {
    EncodedChunk chunk;
    chunk.symbol_count = symbols.size();
    
    // Initialize 8 independent rANS states to L
    for (size_t l = 0; l < NUM_LANES; ++l) {
        chunk.final_states[l] = RANS_L;
    }

    if (symbols.empty()) {
        return chunk;
    }

    // Per-lane emitted 16-bit words
    std::array<std::vector<uint16_t>, NUM_LANES> lane_words;

    // Encode backwards from N-1 down to 0
    for (size_t idx = symbols.size(); idx > 0; --idx) {
        size_t i = idx - 1;
        size_t lane = i % NUM_LANES;
        uint16_t sym = symbols[i];
        
        if (sym >= symbol_stats.size()) {
            throw std::out_of_range("Symbol value exceeds statistics alphabet size");
        }

        const auto& stats = symbol_stats[sym];
        uint32_t f = stats.freq;
        uint32_t c = stats.start;

        if (f == 0) {
            throw std::invalid_argument("Cannot encode symbol with zero frequency");
        }

        uint32_t& x = chunk.final_states[lane];

        // Renormalization: emit 16-bit word while x >= 2 * f * 65536
        uint32_t x_max = (f << 17); // 2 * f * 65536 = f * 131072
        while (x >= x_max) {
            lane_words[lane].push_back(static_cast<uint16_t>(x & 0xFFFF));
            x >>= 16;
        }

        // rANS step
        x = (x / f) * RANS_M + (x % f) + c;
    }

    // Reverse each lane's word buffer so decoder reads sequentially
    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::reverse(lane_words[l].begin(), lane_words[l].end());
    }

    // Pack into byte stream: [word_counts: 8 * uint32_t] + [lane words]
    size_t total_words = 0;
    for (size_t l = 0; l < NUM_LANES; ++l) {
        total_words += lane_words[l].size();
    }

    chunk.byte_stream.resize(NUM_LANES * sizeof(uint32_t) + total_words * sizeof(uint16_t));
    uint8_t* out_ptr = chunk.byte_stream.data();

    // Write counts
    for (size_t l = 0; l < NUM_LANES; ++l) {
        uint32_t count = static_cast<uint32_t>(lane_words[l].size());
        std::memcpy(out_ptr, &count, sizeof(uint32_t));
        out_ptr += sizeof(uint32_t);
    }

    // Write words for each lane
    for (size_t l = 0; l < NUM_LANES; ++l) {
        size_t bytes = lane_words[l].size() * sizeof(uint16_t);
        if (bytes > 0) {
            std::memcpy(out_ptr, lane_words[l].data(), bytes);
            out_ptr += bytes;
        }
    }

    return chunk;
}

std::vector<uint16_t> Rans8Codec::decode(const EncodedChunk& chunk,
                                       const std::vector<uint16_t>& cdf_table,
                                       const std::vector<RansSymbol>& symbol_stats) {
    std::vector<uint16_t> symbols(chunk.symbol_count);
    if (chunk.symbol_count == 0) {
        return symbols;
    }

    if (chunk.byte_stream.size() < NUM_LANES * sizeof(uint32_t)) {
        throw std::runtime_error("Malformed rANS byte stream: truncated header");
    }

    // Unpack lane word counts and pointers
    const uint8_t* in_ptr = chunk.byte_stream.data();
    std::array<uint32_t, NUM_LANES> word_counts{};
    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(&word_counts[l], in_ptr, sizeof(uint32_t));
        in_ptr += sizeof(uint32_t);
    }

    std::array<const uint16_t*, NUM_LANES> lane_words{};
    std::array<size_t, NUM_LANES> word_idx{};
    for (size_t l = 0; l < NUM_LANES; ++l) {
        lane_words[l] = reinterpret_cast<const uint16_t*>(in_ptr);
        word_idx[l] = 0;
        in_ptr += word_counts[l] * sizeof(uint16_t);
    }

    // Initialize state from chunk.final_states
    std::array<uint32_t, NUM_LANES> states = chunk.final_states;

    // Decode symbols in forward order 0 to N-1
    for (size_t i = 0; i < chunk.symbol_count; ++i) {
        size_t lane = i % NUM_LANES;
        uint32_t& x = states[lane];

        // Slot lookup
        uint32_t m = x & (RANS_M - 1);
        if (m >= cdf_table.size()) {
            throw std::runtime_error("Slot exceeds CDF table bounds");
        }
        uint16_t sym = cdf_table[m];
        symbols[i] = sym;

        const auto& stats = symbol_stats[sym];
        uint32_t f = stats.freq;
        uint32_t c = stats.start;

        // Invert rANS step
        x = f * (x >> RANS_PROB_BITS) + (m - c);

        // Renormalize: while x < L, read word
        while (x < RANS_L) {
            if (word_idx[lane] < word_counts[lane]) {
                uint32_t word = lane_words[lane][word_idx[lane]++];
                x = (x << 16) | word;
            } else {
                break;
            }
        }
    }

    return symbols;
}

std::vector<uint8_t> Rans8Codec::serialize(const EncodedChunk& chunk) {
    // Header format:
    // [symbol_count: uint64_t]
    // [final_states: 8 * uint32_t]
    // [stream_len:   uint32_t]
    // [byte_stream:  stream_len bytes]
    size_t header_size = sizeof(uint64_t) + (NUM_LANES * sizeof(uint32_t)) + sizeof(uint32_t);
    std::vector<uint8_t> out(header_size + chunk.byte_stream.size());

    uint8_t* ptr = out.data();
    uint64_t sc = static_cast<uint64_t>(chunk.symbol_count);
    std::memcpy(ptr, &sc, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(ptr, &chunk.final_states[l], sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    uint32_t stream_len = static_cast<uint32_t>(chunk.byte_stream.size());
    std::memcpy(ptr, &stream_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (!chunk.byte_stream.empty()) {
        std::memcpy(ptr, chunk.byte_stream.data(), chunk.byte_stream.size());
    }

    return out;
}

Rans8Codec::EncodedChunk Rans8Codec::deserialize(const uint8_t* data, size_t size) {
    size_t min_header = sizeof(uint64_t) + (NUM_LANES * sizeof(uint32_t)) + sizeof(uint32_t);
    if (size < min_header) {
        throw std::runtime_error("Serialized rANS data too short for header");
    }

    EncodedChunk chunk;
    const uint8_t* ptr = data;

    uint64_t sc = 0;
    std::memcpy(&sc, ptr, sizeof(uint64_t));
    chunk.symbol_count = static_cast<size_t>(sc);
    ptr += sizeof(uint64_t);

    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(&chunk.final_states[l], ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    uint32_t stream_len = 0;
    std::memcpy(&stream_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (min_header + stream_len > size) {
        throw std::runtime_error("Serialized rANS byte stream truncated");
    }

    chunk.byte_stream.resize(stream_len);
    if (stream_len > 0) {
        std::memcpy(chunk.byte_stream.data(), ptr, stream_len);
    }

    return chunk;
}

} // namespace prism
