#ifndef __MICROBENCH__
#define __MICROBENCH__

#include "types.h" // u32

#define N	10000

// Intel CPU benchmarks

static inline void synch_tsc(void)
{
	asm volatile("cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");
}

static inline unsigned long rdtscllp(void)
{
	long long r;

#ifdef __x86_64__
	unsigned a, d;

	asm volatile ("rdtscp" : "=a"(a), "=d"(d));
	r = a | ((long long)d << 32);
#else
	asm volatile ("rdtscp" : "=A"(r));
#endif
	return r;
}
#if 0
static inline unsigned long rdtscllp(void)
{
	unsigned int a, d;
	asm volatile("rdtscp" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}
#endif

#endif
