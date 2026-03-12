#include "TechniqueDetector.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <string>

namespace KindPath {

const std::vector<TechniqueDetector::TechniqueRule>& TechniqueDetector::getRules()
{
    static const std::vector<TechniqueRule> rules = {
        {
            "heavy_compression",
            "Heavy limiting applied throughout. Dynamic peaks suppressed to near-zero.",
            "Hypercompression removes the body's natural response to energy change. "
            "Constant loudness causes perceptual fatigue over >20 minutes of exposure. "
            "The body misses the breathing.",
            /* crestMax */ 6.f, /* crestMin */ NAN,
            /* dynMax   */ NAN, /* dynMin   */ NAN,
            NAN, NAN, NAN, NAN, NAN, NAN
        },
        {
            "natural_dynamics",
            "Wide dynamic range preserved. Energy moves freely through the mix.",
            "Preserved dynamics allow the body to breathe with the music. "
            "The nervous system responds to the peaks and valleys — this is what "
            "music felt like before the loudness war.",
            NAN, /* crestMin */ 12.f,
            /* dynMax */ NAN, /* dynMin */ 12.f,
            NAN, NAN, NAN, NAN, NAN, NAN
        },
        {
            "quantised_rhythm",
            "Timing precision indicates grid-quantised performance. "
            "Groove deviation below 3ms — at or near sample precision.",
            "Perfect quantisation removes human timing variation. "
            "The body registers the absence — the groove is gone. "
            "What remains is metronomically correct but biologically flat.",
            NAN, NAN, NAN, NAN,
            /* grooveMax */ 3.f, /* grooveMin */ NAN,
            NAN, NAN, NAN, NAN
        },
        {
            "human_performance",
            "Timing variation indicates live or minimally-quantised performance. "
            "Groove deviation above 15ms.",
            "Human micro-timing is a biological signature of authentic presence. "
            "The body recognises it as different from machine precision — "
            "the slight imperfections create an organic coherence.",
            NAN, NAN, NAN, NAN,
            /* grooveMax */ NAN, /* grooveMin */ 15.f,
            NAN, NAN, NAN, NAN
        },
        {
            "moderate_compression",
            "Moderate compression typical of professional mastering (crest 6–10 dB).",
            "Well-applied compression can enhance perceived punch without removing "
            "the dynamic response. The body still hears the transients.",
            /* crestMax */ 10.f, /* crestMin */ 6.f,
            NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN
        },
        {
            "bright_modern_mix",
            "High spectral centroid — significant energy above 2.5 kHz. "
            "Characteristic of modern pop and electronic production.",
            "High-frequency emphasis activates the upper cochlear region. "
            "In excess, this creates a brightness fatigue distinct from "
            "the warmth of music recorded with more frequency balance.",
            NAN, NAN, NAN, NAN, NAN, NAN,
            NAN, NAN,   // flatness not checked
            /* centroidMax */ NAN, /* centroidMin */ 2500.f
        },
        {
            "dark_or_warm_mix",
            "Low spectral centroid — energy concentrated below 1.5 kHz. "
            "Characteristic of lo-fi, jazz, or vintage production.",
            "Low-frequency dominance creates a physically grounded, warm quality. "
            "The body responds to sub-bass and low-mids with physical resonance — "
            "felt as much as heard.",
            NAN, NAN, NAN, NAN, NAN, NAN,
            NAN, NAN,
            /* centroidMax */ 1500.f, /* centroidMin */ NAN
        },
        {
            "noise_heavy_texture",
            "High spectral flatness — noise component significant relative to tonal content. "
            "May indicate noise floor, lo-fi aesthetic, or heavy distortion.",
            "High noise content desaturates the tonal signal. "
            "This can be intentional (lo-fi warmth) or a quality issue. "
            "The body responds to noise differently from tone — with alertness rather than resonance.",
            NAN, NAN, NAN, NAN, NAN, NAN,
            /* flatnessMax */ NAN, /* flatnessMin */ 0.6f,
            NAN, NAN
        }
    };
    return rules;
}

bool TechniqueDetector::evaluateRule(const TechniqueRule& rule,
                                      float crestDb, float dynRange,
                                      float grooveMs, float flatness,
                                      float centroidHz) const
{
    auto check = [](float val, float lo, float hi) -> bool {
        const bool hasLo = !std::isnan(lo);
        const bool hasHi = !std::isnan(hi);
        if (hasLo && val < lo) return false;
        if (hasHi && val > hi) return false;
        return true;
    };

    return check(crestDb,    rule.crestFactorDbMin, rule.crestFactorDbMax)
        && check(dynRange,   rule.dynamicRangeDbMin, rule.dynamicRangeDbMax)
        && check(grooveMs,   rule.grooveDeviationMsMin, rule.grooveDeviationMsMax)
        && check(flatness,   rule.spectralFlatnessMin, rule.spectralFlatnessMax)
        && check(centroidHz, rule.centroidHzMin, rule.centroidHzMax);
}

float TechniqueDetector::computeRuleConfidence(const TechniqueRule& rule,
                                                float crestDb, float dynRange,
                                                float grooveMs, float flatness) const
{
    // Confidence = how strongly the values exceed the thresholds.
    int   activeConstraints = 0;
    float totalScore        = 0.f;

    auto addScore = [&](float val, float lo, float hi) {
        const bool hasLo = !std::isnan(lo);
        const bool hasHi = !std::isnan(hi);
        if (!hasLo && !hasHi) return;
        ++activeConstraints;
        if (hasHi && val <= hi) totalScore += std::min(1.f, (hi - val) / (hi * 0.5f + 1.f));
        if (hasLo && val >= lo) totalScore += std::min(1.f, (val - lo) / (lo * 0.5f + 1.f));
    };

    addScore(crestDb,  rule.crestFactorDbMin, rule.crestFactorDbMax);
    addScore(dynRange, rule.dynamicRangeDbMin, rule.dynamicRangeDbMax);
    addScore(grooveMs, rule.grooveDeviationMsMin, rule.grooveDeviationMsMax);
    addScore(flatness, rule.spectralFlatnessMin, rule.spectralFlatnessMax);

    return activeConstraints > 0 ? std::min(1.f, totalScore / activeConstraints) : 0.5f;
}

std::vector<FingerprintMatch> TechniqueDetector::detect(
    const SegmentResult& q1, const SegmentResult& q2,
    const SegmentResult& q3, const SegmentResult& q4) const
{
    auto avg4 = [](float a, float b, float c, float d) { return (a + b + c + d) / 4.f; };

    const float crestDb    = avg4(q1.dynamic.crestFactorDb, q2.dynamic.crestFactorDb,
                                   q3.dynamic.crestFactorDb, q4.dynamic.crestFactorDb);
    const float dynRange   = avg4(q1.dynamic.dynamicRangeDb, q2.dynamic.dynamicRangeDb,
                                   q3.dynamic.dynamicRangeDb, q4.dynamic.dynamicRangeDb);
    const float grooveMs   = avg4(q1.temporal.grooveDeviationMs, q2.temporal.grooveDeviationMs,
                                   q3.temporal.grooveDeviationMs, q4.temporal.grooveDeviationMs);
    const float flatness   = avg4(q1.spectral.flatness, q2.spectral.flatness,
                                   q3.spectral.flatness, q4.spectral.flatness);
    const float centroidHz = avg4(q1.spectral.centroidHz, q2.spectral.centroidHz,
                                   q3.spectral.centroidHz, q4.spectral.centroidHz);

    std::vector<FingerprintMatch> matches;
    for (const auto& rule : getRules()) {
        if (!evaluateRule(rule, crestDb, dynRange, grooveMs, flatness, centroidHz))
            continue;

        FingerprintMatch m;
        m.category          = "technique";
        m.name              = rule.name;
        m.confidence        = computeRuleConfidence(rule, crestDb, dynRange, grooveMs, flatness);
        m.description       = rule.description;
        m.psychosomaticNote = rule.psychosomaticNote;

        if (crestDb > 0.f)
            m.evidence.push_back("Crest factor: " + std::to_string((int)crestDb) + " dB");
        if (grooveMs > 0.f)
            m.evidence.push_back("Groove deviation: " + std::to_string((int)grooveMs) + " ms");

        matches.push_back(m);
    }

    std::sort(matches.begin(), matches.end(),
              [](const FingerprintMatch& a, const FingerprintMatch& b) {
                  return a.confidence > b.confidence;
              });
    return matches;
}

} // namespace KindPath
