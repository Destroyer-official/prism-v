#include "test_harness.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/bbr.hpp"
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
