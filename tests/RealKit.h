#pragma once

// Реальный кит на стенде.
//
// Звук в репозиторий не кладётся никогда: восемь стемов по 67 МБ — это не
// фикстура, это чужая запись. Путь к киту стенд берёт из окружения
// (BEAT_REAL_KIT_DIR); без него тесты пропускаются, а не падают, — иначе
// сборка на чужой машине красная по причине, которая к коду не относится.
//
// В репозиторий попадают только числа: docs/real-kit-protocol.md.

#include "WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace beat::test
{

// Порядок фиксирован: индекс канала — часть протокола задержек, и менять его
// молча нельзя, иначе записанные числа поедут вместе с ним.
enum KitMic
{
    kKick = 0,
    kSnareTop,
    kSnareBottom,
    kHat,
    kRide,
    kTom1,
    kTom2,
    kRoom,
    kKitMicCount
};

inline const char* const* kitFiles()
{
    static const char* files[kKitMicCount] = {
        "kik aut2026.wav", "sn top2026.wav", "sn b2026.wav", "het2026.wav",
        "ride2026.wav",    "tom 12026.wav",  "tom 22026.wav", "room2026.wav"
    };
    return files;
}

inline const char* const* kitNames()
{
    static const char* names[kKitMicCount] = {
        "kick", "snare top", "snare bottom", "hat", "ride", "tom 1", "tom 2", "room"
    };
    return names;
}

// Пустая строка — кита нет, тест обязан пропуститься.
inline std::string realKitDir()
{
    const char* value = std::getenv("BEAT_REAL_KIT_DIR");
    if (value == nullptr || *value == '\0')
        return {};

    std::string dir = value;
    while (!dir.empty() && dir.back() == '/')
        dir.pop_back();

    // Кит проверяется целиком: не хватает микрофона — это не «меньше
    // каналов», это другой материал, и мерить по нему протокол нельзя.
    for (int mic = 0; mic < kKitMicCount; ++mic)
    {
        WavInfo info {};
        if (!wavInfoOf(dir + "/" + kitFiles()[mic], info))
            return {};
    }

    return dir;
}

inline std::string kitPath(const std::string& dir, int mic)
{
    return dir + "/" + kitFiles()[mic];
}

inline double medianOf(std::vector<double> values)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// Медиана абсолютных отклонений: разброс, который не сносит один промах.
// Среднеквадратичное здесь врало бы — промахи линейки и есть то, что мерим.
inline double madOf(const std::vector<double>& values, double median)
{
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double value : values)
        deviations.push_back(std::abs(value - median));

    return medianOf(deviations);
}

// Одиночные удары опорного канала: локальные максимумы огибающей, громче
// среднего в `above` раз и не имеющие соседа громче внутри `guard`.
//
// Это разметка для измерения, а не детектор. Она намеренно грубая и работает
// только на редких местах — на плотной игре огибающая не успевает вернуться к
// полу, и линейка молча начинает мерить границу окна вместо прихода.
inline std::vector<int> isolatedHits(const std::vector<float>& envelope, int guard, float above)
{
    const int count = static_cast<int>(envelope.size());
    if (count <= 2 * guard || guard <= 0)
        return {};

    double mean = 0.0;
    for (float value : envelope)
        mean += value;
    mean /= static_cast<double>(count);

    const float level = above * static_cast<float>(mean);

    std::vector<int> hits;
    for (int i = guard; i < count - guard; ++i)
    {
        if (envelope[static_cast<size_t>(i)] < level)
            continue;

        bool peak = true;
        for (int k = i - guard; k <= i + guard && peak; ++k)
            peak = envelope[static_cast<size_t>(k)] <= envelope[static_cast<size_t>(i)];

        if (peak)
        {
            hits.push_back(i);
            i += guard;
        }
    }

    return hits;
}

// Независимая линейка прихода: откат назад по огибающей от самого громкого
// места окна до подъёма над собственным полом канала.
//
// Ни БПФ, ни корреляции — и это условие, а не экономия: проверять GCC-PHAT
// корреляцией значит проверять его самим собой. Возвращает отрицательное
// число, если удара в окне нет.
inline double arrivalNear(const std::vector<float>& envelope,
                          float floorLevel,
                          int centre,
                          int search)
{
    const int count = static_cast<int>(envelope.size());
    const int from = std::max(0, centre - search);
    const int to = std::min(count, centre + search);
    if (to - from < 2)
        return -1.0;

    int loudest = -1;
    float top = 0.0f;
    for (int i = from; i < to; ++i)
    {
        if (envelope[static_cast<size_t>(i)] > top)
        {
            top = envelope[static_cast<size_t>(i)];
            loudest = i;
        }
    }

    const float floorValue = std::max(floorLevel, 1.0e-9f);
    if (loudest < 0 || top < floorValue * 4.0f)
        return -1.0;

    const float onset = floorValue * 2.0f;
    int i = loudest;
    while (i > from && envelope[static_cast<size_t>(i)] > onset)
        --i;

    return static_cast<double>(i);
}

} // namespace beat::test
