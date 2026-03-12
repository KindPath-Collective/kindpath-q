#include "WaveformView.h"

namespace
{
    const juce::StringArray& audioExtensions()
    {
        static const juce::StringArray exts {
            ".wav", ".aiff", ".aif", ".mp3",
            ".flac", ".ogg", ".caf", ".mp4", ".m4a"
        };
        return exts;
    }
}

namespace kindpath::ui
{
    WaveformView::WaveformView(juce::AudioFormatManager& formatManager)
        : thumbnail(512, formatManager, thumbnailCache)
    {
    }

    void WaveformView::loadFile(const juce::File& file)
    {
        if (! file.existsAsFile())
        {
            statusText = "File not found";
            repaint();
            return;
        }

        thumbnail.setSource(new juce::FileInputSource(file));
        statusText = file.getFileName();
        repaint();
    }

    void WaveformView::clear()
    {
        thumbnail.clear();
        statusText = "Load audio to see the waveform";
        repaint();
    }

    bool WaveformView::isInterestedInFileDrag(const juce::StringArray& files)
    {
        for (const auto& f : files)
        {
            if (audioExtensions().contains(juce::File(f).getFileExtension().toLowerCase()))
                return true;
        }
        return false;
    }

    void WaveformView::fileDragEnter(const juce::StringArray&, int, int)
    {
        isDragOver = true;
        repaint();
    }

    void WaveformView::fileDragExit(const juce::StringArray&)
    {
        isDragOver = false;
        repaint();
    }

    void WaveformView::filesDropped(const juce::StringArray& files, int, int)
    {
        isDragOver = false;
        for (const auto& f : files)
        {
            const juce::File file(f);
            if (audioExtensions().contains(file.getFileExtension().toLowerCase()))
            {
                if (onFileDropped)
                    onFileDropped(file);
                break;
            }
        }
        repaint();
    }

    void WaveformView::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff0f1115));
        g.setColour(juce::Colour(0xff23262d));
        g.drawRect(getLocalBounds(), 1);

        if (thumbnail.getTotalLength() > 0.0)
        {
            g.setColour(juce::Colour(0xff7ed0ff));
            thumbnail.drawChannels(g, getLocalBounds().reduced(8), 0.0, thumbnail.getTotalLength(), 1.0f);
        }
        else
        {
            g.setColour(juce::Colours::grey);
            g.setFont(15.0f);
            g.drawFittedText(statusText, getLocalBounds().reduced(12), juce::Justification::centred, 2);
        }

        if (isDragOver)
        {
            g.setColour(juce::Colour(0x447ed0ff));
            g.fillRect(getLocalBounds());
            g.setColour(juce::Colour(0xff7ed0ff));
            g.drawRect(getLocalBounds(), 2);
            g.setFont(15.0f);
            g.drawFittedText("Drop audio file here", getLocalBounds().reduced(12), juce::Justification::centred, 2);
        }
    }

    void WaveformView::resized()
    {
    }
}
