# Security Policy

## 🔒 Commitment to Privacy & Zero-Telemetry

Prism-V is engineered from the ground up on an uncompromising privacy doctrine:
- **100% On-Device Execution**: Every frame transcode, neural entropy estimation, audio normalization, and container remuxing operation runs locally on silicon.
- **Zero Telemetry**: Prism-V contains **no tracking libraries, no analytics SDKs, and no external phoning-home beacons**.
- **Cryptographic File Transactions**: File modifications utilize atomic stage-verify-commit workflows with rollback protection to prevent corruption or data loss.

---

## 🛡️ Supported Versions

| Version | Supported          | Security Maintenance Status |
| ------- | ------------------ | --------------------------- |
| 1.0.x   | :white_check_mark: | Active Support              |
| < 1.0   | :x:                | Deprecated                  |

---

## 🚨 Reporting a Vulnerability

If you discover a security vulnerability or memory leak in Prism-V, please report it responsibly:

1. **Do not create a public GitHub issue**.
2. Email security inquiries directly to the project maintainers via the contact listed on GitHub or open a private GitHub Security Advisory.
3. Include detailed steps to reproduce, sample media files (if safe to share), architecture details (ARM64 vs x86_64), and expected vs actual behavior.
4. We acknowledge receipt within 48 hours and provide patches within standard disclosure timelines.
