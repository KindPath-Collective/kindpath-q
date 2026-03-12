#pragma once

// SpectralAnalyser.h — FFT-based spectral feature extraction.
//
// Computes brightness, texture, timbral identity, and spectral movement
// from the magnitude spectrum. These are the things a trained ear hears
// as "warmth," "harshness," "presence," and "air."
//
// Pure C++ — no JUCE dependency. Testable in isolation.

#include "AnalysisResult.h"
#include <vector>
#include <complex>

namespace KindPath {

class SpectralAnalyser {
public:
    // Call once before beginning to process audio.
    // fftSize must be a power of 2 (default 2048, hop default 512).
    void prepare(double sampleRate, int fftSize = 2048, int hopSize = 512);

    // Feed audio samples incrementally from the audio thread.
    // Returns true when a new spectral frame has been computed and is ready.
    // [AUDIO THREAD SAFE] — lock-free accumulation.
    bool pushSamples(const float* samples, int numSamples);

    // Get the latest spectral result computed from the rolling frame.
    // Call after pushSamples returns true, or anytime for the last known state.
    SpectralResult getResult() const;

    // Analyse a complete buffer in one shot — for offline/segment analysis.
    // Does not affect the rolling pushSamples state.
    SpectralResult analyseBuffer(const float* samples, int numSamples);

private:
    double sampleRate = 44100.0;
    int    fftSize    = 2048;
    int    hopSize    = 512;

    std::vector<float>               hannWindow;
    std::vector<float>               inputBuffer;
    std::vector<float>               magnitudeSpectrum;
    std::vector<float>               prevMagnitudeSpectrum;
    std::vector<std::complex<float>> fftBuffer;

    SpectralResult latestResult;
    bool           frameReady = false;

    // Perform the FFT and update latestResult.
    void computeFrame();

    // Spectral feature computations.
    float computeCentroid   (const float* mag, int bins) const;
    float computeRolloff    (const float* mag, int bins, float percent = 0.85f) const;
    float computeFlux       (const float* mag, const float* prevMag, int bins) const;
    float computeFlatness   (const float* mag, int bins) const;
    float computeBandwidth  (const float* mag, int bins, float centroidHz) const;
    float computeHarmonicRatio(const float* mag, int bins) const;
    std::array<float, 13> computeMFCC(const float* mag, int bins) const;

    // Iterative Cooley-Tukey radix-2 FFT (in-place, no external dependency).
    static void fftInPlace(std::vector<std::complex<float>>& x);

    // Mel scale helpers for MFCC.
    static float hzToMel(float hz) { return 2595.f * std::log10(1.f + hz / 700.f); }
    static float melToHz(float mel) { return 700.f * (std::pow(10.f, mel / 2595.f) - 1.f); }
};

} // namespace KindPath
