#pragma once

// DynamicAnalyser.h — Energy envelope analysis.
//
// The dynamic picture reveals production choices that affect how music
// lands in the human body. Compression removes the body's natural response
// to energy change. Dynamic range tells you whether the creator chose
// expression or efficiency.
//
// Pure C++ — no JUCE dependency.

#include "AnalysisResult.h"
#include <deque>

namespace KindPath {

class DynamicAnalyser {
public:
    void prepare(double sampleRate, int hopSize = 512);

    // [AUDIO THREAD SAFE]
    void pushSamples(const float* samples, int numSamples);

    DynamicResult getResult() const;
    DynamicResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    int    hopSize    = 512;

    std::deque<float> rmsHistory;
    static constexpr int RMS_HISTORY_FRAMES = 100;

    DynamicResult latestResult;

    float computeRms          (const float* samples, int n) const;
    float computePeakDb       (const float* samples, int n) const;
    float computeCrestFactor  (const float* samples, int n) const;
    float computeDynamicRange (const std::deque<float>& rmsHistory) const;
    float computeLufs         (const float* samples, int n) const;
    float computeOnsetDensity (const float* samples, int n) const;
    bool  detectClipping      (const float* samples, int n, float threshold = 0.99f) const;
    float computeClippingPct  (const float* samples, int n, float threshold = 0.99f) const;
    float computeTrend        (const std::deque<float>& values) const;
};

} // namespace KindPath
