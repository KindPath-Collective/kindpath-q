#include "InstrumentDetector.h"
#include <cmath>
#include <algorithm>
#include <string>

namespace KindPath {

std::vector<FingerprintMatch> InstrumentDetector::detect(
    const SegmentResult& q1, const SegmentResult& q2,
    const SegmentResult& q3, const SegmentResult& q4) const
{
    auto avg4 = [](float a, float b, float c, float d) { return (a + b + c + d) / 4.f; };

    // Average features across all four quarters for stable detection
    const float centroidHz  = avg4(q1.spectral.centroidHz,   q2.spectral.centroidHz,
                                    q3.spectral.centroidHz,   q4.spectral.centroidHz);
    const float harmRatio   = avg4(q1.spectral.harmonicRatio, q2.spectral.harmonicRatio,
                                    q3.spectral.harmonicRatio, q4.spectral.harmonicRatio);
    const float flatness    = avg4(q1.spectral.flatness,      q2.spectral.flatness,
                                    q3.spectral.flatness,      q4.spectral.flatness);
    const float dynRange    = avg4(q1.dynamic.dynamicRangeDb, q2.dynamic.dynamicRangeDb,
                                    q3.dynamic.dynamicRangeDb, q4.dynamic.dynamicRangeDb);
    const float onsetDens   = avg4(q1.dynamic.onsetDensity,   q2.dynamic.onsetDensity,
                                    q3.dynamic.onsetDensity,   q4.dynamic.onsetDensity);
    const float crestDb     = avg4(q1.dynamic.crestFactorDb,  q2.dynamic.crestFactorDb,
                                    q3.dynamic.crestFactorDb,  q4.dynamic.crestFactorDb);

    // Each InstrumentProfile defines the centroid range, harmonic ratio range,
    // and optional dynamic/flatness conditions that suggest the presence of that
    // instrument family in the mix.
    struct InstrumentProfile {
        const char* name;
        const char* description;
        const char* psychosomaticNote;
        float centroidMin, centroidMax;     // Hz
        float harmonicRatioMin;             // 0-1
        float harmonicRatioMax;             // 0-1
        float dynamicRangeMin;              // dB (or NAN if not checked)
        float onsetDensityMin;              // events/sec (or NAN)
        bool requireLowFlatness;            // flatness < 0.3 suggests tonal content
    };

    static const InstrumentProfile profiles[] = {
        {
            "strings_or_orchestral",
            "Orchestral string character detected — sustained harmonic content "
            "concentrated in the lower-to-mid frequency range.",
            "Strings carry physical memory in the body — slower attack time and "
            "sustained vibration trigger different physiological responses "
            "than percussive attack transients.",
            200.f, 1500.f, 0.45f, 0.95f, 6.f, NAN, true
        },
        {
            "piano_or_keys",
            "Keyboard instrument character — wide dynamic range, strong mid-range harmonics.",
            "Piano embeds decay into the psychosomatic experience. "
            "The body responds to the falling energy of each note — "
            "a natural instruction in impermanence.",
            600.f, 2800.f, 0.35f, 0.85f, 8.f, NAN, false
        },
        {
            "synthesizer",
            "Synthesised source detected — low harmonic noise and/or characteristic "
            "spectral flatness suggest electronic generation rather than acoustic.",
            "Synthesised sound bypasses the acoustic-space cues the body uses "
            "to locate a sound source. The result can be immersive or dislocating, "
            "depending on whether the mix provides spatial context.",
            500.f, 5000.f, 0.0f, 0.18f, NAN, NAN, false
        },
        {
            "drums_and_percussion",
            "Percussive transient character detected — high onset density and "
            "characteristic crest factor from drum attacks.",
            "Rhythm entrains the body faster than any other musical dimension. "
            "The beat is received as physical instruction — move, stay, flee. "
            "Onset density directly corresponds to physiological activation.",
            200.f, 8000.f, 0.0f, 0.5f, NAN, 2.5f, false
        },
        {
            "voice_or_vocal_element",
            "Vocal-range harmonic content detected — mid-to-upper frequency energy "
            "with the characteristic sustained, variable harmonics of the singing voice.",
            "The body identifies vocal sound before it identifies language. "
            "Pre-linguistic recognition of human voice triggers relational response — "
            "this signal was made by something like you.",
            180.f, 3600.f, 0.28f, 0.80f, 4.f, NAN, false
        },
        {
            "electric_guitar",
            "Electric guitar character — mid-to-upper harmonic content, "
            "characteristic bite above 1 kHz.",
            "Electric guitar carries cultural encoding specific to its era and genre. "
            "The body reads the instrument before it reads the performance — "
            "this is why timbre triggers identity attachment.",
            900.f, 4500.f, 0.32f, 0.85f, 5.f, NAN, false
        },
        {
            "sub_bass_or_bass",
            "Significant low-frequency content with defined pitch character.",
            "Sub-bass is felt before it is heard. Physical resonance in the chest "
            "and abdomen bypasses the cognitive processing layer entirely — "
            "this is direct somatic messaging.",
            50.f, 350.f, 0.3f, 1.0f, NAN, NAN, true
        }
    };

    std::vector<FingerprintMatch> matches;

    for (const auto& p : profiles) {
        bool centroidOk  = (centroidHz  >= p.centroidMin      && centroidHz  <= p.centroidMax);
        bool harmonicOk  = (harmRatio   >= p.harmonicRatioMin && harmRatio   <= p.harmonicRatioMax);
        bool dynOk       = std::isnan(p.dynamicRangeMin) || dynRange >= p.dynamicRangeMin;
        bool onsetOk     = std::isnan(p.onsetDensityMin)  || onsetDens >= p.onsetDensityMin;
        bool flatOk      = !p.requireLowFlatness           || flatness < 0.3f;

        // Special path for synthesizer — low harmonic ratio OR high flatness
        bool isSynth     = (std::string(p.name) == "synthesizer")
                            && (harmRatio < 0.2f || flatness > 0.55f)
                            && centroidOk;

        bool passes = isSynth || (centroidOk && harmonicOk && dynOk && onsetOk && flatOk);
        if (!passes) continue;

        // Confidence based on how well the harmonic ratio and centroid sit
        // within the expected envelope
        float centroidMid = (p.centroidMin + p.centroidMax) * 0.5f;
        float centroidRange = (p.centroidMax - p.centroidMin) * 0.5f;
        float centroidScore = 1.f - std::min(1.f, std::abs(centroidHz - centroidMid) / centroidRange);

        float harmonicMid   = (p.harmonicRatioMin + p.harmonicRatioMax) * 0.5f;
        float harmonicRange = (p.harmonicRatioMax - p.harmonicRatioMin) * 0.5f;
        float harmonicScore = (harmonicRange > 0.f)
                              ? 1.f - std::min(1.f, std::abs(harmRatio - harmonicMid) / harmonicRange)
                              : 0.7f;

        float confidence = (centroidScore * 0.5f + harmonicScore * 0.5f);
        if (confidence < 0.3f) continue;

        FingerprintMatch m;
        m.category          = "instrument";
        m.name              = p.name;
        m.confidence        = confidence;
        m.description       = p.description;
        m.psychosomaticNote = p.psychosomaticNote;
        m.evidence.push_back("Spectral centroid: " + std::to_string((int)centroidHz) + " Hz");
        m.evidence.push_back("Harmonic ratio: " + std::to_string((int)(harmRatio * 100)) + "%");

        matches.push_back(m);
    }

    std::sort(matches.begin(), matches.end(),
              [](const FingerprintMatch& a, const FingerprintMatch& b) {
                  return a.confidence > b.confidence;
              });
    return matches;
}

} // namespace KindPath
