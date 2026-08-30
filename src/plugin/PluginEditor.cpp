#include "PluginEditor.h"

#include "dsp/Constants.h"

#include <algorithm>

BeatEqualizerAudioProcessorEditor::BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    title.setText("Beat Equalizer  |  scope", juce::dontSendNotification);
    title.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    layoutLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(layoutLabel);

    latencyLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(latencyLabel);

    hint.setText("Reaper: Track channels = N, insert this plugin, route each mic to 1..N. "
                 "Waveforms share time (trigger on Reference). Delay earlier mics until attacks line up; Invert flips the trace.",
                 juce::dontSendNotification);
    hint.setFont(juce::FontOptions(13.0f));
    hint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(hint);

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
    setupHeader(headerWave, "Output");
    setupHeader(headerDelay, "Delay (ms)");
    setupHeader(headerPolarity, "Polarity");
    addAndMakeVisible(headerOn);
    addAndMakeVisible(headerName);
    addAndMakeVisible(headerWave);
    addAndMakeVisible(headerDelay);
    addAndMakeVisible(headerPolarity);

    scopeScratch.resize(static_cast<size_t>(beat::ScopeRing::kLength));
    scopeWindow.resize(1024);

    auto& state = audioProcessor.getParameters();
    rows.reserve(static_cast<size_t>(beat::kMaxChannels));
    for (int i = 0; i < beat::kMaxChannels; ++i)
    {
        auto row = std::make_unique<ChannelRow>(state, i);
        rowList.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    viewport.setViewedComponent(&rowList, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.abBypass", abButton);
    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.reference", referenceBox);
    distanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.maxDistanceM", distanceSlider);

    audioProcessor.addChangeListener(this);
    updateLayoutInfo();
    updateRowVisibility();

    setResizable(true, true);
    setResizeLimits(780, 460, 1200, 1000);
    setSize(920, 640);
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
    referenceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    distanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
}

void BeatEqualizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto titleRow = area.removeFromTop(28);
    title.setBounds(titleRow.removeFromLeft(320));
    latencyLabel.setBounds(titleRow.removeFromRight(220));
    layoutLabel.setBounds(titleRow);

    area.removeFromTop(8);
    hint.setBounds(area.removeFromTop(36));
    area.removeFromTop(10);

    auto controls = area.removeFromTop(28);
    abButton.setBounds(controls.removeFromLeft(220));
    controls.removeFromLeft(12);
    referenceLabel.setBounds(controls.removeFromLeft(80));
    referenceBox.setBounds(controls.removeFromLeft(90));
    controls.removeFromLeft(12);
    distanceLabel.setBounds(controls.removeFromLeft(120));
    distanceSlider.setBounds(controls);

    area.removeFromTop(12);
    ChannelRow::layoutHeader(area.removeFromTop(20),
                             headerOn,
                             headerName,
                             headerWave,
                             headerDelay,
                             headerPolarity);

    viewport.setBounds(area);

    const int active = juce::jmax(1, audioProcessor.getTotalNumInputChannels());
    rowList.setSize(viewport.getMaximumVisibleWidth(), active * ChannelRow::kHeight);

    int y = 0;
    for (int i = 0; i < active && i < (int) rows.size(); ++i)
    {
        rows[static_cast<size_t>(i)]->setBounds(0, y, rowList.getWidth(), ChannelRow::kHeight);
        y += ChannelRow::kHeight;
    }
}

void BeatEqualizerAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateLayoutInfo();
    updateRowVisibility();
    resized();
}

void BeatEqualizerAudioProcessorEditor::timerCallback()
{
    updateLayoutInfo();
    updateWaveforms();
}

void BeatEqualizerAudioProcessorEditor::refreshWaveforms()
{
    updateWaveforms();
}

void BeatEqualizerAudioProcessorEditor::updateWaveforms()
{
    const int active = juce::jmin(audioProcessor.getTotalNumInputChannels(), beat::kMaxChannels);
    const int captured = (int) scopeScratch.size();
    const int window = (int) scopeWindow.size();
    if (active <= 0 || captured <= 0 || window <= 0)
        return;

    const auto& ring = audioProcessor.getScope();
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
        rows[static_cast<size_t>(ch)]->setWaveform(scopeWindow.data(), window);
    }
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
    const int active = juce::jmin(audioProcessor.getTotalNumInputChannels(), beat::kMaxChannels);
    for (int i = 0; i < beat::kMaxChannels; ++i)
        rows[static_cast<size_t>(i)]->setActive(i < active);
}
