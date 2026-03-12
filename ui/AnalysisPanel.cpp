#include "AnalysisPanel.h"

namespace kindpath::ui
{
    AnalysisPanel::AnalysisPanel()
    {
    }

    void AnalysisPanel::setSnapshot(const kindpath::analysis::AnalysisSnapshot& snapshot)
    {
        currentSnapshot = snapshot;
        repaint();
    }

    void AnalysisPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff12141a));
        g.setColour(juce::Colour(0xff2f3440));
        g.drawRect(getLocalBounds(), 1);

        auto area = getLocalBounds().reduced(10);
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("Analysis", area.removeFromTop(24), juce::Justification::left, false);

        auto spectrumArea = area.removeFromTop(120);
        spectrumArea.removeFromTop(8);

        const int barCount = static_cast<int>(currentSnapshot.spectrumBars.size());
        const int barWidth = spectrumArea.getWidth() / barCount;
        for (int i = 0; i < barCount; ++i)
        {
            const float value = currentSnapshot.spectrumBars[static_cast<size_t>(i)];
            const int barHeight = juce::jlimit(2, spectrumArea.getHeight(), static_cast<int>(value * spectrumArea.getHeight()));
            const int x = spectrumArea.getX() + i * barWidth;
            const int y = spectrumArea.getBottom() - barHeight;

            g.setColour(juce::Colour(0xff6ee7b7));
            g.fillRect(x + 1, y, barWidth - 2, barHeight);
        }

        area.removeFromTop(10);
        auto meterArea = area.removeFromTop(60);

        const float rmsNorm = juce::jlimit(0.0f, 1.0f, (currentSnapshot.rmsDb + 60.0f) / 60.0f);
        const float corrNorm = juce::jlimit(0.0f, 1.0f, (currentSnapshot.correlation + 1.0f) * 0.5f);

        auto rmsArea = meterArea.removeFromTop(24);
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(12.0f);
        g.drawText("RMS", rmsArea.removeFromLeft(48), juce::Justification::left, false);
        g.setColour(juce::Colour(0xff0f172a));
        g.fillRect(rmsArea);
        g.setColour(juce::Colour(0xff60a5fa));
        g.fillRect(rmsArea.withWidth(static_cast<int>(rmsArea.getWidth() * rmsNorm)));
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRect(rmsArea, 1);

        meterArea.removeFromTop(8);
        auto corrArea = meterArea.removeFromTop(24);
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Corr", corrArea.removeFromLeft(48), juce::Justification::left, false);
        g.setColour(juce::Colour(0xff0f172a));
        g.fillRect(corrArea);
        g.setColour(juce::Colour(0xfffbbf24));
        g.fillRect(corrArea.withWidth(static_cast<int>(corrArea.getWidth() * corrNorm)));
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRect(corrArea, 1);

        area.removeFromTop(6);
        g.setColour(juce::Colours::silver);
        g.setFont(13.0f);

        const juce::String rmsText = "RMS: " + juce::String(currentSnapshot.rmsDb, 1) + " dB";
        const juce::String crestText = "Crest: " + juce::String(currentSnapshot.crestDb, 1) + " dB";
        const juce::String corrText = "Correlation: " + juce::String(currentSnapshot.correlation, 2);
        const juce::String transientText = "Transient density: " + juce::String(currentSnapshot.transientDensity, 3);

        g.drawText(rmsText, area.removeFromTop(18), juce::Justification::left, false);
        g.drawText(crestText, area.removeFromTop(18), juce::Justification::left, false);
        g.drawText(corrText, area.removeFromTop(18), juce::Justification::left, false);
        g.drawText(transientText, area.removeFromTop(18), juce::Justification::left, false);

        area.removeFromTop(8);
        g.setColour(juce::Colour(0xffa7f3d0));
        g.drawText("Feel tags: " + currentSnapshot.feelTags.joinIntoString(", "), area.removeFromTop(18), juce::Justification::left, false);

        // Deep fingerprint section — only shown after a full track cycle completes.
        if (hasFingerprintData && area.getHeight() > 20)
        {
            area.removeFromTop(12);
            g.setColour(juce::Colour(0xff2f3440));
            g.fillRect(area.removeFromTop(1)); // divider
            area.removeFromTop(6);
            paintFingerprintSection(g, area);
        }
    }

    void AnalysisPanel::setFingerprint(const KindPath::FingerprintResult& result)
    {
        currentFingerprint = result;
        hasFingerprintData = true;
        repaint();
    }

    void AnalysisPanel::paintFingerprintSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Section heading.
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(11.0f);
        g.drawText("FINGERPRINT", area.removeFromTop(14), juce::Justification::left, false);
        area.removeFromTop(4);

        // Era: top match by confidence.
        if (!currentFingerprint.eraMatches.empty())
        {
            const auto& era = currentFingerprint.eraMatches.front();
            const juce::String eraText = "Era: " + juce::String(era.name)
                + "  (" + juce::String(static_cast<int>(era.confidence * 100)) + "%)"; 
            g.setColour(juce::Colour(0xffd1d5db));
            g.setFont(12.0f);
            g.drawText(eraText, area.removeFromTop(16), juce::Justification::left, false);
        }

        // Production context summary.
        if (!currentFingerprint.productionContext.empty())
        {
            g.setColour(juce::Colour(0xff9ca3af));
            g.setFont(11.0f);
            g.drawText(juce::String(currentFingerprint.productionContext),
                       area.removeFromTop(14), juce::Justification::left, false);
        }

        area.removeFromTop(4);

        // Technique matches: show up to two with their psychosomatic notes.
        g.setFont(11.0f);
        int shown = 0;
        for (const auto& tech : currentFingerprint.techniqueMatches)
        {
            if (shown >= 2 || area.getHeight() < 28)
                break;

            // Technique name in amber if it's a manufacturing marker, green if authentic.
            const bool isManufacturing = [&]() {
                for (const auto& m : currentFingerprint.manufacturingMarkers)
                    if (m.find(tech.name) != std::string::npos) return true;
                return false;
            }();

            g.setColour(isManufacturing ? juce::Colour(0xfff5a623) : juce::Colour(0xff4ade80));
            g.drawText(juce::String(tech.name),
                       area.removeFromTop(14), juce::Justification::left, false);

            if (!tech.psychosomaticNote.empty() && area.getHeight() >= 13)
            {
                g.setColour(juce::Colour(0xff6b7280));
                // Truncate long notes to fit the panel width.
                juce::String note(tech.psychosomaticNote);
                if (note.length() > 60) note = note.substring(0, 57) + "...";
                g.drawText(note, area.removeFromTop(13), juce::Justification::left, false);
            }
            ++shown;
        }
    }

    void AnalysisPanel::resized()
    {
    }
}
