#include "ChannelRow.h"

#include <algorithm>
#include <cmath>

void ChannelRow::layoutHeader(juce::Rectangle<int> row,
                              juce::Label& on,
                              juce::Label& name,
                              juce::Label& wave,
                              juce::Label& delay,
                              juce::Label& polarity)
{
    on.setBounds(row.removeFromLeft(kEnableWidth));
    name.setBounds(row.removeFromLeft(kNameWidth));
    wave.setBounds(row.removeFromLeft(kWaveWidth));
    polarity.setBounds(row.removeFromRight(kPolarityWidth));
    delay.setBounds(row);
}

ChannelRow::ChannelRow(juce::AudioProcessorValueTreeState& state, int index)
{
    enabledButton.setClickingTogglesState(true);
    addAndMakeVisible(enabledButton);

    nameLabel.setText(juce::String::formatted("%02d", index + 1), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    delaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    delaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    delaySlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(delaySlider);

    polarityBox.addItem("Auto", 1);
    polarityBox.addItem("Positive", 2);
    polarityBox.addItem("Invert", 3);
    addAndMakeVisible(polarityBox);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "enabled"), enabledButton);
    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "delayMs"), delaySlider);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "polarity"), polarityBox);

    setActive(false);
}

juce::Rectangle<int> ChannelRow::waveBounds() const
{
    return getLocalBounds()
        .withTrimmedLeft(kEnableWidth + kNameWidth)
        .removeFromLeft(kWaveWidth)
        .reduced(2, 4);
}

void ChannelRow::resized()
{
    auto row = getLocalBounds();
    enabledButton.setBounds(row.removeFromLeft(kEnableWidth).reduced(2, 8));
    nameLabel.setBounds(row.removeFromLeft(kNameWidth));
    row.removeFromLeft(kWaveWidth);
    polarityBox.setBounds(row.removeFromRight(kPolarityWidth).reduced(2, 10));
    delaySlider.setBounds(row.reduced(4, 12));
}

void ChannelRow::paint(juce::Graphics& g)
{
    g.setColour(active ? juce::Colour(0xff1e222a) : juce::Colour(0xff14161b));
    g.fillRect(getLocalBounds());

    auto bounds = waveBounds();
    g.setColour(juce::Colour(0xff07090c));
    g.fillRect(bounds);
    g.setColour(juce::Colour(0xff4a5a6a));
    g.drawRect(bounds);

    const float midY = (float) bounds.getCentreY();
    g.setColour(juce::Colour(0xff3a4450));
    g.drawHorizontalLine((int) midY, (float) bounds.getX() + 1.0f, (float) bounds.getRight() - 1.0f);

    const auto drawPlaceholder = [&g, bounds]()
    {
        g.setColour(juce::Colour(0xff9aa3ad));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("play to see waveform", bounds, juce::Justification::centred, false);
    };

    if (waveform.size() < 2 || bounds.getWidth() < 2)
    {
        drawPlaceholder();
        return;
    }

    const int count = (int) waveform.size();
    const int width = bounds.getWidth();
    const float height = (float) bounds.getHeight() * 0.5f - 1.0f;

    float peak = 0.0f;
    for (float sample : waveform)
        peak = std::max(peak, std::abs(sample));

    if (peak < 0.002f)
    {
        drawPlaceholder();
        return;
    }

    const float scale = height / std::max(peak, 0.08f);
    const int innerTop = bounds.getY() + 1;
    const int innerBottom = bounds.getBottom() - 1;

    for (int x = 0; x < width; ++x)
    {
        const int i0 = x * count / width;
        const int i1 = std::max(i0 + 1, (x + 1) * count / width);
        float lo = 1.0f;
        float hi = -1.0f;
        for (int i = i0; i < i1 && i < count; ++i)
        {
            lo = std::min(lo, waveform[static_cast<size_t>(i)]);
            hi = std::max(hi, waveform[static_cast<size_t>(i)]);
        }

        const bool clipped = hi > 1.0f || lo < -1.0f;
        g.setColour(clipped ? juce::Colour(0xffe05d5d) : juce::Colour(0xff5ec8ff));

        const float y0 = juce::jlimit((float) innerTop, (float) innerBottom, midY - hi * scale);
        const float y1 = juce::jlimit((float) innerTop, (float) innerBottom, midY - lo * scale);
        const int top = (int) std::floor(std::min(y0, y1));
        const int bottom = (int) std::ceil(std::max(y0, y1));
        g.fillRect(bounds.getX() + x, top, 1, std::max(2, bottom - top + 1));
    }
}

void ChannelRow::setActive(bool shouldBeActive)
{
    active = shouldBeActive;
    setVisible(shouldBeActive);
    setEnabled(shouldBeActive);
    nameLabel.setColour(juce::Label::textColourId,
                        shouldBeActive ? juce::Colours::white : juce::Colour(0xff6b7280));
}

void ChannelRow::setWaveform(const float* samples, int count)
{
    if (samples == nullptr || count <= 0)
    {
        if (!waveform.empty())
        {
            waveform.clear();
            repaint(waveBounds());
        }
        return;
    }

    waveform.assign(samples, samples + count);
    repaint(waveBounds());
}
