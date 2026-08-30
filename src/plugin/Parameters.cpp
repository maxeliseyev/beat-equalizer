#include "Parameters.h"

#include "dsp/Constants.h"

namespace beat
{

juce::String channelParamId(int channelIndex, const juce::String& suffix)
{
    return juce::String::formatted("ch%02d.", channelIndex + 1) + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "global.reference", 1 },
        "Reference Channel",
        1,
        kMaxChannels,
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "global.maxDistanceM", 1 },
        "Max Distance",
        juce::NormalisableRange<float>(0.5f, 10.0f, 0.01f, 0.5f),
        kDefaultMaxDistanceM));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "global.abBypass", 1 },
        "A/B Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "global.monoSum", 1 },
        "Mono Sum",
        false));

    const juce::StringArray polarityLabels { "Auto", "Positive", "Invert" };

    for (int i = 0; i < kMaxChannels; ++i)
    {
        const auto namePrefix = juce::String::formatted("Ch %02d ", i + 1);

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { channelParamId(i, "enabled"), 1 },
            namePrefix + "Enabled",
            true));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { channelParamId(i, "delayMs"), 1 },
            namePrefix + "Delay",
            juce::NormalisableRange<float>(0.0f, kMaxDelayMs, 0.001f),
            0.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { channelParamId(i, "polarity"), 1 },
            namePrefix + "Polarity",
            polarityLabels,
            0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { channelParamId(i, "rotatorAmount"), 1 },
            namePrefix + "Rotator",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { channelParamId(i, "rotatorHz"), 1 },
            namePrefix + "Rotator Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
            600.0f));
    }

    return { params.begin(), params.end() };
}

} // namespace beat
