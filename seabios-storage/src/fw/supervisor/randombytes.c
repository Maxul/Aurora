#include <stdint.h>
#include <stdlib.h>

int rdrand_get_bytes(unsigned int n, unsigned char *dest);
int rdrand_get_n_32(unsigned int n, uint32_t *dest);

void randombytes_buf(void * const buf, const size_t size)
{
    unsigned char *p = (unsigned char *) buf;
    rdrand_get_bytes(size, p);
}

uint32_t
randombytes_uniform(const uint32_t upper_bound)
{
    uint32_t min;
    uint32_t r;

    if (upper_bound < 2) {
        return 0;
    }
    min = (1U + ~upper_bound) % upper_bound; /* = 2**32 mod upper_bound */
    do {
        //r = randombytes_random();
        rdrand_get_n_32(1, &r);
    } while (r < min);
    /* r is now clamped to a set whose size mod upper_bound == 0
     * the worst case (2**31+1) requires ~ 2 attempts */

    return r % upper_bound;
}
