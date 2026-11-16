# Prism-V: 6-Agent Research & Implementation Master Prompts (2027+)
## Comprehensive Division of Work for Autonomous AI Agents

**Date:** November 16, 2026  
**System Codename:** Prism-V (Android Video Compression & Spatial Media Engine)  
**Target Horizon:** 2027–2030 (Android 15 through Android 18+)  
**Purpose:** Provide 6 self-contained, uncompromising research and implementation master prompts for 6 specialized AI agents to advance the Prism-V platform to the absolute global state of the art (#1 in the world).

---

## Architectural Breakdown & Division of Work

To solve the twin challenges of **extreme performance scaling** (processing 1-hour 4K 120 fps video without thermal collapse by dynamically using user-budgeted RAM, CPU, GPU, and NPU) and **ironclad rate control reliability** (eliminating the critical bug where a 234 MB video estimated at 74.5 MB expands to 602.1 MB), the project is divided into 6 specialized, non-overlapping workstreams:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                             PRISM-V 6-AGENT WORKSTREAM MATRIX                               │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 1: HIGH-THROUGHPUT COMPUTE & MASSIVE VIDEO ENGINE (4K 120 FPS / 1-HR OPTIMIZATION)   │
│    • Domain: Scalable parallelization, user-allocated RAM/CPU/GPU budgeting, GOP chunking   │
│    • Focus: 1 hr 4K 120 fps video throughput, zero-copy DMA surfaces, sub-3.5W thermal caps │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 2: PRECISION RATE CONTROL, BIT-BUDGET ALLOCATION & "NEVER-WORSE" CONVERGENCE         │
│    • Domain: Mathematical rate control, empirical 2-pass preflight, MKV/MP4 duration fix    │
│    • Focus: Elimination of file blowup (234 MB -> 602 MB), strict size estimation (±3%)     │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 3: MODE L-BBR 2.0: BITSTREAM-DOMAIN RECOMPRESSION (LOSSLESS PRE-COMPRESSED)          │
│    • Domain: CABAC syntax parsing without pixel decode, SSG-CR4, 16-way SIMD rANS           │
│    • Focus: 15%–35% lossless shrink on H.264/HEVC/AV1, >600 MB/s VFS RAM streaming          │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 4: MODE L-PSC 2.0: DIRECTIONAL WAVELET SENSOR CODEC (LOSSLESS RAW/CAMERA)            │
│    • Domain: Reversible integer wavelets (ADIW 5/3 & 9/7), Quad-Bayer AiKLT, YCoCg-R2       │
│    • Focus: <22% of raw YUV size, 120 fps throughput, mathematical PSNR = ∞                 │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 5: HARDWARE-ANCHORED HYBRID NEURAL SPLIT-STREAM (LOSSY PERCEPTUAL QUALITY)           │
│    • Domain: MediaCodec ASIC base + Cool-chic 5.0 micro-residual + AFGS1 noise synthesis    │
│    • Focus: -45% to -70% BD-rate vs HEVC, Prism-DetInt cross-silicon integer determinism    │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│  AGENT 6: 4D GAUSSIAN SPATIAL VIDEO & SEMANTIC DEDUPLICATION VAULT (FRONTIERS 4 & 6)       │
│    • Domain: 6-DoF 4DGS Chebyshev trajectories, mobile Vulkan rasterizer, Rabin-Mesh vault  │
│    • Focus: 22 Mbps 4K60 spatial video, 40%–60% gallery deduplication without re-encoding  │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Navigation: Individual Prompt & Destination Plan Files

Each agent's prompt has been generated into an independent, ready-to-copy file in `synthesis/prompts/`:

| Agent | Specialized Domain | Individual Prompt File (Ready to Copy) | Target Destination Plan File |
| :---: | :--- | :--- | :--- |
| **Agent 1** | High-Throughput & Massive Video Engine | [AGENT1_PROMPT.md](prompts/AGENT1_PROMPT.md) | [AGENT1_HIGH_THROUGHPUT_COMPUTE_PLAN.md](final/AGENT1_HIGH_THROUGHPUT_COMPUTE_PLAN.md) |
| **Agent 2** | Precision Rate Control & Size Convergence | [AGENT2_PROMPT.md](prompts/AGENT2_PROMPT.md) | [AGENT2_PRECISION_RATE_CONTROL_PLAN.md](final/AGENT2_PRECISION_RATE_CONTROL_PLAN.md) |
| **Agent 3** | Mode L-BBR 2.0 Bitstream Recompression | [AGENT3_PROMPT.md](prompts/AGENT3_PROMPT.md) | [AGENT3_BBR2_LOSSLESS_RECOMPRESSION_PLAN.md](final/AGENT3_BBR2_LOSSLESS_RECOMPRESSION_PLAN.md) |
| **Agent 4** | Mode L-PSC 2.0 Wavelet Sensor Codec | [AGENT4_PROMPT.md](prompts/AGENT4_PROMPT.md) | [AGENT4_PSC2_DIRECTIONAL_WAVELET_PLAN.md](final/AGENT4_PSC2_DIRECTIONAL_WAVELET_PLAN.md) |
| **Agent 5** | Hybrid Neural Split-Stream Lossy Engine | [AGENT5_PROMPT.md](prompts/AGENT5_PROMPT.md) | [AGENT5_HYBRID_NEURAL_LOSSY_PLAN.md](final/AGENT5_HYBRID_NEURAL_LOSSY_PLAN.md) |
| **Agent 6** | 4DGS Spatial Video & Deduplication Vault | [AGENT6_PROMPT.md](prompts/AGENT6_PROMPT.md) | [AGENT6_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md](final/AGENT6_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md) |

---

## Mandatory 6-Phase Execution Protocol for Every Agent

Every agent is strictly required to follow this six-phase methodology:
1. **Phase 1 (Codebase Comprehension):** Read and study the existing repository, architectural diagrams, and mathematical invariants first.
2. **Phase 2 (Research Scoping & Note-Taking):** Identify technical unknowns and formulate explicit research questions before searching.
3. **Phase 3 (Exhaustive Live Web Research):** Perform 50–100+ targeted web searches across 2026/2027 primary literature, standards, and patents (using parallel research subagents if available). Read full papers—zero hallucinations and zero unverified assumptions.
4. **Phase 4 (Synthesis, Invention & Mathematical Derivation):** Synthesize findings into novel algorithms, prove mathematical bounds, and design exact state machines.
5. **Phase 5 (Master Plan Creation):** Save the comprehensive master plan to `synthesis/final/AGENT<N>_<NAME>_PLAN.md`.
6. **Phase 6 (Production Implementation & Verification):** Write production-ready, clean, high-performance C++20 / SIMD / Android code adhering to 16 KiB page alignment, TDD unit testing, and memory safety.

---

## The Inventive Mandate & Quality Rules (From Original Master Prompt)

### Your Freedom: Think, Challenge, and Invent
You have total technical freedom. You decide every design choice. Choose what to research, what to invent, which existing techniques to use, combine, or boldly reject.  
Anything mentioned in this prompt is an illustrative example or benchmark target, NEVER a rigid constraint that limits your creativity. If you believe any part of the framing or conventional wisdom in video compression is flawed, say so clearly and invent something demonstrably superior.  
Do not merely copy what exists in 2024–2026. Your explicit goal is to push beyond the state of the art and invent novel algorithms, mathematical formulations, hardware pipelines, or data structures that establish the global #1 benchmark for 2027+.

### Quality Rules & Epistemic Rigor
- **Be Honest and Evidence-Based:** Date every fact. Never hallucinate papers, authors, numbers, URLs, or specifications. Write "unverified" when you cannot empirically confirm a metric.
- **Tag Every Claim:** Tag claims as `[Verified—Primary]` (direct paper/standard), `[Verified—Secondary]` (reputable review), `[Author Estimate]`, or `[Novel Invention]`.
- **Label Every Technology:** Identify whether each technique is *shipping in production*, *standardized*, *open-source with code*, *research prototype*, or *pure theoretical proposal*.
- **Be Quantitative:** Give estimates with realistic uncertainty intervals (ranges). State plainly when something is hype, computationally unviable on mobile, or an engineering trap.
- **Do Not Stall:** Do not ask questions unless completely blocked. State your assumptions, justify your choices, and push forward aggressively.
