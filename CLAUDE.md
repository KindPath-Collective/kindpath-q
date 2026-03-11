# CLAUDE.md — KindPath Q

## Session Init Protocol

Before reading code or making changes, run:
```bash
cat ~/.kindpath/HANDOVER.md
python3 ~/.kindpath/kp_memory.py dump --domain gotcha
python3 ~/.kindpath/kp_memory.py dump
```

---

## Instructions for Claude Code

Read this file completely before touching a single line of code.
Every decision you make must serve the mission described here,
not just the technical specification.

---

## What This Is

KindPath Q is a real-time audio analysis plugin and standalone application
built on the JUCE framework. It runs inside a producer's DAW (Logic, Ableton,
Pro Tools) as an AU or VST3 plugin, or standalone on macOS.

Its purpose is not to be a mixing tool. It is a **frequency field scientist**
and **creative authenticity instrument**. It reads what music is doing to a
human body, separates authentic creative signal from engineered conditioning,
and surfaces that knowledge in real time — inside the exact environment where
music is made and consumed.

This is the native C++ implementation of the KindPath analyser. Everything
runs in the audio thread or a dedicated analysis thread. No Python, no
external servers. The analysis happens here, now, in the signal path.

**The mission in one sentence:** Make the mechanism visible, in real time,
so that creators and listeners can make genuinely informed choices about
what they make and what they let in.

---

## Repository Structure

```
kindpath-q/
├── CLAUDE.md                   ← You are here. Read this first. Always.
├── CMakeLists.txt              ← JUCE CMake build system
├── .github/                    ← CI/CD workflows
├── apps/
│   └── standalone/             ← Standalone macOS application target
├── assets/                     ← Icons, images, fonts
├── core/                       ← DSP and analysis engine (pure C++, no JUCE)
├── docs/                       ← Architecture and dev documentation
├── external/                   ← Third-party libraries (submodules)
├── plugins/
│   └── kindpath-q/             ← JUCE plugin target (AU + VST3)
├── prompts/                    ← Analysis prompt templates and text
├── scripts/                    ← Build and utility scripts
├── shared/                     ← Code shared between plugin and standalone
└── ui/                         ← JUCE UI components
```

**The separation that matters:**
- `core/` — pure C++ DSP and analysis. Zero JUCE dependency. Testable in isolation.
- `shared/` — JUCE-aware code shared by plugin and standalone
- `plugins/kindpath-q/` — plugin wrapper: audio processor, editor, parameter management
- `apps/standalone/` — standalone wrapper: main component, window management
- `ui/` — all UI components, owned by neither plugin nor standalone

**Do not mix these layers.** `core/` must never include JUCE headers.
The analysis engine must be portable and testable without a DAW.

---

## Build System

JUCE CMake build. macOS is the primary target.

```bash
# Bootstrap (first time only)
./scripts/bootstrap_macos.sh

# Build everything
./scripts/build_macos.sh

# Outputs
build/KindPathQStandalone_artefacts/Debug/KindPath\ Q.app   # Standalone
build/KindPathQ_artefacts/Debug/AU/KindPath\ Q.component    # AU plugin
build/KindPathQ_artefacts/Debug/VST3/KindPath\ Q.vst3       # VST3 plugin
```

When adding new source files, update `CMakeLists.txt` accordingly.
All `core/` files go into the core static library target.
All `ui/` files go into the shared UI target.

---

## What You Are Building

Build these components **in order**. Complete each one with tests before
moving to the next. The architecture flows from DSP engine → analysis →
UI → integration.

---

## COMPONENT 1: DSP Analysis Engine
**Location:** `core/analysis/`
**Priority:** CRITICAL — everything depends on this

### What it does

Real-time spectral, dynamic, harmonic, and temporal feature extraction.
This runs on audio buffers as they arrive. It must be fast enough to
process at 44100Hz without dropouts. All analysis is non-blocking.

### File structure

```
core/analysis/
├── AnalysisEngine.h / .cpp     ← Main engine, owns all analysers
├── SpectralAnalyser.h / .cpp   ← FFT-based spectral features
├── DynamicAnalyser.h / .cpp    ← RMS, crest factor, dynamic range
├── HarmonicAnalyser.h / .cpp   ← Pitch, chroma, key, tension
├── TemporalAnalyser.h / .cpp   ← BPM, groove deviation, onset detection
├── SegmentBuffer.h / .cpp      ← Ring buffer for segment accumulation
└── AnalysisResult.h            ← All result structs (data only, no logic)
```

### AnalysisResult.h — define all result structs here

```cpp
#pragma once
#include <array>
#include <string>
#include <vector>

namespace KindPath {

// ── Spectral ──────────────────────────────────────────────────────────────
struct SpectralResult {
    float centroidHz        = 0.f;  // Brightness centre of mass
    float centroidTrend     = 0.f;  // +ve = brightening, -ve = darkening
    float flux              = 0.f;  // Rate of spectral change (alive = high)
    float rolloff85Hz       = 0.f;  // Frequency containing 85% of energy
    float bandwidth         = 0.f;  // Spectral width (narrow = compressed)
    float flatness          = 0.f;  // 0=tonal, 1=noise-like
    float harmonicRatio     = 0.f;  // Harmonic vs percussive energy
    std::array<float, 13> mfcc {}; // Timbral fingerprint
};

// ── Dynamic ───────────────────────────────────────────────────────────────
struct DynamicResult {
    float rmsMean           = 0.f;
    float rmsStd            = 0.f;
    float rmsTrend          = 0.f;  // Building or decaying?
    float peakDb            = 0.f;
    float crestFactorDb     = 0.f;  // Compression signature. <6dB = loudness war.
    float dynamicRangeDb    = 0.f;  // <6dB = hypercompressed. >12dB = healthy.
    float lufs              = 0.f;  // Integrated loudness (perceptual)
    float onsetDensity      = 0.f;  // Events per second
    bool  isClipping        = false;
    float clippingPct       = 0.f;
};

// ── Harmonic ──────────────────────────────────────────────────────────────
struct HarmonicResult {
    int   keyIndex          = 0;    // 0=C, 1=C#, ... 11=B
    bool  isMajor           = true;
    float keyConfidence     = 0.f;
    std::array<float, 12> chroma {}; // Energy per pitch class
    float tonalityStrength  = 0.f;  // How strongly in-key (0-1)
    float tuningOffsetCents = 0.f;  // Deviation from A440
    float tensionRatio      = 0.f;  // Proportion in dissonant states
    float resolutionIndex   = 0.f;  // +ve = resolving, -ve = increasing tension
    float harmonicComplexity = 0.f; // Chord change density
};

// ── Temporal ──────────────────────────────────────────────────────────────
struct TemporalResult {
    float tempoBpm          = 0.f;
    float tempoConfidence   = 0.f;
    float beatRegularity    = 0.f;  // 0=erratic, 1=metronomic
    float grooveDeviationMs = 0.f;  // Human timing variation. <3ms = over-quantised.
    float rhythmicEntropy   = 0.f;  // Unpredictability of onset pattern
    float syncopationIndex  = 0.f;  // Off-beat emphasis
    int   meterEstimate     = 4;    // 3 or 4
};

// ── Segment (one quarter of the song) ────────────────────────────────────
struct SegmentResult {
    int           quarterIndex = 0; // 0-3 (Q1-Q4)
    double        startTimeSec = 0.0;
    double        endTimeSec   = 0.0;
    SpectralResult  spectral;
    DynamicResult   dynamic;
    HarmonicResult  harmonic;
    TemporalResult  temporal;
};

// ── Late-Song Inversion Index ─────────────────────────────────────────────
// The protest detection layer.
// Measures how much Q4 diverges from the Q1-Q3 trajectory.
// High LSII = the creator stepped outside the frame they built.
struct LateInversionResult {
    float lsii              = 0.f;  // 0.0-1.0 overall inversion score
    float spectralDelta     = 0.f;  // Brightness divergence
    float dynamicDelta      = 0.f;  // Energy divergence
    float harmonicDelta     = 0.f;  // Tension divergence
    float temporalDelta     = 0.f;  // Groove divergence

    // Flag levels
    enum class Flag { None, Low, Moderate, High, Extreme };
    Flag  flag              = Flag::None;
    
    std::string direction;          // e.g. "darker, quieter, more tense"
    std::string dominantAxis;       // Which feature shows greatest divergence
};

// ── Psychosomatic Profile ─────────────────────────────────────────────────
// What this audio is doing to a human body.
struct PsychosomaticResult {
    float valence           = 0.f;  // -1.0 (negative) to 1.0 (positive)
    float arousal           = 0.f;  // 0.0 (sedating) to 1.0 (activating)
    float coherence         = 0.f;  // Internal creative consistency
    float authenticityIndex = 0.f;  // Authentic deviation from convention
    float creativityResidual = 0.f; // What remains after known signatures removed
    float manufacturingScore = 0.f; // How much is engineered delivery
    
    // Conditioning markers
    bool  primingDetected   = false;
    bool  prestigeAttached  = false;
    float tagRisk           = 0.f;  // Identity capture risk 0-1
};

// ── Full Analysis Result ──────────────────────────────────────────────────
struct FullAnalysisResult {
    std::array<SegmentResult, 4> quarters;
    LateInversionResult          lsii;
    PsychosomaticResult          psychosomatic;
    
    // Convenience accessors
    const SegmentResult& q1() const { return quarters[0]; }
    const SegmentResult& q2() const { return quarters[1]; }
    const SegmentResult& q3() const { return quarters[2]; }
    const SegmentResult& q4() const { return quarters[3]; }
};

} // namespace KindPath
```

### AnalysisEngine.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include "SegmentBuffer.h"
#include <functional>
#include <memory>
#include <atomic>

namespace KindPath {

class SpectralAnalyser;
class DynamicAnalyser;
class HarmonicAnalyser;
class TemporalAnalyser;

class AnalysisEngine {
public:
    // Callback signature: called when a complete analysis result is ready
    using ResultCallback = std::function<void(const FullAnalysisResult&)>;

    AnalysisEngine();
    ~AnalysisEngine();

    // Call once before processing begins
    void prepare(double sampleRate, int blockSize);
    
    // Call from the audio thread with each incoming buffer
    // This must be real-time safe: no allocation, no locks, no system calls
    void processBlock(const float* const* channelData,
                      int numChannels,
                      int numSamples);
    
    // Set callback for when a full analysis is complete
    // Called from the analysis thread, not the audio thread
    void setResultCallback(ResultCallback callback);
    
    // Real-time readable state (atomic, safe from UI thread)
    float getCurrentValence()    const { return currentValence.load(); }
    float getCurrentArousal()    const { return currentArousal.load(); }
    float getCurrentLsii()       const { return currentLsii.load(); }
    float getCurrentDynamicRange() const { return currentDynamicRange.load(); }
    float getCurrentGrooveDeviation() const { return currentGrooveDeviation.load(); }
    
    // Get the most recent full result (copies — safe from UI thread)
    FullAnalysisResult getLatestResult() const;
    
    void reset();

private:
    double sampleRate  = 44100.0;
    int    blockSize   = 512;
    
    std::unique_ptr<SpectralAnalyser>  spectral;
    std::unique_ptr<DynamicAnalyser>   dynamic;
    std::unique_ptr<HarmonicAnalyser>  harmonic;
    std::unique_ptr<TemporalAnalyser>  temporal;
    std::unique_ptr<SegmentBuffer>     segmentBuffer;
    
    // Real-time readable atomics for UI
    std::atomic<float> currentValence       {0.f};
    std::atomic<float> currentArousal       {0.f};
    std::atomic<float> currentLsii          {0.f};
    std::atomic<float> currentDynamicRange  {0.f};
    std::atomic<float> currentGrooveDeviation {0.f};
    
    ResultCallback resultCallback;
    
    void runFullAnalysis();
    LateInversionResult computeLsii(const std::array<SegmentResult, 4>&);
    PsychosomaticResult computePsychosomatic(const std::array<SegmentResult, 4>&,
                                              const LateInversionResult&);
};

} // namespace KindPath
```

### SpectralAnalyser.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include <vector>
#include <complex>

namespace KindPath {

class SpectralAnalyser {
public:
    void prepare(double sampleRate, int fftSize = 2048, int hopSize = 512);
    
    // Feed audio samples. Returns true when a new frame is available.
    bool pushSamples(const float* samples, int numSamples);
    
    // Get the latest spectral result
    SpectralResult getResult() const;
    
    // Compute a single-frame result from a buffer (for segment analysis)
    SpectralResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    int    fftSize    = 2048;
    int    hopSize    = 512;
    
    std::vector<float>              window;         // Hann window
    std::vector<float>              inputBuffer;
    std::vector<float>              magnitudeSpectrum;
    std::vector<std::complex<float>> fftBuffer;
    
    SpectralResult latestResult;
    
    void computeFrame(const float* windowed);
    float computeCentroid(const float* mag, int bins) const;
    float computeRolloff(const float* mag, int bins, float percent = 0.85f) const;
    float computeFlux(const float* mag, const float* prevMag, int bins) const;
    float computeFlatness(const float* mag, int bins) const;
    std::array<float, 13> computeMFCC(const float* mag, int bins) const;
    
    std::vector<float> prevMagnitude;
    
    // Hann window precomputation
    static std::vector<float> makeHannWindow(int size);
};

} // namespace KindPath
```

### DynamicAnalyser.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include <deque>

namespace KindPath {

class DynamicAnalyser {
public:
    void prepare(double sampleRate, int hopSize = 512);
    
    void pushSamples(const float* samples, int numSamples);
    DynamicResult getResult() const;
    DynamicResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    int    hopSize    = 512;
    
    // Rolling window of RMS values for trend and range computation
    std::deque<float> rmsHistory;
    static constexpr int RMS_HISTORY_FRAMES = 100;
    
    DynamicResult latestResult;
    
    float computeRms(const float* samples, int n) const;
    float computeLufs(const float* samples, int n, double sr) const;
    float computeCrestFactor(const float* samples, int n) const;
    bool  detectClipping(const float* samples, int n, float threshold = 0.99f) const;
    float computeTrend(const std::deque<float>& values) const;
};

} // namespace KindPath
```

### HarmonicAnalyser.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include <array>
#include <vector>

namespace KindPath {

class HarmonicAnalyser {
public:
    void prepare(double sampleRate);
    
    void pushSamples(const float* samples, int numSamples);
    HarmonicResult getResult() const;
    HarmonicResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    
    HarmonicResult latestResult;
    
    // Krumhansl-Schmuckler key profiles
    static constexpr std::array<float, 12> MAJOR_PROFILE = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    static constexpr std::array<float, 12> MINOR_PROFILE = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };
    
    std::array<float, 12> computeChroma(const float* samples, int numSamples);
    std::pair<int,float>  estimateKey(const std::array<float, 12>& chroma);
    bool  isMajor(const std::array<float, 12>& chroma, int keyIdx);
    float computeTonalityStrength(const std::array<float, 12>& chroma,
                                   int keyIdx, bool major);
    float computeTensionRatio(const std::array<float, 12>& chroma,
                               int keyIdx, bool major);
    float estimateTuning(const float* samples, int numSamples);
    
    // Accumulate chroma over time for rolling estimate
    std::array<float, 12> chromaAccumulator {};
    int chromaFrameCount = 0;
};

} // namespace KindPath
```

### TemporalAnalyser.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include <deque>
#include <vector>

namespace KindPath {

class TemporalAnalyser {
public:
    void prepare(double sampleRate, int hopSize = 512);
    
    void pushSamples(const float* samples, int numSamples);
    TemporalResult getResult() const;
    TemporalResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    int    hopSize    = 512;
    
    TemporalResult latestResult;
    
    std::deque<double>  beatTimes;      // Recent beat timestamps in seconds
    std::deque<float>   onsetStrengths; // Onset strength history
    double              samplePosition = 0.0;
    
    static constexpr int MAX_BEAT_HISTORY = 32;
    
    std::vector<float>  prevOnsetBuffer;
    
    float computeOnsetStrength(const float* samples, int numSamples);
    void  detectBeat(float onsetStrength, double currentTimeSec);
    float computeGrooveDeviation(const std::deque<double>& beats);
    float computeRhythmicEntropy(const std::deque<float>& onsets);
    float computeSyncopation(const std::deque<double>& beats,
                              const std::deque<float>& onsets);
    float estimateTempo(const std::deque<double>& beats);
};

} // namespace KindPath
```

### SegmentBuffer.h

```cpp
#pragma once
#include "AnalysisResult.h"
#include <vector>
#include <atomic>
#include <mutex>

namespace KindPath {

// Accumulates audio for segment (quarter) analysis.
// A segment is complete when the expected duration has elapsed.
// The buffer is designed for use from the audio thread with minimal locking.

class SegmentBuffer {
public:
    // quarterDurationSecs: approximate duration of each quarter
    // (computed from total track length if known, default 60s for live analysis)
    void prepare(double sampleRate, double quarterDurationSecs = 60.0);
    
    // Push samples from the audio thread. Returns true when a quarter is complete.
    bool pushSamples(const float* samples, int numSamples);
    
    // Call after pushSamples returns true to retrieve the completed quarter
    // Returns the quarter index (0-3) and fills the buffer
    int  retrieveCompletedQuarter(std::vector<float>& buffer);
    
    // Set known track duration (enables accurate quarter boundaries)
    void setTrackDuration(double durationSecs);
    
    // Reset for a new track
    void reset();
    
    int  getCurrentQuarter()    const { return currentQuarter.load(); }
    bool isAllQuartersComplete() const { return quartersComplete.load() >= 4; }

private:
    double sampleRate           = 44100.0;
    double quarterDurationSecs  = 60.0;
    
    std::vector<float>  accumBuffer;
    int                 samplesPerQuarter = 0;
    int                 samplesInCurrent  = 0;
    
    std::atomic<int>    currentQuarter    {0};
    std::atomic<int>    quartersComplete  {0};
    
    std::vector<float>  completedBuffer;
    std::mutex          completedMutex;
    int                 completedQuarterIndex = -1;
    bool                completedReady        = false;
};

} // namespace KindPath
```

### LSII Computation (implement in AnalysisEngine.cpp)

```cpp
LateInversionResult AnalysisEngine::computeLsii(
    const std::array<SegmentResult, 4>& quarters)
{
    // Baseline: average of Q1, Q2, Q3
    auto baseline = [&](auto getter) {
        return (getter(quarters[0]) + getter(quarters[1]) + getter(quarters[2])) / 3.f;
    };
    
    // Normalised delta: tanh((q4 - baseline) / (|baseline| + eps))
    auto normDelta = [](float q4val, float baseVal) {
        return std::tanh((q4val - baseVal) / (std::abs(baseVal) + 1e-6f));
    };
    
    LateInversionResult r;
    
    // Brightness divergence
    float baseCentroid = baseline([](auto& q){ return q.spectral.centroidHz; });
    r.spectralDelta = normDelta(quarters[3].spectral.centroidHz, baseCentroid);
    
    // Energy divergence
    float baseRms = baseline([](auto& q){ return q.dynamic.rmsMean; });
    r.dynamicDelta = normDelta(quarters[3].dynamic.rmsMean, baseRms);
    
    // Tension divergence
    float baseTension = baseline([](auto& q){ return q.harmonic.tensionRatio; });
    r.harmonicDelta = normDelta(quarters[3].harmonic.tensionRatio, baseTension);
    
    // Groove divergence
    float baseGroove = baseline([](auto& q){ return q.temporal.grooveDeviationMs; });
    r.temporalDelta = normDelta(quarters[3].temporal.grooveDeviationMs, baseGroove);
    
    // LSII aggregate
    r.lsii = (std::abs(r.spectralDelta) + std::abs(r.dynamicDelta) +
              std::abs(r.harmonicDelta) + std::abs(r.temporalDelta)) / 4.f;
    
    // Flag level
    if      (r.lsii < 0.2f) r.flag = LateInversionResult::Flag::None;
    else if (r.lsii < 0.4f) r.flag = LateInversionResult::Flag::Low;
    else if (r.lsii < 0.6f) r.flag = LateInversionResult::Flag::Moderate;
    else if (r.lsii < 0.8f) r.flag = LateInversionResult::Flag::High;
    else                     r.flag = LateInversionResult::Flag::Extreme;
    
    // Dominant axis
    std::array<std::pair<float,std::string>, 4> axes {{
        {std::abs(r.spectralDelta), "spectral brightness"},
        {std::abs(r.dynamicDelta),  "dynamic energy"},
        {std::abs(r.harmonicDelta), "harmonic tension"},
        {std::abs(r.temporalDelta), "temporal groove"},
    }};
    auto max = std::max_element(axes.begin(), axes.end());
    r.dominantAxis = max->second;
    
    return r;
}
```

### Implementation notes for the DSP engine

**Real-time safety is non-negotiable.** The `processBlock()` method and anything
it calls must be real-time safe:
- No `new`, `delete`, or `malloc`
- No mutexes in the hot path (use atomics)
- No file I/O
- No system calls
- No exceptions

Use a lock-free ring buffer (JUCE's `AbstractFifo` or a custom implementation)
to pass audio data from the audio thread to the analysis thread.

**FFT implementation:** Use JUCE's `juce::dsp::FFT` (already available as a
JUCE dependency). Do not bring in FFTW unless the existing build system already
includes it. Check `external/` first.

**Analysis thread:** Spin up a `std::thread` in `AnalysisEngine::prepare()`.
The analysis thread reads from the ring buffer, runs the heavy analysis,
and fires the callback. Join it in the destructor.

**Segment timing:** For live input, default to 60-second quarters (4-minute
track assumption). If the host provides transport position and track length,
use those. Check JUCE `AudioPlayHead` for this information — it's available
in the plugin context.

### Tests (core/tests/)

```
core/tests/
├── test_spectral.cpp       ← Verify centroid, flux, MFCC on known signals
├── test_dynamic.cpp        ← Verify RMS, crest factor, clipping detection
├── test_harmonic.cpp       ← Verify key detection on synthetic chords
├── test_temporal.cpp       ← Verify BPM detection, groove deviation
├── test_lsii.cpp           ← Verify LSII computation on known divergent data
└── test_segment_buffer.cpp ← Verify quarter accumulation and retrieval
```

Use a minimal test framework (Catch2 is already standard in JUCE projects —
check if it's in `external/`). Tests must build and pass without a DAW.

---

## COMPONENT 2: Fingerprint Engine
**Location:** `core/fingerprints/`
**Priority:** HIGH

### What it does

Identifies what tools and techniques were used to make the audio.
No mystification — technique is public knowledge that belongs to everyone.
This reads what is already in the signal.

```
core/fingerprints/
├── FingerprintEngine.h / .cpp  ← Orchestrator
├── EraDetector.h / .cpp        ← Production decade matching
├── TechniqueDetector.h / .cpp  ← Compression, quantisation, reverb markers
├── InstrumentDetector.h / .cpp ← Instrument family presence
└── FingerprintResult.h         ← Result structs
```

### FingerprintResult.h

```cpp
#pragma once
#include <string>
#include <vector>

namespace KindPath {

struct FingerprintMatch {
    std::string category;       // "era", "technique", "instrument"
    std::string name;           // What was matched
    float       confidence;     // 0-1
    std::vector<std::string> evidence;
    std::string description;    // Human-readable
    std::string psychosomaticNote; // What this does to a listener
};

struct FingerprintResult {
    std::vector<FingerprintMatch> eraMatches;
    std::vector<FingerprintMatch> techniqueMatches;
    std::vector<FingerprintMatch> instrumentMatches;
    
    std::string productionContext;      // Top era summary
    std::vector<std::string> authenticityMarkers;
    std::vector<std::string> manufacturingMarkers;
};

} // namespace KindPath
```

### Era detection logic

Match crest factor, dynamic range, and high-frequency rolloff against
the following era profiles. Each era has characteristic ranges:

| Era       | Crest Factor (dB) | Dynamic Range (dB) | Notes |
|-----------|-------------------|--------------------|-------|
| Pre-1970  | 12-25             | 15-30              | Tape noise, limited HF |
| 1970s     | 10-20             | 12-25              | Analogue warmth |
| 1980s     | 8-16              | 8-18               | Gated reverb, digital sheen |
| 1990s     | 6-14              | 6-14               | Early digital, loudness creep |
| 2000s     | 2-8               | 2-8                | Loudness war peak |
| 2010s     | 4-10              | 5-12               | Streaming pullback |
| 2020s     | 5-12              | 6-14               | Spatial audio, lo-fi reaction |

### Technique markers

```cpp
struct TechniqueRule {
    std::string name;
    std::string description;
    std::string psychosomaticNote;
    
    // Conditions (NaN = not checked)
    float crestFactorMax    = std::numeric_limits<float>::quiet_NaN();
    float crestFactorMin    = std::numeric_limits<float>::quiet_NaN();
    float dynamicRangeMax   = std::numeric_limits<float>::quiet_NaN();
    float dynamicRangeMin   = std::numeric_limits<float>::quiet_NaN();
    float grooveDeviationMax = std::numeric_limits<float>::quiet_NaN();
    float grooveDeviationMin = std::numeric_limits<float>::quiet_NaN();
};

// Rules to implement:
// heavy_compression:   crestFactor < 6 || dynamicRange < 6
//   psychosomatic: "Hypercompression creates perceptual fatigue and 
//                   removes the body's natural response to dynamics."
//
// natural_dynamics:    crestFactor > 12 && dynamicRange > 12  
//   psychosomatic: "Preserved dynamics allow the body to breathe with
//                   the music. This is what music felt like before the
//                   loudness war."
//
// quantised_rhythm:    grooveDeviation < 3ms
//   psychosomatic: "Perfect quantisation removes human timing variation.
//                   The body registers the absence — it misses the groove."
//
// human_performance:   grooveDeviation > 15ms
//   psychosomatic: "Human micro-timing is a biological signature of
//                   authentic presence. The body recognises it."
```

---

## COMPONENT 3: JUCE Plugin Integration
**Location:** `plugins/kindpath-q/` and `shared/`
**Priority:** HIGH

### What it does

Wraps the core analysis engine in JUCE's AudioProcessor/AudioProcessorEditor
framework. Manages audio thread safety, parameter exposure, and host communication.

### shared/KindPathProcessor.h

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/analysis/AnalysisEngine.h"
#include "core/fingerprints/FingerprintEngine.h"

class KindPathProcessor : public juce::AudioProcessor {
public:
    KindPathProcessor();
    ~KindPathProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    const juce::String getName() const override { return "KindPath Q"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    
    // Real-time readable analysis state (for UI polling)
    float getValence()          const;
    float getArousal()          const;
    float getLsii()             const;
    float getDynamicRange()     const;
    float getGrooveDeviation()  const;
    
    // Get latest full result (copy for UI thread)
    KindPath::FullAnalysisResult getLatestResult() const;
    
    // Callback: called from analysis thread when result is ready
    std::function<void(const KindPath::FullAnalysisResult&)> onResultReady;

private:
    KindPath::AnalysisEngine   analysisEngine;
    KindPath::FingerprintEngine fingerprintEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KindPathProcessor)
};
```

---

## COMPONENT 4: UI
**Location:** `ui/`
**Priority:** HIGH — this is what the creator sees

### Philosophy

The UI must feel like a scientific instrument, not a music app.
It surfaces truth without judgment. It does not tell people what to do.
It shows what is. The creator decides what to do with the knowledge.

Design principles:
- Dark background (#0D0D0D or similar). This lives in a DAW.
- Single accent colour for high-divergence signals. Suggest amber (#F5A623).
- Typography: monospace for values, clean sans for labels. System fonts only.
- No decoration. The data is the design.
- Everything readable at plugin sizes (300×500px minimum)
- No animations that could cause UI thread performance issues

### UI structure

```
ui/
├── MainView.h / .cpp           ← Root component, owns all panels
├── LsiiPanel.h / .cpp          ← LSII score — the headline metric
├── TrajectoryPanel.h / .cpp    ← 4-quarter arc display
├── DynamicPanel.h / .cpp       ← Crest factor, dynamic range, compression status
├── HarmonicPanel.h / .cpp      ← Key, tension, tonality
├── TemporalPanel.h / .cpp      ← BPM, groove deviation, syncopation
├── FingerprintPanel.h / .cpp   ← Era, techniques, authenticity markers
├── ElderPanel.h / .cpp         ← The synthesis narrative in plain language
└── Colours.h                   ← Colour constants
```

### LsiiPanel — the most important display element

```cpp
// The LSII score is the headline.
// Display: large numeric score (0.000), flag level, direction text.
// Colour the score by flag level:
//   None    → neutral (white/grey)
//   Low     → neutral
//   Moderate → amber (#F5A623)
//   High    → amber, brighter
//   Extreme → amber, maximum brightness, subtle pulse

// Below the score, show the four delta axes as a small bar chart:
// Spectral / Dynamic / Harmonic / Temporal — each bar shows its delta
// Positive delta = bar to the right, negative = bar to the left
// This shows WHY the LSII is what it is, not just that it exists
```

### TrajectoryPanel — the 4-quarter arc

```cpp
// Four panels side by side, one per quarter, labelled Q1 Q2 Q3 Q4
// Each shows: valence (dot on vertical axis), energy (bar), tension (line)
// Q4 is subtly highlighted if LSII > 0.4 — it draws the eye to the shift
// No animation. Static display updated when new result arrives.
```

### ElderPanel — the synthetic elder's voice

```cpp
// Plain text, left-aligned, comfortable reading size
// Updated when a new result arrives
// The text is generated by a method in the analysis engine:
//   FullAnalysisResult::generateElderReading() const -> std::string
// 
// Example outputs (implement generateElderReading() in AnalysisResult.h/.cpp):
//
// Low LSII, natural dynamics:
// "Consistent throughout. Preserved dynamics — the production chose
//  listener experience over loudness."
//
// High LSII, final quarter darker:
// "The final quarter pulls back. Darker, quieter, more unresolved than
//  what came before. Something shifted here."
//
// Heavy compression, quantised:
// "Heavy limiting throughout. Timing is grid-precise. The production
//  fingerprint is commercial — efficiency over expression."
//
// The elder does not judge. It reads. The creator interprets.
```

### UI thread safety

The UI polls the processor using a `juce::Timer` at 30Hz.
Never block the audio thread waiting for UI.
All values read by the UI come from:
- Atomic floats in the processor (immediate, always safe)
- A copy of the latest FullAnalysisResult (obtained via getLatestResult())

Use `juce::MessageManager::callAsync()` to update UI from the analysis
thread callback if needed.

---

## COMPONENT 5: Standalone App
**Location:** `apps/standalone/`
**Priority:** MEDIUM

The standalone wraps the plugin processor in a windowed application.
It adds:
- File loading (drag-drop or open dialog) for offline analysis of audio files
- A larger display format than the plugin allows
- The Education Mode (see below)

### File analysis mode

When a file is loaded offline:
1. Decode the audio file using `juce::AudioFormatManager`
2. Pass the full audio through the analysis engine (non-real-time, as fast as possible)
3. The engine knows the track duration → can compute accurate quarter boundaries
4. Display the complete analysis including all four quarters + LSII

### Education Mode

A panel that explains what each metric means in plain language.
Not a tooltip — a persistent side panel that can be toggled.
Each section of the analysis has a "What does this mean?" button
that expands the education panel to explain that specific metric,
what it measures, what the possible readings indicate, and why it matters.

The explanations must be accessible to someone with no audio engineering
background. If a 16-year-old can't understand it, rewrite it.

---

## COMPONENT 6: Seedbank Integration
**Location:** `shared/seedbank/`
**Priority:** MEDIUM

The plugin can save analysis profiles to a local seedbank and compare
incoming audio against the seedbank baseline.

```
shared/seedbank/
├── SeedbankManager.h / .cpp    ← Read/write seedbank records
├── SeedbankRecord.h            ← Record struct
└── records/                    ← JSON files (user data dir, not repo)
```

Records are stored in the user's application data directory
(`juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
     .getChildFile("KindPath/seedbank/")`).

### Comparison mode

When a seedbank has records, the UI can display the current analysis
*relative to the seedbank baseline* — how does this piece compare to
the authenticated authentic work in the archive?

This is the mechanism that makes the reference point tangible:
not abstract descriptions of authenticity, but comparison against
documented examples of the real thing.

---

## CODE STANDARDS

### C++ version
C++17. JUCE's CMake setup handles this. Use modern C++: structured bindings,
`std::optional`, range-based for, `[[nodiscard]]`, `noexcept` where appropriate.

### Naming
- Classes: `PascalCase`
- Methods: `camelCase`
- Members: `camelCase` (no prefix/suffix conventions — keep it readable)
- Constants: `SCREAMING_SNAKE` or `constexpr` named in context
- Files: match class name exactly

### Comments
Comments explain *why*, not *what*. The code shows what.

```cpp
// BAD: multiply by sample rate to convert seconds to samples
int samples = seconds * sampleRate;

// GOOD: plugin APIs work in sample space; we receive time in seconds from host
int samples = static_cast<int>(positionSecs * sampleRate);
```

Every class header has a block comment explaining its conceptual purpose
within the larger mission — not just its technical role.

### Real-time safety
Any method that will be called from the audio thread must be marked:
```cpp
// [AUDIO THREAD SAFE] — no allocation, no locks, no blocking
void processBlock(const float* samples, int n) noexcept;
```

Any method that is NOT safe from the audio thread must be marked:
```cpp
// [NOT AUDIO THREAD SAFE] — call from UI or analysis thread only
void setConfiguration(const Config& c);
```

### Error handling
No exceptions in audio thread code. Use return values and optional.
In non-audio code, prefer `juce::Result` or `std::optional`.
Never silently swallow errors — at minimum, `DBG()` in debug builds.

---

## DEPENDENCIES

Check `external/` before adding anything. The existing build likely includes:
- JUCE (core framework) — do not add duplicate JUCE dependency
- Catch2 or similar test framework — check before adding

**Do NOT add without checking first:**
- FFTW — use `juce::dsp::FFT` unless there is a specific reason
- Eigen — only if matrix operations are genuinely needed
- Any network library — this tool is offline by design

If you need something not in `external/`, add it as a git submodule
and document the reason in `docs/DEPENDENCIES.md`.

---

## BUILD ORDER

Build in this sequence. Do not skip ahead — later components depend on earlier ones.

1. `core/analysis/AnalysisResult.h` — the data structures everything uses
2. `core/analysis/SpectralAnalyser` — FFT pipeline
3. `core/analysis/DynamicAnalyser` — RMS and dynamics
4. `core/analysis/HarmonicAnalyser` — pitch and chroma
5. `core/analysis/TemporalAnalyser` — beat and groove
6. `core/analysis/SegmentBuffer` — quarter accumulation
7. `core/analysis/AnalysisEngine` — orchestrator, LSII, psychosomatic
8. Core tests — verify the engine works before touching JUCE
9. `core/fingerprints/` — era, technique, instrument detection
10. `shared/KindPathProcessor` — JUCE wrapper
11. `ui/` — UI components
12. `plugins/kindpath-q/` — plugin target integration
13. `apps/standalone/` — standalone target
14. `shared/seedbank/` — seedbank integration

---

## DOCS TO READ BEFORE STARTING

Read these existing docs in the repo before writing any code:

- `docs/DEV_SETUP.md` — environment setup
- `docs/ARCHITECTURE.md` — existing architecture decisions
- Any existing headers in `core/` — understand what's already there

Do not contradict existing architectural decisions without a documented reason.
If you find a decision that conflicts with these instructions, flag it in a
comment and follow the existing decision until it can be reviewed.

---

## FINAL NOTE

This is not a mixing tool wearing analysis clothing. It is an act of
cultural restoration that runs inside a DAW.

Every variable name, every comment, every UI label is an opportunity to
carry the mission forward or dilute it. Choose to carry it.

The creator who uses this plugin should come away from the session knowing
something true about what they made and what made it. That is the only
metric that matters.

Build it like you are leaving it for someone who needs to understand
not just what it does, but why it exists.
