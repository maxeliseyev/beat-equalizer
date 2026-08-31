#pragma once

#include "dsp/Constants.h"

#include <array>
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

    bool hasMaterial() const { return activeSlot.load() >= 0; }
    int numChannels() const { return loadedChannels.load(); }
    int numSamples() const { return loadedSamples.load(); }
    // Частота, под которую материал пересчитан при загрузке.
    double getSampleRate() const { return clipSampleRate.load(); }
    juce::String getDescription() const { return description; }
    // Message thread: имя дорожки на этом канале ("kick", "OH L"). Пусто, если
    // материала нет. Меняется только в load(), с того же потока.
    juce::String getChannelName(int channel) const;
    juce::Array<juce::File> getFiles() const { return loadedFiles; }

    // Транспорт: пауза не трогает позицию, rewind ставит её в ноль.
    void setPlaying(bool shouldPlay);
    bool isPlaying() const { return playing.load(); }
    void rewind() { position.store(0); }
    int getPosition() const { return position.load(); }
    void setPosition(int sample);

    // Аудиопоток: подменяет вход. false — материала нет или стенд на паузе.
    bool fill(juce::AudioBuffer<float>& buffer, int numChannels);

    // Worker: окно для Analyze, channel-major. Тишину в начале пропускаем,
    // иначе восьмисекундное окно уедет в отсчёт перед первым ударом.
    int readAnalysisWindow(float* dest, int channels, int count) const;

    // Позиция конца окна отрисовки в отсчётах клипа: по ней строится сетка.
    int displayOrigin(int count) const;

    // Message thread: окно одного канала для осциллограммы. Заканчивается на
    // позиции воспроизведения (на паузе — на первом ударе), сдвинуто назад на
    // задержку канала, поэтому строки видно уже выровненными. Клип закольцован.
    int readDisplayWindow(int channel, float* dest, int count, int shiftSamples) const;

    // Message thread, для экспорта.
    const juce::AudioBuffer<float>& getBuffer() const;

private:
    // Клип лежит в двух слотах: загрузка пишет в свободный и переключает
    // индекс, все читатели берут активный. Замка на аудиопути нет вовсе —
    // раньше промах try-lock стоил целого блока чужого звука, а Analyze держал
    // обычный замок на всё копирование восьмисекундного окна.
    static constexpr int kSlots = 2;

    const juce::AudioBuffer<float>* activeClip() const;
    static int firstOnsetIndex(const juce::AudioBuffer<float>& clip);

    juce::AudioFormatManager formats;
    std::array<juce::AudioBuffer<float>, kSlots> slots;
    juce::Array<juce::File> loadedFiles;
    juce::String description;
    juce::StringArray channelNames;
    std::atomic<int> activeSlot { -1 };
    std::atomic<int> loadedChannels { 0 };
    std::atomic<int> loadedSamples { 0 };
    std::atomic<int> position { 0 };
    std::atomic<int> onset { 0 };
    std::atomic<double> clipSampleRate { 0.0 };
    std::atomic<bool> playing { false };
};
