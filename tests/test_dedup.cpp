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

PRISM_TEST(test_dedup_rabin_sub_chunk_and_empty_stream) {
    RabinCdcEngine cdc;
    auto empty_chunks = cdc.partition_stream(nullptr, 0);
    PRISM_ASSERT(empty_chunks.empty());

    // Data smaller than MIN_CHUNK_SIZE (e.g. 64 KB < 128 KB) should form exactly 1 chunk
    std::vector<uint8_t> small_stream(64 * 1024, 0x42);
    auto small_chunks = cdc.partition_stream(small_stream.data(), small_stream.size());
    PRISM_ASSERT_EQ(small_chunks.size(), 1u);
    PRISM_ASSERT_EQ(small_chunks[0].length, 64 * 1024u);
}

PRISM_TEST(test_dedup_rabin_idempotent_duplicate_ingest) {
    std::string test_vid = "test_dedup_idempotent.bin";
    std::string vault_dir = "test_vault_idempotent";

    if (fs::exists(test_vid)) fs::remove(test_vid);
    if (fs::exists(vault_dir)) fs::remove_all(vault_dir);

    // 512 KB payload
    std::vector<uint8_t> stream(512 * 1024);
    for (size_t i = 0; i < stream.size(); ++i) {
        stream[i] = static_cast<uint8_t>((i * 13) % 256);
    }
    {
        std::ofstream out(test_vid, std::ios::binary);
        out.write(reinterpret_cast<const char*>(stream.data()), stream.size());
    }

    RabinCdcEngine cdc;
    ChunkManifest m1;
    uint64_t bytes1 = cdc.ingest_video(test_vid, vault_dir, m1);
    PRISM_ASSERT(bytes1 > 0);

    // Second ingest of identical file: every block already exists in vault!
    ChunkManifest m2;
    uint64_t bytes2 = cdc.ingest_video(test_vid, vault_dir, m2);
    // 0 new bytes written -> 100% deduplication savings!
    PRISM_ASSERT_EQ(bytes2, 0u);
    PRISM_ASSERT_EQ(m1.original_file_sha256, m2.original_file_sha256);

    // Cleanup
    fs::remove(test_vid);
    fs::remove_all(vault_dir);
}
