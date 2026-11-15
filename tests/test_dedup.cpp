#include "test_harness.hpp"
#include "prism/dedup_rabin.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace prism;

PRISM_TEST(test_dedup_rabin_chunking_and_restoration) {
    std::string test_vid = "test_dedup_video.bin";
    std::string test_restore = "test_restored_video.bin";
    std::string vault_dir = "test_vault_blocks";

    if (fs::exists(test_vid)) fs::remove(test_vid);
    if (fs::exists(test_restore)) fs::remove(test_restore);
    if (fs::exists(vault_dir)) fs::remove_all(vault_dir);

    // Create a 1 MB test stream with repeated patterns
    std::vector<uint8_t> stream(1024 * 1024);
    for (size_t i = 0; i < stream.size(); ++i) {
        stream[i] = static_cast<uint8_t>((i % 256) ^ ((i / 4096) & 0xFF));
    }
    {
        std::ofstream out(test_vid, std::ios::binary);
        out.write(reinterpret_cast<const char*>(stream.data()), stream.size());
    }

    RabinCdcEngine cdc;
    ChunkManifest manifest;
    uint64_t new_bytes = cdc.ingest_video(test_vid, vault_dir, manifest);

    PRISM_ASSERT(new_bytes > 0);
    PRISM_ASSERT(!manifest.chunks.empty());

    // Restore from blocks
    bool ok = cdc.restore_video(manifest, vault_dir, test_restore);
    PRISM_ASSERT(ok);

    std::ifstream restored_in(test_restore, std::ios::binary);
    std::vector<uint8_t> restored((std::istreambuf_iterator<char>(restored_in)),
                                  std::istreambuf_iterator<char>());
    restored_in.close(); // Close file descriptor before removal on Windows
    PRISM_ASSERT_EQ(restored.size(), stream.size());

    for (size_t i = 0; i < stream.size(); ++i) {
        PRISM_ASSERT_EQ(restored[i], stream[i]);
    }

    // Cleanup
    fs::remove(test_vid);
    fs::remove(test_restore);
    fs::remove_all(vault_dir);
}
