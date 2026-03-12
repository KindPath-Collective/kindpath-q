#pragma once

// HarmonicAnalyser.h — Pitch class, key, and harmonic tension analysis.
//
// The harmonic layer reveals the creator's relationship with tonal convention.
// A piece can sit perfectly in key (formula) or make choices that are
// internally consistent but unconventional (authentic deviation).
// The difference is what this module is trying to measure.
//
// Uses the Krumhansl-Schmuckler key-finding algorithm (1990).
// Pure C++ — no JUCE dependency.

#include "AnalysisResult.h"
#include <array>
#include <vector>

namespace KindPath {

class HarmonicAnalyser {
public:
    void prepare(double sampleRate);

    // [AUDIO THREAD SAFE]
    void pushSamples(const float* samples, int numSamples);

    HarmonicResult getResult() const;
    HarmonicResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;

    // Accumulated chroma for rolling key estimate.
    std::array<float, 12> chromaAccumulator {};
    int chromaFrameCount = 0;

    HarmonicResult latestResult;

    // Krumhansl-Schmuckler key profiles (from probe-tone experiments, 1990).
    // Major: well-established tonal hierarchy C major → C# major etc.
    static constexpr std::array<float, 12> majorProfile = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    // Minor: Krumhansl's minor profile.
    static constexpr std::array<float, 12> minorProfile = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    // Dissonant pitch-class pairs relative to key root.
    // Used to estimate tension ratio. Semitone intervals: m2(1), M7(11), TT(6).
    static constexpr std::array<int, 4> tensionIntervals = { 1, 6, 10, 11 };

    // Chroma from a block of audio samples using octave-folded magnitude spectrum.
    std::array<float, 12> computeChroma(const float* samples, int numSamples) const;

    // Krumhansl-Schmuckler key estimation.
    std::pair<int, bool> estimateKey(const std::array<float, 12>& chroma) const;

    // Pearson correlation of chroma against a key profile.
    float correlate(const std::array<float, 12>& chroma,
                    const std::array<float, 12>& profile) const;

    float computeTonalityStrength(const std::array<float, 12>& chroma,
                                   int keyIdx, bool major) const;
    float computeTensionRatio    (const std::array<float, 12>& chroma,
                                   int keyIdx) const;
    float computeHarmonicComplexity(const std::array<float, 12>& chroma) const;
    float estimateTuningOffset   (const float* samples, int numSamples) const;
};

} // namespace KindPath
