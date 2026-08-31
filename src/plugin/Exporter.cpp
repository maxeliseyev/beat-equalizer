#include "Exporter.h"

#include "dsp/AllpassRotator.h"
#include "dsp/FractionalDelay.h"
#include "dsp/LatencyModel.h"

#include <algorithm>
#include <cmath>

namespace beat::exporter
{

void renderAligned(const juce::AudioBuffer<float>& source,
                   double sampleRate,
                   const std::vector<ChannelSettings>& settings,
                   juce::AudioBuffer<float>& destination)
{
    const int channels = std::min(source.getNumChannels(), static_cast<int>(settings.size()));
    const int samples = source.getNumSamples();

    if (channels <= 0 || samples <= 0 || sampleRate <= 0.0)
    {
        destination.setSize(0, 0);
        return;
    }

    float maxDelay = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        maxDelay = std::max(maxDelay, settings[static_cast<size_t>(ch)].delaySamples);

    const int tail = static_cast<int>(std::ceil(maxDelay)) + kInterpolatorLatencySamples;

    FractionalDelay delay;
    AllpassRotator rotator;
    delay.prepare(sampleRate, channels);
    rotator.prepare(sampleRate, channels);

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& channelSettings = settings[static_cast<size_t>(ch)];
        delay.setAppliedDelaySamples(ch, channelSettings.delaySamples);
        delay.setInvert(ch, channelSettings.invert);
        rotator.setRotation(ch, channelSettings.rotatorHz, channelSettings.rotatorAmount);
    }

    // Иначе первые миллисекунды экспорта проезжают на сглаживании ручек.
    delay.snapToTargets();
    rotator.snapToTargets();

    destination.setSize(channels, samples + tail);
    destination.clear();

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* in = source.getReadPointer(ch);
        float* out = destination.getWritePointer(ch);

        for (int i = 0; i < samples + tail; ++i)
        {
            const float x = (i < samples) ? in[i] : 0.0f;
            out[i] = rotator.processSample(ch, delay.processSample(ch, x));
        }
    }
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0 || sampleRate <= 0.0)
        return false;

    file.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    if (!stream->openedOk())
        return false;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.get(),
                               sampleRate,
                               static_cast<unsigned int>(buffer.getNumChannels()),
                               24,
                               {},
                               0));

    if (writer == nullptr)
        return false;

    stream.release(); // writer владеет потоком
    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

} // namespace beat::exporter
