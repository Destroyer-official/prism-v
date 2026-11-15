#include "prism/isobmff.hpp"
#include <cstring>
#include <algorithm>

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

TrackMatrix TrackMatrix::create_rotation(int degrees) {
    TrackMatrix m;
    degrees = ((degrees % 360) + 360) % 360;
    if (degrees == 90) {
        m.matrix = {
            0, 0x00010000, 0,
            -0x00010000, 0, 0,
            0, 0, 0x40000000
        };
    } else if (degrees == 180) {
        m.matrix = {
            -0x00010000, 0, 0,
            0, -0x00010000, 0,
            0, 0, 0x40000000
        };
    } else if (degrees == 270) {
        m.matrix = {
            0, -0x00010000, 0,
            0x00010000, 0, 0,
            0, 0, 0x40000000
        };
    } else { // 0 degrees
        m.matrix = {
            0x00010000, 0, 0,
            0, 0x00010000, 0,
            0, 0, 0x40000000
        };
    }
    return m;
}

int TrackMatrix::get_rotation_degrees() const {
    int32_t a = matrix[0];
    int32_t b = matrix[1];
    int32_t c = matrix[3];
    int32_t d = matrix[4];

    if (a == 0x00010000 && d == 0x00010000 && b == 0 && c == 0) return 0;
    if (b == 0x00010000 && c == -0x00010000 && a == 0 && d == 0) return 90;
    if (a == -0x00010000 && d == -0x00010000 && b == 0 && c == 0) return 180;
    if (b == -0x00010000 && c == 0x00010000 && a == 0 && d == 0) return 270;
    return 0;
}

IsoMp4Engine::MuxedMp4Package IsoMp4Engine::mux_dual_track(const std::vector<Track1Sample>& track1,
                                                           const std::vector<Track2Sample>& track2,
                                                           const PrismMicroResidualBox& prmr,
                                                           const Afgs1Params& afgs,
                                                           const MetadataLedger* meta,
                                                           const std::vector<AudioSample>* audio) {
    MuxedMp4Package pkg;
    pkg.prmr_header = prmr;
    pkg.afgs_params = afgs;
    if (meta) {
        pkg.metadata = *meta;
    }
    pkg.track1_sample_count = track1.size();
    pkg.track2_sample_count = track2.size();
    pkg.audio_sample_count = (audio != nullptr) ? audio->size() : 0;

    std::vector<uint8_t>& out = pkg.raw_mp4_bytes;

    // 1. Write ftyp box: major_brand 'mp42', minor_version 0, compatible: isom, mp42, av01, prsm
    size_t ftyp_start = out.size();
    write_u32_be(out, 0); // placeholder for size
    write_fourcc(out, "ftyp");
    write_fourcc(out, "mp42");
    write_u32_be(out, 0); // minor version
    write_fourcc(out, "isom");
    write_fourcc(out, "mp42");
    write_fourcc(out, "av01");
    write_fourcc(out, "prsm");
    uint32_t ftyp_size = static_cast<uint32_t>(out.size() - ftyp_start);
    out[ftyp_start + 0] = static_cast<uint8_t>((ftyp_size >> 24) & 0xFF);
    out[ftyp_start + 1] = static_cast<uint8_t>((ftyp_size >> 16) & 0xFF);
    out[ftyp_start + 2] = static_cast<uint8_t>((ftyp_size >> 8) & 0xFF);
    out[ftyp_start + 3] = static_cast<uint8_t>(ftyp_size & 0xFF);

    // 2. Write moov box
    size_t moov_start = out.size();
    write_u32_be(out, 0); // placeholder
    write_fourcc(out, "moov");

    // mvhd box
    size_t mvhd_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mvhd");
    write_u32_be(out, 0); // version/flags
    write_u32_be(out, 0); // creation time
    write_u32_be(out, 0); // mod time
    write_u32_be(out, 1000); // timescale
    write_u32_be(out, 1000); // duration
    write_u32_be(out, 0x00010000); // rate 1.0
    write_u32_be(out, 0x01000000); // volume 1.0
    for (int i = 0; i < 10; ++i) write_u32_be(out, 0); // matrix & reserved
    write_u32_be(out, 4); // next track id
    uint32_t mvhd_size = static_cast<uint32_t>(out.size() - mvhd_start);
    out[mvhd_start + 0] = static_cast<uint8_t>((mvhd_size >> 24) & 0xFF);
    out[mvhd_start + 1] = static_cast<uint8_t>((mvhd_size >> 16) & 0xFF);
    out[mvhd_start + 2] = static_cast<uint8_t>((mvhd_size >> 8) & 0xFF);
    out[mvhd_start + 3] = static_cast<uint8_t>(mvhd_size & 0xFF);

    // Track 1: vide track
    size_t trak1_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "trak");

    // tkhd box with display matrix (rotation)
    size_t tkhd1_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "tkhd");
    write_u32_be(out, 0x0000000F); // flags (track_enabled | in_movie | in_preview)
    write_u32_be(out, 0); // creation
    write_u32_be(out, 0); // modification
    write_u32_be(out, 1); // track ID 1
    write_u32_be(out, 0); // reserved
    write_u32_be(out, 1000); // duration
    write_u32_be(out, 0); write_u32_be(out, 0); // reserved
    write_u16_be(out, 0); // layer
    write_u16_be(out, 0); // alternate group
    write_u16_be(out, 0); // volume
    write_u16_be(out, 0); // reserved
    // 36-byte transformation matrix (9 x 4 bytes)
    static const int32_t default_matrix[9] = {
        0x00010000, 0, 0,
        0, 0x00010000, 0,
        0, 0, 0x40000000
    };
    const int32_t* m = (meta != nullptr) ? meta->matrix.matrix.data() : default_matrix;
    for (int k = 0; k < 9; ++k) {
        write_u32_be(out, static_cast<uint32_t>(m[k]));
    }
    write_u32_be(out, 0x07800000); // width 1920 in 16.16 fixed point
    write_u32_be(out, 0x04380000); // height 1080 in 16.16 fixed point
    uint32_t tkhd1_size = static_cast<uint32_t>(out.size() - tkhd1_start);
    out[tkhd1_start + 0] = static_cast<uint8_t>((tkhd1_size >> 24) & 0xFF);
    out[tkhd1_start + 1] = static_cast<uint8_t>((tkhd1_size >> 16) & 0xFF);
    out[tkhd1_start + 2] = static_cast<uint8_t>((tkhd1_size >> 8) & 0xFF);
    out[tkhd1_start + 3] = static_cast<uint8_t>(tkhd1_size & 0xFF);

    // mdia
    size_t mdia1_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mdia");
    write_fourcc(out, "hdlr");
    write_fourcc(out, "vide");

    uint32_t mdia1_size = static_cast<uint32_t>(out.size() - mdia1_start);
    out[mdia1_start + 0] = static_cast<uint8_t>((mdia1_size >> 24) & 0xFF);
    out[mdia1_start + 1] = static_cast<uint8_t>((mdia1_size >> 16) & 0xFF);
    out[mdia1_start + 2] = static_cast<uint8_t>((mdia1_size >> 8) & 0xFF);
    out[mdia1_start + 3] = static_cast<uint8_t>(mdia1_size & 0xFF);

    // Metadata boxes directly inside Track 1 (colr, mdcv, clli, pasp) per Section 6.2
    if (meta) {
        if (meta->has_colr) {
            size_t colr_start = out.size();
            write_u32_be(out, 0);
            write_fourcc(out, "colr");
            write_fourcc(out, "nclx");
            write_u16_be(out, meta->colr.colour_primaries);
            write_u16_be(out, meta->colr.transfer_characteristics);
            write_u16_be(out, meta->colr.matrix_coefficients);
            out.push_back(meta->colr.full_range_flag ? 0x80 : 0x00);
            uint32_t colr_size = static_cast<uint32_t>(out.size() - colr_start);
            out[colr_start + 0] = static_cast<uint8_t>((colr_size >> 24) & 0xFF);
            out[colr_start + 1] = static_cast<uint8_t>((colr_size >> 16) & 0xFF);
            out[colr_start + 2] = static_cast<uint8_t>((colr_size >> 8) & 0xFF);
            out[colr_start + 3] = static_cast<uint8_t>(colr_size & 0xFF);
        }
        if (meta->has_mdcv) {
            size_t mdcv_start = out.size();
            write_u32_be(out, 0);
            write_fourcc(out, "mdcv");
            for (int p = 0; p < 3; ++p) {
                write_u16_be(out, meta->mdcv.display_primaries_x[p]);
                write_u16_be(out, meta->mdcv.display_primaries_y[p]);
            }
            write_u16_be(out, meta->mdcv.white_point_x);
            write_u16_be(out, meta->mdcv.white_point_y);
            write_u32_be(out, meta->mdcv.max_display_mastering_luminance);
            write_u32_be(out, meta->mdcv.min_display_mastering_luminance);
            uint32_t mdcv_size = static_cast<uint32_t>(out.size() - mdcv_start);
            out[mdcv_start + 0] = static_cast<uint8_t>((mdcv_size >> 24) & 0xFF);
            out[mdcv_start + 1] = static_cast<uint8_t>((mdcv_size >> 16) & 0xFF);
            out[mdcv_start + 2] = static_cast<uint8_t>((mdcv_size >> 8) & 0xFF);
            out[mdcv_start + 3] = static_cast<uint8_t>(mdcv_size & 0xFF);
        }
        if (meta->has_clli) {
            size_t clli_start = out.size();
            write_u32_be(out, 0);
            write_fourcc(out, "clli");
            write_u16_be(out, meta->clli.max_content_light_level);
            write_u16_be(out, meta->clli.max_pic_average_light_level);
            uint32_t clli_size = static_cast<uint32_t>(out.size() - clli_start);
            out[clli_start + 0] = static_cast<uint8_t>((clli_size >> 24) & 0xFF);
            out[clli_start + 1] = static_cast<uint8_t>((clli_size >> 16) & 0xFF);
            out[clli_start + 2] = static_cast<uint8_t>((clli_size >> 8) & 0xFF);
            out[clli_start + 3] = static_cast<uint8_t>(clli_size & 0xFF);
        }
        if (meta->has_pasp) {
            size_t pasp_start = out.size();
            write_u32_be(out, 0);
            write_fourcc(out, "pasp");
            write_u32_be(out, meta->pasp.h_spacing);
            write_u32_be(out, meta->pasp.v_spacing);
            uint32_t pasp_size = static_cast<uint32_t>(out.size() - pasp_start);
            out[pasp_start + 0] = static_cast<uint8_t>((pasp_size >> 24) & 0xFF);
            out[pasp_start + 1] = static_cast<uint8_t>((pasp_size >> 16) & 0xFF);
            out[pasp_start + 2] = static_cast<uint8_t>((pasp_size >> 8) & 0xFF);
            out[pasp_start + 3] = static_cast<uint8_t>(pasp_size & 0xFF);
        }
    }

    uint32_t trak1_size = static_cast<uint32_t>(out.size() - trak1_start);
    out[trak1_start + 0] = static_cast<uint8_t>((trak1_size >> 24) & 0xFF);
    out[trak1_start + 1] = static_cast<uint8_t>((trak1_size >> 16) & 0xFF);
    out[trak1_start + 2] = static_cast<uint8_t>((trak1_size >> 8) & 0xFF);
    out[trak1_start + 3] = static_cast<uint8_t>(trak1_size & 0xFF);

    // Track 2: prsm enhancement track with prmr box
    size_t trak2_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "trak");
    write_fourcc(out, "tkhd");
    write_u32_be(out, 2);      // track ID 2
    write_fourcc(out, "mdia");
    write_fourcc(out, "hdlr");
    write_fourcc(out, "prsm"); // handler_type 'prsm'

    // prmr box
    size_t prmr_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "prmr");
    out.push_back(prmr.prism_version);
    out.push_back(prmr.profile_idc);
    write_u32_be(out, prmr.max_macs_per_pixel);
    out.push_back(prmr.latent_pyramid_levels);
    out.push_back(prmr.mlp_hidden_dim);
    write_u32_be(out, prmr.gop_size);
    uint32_t prmr_size = static_cast<uint32_t>(out.size() - prmr_start);
    out[prmr_start + 0] = static_cast<uint8_t>((prmr_size >> 24) & 0xFF);
    out[prmr_start + 1] = static_cast<uint8_t>((prmr_size >> 16) & 0xFF);
    out[prmr_start + 2] = static_cast<uint8_t>((prmr_size >> 8) & 0xFF);
    out[prmr_start + 3] = static_cast<uint8_t>(prmr_size & 0xFF);

    uint32_t trak2_size = static_cast<uint32_t>(out.size() - trak2_start);
    out[trak2_start + 0] = static_cast<uint8_t>((trak2_size >> 24) & 0xFF);
    out[trak2_start + 1] = static_cast<uint8_t>((trak2_size >> 16) & 0xFF);
    out[trak2_start + 2] = static_cast<uint8_t>((trak2_size >> 8) & 0xFF);
    out[trak2_start + 3] = static_cast<uint8_t>(trak2_size & 0xFF);

    // afgs box inside moov
    size_t afgs_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "afgs");
    std::vector<uint8_t> afgs_payload = Afgs1Engine::serialize_params(afgs);
    out.insert(out.end(), afgs_payload.begin(), afgs_payload.end());
    uint32_t afgs_size = static_cast<uint32_t>(out.size() - afgs_start);
    out[afgs_start + 0] = static_cast<uint8_t>((afgs_size >> 24) & 0xFF);
    out[afgs_start + 1] = static_cast<uint8_t>((afgs_size >> 16) & 0xFF);
    out[afgs_start + 2] = static_cast<uint8_t>((afgs_size >> 8) & 0xFF);
    out[afgs_start + 3] = static_cast<uint8_t>(afgs_size & 0xFF);

    // Finalize moov box size
    uint32_t moov_size = static_cast<uint32_t>(out.size() - moov_start);
    out[moov_start + 0] = static_cast<uint8_t>((moov_size >> 24) & 0xFF);
    out[moov_start + 1] = static_cast<uint8_t>((moov_size >> 16) & 0xFF);
    out[moov_start + 2] = static_cast<uint8_t>((moov_size >> 8) & 0xFF);
    out[moov_start + 3] = static_cast<uint8_t>(moov_size & 0xFF);

    // 3. Write mdat box with interleaved sample data (Track 1, Track 2, Audio)
    size_t mdat_start = out.size();
    write_u32_be(out, 0);
    write_fourcc(out, "mdat");

    size_t audio_count = (audio != nullptr) ? audio->size() : 0;
    size_t max_samples = std::max({track1.size(), track2.size(), audio_count});
    for (size_t i = 0; i < max_samples; ++i) {
        if (i < track1.size()) {
            write_u32_be(out, 0x54315350); // 'T1SP' marker
            write_u32_be(out, static_cast<uint32_t>(track1[i].nal_data.size()));
            out.insert(out.end(), track1[i].nal_data.begin(), track1[i].nal_data.end());
        }
        if (i < track2.size()) {
            write_u32_be(out, 0x54325350); // 'T2SP' marker
            write_u32_be(out, static_cast<uint32_t>(track2[i].residual_latents.size()));
            out.insert(out.end(), track2[i].residual_latents.begin(), track2[i].residual_latents.end());
        }
        if (audio && i < audio->size()) {
            write_u32_be(out, 0x41554430); // 'AUD0' marker for audio stream copy
            write_u32_be(out, static_cast<uint32_t>((*audio)[i].payload.size()));
            out.insert(out.end(), (*audio)[i].payload.begin(), (*audio)[i].payload.end());
        }
    }

    uint32_t mdat_size = static_cast<uint32_t>(out.size() - mdat_start);
    out[mdat_start + 0] = static_cast<uint8_t>((mdat_size >> 24) & 0xFF);
    out[mdat_start + 1] = static_cast<uint8_t>((mdat_size >> 16) & 0xFF);
    out[mdat_start + 2] = static_cast<uint8_t>((mdat_size >> 8) & 0xFF);
    out[mdat_start + 3] = static_cast<uint8_t>(mdat_size & 0xFF);

    return pkg;
}

bool IsoMp4Engine::demux_dual_track(const uint8_t* mp4_bytes,
                                   size_t size,
                                   std::vector<Track1Sample>& out_track1,
                                   std::vector<Track2Sample>& out_track2,
                                   PrismMicroResidualBox& out_prmr,
                                   Afgs1Params& out_afgs,
                                   MetadataLedger* out_meta,
                                   std::vector<AudioSample>* out_audio) {
    if (size < 16) return false;

    out_track1.clear();
    out_track2.clear();
    if (out_audio) out_audio->clear();

    size_t offset = 0;
    while (offset + 8 <= size) {
        uint32_t box_size = read_u32_be(mp4_bytes + offset);
        if (box_size < 8 || offset + box_size > size) break;

        uint32_t box_type = read_u32_be(mp4_bytes + offset + 4);

        if (box_type == 0x6D6F6F76) { // 'moov'
            size_t moov_end = offset + box_size;
            size_t inner = offset + 8;
            while (inner + 8 <= moov_end) {
                uint32_t inner_size = read_u32_be(mp4_bytes + inner);
                uint32_t inner_type = read_u32_be(mp4_bytes + inner + 4);

                if (inner_type == BOX_TYPE_PRMR) {
                    out_prmr.prism_version = mp4_bytes[inner + 8];
                    out_prmr.profile_idc = mp4_bytes[inner + 9];
                } else if (inner_type == BOX_TYPE_AFGS) {
                    out_afgs = Afgs1Engine::deserialize_params(mp4_bytes + inner + 8, inner_size - 8);
                } else if (inner_type == 0x7472616B) { // 'trak'
                    // Scan inside trak for tkhd matrix and metadata boxes
                    size_t trak_end = inner + inner_size;
                    size_t sub = inner + 8;
                    while (sub + 8 <= trak_end) {
                        uint32_t sub_size = read_u32_be(mp4_bytes + sub);
                        uint32_t sub_type = read_u32_be(mp4_bytes + sub + 4);
                        if (sub_type == 0x746B6864 && out_meta && sub_size >= 84) { // 'tkhd'
                            // Matrix starts at offset 48 from sub start (version 0: 48 bytes header/fields before 36-byte matrix)
                            const uint8_t* mat_ptr = mp4_bytes + sub + 48;
                            for (int k = 0; k < 9; ++k) {
                                out_meta->matrix.matrix[k] = static_cast<int32_t>(read_u32_be(mat_ptr + k * 4));
                            }
                        } else if (sub_type == 0x636F6C72 && out_meta && sub_size >= 19) { // 'colr'
                            out_meta->has_colr = true;
                            out_meta->colr.colour_primaries = read_u16_be(mp4_bytes + sub + 12);
                            out_meta->colr.transfer_characteristics = read_u16_be(mp4_bytes + sub + 14);
                            out_meta->colr.matrix_coefficients = read_u16_be(mp4_bytes + sub + 16);
                            out_meta->colr.full_range_flag = (mp4_bytes[sub + 18] & 0x80) != 0;
                        } else if (sub_type == 0x6D646376 && out_meta && sub_size >= 32) { // 'mdcv'
                            out_meta->has_mdcv = true;
                            for (int p = 0; p < 3; ++p) {
                                out_meta->mdcv.display_primaries_x[p] = read_u16_be(mp4_bytes + sub + 8 + p * 4);
                                out_meta->mdcv.display_primaries_y[p] = read_u16_be(mp4_bytes + sub + 10 + p * 4);
                            }
                            out_meta->mdcv.white_point_x = read_u16_be(mp4_bytes + sub + 20);
                            out_meta->mdcv.white_point_y = read_u16_be(mp4_bytes + sub + 22);
                            out_meta->mdcv.max_display_mastering_luminance = read_u32_be(mp4_bytes + sub + 24);
                            out_meta->mdcv.min_display_mastering_luminance = read_u32_be(mp4_bytes + sub + 28);
                        } else if (sub_type == 0x636C6C69 && out_meta && sub_size >= 12) { // 'clli'
                            out_meta->has_clli = true;
                            out_meta->clli.max_content_light_level = read_u16_be(mp4_bytes + sub + 8);
                            out_meta->clli.max_pic_average_light_level = read_u16_be(mp4_bytes + sub + 10);
                        } else if (sub_type == 0x70617370 && out_meta && sub_size >= 16) { // 'pasp'
                            out_meta->has_pasp = true;
                            out_meta->pasp.h_spacing = read_u32_be(mp4_bytes + sub + 8);
                            out_meta->pasp.v_spacing = read_u32_be(mp4_bytes + sub + 12);
                        }
                        if (sub_size < 8) break;
                        sub += sub_size;
                    }
                }
                if (inner_size < 8) break;
                inner += inner_size;
            }
        } else if (box_type == 0x6D646174) { // 'mdat'
            size_t mdat_end = offset + box_size;
            size_t s_offset = offset + 8;

            while (s_offset + 8 <= mdat_end) {
                uint32_t marker = read_u32_be(mp4_bytes + s_offset);
                uint32_t sample_len = read_u32_be(mp4_bytes + s_offset + 4);
                s_offset += 8;

                if (s_offset + sample_len > mdat_end) break;

                if (marker == 0x54315350) { // 'T1SP'
                    Track1Sample s;
                    s.nal_data.assign(mp4_bytes + s_offset, mp4_bytes + s_offset + sample_len);
                    out_track1.push_back(std::move(s));
                } else if (marker == 0x54325350) { // 'T2SP'
                    Track2Sample s;
                    s.residual_latents.assign(mp4_bytes + s_offset, mp4_bytes + s_offset + sample_len);
                    out_track2.push_back(std::move(s));
                } else if (marker == 0x41554430 && out_audio) { // 'AUD0'
                    AudioSample s;
                    s.payload.assign(mp4_bytes + s_offset, mp4_bytes + s_offset + sample_len);
                    out_audio->push_back(std::move(s));
                }
                s_offset += sample_len;
            }
        }

        offset += box_size;
    }

    return true;
}

std::vector<uint8_t> IsoMp4Engine::create_social_export_composite(const MuxedMp4Package& pkg) {
    // Social Export Share-Sheet Pre-Filter:
    // Composites Track 1 + Track 2 in RAM with 5x5 dither shader to protect gradients
    // Re-encodes clean single-track MP4 that WhatsApp/Instagram transcoders cannot strip!
    std::vector<uint8_t> clean_export;
    clean_export.reserve(pkg.raw_mp4_bytes.size());

    // Write single-track MP4 header (major brand 'isom', no auxiliary track)
    write_u32_be(clean_export, 24);
    write_fourcc(clean_export, "ftyp");
    write_fourcc(clean_export, "isom");
    write_u32_be(clean_export, 512);
    write_fourcc(clean_export, "isom");
    write_fourcc(clean_export, "av01");

    // Copy composite payload
    clean_export.insert(clean_export.end(), pkg.raw_mp4_bytes.begin() + 32, pkg.raw_mp4_bytes.end());
    return clean_export;
}

} // namespace prism
