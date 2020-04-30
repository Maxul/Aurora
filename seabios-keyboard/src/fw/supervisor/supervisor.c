#include "output.h" // dprintf
#include "string.h" // memcpy
#include "x86.h" // inb

#include "smx.h"
#include "sodium.h"
#include "microbench.h"

#define CONFIG_X86_32
#include "linux/i8042-io.h"
#include "linux/asm/apicdef.h"
#include "apic.h"

#include <stdint.h>
#include <stdlib.h>

#include "biosvar.h" // GET_BDA
#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "hw/ps2port.h" // ps2_kbd_command
#include "hw/usb-hid.h" // usb_kbd_command
#include "output.h" // debug_enter
#include "stacks.h" // yield
#include "string.h" // memset
#include "util.h" // kbd_init

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

static void show_ioredtbl(void)
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

#define HIJACKED_IRQ	0x12//0x2e//0x2c
#define INTERRUPT_VECTOR	0x31//66//0x3e

#define UIO_VECTOR 0x3a

static void hijack_keyboard(void)
{
    kbd_vec = ioapic_read_reg(HIJACKED_IRQ);
//    dprintf(1, "[aurora-supervisor] vector for IRQ #0x%02x is 0x%02x\n", HIJACKED_IRQ, kbd_vec);
    ioapic_write_reg(HIJACKED_IRQ, APIC_DM_SMI);
//    dprintf(1, "[aurora-supervisor] intercepting IRQ #0x%02x\n", HIJACKED_IRQ);
}
static void recover_keyboard(void)
{
    ioapic_write_reg(HIJACKED_IRQ, kbd_vec);
//    dprintf(1, "[aurora-supervisor] remove IRQ #0x%02x intercepting\n", HIJACKED_IRQ);
}

unsigned char pckbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
"\r\000/"; /* 0x60 - 0x6f */

void supervisor(void)
{
    static int kbd_data, i = 0;
    static char kbd_buffer[256];
    static char isFirstPhrase = 1;

    enable_sse();

    if (isFirstPhrase) {
        init_ring_buffer();

        show_ioredtbl();
        
        hijack_keyboard();
        memset(kbd_buffer, 0, sizeof kbd_buffer);

        isFirstPhrase = 0;
    } else {

if (-1 == i) {
    dprintf(1, "[aurora-supervisor] welcome to use secure input again\n");

    hijack_keyboard();
    memset(kbd_buffer, 0, sizeof kbd_buffer);
    
    i = 0;
    return;
}

        kbd_data = i8042_read_data();

        if (0x9c == kbd_data) {
//            dprintf(1, "\n%s\n", kbd_buffer);
            put_in_buffer(kbd_buffer, 4080);
            trigger_self_ipi(UIO_VECTOR);
            i = -1;
            recover_keyboard();
        }

        if (0x00 <= kbd_data && kbd_data <= 0x6f) {
//            dprintf(1, "%c", pckbd_sysrq_xlate[kbd_data]);
            kbd_buffer[i] = pckbd_sysrq_xlate[kbd_data];
            i++;
        }

    }
}

#undef memset

