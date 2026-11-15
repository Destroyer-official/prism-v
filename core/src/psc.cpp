#include "prism/psc.hpp"
#include "prism/bbr.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace prism {

namespace {

void validate_rgb_geometry(uint32_t width, uint32_t height, uint8_t bit_depth) {
    if (bit_depth != 8) {
        throw std::invalid_argument("PSC RGB path supports 8-bit input only");
    }
    if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
        throw std::invalid_argument("PSC requires even dimensions of at least 2x2");
    }
    if (width > std::numeric_limits<size_t>::max() / height ||
        static_cast<size_t>(width) * height > std::numeric_limits<size_t>::max() / 3) {
        throw std::length_error("PSC frame dimensions overflow the addressable buffer size");
    }
}

void validate_bayer_geometry(uint32_t width, uint32_t height, uint8_t bit_depth) {
    if (bit_depth < 1 || bit_depth > 16) {
        throw std::invalid_argument("Bayer CFA bit depth must be between 1 and 16");
    }
    if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
        throw std::invalid_argument("Bayer CFA requires even dimensions of at least 2x2");
    }
    if (width > std::numeric_limits<size_t>::max() / height ||
        static_cast<size_t>(width) * height > std::numeric_limits<size_t>::max() / sizeof(uint16_t)) {
        throw std::length_error("Bayer CFA dimensions overflow the addressable buffer size");
    }
}

} // namespace

PscEngine::PscEngine() = default;

YCoCgPixel PscEngine::rgb_to_ycocgr(RgbPixel rgb) {
    int16_t r = static_cast<int16_t>(rgb.r);
    int16_t g = static_cast<int16_t>(rgb.g);
    int16_t b = static_cast<int16_t>(rgb.b);

    int16_t co = r - b;
    int16_t t = b + (co >> 1);
    int16_t cg = g - t;
    int16_t y = t + (cg >> 1);

    return {y, co, cg};
}

RgbPixel PscEngine::ycocgr_to_rgb(YCoCgPixel ycocg) {
    int16_t t = ycocg.y - (ycocg.cg >> 1);
    int16_t g = ycocg.cg + t;
    int16_t b = t - (ycocg.co >> 1);
    int16_t r = ycocg.co + b;

    auto clamp_u8 = [](int16_t val) -> uint8_t {
        if (val < 0) return 0;
        if (val > 255) return 255;
        return static_cast<uint8_t>(val);
    };

    return {clamp_u8(r), clamp_u8(g), clamp_u8(b)};
}

void PscEngine::forward_legall53_1d(const int16_t* in, int16_t* low, int16_t* high, size_t n) {
    if (n < 2) {
        if (n == 1) {
            low[0] = in[0];
            high[0] = 0;
        }
        return;
    }

    size_t m = n / 2;

    // Step 1: Predict (High frequency subband)
    for (size_t i = 0; i < m; ++i) {
        int16_t left = in[2 * i];
        int16_t right = (2 * i + 2 < n) ? in[2 * i + 2] : in[2 * i]; // symmetric boundary extension
        high[i] = in[2 * i + 1] - ((left + right) >> 1);
    }

    // Step 2: Update (Low frequency subband)
    for (size_t i = 0; i < m; ++i) {
        int16_t h_prev = (i > 0) ? high[i - 1] : high[0];
        int16_t h_curr = high[i];
        low[i] = in[2 * i] + ((h_prev + h_curr + 2) >> 2);
    }
}

void PscEngine::inverse_legall53_1d(const int16_t* low, const int16_t* high, int16_t* out, size_t n) {
    if (n < 2) {
        if (n == 1) out[0] = low[0];
        return;
    }

    size_t m = n / 2;

    // Step 1: Invert Update to recover even samples
    for (size_t i = 0; i < m; ++i) {
        int16_t h_prev = (i > 0) ? high[i - 1] : high[0];
        int16_t h_curr = high[i];
        out[2 * i] = low[i] - ((h_prev + h_curr + 2) >> 2);
    }

    // Step 2: Invert Predict to recover odd samples
    for (size_t i = 0; i < m; ++i) {
        int16_t left = out[2 * i];
        int16_t right = (2 * i + 2 < n) ? out[2 * i + 2] : out[2 * i];
        out[2 * i + 1] = high[i] + ((left + right) >> 1);
    }
}

PscEngine::WaveletSubbands PscEngine::forward_wavelet_2d(const int16_t* plane, uint32_t width, uint32_t height) {
    if (plane == nullptr) throw std::invalid_argument("Wavelet input plane is null");
    if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
        throw std::invalid_argument("Wavelet transform requires even dimensions of at least 2x2");
    }

    uint32_t sub_w = width / 2;
    uint32_t sub_h = height / 2;

    WaveletSubbands bands;
    bands.sub_w = sub_w;
    bands.sub_h = sub_h;
    bands.ll.resize(sub_w * sub_h);
    bands.lh.resize(sub_w * sub_h);
    bands.hl.resize(sub_w * sub_h);
    bands.hh.resize(sub_w * sub_h);

    std::vector<int16_t> row_low(sub_w * height);
    std::vector<int16_t> row_high(sub_w * height);

    // Row-wise transform
    for (uint32_t y = 0; y < height; ++y) {
        forward_legall53_1d(plane + (y * width),
                            row_low.data() + (y * sub_w),
                            row_high.data() + (y * sub_w),
                            width);
    }

    // Column-wise transform on row outputs
    std::vector<int16_t> col_in(height);
    std::vector<int16_t> col_low(sub_h);
    std::vector<int16_t> col_high(sub_h);

    // Low subband columns -> LL and LH
    for (uint32_t x = 0; x < sub_w; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            col_in[y] = row_low[y * sub_w + x];
        }
        forward_legall53_1d(col_in.data(), col_low.data(), col_high.data(), height);
        for (uint32_t y = 0; y < sub_h; ++y) {
            bands.ll[y * sub_w + x] = col_low[y];
            bands.lh[y * sub_w + x] = col_high[y];
        }
    }

    // High subband columns -> HL and HH
    for (uint32_t x = 0; x < sub_w; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            col_in[y] = row_high[y * sub_w + x];
        }
        forward_legall53_1d(col_in.data(), col_low.data(), col_high.data(), height);
        for (uint32_t y = 0; y < sub_h; ++y) {
            bands.hl[y * sub_w + x] = col_low[y];
            bands.hh[y * sub_w + x] = col_high[y];
        }
    }

    return bands;
}

void PscEngine::inverse_wavelet_2d(const WaveletSubbands& bands, int16_t* plane, uint32_t width, uint32_t height) {
    if (plane == nullptr) throw std::invalid_argument("Wavelet output plane is null");
    if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
        throw std::invalid_argument("Wavelet transform requires even dimensions of at least 2x2");
    }

    uint32_t sub_w = bands.sub_w;
    uint32_t sub_h = bands.sub_h;

    const size_t expected = static_cast<size_t>(sub_w) * sub_h;
    if (sub_w != width / 2 || sub_h != height / 2 ||
        bands.ll.size() != expected || bands.lh.size() != expected ||
        bands.hl.size() != expected || bands.hh.size() != expected) {
        throw std::invalid_argument("Wavelet subbands do not match the requested geometry");
    }

    std::vector<int16_t> row_low(sub_w * height);
    std::vector<int16_t> row_high(sub_w * height);

    std::vector<int16_t> col_out(height);
    std::vector<int16_t> col_low(sub_h);
    std::vector<int16_t> col_high(sub_h);

    // Invert column-wise for row_low (from LL and LH)
    for (uint32_t x = 0; x < sub_w; ++x) {
        for (uint32_t y = 0; y < sub_h; ++y) {
            col_low[y] = bands.ll[y * sub_w + x];
            col_high[y] = bands.lh[y * sub_w + x];
        }
        inverse_legall53_1d(col_low.data(), col_high.data(), col_out.data(), height);
        for (uint32_t y = 0; y < height; ++y) {
            row_low[y * sub_w + x] = col_out[y];
        }
    }

    // Invert column-wise for row_high (from HL and HH)
    for (uint32_t x = 0; x < sub_w; ++x) {
        for (uint32_t y = 0; y < sub_h; ++y) {
            col_low[y] = bands.hl[y * sub_w + x];
            col_high[y] = bands.hh[y * sub_w + x];
        }
        inverse_legall53_1d(col_low.data(), col_high.data(), col_out.data(), height);
        for (uint32_t y = 0; y < height; ++y) {
            row_high[y * sub_w + x] = col_out[y];
        }
    }

    // Invert row-wise to recover original plane
    for (uint32_t y = 0; y < height; ++y) {
        inverse_legall53_1d(row_low.data() + (y * sub_w),
                            row_high.data() + (y * sub_w),
                            plane + (y * width),
                            width);
    }
}

int16_t PscEngine::med_predict(int16_t left, int16_t top, int16_t topleft) {
    int16_t min_val = std::min(left, top);
    int16_t max_val = std::max(left, top);

    if (topleft >= max_val) {
        return min_val;
    } else if (topleft <= min_val) {
        return max_val;
    } else {
        return static_cast<int16_t>(left + top - topleft);
    }
}

PscEngine::CompressedPscFrame PscEngine::compress_frame(const uint8_t* raw_rgb_or_yuv, const FrameGeometry& geom) {
    if (raw_rgb_or_yuv == nullptr) throw std::invalid_argument("PSC input frame is null");
    validate_rgb_geometry(geom.width, geom.height, geom.bit_depth);

    CompressedPscFrame frame;
    frame.geometry = geom;

    size_t total_bytes = geom.width * geom.height * 3;
    frame.original_pixel_sha256 = BbrEngine::compute_sha256(raw_rgb_or_yuv, total_bytes);

    // Convert RGB to YCoCg-R planes
    std::vector<int16_t> y_plane(geom.width * geom.height);
    std::vector<int16_t> co_plane(geom.width * geom.height);
    std::vector<int16_t> cg_plane(geom.width * geom.height);

    for (size_t i = 0; i < geom.width * geom.height; ++i) {
        RgbPixel rgb{raw_rgb_or_yuv[i * 3 + 0], raw_rgb_or_yuv[i * 3 + 1], raw_rgb_or_yuv[i * 3 + 2]};
        YCoCgPixel ycocg = rgb_to_ycocgr(rgb);
        y_plane[i] = ycocg.y;
        co_plane[i] = ycocg.co;
        cg_plane[i] = ycocg.cg;
    }

    // Forward 2D LeGall 5/3 Wavelet on Y plane
    WaveletSubbands y_bands = forward_wavelet_2d(y_plane.data(), geom.width, geom.height);

    // Encode subband residuals with MED prediction and rANS.
    std::vector<uint16_t> symbols;
    symbols.reserve(geom.width * geom.height * 6);

    auto encode_band = [&](const std::vector<int16_t>& band, uint32_t w, uint32_t h) {
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                int16_t left = (x > 0) ? band[y * w + (x - 1)] : 0;
                int16_t top = (y > 0) ? band[(y - 1) * w + x] : 0;
                int16_t topleft = (x > 0 && y > 0) ? band[(y - 1) * w + (x - 1)] : 0;
                int16_t pred = med_predict(left, top, topleft);
                int16_t diff = band[y * w + x] - pred;

                // Map signed difference to non-negative symbol
                uint16_t uval = (diff >= 0) ? static_cast<uint16_t>(diff * 2)
                                            : static_cast<uint16_t>(-diff * 2 - 1);
                // Split into two 8-bit bytes
                symbols.push_back(uval & 0xFF);
                symbols.push_back((uval >> 8) & 0xFF);
            }
        }
    };

    encode_band(y_bands.ll, y_bands.sub_w, y_bands.sub_h);
    encode_band(y_bands.lh, y_bands.sub_w, y_bands.sub_h);
    encode_band(y_bands.hl, y_bands.sub_w, y_bands.sub_h);
    encode_band(y_bands.hh, y_bands.sub_w, y_bands.sub_h);

    // Also encode Chroma planes Co and Cg directly via MED
    auto encode_plane = [&](const std::vector<int16_t>& p, uint32_t w, uint32_t h) {
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                int16_t left = (x > 0) ? p[y * w + (x - 1)] : 0;
                int16_t top = (y > 0) ? p[(y - 1) * w + x] : 0;
                int16_t topleft = (x > 0 && y > 0) ? p[(y - 1) * w + (x - 1)] : 0;
                int16_t pred = med_predict(left, top, topleft);
                int16_t diff = p[y * w + x] - pred;

                uint16_t uval = (diff >= 0) ? static_cast<uint16_t>(diff * 2)
                                            : static_cast<uint16_t>(-diff * 2 - 1);
                symbols.push_back(uval & 0xFF);
                symbols.push_back((uval >> 8) & 0xFF);
            }
        }
    };

    encode_plane(co_plane, geom.width, geom.height);
    encode_plane(cg_plane, geom.width, geom.height);

    // Uniform / Laplace symbol stats for 256 symbols
    std::vector<RansSymbol> symbol_stats(256);
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
        symbol_stats[s].start = running;
        symbol_stats[s].freq = 128; // 256 * 128 = 32768
        running += 128;
    }

    Rans8Codec::EncodedChunk rchunk = Rans8Codec::encode(symbols, symbol_stats);
    frame.rans_payload = Rans8Codec::serialize(rchunk);

    return frame;
}

std::vector<uint8_t> PscEngine::decompress_frame(const CompressedPscFrame& frame) {
    uint32_t w = frame.geometry.width;
    uint32_t h = frame.geometry.height;
    validate_rgb_geometry(w, h, frame.geometry.bit_depth);

    if (frame.rans_payload.empty()) {
        throw std::runtime_error("Compressed PSC frame has no payload");
    }

    Rans8Codec::EncodedChunk rchunk = Rans8Codec::deserialize(frame.rans_payload.data(),
                                                             frame.rans_payload.size());

    std::vector<RansSymbol> symbol_stats(256);
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
        symbol_stats[s].start = running;
        symbol_stats[s].freq = 128;
        running += 128;
    }
    std::vector<uint16_t> cdf_table = Rans8Codec::build_cdf_lut(symbol_stats);
    std::vector<uint16_t> symbols = Rans8Codec::decode(rchunk, cdf_table, symbol_stats);

    const size_t expected_symbols =
        (static_cast<size_t>(w / 2) * (h / 2) * 4 + static_cast<size_t>(w) * h * 2) * 2;
    if (symbols.size() != expected_symbols) {
        throw std::runtime_error("PSC payload symbol count does not match the declared geometry");
    }

    size_t sym_idx = 0;
    auto decode_uval = [&]() -> uint16_t {
        if (sym_idx + 1 >= symbols.size()) return 0;
        uint16_t low = symbols[sym_idx++];
        uint16_t high = symbols[sym_idx++];
        return (high << 8) | low;
    };

    uint32_t sub_w = w / 2;
    uint32_t sub_h = h / 2;
    WaveletSubbands y_bands;
    y_bands.sub_w = sub_w;
    y_bands.sub_h = sub_h;
    y_bands.ll.resize(sub_w * sub_h);
    y_bands.lh.resize(sub_w * sub_h);
    y_bands.hl.resize(sub_w * sub_h);
    y_bands.hh.resize(sub_w * sub_h);

    auto decode_band = [&](std::vector<int16_t>& band, uint32_t bw, uint32_t bh) {
        for (uint32_t y = 0; y < bh; ++y) {
            for (uint32_t x = 0; x < bw; ++x) {
                int16_t left = (x > 0) ? band[y * bw + (x - 1)] : 0;
                int16_t top = (y > 0) ? band[(y - 1) * bw + x] : 0;
                int16_t topleft = (x > 0 && y > 0) ? band[(y - 1) * bw + (x - 1)] : 0;
                int16_t pred = med_predict(left, top, topleft);

                uint16_t uval = decode_uval();
                int16_t diff = (uval % 2 == 0) ? static_cast<int16_t>(uval / 2)
                                               : -static_cast<int16_t>((uval + 1) / 2);
                band[y * bw + x] = pred + diff;
            }
        }
    };

    decode_band(y_bands.ll, sub_w, sub_h);
    decode_band(y_bands.lh, sub_w, sub_h);
    decode_band(y_bands.hl, sub_w, sub_h);
    decode_band(y_bands.hh, sub_w, sub_h);

    // Invert 2D Wavelet on Y
    std::vector<int16_t> y_plane(w * h);
    inverse_wavelet_2d(y_bands, y_plane.data(), w, h);

    // Decode Chroma Co and Cg planes
    std::vector<int16_t> co_plane(w * h);
    std::vector<int16_t> cg_plane(w * h);

    auto decode_plane = [&](std::vector<int16_t>& p, uint32_t pw, uint32_t ph) {
        for (uint32_t y = 0; y < ph; ++y) {
            for (uint32_t x = 0; x < pw; ++x) {
                int16_t left = (x > 0) ? p[y * pw + (x - 1)] : 0;
                int16_t top = (y > 0) ? p[(y - 1) * pw + x] : 0;
                int16_t topleft = (x > 0 && y > 0) ? p[(y - 1) * pw + (x - 1)] : 0;
                int16_t pred = med_predict(left, top, topleft);

                uint16_t uval = decode_uval();
                int16_t diff = (uval % 2 == 0) ? static_cast<int16_t>(uval / 2)
                                               : -static_cast<int16_t>((uval + 1) / 2);
                p[y * pw + x] = pred + diff;
            }
        }
    };

    decode_plane(co_plane, w, h);
    decode_plane(cg_plane, w, h);

    // Invert YCoCg-R to RGB
    std::vector<uint8_t> rgb_out(w * h * 3);
    for (size_t i = 0; i < w * h; ++i) {
        YCoCgPixel ycocg{y_plane[i], co_plane[i], cg_plane[i]};
        RgbPixel rgb = ycocgr_to_rgb(ycocg);
        rgb_out[i * 3 + 0] = rgb.r;
        rgb_out[i * 3 + 1] = rgb.g;
        rgb_out[i * 3 + 2] = rgb.b;
    }

    // Verify cryptographic SHA-256
    Sha256Digest recon_sha = BbrEngine::compute_sha256(rgb_out.data(), rgb_out.size());
    if (recon_sha != frame.original_pixel_sha256) {
        throw std::runtime_error("PSC Decompression SHA-256 parity mismatch! Bit-exact PSNR=inf violated.");
    }

    return rgb_out;
}

bool PscEngine::verify_sample_parity(const uint8_t* original, const uint8_t* reconstructed, size_t size) {
    if (!original || !reconstructed) return false;
    return std::memcmp(original, reconstructed, size) == 0;
}

PscEngine::BayerCfaSubplanes PscEngine::splice_bayer_cfa(const uint16_t* bayer_raw,
                                                        uint32_t width,
                                                        uint32_t height,
                                                        uint8_t bit_depth) {
    if (bayer_raw == nullptr) throw std::invalid_argument("Bayer input frame is null");
    validate_bayer_geometry(width, height, bit_depth);

    BayerCfaSubplanes planes;
    planes.sub_w = width / 2;
    planes.sub_h = height / 2;
    planes.bit_depth = bit_depth;

    size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;
    planes.r.resize(count);
    planes.g1.resize(count);
    planes.g2.resize(count);
    planes.b.resize(count);

    for (uint32_t y = 0; y < planes.sub_h; ++y) {
        for (uint32_t x = 0; x < planes.sub_w; ++x) {
            size_t sub_idx = static_cast<size_t>(y) * planes.sub_w + x;
            size_t r0 = (static_cast<size_t>(2 * y)) * width;
            size_t r1 = (static_cast<size_t>(2 * y + 1)) * width;

            planes.r[sub_idx]  = bayer_raw[r0 + 2 * x];
            planes.g1[sub_idx] = bayer_raw[r0 + 2 * x + 1];
            planes.g2[sub_idx] = bayer_raw[r1 + 2 * x];
            planes.b[sub_idx]  = bayer_raw[r1 + 2 * x + 1];
        }
    }
    return planes;
}

void PscEngine::merge_bayer_cfa(const BayerCfaSubplanes& planes,
                                uint16_t* out_bayer_raw,
                                uint32_t width,
                                uint32_t height) {
    if (out_bayer_raw == nullptr) throw std::invalid_argument("Bayer output frame is null");
    validate_bayer_geometry(width, height, planes.bit_depth);
    if (planes.sub_w != width / 2 || planes.sub_h != height / 2) {
        throw std::invalid_argument("Bayer subplane geometry does not match the output frame");
    }
    const size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;
    if (planes.r.size() != count || planes.g1.size() != count ||
        planes.g2.size() != count || planes.b.size() != count) {
        throw std::invalid_argument("Bayer subplane storage does not match its geometry");
    }
    for (uint32_t y = 0; y < planes.sub_h; ++y) {
        for (uint32_t x = 0; x < planes.sub_w; ++x) {
            size_t sub_idx = static_cast<size_t>(y) * planes.sub_w + x;
            size_t r0 = (static_cast<size_t>(2 * y)) * width;
            size_t r1 = (static_cast<size_t>(2 * y + 1)) * width;

            out_bayer_raw[r0 + 2 * x]         = planes.r[sub_idx];
            out_bayer_raw[r0 + 2 * x + 1]     = planes.g1[sub_idx];
            out_bayer_raw[r1 + 2 * x]         = planes.g2[sub_idx];
            out_bayer_raw[r1 + 2 * x + 1]     = planes.b[sub_idx];
        }
    }
}

PscEngine::BayerCfaSubplanes PscEngine::compute_bayer_temporal_delta(const BayerCfaSubplanes& curr,
                                                                    const BayerCfaSubplanes& ref) {
    validate_bayer_geometry(curr.sub_w * 2, curr.sub_h * 2, curr.bit_depth);
    size_t count = static_cast<size_t>(curr.sub_w) * curr.sub_h;
    if (ref.sub_w != curr.sub_w || ref.sub_h != curr.sub_h || ref.bit_depth != curr.bit_depth ||
        ref.r.size() != count || ref.g1.size() != count || ref.g2.size() != count || ref.b.size() != count) {
        throw std::invalid_argument("Bayer reference subplanes do not match the current frame");
    }
    BayerCfaSubplanes delta = curr;
    uint32_t mask = (1u << curr.bit_depth) - 1u;

    for (size_t i = 0; i < count; ++i) {
        delta.r[i]  = static_cast<uint16_t>((curr.r[i]  - ref.r[i])  & mask);
        delta.g1[i] = static_cast<uint16_t>((curr.g1[i] - ref.g1[i]) & mask);
        delta.g2[i] = static_cast<uint16_t>((curr.g2[i] - ref.g2[i]) & mask);
        delta.b[i]  = static_cast<uint16_t>((curr.b[i]  - ref.b[i])  & mask);
    }
    return delta;
}

PscEngine::BayerCfaSubplanes PscEngine::apply_bayer_temporal_delta(const BayerCfaSubplanes& delta,
                                                                  const BayerCfaSubplanes& ref) {
    validate_bayer_geometry(delta.sub_w * 2, delta.sub_h * 2, delta.bit_depth);
    size_t count = static_cast<size_t>(delta.sub_w) * delta.sub_h;
    if (ref.sub_w != delta.sub_w || ref.sub_h != delta.sub_h || ref.bit_depth != delta.bit_depth ||
        ref.r.size() != count || ref.g1.size() != count || ref.g2.size() != count || ref.b.size() != count) {
        throw std::invalid_argument("Bayer reference subplanes do not match the delta frame");
    }
    BayerCfaSubplanes restored = delta;
    uint32_t mask = (1u << delta.bit_depth) - 1u;

    for (size_t i = 0; i < count; ++i) {
        restored.r[i]  = static_cast<uint16_t>((delta.r[i]  + ref.r[i])  & mask);
        restored.g1[i] = static_cast<uint16_t>((delta.g1[i] + ref.g1[i]) & mask);
        restored.g2[i] = static_cast<uint16_t>((delta.g2[i] + ref.g2[i]) & mask);
        restored.b[i]  = static_cast<uint16_t>((delta.b[i]  + ref.b[i])  & mask);
    }
    return restored;
}

PscEngine::CompressedBayerFrame PscEngine::compress_bayer_frame(const uint16_t* bayer_raw,
                                                                uint32_t width,
                                                                uint32_t height,
                                                                uint8_t bit_depth,
                                                                const BayerCfaSubplanes* ref_planes) {
    if (bayer_raw == nullptr) throw std::invalid_argument("Bayer input frame is null");
    validate_bayer_geometry(width, height, bit_depth);

    CompressedBayerFrame frame;
    frame.width = width;
    frame.height = height;
    frame.bit_depth = bit_depth;
    frame.is_delta = (ref_planes != nullptr);

    size_t raw_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
    frame.original_bayer_sha256 = BbrEngine::compute_sha256(reinterpret_cast<const uint8_t*>(bayer_raw), raw_bytes);

    BayerCfaSubplanes planes = splice_bayer_cfa(bayer_raw, width, height, bit_depth);
    if (ref_planes) {
        planes = compute_bayer_temporal_delta(planes, *ref_planes);
    }

    std::vector<uint16_t> symbols;
    symbols.reserve(static_cast<size_t>(width) * height * 2);

    auto encode_subplane = [&](const std::vector<uint16_t>& p, uint32_t sw, uint32_t sh) {
        for (uint32_t y = 0; y < sh; ++y) {
            for (uint32_t x = 0; x < sw; ++x) {
                int16_t left = (x > 0) ? static_cast<int16_t>(p[y * sw + (x - 1)]) : 0;
                int16_t top = (y > 0) ? static_cast<int16_t>(p[(y - 1) * sw + x]) : 0;
                int16_t topleft = (x > 0 && y > 0) ? static_cast<int16_t>(p[(y - 1) * sw + (x - 1)]) : 0;
                int16_t pred = med_predict(left, top, topleft);
                int16_t val = static_cast<int16_t>(p[y * sw + x]);
                int16_t diff = val - pred;

                uint16_t uval = (diff >= 0) ? static_cast<uint16_t>(diff * 2)
                                            : static_cast<uint16_t>(-diff * 2 - 1);
                symbols.push_back(uval & 0xFF);
                symbols.push_back((uval >> 8) & 0xFF);
            }
        }
    };

    encode_subplane(planes.r, planes.sub_w, planes.sub_h);
    encode_subplane(planes.g1, planes.sub_w, planes.sub_h);
    encode_subplane(planes.g2, planes.sub_w, planes.sub_h);
    encode_subplane(planes.b, planes.sub_w, planes.sub_h);

    std::vector<RansSymbol> symbol_stats(256);
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
        symbol_stats[s].start = running;
        symbol_stats[s].freq = 128;
        running += 128;
    }

    Rans8Codec::EncodedChunk rchunk = Rans8Codec::encode(symbols, symbol_stats);
    frame.rans_payload = Rans8Codec::serialize(rchunk);

    return frame;
}

std::vector<uint16_t> PscEngine::decompress_bayer_frame(const CompressedBayerFrame& frame,
                                                        const BayerCfaSubplanes* ref_planes) {
    validate_bayer_geometry(frame.width, frame.height, frame.bit_depth);
    if (frame.rans_payload.empty()) {
        throw std::runtime_error("Compressed Bayer frame has no payload");
    }

    uint32_t sw = frame.width / 2;
    uint32_t sh = frame.height / 2;

    Rans8Codec::EncodedChunk rchunk = Rans8Codec::deserialize(frame.rans_payload.data(),
                                                             frame.rans_payload.size());

    std::vector<RansSymbol> symbol_stats(256);
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
        symbol_stats[s].start = running;
        symbol_stats[s].freq = 128;
        running += 128;
    }

    std::vector<uint16_t> cdf_table = Rans8Codec::build_cdf_lut(symbol_stats);
    std::vector<uint16_t> symbols = Rans8Codec::decode(rchunk, cdf_table, symbol_stats);

    const size_t expected_symbols = static_cast<size_t>(sw) * sh * 4 * 2;
    if (symbols.size() != expected_symbols) {
        throw std::runtime_error("Bayer payload symbol count does not match the declared geometry");
    }
    size_t sym_idx = 0;

    auto decode_uval = [&]() -> uint16_t {
        if (sym_idx + 1 >= symbols.size()) return 0;
        uint16_t low = symbols[sym_idx++];
        uint16_t high = symbols[sym_idx++];
        return (high << 8) | low;
    };

    BayerCfaSubplanes planes;
    planes.sub_w = sw;
    planes.sub_h = sh;
    planes.bit_depth = frame.bit_depth;
    size_t count = static_cast<size_t>(sw) * sh;
    planes.r.resize(count);
    planes.g1.resize(count);
    planes.g2.resize(count);
    planes.b.resize(count);

    auto decode_subplane = [&](std::vector<uint16_t>& p) {
        for (uint32_t y = 0; y < sh; ++y) {
            for (uint32_t x = 0; x < sw; ++x) {
                int16_t left = (x > 0) ? static_cast<int16_t>(p[y * sw + (x - 1)]) : 0;
                int16_t top = (y > 0) ? static_cast<int16_t>(p[(y - 1) * sw + x]) : 0;
                int16_t topleft = (x > 0 && y > 0) ? static_cast<int16_t>(p[(y - 1) * sw + (x - 1)]) : 0;
                int16_t pred = med_predict(left, top, topleft);

                uint16_t uval = decode_uval();
                int16_t diff = (uval % 2 == 0) ? static_cast<int16_t>(uval / 2)
                                               : -static_cast<int16_t>((uval + 1) / 2);
                p[y * sw + x] = static_cast<uint16_t>(pred + diff);
            }
        }
    };

    decode_subplane(planes.r);
    decode_subplane(planes.g1);
    decode_subplane(planes.g2);
    decode_subplane(planes.b);

    if (frame.is_delta && ref_planes) {
        planes = apply_bayer_temporal_delta(planes, *ref_planes);
    }

    std::vector<uint16_t> bayer_out(static_cast<size_t>(frame.width) * frame.height);
    merge_bayer_cfa(planes, bayer_out.data(), frame.width, frame.height);

    // Verify SHA-256 bit-exact parity
    size_t raw_bytes = bayer_out.size() * sizeof(uint16_t);
    Sha256Digest recon_sha = BbrEngine::compute_sha256(reinterpret_cast<const uint8_t*>(bayer_out.data()), raw_bytes);
    if (recon_sha != frame.original_bayer_sha256) {
        throw std::runtime_error("Bayer CFA Decompression SHA-256 parity mismatch! Bit-exact PSNR=inf violated.");
    }

    return bayer_out;
}

bool PscEngine::verify_bayer_parity(const uint16_t* orig, const uint16_t* recon, size_t count) {
    if (!orig || !recon) return false;
    return std::memcmp(orig, recon, count * sizeof(uint16_t)) == 0;
}

} // namespace prism
