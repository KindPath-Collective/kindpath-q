#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

KindPathQAudioProcessor::KindPathQAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

KindPathQAudioProcessor::~KindPathQAudioProcessor() = default;

const juce::String KindPathQAudioProcessor::getName() const
{
    return "KindPath Q";
}

bool KindPathQAudioProcessor::acceptsMidi() const
{
    return false;
}

bool KindPathQAudioProcessor::producesMidi() const
{
    return false;
}

bool KindPathQAudioProcessor::isMidiEffect() const
{
    return false;
}

double KindPathQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KindPathQAudioProcessor::getNumPrograms()
{
    return 1;
}

int KindPathQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KindPathQAudioProcessor::setCurrentProgram(int)
{
}

const juce::String KindPathQAudioProcessor::getProgramName(int)
{
    return {};
}

void KindPathQAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void KindPathQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    analysisEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());

    // Prepare the quarter-based deep analysis pipeline.
    segmentBuffer.prepare(sampleRate);
    spectralAnalyser.prepare(sampleRate);
    dynamicAnalyser.prepare(sampleRate);
    harmonicAnalyser.prepare(sampleRate);
    temporalAnalyser.prepare(sampleRate);

    // Pre-allocate the mono mix buffer to avoid audio-thread allocation.
    monoMixBuffer.assign(static_cast<size_t>(samplesPerBlock), 0.f);

    quartersAnalysed = 0;
    fingerprintReady.store(false);
}

void KindPathQAudioProcessor::releaseResources()
{
}

bool KindPathQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void KindPathQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Real-time display analysis (fast, JUCE-based).
    analysisEngine.processBlock(buffer);

    // Deep per-quarter analysis: mono-mix and push to SegmentBuffer.
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = getTotalNumInputChannels();

    // Resize guard — monoMixBuffer pre-allocated in prepareToPlay, but handle host variability.
    if (static_cast<int>(monoMixBuffer.size()) < numSamples)
        monoMixBuffer.assign(static_cast<size_t>(numSamples), 0.f);

    // Mix all input channels down to mono (in-place sum, then normalise).
    std::fill(monoMixBuffer.begin(), monoMixBuffer.begin() + numSamples, 0.f);
    if (numChannels > 0)
    {
        const float scale = 1.f / static_cast<float>(numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                monoMixBuffer[i] += src[i] * scale;
        }
    }

    // Push to SegmentBuffer; when a quarter completes, dispatch analysis to the
    // message thread — retrieveCompletedQuarter() and fingerprintEngine.analyse()
    // both allocate and are not audio-thread safe. callAsync is lock-free here.
    if (segmentBuffer.pushSamples(monoMixBuffer.data(), numSamples))
    {
        juce::MessageManager::callAsync([this]()
        {
            std::vector<float> quarterAudio;
            const int quarterIdx = segmentBuffer.retrieveCompletedQuarter(quarterAudio);
            if (quarterIdx >= 0 && quarterIdx < 4)
                processCompletedQuarter(quarterIdx, quarterAudio);
        });
    }
}

void KindPathQAudioProcessor::processCompletedQuarter(int quarterIndex,
                                                       const std::vector<float>& audio)
{
    // Called on the message thread (via callAsync) — allocation is fine here.
    // Run all four analysers on this quarter's audio using their direct buffer paths.
    KindPath::SegmentResult& seg = quarterResults[static_cast<size_t>(quarterIndex)];
    seg.quarterIndex  = quarterIndex;
    seg.spectral  = spectralAnalyser.analyseBuffer(audio.data(), static_cast<int>(audio.size()));
    seg.dynamic   = dynamicAnalyser.analyseBuffer(audio.data(), static_cast<int>(audio.size()));
    seg.harmonic  = harmonicAnalyser.analyseBuffer(audio.data(), static_cast<int>(audio.size()));
    seg.temporal  = temporalAnalyser.analyseBuffer(audio.data(), static_cast<int>(audio.size()));

    ++quartersAnalysed;

    // Once all four quarters are in, build the full result and run fingerprinting.
    if (quartersAnalysed >= 4)
    {
        KindPath::FullAnalysisResult full;
        full.quarters = quarterResults;

        // Compute LSII inline (mirrors AnalysisEngine logic, without a separate engine).
        const auto baseline = [&](auto getter) {
            return (getter(full.q1()) + getter(full.q2()) + getter(full.q3())) / 3.f;
        };
        const auto normDelta = [](float q4val, float base) {
            return std::tanh((q4val - base) / (std::abs(base) + 1e-6f));
        };
        full.lsii.spectralDelta = normDelta(full.q4().spectral.centroidHz,
                                             baseline([](auto& q){ return q.spectral.centroidHz; }));
        full.lsii.dynamicDelta  = normDelta(full.q4().dynamic.rmsMean,
                                             baseline([](auto& q){ return q.dynamic.rmsMean; }));
        full.lsii.harmonicDelta = normDelta(full.q4().harmonic.tensionRatio,
                                             baseline([](auto& q){ return q.harmonic.tensionRatio; }));
        full.lsii.temporalDelta = normDelta(full.q4().temporal.grooveDeviationMs,
                                             baseline([](auto& q){ return q.temporal.grooveDeviationMs; }));
        full.lsii.lsii = (std::abs(full.lsii.spectralDelta) + std::abs(full.lsii.dynamicDelta) +
                          std::abs(full.lsii.harmonicDelta)  + std::abs(full.lsii.temporalDelta)) / 4.f;

        const auto fp = fingerprintEngine.analyse(full);
        {
            juce::SpinLock::ScopedLockType lock(fingerprintLock);
            latestFingerprint = fp;
        }
        fingerprintReady.store(true);

        // Reset for the next track cycle — keep analysers ready.
        quartersAnalysed = 0;
        segmentBuffer.reset();
    }
}

KindPath::FingerprintResult KindPathQAudioProcessor::getLatestFingerprint() const
{
    juce::SpinLock::ScopedLockType lock(fingerprintLock);
    return latestFingerprint;
}

bool KindPathQAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* KindPathQAudioProcessor::createEditor()
{
    return new KindPathQAudioProcessorEditor(*this);
}

void KindPathQAudioProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void KindPathQAudioProcessor::setStateInformation(const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KindPathQAudioProcessor();
}
