#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <ctype.h>
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

#define N	10000

static inline void synch_tsc(void)
{
	asm volatile("cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");
}

static inline unsigned long rdtscll(void)
{
	unsigned int a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}

static inline unsigned long rdtscllp(void)
{
	unsigned int a, d;
	asm volatile("rdtscp" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}

static unsigned long measure_tsc_overhead(void)
{
	unsigned long t0, t1, overhead = ~0UL;
	int i;

	for (i = 0; i < N; i++) {
		t0 = rdtscll();
		asm volatile("");
		t1 = rdtscllp();
		if (t1 - t0 < overhead)
			overhead = t1 - t0;
	}

	return overhead;
}

unsigned long trap_tsc, overhead;

#define SVM_SIZE ((1<<20))

extern unsigned char *uio_ram;
extern int uiofd;

    int uiofd, intr_cnt = 0;
    fd_set readset;

void uio_open()
{
    uiofd = open("/dev/uio0", O_RDWR);
    if (uiofd < 0) {
        perror("uio open:");
        return errno;
    }

    uio_ram = mmap(NULL, SVM_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED, uiofd, 0);
}

void uio_close()
{
    munmap(uio_ram, SVM_SIZE);
    close(uiofd);
}

#define printf

void uio_poll()
{
    printf("enter uio_poll\n");
    while (1) {
        FD_ZERO(&readset);
        FD_SET(uiofd, &readset);
        if (select(uiofd + 1, &readset, NULL, NULL, NULL) <= 0) {
            perror("select:");
            break;
        }
        if (FD_ISSET(uiofd, &readset)) {
            if (read(uiofd, &intr_cnt, 4) != 4) {
                perror("uio read:");
                break;
            }
            printf("UIO count %d\n", intr_cnt);
            break;
        }
    }
    printf("return from uio_poll\n");
}
