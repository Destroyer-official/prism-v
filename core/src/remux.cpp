#include "prism/remux.hpp"
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace prism {

namespace {

void write_u16_be(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void write_u32_be(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void write_fourcc(std::vector<uint8_t>& buf, const char* fourcc) {
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<uint8_t>(fourcc[i]));
    }
}

uint16_t read_u16_be(const uint8_t* ptr) {
    return (static_cast<uint16_t>(ptr[0]) << 8) | static_cast<uint16_t>(ptr[1]);
}

uint32_t read_u32_be(const uint8_t* ptr) {
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8)  |
           (static_cast<uint32_t>(ptr[3]));
}

} // anonymous namespace

bool RemuxEngine::should_use_remux(double estimated_bbr_savings_ratio) {
    // Contract B kicks in if bitstream syntax transcoding yields < 3% savings
    return estimated_bbr_savings_ratio < 0.03;
}

Sha256Digest RemuxEngine::compute_payload_hash(const std::vector<RemuxPacket>& packets) {
    std::vector<uint8_t> all_payloads;
    size_t total_bytes = 0;
    for (const auto& p : packets) {
        total_bytes += p.payload.size();
    }
    all_payloads.reserve(total_bytes);
    for (const auto& p : packets) {
        all_payloads.insert(all_payloads.end(), p.payload.begin(), p.payload.end());
    }
    return BbrEngine::compute_sha256(all_payloads.data(), all_payloads.size());
}

std::vector<uint8_t> RemuxEngine::remux_to_clean_isobmff(const RemuxPackage& pkg) {
    std::vector<uint8_t> out;

    uint64_t payload_bytes = 0;
    for (const auto& pkt : pkg.packets) {
        if (pkt.payload.size() > UINT32_MAX) {
            throw std::length_error("Remux packet payload exceeds the 32-bit box size field");
        }
        payload_bytes += 8 + pkt.payload.size();
    }
    // Leave headroom for ftyp/moov before the 32-bit mdat size is finalized.
    if (payload_bytes > UINT32_MAX - 65536) {
        throw std::length_error("Remux output exceeds the 32-bit ISOBMFF box size field");
    }

    // 1. Write ftyp box
    size_t ftyp_start = out.size();
    write_u32_be(out, 0); // size placeholder
    write_fourcc(out, "ftyp");
    write_fourcc(out, "mp42");
    write_u32_be(out, 0);
    write_fourcc(out, "isom");
    write_fourcc(out, "mp42");
    write_fourcc(out, "hvc1");
    write_fourcc(out, "av01");
    uint32_t ftyp_size = static_cast<uint32_t>(out.size() - ftyp_start);
    out[ftyp_start + 0] = static_cast<uint8_t>((ftyp_size >> 24) & 0xFF);
    out[ftyp_start + 1] = static_cast<uint8_t>((ftyp_size >> 16) & 0xFF);
    out[ftyp_start + 2] = static_cast<uint8_t>((ftyp_size >> 8) & 0xFF);
    out[ftyp_start + 3] = static_cast<uint8_t>(ftyp_size & 0xFF);

    // 2. Write moov box
    size_t moov_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "moov");

    // mvhd
    size_t mvhd_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mvhd");
    write_u32_be(out, 0);
    write_u32_be(out, 0);
    write_u32_be(out, 0);
    write_u32_be(out, 1000); // timescale
    write_u32_be(out, 1000); // duration
    write_u32_be(out, 0x00010000);
    write_u32_be(out, 0x01000000);
    for (int i = 0; i < 10; ++i) write_u32_be(out, 0);
    write_u32_be(out, 3); // next track id
    uint32_t mvhd_size = static_cast<uint32_t>(out.size() - mvhd_start);
    out[mvhd_start + 0] = static_cast<uint8_t>((mvhd_size >> 24) & 0xFF);
    out[mvhd_start + 1] = static_cast<uint8_t>((mvhd_size >> 16) & 0xFF);
    out[mvhd_start + 2] = static_cast<uint8_t>((mvhd_size >> 8) & 0xFF);
    out[mvhd_start + 3] = static_cast<uint8_t>(mvhd_size & 0xFF);

    // Track 1: Video Track
    size_t trak_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "trak");

    // tkhd
    size_t tkhd_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "tkhd");
    write_u32_be(out, 0x0000000F);
    write_u32_be(out, 0);
    write_u32_be(out, 0);
    write_u32_be(out, 1); // track ID 1
    write_u32_be(out, 0);
    write_u32_be(out, 1000);
    write_u32_be(out, 0); write_u32_be(out, 0);
    write_u16_be(out, 0);
    write_u16_be(out, 0);
    write_u16_be(out, 0);
    write_u16_be(out, 0);

    const int32_t* m = pkg.metadata.matrix.matrix.data();
    for (int k = 0; k < 9; ++k) {
        write_u32_be(out, static_cast<uint32_t>(m[k]));
    }
    write_u32_be(out, 0x07800000);
    write_u32_be(out, 0x04380000);
    uint32_t tkhd_size = static_cast<uint32_t>(out.size() - tkhd_start);
    out[tkhd_start + 0] = static_cast<uint8_t>((tkhd_size >> 24) & 0xFF);
    out[tkhd_start + 1] = static_cast<uint8_t>((tkhd_size >> 16) & 0xFF);
    out[tkhd_start + 2] = static_cast<uint8_t>((tkhd_size >> 8) & 0xFF);
    out[tkhd_start + 3] = static_cast<uint8_t>(tkhd_size & 0xFF);

    // mdia
    size_t mdia_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mdia");
    write_fourcc(out, "hdlr");
    write_fourcc(out, "vide");
    uint32_t mdia_size = static_cast<uint32_t>(out.size() - mdia_start);
    out[mdia_start + 0] = static_cast<uint8_t>((mdia_size >> 24) & 0xFF);
    out[mdia_start + 1] = static_cast<uint8_t>((mdia_size >> 16) & 0xFF);
    out[mdia_start + 2] = static_cast<uint8_t>((mdia_size >> 8) & 0xFF);
    out[mdia_start + 3] = static_cast<uint8_t>(mdia_size & 0xFF);

    // Metadata preservation boxes directly inside Track 1 (colr, mdcv, clli, pasp)
    if (pkg.metadata.has_colr) {
        size_t colr_start = out.size();
        write_u32_be(out, 0);
        write_fourcc(out, "colr");
        write_fourcc(out, "nclx");
        write_u16_be(out, pkg.metadata.colr.colour_primaries);
        write_u16_be(out, pkg.metadata.colr.transfer_characteristics);
        write_u16_be(out, pkg.metadata.colr.matrix_coefficients);
        out.push_back(pkg.metadata.colr.full_range_flag ? 0x80 : 0x00);
        uint32_t colr_size = static_cast<uint32_t>(out.size() - colr_start);
        out[colr_start + 0] = static_cast<uint8_t>((colr_size >> 24) & 0xFF);
        out[colr_start + 1] = static_cast<uint8_t>((colr_size >> 16) & 0xFF);
        out[colr_start + 2] = static_cast<uint8_t>((colr_size >> 8) & 0xFF);
        out[colr_start + 3] = static_cast<uint8_t>(colr_size & 0xFF);
    }
    if (pkg.metadata.has_mdcv) {
        size_t mdcv_start = out.size();
        write_u32_be(out, 0);
        write_fourcc(out, "mdcv");
        for (int p = 0; p < 3; ++p) {
            write_u16_be(out, pkg.metadata.mdcv.display_primaries_x[p]);
            write_u16_be(out, pkg.metadata.mdcv.display_primaries_y[p]);
        }
        write_u16_be(out, pkg.metadata.mdcv.white_point_x);
        write_u16_be(out, pkg.metadata.mdcv.white_point_y);
        write_u32_be(out, pkg.metadata.mdcv.max_display_mastering_luminance);
        write_u32_be(out, pkg.metadata.mdcv.min_display_mastering_luminance);
        uint32_t mdcv_size = static_cast<uint32_t>(out.size() - mdcv_start);
        out[mdcv_start + 0] = static_cast<uint8_t>((mdcv_size >> 24) & 0xFF);
        out[mdcv_start + 1] = static_cast<uint8_t>((mdcv_size >> 16) & 0xFF);
        out[mdcv_start + 2] = static_cast<uint8_t>((mdcv_size >> 8) & 0xFF);
        out[mdcv_start + 3] = static_cast<uint8_t>(mdcv_size & 0xFF);
    }
    if (pkg.metadata.has_clli) {
        size_t clli_start = out.size();
        write_u32_be(out, 0);
        write_fourcc(out, "clli");
        write_u16_be(out, pkg.metadata.clli.max_content_light_level);
        write_u16_be(out, pkg.metadata.clli.max_pic_average_light_level);
        uint32_t clli_size = static_cast<uint32_t>(out.size() - clli_start);
        out[clli_start + 0] = static_cast<uint8_t>((clli_size >> 24) & 0xFF);
        out[clli_start + 1] = static_cast<uint8_t>((clli_size >> 16) & 0xFF);
        out[clli_start + 2] = static_cast<uint8_t>((clli_size >> 8) & 0xFF);
        out[clli_start + 3] = static_cast<uint8_t>(clli_size & 0xFF);
    }
    if (pkg.metadata.has_pasp) {
        size_t pasp_start = out.size();
        write_u32_be(out, 0);
        write_fourcc(out, "pasp");
        write_u32_be(out, pkg.metadata.pasp.h_spacing);
        write_u32_be(out, pkg.metadata.pasp.v_spacing);
        uint32_t pasp_size = static_cast<uint32_t>(out.size() - pasp_start);
        out[pasp_start + 0] = static_cast<uint8_t>((pasp_size >> 24) & 0xFF);
        out[pasp_start + 1] = static_cast<uint8_t>((pasp_size >> 16) & 0xFF);
        out[pasp_start + 2] = static_cast<uint8_t>((pasp_size >> 8) & 0xFF);
        out[pasp_start + 3] = static_cast<uint8_t>(pasp_size & 0xFF);
    }

    uint32_t trak_size = static_cast<uint32_t>(out.size() - trak_start);
    out[trak_start + 0] = static_cast<uint8_t>((trak_size >> 24) & 0xFF);
    out[trak_start + 1] = static_cast<uint8_t>((trak_size >> 16) & 0xFF);
    out[trak_start + 2] = static_cast<uint8_t>((trak_size >> 8) & 0xFF);
    out[trak_start + 3] = static_cast<uint8_t>(trak_size & 0xFF);

    // Finalize moov size
    uint32_t moov_size = static_cast<uint32_t>(out.size() - moov_start);
    out[moov_start + 0] = static_cast<uint8_t>((moov_size >> 24) & 0xFF);
    out[moov_start + 1] = static_cast<uint8_t>((moov_size >> 16) & 0xFF);
    out[moov_start + 2] = static_cast<uint8_t>((moov_size >> 8) & 0xFF);
    out[moov_start + 3] = static_cast<uint8_t>(moov_size & 0xFF);

    // 3. Write mdat box containing packet payloads
    size_t mdat_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mdat");

    for (const auto& pkt : pkg.packets) {
        if (pkt.type == MediaType::VIDEO_NAL) {
            write_u32_be(out, 0x564E414C); // 'VNAL'
        } else if (pkt.type == MediaType::AUDIO_FRAME) {
            write_u32_be(out, 0x41554430); // 'AUD0'
        } else {
            write_u32_be(out, 0x4D455441); // 'META'
        }
        write_u32_be(out, static_cast<uint32_t>(pkt.payload.size()));
        out.insert(out.end(), pkt.payload.begin(), pkt.payload.end());
    }

    uint32_t mdat_size = static_cast<uint32_t>(out.size() - mdat_start);
    out[mdat_start + 0] = static_cast<uint8_t>((mdat_size >> 24) & 0xFF);
    out[mdat_start + 1] = static_cast<uint8_t>((mdat_size >> 16) & 0xFF);
    out[mdat_start + 2] = static_cast<uint8_t>((mdat_size >> 8) & 0xFF);
    out[mdat_start + 3] = static_cast<uint8_t>(mdat_size & 0xFF);

    return out;
}

bool RemuxEngine::parse_and_verify_isobmff(const uint8_t* mp4_data, size_t size, RemuxPackage& out_pkg) {
    if (mp4_data == nullptr || size < 16) return false;

    out_pkg.packets.clear();
    out_pkg.metadata = IsoMp4Engine::MetadataLedger{};
    out_pkg.video_packet_count = 0;
    out_pkg.audio_packet_count = 0;

    size_t offset = 0;
    while (offset + 8 <= size) {
        uint32_t box_size = read_u32_be(mp4_data + offset);
        if (box_size == 0) {
            size_t remaining = size - offset;
            if (remaining > UINT32_MAX) return false;
            box_size = static_cast<uint32_t>(remaining); // extends to EOF
        }

        if (box_size < 8 || box_size > size - offset) return false;

        uint32_t box_type = read_u32_be(mp4_data + offset + 4);

        if (box_type == 0x6D6F6F76) { // 'moov'
            size_t moov_end = offset + box_size;
            size_t inner = offset + 8;
            while (moov_end - inner >= 8) {
                uint32_t inner_size = read_u32_be(mp4_data + inner);
                uint32_t inner_type = read_u32_be(mp4_data + inner + 4);
                if (inner_size < 8 || inner_size > moov_end - inner) return false;

                if (inner_type == 0x7472616B) { // 'trak'
                    size_t trak_end = inner + inner_size;
                    size_t sub = inner + 8;
                    while (trak_end - sub >= 8) {
                        uint32_t sub_size = read_u32_be(mp4_data + sub);
                        uint32_t sub_type = read_u32_be(mp4_data + sub + 4);
                        if (sub_size < 8 || sub_size > trak_end - sub) return false;

                        if (sub_type == 0x746B6864 && sub_size >= 84) {
                            const uint8_t* mat_ptr = mp4_data + sub + 48;
                            for (int k = 0; k < 9; ++k) {
                                out_pkg.metadata.matrix.matrix[k] = static_cast<int32_t>(read_u32_be(mat_ptr + k * 4));
                            }
                        } else if (sub_type == 0x636F6C72 && sub_size >= 19) {
                            out_pkg.metadata.has_colr = true;
                            out_pkg.metadata.colr.colour_primaries = read_u16_be(mp4_data + sub + 12);
                            out_pkg.metadata.colr.transfer_characteristics = read_u16_be(mp4_data + sub + 14);
                            out_pkg.metadata.colr.matrix_coefficients = read_u16_be(mp4_data + sub + 16);
                            out_pkg.metadata.colr.full_range_flag = (mp4_data[sub + 18] & 0x80) != 0;
                        } else if (sub_type == 0x6D646376 && sub_size >= 32) {
                            out_pkg.metadata.has_mdcv = true;
                            for (int p = 0; p < 3; ++p) {
                                out_pkg.metadata.mdcv.display_primaries_x[p] = read_u16_be(mp4_data + sub + 8 + p * 4);
                                out_pkg.metadata.mdcv.display_primaries_y[p] = read_u16_be(mp4_data + sub + 10 + p * 4);
                            }
                            out_pkg.metadata.mdcv.white_point_x = read_u16_be(mp4_data + sub + 20);
                            out_pkg.metadata.mdcv.white_point_y = read_u16_be(mp4_data + sub + 22);
                            out_pkg.metadata.mdcv.max_display_mastering_luminance = read_u32_be(mp4_data + sub + 24);
                            out_pkg.metadata.mdcv.min_display_mastering_luminance = read_u32_be(mp4_data + sub + 28);
                        } else if (sub_type == 0x636C6C69 && sub_size >= 12) {
                            out_pkg.metadata.has_clli = true;
                            out_pkg.metadata.clli.max_content_light_level = read_u16_be(mp4_data + sub + 8);
                            out_pkg.metadata.clli.max_pic_average_light_level = read_u16_be(mp4_data + sub + 10);
                        } else if (sub_type == 0x70617370 && sub_size >= 16) {
                            out_pkg.metadata.has_pasp = true;
                            out_pkg.metadata.pasp.h_spacing = read_u32_be(mp4_data + sub + 8);
                            out_pkg.metadata.pasp.v_spacing = read_u32_be(mp4_data + sub + 12);
                        }

                        sub += sub_size;
                    }
                }
                inner += inner_size;
            }
        } else if (box_type == 0x6D646174) { // 'mdat'
            size_t mdat_end = offset + box_size;
            size_t s_offset = offset + 8;

            while (mdat_end - s_offset >= 8) {
                uint32_t marker = read_u32_be(mp4_data + s_offset);
                uint32_t sample_len = read_u32_be(mp4_data + s_offset + 4);
                s_offset += 8;

                if (sample_len > mdat_end - s_offset) return false;

                RemuxPacket pkt;
                if (marker == 0x564E414C) { // 'VNAL'
                    pkt.type = MediaType::VIDEO_NAL;
                    out_pkg.video_packet_count++;
                } else if (marker == 0x41554430) { // 'AUD0'
                    pkt.type = MediaType::AUDIO_FRAME;
                    out_pkg.audio_packet_count++;
                } else {
                    pkt.type = MediaType::TIMED_METADATA;
                }
                pkt.payload.assign(mp4_data + s_offset, mp4_data + s_offset + sample_len);
                out_pkg.packets.push_back(std::move(pkt));

                s_offset += sample_len;
            }
            if (s_offset != mdat_end) return false;
        }

        offset += box_size;
    }

    if (offset != size) return false;

    out_pkg.payload_hash = compute_payload_hash(out_pkg.packets);
    return true;
}

bool RemuxEngine::verify_packet_preservation(const RemuxPackage& src, const RemuxPackage& dst) {
    if (src.packets.size() != dst.packets.size()) return false;
    if (src.payload_hash != dst.payload_hash) return false;

    for (size_t i = 0; i < src.packets.size(); ++i) {
        if (src.packets[i].type != dst.packets[i].type) return false;
        if (src.packets[i].payload != dst.packets[i].payload) return false;
    }
    return true;
}

} // namespace prism
