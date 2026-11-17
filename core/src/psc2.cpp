#include "prism/psc2.hpp"
#include "prism/bbr.hpp"
#include "prism/psc.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

namespace prism {

namespace {

void validate_dimensions(uint32_t width, uint32_t height) {
  if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
    throw std::invalid_argument(
        "PSC 2.0 requires even dimensions of at least 2x2");
  }
  if (width > std::numeric_limits<size_t>::max() / height ||
      static_cast<size_t>(width) * height >
          std::numeric_limits<size_t>::max() / 4) {
    throw std::length_error(
        "Frame dimensions overflow the addressable memory size");
  }
}

// Fixed-point signed clamp
inline int16_t clamp_i16(int32_t val) {
  return static_cast<int16_t>(std::clamp<int32_t>(val, -32768, 32767));
}

inline uint8_t clamp_u8(int32_t val) {
  return static_cast<uint8_t>(std::clamp<int32_t>(val, 0, 255));
}

} // namespace

// =========================================================================
// Pillar 0: 4-Pass Checkerboard Causal Integer ConvNet (Neural Distillation)
// =========================================================================

CheckerboardConvNet::CheckerboardConvNet() {
  // Fixed deterministic weights conforming to Prism-DetInt bounds (K=8 <= 512,
  // weights in [-127, 127])
  const int8_t init_l1[8 * 8] = {
      // [left, top, top_left, top_right, ll, left-tl, top-tl, pass]
      32, 32, -16, 0,  16, 16, 16, 4, 64,  0,   -32, 0,   0,  32,  0,   0,
      0,  64, -32, 0,  0,  0,  32, 0, -16, -16, 48,  -16, 0,  -16, -16, 0,
      16, 16, 16,  16, 32, 0,  0,  8, 24,  -24, 0,   12,  0,  24,  -12, 0,
      40, 40, -40, 0,  12, 20, 20, 4, 8,   8,   8,   8,   16, 8,   8,   16};
  std::memcpy(layer1_weights_.data(), init_l1, sizeof(init_l1));
  layer1_bias_.fill(0);

  const int8_t init_l2[2 * 8] = {40, 16, 16, -8, 20, 10, 30, 0,
                                 0,  8,  8,  12, 0,  10, 8,  48};
  std::memcpy(layer2_weights_.data(), init_l2, sizeof(init_l2));
  layer2_bias_[0] = 0;
  layer2_bias_[1] = 2048; // Base scale bias
}

CheckerboardConvNet::LogisticParams
CheckerboardConvNet::predict(int16_t left, int16_t top, int16_t top_left,
                             int16_t top_right, int16_t ll_context,
                             uint8_t pass_index) const {
  int16_t med = PscEngine::med_predict(left, top, top_left);

  int16_t x[8];
  x[0] = static_cast<int16_t>(std::clamp<int32_t>(left - med, -256, 255));
  x[1] = static_cast<int16_t>(std::clamp<int32_t>(top - med, -256, 255));
  x[2] = static_cast<int16_t>(std::clamp<int32_t>(top_left - med, -256, 255));
  x[3] = static_cast<int16_t>(std::clamp<int32_t>(top_right - med, -256, 255));
  x[4] = static_cast<int16_t>(std::clamp<int32_t>(ll_context >> 2, -256, 255));
  x[5] = static_cast<int16_t>(std::clamp<int32_t>(left - top_left, -256, 255));
  x[6] = static_cast<int16_t>(std::clamp<int32_t>(top - top_left, -256, 255));
  x[7] = static_cast<int16_t>(pass_index * 16);

  int16_t h[8];
  for (size_t m = 0; m < 8; ++m) {
    int32_t acc = DetInt::dot_product(x, layer1_weights_.data() + (m * 8), 8) +
                  layer1_bias_[m];
    h[m] = DetInt::dyadic_rescale(acc, 128, 7);
    if (h[m] > 127)
      h[m] = 127;
    if (h[m] < -128)
      h[m] = -128;
  }

  int32_t acc_mu =
      DetInt::dot_product(h, layer2_weights_.data(), 8) + layer2_bias_[0];
  int32_t delta_mu =
      (acc_mu + 256) >> 9; // Bounded delta correction in [-32, +32]
  delta_mu = std::clamp<int32_t>(delta_mu, -32, 32);

  int16_t mu = static_cast<int16_t>(
      std::clamp<int32_t>(static_cast<int32_t>(med) + delta_mu, -32768, 32767));

  int32_t acc_sigma =
      DetInt::dot_product(h, layer2_weights_.data() + 8, 8) + layer2_bias_[1];
  int32_t raw_sigma = (acc_sigma + 64) >> 7;
  uint8_t sigma = static_cast<uint8_t>(std::clamp<int32_t>(raw_sigma, 4, 127));

  return {mu, sigma};
}

void CheckerboardConvNet::evaluate_tile_pass(
    const int16_t *tile_data, uint32_t stride, uint32_t w, uint32_t h,
    const int16_t *ll_plane, uint32_t ll_stride, uint8_t pass_index,
    std::vector<LogisticParams> &out_params) const {
  out_params.clear();
  uint32_t r_start = (pass_index >= 2) ? 1 : 0;
  uint32_t c_start = (pass_index % 2 == 1) ? 1 : 0;

  for (uint32_t r = r_start; r < h; r += 2) {
    for (uint32_t c = c_start; c < w; c += 2) {
      int16_t left = (c > 0) ? tile_data[r * stride + (c - 1)] : 0;
      int16_t top = (r > 0) ? tile_data[(r - 1) * stride + c] : 0;
      int16_t tl = (c > 0 && r > 0) ? tile_data[(r - 1) * stride + (c - 1)] : 0;
      int16_t tr =
          (c + 1 < w && r > 0) ? tile_data[(r - 1) * stride + (c + 1)] : top;
      int16_t ll = (ll_plane) ? ll_plane[(r / 2) * ll_stride + (c / 2)] : 0;

      out_params.push_back(predict(left, top, tl, tr, ll, pass_index));
    }
  }
}

const CheckerboardConvNet &Psc2Engine::get_convnet() {
  static const CheckerboardConvNet s_convnet;
  return s_convnet;
}

Psc2Engine::Psc2Engine(Psc2Config config) : config_(std::move(config)) {
  if (config_.tile_size < 16 || (config_.tile_size % 2) != 0) {
    config_.tile_size = 128;
  }
  if (config_.num_worker_threads == 0) {
    config_.num_worker_threads =
        std::max(1u, std::thread::hardware_concurrency());
  }
}

Psc2Engine::~Psc2Engine() = default;
Psc2Engine::Psc2Engine(Psc2Engine &&) noexcept = default;
Psc2Engine &Psc2Engine::operator=(Psc2Engine &&) noexcept = default;

// =========================================================================
// Pillar 1: Adaptive Directional Integer Wavelet (ADIW 5/3 & 9/7)
// =========================================================================

void Psc2Engine::forward_adiw_2d(int16_t *tile_data, uint32_t stride,
                                 uint32_t width, uint32_t height,
                                 AdiwOrientation orientation,
                                 WaveletFilterType filter) {
  if (!tile_data || width < 2 || height < 2)
    return;

  uint32_t half_w = width / 2;
  uint32_t half_h = height / 2;

  std::vector<int16_t> temp_row_low(half_w);
  std::vector<int16_t> temp_row_high(half_w);

  // Pass 1: 1D Horizontal Lifting along rows
  for (uint32_t y = 0; y < height; ++y) {
    int16_t *row = tile_data + y * stride;
    if (filter == WaveletFilterType::LEGALL_53) {
      // Predict
      for (uint32_t i = 0; i < half_w; ++i) {
        int16_t left = row[2 * i];
        int16_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        temp_row_high[i] = row[2 * i + 1] - ((left + right + 1) >> 1);
      }
      // Update
      for (uint32_t i = 0; i < half_w; ++i) {
        int16_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int16_t h_curr = temp_row_high[i];
        temp_row_low[i] = row[2 * i] + ((h_prev + h_curr + 2) >> 2);
      }
    } else {
      // 4-Stage Rational Dyadic 9/7 Integer Lifting
      // Predict 1 (alpha = -203/128)
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t left = row[2 * i];
        int32_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        int32_t p = (-203 * (left + right) + 64) >> 7;
        temp_row_high[i] = clamp_i16(row[2 * i + 1] + p);
      }
      // Update 1 (beta = -7/128)
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int32_t h_curr = temp_row_high[i];
        int32_t u = (-7 * (h_prev + h_curr) + 64) >> 7;
        temp_row_low[i] = clamp_i16(row[2 * i] + u);
      }
      // Predict 2 (gamma = 113/128)
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t l_curr = temp_row_low[i];
        int32_t l_next =
            (i + 1 < half_w) ? temp_row_low[i + 1] : temp_row_low[i];
        int32_t p = (113 * (l_curr + l_next) + 64) >> 7;
        temp_row_high[i] = clamp_i16(temp_row_high[i] + p);
      }
      // Update 2 (delta = 57/128)
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int32_t h_curr = temp_row_high[i];
        int32_t u = (57 * (h_prev + h_curr) + 64) >> 7;
        temp_row_low[i] = clamp_i16(temp_row_low[i] + u);
      }
    }

    // Store split back into row: [Low | High]
    for (uint32_t i = 0; i < half_w; ++i) {
      row[i] = temp_row_low[i];
      row[half_w + i] = temp_row_high[i];
    }
  }

  // Pass 2: Directional Lifting along columns
  // Orientation determines column displacement delta:
  // HORIZONTAL (0 deg): delta = 0
  // VERTICAL   (90 deg): delta = 0
  // DIAGONAL   (45 deg): delta = -1 (left-slanted trajectory)
  // ANTI_DIAG  (135 deg): delta = +1 (right-slanted trajectory)
  int32_t delta = 0;
  if (orientation == AdiwOrientation::DIAGONAL) {
    delta = -1;
  } else if (orientation == AdiwOrientation::ANTI_DIAG) {
    delta = +1;
  }

  std::vector<int16_t> col_pass(height * stride);

  // Apply directional column lifting separately to Low half (x < half_w) and
  // High half (x >= half_w)
  for (uint32_t sub_band = 0; sub_band < 2; ++sub_band) {
    uint32_t x_offset = (sub_band == 0) ? 0 : half_w;

    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;

      // Predict odd row samples from even row samples along trajectory delta
      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t odd_y = 2 * j + 1;
        uint32_t prev_y = 2 * j;
        uint32_t next_y = (2 * j + 2 < height) ? 2 * j + 2 : 2 * j;

        int32_t prev_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) - delta, x_offset,
                                x_offset + half_w - 1);
        int32_t next_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) + delta, x_offset,
                                x_offset + half_w - 1);

        int16_t top = tile_data[prev_y * stride + prev_x];
        int16_t bot = tile_data[next_y * stride + next_x];
        int16_t odd_val = tile_data[odd_y * stride + cur_x];

        col_pass[(half_h + j) * stride + cur_x] =
            odd_val - ((top + bot + 1) >> 1);
      }

      // Update even row samples using high-frequency details
      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t even_y = 2 * j;
        int16_t h_prev = (j > 0) ? col_pass[(half_h + j - 1) * stride + cur_x]
                                 : col_pass[half_h * stride + cur_x];
        int16_t h_curr = col_pass[(half_h + j) * stride + cur_x];
        int16_t even_val = tile_data[even_y * stride + cur_x];

        col_pass[j * stride + cur_x] = even_val + ((h_prev + h_curr + 2) >> 2);
      }
    }
  }

  // Copy col_pass back into tile_data
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(tile_data + y * stride, col_pass.data() + y * stride,
                width * sizeof(int16_t));
  }
}

void Psc2Engine::inverse_adiw_2d(int16_t *tile_data, uint32_t stride,
                                 uint32_t width, uint32_t height,
                                 AdiwOrientation orientation,
                                 WaveletFilterType filter) {
  if (!tile_data || width < 2 || height < 2)
    return;

  uint32_t half_w = width / 2;
  uint32_t half_h = height / 2;

  int32_t delta = 0;
  if (orientation == AdiwOrientation::DIAGONAL) {
    delta = -1;
  } else if (orientation == AdiwOrientation::ANTI_DIAG) {
    delta = +1;
  }

  std::vector<int16_t> col_recon(height * stride);

  // Invert Pass 2: Directional Lifting along columns
  for (uint32_t sub_band = 0; sub_band < 2; ++sub_band) {
    uint32_t x_offset = (sub_band == 0) ? 0 : half_w;

    // Step 1: Invert Update to recover all even row samples across all columns
    // first
    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;
      for (uint32_t j = 0; j < half_h; ++j) {
        int16_t h_prev = (j > 0) ? tile_data[(half_h + j - 1) * stride + cur_x]
                                 : tile_data[half_h * stride + cur_x];
        int16_t h_curr = tile_data[(half_h + j) * stride + cur_x];
        int16_t low_val = tile_data[j * stride + cur_x];
        col_recon[(2 * j) * stride + cur_x] =
            low_val - ((h_prev + h_curr + 2) >> 2);
      }
    }

    // Step 2: Invert Predict to recover odd row samples from reconstructed even
    // row samples
    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;
      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t odd_y = 2 * j + 1;
        uint32_t prev_y = 2 * j;
        uint32_t next_y = (2 * j + 2 < height) ? 2 * j + 2 : 2 * j;

        int32_t prev_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) - delta, x_offset,
                                x_offset + half_w - 1);
        int32_t next_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) + delta, x_offset,
                                x_offset + half_w - 1);

        int16_t top = col_recon[prev_y * stride + prev_x];
        int16_t bot = col_recon[next_y * stride + next_x];
        int16_t h_val = tile_data[(half_h + j) * stride + cur_x];

        col_recon[odd_y * stride + cur_x] = h_val + ((top + bot + 1) >> 1);
      }
    }
  }

  // Copy col_recon back into tile_data
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(tile_data + y * stride, col_recon.data() + y * stride,
                width * sizeof(int16_t));
  }

  // Invert Pass 1: 1D Horizontal Lifting along rows
  std::vector<int16_t> row_low(half_w);
  std::vector<int16_t> row_high(half_w);

  for (uint32_t y = 0; y < height; ++y) {
    int16_t *row = tile_data + y * stride;
    for (uint32_t i = 0; i < half_w; ++i) {
      row_low[i] = row[i];
      row_high[i] = row[half_w + i];
    }

    if (filter == WaveletFilterType::LEGALL_53) {
      // Invert Update
      for (uint32_t i = 0; i < half_w; ++i) {
        int16_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int16_t h_curr = row_high[i];
        row[2 * i] = row_low[i] - ((h_prev + h_curr + 2) >> 2);
      }
      // Invert Predict
      for (uint32_t i = 0; i < half_w; ++i) {
        int16_t left = row[2 * i];
        int16_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        row[2 * i + 1] = row_high[i] + ((left + right + 1) >> 1);
      }
    } else {
      // Invert 4-Stage Dyadic 9/7 in exact reverse sequence
      // Invert Update 2
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int32_t h_curr = row_high[i];
        int32_t u = (57 * (h_prev + h_curr) + 64) >> 7;
        row_low[i] = clamp_i16(row_low[i] - u);
      }
      // Invert Predict 2
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t l_curr = row_low[i];
        int32_t l_next = (i + 1 < half_w) ? row_low[i + 1] : row_low[i];
        int32_t p = (113 * (l_curr + l_next) + 64) >> 7;
        row_high[i] = clamp_i16(row_high[i] - p);
      }
      // Invert Update 1
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int32_t h_curr = row_high[i];
        int32_t u = (-7 * (h_prev + h_curr) + 64) >> 7;
        row_low[i] = clamp_i16(row_low[i] - u);
      }
      // Invert Predict 1
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t left = row_low[i];
        int32_t right = (i + 1 < half_w) ? row_low[i + 1] : row_low[i];
        int32_t p = (-203 * (left + right) + 64) >> 7;
        row[2 * i + 1] = clamp_i16(row_high[i] - p);
        row[2 * i] = row_low[i];
      }
    }
  }
}

void Psc2Engine::forward_adiw_2d_i32(int32_t *tile_data, uint32_t stride,
                                     uint32_t width, uint32_t height,
                                     AdiwOrientation orientation,
                                     WaveletFilterType filter) {
  if (!tile_data || width < 2 || height < 2)
    return;

  uint32_t half_w = width / 2;
  uint32_t half_h = height / 2;

  std::vector<int32_t> temp_row_low(half_w);
  std::vector<int32_t> temp_row_high(half_w);

  // Pass 1: 1D Horizontal Lifting along rows
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *row = tile_data + y * stride;
    if (filter == WaveletFilterType::LEGALL_53) {
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t left = row[2 * i];
        int32_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        temp_row_high[i] = row[2 * i + 1] - ((left + right + 1) >> 1);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int32_t h_curr = temp_row_high[i];
        temp_row_low[i] = row[2 * i] + ((h_prev + h_curr + 2) >> 2);
      }
    } else {
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t left = row[2 * i];
        int64_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        int64_t p = (-203 * (left + right) + 64) >> 7;
        temp_row_high[i] = static_cast<int32_t>(row[2 * i + 1] + p);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int64_t h_curr = temp_row_high[i];
        int64_t u = (-7 * (h_prev + h_curr) + 64) >> 7;
        temp_row_low[i] = static_cast<int32_t>(row[2 * i] + u);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t l_curr = temp_row_low[i];
        int64_t l_next =
            (i + 1 < half_w) ? temp_row_low[i + 1] : temp_row_low[i];
        int64_t p = (113 * (l_curr + l_next) + 64) >> 7;
        temp_row_high[i] = static_cast<int32_t>(temp_row_high[i] + p);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t h_prev = (i > 0) ? temp_row_high[i - 1] : temp_row_high[0];
        int64_t h_curr = temp_row_high[i];
        int64_t u = (57 * (h_prev + h_curr) + 64) >> 7;
        temp_row_low[i] = static_cast<int32_t>(temp_row_low[i] + u);
      }
    }

    for (uint32_t i = 0; i < half_w; ++i) {
      row[i] = temp_row_low[i];
      row[half_w + i] = temp_row_high[i];
    }
  }

  // Pass 2: Directional Lifting along columns
  int32_t delta = 0;
  if (orientation == AdiwOrientation::DIAGONAL) {
    delta = -1;
  } else if (orientation == AdiwOrientation::ANTI_DIAG) {
    delta = +1;
  }

  std::vector<int32_t> col_pass(height * stride);

  for (uint32_t sub_band = 0; sub_band < 2; ++sub_band) {
    uint32_t x_offset = (sub_band == 0) ? 0 : half_w;

    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;

      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t odd_y = 2 * j + 1;
        uint32_t prev_y = 2 * j;
        uint32_t next_y = (2 * j + 2 < height) ? 2 * j + 2 : 2 * j;

        int32_t prev_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) - delta, x_offset,
                                x_offset + half_w - 1);
        int32_t next_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) + delta, x_offset,
                                x_offset + half_w - 1);

        int32_t top = tile_data[prev_y * stride + prev_x];
        int32_t bot = tile_data[next_y * stride + next_x];
        int32_t odd_val = tile_data[odd_y * stride + cur_x];

        col_pass[(half_h + j) * stride + cur_x] =
            odd_val - ((top + bot + 1) >> 1);
      }

      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t even_y = 2 * j;
        int32_t h_prev = (j > 0) ? col_pass[(half_h + j - 1) * stride + cur_x]
                                 : col_pass[half_h * stride + cur_x];
        int32_t h_curr = col_pass[(half_h + j) * stride + cur_x];
        int32_t even_val = tile_data[even_y * stride + cur_x];

        col_pass[j * stride + cur_x] = even_val + ((h_prev + h_curr + 2) >> 2);
      }
    }
  }

  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(tile_data + y * stride, col_pass.data() + y * stride,
                width * sizeof(int32_t));
  }
}

void Psc2Engine::inverse_adiw_2d_i32(int32_t *tile_data, uint32_t stride,
                                     uint32_t width, uint32_t height,
                                     AdiwOrientation orientation,
                                     WaveletFilterType filter) {
  if (!tile_data || width < 2 || height < 2)
    return;

  uint32_t half_w = width / 2;
  uint32_t half_h = height / 2;

  int32_t delta = 0;
  if (orientation == AdiwOrientation::DIAGONAL) {
    delta = -1;
  } else if (orientation == AdiwOrientation::ANTI_DIAG) {
    delta = +1;
  }

  std::vector<int32_t> col_recon(height * stride);

  // Invert Pass 2
  for (uint32_t sub_band = 0; sub_band < 2; ++sub_band) {
    uint32_t x_offset = (sub_band == 0) ? 0 : half_w;

    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;
      for (uint32_t j = 0; j < half_h; ++j) {
        int32_t h_prev = (j > 0) ? tile_data[(half_h + j - 1) * stride + cur_x]
                                 : tile_data[half_h * stride + cur_x];
        int32_t h_curr = tile_data[(half_h + j) * stride + cur_x];
        int32_t low_val = tile_data[j * stride + cur_x];
        col_recon[(2 * j) * stride + cur_x] =
            low_val - ((h_prev + h_curr + 2) >> 2);
      }
    }

    for (uint32_t x = 0; x < half_w; ++x) {
      uint32_t cur_x = x_offset + x;
      for (uint32_t j = 0; j < half_h; ++j) {
        uint32_t odd_y = 2 * j + 1;
        uint32_t prev_y = 2 * j;
        uint32_t next_y = (2 * j + 2 < height) ? 2 * j + 2 : 2 * j;

        int32_t prev_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) - delta, x_offset,
                                x_offset + half_w - 1);
        int32_t next_x =
            std::clamp<int32_t>(static_cast<int32_t>(cur_x) + delta, x_offset,
                                x_offset + half_w - 1);

        int32_t top = col_recon[prev_y * stride + prev_x];
        int32_t bot = col_recon[next_y * stride + next_x];
        int32_t h_val = tile_data[(half_h + j) * stride + cur_x];

        col_recon[odd_y * stride + cur_x] = h_val + ((top + bot + 1) >> 1);
      }
    }
  }

  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(tile_data + y * stride, col_recon.data() + y * stride,
                width * sizeof(int32_t));
  }

  // Invert Pass 1
  std::vector<int32_t> row_low(half_w);
  std::vector<int32_t> row_high(half_w);

  for (uint32_t y = 0; y < height; ++y) {
    int32_t *row = tile_data + y * stride;
    for (uint32_t i = 0; i < half_w; ++i) {
      row_low[i] = row[i];
      row_high[i] = row[half_w + i];
    }

    if (filter == WaveletFilterType::LEGALL_53) {
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int32_t h_curr = row_high[i];
        row[2 * i] = row_low[i] - ((h_prev + h_curr + 2) >> 2);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int32_t left = row[2 * i];
        int32_t right = (2 * i + 2 < width) ? row[2 * i + 2] : row[2 * i];
        row[2 * i + 1] = row_high[i] + ((left + right + 1) >> 1);
      }
    } else {
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int64_t h_curr = row_high[i];
        int64_t u = (57 * (h_prev + h_curr) + 64) >> 7;
        row_low[i] = static_cast<int32_t>(row_low[i] - u);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t l_curr = row_low[i];
        int64_t l_next = (i + 1 < half_w) ? row_low[i + 1] : row_low[i];
        int64_t p = (113 * (l_curr + l_next) + 64) >> 7;
        row_high[i] = static_cast<int32_t>(row_high[i] - p);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t h_prev = (i > 0) ? row_high[i - 1] : row_high[0];
        int64_t h_curr = row_high[i];
        int64_t u = (-7 * (h_prev + h_curr) + 64) >> 7;
        row_low[i] = static_cast<int32_t>(row_low[i] - u);
      }
      for (uint32_t i = 0; i < half_w; ++i) {
        int64_t left = row_low[i];
        int64_t right = (i + 1 < half_w) ? row_low[i + 1] : row_low[i];
        int64_t p = (-203 * (left + right) + 64) >> 7;
        int32_t odd = static_cast<int32_t>(row_high[i] - p);
        row[2 * i] = row_low[i];
        row[2 * i + 1] = odd;
      }
    }
  }
}

AdiwOrientation
Psc2Engine::detect_orientation_from_ll(const int16_t *ll_plane, uint32_t stride,
                                       uint32_t x_start, uint32_t y_start,
                                       uint32_t block_w, uint32_t block_h) {
  if (!ll_plane || block_w < 2 || block_h < 2)
    return AdiwOrientation::HORIZONTAL;

  int64_t grad_h = 0;  // Horizontal gradient (indicates vertical edges)
  int64_t grad_v = 0;  // Vertical gradient (indicates horizontal edges)
  int64_t grad_d1 = 0; // 45 deg gradient
  int64_t grad_d2 = 0; // 135 deg gradient

  for (uint32_t y = 1; y + 1 < block_h; ++y) {
    for (uint32_t x = 1; x + 1 < block_w; ++x) {
      uint32_t cx = x_start + x;
      uint32_t cy = y_start + y;

      int16_t l = ll_plane[cy * stride + (cx - 1)];
      int16_t r = ll_plane[cy * stride + (cx + 1)];
      int16_t t = ll_plane[(cy - 1) * stride + cx];
      int16_t b = ll_plane[(cy + 1) * stride + cx];
      int16_t tl = ll_plane[(cy - 1) * stride + (cx - 1)];
      int16_t tr = ll_plane[(cy - 1) * stride + (cx + 1)];
      int16_t bl = ll_plane[(cy + 1) * stride + (cx - 1)];
      int16_t br = ll_plane[(cy + 1) * stride + (cx + 1)];

      grad_h += std::abs(r - l);
      grad_v += std::abs(b - t);
      grad_d1 += std::abs(tr - bl);
      grad_d2 += std::abs(br - tl);
    }
  }

  // Pick orientation with minimal directional change (aligned along edge)
  int64_t min_grad = grad_h;
  AdiwOrientation best_ori =
      AdiwOrientation::VERTICAL; // Strong horizontal variation -> vertical edge

  if (grad_v < min_grad) {
    min_grad = grad_v;
    best_ori = AdiwOrientation::HORIZONTAL;
  }
  if (grad_d1 < min_grad) {
    min_grad = grad_d1;
    best_ori = AdiwOrientation::DIAGONAL;
  }
  if (grad_d2 < min_grad) {
    best_ori = AdiwOrientation::ANTI_DIAG;
  }

  return best_ori;
}

// =========================================================================
// Pillar 2: Reversible Spectral Chroma Decorrelation (YCoCg-R2 & A-IKLT)
// =========================================================================

void Psc2Engine::forward_ycocgr2(int16_t *y, int16_t *co, int16_t *cg,
                                 size_t count, int8_t alpha, int8_t beta) {
  if (!y || !co || !cg || count == 0)
    return;

  for (size_t i = 0; i < count; ++i) {
    int32_t alpha_pred = (static_cast<int32_t>(alpha) * y[i] + 128) >> 8;
    co[i] = static_cast<int16_t>(co[i] - alpha_pred);

    int32_t beta_pred = (static_cast<int32_t>(beta) * co[i] + 128) >> 8;
    cg[i] = static_cast<int16_t>(cg[i] - beta_pred);
  }
}

void Psc2Engine::inverse_ycocgr2(int16_t *y, int16_t *co, int16_t *cg,
                                 size_t count, int8_t alpha, int8_t beta) {
  if (!y || !co || !cg || count == 0)
    return;

  for (size_t i = 0; i < count; ++i) {
    int32_t beta_pred = (static_cast<int32_t>(beta) * co[i] + 128) >> 8;
    cg[i] = static_cast<int16_t>(cg[i] + beta_pred);

    int32_t alpha_pred = (static_cast<int32_t>(alpha) * y[i] + 128) >> 8;
    co[i] = static_cast<int16_t>(co[i] + alpha_pred);
  }
}

std::pair<int8_t, int8_t>
Psc2Engine::estimate_shear_parameters(const int16_t *y, const int16_t *co,
                                      const int16_t *cg, size_t count) {
  if (!y || !co || !cg || count == 0)
    return {int8_t(0), int8_t(0)};

  int64_t sum_y_co = 0;
  int64_t sum_y2 = 0;
  for (size_t i = 0; i < count; ++i) {
    sum_y_co += static_cast<int64_t>(y[i]) * co[i];
    sum_y2 += static_cast<int64_t>(y[i]) * y[i];
  }

  int8_t alpha = 0;
  if (sum_y2 > 0) {
    int64_t a = (sum_y_co * 256 + (sum_y2 / 2)) / sum_y2;
    alpha = static_cast<int8_t>(std::clamp<int64_t>(a, -128, 127));
  }

  int64_t sum_co_cg = 0;
  int64_t sum_co2 = 0;
  for (size_t i = 0; i < count; ++i) {
    int32_t alpha_pred = (static_cast<int32_t>(alpha) * y[i] + 128) >> 8;
    int16_t co_prime = static_cast<int16_t>(co[i] - alpha_pred);
    sum_co_cg += static_cast<int64_t>(co_prime) * cg[i];
    sum_co2 += static_cast<int64_t>(co_prime) * co_prime;
  }

  int8_t beta = 0;
  if (sum_co2 > 0) {
    int64_t b = (sum_co_cg * 256 + (sum_co2 / 2)) / sum_co2;
    beta = static_cast<int8_t>(std::clamp<int64_t>(b, -128, 127));
  }

  return {alpha, beta};
}

void Psc2Engine::forward_quad_bayer_aiklt(QuadBayerPlanes &planes,
                                          int8_t alpha_r, int8_t alpha_b,
                                          int8_t beta_rb) {
  size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;

  for (size_t i = 0; i < count; ++i) {
    int32_t g1 = planes.g1[i];
    int32_t g2 = planes.g2[i];
    int32_t delta_g = g1 - g2;
    int32_t g_base = g2 + (delta_g >> 1);

    int32_t r_pred = (static_cast<int32_t>(alpha_r) * g_base + 128) >> 8;
    int32_t r_prime = static_cast<int32_t>(planes.r[i]) - r_pred;

    int32_t b_pred = (static_cast<int32_t>(alpha_b) * g_base + 128) >> 8;
    int32_t b_prime = static_cast<int32_t>(planes.b[i]) - b_pred;

    int32_t rb_pred = (static_cast<int32_t>(beta_rb) * b_prime + 128) >> 8;
    int32_t r_double_prime = r_prime - rb_pred;

    planes.g1[i] = static_cast<uint16_t>(static_cast<int16_t>(delta_g));
    planes.g2[i] = static_cast<uint16_t>(static_cast<int16_t>(g_base));
    planes.r[i] = static_cast<uint16_t>(static_cast<int16_t>(r_double_prime));
    planes.b[i] = static_cast<uint16_t>(static_cast<int16_t>(b_prime));
  }
}

void Psc2Engine::inverse_quad_bayer_aiklt(QuadBayerPlanes &planes,
                                          int8_t alpha_r, int8_t alpha_b,
                                          int8_t beta_rb) {
  size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;

  for (size_t i = 0; i < count; ++i) {
    int32_t delta_g = static_cast<int16_t>(planes.g1[i]);
    int32_t g_base = static_cast<int16_t>(planes.g2[i]);
    int32_t b_prime = static_cast<int16_t>(planes.b[i]);
    int32_t r_double_prime = static_cast<int16_t>(planes.r[i]);

    int32_t rb_pred = (static_cast<int32_t>(beta_rb) * b_prime + 128) >> 8;
    int32_t r_prime = r_double_prime + rb_pred;

    int32_t b_pred = (static_cast<int32_t>(alpha_b) * g_base + 128) >> 8;
    int32_t b = b_prime + b_pred;

    int32_t r_pred = (static_cast<int32_t>(alpha_r) * g_base + 128) >> 8;
    int32_t r = r_prime + r_pred;

    int32_t g2 = g_base - (delta_g >> 1);
    int32_t g1 = delta_g + g2;

    planes.r[i] = static_cast<uint16_t>(r);
    planes.g1[i] = static_cast<uint16_t>(g1);
    planes.g2[i] = static_cast<uint16_t>(g2);
    planes.b[i] = static_cast<uint16_t>(b);
  }
}

std::tuple<int8_t, int8_t, int8_t>
Psc2Engine::estimate_quad_bayer_shears(const QuadBayerPlanes &planes) {
  size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;
  if (count == 0)
    return {int8_t(0), int8_t(0), int8_t(0)};

  int64_t sum_g_r = 0, sum_g_b = 0, sum_g2 = 0;
  for (size_t i = 0; i < count; ++i) {
    int64_t g_base = (planes.g1[i] + planes.g2[i]) / 2;
    sum_g_r += g_base * planes.r[i];
    sum_g_b += g_base * planes.b[i];
    sum_g2 += g_base * g_base;
  }

  int8_t alpha_r = 0, alpha_b = 0;
  if (sum_g2 > 0) {
    alpha_r = static_cast<int8_t>(std::clamp<int64_t>(
        (sum_g_r * 256 + (sum_g2 / 2)) / sum_g2, -128, 127));
    alpha_b = static_cast<int8_t>(std::clamp<int64_t>(
        (sum_g_b * 256 + (sum_g2 / 2)) / sum_g2, -128, 127));
  }

  int64_t sum_b_r = 0, sum_b2 = 0;
  for (size_t i = 0; i < count; ++i) {
    int64_t g_base = (planes.g1[i] + planes.g2[i]) / 2;
    int64_t r_prime = planes.r[i] - ((alpha_r * g_base + 128) >> 8);
    int64_t b_prime = planes.b[i] - ((alpha_b * g_base + 128) >> 8);
    sum_b_r += b_prime * r_prime;
    sum_b2 += b_prime * b_prime;
  }

  int8_t beta_rb = 0;
  if (sum_b2 > 0) {
    beta_rb = static_cast<int8_t>(std::clamp<int64_t>(
        (sum_b_r * 256 + (sum_b2 / 2)) / sum_b2, -128, 127));
  }

  return {alpha_r, alpha_b, beta_rb};
}

// =========================================================================
// Pillar 3: Tile-Wavefront Lock-Free SIMD Pipelining & High-Level API
// =========================================================================

static std::vector<RansSymbol> compute_byte_stats(const std::vector<uint16_t> &symbols) {
  std::array<uint32_t, 256> counts{};
  counts.fill(1); // Laplace add-1 smoothing

  for (uint16_t sym : symbols) {
    if (sym < 256) {
      counts[sym]++;
    }
  }

  uint32_t total_count = 0;
  for (size_t s = 0; s < 256; ++s) {
    total_count += counts[s];
  }

  constexpr uint32_t MIN_FREQ = 1;
  constexpr uint32_t TOTAL_MASS = Rans8Codec::RANS_M;         // 32768
  constexpr uint32_t RESERVED_MASS = MIN_FREQ * 256;          // 256
  constexpr uint32_t USABLE_MASS = TOTAL_MASS - RESERVED_MASS; // 32512

  std::vector<RansSymbol> stats(256);
  uint16_t running_start = 0;

  for (size_t s = 0; s < 256; ++s) {
    uint32_t scaled =
        MIN_FREQ +
        static_cast<uint32_t>((static_cast<uint64_t>(counts[s]) * USABLE_MASS) /
                              total_count);
    if (s == 255) {
      scaled = TOTAL_MASS - running_start;
    }

    stats[s].start = running_start;
    stats[s].freq = static_cast<uint16_t>(scaled);
    running_start += static_cast<uint16_t>(scaled);
  }

  return stats;
}

Psc2Engine::CompressedPsc2Frame
Psc2Engine::compress_frame_rgb(const uint8_t *raw_rgb, size_t total_bytes) {
  if (!raw_rgb)
    throw std::invalid_argument("Input RGB frame buffer is null");
  validate_dimensions(config_.width, config_.height);

  size_t expected_bytes =
      static_cast<size_t>(config_.width) * config_.height * 3;
  if (total_bytes != expected_bytes) {
    throw std::invalid_argument(
        "Input byte count does not match width x height x 3");
  }

  CompressedPsc2Frame frame;
  frame.config = config_;
  frame.original_sha256 = BbrEngine::compute_sha256(raw_rgb, total_bytes);
  frame.is_quad_bayer = false;

  uint32_t w = config_.width;
  uint32_t h = config_.height;
  uint16_t ts = config_.tile_size;

  uint32_t tiles_x = (w + ts - 1) / ts;
  uint32_t tiles_y = (h + ts - 1) / ts;

  // Convert full frame to planar YCoCg-R
  std::vector<int16_t> y_plane(w * h);
  std::vector<int16_t> co_plane(w * h);
  std::vector<int16_t> cg_plane(w * h);

  size_t total_pixels = static_cast<size_t>(w) * h;
  size_t num_threads = std::max<uint32_t>(1, config_.num_worker_threads);

  auto convert_rgb_chunk = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      RgbPixel rgb{raw_rgb[i * 3 + 0], raw_rgb[i * 3 + 1], raw_rgb[i * 3 + 2]};
      YCoCgPixel ycocg = PscEngine::rgb_to_ycocgr(rgb);
      y_plane[i] = ycocg.y;
      co_plane[i] = ycocg.co;
      cg_plane[i] = ycocg.cg;
    }
  };

  if (num_threads > 1 && total_pixels >= 65536) {
    std::vector<std::future<void>> futures;
    size_t chunk = (total_pixels + num_threads - 1) / num_threads;
    for (size_t t = 0; t < num_threads; ++t) {
      size_t start = t * chunk;
      size_t end = std::min(start + chunk, total_pixels);
      if (start < end) {
        futures.push_back(
            std::async(std::launch::async, convert_rgb_chunk, start, end));
      }
    }
    for (auto &f : futures) {
      f.get();
    }
  } else {
    convert_rgb_chunk(0, total_pixels);
  }

  // Build Wavefront Execution DAG
  // Wavefront index: wave = tx + ty
  uint32_t max_wave = (tiles_x - 1) + (tiles_y - 1);
  std::vector<Psc2TileHeader> headers(tiles_x * tiles_y);
  std::vector<std::vector<uint8_t>> tile_payloads(tiles_x * tiles_y);

  auto process_tile = [&](uint32_t tx, uint32_t ty) {
    uint32_t tile_idx = ty * tiles_x + tx;
    uint32_t cur_x = tx * ts;
    uint32_t cur_y = ty * ts;
    uint32_t cur_w = std::min<uint32_t>(ts, w - cur_x);
    uint32_t cur_h = std::min<uint32_t>(ts, h - cur_y);

    // Extract tile to local buffers (100% L1-resident)
    std::vector<int16_t> tile_y(cur_w * cur_h);
    std::vector<int16_t> tile_co(cur_w * cur_h);
    std::vector<int16_t> tile_cg(cur_w * cur_h);

    for (uint32_t r = 0; r < cur_h; ++r) {
      std::memcpy(tile_y.data() + r * cur_w,
                  y_plane.data() + (cur_y + r) * w + cur_x,
                  cur_w * sizeof(int16_t));
      std::memcpy(tile_co.data() + r * cur_w,
                  co_plane.data() + (cur_y + r) * w + cur_x,
                  cur_w * sizeof(int16_t));
      std::memcpy(tile_cg.data() + r * cur_w,
                  cg_plane.data() + (cur_y + r) * w + cur_x,
                  cur_w * sizeof(int16_t));
    }

    // Estimate and apply YCoCg-R2 adaptive shear
    int8_t alpha = 0, beta = 0;
    if (config_.enable_spectral_coupling) {
      auto shears = estimate_shear_parameters(tile_y.data(), tile_co.data(),
                                              tile_cg.data(), cur_w * cur_h);
      alpha = shears.first;
      beta = shears.second;
      forward_ycocgr2(tile_y.data(), tile_co.data(), tile_cg.data(),
                      cur_w * cur_h, alpha, beta);
    }

    // Detect orientation and apply ADIW 2D lifting
    AdiwOrientation ori = AdiwOrientation::HORIZONTAL;
    if (config_.enable_directional_wavelet && (cur_w % 2 == 0) &&
        (cur_h % 2 == 0)) {
      ori =
          detect_orientation_from_ll(tile_y.data(), cur_w, 0, 0, cur_w, cur_h);
      forward_adiw_2d(tile_y.data(), cur_w, cur_w, cur_h, ori, config_.filter);
      forward_adiw_2d(tile_co.data(), cur_w, cur_w, cur_h, ori, config_.filter);
      forward_adiw_2d(tile_cg.data(), cur_w, cur_w, cur_h, ori, config_.filter);
    }

    // Map subbands to residual symbols via MED or 4-Pass Checkerboard ConvNet
    std::vector<uint16_t> symbols;
    symbols.reserve(cur_w * cur_h * 6);

    auto encode_plane = [&](const std::vector<int16_t> &p) {
      if (config_.enable_checkerboard_convnet) {
        const auto &convnet = get_convnet();
        std::vector<int16_t> work_p(cur_w * cur_h, 0);

        for (uint8_t pass_idx = 0; pass_idx < 4; ++pass_idx) {
          uint32_t r_start = (pass_idx >= 2) ? 1 : 0;
          uint32_t c_start = (pass_idx % 2 == 1) ? 1 : 0;
          for (uint32_t r = r_start; r < cur_h; r += 2) {
            for (uint32_t c = c_start; c < cur_w; c += 2) {
              int16_t left = (c > 0) ? work_p[r * cur_w + (c - 1)] : 0;
              int16_t top = (r > 0) ? work_p[(r - 1) * cur_w + c] : 0;
              int16_t tl =
                  (c > 0 && r > 0) ? work_p[(r - 1) * cur_w + (c - 1)] : 0;
              int16_t tr = (c + 1 < cur_w && r > 0)
                               ? work_p[(r - 1) * cur_w + (c + 1)]
                               : top;
              auto params = convnet.predict(left, top, tl, tr, 0, pass_idx);

              int16_t val = p[r * cur_w + c];
              int16_t diff = val - params.mu;
              work_p[r * cur_w + c] = val; // causal store for subsequent passes

              uint16_t uval = (diff >= 0)
                                  ? static_cast<uint16_t>(diff * 2)
                                  : static_cast<uint16_t>(-diff * 2 - 1);
              symbols.push_back(uval & 0xFF);
              symbols.push_back((uval >> 8) & 0xFF);
            }
          }
        }
      } else {
        for (uint32_t r = 0; r < cur_h; ++r) {
          for (uint32_t c = 0; c < cur_w; ++c) {
            int16_t left = (c > 0) ? p[r * cur_w + (c - 1)] : 0;
            int16_t top = (r > 0) ? p[(r - 1) * cur_w + c] : 0;
            int16_t tl = (c > 0 && r > 0) ? p[(r - 1) * cur_w + (c - 1)] : 0;
            int16_t pred = PscEngine::med_predict(left, top, tl);
            int16_t diff = p[r * cur_w + c] - pred;

            uint16_t uval = (diff >= 0) ? static_cast<uint16_t>(diff * 2)
                                        : static_cast<uint16_t>(-diff * 2 - 1);
            symbols.push_back(uval & 0xFF);
            symbols.push_back((uval >> 8) & 0xFF);
          }
        }
      }
    };

    encode_plane(tile_y);
    encode_plane(tile_co);
    encode_plane(tile_cg);

    // rANS encode tile with empirical statistics
    std::vector<RansSymbol> stats = compute_byte_stats(symbols);
    Rans8Codec::EncodedChunk rchunk = Rans8Codec::encode(symbols, stats);
    std::vector<uint8_t> serialized_chunk = Rans8Codec::serialize(rchunk);

    size_t raw_tile_bytes = static_cast<size_t>(cur_w) * cur_h * 3;
    size_t comp_size = 256 * sizeof(uint16_t) + serialized_chunk.size();

    Psc2TileHeader hdr;
    hdr.tile_x = static_cast<uint16_t>(cur_x);
    hdr.tile_y = static_cast<uint16_t>(cur_y);
    hdr.tile_w = static_cast<uint16_t>(cur_w);
    hdr.tile_h = static_cast<uint16_t>(cur_h);
    hdr.alpha_shear = alpha;
    hdr.beta_shear = beta;
    hdr.dominant_orientation = static_cast<uint8_t>(ori);

    if (comp_size < raw_tile_bytes) {
      hdr.tile_codec_mode = 1; // Empirical rANS
      tile_payloads[tile_idx].resize(comp_size);
      uint8_t *fptr = tile_payloads[tile_idx].data();
      for (size_t s = 0; s < 256; ++s) {
        uint16_t freq = stats[s].freq;
        std::memcpy(fptr, &freq, sizeof(uint16_t));
        fptr += sizeof(uint16_t);
      }
      std::memcpy(fptr, serialized_chunk.data(), serialized_chunk.size());
    } else {
      // Zero-expansion bypass: raw RGB tile
      hdr.tile_codec_mode = 2; // Raw RGB bypass
      tile_payloads[tile_idx].resize(raw_tile_bytes);
      uint8_t *bptr = tile_payloads[tile_idx].data();
      for (uint32_t r = 0; r < cur_h; ++r) {
        std::memcpy(bptr, raw_rgb + ((cur_y + r) * w + cur_x) * 3,
                    cur_w * 3);
        bptr += cur_w * 3;
      }
    }
    hdr.payload_byte_size =
        static_cast<uint32_t>(tile_payloads[tile_idx].size());
    headers[tile_idx] = hdr;
  };

  for (uint32_t wave = 0; wave <= max_wave; ++wave) {
    // Collect tiles in this wavefront level
    std::vector<std::pair<uint32_t, uint32_t>> wave_tiles;
    for (uint32_t ty = 0; ty < tiles_y; ++ty) {
      if (wave >= ty && (wave - ty) < tiles_x) {
        wave_tiles.emplace_back(wave - ty, ty);
      }
    }

    // Process wave_tiles concurrently (mutually independent)
    if (num_threads > 1 && wave_tiles.size() > 1) {
      std::vector<std::future<void>> futures;
      futures.reserve(wave_tiles.size());
      for (const auto &[tx, ty] : wave_tiles) {
        futures.push_back(
            std::async(std::launch::async, [&process_tile, tx = tx, ty = ty]() {
              process_tile(tx, ty);
            }));
      }
      for (auto &f : futures) {
        f.get();
      }
    } else {
      for (const auto &[tx, ty] : wave_tiles) {
        process_tile(tx, ty);
      }
    }
  }

  // Pack headers and bitstream payload
  frame.tile_headers = headers;
  for (size_t i = 0; i < headers.size(); ++i) {
    frame.tile_headers[i].payload_byte_offset =
        static_cast<uint32_t>(frame.bitstream_payload.size());
    frame.bitstream_payload.insert(frame.bitstream_payload.end(),
                                   tile_payloads[i].begin(),
                                   tile_payloads[i].end());
  }

  return frame;
}

std::vector<uint8_t>
Psc2Engine::decompress_frame_rgb(const CompressedPsc2Frame &frame) {
  uint32_t w = frame.config.width;
  uint32_t h = frame.config.height;
  validate_dimensions(w, h);

  if (frame.tile_headers.empty() || frame.bitstream_payload.empty()) {
    throw std::runtime_error(
        "Compressed PSC 2.0 frame has empty payload or headers");
  }

  std::vector<int16_t> y_plane(w * h);
  std::vector<int16_t> co_plane(w * h);
  std::vector<int16_t> cg_plane(w * h);

  std::vector<RansSymbol> uniform_stats(256);
  uint16_t uniform_running = 0;
  for (size_t s = 0; s < 256; ++s) {
    uniform_stats[s].start = uniform_running;
    uniform_stats[s].freq = 128;
    uniform_running += 128;
  }
  std::vector<uint16_t> uniform_cdf_table =
      Rans8Codec::build_cdf_lut(uniform_stats);

  auto decompress_tile = [&](size_t tile_idx) {
    const auto &hdr = frame.tile_headers[tile_idx];
    uint32_t cur_x = hdr.tile_x;
    uint32_t cur_y = hdr.tile_y;
    uint32_t cur_w = hdr.tile_w;
    uint32_t cur_h = hdr.tile_h;

    const uint8_t *payload_ptr =
        frame.bitstream_payload.data() + hdr.payload_byte_offset;

    if (hdr.tile_codec_mode == 2) {
      // Mode 2: Raw RGB bypass tile
      size_t raw_tile_bytes = static_cast<size_t>(cur_w) * cur_h * 3;
      if (hdr.payload_byte_size < raw_tile_bytes) {
        throw std::runtime_error("Corrupted raw RGB bypass tile payload");
      }
      for (uint32_t r = 0; r < cur_h; ++r) {
        for (uint32_t c = 0; c < cur_w; ++c) {
          size_t idx = (r * cur_w + c) * 3;
          RgbPixel rgb{payload_ptr[idx], payload_ptr[idx + 1],
                       payload_ptr[idx + 2]};
          YCoCgPixel ycocg = PscEngine::rgb_to_ycocgr(rgb);
          y_plane[(cur_y + r) * w + cur_x + c] = ycocg.y;
          co_plane[(cur_y + r) * w + cur_x + c] = ycocg.co;
          cg_plane[(cur_y + r) * w + cur_x + c] = ycocg.cg;
        }
      }
      return;
    }

    std::vector<RansSymbol> local_stats;
    const std::vector<RansSymbol> *stats_ptr = &uniform_stats;
    std::vector<uint16_t> local_cdf_table;
    const std::vector<uint16_t> *cdf_table_ptr = &uniform_cdf_table;
    const uint8_t *rchunk_ptr = payload_ptr;
    size_t rchunk_size = hdr.payload_byte_size;

    if (hdr.tile_codec_mode == 1) {
      if (hdr.payload_byte_size < 256 * sizeof(uint16_t)) {
        throw std::runtime_error(
            "PSC 2.0 tile payload too small for empirical frequency table");
      }
      local_stats.resize(256);
      uint16_t running = 0;
      for (size_t s = 0; s < 256; ++s) {
        uint16_t freq = 0;
        std::memcpy(&freq, rchunk_ptr, sizeof(uint16_t));
        rchunk_ptr += sizeof(uint16_t);
        local_stats[s].start = running;
        local_stats[s].freq = freq;
        running += freq;
      }
      rchunk_size -= 256 * sizeof(uint16_t);
      local_cdf_table = Rans8Codec::build_cdf_lut(local_stats);
      stats_ptr = &local_stats;
      cdf_table_ptr = &local_cdf_table;
    }

    Rans8Codec::EncodedChunk rchunk =
        Rans8Codec::deserialize(rchunk_ptr, rchunk_size);
    std::vector<uint16_t> symbols =
        Rans8Codec::decode(rchunk, *cdf_table_ptr, *stats_ptr);

    size_t sym_idx = 0;
    auto decode_uval = [&]() -> uint16_t {
      if (sym_idx + 1 >= symbols.size())
        return 0;
      uint16_t low = symbols[sym_idx++];
      uint16_t high = symbols[sym_idx++];
      return (high << 8) | low;
    };

    auto decode_plane = [&](std::vector<int16_t> &p) {
      p.resize(cur_w * cur_h);
      if (frame.config.enable_checkerboard_convnet) {
        const auto &convnet = get_convnet();
        for (uint8_t pass_idx = 0; pass_idx < 4; ++pass_idx) {
          uint32_t r_start = (pass_idx >= 2) ? 1 : 0;
          uint32_t c_start = (pass_idx % 2 == 1) ? 1 : 0;
          for (uint32_t r = r_start; r < cur_h; r += 2) {
            for (uint32_t c = c_start; c < cur_w; c += 2) {
              int16_t left = (c > 0) ? p[r * cur_w + (c - 1)] : 0;
              int16_t top = (r > 0) ? p[(r - 1) * cur_w + c] : 0;
              int16_t tl = (c > 0 && r > 0) ? p[(r - 1) * cur_w + (c - 1)] : 0;
              int16_t tr =
                  (c + 1 < cur_w && r > 0) ? p[(r - 1) * cur_w + (c + 1)] : top;
              auto params = convnet.predict(left, top, tl, tr, 0, pass_idx);

              uint16_t uval = decode_uval();
              int16_t diff = (uval % 2 == 0)
                                 ? static_cast<int16_t>(uval / 2)
                                 : -static_cast<int16_t>((uval + 1) / 2);
              p[r * cur_w + c] = params.mu + diff;
            }
          }
        }
      } else {
        for (uint32_t r = 0; r < cur_h; ++r) {
          for (uint32_t c = 0; c < cur_w; ++c) {
            int16_t left = (c > 0) ? p[r * cur_w + (c - 1)] : 0;
            int16_t top = (r > 0) ? p[(r - 1) * cur_w + c] : 0;
            int16_t tl = (c > 0 && r > 0) ? p[(r - 1) * cur_w + (c - 1)] : 0;
            int16_t pred = PscEngine::med_predict(left, top, tl);

            uint16_t uval = decode_uval();
            int16_t diff = (uval % 2 == 0)
                               ? static_cast<int16_t>(uval / 2)
                               : -static_cast<int16_t>((uval + 1) / 2);
            p[r * cur_w + c] = pred + diff;
          }
        }
      }
    };

    std::vector<int16_t> tile_y, tile_co, tile_cg;
    decode_plane(tile_y);
    decode_plane(tile_co);
    decode_plane(tile_cg);

    // Invert ADIW lifting
    auto ori = static_cast<AdiwOrientation>(hdr.dominant_orientation);
    if (frame.config.enable_directional_wavelet && (cur_w % 2 == 0) &&
        (cur_h % 2 == 0)) {
      inverse_adiw_2d(tile_cg.data(), cur_w, cur_w, cur_h, ori,
                      frame.config.filter);
      inverse_adiw_2d(tile_co.data(), cur_w, cur_w, cur_h, ori,
                      frame.config.filter);
      inverse_adiw_2d(tile_y.data(), cur_w, cur_w, cur_h, ori,
                      frame.config.filter);
    }

    // Invert YCoCg-R2 shear
    if (frame.config.enable_spectral_coupling) {
      inverse_ycocgr2(tile_y.data(), tile_co.data(), tile_cg.data(),
                      cur_w * cur_h, hdr.alpha_shear, hdr.beta_shear);
    }

    // Write tile back into full-frame planes
    for (uint32_t r = 0; r < cur_h; ++r) {
      std::memcpy(y_plane.data() + (cur_y + r) * w + cur_x,
                  tile_y.data() + r * cur_w, cur_w * sizeof(int16_t));
      std::memcpy(co_plane.data() + (cur_y + r) * w + cur_x,
                  tile_co.data() + r * cur_w, cur_w * sizeof(int16_t));
      std::memcpy(cg_plane.data() + (cur_y + r) * w + cur_x,
                  tile_cg.data() + r * cur_w, cur_w * sizeof(int16_t));
    }
  };

  size_t num_decode_threads =
      std::max<uint32_t>(1, frame.config.num_worker_threads);
  size_t total_tiles = frame.tile_headers.size();

  if (num_decode_threads > 1 && total_tiles > 1) {
    size_t actual_threads = std::min(num_decode_threads, total_tiles);
    std::vector<std::future<void>> futures;
    futures.reserve(actual_threads);
    size_t chunk_size = (total_tiles + actual_threads - 1) / actual_threads;
    for (size_t t = 0; t < actual_threads; ++t) {
      size_t start = t * chunk_size;
      size_t end = std::min(start + chunk_size, total_tiles);
      if (start < end) {
        futures.push_back(
            std::async(std::launch::async, [&decompress_tile, start, end]() {
              for (size_t i = start; i < end; ++i) {
                decompress_tile(i);
              }
            }));
      }
    }
    for (auto &f : futures) {
      f.get();
    }
  } else {
    for (size_t i = 0; i < total_tiles; ++i) {
      decompress_tile(i);
    }
  }

  // Invert YCoCg-R to RGB
  const size_t total_pixels = static_cast<size_t>(w) * h;
  std::vector<uint8_t> rgb_out(total_pixels * 3);
  auto convert_ycocg_chunk = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      YCoCgPixel ycocg{y_plane[i], co_plane[i], cg_plane[i]};
      RgbPixel rgb = PscEngine::ycocgr_to_rgb(ycocg);
      rgb_out[i * 3 + 0] = rgb.r;
      rgb_out[i * 3 + 1] = rgb.g;
      rgb_out[i * 3 + 2] = rgb.b;
    }
  };

  if (num_decode_threads > 1 && total_pixels >= 65536) {
    std::vector<std::future<void>> futures;
    size_t chunk = (total_pixels + num_decode_threads - 1) / num_decode_threads;
    for (size_t t = 0; t < num_decode_threads; ++t) {
      size_t start = t * chunk;
      size_t end = std::min(start + chunk, total_pixels);
      if (start < end) {
        futures.push_back(
            std::async(std::launch::async, convert_ycocg_chunk, start, end));
      }
    }
    for (auto &f : futures) {
      f.get();
    }
  } else {
    convert_ycocg_chunk(0, total_pixels);
  }

  // Cryptographic parity assertion
  Sha256Digest recon_sha =
      BbrEngine::compute_sha256(rgb_out.data(), rgb_out.size());
  if (recon_sha != frame.original_sha256) {
    throw std::runtime_error(
        "PSC 2.0 SHA-256 parity mismatch! Bit-exact PSNR=inf violated.");
  }

  return rgb_out;
}

Psc2Engine::CompressedPsc2Frame
Psc2Engine::compress_quad_bayer(const uint16_t *bayer_raw, uint32_t width,
                                uint32_t height, uint8_t bit_depth) {
  if (!bayer_raw)
    throw std::invalid_argument("Input Bayer raw buffer is null");
  validate_dimensions(width, height);

  CompressedPsc2Frame frame;
  frame.config = config_;
  frame.config.width = width;
  frame.config.height = height;
  frame.config.bit_depth = bit_depth;
  frame.is_quad_bayer = true;

  size_t raw_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
  frame.original_sha256 = BbrEngine::compute_sha256(
      reinterpret_cast<const uint8_t *>(bayer_raw), raw_bytes);

  // Splice Bayer into 4 sub-planes: R, G1, G2, B
  auto cfa_planes =
      PscEngine::splice_bayer_cfa(bayer_raw, width, height, bit_depth);

  QuadBayerPlanes planes;
  planes.sub_w = cfa_planes.sub_w;
  planes.sub_h = cfa_planes.sub_h;
  planes.bit_depth = bit_depth;
  planes.r = std::move(cfa_planes.r);
  planes.g1 = std::move(cfa_planes.g1);
  planes.g2 = std::move(cfa_planes.g2);
  planes.b = std::move(cfa_planes.b);

  int8_t ar = 0, ab = 0, brb = 0;
  if (config_.enable_quad_bayer_coupling) {
    std::tie(ar, ab, brb) = estimate_quad_bayer_shears(planes);
    forward_quad_bayer_aiklt(planes, ar, ab, brb);
  }

  // Residual symbols via MED
  std::vector<uint16_t> symbols;
  symbols.reserve(static_cast<size_t>(width) * height * 2);

  auto encode_subplane = [&](const std::vector<uint16_t> &p) {
    for (uint32_t y = 0; y < planes.sub_h; ++y) {
      for (uint32_t x = 0; x < planes.sub_w; ++x) {
        int16_t left =
            (x > 0) ? static_cast<int16_t>(p[y * planes.sub_w + (x - 1)]) : 0;
        int16_t top =
            (y > 0) ? static_cast<int16_t>(p[(y - 1) * planes.sub_w + x]) : 0;
        int16_t tl =
            (x > 0 && y > 0)
                ? static_cast<int16_t>(p[(y - 1) * planes.sub_w + (x - 1)])
                : 0;
        int16_t pred = PscEngine::med_predict(left, top, tl);
        int16_t val = static_cast<int16_t>(p[y * planes.sub_w + x]);
        int16_t diff = val - pred;

        uint16_t uval = (diff >= 0) ? static_cast<uint16_t>(diff * 2)
                                    : static_cast<uint16_t>(-diff * 2 - 1);
        symbols.push_back(uval & 0xFF);
        symbols.push_back((uval >> 8) & 0xFF);
      }
    }
  };

  encode_subplane(planes.r);
  encode_subplane(planes.g1);
  encode_subplane(planes.g2);
  encode_subplane(planes.b);

  std::vector<RansSymbol> stats = compute_byte_stats(symbols);
  Rans8Codec::EncodedChunk rchunk = Rans8Codec::encode(symbols, stats);
  std::vector<uint8_t> serialized_chunk = Rans8Codec::serialize(rchunk);

  size_t raw_bayer_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
  size_t comp_size = 256 * sizeof(uint16_t) + serialized_chunk.size();

  Psc2TileHeader hdr;
  hdr.tile_w = static_cast<uint16_t>(width);
  hdr.tile_h = static_cast<uint16_t>(height);
  hdr.alpha_shear = ar;
  hdr.beta_shear = ab;
  hdr.dominant_orientation = static_cast<uint8_t>(brb);

  if (comp_size < raw_bayer_bytes) {
    hdr.tile_codec_mode = 1; // Empirical rANS
    frame.bitstream_payload.resize(comp_size);
    uint8_t *fptr = frame.bitstream_payload.data();
    for (size_t s = 0; s < 256; ++s) {
      uint16_t freq = stats[s].freq;
      std::memcpy(fptr, &freq, sizeof(uint16_t));
      fptr += sizeof(uint16_t);
    }
    std::memcpy(fptr, serialized_chunk.data(), serialized_chunk.size());
  } else {
    // Zero-expansion bypass: raw Bayer samples
    hdr.tile_codec_mode = 2; // Raw Bayer bypass
    frame.bitstream_payload.resize(raw_bayer_bytes);
    std::memcpy(frame.bitstream_payload.data(), bayer_raw, raw_bayer_bytes);
  }

  hdr.payload_byte_size = static_cast<uint32_t>(frame.bitstream_payload.size());
  frame.tile_headers.push_back(hdr);

  return frame;
}

std::vector<uint16_t>
Psc2Engine::decompress_quad_bayer(const CompressedPsc2Frame &frame) {
  uint32_t width = frame.config.width;
  uint32_t height = frame.config.height;
  validate_dimensions(width, height);

  if (frame.tile_headers.empty() || frame.bitstream_payload.empty()) {
    throw std::runtime_error("Compressed Quad-Bayer frame has empty payload");
  }

  const auto &hdr = frame.tile_headers[0];
  if (hdr.tile_codec_mode == 2) {
    // Mode 2: Raw Bayer bypass
    size_t expected_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
    if (frame.bitstream_payload.size() < expected_bytes) {
      throw std::runtime_error("Corrupted Quad-Bayer raw bypass payload");
    }
    std::vector<uint16_t> out(static_cast<size_t>(width) * height);
    std::memcpy(out.data(), frame.bitstream_payload.data(), expected_bytes);
    return out;
  }

  int8_t ar = hdr.alpha_shear;
  int8_t ab = hdr.beta_shear;
  int8_t brb = static_cast<int8_t>(hdr.dominant_orientation);

  std::vector<RansSymbol> stats(256);
  const uint8_t *rchunk_ptr = frame.bitstream_payload.data();
  size_t rchunk_size = frame.bitstream_payload.size();

  if (hdr.tile_codec_mode == 1) {
    if (frame.bitstream_payload.size() < 256 * sizeof(uint16_t)) {
      throw std::runtime_error("Corrupted Quad-Bayer empirical payload");
    }
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
      uint16_t freq = 0;
      std::memcpy(&freq, rchunk_ptr, sizeof(uint16_t));
      rchunk_ptr += sizeof(uint16_t);
      stats[s].start = running;
      stats[s].freq = freq;
      running += freq;
    }
    rchunk_size -= 256 * sizeof(uint16_t);
  } else {
    // Mode 0: Uniform fallback
    uint16_t running = 0;
    for (size_t s = 0; s < 256; ++s) {
      stats[s].start = running;
      stats[s].freq = 128;
      running += 128;
    }
  }

  std::vector<uint16_t> cdf_table = Rans8Codec::build_cdf_lut(stats);
  Rans8Codec::EncodedChunk rchunk =
      Rans8Codec::deserialize(rchunk_ptr, rchunk_size);
  std::vector<uint16_t> symbols = Rans8Codec::decode(rchunk, cdf_table, stats);

  QuadBayerPlanes planes;
  planes.sub_w = width / 2;
  planes.sub_h = height / 2;
  planes.bit_depth = frame.config.bit_depth;
  size_t count = static_cast<size_t>(planes.sub_w) * planes.sub_h;

  size_t sym_idx = 0;
  auto decode_uval = [&]() -> uint16_t {
    if (sym_idx + 1 >= symbols.size())
      return 0;
    uint16_t low = symbols[sym_idx++];
    uint16_t high = symbols[sym_idx++];
    return (high << 8) | low;
  };

  auto decode_subplane = [&](std::vector<uint16_t> &p) {
    p.resize(count);
    for (uint32_t y = 0; y < planes.sub_h; ++y) {
      for (uint32_t x = 0; x < planes.sub_w; ++x) {
        int16_t left =
            (x > 0) ? static_cast<int16_t>(p[y * planes.sub_w + (x - 1)]) : 0;
        int16_t top =
            (y > 0) ? static_cast<int16_t>(p[(y - 1) * planes.sub_w + x]) : 0;
        int16_t tl =
            (x > 0 && y > 0)
                ? static_cast<int16_t>(p[(y - 1) * planes.sub_w + (x - 1)])
                : 0;
        int16_t pred = PscEngine::med_predict(left, top, tl);

        uint16_t uval = decode_uval();
        int16_t diff = (uval % 2 == 0) ? static_cast<int16_t>(uval / 2)
                                       : -static_cast<int16_t>((uval + 1) / 2);
        p[y * planes.sub_w + x] = static_cast<uint16_t>(pred + diff);
      }
    }
  };

  decode_subplane(planes.r);
  decode_subplane(planes.g1);
  decode_subplane(planes.g2);
  decode_subplane(planes.b);

  if (frame.config.enable_quad_bayer_coupling) {
    inverse_quad_bayer_aiklt(planes, ar, ab, brb);
  }

  PscEngine::BayerCfaSubplanes cfa_planes;
  cfa_planes.sub_w = planes.sub_w;
  cfa_planes.sub_h = planes.sub_h;
  cfa_planes.bit_depth = planes.bit_depth;
  cfa_planes.r = std::move(planes.r);
  cfa_planes.g1 = std::move(planes.g1);
  cfa_planes.g2 = std::move(planes.g2);
  cfa_planes.b = std::move(planes.b);

  std::vector<uint16_t> bayer_out(static_cast<size_t>(width) * height);
  PscEngine::merge_bayer_cfa(cfa_planes, bayer_out.data(), width, height);

  // Parity verification
  size_t raw_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
  Sha256Digest recon_sha = BbrEngine::compute_sha256(
      reinterpret_cast<const uint8_t *>(bayer_out.data()), raw_bytes);
  if (recon_sha != frame.original_sha256) {
    throw std::runtime_error(
        "Quad-Bayer SHA-256 parity mismatch! Bit-exact PSNR=inf violated.");
  }

  return bayer_out;
}

bool Psc2Engine::verify_pixel_parity(const uint8_t *a, const uint8_t *b,
                                     size_t count) {
  if (!a || !b)
    return false;
  return std::memcmp(a, b, count) == 0;
}

bool Psc2Engine::verify_sample_parity_u16(const uint16_t *a, const uint16_t *b,
                                          size_t count) {
  if (!a || !b)
    return false;
  return std::memcmp(a, b, count * sizeof(uint16_t)) == 0;
}

std::vector<uint8_t>
Psc2Engine::serialize_frame(const CompressedPsc2Frame &frame) {
  uint8_t flags = 0;
  if (frame.is_quad_bayer)
    flags |= 0x01;
  if (frame.config.enable_quad_bayer_coupling)
    flags |= 0x02;
  if (frame.config.enable_directional_wavelet)
    flags |= 0x04;
  if (frame.config.enable_spectral_coupling)
    flags |= 0x08;
  if (frame.config.enable_checkerboard_convnet)
    flags |= 0x10;

  uint32_t num_tiles = static_cast<uint32_t>(frame.tile_headers.size());
  uint32_t payload_len = static_cast<uint32_t>(frame.bitstream_payload.size());

  size_t total_header_len = 56 + static_cast<size_t>(num_tiles) * 20 + 4;
  std::vector<uint8_t> buffer(total_header_len + payload_len);

  uint8_t *ptr = buffer.data();
  uint32_t magic = BOX_TYPE_PSC2;
  std::memcpy(ptr, &magic, 4);
  ptr += 4;
  *ptr++ = flags;
  *ptr++ = static_cast<uint8_t>(frame.config.filter);
  *ptr++ = frame.config.bit_depth;
  *ptr++ = 0; // reserved
  std::memcpy(ptr, &frame.config.tile_size, 2);
  ptr += 2;
  *ptr++ = 0;
  *ptr++ = 0; // reserved
  std::memcpy(ptr, &frame.config.width, 4);
  ptr += 4;
  std::memcpy(ptr, &frame.config.height, 4);
  ptr += 4;
  std::memcpy(ptr, frame.original_sha256.data(), 32);
  ptr += 32;
  std::memcpy(ptr, &num_tiles, 4);
  ptr += 4;

  for (const auto &th : frame.tile_headers) {
    std::memcpy(ptr, &th.tile_x, 2);
    ptr += 2;
    std::memcpy(ptr, &th.tile_y, 2);
    ptr += 2;
    std::memcpy(ptr, &th.tile_w, 2);
    ptr += 2;
    std::memcpy(ptr, &th.tile_h, 2);
    ptr += 2;
    *ptr++ = static_cast<uint8_t>(th.alpha_shear);
    *ptr++ = static_cast<uint8_t>(th.beta_shear);
    *ptr++ = th.dominant_orientation;
    *ptr++ = th.tile_codec_mode;
    std::memcpy(ptr, &th.payload_byte_offset, 4);
    ptr += 4;
    std::memcpy(ptr, &th.payload_byte_size, 4);
    ptr += 4;
  }

  std::memcpy(ptr, &payload_len, 4);
  ptr += 4;
  if (payload_len > 0) {
    std::memcpy(ptr, frame.bitstream_payload.data(), payload_len);
  }

  return buffer;
}

Psc2Engine::CompressedPsc2Frame
Psc2Engine::deserialize_frame(const uint8_t *data, size_t size) {
  if (!data || size < 60) {
    throw std::runtime_error("Invalid or truncated PSC 2.0 frame payload");
  }

  const uint8_t *ptr = data;
  uint32_t magic = 0;
  std::memcpy(&magic, ptr, 4);
  ptr += 4;
  if (magic != BOX_TYPE_PSC2) {
    throw std::runtime_error("Invalid PSC 2.0 box magic: expected 'psc2'");
  }

  CompressedPsc2Frame frame;
  uint8_t flags = *ptr++;
  frame.is_quad_bayer = (flags & 0x01) != 0;
  frame.config.enable_quad_bayer_coupling = (flags & 0x02) != 0;
  frame.config.enable_directional_wavelet = (flags & 0x04) != 0;
  frame.config.enable_spectral_coupling = (flags & 0x08) != 0;
  frame.config.enable_checkerboard_convnet = (flags & 0x10) != 0;

  frame.config.filter = static_cast<WaveletFilterType>(*ptr++);
  frame.config.bit_depth = *ptr++;
  ptr++; // reserved

  std::memcpy(&frame.config.tile_size, ptr, 2);
  ptr += 2;
  ptr += 2; // reserved
  std::memcpy(&frame.config.width, ptr, 4);
  ptr += 4;
  std::memcpy(&frame.config.height, ptr, 4);
  ptr += 4;
  std::memcpy(frame.original_sha256.data(), ptr, 32);
  ptr += 32;

  uint32_t num_tiles = 0;
  std::memcpy(&num_tiles, ptr, 4);
  ptr += 4;

  size_t header_consumed = static_cast<size_t>(ptr - data);
  if (header_consumed + static_cast<size_t>(num_tiles) * 20 + 4 > size) {
    throw std::runtime_error("Corrupted PSC 2.0 tile header table");
  }

  frame.tile_headers.resize(num_tiles);
  for (uint32_t i = 0; i < num_tiles; ++i) {
    auto &th = frame.tile_headers[i];
    std::memcpy(&th.tile_x, ptr, 2);
    ptr += 2;
    std::memcpy(&th.tile_y, ptr, 2);
    ptr += 2;
    std::memcpy(&th.tile_w, ptr, 2);
    ptr += 2;
    std::memcpy(&th.tile_h, ptr, 2);
    ptr += 2;
    th.alpha_shear = static_cast<int8_t>(*ptr++);
    th.beta_shear = static_cast<int8_t>(*ptr++);
    th.dominant_orientation = *ptr++;
    th.tile_codec_mode = *ptr++;
    std::memcpy(&th.payload_byte_offset, ptr, 4);
    ptr += 4;
    std::memcpy(&th.payload_byte_size, ptr, 4);
    ptr += 4;
  }

  uint32_t payload_len = 0;
  std::memcpy(&payload_len, ptr, 4);
  ptr += 4;

  size_t remaining = size - static_cast<size_t>(ptr - data);
  if (payload_len > remaining) {
    throw std::runtime_error("Truncated PSC 2.0 bitstream payload");
  }

  frame.bitstream_payload.resize(payload_len);
  if (payload_len > 0) {
    std::memcpy(frame.bitstream_payload.data(), ptr, payload_len);
  }

  return frame;
}

std::vector<uint8_t>
Psc2Engine::compress_chunk_payload(const uint8_t *raw_data, size_t raw_size,
                                   const Psc2Config &config,
                                   bool is_quad_bayer) {
  if (!raw_data || raw_size == 0)
    return {};

  Psc2Config cfg = config;
  if (is_quad_bayer) {
    uint32_t w = cfg.width;
    uint32_t h = cfg.height;
    if (w == 0 || h == 0 ||
        static_cast<size_t>(w) * h * sizeof(uint16_t) != raw_size) {
      size_t num_samples = raw_size / sizeof(uint16_t);
      w = static_cast<uint32_t>(std::sqrt(num_samples));
      w = std::max(2u, (w / 2) * 2);
      h = static_cast<uint32_t>(num_samples / w);
      h = std::max(2u, (h / 2) * 2);
    }
    cfg.width = w;
    cfg.height = h;
    Psc2Engine engine(cfg);
    const uint16_t *u16_ptr = reinterpret_cast<const uint16_t *>(raw_data);
    auto comp = engine.compress_quad_bayer(u16_ptr, w, h, cfg.bit_depth);
    return serialize_frame(comp);
  } else {
    uint32_t w = cfg.width;
    uint32_t h = cfg.height;
    if (w == 0 || h == 0 || static_cast<size_t>(w) * h * 3 != raw_size) {
      size_t num_pixels = raw_size / 3;
      w = static_cast<uint32_t>(std::sqrt(num_pixels));
      w = std::max(2u, (w / 2) * 2);
      h = static_cast<uint32_t>(num_pixels / w);
      h = std::max(2u, (h / 2) * 2);
    }
    cfg.width = w;
    cfg.height = h;
    Psc2Engine engine(cfg);
    auto comp =
        engine.compress_frame_rgb(raw_data, static_cast<size_t>(w) * h * 3);
    return serialize_frame(comp);
  }
}

std::vector<uint8_t>
Psc2Engine::decompress_chunk_payload(const uint8_t *comp_data, size_t comp_size,
                                     size_t uncomp_size) {
  if (!comp_data || comp_size == 0)
    return {};

  CompressedPsc2Frame frame = deserialize_frame(comp_data, comp_size);
  Psc2Engine engine(frame.config);

  if (frame.is_quad_bayer) {
    std::vector<uint16_t> samples = engine.decompress_quad_bayer(frame);
    size_t byte_count = samples.size() * sizeof(uint16_t);
    std::vector<uint8_t> result(byte_count);
    std::memcpy(result.data(), samples.data(), byte_count);
    if (uncomp_size > 0 && result.size() > uncomp_size) {
      result.resize(uncomp_size);
    }
    return result;
  } else {
    std::vector<uint8_t> rgb = engine.decompress_frame_rgb(frame);
    if (uncomp_size > 0 && rgb.size() > uncomp_size) {
      rgb.resize(uncomp_size);
    }
    return rgb;
  }
}

} // namespace prism
