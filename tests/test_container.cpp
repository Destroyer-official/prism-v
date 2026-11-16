#include "test_harness.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/isobmff.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace prism;

PRISM_TEST(test_container_pvloss_roundtrip_and_seek) {
    std::string test_file = "test_archive.pvloss";
    if (fs::exists(test_file)) fs::remove(test_file);

    PvlossHeader header;
    header.lossless_mode = LosslessContract::CONTRACT_A_BBR;
    header.source_codec_id = SourceCodecId::CODEC_H264;
    header.orig_file_size = 512 * 1024; // 512 KB virtual MP4
    header.width = 1920;
    header.height = 1080;

    PvlossWriter writer;
    writer.open(test_file, header);

    // Write two 256 KB chunks
    std::vector<uint8_t> chunk1_comp = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<uint8_t> chunk2_comp = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    writer.write_chunk(chunk1_comp.data(), static_cast<uint32_t>(chunk1_comp.size()), 256 * 1024, 0);
    writer.write_chunk(chunk2_comp.data(), static_cast<uint32_t>(chunk2_comp.size()), 256 * 1024, 256 * 1024);
    writer.close();

    // Now read back with PvlossReader
    PvlossReader reader;
    bool ok = reader.open(test_file);
    PRISM_ASSERT(ok);
    PRISM_ASSERT_EQ(reader.header().orig_file_size, 512 * 1024);
    PRISM_ASSERT_EQ(reader.index().size(), 2);

    // Test seek lookup: offset 300,000 should map to Chunk 2 (offset 262,144)
    const PidxEntry* entry = reader.find_entry_for_offset(300000);
    PRISM_ASSERT(entry != nullptr);
    PRISM_ASSERT_EQ(entry->virtual_byte_offset, 256 * 1024);

    // Read payload and verify CRC32C
    std::vector<uint8_t> payload = reader.read_chunk_payload(*entry);
    PRISM_ASSERT_EQ(payload.size(), chunk2_comp.size());
    for (size_t i = 0; i < payload.size(); ++i) {
        PRISM_ASSERT_EQ(payload[i], chunk2_comp[i]);
    }

    reader.close();
    fs::remove(test_file);
}

PRISM_TEST(test_container_isobmff_dual_track_mux_demux) {
    std::vector<IsoMp4Engine::Track1Sample> t1(2);
    t1[0].nal_data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00};
    t1[1].nal_data = {0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x38};

    std::vector<IsoMp4Engine::Track2Sample> t2(2);
    t2[0].residual_latents = {0x10, 0x20, 0x30};
    t2[1].residual_latents = {0x40, 0x50, 0x60};

    PrismMicroResidualBox prmr;
    prmr.prism_version = 1;
    prmr.max_macs_per_pixel = 600;

    Afgs1Params afgs;
    afgs.grain_scale = 12;

    auto pkg = IsoMp4Engine::mux_dual_track(t1, t2, prmr, afgs);
    PRISM_ASSERT(!pkg.raw_mp4_bytes.empty());

    // Demux
    std::vector<IsoMp4Engine::Track1Sample> out_t1;
    std::vector<IsoMp4Engine::Track2Sample> out_t2;
    PrismMicroResidualBox out_prmr{};
    Afgs1Params out_afgs{};

    bool demux_ok = IsoMp4Engine::demux_dual_track(pkg.raw_mp4_bytes.data(),
                                                   pkg.raw_mp4_bytes.size(),
                                                   out_t1, out_t2, out_prmr, out_afgs);
    PRISM_ASSERT(demux_ok);
    PRISM_ASSERT_EQ(out_t1.size(), 2);
    PRISM_ASSERT_EQ(out_t2.size(), 2);
    PRISM_ASSERT_EQ(out_afgs.grain_scale, 12);
}

PRISM_TEST(test_container_isobmff_metadata_ledger_preservation) {
    std::vector<IsoMp4Engine::Track1Sample> t1(1);
    t1[0].nal_data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00};

    std::vector<IsoMp4Engine::Track2Sample> t2(1);
    t2[0].residual_latents = {0x11, 0x22};

    PrismMicroResidualBox prmr;
    Afgs1Params afgs;

    // Build Metadata Ledger per Section 6.2
    IsoMp4Engine::MetadataLedger meta;
    meta.has_colr = true;
    meta.colr.colour_primaries = 9;         // BT.2020
    meta.colr.transfer_characteristics = 16; // SMPTE ST 2084 (PQ)
    meta.colr.matrix_coefficients = 9;      // BT.2020
    meta.colr.full_range_flag = true;

    meta.has_mdcv = true;
    meta.mdcv.max_display_mastering_luminance = 10000000; // 1000 nits
    meta.mdcv.min_display_mastering_luminance = 50;

    meta.has_clli = true;
    meta.clli.max_content_light_level = 1000;
    meta.clli.max_pic_average_light_level = 400;

    meta.has_pasp = true;
    meta.pasp.h_spacing = 1;
    meta.pasp.v_spacing = 1;

    meta.matrix = TrackMatrix::create_rotation(90);

    // Audio stream copy
    std::vector<AudioSample> audio(2);
    audio[0].payload = {0xFF, 0xF1, 0x50, 0x80}; // AAC syncword
    audio[1].payload = {0xFF, 0xF1, 0x50, 0x81};

    auto pkg = IsoMp4Engine::mux_dual_track(t1, t2, prmr, afgs, &meta, &audio);
    PRISM_ASSERT(!pkg.raw_mp4_bytes.empty());

    // Demux and verify metadata recovery
    std::vector<IsoMp4Engine::Track1Sample> out_t1;
    std::vector<IsoMp4Engine::Track2Sample> out_t2;
    PrismMicroResidualBox out_prmr{};
    Afgs1Params out_afgs{};
    IsoMp4Engine::MetadataLedger out_meta{};
    std::vector<AudioSample> out_audio;

    bool ok = IsoMp4Engine::demux_dual_track(pkg.raw_mp4_bytes.data(),
                                            pkg.raw_mp4_bytes.size(),
                                            out_t1, out_t2, out_prmr, out_afgs,
                                            &out_meta, &out_audio);
    PRISM_ASSERT(ok);
    PRISM_ASSERT_EQ(out_t1.size(), 1);
    PRISM_ASSERT_EQ(out_t2.size(), 1);
    PRISM_ASSERT_EQ(out_audio.size(), 2);

    PRISM_ASSERT(out_meta.has_colr);
    PRISM_ASSERT_EQ(out_meta.colr.colour_primaries, 9);
    PRISM_ASSERT_EQ(out_meta.colr.transfer_characteristics, 16);
    PRISM_ASSERT(out_meta.colr.full_range_flag);

    PRISM_ASSERT(out_meta.has_mdcv);
    PRISM_ASSERT_EQ(out_meta.mdcv.max_display_mastering_luminance, 10000000);

    PRISM_ASSERT(out_meta.has_clli);
    PRISM_ASSERT_EQ(out_meta.clli.max_content_light_level, 1000);
    PRISM_ASSERT_EQ(out_meta.clli.max_pic_average_light_level, 400);

    PRISM_ASSERT(out_meta.has_pasp);
    PRISM_ASSERT_EQ(out_meta.pasp.h_spacing, 1);
    PRISM_ASSERT_EQ(out_meta.pasp.v_spacing, 1);

    PRISM_ASSERT_EQ(out_meta.matrix.get_rotation_degrees(), 90);
}

PRISM_TEST(test_container_isobmff_dual_track_gen_mux_demux) {
    std::vector<IsoMp4Engine::Track1Sample> t1(2);
    t1[0].nal_data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00};
    t1[1].nal_data = {0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x38};

    std::vector<IsoMp4Engine::Track2Sample> t2(2);
    t2[0].residual_latents = {0xAA, 0xBB, 0xCC, 0xDD};
    t2[1].residual_latents = {0x11, 0x22, 0x33, 0x44};

    PrismGenBox pgen;
    pgen.prism_version = 2;
    pgen.keypoint_count = 32;
    pgen.anchor_interval = 240;
    pgen.fsq_stage1_levels = 15;
    pgen.fsq_stage2_levels = 7;
    pgen.max_macs_per_pixel = 450;
    pgen.has_audio_concealment = true;

    Afgs1Params afgs;
    afgs.grain_scale = 10;

    IsoMp4Engine::MetadataLedger meta;
    meta.has_colr = true;
    meta.colr.colour_primaries = 1;
    meta.colr.transfer_characteristics = 1;
    meta.colr.matrix_coefficients = 1;

    std::vector<AudioSample> audio(1);
    audio[0].payload = {0xFF, 0xF1, 0x50, 0x80};

    auto pkg = IsoMp4Engine::mux_dual_track_gen(t1, t2, pgen, afgs, &meta, &audio);
    PRISM_ASSERT(!pkg.raw_mp4_bytes.empty());
    PRISM_ASSERT(pkg.has_pgen);
    PRISM_ASSERT_EQ(pkg.track1_sample_count, 2);
    PRISM_ASSERT_EQ(pkg.track2_sample_count, 2);
    PRISM_ASSERT_EQ(pkg.audio_sample_count, 1);

    // Demux and verify
    std::vector<IsoMp4Engine::Track1Sample> out_t1;
    std::vector<IsoMp4Engine::Track2Sample> out_t2;
    PrismGenBox out_pgen{};
    Afgs1Params out_afgs{};
    IsoMp4Engine::MetadataLedger out_meta{};
    std::vector<AudioSample> out_audio;

    bool ok = IsoMp4Engine::demux_dual_track_gen(pkg.raw_mp4_bytes.data(),
                                                pkg.raw_mp4_bytes.size(),
                                                out_t1, out_t2, out_pgen, out_afgs,
                                                &out_meta, &out_audio);
    PRISM_ASSERT(ok);
    PRISM_ASSERT_EQ(out_t1.size(), 2);
    PRISM_ASSERT_EQ(out_t2.size(), 2);
    PRISM_ASSERT_EQ(out_pgen.prism_version, 2);
    PRISM_ASSERT_EQ(out_pgen.keypoint_count, 32);
    PRISM_ASSERT_EQ(out_pgen.anchor_interval, 240);
    PRISM_ASSERT_EQ(out_pgen.fsq_stage1_levels, 15);
    PRISM_ASSERT_EQ(out_pgen.fsq_stage2_levels, 7);
    PRISM_ASSERT_EQ(out_pgen.max_macs_per_pixel, 450);
    PRISM_ASSERT(out_pgen.has_audio_concealment);
    PRISM_ASSERT_EQ(out_afgs.grain_scale, 10);
    PRISM_ASSERT(out_meta.has_colr);
    PRISM_ASSERT_EQ(out_audio.size(), 1);
}
