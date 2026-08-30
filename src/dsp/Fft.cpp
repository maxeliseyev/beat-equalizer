#include "Fft.h"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace beat
{

Fft::Fft(int order)
{
    if (order < 4 || order > 16)
        throw std::invalid_argument("FFT order must be in [4, 16]");

    n = 1 << order;
}

void Fft::forward(std::complex<float>* data)
{
    transform(data, false);
}

void Fft::inverse(std::complex<float>* data)
{
    transform(data, true);
}

void Fft::transform(std::complex<float>* data, bool inverse) const
{
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(data[i], data[j]);
    }

    constexpr float pi = std::numbers::pi_v<float>;

    for (int len = 2; len <= n; len <<= 1)
    {
        const float ang = (inverse ? 2.0f : -2.0f) * pi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));

        for (int i = 0; i < n; i += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            const int half = len / 2;

            for (int k = 0; k < half; ++k)
            {
                const auto u = data[i + k];
                const auto v = data[i + k + half] * w;
                data[i + k] = u + v;
                data[i + k + half] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        const float invN = 1.0f / static_cast<float>(n);
        for (int i = 0; i < n; ++i)
            data[i] *= invN;
    }
}

} // namespace beat
