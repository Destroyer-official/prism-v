# MASTER PROMPT: PRISM-V AGENT 5 (TEAM OF 5)
## Volumetric 4DGS Spatial Video, Global Gallery Deduplication & Storage Vault (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must thoroughly inspect the existing 4D Gaussian splatting shaders, Rabin deduplication chunkers, and atomic storage transaction routines before writing code.  
> 2. You must formulate research hypotheses and conduct extensive live web research across the latest 2026/2027 primary research papers, SIGGRAPH / CVPR / Eurographics publications, and USENIX FAST storage literature (aiming for 50–100+ targeted searches). Never hallucinate or rely on unverified claims.  
> 3. Synthesize your discoveries into a production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT5_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md`**  
> 4. Implement clean, production-ready C++20 / Vulkan 1.3+ SPIR-V compute shaders / Android code guaranteeing zero data loss, sub-millisecond seek times, and real-time 60 fps 6-DoF spatial rendering on mobile TBDR GPUs.

---

### Project Identity & Core Assignment
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Spatial Computing & Storage Deduplication Architect
- **Workstream Share:** Exactly **20.0% of total system** (1 of 5 agents, full end-to-end ownership with zero overlap).
- **Target OS:** Android 15, 16, 17+ (API 26–36+, targeting 2027–2030 horizon).
- **Target Subsystems:** Frontier 4 (Dynamic 4DGS Volumetric Video) & Frontier 6 (Rabin-Mesh Global Deduplication & Storage Vault). Mobile TBDR Vulkan Compute, VectorCDC SIMD, ACID Atomic Journaling.

### Exclusive File & Module Ownership (100% Non-Colliding)
You own and modify the following files exclusively. No other agent touches these:
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

### Subsystem Interface Contracts (Clean Modular Boundaries)
- **Consumes from Agent 1:** Obtains memory tiles from `TilePool` within user RAM budget (<512 MB).
- **Interfaces with Android VFS:** Registers independent `.pvmesh` virtual file descriptor hooks into `PrismVfsProxy` without colliding with Agent 3's `.pvloss` streams.

---

### Your Technical Freedom & Creative Mandate
You have full technical autonomy. You decide every design choice. Choose what to research, what to invent, what to combine, and what to boldly reject.  
Anything mentioned here is an illustrative baseline, never a restriction. If you believe standard 3D/4D Gaussian splatting rasterizers (like CUDA-based sorting) cannot run efficiently on tile-based mobile GPUs (Adreno / Immortalis), reject them and invent a mobile-native tile-binned compute rasterizer. Pushing beyond 2026/2027 state-of-the-art to establish the global #1 benchmark is your explicit mandate.

### Quality Rules & Epistemic Rigor
- **Zero Data Loss Invariant:** Original video files must NEVER be modified or unlinked until the compressed file passes full structural verification and a 2-phase commit journal entry is persisted to flash storage.
- **Evidence-Based:** Date every fact. Accurately cite SIGGRAPH, CVPR, and USENIX papers. Mark unverified claims as `[Unverified]`.
- **Claim Tagging:** Tag claims as `[Verified—Primary]` (direct paper/standard), `[Verified—Secondary]` (reputable review), `[Author Estimate]`, or `[Novel Invention]`.
- **Do Not Stall:** Do not ask questions unless completely blocked. State your assumptions, justify your choices, and push forward.

---

### 1. The Core Engineering Challenges to Solve
1. **Rendering Dynamic 4DGS on Mobile TBDR Silicons:** 4D Gaussian Splatting provides revolutionary 6-DoF free-viewpoint video, but mobile GPUs lack the brute-force memory bandwidth of desktop cards. Storing and rasterizing 500,000+ moving 3D Gaussians per frame without thermal throttling requires Chebyshev polynomial trajectory compression and a tile-binned Vulkan compute shader.
2. **Whole-Gallery Storage Redundancy:** Users store dozens of takes of the same scene, burst captures, and identical meme clips. Standard video codecs treat each file in isolation. The Rabin-Mesh engine scans the video gallery in the background using SIMD-accelerated VectorCDC (Content-Defined Chunking) and eliminates multi-gigabyte cross-video redundancy.
3. **Storage Crash-Consistency on Mobile:** Power loss or app termination during background compression can corrupt user files. An ACID 2-phase commit journal with cryptographic verification is required to guarantee bulletproof reliability.

---

### 2. Step-by-Step Execution Plan

#### Phase 1: Deep Codebase & Storage Transaction Audit
- Audit `core/src/spatial.cpp`, `core/src/rabin_mesh.cpp`, `core/src/transaction.cpp`, and `core/shaders/spatial_rasterize.comp`.
- Verify atomic rename guarantees and rollback mechanics in `VerifiedReplacement.java`.

#### Phase 2: Exhaustive 2026/2027 Independent Research
- Run 50–100+ targeted live web searches across primary sources:
  - **SIGGRAPH / CVPR / Eurographics (2025–2027):** Dynamic 4D Gaussian Splatting, temporal trajectory compression (Chebyshev, Fourier bases), tile-based sorting and alpha blending on mobile GPUs.
  - **USENIX FAST / ACM Transactions on Storage:** Content-Defined Chunking (FastCDC, VectorCDC), rolling hashes (Rabin, Gear, Buzhash), multi-tier deduplication.
  - **Mobile GPU Architecture Guides:** Qualcomm Adreno 7xx/8xx tiling memory (GMEM), ARM Immortalis tile buffers, subpass load/store optimizations in Vulkan 1.3/1.4.
  - **Storage Consistency Standards:** SQLite WAL mechanics, atomic rename (`renameat2` with `RENAME_NOREPLACE` on ext4/f2fs).

#### Phase 3: Inventions & Technical Upgrades
- **Chebyshev 4DGS Temporal Trajectory Encoding:** Compresses the 3D position and covariance trajectories of Gaussians across time using 4th-order Chebyshev polynomials, slashing storage from 25 MB/sec to <3.5 MB/sec.
- **Tile-Binned Vulkan Compute Rasterizer (`spatial_rasterize.comp`):** Sorts and rasterizes Gaussians directly within 16x16 pixel on-chip GPU tile memory, avoiding off-chip DRAM roundtrips and sustaining 60 fps on mobile SoCs.
- **VectorCDC SIMD Spatio-Temporal Chunker:** 16-byte vector-parallel Rabin hash chunker scanning video files at >2.2 GB/s, identifying identical I-frames and macro-blocks across distinct videos.
- **Duo Comparison Player Deck (`DuoComparisonPlayerView.java`):** Interactive UI offering 4 visual evaluation modes: Split-Curtain slider, Dual Side-by-Side, Touch-Hold Peek, and 6-DoF Gyro Viewport for spatial video.

#### Phase 4: Master Implementation Plan
Save your comprehensive plan to:  
**`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT5_SPATIAL_4DGS_DEDUP_VAULT_PLAN.md`**  
Include:
1. Executive Summary & Storage Reclamation Metrics (dedup ratio, 4DGS fps/power).
2. 2026/2027 Research Findings with primary citations and URLs.
3. Inventions & Algorithmic Formulations (Chebyshev polynomial math, Rabin polynomial constants, Vulkan workgroup tiling).
4. Full Systems Architecture: C++20 / Vulkan core, JNI bindings, Gallery UI views.
5. Falsification Ledger: 10 critical failure modes and contingency pivots.

#### Phase 5: Production Implementation & Verification
- Implement all C++20, Vulkan SPIR-V shaders, and Java components under your exclusive ownership.
- Add unit tests in `tests/test_spatial.cpp`, `tests/test_dedup.cpp`, and `tests/test_transaction.cpp` validating:
  - 100% crash recovery without file corruption under simulated SIGKILL interrupts.
  - Flawless chunk deduplication across synthetic duplicate video streams.
  - 60 fps rendering of 200k Gaussians on Vulkan compute backend.
