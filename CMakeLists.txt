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

add_library(kindpath_core STATIC
    core/analysis/AnalysisEngine.cpp
    core/auth/UserAuth.cpp
    core/education/EducationCards.cpp
)

target_include_directories(kindpath_core PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(kindpath_core PUBLIC
    juce::juce_audio_basics
    juce::juce_cryptography
    juce::juce_dsp
)

add_library(kindpath_ui STATIC
    ui/LoginComponent.cpp
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
