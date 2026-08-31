#include "PluginEditor.h"

#include "dsp/Constants.h"
#include "dsp/Correlation.h"

#include <algorithm>

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
        auto& player = audioProcessor.getFilePlayer();
        player.setPlaying(!player.isPlaying());
        updateBench();
    };

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

    for (auto* button : { &loadButton, &playButton, &exportButton })
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
    setupHeader(headerName, "Ch");
    setupHeader(headerRole, "Role");
    setupHeader(headerDelay, "Delay (ms)");
    setupHeader(headerRotator, "Rotator");
    setupHeader(headerPolarity, "Polarity");
    setupHeader(headerCorr, "Corr");
    headerCorr.setJustificationType(juce::Justification::centredRight);
    for (auto* header : { &headerOn, &headerName, &headerRole, &headerDelay, &headerRotator,
                          &headerPolarity, &headerCorr })
        addAndMakeVisible(*header);

    addAndMakeVisible(correlometer);

    scopeHeader.setText("Output  -  one trace per channel, shared time",
                        juce::dontSendNotification);
    scopeHeader.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(scopeHeader);

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
    setSize(juce::jmax(minWidth, 1240), chromeHeight() + lastActiveChannels * ChannelRow::kHeight);
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
    coherenceLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7ddc9a));
    referenceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    distanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeHeader.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeTimeLeft.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    scopeTimeRight.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
}

int BeatEqualizerAudioProcessorEditor::chromeHeight() const
{
    return 2 * kMargin + kTitleHeight + kGapS + kHintHeight + kGapM + kControlsHeight + kGapS
           + kAnalysisHeight + (standalone ? kGapS + kBenchHeight : 0) + kGapL
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
        playButton.setBounds(bench.removeFromLeft(80));
        bench.removeFromLeft(8);
        exportButton.setBounds(bench.removeFromLeft(160));
        bench.removeFromLeft(12);
        benchLabel.setBounds(bench);
    }

    area.removeFromTop(kGapL);
    correlometer.setBounds(area.removeFromTop(Correlometer::kHeight));
    area.removeFromTop(kGapL);

    auto scopeControls = area.removeFromTop(kScopeControlsHeight);
    scopeHeader.setBounds(scopeControls.removeFromLeft(280));
    timeLabel.setBounds(scopeControls.removeFromLeft(40));
    timeSlider.setBounds(scopeControls);
    area.removeFromTop(kGapS);

    const int active = juce::jmax(1, activeChannelCount());
    const auto headerColumns = ChannelColumns::from(area.removeFromTop(kTableHeaderHeight));
    headerOn.setBounds(headerColumns.enable);
    headerName.setBounds(headerColumns.name);
    headerRole.setBounds(headerColumns.role);
    headerDelay.setBounds(headerColumns.delay);
    headerRotator.setBounds(headerColumns.rotator);
    headerPolarity.setBounds(headerColumns.polarity);
    headerCorr.setBounds(headerColumns.corr);

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
    updateWaveforms();
}

void BeatEqualizerAudioProcessorEditor::updateBench()
{
    if (!standalone)
        return;

    auto& player = audioProcessor.getFilePlayer();
    const bool loaded = player.hasMaterial();

    playButton.setEnabled(loaded);
    exportButton.setEnabled(loaded);
    playButton.setButtonText(player.isPlaying() ? "Stop" : "Play");

    // Строку перерисовываем только на смене состояния: иначе таймер затрёт
    // сообщение об экспорте через сорок миллисекунд.
    if (loaded != benchLoaded)
    {
        benchLoaded = loaded;
        benchLabel.setText(loaded ? player.getDescription() : "No files loaded",
                           juce::dontSendNotification);
    }

    syncChannelCount();
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

int BeatEqualizerAudioProcessorEditor::getScopeWindowSamples() const
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

void BeatEqualizerAudioProcessorEditor::syncChannelCount()
{
    const int active = activeChannelCount();
    if (active == lastActiveChannels)
        return;

    lastActiveChannels = active;
    updateRowVisibility();
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
    const int window = beat::ScopeRing::windowSamples(timeMs, audioProcessor.getCurrentSampleRate());

    // Стенд рисуется из файлов: устройство отдаёт два канала, а строк кита
    // бывает шестнадцать, и кольцо выхода их физически не видит.
    const bool fromFiles = player.hasMaterial();
    if (active <= 0 || window <= 0)
        return;
    if (!fromFiles && (captured <= 0 || window > captured))
        return;

    if (captured > 0 && (int) scopeScratch.size() != captured)
        scopeScratch.resize(static_cast<size_t>(captured));
    if ((int) scopeWindow.size() != window)
    {
        scopeWindow.resize(static_cast<size_t>(window));
        referenceWindow.resize(static_cast<size_t>(window));
        sumWindow.resize(static_cast<size_t>(window));
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

    int origin = captured - window;
    if (!fromFiles)
    {
        ring.copyLast(ref, scopeScratch.data(), captured);

        constexpr float triggerLevel = 0.12f;
        const int trigger =
            beat::ScopeRing::findRisingTrigger(scopeScratch.data(), captured, triggerLevel);
        if (trigger >= 0)
            origin = juce::jlimit(0, captured - window, trigger - window / 5);
    }

    const auto readChannel = [&](int ch, std::vector<float>& dest)
    {
        if (fromFiles)
        {
            const float shift = (bypass || !enabled[ch]) ? maxApplied : applied[ch];
            player.readDisplayWindow(ch, dest.data(), window, juce::roundToInt(shift));

            auto* polarity = polarityParams[static_cast<size_t>(ch)];
            const int mode = (polarity != nullptr) ? juce::roundToInt(polarity->load()) : 0;
            if (!bypass && enabled[ch] && mode == static_cast<int>(beat::PolarityMode::invert))
                for (auto& sample : dest)
                    sample = -sample;

            return;
        }

        ring.copyLast(ch, scopeScratch.data(), captured);
        std::copy(scopeScratch.begin() + origin,
                  scopeScratch.begin() + origin + window,
                  dest.begin());
    };

    readChannel(ref, referenceWindow);
    std::fill(sumWindow.begin(), sumWindow.end(), 0.0f);

    // Корреляция считается 25 раз в секунду по всем каналам, поэтому окно
    // прореживается: на глаз разницы нет, а работы в message thread втрое меньше.
    const int stride = juce::jmax(1, window / 2048);
    int summed = 0;

    for (int ch = 0; ch < active; ++ch)
    {
        readChannel(ch, scopeWindow);

        auto& row = *rows[static_cast<size_t>(ch)];
        row.setWaveform(scopeWindow.data(), window);
        row.setIsReference(ch == ref);

        if (ch == ref)
            continue;

        row.setCorrelation(
            beat::correlation(referenceWindow.data(), scopeWindow.data(), window, stride));

        auto* on = enabledParams[static_cast<size_t>(ch)];
        if (on != nullptr && on->load() < 0.5f)
            continue;

        for (int i = 0; i < window; ++i)
            sumWindow[static_cast<size_t>(i)] += scopeWindow[static_cast<size_t>(i)];
        ++summed;
    }

    if (summed > 0)
    {
        const float gain = 1.0f / static_cast<float>(summed);
        for (auto& sample : sumWindow)
            sample *= gain;

        correlometer.setPair(referenceWindow.data(), sumWindow.data(), window);
    }
    else
    {
        correlometer.setPair(nullptr, nullptr, 0);
    }

    correlometer.repaint();

    const double rate = audioProcessor.getCurrentSampleRate();
    const double windowMs = (rate > 0.0) ? 1000.0 * (double) window / rate : 0.0;
    scopeTimeRight.setText(juce::String(windowMs, 1) + " ms", juce::dontSendNotification);
}

void BeatEqualizerAudioProcessorEditor::updateLayoutInfo()
{
    const int channels = audioProcessor.getTotalNumInputChannels();
    layoutLabel.setText(juce::String(channels) + " in / "
                            + juce::String(audioProcessor.getTotalNumOutputChannels()) + " out",
                        juce::dontSendNotification);

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
