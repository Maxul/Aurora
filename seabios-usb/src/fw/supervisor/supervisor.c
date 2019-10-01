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

#include "block.h" // DTYPE_USB
#include "biosvar.h" // GET_GLOBAL
#include "config.h" // CONFIG_*
#include "malloc.h" // free
#include "output.h" // dprintf
#include "romfile.h" // romfile_loadint
#include "string.h" // memset
#include "hw/usb.h" // struct usb_s
#include "hw/usb-ehci.h" // ehci_setup
#include "hw/usb-xhci.h" // xhci_setup
#include "hw/usb-hid.h" // usb_keyboard_setup
#include "hw/usb-hub.h" // usb_hub_setup
#include "hw/usb-msc.h" // usb_msc_setup
#include "hw/usb-ohci.h" // ohci_setup
#include "hw/usb-uas.h" // usb_uas_setup
#include "hw/usb-uhci.h" // uhci_setup
#include "util.h" // msleep
#include "x86.h" // __fls

#include "hw/pcidevice.h" // foreachpci
#include "hw/pci_ids.h" // PCI_CLASS_SERIAL_USB_OHCI
#include "hw/pci_regs.h" // PCI_BASE_ADDRESS_0

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
#define USB_IRQ 10
#define NIC_IRQ 11
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

#define HIJACKED_OFF	0x24

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

char isSupervisorOn = 0;

///
struct _keyevent {
    u8 modifiers;
    u8 reserved;
    u8 keys[6];
};
struct _keyevent *data;

static u16 _KeyToScanCode[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x001e, 0x0030, 0x002e, 0x0020,
    0x0012, 0x0021, 0x0022, 0x0023, 0x0017, 0x0024, 0x0025, 0x0026,
    0x0032, 0x0031, 0x0018, 0x0019, 0x0010, 0x0013, 0x001f, 0x0014,
    0x0016, 0x002f, 0x0011, 0x002d, 0x0015, 0x002c, 0x0002, 0x0003,
    0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b,
    0x001c, 0x0001, 0x000e, 0x000f, 0x0039, 0x000c, 0x000d, 0x001a,
    0x001b, 0x002b, 0x0000, 0x0027, 0x0028, 0x0029, 0x0033, 0x0034,
    0x0035, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f, 0x0040,
    0x0041, 0x0042, 0x0043, 0x0044, 0x0057, 0x0058, 0xe037, 0x0046,
    0xe145, 0xe052, 0xe047, 0xe049, 0xe053, 0xe04f, 0xe051, 0xe04d,
    0xe04b, 0xe050, 0xe048, 0x0045, 0xe035, 0x0037, 0x004a, 0x004e,
    0xe01c, 0x004f, 0x0050, 0x0051, 0x004b, 0x004c, 0x004d, 0x0047,
    0x0048, 0x0049, 0x0052, 0x0053
};

unsigned char pckbd[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
"\r\000/"; /* 0x60 - 0x6f */
///

void supervisor(void)
{
    enable_sse();

    if (!isSupervisorOn) {
        init_ring_buffer();

        show_ioredtbl();
        
        intercept_ehci();
        
        int *p = 0x100;
        dprintf(1, "%p\n", *p);
        data = *p;

        isSupervisorOn = 1;
    } else {
//        static int hijacked_times = 0;
//        dprintf(1, "[aurora] USB ehci interrupt: %d!\n", ++hijacked_times);

        if (data->keys[0])
        dprintf(1, "[aurora] USB ehci key 0x%x %c\n",
            data->keys[0], pckbd[_KeyToScanCode[data->keys[0]]]);
        data->keys[0] = 0;

        trigger_self_ipi(usb_vec);
    }

}

#undef memset

