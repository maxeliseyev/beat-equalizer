#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Полоса транспорта стенда: вся партия целиком, позиция воспроизведения и
// участок, который сейчас видно в строках каналов. Без неё в Standalone не
// понять ни длины материала, ни того, что осталось впереди.
class OverviewStrip final : public juce::Component
{
public:
    static constexpr int kHeight = 46;

    OverviewStrip();

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    void setOverview(const float* peaks, int count);
    void setPlayhead(double normalised);
    // Границы окна осциллограмм, 0…1 от длины партии.
    void setWindow(double startNormalised, double endNormalised);
    void setTotalSeconds(double seconds);

    std::function<void(double)> onSeek;

    double getPlayhead() const { return playhead; }
    int getOverviewSize() const { return (int) peaks.size(); }

private:
    juce::Rectangle<int> waveBounds() const;
    void seekFromMouse(const juce::MouseEvent& event);

    std::vector<float> peaks;
    double playhead = 0.0;
    double windowStart = 0.0;
    double windowEnd = 0.0;
    double totalSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverviewStrip)
};
