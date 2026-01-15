#include "Biquad.h"

namespace eqdsp
{
void Biquad::prepare(double sampleRate)
{
    sampleRateHz = sampleRate;
    reset();
    firstUpdate = true;
}

void Biquad::reset()
{
    z1 = 0.0;
    z2 = 0.0;
}

void Biquad::update(const BandParams& params)
{
    if (firstUpdate
        || params.frequencyHz != lastParams.frequencyHz
        || params.gainDb != lastParams.gainDb
        || params.q != lastParams.q
        || params.type != lastParams.type)
    {
        setCoefficients(params);
        lastParams = params;
        firstUpdate = false;
    }
}

float Biquad::processSample(float x)
{
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return static_cast<float>(y);
}

void Biquad::setCoefficients(const BandParams& params)
{
    const double nyquist = sampleRateHz * 0.5;
    const double clampedFreq = std::fmax(10.0, std::fmin(params.frequencyHz, nyquist * 0.99));
    constexpr double kPi = 3.14159265358979323846;
    const double omega = 2.0 * kPi * clampedFreq / sampleRateHz;
    const double sinW = std::sin(omega);
    const double cosW = std::cos(omega);
    const double q = std::fmax(0.1, params.q);
    const double alpha = sinW / (2.0 * q);
    const double a = std::pow(10.0, params.gainDb / 40.0);

    double b0d = 1.0;
    double b1d = 0.0;
    double b2d = 0.0;
    double a0d = 1.0;
    double a1d = 0.0;
    double a2d = 0.0;

    switch (params.type)
    {
        case FilterType::bell:
            b0d = 1.0 + alpha * a;
            b1d = -2.0 * cosW;
            b2d = 1.0 - alpha * a;
            a0d = 1.0 + alpha / a;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha / a;
            break;
        case FilterType::lowShelf:
        {
            const double sqrtA = std::sqrt(a);
            const double beta = std::sqrt(a) / q;
            b0d = a * ((a + 1.0) - (a - 1.0) * cosW + beta * sinW);
            b1d = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosW);
            b2d = a * ((a + 1.0) - (a - 1.0) * cosW - beta * sinW);
            a0d = (a + 1.0) + (a - 1.0) * cosW + beta * sinW;
            a1d = -2.0 * ((a - 1.0) + (a + 1.0) * cosW);
            a2d = (a + 1.0) + (a - 1.0) * cosW - beta * sinW;
            break;
        }
        case FilterType::highShelf:
        {
            const double sqrtA = std::sqrt(a);
            const double beta = std::sqrt(a) / q;
            b0d = a * ((a + 1.0) + (a - 1.0) * cosW + beta * sinW);
            b1d = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW);
            b2d = a * ((a + 1.0) + (a - 1.0) * cosW - beta * sinW);
            a0d = (a + 1.0) - (a - 1.0) * cosW + beta * sinW;
            a1d = 2.0 * ((a - 1.0) - (a + 1.0) * cosW);
            a2d = (a + 1.0) - (a - 1.0) * cosW - beta * sinW;
            break;
        }
        case FilterType::lowPass:
        {
            b0d = (1.0 - cosW) * 0.5;
            b1d = 1.0 - cosW;
            b2d = (1.0 - cosW) * 0.5;
            a0d = 1.0 + alpha;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha;
            break;
        }
        case FilterType::highPass:
        {
            b0d = (1.0 + cosW) * 0.5;
            b1d = -(1.0 + cosW);
            b2d = (1.0 + cosW) * 0.5;
            a0d = 1.0 + alpha;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha;
            break;
        }
        case FilterType::notch:
            b0d = 1.0;
            b1d = -2.0 * cosW;
            b2d = 1.0;
            a0d = 1.0 + alpha;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha;
            break;
        case FilterType::bandPass:
            b0d = alpha;
            b1d = 0.0;
            b2d = -alpha;
            a0d = 1.0 + alpha;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha;
            break;
        case FilterType::allPass:
            b0d = 1.0 - alpha;
            b1d = -2.0 * cosW;
            b2d = 1.0 + alpha;
            a0d = 1.0 + alpha;
            a1d = -2.0 * cosW;
            a2d = 1.0 - alpha;
            break;
        case FilterType::tilt:
        {
            const double aTilt = std::pow(10.0, (params.gainDb * 0.5) / 40.0);
            const double beta = std::sqrt(aTilt) / q;
            b0d = aTilt * ((aTilt + 1.0) - (aTilt - 1.0) * cosW + beta * sinW);
            b1d = 2.0 * aTilt * ((aTilt - 1.0) - (aTilt + 1.0) * cosW);
            b2d = aTilt * ((aTilt + 1.0) - (aTilt - 1.0) * cosW - beta * sinW);
            a0d = (aTilt + 1.0) + (aTilt - 1.0) * cosW + beta * sinW;
            a1d = -2.0 * ((aTilt - 1.0) + (aTilt + 1.0) * cosW);
            a2d = (aTilt + 1.0) + (aTilt - 1.0) * cosW - beta * sinW;
            break;
        }
    }

    const double invA0 = 1.0 / a0d;
    b0 = b0d * invA0;
    b1 = b1d * invA0;
    b2 = b2d * invA0;
    a1 = a1d * invA0;
    a2 = a2d * invA0;
}
} // namespace eqdsp
