#include "ScopeStrip.h"

#include <algorithm>
#include <cmath>

ScopeStrip::ScopeStrip(int channelIndex)
    : index(channelIndex)
{
    setInterceptsMouseClicks(false, false);
    setActive(false);
}

juce::Rectangle<int> ScopeStrip::waveBounds() const
{
    return getLocalBounds().withTrimmedLeft(showIndex ? kLabelWidth : 0).reduced(2, 6);
}

void ScopeStrip::paint(juce::Graphics& g)
{
    g.setColour(reference ? juce::Colour(0xff1c2430) : juce::Colour(0xff14181f));
    g.fillRect(getLocalBounds());

    if (showIndex)
    {
        auto label = getLocalBounds().removeFromLeft(kLabelWidth);
        g.setColour(reference ? juce::Colour(0xff5ec8ff) : juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String::formatted("%02d", index + 1),
                   label,
                   juce::Justification::centred,
                   false);
    }

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
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("play to see waveform", bounds, juce::Justification::centred, false);
    };

    if (waveform.size() < 2 || bounds.getWidth() < 2)
    {
        drawPlaceholder();
        return;
    }

    const int count = (int) waveform.size();
    const int width = bounds.getWidth();
    const float height = (float) bounds.getHeight() * 0.5f - 2.0f;

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

void ScopeStrip::setActive(bool shouldBeActive)
{
    setVisible(shouldBeActive);
}

void ScopeStrip::setShowIndex(bool shouldShow)
{
    if (showIndex == shouldShow)
        return;

    showIndex = shouldShow;
    repaint();
}

void ScopeStrip::setReference(bool isReference)
{
    if (reference == isReference)
        return;

    reference = isReference;
    repaint();
}

void ScopeStrip::setWaveform(const float* samples, int count)
{
    if (samples == nullptr || count <= 0)
    {
        if (!waveform.empty())
        {
            waveform.clear();
            repaint();
        }
        return;
    }

    waveform.assign(samples, samples + count);
    repaint(waveBounds());
}
