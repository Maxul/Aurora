#include "biosvar.h" // GET_LOW
#include "config.h"  // CONFIG_*
#include "output.h"  // dprintf
#include "stacks.h"  // yield
#include "util.h"    // timer_setup
#include "x86.h"     // cpuid

#include <time.h>

#include "microbench.h"

#define HPET_COUNTER 0x0f0
#define PORT_PIT_COUNTER0 0x0040
#define PORT_PIT_MODE 0x0043
#define PM_SEL_READBACK (3 << 6)
#define PM_READ_COUNTER0 (1 << 1)
#define PM_READ_VALUE (1 << 4)

#include "linux/asm/apicdef.h"

extern u16 TimerPort;
extern u8 ShiftTSC;

time_t rtc_now ( void );

u64 tsc_now(void)
{
  return rdtscll() >> GET_GLOBAL(ShiftTSC);
}

u64 pmtimer_now(void)
{
  u16 port = GET_GLOBAL(TimerPort);
  return timer_adjust_bits(inl(port), 0xffffff);
}

u64 pit_now(void)
{
  // Read from PIT.
  outb(PM_SEL_READBACK | PM_READ_VALUE | PM_READ_COUNTER0, PORT_PIT_MODE);
  u16 v = inb(PORT_PIT_COUNTER0) | (inb(PORT_PIT_COUNTER0) << 8);
  return timer_adjust_bits(v, 0xffff);
}

u64 hpet_now(void)
{
  return readl(BUILD_HPET_ADDRESS + HPET_COUNTER);
}

void time_service(void)
{
    tsc_now();
    pmtimer_now();
    pit_now();
    hpet_now();
/*
    rtc_now();
*/
}
