#include "EraDetector.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace KindPath {

const std::vector<EraDetector::EraProfile>& EraDetector::getEraProfiles()
{
    // Era profiles derived from analysis of production conventions across decades.
    // Dynamic range figures follow the work of Ian Shepherd and the Mastering the Mix
    // research into the loudness war (2010–2020).
    static const std::vector<EraProfile> profiles = {
        {
            "Pre-1970", "Tape-era recording — analogue warmth, natural compression.",
            12.f, 30.f,   // crest factor range (dB)
            15.f, 40.f,   // dynamic range (dB)
            800.f, 2500.f, // spectral centroid range (Hz) — limited HF from tape
            "Music from this era has the natural dynamic compression of analogue tape. "
            "The frequency ceiling of 10–12 kHz from tape limitations gives warmth "
            "that many producers now artificially recreate."
        },
        {
            "1970s", "Analogue studio era — wide dynamics, analogue warmth.",
            10.f, 22.f,
            12.f, 28.f,
            1000.f, 3000.f,
            "Production from the 1970s has wide dynamic range and analogue-tape saturation. "
            "The body responds to the natural peaks and valleys of the energy envelope."
        },
        {
            "1980s", "Digital dawn — gated reverb, bright upper register, early mix compression.",
            8.f, 18.f,
            8.f, 20.f,
            1500.f, 4000.f,
            "1980s production introduced gated reverb and digital brightness. "
            "The high upper register was a marker of expensive studio technology "
            "— prestige attached to sound, as much as to content."
        },
        {
            "1990s", "Early digital production — loudness creep begins.",
            6.f, 14.f,
            6.f, 16.f,
            1200.f, 3500.f,
            "The 1990s saw the start of the loudness war. "
            "Dynamic range narrowed as commercial mastering favoured perceived loudness "
            "on radio and early CD players. The body gets less variation to respond to."
        },
        {
            "2000s", "Loudness war peak — heavy limiting, narrow dynamic range.",
            2.f, 8.f,
            2.f, 9.f,
            1500.f, 4500.f,
            "This is the loudness war at its worst. Crest factors approaching 0 dB. "
            "The dynamic body response is almost entirely removed. "
            "Perceptual fatigue from constant loudness is measurable in listener surveys."
        },
        {
            "2010s", "Streaming normalisation pullback — partial dynamic recovery.",
            4.f, 11.f,
            5.f, 13.f,
            1200.f, 4000.f,
            "Streaming platforms introduced loudness normalisation (~-14 LUFS), "
            "partially reversing the loudness war incentive. "
            "Dynamic range modestly recovered in commercial music."
        },
        {
            "2020s", "Post-streaming era — spatial audio awareness, lo-fi reaction.",
            5.f, 14.f,
            6.f, 16.f,
            1000.f, 3800.f,
            "Two competing trends: mainstream pop remains heavily compressed; "
            "lo-fi and bedroom pop deliberately introduces dynamics and warmth. "
            "Spatial audio formats (Dolby Atmos) are incentivising wider dynamic range "
            "in some contexts."
        }
    };
    return profiles;
}

float EraDetector::scoreEra(const EraProfile& p,
                              float crestDb, float dynRange, float centroidHz) const
{
    // Score = average of how well each feature falls within the profile range.
    // 1.0 if centred, decaying to 0 outside.
    auto rangeScore = [](float val, float lo, float hi) -> float {
        if (hi <= lo) return 0.f;
        if (val < lo || val > hi) {
            // Partial credit outside range — decays with distance.
            const float dist = std::min(std::abs(val - lo), std::abs(val - hi));
            const float range = hi - lo;
            return std::max(0.f, 1.f - dist / range);
        }
        // Full credit inside — peaks at 1.0 at the midpoint.
        const float mid  = (lo + hi) / 2.f;
        const float half = (hi - lo) / 2.f;
        return 1.f - std::abs(val - mid) / half;
    };

    const float crestScore    = rangeScore(crestDb, p.crestFactorDbMin, p.crestFactorDbMax);
    const float dynScore      = rangeScore(dynRange, p.dynamicRangeDbMin, p.dynamicRangeDbMax);
    const float centroidScore = rangeScore(centroidHz, p.spectralCentroidMin, p.spectralCentroidMax);

    // Crest factor and dynamic range are most diagnostic.
    return (crestScore * 0.4f + dynScore * 0.4f + centroidScore * 0.2f);
}

std::vector<FingerprintMatch> EraDetector::detect(
    const SegmentResult& q1, const SegmentResult& q2,
    const SegmentResult& q3, const SegmentResult& q4) const
{
    // Use the average across all four quarters.
    auto avg4 = [](float a, float b, float c, float d) { return (a + b + c + d) / 4.f; };

    const float crestDb    = avg4(q1.dynamic.crestFactorDb, q2.dynamic.crestFactorDb,
                                   q3.dynamic.crestFactorDb, q4.dynamic.crestFactorDb);
    const float dynRange   = avg4(q1.dynamic.dynamicRangeDb, q2.dynamic.dynamicRangeDb,
                                   q3.dynamic.dynamicRangeDb, q4.dynamic.dynamicRangeDb);
    const float centroidHz = avg4(q1.spectral.centroidHz, q2.spectral.centroidHz,
                                   q3.spectral.centroidHz, q4.spectral.centroidHz);

    std::vector<FingerprintMatch> matches;
    for (const auto& profile : getEraProfiles()) {
        const float score = scoreEra(profile, crestDb, dynRange, centroidHz);
        if (score < 0.2f) continue;  // Too dissimilar to include.

        FingerprintMatch m;
        m.category   = "era";
        m.name       = profile.name;
        m.confidence = score;
        m.description = profile.description;
        m.psychosomaticNote = profile.psychosomaticNote;
        m.evidence = {
            "Crest factor: " + std::to_string((int)crestDb) + " dB",
            "Dynamic range: " + std::to_string((int)dynRange) + " dB",
            "Spectral centroid: " + std::to_string((int)centroidHz) + " Hz"
        };
        matches.push_back(m);
    }

    // Sort by confidence descending.
    std::sort(matches.begin(), matches.end(),
              [](const FingerprintMatch& a, const FingerprintMatch& b) {
                  return a.confidence > b.confidence;
              });

    return matches;
}

} // namespace KindPath
