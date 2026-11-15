#include "prism/container_pvloss.hpp"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace prism {

// Standard CRC32C (Castagnoli 0x82F63B78) table
static const std::array<uint32_t, 256> CRC32C_TABLE = []() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0x82F63B78u : 0);
        }
        table[i] = crc;
    }
    return table;
}();

uint32_t PvlossWriter::compute_crc32c(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ CRC32C_TABLE[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFu;
}

void PvlossWriter::open(const std::string& filepath, const PvlossHeader& header) {
    out_.open(filepath, std::ios::binary | std::ios::trunc);
    if (!out_.is_open()) {
        throw std::runtime_error("Failed to open .pvloss file for writing: " + filepath);
    }
    header_ = header;
    index_entries_.clear();

    // Reserve space for fixed 0x48 (72-byte) header
    std::vector<uint8_t> blank_header(0x48, 0);
    out_.write(reinterpret_cast<const char*>(blank_header.data()), blank_header.size());
    current_file_offset_ = 0x48;
    is_open_ = true;
}

void PvlossWriter::write_chunk(const uint8_t* compressed_data,
                              uint32_t comp_size,
                              uint32_t uncomp_size,
                              uint64_t virtual_offset) {
    if (!is_open_) throw std::runtime_error("PvlossWriter is not open");

    PidxEntry entry;
    entry.virtual_byte_offset = virtual_offset;
    entry.file_byte_offset = current_file_offset_;
    entry.compressed_size = comp_size;
    entry.uncompressed_size = uncomp_size;
    entry.crc32c = compute_crc32c(compressed_data, comp_size);

    // Write chunk framing: [comp_size: uint32_t] [crc32c: uint32_t] [payload: comp_size bytes]
    out_.write(reinterpret_cast<const char*>(&comp_size), sizeof(uint32_t));
    out_.write(reinterpret_cast<const char*>(&entry.crc32c), sizeof(uint32_t));
    out_.write(reinterpret_cast<const char*>(compressed_data), comp_size);

    current_file_offset_ += sizeof(uint32_t) * 2 + comp_size;
    index_entries_.push_back(entry);
}

void PvlossWriter::close() {
    if (!is_open_) return;

    // Write pidx table at current offset
    uint64_t pidx_offset = current_file_offset_;
    uint32_t num_entries = static_cast<uint32_t>(index_entries_.size());
    out_.write(reinterpret_cast<const char*>(&num_entries), sizeof(uint32_t));

    for (const auto& entry : index_entries_) {
        out_.write(reinterpret_cast<const char*>(&entry.virtual_byte_offset), sizeof(uint64_t));
        out_.write(reinterpret_cast<const char*>(&entry.file_byte_offset), sizeof(uint64_t));
        out_.write(reinterpret_cast<const char*>(&entry.compressed_size), sizeof(uint32_t));
        out_.write(reinterpret_cast<const char*>(&entry.uncompressed_size), sizeof(uint32_t));
        out_.write(reinterpret_cast<const char*>(&entry.crc32c), sizeof(uint32_t));
    }

    // Now seek back and rewrite full 0x48 header with finalized index_offset
    header_.index_offset = pidx_offset;
    out_.seekp(0, std::ios::beg);

    std::vector<uint8_t> hbuf(0x48, 0);
    std::memcpy(hbuf.data() + 0x00, header_.magic.data(), 8);
    hbuf[0x08] = static_cast<uint8_t>(header_.lossless_mode);
    hbuf[0x09] = static_cast<uint8_t>(header_.source_codec_id);
    std::memcpy(hbuf.data() + 0x0A, &header_.flags, sizeof(uint16_t));
    std::memcpy(hbuf.data() + 0x0C, &header_.orig_file_size, sizeof(uint64_t));
    std::memcpy(hbuf.data() + 0x14, header_.orig_sha256.data(), 32);

    uint16_t geom[6] = {
        header_.width, header_.height, header_.fps_num,
        header_.fps_den, header_.bit_depth, header_.chroma_format
    };
    std::memcpy(hbuf.data() + 0x34, geom, sizeof(geom));
    std::memcpy(hbuf.data() + 0x40, &header_.index_offset, sizeof(uint64_t));

    out_.write(reinterpret_cast<const char*>(hbuf.data()), hbuf.size());
    out_.close();
    is_open_ = false;
}

PvlossReader::~PvlossReader() {
    close();
}

bool PvlossReader::open(const std::string& filepath) {
    close();
    in_.open(filepath, std::ios::binary);
    if (!in_.is_open()) return false;

    // Read fixed 0x48 header
    std::vector<uint8_t> hbuf(0x48);
    in_.read(reinterpret_cast<char*>(hbuf.data()), hbuf.size());
    if (in_.gcount() < 0x48) return false;

    // Verify magic
    if (std::memcmp(hbuf.data(), PVLOSS_MAGIC.data(), 8) != 0) {
        return false;
    }

    header_.lossless_mode = static_cast<LosslessContract>(hbuf[0x08]);
    header_.source_codec_id = static_cast<SourceCodecId>(hbuf[0x09]);
    std::memcpy(&header_.flags, hbuf.data() + 0x0A, sizeof(uint16_t));
    std::memcpy(&header_.orig_file_size, hbuf.data() + 0x0C, sizeof(uint64_t));
    std::memcpy(header_.orig_sha256.data(), hbuf.data() + 0x14, 32);

    uint16_t geom[6]{};
    std::memcpy(geom, hbuf.data() + 0x34, sizeof(geom));
    header_.width = geom[0];
    header_.height = geom[1];
    header_.fps_num = geom[2];
    header_.fps_den = geom[3];
    header_.bit_depth = geom[4];
    header_.chroma_format = geom[5];
    std::memcpy(&header_.index_offset, hbuf.data() + 0x40, sizeof(uint64_t));

    // Read pidx table
    in_.seekg(header_.index_offset, std::ios::beg);
    uint32_t num_entries = 0;
    in_.read(reinterpret_cast<char*>(&num_entries), sizeof(uint32_t));

    index_entries_.resize(num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        in_.read(reinterpret_cast<char*>(&index_entries_[i].virtual_byte_offset), sizeof(uint64_t));
        in_.read(reinterpret_cast<char*>(&index_entries_[i].file_byte_offset), sizeof(uint64_t));
        in_.read(reinterpret_cast<char*>(&index_entries_[i].compressed_size), sizeof(uint32_t));
        in_.read(reinterpret_cast<char*>(&index_entries_[i].uncompressed_size), sizeof(uint32_t));
        in_.read(reinterpret_cast<char*>(&index_entries_[i].crc32c), sizeof(uint32_t));
    }

    is_open_ = true;
    return true;
}

void PvlossReader::close() {
    if (is_open_) {
        in_.close();
        is_open_ = false;
    }
}

const PidxEntry* PvlossReader::find_entry_for_offset(uint64_t virtual_offset) const {
    if (index_entries_.empty()) return nullptr;

    // Binary search through sorted virtual_byte_offsets
    auto it = std::upper_bound(index_entries_.begin(), index_entries_.end(), virtual_offset,
        [](uint64_t val, const PidxEntry& e) {
            return val < e.virtual_byte_offset;
        });

    if (it == index_entries_.begin()) {
        return &index_entries_[0];
    }
    --it;
    return &(*it);
}

std::vector<uint8_t> PvlossReader::read_chunk_payload(const PidxEntry& entry) {
    if (!is_open_) return {};

    // Seek to chunk offset + 8 bytes (skipping framing uint32 size and crc)
    in_.seekg(entry.file_byte_offset + sizeof(uint32_t) * 2, std::ios::beg);
    std::vector<uint8_t> payload(entry.compressed_size);
    in_.read(reinterpret_cast<char*>(payload.data()), entry.compressed_size);

    // Verify CRC32C integrity
    uint32_t actual_crc = PvlossWriter::compute_crc32c(payload.data(), payload.size());
    if (actual_crc != entry.crc32c) {
        throw std::runtime_error("Chunk CRC32C corruption detected in .pvloss container");
    }

    return payload;
}

} // namespace prism
