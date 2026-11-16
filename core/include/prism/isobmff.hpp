#pragma once
#ifndef PRISM_ISOBMFF_HPP
#define PRISM_ISOBMFF_HPP

#include "prism/types.hpp"
#include "prism/afgs1.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace prism {

/**
 * @brief ISO/IEC 14496-12 (ISOBMFF) Box Header.
 */
struct IsoBoxHeader {
    uint32_t size = 8;
    uint32_t type = 0; // 4-character code (FourCC)
};

/**
 * @brief Colour Information Box ('colr') per Section 6.2 Metadata Preservation Ledger.
 * Holds NCLX / CICP colour primaries and matrix coefficients.
 */
struct ColrBox {
    std::string colour_type = "nclx";
    uint16_t colour_primaries = 1;         // 1 = BT.709, 9 = BT.2020
    uint16_t transfer_characteristics = 1; // 1 = BT.709, 14 = BT.2020-10, 16 = SMPTE ST 2084 (PQ), 18 = HLG
    uint16_t matrix_coefficients = 1;      // 1 = BT.709, 9 = BT.2020 non-constant luminance
    bool full_range_flag = false;          // 0 = narrow range (16-235), 1 = full range (0-255)
};

/**
 * @brief Mastering Display Colour Volume Box ('mdcv') per Section 6.2.
 * SMPTE ST 2086 static HDR metadata.
 */
struct MdcvBox {
    uint16_t display_primaries_x[3] = {34000, 13250, 7500}; // R, G, B normalized 0.00002 units
    uint16_t display_primaries_y[3] = {16000, 34500, 3000};
    uint16_t white_point_x = 15635; // D65 (0.3127)
    uint16_t white_point_y = 16450; // D65 (0.3290)
    uint32_t max_display_mastering_luminance = 10000000; // in 0.0001 cd/m^2 (1000 nits)
    uint32_t min_display_mastering_luminance = 50;       // in 0.0001 cd/m^2 (0.005 nits)
};

/**
 * @brief Content Light Level Information Box ('clli') per Section 6.2.
 * CTA-861.3 static HDR metadata.
 */
struct ClliBox {
    uint16_t max_content_light_level = 1000;    // MaxCLL in cd/m^2
    uint16_t max_pic_average_light_level = 400; // MaxFALL in cd/m^2
};

/**
 * @brief Pixel Aspect Ratio Box ('pasp') per Section 6.2.
 */
struct PaspBox {
    uint32_t h_spacing = 1;
    uint32_t v_spacing = 1;
};

/**
 * @brief Track Header Matrix for Display Geometry (rotation 0, 90, 180, 270 degrees).
 */
struct TrackMatrix {
    std::array<int32_t, 9> matrix = {
        0x00010000, 0, 0,
        0, 0x00010000, 0,
        0, 0, 0x40000000
    };

    static TrackMatrix create_rotation(int degrees);
    int get_rotation_degrees() const;
};

/**
 * @brief Audio Sample for Stream Copy preservation per Section 6.2.
 */
struct AudioSample {
    uint64_t pts_ms = 0;
    std::vector<uint8_t> payload;
};

/**
 * @brief Prism Micro-Residual Box ('prmr').
 * Standard ISOBMFF extension box signaling Track 2 parameters.
 */
struct PrismMicroResidualBox {
    uint8_t  prism_version = 1;
    uint8_t  profile_idc   = 0x01; // Main10 Hybrid
    uint16_t max_macs_per_pixel = 600;
    uint8_t  latent_pyramid_levels = 3;
    uint8_t  mlp_hidden_dim = 12;
    uint16_t gop_size = 60;
};

/**
 * @brief Prism Generative Semantic Box ('pgen') per Frontier 3.
 * Signals 3D keypoint configuration and JVET VSEI SEI message mapping.
 */
struct PrismGenBox {
    uint8_t  prism_version = 2;
    uint8_t  keypoint_count = 32;
    uint16_t anchor_interval = 240;
    uint8_t  fsq_stage1_levels = 15;
    uint8_t  fsq_stage2_levels = 7;
    uint16_t max_macs_per_pixel = 450;
    bool     has_audio_concealment = true;
};

/**
 * @brief Prism Spatial 4D Gaussian Splatting Box ('4dgs') per Frontier 4.
 * Signals 4D Gaussian volumetric field properties, polynomial degree, and quantization.
 */
struct PrismSpatialBox {
    uint8_t  prism_version = 1;
    uint32_t num_gaussians = 0;
    uint16_t gop_size = 30;
    uint8_t  max_poly_degree = 3;
    uint8_t  sh_degree = 2;
    uint8_t  quant_bits = 8;
    bool     has_epipolar_flow = true;
    bool     has_subspace_factorization = true;
    float    fps = 60.0f;
};

/**
 * @brief Dual-Track ISOBMFF MP4 Multiplexer & Demultiplexer.
 *
 * Implements Track 1 (Hardware AV1/HEVC, playable anywhere) and
 * Track 2 ('prsm' / 'prmr' enhancement micro-residual layer).
 */
class IsoMp4Engine {
public:
    struct Track1Sample {
        uint64_t pts_ms = 0;
        uint64_t dts_ms = 0;
        bool is_keyframe = false;
        std::vector<uint8_t> nal_data;
    };

    struct Track2Sample {
        uint64_t pts_ms = 0;
        std::vector<uint8_t> residual_latents;
    };

    struct TrackSpatialSample {
        uint64_t pts_ms = 0;
        bool is_anchor = false;
        std::vector<uint8_t> bitstream;
    };

    struct MetadataLedger {
        ColrBox colr;
        MdcvBox mdcv;
        ClliBox clli;
        TrackMatrix matrix;
        PaspBox pasp;
        bool has_colr = false;
        bool has_mdcv = false;
        bool has_clli = false;
        bool has_pasp = false;
    };

    struct MuxedMp4Package {
        std::vector<uint8_t> raw_mp4_bytes;
        PrismMicroResidualBox prmr_header;
        PrismGenBox           pgen_header;
        bool                  has_pgen = false;
        PrismSpatialBox       spatial_header;
        bool                  has_spatial = false;
        Afgs1Params afgs_params;
        MetadataLedger metadata;
        size_t track1_sample_count = 0;
        size_t track2_sample_count = 0;
        size_t spatial_sample_count = 0;
        size_t audio_sample_count = 0;
    };

    /**
     * @brief Mux Track 1 base video + Track 2 micro-residual + AFGS1 grain + Metadata Ledger into standard MP4.
     */
    static MuxedMp4Package mux_dual_track(const std::vector<Track1Sample>& track1,
                                         const std::vector<Track2Sample>& track2,
                                         const PrismMicroResidualBox& prmr,
                                         const Afgs1Params& afgs,
                                         const MetadataLedger* meta = nullptr,
                                         const std::vector<AudioSample>* audio = nullptr);

    /**
     * @brief Mux Track 1 base video + Track 2 semantic keypoints ('pgen') + AFGS1 grain + Metadata Ledger into standard MP4.
     */
    static MuxedMp4Package mux_dual_track_gen(const std::vector<Track1Sample>& track1,
                                             const std::vector<Track2Sample>& track2_gen,
                                             const PrismGenBox& pgen,
                                             const Afgs1Params& afgs,
                                             const MetadataLedger* meta = nullptr,
                                             const std::vector<AudioSample>* audio = nullptr);

    /**
     * @brief Demux dual-track MP4 to extract Track 1, Track 2, metadata boxes, and audio.
     */
    static bool demux_dual_track(const uint8_t* mp4_bytes,
                                size_t size,
                                std::vector<Track1Sample>& out_track1,
                                std::vector<Track2Sample>& out_track2,
                                PrismMicroResidualBox& out_prmr,
                                Afgs1Params& out_afgs,
                                MetadataLedger* out_meta = nullptr,
                                std::vector<AudioSample>* out_audio = nullptr);

    /**
     * @brief Demux dual-track MP4 with 'pgen' Track 2.
     */
    static bool demux_dual_track_gen(const uint8_t* mp4_bytes,
                                    size_t size,
                                    std::vector<Track1Sample>& out_track1,
                                    std::vector<Track2Sample>& out_track2,
                                    PrismGenBox& out_pgen,
                                    Afgs1Params& out_afgs,
                                    MetadataLedger* out_meta = nullptr,
                                    std::vector<AudioSample>* out_audio = nullptr);

    /**
     * @brief Mux Track 1 base video + Track 2 4D Gaussian field ('4dgs') + AFGS1 grain + Metadata Ledger into standard MP4.
     */
    static MuxedMp4Package mux_dual_track_spatial(const std::vector<Track1Sample>& track1,
                                                 const std::vector<TrackSpatialSample>& track2_spatial,
                                                 const PrismSpatialBox& spatial_box,
                                                 const Afgs1Params& afgs,
                                                 const MetadataLedger* meta = nullptr,
                                                 const std::vector<AudioSample>* audio = nullptr);

    /**
     * @brief Demux dual-track MP4 with '4dgs' Track 2.
     */
    static bool demux_dual_track_spatial(const uint8_t* mp4_bytes,
                                        size_t size,
                                        std::vector<Track1Sample>& out_track1,
                                        std::vector<TrackSpatialSample>& out_track2_spatial,
                                        PrismSpatialBox& out_spatial,
                                        Afgs1Params& out_afgs,
                                        MetadataLedger* out_meta = nullptr,
                                        std::vector<AudioSample>* out_audio = nullptr);

    /**
     * @brief Composite Track 1 and Track 2 in RAM with 5x5 dither for Social Export (WhatsApp/Instagram).
     * Eliminates auxiliary track stripping on social media upload.
     */
    static std::vector<uint8_t> create_social_export_composite(const MuxedMp4Package& pkg);
};

} // namespace prism

#endif // PRISM_ISOBMFF_HPP
