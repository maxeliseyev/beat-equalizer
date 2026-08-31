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
        juce::NormalisableRange<float>(kMinDistanceM, kMaxDistanceM, 0.01f, 0.5f),
        kDefaultMaxDistanceM));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "global.abBypass", 1 },
        "A/B Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "global.freeze", 1 },
        "Freeze",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "global.monoSum", 1 },
        "Mono Sum",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "global.scopeTimeMs", 1 },
        "Time",
        juce::NormalisableRange<float>(kMinScopeTimeMs, kMaxScopeTimeMs, 0.1f, 0.4f),
        kDefaultScopeTimeMs));

    const juce::StringArray polarityLabels { "Auto", "Positive", "Invert" };
    // Роль — метаданные этапа 1: подписи и дефолтная опора, не ветка алгоритма.
    const juce::StringArray roleLabels { "-", "Close", "OH", "Room", "Hats" };

    for (int i = 0; i < kMaxChannels; ++i)
    {
        const auto namePrefix = juce::String::formatted("Ch %02d ", i + 1);

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { channelParamId(i, "enabled"), 1 },
            namePrefix + "Enabled",
            true));

        // Solo/Mute — мониторинг: глушат выход канала, но не трогают ни оценки,
        // ни офлайн-рендер, иначе экспорт зависел бы от того, что сейчас слушают.
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { channelParamId(i, "mute"), 1 },
            namePrefix + "Mute",
            false));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { channelParamId(i, "solo"), 1 },
            namePrefix + "Solo",
            false));

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

        // Глубина хранится 0…1, но и в таблице, и в списке параметров хоста
        // читается процентами: «35 %» понятнее, чем «0.350».
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { channelParamId(i, "rotatorAmount"), 1 },
            namePrefix + "Rotator",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [](float value, int) { return juce::String(juce::roundToInt(100.0f * value)) + " %"; })));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { channelParamId(i, "role"), 1 },
            namePrefix + "Role",
            roleLabels,
            0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { channelParamId(i, "rotatorHz"), 1 },
            namePrefix + "Rotator Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
            kDefaultRotatorHz));
    }

    return { params.begin(), params.end() };
}

} // namespace beat
