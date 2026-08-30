#include "PluginEditor.h"

#include "dsp/Constants.h"

BeatEqualizerAudioProcessorEditor::BeatEqualizerAudioProcessorEditor(BeatEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    title.setText("Beat Equalizer", juce::dontSendNotification);
    title.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    channelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(channelLabel);

    note.setText("PR 3: manual delay + invert + A/B with host PDC. Analyze is PR 4.",
                 juce::dontSendNotification);
    note.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(note);

    referenceLabel.setText("Reference", juce::dontSendNotification);
    addAndMakeVisible(referenceLabel);

    for (int i = 1; i <= beat::kMaxChannels; ++i)
        referenceBox.addItem("Ch " + juce::String(i), i);

    addAndMakeVisible(referenceBox);

    distanceLabel.setText("Max distance (m)", juce::dontSendNotification);
    addAndMakeVisible(distanceLabel);

    distanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    addAndMakeVisible(distanceSlider);

    addAndMakeVisible(abButton);
    addAndMakeVisible(monoSumButton);

    ch1DelayLabel.setText("Ch 1 delay (ms)", juce::dontSendNotification);
    addAndMakeVisible(ch1DelayLabel);
    ch1DelaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ch1DelaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    addAndMakeVisible(ch1DelaySlider);

    ch2DelayLabel.setText("Ch 2 delay (ms)", juce::dontSendNotification);
    addAndMakeVisible(ch2DelayLabel);
    ch2DelaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ch2DelaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    addAndMakeVisible(ch2DelaySlider);

    ch2PolarityLabel.setText("Ch 2 polarity", juce::dontSendNotification);
    addAndMakeVisible(ch2PolarityLabel);
    ch2PolarityBox.addItem("Auto", 1);
    ch2PolarityBox.addItem("Positive", 2);
    ch2PolarityBox.addItem("Invert", 3);
    addAndMakeVisible(ch2PolarityBox);

    auto& state = audioProcessor.getParameters();
    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.reference", referenceBox);
    distanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.maxDistanceM", distanceSlider);
    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.abBypass", abButton);
    monoSumAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.monoSum", monoSumButton);
    ch1DelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(0, "delayMs"), ch1DelaySlider);
    ch2DelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(1, "delayMs"), ch2DelaySlider);
    ch2PolarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(1, "polarity"), ch2PolarityBox);

    audioProcessor.addChangeListener(this);
    updateChannelLabel();

    setSize(720, 420);
}

BeatEqualizerAudioProcessorEditor::~BeatEqualizerAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
}

void BeatEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16181d));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc5cad3));
    note.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
    referenceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    distanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    ch1DelayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    ch2DelayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    ch2PolarityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
}

void BeatEqualizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    title.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);
    channelLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    note.setBounds(area.removeFromTop(24));
    area.removeFromTop(16);

    auto row = area.removeFromTop(28);
    referenceLabel.setBounds(row.removeFromLeft(100));
    referenceBox.setBounds(row.removeFromLeft(140));

    area.removeFromTop(12);
    row = area.removeFromTop(28);
    distanceLabel.setBounds(row.removeFromLeft(140));
    distanceSlider.setBounds(row);

    area.removeFromTop(16);
    row = area.removeFromTop(28);
    abButton.setBounds(row.removeFromLeft(140));
    monoSumButton.setBounds(row.removeFromLeft(140));

    area.removeFromTop(16);
    row = area.removeFromTop(28);
    ch1DelayLabel.setBounds(row.removeFromLeft(140));
    ch1DelaySlider.setBounds(row);

    area.removeFromTop(12);
    row = area.removeFromTop(28);
    ch2DelayLabel.setBounds(row.removeFromLeft(140));
    ch2DelaySlider.setBounds(row);

    area.removeFromTop(12);
    row = area.removeFromTop(28);
    ch2PolarityLabel.setBounds(row.removeFromLeft(140));
    ch2PolarityBox.setBounds(row.removeFromLeft(140));
}

void BeatEqualizerAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateChannelLabel();
}

void BeatEqualizerAudioProcessorEditor::updateChannelLabel()
{
    const int channels = audioProcessor.getTotalNumInputChannels();
    channelLabel.setText("Layout: " + juce::String(channels) + " in / "
                             + juce::String(audioProcessor.getTotalNumOutputChannels()) + " out  (supports "
                             + juce::String(beat::kMinChannels) + "–"
                             + juce::String(beat::kMaxChannels) + ")",
                         juce::dontSendNotification);
}
