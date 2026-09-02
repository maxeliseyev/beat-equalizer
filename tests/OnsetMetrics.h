#pragma once

// Precision и recall считаются и показываются порознь. Ложное срабатывание
// хуже пропуска: лишняя точка реза портит звук там, где ничего не было, а
// пропущенный удар просто не отредактируется (AGENTS, инвариант 13).
// Поэтому F1 здесь нет и не будет.

#include <algorithm>
#include <cmath>
#include <vector>

namespace beat::test
{

struct OnsetMatch
{
    int truePositives = 0;
    int falsePositives = 0;
    int falseNegatives = 0;
    double worstErrorSamples = 0.0;
    double medianErrorSamples = 0.0;

    double precision() const
    {
        const int found = truePositives + falsePositives;
        return found > 0 ? static_cast<double>(truePositives) / found : 0.0;
    }

    double recall() const
    {
        const int expected = truePositives + falseNegatives;
        return expected > 0 ? static_cast<double>(truePositives) / expected : 0.0;
    }
};

// Жадное сопоставление: каждый истинный удар забирает ближайший найденный в
// пределах допуска, найденное без пары — ложное срабатывание.
inline OnsetMatch matchOnsets(std::vector<double> found,
                              const std::vector<double>& truth,
                              double toleranceSamples)
{
    OnsetMatch match;
    std::vector<bool> used(found.size(), false);
    std::vector<double> errors;

    for (double expected : truth)
    {
        int best = -1;
        double bestError = toleranceSamples;

        for (size_t i = 0; i < found.size(); ++i)
        {
            if (used[i])
                continue;

            const double error = std::abs(found[i] - expected);
            if (error <= bestError)
            {
                bestError = error;
                best = static_cast<int>(i);
            }
        }

        if (best < 0)
        {
            ++match.falseNegatives;
            continue;
        }

        used[static_cast<size_t>(best)] = true;
        ++match.truePositives;
        errors.push_back(bestError);
    }

    match.falsePositives = static_cast<int>(std::count(used.begin(), used.end(), false));

    if (!errors.empty())
    {
        std::sort(errors.begin(), errors.end());
        match.worstErrorSamples = errors.back();
        match.medianErrorSamples = errors[errors.size() / 2];
    }

    return match;
}

} // namespace beat::test
