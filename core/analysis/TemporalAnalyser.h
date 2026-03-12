#pragma once

// TemporalAnalyser.h — Beat, groove, and rhythmic character analysis.
//
// A grid is not a groove. The TemporalAnalyser measures what separates them:
// the deviation, entropy, and syncopation that human performance leaves in
// the timing record. The body registers the absence of these things as flatness.
//
// Pure C++ — no JUCE dependency.

#include "AnalysisResult.h"
#include <deque>
#include <vector>

namespace KindPath {

class TemporalAnalyser {
public:
    void prepare(double sampleRate, int hopSize = 512);

    // [AUDIO THREAD SAFE]
    void pushSamples(const float* samples, int numSamples);

    TemporalResult getResult() const;
    TemporalResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate    = 44100.0;
    int    hopSize       = 512;
    double samplePosition = 0.0;  // Tracks absolute position in samples.

    static constexpr int MAX_BEAT_HISTORY = 48;   // Enough for ~12 bars at 120 BPM.
    static constexpr int MAX_ONSET_HISTORY = 256;

    std::deque<double> beatTimestamps;   // Absolute time (seconds) of detected beats.
    std::deque<float>  onsetStrengths;   // Onset strength at each hop.
    std::vector<float> prevMagnitudes;   // Previous frame magnitudes for flux.

    TemporalResult latestResult;

    // Onset strength = half-wave rectified spectral flux.
    float computeOnsetStrength(const float* samples, int numSamples);

    // Peak-picking for beat tracking (simple threshold + suppression window).
    void  detectBeat(float onsetStrength, double currentTimeSec);

    // Groove deviation: std deviation of IOIs (inter-onset intervals) in ms.
    float computeGrooveDeviation(const std::deque<double>& beats) const;

    // Tempo from IOI median.
    float estimateTempo(const std::deque<double>& beats) const;
    float estimateTempoConfidence(const std::deque<double>& beats) const;

    // Rhythmic entropy: how unpredictable is the onset pattern?
    float computeRhythmicEntropy(const std::deque<float>& onsets) const;

    // Syncopation: fraction of strong onsets that fall off the beat grid.
    float computeSyncopation(const std::deque<double>& beats,
                              float tempoBpm) const;

    // Meter estimation (3 or 4): autocorrelation ratio at 3x vs 4x beat period.
    int estimateMeter(const std::deque<double>& beats, float tempoBpm) const;

    // Beat regularity: 1 - normalised std of IOI / mean IOI.
    float computeBeatRegularity(const std::deque<double>& beats) const;
};

} // namespace KindPath
