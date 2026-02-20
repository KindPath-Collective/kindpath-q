#pragma once

#include "shared/JuceIncludes.h"
#include "TransportBar.h"
#include "WaveformView.h"
#include "AnalysisPanel.h"
#include "EducationPanel.h"
#include "../core/analysis/AnalysisEngine.h"
#include "../core/education/EducationCards.h"

namespace kindpath::ui
{
    class KindPathMainComponent : public juce::Component,
                                 private juce::Timer
    {
    public:
        explicit KindPathMainComponent(kindpath::analysis::AnalysisEngine& engine);

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
        juce::AudioFormatManager formatManager;

        TransportBar transportBar;
        WaveformView waveformView;
        AnalysisPanel analysisPanel;
        EducationPanel educationPanel;
    };
}
