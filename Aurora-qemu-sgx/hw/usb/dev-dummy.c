/*
 * A dummy QEMU USB device to use as a skeleton for writing your own.
 *
 * Copyright (c) 2012 Pantelis Koukousoulas (pktoss@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "sysemu/char.h"

#include "hw/hw.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"

#include <stdint.h>

#define TYPE_USB_DUMMY "usb-dummy-dev"
#define USB_DUMMY_DEV(obj) OBJECT_CHECK(MSDState, (obj), TYPE_USB_DUMMY)

typedef struct USBDummyState {
    USBDevice dev;
} USBDummyState;

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER]     = "QEMU-AURORA",
    [STR_PRODUCT]          = "USB-AURORA",
    [STR_SERIALNUMBER]     = "1",
};

static const VMStateDescription vmstate_usb_dummy = {
    .name                  = "usb-dummy",
    .unmigratable          = 1,
};

static const USBDescIface desc_iface_dummy = {
    .bInterfaceNumber      = 0,
    .bNumEndpoints         = 0,
    .bInterfaceClass       = USB_CLASS_VENDOR_SPEC,
};

static const USBDescDevice desc_device_dummy = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = 8,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 1,
            .bConfigurationValue   = 1,
            .bmAttributes          = 0x80,
            .bMaxPower             = 40,
            .nif = 1,
            .ifs = &desc_iface_dummy,
        },
    },
};

static const USBDesc desc_usbdummy = {
    .id = {
        .idVendor          = 0xdada,
        .idProduct         = 0xbeef,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_dummy,
    .str  = desc_strings,
};

static void usb_dummy_initfn(USBDevice *dev, Error **errp)
{
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
}

static int usb_dummy_handle_control(
USBDevice *dev, USBPacket *p, int request,
int value, int index, int length, uint8_t *data)
{
    int ret;
    ret = usb_desc_handle_control(dev, p, request,
                                  value, index,
                                  length, data);
    if (ret >= 0) {
        return ret;
    }

    return 0;
}

static void usb_dummy_class_init(ObjectClass *klass,
                                void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "A dummy USB Device";
    uc->usb_desc       = &desc_usbdummy;
    uc->realize           = usb_dummy_initfn;
    uc->handle_control = usb_dummy_handle_control;
    dc->desc = "Dummy USB Device for QEMU";
    dc->vmsd = &vmstate_usb_dummy;
}

static TypeInfo dummy_info = {
    .name          = "usb-dummy",
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBDummyState),
    .class_init    = usb_dummy_class_init,
};

static void usb_dummy_register_types(void)
{
    type_register_static(&dummy_info);
    usb_legacy_register("usb-dummy", "dummy", NULL);
}

type_init(usb_dummy_register_types)

