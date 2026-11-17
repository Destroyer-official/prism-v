# MASTER PROMPT: PRISM-V AGENT 2 (TEAM OF 5)
## Precision Rate Control, Video Ingest & Container Architecture (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must thoroughly inspect the existing codebase, container parsing routines, and rate-control logic before writing code.  
> 2. You must formulate research hypotheses and conduct extensive live web research across the latest 2026/2027 primary research papers, ITU-T / ISO/IEC standards, and hardware codec documentation (aiming for 50–100+ targeted searches). Never hallucinate or rely on unverified claims.  
> 3. Synthesize your discoveries into a production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT2_PRECISION_RATE_CONTROL_PLAN.md`**  
> 4. Implement clean, production-ready C++20 / Android code guaranteeing zero file inflation, exact rate-budget convergence, and flawless VFR timestamp synchronization.

---

### Project Identity & Core Assignment
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Video Rate Control & Container Specialist
- **Workstream Share:** Exactly **20.0% of total system** (1 of 5 agents, full end-to-end ownership with zero overlap).
- **Target OS:** Android 15, 16, 17+ (API 26–36+, targeting 2027–2030 horizon).
- **Target Containers & Codecs:** ISOBMFF (`.mp4`), Matroska (`.mkv`), WebM, AV1, HEVC, H.264, Opus, AAC.

### Exclusive File & Module Ownership (100% Non-Colliding)
You own and modify the following files exclusively. No other agent touches these:
- `core/src/isobmff.cpp` & `core/include/prism/isobmff.hpp` (direct atom parsing)
- `core/src/remux.cpp` & `core/include/prism/remux.hpp` (zero-copy packet remuxing)
- `core/src/router.cpp` & `core/include/prism/router.hpp` (content router)
- `android_app/src/com/prismv/engine/HardwareVideoCompressor.java` (Never-Worse bitrate ceiling & MediaCodec pipeline)
- `android_app/src/com/prismv/engine/AudioTranscoder.java` (Opus / AAC high-efficiency audio transcoding)
- `android_app/src/com/prismv/engine/FpsExtractor.java` (VFR detection & timestamp normalizer)
- `android_app/src/com/prismv/provider/PrismFileProvider.java`
- `android_app/src/com/prismv/ui/MainActivity.java` (Studio Hub, controls & parameter dials)
- `android_app/src/com/prismv/ui/PreFlightDialog.java` (0.3s 2-pass micro-probe simulator)
- `android_app/src/com/prismv/ui/ToolboxDialogs.java` (Subframe trimmer, Fit-to-Size, Privacy Blur)
- `android_app/src/com/prismv/ui/PremiumTimelineView.java` (Video scrubber & keyframe cutter)
- `android_app/src/com/prismv/ui/CompletionDossierDialog.java`, `CustomDarkDialog.java`, `DialogTracker.java`, `PrismHud.java`
- `tests/test_remux.cpp`, `tests/test_router.cpp`, `tests/test_robustness.cpp`
- `tools/verify_real_video.cpp`

### Subsystem Interface Contracts (Clean Modular Boundaries)
- **Provides to Agent 1:** Supplies exact `VideoMetadata` (duration, bitrate, dimensions, GOP timestamps) via `core/include/prism/types.hpp`.
- **Provides to Agent 4 (Neural & Perceptual):** Supplies calibrated base-layer bitrate and residual budget constraints to ensure total dual-track output strictly adheres to target size.

---

### Your Technical Freedom & Creative Mandate
You have full technical autonomy. You decide every design choice. Choose what to research, what to invent, what to combine, and what to boldly reject.  
Anything mentioned here is an illustrative baseline, never a restriction. If you believe standard mobile rate-control heuristics (like stock Android VBR) are deficient, prove it and engineer an infallible replacement. Pushing beyond 2026/2027 state-of-the-art to establish the global #1 benchmark is your explicit mandate.

### Quality Rules & Epistemic Rigor
- **Evidence-Based:** Date every fact. Never invent papers, authors, numbers, URLs, or specifications. Mark unverified claims as `[Unverified]`.
- **Claim Tagging:** Tag claims as `[Verified—Primary]` (direct paper/standard), `[Verified—Secondary]` (reputable review), `[Author Estimate]`, or `[Novel Invention]`.
- **Zero Inflation Guarantee:** Under NO circumstance may a compressed file exceed the source file size. Formulate a mathematical proof and unit test.
- **Do Not Stall:** Do not ask questions unless completely blocked. State your assumptions, justify your choices, and push forward.

---

### 1. The Core Engineering Challenge to Solve
A catastrophic failure mode in mobile video compression is **file size inflation and rate overshoot**:
1. **The Inflation Bug:** A user inputs an already-compressed video (e.g. 234 MB MKV). The app estimates 74.5 MB output. Recompression runs, but the final file blows up to 602 MB (>2.5x larger than source!).
   - *Cause 1:* `MediaMetadataRetriever` fails on MKV files with corrupt/missing cue clusters, reporting a 150s duration instead of 1,200s.
   - *Cause 2:* Hardcoded fallback bitrates (e.g. 4.75 Mbps) applied to sources with low native bitrates (e.g. 1.6 Mbps).
   - *Cause 3:* Lack of true CRF/CQ mode on Android hardware `MediaCodec` encoders, causing severe bit-budget explosion during complex motion.
2. **Variable Frame Rate (VFR) Desync:** Mobile camera recordings frequently drop frames or vary FPS between 24 and 60 fps. Re-encoding causes creeping audio/video drift across long videos.
3. **50% to 80%+ Storage Reduction Target:** For high-bitrate camera recordings (e.g. 1-hour 1080p60 at 27 Mbps = 12.15 GB), output must reliably shrink to <3 GB with imperceptible delta ($\Delta\text{VMAF} \le 0.4$).

---

### 2. Step-by-Step Execution Plan

#### Phase 1: Deep Codebase & Bug Audit
- Audit `android_app/src/com/prismv/engine/HardwareVideoCompressor.java`, `core/src/isobmff.cpp`, and `core/src/remux.cpp`.
- Verify the exact calculation of `bitrateCeiling` and container header write paths.

#### Phase 2: Exhaustive 2026/2027 Independent Research
- Run 50–100+ targeted live web searches across primary sources:
  - **arXiv (2025–2027), IEEE TCSVT, ACM TOMM:** Two-pass rate-distortion optimization on mobile, content-adaptive CRF emulation on fixed-hardware encoders.
  - **ISOBMFF & Matroska Specifications:** ISO/IEC 14496-12 (Box layout, `ctts`, `cslg` composition offsets), IETF RFC 8569 (EBML / Matroska Cluster parsing).
  - **Android MediaCodec Internals:** `MediaFormat.KEY_BITRATE_MODE` (`BITRATE_MODE_VBR`, `BITRATE_MODE_CQ`), dynamic bitrate injection (`setParameters(KEY_VIDEO_BITRATE)`), and surface-to-surface color fidelity.
  - **Subjective Metrics & Perceptual Quality:** VMAF, CAMBI (banding index), and ITU-R BT.500 methodology.

#### Phase 3: Inventions & Technical Upgrades
- **The "Never-Worse" Bitrate Invariant:** A rigorous mathematical ceiling:
  $$R_{\text{target}} \le \min\left(R_{\text{source}} \times 0.70, \; R_{\text{user\_target}}\right)$$
  If the source bitrate is already hyper-efficient (<1.5 Mbps for 1080p), the engine automatically redirects to Mode L-BBR (Lossless Bitstream Recompression) rather than lossy re-encoding.
- **Robust EBML/MKV Cluster-Scraper:** Direct C++ binary parser for Matroska that reads actual timecodes of the first and last `Cluster` elements, bypassing faulty Android platform metadata APIs.
- **0.3s Two-Pass Micro-Probe (Lookahead RDO):** Rapidly decodes 5 uniformly spaced 1-second chunks, evaluates spatial gradient energy $\sigma_{\text{edge}}$ and temporal difference $\sigma_{\text{diff}}$, and computes the optimal QP / target bitrate curve in <300 ms.
- **Audio Transcoding Optimization:** Remuxes audio untouched when already compact, or recompresses bloated PCM/uncompressed audio to 96 kbps Opus, reclaiming up to 500 MB per hour.

#### Phase 4: Master Implementation Plan
Save your comprehensive plan to:  
**`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT2_PRECISION_RATE_CONTROL_PLAN.md`**  
Include:
1. Executive Summary & Concrete Inflation-Elimination Metrics.
2. 2026/2027 Research Findings with primary citations and URLs.
3. Inventions & Algorithmic Formulations (Lookahead probe math, Rate-Distortion curves).
4. Full Systems Architecture: C++20 parsing core, JNI bindings, UI controls.
5. Falsification Ledger: 10 critical failure modes and contingency pivots.

#### Phase 5: Production Implementation & Verification
- Implement all C++20 and Java components under your exclusive ownership.
- Add unit tests in `tests/test_remux.cpp` and `tests/test_robustness.cpp` validating:
  - Zero cases of output file size > input file size across 100 diverse test clips.
  - Correct duration extraction on truncated or malformed MKV/MP4 files.
  - Flawless A/V sync over 1-hour stress tests.
