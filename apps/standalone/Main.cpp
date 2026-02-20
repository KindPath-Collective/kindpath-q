#include "shared/JuceIncludes.h"
#include "ui/KindPathMainComponent.h"
#include "ui/LoginComponent.h"
#include "core/analysis/AnalysisEngine.h"
#include "core/auth/UserAuth.h"
#include "core/education/EducationCards.h"

namespace
{
    juce::File findCardsFile()
    {
        const auto cwd = juce::File::getCurrentWorkingDirectory().getChildFile("core/education/cards.json");
        if (cwd.existsAsFile())
            return cwd;

        const auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        const auto resources = appFile.getParentDirectory().getChildFile("Resources");
        const auto bundled = resources.getChildFile("cards.json");
        return bundled;
    }
}

// StandaloneMainComponent is the audio-capable main view, shown after login.
class StandaloneMainComponent : public juce::AudioAppComponent
{
public:
    StandaloneMainComponent()
        : mainView(analysisEngine)
    {
        formatManager.registerBasicFormats();

        addAndMakeVisible(mainView);
        mainView.setOnLoadFile([this] { loadFile(); });
        mainView.setLoadEnabled(true);
        mainView.setMonoState(false);

        loadEducationDeck();

        setSize(1100, 720);
        setAudioChannels(0, 2);
    }

    ~StandaloneMainComponent() override
    {
        shutdownAudio();
        transportSource.setSource(nullptr);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        analysisEngine.prepare(sampleRate, samplesPerBlockExpected, 2);
        transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (readerSource != nullptr)
            transportSource.getNextAudioBlock(bufferToFill);
        else
            bufferToFill.clearActiveBufferRegion();

        analysisEngine.processBlock(*bufferToFill.buffer);
    }

    void releaseResources() override
    {
        transportSource.releaseResources();
    }

    void resized() override
    {
        mainView.setBounds(getLocalBounds());
    }

private:
    void loadFile()
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select an audio file", juce::File{}, "*.wav;*.aiff;*.aif");
        const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (! file.existsAsFile())
                return;

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader == nullptr)
            {
                mainView.setFileName("Unsupported file");
                mainView.clearWaveform();
                return;
            }

            const auto sourceSampleRate = reader->sampleRate;
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
            transportSource.stop();
            transportSource.setSource(newSource.get(), 0, nullptr, sourceSampleRate);
            readerSource = std::move(newSource);
            transportSource.start();

            mainView.setFileName(file.getFileName());
            mainView.setWaveformFile(file);
        });
    }

    void loadEducationDeck()
    {
        juce::String errorMessage;
        const auto cardsFile = findCardsFile();
        if (deck.loadFromFile(cardsFile, errorMessage))
            mainView.setDeck(&deck);
    }

    kindpath::analysis::AnalysisEngine analysisEngine;
    kindpath::education::EducationDeck deck;
    kindpath::ui::KindPathMainComponent mainView;

    juce::AudioTransportSource transportSource;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

class KindPathQApplication : public juce::JUCEApplication
{
public:
    KindPathQApplication() = default;

    const juce::String getApplicationName() override { return "KindPath Q"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override
    {
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            showLoginScreen();
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        void showLoginScreen()
        {
            auto* login = new kindpath::ui::LoginComponent(userAuth);
            login->setSize(600, 480);

            login->onAuthSuccess = [this](const juce::String& /*email*/)
            {
                showMainApp();
            };

            setContentOwned(login, true);
        }

        void showMainApp()
        {
            setContentOwned(new StandaloneMainComponent(), true);
        }

        kindpath::auth::UserAuth userAuth;
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(KindPathQApplication)
