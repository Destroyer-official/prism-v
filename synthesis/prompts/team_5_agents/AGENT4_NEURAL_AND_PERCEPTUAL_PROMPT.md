# MASTER PROMPT: PRISM-V AGENT 4 (TEAM OF 5)
## Neural Video Compression, Film Grain Synthesis & Generative Residuals (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must thoroughly inspect the existing neural synthesis implementations, AFGS1 film grain modules, and integer quantization models before writing code.  
> 2. You must formulate research hypotheses and conduct extensive live web research across the latest 2026/2027 primary research papers, CVPR/ICCV/NeurIPS publications, and NPU acceleration manuals (aiming for 50–100+ targeted searches). Never hallucinate or rely on unverified claims.  
> 3. Synthesize your discoveries into a production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT4_HYBRID_NEURAL_LOSSY_PLAN.md`**  
> 4. Implement clean, production-ready C++20 / GLSL compute shaders / Android code guaranteeing cross-silicon deterministic integer arithmetic, real-time mobile NPU/GPU execution (<0.35 GMACs/frame), and 60%–80% storage reclamation with $\text{VMAF} \ge 94$.

---

### Project Identity & Core Assignment
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Neural Video Compression & Perceptual Rate-Distortion Architect
- **Workstream Share:** Exactly **20.0% of total system** (1 of 5 agents, full end-to-end ownership with zero overlap).
- **Target OS:** Android 15, 16, 17+ (API 26–36+, targeting 2027–2030 horizon).
- **Target Silicon:** Android `MediaCodec` ASICs, ARM NEON/SME2, Vulkan 1.4 Compute, Android LiteRT / NPU Delegates (Qualcomm Hexagon, MediaTek NPU, Google TPU).

### Exclusive File & Module Ownership (100% Non-Colliding)
You own and modify the following files exclusively. No other agent touches these:
- `core/src/coolchic.cpp` & `core/include/prism/coolchic.hpp` (Cool-chic 5.0 neural synthesis engine)
- `core/src/afgs1.cpp` & `core/include/prism/afgs1.hpp` (AOMedia AFGS1 noise separation & synthesis)
- `core/src/rfsq.cpp` & `core/include/prism/rfsq.hpp` (Bounded Integer-Scaled Residual FSQ)
- `core/src/prism_gen.cpp` & `core/include/prism/prism_gen.hpp` (32-landmark semantic codec @ 1.5 kbps)
- `core/src/nerv_engine.cpp` & `core/include/prism/nerv_engine.hpp` (NeRV-S continuous coordinate fields)
- `core/shaders/detint_flow_gen.comp` (1-step distilled flow matching generator shader)
- `android_app/src/com/prismv/worker/PrismNeRVBackgroundWorker.java` (Charging-time overfit background worker)
- `android_app/src/com/prismv/ai/GeminiCopilot.java`, `GeminiClient.java`, `GeminiCopilotDialog.java`
- `tests/test_coolchic.cpp`, `tests/test_afgs1.cpp`, `tests/test_rfsq.cpp`, `tests/test_prism_gen.cpp`, `tests/test_nerv.cpp`
- `tools/qat_coolchic_training.py`
- `tools/exp_1b_lossy.cpp`, `tools/exp_1c_frontier3.cpp`

### Subsystem Interface Contracts (Clean Modular Boundaries)
- **Consumes from Agent 2:** Accepts calibrated base-layer bitrate and residual bit budget allocations.
- **Consumes from Agent 3:** Utilizes normative DetInt integer layer primitives (`detint.hpp`) for 100% cross-silicon bitstream reproducibility without floating-point mismatch.

---

### Your Technical Freedom & Creative Mandate
You have full technical autonomy. You decide every design choice. Choose what to research, what to invent, what to combine, and what to boldly reject.  
Anything mentioned here is an illustrative baseline, never a restriction. If you believe standard monolithic end-to-end neural autoencoders (like Scale-space flow or classic learned image codecs) are too bloated or slow for mobile devices, reject them and invent ultra-lean overfitted or hybrid neural-residual paradigms. Pushing beyond 2026/2027 state-of-the-art to establish the global #1 benchmark is your explicit mandate.

### Quality Rules & Epistemic Rigor
- **Deterministic Synthesis:** Neural networks on video decoders must never introduce non-deterministic floating-point divergence across different chipsets. Quantization must be mathematically closed (INT8/INT16 with deterministic rounding).
- **Evidence-Based:** Date every fact. Accurately cite CVPR, ICCV, NeurIPS, and IEEE papers. Mark unverified claims as `[Unverified]`.
- **Claim Tagging:** Tag claims as `[Verified—Primary]` (direct paper/standard), `[Verified—Secondary]` (reputable review), `[Author Estimate]`, or `[Novel Invention]`.
- **Do Not Stall:** Do not ask questions unless completely blocked. State your assumptions, justify your choices, and push forward.

---

### 1. The Core Engineering Challenges to Solve
1. **The Sensor Shot-Noise Tax (35%–45% Bitrate Waste):** CMOS smartphone sensors introduce quantum Poisson-Gaussian photon shot noise ($\eta \sim \mathcal{N}(0, \sigma^2(I))$). Traditional DCT/wavelet encoders waste over a third of their bits quantizing this imperceptible random noise. AFGS1 parametric modeling separates this noise, transmitting lean mathematical parameters (<2 kbps) while compressing clean video at drastically reduced bitrates, synthesizing identical perceptual texture on playback.
2. **Mobile Computational Budget Bottleneck:** Heavy neural networks cannot run in real time on mobile batteries. The synthesis decoder must consume $<0.35\text{ GMACs/frame}$ to sustain 60 fps at $<1.5\text{ W}$ power draw.
3. **Cross-Vendor NPU Floating-Point Non-Determinism:** Different mobile NPUs (Qualcomm Hexagon, MediaTek APU, Samsung NPU) execute FP16/FP32 operations with varying rounding modes and precision shortcuts. Without bounded integerization (RFSQ & DetInt), decoders on different phones output corrupted frames.

---

### 2. Step-by-Step Execution Plan

#### Phase 1: Deep Codebase & Mathematical Invariants Audit
- Audit `core/src/afgs1.cpp`, `core/src/coolchic.cpp`, `core/src/rfsq.cpp`, and `core/shaders/detint_flow_gen.comp`.
- Verify strict INT8/INT16 quantized weight execution with zero runtime floating-point drift.

#### Phase 2: Exhaustive 2026/2027 Independent Research
- Run 50–100+ targeted live web searches across primary sources:
  - **AOMedia Film Grain Synthesis (AFGS1) Specification:** Autoregressive model coefficients, frequency-domain noise filtering, and hardware grain rendering.
  - **CVPR / ICCV / NeurIPS (2025–2027):** Overfitted coordinate networks (Cool-chic, NeRV, HNeRV), implicit neural representations for video compression.
  - **Vector & Scalar Quantization:** Residual Finite Scalar Quantization (RFSQ), learned entropy coding for latent residual feature maps.
  - **Android LiteRT & Vulkan Machine Learning:** LiteRT NPU acceleration, Vulkan compute shader neural inference, GLSL integer dot products (`sdot2`, `sdot4`).

#### Phase 3: Inventions & Technical Upgrades
- **AOMedia AFGS1 Parametric Decoupling Pipeline:** Fast multi-scale Gaussian blur and temporal median filtering in C++20 to extract grain parameters, reducing base video bitrate by up to 40% with zero perceived sharpness loss.
- **Cool-chic 5.0 Low-Complexity Synthesis Decoder:** Ultra-compact MLP architecture (<50k parameters) synthesized entirely in Vulkan compute shaders, achieving 60 fps decoding under 1.2 W power consumption.
- **Deterministic Residual FSQ (RFSQ):** Eliminates floating-point non-determinism by quantizing neural residual latent vectors into integer grid indices, perfectly compressible with Agent 3's 16-way rANS.
- **Multimodal Gemini Copilot Dialog:** Seamless UI integration providing intelligent scene categorization, bitrate-quality trade-off recommendations, and compression artifact diagnosis.

#### Phase 4: Master Implementation Plan
Save your comprehensive plan to:  
**`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT4_HYBRID_NEURAL_LOSSY_PLAN.md`**  
Include:
1. Executive Summary & Perceptual Quality Benchmarks (VMAF $\ge 94$, CVVDP $\ge 9.3$, bpp reduction).
2. 2026/2027 Research Findings with primary citations and URLs.
3. Inventions & Algorithmic Formulations (AFGS1 parameter matrices, Cool-chic architecture, RFSQ math).
4. Full Systems Architecture: C++20 / GLSL shader core, NPU delegate pipeline, JNI interface.
5. Falsification Ledger: 10 critical failure modes and contingency pivots.

#### Phase 5: Production Implementation & Verification
- Implement all C++20, GLSL compute shaders, and Java components under your exclusive ownership.
- Add unit tests in `tests/test_afgs1.cpp`, `tests/test_coolchic.cpp`, `tests/test_rfsq.cpp`, and `tests/test_nerv.cpp` validating:
  - Exact grain reconstruction fidelity without temporal flickering.
  - Zero floating-point divergence across multi-platform test suites.
  - Peak inference latency $<16.6\text{ ms}$ for 1080p60 on mobile GPU/NPU.
