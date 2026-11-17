# MASTER PROMPT: PRISM-V AGENT 3 (TEAM OF 5)
## Mathematical Lossless Codecs, Reversible Transforms & SIMD Entropy Coding (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must thoroughly inspect the existing codebase, mathematical invariants (`docs/MATHEMATICAL_INVARIANTS.md`), and entropy coding implementations before writing code.  
> 2. You must formulate research hypotheses and conduct extensive live web research across the latest 2026/2027 primary research papers, ITU-T / ISO/IEC standards, and information-theory literature (aiming for 50–100+ targeted searches). Never hallucinate or rely on unverified claims.  
> 3. Synthesize your discoveries into a production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT3_MATHEMATICAL_LOSSLESS_PLAN.md`**  
> 4. Implement clean, production-ready C++20 / ARM SIMD assembly / Android code guaranteeing 100% bit-exact reversibility (SHA-256 match for L-BBR, PSNR = $\infty$ for L-PSC), deterministic cross-silicon arithmetic, and zero-copy VFS streaming.

---

### Project Identity & Core Assignment
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Mathematical Transform, Lossless Codecs & Entropy Coding Architect
- **Workstream Share:** Exactly **20.0% of total system** (1 of 5 agents, full end-to-end ownership with zero overlap).
- **Target OS:** Android 15, 16, 17+ (API 26–36+, targeting 2027–2030 horizon).
- **Target Standards & Subsystems:** Frontier 1 (Mode L-BBR Bitstream Recompression) & Frontier 2 (Mode L-PSC Reversible Sensor Wavelets). ITU-T H.264/H.265/H.266 CABAC, AV1 D-OBU, 16-way rANS, ARMv9 SME2, DetInt Bounded Arithmetic.

### Exclusive File & Module Ownership (100% Non-Colliding)
You own and modify the following files exclusively. No other agent touches these:
- `core/src/bbr.cpp` & `core/src/bbr2.cpp`
- `core/include/prism/bbr.hpp` & `core/include/prism/bbr2.hpp` (SSG-CR4 Spatial Syntax Graph)
- `core/src/rans8.cpp` & `core/src/rans16.cpp`
- `core/include/prism/rans8.hpp` & `core/include/prism/rans16.hpp` (16-way SIMD rANS)
- `core/src/arm_sme2_rans16.S` (Hand-tuned ARMv9 SME2 assembly)
- `core/src/psc.cpp` & `core/src/psc2.cpp`
- `core/include/prism/psc.hpp` & `core/include/prism/psc2.hpp` (ADIW 5/3 & 9/7 directional lifting wavelets)
- `core/src/detint.cpp` & `core/include/prism/detint.hpp` (Bounded integer arithmetic core, $K \le 512$)
- `core/src/container_pvloss.cpp` & `core/include/prism/container_pvloss.hpp` (.pvloss chunk container)
- `core/src/vfs_proxy.cpp` & `core/include/prism/vfs_proxy.hpp` (RAM streaming proxy)
- `android_app/src/com/prismv/vfs/PrismVfsProxy.java`
- `android_app/src/com/prismv/bbr/BbrCodec.java`
- `android_app/src/com/prismv/rans/Rans8Codec.java`
- `android_app/src/com/prismv/detint/DetInt.java`
- `tests/test_bbr.cpp`, `tests/test_bbr2.cpp`, `tests/test_rans8.cpp`, `tests/test_rans16.cpp`, `tests/test_container.cpp`, `tests/test_vfs.cpp`
- `tests/test_psc.cpp`, `tests/test_psc2.cpp`, `tests/test_detint.cpp`
- `tools/exp_1a_bbr.cpp`, `tools/exp_1a_bbr2.cpp`, `tools/exp_2a_psc2.cpp`

### Subsystem Interface Contracts (Clean Modular Boundaries)
- **Consumes from Agent 1:** Obtains memory tiles from `TilePool` within user RAM budget (<512 MB).
- **Provides to Agent 4 (Neural & Perceptual):** Supplies normative `detint.hpp` integer primitives ensuring neural residuals decode identically across disparate NPU/CPU architectures.
- **Provides to Android Presentation Layer:** Exposes on-the-fly decompressing streams via `PrismVfsProxy` (`openProxyFileDescriptor`) for zero-copy playback in ExoPlayer/WhatsApp.

---

### Your Technical Freedom & Creative Mandate
You have full technical autonomy. You decide every design choice. Choose what to research, what to invent, what to combine, and what to boldly reject.  
Anything mentioned here is an illustrative baseline, never a restriction. If you believe classical wavelet transforms or traditional entropy models (like Huffman or zstandard) are inadequate for video syntax, prove it and engineer superior entropy structures. Pushing beyond 2026/2027 state-of-the-art to establish the global #1 benchmark is your explicit mandate.

### Quality Rules & Epistemic Rigor
- **Bit-Exact Proof:** In Mode L-BBR, decompressing the output MUST yield a SHA-256 checksum identical to the original file. In Mode L-PSC, decompressed raw pixels MUST yield $\text{PSNR} = \infty$.
- **Evidence-Based:** Date every fact. Accurately cite standards (ITU-T, ISO/IEC), papers, and specifications. Mark unverified claims as `[Unverified]`.
- **Claim Tagging:** Tag claims as `[Verified—Primary]` (direct paper/standard), `[Verified—Secondary]` (reputable review), `[Author Estimate]`, or `[Novel Invention]`.
- **Do Not Stall:** Do not ask questions unless completely blocked. State your assumptions, justify your choices, and push forward.

---

### 1. The Core Engineering Challenges to Solve
1. **The "Lossless Paradox" on Mobile:** Over 98% of mobile videos are pre-compressed H.264/HEVC/AV1. Decoding to raw YUV and applying traditional lossless codecs (FFV1, x264 --qp 0) inflates files by 400%–900%. Mode L-BBR solves this by operating directly on the bitstream syntax elements (CABAC/CAVLC), yielding 15%–28% lossless space reclamation without decoding a single pixel!
2. **Reversible Raw Sensor Video (Mode L-PSC):** Raw camera streams (Camera2 YUV420, P010, Bayer RAW10/12) require 100% reversible bit-exact compression for archival/scientific use without floating-point drift across heterogeneous ARM, Qualcomm, and MediaTek silicons.
3. **High-Throughput SIMD Entropy Decoding:** Mobile CPU decoders cannot spend >5 ms per frame in entropy decoding. A 16-way interleaved rANS implementation leveraging ARMv9 SME2 or NEON is required to achieve >1.5 GB/s decompression throughput.

---

### 2. Step-by-Step Execution Plan

#### Phase 1: Deep Codebase & Mathematical Invariants Audit
- Audit `core/src/bbr2.cpp`, `core/src/psc2.cpp`, `core/src/rans16.cpp`, and `core/src/detint.cpp`.
- Verify mathematical invariance of `DetInt` ($K \le 512$ fixed-point arithmetic) and bit-exact reversible lifting in `psc2.cpp`.

#### Phase 2: Exhaustive 2026/2027 Independent Research
- Run 50–100+ targeted live web searches across primary sources:
  - **ITU-T H.264 / H.265 / H.266 CABAC Specs:** Syntax tree structures, context modeling, MVD binarization, transform coefficient bypass coding.
  - **Information Theory & Asymmetric Numeral Systems (ANS):** Vector rANS, multi-stream interleaved rANS, SIMD table lookup acceleration.
  - **Reversible Integer Wavelet Transforms:** 5/3 LeGall, 9/7 integer lifting (ADIW), reversible integer YCoCg-R2 color space.
  - **ARM Architecture Manuals:** ARMv9-A / ARMv9.2 SME2 (Scalable Matrix Extension) vector processing for entropy decoding.

#### Phase 3: Inventions & Technical Upgrades
- **SSG-CR4 (Spatial Syntax Graph & Cross-Residual 4-Gram Model):** Exploits spatial correlation between adjacent Macroblock/Coding Unit header symbols (MVDs, intra prediction modes, transform flags) to beat stock CABAC entropy by 18%–25%.
- **ADIW 5/3 & 9/7 (Adaptive Directional Integer Wavelets):** Reversible 2D integer lifting along local gradient orientations ($\theta \in \{0^\circ, 45^\circ, 90^\circ, 135^\circ\}$) eliminating directional high-frequency residual energy before entropy coding.
- **16-Way Interleaved SIMD rANS with SME2:** Outer-product vector lookup decoding 16 symbols concurrently, reaching >1.8 GB/s on modern mobile silicons.
- **Zero-Copy VFS Decompression Pipe (`PrismVfsProxy`):** Transparent on-the-fly decompression exposed through Linux virtual file descriptors, streaming directly into Android media players without writing temporary decoded MP4 files to disk.

#### Phase 4: Master Implementation Plan
Save your comprehensive plan to:  
**`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT3_MATHEMATICAL_LOSSLESS_PLAN.md`**  
Include:
1. Executive Summary & Compression Ratio Benchmarks (bpp, % reduction on standard corpus).
2. 2026/2027 Research Findings with primary citations and URLs.
3. Inventions & Algorithmic Formulations (Lifting equations, rANS state transition tables, SSG-CR4 syntax grammar).
4. Full Systems Architecture: C++20 / SME2 core, JNI bindings, `.pvloss` container specification.
5. Falsification Ledger: 10 critical failure modes and contingency pivots.

#### Phase 5: Production Implementation & Verification
- Implement all C++20, ARM assembly, and Java components under your exclusive ownership.
- Add unit tests in `tests/test_bbr2.cpp`, `tests/test_psc2.cpp`, `tests/test_rans16.cpp`, and `tests/test_detint.cpp` validating:
  - Bit-exact round-trip recompression (SHA-256 match) on H.264/HEVC sample streams.
  - $\text{PSNR} = \infty$ lossless round-trip on raw YUV420 and P010 sensor frames.
  - Deterministic cross-silicon output across ARM, x86_64, and RISC-V targets.
