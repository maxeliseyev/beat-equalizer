#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class ScopeStrip final : public juce::Component
{
public:
    static constexpr int kMinHeight = 88;
    static constexpr int kLabelWidth = 40;

    explicit ScopeStrip(int channelIndex);

    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);
    void setReference(bool isReference);
    void setWaveform(const float* samples, int count);

private:
    juce::Rectangle<int> waveBounds() const;

    int index = 0;
    bool reference = false;
    std::vector<float> waveform;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeStrip)
};
