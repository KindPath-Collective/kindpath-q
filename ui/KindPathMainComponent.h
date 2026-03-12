#pragma once

#include "shared/JuceIncludes.h"
#include "TransportBar.h"
#include "WaveformView.h"
#include "AnalysisPanel.h"
#include "EducationPanel.h"
#include "../core/analysis/AnalysisEngine.h"
#include "../core/education/EducationCards.h"

// Forward declaration to avoid a circular dependency between the UI layer and
// the plugin processor. The processor is only accessed via its public getter
// methods; the UI does not call processBlock or any JUCE internals.
class KindPathQAudioProcessor;

namespace kindpath::ui
{
    class KindPathMainComponent : public juce::Component,
                                 private juce::Timer
    {
    public:
        explicit KindPathMainComponent(kindpath::analysis::AnalysisEngine& engine,
                                       KindPathQAudioProcessor* processor = nullptr);

        void setDeck(kindpath::education::EducationDeck* deck);
        void setOnLoadFile(std::function<void()> handler);
        void setLoadEnabled(bool enabled);
        void setFileName(const juce::String& name);
        void setMonoState(bool enabled);
        void setWaveformFile(const juce::File& file);
        void clearWaveform();
        void setOnFileDrop(std::function<void(const juce::File&)> handler);

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void timerCallback() override;

        kindpath::analysis::AnalysisEngine& analysisEngine;
        // Non-owning pointer — optional, used to poll the deep fingerprint result.
        KindPathQAudioProcessor*            audioProcessor = nullptr;
        juce::AudioFormatManager formatManager;

        TransportBar transportBar;
        WaveformView waveformView;
        AnalysisPanel analysisPanel;
        EducationPanel educationPanel;
    };
}
