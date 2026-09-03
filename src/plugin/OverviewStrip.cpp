#include "OverviewStrip.h"

#include <algorithm>
#include <cmath>

OverviewStrip::OverviewStrip()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<int> OverviewStrip::waveBounds() const
{
    return getLocalBounds().reduced(1, 1);
}

void OverviewStrip::setOverview(const float* newPeaks, int count)
{
    if (newPeaks == nullptr || count <= 0)
    {
        if (!peaks.empty())
        {
            peaks.clear();
            repaint();
        }
        return;
    }

    peaks.assign(newPeaks, newPeaks + count);
    repaint();
}

void OverviewStrip::setMarkers(const float* positions, int count)
{
    if (positions == nullptr || count <= 0)
    {
        if (!markers.empty())
        {
            markers.clear();
            repaint();
        }
        return;
    }

    markers.assign(positions, positions + count);
    repaint();
}

void OverviewStrip::setPlayhead(double normalised)
{
    const double clamped = juce::jlimit(0.0, 1.0, normalised);
    if (std::abs(clamped - playhead) < 1.0e-6)
        return;

    playhead = clamped;
    repaint();
}

void OverviewStrip::setWindow(double startNormalised, double endNormalised)
{
    const double from = juce::jlimit(0.0, 1.0, startNormalised);
    const double to = juce::jlimit(0.0, 1.0, endNormalised);
    if (std::abs(from - windowStart) < 1.0e-6 && std::abs(to - windowEnd) < 1.0e-6)
        return;

    windowStart = from;
    windowEnd = to;
    repaint();
}

void OverviewStrip::setTotalSeconds(double seconds)
{
    if (std::abs(seconds - totalSeconds) < 1.0e-3)
        return;

    totalSeconds = juce::jmax(0.0, seconds);
    repaint();
}

void OverviewStrip::seekFromMouse(const juce::MouseEvent& event)
{
    const auto bounds = waveBounds();
    if (bounds.getWidth() <= 1 || !onSeek)
        return;

    const double x = static_cast<double>(event.position.x) - static_cast<double>(bounds.getX());
    onSeek(juce::jlimit(0.0, 1.0, x / static_cast<double>(bounds.getWidth() - 1)));
}

void OverviewStrip::mouseDown(const juce::MouseEvent& event)
{
    seekFromMouse(event);
}

void OverviewStrip::mouseDrag(const juce::MouseEvent& event)
{
    seekFromMouse(event);
}

void OverviewStrip::paint(juce::Graphics& g)
{
    const auto bounds = waveBounds();
    g.setColour(juce::Colour(0xff07090c));
    g.fillRect(bounds);
    g.setColour(juce::Colour(0xff2b333d));
    g.drawRect(getLocalBounds());

    if (bounds.getWidth() < 2 || bounds.getHeight() < 4)
        return;

    const int width = bounds.getWidth();
    const float midY = (float) bounds.getCentreY();
    const float half = (float) bounds.getHeight() * 0.5f - 2.0f;

    // Отметки времени: по ним длина партии читается без чтения цифр. Шаг
    // выбирается так, чтобы их было полтора-три десятка на любой длине.
    if (totalSeconds > 0.0)
    {
        const double steps[] = { 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0 };
        double step = steps[std::size(steps) - 1];
        for (double candidate : steps)
        {
            if (totalSeconds / candidate <= 24.0)
            {
                step = candidate;
                break;
            }
        }

        g.setColour(juce::Colour(0xff20272f));
        for (double t = step; t < totalSeconds; t += step)
        {
            const int x = bounds.getX()
                          + juce::roundToInt((t / totalSeconds) * (double) (width - 1));
            g.drawVerticalLine(x, (float) bounds.getY() + 1.0f, (float) bounds.getBottom() - 1.0f);
        }
    }

    if (peaks.empty())
    {
        g.setColour(juce::Colour(0xff9aa3ad));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("no material loaded", bounds, juce::Justification::centred, false);
        return;
    }

    // Окно строк каналов: показывает, какой кусок партии сейчас на трассах.
    if (windowEnd > windowStart)
    {
        const int from = bounds.getX() + juce::roundToInt(windowStart * (double) (width - 1));
        const int to = bounds.getX() + juce::roundToInt(windowEnd * (double) (width - 1));
        g.setColour(juce::Colour(0x2233435a));
        g.fillRect(from, bounds.getY(), juce::jmax(2, to - from), bounds.getHeight());
    }

    const int count = (int) peaks.size();
    float top = 0.0f;
    for (float peak : peaks)
        top = std::max(top, peak);

    const float scale = half / std::max(top, 0.05f);
    const int headX = bounds.getX() + juce::roundToInt(playhead * (double) (width - 1));

    for (int x = 0; x < width; ++x)
    {
        const int i0 = x * count / width;
        const int i1 = std::max(i0 + 1, (x + 1) * count / width);

        float peak = 0.0f;
        for (int i = i0; i < i1 && i < count; ++i)
            peak = std::max(peak, peaks[static_cast<size_t>(i)]);

        // Сыгранное ярче оставшегося: видно не только где стоим, но и сколько
        // прошло.
        g.setColour(bounds.getX() + x <= headX ? juce::Colour(0xff5ec8ff)
                                               : juce::Colour(0xff3c5a72));

        const float height = std::max(1.0f, peak * scale);
        g.fillRect((float) (bounds.getX() + x), midY - height, 1.0f, 2.0f * height);
    }

    // Удары — короткими штрихами сверху: на полной партии их сотни, и во всю
    // высоту они закрыли бы волну.
    if (!markers.empty())
    {
        g.setColour(juce::Colour(0x99e8c547));
        const float tickBottom = (float) bounds.getY() + (float) bounds.getHeight() * 0.28f;
        for (float marker : markers)
        {
            const int x = bounds.getX()
                          + juce::roundToInt(juce::jlimit(0.0f, 1.0f, marker)
                                             * (float) (width - 1));
            g.drawVerticalLine(x, (float) bounds.getY() + 1.0f, tickBottom);
        }
    }

    g.setColour(juce::Colour(0xffe8c547));
    g.drawVerticalLine(headX, (float) bounds.getY(), (float) bounds.getBottom());
}
