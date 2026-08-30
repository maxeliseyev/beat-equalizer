add_executable(beat_tests
    test_constants.cpp
    test_snapshot.cpp
    test_gcc_phat.cpp
    test_latency_model.cpp
    test_fractional_delay.cpp
    test_scope_ring.cpp)

target_link_libraries(beat_tests
    PRIVATE
        beat_dsp
        Catch2::Catch2WithMain)

target_compile_features(beat_tests PRIVATE cxx_std_20)

add_test(NAME beat_tests COMMAND beat_tests)
