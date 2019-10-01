#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "sysemu/kvm.h"

typedef struct SGX_State {
  PCIDevice parent_obj;

  int pos;
  char *buf;
  int buflen;

  MemoryRegion mmio;
  MemoryRegion portio;
} SGX_State;

#define TYPE_PCI_SGX_DEV "pci-virt-dev"
#define PCI_SGX_DEV(obj) \
    OBJECT_CHECK(SGX_State, (obj), TYPE_PCI_SGX_DEV)

static uint64_t pci_sgx_read(void *opaque, hwaddr addr, unsigned size) {
  SGX_State *d = opaque;

  if (addr == 0)
    return d->buf[d->pos++];
  else
    return d->buflen;
}

static void pci_sgx_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                  unsigned size) {

  SGX_State *d = opaque;

  switch (addr) {
  case 0:
    /* write byte */
    if (!d->buf)
      break;
    if (d->pos >= d->buflen)
      break;
    d->buf[d->pos++] = (uint8_t)val;
    break;
  case 1:
    /* reset pos */
    d->pos = 0;
    break;
  case 2:
    /* set buffer length */
    d->buflen = val + 1;
    g_free(d->buf);
    d->buf = g_malloc(d->buflen);
    break;
  }

  return;
}

static const MemoryRegionOps pci_sgx_mmio_ops = {
    .read = pci_sgx_read,
    .write = pci_sgx_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int pci_sgx_init(PCIDevice *pci_dev) {
  SGX_State *d = PCI_SGX_DEV(pci_dev);
  uint8_t *pci_conf;

  pci_conf = pci_dev->config;
  pci_conf[PCI_INTERRUPT_PIN] = 0; /* no interrupt pin */

  memory_region_init_io(&d->mmio, OBJECT(d), &pci_sgx_mmio_ops, d,
                        "pci-fake-dev-mmio", 1 << 8);
  pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);

  d->pos = 0;

#define MSG "Hey I wish You love me like I love you!\n"
#define LENGTH strlen(MSG)
  d->buf = g_malloc(LENGTH);
  memcpy(d->buf, MSG, LENGTH);
  d->buflen = LENGTH + 1;

  return 0;
}

static void pci_sgx_uninit(PCIDevice *dev) {
}

static void qdev_pci_sgx_reset(DeviceState *dev) {
}

static void pci_sgx_class_init(ObjectClass *klass, void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);
  PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

  k->init = pci_sgx_init;
  k->exit = pci_sgx_uninit;
  k->vendor_id = 0x2017;
  k->device_id = 0x2018;
  k->revision = 0x00;
  k->class_id = PCI_CLASS_OTHERS;
  dc->desc = "SGX PCI";
  set_bit(DEVICE_CATEGORY_MISC, dc->categories);
  dc->reset = qdev_pci_sgx_reset;
}

static const TypeInfo pci_lev_info = {
    .name = TYPE_PCI_SGX_DEV,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(SGX_State),
    .class_init = pci_sgx_class_init,
};

static void pci_lev_register_types(void) {
  type_register_static(&pci_lev_info);
}

type_init(pci_lev_register_types)
