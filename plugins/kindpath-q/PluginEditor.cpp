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
      mainView(audioProcessor.getAnalysisEngine())
{
    formatManager.registerBasicFormats();

    addAndMakeVisible(mainView);
    mainView.setLoadEnabled(true);
    mainView.setOnLoadFile([this] { loadFile(); });
    mainView.setOnFileDrop([this](const juce::File& file) { loadFromFile(file); });
    mainView.setFileName("Live input — drop a reference file to compare");
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

void KindPathQAudioProcessorEditor::loadFromFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // In the plugin, we display the waveform as a visual reference only.
    // Live audio analysis continues via processBlock from the DAW input bus.
    mainView.setFileName(file.getFileName());
    mainView.setWaveformFile(file);
}

void KindPathQAudioProcessorEditor::loadFile()
{
    fileChooser = std::make_unique<juce::FileChooser>("Select an audio file", juce::File{},
        "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg;*.caf;*.mp4;*.m4a");
    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        loadFromFile(chooser.getResult());
    });
}

void KindPathQAudioProcessorEditor::loadEducationDeck()
{
    juce::String errorMessage;
    const auto cardsFile = findCardsFile();
    if (deck.loadFromFile(cardsFile, errorMessage))
        mainView.setDeck(&deck);
}
