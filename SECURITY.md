# Security Policy

## Scope

Prism-V is a local media toolchain. The native core and the standalone Android
app do not include analytics or advertising SDKs. The Kotlin preview module
declares standard AndroidX and Media3 dependencies; review
`android/build.gradle.kts` before shipping it.

File replacement in the native core uses a stage/verify/commit sequence with a
rollback path (`core/src/transaction.cpp`). This protects against verification
failure and ordinary write errors. It has not been tested against power loss or
a process kill during the swap.

## Supported versions

The repository does not currently publish versioned releases. Security fixes
are applied to the `main` branch.

## Reporting a vulnerability

Report suspected vulnerabilities privately through GitHub Security Advisories
for this repository, or by contacting the maintainer listed on the repository
page. Do not open a public issue for an unfixed vulnerability.

Include:

- Steps to reproduce
- The affected component (`core/`, `android_app/`, `android/`, or `rust/`)
- Host platform and compiler, or Android version and device
- Expected and actual behavior
- A minimal input file if the issue depends on one

There is no guaranteed response time. Confirmed issues will be fixed on
`main` and noted in the commit message.
