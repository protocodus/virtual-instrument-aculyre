#pragma once

#include <cmath>

// Reason's SDK compiles with -fno-builtin, which makes the standard
// library's spellings of these out-of-line calls there (its square root is a
// software routine). The builtins are the same IEEE operations in line, so
// the code that runs while the instrument plays uses these; setup keeps the
// standard names. After Respira's DspMath.h.
namespace aculyre::detail
{
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_elementwise_sqrt)
#define ACULYRE_ELEMENTWISE_SQRT 1
#endif
#endif

#if defined(ACULYRE_ELEMENTWISE_SQRT)
// With math errno on, clang still calls the library for __builtin_sqrt.
inline double squareRoot(double x) noexcept { return __builtin_elementwise_sqrt(x); }
#elif defined(__GNUC__) || defined(__clang__)
inline double squareRoot(double x) noexcept { return __builtin_sqrt(x); }
#else
inline double squareRoot(double x) noexcept { return std::sqrt(x); }
#endif

#if defined(__GNUC__) || defined(__clang__)
inline double magnitude(double x) noexcept { return __builtin_fabs(x); }
inline bool isFinite(double x) noexcept { return __builtin_isfinite(x); }
#else
inline double magnitude(double x) noexcept { return std::fabs(x); }
inline bool isFinite(double x) noexcept { return std::isfinite(x); }
#endif

#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_elementwise_floor)
#define ACULYRE_ELEMENTWISE_FLOOR 1
#endif
#endif

#if defined(ACULYRE_ELEMENTWISE_FLOOR)
inline double floorValue(double x) noexcept { return __builtin_elementwise_floor(x); }
#elif defined(__GNUC__) || defined(__clang__)
inline double floorValue(double x) noexcept { return __builtin_floor(x); }
#else
inline double floorValue(double x) noexcept { return std::floor(x); }
#endif

// std::lround of a positive value that fits an int.
inline int nearest(double positive) noexcept { return static_cast<int>(positive + 0.5); }
} // namespace aculyre::detail
