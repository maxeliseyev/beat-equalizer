#pragma once

#include "dsp/Grid.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

// Найденный удар на осциллограмме. Хозяин показывается ярче остальных:
// событие видно во многих микрофонах, но принадлежит одному — по этому и
// отличают прямой звук от просачивания.
struct ScopeMarker
{
    float position = 0.0f; // 0…1 внутри окна
    bool owned = false;
    float confidence = 0.0f;
};

class ScopeStrip final : public juce::Component
{
public:
    static constexpr int kMaxMarkers = 256;

    static constexpr int kMinHeight = 88;
    static constexpr int kLabelWidth = 40;

    explicit ScopeStrip(int channelIndex);

    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);
    void setReference(bool isReference);
    // Внутри строки канала номер уже нарисован колонкой Ch.
    void setShowIndex(bool shouldShow);
    void setWaveform(const float* samples, int count);
    // Сетка одна на все строки: её считает редактор по темпу и позиции.
    void setGrid(const beat::grid::Line* lines, int count);
    int getGridCount() const { return gridCount; }
    // Маркеры считает редактор: положение удара зависит от окна и от сдвига
    // канала, а строка про них не знает.
    void setMarkers(const ScopeMarker* markers, int count);
    int getMarkerCount() const { return markerCount; }
    const ScopeMarker& getMarker(int i) const { return markers[static_cast<size_t>(i)]; }

private:
    juce::Rectangle<int> waveBounds() const;

    int index = 0;
    bool reference = false;
    bool showIndex = true;
    std::vector<float> waveform;
    std::array<beat::grid::Line, beat::grid::kMaxLines> grid {};
    int gridCount = 0;
    std::array<ScopeMarker, kMaxMarkers> markers {};
    int markerCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeStrip)
};
