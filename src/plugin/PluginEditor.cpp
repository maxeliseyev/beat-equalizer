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

    note.setText("PR 1 skeleton: N-in / N-out passthrough. Analysis lands in later PRs.",
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

    auto& state = audioProcessor.getParameters();
    referenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "global.reference", referenceBox);
    distanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "global.maxDistanceM", distanceSlider);
    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.abBypass", abButton);
    monoSumAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "global.monoSum", monoSumButton);

    audioProcessor.addChangeListener(this);
    updateChannelLabel();

    setSize(720, 280);
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
