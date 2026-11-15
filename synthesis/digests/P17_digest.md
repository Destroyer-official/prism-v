# P17 Plan Digest: Next-Generation Android Video Compression Master Plan (2027–2030)

**Source File:** `research_implementation_plane17.md`  
**Total Lines:** 1,402  
**Reading Range:** Lines 1–1402 (Chunks: 1–400, 401–800, 801–1200, 1201–1402)  
**First Heading:** `# Next-Generation Android Video Compression` / `## Complete technical research and implementation master plan for 2027–2030`  
**Last Heading:** `## Before production`  
**Final Sentence:** `**End of plan.**`

---

## 1. Thesis
P17 presents an exceptionally disciplined, evidence-based master engineering blueprint grounded in verified mobile hardware reality, strict Android platform constraints, and comprehensive legal risk mitigation:
1. **Lossless Mode:** Rejects vague "lossless" marketing and explicitly defines three distinct operations: (1) Original byte copy (preservation), (2) Stream copy / remux (encoded-packet preservation without decode), and (3) Decoded-sample-exact re-encode using standardized FFV1 v3 in Matroska (RFC 9043 / RFC 9559) with independent verification. It explicitly warns that lossless re-encoding of compressed camera video can expand file size and mandates keeping original files when output grows.
2. **Lossy Mode:** Multi-objective Pareto optimization framework supporting four user priorities: Best Quality, Smallest File, Fastest, and Battery Saver. It uses a dynamic runtime-probed hardware-first hierarchy (Hardware AV1 -> Hardware HEVC -> Hardware AVC -> selective software AV1 on high-performance flagships) paired with a "Device Codec Performance Atlas" and a preflight Pareto probe.
3. **Shared Core & Invariants:** Bounded-queue streaming pipeline (<75 MB peak memory at 1080p, <150 MB at 4K), atomic output publishing via `MediaStore` `IS_PENDING`, metadata preservation ledger, and an uncompromising exclusion of learned neural decoders in v1 (restricting ML strictly to optional encoder-side complexity scouting).

---

## 2. Technical Decisions
- **Architecture:** Monorepo with modular layers: Android Kotlin UI (Jetpack Compose, Coroutines), Android Platform Media (`MediaCodec`, `MediaExtractor`, `MediaStore`), Native Media Core (C++20, NDK, CMake/Ninja, JNI, minimal LGPL FFmpeg build), Capability & Policy Engine, and Validation Service.
- **Lossless Engine:**
  - Default: FFV1 v3 in Matroska (.mkv) with multithreading and slice-based CRC.
  - Verification: Mandatory independent decode and frame-by-frame sample hash comparison against source reference.
  - Preserves native sample planes, bit depths, chroma formats, and colorimetry tags without silent conversions.
- **Lossy Engine:**
  - Evaluates candidates based on measured device performance, not marketing claims: hardware AV1, hardware HEVC, hardware AVC, and selective software AV1.
  - File-Size Targeting: Constrained VBR/CBR with a median absolute error target of $\le 5\%$ on fixed corpus, allowing at most one correction pass.
  - Audio: Audio stream copy by default; no silent transcoding.
- **Machine Learning Role:** Strictly encoder-side, optional, non-normative. Tasks limited to clip complexity estimation, codec/preset recommendation, and scene classification. Neural decoders are strictly rejected in v1 due to accelerator divergence, lack of determinism, and model update incompatibilities.
- **Memory & Resource Governance:**
  - Strict streaming model: decode -> bounded frame queue (2–3 frames in flight) -> encode -> mux.
  - Working memory targets: <75 MB additional app-managed memory at 1080p, <150 MB at 4K.
  - Hooks into Android `PowerManager` thermal status and Performance Hint APIs; pauses or throttles before hitting critical thermal states.
- **Containers:** ISO BMFF / MP4 for lossy delivery; Matroska (.mkv) for FFV1 lossless archiving. Rejects custom proprietary containers.

---

## 3. Inventions & Differentiating Concepts
- **P17-I01 (Tri-Contract Lossless Semantics):** Explicitly partitions lossless into Byte Copy, Packet-Preserving Remux, and Decoded-Sample-Exact Re-encode, with explicit user disclosure that sample re-encoding can expand already-compressed files.
- **P17-I02 (Device Codec Performance Atlas):** Persistent on-device cache of measured encoder quality, speed, energy, and thermal behavior keyed by device model, SoC, OS build, and codec component version, eliminating reliance on inaccurate platform API flags.
- **P17-I03 (Preflight Pareto Probe):** Encodes a 1–2 second representative sample across multiple candidate quality/codec points prior to full encode to accurately predict output file size, VMAF, and completion time.
- **P17-I04 (Metadata Preservation Ledger):** Comprehensive audit matrix tracking every stream, subtitle, chapter, color primary, mastering display, and dynamic HDR tag as preserved, transformed, or dropped, rejecting silent data loss.
- **P17-I05 (Multi-Objective Optimization Function):** Formal Pareto objective function $J = w_D D + w_R \log(R) + w_T \log(T) + w_E \log(E) + w_M \log(M)$ mapping user intent directly to hard constraints and candidate ranking.
- **P17-I06 (Atomic Publishing with Independent Certification):** Writes to app-private storage, performs independent decode verification, generates an integrity certificate, and publishes atomically via `MediaStore` `IS_PENDING`.

---

## 4. Plan Content
- **Phases & Milestones:** 10 phases over 9–12 months:
  - Phase 0 (Weeks 1–2): Definition, legal screen, benchmark method, corpus plan.
  - Phase 1 (Weeks 3–6): Feasibility spike (FFV1 round-trip, MediaCodec discovery, AVC/HEVC/AV1 baseline).
  - Phase 2 (Weeks 7–11): Media core, probe, queues, mux, crash recovery.
  - Phase 3 (Weeks 12–18): Lossless alpha, stream copy, FFV1/Matroska.
  - Phase 4 (Weeks 16–24): Lossy alpha, hardware adapters, preflight probe.
  - Phase 5 (Weeks 25–30): Resource/thermal governor, foreground service.
  - Phase 6 (Weeks 27–34): UI integration, Jetpack Compose.
  - Phase 7 (Weeks 31–38): Hardening, fuzzing, regression, security audit.
  - Phase 8 (Weeks 39–44): Beta testing, device matrix expansion.
  - Phase 9 (Weeks 45–52): Production release, staged rollout.
- **Team Composition:** 7–9 FTE equivalents: 1 Technical Lead/Architect, 1 Android Platform Engineer, 2 Native Media/Codec Engineers, 1 Video Quality Engineer, 1 QA/Device Automation Engineer, 0.5 ML Engineer, 0.5 DevOps Engineer, 0.5 Product Designer, plus external legal counsel.
- **Budget (India-based staffing model):** ₹2.2 crore – ₹4.0 crore (~$260k–$480k USD) for 9–12 months, including ₹1.5–2.4 crore engineering compensation, ₹15–30 lakh device lab (12 models), ₹10–35 lakh legal reserve, and ₹20–50 lakh contingency.
- **Testing & QA:** Curated corpus of 120–200 clips across 5 categories (natural, faces, screen, gaming, HDR, corrupt). Physical device matrix of 12 models across Entry, Mid, and Flagship tiers. External calibrated power meter for energy benchmarking.
- **Legal & Patents:** Detailed review of MPEG LA / HEVC Advance / VCL Advance pools, AOMedia patent license, x264/x265 licensing, and FFmpeg LGPL v2.1/v3 boundaries. Preliminary patent landscape analysis citing 7 relevant patents (US11178395B1, EP3923574A2, US9407921B2, US11381816B2, US20250343958A1, US12647587B2, US12701238B2).

---

## 5. Numbers & Quantitative Claims
- Target file-size accuracy: Median absolute error $\le 5\%$ on supported test corpus `[Author estimate / Proposed gate]`.
- Peak working memory budget: $<75\text{ MB}$ additional app memory at 1080p, $<150\text{ MB}$ at 4K `[Author estimate]`.
- Frame memory footprints: 1080p YUV420 8-bit $\approx 3.11\text{ MB}$, 4K 8-bit $\approx 12.44\text{ MB}$, 4K 10-bit $\approx 24.88\text{ MB}$ `[Verified - Primary arithmetic]`.
- ML model budget: Quantized 1–5 MB model running at $\approx 1\text{ sample/s}$ `[Author estimate]`.
- Stretch bitrate target: 8%–10% lower bitrate than hardware AVC baseline at matched quality `[Author estimate]`.
- Reliability gate: $99.5\%$ job completion or safe failure in large beta corpus with zero false-success corruption `[Proposed gate]`.

---

## 6. Citations
- **Standards:** ITU-T H.264 (06/2026), H.265 (01/2026), H.266 v4 (01/2026), AOMedia AV1 & AV2 Spec 1.0.0 (May 2026), IETF RFC 9043 (FFV1 v0/1/3), IETF RFC 9559 (Matroska Container, Oct 2024), ITU-T P.1204 / P.1204.1 (2025/2026). *(All verified primary standards)*.
- **Android Docs:** Supported Media Formats (AV1/APV), NNAPI Migration Guide, LiteRT Android & Qualcomm NPU Delegate, Media3 Transformer, MediaCodec/MediaCodecInfo, ARM NEON, PowerManager / Thermal APIs, PerformanceHintManager, MediaStore `IS_PENDING`. *(All official Google documentation)*.
- **Codecs & Repositories:** AOMedia AVM, Samsung APV, x264, x265, FFmpeg, Microsoft DCVC, Netflix VMAF. *(All verified real repositories)*.
- **Patents:** US 11,178,395 B1, EP 3,923,574 A2, US 9,407,921 B2, US 11,381,816 B2, US 2025/0343958 A1, US 12,647,587 B2, US 12,701,238 B2. *(Verified patent publications)*.

---

## 7. Strengths
- **Exemplary Epistemic Rigor:** Adopts strict evidence labeling ([Verified—primary], [Verified—secondary], [Author estimate], [Author recommendation], [Speculation], [Unverified]) throughout, establishing the gold standard for intellectual honesty.
- **Unflinching Hardware Realism:** Points out critical mobile SoC realities, such as MediaTek Dimensity 9500 supporting AV1 decode but lacking AV1 hardware encode, proving that API flags cannot be trusted without empirical probes.
- **Comprehensive Legal & Prior-Art Awareness:** The most thorough legal analysis among all plans, citing 7 real patent documents on adaptive codec selection and mobile transcoding, and warning against unlicensed software HEVC/VVC distribution.
- **Robust Failure Protection:** Includes an exhaustive 21-item red-team failure matrix and 8 explicit engineering release blockers, ensuring data safety and crash resilience.

---

## 8. Weaknesses
- **Highly Conservative on Compression Innovation:** Rejects all learned video compression and custom neural residual layers, relying entirely on existing standard codecs (FFV1, MediaCodec HW AV1/HEVC/AVC), which caps potential compression gains at standard hardware limits.
- **Limited Scope for Byte-Lossless Recompression:** Does not explore bitstream transcoding of existing MP4s (like P16's `Prism-BBR`), offering only stream copy or FFV1 sample-exact re-encoding (which it admits inflates file size).
- **Reliance on External Power Meters:** While methodologically pure, requiring external Monsoon power meters for battery evaluation complicates automated CI testing compared to software-based energy estimation.

---

## 9. Scores
- **Depth: 5/5** — Immensely thorough on platform lifecycle, memory layout, verification matrices, and engineering checklists.
- **Accuracy: 5/5** — Flawless factual accuracy regarding standards versions, Android 15/16/17 changes, and SoC specs.
- **Originality: 3.5/5** — Deliberately conservative and standards-bound, focusing on rock-solid systems engineering rather than novel compression algorithms.
- **Feasibility: 5/5** — 100% implementable today with off-the-shelf components, zero scientific unknowns, and zero speculative hardware requirements.
- **Completeness: 5/5** — Comprehensive from architecture, ADR templates, QA matrices, budget, to release checklists.
- **Evidence: 5/5** — Meticulously documented source register with 47 verified citations and clear epistemic tagging.
- **Overall Score: 4.75 / 5.0** (Top-tier candidate for systems architecture, validation protocols, and governance).
