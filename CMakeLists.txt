cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal 2 architectures")
endif()
project(Aura VERSION 1.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(AURA_FORMATS AU VST3)
elseif(WIN32)
    set(AURA_FORMATS VST3 Standalone)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    message(FATAL_ERROR "Aura currently supports macOS and Windows builds")
endif()
include(FetchContent)
# Complete-source release archives include the exact pinned JUCE source.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE")
endif()
# Pin the JUCE 8 release commit, rather than tracking a moving branch.
FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/91ad83ae34a81e0833b1a2b0866f54846370ae53
    URL_HASH SHA256=04f8d5055382582c757be9da069ea98338005f98248facd9c2804435ac853e70
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Aura
    COMPANY_NAME "AudioStudio"
    BUNDLE_ID "com.audiostudio.aura"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE AS01
    PLUGIN_CODE Aura
    FORMATS ${AURA_FORMATS}
    VST3_CATEGORIES Fx PitchShift
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "Aura")
set(AURA_SOURCES
    Source/SpectralProcessor.cpp Source/PluginProcessor.cpp
    Source/PluginEditor.cpp Source/SpectralVisualizer.cpp Source/KeyboardSelector.cpp)
target_sources(Aura PRIVATE ${AURA_SOURCES})
target_include_directories(Aura PUBLIC Source)
if(MSVC)
    target_compile_options(Aura PRIVATE /utf-8)
endif()
target_compile_definitions(Aura PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_VDSP_FRAMEWORK=$<BOOL:${APPLE}>)
target_link_libraries(Aura PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(AURA_BUILD_TESTS "Build DSP and processor regression tests" ON)
if(AURA_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(AuraTests PRODUCT_NAME "Aura Tests")
    target_sources(AuraTests PRIVATE Tests/TestMain.cpp ${AURA_SOURCES})
    target_include_directories(AuraTests PRIVATE Source)
    target_compile_definitions(AuraTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_VDSP_FRAMEWORK=$<BOOL:${APPLE}>)
    target_link_libraries(AuraTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(MSVC)
        target_compile_options(AuraTests PRIVATE /utf-8)
        target_link_options(AuraTests PRIVATE /STACK:8388608)
    endif()
    add_test(NAME AuraRegression COMMAND AuraTests)
endif()

option(AURA_BUILD_PREVIEW "Build a silent DSP/editor preview app for GUI review" OFF)
if(AURA_BUILD_PREVIEW)
    juce_add_gui_app(AuraPreview PRODUCT_NAME "Aura Preview" BUNDLE_ID "com.audiostudio.aura.preview")
    target_sources(AuraPreview PRIVATE Tests/EditorPreview.cpp)
    target_include_directories(AuraPreview PRIVATE Source "${juce_SOURCE_DIR}/modules")
    target_compile_definitions(AuraPreview PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_VDSP_FRAMEWORK=$<BOOL:${APPLE}>)
    # Reuse the archive and its system link dependencies without inheriting
    # plugin-wrapper compile definitions into a standalone developer app.
    get_target_property(aura_preview_dependencies Aura INTERFACE_LINK_LIBRARIES)
    add_dependencies(AuraPreview Aura)
    target_link_libraries(AuraPreview PRIVATE "$<TARGET_FILE:Aura>" ${aura_preview_dependencies})
endif()
