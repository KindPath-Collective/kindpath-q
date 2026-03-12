#pragma once

// FingerprintEngine.h — Orchestrates all fingerprint detection.
//
// Takes the four-quarter analysis results and runs era, technique, and
// instrument detection against them. Returns a unified FingerprintResult.

#include "../analysis/AnalysisResult.h"
#include "FingerprintResult.h"
#include "EraDetector.h"
#include "TechniqueDetector.h"
#include "InstrumentDetector.h"

namespace KindPath {

class FingerprintEngine {
public:
    FingerprintEngine();

    // Run all detectors against the four-quarter analysis.
    // Returns a fully populated FingerprintResult.
    FingerprintResult analyse(const FullAnalysisResult& analysis) const;

private:
    EraDetector        eraDetector;
    TechniqueDetector  techniqueDetector;
    InstrumentDetector instrumentDetector;

    // Derive authenticity and manufacturing marker lists from all matches.
    void classifyMarkers(FingerprintResult& result) const;

    // Build top-line production context string from era matches.
    std::string buildProductionContext(const std::vector<FingerprintMatch>& eras) const;
};

} // namespace KindPath
