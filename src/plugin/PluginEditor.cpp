#include "PluginEditor.h"

#include "dsp/Constants.h"
#include "dsp/Correlation.h"

#include <algorithm>

// Настройки устройства (частота, размер буфера) живут в обёртке Standalone.
// Заголовок header-only и требует juce_audio_utils; в других форматах
// StandalonePluginHolder::getInstance() просто отдаёт nullptr.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
// Вертикальная раскладка задаётся один раз: resized() и chromeHeight() обязаны
// считать одинаково, иначе окно не сойдётся с числом строк.
constexpr int kMargin = 16;
constexpr int kTitleHeight = 28;
constexpr int kHintHeight = 22;
constexpr int kControlsHeight = 28;
constexpr int kAnalysisHeight = 28;
constexpr int kBenchHeight = 28;
constexpr int kBenchTransportHeight = OverviewStrip::kHeight;
constexpr int kScopeControlsHeight = 28;
constexpr int kTableHeaderHeight = 20;
constexpr int kGapS = 8;
constexpr int kGapM = 10;
constexpr int kGapL = 12;
} // namespace

BeatEqualizerAudioProcessorEditor::BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    title.setText("Beat Equalizer  " + juce::String(JucePlugin_VersionString),
                 juce::dontSendNotification);
    title.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    layoutLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(layoutLabel);

    latencyLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(latencyLabel);

    hint.setText("Route every mic into this insert (track channels = N). Play a few bars, "
                 "then Analyze: delays and polarity are estimated against the Reference channel.",
                 juce::dontSendNotification);
    hint.setFont(juce::FontOptions(13.0f));
    hint.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hint);

    // Стенд с файлами — только в Standalone: в хосте материал приходит с дорожки.
    standalone = audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Load kit stems",
                                                      juce::File {},
                                                      "*.wav;*.aif;*.aiff;*.flac");
        const auto flags = juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::canSelectMultipleItems;

        chooser->launchAsync(flags,
                             [this](const juce::FileChooser& browser)
                             {
                                 const auto files = browser.getResults();
                                 if (files.isEmpty())
                                     return;

                                 auto& player = audioProcessor.getFilePlayer();
                                 const auto error =
                                     player.load(files, audioProcessor.getCurrentSampleRate());
                                 benchLabel.setText(error.isEmpty() ? player.getDescription()
                                                                    : error,
                                                    juce::dontSendNotification);
                                 updateBench();
                             });
    };

    playButton.onClick = [this]
    {
        // Пауза, а не стоп: позиция остаётся, повторное нажатие продолжает
        // с того же места. С начала — кнопкой отмотки.
        auto& player = audioProcessor.getFilePlayer();
        player.setPlaying(!player.isPlaying());
        updateBench();
    };

    rewindButton.onClick = [this]
    {
        audioProcessor.getFilePlayer().rewind();
        updateTransportRow();
    };

    overview.onSeek = [this](double normalised)
    {
        auto& player = audioProcessor.getFilePlayer();
        const int total = player.numSamples();
        if (total <= 0)
            return;

        player.setPosition(juce::roundToInt(normalised * static_cast<double>(total - 1)));
        updateTransportRow();
    };
    addChildComponent(overview);
    overview.setVisible(standalone);

    positionLabel.setJustificationType(juce::Justification::centredRight);
    positionLabel.setFont(juce::FontOptions(12.0f));
    addChildComponent(positionLabel);
    positionLabel.setVisible(standalone);

    deviceLabel.setJustificationType(juce::Justification::centredRight);
    deviceLabel.setFont(juce::FontOptions(12.0f));
    addChildComponent(deviceLabel);
    deviceLabel.setVisible(standalone);

    exportButton.onClick = [this]
    {
        const auto suggested =
            juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("aligned.wav");
        chooser = std::make_unique<juce::FileChooser>("Export aligned WAV", suggested, "*.wav");
        const auto flags = juce::FileBrowserComponent::saveMode
                           | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync(flags,
                             [this](const juce::FileChooser& browser)
                             {
                                 const auto file = browser.getResult();
                                 if (file == juce::File {})
                                     return;

                                 const auto error = audioProcessor.exportAligned(file);
                                 benchLabel.setText(error.isEmpty()
                                                        ? "Exported " + file.getFileName()
                                                        : error,
                                                    juce::dontSendNotification);
                             });
    };

    audioButton.onClick = []
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
    };

    for (auto* button : { &loadButton, &rewindButton, &playButton, &exportButton, &audioButton })
    {
        addChildComponent(*button);
        button->setVisible(standalone);
    }

    benchLabel.setJustificationType(juce::Justification::centredLeft);
    benchLabel.setFont(juce::FontOptions(13.0f));
    benchLabel.setText("No files loaded", juce::dontSendNotification);
    addChildComponent(benchLabel);
    benchLabel.setVisible(standalone);

    analyzeButton.onClick = [this] { audioProcessor.requestAnalyze(); };
    addAndMakeVisible(analyzeButton);
    addAndMakeVisible(freezeButton);

    analysisStatus.setJustificationType(juce::Justification::centredLeft);
    analysisStatus.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(analysisStatus);

    coherenceLabel.setJustificationType(juce::Justification::centredRight);
    coherenceLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(coherenceLabel);

    addAndMakeVisible(abButton);
    addAndMakeVisible(monoSumButton);

    referenceLabel.setText("Reference", juce::dontSendNotification);
    addAndMakeVisible(referenceLabel);
    for (int i = 1; i <= beat::kMaxChannels; ++i)
        referenceBox.addItem("Ch " + juce::String(i), i);
    addAndMakeVisible(referenceBox);

    distanceLabel.setText("Max distance (m)", juce::dontSendNotification);
    addAndMakeVisible(distanceLabel);
    distanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
    addAndMakeVisible(distanceSlider);

    const auto headerColour = juce::Colour(0xff8b919c);
    auto setupHeader = [headerColour](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, headerColour);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    };
    setupHeader(headerOn, "On");
    setupHeader(headerSolo, "S");
    setupHeader(headerMute, "M");
    setupHeader(headerName, "Ch / file");
    setupHeader(headerRole, "Role");
    setupHeader(headerLevel, "Level");
    setupHeader(headerPan, "Pan");
    setupHeader(headerDelay, "Delay (ms)");
    setupHeader(headerRotator, "Rotator");
    setupHeader(headerPolarity, "Polarity");
    setupHeader(headerCorr, "Corr");
    setupHeader(headerPhase, "Phase %");
    headerCorr.setJustificationType(juce::Justification::centredRight);
    headerPhase.setJustificationType(juce::Justification::centredRight);
    for (auto* header : { &headerOn, &headerSolo, &headerMute, &headerName, &headerRole,
                          &headerDelay, &headerRotator, &headerPolarity, &headerCorr,
                          &headerPhase })
        addAndMakeVisible(*header);

    // Level и Pan прячутся вместе со своими колонками: без материала стенда
    // монитор-микса нет, а в хосте его нет вовсе.
    addChildComponent(headerLevel);
    addChildComponent(headerPan);

    addAndMakeVisible(correlometer);

    scopeHeader.setText("Output  -  one trace per channel, shared time",
                        juce::dontSendNotification);
    scopeHeader.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(scopeHeader);

    tempoLabel.setText("Tempo", juce::dontSendNotification);
    addAndMakeVisible(tempoLabel);
    tempoBox.addItem("Host", 1);
    tempoBox.addItem("Manual", 2);
    addAndMakeVisible(tempoBox);

    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 22);
    addAndMakeVisible(tempoSlider);

    gridLabel.setText("Grid", juce::dontSendNotification);
    addAndMakeVisible(gridLabel);
    for (const auto* name : { "Off", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" })
        gridBox.addItem(name, gridBox.getNumItems() + 1);
    addAndMakeVisible(gridBox);

    timeLabel.setText("Time", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);
    timeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    timeSlider.setTextValueSuffix(" ms");
    timeSlider.setNumDecimalPlacesToDisplay(1);
    addAndMakeVisible(timeSlider);

    scopeTimeLeft.setText("0 ms", juce::dontSendNotification);
    scopeTimeLeft.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(scopeTimeLeft);
    scopeTimeRight.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(scopeTimeRight);

    scopeScratch.resize(static_cast<size_t>(audioProcessor.getScope().length()));

    auto& state = audioProcessor.getParameters();
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        enabledParams[static_cast<size_t>(i)] =
            state.getRawParameterValue(beat::channelParamId(i, "enabled"));
        delayParams[static_cast<size_t>(i)] =
            state.getRawParameterValue(beat::channelParamId(i, "delayMs"));
        polarityParams[static_cast<size_t>(i)] =
            state.getRawParameterValue(beat::channelParamId(i, "polarity"));
        muteParams[static_cast<size_t>(i)] =
            state.getRawParameterValue(beat::channelParamId(i, "mute"));
        soloParams[static_cast<size_t>(i)] =
            state.getRawParameterValue(beat::channelParamId(i, "solo"));
    }
    bypassParam = state.getRawParameterValue("global.abBypass");

    rows.reserve(static_cast<size_t>(beat::kMaxChannels));
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        auto row = std::make_unique<ChannelRow>(state, i);
        tableList.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    // Полоса прокрутки — только страховка на случай, когда окно руками сжали
    // ниже своей высоты: по умолчанию все строки помещаются целиком.
    tableViewport.setViewedComponent(&tableList, false);
    tableViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(tableViewport);

    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.abBypass", abButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.freeze", freezeButton);
    monoSumAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.monoSum", monoSumButton);
    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.reference", referenceBox);
    distanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.maxDistanceM", distanceSlider);
    timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.scopeTimeMs", timeSlider);
    tempoSourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.tempoSource", tempoBox);
    tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.tempoBpm", tempoSlider);
    gridAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.gridDivision", gridBox);
    scopeTimeParam = state.getRawParameterValue("global.scopeTimeMs");

    audioProcessor.addChangeListener(this);
    updateLayoutInfo();
    updateRowVisibility();
    updateAnalysisStatus();
    updateBench();

    // Окно растёт вниз линейно: одна строка = канал плюс его осциллограмма.
    // Ширина не зависит от числа каналов, поэтому вбок оно не разъезжается.
    const int minWidth = 2 * kMargin + ChannelRow::kControlsWidth + ChannelRow::kMinScopeWidth;
    lastActiveChannels = juce::jmax(1, activeChannelCount());
    setResizable(true, true);
    setResizeLimits(minWidth,
                    chromeHeight() + ChannelRow::kHeight,
                    2400,
                    chromeHeight() + beat::kMaxChannels * ChannelRow::kHeight);
    setSize(juce::jmax(minWidth, 1560), chromeHeight() + lastActiveChannels * ChannelRow::kHeight);
    startTimerHz(25);
}

BeatEqualizerAudioProcessorEditor::~BeatEqualizerAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.removeChangeListener(this);
}

void BeatEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16181d));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    layoutLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8c547));
    hint.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    monoSumButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    analysisStatus.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    benchLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    coherenceLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7ddc9a));
    referenceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    distanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeHeader.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    tempoLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gridLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeTimeLeft.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    scopeTimeRight.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
}

int BeatEqualizerAudioProcessorEditor::chromeHeight() const
{
    return 2 * kMargin + kTitleHeight + kGapS + kHintHeight + kGapM + kControlsHeight + kGapS
           + kAnalysisHeight
           + (standalone ? kGapS + kBenchHeight + kGapS + kBenchTransportHeight : 0) + kGapL
           + Correlometer::kHeight + kGapL + kScopeControlsHeight + kGapS + kTableHeaderHeight;
}

void BeatEqualizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto titleRow = area.removeFromTop(kTitleHeight);
    title.setBounds(titleRow.removeFromLeft(320));
    latencyLabel.setBounds(titleRow.removeFromRight(220));
    layoutLabel.setBounds(titleRow);

    area.removeFromTop(kGapS);
    hint.setBounds(area.removeFromTop(kHintHeight));
    area.removeFromTop(kGapM);

    auto controls = area.removeFromTop(kControlsHeight);
    abButton.setBounds(controls.removeFromLeft(100));
    monoSumButton.setBounds(controls.removeFromLeft(110));
    controls.removeFromLeft(12);
    referenceLabel.setBounds(controls.removeFromLeft(76));
    referenceBox.setBounds(controls.removeFromLeft(90));
    controls.removeFromLeft(12);
    distanceLabel.setBounds(controls.removeFromLeft(120));
    distanceSlider.setBounds(controls);

    area.removeFromTop(kGapS);
    auto analysisRow = area.removeFromTop(kAnalysisHeight);
    analyzeButton.setBounds(analysisRow.removeFromLeft(120));
    analysisRow.removeFromLeft(12);
    freezeButton.setBounds(analysisRow.removeFromLeft(90));
    analysisRow.removeFromLeft(12);
    coherenceLabel.setBounds(analysisRow.removeFromRight(240));
    analysisStatus.setBounds(analysisRow);

    if (standalone)
    {
        area.removeFromTop(kGapS);
        auto bench = area.removeFromTop(kBenchHeight);
        loadButton.setBounds(bench.removeFromLeft(130));
        bench.removeFromLeft(8);
        rewindButton.setBounds(bench.removeFromLeft(44));
        bench.removeFromLeft(4);
        playButton.setBounds(bench.removeFromLeft(80));
        bench.removeFromLeft(8);
        exportButton.setBounds(bench.removeFromLeft(160));
        bench.removeFromLeft(8);
        audioButton.setBounds(bench.removeFromLeft(100));
        bench.removeFromLeft(12);
        deviceLabel.setBounds(bench.removeFromRight(190));
        benchLabel.setBounds(bench);

        area.removeFromTop(kGapS);
        auto transport = area.removeFromTop(kBenchTransportHeight);
        positionLabel.setBounds(transport.removeFromRight(190));
        transport.removeFromRight(12);
        overview.setBounds(transport);
    }

    area.removeFromTop(kGapL);
    correlometer.setBounds(area.removeFromTop(Correlometer::kHeight));
    area.removeFromTop(kGapL);

    auto scopeControls = area.removeFromTop(kScopeControlsHeight);
    scopeHeader.setBounds(scopeControls.removeFromLeft(300));
    tempoLabel.setBounds(scopeControls.removeFromLeft(52));
    tempoBox.setBounds(scopeControls.removeFromLeft(84));
    scopeControls.removeFromLeft(6);
    tempoSlider.setBounds(scopeControls.removeFromLeft(180));
    scopeControls.removeFromLeft(10);
    gridLabel.setBounds(scopeControls.removeFromLeft(36));
    gridBox.setBounds(scopeControls.removeFromLeft(84));
    scopeControls.removeFromLeft(10);
    timeLabel.setBounds(scopeControls.removeFromLeft(40));
    timeSlider.setBounds(scopeControls);
    area.removeFromTop(kGapS);

    const int active = juce::jmax(1, activeChannelCount());
    const auto headerColumns =
        ChannelColumns::from(area.removeFromTop(kTableHeaderHeight), monitorColumns);
    headerOn.setBounds(headerColumns.enable);
    headerSolo.setBounds(headerColumns.solo);
    headerMute.setBounds(headerColumns.mute);
    headerName.setBounds(headerColumns.name);
    headerRole.setBounds(headerColumns.role);
    headerLevel.setBounds(headerColumns.level);
    headerPan.setBounds(headerColumns.pan);
    headerDelay.setBounds(headerColumns.delay);
    headerRotator.setBounds(headerColumns.rotator);
    headerPolarity.setBounds(headerColumns.polarity);
    headerCorr.setBounds(headerColumns.corr);
    headerPhase.setBounds(headerColumns.phase);

    // Шкала времени стоит над колонкой осциллограмм, а не под всей таблицей:
    // так подписи остаются напротив того, что они размечают.
    auto axis = headerColumns.scope;
    scopeTimeLeft.setBounds(axis.removeFromLeft(90));
    scopeTimeRight.setBounds(axis.removeFromRight(90));

    tableViewport.setBounds(area);
    tableList.setSize(tableViewport.getMaximumVisibleWidth(), active * ChannelRow::kHeight);

    int y = 0;
    for (int i = 0; i < active && i < (int) rows.size(); ++i)
    {
        rows[static_cast<size_t>(i)]->setBounds(0, y, tableList.getWidth(), ChannelRow::kHeight);
        y += ChannelRow::kHeight;
    }
}

void BeatEqualizerAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateLayoutInfo();
    updateRowVisibility();
    updateAnalysisStatus();
    syncChannelCount();
    resized();
}

void BeatEqualizerAudioProcessorEditor::timerCallback()
{
    updateLayoutInfo();
    updateAnalysisStatus();
    updateBench();
    updateTransportRow();
    updateTransportInfo();
    updateWaveforms();
}

void BeatEqualizerAudioProcessorEditor::updateBench()
{
    if (!standalone)
        return;

    auto& player = audioProcessor.getFilePlayer();
    const bool loaded = player.hasMaterial();

    playButton.setEnabled(loaded);
    rewindButton.setEnabled(loaded);
    exportButton.setEnabled(loaded);
    overview.setEnabled(loaded);
    playButton.setButtonText(player.isPlaying() ? "Pause" : "Play");

    // Строку перерисовываем только на смене состояния: иначе таймер затрёт
    // сообщение об экспорте через сорок миллисекунд.
    if (loaded != benchLoaded)
    {
        benchLoaded = loaded;
        benchLabel.setText(loaded ? player.getDescription() : "No files loaded",
                           juce::dontSendNotification);
        updateChannelNames();
        setMonitorColumns(loaded);

        // Полоса обзора и время не ждут следующего тика таймера: после Load
        // они должны показывать материал сразу.
        updateTransportRow();
    }

    syncChannelCount();
}

void BeatEqualizerAudioProcessorEditor::updateTransportRow()
{
    if (!standalone)
        return;

    auto& player = audioProcessor.getFilePlayer();
    const double rate = audioProcessor.getCurrentSampleRate();
    const int total = player.numSamples();

    const auto format = [](double seconds)
    {
        const int whole = static_cast<int>(seconds);
        return juce::String(whole / 60) + ":"
               + juce::String(whole % 60).paddedLeft('0', 2) + "."
               + juce::String(juce::jlimit(0, 9, static_cast<int>((seconds - whole) * 10.0)));
    };

    if (total <= 0 || rate <= 0.0)
    {
        positionLabel.setText("-", juce::dontSendNotification);
        overview.setOverview(nullptr, 0);
        overview.setTotalSeconds(0.0);
        overview.setPlayhead(0.0);
        overview.setWindow(0.0, 0.0);
    }
    else
    {
        const double position = player.getPosition() / rate;
        const double length = total / rate;

        // Сколько прошло, сколько всего и сколько осталось: по одной строке
        // видно и место в партии, и её длину.
        positionLabel.setText(format(position) + " / " + format(length) + "   -"
                                  + format(juce::jmax(0.0, length - position)),
                              juce::dontSendNotification);

        if (overviewGeneration != player.getGeneration())
        {
            overviewGeneration = player.getGeneration();
            std::vector<float> peaks(FilePlayer::kOverviewBins, 0.0f);
            overview.setOverview(peaks.data(), player.readOverview(peaks.data(), (int) peaks.size()));
        }

        overview.setTotalSeconds(length);
        overview.setPlayhead(static_cast<double>(player.getPosition())
                             / static_cast<double>(juce::jmax(1, total - 1)));

        // Окно строк каналов кончается на позиции воспроизведения: показываем
        // на обзоре, какой кусок партии сейчас на трассах.
        const float timeMs = (scopeTimeParam != nullptr) ? scopeTimeParam->load()
                                                         : beat::kDefaultScopeTimeMs;
        const int span = beat::ScopeRing::windowSamples(timeMs, rate);
        const double end = static_cast<double>(player.displayOrigin(span))
                           / static_cast<double>(total);
        overview.setWindow(end - static_cast<double>(span) / static_cast<double>(total), end);
    }

    // Загрузка устройства и счётчик срывов: по ним видно, кто виноват в
    // рваном звуке — машина или плагин.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        juce::String text = "CPU " + juce::String(juce::roundToInt(
                                100.0 * holder->deviceManager.getCpuUsage()))
                            + " %";

        const int xruns = holder->deviceManager.getXRunCount();
        if (xruns >= 0)
            text += "   xruns " + juce::String(xruns);

        if (text != deviceLabel.getText())
            deviceLabel.setText(text, juce::dontSendNotification);
    }
}

void BeatEqualizerAudioProcessorEditor::updateAnalysisStatus()
{
    analyzeButton.setEnabled(!audioProcessor.isAnalysisBusy());
    analysisStatus.setText(audioProcessor.getAnalysisStatus(), juce::dontSendNotification);

    const float after = audioProcessor.getCoherenceAfter();
    if (after <= 0.0f)
    {
        coherenceLabel.setText("Sum coherence  -", juce::dontSendNotification);
        return;
    }

    const auto percent = [](float value)
    { return juce::String(juce::roundToInt(100.0f * value)) + "%"; };

    coherenceLabel.setText("Sum coherence  " + percent(audioProcessor.getCoherenceBefore())
                               + " -> " + percent(after),
                           juce::dontSendNotification);
}

void BeatEqualizerAudioProcessorEditor::refreshWaveforms()
{
    updateWaveforms();
}

int BeatEqualizerAudioProcessorEditor::getScopeDisplayPoints() const
{
    return (int) scopeWindow.size();
}

int BeatEqualizerAudioProcessorEditor::activeChannelCount() const
{
    // В Standalone число входов задаёт звуковая карта, а кит на стенде обычно
    // шире: строки показываем по загруженному материалу, иначе каналы 3…16
    // анализируются и экспортируются, но их не видно и не поправить руками.
    const int channels = juce::jmax(audioProcessor.getTotalNumInputChannels(),
                                    audioProcessor.getFilePlayer().numChannels());
    return juce::jlimit(1, beat::kMaxChannels, channels);
}

bool BeatEqualizerAudioProcessorEditor::isAudible(int channel) const
{
    const auto on = [](std::atomic<float>* param)
    { return param != nullptr && param->load() >= 0.5f; };

    if (on(muteParams[static_cast<size_t>(channel)]))
        return false;

    const int active = activeChannelCount();
    bool anySolo = false;
    for (int ch = 0; ch < active; ++ch)
        anySolo = anySolo || on(soloParams[static_cast<size_t>(ch)]);

    return !anySolo || on(soloParams[static_cast<size_t>(channel)]);
}

void BeatEqualizerAudioProcessorEditor::updateTransportInfo()
{
    const auto transport = audioProcessor.getTransport();

    tempoSlider.setEnabled(!transport.fromHost);

    juce::String text = "Output  -  one trace per channel";
    if (transport.division != beat::grid::Division::off)
    {
        text += "   grid " + gridBox.getText() + " @ " + juce::String(transport.bpm, 1) + " BPM ";
        text += transport.fromHost ? "host" : "manual";
        text += "  " + juce::String(transport.numerator) + "/"
                + juce::String(transport.denominator);
    }

    if (text != scopeHeader.getText())
        scopeHeader.setText(text, juce::dontSendNotification);
}

int BeatEqualizerAudioProcessorEditor::buildGrid(double startQuarters,
                                                 int windowSamples,
                                                 beat::grid::Line* out) const
{
    const auto transport = audioProcessor.getTransport();
    const double step = beat::grid::stepQuarters(transport.division);
    const double sampleRate = audioProcessor.getCurrentSampleRate();

    if (step <= 0.0 || transport.bpm <= 0.0 || sampleRate <= 0.0 || windowSamples <= 0)
        return 0;

    const double windowQuarters = static_cast<double>(windowSamples) / sampleRate
                                  * beat::grid::quartersPerSecond(transport.bpm);

    return beat::grid::linesInWindow(startQuarters,
                                     windowQuarters,
                                     step,
                                     beat::grid::barQuarters(transport.numerator,
                                                             transport.denominator),
                                     out,
                                     beat::grid::kMaxLines);
}

void BeatEqualizerAudioProcessorEditor::updateChannelNames()
{
    auto& player = audioProcessor.getFilePlayer();
    for (int ch = 0; ch < beat::kMaxChannels; ++ch)
        rows[static_cast<size_t>(ch)]->setChannelName(player.getChannelName(ch));
}

void BeatEqualizerAudioProcessorEditor::setMonitorColumns(bool visible)
{
    if (monitorColumns == visible)
        return;

    monitorColumns = visible;
    headerLevel.setVisible(visible);
    headerPan.setVisible(visible);

    for (auto& row : rows)
        row->setMonitorVisible(visible);

    resized();
}

void BeatEqualizerAudioProcessorEditor::syncChannelCount()
{
    const int active = activeChannelCount();
    if (active == lastActiveChannels)
        return;

    lastActiveChannels = active;
    updateRowVisibility();
    updateChannelNames();
    setSize(getWidth(), chromeHeight() + active * ChannelRow::kHeight);
    resized();
}

void BeatEqualizerAudioProcessorEditor::updateWaveforms()
{
    const int active = activeChannelCount();
    const auto& ring = audioProcessor.getScope();
    auto& player = audioProcessor.getFilePlayer();
    const int captured = ring.length();
    const float timeMs = (scopeTimeParam != nullptr)
                             ? scopeTimeParam->load()
                             : beat::kDefaultScopeTimeMs;
    const int asked = beat::ScopeRing::windowSamples(timeMs, audioProcessor.getCurrentSampleRate());

    // Стенд рисуется из файлов: устройство отдаёт два канала, а строк кита
    // бывает шестнадцать, и кольцо выхода их физически не видит.
    const bool fromFiles = player.hasMaterial();
    if (active <= 0 || asked <= 0)
        return;
    if (!fromFiles && captured <= 0)
        return;

    // Кольцо живого входа держит секунду, клип — сколько угодно. В хосте окно
    // упирается в длину кольца, на стенде — нет.
    const int window = fromFiles ? asked : juce::jmin(asked, captured);

    // Длинное окно прореживается: рисовать больше kMaxDisplayPoints точек на
    // полосе в триста пикселей незачем, а память и работа растут линейно.
    const int decimation =
        juce::jmax(1, (window + beat::kMaxDisplayPoints - 1) / beat::kMaxDisplayPoints);
    const int points = juce::jmax(2, window / decimation);
    const int span = points * decimation;
    displaySpan = span;

    if (captured > 0 && (int) scopeScratch.size() != captured)
        scopeScratch.resize(static_cast<size_t>(captured));
    if ((int) scopeWindow.size() != points)
    {
        scopeWindow.resize(static_cast<size_t>(points));
        referenceWindow.resize(static_cast<size_t>(points));
        sumWindow.resize(static_cast<size_t>(points));
    }

    const int ref = juce::jlimit(0, active - 1, audioProcessor.getReferenceChannelIndex());

    // Сдвиг и полярность для файлового окна берём из тех же параметров, что и
    // аудиопоток, включая A/B: иначе картинка врёт про то, что уйдёт в экспорт.
    const float sr = static_cast<float>(audioProcessor.getCurrentSampleRate());
    const bool bypass = bypassParam != nullptr && bypassParam->load() >= 0.5f;
    float applied[beat::kMaxChannels] {};
    bool enabled[beat::kMaxChannels] {};
    float maxApplied = 0.0f;

    if (fromFiles)
    {
        for (int ch = 0; ch < active; ++ch)
        {
            auto* on = enabledParams[static_cast<size_t>(ch)];
            enabled[ch] = on == nullptr || on->load() >= 0.5f;

            auto* ms = delayParams[static_cast<size_t>(ch)];
            applied[ch] = (enabled[ch] && ms != nullptr)
                              ? juce::jmax(0.0f, ms->load() * 0.001f * sr)
                              : 0.0f;
            maxApplied = juce::jmax(maxApplied, applied[ch]);
        }
    }

    int origin = captured - span;
    if (!fromFiles)
    {
        ring.copyLast(ref, scopeScratch.data(), captured);

        constexpr float triggerLevel = 0.12f;
        const int trigger =
            beat::ScopeRing::findRisingTrigger(scopeScratch.data(), captured, triggerLevel);
        if (trigger >= 0)
            origin = juce::jlimit(0, captured - span, trigger - span / 5);
    }

    const auto readChannel = [&](int ch, std::vector<float>& dest)
    {
        if (fromFiles)
        {
            const float shift = (bypass || !enabled[ch]) ? maxApplied : applied[ch];
            player.readDisplayWindow(ch, dest.data(), points, juce::roundToInt(shift), decimation);

            auto* polarity = polarityParams[static_cast<size_t>(ch)];
            const int mode = (polarity != nullptr) ? juce::roundToInt(polarity->load()) : 0;
            if (!bypass && enabled[ch] && mode == static_cast<int>(beat::PolarityMode::invert))
                for (auto& sample : dest)
                    sample = -sample;

            return;
        }

        ring.copyLast(ch, scopeScratch.data(), captured);

        for (int i = 0; i < points; ++i)
        {
            // Точка — отсчёт с наибольшим модулем в группе, как и у клипа:
            // иначе на длинном окне удар проваливается между точками.
            float peak = 0.0f;
            for (int k = 0; k < decimation; ++k)
            {
                const float sample = scopeScratch[static_cast<size_t>(origin + i * decimation + k)];
                if (std::abs(sample) > std::abs(peak))
                    peak = sample;
            }

            dest[static_cast<size_t>(i)] = peak;
        }
    };

    // Сетка одна на все строки: она размечает окно, а не канал. В стенде
    // отсчёт идёт от начала клипа, в хосте — от позиции на таймлайне.
    const auto transport = audioProcessor.getTransport();
    const double quartersPerSample =
        (sr > 0.0f) ? beat::grid::quartersPerSecond(transport.bpm) / static_cast<double>(sr) : 0.0;

    beat::grid::Line gridLines[beat::grid::kMaxLines] {};
    int gridCount = 0;

    if (fromFiles)
    {
        const double start = static_cast<double>(player.displayOrigin(span) - span);
        gridCount = buildGrid(start * quartersPerSample, span, gridLines);
    }
    else if (transport.hasPosition)
    {
        const double behind = static_cast<double>(captured - origin);
        gridCount = buildGrid(transport.quartersAtWrite - behind * quartersPerSample,
                              span,
                              gridLines);
    }

    const auto& estimates = audioProcessor.getLastResult();

    readChannel(ref, referenceWindow);
    std::fill(sumWindow.begin(), sumWindow.end(), 0.0f);

    // Корреляция считается 25 раз в секунду по всем каналам, поэтому окно
    // прореживается: на глаз разницы нет, а работы в message thread втрое меньше.
    const int stride = juce::jmax(1, points / 2048);
    int summed = 0;

    for (int ch = 0; ch < active; ++ch)
    {
        readChannel(ch, scopeWindow);

        auto& row = *rows[static_cast<size_t>(ch)];
        row.setWaveform(scopeWindow.data(), points);
        row.setGrid(gridLines, gridCount);
        row.setIsReference(ch == ref);

        // Сравнение фаз считает Analyze: колонка показывает когерентность пары
        // «канал + опора» до и после выравнивания.
        const auto& estimate = estimates.channels[static_cast<size_t>(ch)];
        const bool measured = ch != estimates.reference && ch < estimates.numChannels
                              && estimate.valid;
        row.setPhaseMatch(estimate.coherenceBefore, estimate.coherenceAfter, measured);

        if (ch == ref)
            continue;

        row.setCorrelation(
            beat::correlation(referenceWindow.data(), scopeWindow.data(), points, stride));

        auto* on = enabledParams[static_cast<size_t>(ch)];
        if (on != nullptr && on->load() < 0.5f)
            continue;

        // Коррелометр показывает то, что слышно: заглушенный канал в сумму
        // монитора не идёт.
        if (!isAudible(ch))
            continue;

        for (int i = 0; i < points; ++i)
            sumWindow[static_cast<size_t>(i)] += scopeWindow[static_cast<size_t>(i)];
        ++summed;
    }

    if (summed > 0)
    {
        const float gain = 1.0f / static_cast<float>(summed);
        for (auto& sample : sumWindow)
            sample *= gain;

        correlometer.setPair(referenceWindow.data(), sumWindow.data(), points);
    }
    else
    {
        correlometer.setPair(nullptr, nullptr, 0);
    }

    correlometer.repaint();

    const double rate = audioProcessor.getCurrentSampleRate();
    const double windowMs = (rate > 0.0) ? 1000.0 * (double) span / rate : 0.0;
    scopeTimeRight.setText(juce::String(windowMs, 1) + " ms", juce::dontSendNotification);
}

void BeatEqualizerAudioProcessorEditor::updateLayoutInfo()
{
    const int channels = audioProcessor.getTotalNumInputChannels();
    const double rate = audioProcessor.getCurrentSampleRate();
    const int block = audioProcessor.getBlockSize();

    // Размер буфера видно прямо в шапке: на стенде это первое, что крутят,
    // когда звук захлёбывается.
    juce::String layout = juce::String(channels) + " in / "
                          + juce::String(audioProcessor.getTotalNumOutputChannels()) + " out";
    if (rate > 0.0)
        layout += "   " + juce::String(rate / 1000.0, 1) + " kHz";
    if (block > 0)
        layout += " / " + juce::String(block) + " smp buffer";

    layoutLabel.setText(layout, juce::dontSendNotification);

    const int latency = audioProcessor.getLatencySamples();
    const double sr = audioProcessor.getCurrentSampleRate();
    const double ms = (sr > 0.0) ? 1000.0 * (double) latency / sr : 0.0;
    latencyLabel.setText("PDC " + juce::String(latency) + " smp / "
                             + juce::String(ms, 2) + " ms",
                         juce::dontSendNotification);
}

void BeatEqualizerAudioProcessorEditor::updateRowVisibility()
{
    const int active = activeChannelCount();
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        rows[static_cast<size_t>(i)]->setActive(i < active);
    }
}
