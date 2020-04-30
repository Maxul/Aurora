#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

#include <stddef.h>
#include <ctype.h>

//#define TIMESTAMP_DEBUG

typedef struct __check_point {
    unsigned char *event;
    unsigned long timestamp;
} check_point;

check_point checkpoints[64];

static size_t occurence = 0;

void checkpoint(const char *what)
{
#ifdef TIMESTAMP_DEBUG
    long unsigned tsc = 0;
    ocall_tsc(&tsc);

    checkpoints[occurence].event = (unsigned char *)what;
    checkpoints[occurence].timestamp = tsc;
    occurence++;
#endif
}

void _print_info()
{
#ifdef TIMESTAMP_DEBUG
    static int n = 1;
    printf("Microbenchmark inside SGX : %d\n", n++);
    for (int i = 1; i < occurence; ++i) {
        printf("%s - %s: TSC delta = %lu == %lu us\n", checkpoints[i].event, checkpoints[i-1].event,
            checkpoints[i].timestamp - checkpoints[i-1].timestamp,
            (checkpoints[i].timestamp - checkpoints[i-1].timestamp) / 2808);
    }

    long unsigned diff = checkpoints[occurence-1].timestamp - checkpoints[0].timestamp;
    printf("Summary: loopback used %lu ticks == %lu us.\n\n", diff, diff / 2808);

    occurence = 0;
#endif
}

void _print_hex(const char* what, const void* v, const unsigned long l)
{
  const unsigned char* p = (unsigned char*)v;
  unsigned long x, y = 0, z;
  printf("%s contents: \n", what);
  for (x = 0; x < l; ) {
      printf("%02X ", p[x]);
      if (!(++x % 16) || x == l) {
         if((x % 16) != 0) {
            z = 16 - (x % 16);
            if(z >= 8)
               printf(" ");
            for (; z != 0; --z) {
               printf("   ");
            }
         }
         printf(" | ");
         for(; y < x; y++) {
            if((y % 8) == 0)
               printf(" ");
            if(isgraph(p[y]))
               printf("%c", p[y]);
            else
               printf(".");
         }
         printf("\n");
      }
      else if((x % 8) == 0) {
         printf(" ");
      }
  }
}
