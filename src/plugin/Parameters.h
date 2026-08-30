#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace beat
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

juce::String channelParamId(int channelIndex, const juce::String& suffix);

} // namespace beat
