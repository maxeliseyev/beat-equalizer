#pragma once

#include "dsp/Constants.h"

#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

// Офлайн-рендер выровненного материала: та же цепочка, что в processBlock
// (инверсия, дробная задержка, ротатор), но без сглаживания и без A/B.
namespace beat::exporter
{

struct ChannelSettings
{
    float delaySamples = 0.0f;
    bool invert = false;
    float rotatorHz = kDefaultRotatorHz;
    float rotatorAmount = 0.0f;
};

// Результат длиннее исходника на максимальную задержку: хвост не режем.
void renderAligned(const juce::AudioBuffer<float>& source,
                   double sampleRate,
                   const std::vector<ChannelSettings>& settings,
                   juce::AudioBuffer<float>& destination);

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);

} // namespace beat::exporter
