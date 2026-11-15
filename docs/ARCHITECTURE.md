# Prism-V Architectural Specification

## 📐 High-Level Architecture Overview

Prism-V is a clean-room, dual-layer media processing architecture combining a deterministic C++20 core library (`prism_core`) with an asynchronous hardware-accelerated Android MediaCodec pipeline and a Material 3 reactive control layer.

```
+-------------------------------------------------------------------------+
|                  Prism-V Material 3 Interface (30 Screens)              |
|  [ Studio Tab ]  [ Duo Player ]  [ Toolbox Hub ]  [ Matrix ]  [ Settings] |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                    Android Media Processing Services                     |
|  - PrismMediaProcessingService (FGS: mediaProcessing, onTimeout)         |
|  - HardwareVideoCompressor (Asynchronous MediaCodec, Surface rendering) |
|  - HardwareRamBufferPool (Bounded 128 MB Direct Off-Heap Allocations)   |
|  - StorageStatsChecker & BatchTrashManager (Scoped Storage Android 16)  |
+-------------------------------------------------------------------------+
                                    |
                            JNI Bridge (prism_jni)
                                    |
                                    v
+-------------------------------------------------------------------------+
|                     Prism-V Native Core (C++20 Engine)                  |
|  +--------------------+--------------------+--------------------------+ |
|  | bbr.cpp (BBR)      | rans8.cpp (rANS)   | coolchic.cpp (Neural)    | |
|  | afgs1.cpp (Grain)  | psc.cpp (Metrics)  | detint.cpp (Fixed-Pt)    | |
|  | isobmff.cpp (MP4)  | remux.cpp (Streams)| dedup_rabin.cpp (Chunks) | |
|  | vfs_proxy.cpp(VFS) | router.cpp (HW/SW) | transaction.cpp (Atomic) | |
|  +--------------------+--------------------+--------------------------+ |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                      Hardware & System Foundation                       |
|   ARM64 / x86_64 Silicon  |  16 KB Page Alignment  |  Zero Telemetry   |
+-------------------------------------------------------------------------+
```

---

## 🧩 The 13 Native C++ Modules

| Module Header | Source | Responsibilities & Invariants |
|---------------|--------|--------------------------------|
| `detint.hpp` | `detint.cpp` | **Deterministic Integer Math**: Bit-exact 64-bit fixed-point arithmetic ($\text{scale} = 2^{16}$), eliminating floating-point non-determinism across ARM64, x86_64, and NEON/AVX2. |
| `rans8.hpp` | `rans8.cpp` | **rANS 8-Bit Entropy Coder**: Streaming Asymmetric Numeral Systems entropy coder delivering Shannon-limit bit efficiency with single-cycle table lookups. |
| `bbr.hpp` | `bbr.cpp` | **BBR Video Rate Controller**: Models hardware encoder queue buffer fullness as a network bottleneck; paces frame delivery to prevent buffer bloat and frame drops. |
| `psc.hpp` | `psc.cpp` | **Perceptual Spatial Complexity**: Computes edge density, luminance variance, and high-frequency energy across macroblocks to bias QP dynamically. |
| `tile_pool.hpp` | `tile_pool.cpp` | **Frame Tiling & SIMD Dispatcher**: Partitions video frames into cache-aligned tiles ($64 \times 64$) for parallel multi-threaded worker dispatch. |
| `coolchic.hpp` | `coolchic.cpp` | **Neural Coordinate Entropy Engine**: Continuous-coordinate representation modeling spatial residuals using continuous coordinate neural decoders. |
| `afgs1.hpp` | `afgs1.cpp` | **AOM Film Grain Synthesis 1**: Strips high-frequency noise prior to quantization, generating parametric synthesis metadata for client-side grain reconstruction. |
| `container_pvloss.hpp` | `container_pvloss.cpp` | **Lossless Container Format**: Lightweight, header-compressed streaming container for intermediate atomic GOP caching. |
| `isobmff.hpp` | `isobmff.cpp` | **Zero-Copy MP4 Atom Parser**: Parses and rewrites `ftyp`, `moov`, `mvhd`, `trak`, `mdia`, `minf`, `stbl`, `stco`/`co64`, and `mdat` boxes without copying video payloads. |
| `vfs_proxy.hpp` | `vfs_proxy.cpp` | **Virtual In-Memory MP4 Proxy**: Allows ExoPlayer and media players to stream and seek through dynamically transcoded GOP chunks in RAM. |
| `transaction.hpp` | `transaction.cpp` | **Two-Phase Atomic Commit Engine**: Manages atomic stage-verify-commit operations on local storage with rollbacks upon power failure or OOM. |
| `dedup_rabin.hpp` | `dedup_rabin.cpp` | **Rabin Fingerprint Chunk Deduplication**: Identifies identical GOPs or duplicate scenes across multiple input videos using rolling hash boundaries. |
| `remux.hpp` | `remux.cpp` | **Track Remuxer & Stream Sanitizer**: Extracts, sanitizes, and repackages AAC, Opus, and H.264/HEVC elementary streams into standard MP4 containers. |
| `router.hpp` | `router.cpp` | **Adaptive HW/SW Router**: Queries device capabilities, SoC chipset profile, and thermal headroom to route GOPs to MediaCodec HW or native C++ fallback. |

---

## ⚡ Android MediaCodec Hardware Pipeline

### 1. Asynchronous Buffer Flow
`HardwareVideoCompressor.java` utilizes the Android `MediaCodec` asynchronous callback mechanism (`setCallback(MediaCodec.Callback)`):
1. **Extractor Input Queue**: Buffers read from `MediaExtractor` are passed to `codec.queueInputBuffer()`.
2. **Surface Rendering**: Decoding renders directly to an `InputSurface` bound to the encoder, executing zero-copy GPU texture transfer.
3. **Muxer Pacing**: Encoded samples are queued to `MediaMuxer` with strict monotonic PTS ordering.

### 2. Timebase Normalization & Audio Synchronization
- **Zero-Base Snapping**: `firstVideoPtsUs` and `firstAudioPtsUs` are recorded on the initial keyframe and subtracted from subsequent timestamps, guaranteeing that exported video starts at $PTS = 0\mu\text{s}$.
- **Speed Multiplier Scaling**: Audio PTS values are scaled dynamically by $PTS_{\text{audio}}' = \frac{PTS_{\text{audio}}}{\text{speedMultiplier}}$ to remain in sample-exact lockstep with scaled video frames.

### 3. Android 15 & 16 (API 36) Compliance
- **Foreground Service Type**: Declared as `android:foregroundServiceType="mediaProcessing"`.
- **Quota Timeout Guard**: Overrides `Service.onTimeout(int startId, int fgsType)` to perform clean GOP finalization and state flush before OS termination.
- **Off-Heap RAM Budgeting**: `HardwareRamBufferPool` enforces a hard 128 MB direct memory ceiling with LRU reclamation.
