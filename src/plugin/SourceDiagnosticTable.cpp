#include "SourceDiagnosticTable.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kChannelWidth = 48;
constexpr int kRoleWidth = 58;
constexpr int kUsageWidth = 92;
constexpr int kObsWidth = 48;
constexpr int kNaturalWidth = 82;
constexpr int kSpreadWidth = 70;
constexpr int kFullWidth = 74;
constexpr int kResidualWidth = 74;

void drawCell(juce::Graphics& g,
              juce::Rectangle<int>& row,
              int width,
              const juce::String& text,
              juce::Justification justification,
              juce::Colour colour)
{
    auto cell = row.removeFromLeft(width).reduced(4, 0);
    g.setColour(colour);
    g.drawFittedText(text, cell, justification, 1);
}
} // namespace

int SourceDiagnosticTable::heightForRows(int rows)
{
    return kHeaderHeight
           + juce::jlimit(1, beat::kMaxChannels, rows) * kRowHeight;
}

beat::ChannelRole SourceDiagnosticTable::roleFromParameter(std::atomic<float>* parameter)
{
    if (parameter == nullptr)
        return beat::ChannelRole::unknown;

    const int choice = juce::roundToInt(parameter->load(std::memory_order_relaxed));
    switch (choice)
    {
        case 1: return beat::ChannelRole::close;
        case 2: return beat::ChannelRole::overhead;
        case 3: return beat::ChannelRole::room;
        case 4: return beat::ChannelRole::hats;
        default: return beat::ChannelRole::unknown;
    }
}

juce::String SourceDiagnosticTable::roleLabel(beat::ChannelRole role)
{
    switch (role)
    {
        case beat::ChannelRole::close: return "Close";
        case beat::ChannelRole::overhead: return "OH";
        case beat::ChannelRole::room: return "Room";
        case beat::ChannelRole::hats: return "Hats";
        case beat::ChannelRole::unknown:
        default: return "-";
    }
}

juce::String SourceDiagnosticTable::usageLabel(const beat::doc::SourceDiagnostic& diagnostic,
                                               int channel,
                                               beat::ChannelRole role)
{
    if (channel == diagnostic.sourceChannel)
        return "source";

    const auto& row = diagnostic.channels[static_cast<size_t>(channel)];
    if (row.observations <= 0)
        return "no hit";

    if (channel == diagnostic.closeChannel)
        return "close pair";

    if (role == beat::ChannelRole::overhead)
        return "OH return";

    if (role == beat::ChannelRole::room)
        return "room return";

    if (channel == diagnostic.lateChannel)
        return "late";

    return "bleed";
}

void SourceDiagnosticTable::setDiagnostic(
    const beat::doc::SourceDiagnostic* diagnostic,
    double sampleRate,
    int channels,
    const std::array<beat::ChannelRole, beat::kMaxChannels>& channelRoles)
{
    rowCount = juce::jlimit(1, beat::kMaxChannels, channels);

    const bool valid = diagnostic != nullptr && diagnostic->valid && sampleRate > 0.0;
    for (int ch = 0; ch < rowCount; ++ch)
    {
        auto& row = rows[static_cast<size_t>(ch)];
        row = Row {};
        row.channel = "Ch " + juce::String(ch + 1).paddedLeft('0', 2);
        row.role = roleLabel(channelRoles[static_cast<size_t>(ch)]);

        if (!valid)
        {
            row.usage = "-";
            row.observations = "-";
            row.naturalMs = "-";
            row.spreadMs = "-";
            row.fullMs = "-";
            row.residualMs = "-";
            row.calibrated = "-";
            continue;
        }

        const auto& source = *diagnostic;
        const auto& stats = source.channels[static_cast<size_t>(ch)];
        row.usage = usageLabel(source, ch, channelRoles[static_cast<size_t>(ch)]);
        row.source = ch == source.sourceChannel;
        row.returnChannel = row.usage.containsIgnoreCase("return");
        row.observations = stats.observations > 0 ? juce::String(stats.observations) : "-";
        row.calibrated = stats.calibrated ? "yes" : "-";

        if (stats.observations <= 0)
        {
            row.naturalMs = "-";
            row.spreadMs = "-";
            row.fullMs = "-";
            row.residualMs = "-";
            continue;
        }

        row.naturalMs = msString(stats.naturalOffsetSamples, sampleRate);
        row.spreadMs = msString(stats.rawSpreadSamples, sampleRate);
        row.fullMs = msString(stats.fullAlignOffsetSamples, sampleRate);
        row.residualMs = (ch != source.sourceChannel && stats.calibrated)
                             ? msString(stats.calibrationResidualSamples, sampleRate)
                             : "-";
    }

    rebuildPlainText();
    repaint();
}

void SourceDiagnosticTable::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff12151a));
    g.fillRect(bounds);
    g.setColour(juce::Colour(0xff2a3038));
    g.drawRect(bounds);

    const auto headerColour = juce::Colour(0xff8b919c);
    const auto textColour = juce::Colour(0xffc5cad3);
    const auto sourceColour = juce::Colour(0xff7ddc9a);
    const auto returnColour = juce::Colour(0xff5ec8ff);
    const auto dimColour = juce::Colour(0xff6b7280);

    auto header = bounds.removeFromTop(kHeaderHeight);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    drawCell(g, header, kChannelWidth, "Ch", juce::Justification::centredLeft, headerColour);
    drawCell(g, header, kRoleWidth, "Role", juce::Justification::centredLeft, headerColour);
    drawCell(g, header, kUsageWidth, "Use", juce::Justification::centredLeft, headerColour);
    drawCell(g, header, kObsWidth, "Obs", juce::Justification::centredRight, headerColour);
    drawCell(g, header, kNaturalWidth, "Natural", juce::Justification::centredRight, headerColour);
    drawCell(g, header, kSpreadWidth, "Spread", juce::Justification::centredRight, headerColour);
    drawCell(g, header, kFullWidth, "Full", juce::Justification::centredRight, headerColour);
    drawCell(g,
             header,
             kResidualWidth,
             "Residual",
             juce::Justification::centredRight,
             headerColour);
    drawCell(g, header, header.getWidth(), "Cal", juce::Justification::centredRight, headerColour);

    g.setFont(juce::FontOptions(12.0f));
    for (int i = 0; i < rowCount; ++i)
    {
        auto rowBounds = bounds.removeFromTop(kRowHeight);
        if (i % 2 == 0)
        {
            g.setColour(juce::Colour(0xff161a20));
            g.fillRect(rowBounds);
        }

        auto cells = rowBounds;
        const auto& row = rows[static_cast<size_t>(i)];
        const auto colour = row.source ? sourceColour
                          : row.returnChannel ? returnColour
                                              : textColour;
        const bool empty = row.observations == "-";
        drawCell(g, cells, kChannelWidth, row.channel, juce::Justification::centredLeft, colour);
        drawCell(g, cells, kRoleWidth, row.role, juce::Justification::centredLeft, colour);
        drawCell(g,
                 cells,
                 kUsageWidth,
                 row.usage,
                 juce::Justification::centredLeft,
                 empty ? dimColour : colour);
        drawCell(g,
                 cells,
                 kObsWidth,
                 row.observations,
                 juce::Justification::centredRight,
                 empty ? dimColour : textColour);
        drawCell(g,
                 cells,
                 kNaturalWidth,
                 row.naturalMs,
                 juce::Justification::centredRight,
                 empty ? dimColour : textColour);
        drawCell(g,
                 cells,
                 kSpreadWidth,
                 row.spreadMs,
                 juce::Justification::centredRight,
                 empty ? dimColour : textColour);
        drawCell(g,
                 cells,
                 kFullWidth,
                 row.fullMs,
                 juce::Justification::centredRight,
                 empty ? dimColour : textColour);
        drawCell(g,
                 cells,
                 kResidualWidth,
                 row.residualMs,
                 juce::Justification::centredRight,
                 empty ? dimColour : textColour);
        drawCell(g,
                 cells,
                 cells.getWidth(),
                 row.calibrated,
                 juce::Justification::centredRight,
                 row.calibrated == "yes" ? textColour : dimColour);
    }
}

juce::String SourceDiagnosticTable::msString(double samples, double sampleRate)
{
    return juce::String(1000.0 * samples / sampleRate, 2);
}

void SourceDiagnosticTable::rebuildPlainText()
{
    plainText = "Ch\tRole\tUse\tObs\tNatural ms\tSpread ms\tFull ms\tResidual ms\tCal";
    for (int i = 0; i < rowCount; ++i)
    {
        const auto& row = rows[static_cast<size_t>(i)];
        plainText += "\n" + row.channel + "\t" + row.role + "\t" + row.usage + "\t"
                     + row.observations + "\t" + row.naturalMs + "\t" + row.spreadMs
                     + "\t" + row.fullMs + "\t" + row.residualMs + "\t"
                     + row.calibrated;
    }
}
