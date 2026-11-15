# P11 Digest: LOOM — Next-Gen On-Device Android Video Compression System (2027–2030)

**Source File:** `research_implementation_plane11.md`  
**Total Lines:** 1,868 | **Bytes:** 188,862  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Prepared for @shantanu (Revision 1.0)  

---

## 1. Thesis
P11 argues that the primary lever for mobile video compression in 2027–2030 is **not inventing a new codec**, but exploiting the critical asymmetry of modern mobile hardware:
- **The Core Asymmetry:** Phone hardware encoders are measurably inferior to software reference encoders (e.g., Qualcomm hardware HEVC is ~6.8 to 10.2 VMAF points below software reference at 1080p, largely lacking B-frames), yet pure software encoding at 4K consumes 8%–28% of a phone battery per 10 minutes of video. Furthermore, neural *encoding* is non-viable on-device (~3 fps in MobileNVC), whereas neural *decoding* is viable in real-time (~38.9 fps 1080p in MobileNVC, ~100 fps 540p in MLVC).
- **Three User-Facing Modes (Not Two):**
  1. *Lossless-Repack (L-A):* Bit-exact recompression of existing files (container cleanup + symbol rearrangement; honest expectation: 2%–8% median, 0% on AV1). Plus *Lossless-Library (L-B)* (cross-file gallery deduplication, saving 20%–50% of duplicate bytes) and *Lossless-Capture (L-C)* (raw screen recording / UI frames, achieving 5–15× ratio).
  2. *Quality-Targeted Lossy:* Mode Q-S (share, <60s) using hardware `MediaCodec` with quality gating; Mode Q-B (bulk, charging) using SVT-AV1 tuned for ARM, a single-pass per-shot rate-quality allocator, and denoise + film-grain synthesis (FGS).
  3. *Flagship Hybrid Architecture (I-8):* Hardware encoder generates a standard, universally decodable base track in real time (~0.02 Wh/min); an energy-budgeted learned enhancement/residual layer (non-normative, integer-bounded, transmitted scale parameters) recovers 50%+ of the hardware encoder's quality deficit at <30% of a full software encode's energy.
- **Architectural Tenet:** **Machine learning is NEVER in the normative decode path.** ML is strictly non-normative post-enhancement, completely preventing cross-device floating-point decoder drift.
- **Transactional Safety:** No original video is ever deleted until the replacement is verified on-device via a cryptographic **Quality Certificate** attached to a **Verified Replacement Transaction**.

---

## 2. Technical Decisions
- **Architecture & System Design:** 7-layer stack: UI (Kotlin/Compose), Orchestration (WorkManager/ADPF), Policy Core (Rust), Codec Layer (MediaCodec/SVT-AV1), Perception (C++ NEON/SVE2), ML (LiteRT 2.3+), Storage (Chunker/Lineage).
- **Codecs & Standards Selection:**
  - Lossy Base & SW Encode: Hardware `MediaCodec` (HEVC/AVC) for capture/fast mode; `SVT-AV1` (BSD-3) for batch charging mode; `APV` (IETF RFC 9924) for professional creator editing.
  - Decoders: `dav1d` (BSD-2) for AV1; platform `MediaCodec` for HEVC. Strictly **rejects `libde265`** (LGPL relinking risks) and **`x265`** (GPLv2 viral open-source risk).
  - Rejections: Defers VVC (licensing chaos, no mobile encoders) and AV2 (silicon decoders absent until 2028+).
- **Machine Learning & Determinism:**
  - Runtime: LiteRT 2.3+ (`CompiledModel` API, QNN delegate for Qualcomm Hexagon, GPU delegate fallback).
  - Determinism & Drift Elimination: Transmits scale parameters through the hyperprior (MLVC method); integer-bounded post-filtering (E-1) and low-rank residuals (E-2); non-normative execution (file remains 100% playable if NPU enhancement is skipped).
  - Model Sizes: E-1 post-filter ≤3 MB INT8 (bundled); E-2 low-rank residual ≤18 MB INT8 (on-demand Play Feature Delivery).
- **Languages & Tools:**
  - Rust: Transaction state machine, per-shot allocator, energy governor math, storage chunker, lineage graph, JNI boundary (`loom-ffi`).
  - C++20: `AMediaCodec`/`AMediaMuxer` wrappers, ISO BMFF parser/writer, NEON/SVE2 SIMD kernels.
  - Kotlin + Compose: UI, WorkManager, Media3 ExoPlayer custom `VideoRenderer` surface hook.
  - Python / PyTorch: Training on 8×A100/H100 clusters.
- **Hardware Mapping:** CPU (Armv9.3-A SVE2/SME2), VPU (`MediaCodec`), NPU (LiteRT QNN), GPU (Vulkan compute). Media buffers stay in `AHardwareBuffer` / zero-copy direct buffers; never crossed over JNI.
- **Container Format:** ISO BMFF (`.mp4`) with standard base track (`av01` or `hvc1`) + private enhancement track in a `uuid` box + Quality Certificate.
- **Quality Metrics:** Primary: **SSIMULACRA2** (p10/p50/p90 per shot); Banding: **CAMBI**; Temporal Consistency: **Warp-Error ($E_{\text{warp}}$)**; Grain-blind comparison; Secondary: VMAF-NEG, CVVDP. Banned: reporting a single mean BD-rate without distribution and hard floors.

---

## 3. Inventions & New Ideas
P11 details 12 concrete inventions (I-1 through I-12):
1. **I-1: Verified Replacement Transaction:** Five-state transactional replacement (`CANDIDATE` $\to$ `LOCAL_VERIFY` $\to$ `COMMIT` $\to$ `GRACE` $\to$ `IRREVERSIBLE`) with shadow copies, persistent receipts, and an undo window.
2. **I-2: Single-Pass Per-Shot Budget Allocation:** Floor-maximizing allocator (optimizes $p10$ quality instead of mean) using a lightweight gradient-boosted tree rate-quality predictor (<200 KB, <1 ms/shot) saving 15%–30%.
3. **I-3: Energy Governor:** Treats encoding as an ADPF power contract (sustained $\le$3.5 W); dynamic work shaping (reduces candidate modes, then analysis resolution, then preset) and WorkManager thermal deferral to charging/overnight.
4. **I-4: Sensor-Noise-Aware Denoise and Re-Grain:** Metadata-conditioned AR noise modeling reading Exif ISO/exposure; smooth inter-shot grain interpolation and debanding dither (saving 25%–40% on noisy video).
5. **I-5: Hybrid Bitstream:** ISO BMFF container housing a hardware base track + private `uuid` enhancement track under a bounded-drift contract.
6. **I-6: Gallery Redundancy Elimination with Quality Tiers:** Exact + near-duplicate content-defined chunking (Rabin hash); preserves paths by downcoding inferior copies and referencing the master.
7. **I-7: Honest Lossless Repack (L-A):** Container/metadata trimming + bitstream symbol rearrangement with reversible delta patch (2%–8% median).
8. **I-8: Flagship Hybrid Architecture:** Hardware base + learned enhancement layer (E-1 post-filter 5%–10%; E-2 low-rank residual 12%–22%; E-3 generative refinement 20%–35%).
9. **I-9: Conformance Oracle:** Golden vectors + on-device startup self-test + fleet quirk database to catch cross-device NPU divergence before users do.
10. **I-10: Sampled Quality Verification:** Verifies quality on shot boundaries, lowest-complexity 20% frames, and high-motion frames with an adaptive early-stop rule (reducing verification energy to 15%–25%).
11. **I-11: Cryptographic Quality Certificate:** Embedded signed manifest containing per-shot metric vectors, thresholds, provenance hashes, and measured joules.
12. **I-12: Edit-Lineage Storage:** Immutable masters with derived compressed children, exposing marginal storage cost and "edit-from-master" workflows.

---

## 4. Plan Content
- **Phases & Roadmap (8 Phases, 20.5 Months / 88 Weeks):**
  - *M0 (W1–8, $90k):* Test rig, 6-phone device lab, 320-clip corpus, G0 reproduction gate.
  - *M1 (W9–20, $320k):* Five kill-tests (K1 encoder gap, K2 hybrid recovery, K5 conformance, K3 lossless, K4 dedup) $\to$ **Gate G1 Decision Point**.
  - *M2 (W21–36, $700k):* Transactional core, hardware path, per-shot CRF, denoise/re-grain, UI v1 (Gate G2).
  - *M3 (W37–56, $1.3M):* SVT-AV1, E-1/E-2 models, LiteRT/QNN, hybrid BMFF writer (Gate G3).
  - *M4 (W57–68, $600k):* Lossless L-A repack, L-C screen capture, L-B dedup, space dashboard (Gate G4).
  - *M5 (W69–80, $560k):* Low-end 2GB RAM path, thermal deferral, localisation (Gate G5).
  - *M6 (W81–88, $450k + legal):* Security fuzzing, licence audit, 10,000-file restore test, 1.0 release (Gate G6).
  - *M7 (M21+):* Post-1.0 research (AV2 readiness, VVC decode, E-3 evaluation).
- **Team & Budget:** Peak 18 people (~15 FTE engineers). Total budget: **$4.6M–$5.2M** (₹40–46 crore). Fallback Plan C (no ML): $1.9M (₹17 crore) over 12 months.
- **Testing & Lab:** 320-clip multi-class corpus; 6-device hardware farm (SD 8 Elite Gen 5, Dimensity 9600 Pro, Tensor G5, SD 7-series, 4GB Mali-G52 low-end, 2GB Android 11/12).
- **Legal Strategy:** Focuses on patenting 5 system-level inventions (I-1+I-11, I-6+I-1, I-3, I-5+I-8, I-4) while defensively publishing algorithms. Avoids GPLv2 completely. Allocates $250k–$420k legal budget.

---

## 5. Numbers & Quantitative Claims
- **Hardware Deficit:** Qualcomm HW HEVC is −6.77 VMAF points vs software at 1080p, −10.20 at low bitrate `[V1, arXiv:2511.18686]`.
- **Software Encode Energy:** 1 minute 1080p software AV1 costs 0.2%–0.7% battery; 10 minutes 4K costs 8%–28% battery `[EST]`.
- **FGS Bitrate Reduction:** Netflix production rollout achieved −31.6% average bitrate with film-grain synthesis `[V1]`.
- **Dynamic Optimizer Per-Shot Gain:** 17.1% VP9 average saving (Netflix) `[V2]`; 35.34% in independent replication (Ozer 2026) `[V2]`.
- **MobileNVC Throughput:** 38.9 fps 1080p decode on Snapdragon 8 Gen 2 (INT8); encode ~3 fps `[V1, arXiv:2310.01258]`.
- **MLVC Performance:** ~100 fps encode/decode at 540p on mobile NPUs; >70% MOS BD-rate vs HW HEVC `[V1, arXiv:2606.28027]`.
- **APV Storage Cost:** ~6 GB/min at UHD30 (up to 2 Gbps) `[V2]`.
- **Lossless Realities:** Bit-exact repack saves 2%–8% median; gallery dedup saves 20%–50% of duplicate bytes `[EST]`.

---

## 6. Citations & Live Verification Status
- *RFC 9924 (APV Codec, Feb 2026)*: Verified primary standard.
- *RFC 9043 (FFV1, Aug 2021)*: Verified primary standard.
- *AV2 Specification v1.0.0 (May 28 / Jun 9, 2026)*: Verified primary standard.
- *arXiv:2502.20762 (DCVC-RT)* & *arXiv:2606.04410 (DCVC-UF)*: Verified primary papers.
- *arXiv:2606.28027 (MLVC, Jul 2026)*: Verified primary paper with code.
- *arXiv:2310.01258 (MobileNVC, WACV 2024)*: Verified primary paper.
- *arXiv:2511.18686 (MSU HW Encoder Comparison, Nov 2025)*: Verified primary paper.
- *Android 16 CDD (§5.1) & Android 17 API 37 docs*: Verified primary documentation.
- *Netflix Tech Blog (FGS production, Jul 2025)*: Verified primary industry report.

---

## 7. Strengths
- **Unrivaled Epistemic Discipline (§0.1, §5.1, §6):** Every single fact is tagged (`[V1]`, `[V2]`, `[AC]`, `[EST]`, `[SPEC]`), and sources distinguish between primary documents and search summaries.
- **Identification of Mobile Hardware Asymmetry (§1.1, §2.2, §3.8):** Masterfully identifies that hardware encoders have a severe quality deficit while software encoders have a crippling energy penalty, making the hybrid architecture the only viable mobile path.
- **Architectural Immunity to NVC Drift (§1.1, §3.5, §3.8):** Strictly places ML in non-normative post-enhancement, eliminating cross-device decode corruption.
- **Transactional Safety & Trust Architecture (§3.1, §3.11):** I-1 and I-11 solve the human trust problem of compression apps by guaranteeing reversibility and cryptographic verification.
- **Transparent Self-Audit (§6.1):** Openly confesses the composition fallacy in adding separate percentages together and details exact falsification kill-tests.

---

## 8. Weaknesses
- **Composition Fallacy in Claimed Gains (§6.1):** Claims a 30%–45% total lossy saving by compounding separate gains (gap closure, FGS, per-shot, hybrid layer) that interact and may not add linearly.
- **Share-Sheet Fragility for Hybrid Container (§3.5, §6.1):** The private `uuid` enhancement track in ISO BMFF may be stripped by messaging apps (WhatsApp, Telegram) or Google Photos, degrading exports to base quality.
- **Large Team & Extended Timeline (§4.5):** 20.5 months and $4.6M–$5.2M budget is the largest among all plans, requiring 18 specialists across video codecs, Arm microarchitecture, and ML.

---

## 9. Scores
- **Depth:** 5/5 — Exhaustive technical analysis from Arm SME2/SVE2 assembly to LiteRT delegates, ADPF thermal loops, and BMFF boxes.
- **Accuracy:** 5/5 — Highest factual accuracy in the entire plan set; catches nuances like APV RFC 9924, CDD 480p AV1 encode minimum, and MLVC scale-transmission.
- **Originality:** 5/5 — Inventions I-1 through I-12 provide genuine systems-level innovations.
- **Feasibility:** 4/5 — Hybrid path is grounded in verified components, with clear Plan B and Plan C fallback gates.
- **Completeness:** 5/5 — Fully articulated roadmap, device lab, risk register, legal/patent strategy, and red-team self-audit.
- **Evidence:** 5/5 — Benchmark references, vendor databooks, and primary standards verified throughout.
