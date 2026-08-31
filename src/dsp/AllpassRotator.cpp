#include "AllpassRotator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace beat
{

namespace
{
constexpr float kPi = std::numbers::pi_v<float>;
}

void AllpassRotator::prepare(double newSampleRate, int newNumChannels)
{
    sampleRate = std::max(newSampleRate, 1.0);
    numChannels = std::clamp(newNumChannels, 1, kMaxChannels);

    const auto size = static_cast<size_t>(numChannels);
    targetCoeff.assign(size, 0.0f);
    currentCoeff.assign(size, 0.0f);
    targetAmount.assign(size, 0.0f);
    currentAmount.assign(size, 0.0f);
    lastInput.assign(size, 0.0f);
    lastAllpass.assign(size, 0.0f);

    const float tauSamples = std::max(1.0f, kDelaySmoothMs * 0.001f * static_cast<float>(sampleRate));
    smoothCoeff = 1.0f - std::exp(-1.0f / tauSamples);

    reset();
}

void AllpassRotator::reset()
{
    std::fill(lastInput.begin(), lastInput.end(), 0.0f);
    std::fill(lastAllpass.begin(), lastAllpass.end(), 0.0f);
    std::copy(targetCoeff.begin(), targetCoeff.end(), currentCoeff.begin());
    std::copy(targetAmount.begin(), targetAmount.end(), currentAmount.begin());
}

float AllpassRotator::coefficient(float frequencyHz, double sampleRate)
{
    if (sampleRate <= 0.0)
        return 0.0f;

    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float hz = std::clamp(frequencyHz, 1.0f, nyquist * 0.99f);
    const float t = std::tan(kPi * hz / static_cast<float>(sampleRate));

    // Знак такой, чтобы разностное уравнение y = c*x + x1 - c*y1 давало
    // фазу -90 градусов ровно на frequencyHz.
    return (t - 1.0f) / (t + 1.0f);
}

std::complex<float> AllpassRotator::response(float frequencyHz,
                                             float amount,
                                             float atHz,
                                             double sampleRate)
{
    if (sampleRate <= 0.0)
        return { 1.0f, 0.0f };

    const float c = coefficient(frequencyHz, sampleRate);
    const float omega = 2.0f * kPi * atHz / static_cast<float>(sampleRate);
    const std::complex<float> z1(std::cos(omega), -std::sin(omega));
    const auto allpass = (c + z1) / (1.0f + c * z1);
    const float a = std::clamp(amount, 0.0f, 1.0f);

    return (1.0f - a) + a * allpass;
}

void AllpassRotator::setRotation(int channel, float frequencyHz, float amount)
{
    if (channel < 0 || channel >= numChannels)
        return;

    const auto ch = static_cast<size_t>(channel);
    targetCoeff[ch] = coefficient(frequencyHz, sampleRate);
    targetAmount[ch] = std::clamp(amount, 0.0f, 1.0f);
}

float AllpassRotator::processSample(int channel, float input)
{
    if (channel < 0 || channel >= numChannels)
        return input;

    const auto ch = static_cast<size_t>(channel);
    currentCoeff[ch] += (targetCoeff[ch] - currentCoeff[ch]) * smoothCoeff;
    currentAmount[ch] += (targetAmount[ch] - currentAmount[ch]) * smoothCoeff;

    const float c = currentCoeff[ch];
    const float allpass = c * input + lastInput[ch] - c * lastAllpass[ch];

    lastInput[ch] = input;
    lastAllpass[ch] = allpass;

    const float a = currentAmount[ch];
    return (1.0f - a) * input + a * allpass;
}

} // namespace beat
