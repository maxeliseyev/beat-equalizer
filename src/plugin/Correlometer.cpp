#include "Correlometer.h"

#include "dsp/Correlation.h"

#include <algorithm>
#include <cmath>

void Correlometer::setPair(const float* reference, const float* sum, int count)
{
    points.clear();
    correlation = 0.0f;
    scale = 1.0f;

    if (reference == nullptr || sum == nullptr || count <= 0)
        return;

    const int stride = std::max(1, count / kMaxPoints);
    correlation = beat::correlation(reference, sum, count, stride);

    float peak = 0.0f;
    for (int i = 0; i < count; i += stride)
    {
        points.push_back({ reference[i], sum[i] });
        peak = std::max({ peak, std::abs(reference[i]), std::abs(sum[i]) });
    }

    scale = (peak > 1.0e-4f) ? 1.0f / peak : 1.0f;
}

void Correlometer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff14161b));
    g.fillRect(bounds);

    auto square = bounds.removeFromLeft(bounds.getHeight()).reduced(6);
    const auto centre = square.getCentre().toFloat();
    const float radius = 0.5f * static_cast<float>(std::min(square.getWidth(), square.getHeight()));

    g.setColour(juce::Colour(0xff2a2f3a));
    g.drawEllipse(square.toFloat(), 1.0f);
    g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);
    g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);

    // Классический поворот на 45 градусов: вертикаль — синфазная сумма,
    // горизонталь — расхождение. Противофаза растягивает точки поперёк.
    g.setColour(juce::Colour(0x995ec8ff));
    for (const auto& point : points)
    {
        const float a = point.x * scale;
        const float b = point.y * scale;
        const float x = centre.x + radius * 0.70711f * (a - b);
        const float y = centre.y - radius * 0.70711f * (a + b);
        g.fillRect(x - 0.75f, y - 0.75f, 1.5f, 1.5f);
    }

    auto meter = bounds.reduced(10, 0);
    auto text = meter.removeFromRight(70);
    auto bar = meter.withSizeKeepingCentre(meter.getWidth(), 16);

    g.setColour(juce::Colour(0xff2a2f3a));
    g.fillRect(bar);

    const float middle = static_cast<float>(bar.getCentreX());
    const float half = 0.5f * static_cast<float>(bar.getWidth());
    const float end = middle + half * correlation;

    g.setColour(correlation < 0.0f ? juce::Colour(0xffe06c75) : juce::Colour(0xff7ddc9a));
    g.fillRect(juce::Rectangle<float>(std::min(middle, end),
                                      static_cast<float>(bar.getY()),
                                      std::abs(end - middle),
                                      static_cast<float>(bar.getHeight())));

    g.setColour(juce::Colour(0xff8b919c));
    g.drawLine(middle, static_cast<float>(bar.getY()) - 4.0f, middle,
               static_cast<float>(bar.getBottom()) + 4.0f, 1.0f);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("-1", bar.getX(), bar.getBottom() + 2, 24, 14, juce::Justification::centredLeft);
    g.drawText("+1", bar.getRight() - 24, bar.getBottom() + 2, 24, 14,
               juce::Justification::centredRight);
    g.drawText("Reference vs sum of the rest", bar.getX(), bar.getY() - 18, bar.getWidth(), 14,
               juce::Justification::centredLeft);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText(juce::String(correlation, 2), text, juce::Justification::centredRight);
}
