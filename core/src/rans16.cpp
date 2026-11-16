#include "prism/rans16.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace prism {

namespace {

void validate_symbol_stats16(const std::vector<RansSymbol>& symbol_stats) {
    uint32_t expected_start = 0;
    for (size_t s = 0; s < symbol_stats.size(); ++s) {
        const auto& sym = symbol_stats[s];
        if (sym.start != expected_start) {
            throw std::invalid_argument("Symbol statistics are not contiguous");
        }
        if (sym.freq == 0) {
            throw std::invalid_argument("Symbol statistics contain a zero frequency");
        }
        expected_start += sym.freq;
        if (expected_start > Rans16Codec::RANS_M) {
            throw std::invalid_argument("Symbol statistics exceed the probability space");
        }
    }
    if (expected_start != Rans16Codec::RANS_M) {
        throw std::invalid_argument("Symbol frequencies do not sum to the rANS probability space");
    }
}

} // namespace

std::vector<uint16_t> Rans16Codec::build_cdf_lut(const std::vector<RansSymbol>& symbol_stats) {
    std::vector<uint16_t> lut(RANS_M, 0);
    for (size_t s = 0; s < symbol_stats.size(); ++s) {
        uint32_t start = symbol_stats[s].start;
        uint32_t end = start + symbol_stats[s].freq;
        for (uint32_t m = start; m < end && m < RANS_M; ++m) {
            lut[m] = static_cast<uint16_t>(s);
        }
    }
    return lut;
}

Rans16Codec::EncodedChunk Rans16Codec::encode(const std::vector<uint16_t>& symbols,
                                             const std::vector<RansSymbol>& symbol_stats) {
    EncodedChunk chunk;
    chunk.symbol_count = symbols.size();

    // Initialize 16 independent rANS states to L
    for (size_t l = 0; l < NUM_LANES; ++l) {
        chunk.final_states[l] = RANS_L;
    }

    if (symbols.empty()) {
        return chunk;
    }

    validate_symbol_stats16(symbol_stats);

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

        uint32_t& x = chunk.final_states[lane];

        // Renormalization: emit 16-bit words while x >= 2 * f * 65536.
        const uint64_t x_max = static_cast<uint64_t>(f) << 17;
        while (static_cast<uint64_t>(x) >= x_max) {
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

    // Pack into byte stream: [word_counts: 16 * uint32_t] + [lane words]
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

std::vector<uint16_t> Rans16Codec::decode(const EncodedChunk& chunk,
                                         const std::vector<uint16_t>& cdf_table,
                                         const std::vector<RansSymbol>& symbol_stats) {
    std::vector<uint16_t> symbols(chunk.symbol_count);
    if (chunk.symbol_count == 0) {
        return symbols;
    }

    if (chunk.byte_stream.size() < NUM_LANES * sizeof(uint32_t)) {
        throw std::runtime_error("Malformed rANS16 byte stream: truncated header");
    }

    // Unpack lane word counts and pointers
    const uint8_t* in_ptr = chunk.byte_stream.data();
    const uint8_t* const stream_end = chunk.byte_stream.data() + chunk.byte_stream.size();
    std::array<uint32_t, NUM_LANES> word_counts{};
    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(&word_counts[l], in_ptr, sizeof(uint32_t));
        in_ptr += sizeof(uint32_t);
    }

    std::array<const uint16_t*, NUM_LANES> lane_words{};
    std::array<size_t, NUM_LANES> word_idx{};
    for (size_t l = 0; l < NUM_LANES; ++l) {
        size_t remaining = static_cast<size_t>(stream_end - in_ptr);
        if (static_cast<uint64_t>(word_counts[l]) * sizeof(uint16_t) > remaining) {
            throw std::runtime_error("Malformed rANS16 byte stream: lane exceeds stream bounds");
        }
        lane_words[l] = reinterpret_cast<const uint16_t*>(in_ptr);
        word_idx[l] = 0;
        in_ptr += word_counts[l] * sizeof(uint16_t);
    }

    // Initialize state from chunk.final_states
    std::array<uint32_t, NUM_LANES> states = chunk.final_states;

    const size_t full_blocks = chunk.symbol_count / NUM_LANES;
    const size_t tail = chunk.symbol_count % NUM_LANES;

    // Pointer arithmetic for lane word buffers
    std::array<const uint16_t*, NUM_LANES> lane_ptrs = lane_words;
    std::array<const uint16_t*, NUM_LANES> lane_ends{};
    for (size_t l = 0; l < NUM_LANES; ++l) {
        lane_ends[l] = lane_words[l] + word_counts[l];
    }

    const uint16_t* const cdf_ptr = cdf_table.data();
    const RansSymbol* const stats_ptr = symbol_stats.data();
    uint16_t* out_sym_ptr = symbols.data();

    // Vectorized / Unrolled 16-way decode loop
    for (size_t b = 0; b < full_blocks; ++b) {
        for (size_t lane = 0; lane < NUM_LANES; ++lane) {
            uint32_t x = states[lane];
            uint32_t m = x & (RANS_M - 1);
            uint16_t sym = cdf_ptr[m];
            out_sym_ptr[lane] = sym;

            const RansSymbol& stats = stats_ptr[sym];
            uint32_t f = stats.freq;
            uint32_t c = stats.start;

            x = f * (x >> RANS_PROB_BITS) + (m - c);

            if (x < RANS_L) {
                if (lane_ptrs[lane] < lane_ends[lane]) {
                    x = (x << 16) | (*lane_ptrs[lane]++);
                }
            }

            states[lane] = x;
        }
        out_sym_ptr += NUM_LANES;
    }

    // Tail symbols
    for (size_t lane = 0; lane < tail; ++lane) {
        uint32_t x = states[lane];
        uint32_t m = x & (RANS_M - 1);
        uint16_t sym = cdf_ptr[m];
        out_sym_ptr[lane] = sym;

        const RansSymbol& stats = stats_ptr[sym];
        uint32_t f = stats.freq;
        uint32_t c = stats.start;

        x = f * (x >> RANS_PROB_BITS) + (m - c);

        if (x < RANS_L) {
            if (lane_ptrs[lane] < lane_ends[lane]) {
                x = (x << 16) | (*lane_ptrs[lane]++);
            }
        }

        states[lane] = x;
    }

    return symbols;
}

std::vector<uint8_t> Rans16Codec::serialize(const EncodedChunk& chunk) {
    std::vector<uint8_t> out;
    size_t header_size = sizeof(uint64_t) + NUM_LANES * sizeof(uint32_t);
    out.resize(header_size + chunk.byte_stream.size());
    uint8_t* ptr = out.data();

    uint64_t sc = static_cast<uint64_t>(chunk.symbol_count);
    std::memcpy(ptr, &sc, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(ptr, &chunk.final_states[l], sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    if (!chunk.byte_stream.empty()) {
        std::memcpy(ptr, chunk.byte_stream.data(), chunk.byte_stream.size());
    }

    return out;
}

Rans16Codec::EncodedChunk Rans16Codec::deserialize(const uint8_t* data, size_t size) {
    size_t header_size = sizeof(uint64_t) + NUM_LANES * sizeof(uint32_t);
    if (size < header_size) {
        throw std::runtime_error("Malformed rANS16 serialized payload: too short for header");
    }

    EncodedChunk chunk;
    const uint8_t* ptr = data;

    uint64_t sc = 0;
    std::memcpy(&sc, ptr, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    if (sc > std::numeric_limits<size_t>::max() / 2) {
        throw std::runtime_error("Malformed rANS16 serialized payload: symbol count overflow");
    }
    chunk.symbol_count = static_cast<size_t>(sc);

    for (size_t l = 0; l < NUM_LANES; ++l) {
        std::memcpy(&chunk.final_states[l], ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    size_t stream_size = size - header_size;
    chunk.byte_stream.resize(stream_size);
    if (stream_size > 0) {
        std::memcpy(chunk.byte_stream.data(), ptr, stream_size);
    }

    return chunk;
}

} // namespace prism
