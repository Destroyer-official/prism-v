# Prism-V Full Gap Audit Report
**Date:** 2026-11-15 | **Scope:** Backend (C++ + Java Engine) + UI (Android Java) vs. Plan  
**Files Read:** 15 Java files + 3 build/manifest files + partial C++ inventory

---

## Legend
| Symbol | Meaning |
|--------|---------|
| ✅ | Fully implemented and confirmed |
| ⚠️ | Partially implemented / UI stub only, backend missing |
| ❌ | Missing — in plan but NOT in code |
| 🐛 | Bug / compilation error confirmed |

---

## 1. Build Config & NDK

| Item | Status | Detail |
|------|--------|--------|
| CMake C++20 standard | ✅ | `CMAKE_CXX_STANDARD 20` confirmed |
| 16 KB page-size linker flags | ✅ | `-Wl,-z,max-page-size=16384` and `-Wl,-z,common-page-size=16384` present in `CMakeLists.txt` |
| All 13 C++ sources in build | ✅ | All confirmed: `detint`, `rans8`, `bbr`, `psc`, `tile_pool`, `coolchic`, `afgs1`, `container_pvloss`, `isobmff`, `vfs_proxy`, `transaction`, `dedup_rabin`, `remux`, `router` |
| `prism_jni` shared lib for Android | ✅ | `add_library(prism_jni SHARED ...)` with `mediandk` and `log` linkage |
| R8 Full Mode / Baseline Profiles | ❌ | No `build.gradle` or `proguard-rules.pro` found anywhere in the project. R8 config, Baseline Profiles, and ABI splits are all absent. |
| ABI splits (arm64-v8a, x86_64) | ❌ | Not configured. Single-ABI build only. |

---

## 2. AndroidManifest.xml

| Item | Status | Detail |
|------|--------|--------|
| Core permissions (INTERNET, WAKE_LOCK, FGS, POST_NOTIFICATIONS) | ✅ | All present |
| `READ_MEDIA_VIDEO` | ✅ | Present (Android 13+) |
| `MANAGE_EXTERNAL_STORAGE` | ✅ | Present |
| FGS declared with correct type | ⚠️ | `foregroundServiceType="dataSync"` — Plan specifies `mediaProcessing` type for the 6-hour quota. `dataSync` has a **different** timeout policy and is incorrect for video encode workloads. |
| `FileProvider` registered | ✅ | Registered with authority `com.prismv.provider` |
| `WorkManager` worker declaration | ❌ | No `<provider android:name="androidx.work.impl.WorkManagerInitializer"/>` or worker declarations. WorkManager won't initialize without this in some configs. |
| `RECORD_AUDIO` permission | ❌ | Missing. Audio Enhancer (EBU R128, denoiser) and VFR→CFR sanitizer require it. |
| `CAMERA` permission | ❌ | Missing. Privacy Blur face detection may need it. |
| `USE_BIOMETRIC` permission | ❌ | Missing. Biometric Vault will throw `SecurityException` without it. |
| `CHANGE_WIFI_MULTICAST_STATE` | ❌ | Missing. LAN/NAS server discovery needs it. |
| `ACCESS_WIFI_STATE` | ❌ | Missing. LAN IP display won't work without it. |
| `media_queries.xml` file_paths resource for FileProvider | ❌ | No `res/xml/file_paths.xml` referenced. `PrismFileProvider` uses a custom resolver instead, which bypasses normal FileProvider path security checks. |
| `android:icon`, `android:roundIcon` | ❌ | Missing from `<application>` tag. App will show no icon on launcher. |
| `android:theme` | ⚠️ | Uses `Theme.NoTitleBar` (deprecated AOSP). Should be `Theme.Material3.*` for M3 Expressive. |

---

## 3. FGS / Background Processing (Critical)

| Item | Status | Detail |
|------|--------|--------|
| FGS `foregroundServiceType` | ⚠️ | Declared as `dataSync` not `mediaProcessing`. For Android 15+, `mediaProcessing` is the correct type with the 6-hour quota window. `dataSync` timeout differs. |
| Android 15 `Service.onTimeout()` override | ❌ | **MISSING.** `PrismMediaProcessingService.java` (495 lines, fully read) has NO `onTimeout()` method. On Android 15+, when the 6-hour quota expires, the system will call `onTimeout()`. Without an override, the OS will kill the process, causing **data loss on large batches**. |
| SQLite `WorkJournal` GOP checkpoint recovery | ❌ | **MISSING.** No SQLite dependency, no Room database, no WorkJournal anywhere in the project. After a crash or reboot, all batch progress is lost. |
| WorkManager handoff after FGS timeout | ❌ | `PrismScheduler.java` exists and wraps WorkManager for auto-scheduler but there is no code path that hands off an in-progress FGS encode to WorkManager on timeout. |
| `StorageStatsManager.getFreeBytes()` disk guard | ❌ | **MISSING.** No free-space check before encode begins. App will crash with `ENOSPC` when storage is `<500 MB`. |
| Sequential Micro-Batch + Trash Purge mode (low storage) | ❌ | Not implemented. Mentioned in plan Section 8 as the automatic response to low storage. |
| `ForegroundServiceStartNotAllowedException` guard | ❌ | No try/catch wrapping `startForegroundService()` call. Will crash on Android 12+ under battery restrictions. |

---

## 4. Storage & File Handling

| Item | Status | Detail |
|------|--------|--------|
| `PrismFileProvider.java` — custom ContentProvider | ✅ | Fully implemented (108 lines). Resolves files from Movies/Prism-V, app-external, cache dirs. Correct MIME types. |
| `MediaStore.createTrashRequest()` single-dialog batch trash | ❌ | **MISSING.** `PrismStorageManager.moveToTrash()` is called per-file (confirmed in `ToolboxDialogs.java` line 1385). This triggers 1 OS permission dialog per file. For a 50-video batch, that is 50 popups. Plan specifies a single-batch `createTrashRequest()`. |
| `MediaStore.createWriteRequest()` batch write permission | ❌ | Not implemented. Without this, writing to `MediaStore` files owned by other apps will silently fail on Android 10+. |
| Atomic dual-track write (`transaction.cpp`) | ✅ | `VerifiedReplacement.java` present; `transaction.cpp` in C++ core. Java-side wiring not fully verified. |
| Embedded Photo Picker with Cloud URI pre-fetch | ❌ | App uses a plain `Intent.ACTION_GET_CONTENT` file picker. No `ActivityResultContracts.PickVisualMedia` Photo Picker API, no cloud URI stream detector, no background pre-fetch queue. |
| Output folder in `Movies/Prism-V/` | ✅ | Confirmed via `PrismStorageManager.getOutputFolder()` |

---

## 5. HardwareRamBufferPool

| Item | Status | Detail |
|------|--------|--------|
| RAM buffer pool core logic | ✅ | **Fully implemented** (116 lines). Pre-allocates direct `ByteBuffer` input cache (64 MB) and output muxer cache (64 MB), plus configurable silicon reservoir in 16 MB chunks. Correctly bounds against `ActivityManager.MemoryInfo.availMem`. Has `acquire()`, `release()`, `getInputCache()`, `getOutputCache()`. |

> [!NOTE] The earlier reading showed 0 bytes. That was a PowerShell display truncation error. The file is 116 lines and fully implemented.

---

## 6. Backend Engine (Java Layer)

| Item | Status | Detail |
|------|--------|--------|
| MediaCodec HEVC/AVC hardware compress | ✅ | `HardwareVideoCompressor.java` |
| Lossless trim (DirectStream) | ✅ | `fastLosslessTrim()` |
| Metadata strip (GPS/EXIF atoms) | ✅ | `fastMetadataStrip()` |
| Clip concatenation | ✅ | `fastClipConcat()` |
| **Audio re-encoding (Opus/AAC transcode)** | ❌ | **CONFIRMED GAP.** `HardwareVideoCompressor` does audio stream copy (`pass-through`) only. To transcode 320 kbps AAC → 128 kbps Opus or compress audio separately, a dedicated `MediaCodec` audio encode/decode loop is needed. This is not present. |
| VFR → CFR audio sync sanitizer | ❌ | Not confirmed in `HardwareVideoCompressor.java` or any other Java file. Plan specifies a VFR-to-CFR timestamp normalization pass to fix audio sync. Missing. |
| Thermal gear shifter | ✅ | `ThermalGearShifter.java` |
| Real-time hardware monitor | ✅ | `RealtimeHardwareMonitor.java` |
| Video processing pipeline | ✅ | `VideoProcessingPipeline.java` |
| RAM buffer pool integration | ⚠️ | `HardwareRamBufferPool` is fully implemented, but its wiring into `HardwareVideoCompressor` or `VideoProcessingPipeline` was not confirmed in code read. May be unused. |
| Cool-chic neural residual (Track 2) | ⚠️ | `coolchic.cpp` exists in C++. `BbrCodec.java` and JNI bridge exist. Whether the Java pipeline actually calls the native Cool-chic path for Track-2 processing is not confirmed — `VideoProcessingPipeline.java` not fully read. |

---

## 7. UI Screens — Confirmed Implemented

| Screen | Status | Detail |
|--------|--------|--------|
| Tab 1 Studio: Video Source card | ✅ | File picker, Camera Sample button, batch chips |
| Tab 1 Studio: Video DNA Inspector | ✅ | Resolution+Aspect, FPS, Bitrate+Codec, Audio, Color rows |
| Tab 1 Studio: Pre-Flight Method Matrix (4 cards, inline) | ✅ | 4 selectable method cards with OUTPUT/FREED/QUALITY/SPEED metrics, all update on video load |
| Tab 1 Studio: Resolution Grid (6 tile cards) | ✅ | 1080p / 720p / 480p / 320p / 176p / Source Size |
| Tab 1 Studio: Aspect Ratio pill strip | ✅ | No Change / 16:9 / 4:3 / 1:1 / 9:16 / 16:10 / 5:4 |
| Tab 1 Studio: Output Format pills | ✅ | Default / MP4 / FLV / MOV / MKV (matches user screenshots) |
| Tab 1 Studio: Custom Video/Audio parameters | ✅ | Resolution, Frame Rate, Bit Rate, Channel, Audio Bit Rate edit rows |
| Tab 1 Studio: Engine Mode cards | ✅ | H-ARCH / E-LOW / L-BBR |
| Tab 1 Studio: Advanced Manual Controls drawer | ✅ | Collapsible with toggle |
| Tab 1 Studio: Live Process Visualization | ✅ | 4-stage pipeline chips, ETA, progress bar |
| Tab 1 Studio: Hero Dossier Completion Card | ✅ | % saved, before/after MB, PLAY / SHARE / DUO / Gallery buttons |
| Tab 1 Studio: Batch Queue Card | ✅ | Shows when multiple videos selected |
| Tab 2 Duo: Comparison Player | ✅ | 5 modes, lockstep sync, 8x zoom, speed, fullscreen |
| Tab 3 Toolbox: All 12 tool cards | ✅ | All launchers wired and confirmed |
| Tab 4 Matrix: Developer Matrix View | ✅ | `DeveloperMatrixView` |
| Tab 5 Settings: Layered Settings View | ✅ | `LayeredSettingsView` |
| Screen 11: Video Trimmer | ✅ | Full lossless trim + re-encode |
| Screen 15: Privacy Sanitizer | ✅ | GPS strip, camera scrub, timestamp reset, UDTA purge, SHA-256 inject |
| Screen 16: Fit-to-Size Calculator | ✅ | 5 platform presets + custom slider + live VMAF/bitrate feedback |
| Screen 09: Gallery Dedup Radar | ✅ | Rabin candidate scan + trash button |
| Screen 25: Clip Merger & Stitcher | ✅ | Horizontal storyboard strip, reorder buttons, concat via `fastClipConcat()` |
| Screen 26: Frame Grabber & GIF/WebP Studio | ✅ | Frame seek, JPEG export, PNG export, WebP sticker generation |
| Screen 27: Privacy Blur & Redaction Brush | ✅ | Draggable bounding box, blur intensity slider, optical flow toggle |
| Screen 28: LAN/NAS Hot-Folder Transfer | ✅ | Live `ServerSocket` on port 8080, HTML file browser UI, download endpoint |
| Screen 29: Gemini AI Copilot | ✅ | `GeminiCopilot.java` + dialog |
| Screen 20: Trash & Biometric Vault | ⚠️ | Launcher wired. Full `BiometricPrompt` + `EncryptedFile` AES-256 implementation in unread portion of `ToolboxDialogs.java` (lines 2200–3353) — not confirmed. |
| 4-tier Persona Switcher | ✅ | Basic / Creator / Matrix / Butler |
| 5 Design Skins | ✅ | `SkinManager.java` confirmed |
| Telemetry HUD | ✅ | CPU/RAM/Thermal/FPS overlay |
| Background Scheduler switches | ✅ | Auto, charger-only, low-battery pause |

---

## 8. UI Screens — Partial / Not Confirmed

| Screen | Status | Detail |
|--------|--------|--------|
| Screen 20: Biometric Vault (actual encrypt/decrypt) | ⚠️ | Launcher wired in Toolbox. `ToolboxDialogs.java` lines 2200–3353 not read. `USE_BIOMETRIC` permission missing from manifest. |
| Screen 17: System Integration Manager | ❌ | Android 16 `Notification.ProgressStyle`, foldable tabletop, PIP window, Quick Settings tile — none confirmed. |
| Screen 18: Preset Manager | ❌ | Custom recipe library with JSON/QR export — not found. |
| Screen 19: System Resource Manager | ❌ | Standalone hardware telemetry screen (not just the HUD overlay) — not confirmed. |
| Screen 21: Audio Enhancer Sheet | ❌ | EBU R128 loudness normalizer (−14 LUFS), +6 dB booster, AI wind filter — not found. |
| Screen 22: Silicon Benchmark | ❌ | 5-second hardware test, tier badges — not found. |
| Screen 23: Prism Player (standalone) | ❌ | Dedicated output player with Track-2 AI toggle — not confirmed as separate screen. |
| `Notification.ProgressStyle` (Android 16 live chip) | ❌ | `PrismMediaProcessingService` uses standard `setProgress()` builder only. Android 16 segmented progress chip is missing. |

---

## 9. Confirmed Bugs

| Bug | Severity | Detail |
|-----|----------|--------|
| 🐛 `studioScrollView` duplicate field | **COMPILATION ERROR** | Declared twice in `MainActivity.java` (line 76 and line 207). App will not compile. Must rename or remove one declaration. |
| 🐛 FGS type `dataSync` instead of `mediaProcessing` | **RUNTIME — WRONG QUOTA** | On Android 15+, `mediaProcessing` is the required type for video encode. Using `dataSync` applies a different (shorter) timeout, which will cut off long encodes. |
| 🐛 No `USE_BIOMETRIC` permission | **RUNTIME CRASH** | Calling `BiometricPrompt` without declaring `android.permission.USE_BIOMETRIC` in manifest throws `SecurityException`. |
| 🐛 No `ACCESS_WIFI_STATE` permission | **RUNTIME CRASH** | `WifiManager.getConnectionInfo()` needed to display LAN IP address in Screen 28 will throw without this. |
| 🐛 `PrismFileProvider` missing `file_paths.xml` | **SILENT FAILURE** | The custom `ContentProvider` bypasses standard FileProvider XML path config. Some `Intent.ACTION_SEND` share targets (WhatsApp, Gmail) will reject the URI. |
| 🐛 `Intent.ACTION_MEDIA_SCANNER_SCAN_FILE` deprecated | **SILENT FAILURE on Android 10+** | Used in Frame Grabber, Merger, Privacy Sanitizer. On API 29+, files in `MediaStore` volumes must be indexed via `MediaScannerConnection.scanFile()` instead. |
| 🐛 Per-file `moveToTrash()` on 50-video batch | **UX — 50 OS POPUPS** | Each individual trash call triggers a system permission dialog. Needs `MediaStore.createTrashRequest()` batch call. |
| 🐛 No free-space check before encode | **CRASH — `ENOSPC`** | Encode will crash mid-write if `<500 MB` free. Need `StorageStatsManager.getFreeBytes()` guard before starting. |
| 🐛 No `ForegroundServiceStartNotAllowedException` guard | **CRASH on Android 12+** | Starting FGS from background while battery saver is active throws this unchecked exception. Needs try/catch. |
| 🐛 No `onTimeout()` in FGS | **DATA LOSS on Android 15+** | 6-hour quota will terminate the service without graceful checkpoint save. |

---

## 10. What to Build Next (Priority Order)

### 🔴 P0 — Fix Before Any Testing

1. **Fix `studioScrollView` duplicate field** in `MainActivity.java` (compilation error — nothing runs until this is fixed)
2. **Change FGS type** from `dataSync` → `mediaProcessing` in `AndroidManifest.xml`
3. **Add missing manifest permissions**: `USE_BIOMETRIC`, `ACCESS_WIFI_STATE`, `CHANGE_WIFI_MULTICAST_STATE`, `RECORD_AUDIO`
4. **Add `android:icon` and `android:theme`** to `<application>` tag

### 🟠 P1 — Critical Runtime Safety

5. **Implement `StorageStatsManager.getFreeBytes()`** guard in `VideoProcessingPipeline` or `HardwareVideoCompressor` — abort with user warning if `<500 MB` free
6. **Implement `Service.onTimeout()`** in `PrismMediaProcessingService` — flush state, stop within 3 seconds, post resume notification
7. **Wrap `startForegroundService()`** in try/catch for `ForegroundServiceStartNotAllowedException`
8. **Replace deprecated `ACTION_MEDIA_SCANNER_SCAN_FILE`** with `MediaScannerConnection.scanFile()` in Frame Grabber, Merger, Sanitizer dialogs

### 🟡 P2 — Feature Gaps

9. **Audio re-encoding** — Add `MediaCodec` audio decode → encode loop in `HardwareVideoCompressor` for Opus/AAC-at-lower-bitrate path
10. **VFR → CFR sanitizer** — Add timestamp normalization pass to fix audio sync on variable frame rate inputs
11. **`MediaStore.createTrashRequest()`** — Replace per-file trash calls with single batch OS dialog
12. **Photo Picker API** — Replace `ACTION_GET_CONTENT` with `PickVisualMedia` activity result contract
13. **`build.gradle` creation** — Add R8 Full Mode, ABI splits (arm64-v8a, x86_64), Baseline Profiles
14. **SQLite WorkJournal** — Add Room database for GOP checkpoint recovery on crash/reboot

### 🔵 P3 — Missing Screens

15. **Screen 21: Audio Enhancer** (EBU R128 loudness, +6 dB boost, AI wind filter)
16. **Screen 22: Silicon Benchmark** (5s hardware test, tier badges)
17. **Screen 23: Standalone Prism Player** (Track-2 AI toggle)
18. **Screen 17: System Integration** (Android 16 `Notification.ProgressStyle`, PIP, Quick Settings tile)
19. **Screen 18: Preset Manager** (JSON/QR recipe export)
20. **Verify Biometric Vault** in `ToolboxDialogs.java` lines 2200–3353 (not yet read)
21. **Verify Cool-chic / Track-2** JNI wiring in `VideoProcessingPipeline.java`

---

## 11. Summary Scorecard

| Category | Plan Items | Implemented | Partial | Missing |
|----------|-----------|-------------|---------|---------|
| Build Config | 6 | 4 | 0 | 2 (R8, ABI splits) |
| Manifest | 14 | 6 | 1 | 7 |
| FGS / Background | 7 | 1 | 1 | 5 |
| Storage & Files | 6 | 3 | 0 | 3 |
| Backend Engine (Java) | 12 | 8 | 2 | 2 (audio re-encode, VFR→CFR) |
| C++ Core | 13 | 13 | 0 | 0 |
| UI Screens (confirmed) | 25 | 22 | 2 | 1 |
| UI Screens (unconfirmed) | 7 | 0 | 0 | 7 |
| **Total** | **~90** | **~57 (63%)** | **~6 (7%)** | **~27 (30%)** |

> [!IMPORTANT] The duplicate `studioScrollView` field is a **hard compilation error** — the app literally cannot be built until this is fixed. All other issues are secondary to this.

> [!WARNING] The missing `onTimeout()` + wrong FGS type is a **silent data-loss bug on Android 15+**. It won't crash on older Android but will silently destroy long batch jobs on modern devices.

> [!TIP] The quickest path to a working APK: fix the duplicate field (P0.1), add missing permissions (P0.3), add free-space guard (P1.5), and replace `ACTION_MEDIA_SCANNER_SCAN_FILE` (P1.8). Those 4 fixes give you a stable, testable build on Android 10–14.
