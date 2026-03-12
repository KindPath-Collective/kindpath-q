#include "FingerprintEngine.h"
#include <algorithm>
#include <string>
#include <sstream>

namespace KindPath {

FingerprintEngine::FingerprintEngine()  = default;
FingerprintEngine::~FingerprintEngine() = default;

FingerprintResult FingerprintEngine::analyse(const FullAnalysisResult& analysis) const
{
    FingerprintResult result;

    // Run each detector independently against all four quarters.
    // Each detector averages across quarters internally for stable read.
    result.eraMatches       = eraDetector.detect(analysis.q1(), analysis.q2(),
                                                   analysis.q3(), analysis.q4());
    result.techniqueMatches = techniqueDetector.detect(analysis.q1(), analysis.q2(),
                                                        analysis.q3(), analysis.q4());
    result.instrumentMatches = instrumentDetector.detect(analysis.q1(), analysis.q2(),
                                                          analysis.q3(), analysis.q4());

    result.productionContext = buildProductionContext(result.eraMatches);
    classifyMarkers(result);

    return result;
}

// Walk the technique matches and sort them into authenticity vs manufacturing buckets.
// Authenticity: things that were preserved (dynamics, groove, live character)
// Manufacturing: things that were applied to constrain or process
void FingerprintEngine::classifyMarkers(FingerprintResult& result) const
{
    static const std::vector<std::string> authenticityNames = {
        "natural_dynamics", "human_performance", "acoustic_instruments"
    };
    static const std::vector<std::string> manufacturingNames = {
        "heavy_compression", "quantised_rhythm", "moderate_compression"
    };

    auto contains = [](const std::vector<std::string>& list, const std::string& item) {
        return std::find(list.begin(), list.end(), item) != list.end();
    };

    for (const auto& m : result.techniqueMatches) {
        if (m.confidence < 0.4f) continue;

        const std::string label = m.name + ": " + m.description;

        if (contains(authenticityNames, m.name))
            result.authenticityMarkers.push_back(label);
        else if (contains(manufacturingNames, m.name))
            result.manufacturingMarkers.push_back(label);
    }
}

std::string FingerprintEngine::buildProductionContext(
    const std::vector<FingerprintMatch>& eraMatches) const
{
    if (eraMatches.empty())
        return "No era profile matched with sufficient confidence.";

    const auto& top = eraMatches[0];

    std::ostringstream oss;
    oss << "Production context consistent with " << top.name << " era production. "
        << top.description;

    // If a second era matches well, mention it as a possible influence or blending
    if (eraMatches.size() >= 2 && eraMatches[1].confidence > 0.4f) {
        oss << " Elements of " << eraMatches[1].name << " production are also present.";
    }

    return oss.str();
}

} // namespace KindPath
