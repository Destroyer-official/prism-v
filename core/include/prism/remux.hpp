#pragma once
#ifndef PRISM_REMUX_HPP
#define PRISM_REMUX_HPP

#include "prism/types.hpp"
#include "prism/isobmff.hpp"
#include "prism/bbr.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace prism {

/**
 * @brief Mode L-Remux: Packet-Preserving Remux Engine (Contract B).
 *
 * Guaranteed 100% preservation of encoded video and audio NAL packet
 * payloads without decoding or re-quantization.
 * Used for fast container repacking, audio passthrough, or when
 * Mode L-BBR bitstream syntax savings yield < 3%.
 */
class RemuxEngine {
public:
    enum class MediaType : uint8_t {
        VIDEO_NAL = 0x01,
        AUDIO_FRAME = 0x02,
        TIMED_METADATA = 0x03
    };

    struct RemuxPacket {
        MediaType type = MediaType::VIDEO_NAL;
        uint64_t pts_ms = 0;
        uint64_t dts_ms = 0;
        bool is_keyframe = false;
        std::vector<uint8_t> payload;
    };

    struct RemuxPackage {
        std::vector<RemuxPacket> packets;
        IsoMp4Engine::MetadataLedger metadata;
        Sha256Digest payload_hash{};
        size_t video_packet_count = 0;
        size_t audio_packet_count = 0;
    };

    /**
     * @brief Ingest check: returns true if estimated bitstream savings < 3% (0.03).
     */
    static bool should_use_remux(double estimated_bbr_savings_ratio);

    /**
     * @brief Computes combined SHA-256 digest over all packet payloads.
     */
    static Sha256Digest compute_payload_hash(const std::vector<RemuxPacket>& packets);

    /**
     * @brief Remux packets into a standardized, clean ISOBMFF container.
     */
    static std::vector<uint8_t> remux_to_clean_isobmff(const RemuxPackage& pkg);

    /**
     * @brief Parse an ISOBMFF stream and extract RemuxPackage with exact payloads.
     */
    static bool parse_and_verify_isobmff(const uint8_t* mp4_data, size_t size, RemuxPackage& out_pkg);

    /**
     * @brief Verify Contract B guarantee: 100% bit-exact packet payload identity.
     */
    static bool verify_packet_preservation(const RemuxPackage& src, const RemuxPackage& dst);
};

} // namespace prism

#endif // PRISM_REMUX_HPP
