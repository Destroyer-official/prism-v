# P15 Plan Digest: Architectural and Implementation Blueprint for a Next-Generation Android Video Compression System (2027–2030)

**Source File:** `research_implementation_plane15.md`  
**Total Lines:** 1,930  
**Reading Range:** Lines 1–1930 (Chunks: 1–400, 401–800, 801–1200, 1201–1600, 1601–1930)  
**First Heading:** `# Architectural and Implementation Blueprint for a Next-Generation Android Video Compression System (2027–2030)`  
**Last Heading:** `## Weakest claims`  
**Final Sentence:** `It must earn its place through blind testing and may never ship.`

---

## 1. Thesis
P15 presents an exhaustive, engineering-rigorous blueprint designed for consumer Android devices across 2027–2030. It rejects "AI magic" and speculative codec hype, adopting a strict tripartite taxonomy:
1. **Lossless Mode:** Clearly splits into **Byte-Lossless Archive** (repackaging/remuxing standard streams without decode, SHA-256 verified) and **Pixel-Lossless Preservation** (using standardized FFV1 in Matroska with research exploration into Magma; explicit warning that pixel-lossless re-encoding of compressed camera footage causes file inflation).
2. **Lossy Mode:** Dual-path standards-first architecture using **AV1 as primary** (via MediaCodec hardware where verified, falling back to tuned SVT-AV1 / libaom software) and **HEVC as high-efficiency legacy fallback** (MediaCodec / x265). It features a 3-tier perceptual quality engine (Balanced, Quality-first, Extreme Size) with rate control driven by scene classification and spatiotemporal complexity.
3. **Shared Foundation:** A native C/C++ core with clean C API wrappers to Kotlin, zero-copy buffer handoff via Surface/GraphicBuffer, unified metadata normalization (colorimetry, HDR, orientation, VFR timestamps), strict thermal/battery throttling governors, and an uncompromising stance that neural models must remain non-normative sidecars or offline pre/post-filters—never inside the decode loop.

---

## 2. Technical Decisions
- **Architecture:** 4-layer modular design: (1) Android Presentation & Lifecycle (Kotlin, Jetpack Compose, WorkManager, Foreground Services), (2) Orchestration & Policy Core (Kotlin/C++ bridge, device capability profiling, thermal governor), (3) Native Processing Engine (C/C++, libavcodec/libavformat, SVT-AV1, libaom, x265, FFV1, dav1d), and (4) Hardware Abstraction Layer (MediaCodec NDK AMediaCodec, Vulkan/OpenGL ES compute, LiteRT/ExecuTorch for ML).
- **Lossless Pipeline:**
  - *Mode A (Archive Remux):* Stream copy, container normalization (MP4/MKV), bitstream filtering, SHA-256 payload checksum validation.
  - *Mode B (Pixel-Lossless):* Decode to raw YUV/RGB, feed to FFV1 v3/v4 in Matroska (.mkv) with multithreading and slice-based CRC. Optional research prototype with Magma.
- **Lossy Pipeline:**
  - Fast single-pass pre-analysis: extracts spatial variance, temporal difference, scene change flags.
  - Hardware/Software Router: Probes device for hardware AV1 encoder capability (`FEATURE_HdrEditing`, vendor-specific profiles). If hardware quality meets threshold, use MediaCodec; otherwise, dispatch to multithreaded SVT-AV1 preset 6-8 on high-tier or preset 8-10 on mid-tier.
  - Rate Control: CRF/CQP with capped VBR, content-adaptive QP offsets, spatiotemporal pooling.
- **Machine Learning Role:** Explicitly non-normative. ML is restricted to: (1) Scene and complexity classification (lightweight CNN), (2) Encoder speed-up / early CU termination hints, and (3) Non-normative decode-time post-filter (optional enhancement layer). Decoders never depend on ML for standard playback.
- **Languages & Tooling:** Modern C++ (C++20) for engine core, C99 for codec glue, Kotlin for Android UI/services, CMake/NDK r27+, Ninja, clang-tidy, ASan/UBSan, Valgrind.
- **Target Hardware & Tiering:**
  - *Flagship (Tier 1, Snapdragon 8 Gen 3/4/Elite, Dimensity 9300/9400):* 4K60 encode, hardware AV1, full pre-analysis, optional neural post-filter.
  - *Mid-range (Tier 2, Snapdragon 7-series, Dimensity 7000/8000):* 1080p60 / 4K30, hardware AV1 or fast software AV1 preset 8+, HEVC fallback.
  - *Budget / Low-end (Tier 3, 3–4 GB RAM, A55 cores):* 720p/1080p30, hardware HEVC preferred, software AV1 restricted to preset 10 or deferred to plugged-in charging state.
- **Containers:** ISO BMFF / MP4 for lossy delivery (fragmented MP4 option for crash resilience); Matroska (.mkv) for FFV1 lossless and advanced stream preservation.

---

## 3. Inventions & Differentiating Concepts
- **P15-I01 (Tripartite Lossless Taxonomy):** Rigidly separates Byte-lossless (remux), Pixel-lossless (FFV1/Magma), and Near-lossless, preventing user deception and catastrophic file inflation.
- **P15-I02 (Pre-Flight Hardware Conformance Probe):** Runs a tiny test-vector encode during initial app calibration to evaluate vendor MediaCodec quality, rate control drift, and VMAF against software ground truth before trusting hardware AV1.
- **P15-I03 (Thermal-State Dynamic Gear Shifting):** Monitors Android `ThermalManager` callbacks; dynamically drops encoder presets (e.g. SVT-AV1 preset 6 -> 8 -> 10) or pauses encoding when thermal status reaches `THERMAL_STATUS_SEVERE` to avoid OS-level kill.
- **P15-I04 (Deterministic Verification Loop):** Integrated post-encode validation worker that decodes output frames and verifies exact PSNR=$\infty$ for lossless or VMAF/SSIM sanity check for lossy before replacing/deleting original files.
- **P15-I05 (Crash-Resilient Segmented Pipeline):** Writes video in independent segments using fragmented MP4 or segment files, allowing interrupted background jobs (battery death or process kill) to resume without re-encoding completed segments.
- **P15-I06 (Adaptive Power-Budget Governor):** Integrates with Android `BatteryManager` and `WorkManager`; schedules heavy AV1 software encodes only when device is charging, on Wi-Fi, and battery $>50\%$.

---

## 4. Plan Content
- **Phases & Milestones:**
  - Phase 0 (Month 1): Specification, legal review, test harness, CI/CD, reference corpus.
  - Phase 1 (Months 2–3): Core architecture, lossless remux/FFV1, benchmark harness, baseline metrics.
  - Phase 2 (Months 4–6): Lossy pipeline, hardware/software AV1/HEVC abstraction, rate control engine.
  - Phase 3 (Months 7–8): ML pre-analysis, thermal/power governors, WorkManager background integration.
  - Phase 4 (Months 9–10): Android UI/UX, battery/thermal hardening across physical device lab (30+ devices).
  - Phase 5 (Months 11–12): Beta testing, security/SBOM audits, performance optimization, public launch.
- **Team Composition:** 8 FTEs: 1 Principal/Lead Architect, 2 Native Video Codec Engineers (C++/SIMD), 1 Android Platform Lead (Kotlin/NDK/MediaCodec), 1 ML/Computer Vision Engineer, 1 QA/Test Automation Engineer, 1 Product/UX Designer, 1 DevOps/Release Engineer. External counsel for IP/patents.
- **Cost & Budget:** ~$1.8M–$2.4M USD for 12 months (salaries, device lab of 40 phones, cloud CI, patent counsel).
- **Testing & Verification:** Comprehensive automated test matrix with 500+ curated clips, PSNR, SSIM, VMAF (via libvmaf), continuous memory profiling, thermal Chamber testing, and exact byte-level verification suites.
- **Legal & IP:** Comprehensive analysis of MPEG LA / HEVC Advance pools, AOMedia patent cross-license, FFmpeg LGPL v2.1/v3 compliance (dynamic linking, avoiding `--enable-gpl` for proprietary shipping), and defensive patent search on rate-control/codec-routing.

---

## 5. Numbers & Quantitative Claims
- Target bitrate savings: 25%–50% vs H.264 baseline at equal subjective quality (VMAF 93+) *(Stated source: standard AV1 performance claims, flagged in self-audit as estimate/hypothesis)*.
- Peak memory footprint: Restricted to $<150\text{ MB}$ for 1080p and $<350\text{ MB}$ for 4K encoding *(Stated source: engineering design budget)*.
- Working buffer target: $\approx 2\times$ decoded frame size plus encoder reference pool *(Stated source: design target, flagged in self-audit as optimistic)*.
- Hardware decode power consumption: $0.2\text{ W} - 0.6\text{ W}$ vs software decode $1.5\text{ W} - 4.0\text{ W}$ *(Stated source: typical mobile SoC metrics)*.
- Minimum background completion rate: $>90\%$ across Android 12–15+ under foreground service restrictions *(Stated source: design objective)*.

---

## 6. Citations
- **Standards:** ITU-T H.264, H.265, H.266, AOMedia AV1 Specification, ISO/IEC 14496-12 (ISOBMFF), ISO/IEC 23093-2 (MPEG-5 Part 2 / LCEVC), IETF RFC 6381. *(All real, verified standards)*.
- **Codecs & Repositories:** AOMedia libaom, SVT-AV1 (`gitlab.com/AOMediaCodec/SVT-AV1`), dav1d (`github.com/videolan/dav1d`), FFmpeg, x265, x264, OpenH264, Magma (`github.com/fab-jul/magma`), Netflix VMAF / libvmaf (`github.com/Netflix/vmaf`). *(All verified real repositories)*.
- **Frameworks:** ExecuTorch, ONNX Runtime, TensorFlow Lite. *(Verified)*.
- **Android Docs:** Android MediaCodec, WorkManager, Foreground Services, Storage Access Framework, MediaStore, NDK ABIs, CDD, BatteryStats. *(All official Google documentation)*.

---

## 7. Strengths
- **Engineering Realism & Intellectual Honesty:** Contains an unprecedented self-audit section (§5 / Self-audit) explicitly highlighting its own weakest claims, optimistic memory budgets, and speculative hardware assumptions.
- **Flawless Taxonomy:** One of the very few plans that completely dissects the "lossless" trap, warning developers and users that re-compressing lossy video with FFV1 expands file size by 300%–800%, and providing the byte-lossless remux fallback.
- **Exhaustive Android Platform Deep Dive:** Comprehensive coverage of Android 15/16+ background limits, thermal manager throttling, 16 KB memory page compatibility, and Scoped Storage/MediaStore requirements.
- **Defensive Legal Clarity:** Clean separation between LGPL and GPL components in FFmpeg, explicit patent pool risks (HEVC Advance vs Access Advance), and realistic open-source licensing guidelines.

---

## 8. Weaknesses
- **Conservative on Algorithmic Inventions:** Rejects all learned video compression (DCVC, MLVC) and neural entropy models due to mobile non-determinism, potentially leaving future efficiency gains on the table if standardized neural codecs mature.
- **Memory Budget Optimism:** Target of holding working memory to $2\times$ decoded frame size is acknowledged by the author as overly optimistic for multi-threaded SVT-AV1 4K encoding which typically demands 800 MB–1.5 GB.
- **Lack of Micro-Architectural SIMD Detail:** Focuses on high-level library integration and architectural routing rather than custom NEON assembly kernels or specific fast motion-estimation algorithms.

---

## 9. Scores
- **Depth: 5/5** — Immensely detailed across architectural layers, Android lifecycle, memory layout, and operational failure modes.
- **Accuracy: 5/5** — Highly accurate regarding mobile SoC constraints, VMAF modeling, licensing, and Android OS limitations.
- **Originality: 3.5/5** — Prioritizes proven, battle-tested standard codecs over speculative novel algorithms, though its hardware pre-flight probe and thermal gear-shifting are excellent.
- **Feasibility: 5/5** — Completely implementable by a senior engineering team today without unresolved scientific roadblocks.
- **Completeness: 5/5** — Covers end-to-end scope from CI/CD, testing corpus, red-team analysis, team budget, to post-launch adoption metrics.
- **Evidence: 5/5** — Every single citation is real, every claim is classified (verified, author claim, estimate, or hypothesis), and includes a full claims ledger.
- **Overall Score: 4.75 / 5.0** (Top-tier candidate for architectural backbone).
