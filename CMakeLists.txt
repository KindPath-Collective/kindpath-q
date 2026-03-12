cmake_minimum_required(VERSION 3.22)

project(KindPathQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Clone https://github.com/juce-framework/JUCE into external/JUCE.")
endif()

# ── Pure C++ analysis engine ─────────────────────────────────────────────────
# No JUCE dependency. Testable in isolation. core/ architectural law: no JUCE headers.
add_library(kindpath_analysis STATIC
    core/analysis/SpectralAnalyser.cpp
    core/analysis/DynamicAnalyser.cpp
    core/analysis/HarmonicAnalyser.cpp
    core/analysis/TemporalAnalyser.cpp
    core/analysis/SegmentBuffer.cpp
    core/fingerprints/EraDetector.cpp
    core/fingerprints/TechniqueDetector.cpp
    core/fingerprints/InstrumentDetector.cpp
    core/fingerprints/FingerprintEngine.cpp
)

target_include_directories(kindpath_analysis PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_features(kindpath_analysis PUBLIC cxx_std_17)

# ── JUCE-dependent core ───────────────────────────────────────────────────────
# Wraps the pure analysis engine with JUCE audio processing infrastructure.
add_library(kindpath_core STATIC
    core/analysis/AnalysisEngine.cpp
    core/education/EducationCards.cpp
)

target_include_directories(kindpath_core PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(kindpath_core PUBLIC
    kindpath_analysis
    juce::juce_audio_basics
    juce::juce_dsp
)

add_library(kindpath_ui STATIC
    ui/TransportBar.cpp
    ui/WaveformView.cpp
    ui/AnalysisPanel.cpp
    ui/EducationPanel.cpp
    ui/KindPathMainComponent.cpp
)

target_include_directories(kindpath_ui PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(kindpath_ui PUBLIC
    kindpath_core
    juce::juce_audio_utils
    juce::juce_gui_basics
)

add_subdirectory(apps/standalone)
add_subdirectory(plugins/kindpath-q)
