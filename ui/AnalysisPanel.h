#pragma once

#include "shared/JuceIncludes.h"
#include "../core/analysis/AnalysisEngine.h"
#include "../core/fingerprints/FingerprintResult.h"

namespace kindpath::ui
{
    class AnalysisPanel : public juce::Component
    {
    public:
        AnalysisPanel();

        void setSnapshot(const kindpath::analysis::AnalysisSnapshot& snapshot);

        // Update the deep fingerprint section (called after full-track analysis completes).
        void setFingerprint(const KindPath::FingerprintResult& result);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        kindpath::analysis::AnalysisSnapshot currentSnapshot;
        KindPath::FingerprintResult          currentFingerprint;
        bool                                 hasFingerprintData = false;

        // Draws the fingerprint section (era + techniques) into the given area.
        void paintFingerprintSection(juce::Graphics& g, juce::Rectangle<int> area);
    };
}
