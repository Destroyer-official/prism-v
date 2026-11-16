# Prism-V: 100% Comprehensive 6-Agent Project Coverage & File Ownership Matrix
## Complete System Breakdown: Zero Gaps, Zero Overlap, 100% Surface Area Covered

**Document Path:** `synthesis/INDEPENDENT_6_AGENT_WORK_PARTITION.md`  
**Date:** November 16, 2026  
**Status:** Canonical 6-Agent Production Governance & File Ownership Specification

---

## 1. Governance & 100% Coverage Principle

The entire Prism-V platform is partitioned into **6 specialized, non-overlapping workstreams**. Together, they cover **100.00% of the repository**—every native C++ file, Java class, Vulkan shader, test harness, build script, and UI screen is accounted for with an unambiguous owner.

```
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    100% TOTAL REPOSITORY COVERAGE                                      │
├──────────────────┬─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 1          │ Systems, Heterogeneous Compute, Massive 1-Hr 4K120 Slicing, Build Pipeline & FGS    │
├──────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 2          │ Rate Control, Container Demuxing (EBML/ISOBMFF), Studio UI & 234MB->602MB Bug Fix   │
├──────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 3          │ Mode L-BBR 2.0 Bitstream Syntax Recompression (H.264/HEVC/AV1) & VFS RAM Streaming │
├──────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 4          │ Mode L-PSC 2.0 Pixel-Exact Sensor Codec (Camera2 RAW/YUV) & Directional Wavelets   │
├──────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 5          │ Hybrid Neural Split-Stream (MediaCodec Base + Cool-chic 5.0 + AFGS1) & AI Copilot   │
├──────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
│ AGENT 6          │ Volumetric 4DGS Spatial Video (6-DoF), Duo Comparison Deck & Rabin-Mesh Dedup Vault │
└──────────────────┴─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Complete Inventory & File Ownership (Every Single File in the Project)

### Agent 1: Systems Infrastructure, Heterogeneous Compute & 1-Hr 4K120 Engine
**Mission:** Ensure massive video workloads (1 hr 4K 120 fps = 432,000 frames) run fast without memory crashes or thermal throttling, governed by user resource budgets.
- **Native C++ & Shaders:**
  - `core/src/frontier5_pipeline.cpp` & `core/include/prism/frontier5_pipeline.hpp`
  - `core/src/tile_pool.cpp` & `core/include/prism/tile_pool.hpp`
  - `core/shaders/frontier5_pipeline.comp`
- **Android Services & Runtime:**
  - `android_app/src/com/prismv/worker/PrismMediaProcessingService.java` (Android 15+ 6-hour FGS lifecycle)
  - `android_app/src/com/prismv/worker/PrismJobService.java`
  - `android_app/src/com/prismv/scheduler/PrismScheduler.java` (Charging/idle triggers)
  - `android_app/src/com/prismv/engine/SiliconFusedPipelineEngine.java`
  - `android_app/src/com/prismv/engine/VideoProcessingPipeline.java`
  - `android_app/src/com/prismv/engine/HardwareRamBufferPool.java`
  - `android_app/src/com/prismv/engine/RealtimeHardwareMonitor.java`
  - `android_app/src/com/prismv/thermal/ThermalGearShifter.java` (ADPF thermal throttling guard)
  - `android_app/src/com/prismv/notification/PrismNotificationManager.java` & `PrismQuickTileService.java`
  - `android_app/src/com/prismv/test/TestRunnerReceiver.java`
- **UI & Telemetry:**
  - `android_app/src/com/prismv/ui/DeveloperMatrixView.java` (Telemetry HUD & CABAC Histograms)
  - `android_app/src/com/prismv/ui/LayeredSettingsView.java` (User RAM/CPU/GPU resource budget sliders)
  - `android_app/src/com/prismv/ui/PrismPreferences.java`, `SkinManager.java`, `ThemeStyles.java`
- **Build Infrastructure & Tooling:**
  - `CMakeLists.txt` (16 KiB page-size linker flags, C++20 build configuration)
  - `build_apk.ps1`, `build.gradle.kts`, `.github/workflows/ci.yml`
  - `tests/test_frontier5_pipeline.cpp`, `tests/test_harness.hpp`, `tests/test_main.cpp`
  - `tools/prism_cli.cpp` (core systems commands & audit runners)

---

### Agent 2: Rate Control, Container Parsing, Studio Hub & Bug Fix
**Mission:** Permanently eliminate the file expansion bug (234 MB $\to$ 602.1 MB), guarantee output sizes within $\pm 3\%$, provide direct EBML/ISOBMFF container parsing, and govern the Compression Studio.
- **Native C++ & Muxers:**
  - `core/src/isobmff.cpp` & `core/include/prism/isobmff.hpp` (direct atom parsing)
  - `core/src/remux.cpp` & `core/include/prism/remux.hpp` (clean packet remuxing)
  - `core/src/router.cpp` & `core/include/prism/router.hpp` (content router)
- **Android Compression Engine & Audio:**
  - `android_app/src/com/prismv/engine/HardwareVideoCompressor.java` (Never-Worse bitrate ceiling)
  - `android_app/src/com/prismv/engine/AudioTranscoder.java` (Stereo/Mono, AAC/Opus transcode)
  - `android_app/src/com/prismv/engine/FpsExtractor.java` (VFR detection & timestamp normalizer)
  - `android_app/src/com/prismv/provider/PrismFileProvider.java`
- **UI Studio & Forensic Tools:**
  - `android_app/src/com/prismv/ui/MainActivity.java` (Compression Studio Hub & parameter dials)
  - `android_app/src/com/prismv/ui/PreFlightDialog.java` (0.3s 2-pass micro-probe simulator)
  - `android_app/src/com/prismv/ui/ToolboxDialogs.java` (Subframe trimmer, Fit-to-Size, Privacy Blur)
  - `android_app/src/com/prismv/ui/PremiumTimelineView.java` (Video scrubber & keyframe cutter)
  - `android_app/src/com/prismv/ui/CompletionDossierDialog.java`, `CustomDarkDialog.java`, `DialogTracker.java`, `PrismHud.java`
- **Tests & Verification Tools:**
  - `tests/test_remux.cpp`, `tests/test_router.cpp`, `tests/test_robustness.cpp`
  - `tools/verify_real_video.cpp`

---

### Agent 3: Mode L-BBR 2.0 (Lossless Bitstream Syntax Recompression)
**Mission:** Recompress existing H.264, HEVC, and AV1 video bitstreams by 15%–35% with 100.000% SHA-256 byte parity, served on-the-fly at $>600\text{ MB/s}$ via virtual RAM.
- **Native C++ & SIMD Assembly:**
  - `core/src/bbr.cpp` & `core/include/prism/bbr.hpp`
  - `core/src/bbr2.cpp` & `core/include/prism/bbr2.hpp` (SSG-CR4 Attentive Spatial Syntax Graph)
  - `core/src/rans8.cpp` & `core/include/prism/rans8.hpp` (8-way SIMD rANS)
  - `core/src/rans16.cpp` & `core/include/prism/rans16.hpp` (16-way SIMD rANS)
  - `core/src/arm_sme2_rans16.S` (Hand-tuned ARMv9 SME2 outer product assembly)
  - `core/src/container_pvloss.cpp` & `core/include/prism/container_pvloss.hpp` (.pvloss chunk format)
  - `core/src/vfs_proxy.cpp` & `core/include/prism/vfs_proxy.hpp` (RAM streaming proxy)
- **Android VFS & Codec Classes:**
  - `android_app/src/com/prismv/vfs/PrismVfsProxy.java` (`StorageManager.openProxyFileDescriptor`)
  - `android_app/src/com/prismv/bbr/BbrCodec.java`
  - `android_app/src/com/prismv/rans/Rans8Codec.java`
- **Tests & Experiments:**
  - `tests/test_bbr.cpp`, `tests/test_bbr2.cpp`, `tests/test_rans8.cpp`, `tests/test_rans16.cpp`, `tests/test_container.cpp`, `tests/test_vfs.cpp`
  - `tools/exp_1a_bbr.cpp`, `tools/exp_1a_bbr2.cpp`

---

### Agent 4: Mode L-PSC 2.0 (Directional Wavelet Sensor Codec)
**Mission:** Deliver true pixel-exact ($PSNR = \infty$) compression for Camera2 uncompressed YUV and Quad-Bayer RAW video, compressing to $<22\%$ raw size at 120 fps.
- **Native C++ & Primitives:**
  - `core/src/psc.cpp` & `core/include/prism/psc.hpp` (Reversible YCoCg-R & LeGall 5/3 wavelets)
  - `core/src/psc2.cpp` & `core/include/prism/psc2.hpp` (ADIW 5/3 & 9/7 directional integer wavelets)
  - `core/src/detint.cpp` & `core/include/prism/detint.hpp` (Bounded integer arithmetic core)
- **Android Sensor Ingest & Bridge:**
  - `android_app/src/com/prismv/detint/DetInt.java`
- **Tests & Experiments:**
  - `tests/test_psc.cpp`, `tests/test_psc2.cpp`, `tests/test_detint.cpp`
  - `tools/exp_2a_psc2.cpp`

---

### Agent 5: Hardware-Anchored Hybrid Neural Split-Stream Engine
**Mission:** Deliver -45% to -70% BD-rate vs HEVC by combining hardware MediaCodec ASICs with ultra-lean Cool-chic 5.0 micro-residuals (<600 MAC/pixel), AOMedia AFGS1 grain, and Gemini AI Copilot.
- **Native C++ & Shaders:**
  - `core/src/coolchic.cpp` & `core/include/prism/coolchic.hpp` (Cool-chic 5.0 synthesis engine)
  - `core/src/afgs1.cpp` & `core/include/prism/afgs1.hpp` (AOMedia AFGS1 noise separation & synthesis)
  - `core/src/rfsq.cpp` & `core/include/prism/rfsq.hpp` (Bounded Integer-Scaled Residual FSQ)
  - `core/src/prism_gen.cpp` & `core/include/prism/prism_gen.hpp` (32-landmark semantic codec @ 1.5 kbps)
  - `core/src/nerv_engine.cpp` & `core/include/prism/nerv_engine.hpp` (NeRV-S continuous coordinate fields)
  - `core/shaders/detint_flow_gen.comp` (1-step distilled flow matching generator shader)
- **Android Neural Workers & AI Copilot:**
  - `android_app/src/com/prismv/worker/PrismNeRVBackgroundWorker.java` (Overnight charging overfit worker)
  - `android_app/src/com/prismv/ai/GeminiCopilot.java`, `GeminiClient.java`, `GeminiCopilotDialog.java`
- **Tests, Python Training & Experiments:**
  - `tests/test_coolchic.cpp`, `tests/test_afgs1.cpp`, `tests/test_rfsq.cpp`, `tests/test_prism_gen.cpp`, `tests/test_nerv.cpp`
  - `tools/qat_coolchic_training.py`
  - `tools/exp_1b_lossy.cpp`, `tools/exp_1c_frontier3.cpp`

---

### Agent 6: Volumetric 4DGS Spatial Video & Deduplication Vault
**Mission:** Provide 6-DoF continuous spatial video at 18–32 Mbps with mobile Vulkan rendering, synchronized Duo Comparison inspection, and 40%–60% camera roll storage reclamation.
- **Native C++ & Vulkan Shaders:**
  - `core/src/spatial.cpp` & `core/include/prism/spatial.hpp` (4DGS Chebyshev trajectory tracking)
  - `core/shaders/spatial_rasterize.comp` (Mobile Vulkan 1.3/1.4 compute rasterizer)
  - `core/src/dedup_rabin.cpp` & `core/include/prism/dedup_rabin.hpp` (Rabin rolling hash engine)
  - `core/src/rabin_mesh.cpp` & `core/include/prism/rabin_mesh.hpp` (VectorCDC SIMD spatio-temporal chunker)
  - `core/src/transaction.cpp` & `core/include/prism/transaction.hpp` (2-phase commit atomic journal)
- **Android Spatial Player, Vault & History:**
  - `android_app/src/com/prismv/spatial/PrismSpatialEngine.java`
  - `android_app/src/com/prismv/worker/RabinMeshWorker.java` (Background gallery deduplication worker)
  - `android_app/src/com/prismv/storage/PrismStorageManager.java` & `PrismWorkJournal.java`
  - `android_app/src/com/prismv/transaction/VerifiedReplacement.java`
  - `android_app/src/com/prismv/history/CompressionHistoryManager.java`
  - `android_app/src/com/prismv/ui/DuoComparisonPlayerView.java` (Duo Player Deck: Curtain, Dual, Peek, Spatial 6-DoF)
  - `android_app/src/com/prismv/ui/CompressedVideosView.java` (Gallery management & deduplicated library view)
- **Tests & Verification Tools:**
  - `tests/test_spatial.cpp`, `tests/test_dedup.cpp`, `tests/test_rabin_mesh.cpp`, `tests/test_transaction.cpp`
  - `tools/verify_mesh_dedup.cpp`, `tools/exp_1d_frontier4.cpp`, `tools/exp_1e_frontier6.cpp`

---

## 3. Shared Cross-Cutting Files (Bridge Contracts)
Only two files serve as bridge headers across subsystems, governed by strict ABI compatibility:
1. `core/include/prism/types.hpp`: Common data structs (`VideoMetadata`, `FrameGeometry`, `ColorFormat`).
2. `core/src/prism_jni.cpp`: JNI entry points. Each agent appends their native methods to their isolated JNI section.
