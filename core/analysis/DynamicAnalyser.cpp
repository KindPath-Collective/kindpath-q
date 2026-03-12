#include "DynamicAnalyser.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace KindPath {

void DynamicAnalyser::prepare(double sr, int hSize)
{
    sampleRate = sr;
    hopSize    = hSize;
    rmsHistory.clear();
    latestResult = {};
}

void DynamicAnalyser::pushSamples(const float* samples, int numSamples)
{
    const float rms = computeRms(samples, numSamples);
    rmsHistory.push_back(rms);
    if ((int)rmsHistory.size() > RMS_HISTORY_FRAMES)
        rmsHistory.pop_front();

    latestResult.rmsMean        = computeRms(samples, numSamples);
    latestResult.peakDb         = computePeakDb(samples, numSamples);
    latestResult.crestFactorDb  = computeCrestFactor(samples, numSamples);
    latestResult.dynamicRangeDb = computeDynamicRange(rmsHistory);
    latestResult.lufs           = computeLufs(samples, numSamples);
    latestResult.onsetDensity   = computeOnsetDensity(samples, numSamples);
    latestResult.isClipping     = detectClipping(samples, numSamples);
    latestResult.clippingPct    = computeClippingPct(samples, numSamples);
    latestResult.rmsTrend       = computeTrend(rmsHistory);

    if (rmsHistory.size() > 1) {
        float sumSq = 0.f;
        for (float v : rmsHistory) sumSq += (v - latestResult.rmsMean) * (v - latestResult.rmsMean);
        latestResult.rmsStd = std::sqrt(sumSq / rmsHistory.size());
    }
}

DynamicResult DynamicAnalyser::getResult() const
{
    return latestResult;
}

DynamicResult DynamicAnalyser::analyseBuffer(const float* samples, int numSamples)
{
    DynamicResult r;
    r.rmsMean       = computeRms(samples, numSamples);
    r.peakDb        = computePeakDb(samples, numSamples);
    r.crestFactorDb = computeCrestFactor(samples, numSamples);
    r.lufs          = computeLufs(samples, numSamples);
    r.onsetDensity  = computeOnsetDensity(samples, numSamples);
    r.isClipping    = detectClipping(samples, numSamples);
    r.clippingPct   = computeClippingPct(samples, numSamples);
    // dynamicRangeDb needs history — single-buffer approximation using crest factor.
    r.dynamicRangeDb = r.crestFactorDb;
    r.rmsStd  = 0.f;
    r.rmsTrend = 0.f;
    return r;
}

// ── Private ───────────────────────────────────────────────────────────────────

float DynamicAnalyser::computeRms(const float* samples, int n) const
{
    if (n <= 0) return 0.f;
    float sumSq = 0.f;
    for (int i = 0; i < n; ++i) sumSq += samples[i] * samples[i];
    return std::sqrt(sumSq / n);
}

float DynamicAnalyser::computePeakDb(const float* samples, int n) const
{
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
        peak = std::max(peak, std::abs(samples[i]));
    return (peak > 1e-10f) ? 20.f * std::log10(peak) : -120.f;
}

float DynamicAnalyser::computeCrestFactor(const float* samples, int n) const
{
    if (n <= 0) return 0.f;
    const float rms  = computeRms(samples, n);
    const float peak = [&]() {
        float p = 0.f;
        for (int i = 0; i < n; ++i) p = std::max(p, std::abs(samples[i]));
        return p;
    }();
    if (rms < 1e-10f) return 0.f;
    return 20.f * std::log10(peak / rms);
}

float DynamicAnalyser::computeDynamicRange(const std::deque<float>& history) const
{
    if (history.size() < 2) return 0.f;
    const float maxRms = *std::max_element(history.begin(), history.end());
    const float minRms = *std::min_element(history.begin(), history.end());
    if (minRms < 1e-10f) return 60.f; // Silence present → large dynamic range.
    return 20.f * std::log10(maxRms / minRms);
}

float DynamicAnalyser::computeLufs(const float* samples, int n) const
{
    // Simplified integrated loudness: RMS with perceptual weighting approximation.
    // Full ITU-R BS.1770 requires a K-weighting filter; this is a linear approximation
    // suitable for showing relative trends rather than absolute compliance values.
    const float rms = computeRms(samples, n);
    return (rms > 1e-10f) ? -0.691f + 10.f * std::log10(rms * rms) : -70.f;
}

float DynamicAnalyser::computeOnsetDensity(const float* samples, int n) const
{
    if (n <= 0 || sampleRate <= 0.0) return 0.f;
    // Count samples where energy exceeds a local mean by 3× (simple onset detector).
    const float rms = computeRms(samples, n);
    const float threshold = rms * 3.f;
    int onsets = 0;
    bool inOnset = false;
    for (int i = 0; i < n; ++i) {
        if (std::abs(samples[i]) > threshold && !inOnset) {
            ++onsets;
            inOnset = true;
        } else if (std::abs(samples[i]) <= threshold) {
            inOnset = false;
        }
    }
    const float durationSec = (float)n / (float)sampleRate;
    return (durationSec > 0.f) ? onsets / durationSec : 0.f;
}

bool DynamicAnalyser::detectClipping(const float* samples, int n, float threshold) const
{
    for (int i = 0; i < n; ++i)
        if (std::abs(samples[i]) >= threshold) return true;
    return false;
}

float DynamicAnalyser::computeClippingPct(const float* samples, int n, float threshold) const
{
    if (n <= 0) return 0.f;
    int count = 0;
    for (int i = 0; i < n; ++i)
        if (std::abs(samples[i]) >= threshold) ++count;
    return (float)count / n;
}

float DynamicAnalyser::computeTrend(const std::deque<float>& values) const
{
    // Linear regression slope — positive means increasing, negative means decreasing.
    const int n = (int)values.size();
    if (n < 2) return 0.f;

    float sumX = 0.f, sumY = 0.f, sumXY = 0.f, sumX2 = 0.f;
    for (int i = 0; i < n; ++i) {
        sumX  += i;
        sumY  += values[i];
        sumXY += i * values[i];
        sumX2 += i * i;
    }
    const float denom = n * sumX2 - sumX * sumX;
    return (std::abs(denom) > 1e-10f) ? (n * sumXY - sumX * sumY) / denom : 0.f;
}

} // namespace KindPath
