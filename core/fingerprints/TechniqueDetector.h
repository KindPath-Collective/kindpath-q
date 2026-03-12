#pragma once

// TechniqueDetector.h — Production technique fingerprinting.
//
// Identifies what processing choices were applied to this recording.
// Each technique is described plainly: what it is, why it was used,
// and what it does to the listener's body without their awareness.
//
// This is not accusatory. Most technique use is habitual and contextual.
// The purpose is visibility, not judgment.

#include "../analysis/AnalysisResult.h"
#include "FingerprintResult.h"
#include <vector>
#include <limits>

namespace KindPath {

class TechniqueDetector {
public:
    std::vector<FingerprintMatch> detect(const SegmentResult& q1,
                                          const SegmentResult& q2,
                                          const SegmentResult& q3,
                                          const SegmentResult& q4) const;

private:
    // A technique rule: threshold conditions and their result if met.
    struct TechniqueRule {
        std::string name;
        std::string description;
        std::string psychosomaticNote;

        // Threshold conditions — NaN means "not checked".
        float crestFactorDbMax  = std::numeric_limits<float>::quiet_NaN();
        float crestFactorDbMin  = std::numeric_limits<float>::quiet_NaN();
        float dynamicRangeDbMax = std::numeric_limits<float>::quiet_NaN();
        float dynamicRangeDbMin = std::numeric_limits<float>::quiet_NaN();
        float grooveDeviationMsMax = std::numeric_limits<float>::quiet_NaN();
        float grooveDeviationMsMin = std::numeric_limits<float>::quiet_NaN();
        float spectralFlatnessMax  = std::numeric_limits<float>::quiet_NaN();
        float spectralFlatnessMin  = std::numeric_limits<float>::quiet_NaN();
        float centroidHzMax  = std::numeric_limits<float>::quiet_NaN();
        float centroidHzMin  = std::numeric_limits<float>::quiet_NaN();
    };

    static const std::vector<TechniqueRule>& getRules();

    // Evaluate a rule against aggregated features.
    bool evaluateRule(const TechniqueRule& rule,
                      float crestDb, float dynRange,
                      float grooveMs, float flatness,
                      float centroidHz) const;

    float computeRuleConfidence(const TechniqueRule& rule,
                                float crestDb, float dynRange,
                                float grooveMs, float flatness) const;
};

} // namespace KindPath
