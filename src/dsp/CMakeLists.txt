add_library(beat_dsp STATIC
    AlignmentSnapshot.cpp)

target_include_directories(beat_dsp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(beat_dsp PUBLIC cxx_std_20)
