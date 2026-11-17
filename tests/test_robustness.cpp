#include "test_harness.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/coolchic.hpp"
#include "prism/psc.hpp"
#include "prism/rans8.hpp"
#include "prism/remux.hpp"
#include "prism/isobmff.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

namespace fs = std::filesystem;
using namespace prism;

namespace {

bool throws_runtime(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

std::vector<RansSymbol> uniform_stats(size_t alphabet) {
    std::vector<RansSymbol> stats(alphabet);
    uint32_t running = 0;
    const uint32_t freq = static_cast<uint32_t>(Rans8Codec::RANS_M / alphabet);
    for (size_t i = 0; i < alphabet; ++i) {
        stats[i].start = static_cast<uint16_t>(running);
        stats[i].freq = static_cast<uint16_t>((i + 1 == alphabet)
                                                  ? (Rans8Codec::RANS_M - running)
                                                  : freq);
        running += stats[i].freq;
    }
    return stats;
}

} // namespace

PRISM_TEST(test_rans8_single_symbol_full_frequency) {
    std::vector<RansSymbol> stats(1);
    stats[0].start = 0;
    stats[0].freq = static_cast<uint16_t>(Rans8Codec::RANS_M);

    std::vector<uint16_t> original(1000, 0);
    auto chunk = Rans8Codec::encode(original, stats);
    auto cdf = Rans8Codec::build_cdf_lut(stats);
    auto decoded = Rans8Codec::decode(chunk, cdf, stats);

    PRISM_ASSERT_EQ(decoded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        PRISM_ASSERT_EQ(decoded[i], original[i]);
    }
}

PRISM_TEST(test_rans8_rejects_zero_frequency_stats) {
    std::vector<RansSymbol> stats(2);
    stats[0].start = 0;
    stats[0].freq = 0;
    stats[1].start = 0;
    stats[1].freq = static_cast<uint16_t>(Rans8Codec::RANS_M);

    PRISM_ASSERT(throws_runtime([&]() {
        Rans8Codec::encode(std::vector<uint16_t>(16, 0), stats);
    }));
}

PRISM_TEST(test_rans8_rejects_truncated_stream) {
    auto stats = uniform_stats(16);
    std::vector<uint16_t> original(4096);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint16_t>(i % 16);
    }

    auto chunk = Rans8Codec::encode(original, stats);
    PRISM_ASSERT(chunk.byte_stream.size() > 40);
    chunk.byte_stream.resize(chunk.byte_stream.size() - 2);

    PRISM_ASSERT(throws_runtime([&]() {
        auto cdf = Rans8Codec::build_cdf_lut(stats);
        Rans8Codec::decode(chunk, cdf, stats);
    }));
}

PRISM_TEST(test_psc_rejects_odd_dimensions) {
    PscEngine engine;
    PscEngine::FrameGeometry geom;
    geom.width = 63;
    geom.height = 64;
    geom.bit_depth = 8;

    std::vector<uint8_t> frame(geom.width * geom.height * 3, 0);
    PRISM_ASSERT(throws_runtime([&]() {
        engine.compress_frame(frame.data(), geom);
    }));
}

PRISM_TEST(test_psc_minimal_frame_roundtrip) {
    PscEngine engine;
    PscEngine::FrameGeometry geom;
    geom.width = 2;
    geom.height = 2;
    geom.bit_depth = 8;

    std::vector<uint8_t> frame = {
        1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12
    };

    auto compressed = engine.compress_frame(frame.data(), geom);
    auto restored = engine.decompress_frame(compressed);
    PRISM_ASSERT_EQ(restored.size(), frame.size());
    for (size_t i = 0; i < frame.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], frame[i]);
    }
}

PRISM_TEST(test_psc_rejects_geometry_mismatch_on_decode) {
    PscEngine engine;
    PscEngine::FrameGeometry geom;
    geom.width = 8;
    geom.height = 8;
    geom.bit_depth = 8;

    std::vector<uint8_t> frame(geom.width * geom.height * 3, 7);
    auto compressed = engine.compress_frame(frame.data(), geom);
    compressed.geometry.width = 16;

    PRISM_ASSERT(throws_runtime([&]() {
        engine.decompress_frame(compressed);
    }));
}

PRISM_TEST(test_psc_rejects_invalid_bayer_depth) {
    std::vector<uint16_t> bayer(16 * 16, 0);
    PRISM_ASSERT(throws_runtime([&]() {
        PscEngine::splice_bayer_cfa(bayer.data(), 16, 16, 0);
    }));
}

PRISM_TEST(test_coolchic_probe_handles_small_dimensions) {
    ParetoTarget target;
    std::vector<uint8_t> frame(300 * 200, 128);

    auto exact_tile = ParetoDialSolver::run_preflight_probe(frame.data(), 256, 256, target);
    PRISM_ASSERT(exact_tile.predicted_vmaf > 0.0f);
    PRISM_ASSERT(exact_tile.predicted_vmaf <= 100.0f);

    auto wide = ParetoDialSolver::run_preflight_probe(frame.data(), 300, 200, target);
    PRISM_ASSERT(wide.predicted_vmaf > 0.0f);

    auto tiny = ParetoDialSolver::run_preflight_probe(frame.data(), 8, 8, target);
    PRISM_ASSERT(tiny.predicted_vmaf > 0.0f);

    auto null_frame = ParetoDialSolver::run_preflight_probe(nullptr, 1920, 1080, target);
    PRISM_ASSERT(null_frame.predicted_vmaf > 0.0f);
}

PRISM_TEST(test_container_rejects_short_and_bad_index) {
    const std::string path = "test_malformed.pvloss";
    if (fs::exists(path)) fs::remove(path);

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("PRISMV01", 8);
    }

    PvlossReader reader;
    PRISM_ASSERT(!reader.open(path));

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        std::vector<uint8_t> header(0x48, 0);
        std::copy(PVLOSS_MAGIC.begin(), PVLOSS_MAGIC.end(), header.begin());
        uint64_t bad_offset = 0x1000;
        std::memcpy(header.data() + 0x40, &bad_offset, sizeof(bad_offset));
        out.write(reinterpret_cast<const char*>(header.data()), header.size());
    }

    PRISM_ASSERT(!reader.open(path));
    fs::remove(path);
}

PRISM_TEST(test_container_rejects_truncated_chunk) {
    const std::string path = "test_truncated.pvloss";
    if (fs::exists(path)) fs::remove(path);

    PvlossHeader header;
    header.orig_file_size = 8;
    PvlossWriter writer;
    writer.open(path, header);
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};
    writer.write_chunk(payload.data(), static_cast<uint32_t>(payload.size()), 8, 0);
    writer.close();

    fs::resize_file(path, fs::file_size(path) - 4);

    PvlossReader reader;
    PRISM_ASSERT(!reader.open(path));
    fs::remove(path);
}

PRISM_TEST(test_remux_rejects_truncated_and_trailing_data) {
    RemuxEngine::RemuxPackage pkg;
    RemuxEngine::RemuxPacket packet;
    packet.type = RemuxEngine::MediaType::VIDEO_NAL;
    packet.payload = {0x00, 0x00, 0x00, 0x01, 0x67};
    pkg.packets.push_back(packet);

    std::vector<uint8_t> stream = RemuxEngine::remux_to_clean_isobmff(pkg);
    PRISM_ASSERT(!stream.empty());

    RemuxEngine::RemuxPackage parsed;
    PRISM_ASSERT(RemuxEngine::parse_and_verify_isobmff(stream.data(), stream.size(), parsed));

    std::vector<uint8_t> truncated(stream.begin(), stream.end() - 1);
    PRISM_ASSERT(!RemuxEngine::parse_and_verify_isobmff(truncated.data(), truncated.size(), parsed));

    std::vector<uint8_t> trailing = stream;
    trailing.push_back(0);
    PRISM_ASSERT(!RemuxEngine::parse_and_verify_isobmff(trailing.data(), trailing.size(), parsed));

    PRISM_ASSERT(!RemuxEngine::parse_and_verify_isobmff(nullptr, 0, parsed));
}

PRISM_TEST(test_native_container_parser_corrupted_headers_resilience) {
    VideoMetadata meta;

    // 1. Null / Empty data
    PRISM_ASSERT(!NativeContainerParser::parse_bytes(nullptr, 0, meta));
    PRISM_ASSERT(!NativeContainerParser::parse_bytes(nullptr, 100, meta));

    // 2. Truncated EBML Header (<12 bytes)
    std::vector<uint8_t> short_ebml = {0x1A, 0x45, 0xDF, 0xA3, 0x85};
    PRISM_ASSERT(!NativeContainerParser::parse_bytes(short_ebml.data(), short_ebml.size(), meta));

    // 3. Corrupted EBML VINT with infinite size or out-of-bounds offsets
    std::vector<uint8_t> corrupt_vint = {
        0x1A, 0x45, 0xDF, 0xA3, 0x85, 0x42, 0x82, 0x81, 'm',
        0x18, 0x53, 0x80, 0x67, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    // Must handle gracefully without crash
    NativeContainerParser::parse_bytes(corrupt_vint.data(), corrupt_vint.size(), meta);

    // 4. Corrupted ISOBMFF Box Size (> total size)
    std::vector<uint8_t> corrupt_isobmff = {
        0x00, 0x00, 0xFF, 0x00, 'f', 't', 'y', 'p',
        'm', 'p', '4', '2', 0x00, 0x00, 0x00, 0x00
    };
    PRISM_ASSERT(!NativeContainerParser::parse_bytes(corrupt_isobmff.data(), corrupt_isobmff.size(), meta));

    // 5. Non-existent file and truncated tail recovery
    uint64_t dur_non_exist = NativeContainerParser::recover_mkv_duration_from_tail("non_existent_file_xyz.mkv");
    PRISM_ASSERT_EQ(dur_non_exist, 0);

    const std::string dummy_file = "test_truncated_tail.mkv";
    {
        std::ofstream out(dummy_file, std::ios::binary);
        std::vector<uint8_t> garbage = {0x1A, 0x45, 0xDF, 0xA3, 0x99, 0x88, 0x77};
        out.write(reinterpret_cast<const char*>(garbage.data()), garbage.size());
    }
    uint64_t dur_truncated = NativeContainerParser::recover_mkv_duration_from_tail(dummy_file);
    PRISM_ASSERT_EQ(dur_truncated, 0);
    std::filesystem::remove(dummy_file);
}
