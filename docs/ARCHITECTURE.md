# Prism-V Subsystem Architecture & Engineering Specification

This document provides a comprehensive architectural breakdown of the Prism-V media engine, Android presentation stack, JNI interoperability layer, and native C++20 core.

---

## 1. System Layers

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Android Presentation Layer                      │
│  MainActivity • DuoComparisonPlayerView • CompressedVideosView         │
│  DeveloperMatrixView • LayeredSettingsView • DialogTracker             │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Application Context / Intents
┌───────────────────────────────────▼────────────────────────────────────┐
│                    Service & Asynchronous Worker Layer                 │
│  PrismMediaProcessingService (Foreground Service with Sticky Priority) │
│  HardwareVideoCompressor (MediaCodec surface-to-surface transcode)     │
│  RealtimeHardwareMonitor (CPU, GPU, thermal, and power budgeting)      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ JNI Bridge (core/src/prism_jni.cpp)
┌───────────────────────────────────▼────────────────────────────────────┐
│                   Native Core Engine (prism_core C++20)                │
│                                                                        │
│   ┌────────────────────┐ ┌────────────────────┐ ┌───────────────────┐  │
│   │   rans8 (SIMD)     │ │   bbr (Bit-Budget) │ │   psc (Wavelet)   │  │
│   └────────────────────┘ └────────────────────┘ └───────────────────┘  │
│   ┌────────────────────┐ ┌────────────────────┐ ┌───────────────────┐  │
│   │  container_pvloss  │ │  isobmff / remux   │ │  dedup_rabin      │  │
│   └────────────────────┘ └────────────────────┘ └───────────────────┘  │
│   ┌────────────────────┐ ┌────────────────────┐ ┌───────────────────┐  │
│   │     vfs_proxy      │ │    transaction     │ │  detint (Integer) │  │
│   └────────────────────┘ └────────────────────┘ └───────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│             OS, Memory, & Hardware Layer (Android / Linux / Windows)   │
│   16 KiB Memory Page Alignment • MediaCodec / NDK • Storage Access     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Duo Comparison Deck Architecture (`DuoComparisonPlayerView`)

The Duo Comparison Deck provides frame-accurate visual and analytical comparison between source footage and compressed media.

### Synchronization Subsystem
- **Master/Slave Model**: The original (uncompressed) stream acts as the reference master; the compressed stream acts as the slave.
- **Hardware-Level Frame Seek**:
  - Replaces traditional keyframe-only seeking with atomic `seekTo(targetMs, MediaPlayer.SEEK_CLOSEST)`.
  - On pause events, the master decoder's presentation timestamp is read, and the slave decoder is commanded to seek to the identical millisecond timestamp.
  - Achieves zero-drift synchronization ($\Delta = 0\text{ ms}$).
- **State-Safe Speed Rescaling**:
  - Android's native `MediaPlayerService` automatically switches from `PAUSED` to `STARTED` if `setPlaybackParams()` is called with a non-zero speed while paused.
  - The synchronization engine guards all speed changes with active playback checks (`if (!isPlaying || !mp.isPlaying()) return;`), preventing accidental background unpauses.

### Display Modes & Surface Management
1. **Curtain Mode**:
   - Both streams render simultaneously to layered `TextureView` surfaces.
   - The top surface (`viewCompressed`) is clipped dynamically using `Canvas.clipRect()` based on the divider coordinate.
   - Divider handles suppress parent container touch interception (`requestDisallowInterceptTouchEvent(true)`) to guarantee fluid swipes.
2. **Dual Mode**:
   - Splits the viewport vertically into side-by-side viewports.
   - Calculates custom scale and translation matrices based on measured surface dimensions (`tvW`, `tvH`) rather than raw container bounds, preserving proper 16:9 and 9:16 aspect ratios.
3. **Peek Mode**:
   - Top stream visibility is toggled instantaneously on press down and restored on finger lift.
4. **Solo Mode**:
   - Single-tap sequential toggling between master and slave streams.
5. **Fullscreen Cinema**:
   - Maximizes video surfaces to full screen, hides chrome, and expands touch zones for high-precision inspection.

---

## 3. Hardware Video Compression Pipeline (`HardwareVideoCompressor`)

Prism-V leverages Android's asynchronous `MediaCodec` architecture for high-performance hardware video encoding.

1. **Extraction**: `MediaExtractor` demuxes audio and video tracks from source containers.
2. **Decoding**: Hardware decoder surface outputs uncompressed frames directly to an input `Surface` configured on the hardware encoder.
3. **Encoding**:
   - Codec targets: `video/hevc` (H.265), `video/avc` (H.264), or `video/av01` (AV1).
   - Rate control: `BITRATE_MODE_VBR` (Variable Bitrate) with dynamic bitrate allocation computed based on resolution, framerate, and motion complexity.
4. **Muxing**: Compressed elementary streams and audio samples are multiplexed into a sanitized MP4 container via `MediaMuxer`.
5. **Telemetry Monitoring**: `RealtimeHardwareMonitor` polls system thermal status (`ThermalManager`), CPU load, and battery power envelope to prevent thermal throttling.

---

## 4. Native C++20 Core Library (`prism_core`)

### `detint` (Deterministic Integer Engine)
- Provides 100% overflow-free 32-bit fixed-point arithmetic for entropy models and rate estimators.
- Includes a 512-entry static sigmoid cumulative distribution function (CDF) lookup table.
- Guarantees bit-exact reproducibility across ARM64, x86_64, and RISC-V architectures.

### `rans8` (8-Lane SIMD rANS Entropy Coder)
- Implements an 8-lane interleaved asymmetric numeral systems (ANS) coder with 16-bit state renormalization.
- Validates frequency distributions contiguously summing to $32,768$ ($2^{15}$).
- Enforces strict input validation to prevent buffer underflows or corrupt bitstream crashes.

### `bbr` (Bit-Budget Rate Controller)
- Compresses arbitrary bitstreams via byte-to-nibble symbolization.
- Employs an adaptive integer probability model coupled with rANS-8 entropy coding.
- Guaranteed zero-expansion invariant: if compressed output exceeds input size, the engine falls back to an uncompressed raw payload.

### `psc` (Prism Stream Codec)
- Lossless image and video frame coder utilizing:
  1. Reversible integer YCoCg-R color transform:
     $$Y = \lfloor (R + 2G + B) / 4 \rfloor, \quad Co = R - B, \quad Cg = G - \lfloor (R + B) / 2 \rfloor$$
  2. 2D LeGall 5/3 integer lifting wavelet transform.
  3. Median Edge Detection (MED) spatial predictor.
  4. rANS entropy coding.
- Supports 8-bit RGB frames and 4-plane RAW Bayer CFA sensor data.

### `container_pvloss` (`.pvloss` Specification)
- Self-contained binary container format consisting of:
  - 72-byte fixed header (magic `PVLS`, version, flags, frame count, original SHA-256 hash).
  - CRC32C-framed payload chunks.
  - 28-byte per entry seek table for $O(1)$ random-access playback.

### `dedup_rabin` (Content-Defined Deduplication Vault)
- Implements variable-block Rabin rolling hash chunking.
- Chunks are stored in a content-addressed vault referenced by SHA-256 digests.
- Reconstruction verifies each chunk's integrity prior to restoration.

### `transaction` (Verified Replacement Engine)
- Implements two-phase commit file replacement:
  1. Writes compressed output to an isolated `.tmp` file.
  2. Verifies cryptographic hashes and container integrity.
  3. Creates a rollback journal and swaps files atomically using OS-level atomic renames.

---

## 5. Memory Management & Alignment Standards

- **16 KiB Memory Page Alignment**:
  Android 15+ mandates 16 KiB page size support for all native shared libraries. All ELF binaries in Prism-V are linked with:
  `-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384`
- **Zero-Allocation Inner Loops**:
  All per-frame compression routines operate on pre-allocated buffers managed by `tile_pool`, preventing heap fragmentation and GC pauses during real-time streaming.
