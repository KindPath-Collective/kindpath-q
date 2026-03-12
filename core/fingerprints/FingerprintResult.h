#pragma once

// FingerprintResult.h — Result types for the fingerprint engine.
//
// A fingerprint is a match between what is in the signal and what we know
// about how things were made. Era, technique, instrument — each match comes
// with a description and a psychosomatic note: what does this tool do to
// a listener's body?
//
// Technique belongs to everyone. What is detectable in a signal was put
// there by the creator and released with the work.

#include <string>
#include <vector>

namespace KindPath {

struct FingerprintMatch {
    std::string category;           // "era", "technique", "instrument"
    std::string name;               // What matched (e.g. "2000s", "heavy_compression")
    float       confidence = 0.f;   // 0–1
    std::vector<std::string> evidence;          // Data points that triggered the match.
    std::string description;        // Human-readable explanation.
    std::string psychosomaticNote;  // What this technique does to a listener.
};

struct FingerprintResult {
    std::vector<FingerprintMatch> eraMatches;        // Sorted by confidence descending.
    std::vector<FingerprintMatch> techniqueMatches;
    std::vector<FingerprintMatch> instrumentMatches;

    std::string productionContext;               // Top-line summary of production era.
    std::vector<std::string> authenticityMarkers;   // Present authentic creation signals.
    std::vector<std::string> manufacturingMarkers;  // Present conditioning/formula signals.
};

} // namespace KindPath
