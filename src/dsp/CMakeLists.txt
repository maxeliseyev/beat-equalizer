add_library(beat_dsp STATIC
    AlignmentEngine.cpp
    AlignmentSnapshot.cpp
    AllpassRotator.cpp
    AnalysisState.cpp
    Coherence.cpp
    Envelope.cpp
    Fft.cpp
    FractionalDelay.cpp
    GccPhat.cpp
    GlideRenderer.cpp
    HitSegment.cpp
    OnsetAnalysis.cpp
    PeakPicker.cpp)

target_include_directories(beat_dsp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(beat_dsp PUBLIC cxx_std_20)
