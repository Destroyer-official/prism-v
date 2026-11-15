# Mathematical Invariants & Formal Proofs

Prism-V enforces 7 core mathematical invariants across its C++20 core, JNI layer, and Android MediaCodec pipeline. These invariants are verified on every continuous integration run and on physical silicon.

---

## 🧮 Invariant 1: Bit-Exact Deterministic Integer Arithmetic

### Formulation
Floating-point arithmetic introduces discrepancies between CPU instruction sets (e.g. x86 FMA vs ARM64 NEON). Prism-V mandates 64-bit signed fixed-point arithmetic with a 16-bit fractional scale:

$$\text{Fixed}(x) = \lfloor x \cdot 2^{16} + 0.5 \rfloor$$

Multiplication and division are defined over 64-bit integer domains:

$$A \otimes B = \frac{A \cdot B}{2^{16}}, \quad A \oslash B = \frac{A \cdot 2^{16}}{B}$$

### Invariant
For any input vector $\vec{v} \in \mathbb{Z}^N$, computation results are bit-identical across x86_64, ARMv8-A, ARMv9-A, and WebAssembly:

$$\text{CRC64}(\text{Pipeline}_{\text{x86}}(\vec{v})) \equiv \text{CRC64}(\text{Pipeline}_{\text{ARM64}}(\vec{v}))$$

---

## 📉 Invariant 2: rANS Shannon Entropy Limit

### Formulation
Prism-V employs an 8-bit streaming rANS (Asymmetric Numeral Systems) coder. Given an alphabet of symbols $s \in \mathcal{S}$ with frequencies $f_s$ and cumulative distribution $c_s = \sum_{i < s} f_i$ where $M = \sum f_s = 2^{12}$:

The encoding function $C(s, x)$ transforms state $x$ and symbol $s$:

$$C(s, x) = M \cdot \lfloor x / f_s \rfloor + (x \bmod f_s) + c_s$$

The decoding function $D(x)$ satisfies:

$$s = \text{lookup}(x \bmod M), \quad x' = f_s \cdot \lfloor x / M \rfloor + (x \bmod M) - c_s$$

### Invariant (Reversibility & Compression Efficiency)
1. Exact invertibility: $D(C(s, x)) = (s, x)$ for all states $x \in [L, 2L-1]$.
2. The empirical bitrate $R$ is strictly bounded by Shannon entropy $H(S)$:

$$H(S) \le R < H(S) + 0.05 \text{ bits/symbol}$$

---

## ⏱️ Invariant 3: BBR Rate Control & Encoder Queue Fullness

### Formulation
Hardware video encoders undergo latency spikes when input frame generation exceeds encoder hardware capability. Prism-V implements Bottleneck Bandwidth and RTT (BBR) rate pacing:

$$R_{\text{pacing}} = \text{pacing\_gain} \cdot \text{BtlBw}$$

Where $\text{BtlBw} = \max_{t \in W} (\text{DeliveryRate}(t))$ over an 8-GOP window $W$.

### Invariant (No Queue Bloat)
The hardware buffer queue length $Q(t)$ at time $t$ never exceeds double the Bandwidth-Delay Product (BDP):

$$Q(t) \le 2 \cdot \text{BDP} = 2 \cdot (\text{BtlBw} \cdot \text{RTT}_{\min})$$

Preventing hardware frame drops and Dalvik direct buffer starvation.

---

## 🧠 Invariant 4: Continuous-Coordinate Entropy Modeling

### Formulation
The continuous neural coordinate mapping engine models high-frequency residual variations using coordinate multi-layer perceptrons:

$$F(\mathbf{x}) = W_2 \cdot \text{GELU}(W_1 \cdot \mathbf{x} + b_1) + b_2, \quad \mathbf{x} = \begin{pmatrix} x / W \\ y / H \end{pmatrix}$$

### Invariant (Bounded Output Range)
For all spatial coordinates $(x, y) \in [0, W] \times [0, H]$:

$$|F(\mathbf{x})| \le \text{MaxResidual}, \quad \text{with } \text{NaN} \notin \text{Range}(F)$$

---

## 🌾 Invariant 5: AOM Film Grain Synthesis 1 (AFGS1)

### Formulation
High-frequency grain consumes up to 40% of video bitrate. AFGS1 strips grain during preprocessing and regenerates it parametrically via an auto-regressive (AR) filter:

$$G(x, y) = \sum_{(i,j) \in \Lambda} c_{i,j} \cdot G(x-i, y-j) + \epsilon(x, y)$$

### Invariant (Filter Stability)
The spectral radius $\rho$ of the AR coefficient matrix $C$ satisfies:

$$\rho(C) < 1.0$$

Ensuring synthesized grain remains stationary without numerical divergence.

---

## 📦 Invariant 6: Zero-Copy ISOBMFF Box Atom Invariance

### Formulation
When rebuilding or trimming MP4 containers, atoms (`ftyp`, `moov`, `mdat`) are indexed without payload replication.

### Invariant (Conservation of Media Bytes)
$$\sum_{i} \text{BoxSize}_i = \text{FileSizeBytes}$$

Chunk offsets in `stco` / `co64` strictly monotonically increase:

$$\text{Offset}_{k+1} > \text{Offset}_k \quad \forall k \in [1, K-1]$$

---

## 🔄 Invariant 7: Two-Phase Commit Atomic Storage Swap

### Formulation
File replacement on Android Scoped Storage must guarantee fault tolerance against crashes, low-memory kills (LMK), and battery exhaustion:

$$\text{State}(F) \in \{ \text{Original}, \text{Transcoded} \}$$

### Invariant
$$\text{State}(F) \neq \text{Corrupted} \quad \forall t$$

If an unhandled exception or SIGKILL occurs during stage $S$, the rollback journal restores $F \gets \text{Original}$ upon subsequent initialization.
