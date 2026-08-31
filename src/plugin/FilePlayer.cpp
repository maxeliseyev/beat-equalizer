#include "FilePlayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

FilePlayer::FilePlayer()
{
    formats.registerBasicFormats();
}

juce::String FilePlayer::load(const juce::Array<juce::File>& files, double targetSampleRate)
{
    if (files.isEmpty())
        return "No files selected";

    if (targetSampleRate <= 0.0)
        return "Audio device is not running yet";

    std::vector<std::vector<float>> channels;
    juce::StringArray names;
    juce::StringArray perChannelNames;

    for (const auto& file : files)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
            return "Cannot read " + file.getFileName();

        const int fileSamples = static_cast<int>(reader->lengthInSamples);
        const int fileChannels = static_cast<int>(reader->numChannels);
        if (fileSamples <= 0 || fileChannels <= 0)
            continue;

        juce::AudioBuffer<float> raw(fileChannels, fileSamples);
        reader->read(&raw, 0, fileSamples, 0, true, true);

        // Всё приводим к частоте устройства: иначе задержка в миллисекундах
        // и экспорт считались бы в разных временах.
        const double ratio = reader->sampleRate / targetSampleRate;

        const auto stem = file.getFileNameWithoutExtension();

        for (int ch = 0; ch < fileChannels; ++ch)
        {
            if (static_cast<int>(channels.size()) >= beat::kMaxChannels)
                break;

            const float* source = raw.getReadPointer(ch);

            // Моно-файл даёт имя как есть, многоканальный — с номером дорожки:
            // иначе шесть каналов одного wav подписаны одинаково. Имя кладём
            // только вместе с данными, чтобы подписи не разъехались с каналами.
            const auto channelName = (fileChannels > 1) ? stem + " " + juce::String(ch + 1) : stem;

            if (std::abs(ratio - 1.0) < 1.0e-9)
            {
                channels.emplace_back(source, source + fileSamples);
                perChannelNames.add(channelName);
                continue;
            }

            const int outSamples = static_cast<int>(std::floor(fileSamples / ratio));
            if (outSamples <= 0)
                continue;

            std::vector<float> resampled(static_cast<size_t>(outSamples), 0.0f);
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio, source, resampled.data(), outSamples);
            channels.push_back(std::move(resampled));
            perChannelNames.add(channelName);
        }

        names.add(file.getFileName());
    }

    if (channels.empty())
        return "Nothing to play in those files";

    int longest = 0;
    for (const auto& channel : channels)
        longest = std::max(longest, static_cast<int>(channel.size()));

    juce::AudioBuffer<float> loaded(static_cast<int>(channels.size()), longest);
    loaded.clear();
    for (int ch = 0; ch < loaded.getNumChannels(); ++ch)
        loaded.copyFrom(ch, 0, channels[static_cast<size_t>(ch)].data(),
                        static_cast<int>(channels[static_cast<size_t>(ch)].size()));

    {
        const juce::SpinLock::ScopedLockType scoped(lock);
        clip = std::move(loaded);
    }

    channelNames = perChannelNames;

    loadedChannels.store(clip.getNumChannels());
    loadedSamples.store(clip.getNumSamples());

    // Первый удар ищем один раз при загрузке: отрисовка спрашивает его 25 раз
    // в секунду, а материал между Load не меняется.
    {
        const juce::SpinLock::ScopedLockType scoped(lock);
        onset.store(firstOnsetIndex(clip.getNumChannels()));
    }

    position.store(0);
    playing.store(false);

    description = juce::String(clip.getNumChannels()) + " ch / "
                  + juce::String(clip.getNumSamples() / targetSampleRate, 1) + " s  ("
                  + names.joinIntoString(", ") + ")";

    return {};
}

void FilePlayer::clear()
{
    playing.store(false);

    {
        const juce::SpinLock::ScopedLockType scoped(lock);
        clip.setSize(0, 0);
    }

    loadedChannels.store(0);
    loadedSamples.store(0);
    position.store(0);
    onset.store(0);
    description = {};
    channelNames.clear();
}

juce::String FilePlayer::getChannelName(int channel) const
{
    if (channel < 0 || channel >= channelNames.size())
        return {};

    return channelNames[channel];
}

void FilePlayer::setPlaying(bool shouldPlay)
{
    if (shouldPlay && !hasMaterial())
        return;

    playing.store(shouldPlay);
}

bool FilePlayer::fill(juce::AudioBuffer<float>& buffer, int numChannels)
{
    if (!playing.load() || !hasMaterial())
        return false;

    const juce::SpinLock::ScopedTryLockType scoped(lock);
    if (!scoped.isLocked())
        return false;

    const int available = clip.getNumSamples();
    const int clipChannels = clip.getNumChannels();
    const int wanted = std::min(numChannels, buffer.getNumChannels());
    if (available <= 0 || wanted <= 0)
        return false;

    int pos = position.load();
    int written = 0;
    const int total = buffer.getNumSamples();

    while (written < total)
    {
        const int chunk = std::min(total - written, available - pos);

        for (int ch = 0; ch < wanted; ++ch)
        {
            if (ch < clipChannels)
                buffer.copyFrom(ch, written, clip, ch, pos, chunk);
            else
                buffer.clear(ch, written, chunk);
        }

        written += chunk;
        pos += chunk;
        if (pos >= available)
            pos = 0; // стенд играет по кругу: анализировать удобнее, чем ловить конец
    }

    position.store(pos);
    return true;
}

int FilePlayer::firstOnsetIndex(int channels) const
{
    const int available = clip.getNumSamples();
    constexpr float onsetThreshold = 0.01f;

    for (int i = 0; i < available; ++i)
        for (int ch = 0; ch < channels; ++ch)
            if (std::abs(clip.getSample(ch, i)) > onsetThreshold)
                return i;

    return 0;
}

int FilePlayer::readDisplayWindow(int channel, float* dest, int count, int shiftSamples) const
{
    if (dest == nullptr || count <= 0 || !hasMaterial())
        return 0;

    // Отрисовка ждать не имеет права: аудиопоток берёт этот же замок try-версией
    // и на неудаче отдаёт блок мимо стенда, то есть слышимым щелчком.
    const juce::SpinLock::ScopedTryLockType scoped(lock);
    if (!scoped.isLocked())
        return 0;

    const int available = clip.getNumSamples();
    if (channel < 0 || channel >= clip.getNumChannels() || available <= 0)
        return 0;

    const long long start = static_cast<long long>(displayOrigin(count)) - count - shiftSamples;
    const float* source = clip.getReadPointer(channel);

    for (int i = 0; i < count; ++i)
    {
        long long index = (start + i) % available;
        if (index < 0)
            index += available;

        dest[i] = source[index];
    }

    return count;
}

int FilePlayer::displayOrigin(int count) const
{
    // На паузе окно стоит на первом ударе: сразу после Load строка должна
    // что-то показывать, а не хвост тишины перед нулём.
    return playing.load() ? position.load() : onset.load() + count;
}

int FilePlayer::readAnalysisWindow(float* dest, int channels, int count) const
{
    if (dest == nullptr || count <= 0 || channels <= 0 || !hasMaterial())
        return 0;

    const juce::SpinLock::ScopedLockType scoped(lock);

    const int clipChannels = clip.getNumChannels();
    const int available = clip.getNumSamples();
    const int wanted = std::min(channels, clipChannels);
    if (wanted <= 0 || available <= 0)
        return 0;

    const int start = std::min(onset.load(), available - 1);
    const int n = std::min(count, available - start);
    if (n <= 0)
        return 0;

    for (int ch = 0; ch < wanted; ++ch)
    {
        float* out = dest + static_cast<std::ptrdiff_t>(ch) * count;
        std::copy_n(clip.getReadPointer(ch) + start, n, out);
    }

    return n;
}
