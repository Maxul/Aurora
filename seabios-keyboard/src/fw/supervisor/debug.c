#include "output.h"
#include "string.h"
#include "microbench.h"

unsigned long measure_tsc_overhead(void)
{
	unsigned long t0, t1, overhead = ~0UL;
	int i;

	for (i = 0; i < N; i++) {
		t0 = rdtscllp();
		asm volatile("");
		t1 = rdtscllp();
		if (t1 - t0 < overhead)
			overhead = t1 - t0;
	}

	return overhead;
}

//#define TIMESTAMP_DEBUG

struct __check_point {
    unsigned char *event;
    unsigned long timestamp;
};

struct __check_point checkpoints[64];

static size_t occurence = 0;

void checkpoint(const char *what)
{
#ifdef TIMESTAMP_DEBUG
    checkpoints[occurence].event = (unsigned char *)what;
    checkpoints[occurence].timestamp = rdtscll();
    occurence++;
#endif
}

void _print_info(void)
{
#ifdef TIMESTAMP_DEBUG
    static int n = 1;
    unsigned long start = rdtscll();
    dprintf(1, "\nMicrobenchmark inside SMM : %d\n%s : TSC = %lu\n",
        n++, checkpoints[0].event, checkpoints[0].timestamp);

    for (int i = 1; i < occurence; ++i) {
        dprintf(1, "%s - %s: TSC delta = %lu == %lu us\n", checkpoints[i].event, checkpoints[i-1].event,
            checkpoints[i].timestamp - checkpoints[i-1].timestamp,
            (checkpoints[i].timestamp - checkpoints[i-1].timestamp) / 2808);
    }

    unsigned long diff = checkpoints[occurence-1].timestamp - checkpoints[0].timestamp;
    unsigned long end = rdtscll() - start;
    dprintf(1, "Summary: loopback used %lu ticks == %lu us.\n"
        "This dprintf() process cost %lu ticks == %lu us.\n\n", diff, diff / 2808, end, end / 2808);
#endif
    occurence = 0;
}

void serial_debug_puts(char *s, int len)
{
    for (int i=0; i<len; ++i)
    {
        outb(s[i], 0x3f8);
    }
    outb('\n', 0x3f8);
}

int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }

void _print_hex(const char *what, const void *v, const unsigned long l) {
  const unsigned char *p = v;
  unsigned long x, y = 0, z;
  dprintf(1, "%s contents: \n", what);
  for (x = 0; x < l;) {
    dprintf(1, "%02x ", p[x]);
    if (!(++x % 16) || x == l) {
      if ((x % 16) != 0) {
        z = 16 - (x % 16);
        if (z >= 8)
          dprintf(1, " ");
        for (; z != 0; --z) {
          dprintf(1, "   ");
        }
      }
      dprintf(1, " | ");
      for (; y < x; y++) {
        if ((y % 8) == 0)
          dprintf(1, " ");
        if (isgraph(p[y]))
          dprintf(1, "%c", p[y]);
        else
          dprintf(1, ".");
      }
      dprintf(1, "\n");
    } else if ((x % 8) == 0) {
      dprintf(1, " ");
    }
  }
}

int compare_testvector(const void *is, const unsigned long is_len,
                       const void *should, const unsigned long should_len) {
  int res = 0;
  if (is_len != should_len)
    res = is_len > should_len ? -1 : 1;
  else
    res = memcmp(is, should, is_len);

  if (res != 0) {
    _print_hex("SHOULD", should, should_len);
    _print_hex("IS    ", is, is_len);
  }

  return res;
}
