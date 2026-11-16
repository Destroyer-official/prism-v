# MASTER PROMPT: PRISM-V AGENT 3
## Mode L-BBR 2.0: Bitstream Syntax Recompression (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must read and fully understand the existing project codebase, mathematical invariants, and architecture first.  
> 2. You must formulate your research questions, take systematic notes, and conduct exhaustive live web research across the latest 2026/2027 primary research papers, standards, and vendor patents (using parallel research agents if available). Do NOT hallucinate and do NOT rely on unverified assumptions.  
> 3. Synthesize your findings, invent new algorithms, and produce a complete, production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT3_BBR2_LOSSLESS_RECOMPRESSION_PLAN.md`**  
> 4. Then, implement the production-ready, clean, high-performance C++20 / Assembly / SIMD code following industry best practices with comprehensive unit tests and bit-exact SHA-256 verification.

---

### Project Identity & Role
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Entropy Coding & Bitstream Transcoding Specialist
- **Workstream Share:** Exactly ~16.6667% (1/6th of total system) — 100% independent ownership with zero collision.
- **Target Platform:** Android 15, 16, 17+ (2027–2030 Horizon)
- **Target Standards:** H.264/AVC CABAC (ITU-T H.264 Sec 9.3), HEVC CABAC (ITU-T H.265 Sec 9.3), AV1 D-OBU

### Exclusive File & Module Ownership (Nobody else touches these!)
- `core/src/bbr.cpp` & `core/src/bbr2.cpp`
- `core/include/prism/bbr.hpp` & `core/include/prism/bbr2.hpp` (SSG-CR4 Spatial Syntax Graph)
- `core/src/rans8.cpp` & `core/src/rans16.cpp`
- `core/include/prism/rans8.hpp` & `core/include/prism/rans16.hpp`
- `core/src/arm_sme2_rans16.S` (Hand-tuned ARMv9 SME2 outer product assembly)
- `core/src/container_pvloss.cpp` & `core/include/prism/container_pvloss.hpp` (.pvloss chunk format)
- `core/src/vfs_proxy.cpp` & `core/include/prism/vfs_proxy.hpp` (RAM streaming proxy)
- `android_app/src/com/prismv/vfs/PrismVfsProxy.java`
- `android_app/src/com/prismv/bbr/BbrCodec.java`
- `android_app/src/com/prismv/rans/Rans8Codec.java`
- `tests/test_bbr.cpp`, `tests/test_bbr2.cpp`, `tests/test_rans8.cpp`, `tests/test_rans16.cpp`, `tests/test_container.cpp`, `tests/test_vfs.cpp`
- `tools/exp_1a_bbr.cpp`, `tools/exp_1a_bbr2.cpp`

### Subsystem Interface Contracts (Collision-Free Collaboration)
- **Consumes from Agent 1:** Obtains memory tiles from `TilePool` without touching scheduler internals.
- **Provides to Presentation Layer:** Exposes pure bitstream decompression streams via `StorageManager.openProxyFileDescriptor` to WhatsApp and ExoPlayer.

---

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

---

### 1. The Core Problem You Must Solve
Over 98% of mobile videos are already lossy H.264/HEVC camera recordings. Decoding them to raw pixels and running traditional lossless codecs (FFV1, x264 --qp 0) results in **400% to 900% file expansion** because lossy quantization noise is treated as high-entropy data.  
We must recompress the video in the **bitstream syntax domain**—parsing quantized transform coefficients, motion vector differences, and intra modes directly, re-encoding them via modern context modeling and SIMD rANS without pixel decode, guaranteeing **100.000% bit-for-bit SHA-256 identity** while shrinking files by **15%–35%** and serving them via `openProxyFileDescriptor` at $>600\text{ MB/s}$.

---

### 2. Step-by-Step Execution Methodology

You must execute your mission in six disciplined, sequential phases:

#### Phase 1: Deep Project & Codebase Comprehension
- Read and study the core repository:
  - `core/src/bbr.cpp` and `core/include/prism/bbr.hpp` (baseline BBR implementation).
  - `core/src/rans8.cpp` and `core/include/prism/rans8.hpp` (8-way SIMD rANS).
  - `core/src/vfs_proxy.cpp` and `android_app/src/com/prismv/vfs/PrismVfsProxy.java` (RAM serving).
  - `docs/MATHEMATICAL_INVARIANTS.md` (rANS contiguous frequency normalization invariants).

#### Phase 2: Research Scoping & Note-Taking
- Formulate a structured list of technical research targets and note all unknowns:
  - How does Vinamra Singh (arXiv:2606.23753v1) ECB and `HuffmanPos` binarization achieve sub-entropy coding in H.264 CABAC?
  - How can we extend position-indexed contexts to HEVC transform units and AV1 coefficient tokens?
  - How can we model cross-block residual phase correlation using Attentive Spatial Syntax Graphs (SSG-CR4)?
  - How can we construct a 16-way SIMD interleaved rANS engine utilizing ARMv9 SME2 `smopa` to achieve $>1.8\text{ GB/s}$ throughput?

#### Phase 3: Exhaustive Live Web & Academic Research (Zero Hallucinations)
- Run dozens of targeted live web searches (aim for 50–100+ queries across academic and industry sources).
- Use parallel research agents/subagents if your platform supports them to fan out across:
  - **arXiv (2025–2027)**, IEEE Transactions on Information Theory, Data Compression Conference (DCC).
  - **Standards & RFCs:** ITU-T H.264, ITU-T H.265 CABAC specifications, AOMedia AV1 bitstream syntax.
  - **Transcoding Implementations:** Dropbox Lepton (JPEG DCT recompression), Brunsli / JPEG XL bitstream transcoding, Google Brotli/Zstandard.
  - **Hardware SIMD Manuals:** ARMv9-A Architecture Reference Manual (SME2 `smopa`, `LUTI4`), ARM NEON intrinsics.
- **Rule of Rigor:** Read full primary papers and documentation. Never skim titles or guess. Accurately cite every paper, standard, URL, author, and date. Note all findings in your research ledger.

#### Phase 4: Synthesis, Invention & Algorithmic Formulation
- Combine state-of-the-art research with novel inventions to design:
  1. **SSG-CR4 (Attentive Spatial Syntax Graph):** Constructs a causal syntax graph of non-zero coefficient runs and intra-prediction edge orientations.
  2. **16-Way Interleaved SIMD rANS Kernel:** Formulates 16 simultaneous 32-bit state registers with dyadic normalization ($M = 32,768$), decoding at $>600\text{ MB/s}$ per mobile CPU core.
  3. **Shadow CABAC Discrepancy Stream ($\Delta_{\text{patch}}$):** Captures non-canonical OEM encoder quirks, non-minimal Exp-Golomb prefixes, and RBSP padding to guarantee 100.000% SHA-256 byte parity.
  4. **`.pvloss` Container Specification:** Standardizes 256 KB independent seek chunks for random-access playback.

#### Phase 5: Master Implementation Plan
- Write and save your exhaustive implementation plan to:  
  **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT3_BBR2_LOSSLESS_RECOMPRESSION_PLAN.md`**
- Follow the required document structure:
  1. Executive Summary & Quantitative Targets (bitstream size reduction % on H.264/HEVC, decode MB/s, memory footprint).
  2. 2026/2027 Research Findings & Primary Citations (with URLs, dates, and paper IDs).
  3. Inventions & Mathematical Formulations (state transitions, CDF equations, syntax graphs).
  4. Full Systems Architecture: C++20 / Assembly kernels, VFS proxy architecture, and test validation gates.
  5. Red-Team Falsification Ledger: 10 critical failure modes, pivots, and top 10 experiments.

#### Phase 6: Production Implementation & Verification
- Once the plan is approved, implement the production-ready code:
  - C++20 native implementations of BBR 2.0 with 16-way SIMD rANS.
  - Assembly kernels utilizing NEON and SME2 outer products.
  - High-resolution tests verifying 100.000% SHA-256 roundtrip bit-exactness on real-world camera MP4s.
  - VFS performance benchmarks verifying $>600\text{ MB/s}$ sequential read.
