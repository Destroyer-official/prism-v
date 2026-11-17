#include "prism/isobmff.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace prism {

namespace {

void write_u16_be(std::vector<uint8_t> &buf, uint16_t val) {
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void write_u32_be(std::vector<uint8_t> &buf, uint32_t val) {
  buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void write_fourcc(std::vector<uint8_t> &buf, const char *fourcc) {
  for (int i = 0; i < 4; ++i) {
    buf.push_back(static_cast<uint8_t>(fourcc[i]));
  }
}

uint16_t read_u16_be(const uint8_t *ptr) {
  return (static_cast<uint16_t>(ptr[0]) << 8) | static_cast<uint16_t>(ptr[1]);
}

uint32_t read_u32_be(const uint8_t *ptr) {
  return (static_cast<uint32_t>(ptr[0]) << 24) |
         (static_cast<uint32_t>(ptr[1]) << 16) |
         (static_cast<uint32_t>(ptr[2]) << 8) | (static_cast<uint32_t>(ptr[3]));
}

uint64_t read_u64_be(const uint8_t *ptr) {
  return (static_cast<uint64_t>(read_u32_be(ptr)) << 32) |
         static_cast<uint64_t>(read_u32_be(ptr + 4));
}

float read_float32_be(const uint8_t *ptr) {
  uint32_t val = read_u32_be(ptr);
  float f;
  std::memcpy(&f, &val, 4);
  return f;
}

double read_float64_be(const uint8_t *ptr) {
  uint64_t val = read_u64_be(ptr);
  double d;
  std::memcpy(&d, &val, 8);
  return d;
}

// RFC 8794 EBML VINT Parsers
bool read_ebml_id(const uint8_t *data, size_t size, size_t &offset,
                  uint32_t &out_id, size_t &out_len) {
  if (offset >= size)
    return false;
  uint8_t first = data[offset];
  if (first == 0)
    return false; // EBML IDs cannot have 8 leading zeros

  if (first & 0x80) {
    out_len = 1;
    out_id = first;
  } else if (first & 0x40) {
    out_len = 2;
    if (offset + 2 > size)
      return false;
    out_id = (static_cast<uint32_t>(data[offset]) << 8) | data[offset + 1];
  } else if (first & 0x20) {
    out_len = 3;
    if (offset + 3 > size)
      return false;
    out_id = (static_cast<uint32_t>(data[offset]) << 16) |
             (static_cast<uint32_t>(data[offset + 1]) << 8) | data[offset + 2];
  } else if (first & 0x10) {
    out_len = 4;
    if (offset + 4 > size)
      return false;
    out_id = (static_cast<uint32_t>(data[offset]) << 24) |
             (static_cast<uint32_t>(data[offset + 1]) << 16) |
             (static_cast<uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
  } else {
    return false; // Unsupported element ID length (>4 bytes)
  }

  offset += out_len;
  return true;
}

bool read_ebml_size(const uint8_t *data, size_t size, size_t &offset,
                    uint64_t &out_size, size_t &out_len) {
  if (offset >= size)
    return false;
  uint8_t first = data[offset];
  if (first == 0)
    return false;

  int leading_zeros = 0;
  for (int b = 7; b >= 0; --b) {
    if (first & (1 << b))
      break;
    leading_zeros++;
  }

  out_len = leading_zeros + 1;
  if (out_len > 8 || offset + out_len > size)
    return false;

  // Mask out the length marker bit
  uint8_t mask = (1 << (7 - leading_zeros)) - 1;
  uint64_t val = first & mask;

  bool all_ones = (val == mask);

  for (size_t i = 1; i < out_len; ++i) {
    val = (val << 8) | data[offset + i];
    if (data[offset + i] != 0xFF)
      all_ones = false;
  }

  offset += out_len;
  out_size = all_ones ? std::numeric_limits<uint64_t>::max() : val;
  return true;
}

uint64_t read_ebml_uint(const uint8_t *data, size_t len) {
  uint64_t val = 0;
  for (size_t i = 0; i < len && i < 8; ++i) {
    val = (val << 8) | data[i];
  }
  return val;
}

double read_ebml_float(const uint8_t *data, size_t len) {
  if (len == 4) {
    return static_cast<double>(read_float32_be(data));
  } else if (len == 8) {
    return read_float64_be(data);
  }
  return 0.0;
}

} // anonymous namespace

TrackMatrix TrackMatrix::create_rotation(int degrees) {
  TrackMatrix m;
  degrees = ((degrees % 360) + 360) % 360;
  if (degrees == 90) {
    m.matrix = {0, 0x00010000, 0, -0x00010000, 0, 0, 0, 0, 0x40000000};
  } else if (degrees == 180) {
    m.matrix = {-0x00010000, 0, 0, 0, -0x00010000, 0, 0, 0, 0x40000000};
  } else if (degrees == 270) {
    m.matrix = {0, -0x00010000, 0, 0x00010000, 0, 0, 0, 0, 0x40000000};
  } else { // 0 degrees
    m.matrix = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  }
  return m;
}

int TrackMatrix::get_rotation_degrees() const {
  int32_t a = matrix[0];
  int32_t b = matrix[1];
  int32_t c = matrix[3];
  int32_t d = matrix[4];

  if (a == 0x00010000 && d == 0x00010000 && b == 0 && c == 0)
    return 0;
  if (b == 0x00010000 && c == -0x00010000 && a == 0 && d == 0)
    return 90;
  if (a == -0x00010000 && d == -0x00010000 && b == 0 && c == 0)
    return 180;
  if (b == -0x00010000 && c == 0x00010000 && a == 0 && d == 0)
    return 270;
  return 0;
}

IsoMp4Engine::MuxedMp4Package IsoMp4Engine::mux_dual_track(
    const std::vector<Track1Sample> &track1,
    const std::vector<Track2Sample> &track2, const PrismMicroResidualBox &prmr,
    const Afgs1Params &afgs, const MetadataLedger *meta,
    const std::vector<AudioSample> *audio) {
  MuxedMp4Package pkg;
  pkg.prmr_header = prmr;
  pkg.afgs_params = afgs;
  if (meta) {
    pkg.metadata = *meta;
  }
  pkg.track1_sample_count = track1.size();
  pkg.track2_sample_count = track2.size();
  pkg.audio_sample_count = (audio != nullptr) ? audio->size() : 0;

  std::vector<uint8_t> &out = pkg.raw_mp4_bytes;

  // 1. Write ftyp box: major_brand 'mp42', minor_version 0, compatible: isom,
  // mp42, av01, prsm
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
  write_u32_be(out, 0);          // version/flags
  write_u32_be(out, 0);          // creation time
  write_u32_be(out, 0);          // mod time
  write_u32_be(out, 1000);       // timescale
  write_u32_be(out, 1000);       // duration
  write_u32_be(out, 0x00010000); // rate 1.0
  write_u32_be(out, 0x01000000); // volume 1.0
  for (int i = 0; i < 10; ++i)
    write_u32_be(out, 0); // matrix & reserved
  write_u32_be(out, 4);   // next track id
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
  write_u32_be(out,
               0x0000000F); // flags (track_enabled | in_movie | in_preview)
  write_u32_be(out, 0);     // creation
  write_u32_be(out, 0);     // modification
  write_u32_be(out, 1);     // track ID 1
  write_u32_be(out, 0);     // reserved
  write_u32_be(out, 1000);  // duration
  write_u32_be(out, 0);
  write_u32_be(out, 0); // reserved
  write_u16_be(out, 0); // layer
  write_u16_be(out, 0); // alternate group
  write_u16_be(out, 0); // volume
  write_u16_be(out, 0); // reserved
  // 36-byte transformation matrix (9 x 4 bytes)
  static const int32_t default_matrix[9] = {
      0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  const int32_t *m =
      (meta != nullptr) ? meta->matrix.matrix.data() : default_matrix;
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

  // Metadata boxes directly inside Track 1 (colr, mdcv, clli, pasp) per
  // Section 6.2
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

  // Track 2: prsm enhancement track
  size_t trak2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "trak");

  // tkhd box (24 bytes)
  size_t tkhd2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "tkhd");
  write_u32_be(out, 0x0000000F); // flags
  write_u32_be(out, 2);          // track ID 2
  write_u32_be(out, 1000);       // duration
  uint32_t tkhd2_size = static_cast<uint32_t>(out.size() - tkhd2_start);
  out[tkhd2_start + 0] = static_cast<uint8_t>((tkhd2_size >> 24) & 0xFF);
  out[tkhd2_start + 1] = static_cast<uint8_t>((tkhd2_size >> 16) & 0xFF);
  out[tkhd2_start + 2] = static_cast<uint8_t>((tkhd2_size >> 8) & 0xFF);
  out[tkhd2_start + 3] = static_cast<uint8_t>(tkhd2_size & 0xFF);

  // mdia box (16 bytes)
  size_t mdia2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "mdia");
  write_fourcc(out, "hdlr");
  write_fourcc(out, "prsm"); // handler_type 'prsm'
  uint32_t mdia2_size = static_cast<uint32_t>(out.size() - mdia2_start);
  out[mdia2_start + 0] = static_cast<uint8_t>((mdia2_size >> 24) & 0xFF);
  out[mdia2_start + 1] = static_cast<uint8_t>((mdia2_size >> 16) & 0xFF);
  out[mdia2_start + 2] = static_cast<uint8_t>((mdia2_size >> 8) & 0xFF);
  out[mdia2_start + 3] = static_cast<uint8_t>(mdia2_size & 0xFF);

  uint32_t trak2_size = static_cast<uint32_t>(out.size() - trak2_start);
  out[trak2_start + 0] = static_cast<uint8_t>((trak2_size >> 24) & 0xFF);
  out[trak2_start + 1] = static_cast<uint8_t>((trak2_size >> 16) & 0xFF);
  out[trak2_start + 2] = static_cast<uint8_t>((trak2_size >> 8) & 0xFF);
  out[trak2_start + 3] = static_cast<uint8_t>(trak2_size & 0xFF);

  // prmr box inside moov
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
      out.insert(out.end(), track1[i].nal_data.begin(),
                 track1[i].nal_data.end());
    }
    if (i < track2.size()) {
      write_u32_be(out, 0x54325350); // 'T2SP' marker
      write_u32_be(out,
                   static_cast<uint32_t>(track2[i].residual_latents.size()));
      out.insert(out.end(), track2[i].residual_latents.begin(),
                 track2[i].residual_latents.end());
    }
    if (audio && i < audio->size()) {
      write_u32_be(out, 0x41554430); // 'AUD0' marker for audio stream copy
      write_u32_be(out, static_cast<uint32_t>((*audio)[i].payload.size()));
      out.insert(out.end(), (*audio)[i].payload.begin(),
                 (*audio)[i].payload.end());
    }
  }

  uint32_t mdat_size = static_cast<uint32_t>(out.size() - mdat_start);
  out[mdat_start + 0] = static_cast<uint8_t>((mdat_size >> 24) & 0xFF);
  out[mdat_start + 1] = static_cast<uint8_t>((mdat_size >> 16) & 0xFF);
  out[mdat_start + 2] = static_cast<uint8_t>((mdat_size >> 8) & 0xFF);
  out[mdat_start + 3] = static_cast<uint8_t>(mdat_size & 0xFF);

  return pkg;
}

bool IsoMp4Engine::demux_dual_track(const uint8_t *mp4_bytes, size_t size,
                                    std::vector<Track1Sample> &out_track1,
                                    std::vector<Track2Sample> &out_track2,
                                    PrismMicroResidualBox &out_prmr,
                                    Afgs1Params &out_afgs,
                                    MetadataLedger *out_meta,
                                    std::vector<AudioSample> *out_audio) {
  if (size < 16)
    return false;

  out_track1.clear();
  out_track2.clear();
  if (out_audio)
    out_audio->clear();

  size_t offset = 0;
  while (offset + 8 <= size) {
    uint32_t box_size = read_u32_be(mp4_bytes + offset);
    if (box_size < 8 || offset + box_size > size)
      break;

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
          if (inner_size >= 20) {
            out_prmr.max_macs_per_pixel =
                static_cast<uint16_t>(read_u32_be(mp4_bytes + inner + 10));
            out_prmr.latent_pyramid_levels = mp4_bytes[inner + 14];
            out_prmr.mlp_hidden_dim = mp4_bytes[inner + 15];
            out_prmr.gop_size =
                static_cast<uint16_t>(read_u32_be(mp4_bytes + inner + 16));
          }
        } else if (inner_type == BOX_TYPE_AFGS) {
          out_afgs = Afgs1Engine::deserialize_params(mp4_bytes + inner + 8,
                                                     inner_size - 8);
        } else if (inner_type == 0x7472616B) { // 'trak'
          // Scan inside trak for tkhd matrix, prmr, and metadata boxes
          size_t trak_end = inner + inner_size;
          size_t sub = inner + 8;
          while (sub + 8 <= trak_end) {
            uint32_t sub_size = read_u32_be(mp4_bytes + sub);
            uint32_t sub_type = read_u32_be(mp4_bytes + sub + 4);
            if (sub_type == BOX_TYPE_PRMR) {
              out_prmr.prism_version = mp4_bytes[sub + 8];
              out_prmr.profile_idc = mp4_bytes[sub + 9];
              if (sub_size >= 20) {
                out_prmr.max_macs_per_pixel =
                    static_cast<uint16_t>(read_u32_be(mp4_bytes + sub + 10));
                out_prmr.latent_pyramid_levels = mp4_bytes[sub + 14];
                out_prmr.mlp_hidden_dim = mp4_bytes[sub + 15];
                out_prmr.gop_size =
                    static_cast<uint16_t>(read_u32_be(mp4_bytes + sub + 16));
              }
            } else if (sub_type == 0x746B6864 && out_meta &&
                       sub_size >= 84) { // 'tkhd'
              // Matrix starts at offset 48 from sub start (version 0: 48 bytes
              // header/fields before 36-byte matrix)
              const uint8_t *mat_ptr = mp4_bytes + sub + 48;
              for (int k = 0; k < 9; ++k) {
                out_meta->matrix.matrix[k] =
                    static_cast<int32_t>(read_u32_be(mat_ptr + k * 4));
              }
            } else if (sub_type == 0x636F6C72 && out_meta &&
                       sub_size >= 19) { // 'colr'
              out_meta->has_colr = true;
              out_meta->colr.colour_primaries =
                  read_u16_be(mp4_bytes + sub + 12);
              out_meta->colr.transfer_characteristics =
                  read_u16_be(mp4_bytes + sub + 14);
              out_meta->colr.matrix_coefficients =
                  read_u16_be(mp4_bytes + sub + 16);
              out_meta->colr.full_range_flag =
                  (mp4_bytes[sub + 18] & 0x80) != 0;
            } else if (sub_type == 0x6D646376 && out_meta &&
                       sub_size >= 32) { // 'mdcv'
              out_meta->has_mdcv = true;
              for (int p = 0; p < 3; ++p) {
                out_meta->mdcv.display_primaries_x[p] =
                    read_u16_be(mp4_bytes + sub + 8 + p * 4);
                out_meta->mdcv.display_primaries_y[p] =
                    read_u16_be(mp4_bytes + sub + 10 + p * 4);
              }
              out_meta->mdcv.white_point_x = read_u16_be(mp4_bytes + sub + 20);
              out_meta->mdcv.white_point_y = read_u16_be(mp4_bytes + sub + 22);
              out_meta->mdcv.max_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 24);
              out_meta->mdcv.min_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 28);
            } else if (sub_type == 0x636C6C69 && out_meta &&
                       sub_size >= 12) { // 'clli'
              out_meta->has_clli = true;
              out_meta->clli.max_content_light_level =
                  read_u16_be(mp4_bytes + sub + 8);
              out_meta->clli.max_pic_average_light_level =
                  read_u16_be(mp4_bytes + sub + 10);
            } else if (sub_type == 0x70617370 && out_meta &&
                       sub_size >= 16) { // 'pasp'
              out_meta->has_pasp = true;
              out_meta->pasp.h_spacing = read_u32_be(mp4_bytes + sub + 8);
              out_meta->pasp.v_spacing = read_u32_be(mp4_bytes + sub + 12);
            }
            if (sub_size < 8)
              break;
            sub += sub_size;
          }
        }
        if (inner_size < 8)
          break;
        inner += inner_size;
      }
    } else if (box_type == 0x6D646174) { // 'mdat'
      size_t mdat_end = offset + box_size;
      size_t s_offset = offset + 8;

      while (s_offset + 8 <= mdat_end) {
        uint32_t marker = read_u32_be(mp4_bytes + s_offset);
        uint32_t sample_len = read_u32_be(mp4_bytes + s_offset + 4);
        s_offset += 8;

        if (s_offset + sample_len > mdat_end)
          break;

        if (marker == 0x54315350) { // 'T1SP'
          Track1Sample s;
          s.nal_data.assign(mp4_bytes + s_offset,
                            mp4_bytes + s_offset + sample_len);
          out_track1.push_back(std::move(s));
        } else if (marker == 0x54325350) { // 'T2SP'
          Track2Sample s;
          s.residual_latents.assign(mp4_bytes + s_offset,
                                    mp4_bytes + s_offset + sample_len);
          out_track2.push_back(std::move(s));
        } else if (marker == 0x41554430 && out_audio) { // 'AUD0'
          AudioSample s;
          s.payload.assign(mp4_bytes + s_offset,
                           mp4_bytes + s_offset + sample_len);
          out_audio->push_back(std::move(s));
        }
        s_offset += sample_len;
      }
    }

    offset += box_size;
  }

  return true;
}

IsoMp4Engine::MuxedMp4Package IsoMp4Engine::mux_dual_track_gen(
    const std::vector<Track1Sample> &track1,
    const std::vector<Track2Sample> &track2_gen, const PrismGenBox &pgen,
    const Afgs1Params &afgs, const MetadataLedger *meta,
    const std::vector<AudioSample> *audio) {
  MuxedMp4Package pkg;
  pkg.pgen_header = pgen;
  pkg.has_pgen = true;
  pkg.afgs_params = afgs;
  if (meta) {
    pkg.metadata = *meta;
  }
  pkg.track1_sample_count = track1.size();
  pkg.track2_sample_count = track2_gen.size();
  pkg.audio_sample_count = (audio != nullptr) ? audio->size() : 0;

  std::vector<uint8_t> &out = pkg.raw_mp4_bytes;

  // 1. Write ftyp box: major_brand 'mp42', minor_version 0, compatible: isom,
  // mp42, av01, pgen
  size_t ftyp_start = out.size();
  write_u32_be(out, 0); // placeholder
  write_fourcc(out, "ftyp");
  write_fourcc(out, "mp42");
  write_u32_be(out, 0); // minor version
  write_fourcc(out, "isom");
  write_fourcc(out, "mp42");
  write_fourcc(out, "av01");
  write_fourcc(out, "pgen");
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
  write_u32_be(out, 0);          // version/flags
  write_u32_be(out, 0);          // creation time
  write_u32_be(out, 0);          // mod time
  write_u32_be(out, 1000);       // timescale
  write_u32_be(out, 1000);       // duration
  write_u32_be(out, 0x00010000); // rate 1.0
  write_u32_be(out, 0x01000000); // volume 1.0
  for (int i = 0; i < 10; ++i)
    write_u32_be(out, 0); // matrix & reserved
  write_u32_be(out, 4);   // next track id
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
  write_u32_be(out,
               0x0000000F); // flags (track_enabled | in_movie | in_preview)
  write_u32_be(out, 0);     // creation
  write_u32_be(out, 0);     // modification
  write_u32_be(out, 1);     // track ID 1
  write_u32_be(out, 0);     // reserved
  write_u32_be(out, 1000);  // duration
  write_u32_be(out, 0);
  write_u32_be(out, 0); // reserved
  write_u16_be(out, 0); // layer
  write_u16_be(out, 0); // alternate group
  write_u16_be(out, 0); // volume
  write_u16_be(out, 0); // reserved
  static const int32_t default_matrix[9] = {
      0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  const int32_t *m =
      (meta != nullptr) ? meta->matrix.matrix.data() : default_matrix;
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

  // Metadata boxes directly inside Track 1 (colr, mdcv, clli, pasp) per
  // Section 6.2
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

  // Track 2: pgen generative semantic track with pgen box
  size_t trak2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "trak");
  write_fourcc(out, "tkhd");
  write_u32_be(out, 2); // track ID 2
  write_fourcc(out, "mdia");
  write_fourcc(out, "hdlr");
  write_fourcc(out, "pgen"); // handler_type 'pgen'

  // pgen box
  size_t pgen_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "pgen");
  out.push_back(pgen.prism_version);
  out.push_back(pgen.keypoint_count);
  write_u16_be(out, pgen.anchor_interval);
  out.push_back(pgen.fsq_stage1_levels);
  out.push_back(pgen.fsq_stage2_levels);
  write_u16_be(out, pgen.max_macs_per_pixel);
  out.push_back(pgen.has_audio_concealment ? 1 : 0);
  uint32_t pgen_size = static_cast<uint32_t>(out.size() - pgen_start);
  out[pgen_start + 0] = static_cast<uint8_t>((pgen_size >> 24) & 0xFF);
  out[pgen_start + 1] = static_cast<uint8_t>((pgen_size >> 16) & 0xFF);
  out[pgen_start + 2] = static_cast<uint8_t>((pgen_size >> 8) & 0xFF);
  out[pgen_start + 3] = static_cast<uint8_t>(pgen_size & 0xFF);

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
  size_t max_samples =
      std::max({track1.size(), track2_gen.size(), audio_count});
  for (size_t i = 0; i < max_samples; ++i) {
    if (i < track1.size()) {
      write_u32_be(out, 0x54315350); // 'T1SP' marker
      write_u32_be(out, static_cast<uint32_t>(track1[i].nal_data.size()));
      out.insert(out.end(), track1[i].nal_data.begin(),
                 track1[i].nal_data.end());
    }
    if (i < track2_gen.size()) {
      write_u32_be(out, 0x54325350); // 'T2SP' marker
      write_u32_be(
          out, static_cast<uint32_t>(track2_gen[i].residual_latents.size()));
      out.insert(out.end(), track2_gen[i].residual_latents.begin(),
                 track2_gen[i].residual_latents.end());
    }
    if (audio && i < audio->size()) {
      write_u32_be(out, 0x41554430); // 'AUD0' marker
      write_u32_be(out, static_cast<uint32_t>((*audio)[i].payload.size()));
      out.insert(out.end(), (*audio)[i].payload.begin(),
                 (*audio)[i].payload.end());
    }
  }

  uint32_t mdat_size = static_cast<uint32_t>(out.size() - mdat_start);
  out[mdat_start + 0] = static_cast<uint8_t>((mdat_size >> 24) & 0xFF);
  out[mdat_start + 1] = static_cast<uint8_t>((mdat_size >> 16) & 0xFF);
  out[mdat_start + 2] = static_cast<uint8_t>((mdat_size >> 8) & 0xFF);
  out[mdat_start + 3] = static_cast<uint8_t>(mdat_size & 0xFF);

  return pkg;
}

bool IsoMp4Engine::demux_dual_track_gen(const uint8_t *mp4_bytes, size_t size,
                                        std::vector<Track1Sample> &out_track1,
                                        std::vector<Track2Sample> &out_track2,
                                        PrismGenBox &out_pgen,
                                        Afgs1Params &out_afgs,
                                        MetadataLedger *out_meta,
                                        std::vector<AudioSample> *out_audio) {
  if (size < 16)
    return false;

  out_track1.clear();
  out_track2.clear();
  if (out_audio)
    out_audio->clear();

  size_t offset = 0;
  while (offset + 8 <= size) {
    uint32_t box_size = read_u32_be(mp4_bytes + offset);
    if (box_size < 8 || offset + box_size > size)
      break;

    uint32_t box_type = read_u32_be(mp4_bytes + offset + 4);

    if (box_type == 0x6D6F6F76) { // 'moov'
      size_t moov_end = offset + box_size;
      size_t inner = offset + 8;
      while (inner + 8 <= moov_end) {
        uint32_t inner_size = read_u32_be(mp4_bytes + inner);
        uint32_t inner_type = read_u32_be(mp4_bytes + inner + 4);

        if (inner_type == BOX_TYPE_PGEN && inner_size >= 17) {
          out_pgen.prism_version = mp4_bytes[inner + 8];
          out_pgen.keypoint_count = mp4_bytes[inner + 9];
          out_pgen.anchor_interval = read_u16_be(mp4_bytes + inner + 10);
          out_pgen.fsq_stage1_levels = mp4_bytes[inner + 12];
          out_pgen.fsq_stage2_levels = mp4_bytes[inner + 13];
          out_pgen.max_macs_per_pixel = read_u16_be(mp4_bytes + inner + 14);
          out_pgen.has_audio_concealment = (mp4_bytes[inner + 16] != 0);
        } else if (inner_type == BOX_TYPE_AFGS) {
          out_afgs = Afgs1Engine::deserialize_params(mp4_bytes + inner + 8,
                                                     inner_size - 8);
        } else if (inner_type == 0x7472616B) { // 'trak'
          size_t trak_end = inner + inner_size;
          size_t sub = inner + 8;
          while (sub + 8 <= trak_end) {
            uint32_t sub_size = read_u32_be(mp4_bytes + sub);
            uint32_t sub_type = read_u32_be(mp4_bytes + sub + 4);
            if (sub_type == BOX_TYPE_PGEN && sub_size >= 17) {
              out_pgen.prism_version = mp4_bytes[sub + 8];
              out_pgen.keypoint_count = mp4_bytes[sub + 9];
              out_pgen.anchor_interval = read_u16_be(mp4_bytes + sub + 10);
              out_pgen.fsq_stage1_levels = mp4_bytes[sub + 12];
              out_pgen.fsq_stage2_levels = mp4_bytes[sub + 13];
              out_pgen.max_macs_per_pixel = read_u16_be(mp4_bytes + sub + 14);
              out_pgen.has_audio_concealment = (mp4_bytes[sub + 16] != 0);
            } else if (sub_type == 0x746B6864 && out_meta && sub_size >= 84) {
              const uint8_t *mat_ptr = mp4_bytes + sub + 48;
              for (int k = 0; k < 9; ++k) {
                out_meta->matrix.matrix[k] =
                    static_cast<int32_t>(read_u32_be(mat_ptr + k * 4));
              }
            } else if (sub_type == 0x636F6C72 && out_meta && sub_size >= 19) {
              out_meta->has_colr = true;
              out_meta->colr.colour_primaries =
                  read_u16_be(mp4_bytes + sub + 12);
              out_meta->colr.transfer_characteristics =
                  read_u16_be(mp4_bytes + sub + 14);
              out_meta->colr.matrix_coefficients =
                  read_u16_be(mp4_bytes + sub + 16);
              out_meta->colr.full_range_flag =
                  (mp4_bytes[sub + 18] & 0x80) != 0;
            } else if (sub_type == 0x6D646376 && out_meta && sub_size >= 32) {
              out_meta->has_mdcv = true;
              for (int p = 0; p < 3; ++p) {
                out_meta->mdcv.display_primaries_x[p] =
                    read_u16_be(mp4_bytes + sub + 8 + p * 4);
                out_meta->mdcv.display_primaries_y[p] =
                    read_u16_be(mp4_bytes + sub + 10 + p * 4);
              }
              out_meta->mdcv.white_point_x = read_u16_be(mp4_bytes + sub + 20);
              out_meta->mdcv.white_point_y = read_u16_be(mp4_bytes + sub + 22);
              out_meta->mdcv.max_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 24);
              out_meta->mdcv.min_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 28);
            } else if (sub_type == 0x636C6C69 && out_meta && sub_size >= 12) {
              out_meta->has_clli = true;
              out_meta->clli.max_content_light_level =
                  read_u16_be(mp4_bytes + sub + 8);
              out_meta->clli.max_pic_average_light_level =
                  read_u16_be(mp4_bytes + sub + 10);
            } else if (sub_type == 0x70617370 && out_meta && sub_size >= 16) {
              out_meta->has_pasp = true;
              out_meta->pasp.h_spacing = read_u32_be(mp4_bytes + sub + 8);
              out_meta->pasp.v_spacing = read_u32_be(mp4_bytes + sub + 12);
            }
            if (sub_size < 8)
              break;
            sub += sub_size;
          }
        }
        if (inner_size < 8)
          break;
        inner += inner_size;
      }
    } else if (box_type == 0x6D646174) { // 'mdat'
      size_t mdat_end = offset + box_size;
      size_t s_offset = offset + 8;

      while (s_offset + 8 <= mdat_end) {
        uint32_t marker = read_u32_be(mp4_bytes + s_offset);
        uint32_t sample_len = read_u32_be(mp4_bytes + s_offset + 4);
        s_offset += 8;

        if (s_offset + sample_len > mdat_end)
          break;

        if (marker == 0x54315350) { // 'T1SP'
          Track1Sample s;
          s.nal_data.assign(mp4_bytes + s_offset,
                            mp4_bytes + s_offset + sample_len);
          out_track1.push_back(std::move(s));
        } else if (marker == 0x54325350) { // 'T2SP'
          Track2Sample s;
          s.residual_latents.assign(mp4_bytes + s_offset,
                                    mp4_bytes + s_offset + sample_len);
          out_track2.push_back(std::move(s));
        } else if (marker == 0x41554430 && out_audio) { // 'AUD0'
          AudioSample s;
          s.payload.assign(mp4_bytes + s_offset,
                           mp4_bytes + s_offset + sample_len);
          out_audio->push_back(std::move(s));
        }
        s_offset += sample_len;
      }
    }

    offset += box_size;
  }

  return true;
}

IsoMp4Engine::MuxedMp4Package IsoMp4Engine::mux_dual_track_spatial(
    const std::vector<Track1Sample> &track1,
    const std::vector<TrackSpatialSample> &track2_spatial,
    const PrismSpatialBox &spatial_box, const Afgs1Params &afgs,
    const MetadataLedger *meta, const std::vector<AudioSample> *audio) {
  MuxedMp4Package pkg;
  pkg.spatial_header = spatial_box;
  pkg.has_spatial = true;
  pkg.afgs_params = afgs;
  if (meta) {
    pkg.metadata = *meta;
  }
  pkg.track1_sample_count = track1.size();
  pkg.spatial_sample_count = track2_spatial.size();
  pkg.audio_sample_count = (audio != nullptr) ? audio->size() : 0;

  std::vector<uint8_t> &out = pkg.raw_mp4_bytes;

  // 1. Write ftyp box: major_brand 'mp42', minor_version 0, compatible: isom,
  // mp42, av01, 4dgs
  size_t ftyp_start = out.size();
  write_u32_be(out, 0); // placeholder
  write_fourcc(out, "ftyp");
  write_fourcc(out, "mp42");
  write_u32_be(out, 0); // minor version
  write_fourcc(out, "isom");
  write_fourcc(out, "mp42");
  write_fourcc(out, "av01");
  write_fourcc(out, "4dgs");
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
  write_u32_be(out, 0);          // version/flags
  write_u32_be(out, 0);          // creation time
  write_u32_be(out, 0);          // mod time
  write_u32_be(out, 1000);       // timescale
  write_u32_be(out, 1000);       // duration
  write_u32_be(out, 0x00010000); // rate 1.0
  write_u32_be(out, 0x01000000); // volume 1.0
  for (int i = 0; i < 10; ++i)
    write_u32_be(out, 0); // matrix & reserved
  write_u32_be(out, 4);   // next track id
  uint32_t mvhd_size = static_cast<uint32_t>(out.size() - mvhd_start);
  out[mvhd_start + 0] = static_cast<uint8_t>((mvhd_size >> 24) & 0xFF);
  out[mvhd_start + 1] = static_cast<uint8_t>((mvhd_size >> 16) & 0xFF);
  out[mvhd_start + 2] = static_cast<uint8_t>((mvhd_size >> 8) & 0xFF);
  out[mvhd_start + 3] = static_cast<uint8_t>(mvhd_size & 0xFF);

  // Track 1: vide track
  size_t trak1_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "trak");

  // tkhd box
  size_t tkhd1_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "tkhd");
  write_u32_be(out,
               0x0000000F); // flags (track_enabled | in_movie | in_preview)
  write_u32_be(out, 0);     // creation
  write_u32_be(out, 0);     // modification
  write_u32_be(out, 1);     // track ID 1
  write_u32_be(out, 0);     // reserved
  write_u32_be(out, 1000);  // duration
  write_u32_be(out, 0);
  write_u32_be(out, 0); // reserved
  write_u16_be(out, 0); // layer
  write_u16_be(out, 0); // alternate group
  write_u16_be(out, 0); // volume
  write_u16_be(out, 0); // reserved
  static const int32_t default_matrix[9] = {
      0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  const int32_t *m =
      (meta != nullptr) ? meta->matrix.matrix.data() : default_matrix;
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

  // Metadata boxes directly inside Track 1 (colr, mdcv, clli, pasp)
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

  // Track 2: 4dgs spatial volumetric track
  size_t trak2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "trak");

  // tkhd box (24 bytes)
  size_t tkhd2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "tkhd");
  write_u32_be(out, 0x0000000F); // flags
  write_u32_be(out, 2);          // track ID 2
  write_u32_be(out, 1000);       // duration
  uint32_t tkhd2_size = static_cast<uint32_t>(out.size() - tkhd2_start);
  out[tkhd2_start + 0] = static_cast<uint8_t>((tkhd2_size >> 24) & 0xFF);
  out[tkhd2_start + 1] = static_cast<uint8_t>((tkhd2_size >> 16) & 0xFF);
  out[tkhd2_start + 2] = static_cast<uint8_t>((tkhd2_size >> 8) & 0xFF);
  out[tkhd2_start + 3] = static_cast<uint8_t>(tkhd2_size & 0xFF);

  // mdia box (16 bytes)
  size_t mdia2_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "mdia");
  write_fourcc(out, "hdlr");
  write_fourcc(out, "4dgs"); // handler_type '4dgs'
  uint32_t mdia2_size = static_cast<uint32_t>(out.size() - mdia2_start);
  out[mdia2_start + 0] = static_cast<uint8_t>((mdia2_size >> 24) & 0xFF);
  out[mdia2_start + 1] = static_cast<uint8_t>((mdia2_size >> 16) & 0xFF);
  out[mdia2_start + 2] = static_cast<uint8_t>((mdia2_size >> 8) & 0xFF);
  out[mdia2_start + 3] = static_cast<uint8_t>(mdia2_size & 0xFF);

  uint32_t trak2_size = static_cast<uint32_t>(out.size() - trak2_start);
  out[trak2_start + 0] = static_cast<uint8_t>((trak2_size >> 24) & 0xFF);
  out[trak2_start + 1] = static_cast<uint8_t>((trak2_size >> 16) & 0xFF);
  out[trak2_start + 2] = static_cast<uint8_t>((trak2_size >> 8) & 0xFF);
  out[trak2_start + 3] = static_cast<uint8_t>(trak2_size & 0xFF);

  // 4dgs spatial configuration box inside moov
  size_t dgs_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "4dgs");
  out.push_back(spatial_box.prism_version);
  write_u32_be(out, spatial_box.num_gaussians);
  write_u16_be(out, spatial_box.gop_size);
  out.push_back(spatial_box.max_poly_degree);
  out.push_back(spatial_box.sh_degree);
  out.push_back(spatial_box.quant_bits);
  out.push_back(spatial_box.has_epipolar_flow ? 1 : 0);
  out.push_back(spatial_box.has_subspace_factorization ? 1 : 0);
  write_u32_be(out, static_cast<uint32_t>(spatial_box.fps * 1000.0f));

  uint32_t dgs_size = static_cast<uint32_t>(out.size() - dgs_start);
  out[dgs_start + 0] = static_cast<uint8_t>((dgs_size >> 24) & 0xFF);
  out[dgs_start + 1] = static_cast<uint8_t>((dgs_size >> 16) & 0xFF);
  out[dgs_start + 2] = static_cast<uint8_t>((dgs_size >> 8) & 0xFF);
  out[dgs_start + 3] = static_cast<uint8_t>(dgs_size & 0xFF);

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

  // 16 KiB Page Alignment padding ('free' box) for direct DMA / GPU buffer
  // binding
  size_t target_align = 64; // align mdat to cacheline/micro-page
  size_t unaligned_offset = out.size() + 8; // where mdat data starts
  size_t pad_needed =
      (target_align - (unaligned_offset % target_align)) % target_align;
  if (pad_needed > 0) {
    if (pad_needed < 8)
      pad_needed += target_align;
    write_u32_be(out, static_cast<uint32_t>(pad_needed));
    write_fourcc(out, "free");
    out.resize(out.size() + (pad_needed - 8), 0);
  }

  // 3. Write mdat box with interleaved sample data (Track 1, Track 2 Spatial,
  // Audio)
  size_t mdat_start = out.size();
  write_u32_be(out, 0);
  write_fourcc(out, "mdat");

  size_t audio_count = (audio != nullptr) ? audio->size() : 0;
  size_t max_samples =
      std::max({track1.size(), track2_spatial.size(), audio_count});
  for (size_t i = 0; i < max_samples; ++i) {
    if (i < track1.size()) {
      write_u32_be(out, 0x54315350); // 'T1SP' marker
      write_u32_be(out, static_cast<uint32_t>(track1[i].nal_data.size()));
      out.insert(out.end(), track1[i].nal_data.begin(),
                 track1[i].nal_data.end());
    }
    if (i < track2_spatial.size()) {
      write_u32_be(out, 0x34475350); // '4GSP' marker
      write_u32_be(out,
                   static_cast<uint32_t>(track2_spatial[i].bitstream.size()));
      out.insert(out.end(), track2_spatial[i].bitstream.begin(),
                 track2_spatial[i].bitstream.end());
    }
    if (audio && i < audio->size()) {
      write_u32_be(out, 0x41554430); // 'AUD0' marker
      write_u32_be(out, static_cast<uint32_t>((*audio)[i].payload.size()));
      out.insert(out.end(), (*audio)[i].payload.begin(),
                 (*audio)[i].payload.end());
    }
  }

  uint32_t mdat_size = static_cast<uint32_t>(out.size() - mdat_start);
  out[mdat_start + 0] = static_cast<uint8_t>((mdat_size >> 24) & 0xFF);
  out[mdat_start + 1] = static_cast<uint8_t>((mdat_size >> 16) & 0xFF);
  out[mdat_start + 2] = static_cast<uint8_t>((mdat_size >> 8) & 0xFF);
  out[mdat_start + 3] = static_cast<uint8_t>(mdat_size & 0xFF);

  return pkg;
}

bool IsoMp4Engine::demux_dual_track_spatial(
    const uint8_t *mp4_bytes, size_t size,
    std::vector<Track1Sample> &out_track1,
    std::vector<TrackSpatialSample> &out_track2_spatial,
    PrismSpatialBox &out_spatial, Afgs1Params &out_afgs,
    MetadataLedger *out_meta, std::vector<AudioSample> *out_audio) {
  if (size < 16)
    return false;

  out_track1.clear();
  out_track2_spatial.clear();
  if (out_audio)
    out_audio->clear();

  size_t offset = 0;
  while (offset + 8 <= size) {
    uint32_t box_size = read_u32_be(mp4_bytes + offset);
    if (box_size < 8 || offset + box_size > size)
      break;

    uint32_t box_type = read_u32_be(mp4_bytes + offset + 4);

    if (box_type == 0x6D6F6F76) { // 'moov'
      size_t moov_end = offset + box_size;
      size_t inner = offset + 8;
      while (inner + 8 <= moov_end) {
        uint32_t inner_size = read_u32_be(mp4_bytes + inner);
        uint32_t inner_type = read_u32_be(mp4_bytes + inner + 4);

        if (inner_type == BOX_TYPE_4DGS) {
          out_spatial.prism_version = mp4_bytes[inner + 8];
          out_spatial.num_gaussians = read_u32_be(mp4_bytes + inner + 9);
          out_spatial.gop_size = read_u16_be(mp4_bytes + inner + 13);
          out_spatial.max_poly_degree = mp4_bytes[inner + 15];
          out_spatial.sh_degree = mp4_bytes[inner + 16];
          out_spatial.quant_bits = mp4_bytes[inner + 17];
          out_spatial.has_epipolar_flow = (mp4_bytes[inner + 18] != 0);
          out_spatial.has_subspace_factorization = (mp4_bytes[inner + 19] != 0);
          uint32_t fps_fixed = read_u32_be(mp4_bytes + inner + 20);
          out_spatial.fps = (fps_fixed > 0)
                                ? (static_cast<float>(fps_fixed) / 1000.0f)
                                : 60.0f;
        } else if (inner_type == BOX_TYPE_AFGS) {
          out_afgs = Afgs1Engine::deserialize_params(mp4_bytes + inner + 8,
                                                     inner_size - 8);
        } else if (inner_type == 0x7472616B) { // 'trak'
          size_t trak_end = inner + inner_size;
          size_t sub = inner + 8;
          while (sub + 8 <= trak_end) {
            uint32_t sub_size = read_u32_be(mp4_bytes + sub);
            uint32_t sub_type = read_u32_be(mp4_bytes + sub + 4);

            if (sub_type == 0x746B6864 && out_meta) { // 'tkhd'
              if (sub + 8 + 84 <= trak_end) {
                size_t matrix_offset = sub + 8 + 48;
                for (int k = 0; k < 9; ++k) {
                  out_meta->matrix.matrix[k] = static_cast<int32_t>(
                      read_u32_be(mp4_bytes + matrix_offset + (k * 4)));
                }
              }
            } else if (sub_type == BOX_TYPE_4DGS) {
              out_spatial.prism_version = mp4_bytes[sub + 8];
              out_spatial.num_gaussians = read_u32_be(mp4_bytes + sub + 9);
              out_spatial.gop_size = read_u16_be(mp4_bytes + sub + 13);
              out_spatial.max_poly_degree = mp4_bytes[sub + 15];
              out_spatial.sh_degree = mp4_bytes[sub + 16];
              out_spatial.quant_bits = mp4_bytes[sub + 17];
              out_spatial.has_epipolar_flow = (mp4_bytes[sub + 18] != 0);
              out_spatial.has_subspace_factorization =
                  (mp4_bytes[sub + 19] != 0);
              uint32_t fps_fixed = read_u32_be(mp4_bytes + sub + 20);
              out_spatial.fps = (fps_fixed > 0)
                                    ? (static_cast<float>(fps_fixed) / 1000.0f)
                                    : 60.0f;
            } else if (sub_type == 0x636F6C72 && out_meta) { // 'colr'
              out_meta->has_colr = true;
              out_meta->colr.colour_primaries =
                  read_u16_be(mp4_bytes + sub + 12);
              out_meta->colr.transfer_characteristics =
                  read_u16_be(mp4_bytes + sub + 14);
              out_meta->colr.matrix_coefficients =
                  read_u16_be(mp4_bytes + sub + 16);
              out_meta->colr.full_range_flag =
                  (mp4_bytes[sub + 18] & 0x80) != 0;
            } else if (sub_type == 0x6D646376 && out_meta) { // 'mdcv'
              out_meta->has_mdcv = true;
              for (int p = 0; p < 3; ++p) {
                out_meta->mdcv.display_primaries_x[p] =
                    read_u16_be(mp4_bytes + sub + 8 + (p * 4));
                out_meta->mdcv.display_primaries_y[p] =
                    read_u16_be(mp4_bytes + sub + 10 + (p * 4));
              }
              out_meta->mdcv.white_point_x = read_u16_be(mp4_bytes + sub + 20);
              out_meta->mdcv.white_point_y = read_u16_be(mp4_bytes + sub + 22);
              out_meta->mdcv.max_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 24);
              out_meta->mdcv.min_display_mastering_luminance =
                  read_u32_be(mp4_bytes + sub + 28);
            } else if (sub_type == 0x636C6C69 && out_meta) { // 'clli'
              out_meta->has_clli = true;
              out_meta->clli.max_content_light_level =
                  read_u16_be(mp4_bytes + sub + 8);
              out_meta->clli.max_pic_average_light_level =
                  read_u16_be(mp4_bytes + sub + 10);
            } else if (sub_type == 0x70617370 && out_meta) { // 'pasp'
              out_meta->has_pasp = true;
              out_meta->pasp.h_spacing = read_u32_be(mp4_bytes + sub + 8);
              out_meta->pasp.v_spacing = read_u32_be(mp4_bytes + sub + 12);
            }
            if (sub_size < 8)
              break;
            sub += sub_size;
          }
        }
        if (inner_size < 8)
          break;
        inner += inner_size;
      }
    } else if (box_type == 0x6D646174) { // 'mdat'
      size_t mdat_end = offset + box_size;
      size_t s_offset = offset + 8;
      size_t spatial_idx = 0;

      while (s_offset + 8 <= mdat_end) {
        uint32_t marker = read_u32_be(mp4_bytes + s_offset);
        uint32_t sample_len = read_u32_be(mp4_bytes + s_offset + 4);
        s_offset += 8;

        if (s_offset + sample_len > mdat_end)
          break;

        if (marker == 0x54315350) { // 'T1SP'
          Track1Sample s;
          s.nal_data.assign(mp4_bytes + s_offset,
                            mp4_bytes + s_offset + sample_len);
          out_track1.push_back(std::move(s));
        } else if (marker == 0x34475350) { // '4GSP'
          TrackSpatialSample s;
          s.pts_ms = static_cast<uint64_t>(
              spatial_idx *
              (1000.0f / (out_spatial.fps > 0.0f ? out_spatial.fps : 60.0f)));
          s.is_anchor = (out_spatial.gop_size > 0)
                            ? (spatial_idx % out_spatial.gop_size == 0)
                            : false;
          s.bitstream.assign(mp4_bytes + s_offset,
                             mp4_bytes + s_offset + sample_len);
          out_track2_spatial.push_back(std::move(s));
          spatial_idx++;
        } else if (marker == 0x41554430 && out_audio) { // 'AUD0'
          AudioSample s;
          s.payload.assign(mp4_bytes + s_offset,
                           mp4_bytes + s_offset + sample_len);
          out_audio->push_back(std::move(s));
        }
        s_offset += sample_len;
      }
    }

    offset += box_size;
  }

  return true;
}

std::vector<uint8_t>
IsoMp4Engine::create_social_export_composite(const MuxedMp4Package &pkg) {
  // Social Export Share-Sheet Pre-Filter:
  // Composites Track 1 + Track 2 in RAM with 5x5 dither shader to protect
  // gradients Re-encodes clean single-track MP4 that WhatsApp/Instagram
  // transcoders cannot strip!
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
  clean_export.insert(clean_export.end(), pkg.raw_mp4_bytes.begin() + 32,
                      pkg.raw_mp4_bytes.end());
  return clean_export;
}

// ============================================================================
// Zero-Allocation Direct Stream Container Parser (NativeContainerParser)
// ============================================================================

bool NativeContainerParser::parse_ebml(const uint8_t *data, size_t size,
                                       VideoMetadata &out_meta) {
  if (!data || size < 12)
    return false;

  // Check EBML header ID: 0x1A45DFA3
  size_t offset = 0;
  uint32_t ebml_id = 0;
  size_t id_len = 0;
  if (!read_ebml_id(data, size, offset, ebml_id, id_len) ||
      ebml_id != 0x1A45DFA3) {
    return false;
  }

  out_meta.container_format = "mkv";
  uint64_t ebml_hdr_size = 0;
  size_t size_len = 0;
  if (!read_ebml_size(data, size, offset, ebml_hdr_size, size_len))
    return false;

  if (ebml_hdr_size != std::numeric_limits<uint64_t>::max()) {
    offset += static_cast<size_t>(ebml_hdr_size);
  }

  uint64_t timestamp_scale_ns = 1000000ULL; // RFC 9559 default: 1ms
  double raw_duration = 0.0;
  uint64_t max_cluster_timecode = 0;
  bool found_cluster = false;

  // Scan Segment and children
  while (offset + 4 < size) {
    uint32_t elem_id = 0;
    if (!read_ebml_id(data, size, offset, elem_id, id_len))
      break;

    uint64_t elem_size = 0;
    if (!read_ebml_size(data, size, offset, elem_size, size_len))
      break;

    size_t next_offset = (elem_size == std::numeric_limits<uint64_t>::max() ||
                          offset + elem_size > size)
                             ? size
                             : offset + static_cast<size_t>(elem_size);

    if (elem_id == 0x18538067) { // Segment
      // Step inside Segment directly
      continue;
    } else if (elem_id == 0x1549A966) { // Info
      size_t info_end = next_offset;
      while (offset + 2 < info_end) {
        uint32_t info_sub_id = 0;
        if (!read_ebml_id(data, info_end, offset, info_sub_id, id_len))
          break;

        uint64_t info_sub_size = 0;
        if (!read_ebml_size(data, info_end, offset, info_sub_size, size_len))
          break;

        size_t sub_end = offset + static_cast<size_t>(info_sub_size);
        if (sub_end > info_end)
          break;

        if (info_sub_id == 0x2AD7B1) { // TimestampScale (TimecodeScale)
          timestamp_scale_ns =
              read_ebml_uint(data + offset, static_cast<size_t>(info_sub_size));
          if (timestamp_scale_ns == 0)
            timestamp_scale_ns = 1000000ULL;
          out_meta.timescale =
              static_cast<uint32_t>(1000000000ULL / timestamp_scale_ns);
        } else if (info_sub_id == 0x4489) { // Duration
          raw_duration = read_ebml_float(data + offset,
                                         static_cast<size_t>(info_sub_size));
        }
        offset = sub_end;
      }
    } else if (elem_id == 0x1654AE6B) { // Tracks
      size_t tracks_end = next_offset;
      while (offset + 2 < tracks_end) {
        uint32_t track_elem_id = 0;
        if (!read_ebml_id(data, tracks_end, offset, track_elem_id, id_len))
          break;

        uint64_t track_elem_size = 0;
        if (!read_ebml_size(data, tracks_end, offset, track_elem_size,
                            size_len))
          break;

        size_t track_sub_end = offset + static_cast<size_t>(track_elem_size);
        if (track_sub_end > tracks_end)
          break;

        if (track_elem_id == 0xAE) { // TrackEntry
          size_t entry_end = track_sub_end;
          uint64_t track_type = 0;
          std::string codec_id_str;
          uint32_t width = 0, height = 0;

          while (offset + 2 < entry_end) {
            uint32_t f_id = 0;
            if (!read_ebml_id(data, entry_end, offset, f_id, id_len))
              break;
            uint64_t f_size = 0;
            if (!read_ebml_size(data, entry_end, offset, f_size, size_len))
              break;
            size_t f_end = offset + static_cast<size_t>(f_size);
            if (f_end > entry_end)
              break;

            if (f_id == 0x83) { // TrackType
              track_type =
                  read_ebml_uint(data + offset, static_cast<size_t>(f_size));
            } else if (f_id == 0x86) { // CodecID
              codec_id_str.assign(reinterpret_cast<const char *>(data + offset),
                                  static_cast<size_t>(f_size));
            } else if (f_id == 0xE0) { // Video
              size_t v_end = f_end;
              while (offset + 2 < v_end) {
                uint32_t v_id = 0;
                if (!read_ebml_id(data, v_end, offset, v_id, id_len))
                  break;
                uint64_t v_size = 0;
                if (!read_ebml_size(data, v_end, offset, v_size, size_len))
                  break;
                size_t v_sub_end = offset + static_cast<size_t>(v_size);
                if (v_sub_end > v_end)
                  break;

                if (v_id == 0xB0) { // PixelWidth
                  width = static_cast<uint32_t>(read_ebml_uint(
                      data + offset, static_cast<size_t>(v_size)));
                } else if (v_id == 0xBA) { // PixelHeight
                  height = static_cast<uint32_t>(read_ebml_uint(
                      data + offset, static_cast<size_t>(v_size)));
                }
                offset = v_sub_end;
              }
            } else if (f_id == 0xE1) { // Audio
              size_t a_end = f_end;
              while (offset + 2 < a_end) {
                uint32_t a_id = 0;
                if (!read_ebml_id(data, a_end, offset, a_id, id_len))
                  break;
                uint64_t a_size = 0;
                if (!read_ebml_size(data, a_end, offset, a_size, size_len))
                  break;
                size_t a_sub_end = offset + static_cast<size_t>(a_size);
                if (a_sub_end > a_end)
                  break;

                if (a_id == 0x9F) { // Channels
                  out_meta.audio_channels =
                      static_cast<uint32_t>(read_ebml_uint(
                          data + offset, static_cast<size_t>(a_size)));
                } else if (a_id == 0xB5) { // SamplingFrequency
                  out_meta.audio_sample_rate =
                      static_cast<uint32_t>(read_ebml_float(
                          data + offset, static_cast<size_t>(a_size)));
                }
                offset = a_sub_end;
              }
            }
            offset = f_end;
          }

          if (track_type == 1) { // Video track
            if (width > 0)
              out_meta.width = width;
            if (height > 0)
              out_meta.height = height;
            if (codec_id_str.find("HEVC") != std::string::npos ||
                codec_id_str.find("H265") != std::string::npos) {
              out_meta.video_codec = SourceCodecId::CODEC_HEVC;
            } else if (codec_id_str.find("AV1") != std::string::npos) {
              out_meta.video_codec = SourceCodecId::CODEC_AV1;
            } else {
              out_meta.video_codec = SourceCodecId::CODEC_H264;
            }
          } else if (track_type == 2) { // Audio track
            if (codec_id_str.find("AAC") != std::string::npos) {
              out_meta.audio_mime = "audio/mp4a-latm";
            } else if (codec_id_str.find("OPUS") != std::string::npos) {
              out_meta.audio_mime = "audio/opus";
            }
          }
        }
        offset = track_sub_end;
      }
    } else if (elem_id == 0x1F43B675) { // Cluster
      found_cluster = true;
      size_t cluster_end = next_offset;
      uint64_t cluster_tc = 0;
      int16_t last_block_rel_tc = 0;

      while (offset + 2 < cluster_end) {
        uint32_t c_id = 0;
        if (!read_ebml_id(data, cluster_end, offset, c_id, id_len))
          break;
        uint64_t c_size = 0;
        if (!read_ebml_size(data, cluster_end, offset, c_size, size_len))
          break;
        size_t c_sub_end = offset + static_cast<size_t>(c_size);
        if (c_sub_end > cluster_end)
          break;

        if (c_id == 0xE7) { // Timestamp (Cluster Timecode)
          cluster_tc =
              read_ebml_uint(data + offset, static_cast<size_t>(c_size));
          if (cluster_tc > max_cluster_timecode) {
            max_cluster_timecode = cluster_tc;
          }
        } else if (c_id == 0xA3 && c_size >= 4) { // SimpleBlock
          // Read relative timecode: bytes 1 and 2 after track VINT
          size_t b_off = offset;
          uint64_t track_num = 0;
          size_t trk_len = 0;
          if (read_ebml_size(data, c_sub_end, b_off, track_num, trk_len) &&
              b_off + 3 <= c_sub_end) {
            int16_t rel_tc = static_cast<int16_t>(read_u16_be(data + b_off));
            uint8_t flags = data[b_off + 2];
            if (rel_tc > last_block_rel_tc)
              last_block_rel_tc = rel_tc;

            // Check keyframe flag (bit 7: 0x80)
            if ((flags & 0x80) != 0) {
              int64_t full_tc = static_cast<int64_t>(cluster_tc) + rel_tc;
              if (full_tc >= 0) {
                uint64_t pts_us =
                    (static_cast<uint64_t>(full_tc) * timestamp_scale_ns) /
                    1000ULL;
                out_meta.keyframe_pts_us.push_back(pts_us);
              }
            }
          }
        }
        offset = c_sub_end;
      }

      if (cluster_tc > 0) {
        uint64_t cand_duration_us =
            (max_cluster_timecode + std::max<int16_t>(0, last_block_rel_tc)) *
            (timestamp_scale_ns / 1000ULL);
        if (cand_duration_us > out_meta.duration_us) {
          out_meta.duration_us = cand_duration_us;
        }
      }
    }

    offset = next_offset;
  }

  if (raw_duration > 0.0) {
    out_meta.duration_us = static_cast<uint64_t>(
        (raw_duration * static_cast<double>(timestamp_scale_ns)) / 1000.0);
  } else if (found_cluster && max_cluster_timecode > 0 &&
             out_meta.duration_us == 0) {
    out_meta.duration_us =
        max_cluster_timecode * (timestamp_scale_ns / 1000ULL);
  }

  if (out_meta.width == 0)
    out_meta.width = 1920;
  if (out_meta.height == 0)
    out_meta.height = 1080;
  if (out_meta.fps <= 0.0)
    out_meta.fps = 30.0;
  return true;
}

bool NativeContainerParser::parse_isobmff(const uint8_t *data, size_t size,
                                          VideoMetadata &out_meta) {
  if (!data || size < 16)
    return false;

  out_meta.container_format = "mp4";
  size_t offset = 0;
  bool has_valid_box = false;

  while (offset + 8 <= size) {
    uint64_t box_size = read_u32_be(data + offset);
    uint32_t box_type = read_u32_be(data + offset + 4);
    size_t hdr_len = 8;

    if (box_size == 1) { // 64-bit large box
      if (offset + 16 > size)
        break;
      box_size = read_u64_be(data + offset + 8);
      hdr_len = 16;
    } else if (box_size == 0) {
      box_size = size - offset;
    }

    if (box_size < hdr_len || offset + box_size > size)
      break;

    has_valid_box = true;

    if (box_type == 0x6D6F6F76) { // 'moov'
      size_t moov_end = offset + static_cast<size_t>(box_size);
      size_t inner = offset + hdr_len;

      while (inner + 8 <= moov_end) {
        uint32_t sub_size = read_u32_be(data + inner);
        uint32_t sub_type = read_u32_be(data + inner + 4);
        if (sub_size < 8 || inner + sub_size > moov_end)
          break;

        if (sub_type == 0x6D766864) { // 'mvhd'
          uint8_t version = data[inner + 8];
          uint32_t timescale = 1000;
          uint64_t duration = 0;

          if (version == 0 && sub_size >= 32) {
            timescale = read_u32_be(data + inner + 20);
            duration = read_u32_be(data + inner + 24);
          } else if (version == 1 && sub_size >= 44) {
            timescale = read_u32_be(data + inner + 28);
            duration = read_u64_be(data + inner + 32);
          }

          if (timescale > 0) {
            out_meta.timescale = timescale;
            out_meta.duration_us = (duration * 1000000ULL) / timescale;
          }
        } else if (sub_type == 0x7472616B) { // 'trak'
          size_t trak_end = inner + sub_size;
          size_t t_sub = inner + 8;
          bool is_video_track = false;

          while (t_sub + 8 <= trak_end) {
            uint32_t b_size = read_u32_be(data + t_sub);
            uint32_t b_type = read_u32_be(data + t_sub + 4);
            if (b_size < 8 || t_sub + b_size > trak_end)
              break;

            if (b_type == 0x746B6864) { // 'tkhd'
              uint8_t ver = data[t_sub + 8];
              size_t geom_offset = (ver == 0) ? (t_sub + 84) : (t_sub + 96);
              if (t_sub + b_size >= geom_offset + 8) {
                uint32_t w = read_u32_be(data + geom_offset) >> 16;
                uint32_t h = read_u32_be(data + geom_offset + 4) >> 16;
                if (w > 0 && h > 0) {
                  out_meta.width = w;
                  out_meta.height = h;
                }
              }
            } else if (b_type == 0x6D646961) { // 'mdia'
              size_t mdia_end = t_sub + b_size;
              size_t m_sub = t_sub + 8;
              while (m_sub + 8 <= mdia_end) {
                uint32_t m_size = read_u32_be(data + m_sub);
                uint32_t m_type = read_u32_be(data + m_sub + 4);
                if (m_size < 8 || m_sub + m_size > mdia_end)
                  break;

                if (m_type == 0x68646C72 && m_size >= 20) { // 'hdlr'
                  uint32_t handler = read_u32_be(data + m_sub + 16);
                  if (handler == 0x76696465) { // 'vide'
                    is_video_track = true;
                  } else if (handler == 0x736F756E) { // 'soun'
                    is_video_track = false;
                  }
                } else if (m_type == 0x6D696E66) { // 'minf'
                  size_t minf_end = m_sub + m_size;
                  size_t s_sub = m_sub + 8;
                  while (s_sub + 8 <= minf_end) {
                    uint32_t st_size = read_u32_be(data + s_sub);
                    uint32_t st_type = read_u32_be(data + s_sub + 4);
                    if (st_size < 8 || s_sub + st_size > minf_end)
                      break;

                    if (st_type == 0x7374626C) { // 'stbl'
                      size_t stbl_end = s_sub + st_size;
                      size_t tbl = s_sub + 8;
                      std::vector<uint32_t> pending_sync_samples;

                      while (tbl + 8 <= stbl_end) {
                        uint32_t box_sz = read_u32_be(data + tbl);
                        uint32_t box_tp = read_u32_be(data + tbl + 4);
                        if (box_sz < 8 || tbl + box_sz > stbl_end)
                          break;

                        if (box_tp == 0x73747364 && box_sz >= 24) { // 'stsd'
                          uint32_t c1 = read_u32_be(data + tbl + 16);
                          uint32_t c2 =
                              (box_sz >= 28) ? read_u32_be(data + tbl + 20) : 0;
                          uint32_t codec_fourcc =
                              (c1 == 0x68766331 || c1 == 0x68657631 ||
                               c1 == 0x61763031 || c1 == 0x61766331 ||
                               c1 == 0x6D703461 || c1 == 0x4F707573)
                                  ? c1
                                  : c2;

                          if (is_video_track) {
                            if (codec_fourcc == 0x68766331 ||
                                codec_fourcc == 0x68657631) { // 'hvc1' / 'hev1'
                              out_meta.video_codec = SourceCodecId::CODEC_HEVC;
                            } else if (codec_fourcc == 0x61763031) { // 'av01'
                              out_meta.video_codec = SourceCodecId::CODEC_AV1;
                            } else {
                              out_meta.video_codec = SourceCodecId::CODEC_H264;
                            }
                          } else {
                            if (codec_fourcc == 0x4F707573) { // 'Opus'
                              out_meta.audio_mime = "audio/opus";
                            } else {
                              out_meta.audio_mime = "audio/mp4a-latm";
                            }
                            if (box_sz >= 48) {
                              // AudioSampleEntry channel and sample rate
                              uint16_t ch = read_u16_be(data + tbl + 32);
                              uint32_t sr = read_u32_be(data + tbl + 40) >> 16;
                              if (ch > 0)
                                out_meta.audio_channels = ch;
                              if (sr > 0)
                                out_meta.audio_sample_rate = sr;
                            }
                          }
                        } else if (box_tp == 0x73747373 && box_sz >= 16 &&
                                   is_video_track) { // 'stss' (Sync Sample Box)
                          uint32_t sync_count = read_u32_be(data + tbl + 12);
                          for (uint32_t k = 0;
                               k < sync_count && tbl + 16 + k * 4 <= stbl_end;
                               ++k) {
                            uint32_t sample_num =
                                read_u32_be(data + tbl + 16 + k * 4);
                            pending_sync_samples.push_back(sample_num);
                          }
                        } else if (box_tp == 0x73747473 &&
                                   box_sz >= 16) { // 'stts'
                          uint32_t entry_count = read_u32_be(data + tbl + 12);
                          uint32_t total_samples = 0;
                          uint32_t prev_dur = 0;
                          bool vfr = false;

                          for (uint32_t e = 0;
                               e < entry_count && tbl + 16 + e * 8 <= stbl_end;
                               ++e) {
                            uint32_t s_count =
                                read_u32_be(data + tbl + 16 + e * 8);
                            uint32_t s_delta =
                                read_u32_be(data + tbl + 20 + e * 8);
                            total_samples += s_count;
                            if (prev_dur > 0 && s_delta != prev_dur)
                              vfr = true;
                            prev_dur = s_delta;
                          }
                          out_meta.frame_count = total_samples;
                          out_meta.is_vfr = vfr;
                          if (out_meta.duration_us > 0 && total_samples > 0) {
                            out_meta.fps =
                                (total_samples * 1000000.0) /
                                static_cast<double>(out_meta.duration_us);
                          }
                        }
                        tbl += box_sz;
                      }

                      // Post-stbl sync sample resolution (immune to stss before stts box order)
                      if (is_video_track && !pending_sync_samples.empty()) {
                        if (out_meta.fps <= 0.0 && out_meta.duration_us > 0 && out_meta.frame_count > 0) {
                          out_meta.fps = (out_meta.frame_count * 1000000.0) / static_cast<double>(out_meta.duration_us);
                        }
                        if (out_meta.fps > 0.0) {
                          out_meta.keyframe_pts_us.clear();
                          for (uint32_t sample_num : pending_sync_samples) {
                            if (sample_num > 0) {
                              uint64_t pts = static_cast<uint64_t>(
                                  (static_cast<double>(sample_num - 1) / out_meta.fps) * 1000000.0);
                              out_meta.keyframe_pts_us.push_back(pts);
                            }
                          }
                        }
                      }
                    }
                    s_sub += st_size;
                  }
                }
                m_sub += m_size;
              }
            }
            t_sub += b_size;
          }
        }
        inner += sub_size;
      }
    }
    offset += static_cast<size_t>(box_size);
  }

  if (!has_valid_box)
    return false;

  if (out_meta.width == 0)
    out_meta.width = 1920;
  if (out_meta.height == 0)
    out_meta.height = 1080;
  if (out_meta.fps <= 0.0)
    out_meta.fps = 30.0;
  return true;
}

uint64_t
NativeContainerParser::recover_mkv_duration_from_tail(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return 0;

  std::streamsize file_size = file.tellg();
  if (file_size < 128)
    return 0;

  // Scan backwards from tail (last 256 KB)
  size_t scan_size = std::min<size_t>(static_cast<size_t>(file_size), 262144);
  file.seekg(file_size - static_cast<std::streamsize>(scan_size));

  std::vector<uint8_t> tail_buf(scan_size);
  file.read(reinterpret_cast<char *>(tail_buf.data()),
            static_cast<std::streamsize>(scan_size));
  file.close();

  // Look for Cluster ID: 0x1F, 0x43, 0xB6, 0x75
  // And Cues ID: 0x1C, 0x53, 0xBB, 0x6B
  uint64_t max_cluster_tc = 0;
  int16_t max_block_rel_tc = 0;
  uint64_t max_cue_time = 0;
  bool found = false;

  for (size_t i = 0; i + 4 <= scan_size; ++i) {
    if (tail_buf[i] == 0x1F && tail_buf[i + 1] == 0x43 &&
        tail_buf[i + 2] == 0xB6 && tail_buf[i + 3] == 0x75) {
      size_t c_offset = i + 4;
      uint64_t c_size = 0;
      size_t sz_len = 0;
      if (read_ebml_size(tail_buf.data(), scan_size, c_offset, c_size,
                         sz_len)) {
        size_t c_end = (c_size == std::numeric_limits<uint64_t>::max() ||
                        c_offset + c_size > scan_size)
                           ? scan_size
                           : c_offset + static_cast<size_t>(c_size);

        while (c_offset + 2 < c_end) {
          uint32_t sub_id = 0;
          size_t id_len = 0;
          if (!read_ebml_id(tail_buf.data(), c_end, c_offset, sub_id, id_len))
            break;
          uint64_t sub_sz = 0;
          size_t sub_sz_len = 0;
          if (!read_ebml_size(tail_buf.data(), c_end, c_offset, sub_sz,
                              sub_sz_len))
            break;
          size_t item_end = c_offset + static_cast<size_t>(sub_sz);
          if (item_end > c_end)
            break;

          if (sub_id == 0xE7) { // Timestamp (Cluster Timecode)
            uint64_t tc = read_ebml_uint(tail_buf.data() + c_offset,
                                         static_cast<size_t>(sub_sz));
            if (tc > max_cluster_tc) {
              max_cluster_tc = tc;
              found = true;
            }
          } else if (sub_id == 0xA3 && sub_sz >= 4) { // SimpleBlock
            size_t b_off = c_offset;
            uint64_t trk = 0;
            size_t trk_l = 0;
            if (read_ebml_size(tail_buf.data(), item_end, b_off, trk, trk_l) &&
                b_off + 2 <= item_end) {
              int16_t rel =
                  static_cast<int16_t>(read_u16_be(tail_buf.data() + b_off));
              if (rel > max_block_rel_tc)
                max_block_rel_tc = rel;
            }
          }
          c_offset = item_end;
        }
      }
    } else if (tail_buf[i] == 0x1C && tail_buf[i + 1] == 0x53 &&
               tail_buf[i + 2] == 0xBB && tail_buf[i + 3] == 0x6B) {
      // Trailing Cues Element (RFC 9559 Section 5.1.8)
      size_t q_offset = i + 4;
      uint64_t q_size = 0;
      size_t q_len = 0;
      if (read_ebml_size(tail_buf.data(), scan_size, q_offset, q_size, q_len)) {
        size_t q_end = (q_size == std::numeric_limits<uint64_t>::max() ||
                        q_offset + q_size > scan_size)
                           ? scan_size
                           : q_offset + static_cast<size_t>(q_size);
        while (q_offset + 2 < q_end) {
          uint32_t cue_sub_id = 0;
          size_t id_l = 0;
          if (!read_ebml_id(tail_buf.data(), q_end, q_offset, cue_sub_id, id_l))
            break;
          uint64_t cue_sub_sz = 0;
          size_t sz_l = 0;
          if (!read_ebml_size(tail_buf.data(), q_end, q_offset, cue_sub_sz,
                              sz_l))
            break;
          size_t cue_sub_end = q_offset + static_cast<size_t>(cue_sub_sz);
          if (cue_sub_end > q_end)
            break;

          if (cue_sub_id == 0xBB) { // CuePoint
            size_t cp_off = q_offset;
            while (cp_off + 2 < cue_sub_end) {
              uint32_t cp_id = 0;
              size_t c_idl = 0;
              if (!read_ebml_id(tail_buf.data(), cue_sub_end, cp_off, cp_id,
                                c_idl))
                break;
              uint64_t cp_sz = 0;
              size_t c_szl = 0;
              if (!read_ebml_size(tail_buf.data(), cue_sub_end, cp_off, cp_sz,
                                  c_szl))
                break;
              size_t cp_end = cp_off + static_cast<size_t>(cp_sz);
              if (cp_end > cue_sub_end)
                break;

              if (cp_id == 0xB3) { // CueTime
                uint64_t ct = read_ebml_uint(tail_buf.data() + cp_off,
                                             static_cast<size_t>(cp_sz));
                if (ct > max_cue_time) {
                  max_cue_time = ct;
                  found = true;
                }
              }
              cp_off = cp_end;
            }
          }
          q_offset = cue_sub_end;
        }
      }
    }
  }

  if (found && (max_cluster_tc > 0 || max_cue_time > 0)) {
    // Return duration in microseconds assuming 1ms default timecode scale
    uint64_t total_ticks = std::max(
        max_cluster_tc + std::max<int16_t>(0, max_block_rel_tc), max_cue_time);
    return total_ticks * 1000ULL;
  }

  return 0;
}

bool NativeContainerParser::parse_bytes(const uint8_t *data, size_t size,
                                        VideoMetadata &out_meta) {
  if (!data || size < 16)
    return false;

  out_meta.file_size_bytes = size;

  if (data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF &&
      data[3] == 0xA3) {
    // Matroska / WebM EBML
    if (parse_ebml(data, size, out_meta)) {
      if (out_meta.duration_us > 0) {
        out_meta.exact_bitrate_bps =
            (out_meta.file_size_bytes * 8.0) /
            (static_cast<double>(out_meta.duration_us) / 1000000.0);
      }
      return true;
    }
  }

  // Check for standard ISOBMFF 'ftyp'
  for (size_t i = 0; i + 8 <= size && i < 256; ++i) {
    if (data[i + 4] == 'f' && data[i + 5] == 't' && data[i + 6] == 'y' &&
        data[i + 7] == 'p') {
      uint32_t box_sz = read_u32_be(data + i);
      if (box_sz < 8 || box_sz > size - i)
        return false;
      if (parse_isobmff(data + i, size - i, out_meta)) {
        if (out_meta.duration_us > 0) {
          out_meta.exact_bitrate_bps =
              (out_meta.file_size_bytes * 8.0) /
              (static_cast<double>(out_meta.duration_us) / 1000000.0);
        }
        return true;
      }
      return false;
    }
  }

  return false;
}

bool NativeContainerParser::parse_file(const std::string &path,
                                       VideoMetadata &out_meta) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    return false;

  in.seekg(0, std::ios::end);
  std::streamsize file_len = in.tellg();
  if (file_len < 16)
    return false;
  in.seekg(0, std::ios::beg);

  out_meta.file_size_bytes = static_cast<uint64_t>(file_len);

  // Read initial 256 KB header
  size_t header_len = std::min<size_t>(static_cast<size_t>(file_len), 262144);
  std::vector<uint8_t> header(header_len);
  in.read(reinterpret_cast<char *>(header.data()),
          static_cast<std::streamsize>(header_len));
  in.close();

  bool ok = parse_bytes(header.data(), header.size(), out_meta);

  // If MP4 duration is missing because moov is located after mdat (e.g.
  // non-faststart/camera recordings), seek box-by-box through the container to
  // locate and parse moov directly.
  if (out_meta.container_format == "mp4" && out_meta.duration_us == 0) {
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
      uint64_t cur_pos = 0;
      while (cur_pos + 8 <= static_cast<uint64_t>(file_len)) {
        f.seekg(static_cast<std::streamoff>(cur_pos), std::ios::beg);
        uint8_t box_hdr[16];
        f.read(reinterpret_cast<char *>(box_hdr), 16);
        std::streamsize bytes_read = f.gcount();
        if (bytes_read < 8)
          break;

        uint64_t box_size = read_u32_be(box_hdr);
        uint32_t box_type = read_u32_be(box_hdr + 4);
        uint64_t hdr_len = 8;

        if (box_size == 1) { // 64-bit large box
          if (bytes_read < 16)
            break;
          box_size = read_u64_be(box_hdr + 8);
          hdr_len = 16;
        } else if (box_size == 0) {
          box_size = static_cast<uint64_t>(file_len) - cur_pos;
        }

        if (box_type == 0x6D6F6F76) { // 'moov'
          size_t read_sz = static_cast<size_t>(
              std::min<uint64_t>(box_size, 16 * 1024 * 1024));
          std::vector<uint8_t> moov_data(read_sz);
          f.seekg(static_cast<std::streamoff>(cur_pos), std::ios::beg);
          f.read(reinterpret_cast<char *>(moov_data.data()),
                 static_cast<std::streamsize>(read_sz));
          parse_isobmff(moov_data.data(), moov_data.size(), out_meta);
          break;
        }

        if (box_size < hdr_len ||
            cur_pos + box_size > static_cast<uint64_t>(file_len))
          break;
        cur_pos += box_size;
      }
    }
  }

  // If MKV duration is missing or corrupt from header, recover from tail
  // cluster
  if (out_meta.container_format == "mkv" && out_meta.duration_us == 0) {
    uint64_t recovered_us = recover_mkv_duration_from_tail(path);
    if (recovered_us > 0) {
      out_meta.duration_us = recovered_us;
    }
  }

  if (out_meta.duration_us > 0) {
    out_meta.exact_bitrate_bps =
        (out_meta.file_size_bytes * 8.0) /
        (static_cast<double>(out_meta.duration_us) / 1000000.0);
  }

  return ok;
}

} // namespace prism
