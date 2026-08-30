#pragma once

#include <complex>

namespace beat
{

class Fft
{
public:
    explicit Fft(int order);

    int size() const { return n; }

    void forward(std::complex<float>* data);
    void inverse(std::complex<float>* data);

private:
    void transform(std::complex<float>* data, bool inverse) const;

    int n = 0;
};

} // namespace beat
