#include "prism/types.hpp"
#include "prism/coolchic.hpp"

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace prism;

namespace {

double psnr(double mse) {
    if (mse <= 0.0) return 0.0;
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

} // namespace

// Synthetic check of the Cool-chic residual blend path.
// The input frame is generated, not a real video; the numbers below are
// properties of this synthetic frame only and are not codec benchmarks.
int main() {
    std::cout << "===============================================================\n";
    std::cout << "  SYNTHETIC RESIDUAL BLEND CHECK (not a video benchmark)      \n";
    std::cout << "===============================================================\n\n";

    constexpr uint32_t W = 1920;
    constexpr uint32_t H = 1080;
    constexpr size_t PIXELS = static_cast<size_t>(W) * H;

    std::vector<uint8_t> clean_frame(PIXELS);
    for (size_t y = 0; y < H; ++y) {
        for (size_t x = 0; x < W; ++x) {
            clean_frame[y * W + x] = static_cast<uint8_t>((x * 255 / W) ^ (y * 255 / H));
        }
    }

    // Coarse quantization stands in for a hardware base layer.
    std::vector<uint8_t> base_frame(PIXELS);
    double base_mse = 0.0;
    for (size_t i = 0; i < PIXELS; ++i) {
        uint8_t orig = clean_frame[i];
        uint8_t base = static_cast<uint8_t>((orig / 24) * 24 + 12);
        base_frame[i] = base;
        double diff = static_cast<double>(orig) - base;
        base_mse += diff * diff;
    }
    base_mse /= static_cast<double>(PIXELS);

    // Apply the actual Cool-chic residual model to the base layer.
    CoolchicResidualModel model;
    const int16_t latents[8] = {10, 20, -10, 5, 0, 15, -5, 8};
    double blended_mse = 0.0;
    size_t changed_pixels = 0;

    for (size_t i = 0; i < PIXELS; ++i) {
        int16_t base_feats[4] = {static_cast<int16_t>(base_frame[i]), 0, 0, 0};
        int16_t residual[3]{};
        uint8_t alpha = 0;
        model.synthesize_pixel(latents, base_feats, residual, alpha);

        uint8_t orig = clean_frame[i];
        uint8_t blended = CoolchicResidualModel::blend_pixel(base_frame[i], orig, residual[0], alpha);
        if (blended != base_frame[i]) ++changed_pixels;

        double diff = static_cast<double>(orig) - blended;
        blended_mse += diff * diff;
    }
    blended_mse /= static_cast<double>(PIXELS);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Synthetic frame: " << W << "x" << H << " (" << PIXELS << " luma samples)\n";
    std::cout << "Base layer PSNR (coarse quantization): " << psnr(base_mse) << " dB\n";
    std::cout << "Blended PSNR (Cool-chic residual):     " << psnr(blended_mse) << " dB\n";
    std::cout << "Pixels changed by blend:               " << changed_pixels
              << " (" << (100.0 * static_cast<double>(changed_pixels) / static_cast<double>(PIXELS))
              << "%)\n";
    std::cout << "This check exercises the model path only; it does not measure VMAF,\n"
                 "BD-rate, or device energy.\n";

    return 0;
}
