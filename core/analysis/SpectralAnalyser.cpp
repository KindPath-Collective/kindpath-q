#include "SpectralAnalyser.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace KindPath {

void SpectralAnalyser::prepare(double sr, int fSize, int hSize)
{
    sampleRate = sr;
    fftSize    = fSize;
    hopSize    = hSize;

    // Precompute Hann window.
    hannWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        hannWindow[i] = 0.5f * (1.f - std::cos(2.f * (float)M_PI * i / (fftSize - 1)));

    inputBuffer.assign(fftSize, 0.f);
    magnitudeSpectrum.assign(fftSize / 2, 0.f);
    prevMagnitudeSpectrum.assign(fftSize / 2, 0.f);
    fftBuffer.resize(fftSize);

    latestResult = {};
    frameReady   = false;
}

bool SpectralAnalyser::pushSamples(const float* samples, int numSamples)
{
    frameReady = false;
    // Shift existing buffer left, append new samples.
    for (int i = 0; i < numSamples && i < fftSize; ++i) {
        // Rolling window: shift by hopSize when we've collected enough new samples.
        // Simplified: treat each call as appending; compute frame when we have fftSize samples.
        inputBuffer.push_back(samples[i]);
        if ((int)inputBuffer.size() >= fftSize) {
            computeFrame();
            // Advance by hopSize.
            inputBuffer.erase(inputBuffer.begin(),
                              inputBuffer.begin() + hopSize);
            frameReady = true;
        }
    }
    return frameReady;
}

SpectralResult SpectralAnalyser::getResult() const
{
    return latestResult;
}

SpectralResult SpectralAnalyser::analyseBuffer(const float* samples, int numSamples)
{
    // Use up to fftSize samples from the buffer.
    const int n = std::min(numSamples, fftSize);

    // Apply window.
    fftBuffer.resize(fftSize);
    for (int i = 0; i < fftSize; ++i) {
        float s = (i < n) ? samples[i] : 0.f;
        fftBuffer[i] = { s * hannWindow[i], 0.f };
    }

    fftInPlace(fftBuffer);

    const int bins = fftSize / 2;
    std::vector<float> mag(bins);
    for (int i = 0; i < bins; ++i)
        mag[i] = std::abs(fftBuffer[i]);

    SpectralResult r;
    r.centroidHz    = computeCentroid(mag.data(), bins);
    r.rolloff85Hz   = computeRolloff(mag.data(), bins);
    r.flux          = computeFlux(mag.data(), prevMagnitudeSpectrum.data(), bins);
    r.flatness      = computeFlatness(mag.data(), bins);
    r.bandwidth     = computeBandwidth(mag.data(), bins, r.centroidHz);
    r.harmonicRatio = computeHarmonicRatio(mag.data(), bins);
    r.mfcc          = computeMFCC(mag.data(), bins);
    // centroidTrend requires history — left at 0 for single-buffer analysis.

    prevMagnitudeSpectrum.assign(mag.begin(), mag.end());
    return r;
}

// ── Private ───────────────────────────────────────────────────────────────────

void SpectralAnalyser::computeFrame()
{
    const int n    = fftSize;
    const int bins = n / 2;

    fftBuffer.resize(n);
    for (int i = 0; i < n; ++i)
        fftBuffer[i] = { inputBuffer[i] * hannWindow[i], 0.f };

    fftInPlace(fftBuffer);

    for (int i = 0; i < bins; ++i)
        magnitudeSpectrum[i] = std::abs(fftBuffer[i]);

    const float prevCentroid = latestResult.centroidHz;

    latestResult.centroidHz    = computeCentroid(magnitudeSpectrum.data(), bins);
    latestResult.rolloff85Hz   = computeRolloff(magnitudeSpectrum.data(), bins);
    latestResult.flux          = computeFlux(magnitudeSpectrum.data(), prevMagnitudeSpectrum.data(), bins);
    latestResult.flatness      = computeFlatness(magnitudeSpectrum.data(), bins);
    latestResult.bandwidth     = computeBandwidth(magnitudeSpectrum.data(), bins, latestResult.centroidHz);
    latestResult.harmonicRatio = computeHarmonicRatio(magnitudeSpectrum.data(), bins);
    latestResult.mfcc          = computeMFCC(magnitudeSpectrum.data(), bins);
    // centroidTrend: +ve if getting brighter.
    latestResult.centroidTrend = latestResult.centroidHz - prevCentroid;

    prevMagnitudeSpectrum = magnitudeSpectrum;
}

float SpectralAnalyser::computeCentroid(const float* mag, int bins) const
{
    float weightedSum = 0.f, totalEnergy = 0.f;
    for (int i = 0; i < bins; ++i) {
        const float freq = i * (float)sampleRate / fftSize;
        weightedSum  += freq * mag[i];
        totalEnergy  += mag[i];
    }
    return totalEnergy > 1e-10f ? weightedSum / totalEnergy : 0.f;
}

float SpectralAnalyser::computeRolloff(const float* mag, int bins, float percent) const
{
    float totalEnergy = 0.f;
    for (int i = 0; i < bins; ++i) totalEnergy += mag[i];

    const float threshold = totalEnergy * percent;
    float cumEnergy = 0.f;
    for (int i = 0; i < bins; ++i) {
        cumEnergy += mag[i];
        if (cumEnergy >= threshold)
            return i * (float)sampleRate / fftSize;
    }
    return (float)sampleRate / 2.f;
}

float SpectralAnalyser::computeFlux(const float* mag, const float* prevMag, int bins) const
{
    // Half-wave rectified spectral flux — only increases contribute.
    float flux = 0.f;
    for (int i = 0; i < bins; ++i) {
        const float diff = mag[i] - prevMag[i];
        if (diff > 0.f) flux += diff;
    }
    return flux;
}

float SpectralAnalyser::computeFlatness(const float* mag, int bins) const
{
    float logSum = 0.f, arithmeticSum = 0.f;
    int   validBins = 0;
    for (int i = 1; i < bins; ++i) {
        if (mag[i] > 1e-10f) {
            logSum       += std::log(mag[i]);
            arithmeticSum += mag[i];
            ++validBins;
        }
    }
    if (validBins == 0 || arithmeticSum < 1e-10f) return 0.f;
    const float geometricMean  = std::exp(logSum / validBins);
    const float arithmeticMean = arithmeticSum / validBins;
    return geometricMean / arithmeticMean;
}

float SpectralAnalyser::computeBandwidth(const float* mag, int bins, float centroidHz) const
{
    float weightedSum = 0.f, totalEnergy = 0.f;
    for (int i = 0; i < bins; ++i) {
        const float freq = i * (float)sampleRate / fftSize;
        const float diff = freq - centroidHz;
        weightedSum  += diff * diff * mag[i];
        totalEnergy  += mag[i];
    }
    return totalEnergy > 1e-10f ? std::sqrt(weightedSum / totalEnergy) : 0.f;
}

float SpectralAnalyser::computeHarmonicRatio(const float* mag, int bins) const
{
    // Simple harmonic/percussive ratio: energy in narrowband peaks vs total.
    // Peaks defined as local maxima with neighbours 20% lower.
    float harmonicEnergy = 0.f, totalEnergy = 0.f;
    for (int i = 1; i < bins - 1; ++i) {
        totalEnergy += mag[i];
        if (mag[i] > mag[i-1] * 1.2f && mag[i] > mag[i+1] * 1.2f)
            harmonicEnergy += mag[i];
    }
    return totalEnergy > 1e-10f ? harmonicEnergy / totalEnergy : 0.f;
}

std::array<float, 13> SpectralAnalyser::computeMFCC(const float* mag, int bins) const
{
    constexpr int numFilters = 40;

    const float melMin = hzToMel(80.f);
    const float melMax = hzToMel((float)sampleRate / 2.f);
    const float melStep = (melMax - melMin) / (numFilters + 1);

    // Compute mel filterbank energies.
    std::array<float, numFilters> filterEnergies {};
    for (int m = 0; m < numFilters; ++m) {
        const float melLow  = melMin + m * melStep;
        const float melMid  = melMin + (m + 1) * melStep;
        const float melHigh = melMin + (m + 2) * melStep;

        // Convert mel centres to bin indices.
        const int binLow  = std::max(0,        (int)(melToHz(melLow)  / sampleRate * 2 * bins));
        const int binMid  = std::min(bins - 1, (int)(melToHz(melMid)  / sampleRate * 2 * bins));
        const int binHigh = std::min(bins - 1, (int)(melToHz(melHigh) / sampleRate * 2 * bins));

        float energy = 0.f;
        for (int b = binLow; b <= binHigh; ++b) {
            float weight;
            if (b <= binMid)
                weight = (binMid > binLow) ? (float)(b - binLow) / (binMid - binLow) : 1.f;
            else
                weight = (binHigh > binMid) ? (float)(binHigh - b) / (binHigh - binMid) : 0.f;
            energy += weight * mag[b];
        }
        filterEnergies[m] = std::log(std::max(energy, 1e-10f));
    }

    // DCT-II → first 13 cepstral coefficients.
    std::array<float, 13> mfcc {};
    for (int k = 0; k < 13; ++k) {
        float sum = 0.f;
        for (int m = 0; m < numFilters; ++m)
            sum += filterEnergies[m] * std::cos((float)M_PI / numFilters * (m + 0.5f) * k);
        mfcc[k] = sum;
    }
    return mfcc;
}

// ── Cooley-Tukey radix-2 iterative FFT ────────────────────────────────────────
// Classic in-place FFT. Input size must be a power of 2.
void SpectralAnalyser::fftInPlace(std::vector<std::complex<float>>& x)
{
    const int n = (int)x.size();

    // Bit-reversal permutation.
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // Butterfly passes.
    for (int len = 2; len <= n; len <<= 1) {
        const float angle = -2.f * (float)M_PI / len;
        const std::complex<float> wlen { std::cos(angle), std::sin(angle) };
        for (int i = 0; i < n; i += len) {
            std::complex<float> w { 1.f, 0.f };
            for (int j = 0; j < len / 2; ++j) {
                const auto u = x[i + j];
                const auto v = x[i + j + len / 2] * w;
                x[i + j]         = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

} // namespace KindPath
