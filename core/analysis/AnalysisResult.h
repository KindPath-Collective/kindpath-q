#pragma once

// AnalysisResult.h — The shared language of the KindPath Q analysis engine.
//
// Every struct here is a data container only. No logic lives here.
// These structures carry the signal from the audio thread, through the
// analysis pipeline, to the UI and to the elder's reading.
//
// The separation matters: data structures in core/ can be included anywhere
// without pulling in JUCE or other dependencies.

#include <array>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace KindPath {

// ── Spectral ──────────────────────────────────────────────────────────────────
// What the frequency content of the audio reveals about brightness, movement,
// and tonal character. The spectral picture of the signal's skin.
struct SpectralResult {
    float centroidHz    = 0.f;  // Brightness: centre of mass of energy. High = bright/harsh.
    float centroidTrend = 0.f;  // +ve = brightening over time, -ve = darkening.
    float flux          = 0.f;  // Rate of spectral change. High = alive. Low = static/machined.
    float rolloff85Hz   = 0.f;  // Frequency containing 85% of energy. Proxy for tone body.
    float bandwidth     = 0.f;  // Spectral width. Narrow = clean/compressed. Wide = busy/complex.
    float flatness      = 0.f;  // 0 = pure tone, 1 = white noise. Texture indicator.
    float harmonicRatio = 0.f;  // Ratio of harmonic to percussive energy.
    std::array<float, 13> mfcc {};  // Mel-Frequency Cepstral Coefficients — timbral fingerprint.
};

// ── Dynamic ───────────────────────────────────────────────────────────────────
// What the energy envelope reveals about production choices and creative intent.
// Dynamic range is one of the most honest signals in the data.
struct DynamicResult {
    float rmsMean           = 0.f;  // Average energy. The body registers this as loudness.
    float rmsStd            = 0.f;  // Energy variation. Low = compressed. High = breathes.
    float rmsTrend          = 0.f;  // Building (+) or decaying (-) over the segment.
    float peakDb            = 0.f;  // Peak level in dBFS.
    float crestFactorDb     = 0.f;  // Peak-to-RMS ratio. <6dB = loudness war signature.
    float dynamicRangeDb    = 0.f;  // <6dB = hypercompressed. >12dB = healthy.
    float lufs              = -70.f; // Integrated perceived loudness (EBU R128 approximation).
    float onsetDensity      = 0.f;  // Events per second. High = busy. Low = spacious.
    bool  isClipping        = false; // Hard clipping detected.
    float clippingPct       = 0.f;  // Proportion of samples above threshold.
};

// ── Harmonic ──────────────────────────────────────────────────────────────────
// What the pitch content reveals about emotional grounding, tension, and
// the creator's relationship with harmonic convention.
struct HarmonicResult {
    int   keyIndex          = 0;    // 0=C, 1=C#, ... 11=B (estimated tonal centre).
    bool  isMajor           = true; // Major vs minor mode.
    float keyConfidence     = 0.f;  // How strongly the signal sits in this key.
    std::array<float, 12> chroma {}; // Energy per pitch class. The harmonic fingerprint.
    float tonalityStrength  = 0.f;  // How committed this is to a tonal centre (0–1).
    float tuningOffsetCents = 0.f;  // Deviation from A440. Positive = sharp.
    float tensionRatio      = 0.f;  // Proportion of time in dissonant harmonic states.
    float resolutionIndex   = 0.f;  // +ve = resolving, -ve = tension accumulating.
    float harmonicComplexity = 0.f; // Chord change density and variety.
};

// ── Temporal ──────────────────────────────────────────────────────────────────
// What the rhythm reveals about human presence, creative intention, and the
// difference between a grid and a groove.
struct TemporalResult {
    float tempoBpm          = 0.f;  // Estimated tempo.
    float tempoConfidence   = 0.f;  // How certain we are.
    float beatRegularity    = 0.f;  // 0=erratic, 1=metronomic. Low can be alive or broken.
    float grooveDeviationMs = 0.f;  // Human timing variance in ms. <3ms = over-quantised.
    float rhythmicEntropy   = 0.f;  // Unpredictability of onset placement.
    float syncopationIndex  = 0.f;  // Off-beat emphasis. A cultural and rhythmic signature.
    int   meterEstimate     = 4;    // 3 or 4. Waltz or march.
};

// ── Segment (one quarter of the piece) ───────────────────────────────────────
// A time-bounded slice of the total analysis. KindPath Q divides each piece
// into four quarters and analyses each separately. The arc across these four
// segments is where the meaning lives.
struct SegmentResult {
    int           quarterIndex = 0; // 0–3 (Q1–Q4). Q4 is where we look hardest.
    double        startTimeSec = 0.0;
    double        endTimeSec   = 0.0;
    SpectralResult  spectral;
    DynamicResult   dynamic;
    HarmonicResult  harmonic;
    TemporalResult  temporal;
};

// ── Late-Song Inversion Index ─────────────────────────────────────────────────
// The LSII is the most important number this engine produces.
//
// It measures how much the final quarter (Q4) diverges from the trajectory
// established in the first three quarters (Q1–Q3). A high LSII score means
// that something changed in the last section — the creator stepped outside
// the frame they built.
//
// This can be a protest, an involuntary confession, a production compromise,
// or a deliberate artistic choice. The score names the divergence; the elder
// reading interprets what it might mean.
struct LateInversionResult {
    float lsii          = 0.f;  // 0.0–1.0 overall inversion score.
    float spectralDelta = 0.f;  // Brightness divergence Q4 vs Q1–Q3 baseline.
    float dynamicDelta  = 0.f;  // Energy divergence.
    float harmonicDelta = 0.f;  // Tension divergence.
    float temporalDelta = 0.f;  // Groove divergence.

    enum class Flag { None, Low, Moderate, High, Extreme };
    Flag flag = Flag::None;

    std::string direction;      // e.g. "darker, quieter, more tense"
    std::string dominantAxis;   // Which feature shows the greatest divergence.

    static std::string flagToString(Flag f) {
        switch (f) {
            case Flag::None:     return "None";
            case Flag::Low:      return "Low";
            case Flag::Moderate: return "Moderate";
            case Flag::High:     return "High";
            case Flag::Extreme:  return "Extreme";
            default:             return "Unknown";
        }
    }
};

// ── Psychosomatic Profile ─────────────────────────────────────────────────────
// What this audio is doing to a human body.
// Not neuroscience claiming to be definitive — a rigorous attempt to name
// what the data suggests about the pre-linguistic emotional transmission
// embedded in the work.
struct PsychosomaticResult {
    float valence           = 0.f;  // -1.0 (negative) to 1.0 (positive) emotional tone.
    float arousal           = 0.f;  // 0.0 (sedating) to 1.0 (activating).
    float coherence         = 0.f;  // Internal creative consistency — deliberate authorship.
    float authenticityIndex = 0.f;  // Authentic deviation from genre/era convention.
    float creativityResidual = 0.f; // What remains after known genre signatures subtracted.
    float manufacturingScore = 0.f; // How much is engineered delivery mechanism.

    bool  primingDetected   = false; // Stage 1: psychosomatic entrainment markers present.
    bool  prestigeAttached  = false; // Stage 2: false prestige signals detected.
    float tagRisk           = 0.f;   // Stage 3: identity capture risk 0–1.
};

// ── Full Analysis Result ──────────────────────────────────────────────────────
// The complete picture. This is what gets passed to the UI, the seedbank,
// and the elder reading generator.
struct FullAnalysisResult {
    std::array<SegmentResult, 4> quarters;
    LateInversionResult          lsii;
    PsychosomaticResult          psychosomatic;

    bool isComplete = false;  // True once all four quarters have been analysed.

    // Convenience accessors
    const SegmentResult& q1() const { return quarters[0]; }
    const SegmentResult& q2() const { return quarters[1]; }
    const SegmentResult& q3() const { return quarters[2]; }
    const SegmentResult& q4() const { return quarters[3]; }

    // The elder reading — a plain-language synthesis of what this piece is doing.
    // Called from the UI's ElderPanel. Generates from the analysis data.
    std::string generateElderReading() const;
};

// ── Implementation of generateElderReading ───────────────────────────────────
// Inline in the header so it compiles without a separate translation unit.
// The elder does not judge. It reads. The creator interprets.
inline std::string FullAnalysisResult::generateElderReading() const {
    if (!isComplete) return "Analysis in progress.";

    std::string reading;

    // ── LSII interpretation ─────────────────────────────────────────────────
    if (lsii.lsii >= 0.6f) {
        reading += "The final section changes. ";
        if (!lsii.direction.empty())
            reading += lsii.direction + ". ";
        reading += "Whatever was being communicated in the last quarter differs "
                   "from the frame the piece was built in. Worth listening to again "
                   "with that in mind. ";
    } else if (lsii.lsii >= 0.4f) {
        reading += "There is a shift in the final quarter — not dramatic, but present. ";
        if (!lsii.dominantAxis.empty())
            reading += "The " + lsii.dominantAxis + " changes direction. ";
    } else if (lsii.lsii >= 0.2f) {
        reading += "The piece stays largely consistent throughout. "
                   "Minor variation in the final section, within normal range. ";
    } else {
        reading += "Consistent throughout. The four quarters hold a steady character. ";
    }

    // ── Dynamic interpretation ──────────────────────────────────────────────
    const auto& dyn = q1().dynamic;
    if (dyn.crestFactorDb < 6.0f || dyn.dynamicRangeDb < 6.0f) {
        reading += "Heavy limiting throughout — the production chose loudness "
                   "over dynamic range. ";
    } else if (dyn.dynamicRangeDb > 12.0f) {
        reading += "Preserved dynamics — the production allowed the energy to move "
                   "rather than holding it flat. ";
    }

    // ── Groove interpretation ────────────────────────────────────────────────
    const auto& temp = q1().temporal;
    if (temp.grooveDeviationMs < 3.0f && temp.grooveDeviationMs > 0.01f) {
        reading += "Timing is grid-precise — quantised to the sample. "
                   "The body notices the absence of human variation. ";
    } else if (temp.grooveDeviationMs > 15.0f) {
        reading += "The timing is alive — human variation is preserved. "
                   "The performer is present in the groove. ";
    }

    // ── Authenticity summary ─────────────────────────────────────────────────
    if (psychosomatic.authenticityIndex > 0.7f) {
        reading += "Strong authentic markers throughout. "
                   "The harmonic choices are internally consistent in ways that "
                   "suggest deliberate authorship rather than formula. ";
    } else if (psychosomatic.manufacturingScore > 0.7f) {
        reading += "The production fingerprint is commercial — "
                   "the piece sits efficiently within its genre expectations. ";
    }

    if (reading.empty())
        reading = "Analysis complete. No strong signals detected in either direction.";

    return reading;
}

} // namespace KindPath
