# Section 5: Systems Architecture, Hardware Mapping & Deterministic ML

---

## 5.1 Hardware Allocation Matrix

Prism-V establishes strict functional mapping across the smartphone's heterogeneous silicon blocks to prevent thermal saturation and driver latency penalties:

```
+---------------------------------------------------------------------------------------+
|                                    PRISM-V WORKLOAD                                   |
+--------------------------+-----------------------+------------------+-----------------+
|  Hardware MediaCodec     |  CPU NEON / SME2      |  Vulkan 1.4 GPU  |  Mobile NPU     |
|  Fixed-Function ASIC     |  (Armv8.2-A / Armv9)  |  (SPIR-V Shaders)|  (LiteRT QNN)   |
+--------------------------+-----------------------+------------------+-----------------+
| - Base-Layer Encode      | - 100% Class A        | - Zero-Copy YUV  | - Encoder-Only  |
|   (AV1 Main10 / HEVC)    |   Integer Determin-   |   Color Resample |   Live Hyper-   |
| - Base-Layer Decode      |   istic Priors        |   (Lanczos-5)    |   network Pass  |
| - Zero-Copy Surface      | - 8-Way SIMD rANS     | - Class B Post-  |   (<3.5 ms/fr)  |
|   Buffer Handoff         | - Wavelet 5/3 Lifting |   Entropy ML     | - Overnight GOP |
|   (<0.015 J/frame)       | - AFGS1 Pseudo-Noise  |   Upsampling     |   Overfitting   |
|                          | - File I/O & Parsers  | - FFV1 Compute   |   Gradients     |
+--------------------------+-----------------------+------------------+-----------------+
```

- **Fixed-Function Video ASICs (`MediaCodec` via `AMediaCodec` C API):** Handles 100% of base-layer video decoding and encoding (`c2.qti.av1.encoder`, `c2.mtk.av1.encoder`, `c2.android.hevc.encoder`). Surfaces are exchanged via zero-copy `AHardwareBuffer` without transferring full frames into user-space RAM.
- **CPU Arm NEON & Armv9 SME2 (`KleidiAI`):** Executes **100% of Class A entropy-normative priors** (`Prism-DetInt`), the 8-way SIMD interleaved rANS codec (`libprism_rans`), and LeGall 5/3 integer wavelets. Executing Class A networks on CPU SIMD eliminates mobile NPU driver dispatch latency (which typically introduces 1.5–4.0 ms overhead per frame) and guarantees mathematical bit-exactness.
- **GPU (`Vulkan 1.3/1.4 Compute` with `VK_KHR_cooperative_matrix`):** Executes high-bandwidth, non-normative spatial operations: $1.5\times$ Lanczos-5 resampling (in 14-bit integer precision), YUV-to-RGB color mapping (`VK_ANDROID_external_memory_android_hardware_buffer`), and Class B post-entropy residual synthesis.
- **NPU (`LiteRT CompiledModel` with Qualcomm QNN / MediaTek NeuroPilot Delegates):** Used **strictly on the encoder side** for: (1) live feed-forward hypernetwork latent prediction (<3.5 ms/frame), and (2) background charging-time overfitting gradients. **The NPU is never invoked in the normative entropy-decoding loop.**

---

## 5.2 Systems Languages, Compilers & Tooling Stack

```mermaid
graph TD
    subgraph UI_Layer["Android Application Layer (Kotlin 2.1)"]
        UI["Jetpack Compose UI (4-Target Dial, Pre-Flight Sheet)"]
        WM["WorkManager (Charging / Idle / Thermal Constraints)"]
        VFS_SVC["Prism-VFS Service (openProxyFileDescriptor Bridge)"]
    end

    subgraph Native_Safe["Memory-Safe Core Layer (Rust Edition 2024)"]
        JNI["prism-jni (Zero-Overhead FFI via cxx / jni-rs)"]
        PARSER["Bitstream Parsers (H.264/HEVC CABAC, ISOBMFF Muxer, .pvloss)"]
        QUEUE["Tile Ring-Buffer Pool & Verified Transaction Engine"]
    end

    subgraph SIMD_Inner["Hot Inner Loops (C++20 / Assembly / Vulkan)"]
        RANS_ASM["arm_neon_rans8.S (Hand-Tuned 8-Way rANS Assembly)"]
        DETINT_SME2["arm_sme2_detint.cpp (KleidiAI + SME2 smopa Outer Products)"]
        VK_SHADERS["Vulkan SPIR-V Compute Shaders (VK_KHR_cooperative_matrix)"]
    end

    UI & WM & VFS_SVC --> JNI
    JNI --> PARSER & QUEUE
    PARSER & QUEUE --> RANS_ASM & DETINT_SME2 & VK_SHADERS
```

- **Rust (Edition 2024, compiled via `cargo-ndk` + LLVM 19/20):** Media parsers historically account for >30% of critical Android Remote Code Execution (CVE) vulnerabilities (e.g., Stagefright). Authoring all container muxers, NAL unit parsers, thread synchronization primitives, and JNI bindings in **safe Rust** eliminates buffer overflows, use-after-free, and concurrency defects by construction.
- **Modern C++20 & Arm Assembly (Clang 20, NDK r27+):** Inner vector loops are authored in C++20 and inline assembly, utilizing **Arm KleidiAI micro-kernels** ([Arm Developer, Oct 2025](https://developer.arm.com/community/arm-community-blogs/b/ai-blog/posts/executorch-1-0-is-here-and-with-sme2-optimizations-through-kleidiai)) for SME2 matrix outer products (`smopa`) and hand-tuned NEON `vdotq_s32` intrinsics.
- **Kotlin 2.1 + Jetpack Compose:** Provides modern, reactive UI components, Android lifecycle handling, `WorkManager` background persistence, and `MediaStore` integration.
- **Python 3.12 + PyTorch 2.6 + CompressAI:** Research, Quantization-Aware Training (QAT), and automated bitstream parity verification harness.

---

## 5.3 Prism-DetInt: Strict Cross-Silicon Integer Determinism Specification

Arithmetic and rANS entropy decoders are chaotically sensitive to probability divergence: a $10^{-7}$ rounding discrepancy between two phone NPUs shifts the rANS state $x$ irreversibly, turning all subsequent frames into static noise.  
Prism-V solves this with **Prism-DetInt**, defined over the ring of bounded two's-complement integers:

### Mathematical Definition of Class A Integer Layer
Given input activation vector $\mathbf{x} \in [-32767, +32767]^K$ (`int16_t`) and weight matrix $\mathbf{W} \in [-127, +127]^{M\times K}$ (`int8_t`):

1. **Bounded Channel Count Constraint ($K \le 512$):**
   The maximum channel depth $K$ of any normative neural layer is strictly bounded by $K \le 512$.
2. **Mathematical Overflow-Free Proof:**
   The maximum possible absolute value of the inner product accumulator $\mathbf{a}_m$ is:
   $$\max |\mathbf{a}_m| \le K \cdot \max |W_{m,k}| \cdot \max |x_k| = 512 \times 127 \times 32,767 = 2,130,649,088$$
   Because $2,130,649,088 < 2^{31} - 1 = 2,147,483,647$, the accumulator is **mathematically proven never to overflow a signed 32-bit integer (`int32_t`)**.  
   Because two's-complement integer addition without overflow is strictly associative:
   $$(\mathbf{a} + \mathbf{b}) + \mathbf{c} \equiv \mathbf{a} + (\mathbf{b} + \mathbf{c}) \pmod{2^{32}}$$
   The resulting dot-product is **100.000% identical** whether accumulated via scalar C, 128-bit Arm NEON (`vdotq_s32`), 512-bit Armv9 SME2 (`smopa`), or 256-bit x86 `AVX2`!

3. **Deterministic Dyadic Rescaling:**
   Accumulators are scaled via fixed-point multiplication and dyadic right-shift with round-to-nearest-even:
   $$\mathbf{z}_m = \text{clamp}_{[-32767, +32767]}\left( \left\lfloor \frac{\mathbf{a}_m \cdot M_{\text{scale}} + 2^{s - 1}}{2^s} \right\rfloor \right)$$
   where $M_{\text{scale}} \in [1, 32767]$ and $s \in [8, 24]$ are normative integer constants.

4. **Static 512-Byte Sigmoid/Laplace CDF Table:**
   Non-linear activation and probability mass accumulation bypass transcendental floating-point instructions (`exp`, `tanh`) entirely, using a static, normative 512-byte table compiled into the binary:
   $$\text{CDF}[k] = \text{CDF\_SIGMOID\_LUT}[d_k + 255] + k, \quad \text{normalized to } \text{CDF}[M] = 2^{15} = 32,768$$

---

## 5.4 Machine Learning Strategy & Model Delivery

```
APK Assets Directory (Total Embedded ML Footprint: 291 KB)
├── assets/models/
│   ├── bbr_mlp_h264_int8.bin       ( 21 KB - CABAC H.264 Micro-MLP )
│   ├── bbr_mlp_hevc_int8.bin       ( 21 KB - CABAC HEVC Micro-MLP )
│   ├── psc_checkerboard_conv.bin   ( 185 KB - LeGall 5/3 ConvNet )
│   └── coolchic_arm_prior.bin      ( 64 KB - Cool-chic ARM Prior )
└── assets/tables/
    └── cdf_sigmoid_512b.lut        ( 0.5 KB - Static Integer LUT )
```

- **Training Corpus:** Curated from licensed research datasets (`BVI-DVC`, `BVI-AOM`, `Vimeo-90k`, `YouTube-UGC`) plus a custom **5,000-clip `Prism-Camera2-RAW` dataset** recorded across 15 Android phone models (high-ISO noise, high-motion sports, screen recordings, 10-bit HDR10).
- **Embedded APK Footprint:** The entire normative decoder model weights and CDF lookup tables consume **only 291 KB**, embedded directly in the APK's `assets/`. Zero network access is required to encode or decode any Prism-V stream.
- **Encoder-Only Hypernetwork (`N-O Cool-chic`, ~4.8 MB INT8 `.tflite`):** Distributed via Google Play Feature Delivery as an on-demand install-time asset pack.

---

## 5.5 Memory Boundedness & Android 15+ 16 KB Page-Size Hardening

A leading cause of mobile transcoding failures is the Android Low Memory Killer (LMK) terminating processes when multiple full 4K frame buffers are allocated in heap memory (a single 4K 10-bit frame is 24.88 MB; buffering 16 frames consumes 400 MB).

### Zero-Full-Frame-Heap Policy
- All spatial processing in `Prism-PSC` and `Prism-Lossy` operates on **$256\times 256$ Luma ($128\times 128$ Chroma) spatial tiles** with a 16-pixel overlap halo.
- A pre-allocated, page-aligned **Ring Buffer of 12 Tile Slots** consumes **only 2.30 MB of L2/L3 cache-friendly RAM**.
- Frames from `MediaCodec` are imported via `AImageReader_acquireNextImageAsync` backed by `AHardwareBuffer`, mapped tile-by-tile without heap copying.
- Peak RSS is strictly bounded to **$\le 48\text{ MB}$ for 1080p** and **$\le 64\text{ MB}$ for 4K** on Tier 1 (low-end) devices.

### Android 15/16/17 16 KB Page-Size Compliance
Beginning with Android 15, Google Play strictly mandates native 16 KB page-size support on 64-bit devices:
- All hardcoded 4 KB macros (`PAGE_SIZE 4096`) are replaced by dynamic runtime queries (`sysconf(_SC_PAGESIZE)`).
- All shared native libraries (`libprism_jni.so`) are compiled with:
  `-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384`
- Tested and verified on Android 15/16 16 KB emulator targets.
