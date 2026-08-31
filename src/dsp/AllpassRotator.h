#pragma once

#include "Constants.h"

#include <complex>
#include <vector>

namespace beat
{

// Каскад allpass 1-го порядка после задержки: y[n] = c*x[n] + x[n-1] - c*y[n-1].
// Выход смешивается с сухим по amount (0 = bypass, 1 = чистый allpass) — при
// amount < 1 это уже не allpass, а поворот фазы с провалом, как в IBP.
class AllpassRotator
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setRotation(int channel, float frequencyHz, float amount);
    void snapToTargets();
    float processSample(int channel, float input);

    // c для частоты, на которой фаза allpass равна -90 градусов.
    static float coefficient(float frequencyHz, double sampleRate);

    // Реакция того же блока на частоте atHz: (1 - amount) + amount * H_ap.
    // Analyze пользуется ею, чтобы перебирать ротатор в спектре, не гоняя звук.
    static std::complex<float> response(float frequencyHz,
                                        float amount,
                                        float atHz,
                                        double sampleRate);

private:
    double sampleRate = 48000.0;
    int numChannels = 0;
    float smoothCoeff = 1.0f;

    std::vector<float> targetCoeff;
    std::vector<float> currentCoeff;
    std::vector<float> targetAmount;
    std::vector<float> currentAmount;
    std::vector<float> lastInput;
    std::vector<float> lastAllpass;
};

} // namespace beat
