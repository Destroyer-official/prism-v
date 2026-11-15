#pragma once
#ifndef PRISM_CONTAINER_PVLOSS_HPP
#define PRISM_CONTAINER_PVLOSS_HPP

#include "prism/types.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>

namespace prism {

/**
 * @brief Entry in the pidx Random-Access Seek Table.
 * Maps virtual MP4 byte offsets (in 256 KB increments) to file offsets in .pvloss.
 */
struct PidxEntry {
    uint64_t virtual_byte_offset = 0; // Offset in uncompressed MP4
    uint64_t file_byte_offset = 0;    // Offset in .pvloss container
    uint32_t compressed_size = 0;     // Size of compressed rANS payload
    uint32_t uncompressed_size = 0;   // Exactly 256 KB (or tail chunk size)
    uint32_t crc32c = 0;              // CRC32C chunk integrity check
};

struct PvlossHeader {
    std::array<uint8_t, 8> magic = PVLOSS_MAGIC;
    LosslessContract lossless_mode = LosslessContract::CONTRACT_A_BBR;
    SourceCodecId source_codec_id = SourceCodecId::CODEC_H264;
    uint16_t flags = 0x0004; // bit 2: seekable 256kb blocks
    uint64_t orig_file_size = 0;
    Sha256Digest orig_sha256{};
    uint16_t width = 1920;
    uint16_t height = 1080;
    uint16_t fps_num = 30;
    uint16_t fps_den = 1;
    uint16_t bit_depth = 8;
    uint16_t chroma_format = 1; // 1 = 4:2:0
    uint64_t index_offset = 0;
};

/**
 * @brief Production Writer for .pvloss Self-Indexed Lossless Containers.
 */
class PvlossWriter {
public:
    PvlossWriter() = default;

    void open(const std::string& filepath, const PvlossHeader& header);
    void write_chunk(const uint8_t* compressed_data,
                     uint32_t comp_size,
                     uint32_t uncomp_size,
                     uint64_t virtual_offset);
    void close();

    static uint32_t compute_crc32c(const uint8_t* data, size_t size);

private:
    std::ofstream out_;
    PvlossHeader header_;
    std::vector<PidxEntry> index_entries_;
    uint64_t current_file_offset_ = 0;
    bool is_open_ = false;
};

/**
 * @brief Production Reader for .pvloss Containers with 256 KB Random-Access Seeking.
 */
class PvlossReader {
public:
    PvlossReader() = default;
    ~PvlossReader();

    bool open(const std::string& filepath);
    void close();

    const PvlossHeader& header() const { return header_; }
    const std::vector<PidxEntry>& index() const { return index_entries_; }

    /**
     * @brief Find chunk entry covering a given uncompressed byte offset.
     */
    const PidxEntry* find_entry_for_offset(uint64_t virtual_offset) const;

    /**
     * @brief Read compressed payload for a specific entry.
     */
    std::vector<uint8_t> read_chunk_payload(const PidxEntry& entry);

private:
    std::ifstream in_;
    PvlossHeader header_;
    std::vector<PidxEntry> index_entries_;
    bool is_open_ = false;
};

} // namespace prism

#endif // PRISM_CONTAINER_PVLOSS_HPP
