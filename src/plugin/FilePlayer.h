#pragma once

#include "dsp/Constants.h"

#include <atomic>

#include <juce_audio_formats/juce_audio_formats.h>

// Standalone-стенд: несколько WAV (моно, стерео или один многоканальный)
// раскладываются по каналам подряд и играют внутренним транспортом.
// Материал живёт в памяти целиком — это стенд для прогонов, не редактор сессии.
class FilePlayer
{
public:
    FilePlayer();

    // Message thread. Пустая строка — загрузилось, иначе текст ошибки.
    juce::String load(const juce::Array<juce::File>& files, double targetSampleRate);
    void clear();

    bool hasMaterial() const { return loadedChannels.load() > 0; }
    int numChannels() const { return loadedChannels.load(); }
    int numSamples() const { return loadedSamples.load(); }
    juce::String getDescription() const { return description; }

    void setPlaying(bool shouldPlay);
    bool isPlaying() const { return playing.load(); }
    void rewind() { position.store(0); }

    // Аудиопоток: подменяет вход. false — материала нет или буфер занят.
    bool fill(juce::AudioBuffer<float>& buffer, int numChannels);

    // Worker: окно для Analyze, channel-major. Тишину в начале пропускаем,
    // иначе восьмисекундное окно уедет в отсчёт перед первым ударом.
    int readAnalysisWindow(float* dest, int channels, int count) const;

    // Message thread, для экспорта.
    const juce::AudioBuffer<float>& getBuffer() const { return clip; }

private:
    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> clip;
    juce::String description;
    mutable juce::SpinLock lock;
    std::atomic<int> loadedChannels { 0 };
    std::atomic<int> loadedSamples { 0 };
    std::atomic<int> position { 0 };
    std::atomic<bool> playing { false };
};
