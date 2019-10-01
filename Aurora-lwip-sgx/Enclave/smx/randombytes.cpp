#include <stdint.h>
#include <stdlib.h>

#include <sgx_trts.h>

#include "core.h"
#include "randombytes.h"
#include "utils.h"

#define RDRAND_RETRY_LOOPS 10

static inline uint8_t rdrand_32(uint32_t *rand)
{
	uint8_t carry;

	__asm__ __volatile__(
	     ".byte 0x0f; .byte 0xc7; .byte 0xf0; setc %1"
	     : "=a" (*rand), "=qm" (carry));
	return carry;
}

int get_random_number_32(uint32_t *rand)
{
	int i;

	/* Perform a loop call until RDRAND succeeds or returns failure. */
	for (i = 0; i < RDRAND_RETRY_LOOPS; i++) {
		if (rdrand_32(rand))
			return 0;
	}
	return -1;
}

void
randombytes_buf(void * const buf, const size_t size)
{
    unsigned char *p = (unsigned char *) buf;
    sgx_read_rand(p, size);
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
        get_random_number_32(&r);
    } while (r < min);
    /* r is now clamped to a set whose size mod upper_bound == 0
     * the worst case (2**31+1) requires ~ 2 attempts */

    return r % upper_bound;
}

