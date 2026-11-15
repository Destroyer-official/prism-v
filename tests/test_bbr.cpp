#include "test_harness.hpp"
#include "prism/bbr.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_bbr_bit_exact_sha256_roundtrip) {
    BbrEngine engine;

    // Synthetic H.264/HEVC NAL bitstream pattern
    std::vector<uint8_t> bitstream(8192);
    for (size_t i = 0; i < bitstream.size(); ++i) {
        bitstream[i] = static_cast<uint8_t>((i * 73 + 19) & 0xFF);
    }

    Sha256Digest orig_sha = BbrEngine::compute_sha256(bitstream.data(), bitstream.size());

    // Compress with Mode L-BBR
    BbrChunk chunk = engine.compress(bitstream.data(), bitstream.size(), SourceCodecId::CODEC_H264);
    PRISM_ASSERT_EQ(chunk.orig_sha256, orig_sha);
    PRISM_ASSERT(!chunk.rans_payload.empty());

    // Decompress with Mode L-BBR
    std::vector<uint8_t> restored = engine.decompress(chunk);
    PRISM_ASSERT_EQ(restored.size(), bitstream.size());

    Sha256Digest recon_sha = BbrEngine::compute_sha256(restored.data(), restored.size());
    PRISM_ASSERT_EQ(recon_sha, orig_sha);

    // Byte-by-byte check
    for (size_t i = 0; i < bitstream.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], bitstream[i]);
    }
}

PRISM_TEST(test_bbr_micro_mlp_deterministic) {
    MicroMlp1K mlp;

    std::array<int16_t, MicroMlp1K::INPUT_DIM> ctx1{};
    ctx1.fill(100);

    std::array<RansSymbol, MicroMlp1K::OUTPUT_DIM> syms1{};
    mlp.forward(ctx1, syms1);

    // Verify CDF validity
    uint32_t total_mass = 0;
    for (size_t i = 0; i < MicroMlp1K::OUTPUT_DIM; ++i) {
        PRISM_ASSERT(syms1[i].freq > 0);
        total_mass += syms1[i].freq;
    }
    PRISM_ASSERT_EQ(total_mass, 32768);

    // Cross-run determinism check: second pass must yield bit-exact identical symbols
    std::array<RansSymbol, MicroMlp1K::OUTPUT_DIM> syms2{};
    mlp.forward(ctx1, syms2);

    for (size_t i = 0; i < MicroMlp1K::OUTPUT_DIM; ++i) {
        PRISM_ASSERT_EQ(syms1[i].start, syms2[i].start);
        PRISM_ASSERT_EQ(syms1[i].freq, syms2[i].freq);
    }
}

PRISM_TEST(test_bbr_huffmanpos_buckets) {
    // 8x8 block position indexing
    uint8_t b_dc = HuffmanPos::get_position_bucket(0, 0, 8);
    uint8_t b_mid = HuffmanPos::get_position_bucket(3, 4, 8);
    uint8_t b_hf = HuffmanPos::get_position_bucket(7, 7, 8);

    PRISM_ASSERT_EQ(b_dc, 0);
    PRISM_ASSERT(b_mid > 0 && b_mid < 15);
    PRISM_ASSERT_EQ(b_hf, 15);
}
