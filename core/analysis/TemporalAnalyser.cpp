#include "TemporalAnalyser.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace KindPath {

void TemporalAnalyser::prepare(double sr, int hSize)
{
    sampleRate    = sr;
    hopSize       = hSize;
    samplePosition = 0.0;
    beatTimestamps.clear();
    onsetStrengths.clear();
    prevMagnitudes.assign(hSize / 2 + 1, 0.f);
    latestResult = {};
}

void TemporalAnalyser::pushSamples(const float* samples, int numSamples)
{
    const double frameDurationSec = numSamples / sampleRate;
    const double frameCentreTimeSec = (samplePosition + numSamples / 2.0) / sampleRate;
    samplePosition += numSamples;

    const float onset = computeOnsetStrength(samples, numSamples);
    onsetStrengths.push_back(onset);
    if ((int)onsetStrengths.size() > MAX_ONSET_HISTORY)
        onsetStrengths.pop_front();

    detectBeat(onset, frameCentreTimeSec);

    if (beatTimestamps.size() >= 4) {
        latestResult.tempoBpm         = estimateTempo(beatTimestamps);
        latestResult.tempoConfidence  = estimateTempoConfidence(beatTimestamps);
        latestResult.beatRegularity   = computeBeatRegularity(beatTimestamps);
        latestResult.grooveDeviationMs = computeGrooveDeviation(beatTimestamps);
        latestResult.meterEstimate    = estimateMeter(beatTimestamps, latestResult.tempoBpm);
    }
    latestResult.rhythmicEntropy = computeRhythmicEntropy(onsetStrengths);
    if (latestResult.tempoBpm > 0.f)
        latestResult.syncopationIndex = computeSyncopation(beatTimestamps, latestResult.tempoBpm);
    (void)frameDurationSec;
}

TemporalResult TemporalAnalyser::getResult() const
{
    return latestResult;
}

TemporalResult TemporalAnalyser::analyseBuffer(const float* samples, int numSamples)
{
    // For a single buffer, compute a minimal set of features.
    // BPM estimation requires multiple beats so these will be approximate.
    const float onset = computeOnsetStrength(samples, numSamples);

    TemporalResult r;
    r.rhythmicEntropy = computeRhythmicEntropy({ onset });

    // Simple autocorrelation-based BPM from the buffer itself.
    // Suitable for segment analysis where the buffer is a full quarter.
    const int n = numSamples;
    if (n > (int)sampleRate) {
        // Compute onset strength envelope at 10ms hop.
        const int localHop = std::max(1, (int)(sampleRate * 0.01));
        std::vector<float> envelope;
        for (int i = 0; i + localHop <= n; i += localHop) {
            float rms = 0.f;
            for (int j = i; j < i + localHop; ++j) rms += samples[j] * samples[j];
            envelope.push_back(std::sqrt(rms / localHop));
        }

        // Autocorrelation of envelope to find dominant period.
        const int envN = (int)envelope.size();
        const int lagMin = std::max(1, (int)(sampleRate / 200.0 / localHop)); // 200 BPM max.
        const int lagMax = (int)(sampleRate / 60.0  / localHop); // 60 BPM min.

        float bestCorr = -1.f;
        int   bestLag  = lagMin;
        for (int lag = lagMin; lag <= std::min(lagMax, envN / 2); ++lag) {
            float corr = 0.f;
            for (int i = 0; i + lag < envN; ++i)
                corr += envelope[i] * envelope[i + lag];
            if (corr > bestCorr) { bestCorr = corr; bestLag = lag; }
        }

        const float periodSec = bestLag * localHop / (float)sampleRate;
        r.tempoBpm = (periodSec > 0.f) ? 60.f / periodSec : 0.f;
        r.tempoConfidence = std::min(bestCorr / (envN * 0.5f + 1e-6f), 1.f);
    }

    return r;
}

// ── Private ───────────────────────────────────────────────────────────────────

float TemporalAnalyser::computeOnsetStrength(const float* samples, int numSamples)
{
    // Spectral flux onset detector.
    // Computes magnitude spectrum and returns half-wave rectified flux vs previous frame.
    constexpr int N = 512;
    const int bins  = N / 2;
    std::vector<std::complex<float>> buf(N, { 0.f, 0.f });
    const int n = std::min(numSamples, N);
    for (int i = 0; i < n; ++i) buf[i] = { samples[i], 0.f };

    // Mini FFT.
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        float angle = -2.f * (float)M_PI / len;
        std::complex<float> wlen { std::cos(angle), std::sin(angle) };
        for (int i = 0; i < N; i += len) {
            std::complex<float> w { 1.f, 0.f };
            for (int j = 0; j < len / 2; ++j) {
                auto u = buf[i + j];
                auto v = buf[i + j + len / 2] * w;
                buf[i + j]         = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if ((int)prevMagnitudes.size() < bins) prevMagnitudes.assign(bins, 0.f);

    float flux = 0.f;
    for (int i = 0; i < bins; ++i) {
        const float mag  = std::abs(buf[i]);
        const float diff = mag - prevMagnitudes[i];
        if (diff > 0.f) flux += diff;
        prevMagnitudes[i] = mag;
    }
    return flux;
}

void TemporalAnalyser::detectBeat(float onsetStrength, double currentTimeSec)
{
    // Simple threshold-based beat detector.
    // A beat is detected when onset strength exceeds 1.5× the recent mean
    // and enough time has passed since the last beat (enforces minimum IOI).
    if (onsetStrengths.empty()) return;

    // Compute local mean onset.
    const int windowSize = std::min(16, (int)onsetStrengths.size());
    float mean = 0.f;
    auto it = onsetStrengths.end() - windowSize;
    for (; it != onsetStrengths.end(); ++it) mean += *it;
    mean /= windowSize;

    if (onsetStrength <= mean * 1.5f || mean < 1e-6f) return;

    // Enforce minimum IOI ≈ 0.25 s (240 BPM max).
    if (!beatTimestamps.empty() &&
        (currentTimeSec - beatTimestamps.back()) < 0.25)
        return;

    beatTimestamps.push_back(currentTimeSec);
    if ((int)beatTimestamps.size() > MAX_BEAT_HISTORY)
        beatTimestamps.pop_front();
}

float TemporalAnalyser::computeGrooveDeviation(const std::deque<double>& beats) const
{
    if (beats.size() < 3) return 0.f;

    // Inter-onset intervals (IOIs) in seconds.
    std::vector<float> iois;
    iois.reserve(beats.size() - 1);
    for (size_t i = 1; i < beats.size(); ++i)
        iois.push_back((float)(beats[i] - beats[i - 1]));

    // Ideal IOI = median (robust to outlier beats).
    std::vector<float> sorted = iois;
    std::sort(sorted.begin(), sorted.end());
    const float medianIoi = sorted[sorted.size() / 2];

    if (medianIoi < 1e-6f) return 0.f;

    // Deviations from the ideal grid in milliseconds.
    float sumSq = 0.f;
    for (float ioi : iois) {
        // Quantise IOI to nearest multiple of median (handles double-time etc.)
        const float multiple = std::round(ioi / medianIoi);
        if (multiple < 1.f) continue;
        const float devSec = (ioi - medianIoi * multiple);
        sumSq += devSec * devSec;
    }
    return std::sqrt(sumSq / iois.size()) * 1000.f;  // Convert to ms.
}

float TemporalAnalyser::estimateTempo(const std::deque<double>& beats) const
{
    if (beats.size() < 2) return 0.f;
    std::vector<float> iois;
    for (size_t i = 1; i < beats.size(); ++i)
        iois.push_back((float)(beats[i] - beats[i-1]));

    std::vector<float> sorted = iois;
    std::sort(sorted.begin(), sorted.end());
    const float median = sorted[sorted.size() / 2];

    return (median > 1e-6f) ? 60.f / median : 0.f;
}

float TemporalAnalyser::estimateTempoConfidence(const std::deque<double>& beats) const
{
    if (beats.size() < 4) return 0.f;
    const float tempo = estimateTempo(beats);
    if (tempo <= 0.f) return 0.f;

    std::vector<float> iois;
    for (size_t i = 1; i < beats.size(); ++i)
        iois.push_back((float)(beats[i] - beats[i-1]));

    float mean = 0.f;
    for (float v : iois) mean += v;
    mean /= iois.size();

    float variance = 0.f;
    for (float v : iois) variance += (v - mean) * (v - mean);
    variance /= iois.size();

    const float cv = (mean > 1e-6f) ? std::sqrt(variance) / mean : 1.f;
    return std::max(0.f, 1.f - cv * 3.f);  // Low CV → high confidence.
}

float TemporalAnalyser::computeRhythmicEntropy(const std::deque<float>& onsets) const
{
    if (onsets.empty()) return 0.f;

    // Normalised histogram-based entropy.
    const float maxOnset = *std::max_element(onsets.begin(), onsets.end());
    if (maxOnset < 1e-10f) return 0.f;

    constexpr int bins = 8;
    std::array<int, bins> histogram {};
    for (float v : onsets) {
        int bin = std::min((int)(v / maxOnset * bins), bins - 1);
        ++histogram[bin];
    }

    float entropy = 0.f;
    const float n = (float)onsets.size();
    for (int h : histogram) {
        if (h > 0) {
            float p = h / n;
            entropy -= p * std::log2(p);
        }
    }
    return entropy / std::log2(bins);  // Normalised to [0,1].
}

float TemporalAnalyser::computeSyncopation(const std::deque<double>& beats,
                                             float tempoBpm) const
{
    if (beats.empty() || tempoBpm <= 0.f) return 0.f;

    const float beatPeriod = 60.f / tempoBpm;  // seconds per beat.

    if (beats.empty()) return 0.f;
    const double gridStart = beats.front();

    int syncopated = 0;
    for (const double t : beats) {
        const float phase = std::fmod((float)(t - gridStart) / beatPeriod, 1.f);
        // Syncopated if more than 0.15 beats away from a full beat.
        if (phase > 0.15f && phase < 0.85f) ++syncopated;
    }
    return (float)syncopated / beats.size();
}

int TemporalAnalyser::estimateMeter(const std::deque<double>& beats, float tempoBpm) const
{
    if (beats.size() < 8 || tempoBpm <= 0.f) return 4;

    // Compare autocorrelation strength at 3× and 4× beat periods.
    const float beatPeriod = 60.f / tempoBpm;
    const double startTime = beats.front();

    auto corrAtMultiple = [&](float multiple) {
        const float lag      = beatPeriod * multiple;
        float       corr     = 0.f;
        int         count    = 0;
        for (size_t i = 0; i < beats.size(); ++i) {
            const double t    = beats[i] - startTime;
            const double tLag = t + lag;
            // Check if there is a beat near t + lag.
            for (const double b : beats) {
                if (std::abs((b - startTime) - tLag) < beatPeriod * 0.1f) {
                    corr += 1.f;
                    ++count;
                    break;
                }
            }
        }
        return count > 0 ? corr / count : 0.f;
    };

    return (corrAtMultiple(3.f) > corrAtMultiple(4.f)) ? 3 : 4;
}

float TemporalAnalyser::computeBeatRegularity(const std::deque<double>& beats) const
{
    if (beats.size() < 3) return 0.f;
    std::vector<float> iois;
    for (size_t i = 1; i < beats.size(); ++i)
        iois.push_back((float)(beats[i] - beats[i-1]));

    float mean = 0.f;
    for (float v : iois) mean += v;
    mean /= iois.size();

    if (mean < 1e-6f) return 0.f;
    float variance = 0.f;
    for (float v : iois) variance += (v - mean) * (v - mean);
    const float cv = std::sqrt(variance / iois.size()) / mean;

    return std::max(0.f, 1.f - cv);
}

} // namespace KindPath
