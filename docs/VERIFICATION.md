# Prism-V Hardware & Engine Verification Report

This document records the official verification receipts, hardware test results, and deterministic test executions for the Prism-V media engine and Android application.

**Target Release:** `v1.0.0`  
**Latest Verification Date:** 2026-11-15  
**Verification Scope:** Native C++20 Core, Standalone Android APK, Live Hardware ADB Device Execution.

---

## 1. Test Environments & Toolchains

### Host System (Build & Core Verification)
| Component | Specification |
|---|---|
| OS | Windows 11 Enterprise (x64) |
| C++ Compiler | MSVC 19.44.35229 (Visual Studio 2022) |
| CMake | 3.31.6 |
| JDK | OpenJDK 25 (Java 17 target bytecode via `--release 17`) |
| Android Tooling | D8 (r8lib.jar), Apktool 2.9.3, uber-apk-signer 1.3.0 |
| GitHub CLI | 2.103.3 |

### Physical Device Target (Live Android Verification)
| Parameter | Device Specification |
|---|---|
| Device Model | Samsung Galaxy (SM-E156B) |
| Serial ID | `RZCX80CEE7M` |
| Android Version | Android 14 (API Level 34 / 36 target) |
| Architecture | ARM64-v8a |
| Display | 1080x2400 FHD+, 90 Hz AMOLED |

---

## 2. Native C++20 Core Test Suite

The native engine is verified using CTest and the standalone `prism_tests` binary.

### CTest Execution
```powershell
ctest --test-dir build --output-on-failure -C Release
```
**Results:**
```text
Test project C:/Users/des/OneDrive/Desktop/video_compression/build
    Start 1: AllPrismTests
1/3 Test #1: AllPrismTests ....................   Passed    0.21 sec
    Start 2: Exp1A_BBR
2/3 Test #2: Exp1A_BBR ........................   Passed    0.06 sec
    Start 3: Exp1B_Lossy
3/3 Test #3: Exp1B_Lossy ......................   Passed    0.67 sec

100% tests passed, 0 tests failed out of 3
Total Test time (real) = 0.95 sec
```

### Standalone Unit Test Suite (`prism_tests.exe`)
```powershell
build\prism_tests.exe
```
**Output:**
```text
========================================================
  Prism-V C++ unit tests                                
========================================================

[ RUN      ] test_detint_overflow_free_proof
[       OK ] test_detint_overflow_free_proof (0.0073 ms)
[ RUN      ] test_detint_dyadic_rescaling
[       OK ] test_detint_dyadic_rescaling (0.0001 ms)
[ RUN      ] test_detint_static_lut
[       OK ] test_detint_static_lut (0.0002 ms)
[ RUN      ] test_detint_16kb_page_alignment
[       OK ] test_detint_16kb_page_alignment (0.0107 ms)
[ RUN      ] test_rans8_interleaved_exact_roundtrip
[       OK ] test_rans8_interleaved_exact_roundtrip (0.1241 ms)
[ RUN      ] test_bbr_bit_exact_sha256_roundtrip
[       OK ] test_bbr_bit_exact_sha256_roundtrip (0.3607 ms)
[ RUN      ] test_bbr_micro_mlp_deterministic
[       OK ] test_bbr_micro_mlp_deterministic (0.0029 ms)
[ RUN      ] test_bbr_huffmanpos_buckets
[       OK ] test_bbr_huffmanpos_buckets (0.0001 ms)
[ RUN      ] test_psc_ycocgr_reversible_identity
[       OK ] test_psc_ycocgr_reversible_identity (0.0808 ms)
[ RUN      ] test_psc_legall53_1d_wavelet_reversibility
[       OK ] test_psc_legall53_1d_wavelet_reversibility (0.0019 ms)
[ RUN      ] test_psc_2d_wavelet_reversibility
[       OK ] test_psc_2d_wavelet_reversibility (0.02 ms)
[ RUN      ] test_psc_frame_compression_psnr_infinity
[       OK ] test_psc_frame_compression_psnr_infinity (0.8453 ms)
[ RUN      ] test_psc_bayer_splice_and_merge
[       OK ] test_psc_bayer_splice_and_merge (0.0271 ms)
[ RUN      ] test_psc_bayer_compression_roundtrip_psnr_infinity
[       OK ] test_psc_bayer_compression_roundtrip_psnr_infinity (0.7359 ms)
[ RUN      ] test_coolchic_synthesis_bounded_macs
[       OK ] test_coolchic_synthesis_bounded_macs (0.0052 ms)
[ RUN      ] test_coolchic_masked_soft_gating_blend
[       OK ] test_coolchic_masked_soft_gating_blend (0.0001 ms)
[ RUN      ] test_coolchic_pareto_dial_solver
[       OK ] test_coolchic_pareto_dial_solver (0.0004 ms)
[ RUN      ] test_afgs1_noise_extraction_and_synthesis
[       OK ] test_afgs1_noise_extraction_and_synthesis (0.0728 ms)
[ RUN      ] test_container_pvloss_roundtrip_and_seek
[       OK ] test_container_pvloss_roundtrip_and_seek (4.9723 ms)
[ RUN      ] test_container_isobmff_dual_track_mux_demux
[       OK ] test_container_isobmff_dual_track_mux_demux (0.0239 ms)
[ RUN      ] test_container_isobmff_metadata_ledger_preservation
[       OK ] test_container_isobmff_metadata_ledger_preservation (0.0085 ms)
[ RUN      ] test_vfs_open_proxy_serving_and_lru
[       OK ] test_vfs_open_proxy_serving_and_lru (32.5706 ms)
[ RUN      ] test_transaction_successful_commit_and_grace
[       OK ] test_transaction_successful_commit_and_grace (17.3802 ms)
[ RUN      ] test_transaction_rollback_on_verification_failure
[       OK ] test_transaction_rollback_on_verification_failure (7.509 ms)
[ RUN      ] test_dedup_rabin_chunking_and_restoration
[       OK ] test_dedup_rabin_chunking_and_restoration (92.1298 ms)
[ RUN      ] test_remux_savings_threshold_predicate
[       OK ] test_remux_savings_threshold_predicate (0.0073 ms)
[ RUN      ] test_remux_packet_preservation_and_sha256_parity
[       OK ] test_remux_packet_preservation_and_sha256_parity (0.0148 ms)
[ RUN      ] test_router_lossless_tri_contract_and_hybrid_dispatch
[       OK ] test_router_lossless_tri_contract_and_hybrid_dispatch (0.0036 ms)
[ RUN      ] test_rans8_single_symbol_full_frequency
[       OK ] test_rans8_single_symbol_full_frequency (0.0479 ms)
[ RUN      ] test_rans8_rejects_zero_frequency_stats
[       OK ] test_rans8_rejects_zero_frequency_stats (0.0478 ms)
[ RUN      ] test_rans8_rejects_truncated_stream
[       OK ] test_rans8_rejects_truncated_stream (0.0869 ms)
[ RUN      ] test_psc_rejects_odd_dimensions
[       OK ] test_psc_rejects_odd_dimensions (0.0131 ms)
[ RUN      ] test_psc_minimal_frame_roundtrip
[       OK ] test_psc_minimal_frame_roundtrip (0.0461 ms)
[ RUN      ] test_psc_rejects_geometry_mismatch_on_decode
[       OK ] test_psc_rejects_geometry_mismatch_on_decode (0.0549 ms)
[ RUN      ] test_psc_rejects_invalid_bayer_depth
[       OK ] test_psc_rejects_invalid_bayer_depth (0.0101 ms)
[ RUN      ] test_coolchic_probe_handles_small_dimensions
[       OK ] test_coolchic_probe_handles_small_dimensions (0.1391 ms)
[ RUN      ] test_container_rejects_short_and_bad_index
[       OK ] test_container_rejects_short_and_bad_index (5.318 ms)
[ RUN      ] test_container_rejects_truncated_chunk
[       OK ] test_container_rejects_truncated_chunk (5.0314 ms)
[ RUN      ] test_remux_rejects_truncated_and_trailing_data
[       OK ] test_remux_rejects_truncated_and_trailing_data (0.0144 ms)

--------------------------------------------------------
Total Tests: 39 | Passed: 39 | Failed: 0
--------------------------------------------------------
```

---

## 3. Native CLI Round-Trip & Invariant Verification

```powershell
# 1. System Invariant Audit
build\prism.exe audit
```
**Result:** Monotonic CDF lookup verified, 16 KiB page size verified, tile buffer within hardware budget. Exit Code 0.

```powershell
# 2. Binary Round-Trip
build\prism.exe compress input.bin out.pvloss --contract bbr
build\prism.exe decompress out.pvloss restored.bin
```
**Result:** Original and restored files produce bit-identical SHA-256 hashes.

---

## 4. Live On-Device Verification (Samsung Galaxy SM-E156B)

The Android application was installed on a physical Samsung device (`RZCX80CEE7M`) and exercised via ADB and interactive UI Automator testing.

### Test Case 1: Duo Comparison Deck Frame Synchronization
- **Observed Bug (Pre-fix):** Pausing playback caused one player to stop immediately while the secondary player continued for several frames, yielding visible frame mismatch and audio drift.
- **Root Cause:** Android's `MediaPlayer.seekTo(int)` defaults to `SEEK_CLOSEST_SYNC` (mode 2), which snaps only to previous keyframes. In streams with keyframe intervals of 1-2 seconds, streams desynchronized by up to 2,000 ms.
- **Fix Implemented:** Replaced with atomic `seekTo(targetMs, MediaPlayer.SEEK_CLOSEST)` targeting the exact master presentation timestamp, followed by master-timestamp lock.
- **Physical Test Result:**
  - Tested pause event at `01:49.470 [F: 3284]`.
  - Both players locked to timestamp `01:49.470` with $\Delta = 0\text{ ms}$.
  - Re-tested across Frame 38, Frame 179, and Frame 180: both decoders displayed the identical video frame.

### Test Case 2: Curtain Slider Touch Gestures
- **Observed Bug (Pre-fix):** Dragging the split-screen divider handle froze intermittently or triggered parent view scrolling.
- **Root Cause:** Parent containers intercepted touch events during horizontal gestures. Touch hit target was too narrow on high-DPI displays.
- **Fix Implemented:** Added `requestDisallowInterceptTouchEvent(true)` on `ACTION_DOWN` / `ACTION_MOVE` across `dividerHandle` and `playerContainer`. Increased touch target hit padding to 14dp.
- **Physical Test Result:** Continuous wipe swipe tested smoothly from 16% to 74% container width without stutters or gesture drops.

### Test Case 3: Black Frames on Mode Switching
- **Observed Bug (Pre-fix):** Switching from Curtain mode to Dual or Peek modes rendered one or both video streams black.
- **Root Cause:** TextureView layout dimensions reset to 0 upon view hierarchy re-binding, and paused decoders do not push new surface frames without a seek or layout dispatch.
- **Fix Implemented:** Set `TextureView` layoutParams to `MATCH_PARENT` and added a deferred UI thread `refreshPausedFrames()` invocation via `post()` on mode change.
- **Physical Test Result:** Transitioned between Curtain, Dual, Peek, Solo, and Fullscreen Cinema modes 20+ times sequentially with immediate frame rendering and zero blank surfaces.

### Test Case 4: Play/Pause State & Speed Resonance
- **Observed Bug (Pre-fix):** Pausing playback caused the top stream to pause, but the bottom stream resumed in the background; subsequently, pressing Play was non-responsive.
- **Root Cause:** In Android `MediaPlayer`, invoking `setPlaybackParams()` while a player is paused causes the native `MediaPlayerService` to automatically resume playback (`msg=6, Received media started message`).
- **Fix Implemented:** Added guard `if (!isPlaying || !mp.isPlaying()) return;` inside `applySpeed()`, preventing speed adjustments from unintentionally resuming paused players.
- **Physical Test Result:** Repeated Play/Pause toggling tested smoothly over 30 cycles; streams remain strictly paused when paused and resume simultaneously when resumed.

---

## 5. Production APK Build Receipts

```text
APK File: release/prism-v-v1.0.0.apk
Package: com.prismv
Version Code: 1
Version Name: 1.0.0
Min SDK: 26 (Android 8.0)
Target SDK: 36 (Android 16 Preview)
Alignment: 4-byte zipalign verified
Signatures: APK Signature Scheme v2 (Verified), v3 (Verified)
SHA-256: 42461a60a5be49075ee75158b0c30c4fb3e6474eb0c8fd46586a39f18fc6fa42
```
