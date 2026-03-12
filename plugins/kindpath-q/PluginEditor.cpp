#include "PluginEditor.h"

namespace
{
    juce::File findCardsFile()
    {
        const auto cwd = juce::File::getCurrentWorkingDirectory().getChildFile("core/education/cards.json");
        if (cwd.existsAsFile())
            return cwd;

        const auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        const auto resources = appFile.getParentDirectory().getChildFile("Resources");
        return resources.getChildFile("cards.json");
    }
}

KindPathQAudioProcessorEditor::KindPathQAudioProcessorEditor(KindPathQAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mainView(audioProcessor.getAnalysisEngine(), &audioProcessor)
{
    addAndMakeVisible(mainView);
    mainView.setLoadEnabled(false);
    mainView.setFileName("Live input");
    mainView.setMonoState(false);

    loadEducationDeck();

    setSize(980, 640);
}

KindPathQAudioProcessorEditor::~KindPathQAudioProcessorEditor() = default;

void KindPathQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void KindPathQAudioProcessorEditor::resized()
{
    mainView.setBounds(getLocalBounds());
}

void KindPathQAudioProcessorEditor::loadEducationDeck()
{
    juce::String errorMessage;
    const auto cardsFile = findCardsFile();
    if (deck.loadFromFile(cardsFile, errorMessage))
        mainView.setDeck(&deck);
}
