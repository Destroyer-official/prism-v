# Contributing to Prism-V

Thank you for your interest in contributing to **Prism-V**! We are committed to building the highest-performance, zero-bloat, clean-room video compression engine and mobile studio.

---

## 🏛️ Clean-Room Implementation Commitment

Prism-V is **100% Clean-Room Apache-2.0 Licensed**. We strictly prohibit any copy-pasting or derivative borrowing of GPL/LGPL licensed codebases (including FFmpeg, libav, x264, x265, or GPL plugins).

1. **Independent Formulation**: All code submitted must be an original work or derived exclusively from public standards (ISO/IEC 14496-12 ISOBMFF, AOM AV1 AFGS1, ITU-T H.264/H.265 specs, or academic papers with permissive licenses).
2. **Deterministic Arithmetic**: Code modifying video entropy, quantization, or rate pacing must use fixed-point deterministic integer arithmetic (`detint.hpp`) to guarantee cross-architecture reproducibility across x86_64, ARM64, and Apple Silicon.
3. **No Unbounded Memory**: All allocations in hot paths must be bounded or pool-managed (`HardwareRamBufferPool`).

---

## 🛠️ Development Workflow

### Prerequisites
- **C++20 Compiler**: Clang 16+, GCC 13+, or MSVC 2022+
- **CMake**: Version 3.20 or newer
- **Android NDK**: r26b or newer (mandatory 16 KB page-size linker compliance)
- **JDK**: Version 17+
- **Python**: 3.10+ (for test automation and verification tools)

### Building Native Core & Tests
```bash
# Configure CMake with Release flags
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20

# Compile targets (prism_core, prism_jni, prism_tests, prism CLI)
cmake --build build --config Release

# Run the 7/7 Invariant Test Suite
ctest --test-dir build --output-on-failure
```

### Building the Android Production APK
```powershell
# Using the automated build script:
.\build_apk.ps1
```

---

## 📐 Coding Conventions & Standards

- **C++**: Follow Modern C++20 conventions. Use `std::span`, `std::unique_ptr`, `constexpr`, and fixed-width integers (`uint8_t`, `int64_t`). No raw owning pointers.
- **Java/Kotlin**: Follow Android Jetpack & Material 3 guidelines. Ensure background services run with `mediaProcessing` foreground service types and implement `Service.onTimeout()`.
- **Memory Safety**: No allocations in per-frame rendering or audio PTS loop. Keep direct buffer pools bounded ($\le$ 128 MB on mobile).
- **Commit Messages**: Follow Conventional Commits format:
  - `feat(core): implement BBR pacing state machine`
  - `fix(engine): normalize first audio PTS on trim boundaries`
  - `docs(readme): add dual-player benchmark receipts`

---

## 🧪 Testing Checklist Before Submitting PRs

- [ ] All 7 native core test suites pass (`prism_tests`)
- [ ] Memory sanitizer / AddressSanitizer passes without leaks
- [ ] Android 16 KB page alignment confirmed via `-Wl,-z,max-page-size=16384`
- [ ] No GPL code or dependencies introduced
- [ ] Automated regression tests added for bug fixes
