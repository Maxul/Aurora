#include "output.h" // dprintf
#include "string.h" // memcpy
#include "x86.h" // inb

#include "smx.h"
#include "sodium.h"
#include "microbench.h"

#define CONFIG_X86_32
#include "linux/asm/apicdef.h"
#include "apic.h"

#include <stdint.h>
#include <stdlib.h>

extern void *shmem_base;

/**
    Since the native memset provided by SeaBIOS is so inefficient in 32-bit SMM
    therefore we need to replace it for better performance
*/
#define memset __builtin_memset

static inline void enable_sse(void)
{
    asm volatile("movl %cr0, %eax\n\t"
               "andl %eax, 0xFFFB\n\t"
               "orl %eax, 0x2\n\t"
               "movl %eax, %cr0\n\t"
               "movl %cr4, %eax\n\t"
               "orl $1536, %eax\n\t"
               "movl %eax, %cr4\n");
}

/* Allowed int vectors are from 0x10 to 0xFE */

#define IOAPIC_BASE	(void *)IO_APIC_DEFAULT_PHYS_BASE
#define LOCAL_APIC_BASE	(void *)APIC_DEFAULT_PHYS_BASE

#define NMI_VECTOR			0x02
#define APIC_BASE (LOCAL_APIC_BASE)

void trigger_self_ipi(int vec)
{
    enable_x2apic();
    apic_icr_write(APIC_DEST_SELF | APIC_DEST_PHYSICAL | APIC_DM_FIXED | vec, 0);
    asm volatile ("nop");
}

#define IOREDTBL(x) (0x10 + 2 * x)

#define KBD_IRQ 1
#define USB_IRQ 11
#define NIC_IRQ 10
int nic_vec = 0, kbd_vec = 0, usb_vec = 0;

static void show_ioredtbl(void)
{
    int vector_address = 0;

    for (int i = 0; i < 24; ++i) {
        vector_address = 0xff & ioapic_read_reg(IOREDTBL(i));
        if (0x00 != vector_address)
            dprintf(1, "IRQ #%d\tIOREDTBL[0x%02x] addr: 0x%02x\n", i, IOREDTBL(i), vector_address);
        if (KBD_IRQ == i)
            kbd_vec = vector_address;
        if (USB_IRQ == i)
            usb_vec = vector_address;
        if (NIC_IRQ == i)
            nic_vec = vector_address;
    }
    dprintf(1, "\n");
}

#define HIJACKED_OFF	0x26

static void intercept_ehci(void)
{
    kbd_vec = ioapic_read_reg(HIJACKED_OFF);
    ioapic_write_reg(HIJACKED_OFF, APIC_DM_SMI);
    dprintf(1, "[aurora] intercepting IRQ #0x%02x\n", HIJACKED_OFF);
}
static void recover_ehci(void)
{
    ioapic_write_reg(HIJACKED_OFF, usb_vec);
    dprintf(1, "[aurora] remove IRQ #0x%02x intercepting\n", HIJACKED_OFF);
}

#define STATE_LEN 4  // In words

void supervisor(void)
{
#if 0
#define SERIALSIZE (1<<16)
    char str[SERIALSIZE];
    
    memset(str, 'X', SERIALSIZE);
    
    int x=0;
    for (;x<16;x++)
    serial_debug_puts(str, SERIALSIZE);
#endif

#if 0
    int i;
    uint32_t a, b;
    extern u8 ShiftTSC;
    
    char mem[1<<16];
    uint32_t hash[STATE_LEN];
    
    memset(mem, 'b', sizeof mem);
    
    a = rdtscll() >> GET_GLOBAL(ShiftTSC);
    for (i=0;i<100;i++)
    md5_hash(mem, 1<<12, hash);
    b = rdtscll() >> GET_GLOBAL(ShiftTSC);
    dprintf(1, "%lld\n", (b-a)/100/2808);

    a = rdtscll() >> GET_GLOBAL(ShiftTSC);
    for (i=0;i<100;i++)
    md5_hash(mem, 1<<14, hash);
    b = rdtscll() >> GET_GLOBAL(ShiftTSC);
    dprintf(1, "%lld\n", (b-a)/100/2808);

    a = rdtscll() >> GET_GLOBAL(ShiftTSC);
    for (i=0;i<100;i++)
    md5_hash(mem, 1<<16, hash);
    b = rdtscll() >> GET_GLOBAL(ShiftTSC);
    dprintf(1, "%lld\n", (b-a)/100/2808);
#endif

    static char isFirstPhrase = 1;

    enable_sse();

    if (isFirstPhrase) {
//        init_ring_buffer();

        show_ioredtbl();

        intercept_ehci();

        isFirstPhrase = 0;
    } else {
#if 0
        static char * str = "Printer comes true";
//        serial_debug_puts(str, strlen(str));

const char *MAGIC = "AURORA";
u32 *p = 0x100;
u32 *p2 = 0x200;

//dprintf(1, "[aurora] %p %p %p\n", *p, *p2, *(p2+1));
//_print_hex("USB DMA IOBUF", *p, *p2);
if (0 == memcmp(MAGIC, *p, sizeof(MAGIC))) {
//    _print_hex("USB DMA IOBUF", *p, 10);
    uint32_t hash[STATE_LEN];
    md5_hash((*p)+10, (*p2)-10, hash);
//    _print_hex("hash", hash, STATE_LEN);
    if (0 != memcmp(hash, (*p)+6, STATE_LEN))
        dprintf(1, "[aurora] MAC check FAILED\n");
}
#endif
    uint32_t hash[STATE_LEN];
    md5_hash(0, 1<<12, hash);

        static int hijacked_times = 0;
//        dprintf(1, "[aurora] USB ehci interrupt: %d!\n", ++hijacked_times);

        trigger_self_ipi(usb_vec);
    }

}

#undef memset

