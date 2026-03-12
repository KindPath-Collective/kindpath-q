#pragma once

// EraDetector.h — Production decade fingerprinting.
//
// Each era of recorded music has characteristic dynamic range, spectral
// character, and production conventions. These leave forensic traces in
// the signal that this module reads.
//
// The era profile comes from the loudness wars — the progressive compression
// of dynamic range from the 1970s through the 2000s, then the partial
// reversal with streaming normalisation in the 2010s.

#include "../analysis/AnalysisResult.h"
#include "FingerprintResult.h"
#include <vector>
#include <string>

namespace KindPath {

class EraDetector {
public:
    // Returns era matches sorted by confidence descending.
    std::vector<FingerprintMatch> detect(const SegmentResult& q1,
                                          const SegmentResult& q2,
                                          const SegmentResult& q3,
                                          const SegmentResult& q4) const;

private:
    // Era profile: characteristic production parameters.
    struct EraProfile {
        std::string name;
        std::string description;
        float crestFactorDbMin;
        float crestFactorDbMax;
        float dynamicRangeDbMin;
        float dynamicRangeDbMax;
        float spectralCentroidMin; // Typical brightness range (Hz)
        float spectralCentroidMax;
        std::string psychosomaticNote;
    };

    // Table of era profiles — derived from research into the loudness war
    // and production practices across decades.
    static const std::vector<EraProfile>& getEraProfiles();

    // Compute a confidence score for how well features match a profile.
    float scoreEra(const EraProfile& profile,
                   float crestDb, float dynRange, float centroidHz) const;
};

} // namespace KindPath
