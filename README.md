<div align="center">

<img src="docs/assets/logo.svg" width="120" height="120" alt="Prism-V Logo" />

# Prism-V

**Next-Generation Hardware Video Compression & Forensic Inspection Engine**

[![CI Build](https://github.com/Destroyer-official/prism-v/actions/workflows/ci.yml/badge.svg)](https://github.com/Destroyer-official/prism-v/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/Destroyer-official/prism-v?color=0ea5e9&label=Release)](https://github.com/Destroyer-official/prism-v/releases/latest)
[![APK Download](https://img.shields.io/badge/Download-APK%20(v1.0.0)-success?logo=android)](https://github.com/Destroyer-official/prism-v/releases/download/v1.0.0/prism-v-v1.0.0.apk)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?logo=cplusplus)](CMakeLists.txt)
[![Android Target](https://img.shields.io/badge/Android-API%2026--36-3DDC84.svg?logo=android)](android_app/apk_root/AndroidManifest.xml)
[![16 KB Page Aligned](https://img.shields.io/badge/Memory-16%20KiB%20Aligned-purple.svg)](docs/MATHEMATICAL_INVARIANTS.md)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

*A unified, production-grade video compression and comparison platform combining a deterministic C++20 core engine, hardware MediaCodec acceleration, and an on-device forensic analysis suite.*

[Download Latest APK](https://github.com/Destroyer-official/prism-v/releases/latest) • [Explore Architecture](docs/ARCHITECTURE.md) • [Mathematical Invariants](docs/MATHEMATICAL_INVARIANTS.md) • [Verification Receipts](docs/VERIFICATION.md)

</div>

---

## Overview

**Prism-V** is an enterprise-grade media compression engine and analytical video player built from the ground up for zero-compromise efficiency, sub-millisecond playback precision, and mathematical determinism.

It bridges low-level high-performance computing in modern **C++20** with an advanced **Android** application, enabling real-time video transcoding, frame-exact side-by-side comparison, content-defined deduplication, and neural-assisted rate control.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Prism-V Unified Media Architecture                  │
├─────────────────────────────────────────────────────────────────────────┤
│  Android Presentation Layer                                             │
│    • Duo Comparison Deck (Atomic Sync, 5 Display Modes, Frame Stepping) │
│    • Hardware Telemetry HUD (FPS, Bitrate, Thermals, Energy Envelope)   │
│    • Forensic Toolbox (Subframe Trimming, Fit-to-Size, Subject Blur)    │
│    • Gemini AI Copilot (Context-Aware Compression Diagnostics)          │
├─────────────────────────────────────────────────────────────────────────┤
│  Service & Background Processing                                        │
│    • PrismMediaProcessingService (Foreground Service + Notification)    │
│    • Hardware MediaCodec Transcoder (HEVC / AVC / AV1 Pipeline)         │
├─────────────────────────────────────────────────────────────────────────┤
│  Native C++20 Engine (prism_core) via JNI Bridge                        │
│    • rANS-8 SIMD Entropy Engine (8-way Interleaved Renormalization)     │
│    • BBR Rate Controller (Integer Probability Models, Zero-Expansion)   │
│    • PSC Codec (Reversible YCoCg-R + 2D LeGall 5/3 Wavelets)            │
│    • .pvloss Container Engine (CRC32C Framing, O(1) Seek Index)         │
│    • Content-Defined Rabin Vault (Deduplication + SHA-256 Hashes)       │
│    • Atomic Transactions & VFS Proxy (Rollback Safe Replacements)       │
├─────────────────────────────────────────────────────────────────────────┤
│  OS & Hardware (Android 8.0 - 16, 16 KiB Page Size, Linux, Windows)     │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Visual Showcase

| Duo Comparison Deck | Compression Studio & HUD |
|:---:|:---:|
| <img src="docs/assets/screenshots/01_duo_player_live.png" width="400" alt="Duo Comparison Deck" /><br/>*Sub-millisecond frame sync ($\Delta = 0\text{ ms}$), interactive curtain wipe, and live diagnostics.* | <img src="docs/assets/screenshots/02_studio_main.png" width="400" alt="Compression Studio" /><br/>*Hardware-accelerated transcoding with real-time CPU, thermal, and power telemetry.* |

| Forensic Toolbox Hub | Developer Performance Matrix |
|:---:|:---:|
| <img src="docs/assets/screenshots/03_toolbox_hub.png" width="400" alt="Forensic Toolbox" /><br/>*Subframe trimmer, bit budget allocator, and privacy shielding tools.* | <img src="docs/assets/screenshots/08_matrix_dashboard.png" width="400" alt="Developer Matrix" /><br/>*Real-time encoder latency histograms, memory page allocations, and codec capabilities.* |

---

## Core Capabilities

### 1. Duo Comparison Deck
- **Zero-Drift Atomic Sync**: Utilizes Android's hardware `MediaPlayer.SEEK_CLOSEST` targeting exact master stream timestamps, ensuring both decoders freeze and resume on the exact identical frame ($\Delta = 0\text{ ms}$).
- **Five Interactive Comparison Modes**:
  1. **Curtain Mode**: Fluid vertical/horizontal wipe divider with parent-touch intercept suppression (`requestDisallowInterceptTouchEvent`).
  2. **Dual Mode**: Side-by-side split screen with aspect-ratio preservation and custom zoom matrices.
  3. **Peek Mode**: Touch-and-hold instant reveal of original uncompressed frames.
  4. **Solo Mode**: Quick single-tap toggling between source and compressed outputs.
  5. **Fullscreen Cinema**: High-immersion panoramic inspection mode with auto-hiding controls.
- **Precision Subframe Stepping**: Step forward and backward frame-by-frame at exact intervals ($1/30\text{s}$ and $1/60\text{s}$).
- **Independent & Synchronous Speed Control**: Variable playback rates ($0.25\times$, $0.5\times$, $1.0\times$, $1.5\times$, $2.0\times$) with state-safe parameter application.

### 2. Adaptive Hardware Transcoding & Telemetry
- **Hardware `MediaCodec` Acceleration**: Optimized asynchronous surface-to-surface encoding for H.264, H.265 (HEVC), and AV1.
- **Thermal & Power Throttling Guards**: Dynamic frequency and bitrate scaling prevents thermal throttling on extended encoding jobs.
- **Background Foreground Service**: `PrismMediaProcessingService` provides resilient processing with active progress notifications and wake-lock management.
- **Forensic Utilities**: Subframe trimming, strict target size budgeting (Fit-to-Size), facial/license plate blurring, and metadata sanitization.

### 3. C++20 Native Compression Core (`prism_core`)
- **rANS-8 SIMD Entropy Coder**: 8-way interleaved 16-bit state range Asymmetric Numeral Systems achieving multi-gigabit throughput.
- **BBR (Bit-Budget Rate Control)**: Reversible nibble symbolization and deterministic integer probability models with guaranteed uncompressed fallback on incompressible payloads.
- **PSC Image & RAW Bayer Codec**: Reversible integer YCoCg-R color transform paired with 2D LeGall 5/3 lifting wavelets, guaranteeing $PSNR = \infty$ lossless round-tripping.
- **`.pvloss` Container Specification**: Self-contained 72-byte header, CRC32C chunk integrity, and an $O(1)$ random-access seek table for fast streaming.
- **Content-Defined Rabin Chunking**: Variable-block deduplication with Rabin rolling hash and SHA-256 chunk vault storage.
- **Verified Replacement Engine**: Two-phase commit transactional file swapping with write-ahead journals for 100% crash and power-loss resilience.

### 4. Gemini AI Copilot
- Contextual analysis of video content characteristics.
- Dynamic recommendation of compression profiles (Archival, Streaming, Mobile, Forensic).
- Real-time quality-vs-size trade-off estimations.

---

## Repository Structure

```
├── .github/
│   └── workflows/              # Multi-OS CI (Linux, Windows, macOS) & Release Automation
├── android_app/                # Production Java Android Application (JNI, UI, Services)
│   ├── apk_root/               # APK manifest, drawables, and mipmaps (API 26-36)
│   └── src/com/prismv/         # Engine, Worker, UI, Telemetry, and AI Copilot packages
├── android/                    # Experimental Jetpack Compose UI preview module
├── core/
│   ├── include/prism/          # Public C++20 headers (rans8, bbr, psc, vfs, container)
│   └── src/                    # C++20 implementations & JNI binding layer
├── docs/                       # Technical documentation & architectural deep dives
│   ├── assets/                 # Vector branding and high-res application screenshots
│   ├── ARCHITECTURE.md         # Full subsystem design, JNI bridge, and threading model
│   ├── MATHEMATICAL_INVARIANTS.md # Formal proofs, dyadic bounds, and 16 KiB alignment
│   ├── VERIFICATION.md         # On-device hardware test receipts and CTest benchmarks
│   └── GAP_AUDIT_REPORT.md     # Engineering gap audit and resolution documentation
├── release/                    # Output directory for signed, aligned production APKs
├── scripts/                    # Standalone APK compilation and packaging automation
├── tests/                      # Comprehensive native test suite (39 verified unit tests)
├── tools/                      # Command-line utility (prism), icon generator, and harnesses
├── build_apk.ps1               # Root PowerShell script for one-click APK generation
└── CMakeLists.txt              # Cross-platform CMake 3.20+ build configuration
```

---

## Installation & Quickstart

### Option A: Install Android APK (Recommended)

Download the production-signed, zipalign-verified APK directly from our releases:

👉 **[Download Prism-V v1.0.0 APK](https://github.com/Destroyer-official/prism-v/releases/download/v1.0.0/prism-v-v1.0.0.apk)**

Install via ADB:
```bash
adb install -r release/prism-v-v1.0.0.apk
adb shell am start -n com.prismv/.ui.MainActivity
```

### Option B: Build the Android APK from Source

Prism-V features a lightweight, lightning-fast compilation pipeline that builds directly without heavy Gradle wrappers using `javac --release 17`, D8, and `apktool`:

```powershell
# Set tool locations (or place them in tools/vendor/)
$env:PRISM_ANDROID_JAR = "C:\path\to\android.jar"
$env:PRISM_APKTOOL_JAR = "C:\path\to\apktool.jar"
$env:PRISM_R8_JAR      = "C:\path\to\r8lib.jar"
$env:PRISM_SIGNER_JAR  = "C:\path\to\uber-apk-signer.jar"

# Build, align, and sign in seconds:
.\build_apk.ps1
```
The output APK will be generated at `android_app/signed/unsigned-aligned-debugSigned.apk` and copied to `release/`.

### Option C: Build the C++20 Native Core & CLI

Requirements: CMake 3.20+, Ninja or MSBuild, and a modern C++20 compiler (Clang 16+, GCC 13+, or MSVC 2022).

```bash
# Configure and build in Release mode
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
cmake --build build --config Release

# Run the full unit test suite
ctest --test-dir build --output-on-failure -C Release
```

---

## Native CLI Usage Reference

The `prism` standalone executable provides full access to native container and compression facilities:

```bash
# 1. Audit system invariants (integer bounds, 16 KiB page alignment, CDF monotonic tables)
prism audit

# 2. Compress video/data into a .pvloss container with BBR bitstream compaction
prism compress input.mp4 output.pvloss --contract bbr

# 3. Decompress and verify bit-exact SHA-256 integrity
prism decompress output.pvloss restored.mp4

# 4. Clean-room ISOBMFF remuxing (packet-preserving MP4 normalization)
prism remux raw_input.mp4 clean_output.mp4

# 5. Content-defined deduplication chunking into an encrypted/isolated vault
prism dedup recording.mp4 vault/ manifest.pvref

# 6. Reconstruct file from deduplicated block vault
prism restore manifest.pvref vault/ restored.mp4
```

---

## Mathematical & Technical Invariants

Prism-V adheres to strict mathematical and operational invariants verified across our test suites:

- **Dyadic Integer Arithmetic (`detint`)**: All probability modeling and inverse CDF queries run exclusively in 32-bit fixed-point integer math ($\le 2^{31}-1$). Zero floating-point divergence across ARM64 and x86_64 architectures.
- **Reversible Color & Wavelet Transforms (`PSC`)**:
  $$\text{YCoCg-R forward: } Y = \lfloor (R + 2G + B) / 4 \rfloor, \; Co = R - B, \; Cg = G - \lfloor (R + B) / 2 \rfloor$$
  Inverse reconstruction guarantees exact bit-for-bit parity ($PSNR = \infty\text{ dB}$).
- **16 KiB Memory Page Alignment**: All native shared libraries compile with `-Wl,-z,max-page-size=16384` to ensure full compliance with Android 15+ 16 KiB page-size kernels.
- **Fail-Safe Rollback Semantics**: Storage operations use transactional two-phase write-ahead journals ensuring zero corrupted files on abnormal termination.

See [docs/MATHEMATICAL_INVARIANTS.md](docs/MATHEMATICAL_INVARIANTS.md) for full proofs and bounds.

---

## Hardware Verification Receipts

Prism-V is verified against real hardware targets and continuous integration environments:

- **Host Matrix**: Windows 11 x64 (MSVC 19.44), Ubuntu 24.04 LTS (GCC 13, Clang 16), macOS Sonoma (AppleClang 15).
- **Physical Device Target**: Samsung Galaxy SM-E156B (Android 14, API 34).
- **Duo Player Live Test Results**:
  - **Frame Desync Delta**: $\Delta = 0\text{ ms}$ on pause (Decoders synchronizing on exact frame numbers: e.g. Frame 38, 179, 3284).
  - **Curtain Divider**: Zero touch-locking from 16% to 74% continuous wipe gestures.
  - **Mode Switching**: Zero black frame artifacts across Curtain, Dual, Peek, Solo, and Cinema transitions.
- **Unit Test Coverage**: 39 native test suites passing 100% with zero failures.

Full test logs, ADB broadcast instructions, and hardware receipts are cataloged in [docs/VERIFICATION.md](docs/VERIFICATION.md).

---

## Contributing

We welcome contributions! Please review our [Contributing Guidelines](CONTRIBUTING.md) and [Architecture Guide](docs/ARCHITECTURE.md) before submitting pull requests.

All code contributions must pass the native C++ test suite (`ctest`) and compile cleanly against the Android platform SDK.

---

## Security

Security is foundational to media integrity. Please review our [Security Policy](SECURITY.md) to report vulnerabilities responsibly.

---

## License

Prism-V is licensed under the **Apache License, Version 2.0**. See the [LICENSE](LICENSE) file for complete details.

Copyright (c) 2026 Prism-V Contributors.
