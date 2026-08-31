#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// Гониометр и полоса корреляции для пары «опора против суммы остальных».
// Мера та же, что в колонке corr, но здесь видно и характер: узкий крест —
// каналы почти совпали, шар — они друг другу чужие.
class Correlometer final : public juce::Component
{
public:
    static constexpr int kHeight = 96;

    void setPair(const float* reference, const float* sum, int count);
    void paint(juce::Graphics&) override;

    float getCorrelation() const { return correlation; }
    int getPointCount() const { return static_cast<int>(points.size()); }

private:
    static constexpr int kMaxPoints = 512;

    std::vector<juce::Point<float>> points;
    float correlation = 0.0f;
    float scale = 1.0f;
};
