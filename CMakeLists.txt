cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum supported macOS")
project(GuitarLink VERSION 0.5.0)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(GuitarLink COMPANY_NAME "Independent Audio"
 BUNDLE_ID "audio.independent.guitarlink" PLUGIN_MANUFACTURER_CODE IndA
 PLUGIN_CODE Glnk PRODUCT_NAME "GuitarLink" FORMATS VST3 Standalone
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT TRUE
 IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(GuitarLink)
target_sources(GuitarLink PRIVATE Source/Processor.cpp)
target_compile_definitions(GuitarLink PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(GuitarLink PRIVATE juce::juce_audio_utils)
add_executable(tracker_test Source/TrackerTest.cpp)
enable_testing()
add_test(NAME tracker COMMAND tracker_test)

juce_add_gui_app(UiPreview PRODUCT_NAME "GuitarLink Preview")
target_sources(UiPreview PRIVATE Source/UiPreview.cpp)
target_compile_definitions(UiPreview PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(UiPreview PRIVATE GuitarLink juce::juce_audio_utils)

add_executable(chord_test Source/ChordTest.cpp)
add_test(NAME chords_and_timing COMMAND chord_test)

add_executable(calibrated_test Source/CalibratedTest.cpp)
add_test(NAME calibrated_strings COMMAND calibrated_test)
add_executable(calibrated_worker_test Source/CalibratedWorkerTest.cpp)
find_package(Threads REQUIRED)
target_link_libraries(calibrated_worker_test PRIVATE Threads::Threads)
add_test(NAME calibrated_worker COMMAND calibrated_worker_test)

add_executable(measured_lead_test Source/MeasuredLeadTest.cpp)
add_test(NAME measured_lead COMMAND measured_lead_test)
