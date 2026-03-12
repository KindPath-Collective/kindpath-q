# KindPath Q — AGENTS.md

## Session Init Protocol

Before reading code or making changes, run:
```bash
cat ~/.kindpath/HANDOVER.md
python3 ~/.kindpath/kp_memory.py dump --domain gotcha
python3 ~/.kindpath/kp_memory.py dump
```

---

## Mission

KindPath Q is a real-time audio analysis plugin and standalone application built on the JUCE framework. It runs inside a DAW (Logic, Ableton, Pro Tools) as an AU or VST3 plugin, or standalone on macOS. It is a frequency field scientist and creative authenticity instrument — not a mixing tool.

## Operational Commands

- **Bootstrap (first time)**: `./scripts/bootstrap_macos.sh`
- **Build**: `./scripts/build_macos.sh`
- **Standalone output**: `build/KindPathQStandalone_artefacts/Debug/KindPath\ Q.app`
- **AU plugin**: `build/KindPathQ_artefacts/Debug/AU/KindPath\ Q.component`
- **VST3 plugin**: `build/KindPathQ_artefacts/Debug/VST3/KindPath\ Q.vst3`

## Architecture

```
core/           — Pure C++ DSP and analysis. ZERO JUCE dependency. Testable standalone.
shared/         — JUCE-aware code shared by plugin and standalone
plugins/        — Plugin wrapper: audio processor, editor, parameter management
apps/standalone — Standalone wrapper: main component, window management
ui/             — JUCE UI components
```

**Layer law: `core/` must never include JUCE headers.**

## Build Order

1. `core/analysis/AnalysisResult.h` — data structures
2. `core/analysis/SpectralAnalyser` — FFT pipeline
3. `core/analysis/DynamicAnalyser` — RMS and dynamics
4. `core/analysis/HarmonicAnalyser` — pitch and chroma
5. `core/analysis/TemporalAnalyser` — beat and groove
6. `core/analysis/SegmentBuffer` — quarter accumulation
7. `core/analysis/AnalysisEngine` — orchestrator, LSII, psychosomatic
8. Core tests — verify before touching JUCE
9. `core/fingerprints/` — era, technique, instrument detection
10. `shared/KindPathProcessor` — JUCE wrapper
11. `ui/` — UI components
12. `plugins/kindpath-q/` — plugin target
13. `apps/standalone/` — standalone target
14. `shared/seedbank/` — seedbank integration

## Key Metrics

- **LSII** (Late-Song Inversion Index): 0–1 score of Q4 divergence from Q1-Q3 trajectory
- **Valence**: −1.0 (negative) to 1.0 (positive) emotional tone
- **Arousal**: 0.0 (sedating) to 1.0 (activating)
- **Creative Residue**: What remains after known genre signatures subtracted
- **Groove Deviation**: Human timing variation in ms (<3ms = over-quantised)
- **Dynamic Range**: <6dB = hypercompressed, >12dB = healthy

## Rules

- Read `CLAUDE.md` for full architecture specification before coding
- `core/` = real-time safe: no allocation, no locks, no system calls in audio thread
- C++17, JUCE CMake build, macOS primary target
- Every class header explains conceptual purpose, not just technical role
- Comments explain WHY, not WHAT
- UI: dark (#0D0D0D), single amber accent (#F5A623) for high-divergence signals
- No FFTW — use `juce::dsp::FFT` unless there is a documented reason

## Security Mandates

- No API keys or secrets in source
- No network calls — this tool is offline by design
- All seedbank records stored in user application data directory, not repo
