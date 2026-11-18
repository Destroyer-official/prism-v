# Security Policy

## Scope

Prism-V is a local media processing toolchain and Android application. The native core and the standalone Android application do not include telemetry trackers, analytics SDKs, or third-party advertising frameworks. All processing, transcoding, deduplication, and frame comparisons execute locally on device.

File operations in the native core use a two-phase transactional sequence with atomic commit and journaled rollback (`core/src/transaction.cpp`), guarding against data corruption from unexpected process termination.

## Supported Versions

Security updates and patches are actively maintained for the following versions:

| Version | Supported | Notes |
| --- | --- | --- |
| `v1.1.x` | Yes | Current stable production release series |
| `v1.0.x` | Yes | Legacy maintenance release series |
| `main` | Yes | Active development branch |
| `< 1.0` | No | Prototype pre-releases |

## Reporting a Vulnerability

We take the security of Prism-V seriously. If you discover a potential vulnerability in the native core, container parsers, Android application, or JNI bindings:

1. **Do not disclose publicly**: Do not create a public GitHub issue or discuss the issue on public channels before a fix is released.
2. **Submit via GitHub Security Advisories**: Use the "Report a vulnerability" tab under the repository's Security menu.
3. **Alternative Contact**: If GitHub Security Advisories is unavailable, contact the project maintainer via the email listed on their GitHub profile.

### Information to Include

Please provide as much information as possible to facilitate rapid triage and remediation:

- **Component**: The affected module (e.g. `core/container_pvloss`, `core/remux`, `android_app/DuoComparisonPlayerView`, JNI bridge).
- **Environment**: Operating system, architecture (ARM64 / x86_64), compiler, Android OS version, and device model.
- **Reproduction**: Step-by-step instructions and minimal reproduction sample or test file.
- **Impact**: Potential consequences (e.g., buffer overflow, out-of-bounds read, denial of service, container desync).

Confirmed vulnerabilities will be patched promptly on `main` and backported to active release branches with security advisories.
