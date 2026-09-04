#pragma once

#include "doc/SourceDiagnostic.h"
#include "dsp/Constants.h"

#include <array>
#include <atomic>

#include <juce_gui_basics/juce_gui_basics.h>

class SourceDiagnosticTable final : public juce::Component
{
public:
    static constexpr int kHeaderHeight = 20;
    static constexpr int kRowHeight = 20;

    static int heightForRows(int rows);
    static beat::ChannelRole roleFromParameter(std::atomic<float>* parameter);
    static juce::String roleLabel(beat::ChannelRole role);
    static juce::String usageLabel(const beat::doc::SourceDiagnostic& diagnostic,
                                   int channel,
                                   beat::ChannelRole role);

    void setDiagnostic(const beat::doc::SourceDiagnostic* diagnostic,
                       double sampleRate,
                       int channels,
                       const std::array<beat::ChannelRole, beat::kMaxChannels>& channelRoles);
    void paint(juce::Graphics&) override;

    juce::String getText() const { return plainText; }
    int getRowCount() const { return rowCount; }

private:
    struct Row
    {
        juce::String channel;
        juce::String role;
        juce::String usage;
        juce::String observations;
        juce::String naturalMs;
        juce::String spreadMs;
        juce::String fullMs;
        juce::String residualMs;
        juce::String calibrated;
        bool source = false;
        bool returnChannel = false;
    };

    static juce::String msString(double samples, double sampleRate);
    void rebuildPlainText();

    std::array<Row, beat::kMaxChannels> rows {};
    int rowCount = 0;
    juce::String plainText;
};
