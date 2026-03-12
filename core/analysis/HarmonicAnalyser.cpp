#include "HarmonicAnalyser.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace KindPath {

void HarmonicAnalyser::prepare(double sr)
{
    sampleRate = sr;
    chromaAccumulator = {};
    chromaFrameCount  = 0;
    latestResult      = {};
}

void HarmonicAnalyser::pushSamples(const float* samples, int numSamples)
{
    auto chroma = computeChroma(samples, numSamples);

    // Accumulate chroma for rolling key estimate (exponential moving average).
    const float alpha = 0.1f; // How much each new frame contributes.
    for (int i = 0; i < 12; ++i)
        chromaAccumulator[i] = chromaAccumulator[i] * (1.f - alpha) + chroma[i] * alpha;
    ++chromaFrameCount;

    auto [keyIdx, isMaj] = estimateKey(chromaAccumulator);

    latestResult.keyIndex         = keyIdx;
    latestResult.isMajor          = isMaj;
    latestResult.chroma           = chromaAccumulator;
    latestResult.keyConfidence    = computeTonalityStrength(chromaAccumulator, keyIdx, isMaj);
    latestResult.tonalityStrength = latestResult.keyConfidence;
    latestResult.tensionRatio     = computeTensionRatio(chromaAccumulator, keyIdx);
    latestResult.harmonicComplexity = computeHarmonicComplexity(chromaAccumulator);
    latestResult.tuningOffsetCents  = estimateTuningOffset(samples, numSamples);

    // Resolution index: negative tension trend = resolving.
    // Requires history — set to 0 for now; the AnalysisEngine computes cross-segment.
    latestResult.resolutionIndex = 0.f;
}

HarmonicResult HarmonicAnalyser::getResult() const
{
    return latestResult;
}

HarmonicResult HarmonicAnalyser::analyseBuffer(const float* samples, int numSamples)
{
    auto chroma = computeChroma(samples, numSamples);
    auto [keyIdx, isMaj] = estimateKey(chroma);

    HarmonicResult r;
    r.keyIndex          = keyIdx;
    r.isMajor           = isMaj;
    r.chroma            = chroma;
    r.keyConfidence     = computeTonalityStrength(chroma, keyIdx, isMaj);
    r.tonalityStrength  = r.keyConfidence;
    r.tensionRatio      = computeTensionRatio(chroma, keyIdx);
    r.harmonicComplexity = computeHarmonicComplexity(chroma);
    r.tuningOffsetCents = estimateTuningOffset(samples, numSamples);
    return r;
}

// ── Private ───────────────────────────────────────────────────────────────────

std::array<float, 12> HarmonicAnalyser::computeChroma(const float* samples, int numSamples) const
{
    // Constant-Q approximation: map each FFT bin's frequency to a pitch class.
    // Uses a simple 2048-point FFT computed inline.
    constexpr int N = 2048;
    std::vector<std::complex<float>> buf(N, { 0.f, 0.f });
    const int n = std::min(numSamples, N);
    for (int i = 0; i < n; ++i)
        buf[i] = { samples[i], 0.f };

    // In-place FFT.
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

    // Map magnitude bins to pitch classes.
    // A4 = 440 Hz. Pitch class = round(12 * log2(freq / C0)) % 12.
    // C0 ≈ 16.35 Hz.
    constexpr float C0 = 16.352f;
    std::array<float, 12> chroma {};
    const int bins = N / 2;
    const float freqPerBin = (float)sampleRate / N;

    for (int i = 2; i < bins; ++i) {  // Skip DC and near-DC.
        const float freq = i * freqPerBin;
        if (freq < 20.f || freq > 5000.f) continue;  // Human pitch range.

        const int pitchClass = (int)std::round(12.f * std::log2(freq / C0)) % 12;
        const int pc = ((pitchClass % 12) + 12) % 12;
        chroma[pc] += std::abs(buf[i]);
    }

    // Normalise.
    float maxVal = *std::max_element(chroma.begin(), chroma.end());
    if (maxVal > 1e-10f)
        for (auto& c : chroma) c /= maxVal;

    return chroma;
}

std::pair<int, bool> HarmonicAnalyser::estimateKey(const std::array<float, 12>& chroma) const
{
    float bestCorr  = -1e9f;
    int   bestKey   = 0;
    bool  bestMajor = true;

    for (int root = 0; root < 12; ++root) {
        // Rotate profiles to test each root.
        std::array<float, 12> rotatedMajor {}, rotatedMinor {};
        for (int i = 0; i < 12; ++i) {
            rotatedMajor[i] = majorProfile[(i + 12 - root) % 12];
            rotatedMinor[i] = minorProfile[(i + 12 - root) % 12];
        }
        const float majCorr = correlate(chroma, rotatedMajor);
        const float minCorr = correlate(chroma, rotatedMinor);

        if (majCorr > bestCorr) { bestCorr = majCorr; bestKey = root; bestMajor = true; }
        if (minCorr > bestCorr) { bestCorr = minCorr; bestKey = root; bestMajor = false; }
    }
    return { bestKey, bestMajor };
}

float HarmonicAnalyser::correlate(const std::array<float, 12>& a,
                                    const std::array<float, 12>& b) const
{
    // Pearson correlation coefficient.
    float meanA = 0.f, meanB = 0.f;
    for (int i = 0; i < 12; ++i) { meanA += a[i]; meanB += b[i]; }
    meanA /= 12.f; meanB /= 12.f;

    float num = 0.f, denA = 0.f, denB = 0.f;
    for (int i = 0; i < 12; ++i) {
        num  += (a[i] - meanA) * (b[i] - meanB);
        denA += (a[i] - meanA) * (a[i] - meanA);
        denB += (b[i] - meanB) * (b[i] - meanB);
    }
    const float denom = std::sqrt(denA * denB);
    return (denom > 1e-10f) ? num / denom : 0.f;
}

float HarmonicAnalyser::computeTonalityStrength(const std::array<float, 12>& chroma,
                                                  int keyIdx, bool major) const
{
    std::array<float, 12> rotated {};
    const auto& profile = major ? majorProfile : minorProfile;
    for (int i = 0; i < 12; ++i)
        rotated[i] = profile[(i + 12 - keyIdx) % 12];

    // Map correlation [-1,1] → [0,1].
    return (correlate(chroma, rotated) + 1.f) / 2.f;
}

float HarmonicAnalyser::computeTensionRatio(const std::array<float, 12>& chroma,
                                              int keyIdx) const
{
    // Proportion of energy at tension intervals (semitones 1, 6, 10, 11 from root).
    float tensionEnergy = 0.f, totalEnergy = 0.f;
    for (int i = 0; i < 12; ++i) totalEnergy += chroma[i];

    for (int interval : tensionIntervals)
        tensionEnergy += chroma[(keyIdx + interval) % 12];

    return (totalEnergy > 1e-10f) ? tensionEnergy / totalEnergy : 0.f;
}

float HarmonicAnalyser::computeHarmonicComplexity(const std::array<float, 12>& chroma) const
{
    // Shannon entropy of the chroma distribution.
    // High entropy = many pitch classes active = high complexity.
    float entropy = 0.f;
    for (float c : chroma) {
        if (c > 1e-10f) entropy -= c * std::log2(c);
    }
    // Normalise by log2(12) (max entropy for 12 pitch classes).
    return entropy / std::log2(12.f);
}

float HarmonicAnalyser::estimateTuningOffset(const float* samples, int numSamples) const
{
    // Simplified tuning offset: find the strongest partial in the 400–480 Hz range
    // (around A4 = 440 Hz) and measure its deviation.
    // Full YIN or pYIN would be more accurate — this gives a rough estimate.
    constexpr int N = 2048;
    if (numSamples < N) return 0.f;

    std::vector<std::complex<float>> buf(N);
    for (int i = 0; i < N; ++i) buf[i] = { samples[i], 0.f };

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

    const float freqPerBin = (float)sampleRate / N;
    const int   binLow     = (int)(400.f / freqPerBin);
    const int   binHigh    = (int)(480.f / freqPerBin);

    float peakMag = 0.f;
    int   peakBin = -1;
    for (int i = binLow; i <= binHigh && i < N / 2; ++i) {
        float mag = std::abs(buf[i]);
        if (mag > peakMag) { peakMag = mag; peakBin = i; }
    }
    if (peakBin < 0 || peakMag < 1e-6f) return 0.f;

    const float peakFreq  = peakBin * freqPerBin;
    const float semitones = 12.f * std::log2(peakFreq / 440.f);
    const float cents     = (semitones - std::round(semitones)) * 100.f;
    return cents;
}

} // namespace KindPath
