cmake_minimum_required(VERSION 3.22)

project(SpectralPlayground VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEFAULT_JUCE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/JUCE")
set(JUCE_SOURCE_DIR "${DEFAULT_JUCE_SOURCE_DIR}" CACHE PATH "Path to a JUCE checkout")

if (NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found at '${JUCE_SOURCE_DIR}'.\n"
        "Expected an in-repo JUCE checkout, for example via:\n"
        "  git submodule add https://github.com/juce-framework/JUCE.git third_party/JUCE\n"
        "Or configure with an explicit path:\n"
        "  cmake -S . -B build -DJUCE_SOURCE_DIR=/absolute/path/to/JUCE")
endif()

add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)

set(SPECTRAL_PLAYGROUND_ASSET_DIR "${CMAKE_CURRENT_LIST_DIR}/Assets/Spectral")

juce_add_gui_app(SpectralPlayground
    PRODUCT_NAME "SpectralPlayground")

target_sources(SpectralPlayground
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/WaveformPanel.cpp
        Source/AnalysisEngine.cpp
        Source/SpectralAssetWriter.cpp)

target_include_directories(SpectralPlayground
    PRIVATE
        Source)

target_compile_definitions(SpectralPlayground
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        SPECTRAL_PLAYGROUND_ASSET_DIR="${SPECTRAL_PLAYGROUND_ASSET_DIR}")

target_link_libraries(SpectralPlayground
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
