#include "prism/bbr2.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace prism {

// SsgCr4Predictor Implementation
SsgCr4Predictor::SsgCr4Predictor() { reset(); }

void SsgCr4Predictor::reset() {
  state_.h[0] = 0;
  state_.h[1] = 0;
  state_.h[2] = 0;
  state_.h[3] = 0;
  prev_qp_ = 26;
}

void SsgCr4Predictor::step(uint8_t intra_angle, bool is_edge_h, bool is_edge_v,
                           int16_t level, uint8_t run, uint8_t pos,
                           uint8_t qp) {
  // Phase continuity coefficient along intra direction theta
  int32_t angle_diff = static_cast<int32_t>(intra_angle) - 45;
  if (angle_diff < 0)
    angle_diff = -angle_diff;
  int32_t a0 = 210 - (angle_diff * 2);
  if (a0 < 80)
    a0 = 80;

  int32_t a1 = is_edge_h ? 192 : 128;
  int32_t a2 = is_edge_v ? 192 : 128;
  int32_t a3 = 230;

  int32_t qp_delta = static_cast<int32_t>(qp) - static_cast<int32_t>(prev_qp_);
  prev_qp_ = qp;

  // 4-state causal recurrence update with dyadic shift >> 8
  state_.h[0] =
      static_cast<int16_t>((a0 * state_.h[0] + 64 * level + 128) >> 8);
  state_.h[1] = static_cast<int16_t>((a1 * state_.h[1] + 48 * run + 128) >> 8);
  state_.h[2] = static_cast<int16_t>((a2 * state_.h[2] + 32 * pos + 128) >> 8);
  state_.h[3] =
      static_cast<int16_t>((a3 * state_.h[3] + 16 * qp_delta + 128) >> 8);
}

std::array<int16_t, SsgCr4Predictor::OUTPUT_CTX_DIM>
SsgCr4Predictor::get_context() const {
  std::array<int16_t, OUTPUT_CTX_DIM> ctx{};
  ctx[0] = state_.h[0];
  ctx[1] = state_.h[1];
  ctx[2] = state_.h[2];
  ctx[3] = state_.h[3];
  ctx[4] = static_cast<int16_t>(state_.h[0] + state_.h[1]);
  ctx[5] = static_cast<int16_t>(state_.h[0] + state_.h[2]);
  ctx[6] = static_cast<int16_t>(state_.h[1] + state_.h[2]);
  ctx[7] = static_cast<int16_t>(state_.h[3] * 2);
  ctx[8] = static_cast<int16_t>(state_.h[0] >> 1);
  ctx[9] = static_cast<int16_t>(state_.h[1] >> 1);
  ctx[10] = static_cast<int16_t>(state_.h[2] >> 1);
  ctx[11] = static_cast<int16_t>(state_.h[3] >> 1);
  ctx[12] = static_cast<int16_t>(prev_qp_ * 4);
  ctx[13] = static_cast<int16_t>((state_.h[0] ^ state_.h[1]) & 0x7F);
  ctx[14] = static_cast<int16_t>((state_.h[2] ^ state_.h[3]) & 0x7F);
  ctx[15] = static_cast<int16_t>((state_.h[0] + state_.h[3]) >> 2);
  return ctx;
}

uint8_t SsgCr4Predictor::map_intra_angle(SourceCodecId codec, uint8_t mode) {
  if (codec == SourceCodecId::CODEC_HEVC) {
    // HEVC modes 0..34: Mode 0 = Planar (45 deg), Mode 1 = DC (45 deg)
    if (mode <= 1) return 45;
    uint32_t deg = static_cast<uint32_t>(mode - 2) * 180 / 32;
    return static_cast<uint8_t>(deg > 180 ? 180 : deg);
  } else if (codec == SourceCodecId::CODEC_AV1) {
    // AV1 directional intra angles
    constexpr uint8_t AV1_ANGLES[8] = {0, 45, 90, 135, 180, 22, 67, 112};
    return AV1_ANGLES[mode & 0x07];
  } else {
    // Default H.264 modes 0..8
    constexpr uint8_t H264_ANGLES[9] = {90, 0, 45, 135, 45, 67, 22, 112, 157};
    return (mode < 9) ? H264_ANGLES[mode] : 45;
  }
}

// CmavqTokenizer Implementation
std::vector<CmavqTokenizer::Token> CmavqTokenizer::tokenize(const uint8_t *data,
                                                            size_t size) {
  std::vector<Token> tokens;
  tokens.reserve(size);

  size_t i = 0;
  while (i < size) {
    // Check for run of zeros
    if (data[i] == 0) {
      size_t run = 0;
      while (i + run < size && data[i + run] == 0 && run < 15) {
        run++;
      }
      if (i + run == size && run > 0) {
        // EOB / terminal run
        Token tok;
        tok.symbol = 0; // EOB
        tok.extra_val = static_cast<int32_t>(run);
        tokens.push_back(tok);
        i += run;
        continue;
      }
      if (run > 0 && i + run < size && data[i + run] != 0) {
        uint8_t next_val = data[i + run];
        if (run <= 3 && next_val <= 8) {
          // Class 2: Dense run-level pair (symbols 16..47)
          Token tok;
          tok.symbol = static_cast<uint8_t>(16 + run * 8 + (next_val - 1));
          tokens.push_back(tok);
          i += run + 1;
          continue;
        } else {
          // Class 1: Zero-run squeeze (symbols 1..15)
          Token tok;
          tok.symbol = static_cast<uint8_t>(run);
          tokens.push_back(tok);
          i += run;
          continue;
        }
      } else if (run > 0) {
        Token tok;
        tok.symbol = static_cast<uint8_t>(run);
        tokens.push_back(tok);
        i += run;
        continue;
      }
    }

    uint8_t val = data[i];
    if (val >= 1 && val <= 8) {
      // Run = 0, level 1..8: symbol 16..23
      Token tok;
      tok.symbol = static_cast<uint8_t>(16 + (val - 1));
      tokens.push_back(tok);
      i++;
    } else if (val >= 9 && val <= 16) {
      // Class 3: Medium-run unit levels (symbols 48..55)
      Token tok;
      tok.symbol = static_cast<uint8_t>(48 + (val - 9));
      tokens.push_back(tok);
      i++;
    } else if (val >= 17 && val <= 22) {
      // Class 4: High-level escape prefixes (symbols 56..61)
      Token tok;
      tok.symbol = static_cast<uint8_t>(56 + (val - 17));
      tokens.push_back(tok);
      i++;
    } else {
      // Symbol 63: Full byte literal via ECB escape
      Token tok;
      tok.symbol = 63;
      tok.extra_val = static_cast<int32_t>(val);
      tokens.push_back(tok);
      i++;
    }
  }

  return tokens;
}

std::vector<uint8_t>
CmavqTokenizer::detokenize(const std::vector<Token> &tokens,
                           size_t original_size) {
  std::vector<uint8_t> out(original_size, 0);
  uint8_t *ptr = out.data();
  uint8_t *const end_ptr = ptr + original_size;

  for (const auto &tok : tokens) {
    if (ptr >= end_ptr)
      break;

    if (tok.symbol == 0) {
      size_t run = tok.extra_val > 0 ? static_cast<size_t>(tok.extra_val) : 1;
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (tok.symbol >= 1 && tok.symbol <= 15) {
      size_t run = tok.symbol;
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (tok.symbol >= 16 && tok.symbol <= 47) {
      uint8_t idx = tok.symbol - 16;
      uint8_t run = idx / 8;
      uint8_t val = (idx % 8) + 1;
      if (run > 0) {
        size_t avail = static_cast<size_t>(end_ptr - ptr);
        size_t write_len = std::min(static_cast<size_t>(run), avail);
        std::memset(ptr, 0, write_len);
        ptr += write_len;
      }
      if (ptr < end_ptr) {
        *ptr++ = val;
      }
    } else if (tok.symbol >= 48 && tok.symbol <= 55) {
      uint8_t val = static_cast<uint8_t>((tok.symbol - 48) + 9);
      *ptr++ = val;
    } else if (tok.symbol >= 56 && tok.symbol <= 61) {
      uint8_t val = static_cast<uint8_t>((tok.symbol - 56) + 17);
      *ptr++ = val;
    } else if (tok.symbol == 62) {
      size_t run = tok.extra_val > 0 ? static_cast<size_t>(tok.extra_val) : 16;
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (tok.symbol == 63) {
      *ptr++ = static_cast<uint8_t>(tok.extra_val & 0xFF);
    }
  }

  size_t written = static_cast<size_t>(ptr - out.data());
  if (written < original_size) {
    out.resize(written);
  }
  return out;
}

std::vector<uint8_t>
CmavqTokenizer::detokenize_stream(const uint16_t *symbols, size_t symbol_count,
                                  const uint8_t *extra_payload,
                                  size_t extra_size, size_t original_size) {
  std::vector<uint8_t> out(original_size, 0);
  uint8_t *ptr = out.data();
  uint8_t *const end_ptr = ptr + original_size;
  size_t extra_idx = 0;

  for (size_t i = 0; i < symbol_count; ++i) {
    if (ptr >= end_ptr)
      break;
    uint8_t sym = static_cast<uint8_t>(symbols[i]);

    if (sym == 0) {
      size_t run = 1;
      if (extra_idx + 2 <= extra_size) {
        uint16_t ev =
            static_cast<uint16_t>(extra_payload[extra_idx]) |
            (static_cast<uint16_t>(extra_payload[extra_idx + 1]) << 8);
        extra_idx += 2;
        if (ev > 0)
          run = ev;
      }
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (sym >= 1 && sym <= 15) {
      size_t run = sym;
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (sym >= 16 && sym <= 47) {
      uint8_t idx = sym - 16;
      uint8_t run = idx / 8;
      uint8_t val = (idx % 8) + 1;
      if (run > 0) {
        size_t avail = static_cast<size_t>(end_ptr - ptr);
        size_t write_len = std::min(static_cast<size_t>(run), avail);
        std::memset(ptr, 0, write_len);
        ptr += write_len;
      }
      if (ptr < end_ptr) {
        *ptr++ = val;
      }
    } else if (sym >= 48 && sym <= 55) {
      uint8_t val = static_cast<uint8_t>((sym - 48) + 9);
      *ptr++ = val;
    } else if (sym >= 56 && sym <= 61) {
      uint8_t val = static_cast<uint8_t>((sym - 56) + 17);
      *ptr++ = val;
    } else if (sym == 62) {
      size_t run = 16;
      if (extra_idx + 2 <= extra_size) {
        uint16_t ev =
            static_cast<uint16_t>(extra_payload[extra_idx]) |
            (static_cast<uint16_t>(extra_payload[extra_idx + 1]) << 8);
        extra_idx += 2;
        if (ev > 0)
          run = ev;
      }
      size_t avail = static_cast<size_t>(end_ptr - ptr);
      size_t write_len = std::min(run, avail);
      std::memset(ptr, 0, write_len);
      ptr += write_len;
    } else if (sym == 63) {
      uint8_t val = 0;
      if (extra_idx + 2 <= extra_size) {
        val = extra_payload[extra_idx];
        extra_idx += 2;
      }
      *ptr++ = val;
    }
  }

  size_t written = static_cast<size_t>(ptr - out.data());
  if (written < original_size) {
    out.resize(written);
  }
  return out;
}

std::vector<RansSymbol>
CmavqTokenizer::compute_alphabet_stats(const std::vector<Token> &tokens) {
  std::array<uint32_t, ALPHABET_SIZE> counts{};
  counts.fill(1); // Laplace add-1 smoothing

  for (const auto &tok : tokens) {
    uint8_t s = tok.symbol;
    if (s < ALPHABET_SIZE) {
      counts[s]++;
    }
  }

  uint32_t total_count = 0;
  for (size_t s = 0; s < ALPHABET_SIZE; ++s) {
    total_count += counts[s];
  }

  constexpr uint32_t MIN_FREQ = 8;
  constexpr uint32_t TOTAL_MASS = Rans16Codec::RANS_M;         // 32768
  constexpr uint32_t RESERVED_MASS = MIN_FREQ * ALPHABET_SIZE; // 512
  constexpr uint32_t USABLE_MASS = TOTAL_MASS - RESERVED_MASS;

  std::vector<RansSymbol> stats(ALPHABET_SIZE);
  uint16_t running_start = 0;

  for (size_t s = 0; s < ALPHABET_SIZE; ++s) {
    uint32_t scaled =
        MIN_FREQ +
        static_cast<uint32_t>((static_cast<uint64_t>(counts[s]) * USABLE_MASS) /
                              total_count);

    if (s == ALPHABET_SIZE - 1) {
      scaled = TOTAL_MASS - running_start;
    }

    stats[s].start = running_start;
    stats[s].freq = static_cast<uint16_t>(scaled);
    running_start += static_cast<uint16_t>(scaled);
  }

  return stats;
}

// Bbr2Engine Implementation
Bbr2Engine::Bbr2Engine() = default;

Sha256Digest Bbr2Engine::compute_sha256(const uint8_t *data, size_t size) {
  return BbrEngine::compute_sha256(data, size);
}

Bbr2Chunk Bbr2Engine::compress(const uint8_t *bitstream_data, size_t size,
                               SourceCodecId codec_id) {
  (void)codec_id;
  Bbr2Chunk chunk;
  chunk.uncompressed_size = static_cast<uint32_t>(size);
  chunk.orig_sha256 = compute_sha256(bitstream_data, size);

  if (size == 0) {
    return chunk;
  }

  // Step 1: Tokenize into 64-symbol Vector Context Tokens (VCTs)
  std::vector<CmavqTokenizer::Token> tokens =
      CmavqTokenizer::tokenize(bitstream_data, size);

  // Step 2: Extract symbols and extra ECB payload
  std::vector<uint16_t> symbols;
  symbols.reserve(tokens.size());
  std::vector<uint8_t> extra_bytes;

  ssg_.reset();
  const uint8_t blk_size = (codec_id == SourceCodecId::CODEC_HEVC) ? 32 : 16;
  const uint16_t blk_area = static_cast<uint16_t>(blk_size * blk_size);
  for (size_t t = 0; t < tokens.size(); ++t) {
    const auto &tok = tokens[t];
    symbols.push_back(tok.symbol);

    // Step SSG recurrence with codec-aware intra-prediction angle
    uint8_t intra_angle = SsgCr4Predictor::map_intra_angle(codec_id, static_cast<uint8_t>(t % 9));
    ssg_.step(
        intra_angle, (t % blk_size == 0), (t % (blk_size * 4) == 0),
        static_cast<int16_t>(tok.symbol),
        static_cast<uint8_t>(tok.symbol % 8),
        static_cast<uint8_t>(t % blk_area), 26);

    if (tok.symbol == 0 || tok.symbol == 62 || tok.symbol == 63) {
      // Write 16-bit extra value
      uint16_t ev = static_cast<uint16_t>(tok.extra_val & 0xFFFF);
      extra_bytes.push_back(static_cast<uint8_t>(ev & 0xFF));
      extra_bytes.push_back(static_cast<uint8_t>((ev >> 8) & 0xFF));
    }
  }

  // Step 3: Compute optimal 64-symbol statistics
  std::vector<RansSymbol> symbol_stats =
      CmavqTokenizer::compute_alphabet_stats(tokens);

  // Step 4: Encode via 16-way SIMD interleaved rANS
  Rans16Codec::EncodedChunk rchunk = Rans16Codec::encode(symbols, symbol_stats);
  std::vector<uint8_t> serialized = Rans16Codec::serialize(rchunk);

  // Header storing the 64-symbol frequencies (each uint16_t = 128 bytes)
  std::vector<uint8_t> final_rans_payload;
  final_rans_payload.resize(CmavqTokenizer::ALPHABET_SIZE * sizeof(uint16_t) +
                            serialized.size());
  uint8_t *fptr = final_rans_payload.data();

  for (size_t s = 0; s < CmavqTokenizer::ALPHABET_SIZE; ++s) {
    uint16_t freq = symbol_stats[s].freq;
    std::memcpy(fptr, &freq, sizeof(uint16_t));
    fptr += sizeof(uint16_t);
  }
  std::memcpy(fptr, serialized.data(), serialized.size());

  size_t total_compressed = final_rans_payload.size() + extra_bytes.size();

  if (total_compressed < size) {
    chunk.rans_payload = std::move(final_rans_payload);
    chunk.extra_payload = std::move(extra_bytes);
  } else {
    // Zero-expansion invariant: uncompressed bypass
    chunk.rans_payload.assign(bitstream_data, bitstream_data + size);
    chunk.extra_payload.clear();
  }

  chunk.delta_patches.clear();
  return chunk;
}

std::vector<uint8_t> Bbr2Engine::decompress(const Bbr2Chunk &chunk) {
  if (chunk.uncompressed_size == 0) {
    return {};
  }

  if (chunk.rans_payload.size() == chunk.uncompressed_size &&
      chunk.extra_payload.empty()) {
    // Uncompressed bypass chunk
    return chunk.rans_payload;
  }

  constexpr size_t FREQ_TABLE_SIZE =
      CmavqTokenizer::ALPHABET_SIZE * sizeof(uint16_t);
  if (chunk.rans_payload.size() < FREQ_TABLE_SIZE) {
    throw std::runtime_error(
        "Malformed BBR2 chunk: rans payload too short for stats header");
  }

  // Unpack symbol statistics
  const uint8_t *ptr = chunk.rans_payload.data();
  std::vector<RansSymbol> symbol_stats(CmavqTokenizer::ALPHABET_SIZE);
  uint16_t running_start = 0;

  for (size_t s = 0; s < CmavqTokenizer::ALPHABET_SIZE; ++s) {
    uint16_t freq = 0;
    std::memcpy(&freq, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    symbol_stats[s].start = running_start;
    symbol_stats[s].freq = freq;
    running_start += freq;
  }

  // Deserialize 16-way rANS chunk
  size_t serialized_size = chunk.rans_payload.size() - FREQ_TABLE_SIZE;
  Rans16Codec::EncodedChunk rchunk =
      Rans16Codec::deserialize(ptr, serialized_size);

  std::vector<uint16_t> cdf_lut = Rans16Codec::build_cdf_lut(symbol_stats);
  std::vector<uint16_t> symbols =
      Rans16Codec::decode(rchunk, cdf_lut, symbol_stats);

  // Direct zero-allocation detokenization into destination buffer
  std::vector<uint8_t> reconstructed = CmavqTokenizer::detokenize_stream(
      symbols.data(), symbols.size(), chunk.extra_payload.data(),
      chunk.extra_payload.size(), chunk.uncompressed_size);

  // Apply any discrepancy patches
  for (const auto &patch : chunk.delta_patches) {
    if (patch.byte_offset + patch.patch_bytes.size() <= reconstructed.size()) {
      std::memcpy(reconstructed.data() + patch.byte_offset,
                  patch.patch_bytes.data(), patch.patch_bytes.size());
    }
  }

  // Verify SHA-256 bit-exact contract
  static const Sha256Digest ZERO_SHA{};
  if (chunk.orig_sha256 != ZERO_SHA) {
    Sha256Digest recon_sha =
        compute_sha256(reconstructed.data(), reconstructed.size());
    if (recon_sha != chunk.orig_sha256) {
      throw std::runtime_error(
          "BBR2 Decompression SHA-256 mismatch! Bit-exact contract violated.");
    }
  }

  return reconstructed;
}

std::vector<uint8_t>
Bbr2Engine::serialize_chunk_payload(const Bbr2Chunk &chunk) {
  std::vector<uint8_t> payload;

  // Check if bypass chunk
  if (chunk.extra_payload.empty() &&
      chunk.rans_payload.size() == chunk.uncompressed_size) {
    uint32_t bypass_flag = 0xFFFFFFFFu;
    payload.resize(sizeof(uint32_t) + chunk.rans_payload.size());
    std::memcpy(payload.data(), &bypass_flag, sizeof(uint32_t));
    std::memcpy(payload.data() + sizeof(uint32_t), chunk.rans_payload.data(),
                chunk.rans_payload.size());
    return payload;
  }

  uint32_t extra_len = static_cast<uint32_t>(chunk.extra_payload.size());
  payload.resize(sizeof(uint32_t) + extra_len + chunk.rans_payload.size());

  uint8_t *ptr = payload.data();
  std::memcpy(ptr, &extra_len, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  if (extra_len > 0) {
    std::memcpy(ptr, chunk.extra_payload.data(), extra_len);
    ptr += extra_len;
  }

  if (!chunk.rans_payload.empty()) {
    std::memcpy(ptr, chunk.rans_payload.data(), chunk.rans_payload.size());
  }

  return payload;
}

Bbr2Chunk
Bbr2Engine::deserialize_chunk_payload(const uint8_t *data, size_t size,
                                      uint32_t uncompressed_size,
                                      const Sha256Digest &expected_sha) {
  if (size < sizeof(uint32_t)) {
    throw std::runtime_error(
        "Malformed BBR2 serialized payload: too short for extra_len header");
  }

  Bbr2Chunk chunk;
  chunk.uncompressed_size = uncompressed_size;
  chunk.orig_sha256 = expected_sha;

  uint32_t extra_len = 0;
  std::memcpy(&extra_len, data, sizeof(uint32_t));

  if (extra_len == 0xFFFFFFFFu) {
    // Bypass chunk
    chunk.extra_payload.clear();
    chunk.rans_payload.assign(data + sizeof(uint32_t), data + size);
    return chunk;
  }

  if (size < sizeof(uint32_t) + extra_len) {
    throw std::runtime_error(
        "Malformed BBR2 serialized payload: truncated extra_payload");
  }

  const uint8_t *ptr = data + sizeof(uint32_t);
  if (extra_len > 0) {
    chunk.extra_payload.assign(ptr, ptr + extra_len);
    ptr += extra_len;
  }

  size_t rans_size = size - sizeof(uint32_t) - extra_len;
  if (rans_size > 0) {
    chunk.rans_payload.assign(ptr, ptr + rans_size);
  }

  return chunk;
}

std::vector<uint8_t>
Bbr2Engine::decompress_payload(const uint8_t *payload, size_t payload_size,
                               uint32_t uncompressed_size,
                               const Sha256Digest &expected_sha) {
  Bbr2Chunk chunk = deserialize_chunk_payload(payload, payload_size,
                                              uncompressed_size, expected_sha);
  return decompress(chunk);
}

} // namespace prism
