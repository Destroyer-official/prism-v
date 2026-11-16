#include "test_harness.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/bbr.hpp"
#include "prism/psc2.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace prism;

PRISM_TEST(test_vfs_open_proxy_serving_and_lru) {
    std::string test_pvloss = "test_vfs.pvloss";
    if (fs::exists(test_pvloss)) fs::remove(test_pvloss);

    BbrEngine bbr;
    PvlossWriter writer;

    // Create a 512 KB uncompressed MP4 pattern
    std::vector<uint8_t> virtual_mp4(512 * 1024);
    for (size_t i = 0; i < virtual_mp4.size(); ++i) {
        virtual_mp4[i] = static_cast<uint8_t>((i * 13) & 0xFF);
    }

    PvlossHeader hdr;
    hdr.lossless_mode = LosslessContract::CONTRACT_A_BBR;
    hdr.orig_file_size = virtual_mp4.size();
    hdr.orig_sha256 = BbrEngine::compute_sha256(virtual_mp4.data(), virtual_mp4.size());

    writer.open(test_pvloss, hdr);

    // Compress chunk 1 (first 256 KB)
    BbrChunk c1 = bbr.compress(virtual_mp4.data(), 256 * 1024, SourceCodecId::CODEC_H264);
    writer.write_chunk(c1.rans_payload.data(), static_cast<uint32_t>(c1.rans_payload.size()), 256 * 1024, 0);

    // Compress chunk 2 (second 256 KB)
    BbrChunk c2 = bbr.compress(virtual_mp4.data() + 256 * 1024, 256 * 1024, SourceCodecId::CODEC_H264);
    writer.write_chunk(c2.rans_payload.data(), static_cast<uint32_t>(c2.rans_payload.size()), 256 * 1024, 256 * 1024);
    writer.close();

    // Now test Prism-VFS proxy
    PrismVfsProxy vfs;
    bool mounted = vfs.mount(test_pvloss);
    PRISM_ASSERT(mounted);
    PRISM_ASSERT_EQ(vfs.on_get_size(), 512 * 1024);

    // Test random-access read spanning the 256 KB boundary
    // Read 1024 bytes starting at offset 262,000 (crosses into Chunk 2 at 262,144)
    std::vector<uint8_t> read_buf(1024);
    int32_t bytes_read = vfs.on_read(262000, 1024, read_buf.data());
    PRISM_ASSERT_EQ(bytes_read, 1024);

    for (size_t i = 0; i < 1024; ++i) {
        PRISM_ASSERT_EQ(read_buf[i], virtual_mp4[262000 + i]);
    }

    // Check cache hit on repeat read
    int32_t repeat_read = vfs.on_read(262000, 512, read_buf.data());
    PRISM_ASSERT_EQ(repeat_read, 512);
    auto stats = vfs.get_stats();
    PRISM_ASSERT(stats.cache_hits > 0);

    vfs.unmount();
    fs::remove(test_pvloss);
}

PRISM_TEST(test_vfs_bbr2_open_proxy_serving) {
    std::string test_pvloss = "test_vfs_bbr2.pvloss";
    if (fs::exists(test_pvloss)) fs::remove(test_pvloss);

    Bbr2Engine bbr2;
    PvlossWriter writer;

    // Create a 512 KB uncompressed stream with realistic sparsity
    std::vector<uint8_t> virtual_mp4(512 * 1024);
    for (size_t i = 0; i < virtual_mp4.size(); ++i) {
        if ((i % 10) < 7) {
            virtual_mp4[i] = 0;
        } else {
            virtual_mp4[i] = static_cast<uint8_t>((i * 7 + 3) & 0x1F);
        }
    }

    PvlossHeader hdr;
    hdr.lossless_mode = LosslessContract::CONTRACT_A_BBR2;
    hdr.orig_file_size = virtual_mp4.size();
    hdr.orig_sha256 = Bbr2Engine::compute_sha256(virtual_mp4.data(), virtual_mp4.size());

    writer.open(test_pvloss, hdr);

    // Compress chunk 1 (first 256 KB) via BBR 2.0
    Bbr2Chunk c1 = bbr2.compress(virtual_mp4.data(), 256 * 1024, SourceCodecId::CODEC_H264);
    std::vector<uint8_t> p1 = Bbr2Engine::serialize_chunk_payload(c1);
    writer.write_chunk(p1.data(), static_cast<uint32_t>(p1.size()), 256 * 1024, 0);

    // Compress chunk 2 (second 256 KB) via BBR 2.0
    Bbr2Chunk c2 = bbr2.compress(virtual_mp4.data() + 256 * 1024, 256 * 1024, SourceCodecId::CODEC_H264);
    std::vector<uint8_t> p2 = Bbr2Engine::serialize_chunk_payload(c2);
    writer.write_chunk(p2.data(), static_cast<uint32_t>(p2.size()), 256 * 1024, 256 * 1024);
    writer.close();

    // Mount virtual MP4 via Prism-VFS proxy
    PrismVfsProxy vfs;
    bool mounted = vfs.mount(test_pvloss);
    PRISM_ASSERT(mounted);
    PRISM_ASSERT_EQ(vfs.on_get_size(), 512 * 1024);

    // Test random-access read spanning the 256 KB boundary
    std::vector<uint8_t> read_buf(2048);
    int32_t bytes_read = vfs.on_read(261500, 2048, read_buf.data());
    PRISM_ASSERT_EQ(bytes_read, 2048);

    for (size_t i = 0; i < 2048; ++i) {
        PRISM_ASSERT_EQ(read_buf[i], virtual_mp4[261500 + i]);
    }

    // Check cache hit on repeat read
    int32_t repeat_read = vfs.on_read(261500, 512, read_buf.data());
    PRISM_ASSERT_EQ(repeat_read, 512);
    auto stats = vfs.get_stats();
    PRISM_ASSERT(stats.cache_hits > 0);

    vfs.unmount();
    fs::remove(test_pvloss);
}

PRISM_TEST(test_vfs_psc2_open_proxy_serving) {
    std::string test_pvloss = "test_vfs_psc2.pvloss";
    if (fs::exists(test_pvloss)) fs::remove(test_pvloss);

    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr size_t frame_bytes = W * H * 3; // 12,288 bytes

    Psc2Config pcfg;
    pcfg.width = W;
    pcfg.height = H;
    pcfg.tile_size = 32;

    // Create a 2-frame stream (24,576 bytes)
    std::vector<uint8_t> virtual_rgb(frame_bytes * 2);
    for (size_t i = 0; i < virtual_rgb.size(); ++i) {
        virtual_rgb[i] = static_cast<uint8_t>((i * 19 + 7) & 0xFF);
    }

    PvlossWriter writer;
    PvlossHeader hdr;
    hdr.lossless_mode = LosslessContract::CONTRACT_C_PSC2;
    hdr.source_codec_id = SourceCodecId::CODEC_BAYER_RAW10;
    hdr.orig_file_size = virtual_rgb.size();
    hdr.orig_sha256 = BbrEngine::compute_sha256(virtual_rgb.data(), virtual_rgb.size());

    writer.open(test_pvloss, hdr);

    // Compress chunk 1 (frame 1)
    std::vector<uint8_t> c1 = Psc2Engine::compress_chunk_payload(virtual_rgb.data(), frame_bytes, pcfg, false);
    writer.write_chunk(c1.data(), static_cast<uint32_t>(c1.size()), static_cast<uint32_t>(frame_bytes), 0);

    // Compress chunk 2 (frame 2)
    std::vector<uint8_t> c2 = Psc2Engine::compress_chunk_payload(virtual_rgb.data() + frame_bytes, frame_bytes, pcfg, false);
    writer.write_chunk(c2.data(), static_cast<uint32_t>(c2.size()), static_cast<uint32_t>(frame_bytes), frame_bytes);
    writer.close();

    // Mount virtual RGB via Prism-VFS proxy
    PrismVfsProxy vfs;
    bool mounted = vfs.mount(test_pvloss);
    PRISM_ASSERT(mounted);
    PRISM_ASSERT_EQ(vfs.on_get_size(), virtual_rgb.size());

    // Read spanning across chunk boundary (offset 12,000 for 1024 bytes -> spans into Chunk 2 at 12,288)
    std::vector<uint8_t> read_buf(1024);
    int32_t bytes_read = vfs.on_read(12000, 1024, read_buf.data());
    PRISM_ASSERT_EQ(bytes_read, 1024);

    for (size_t i = 0; i < 1024; ++i) {
        PRISM_ASSERT_EQ(read_buf[i], virtual_rgb[12000 + i]);
    }

    // Cache hit check
    int32_t repeat_read = vfs.on_read(12000, 512, read_buf.data());
    PRISM_ASSERT_EQ(repeat_read, 512);
    auto stats = vfs.get_stats();
    PRISM_ASSERT(stats.cache_hits > 0);

    vfs.unmount();
    fs::remove(test_pvloss);
}
