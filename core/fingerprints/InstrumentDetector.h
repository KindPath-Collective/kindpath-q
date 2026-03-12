#pragma once

// InstrumentDetector.h — Instrument family presence estimation.
//
// Estimates which instrument families are likely present based on
// spectral and harmonic characteristics. This is heuristic, not definitive —
// no attempt is made to claim certainty. The evidence is in the spectrum.

#include "../analysis/AnalysisResult.h"
#include "FingerprintResult.h"
#include <vector>

namespace KindPath {

class InstrumentDetector {
public:
    std::vector<FingerprintMatch> detect(const SegmentResult& q1,
                                          const SegmentResult& q2,
                                          const SegmentResult& q3,
                                          const SegmentResult& q4) const;

private:
    // Spectral region thresholds for instrument families.
    struct InstrumentProfile {
        std::string name;
        std::string description;
        std::string psychosomaticNote;
        float centroidHzMin;
        float centroidHzMax;
        float harmonicRatioMin;   // High = tonal instrument likely.
        float dynamicRangeDbMin;  // Percussive instruments have high transient range.
        float onsetDensityMin;    // Drums/percussion have high onset density.
    };

    static const std::vector<InstrumentProfile>& getProfiles();
};

} // namespace KindPath
