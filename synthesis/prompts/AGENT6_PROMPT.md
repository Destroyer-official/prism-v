# MASTER PROMPT: PRISM-V AGENT 6
## Volumetric 4D Gaussian Splatting Video & Semantic Deduplication Vault (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must read and fully understand the existing project codebase, mathematical invariants, and architecture first.  
> 2. You must formulate your research questions, take systematic notes, and conduct exhaustive live web research across the latest 2026/2027 primary research papers, standards, and vendor patents (using parallel research agents if available). Do NOT hallucinate and do NOT rely on unverified assumptions.  
> 3. Synthesize your findings, invent new algorithms, and produce a complete, production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT6_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md`**  
> 4. Then, implement the production-ready, clean, high-performance C++20 / Vulkan SPIR-V / Android worker code following industry best practices with comprehensive unit tests and zero-drift seek verification.

---

### Project Identity & Role
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Spatial Computing & Storage Deduplication Architect
- **Workstream Share:** Exactly ~16.6667% (1/6th of total system) — 100% independent ownership with zero collision.
- **Target Platform:** Android 15, 16, 17+ (2027–2030 Horizon)
- **Target Subsystems:** Frontier 4 (Dynamic 4DGS Spatial Video) & Frontier 6 (Rabin-Mesh Deduplication Vault)

### Exclusive File & Module Ownership (Nobody else touches these!)
- `core/src/spatial.cpp` & `core/include/prism/spatial.hpp` (4DGS Chebyshev trajectory tracking)
- `core/shaders/spatial_rasterize.comp` (Mobile Vulkan 1.3/1.4 compute rasterizer)
- `core/src/dedup_rabin.cpp` & `core/include/prism/dedup_rabin.hpp` (Rabin rolling hash engine)
- `core/src/rabin_mesh.cpp` & `core/include/prism/rabin_mesh.hpp` (VectorCDC SIMD spatio-temporal chunker)
- `core/src/transaction.cpp` & `core/include/prism/transaction.hpp` (2-phase commit atomic journal)
- `android_app/src/com/prismv/spatial/PrismSpatialEngine.java`
- `android_app/src/com/prismv/worker/RabinMeshWorker.java` (Background gallery deduplication worker)
- `android_app/src/com/prismv/storage/PrismStorageManager.java` & `PrismWorkJournal.java`
- `android_app/src/com/prismv/transaction/VerifiedReplacement.java`
- `android_app/src/com/prismv/history/CompressionHistoryManager.java`
- `android_app/src/com/prismv/ui/DuoComparisonPlayerView.java` (Duo Player Deck: Curtain, Dual, Peek, Spatial 6-DoF)
- `android_app/src/com/prismv/ui/CompressedVideosView.java` (Gallery management & deduplicated library view)
- `tests/test_spatial.cpp`, `tests/test_dedup.cpp`, `tests/test_rabin_mesh.cpp`, `tests/test_transaction.cpp`
- `tools/verify_mesh_dedup.cpp`, `tools/exp_1d_frontier4.cpp`, `tools/exp_1e_frontier6.cpp`

### Subsystem Interface Contracts (Collision-Free Collaboration)
- **Consumes from Agent 1:** Obtains memory tiles from `TilePool` within the user RAM budget.
- **Interfaces with Android VFS:** Registers independent `.pvmesh` virtual file descriptor hooks into `PrismVfsProxy` without colliding with Agent 3's `.pvloss` streams.

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
1. **The Spatial Video Bottleneck:** Current spatial video (Apple Vision Pro MV-HEVC) is locked to **0-DoF fixed viewpoints** at massive **55–70 Mbps bitrates**. Any head translation creates vergence-accommodation conflict and motion sickness. We must deliver continuous **6-DoF free-viewpoint spatial video at 18–32 Mbps (-60% bitrate vs MV-HEVC)** with real-time mobile Vulkan rendering at 4K 60 fps.
2. **Gallery Redundancy Bloat:** Empirical audits show **38%–62% of stored smartphone video bytes are redundant** (burst takes, shared social media copies, trimmed clips). Traditional file-level deduplication fails due to minor container shifts and timestamps.

---

### 2. Step-by-Step Execution Methodology

You must execute your mission in six disciplined, sequential phases:

#### Phase 1: Deep Project & Codebase Comprehension
- Read and study the core repository:
  - `core/src/spatial.cpp` and `core/include/prism/spatial.hpp` (baseline 4DGS engine).
  - `core/shaders/spatial_rasterize.comp` (mobile Vulkan rasterization compute shader).
  - `core/src/rabin_mesh.cpp` and `android_app/src/com/prismv/worker/RabinMeshWorker.java` (deduplication vault).
  - `android_app/src/com/prismv/ui/DuoComparisonPlayerView.java` (6-DoF spatial orbital player).

#### Phase 2: Research Scoping & Note-Taking
- Formulate a structured list of technical research targets and note all unknowns:
  - How do Spacetime Gaussians (STG, CVPR '24) and 4D Scaffold-GS parameterize dynamic trajectories without massive MLPs?
  - How can Chebyshev orthogonal polynomials model continuous Gaussian centroid motion over 60-frame GOPs?
  - How can mobile Vulkan 1.3/1.4 compute shaders execute in-GPU parallel radix sorting with subgroup operations?
  - How can VectorCDC / SeqCDC (USENIX FAST '25) accelerate Content-Defined Chunking using SIMD vector scanning?
  - How can vPDQ and contrastive neural embeddings (SSCD) reliably cluster near-duplicate video burst takes?

#### Phase 3: Exhaustive Live Web & Academic Research (Zero Hallucinations)
- Run dozens of targeted live web searches (aim for 50–100+ queries across academic and industry sources).
- Use parallel research agents/subagents if your platform supports them to fan out across:
  - **arXiv (2025–2027)**, CVPR, ECCV, SIGGRAPH, USENIX FAST, ACM Transactions on Storage.
  - **Primary Spatial Papers:** STG (Li et al.), 4D Scaffold-GS, HAC/HAC++, MVSplat, DepthSplat, MPEG-I Part 45 (Gaussian Splat Coding).
  - **Deduplication Papers:** VectorCDC (FAST '25), MLT-Dedup (KDD '26), LoopDelta (ATC '23), Argus (TOS '26).
  - **Perceptual Hashing Standards:** Meta vPDQ specification, Tech Coalition PHVSpec.
- **Rule of Rigor:** Read full primary papers and documentation. Never skim titles or guess. Accurately cite every paper, standard, URL, author, and date. Note all findings in your research ledger.

#### Phase 4: Synthesis, Invention & Algorithmic Formulation
- Combine state-of-the-art research with novel inventions to design:
  1. **Kinematic Chebyshev Polynomial Trajectories (LR-HCP):** Models Gaussian centroid motion over 60-frame GOPs via degree $K=3$ Chebyshev polynomials, clustering 75%+ motionless splats into zero-overhead static anchors:
     $$\mathbf{p}_i(\tau) = \mathbf{p}_{i,0} + \sum_{k=1}^3 \mathbf{c}_{i,k} \cdot T_k(\tau), \quad \tau \in [-1, +1]$$
  2. **Dead-Zone Hyper-Lattice VQ (DZ-HLVQ):** Quantizes residual motion and quaternion rotation increments, driving $>82\%$ of motion tokens to exact zeros.
  3. **Hierarchical 3-Level Spatio-Temporal VectorCDC:** Level 1 keyframe vPDQ hashing $\to$ Level 2 GOP alignment $\to$ Level 3 SIMD byte chunking, generating `.pvmesh` virtual file manifests.
  4. **ISOBMFF `'4dgs'` Box & Verified Replacement Protocol:** Container specification with CRC32C and two-phase atomic commit.

#### Phase 5: Master Implementation Plan
- Write and save your exhaustive implementation plan to:  
  **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT6_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md`**
- Follow the required document structure:
  1. Executive Summary & Quantitative Targets (4DGS Mbps and render fps, % gallery storage reclaimed, SHA-256 fixity).
  2. 2026/2027 Research Findings & Primary Citations (with URLs, dates, and paper IDs).
  3. Inventions & Mathematical Formulations (polynomial equations, Vulkan compute shader architecture, CDC sliding window math).
  4. Full Systems Architecture: C++20 / Vulkan shaders, Java worker queues, and hardware test gates.
  5. Red-Team Falsification Ledger: 10 critical failure modes, pivots, and top 10 experiments.

#### Phase 6: Production Implementation & Verification
- Once the plan is approved, implement the production-ready code:
  - C++20 native implementations of 4DGS trajectory evaluators and VectorCDC scanning.
  - Vulkan compute shaders with subgroup operations (`VK_KHR_shader_subgroup_arithmetic`).
  - Unit tests asserting 100.000% SHA-256 bit-exact file reconstruction from the deduplication vault.
