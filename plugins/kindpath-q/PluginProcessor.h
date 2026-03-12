#pragma once

#include "shared/JuceIncludes.h"
#include "core/analysis/AnalysisEngine.h"
#include "core/analysis/SegmentBuffer.h"
#include "core/analysis/SpectralAnalyser.h"
#include "core/analysis/DynamicAnalyser.h"
#include "core/analysis/HarmonicAnalyser.h"
#include "core/analysis/TemporalAnalyser.h"
#include "core/analysis/AnalysisResult.h"
#include "core/fingerprints/FingerprintEngine.h"
#include "core/fingerprints/FingerprintResult.h"
#include <array>

class KindPathQAudioProcessor : public juce::AudioProcessor
{
public:
    KindPathQAudioProcessor();
    ~KindPathQAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    kindpath::analysis::AnalysisEngine& getAnalysisEngine() { return analysisEngine; }

    // Returns the most recent deep fingerprint result (thread-safe copy).
    KindPath::FingerprintResult getLatestFingerprint() const;

    // True once all four quarters have been analysed at least once.
    bool hasFingerprintResult() const noexcept { return fingerprintReady.load(); }

private:
    // Real-time display engine (JUCE-based, drives the existing analysis panel).
    kindpath::analysis::AnalysisEngine analysisEngine;

    // Quarter-based deep analysis pipeline (pure C++, audio-thread safe accumulation).
    KindPath::SegmentBuffer   segmentBuffer;
    KindPath::SpectralAnalyser spectralAnalyser;
    KindPath::DynamicAnalyser  dynamicAnalyser;
    KindPath::HarmonicAnalyser harmonicAnalyser;
    KindPath::TemporalAnalyser temporalAnalyser;
    KindPath::FingerprintEngine fingerprintEngine;

    // Accumulates per-quarter SegmentResults until all four are done.
    std::array<KindPath::SegmentResult, 4> quarterResults;
    int quartersAnalysed = 0;

    // The latest full fingerprint result, updated after each completed track cycle.
    mutable juce::SpinLock    fingerprintLock;
    KindPath::FingerprintResult latestFingerprint;
    std::atomic<bool>           fingerprintReady { false };

    // Mono mix buffer — reused each processBlock to avoid allocation.
    std::vector<float> monoMixBuffer;

    // Analyse a completed quarter and update quarterResults/latestFingerprint.
    // Called on the message thread via juce::MessageManager::callAsync().
    // [NOT AUDIO THREAD SAFE] — allocates; use callAsync to dispatch from processBlock.
    void processCompletedQuarter(int quarterIndex, const std::vector<float>& audio);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KindPathQAudioProcessor)
};
