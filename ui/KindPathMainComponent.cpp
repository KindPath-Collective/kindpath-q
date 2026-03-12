#include "KindPathMainComponent.h"
#include "../plugins/kindpath-q/PluginProcessor.h"

namespace kindpath::ui
{
    KindPathMainComponent::KindPathMainComponent(kindpath::analysis::AnalysisEngine& engine,
                                                 KindPathQAudioProcessor* processor)
        : analysisEngine(engine),
          audioProcessor(processor),
          waveformView(formatManager)
    {
        formatManager.registerBasicFormats();

        addAndMakeVisible(transportBar);
        addAndMakeVisible(waveformView);
        addAndMakeVisible(analysisPanel);
        addAndMakeVisible(educationPanel);

        transportBar.onMonoChanged = [this](bool state)
        {
            analysisEngine.setMonoMonitoring(state);
        };

        startTimerHz(30);
    }

    void KindPathMainComponent::setDeck(kindpath::education::EducationDeck* deck)
    {
        educationPanel.setDeck(deck);
    }

    void KindPathMainComponent::setOnLoadFile(std::function<void()> handler)
    {
        transportBar.onLoadFile = std::move(handler);
    }

    void KindPathMainComponent::setLoadEnabled(bool enabled)
    {
        transportBar.setLoadEnabled(enabled);
    }

    void KindPathMainComponent::setFileName(const juce::String& name)
    {
        transportBar.setFileName(name);
    }

    void KindPathMainComponent::setMonoState(bool enabled)
    {
        transportBar.setMonoEnabled(enabled);
        analysisEngine.setMonoMonitoring(enabled);
    }

    void KindPathMainComponent::setWaveformFile(const juce::File& file)
    {
        waveformView.loadFile(file);
    }

    void KindPathMainComponent::clearWaveform()
    {
        waveformView.clear();
    }

    void KindPathMainComponent::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff0b0d12));
    }

    void KindPathMainComponent::resized()
    {
        auto area = getLocalBounds();
        const int topHeight = 56;
        const int sideWidth = 280;

        transportBar.setBounds(area.removeFromTop(topHeight));

        auto rightArea = area.removeFromRight(sideWidth).reduced(8);
        analysisPanel.setBounds(rightArea.removeFromTop(340));
        rightArea.removeFromTop(8);
        educationPanel.setBounds(rightArea);

        waveformView.setBounds(area.reduced(10));
    }

    void KindPathMainComponent::timerCallback()
    {
        analysisPanel.setSnapshot(analysisEngine.getSnapshot());

        // Poll the processor for a completed deep fingerprint result.
        // hasFingerprintResult() is atomic — safe to call from the UI thread.
        if (audioProcessor != nullptr && audioProcessor->hasFingerprintResult())
            analysisPanel.setFingerprint(audioProcessor->getLatestFingerprint());
    }
}
