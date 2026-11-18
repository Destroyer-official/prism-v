# Contributing to Prism-V

Thank you for your interest in contributing to Prism-V! This document outlines our development workflows, coding standards, and contribution guidelines.

---

## 1. Code of Conduct & Intellectual Property

- **License**: Prism-V is licensed under the **Apache License, Version 2.0**. All contributed code must be your own original work or compatible with permissive open-source licenses.
- **Clean-Room Implementations**: Under no circumstances should code from GPL or LGPL projects (such as FFmpeg, libav, x264, or x265) be imported or copied into this repository.
- **Integer Determinism**: Algorithms touching entropy coding, quantization, or rate control must maintain deterministic integer arithmetic (`detint.hpp`). Avoid non-deterministic floating-point operations unless pinned by unit tests.

---

## 2. Development Prerequisites

To develop, build, and test Prism-V:

- **C++ Compiler**: Clang 16+, GCC 13+, or MSVC 2022+ with full C++20 support.
- **Build System**: CMake 3.20+ and Ninja (recommended) or MSBuild.
- **Android Development**:
  - Android platform SDK (API level 26 minimum, API 35/36 target).
  - OpenJDK 17 or higher.
  - Optional: Android NDK r26b+ for native Android compilation.
  - Standalone APK tools: `apktool.jar`, `r8lib.jar`, and `uber-apk-signer.jar`.

---

## 3. Building & Testing

### Native C++20 Core & CLI

```bash
# Configure with CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20

# Compile
cmake --build build --config Release

# Run complete test suite
ctest --test-dir build --output-on-failure -C Release

# Run standalone unit test runner directly
./build/prism_tests
```

### Standalone Android APK

Build the packaged Java application directly without Gradle:

```powershell
# Using the root build script:
.\build_apk.ps1

# Or explicitly specifying tool locations:
$env:PRISM_ANDROID_JAR = "C:\path\to\android.jar"
$env:PRISM_APKTOOL_JAR = "C:\path\to\apktool.jar"
$env:PRISM_R8_JAR      = "C:\path\to\r8lib.jar"
$env:PRISM_SIGNER_JAR  = "C:\path\to\uber-apk-signer.jar"
.\scripts\build_apk.ps1
```

The resulting signed, aligned APK is output to `release/prism-v-v1.1.2.apk` and `android_app/signed/`.

---

## 4. Coding Conventions

### C++ (`core/`, `tools/`, `tests/`)

- Follow modern C++20 conventions: structured bindings, `std::span`, concepts, and RAII.
- Use explicit fixed-width integers (`int32_t`, `uint64_t`, etc.) from `<cstdint>`.
- Zero heap allocations inside critical per-frame processing loops; utilize `tile_pool` for memory budgeting.
- Maintain 16 KiB page alignment compatibility (`-Wl,-z,max-page-size=16384`).

### Android Java (`android_app/`)

- Target Java 17 bytecode compatibility (`--release 17`).
- Keep UI operations thread-safe; offload transcoding, container writing, and heavy computations to background worker threads or `PrismMediaProcessingService`.
- Follow Material 3 dark glassmorphic styling and design consistency.

### Git Commit Style

We adhere to the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```text
feat(duo): add multi-touch curtain slider with parent intercept suppression
fix(player): resolve pause drift using atomic SEEK_CLOSEST synchronization
docs(readme): update hardware verification receipts and screenshot showcase
test(rans): add bounds check for truncated bitstreams
```

---

## 5. Pull Request Checklist

Before submitting a pull request, ensure:

- [ ] `cmake --build build` succeeds without warnings or errors.
- [ ] `ctest` passes 100% of unit tests.
- [ ] Android application compiles cleanly with `javac --release 17` against `android.jar`.
- [ ] Any new feature or bug fix includes corresponding test coverage in `tests/`.
- [ ] Documentation (`README.md`, `ARCHITECTURE.md`, `VERIFICATION.md`) is updated if APIs or behavior changed.
