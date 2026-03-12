#include "SegmentBuffer.h"
#include <algorithm>

namespace KindPath {

void SegmentBuffer::prepare(double sr, double quarterDurSecs)
{
    sampleRate          = sr;
    quarterDurationSecs = quarterDurSecs;
    samplesPerQuarter   = (int)(sr * quarterDurSecs);
    reset();
}

bool SegmentBuffer::pushSamples(const float* samples, int numSamples)
{
    if (quartersComplete.load() >= 4) return false;  // All quarters done.

    for (int i = 0; i < numSamples; ++i) {
        accumBuffer.push_back(samples[i]);
        ++samplesInCurrent;

        if (samplesInCurrent >= samplesPerQuarter) {
            // Quarter complete — hand off under lock.
            {
                std::lock_guard<std::mutex> lock(completedMutex);
                completedBuffer          = accumBuffer;
                completedQuarterIndex    = currentQuarter.load();
                completedReady           = true;
            }
            accumBuffer.clear();
            accumBuffer.reserve(samplesPerQuarter);
            samplesInCurrent = 0;
            currentQuarter.fetch_add(1);
            quartersComplete.fetch_add(1);
            return true;
        }
    }
    return false;
}

int SegmentBuffer::retrieveCompletedQuarter(std::vector<float>& outBuffer)
{
    std::lock_guard<std::mutex> lock(completedMutex);
    if (!completedReady) return -1;
    outBuffer             = std::move(completedBuffer);
    completedBuffer       = {};
    completedReady        = false;
    return completedQuarterIndex;
}

void SegmentBuffer::setTrackDuration(double durationSecs)
{
    quarterDurationSecs = durationSecs / 4.0;
    samplesPerQuarter   = (int)(sampleRate * quarterDurationSecs);
    // Pre-allocate accumulation buffer.
    accumBuffer.clear();
    accumBuffer.reserve(samplesPerQuarter);
    samplesInCurrent = 0;
}

void SegmentBuffer::reset()
{
    accumBuffer.clear();
    accumBuffer.reserve(samplesPerQuarter > 0 ? samplesPerQuarter : 44100);
    samplesInCurrent = 0;
    currentQuarter.store(0);
    quartersComplete.store(0);

    std::lock_guard<std::mutex> lock(completedMutex);
    completedBuffer.clear();
    completedQuarterIndex = -1;
    completedReady        = false;
}

} // namespace KindPath
