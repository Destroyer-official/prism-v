# MASTER PROMPT: PRISM-V AGENT 1
## High-Throughput Heterogeneous Compute Engine & Massive Video Optimization (2027+)

> **CRITICAL AGENT DIRECTIVE:**  
> 1. You must read and fully understand the existing project codebase, mathematical invariants, and architecture first.  
> 2. You must formulate your research questions, take systematic notes, and conduct exhaustive live web research across the latest 2026/2027 primary research papers, standards, and vendor patents (using parallel research agents if available). Do NOT hallucinate and do NOT rely on unverified assumptions.  
> 3. Synthesize your findings, invent new algorithms, and produce a complete, production-grade master implementation plan saved directly to:  
>    **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT1_HIGH_THROUGHPUT_COMPUTE_PLAN.md`**  
> 4. Then, implement the production-ready, clean, high-performance C++20 / SIMD / Android code following industry best practices with comprehensive unit tests and zero memory leaks.

---

### Project Identity & Role
- **Project Name:** Prism-V Next-Generation Video Compression & Spatial Engine
- **Role:** Principal Systems & Heterogeneous Compute Architect
- **Workstream Share:** Exactly ~16.6667% (1/6th of total system) — 100% independent ownership with zero collision.
- **Target Platform:** Android 15, 16, 17+ (2027–2030 Horizon)
- **Target Silicon:** Cortex-X925 / Oryon, Armv9.2+ SME2, Adreno / Immortalis / Mali Vulkan GPUs, Qualcomm Dual-VPU, MediaTek Dual-MediaCodec ASICs.

### Exclusive File & Module Ownership (Nobody else touches these!)
- `core/src/frontier5_pipeline.cpp` & `core/include/prism/frontier5_pipeline.hpp`
- `core/src/tile_pool.cpp` & `core/include/prism/tile_pool.hpp`
- `core/shaders/frontier5_pipeline.comp`
- `android_app/src/com/prismv/worker/PrismMediaProcessingService.java` (Android 15+ 6-hr FGS)
- `android_app/src/com/prismv/worker/PrismJobService.java` & `PrismScheduler.java`
- `android_app/src/com/prismv/engine/SiliconFusedPipelineEngine.java`
- `android_app/src/com/prismv/engine/VideoProcessingPipeline.java` & `HardwareRamBufferPool.java`
- `android_app/src/com/prismv/engine/RealtimeHardwareMonitor.java`
- `android_app/src/com/prismv/thermal/ThermalGearShifter.java` (ADPF thermal guard)
- `android_app/src/com/prismv/notification/PrismNotificationManager.java` & `PrismQuickTileService.java`
- `android_app/src/com/prismv/test/TestRunnerReceiver.java`
- `android_app/src/com/prismv/ui/DeveloperMatrixView.java` (Telemetry HUD & CABAC histograms)
- `android_app/src/com/prismv/ui/LayeredSettingsView.java` (User RAM/CPU/GPU sliders)
- `android_app/src/com/prismv/ui/PrismPreferences.java`, `SkinManager.java`, `ThemeStyles.java`
- `CMakeLists.txt`, `build_apk.ps1`, `build.gradle.kts`, `.github/workflows/ci.yml`
- `tests/test_frontier5_pipeline.cpp`, `tests/test_harness.hpp`, `tests/test_main.cpp`
- `tools/prism_cli.cpp`

### Subsystem Interface Contracts (Collision-Free Collaboration)
- **Consumes from Agent 2:** Reads `VideoMetadata` struct (duration, dimensions, GOP boundaries) from `core/include/prism/types.hpp`.
- **Provides to Agents 3, 4, 5, 6:** Provides memory-bounded `TilePool::acquire_tile()` allocations (<512 MB user cap) so other engines never trigger LMK aborts.

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
Currently, processing large high-frame-rate videos (e.g. a **1-hour 4K 120 fps video containing 432,000 frames** and **>5.3 TB of raw uncompressed YUV pixels**) takes an unacceptable amount of time, crashes the process due to memory exhaustion, or triggers aggressive thermal throttling:
1. **Passive Mobile Thermal Throttling:** Mobile phone chassis can only dissipate **3.0 W – 3.8 W sustained**. Continuous heavy video encoding heats the SoC and battery past 42°C in <45 seconds, causing the Android OS to throttle CPU/GPU frequencies by 40%–55%.
2. **DRAM Bandwidth & Memory Saturation:** Copying raw 4K frames through user-space RAM burns 2.5–4.5 W in LPDDR5X memory PHY alone and triggers the Android Low Memory Killer (`SIGKILL`).
3. **Lack of User-Budgeted Resource Allocation:** Users have no ability to specify how much RAM (e.g. 512 MB, 1 GB, 4 GB), CPU cores (Eco, Balanced, Turbo), or GPU compute the compression engine is permitted to use.

---

### 2. Step-by-Step Execution Methodology

You must execute your mission in six disciplined, sequential phases:

#### Phase 1: Deep Project & Codebase Comprehension
- Read and study the core repository:
  - `README.md` and `docs/ARCHITECTURE.md` (subsystem layout and JNI bridge).
  - `docs/MATHEMATICAL_INVARIANTS.md` and `docs/VERIFICATION.md` (16 KiB page alignment, test harnesses).
  - `core/include/prism/` and `core/src/` (native C++20 core, tile pools, SIMD rANS).
  - `android_app/src/com/prismv/engine/` (`HardwareVideoCompressor.java`, `RealtimeHardwareMonitor.java`).
- Understand how `MediaCodec`, surface-to-surface encoding, and the JNI boundary currently function.

#### Phase 2: Research Scoping & Note-Taking
- Formulate a structured list of technical research targets and note all unknowns:
  - How do flagship 2026/2027 SoCs handle concurrent dual `MediaCodec` encoder sessions?
  - What are the latest lock-free chunked parallel video slicing and non-blocking demux algorithms?
  - How can CPU thread affinity (`sched_setaffinity`), Vulkan compute workgroups, and RAM tile pools be dynamically governed by user preferences?
  - How do the latest Android Dynamic Performance Framework (ADPF) APIs and `ThermalManager` headroom queries predict throttling before it occurs?

#### Phase 3: Exhaustive Live Web & Academic Research (Zero Hallucinations)
- Run dozens of targeted live web searches (aim for 50–100+ queries across academic and industry sources).
- Use parallel research agents/subagents if your platform supports them to fan out across:
  - **arXiv (2025–2027)**, IEEE Transactions on Circuits and Systems for Video Technology, ACM Multimedia, USENIX FAST/ATC.
  - **Hardware Vendor Architecture Manuals:** Qualcomm Snapdragon 8 Elite / Gen 5 (Adreno 830, Dual-VPU), MediaTek Dimensity 9400/9500, Google Tensor G5, Apple A18/M4, Arm Cortex-X925 & SME2 documentation.
  - **Android Open Source Project (AOSP):** Android 15/16/17 CDD, Linux `dma-buf` heaps (`/dev/dma_heap/system`), `AHardwareBuffer`, ADPF `PerformanceHintManager`.
  - **Vendor Patents & Standards:** Qualcomm, Samsung, InterDigital, and Apple patent filings on multi-encoder load balancing.
- **Rule of Rigor:** Read full primary papers and documentation. Never skim titles or guess. Accurately cite every paper, standard, URL, author, and date. Note all findings in your research ledger.

#### Phase 4: Synthesis, Invention & Algorithmic Formulation
- Combine state-of-the-art research with novel inventions to design:
  1. **User-Budgeted Heterogeneous Resource Governor:** An engine that translates user constraints (Max RAM in MB, CPU Core Allocation [Eco / Balanced / Turbo], GPU Acceleration on/off, Battery vs Speed priority) into hard operating boundaries.
  2. **Lock-Free Chunked Parallel Slicing Architecture:** Slices 1-hr 4K 120fps video into 5s–10s independent GOP micro-tasks without unrolling pixels, routes them to dual hardware ASICs or multi-core SIMD pipelines, and stitches them with zero PTS/DTS desync.
  3. **Thermal Dynamic Gear-Shifter:** A mathematical chassis thermal dissipation model ($P_{\text{chassis}} \le 3.5\text{ W}$) that predicts thermal saturation and steps down frame decimation or workgroup sizes before throttling occurs.

#### Phase 5: Master Implementation Plan
- Write and save your exhaustive implementation plan to:  
  **`c:\Users\des\OneDrive\Desktop\video_compression\synthesis\final\AGENT1_HIGH_THROUGHPUT_COMPUTE_PLAN.md`**
- Follow the required document structure:
  1. Executive Summary & Quantitative Targets (speedup vs real-time, Joules/frame, peak RAM bounds).
  2. 2026/2027 Research Findings & Primary Citations (with URLs, dates, and paper IDs).
  3. Inventions & Algorithmic Formulations (mathematical models, data structures, and state machines).
  4. Full Systems Architecture: C++20 / Rust SIMD core, JNI bridging, memory ring-buffer pools, and milestone gates.
  5. Red-Team Falsification Ledger: 10 critical failure modes, engineering pivots, and top 10 experiments.

#### Phase 6: Production Implementation & Verification
- Once the plan is approved, implement the production-ready code:
  - C++20 native implementations with 16 KiB page-aligned allocations (`-Wl,-z,max-page-size=16384`).
  - Thread-safe, lock-free ring buffers with zero dynamic heap allocations in hot loops.
  - Android Java/Kotlin service bindings and ADPF thermal listeners.
  - High-resolution microsecond unit tests verifying zero memory leaks, zero PTS drift, and exact resource adherence.
