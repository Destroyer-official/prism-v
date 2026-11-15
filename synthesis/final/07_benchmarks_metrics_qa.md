# Section 7: Benchmarks, Quality Metrics & Hardware-in-the-Loop QA

---

## 7.1 Tri-Metric Perceptual Consensus Suite & Formal Scoring Functions

Single-metric optimization (e.g. optimizing PSNR or uncalibrated VMAF) leads to deceptive algorithms: neural filters easily inflate VMAF by adding high-contrast sharpening or artificial edge halos, while introducing severe color banding in dark gradients.  
Prism-V establishes a **Tri-Metric Perceptual Consensus** to enforce true visual fidelity:

```mermaid
graph TD
    RECON[Decoded Reconstructed Video] & REF[Original Reference Video] --> VMAF_NEG[VMAF-NEG v3.1.0<br/>No Enhancement Gain Model<br/>Penalizes Artificial Sharpening]
    RECON & REF --> CVVDP[ColorVideoVDP v1.2<br/>Psychophysical Contrast Sensitivity<br/>Models Mobile OLED Viewing Angles & HDR]
    RECON & REF --> CAMBI[CAMBI Banding Index<br/>Detects 10-bit to 8-bit Gradient Steps<br/>Hard Constraint: CAMBI < 2.5]
    
    VMAF_NEG & CVVDP & CAMBI --> CONSENSUS[Consensus Perceptual BD-Rate:<br/>ΔR = 0.50·BD(VMAF) + 0.40·BD(CVVDP) + 0.10·BD(XPSNR)]
```

1. **VMAF-NEG (No Enhancement Gain, `libvmaf v3.1.0`) [VP]:**  
   Evaluates perceptual quality using Netflix's official NEG model, which sets a hard upper bound on image sharpening so that artificial contrast boosts do not receive inflated scores (AV2 CTC standard, arXiv:2605.15800v1).
2. **ColorVideoVDP (`CVVDP v1.2`) [VP]:**  
   Developed by the University of Cambridge Rainbow Group, CVVDP is a full-reference psychophysical metric modeling human spatial, temporal, and chromatic contrast sensitivity across SDR and HDR displays, calibrated for smartphone viewing distances (~30 cm).
3. **CAMBI (Contrast Aware Multiscale Banding Index) [VP]:**  
   Measures false contouring and color banding in smooth regions (e.g., skies, sunsets, dark scenes). Enforces a hard gate: **$\text{CAMBI} < 2.5$**.

### Formal Mathematical Scoring Functions

#### 1. Lossless Composite Score ($S_{\text{Lossless}}$, higher is better; Baseline `FFV1` = 1.000)
Any codec that fails bit-exact verification ($\text{SHA256}(\text{Decoded}) \neq \text{SHA256}(\text{Source})$) immediately receives $S_{\text{Lossless}} = 0$. For verified bit-exact codecs:

$$S_{\text{Lossless}} = \left( \frac{\text{CR}_{\text{codec}}}{\text{CR}_{\text{FFV1}}} \right)^{0.65} \cdot \left( \frac{E_{\text{FFV1}}}{E_{\text{codec}}} \right)^{0.20} \cdot \left( \frac{\text{FPS}_{\text{codec}}}{\text{FPS}_{\text{FFV1}}} \right)^{0.15} \cdot \mathbb{I}\left( \text{RAM}_{\text{peak}} \le M_{\text{tier}} \right)$$

where $\text{CR} = \frac{\text{Size}_{\text{orig}}}{\text{Size}_{\text{compressed}}}$, $E$ is energy in Joules/frame, $\text{FPS}$ is decode throughput, and $M_{\text{tier}}$ is the tier memory ceiling (64 MB on Tier 1).

#### 2. Lossy Composite Score ($S_{\text{Lossy}}(\mathbf{w})$, higher is better; Baseline Hardware HEVC = 1.000)
Let the consensus rate difference be $\Delta R_{\text{consensus}} = 0.50 \cdot \text{BD-Rate}_{\text{VMAF-NEG}} + 0.40 \cdot \text{BD-Rate}_{\text{CVVDP}} + 0.10 \cdot \text{BD-Rate}_{\text{XPSNR}}$ (negative indicates bitrate savings). Given user weights $\mathbf{w} = (w_Q, w_E, w_S)$ selected on the UI dial ($\sum w_i = 1$):

$$S_{\text{Lossy}}(\mathbf{w}) = \left( \frac{1}{1 + \Delta R_{\text{consensus}}} \right)^{w_Q} \cdot \left( \frac{E_{\text{base}}}{E_{\text{codec}}} \right)^{w_E} \cdot \left( \frac{\min(\text{FPS}_{\text{codec}}, 60)}{\min(\text{FPS}_{\text{base}}, 60)} \right)^{w_S} \cdot \mathbb{I}(\text{CAMBI} < 2.5)$$

---

## 7.2 Stratified Test Video Corpora

Benchmarking evaluates two complementary datasets:

### 1. Standard Academic Benchmarks (for Peer-Reviewed Comparability)
- **`UVG` (Ultra Video Group):** 7 sequences at 1080p and 4K (50/120 fps, YUV420 8/10-bit) for classical rate-distortion comparison.
- **`JVET CTC Class A1/A2 (4K) & Class B (1080p)`:** Standardized broadcast and cinema sequences.
- **`AV2 CTC` & `BVI-AOM`:** Official AOMedia AV2 evaluation suites covering high-motion and complex textures.

### 2. Mobile-Specific Real-World Suite (`Prism-Mobile-Bench-50`, 50 clips, 10s each)
- **Category A (15 clips) — Handheld Camera Captures:** 4K60 HDR10 P010 and 1080p30 NV12 recorded across Samsung, Pixel, and Xiaomi devices (daylight, low-light high-ISO sensor noise).
- **Category B (10 clips) — Social Messaging Re-encodes:** 720p/1080p H.264/HEVC files downloaded from WhatsApp, Instagram, and TikTok (vital for validating `Mode L-BBR`).
- **Category C (10 clips) — Screen Recordings & Gaming:** 60/120 fps mobile screen captures with sharp vector text, UI scrolling, and high-contrast synthetic graphics.
- **Category D (10 clips) — Talking-Head Video Calls:** Static backgrounds, low-motion facial micro-expressions, varying lighting conditions.
- **Category E (5 clips) — High-Motion & Inspection:** Sports, fast panning, water splashes, high-frame-rate slow-motion captures.

---

## 7.3 Automated Hardware-in-the-Loop (HIL) Power Rig

Every Git commit in the continuous integration (CI) pipeline is benchmarked across a physical 6-phone hardware rack instrumented with continuous power telemetry:

```
+---------------------------------------------------------------------------------------+
|                AUTOMATED HARDWARE-IN-THE-LOOP (HIL) BENCHMARK RACK                     |
+---------------------------------------------------------------------------------------+
|                                                                                       |
|   [ Peltier Thermal Chamber: 25.0°C ± 0.5°C ]                                         |
|                                                                                       |
|   +-----------------------+   +-----------------------+   +-----------------------+   |
|   | Tier 1A: Redmi Note 13|   | Tier 2A: Pixel 9a     |   | Tier 3A: Xiaomi 16    |   |
|   | (Helio G99 / 4GB RAM) |   | (Tensor G4 / 8GB RAM) |   | (Snapdragon 8 Elite)  |   |
|   +-----------------------+   +-----------------------+   +-----------------------+   |
|   +-----------------------+   +-----------------------+   +-----------------------+   |
|   | Tier 1B: Galaxy A16 5G|   | Tier 2B: Poco X7 Pro  |   | Tier 3B: Vivo X300 Pro|   |
|   | (Dimensity 6300 / 4GB)|   | (Dimensity 8400 / 12G)|   | (Dimensity 9500 / 16G)|   |
|   +-----------------------+   +-----------------------+   +-----------------------+   |
|                                                                                       |
|   [ Dummy Battery Interposers ] <==== Powered by ====> [ Monsoon High Voltage Meter ]  |
|                                                        (Sampling rail at 5 kHz)       |
+---------------------------------------------------------------------------------------+
|  Continuous CI Controller (Jenkins / GitHub Actions):                                 |
|  - Measures exact Joules/frame, PSS/RSS RAM, thermal throttling delta, and VMAF-NEG.  |
|  - Triggers automated regression alerts if energy increases by >3% or BD-rate drops.  |
+---------------------------------------------------------------------------------------+
```

---

## 7.4 Quantitative Quality & Falsification Gates

| Milestone Gate | Target Milestone | Strict Quantitative Criteria (Must Pass 100% to Unlock Next Phase) |
|:---|:---|:---|
| **Gate G0** | **Phase 0 (Day 30)** | (1) `Prism-BBR` achieves $\ge 14\%$ lossless size reduction on H.264 and $\ge 9\%$ on HEVC at $\ge 100\text{ MB/s}$ decode speed;<br>(2) `Prism-DetInt` shows 0 bit-flips across all 5 SoCs on 10,000 frames;<br>(3) `Prism-Lossy` achieves $\ge 15\%$ VMAF-NEG BD-rate gain over HW base at $\le 1,200\text{ MAC/pixel}$. |
| **Gate G1** | **Phase 1 (Month 6)** | (1) `Prism-BBR` sequential decode reaches $\ge 160\text{ MB/s}$ on Tier 2 CPU at $<0.8\text{ W}$;<br>(2) `Prism-PSC` compresses `UVG` 1080p to $\le 31.0\%$ of raw size at $\ge 30\text{ fps}$ on Tier 3 and $\ge 15\text{ fps}$ on Tier 2;<br>(3) Peak working memory strictly capped at $\le 64\text{ MB}$ RSS on Tier 1 (Redmi Note 13);<br>(4) 100M-iteration fuzzing pass with zero memory safety panics. |
| **Gate G2** | **Phase 2 (Month 11)** | (1) Live 1080p60 / 4K30 capture runs with 0 dropped frames and $\le 2.4\text{ W}$ total system power, achieving $\ge 15\%$ BD-rate over HW base;<br>(2) Charging overfit achieves $\ge 25\%$ BD-rate over HW AV1 and $\ge 35\%$ over HW HEVC;<br>(3) Hybrid decode reaches $\ge 60\text{ fps}$ on Tier 1. |
| **Gate G3** | **Phase 3 (Month 15)** | (1) WhatsApp, Instagram, CapCut, and ExoPlayer open and stream 100 `.pvloss` virtual `.mp4` files with 0 crashes and <80 ms latency to first frame;<br>(2) 72-hour continuous stress test on all 6 HIL phones with 0 OOMs and zero thermal aborts. |
| **Gate G4** | **Phase 4 (Month 18 Launch)** | (1) ITU-R BT.500 double-stimulus subjective viewing test (60 human raters) confirms objective CVVDP/VMAF gains within $\pm 3\%$;<br>(2) External security and patent FTO sign-off completed with zero high/critical vulnerabilities. |
