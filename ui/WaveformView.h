#pragma once

#include "shared/JuceIncludes.h"

namespace kindpath::ui
{
    class WaveformView : public juce::Component,
                         public juce::FileDragAndDropTarget
    {
    public:
        explicit WaveformView(juce::AudioFormatManager& formatManager);

        void loadFile(const juce::File& file);
        void clear();

        /** Called on the message thread when an audio file is dropped onto this view. */
        std::function<void(const juce::File&)> onFileDropped;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // FileDragAndDropTarget
        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void fileDragEnter(const juce::StringArray& files, int x, int y) override;
        void fileDragExit(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

    private:
        juce::AudioThumbnailCache thumbnailCache { 8 };
        juce::AudioThumbnail thumbnail;
        juce::String statusText = "Load audio to see the waveform";
        bool isDragOver = false;
    };
}
