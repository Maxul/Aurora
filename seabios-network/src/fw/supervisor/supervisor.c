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
#define NIC_IRQ 11
int nic_vec = 0, kbd_vec = 0;

static void show_ioredtbl()
{
    int vector_address = 0;

    for (int i = 0; i < 24; ++i) {
        vector_address = 0xff & ioapic_read_reg(IOREDTBL(i));
        if (0x00 != vector_address)
            dprintf(1, "IRQ #%d\tIOREDTBL[0x%02x] addr: 0x%02x\n", i, IOREDTBL(i), vector_address);
        if (KBD_IRQ == i)
            kbd_vec = vector_address;
        if (NIC_IRQ == i)
            nic_vec = vector_address;
    }
    dprintf(1, "\n");
}

void supervisor(void)
{
    static char isFirstPhrase = 1;

    enable_sse();

    if (isFirstPhrase) {
        init_ring_buffer();

        show_ioredtbl();
        
        if ( 0x3b == nic_vec ) {
	        ioapic_write_reg(IOREDTBL(NIC_IRQ), APIC_DM_SMI);
            dprintf(1, "=== >>> === >>> QEMU PCNET NIC HAS BEEN TAKEN <<< === <<< ===\n");
            pcnet32_probe();
        }

        isFirstPhrase = 0;
    } else {
        pcnet32_handler();
        trigger_self_ipi(0x3b);
    }
}

#undef memset

