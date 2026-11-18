# Prism-V Mathematical Invariants & Verification Catalog

This specification catalogs all **10 Core Mathematical & Systems Invariants** audited by `prism audit` (`tools/prism_cli.cpp::cmd_audit()`) and enforced continuously by the 156-test native C++20 verification suite (`AllPrismTests`), the 7 CTest regression suites, and on-device Android diagnostics.

Each invariant is documented with its **exact mathematical formulation**, **hard numerical bounds**, **authoritative source files**, and **automated test coverage**. See [VERIFICATION.md](VERIFICATION.md) for live test receipts and physical device execution logs.

---

## Master Invariant Audit Matrix (`prism audit` — 10/10 Verified)

| # | Invariant Domain | `prism audit` Check / Scope | Hard Mathematical / Systems Bound | Primary Source Files | Automated Test Coverage |
|---|---|---|---|---|---|
| **1** | **DetInt Arithmetic & Sigmoid CDF** | `[1]` Dot Bound & `[4]` LUT Monotonicity | $\max\lVert \mathbf{a}\cdot\mathbf{w} \rVert = 2{,}130{,}641{,}408 < 2^{31}-1$; $L[0]=0, L[511]=32768$ | `core/src/detint.cpp`, `core/include/prism/detint.hpp` | 5 DetInt unit tests + `cmd_audit` `[1],[4]` |
| **2** | **16 KiB Page Alignment & Tile Ring** | `[2]` Page Size & `[3]` Tile Ring Budget | $\text{ptr} \bmod 16384 = 0$; Ring $\le 64\text{ MiB}$; Pool $\le 512\text{ MiB}$ | `core/src/detint.cpp`, `core/src/tile_pool.cpp`, `CMakeLists.txt` | 5 Alignment/Pool tests + `cmd_audit` `[2],[3]` |
| **3** | **rANS-8 & 16-Way SIMD rANS-16** | Entropy Coder Foundation (F1 / F2 / F4) | $M = 2^{15} = 32768$, $L = 2^{16} = 65536$, $\sum f_s = M$, $f_s \ge 1$ | `core/src/rans8.cpp`, `core/src/rans16.cpp`, `core/src/arm_sme2_rans16.S` | 10 rANS-8/16 tests |
| **4** | **Frontier 1: BBR 2.0 Recompression** | `[6]` SSG-CR4 + C-MAVQ + SIMD-16 | $\text{SHA256}(\hat{X}) = \text{SHA256}(X)$; Zero-Expansion Bypass | `core/src/bbr2.cpp`, `core/src/bbr.cpp` | 11 BBR/BBR2 tests + `Exp1A_BBR` + `cmd_audit` `[6]` |
| **5** | **Frontier 2: PSC 2.0 Directional Lifting** | `[8]` ADIW + YCoCg-R2 + A-IKLT | $\hat{I} = I \implies \text{PSNR} = \infty$; $\alpha,\beta \in [-128,127]$; $\sigma \in [4,127]$ | `core/src/psc2.cpp`, `core/src/psc.cpp` | 25 PSC/PSC2 tests + `Exp2A_PSC2` + `cmd_audit` `[8]` |
| **6** | **Frontier 3: BIS-RFSQ & Neural Codecs** | `[5]` BIS-RFSQ Determinism | $0\text{ ULPs}$ drift; $\text{Cool-chic} = 336\text{ MACs/px}$; $\text{NeRV} = 432\text{ MACs/px}$; $\Delta \le 55\text{ B/f}$ | `core/src/rfsq.cpp`, `coolchic.cpp`, `afgs1.cpp`, `nerv_engine.cpp`, `prism_gen.cpp` | 23 Frontier 3 tests + `Exp1B/1C` + `cmd_audit` `[5]` |
| **7** | **Frontier 4: Prism-Spatial 4DGS** | `[7]` MEF-LAD + LR-HCP + DZ-HLVQ | $\tau_{\text{dead}} = 1.8\Delta_v$; Sparsity >82% (88.1%); $\text{IPD}=63\text{ mm}$; $t < 16.6\text{ ms}$ | `core/src/spatial.cpp`, `core/shaders/spatial_rasterize.comp` | 14 Spatial tests + `Exp1D_Frontier4` + `cmd_audit` `[7]` |
| **8** | **Frontier 5: Silicon-Fused Pipeline** | `[9]` Sub-1.0 W Power & DRAM Bound | $P_{\text{SoC}} \le 1.0\text{ W}$ ($0.950\text{ W}$); $BW < 2.0\text{ GB/s}$ ($1.40$); $0.000\thinspace \mu\text{s}$ PTS drift | `core/src/frontier5_pipeline.cpp`, `core/src/tile_pool.cpp`, `frontier5_pipeline.comp` | 13 Frontier 5 tests + `cmd_audit` `[9]` |
| **9** | **Frontier 6: Rabin-Mesh Vault & VFS** | `[10]` VectorCDC + Bit-Exact Restore | Resync $\le 64\text{ KiB}$; $\text{SHA256}(\hat{X})=\text{SHA256}(X)$; Seek $\le 250\thinspace \mu\text{s}$ ($182.09\thinspace \mu\text{s}$) | `core/src/rabin_mesh.cpp`, `core/src/dedup_rabin.cpp`, `core/src/vfs_proxy.cpp` | 10 Mesh/Dedup/VFS tests + `Exp1E_Frontier6` + `cmd_audit` `[10]` |
| **10** | **Invariant 6: Zero-Loss 2PC & Containers** | Crash-Safe Atomic Replacement & Integrity | SIGKILL zero data loss; 0 dangling `.tmp`; CRC32C & exact-byte strictness | `core/src/transaction.cpp`, `container_pvloss.cpp`, `remux.cpp`, `isobmff.cpp`, `router.cpp` | 14 Transaction/Container/Remux/Router tests |

---

## Invariant 1 — Deterministic Integer Arithmetic (`DetInt`)

**Audit Check:** `prism audit` `[1/10]` (Max dot product) & `[4/10]` (Monotonic Sigmoid CDF LUT)  
**Source Files:** `core/src/detint.cpp`, `core/include/prism/detint.hpp`

### Exact Formulas & Bounds
1. **Overflow-Free 32-Bit Accumulation**:
   Given signed 16-bit activations $a_i \in [-32768, 32767]$ and signed 8-bit weights $w_i \in [-128, 127]$ with maximum vector length $N_{\max} = 512$:
   $$
   D = \sum_{i=0}^{N-1} a_i w_i, \qquad |D_{\max}| = 512 \times 127 \times 32767 = 2{,}130{,}641{,}408 < 2^{31} - 1 = 2{,}147{,}483{,}647
   $$
   Guarantees zero signed 32-bit integer overflow across all valid inputs without requiring 64-bit accumulators.
2. **Dyadic Fixed-Point Rescaling**:
   $$
   \text{dyadic-rescale}(x, s) = \text{clamp}_{[-32768,\thinspace +32767]}\left(\left\lfloor \frac{x + 2^{s-1}}{2^s} \right\rfloor\right), \quad s \in [1, 30]
   $$
3. **Normative Monotonic Sigmoid CDF Table (`CDF_SIGMOID_LUT`)**:
   512-entry static integer table $L \in \mathbb{Z}^{512}$ satisfying:
   $$
   L[0] = 0, \qquad L[511] = 32768, \qquad L[i] \ge L[i - 1]\quad \forall i \in \lbrace 1, \dots, 511 \rbrace
   $$

### Automated Test Coverage
- `test_detint_overflow_free_proof` (exact all-max and all-min 512-element dot products)
- `test_detint_dyadic_rescaling` & `test_detint_dyadic_rescale_shift_bounds` (rounding, upper/lower clamping, shift limits)
- `test_detint_static_lut` (endpoint values $0$ and $32768$, strict monotonicity)
- `test_detint_linear_layer_and_relu` (deterministic linear layer + integer ReLU)
- `tools/prism_cli.cpp::cmd_audit()` checks `[1]` and `[4]`

---

## Invariant 2 — 16 KiB Memory Page Alignment & Bounded Tile Ring

**Audit Check:** `prism audit` `[2/10]` (System page size) & `[3/10]` (Configured tile ring budget)  
**Source Files:** `core/src/detint.cpp`, `core/src/tile_pool.cpp`, `core/src/frontier5_pipeline.cpp`, `CMakeLists.txt`

### Exact Formulas & Bounds
1. **Android 15/16 16 KiB Page Alignment**:
   - ELF shared library linker invariant: `-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384`.
   - Heap & DMA buffer invariant:
     $$
     \text{addr}(\text{allocate-page-aligned}(n)) \bmod 16384 = 0, \qquad \text{PAGE-SIZE-16KB} = 2^{14} = 16{,}384\text{ bytes}
     $$
2. **Static Tile Ring & User Pool Hard Ceilings**:
   - Configured compile-time ring budget:
     $$
     M_{\text{ring}} = \text{MAX-TILE-SLOTS} \times \text{SpatialTile::PIXEL-COUNT} \times \text{sizeof}(\text{int16-t}) \le 64\text{ MiB}
     $$
   - Dynamic `TilePool` user memory ceiling: clamped strictly to $\le 512\text{ MiB}$ with zero hot-path dynamic allocations.

### Automated Test Coverage
- `test_detint_16kb_page_alignment` & `test_detint_aligned_buffer_raii`
- `test_cat_ring_buffer_pool_allocation_and_alignment` (16,384-byte DMA slot alignment)
- `test_tile_pool_acquire_tile_bounded_and_threads` & `test_tile_pool_multithreaded_stress_and_budget_enforcement` (16 concurrent threads, 16,000 acquire/release cycles, 0 leaks)
- `tools/prism_cli.cpp::cmd_audit()` checks `[2]` and `[3]`

---

## Invariant 3 — Interleaved SIMD rANS Entropy Coding (`rans8` & `rans16`)

**Audit Check:** Underlying entropy core verified in `prism audit` `[6]`, `[7]`, `[8]` and native unit tests  
**Source Files:** `core/src/rans8.cpp`, `core/src/rans16.cpp`, `core/src/arm_sme2_rans16.S`, `core/include/prism/rans8.hpp`, `core/include/prism/rans16.hpp`

### Exact Formulas & Bounds
1. **Probability Mass & Renormalization Bounds**:
   $$
   M = 2^{15} = 32{,}768, \qquad L = 2^{16} = 65{,}536, \qquad \text{Lanes} \in \lbrace 8, 16 \rbrace
   $$
   Valid symbol statistics $\lbrace f_s, C_s \rbrace_{s=0}^{K-1}$ must strictly satisfy:
   $$
   f_s \ge 1\ \forall s, \qquad C_0 = 0, \qquad C_{s+1} = C_s + f_s, \qquad \sum_{s=0}^{K-1} f_s = 32{,}768
   $$
2. **Bijective State Transition (Encode / Decode)**:
   - Encode step (reverse symbol order, lane $j \in [0, \text{Lanes}-1]$):
     $$
     x_{\text{next}} = \left\lfloor \frac{x}{f_s} \right\rfloor \cdot 2^{15} + (x \bmod f_s) + C_s, \quad \text{renormalize while } x \ge \frac{L}{M} \cdot 2^{16} \cdot f_s
     $$
   - Decode step (natural forward order, vectorized in `arm_sme2_rans16_decode_step`):
     $$
     \text{slot} = x \ \mathbin{\mathrm{AND}} \ \text{0x7FFF}, \qquad s = \text{CDF-LUT}[\text{slot}], \qquad x_{\text{prev}} = f_s \cdot (x \gg 15) + \text{slot} - C_s
     $$
3. **Strict Stream Safety**: Zero-frequency statistics and streams truncated by $\ge 2$ bytes are rejected deterministically with zero out-of-bounds reads.

### Automated Test Coverage
- `test_rans8_interleaved_exact_roundtrip`, `test_rans8_statistics_validation`, `test_rans8_truncated_stream_rejected`, `test_rans8_skewed_distribution_and_boundary_conditions`, `test_rans8_single_symbol_full_frequency`, `test_rans8_rejects_zero_frequency_stats`, `test_rans8_rejects_truncated_stream`
- `test_rans16_simd_interleaved_exact_roundtrip`, `test_rans16_high_volume_64k_stress`, `test_rans16_sme2_query`

---

## Invariant 4 — Frontier 1: BBR 2.0 Graph-Syntax Recompression & Zero-Expansion

**Audit Check:** `prism audit` `[6/10]` (SSG-CR4 + C-MAVQ + SIMD-16 roundtrip)  
**Source Files:** `core/src/bbr2.cpp`, `core/src/bbr.cpp`, `core/include/prism/bbr2.hpp`

### Exact Formulas & Bounds
1. **SSG-CR4 4-State Causal Integer Recurrence (`SsgCr4Predictor`)**:
   $$
   h_t = \left(A_t h_{t-1} + B_t x_t + 128\right) \gg 8, \qquad h_t \in \mathbb{Z}_{16}^4
   $$
   Outputs a deterministic 16-element integer syntax context vector (`OUTPUT_CTX_DIM = 16`).
2. **C-MAVQ 64-Symbol Tokenization (`CmavqTokenizer`)**:
   Maps NAL byte/coefficient streams into `ALPHABET_SIZE = 64` Vector Context Tokens plus escaped extra values (`extra_payload`) and discrepancy patches (`DiscrepancyPatch`), with alphabet frequencies summing exactly to $32{,}768$.
3. **Bit-Exact SHA-256 & Zero-Expansion Invariant**:
   For any input bitstream $X$ (H.264/AVC, H.265/HEVC, AV1):
   $$
   \text{Decompress}(\text{Compress}(X)) \equiv X \iff \text{SHA256}(\hat{X}) = \text{SHA256}(X)
   $$
   If the rANS-16 coded chunk exceeds $|X|$, the chunk header sets the raw bypass flag and emits $X$ unmodified, preventing bitrate expansion on incompressible noise.

### Automated Test Coverage
- `test_bbr_bit_exact_sha256_roundtrip`, `test_bbr_micro_mlp_deterministic`, `test_bbr_huffmanpos_buckets`
- `test_bbr2_ssg_cr4_phase_correlation`, `test_bbr2_c_mavq_tokenization`, `test_bbr2_bitstream_recompression_roundtrip`, `test_bbr2_multi_codec_hevc_av1_bit_exact`, `test_bbr2_zero_expansion_bypass_invariant`, `test_bbr2_discrepancy_patch_stream`, `test_bbr2_chunk_serialization_and_bypass`, `test_vfs_bbr2_open_proxy_serving`
- `Exp1A_BBR` (60.61% compression ratio, 266.8 MB/s SIMD-16 rANS, 100.000% SHA-256 parity across 10 GOPs)
- `tools/prism_cli.cpp::cmd_audit()` check `[6]` (256 KiB H.264 syntax roundtrip)

---

## Invariant 5 — Frontier 2: PSC 2.0 Directional Integer Lifting & Quad-Bayer Reversibility

**Audit Check:** `prism audit` `[8/10]` (ADIW + YCoCg-R2 + Wavefront SIMD roundtrip)  
**Source Files:** `core/src/psc2.cpp`, `core/src/psc.cpp`, `core/include/prism/psc2.hpp`

### Exact Formulas & Bounds
1. **Reversible Spectral Color Transforms (`YCoCg-R` & `YCoCg-R2`)**:
   - Base YCoCg-R lifting:
     $$
     Co = R - B, \quad t = B + \lfloor Co / 2 \rfloor, \quad Cg = G - t, \quad Y = t + \lfloor Cg / 2 \rfloor
     $$
   - Extended YCoCg-R2 adds adaptive triangular integer shears parameterized by least-squares slopes $\alpha, \beta \in [-128, +127]$ with exact integer inversion.
2. **Adaptive Directional Integer Wavelet (`ADIW 5/3` & `9/7`)**:
   - Four discrete lifting orientations $\theta \in \lbrace 0^\circ, 45^\circ, 90^\circ, 135^\circ \rbrace$ selected implicitly from coarse $LL$ gradients.
   - Native 32-bit lifting (`forward_adiw_2d_i32`, `inverse_adiw_2d_i32`) guarantees zero overflow on 10-bit, 12-bit, 14-bit, and 16-bit sensor inputs.
3. **Quad-Bayer `A-IKLT` & 4-Pass `CheckerboardConvNet`**:
   - 4-plane Quad-Bayer ($R, G_1, G_2, B$) lossless decorrelation (16.47% of raw 10-bit container, $\text{PSNR} = \infty$).
   - 4-pass causal ConvNet ($0.342\text{ GMACs/frame}$ @ 1080p) enforces logistic parameter bounds $\mu \in [-32768, 32767]$ and $\sigma \in [4, 127]$ with exact NEON (`vmlal_s16`) / SSE4.1 (`_mm_madd_epi16`) determinism.
4. **Wavefront Concurrency & Exact Reconstruction Invariant**:
   Across $1, 2, 4, 8$ worker threads traversing diagonal wavefronts $W = t_x + t_y$ on $128\times 128$ tiles (even dimensions $\ge 2\times 2$):
   $$
   \forall (x, y, c),\ \hat{I}(x, y, c) = I(x, y, c) \implies \text{PSNR} = \infty \land \text{SHA256}(\hat{I}) = \text{SHA256}(I)
   $$

### Automated Test Coverage
- `test_psc_*` (10 legacy RGB/Bayer/validation tests: odd-dimension rejection, $2\times 2$ minimum geometry, geometry mismatch rejection, invalid Bayer depth rejection)
- `test_psc2_*` (14 tests covering ADIW 5/3 & 9/7, `i32` 16-bit lifting, YCoCg-R2, Quad-Bayer A-IKLT 10–16 bit parity, `CheckerboardConvNet` bounds, 1/2/4/8-thread wavefront SHA-256 parity, P010 format, serialization, chunk payloads)
- `test_vfs_psc2_open_proxy_serving`, `Exp2A_PSC2`
- `tools/prism_cli.cpp::cmd_audit()` check `[8]` ($128\times 128$ RGB LeGall 5/3 roundtrip)

---

## Invariant 6 — Frontier 3: BIS-RFSQ Deterministic Quantization & Neural Codecs

**Audit Check:** `prism audit` `[5/10]` (Frontier 3 BIS-RFSQ mathematical determinism)  
**Source Files:** `core/src/rfsq.cpp`, `core/src/coolchic.cpp`, `core/src/afgs1.cpp`, `core/src/nerv_engine.cpp`, `core/src/prism_gen.cpp`, `core/shaders/detint_flow_gen.comp`

### Exact Formulas & Bounds
1. **2-Stage Bounded Integer-Scaled RFSQ (`BisRfsq`, `rfsq.cpp`)**:
   With stage levels $L_1 = 15, L_2 = 7$:
   $$
   k = \text{quantize-scalar}(x, L) \in \left[-\frac{L-1}{2}, +\frac{L-1}{2}\right], \qquad \text{deq}(-k) = -\text{deq}(k), \qquad \text{deq}(0) = 0
   $$
   - Deterministic `integer_layernorm` uses the 512-entry monotonic `ISQRT_LUT_512` ($\text{LUT}[0] = 4096$, $\text{LUT}[511] \in (0, 200)$), guaranteeing **0 ULPs cross-run and cross-platform divergence**.
   - `SemanticDeltaCodec` compresses 32 3D facial keypoints + 6-DoF pose deltas to **$\le 55\text{ bytes/frame}$** ($\le 64\text{ B}$ budget, $<1.5\text{ kbps}$ @ 30 fps).
2. **Cool-chic 5.0 Activity-Guided Complexity & Quality Bounds (`coolchic.cpp`)**:
   - Per-pixel MAC bound: **$336\text{ MACs/pixel}$** ($<600\text{ MACs/px}$ ceiling).
   - Full-frame activity-guided complexity ($8\times 8$ gradient gating): **$<0.35\text{ GMACs/frame}$** ($0.20\text{--}0.28\text{ GMACs}$ verified), $<16.6\text{ ms}$ latency at 60 fps, residuals bounded to 8-bit range.
   - `ParetoDialSolver` bounds: $\text{VMAF} \in (0, 100]$ (target $\ge 94.0$, $\text{CVVDP} \ge 9.50$, $\text{CAMBI} \le 1.30$).
3. **AFGS1 Temporal Grain Stability (`afgs1.cpp`)**:
   - 16-bit Galois LFSR (`0xB400`) + dyadic integer IIR smoothing ($\alpha = 192$) eliminates inter-frame grain boiling with exact deterministic replay.
4. **NeRV-S Continuous Coordinate Bounds (`nerv_engine.cpp`)**:
   - `NeRVSynthesisMLP`: strictly **$432\text{ MACs/pixel}$**, Y/U/V residuals clamped to $[-128, +127]$, fractional subframe latency $<0.05\text{ ms/frame}$.
   - `HessianBitAllocator`: curvature $H_{ii} = \mathbb{E}[(\nabla_{\theta_i}\mathcal{L})^2]$ water-fills mixed precision $\lbrace 2, 4, 8 \rbrace$ bits.
5. **Prism-Gen 2.0 & SPIR-V Validation (`prism_gen.cpp`, `detint_flow_gen.spv`)**:
   - Anchor frame $= 1\text{ byte}$, delta frame $\le 55\text{ bytes}$; `AudioAssistedConcealment` dampens 6-DoF velocity and synthesizes mouth blendshapes from LPC energy.
   - Compiled SPIR-V header validates magic `0x07230203` and `GL_EXT_integer_dot_product` (`sdot4`).

### Automated Test Coverage
- `test_rfsq_lattice_quantization_roundtrip`, `test_rfsq_integer_layernorm_determinism`, `test_rfsq_semantic_delta_codec_bitrate`, `test_rfsq_cross_platform_zero_divergence`
- `test_coolchic_*` (9 tests: bounded MACs, soft-gating blend, Pareto solver, determinism, batch, sub-0.35 GMACs, latency, YUV420, small dimensions)
- `test_afgs1_noise_extraction_and_synthesis`, `test_afgs1_temporal_grain_flicker_free`
- `test_prism_gen_dense_affine_warper_precision`, `test_prism_gen_detint_flow_generator`, `test_prism_gen_audio_concealment`, `test_prism_detint_flow_gen_spirv_validity`
- `test_nerv_triplane_lattice_hadamard_fusion`, `test_nerv_synthesis_mlp_mac_budget`, `test_nerv_hessian_bit_allocator`, `test_nerv_continuous_subframe_render_latency`
- `Exp1B_Lossy`, `Exp1C_Frontier3`, and `tools/prism_cli.cpp::cmd_audit()` check `[5]`

---

## Invariant 7 — Frontier 4: Prism-Spatial Dynamic 4DGS Video Invariants

**Audit Check:** `prism audit` `[7/10]` (MEF-LAD + LR-HCP + DZ-HLVQ roundtrip)  
**Source Files:** `core/src/spatial.cpp`, `core/include/prism/spatial.hpp`, `core/shaders/spatial_rasterize.comp`, `core/src/isobmff.cpp`

### Exact Formulas & Bounds
1. **Chebyshev Kinematic Trajectory Recurrence ($K = 3$)**:
   For normalized GOP time $\tau \in [-1, +1]$:
   $$
   T_1(\tau) = \tau, \quad T_2(\tau) = 2\tau^2 - 1, \quad T_3(\tau) = 4\tau^3 - 3\tau, \qquad \mathbf{p}_i(\tau) = \mathbf{p}_0 + \sum_{k=1}^{3} \mathbf{c}_k T_k(\tau)
   $$
   Analytical derivatives $\frac{d\mathbf{p}_i}{d\tau}$ and unit quaternion normalization ($\lVert \mathbf{q}_i(\tau) \rVert_2 = 1$) hold with zero NaN/Inf divergence; kinematic evaluation latency $<16.6\text{ ms}$ (**$0.07\text{ ms/frame}$**, 149.75M splats/sec).
2. **Static Anchor Classification & Subspace Energy Retention**:
   $$
   E_{\text{kin}} < 0.0015 \implies \text{is-static} = \text{true}
   $$
   Orthonormal 9-D $\to$ 4-D low-rank subspace factorization retains ≥ 98% of trajectory energy (55.5% dimension reduction).
3. **Dead-Zone Hyper-Lattice Vector Quantization (`DZ-HLVQ`)**:
   $$
   \tau_{\text{dead}} = 1.8 \Delta_v \quad (\Delta_v = 0.002 \implies \tau_{\text{dead}} = 0.0036\text{ m/s})
   $$
   Residual motion token zero-sparsity strictly exceeds 82% (**88.1%** verified).
4. **Dual-Domain Spherical Harmonics & 6-DoF Stereoscopic Geometry**:
   - BT.709 $Y'CbCr \leftrightarrow \text{RGB}$ DC round-trip error $\le 0.015$.
   - Binocular stereo view matrices preserve exact physical eye separation $\Delta X = -\text{IPD}$ ($-0.063\text{ m}$ at $63.0\text{ mm}$ virtual IPD) in Vulkan NDC ($Y$-flip, $Z \in [0, 1]$).
   - `'4dgs'` ISOBMFF dual-track mux/demux preserves exact bitstream, splat count, GOP size, and framerate.

### Automated Test Coverage
- `test_chebyshev_polynomial_trajectory_evaluation`, `test_chebyshev_analytical_velocity_derivatives`, `test_chebyshev_unit_quaternion_normalization`, `test_chebyshev_static_anchor_classification`
- `test_dead_zone_vector_quantizer_sparsity`, `test_spherical_harmonics_dc_yuv_roundtrip`, `test_dynamic_gaussian_field_4k60_evaluation_and_roundtrip`, `test_epipolar_flow_lifting`, `test_low_rank_subspace_factorization`, `test_spatial_camera_pose_and_stereo_matrices`, `test_isobmff_spatial_track_mux_and_demux`, `test_vulkan_compute_rasterizer_spirv_validity`, `test_spatial_extreme_edge_cases_and_robustness`, `test_mobile_tile_binning_and_depth_sorting`
- `Exp1D_Frontier4` and `tools/prism_cli.cpp::cmd_audit()` check `[7]`

---

## Invariant 8 — Frontier 5: Silicon-Fused Sub-1.0 W Power, DRAM & Zero-Drift Slicing

**Audit Check:** `prism audit` `[9/10]` (Sub-1.0 W Power Matrix & DRAM Bound)  
**Source Files:** `core/src/frontier5_pipeline.cpp`, `core/src/tile_pool.cpp`, `core/shaders/frontier5_pipeline.comp`

### Exact Formulas & Bounds
1. **Sub-Watt SoC Power & DRAM Bandwidth Envelope**:
   $$
   P_{\text{SoC}} \le 1.000\text{ W} \quad (0.950\text{ W verified}), \qquad BW_{\text{DRAM}} < 2.00\text{ GB/s} \quad (1.40\text{ GB/s verified vs } 9.2\text{ GB/s naive})
   $$
2. **SME2/NEON Micro-Residual & Fixed-Point Lanczos-5 Bounds**:
   - `ArmSme2TileEngine`: $\le 336\text{ MACs/pixel}$ with 100.000% bit-exact sample parity.
   - `VulkanComputeEngine`: 14-bit integer dyadic Lanczos-5 scaling ($2^{14} = 16384$) preserves constant regions with $0$ rounding drift; `frontier5_pipeline.spv` validates SPIR-V binary integrity.
3. **Nanosecond Atom Slicing & Zero PTS Drift (`LockFreeChunkedParallelSlicer`)**:
   - $O(1)$ metadata GOP partitioning and nanosecond epoch stitching guarantees:
     $$
     \Delta \text{PTS}_{\text{cumulative}} = 0.000\thinspace \mu\text{s} \quad \text{across } 1{,}000\text{ consecutive GOP seams}
     $$
4. **Heterogeneous Affinity & Lumped RC Thermal Control**:
   - `HeterogeneousResourceGovernor`: deterministic affinity masks across 1/4/8/12/16-core topologies with strict nice priorities (`+19` Eco, `0` Balanced, `-10` Turbo) and token-bucket memory admission rejection above budget.
   - `PredictiveThermalGearShifter`: lumped chassis RC model with $R_{\text{th}} = 4.86\text{ K/W}$, $C_{\text{th}} = 9.25\text{ J/K}$ ($\tau = R_{\text{th}} C_{\text{th}} = 44.955\text{ s}$), 10 s ADPF lookahead, $0\text{ ms}$ emergency downshift ($P \le 3.50\text{ W}$), and $5{,}000\text{ ms}$ upshift anti-chatter debounce.

### Automated Test Coverage
- `test_cat_ring_buffer_pool_allocation_and_alignment`, `test_vulkan_compute_engine_afgs1_and_lanczos5`, `test_arm_sme2_tile_engine_micro_residual_roundtrip`, `test_silicon_fused_pipeline_sub_watt_power_envelope`, `test_frontier5_dual_track_container_mux_and_demux`, `test_tile_pool_acquire_tile_bounded_and_threads`, `test_tile_pool_multithreaded_stress_and_budget_enforcement`, `test_heterogeneous_resource_governor`, `test_lock_free_chunked_parallel_slicing_and_nanosecond_stitcher`, `test_predictive_thermal_gear_shifter_lumped_rc_model`, `test_lock_free_slicer_1000_gops_zero_drift`, `test_governor_clamp_and_multicore_affinity`, `test_vulkan_frontier5_pipeline_spirv_validity`
- `tools/prism_cli.cpp::cmd_audit()` check `[9]`

---

## Invariant 9 — Frontier 6: Rabin-Mesh Deduplication Vault & Microsecond VFS Proxy

**Audit Check:** `prism audit` `[10/10]` (VectorCDC + Delta Manifest + Bit-Exact Restore)  
**Source Files:** `core/src/rabin_mesh.cpp`, `core/src/dedup_rabin.cpp`, `core/src/vfs_proxy.cpp`

### Exact Formulas & Bounds
1. **Level 1 Perceptual Burst Classification**:
   256-bit vPDQ perceptual hash + 128-D INT8 SSCD neural embedding classify near-duplicate camera takes when:
   $$
   \text{sim}_{\text{SSCD}}(\mathbf{e}_1, \mathbf{e}_2) \ge 0.85 \quad \land \quad d_H(h_1, h_2) \le 15
   $$
2. **Level 3 SIMD VectorCDC Resynchronization & Bit-Exact Restoration**:
   - Rolling polynomial content-defined chunking ($16\text{--}256\text{ KiB}$ blocks) automatically resynchronizes chunk boundaries within $\le 64\text{ KiB}$ downstream of arbitrary local byte edits.
   - Cryptographic reconstruction invariant:
     $$
     \text{SHA256}(\text{Reconstruct}(\mathcal{M}_{\text{pvmesh}}, \mathcal{V})) = \text{SHA256}(X_{\text{orig}})
     $$
   - Missing or hash-corrupted vault blocks cause deterministic restore failure (never silent corruption).
3. **Inversed Delta DAG Master Re-Election & VFS Seek Latency**:
   - `elect_new_master()` promotes a delta node in $O(V)$ with $0$ orphaned blocks and $0$ corrupted references.
   - `read_virtual_range()` satisfies cross-chunk random-access byte reads with mean latency **$\le 250\thinspace \mu\text{s}$** (**$182.09\thinspace \mu\text{s}$** measured across $100 \times 64\text{ KiB}$ reads) and LRU cache-hit tracking.

### Automated Test Coverage
- `test_rabin_mesh_keyframe_fingerprint`, `test_rabin_mesh_burst_duplicate_detection`, `test_rabin_mesh_gop_slicing`, `test_rabin_mesh_vector_cdc_and_virtual_reconstruction`, `test_rabin_mesh_zero_drift_multirange_seek`, `test_rabin_mesh_manifest_serialization_persistence`
- `test_dedup_rabin_chunking_and_restoration`, `test_dedup_rabin_sub_chunk_and_empty_stream`, `test_dedup_rabin_idempotent_duplicate_ingest`
- `test_vfs_open_proxy_serving_and_lru`
- `Exp1E_Frontier6` (57.5% cluster reclamation, 100.000% SHA-256 identity) and `tools/prism_cli.cpp::cmd_audit()` check `[10]`

---

## Invariant 10 (System Invariant 6) — Zero-Loss 2PC Transactions & Container Strictness

**Audit Check:** System Invariant 6 (Atomic 2-Phase Commit Crash Consistency + Container/Remux/Router Integrity)  
**Source Files:** `core/src/transaction.cpp`, `core/src/container_pvloss.cpp`, `core/src/remux.cpp`, `core/src/isobmff.cpp`, `core/src/router.cpp`, `android_app/src/com/prismv/transaction/VerifiedReplacement.java`

### Exact Formulas & Bounds
1. **Zero-Loss Two-Phase Commit (`VerifiedTransaction`, `transaction.cpp`)**:
   $$
   \text{STAGE}(\text{.tmp}) \xrightarrow{\text{verify-cb} \land \text{non-empty}} \text{ATOMIC-SWAP} \to \text{RECEIPT}(\text{GRACE}) + \text{TRASH}(F_{\text{orig}})
   $$
   - **Abort / `SIGKILL` Safety**: On verification failure, missing source, empty temp file, or mid-commit `SIGKILL` interruption:
     $$
     \text{SHA256}(F_{\text{orig}}^{\text{post}}) = \text{SHA256}(F_{\text{orig}}^{\text{pre}}) \quad \land \quad |\lbrace \text{dangling }\texttt{.tmp} \rbrace| = 0
     $$
2. **`.pvloss` Container Integrity (`container_pvloss.cpp`)**:
   - Fixed 72-byte `PVLS` header (carrying original SHA-256), CRC32C checksum per chunk, and 28-byte-per-entry seek index strictly bounds-checked against physical file size (rejects short headers, out-of-bounds index offsets, and truncated chunks).
3. **ISOBMFF, Remux & Tri-Contract Router Strictness (`isobmff.cpp`, `remux.cpp`, `router.cpp`)**:
   - `remux_to_clean_isobmff` & `parse_and_verify_isobmff` preserve exact video/audio packet SHA-256 and HDR/colorimetry boxes (`colr`, `mdcv`, `clli`, `pasp`, rotation), while strictly rejecting null, empty, truncated ($-1$ byte), or trailing-garbage ($+1$ byte) streams.
   - `IngestRouter` enforces the 3% lossless routing boundary: estimated savings $< 3\text{ percent} \implies \text{Contract A (Remux)}$; ≥ 3% on AVC/HEVC/AV1 $\implies \text{Contract B (BBR/BBR 2.0)}$; TIFF/RAW/RGB $\implies \text{Contract C (PSC/PSC 2.0)}$; lossy request $\implies \text{Hybrid Dual-Track}$.

### Automated Test Coverage
- `test_transaction_successful_commit_and_grace`, `test_transaction_rollback_on_verification_failure`, `test_transaction_missing_source_fails_candidate_stage`, `test_transaction_empty_temp_fails_local_verify`, `test_transaction_simulated_sigkill_crash_recovery`
- `test_container_pvloss_roundtrip_and_seek`, `test_container_rejects_short_and_bad_index`, `test_container_rejects_truncated_chunk`
- `test_container_isobmff_dual_track_mux_demux`, `test_container_isobmff_metadata_ledger_preservation`
- `test_remux_savings_threshold_predicate`, `test_remux_packet_preservation_and_sha256_parity`, `test_remux_rejects_truncated_and_trailing_data`
- `test_router_lossless_tri_contract_and_hybrid_dispatch`

---

## Verification Scope & Boundaries

- **Verified Automatically**: All 10 invariants above are executed continuously by `prism audit` (10/10 PASS), `AllPrismTests` (156/156 PASS), `ctest` (7/7 PASS), `JniVerificationTest` (11/11 PASS), and live Android physical hardware self-tests (`SM-E156B` Android 16: 6/6 PASS; `SM-A507FN` Android 11: 7/7 PASS).
- **Explicit Out-of-Scope Boundaries for Unit Harness**: While `DetInt` and `BisRfsq` are architecturally designed for zero-ULP cross-architecture determinism across ARM64/x86_64/RISC-V, the MSVC x64 host unit test binary validates single-ISA host determinism while on-device Android ARM64 self-tests independently verify ARM64 execution parity. Arbitrary non-standard third-party fragmented MP4 edit lists and kernel-level power-cut disk controller failures lie outside the unit harness scope.
