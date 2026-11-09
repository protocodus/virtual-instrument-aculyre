#pragma once

namespace aculyre::detail
{
// Two doubles as one vector. The Rack's SDK compiler does not vectorize
// loops itself, but keeps vector types to the end: written with them, the
// strings' filters and the modes run two lanes per instruction there too
// (SSE2, NEON). Each lane is the scalar arithmetic, so the result is the same
// in every build. Natural scalar alignment: the engine's state lives on the
// heap, which the SDK aligns to 8 bytes.
#if defined(__GNUC__) || defined(__clang__)
typedef double Pair __attribute__((vector_size(16), aligned(8), may_alias));
#else
struct Pair
{
    double v[2];
    Pair operator+(Pair o) const noexcept { return { { v[0] + o.v[0], v[1] + o.v[1] } }; }
    Pair operator-(Pair o) const noexcept { return { { v[0] - o.v[0], v[1] - o.v[1] } }; }
    Pair operator*(Pair o) const noexcept { return { { v[0] * o.v[0], v[1] * o.v[1] } }; }
    Pair operator-() const noexcept { return { { -v[0], -v[1] } }; }
    Pair& operator+=(Pair o) noexcept { return *this = *this + o; }
    double operator[](int i) const noexcept { return v[i]; }
};
#endif
inline Pair* pairs(double* p) noexcept { return reinterpret_cast<Pair*>(p); }
inline const Pair* pairs(const double* p) noexcept { return reinterpret_cast<const Pair*>(p); }
inline Pair splat(double x) noexcept { return Pair { x, x }; }
} // namespace aculyre::detail
