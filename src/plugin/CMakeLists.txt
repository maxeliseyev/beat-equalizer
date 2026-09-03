# Иконка подхватывается, если она положена: JUCE собирает из PNG весь набор
# размеров и прописывает CFBundleIconFile. Нет файла — сборка идёт без иконки,
# а не падает. Готовится скриптом scripts/make-icon.sh.
set(BEAT_ICON "${CMAKE_SOURCE_DIR}/packaging/macos/icon.png")
if(EXISTS "${BEAT_ICON}")
    set(BEAT_ICON_ARGS ICON_BIG "${BEAT_ICON}")
else()
    set(BEAT_ICON_ARGS)
endif()

juce_add_plugin(BeatEqualizer
    VERSION ${PROJECT_VERSION}
    ${BEAT_ICON_ARGS}
    # Стенд открывает вход звуковой карты. Без этого ключа в Info.plist macOS
    # убивает процесс в момент открытия устройства — и падение выглядит как
    # баг в аудио, а не как незаполненный plist.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Beat Equalizer слушает вход звуковой карты, чтобы показывать осциллограммы и мерить задержки между микрофонами."
    HARDENED_RUNTIME_ENABLED TRUE
    COMPANY_NAME "Beat Equalizer"
    BUNDLE_ID com.beatequalizer.align
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Bteq
    PLUGIN_CODE Algn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Beat Equalizer"
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(BeatEqualizer
    PRIVATE
        AnalysisWorker.cpp
        ChannelRow.cpp
        Correlometer.cpp
        Exporter.cpp
        FilePlayer.cpp
        OverviewStrip.cpp
        Parameters.cpp
        PluginProcessor.cpp
        PluginEditor.cpp
        ScopeStrip.cpp)

target_compile_definitions(BeatEqualizer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0)

target_link_libraries(BeatEqualizer
    PRIVATE
        beat_dsp
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
