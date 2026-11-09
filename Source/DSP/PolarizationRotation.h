#pragma once

#include "DspMath.h"

namespace aculyre::detail
{
// The two transverse waveguides have the same characteristic impedance.
// Rotating their directions at a saddle therefore preserves wave power:
// R^T R = I. The returning bridge motion must use the TRANSPOSE rotation,
// otherwise the force/velocity ports are not reciprocal. Their summed
// impedance at the bridge is unchanged by this orthogonal basis change.
struct PolarizationRotation
{
    double cosine { 1.0 }, sine { 0.0 };

    static PolarizationRotation fromSlope(double slope) noexcept
    {
        const double cosine = 1.0 / squareRoot(1.0 + slope * slope);
        return { cosine, slope * cosine };
    }

    double normalForce(double normal, double parallel) const noexcept
    {
        return cosine * normal + sine * parallel;
    }
    double parallelForce(double normal, double parallel) const noexcept
    {
        return cosine * parallel - sine * normal;
    }
    double normalMotion(double normal, double parallel) const noexcept
    {
        return cosine * normal - sine * parallel;
    }
    double parallelMotion(double normal, double parallel) const noexcept
    {
        return sine * normal + cosine * parallel;
    }
};
} // namespace aculyre::detail
