add_executable(beat_tests
    test_constants.cpp
    test_snapshot.cpp
    test_gcc_phat.cpp
    test_alignment_engine.cpp
    test_analysis_ring.cpp
    test_latency_model.cpp
    test_fractional_delay.cpp
    test_allpass_rotator.cpp
    test_coherence.cpp
    test_correlation.cpp
    test_grid.cpp
    test_analysis_state.cpp
    test_scope_ring.cpp
    test_document.cpp
    test_delay_field.cpp
    test_protected_zone.cpp
    test_synthetic_kit.cpp
    test_onset_analysis.cpp
    test_spectral_flux_detector.cpp
    test_cross_mic_match.cpp
    test_real_kit.cpp)

# beat_doc публично тянет beat_dsp: повторять его здесь — дубликат на линковке.
target_link_libraries(beat_tests
    PRIVATE
        beat_doc
        Catch2::Catch2WithMain)

target_compile_features(beat_tests PRIVATE cxx_std_20)

add_test(NAME beat_tests COMMAND beat_tests)

add_executable(beat_gui_tests
    test_scope_paint.cpp
    test_analyze_flow.cpp
    test_table_ui.cpp
    test_standalone.cpp
    test_transport.cpp)

target_link_libraries(beat_gui_tests
    PRIVATE
        BeatEqualizer
        juce::juce_audio_utils
        Catch2::Catch2WithMain)

target_include_directories(beat_gui_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/plugin)

target_compile_definitions(beat_gui_tests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        BEAT_GUI_TEST_PNG="${CMAKE_BINARY_DIR}/scope-gui-test.png")

target_compile_features(beat_gui_tests PRIVATE cxx_std_20)
