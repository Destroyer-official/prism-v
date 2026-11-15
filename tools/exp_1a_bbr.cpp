#include "prism/types.hpp"
#include "prism/bbr.hpp"
#include "prism/rans8.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace prism;

// Synthetic check of the BBR (bitstream recompression) round-trip.
// The GOP payloads are generated pseudo-random bytes, so the compression
// ratio below describes this synthetic input only and is not a benchmark of
// real H.264/HEVC camera bitstreams.
int main() {
    std::cout << "===============================================================\n";
    std::cout << "  SYNTHETIC BBR ROUND-TRIP CHECK (not a video benchmark)      \n";
    std::cout << "===============================================================\n\n";

    constexpr size_t GOP_COUNT = 10;
    constexpr size_t BYTES_PER_GOP = 65536;

    BbrEngine engine;
    uint64_t total_uncompressed_bytes = 0;
    uint64_t total_compressed_bytes = 0;
    bool all_bit_exact = true;

    std::mt19937 rng(1337);

    for (size_t g = 0; g < GOP_COUNT; ++g) {
        // Synthetic byte distribution: mostly small values, some full-range noise.
        std::vector<uint8_t> bitstream(BYTES_PER_GOP);
        for (size_t i = 0; i < BYTES_PER_GOP; ++i) {
            uint32_t r = rng() % 100;
            if (r < 65) {
                bitstream[i] = 0;
            } else if (r < 85) {
                bitstream[i] = static_cast<uint8_t>((rng() % 3) + 1);
            } else {
                bitstream[i] = static_cast<uint8_t>(rng() % 256);
            }
        }

        Sha256Digest orig_sha = BbrEngine::compute_sha256(bitstream.data(), bitstream.size());
        BbrChunk chunk = engine.compress(bitstream.data(), bitstream.size(), SourceCodecId::CODEC_H264);
        std::vector<uint8_t> restored = engine.decompress(chunk);
        Sha256Digest recon_sha = BbrEngine::compute_sha256(restored.data(), restored.size());

        bool exact = (orig_sha == recon_sha && restored == bitstream);
        all_bit_exact = all_bit_exact && exact;

        total_uncompressed_bytes += bitstream.size();
        total_compressed_bytes += chunk.rans_payload.size();

        double gop_ratio = 100.0 * (1.0 - static_cast<double>(chunk.rans_payload.size()) /
                                           static_cast<double>(bitstream.size()));
        std::cout << "  GOP #" << std::setw(2) << (g + 1) << ": input " << bitstream.size()
                  << " B | payload " << chunk.rans_payload.size() << " B | saved "
                  << std::fixed << std::setprecision(2) << gop_ratio
                  << "% | SHA-256 match: " << (exact ? "YES" : "NO") << "\n";
    }

    double overall_ratio = 100.0 * (1.0 - static_cast<double>(total_compressed_bytes) /
                                              static_cast<double>(total_uncompressed_bytes));

    std::cout << "\n---------------------------------------------------------------\n";
    std::cout << "Synthetic payload reduction: " << std::fixed << std::setprecision(2)
              << overall_ratio << "%\n";
    std::cout << "Round-trip SHA-256 match:    " << (all_bit_exact ? "YES" : "NO") << "\n";
    std::cout << "---------------------------------------------------------------\n";

    return all_bit_exact ? 0 : 1;
}
