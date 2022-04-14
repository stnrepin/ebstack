#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdint>

#include <immintrin.h>

namespace eb
{

static inline void Pause()
{
    _mm_pause();
}

static inline uint64_t Rdtsc()
{
    unsigned int hi;
    unsigned int lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ( static_cast< uint64_t >( lo ) ) | ( ( static_cast< uint64_t>( hi ) ) << 32 );
}

} // namespace eb

#endif // !UTILS_HTPP
