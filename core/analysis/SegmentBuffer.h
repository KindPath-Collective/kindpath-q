#pragma once

// SegmentBuffer.h — Quarter-based audio accumulation for offline-aware analysis.
//
// KindPath Q divides each piece into four equal quarters and analyses each
// separately. This buffer collects audio samples until one quarter is complete,
// then signals that a full quarter is ready for deep analysis.
//
// For live input (unknown duration), defaults to 60-second quarters.
// For offline analysis (known duration), use setTrackDuration().
//
// Designed for use from the audio thread with minimal locking.
// Pure C++ — no JUCE dependency.

#include <vector>
#include <atomic>
#include <mutex>

namespace KindPath {

class SegmentBuffer {
public:
    // Call before processing begins.
    // quarterDurationSecs: how long each quarter is expected to be.
    void prepare(double sampleRate, double quarterDurationSecs = 60.0);

    // Push audio samples from the audio thread.
    // Returns true when a complete quarter is ready to retrieve.
    // [AUDIO THREAD SAFE] — accumulation side is lock-free.
    bool pushSamples(const float* samples, int numSamples);

    // After pushSamples returns true, call this to retrieve the completed quarter.
    // Returns the quarter index (0–3) and fills outBuffer with the audio.
    // Returns -1 if no completed quarter is waiting.
    int retrieveCompletedQuarter(std::vector<float>& outBuffer);

    // Call this when the track duration is known (e.g. from file metadata).
    // Enables exact quarter boundaries rather than the time-based estimate.
    void setTrackDuration(double durationSecs);

    // Reset for a new track.
    void reset();

    int  getCurrentQuarter()      const noexcept { return currentQuarter.load(); }
    bool allQuartersComplete()    const noexcept { return quartersComplete.load() >= 4; }
    int  quartersCompletedCount() const noexcept { return quartersComplete.load(); }

private:
    double sampleRate          = 44100.0;
    double quarterDurationSecs = 60.0;
    int    samplesPerQuarter   = 0;

    // Audio accumulation (written from audio thread).
    std::vector<float> accumBuffer;
    int                samplesInCurrent = 0;

    std::atomic<int> currentQuarter  { 0 };
    std::atomic<int> quartersComplete { 0 };

    // Completed quarter handoff (protected by mutex — infrequent, not on audio thread).
    std::mutex         completedMutex;
    std::vector<float> completedBuffer;
    int                completedQuarterIndex = -1;
    bool               completedReady        = false;
};

} // namespace KindPath
