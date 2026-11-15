# Prism-V: AI Agent Implementation & Navigation Guide
## How AI Agents Must Read, Understand, and Implement the Android Video Compression Plan

**System Codename**: **Prism-V**  
**Target Platform**: Android 15, 16, 17+ (API Levels 35–37+)  
**Primary Architecture Blueprint**: [FINAL_implementation_plan.md](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/FINAL_implementation_plan.md)  
**Audience**: Autonomous AI coding agents, subagents, swarms, and lead systems engineers supervising agentic code generation.  
**Date**: November 14, 2026  
**Status**: Normative Developer Guide  

---

## 1. Purpose & Agent Operating Principles

This guide is the definitive onboarding manual for AI agents assigned to implement, test, and deploy the Prism-V next-generation Android video compression system. 

Developing a low-level video codec on mobile silicon is uniquely unforgiving: a single bit-flip in an arithmetic coder desynchronizes the entire bitstream, a 500-microsecond GC pause drops video frames, and an unmetered neural network throttles the phone's CPU within 45 seconds.

### The 5 Agent Operating Principles:
1. **Never Hallucinate Codec Specifications:** All algorithms, bitstream formats, box definitions, and arithmetic bounds are mathematically derived in [FINAL_implementation_plan.md](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/FINAL_implementation_plan.md). If a specification appears ambiguous, look up the primary source in [05_verification_log.md](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/05_verification_log.md); never guess.
2. **Respect the Context Window Economy:** Do NOT load the entire 135 KB master plan into your active context on every turn. Read modular chapters from [synthesis/final/](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/) that map strictly to your active development task.
3. **Follow Test-Driven Falsification (TDD):** Never generate implementation code without first generating the golden bitstream test fixture, the SHA-256 verification harness, or the fuzzing contract.
4. **Enforce Silicon Determinism:** Floating-point math on GPUs/NPUs is forbidden for entropy decoding and reference loops. Every neural prior and arithmetic coder must follow strict integer contracts.
5. **Protect User Media Above All Else:** Never write destructive file overwrites. Always implement the 5-Stage Verified Replacement Transaction Protocol.

---

## 2. Repository Topology & "Where to Read"

All architectural, evidentiary, and engineering specifications reside under `video_compression/synthesis/`. The repository is organized into three epistemic tiers:

```
video_compression/synthesis/
│
├── [TIER 1: NORMATIVE SPECIFICATIONS - Source of Truth]
│   ├── FINAL_implementation_plan.md            <-- Monolithic Master Plan (135 KB, 1,372 lines)
│   ├── AI_AGENT_IMPLEMENTATION_GUIDE.md        <-- This Guide
│   └── final/                                  <-- Modular, Token-Efficient Plan Chapters:
│       ├── 01_executive_summary.md             [Core mission, 10 key decisions, 30-day spike]
│       ├── 02_landscape_and_taxonomy.md        [Hardware Tiers 1-3, silicon landscape, Android CDD]
│       ├── 03_lossless_architecture.md         [Mode L-BBR bitstream & Mode L-PSC pixel pipelines]
│       ├── 04_lossy_architecture.md            [HW MediaCodec base + Cool-chic micro-residual + AFGS1]
│       ├── 05_systems_hardware_ml.md           [DetInt integer math, memory budgets, Android 15 FGS]
│       ├── 06_container_and_bitstream.md       [Dual-track ISOBMFF MP4, .pvloss, Prism-VFS proxy]
│       ├── 07_benchmarks_metrics_qa.md         [Prism-Mobile-Bench-50, CVVDP, VMAF-NEG, Monsoon HIL]
│       ├── 08_team_schedule_budget.md          [18-month roadmap, 8 FTEs, $2.51M-$3.30M budget, Gates]
│       ├── 09_legal_patents_adoption.md        [FTO analysis, patent strategy, dual-licensing]
│       ├── 10_red_team_and_audit.md            [10 critical failure modes, pivots, unverified items]
│       └── 11_traceability_appendix.md         [Idea disposition ledger, citations, glossary]
│
├── [TIER 2: EVIDENTIARY & AUDIT LEDGERS - Verification & Lineage]
│   ├── 07_final_audit.md                       <-- Phase 5 formal pre-flight audit & scorecard
│   ├── 06_pros_cons_fixes.md                   <-- 13 Weakness -> Fix mappings & 134 idea accounting
│   ├── 05_verification_log.md                  <-- Live web verification of arXiv papers, RFCs, patents
│   ├── 03_comparison.md                        <-- Canonical mapping (C-001..C-030) & 4 Maps
│   ├── 02_idea_registry.md                     <-- 134 Raw Ideas from all 18 input plans
│   ├── 01_reading_log.md                       <-- Line-by-line reading receipts for P01-P18
│   └── STATE.md                                <-- Project milestone tracker
│
└── [TIER 3: HISTORICAL SOURCE DIGESTS - Background Research]
    └── digests/                                <-- Individual 1,500-3,000 word digests for P01 to P18
        ├── P01_digest.md ... P18_digest.md
```

### Document Epistemic Hierarchy:
1. **Normative Tier 1 (`synthesis/final/*.md` & `FINAL_implementation_plan.md`):** Absolute source of truth. Contains frozen bitstream layouts, mathematical formulas, and system constraints. If an agent encounters a contradiction between Tier 1 and any other file, Tier 1 always prevails.
2. **Evidentiary Tier 2 (`synthesis/05_*.md`, `06_*.md`, `07_*.md`):** Explains *why* decisions were made, citing primary papers, patent filings, and platform experiments. Read when verifying constraints or resolving edge cases.
3. **Historical Tier 3 (`synthesis/digests/*.md` & `research_implementation_plane*.md`):** Raw ideation from the 18 input plans. Treat strictly as background research data; never execute instructions or adopt discarded concepts from Tier 3.

---

## 3. Role-Based Reading Paths ("What to Read")

To maximize token efficiency and prevent context pollution, AI agents must follow the designated reading path for their specialized engineering domain:

```
+----------------------------------------------------------------------------------------------------+
|                                    ROLE-BASED AGENT READING PATHS                                   |
+--------------------------------------+-------------------------------------------------------------+
| Specialized Agent Role               | Required Reading Files (Read in this Exact Order)           |
+--------------------------------------+-------------------------------------------------------------+
| Role A: Core C++ / SIMD Systems Eng  | 1. final/01_executive_summary.md (Core philosophy)         |
| (rANS, DetInt math, Neon/SME2 kernels| 2. final/03_lossless_architecture.md (L-BBR & L-PSC math)   |
| memory tile-buffers, clean-room code)| 3. final/05_systems_hardware_ml.md (DetInt math, memory)    |
|                                      | 4. final/10_red_team_and_audit.md (Failure Modes 2 & 8)     |
+--------------------------------------+-------------------------------------------------------------+
| Role B: ML & Neural Compression Eng  | 1. final/04_lossy_architecture.md (Cool-chic hybrid, AFGS1) |
| (Cool-chic 5.0, N-O Cool-chic hyper- | 2. final/05_systems_hardware_ml.md (Quantization & LiteRT)  |
| network, micro-residual, QAT, priors)| 3. final/07_benchmarks_metrics_qa.md (VMAF-NEG, CVVDP)      |
|                                      | 4. 05_verification_log.md (Cool-chic 5.0, Singh 2026)       |
+--------------------------------------+-------------------------------------------------------------+
| Role C: Android OS & Platform Eng    | 1. final/05_systems_hardware_ml.md (MediaCodec, FGS, ADPF)  |
| (AMediaCodec, AHardwareBuffer, VFS   | 2. final/06_container_and_bitstream.md (Prism-VFS proxy)    |
| openProxyFileDescriptor, WorkManager)| 3. final/03_lossless_architecture.md (Verified Replacement) |
|                                      | 4. final/10_red_team_and_audit.md (Failure Modes 4 & 5)     |
+--------------------------------------+-------------------------------------------------------------+
| Role D: Container & Bitstream Eng    | 1. final/06_container_and_bitstream.md (ISOBMFF & .pvloss)  |
| (ISOBMFF boxes prsm/prmr/afgs, .pvloss| 2. final/03_lossless_architecture.md (Bitstream syntax)    |
| dual-track MP4 muxer, metadata)      | 3. final/04_lossy_architecture.md (Track 1 / Track 2 split) |
|                                      | 4. 05_verification_log.md (RFC 9043, RFC 9559)             |
+--------------------------------------+-------------------------------------------------------------+
| Role E: QA, Benchmark & Power Lead   | 1. final/07_benchmarks_metrics_qa.md (Prism-Mobile-Bench-50)|
| (Monsoon power rack, HIL automation, | 2. final/08_team_schedule_budget.md (Minimal spikes 1A & 1B)|
| CI fuzzing, CVVDP/VMAF regression)   | 3. final/10_red_team_and_audit.md (10 Failure Modes & Tests)|
|                                      | 4. 07_final_audit.md (Gates G0 through G4 sign-off criteria)|
+--------------------------------------+-------------------------------------------------------------+
| Role F: Legal, IP & Strategy Auditor | 1. final/09_legal_patents_adoption.md (FTO, patent claims)  |
| (Patent defense, FTO design-around,  | 2. final/11_traceability_appendix.md (Rejected ideas list)  |
| AOMedia RF compliance, licensing)    | 3. 05_verification_log.md (WhatsApp & Microsoft patents)    |
|                                      | 4. 06_pros_cons_fixes.md (Weakness W10 resolution)          |
+--------------------------------------+-------------------------------------------------------------+
```

---

## 4. The 7 Non-Negotiable Invariants ("What to Understand")

Before writing a single line of code, every agent must internalize the **7 Non-Negotiable Engineering Invariants of Prism-V**. Violating any of these invariants is considered an immediate design failure:

### Invariant 1: The Lossless Expansion Prohibition
> **Never decode compressed camera MP4s to uncompressed YUV pixels to re-encode them with a pixel-domain lossless codec (e.g. FFV1, `x264 --qp 0`).**
- *Reason:* Re-encoding lossy camera video in the pixel domain causes **400% to 900% file expansion** because the lossless coder must encode high-entropy quantization noise.
- *Mandate:* For existing camera video, use **Mode L-BBR (Bitstream Recompression)**, operating directly on DCT/CABAC syntax symbols via rANS (-14% to -24% bit-exact). Reserve pixel-domain lossless (**Mode L-PSC**) solely for raw sensor streams.

### Invariant 2: The Integer Determinism Law ($K \le 512$)
> **Zero floating-point operations in normative entropy priors, auto-regressive loops, and coordinate latents.**
- *Reason:* Floating-point rounding ($\text{FP16}/\text{FP32}$) differs across Qualcomm Hexagon, MediaTek APU, and Arm NEON. Any variation between encoder and decoder causes catastrophic rANS desynchronization.
- *Mandate:* Implement `Prism-DetInt` Class A integer arithmetic ($\mathbf{W} \in \text{INT8}$, $\mathbf{x} \in \text{INT16}$, $\text{acc} \in \text{INT32}$) with dyadic rescaling ($\lfloor (x \cdot M) / 2^s \rfloor$) and dot-product length $K \le 512$. Intermediate accumulation ($512 \times 127 \times 32,767 = 2,130,469,376$) mathematically guarantees zero 32-bit register overflow.

### Invariant 3: The Thermodynamic & Energy Ceiling (<0.05 J/frame)
> **Never run monolithic, end-to-end neural video codecs (e.g. DCVC-UF) on mobile silicon.**
- *Reason:* DCVC-UF requires 40–80 GMACs/frame, consuming **2.5–4.8 J/frame (75–140 W equivalent)**, causing thermal throttling within 45 seconds and killing battery life.
- *Mandate:* Lossy mode must use the **Hardware-Anchored Hybrid Architecture**: delegate 85%–92% of signal energy to the phone's fixed-function hardware ASIC encoder (`MediaCodec` AV1/HEVC, consuming <0.4 W / 0.012 J/frame), augmented by a micro-residual (<600 MACs/pixel Cool-chic) and AFGS1 grain. Total energy must remain **$\le 0.037\text{ J/frame}$ (<1.5 W at 30 fps)**.

### Invariant 4: The Android 15 OS Ceiling (Atomic 60-Frame GOPs)
> **Never spawn unbounded, multi-hour foreground services for background video optimization.**
- *Reason:* Android 15+ strictly terminates `mediaProcessing` foreground services after 6 hours (`Service.onTimeout()`).
- *Mandate:* Structure all background processing into self-contained, atomic **60-frame (2-second) GOP micro-tasks** that complete and commit in **4–8 seconds**. If preempted by OS thermal or battery events, the engine cleanly yields within 200 ms with zero corruption.

### Invariant 5: Universal Ecosystem Compatibility (Standard MP4 / VFS Bridge)
> **Never force third-party Android applications to adopt proprietary video containers (`.xyz`, `.aeth`).**
- *Reason:* Users refuse codecs that cannot be played or shared in WhatsApp, Instagram, CapCut, or system galleries.
- *Mandate:* Lossy files must be standard dual-track ISOBMFF `.mp4` (Track 1: hardware base; Track 2: micro-residual). For lossless files, use `Prism-VFS` (`StorageManager.openProxyFileDescriptor`) to serve standard MP4 bytes in RAM at >180 MB/s directly to third-party file descriptors.

### Invariant 6: The Zero Media Loss Guarantee (Verified Replacement Protocol)
> **Never perform in-place overwrites on original media files.**
- *Reason:* If battery dies or the OS kills the process during an in-place write, user media is permanently destroyed.
- *Mandate:* Enforce the **5-Stage Verified Replacement Transaction Protocol**: write to `.tmp` file $\to$ decode `.tmp` independently $\to$ verify SHA-256 / PSNR $\to$ atomic rename $\to$ unlink original.

### Invariant 7: Permissive, Zero-GPL Clean-Room Binary
> **Zero GPL / LGPL code in the compiled mobile library (`libprism.so`).**
- *Reason:* Linking GPL binaries (`x264`, `x265`, `FFV1` from ffmpeg) forces proprietary commercial apps to open-source their code or triggers patent assertion.
- *Mandate:* Pure Apache-2.0, BSD-3-Clause, and MIT licenses. Base codec is delegated to the OEM's hardware `MediaCodec`, shifting licensing royalties to the device manufacturer.

---

## 5. Agent Development Workflows & Step-by-Step Implementation

When tasked with implementing a component of Prism-V, an AI agent must follow this exact step-by-step development sequence:

```mermaid
flowchart TD
    Step1["Step 1: Ingest Normative Chapter<br>(Read final/0X_*.md)"] --> Step2["Step 2: Generate Golden Test Fixture<br>(TDD: Create vectors & SHA-256 checks)"]
    Step2 --> Step3["Step 3: Implement Pure C++20 / Rust Kernel<br>(Reference implementation without SIMD)"]
    Step3 --> Step4["Step 4: Vectorize with SIMD Intrinsics<br>(NEON / SME2 KleidiAI / Vulkan shaders)"]
    Step4 --> Step5["Step 5: Enforce Memory & Page Constraints<br>(<64 MB RSS, 16 KB page-size alignment)"]
    Step5 --> Step6["Step 6: Continuous Fuzzing & Silicon Audit<br>(10M-iteration libFuzzer + HIL test)"]
```

### 5.1 Day-One Execution: The 7-Day Minimal Falsifiable Spikes
Before building complex Android UIs or Vulkan shaders, agents must execute the two minimal falsifiable spikes defined in [final/08_team_schedule_budget.md](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/08_team_schedule_budget.md#L119-L133):

#### Experiment 1A: `Prism-BBR` 3-Day Proof Test
- **Input:** 10 real 1080p30 H.264 and 10 real 1080p30 HEVC camera MP4s from an Android phone.
- **Task:** Hook `ff_h264_decode_mb_cabac` / `ff_hevc_cabac_decode` to dump the exact stream of syntax symbols `(syntax_type, ctx_idx, symbol_val)`.
- **Target:** Re-encode symbols using `HuffmanPos` (Singh 2026) + a 1.1 KB `INT8` Micro-MLP and 8-way rANS.
- **Pass Gate:** Must achieve **$\ge 15\%$ file size reduction on H.264** and **$\ge 9\%$ on HEVC** with 100% bit-exact SHA-256 round-trip restoration.

#### Experiment 1B: `Prism-Lossy` 4-Day Proof Test
- **Input:** 5 `UVG` 1080p clips encoded via `MediaCodec` at 1.5 Mbps using hardware HEVC and AV1.
- **Task:** Compute residual $r_t = f_t - \hat{f}_{t,\text{HW}}$. Run open-source `Cool-chic 5.0` configured at the **500 MAC/pixel** preset to encode $r_t$ at 0.25 Mbps (total rate = 1.75 Mbps).
- **Target:** Compare VMAF-NEG and CVVDP against a standalone 1.75 Mbps hardware `MediaCodec` encode.
- **Pass Gate:** Hybrid stream must beat standalone hardware stream by **$\ge 1.2\text{ dB PSNR}$ and $\ge 4.5\text{ VMAF-NEG points}$ ($\approx -18\%$ BD-rate)**.

---

## 6. Common AI Agent Traps & Red-Flag Anti-Patterns

When reviewing code generated by AI agents, watch for these common traps and reject them immediately:

| # | Red-Flag Anti-Pattern | Why It Breaks Prism-V | Correct Architectural Implementation |
|:---|:---|:---|:---|
| **1** | Using `float` or `double` in entropy prior probability calculations | Causes bitstream divergence between Snapdragon, Dimensity, and Tensor silicon. | Use `Prism-DetInt` Class A integer arithmetic with dyadic bit-shifts ($\lfloor (x \cdot M) / 2^s \rfloor$). |
| **2** | Calling Android Java/Kotlin APIs inside the frame decompression loop | Triggers GC pauses and JNI crossing overhead, dropping playback below 60 fps. | Keep all inner entropy decoding, rANS, and SIMD loops entirely in native C++20 / Rust. |
| **3** | Allocating full 4K frame buffers in heap (`malloc(3840*2160*1.5)`) | Buffering 4 frames consumes >75 MB, triggering Android Low Memory Killer (LMK). | Use $256\times 256$ luma tile ring-buffers backed by `AHardwareBuffer` (<64 MB peak RSS). |
| **4** | Linking FFmpeg libraries (`libavcodec.a`) without checking GPL flags | Violates zero-GPL clean-room license and creates multi-megabyte binary bloat. | Write clean-room C++20 parsers (`libprism_bbr`) or link strictly LGPL/BSD-licensed modules. |
| **5** | Hardcoding 4 KB memory page size assumptions in SIMD memory pools | Breaks compatibility on Android 15+, which mandates 16 KB memory page size. | Compile with `-Wl,-z,max-page-size=16384` and use `posix_memalign` aligned to 16 KB boundaries. |
| **6** | Generating custom file extensions without an OS virtual bridge | Third-party apps cannot open `.pvloss` or `.aeth` files, causing user rejection. | Implement `Prism-VFS` (`StorageManager.openProxyFileDescriptor`) to serve virtual MP4 bytes in RAM. |
| **7** | Assuming WhatsApp or Instagram will preserve Track 2 enhancement tracks | Social media ingest transcoders strip auxiliary tracks (`prmr`), losing enhancement. | Provide a Social Export Share-Sheet Interceptor that composites tracks into a single MP4 before upload. |

---

## 7. Concrete Task Prompts for Dispatching AI Agents

When human technical leads or coordinator agents dispatch tasks to subagents or swarm workers, use these battle-tested task prompts:

### Prompt Template 1: For Dispatching a SIMD / rANS Systems Agent
```markdown
You are the Prism-V Core SIMD Systems Agent.
Task: Implement the 8-way SIMD rANS decoder kernel in C++20 for Android NDK r27.
Read First:
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/03_lossless_architecture.md
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/05_systems_hardware_ml.md#L50-L120
Constraints:
1. Target Armv8.2-A NEON and Armv9.2-A SME2 (using KleidiAI primitives).
2. All operations must follow Prism-DetInt integer contracts (K <= 512, INT8/INT16/INT32).
3. Peak throughput must exceed 180 MB/s on a single Cortex-A720 core.
4. Memory buffers must be 16 KB page-size aligned.
Deliverables:
- C++20 header and implementation files with zero external dependencies.
- Catch2 unit test verifying 100% bit-exact restoration against 10,000 synthetic test vectors.
```

### Prompt Template 2: For Dispatching an Android MediaCodec / VFS Agent
```markdown
You are the Prism-V Android Platform Systems Agent.
Task: Implement the Prism-VFS openProxyFileDescriptor in-memory serving bridge.
Read First:
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/06_container_and_bitstream.md#L80-L160
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/10_red_team_and_audit.md#L15-L25
Constraints:
1. Use Android's StorageManager.openProxyFileDescriptor API.
2. Read requests must decompress 256 KB independent chunks in RAM via libprism_rans (<1.2 ms latency).
3. Maintain a 16-MB LRU read-ahead ring buffer.
4. Total memory overhead must not exceed 32 MB RSS.
Deliverables:
- Kotlin service (PrismVfsService) and native C++ JNI bridge.
- Instrumented benchmark measuring onRead seek latency in milliseconds across simulated ExoPlayer scrub events.
```

### Prompt Template 3: For Dispatching an ML Compression Agent
```markdown
You are the Prism-V ML Compression Scientist.
Task: Implement the Cool-chic 5.0 micro-residual synthesis MLP and INT8 quantization pipeline.
Read First:
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/04_lossy_architecture.md
- file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/05_systems_hardware_ml.md#L125-L200
Constraints:
1. Compute complexity must remain strictly <= 600 MACs/decoded pixel.
2. Quantize weights to INT8 and activations to INT16 using Quantization-Aware Training (QAT).
3. The synthesis output must implement Masked Soft-Gating to eliminate base blocking artifacts.
4. Output model must be exportable to LiteRT (TFLite) with QNN / NeuroPilot delegate support.
Deliverables:
- PyTorch training and QAT quantization script.
- Exported LiteRT INT8 model (<500 KB binary size).
- Evaluation script computing VMAF-NEG and CVVDP gains over standalone hardware base frames.
```

---

## 8. Summary Verification Checklist for Merging Agent Work

Before accepting or merging any pull request or artifact generated by an AI agent, verify that it passes all 8 audit criteria:

- [ ] **1. Bit-Exactness Verified:** Lossless modes pass 100.000% SHA-256 round-trip parity on test vectors.
- [ ] **2. Zero Floating-Point in Priors:** All probability calculations use `INT8`/`INT16`/`INT32` dyadic math.
- [ ] **3. Memory Budget Respected:** Peak working RSS is verified $\le 64\text{ MB}$ during 4K video operations.
- [ ] **4. 16 KB Page Aligned:** Native libraries compile with `-Wl,-z,max-page-size=16384`.
- [ ] **5. Android 15 FGS Compliant:** Long jobs decompose into atomic 60-frame GOP micro-tasks.
- [ ] **6. Zero In-Place Overwrites:** File replacement uses the 5-Stage Verified Replacement Protocol.
- [ ] **7. Zero GPL Infection:** All linked code is verified under Apache-2.0, BSD, or MIT licenses.
- [ ] **8. Clean-Room Traceability:** Every decision maps back to a validated concept in [11_traceability_appendix.md](file:///c:/Users/des/OneDrive/Desktop/video_compression/synthesis/final/11_traceability_appendix.md).