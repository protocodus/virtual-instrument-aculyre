#pragma once

namespace instrumentCpu
{
// Setup-time check with no runtime-library dependency. AVX instructions are
// safe only when the OS saves both XMM and YMM state, as well as when the CPU
// implements them. The freestanding Rack toolchain has no libgcc CPU cache.
inline bool hasAvx2() noexcept
{
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    unsigned a,b,c,d;
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0u),"c"(0u));
    if(a<7u)return false;
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(1u),"c"(0u));
    constexpr unsigned required=(1u<<27)|(1u<<28); // OSXSAVE, AVX
    if((c&required)!=required)return false;
    __asm__ volatile("xgetbv" : "=a"(a),"=d"(d) : "c"(0u));
    if((a&6u)!=6u)return false;
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(7u),"c"(0u));
    return (b&(1u<<5))!=0;
#else
    return false;
#endif
}
}
