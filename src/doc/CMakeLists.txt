add_library(beat_doc STATIC
    CrossMicMatcher.cpp
    CrossfadeEditAdapter.cpp
    DelayField.cpp
    Document.cpp
    EditLog.cpp
    EditRegionPlan.cpp
    Event.cpp
    FeatureCache.cpp
    ProtectedZone.cpp
    SessionCalibration.cpp
    SessionProfile.cpp
    SourceDiagnostic.cpp
    SpectralFluxDetector.cpp)

# Документ стоит над ядром и знает про его константы; обратной зависимости
# нет и быть не должно. JUCE здесь не появляется ни в каком виде.
target_link_libraries(beat_doc
    PUBLIC
        beat_dsp)

target_include_directories(beat_doc
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(beat_doc PUBLIC cxx_std_20)
