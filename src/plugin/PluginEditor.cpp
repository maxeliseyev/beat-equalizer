#include "PluginEditor.h"

#include "dsp/Constants.h"

#include <algorithm>

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
    setupHeader(headerDelay, "Delay (ms)");
    setupHeader(headerPolarity, "Polarity");
    addAndMakeVisible(headerOn);
    addAndMakeVisible(headerName);
    addAndMakeVisible(headerDelay);
    addAndMakeVisible(headerPolarity);

    scopeHeader.setText("Output  -  stacked traces, shared time", juce::dontSendNotification);
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
    rows.reserve(static_cast<size_t>(beat::kMaxChannels));
    strips.reserve(static_cast<size_t>(beat::kMaxChannels));
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        auto row = std::make_unique<ChannelRow>(state, i);
        tableList.addAndMakeVisible(*row);
        rows.push_back(std::move(row));

        auto strip = std::make_unique<ScopeStrip>(i);
        scopeList.addAndMakeVisible(*strip);
        strips.push_back(std::move(strip));
    }

    tableViewport.setViewedComponent(&tableList, false);
    tableViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(tableViewport);

    scopeViewport.setViewedComponent(&scopeList, false);
    scopeViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(scopeViewport);

    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.abBypass", abButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.freeze", freezeButton);
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

    setResizable(true, true);
    setResizeLimits(800, 560, 1400, 1200);
    setSize(960, 720);
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
    analysisStatus.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    coherenceLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7ddc9a));
    referenceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    distanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeHeader.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    scopeTimeLeft.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    scopeTimeRight.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
}

void BeatEqualizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto titleRow = area.removeFromTop(28);
    title.setBounds(titleRow.removeFromLeft(320));
    latencyLabel.setBounds(titleRow.removeFromRight(220));
    layoutLabel.setBounds(titleRow);

    area.removeFromTop(8);
    hint.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto controls = area.removeFromTop(28);
    abButton.setBounds(controls.removeFromLeft(220));
    controls.removeFromLeft(12);
    referenceLabel.setBounds(controls.removeFromLeft(80));
    referenceBox.setBounds(controls.removeFromLeft(90));
    controls.removeFromLeft(12);
    distanceLabel.setBounds(controls.removeFromLeft(120));
    distanceSlider.setBounds(controls);

    area.removeFromTop(8);
    auto analysisRow = area.removeFromTop(28);
    analyzeButton.setBounds(analysisRow.removeFromLeft(120));
    analysisRow.removeFromLeft(12);
    freezeButton.setBounds(analysisRow.removeFromLeft(90));
    analysisRow.removeFromLeft(12);
    coherenceLabel.setBounds(analysisRow.removeFromRight(240));
    analysisStatus.setBounds(analysisRow);

    area.removeFromTop(12);

    const int active = juce::jmax(1, activeChannelCount());
    constexpr int kTableMaxVisible = 6;
    const int tableBody = juce::jmin(active, kTableMaxVisible) * ChannelRow::kHeight;
    auto tableArea = area.removeFromTop(20 + tableBody);
    ChannelRow::layoutHeader(tableArea.removeFromTop(20),
                             headerOn,
                             headerName,
                             headerDelay,
                             headerPolarity);
    tableViewport.setBounds(tableArea);
    tableList.setSize(tableViewport.getMaximumVisibleWidth(), active * ChannelRow::kHeight);

    int y = 0;
    for (int i = 0; i < active && i < (int) rows.size(); ++i)
    {
        rows[static_cast<size_t>(i)]->setBounds(0, y, tableList.getWidth(), ChannelRow::kHeight);
        y += ChannelRow::kHeight;
    }

    area.removeFromTop(14);
    auto axisRow = area.removeFromBottom(16);
    scopeTimeLeft.setBounds(axisRow.removeFromLeft(90));
    scopeTimeRight.setBounds(axisRow.removeFromRight(90));

    auto scopeControls = area.removeFromTop(28);
    scopeHeader.setBounds(scopeControls.removeFromLeft(280));
    timeLabel.setBounds(scopeControls.removeFromLeft(40));
    timeSlider.setBounds(scopeControls);
    area.removeFromTop(8);
    scopeViewport.setBounds(area);

    const int available = juce::jmax(ScopeStrip::kMinHeight, scopeViewport.getHeight());
    const int stripH = juce::jmax(ScopeStrip::kMinHeight, available / active);
    scopeList.setSize(scopeViewport.getMaximumVisibleWidth(), active * stripH);

    y = 0;
    for (int i = 0; i < active && i < (int) strips.size(); ++i)
    {
        strips[static_cast<size_t>(i)]->setBounds(0, y, scopeList.getWidth(), stripH);
        y += stripH;
    }
}

void BeatEqualizerAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateLayoutInfo();
    updateRowVisibility();
    updateAnalysisStatus();
    resized();
}

void BeatEqualizerAudioProcessorEditor::timerCallback()
{
    updateLayoutInfo();
    updateAnalysisStatus();
    updateWaveforms();
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
    return juce::jmin(audioProcessor.getTotalNumInputChannels(), beat::kMaxChannels);
}

void BeatEqualizerAudioProcessorEditor::updateWaveforms()
{
    const int active = activeChannelCount();
    const auto& ring = audioProcessor.getScope();
    const int captured = ring.length();
    const float timeMs = (scopeTimeParam != nullptr)
                             ? scopeTimeParam->load()
                             : beat::kDefaultScopeTimeMs;
    const int window = beat::ScopeRing::windowSamples(timeMs, audioProcessor.getCurrentSampleRate());
    if (active <= 0 || captured <= 0 || window <= 0 || window > captured)
        return;

    if ((int) scopeScratch.size() != captured)
        scopeScratch.resize(static_cast<size_t>(captured));
    if ((int) scopeWindow.size() != window)
        scopeWindow.resize(static_cast<size_t>(window));

    const int ref = juce::jlimit(0, active - 1, audioProcessor.getReferenceChannelIndex());
    ring.copyLast(ref, scopeScratch.data(), captured);

    constexpr float triggerLevel = 0.12f;
    int trigger = beat::ScopeRing::findRisingTrigger(scopeScratch.data(), captured, triggerLevel);
    int origin = captured - window;
    if (trigger >= 0)
        origin = juce::jlimit(0, captured - window, trigger - window / 5);

    for (int ch = 0; ch < active; ++ch)
    {
        ring.copyLast(ch, scopeScratch.data(), captured);
        std::copy(scopeScratch.begin() + origin,
                  scopeScratch.begin() + origin + window,
                  scopeWindow.begin());
        strips[static_cast<size_t>(ch)]->setWaveform(scopeWindow.data(), window);
        strips[static_cast<size_t>(ch)]->setReference(ch == ref);
    }

    const double sr = audioProcessor.getCurrentSampleRate();
    const double windowMs = (sr > 0.0) ? 1000.0 * (double) window / sr : 0.0;
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
        const bool on = i < active;
        rows[static_cast<size_t>(i)]->setActive(on);
        strips[static_cast<size_t>(i)]->setActive(on);
    }
}
