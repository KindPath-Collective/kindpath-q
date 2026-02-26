# Security Policy

kindpath-q is a desktop audio analysis application for KindPath Collective — a frequency field scientist and creative intelligence tool. Its codebase handles audio data and user authentication.

## Reporting a Vulnerability

**Do not open a public issue for security vulnerabilities.**

Email: security@kindpathcollective.org

We will acknowledge receipt within 48 hours and coordinate responsible disclosure.

## Scope

Security concerns relevant to this repository include:
- Memory safety issues in the C++ audio processing engine
- Authentication or credential handling vulnerabilities
- Plugin loading vulnerabilities (arbitrary code execution via malicious plugins)
- Supply chain issues in external submodules or CMake dependencies
- File system access outside expected audio file directories

## Supported Versions

| Branch | Supported |
|--------|-----------|
| `main` | ✅ Active |

## Submodule Integrity

This repository uses Git submodules (see `.gitmodules`). Ensure submodule commits are pinned to verified versions. Never update submodule pointers to unreviewed commits.
