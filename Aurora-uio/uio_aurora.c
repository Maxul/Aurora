/*
 * uio_aurora.c
 * This file is derived from svm.c of the VAMOS project
 *
 * Copyright (C) 2017-2018 - Maxul Lee
 *
 * VAMOS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * VAMOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VAMOS. If not, see <http://www.gnu.org/licenses/>.
 */

/*
    Original: Ecular Xu <ecular_xu@trendmicro.com.cn>
    Revised:  Maxul Lee <maxul@bupt.edu.cn>
    Last modified: 2018.6.10
*/

#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/pci.h>
#include <linux/platform_device.h>

#include <linux/uio_driver.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ecular Xu & Maxul Lee");
MODULE_DESCRIPTION("UIO: Interrupts relay from SMM to SGX, vice versa");

#define CDEV_NAME "aurora_pci"
#define EDU_DEVICE_ID 0x11e8
#define QEMU_VENDOR_ID 0x1234

#define SVM_OFFSET 0x64
#define SVM_SIZE ((1<<20))
#define SVM_CAPACITY 0x100

static void *svm_ram = NULL;
static void *svm_vmem = NULL;

/* inform SMM Supervisor to locate UIO shared memory */
static void taint(void)
{
    svm_ram = phys_to_virt(0x0);
    memset(svm_ram, 0x0, SVM_CAPACITY);
    *(long long unsigned int *)(svm_ram + SVM_OFFSET) = virt_to_phys(svm_vmem);
}

/* clear trace */
static void sweep(void)
{
    memset(svm_ram, 0x0, SVM_CAPACITY);
    svm_ram = svm_vmem = NULL;
}

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, }
};

static int major;
static struct pci_dev *pci_dev;

static struct uio_info* info;
static struct platform_device *pdev;

static irqreturn_t irq_handler(int irq, void *dev)
{
    static int i = 0;
	irqreturn_t ret;

	if (*(int *)dev == major) {
        pr_info("Firing IPI interrupt %d\n", ++i);
        uio_event_notify(info);
		ret = IRQ_HANDLED;
	} else {
		ret = IRQ_NONE;
	}
	return ret;
}

static const struct file_operations fops = {
	.owner   = THIS_MODULE,
};

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	dev_info(&(dev->dev), "edu pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pci_dev = dev;
	if (pci_enable_device(dev) < 0) {
		dev_err(&(dev->dev), "edu pci_enable_device\n");
		goto error;
	}

	if (request_irq(dev->irq, irq_handler, IRQF_SHARED, "edu_handler", &major) < 0) {
		dev_err(&(dev->dev), "request_irq\n");
		goto error;
	}
	return 0;
error:
	return 1;
}

static void pci_remove(struct pci_dev *dev)
{
	pr_info("edu pci_remove\n");
	free_irq(pci_dev->irq, &major);
	unregister_chrdev(major, CDEV_NAME);
}

static struct pci_driver pci_driver = {
	.name     = CDEV_NAME,
	.id_table = pci_ids,
	.probe    = pci_probe,
	.remove   = pci_remove,
};

static irqreturn_t uio_irq_handler(int irq, struct uio_info *dev)
{
    uio_event_notify(info);
    return IRQ_HANDLED;
}

static int uio_open(struct uio_info *info, struct inode *inode)
{
    pr_info("%s called\n", __FUNCTION__);
    return 0;
}

static int uio_release(struct uio_info *info, struct inode *inode)
{
    pr_info("%s called\n", __FUNCTION__);
    return 0;
}

static int uio_irq_control(struct uio_info *info, s32 irq_on)
{
    if(irq_on) {
        enable_irq(info->irq);
    } else {
        disable_irq_nosync(info->irq);
    }
    
    pr_info("%s called\n", __FUNCTION__);
    return 0;
}

static int aurora_init(void)
{
	if (pci_register_driver(&pci_driver) < 0) {
		return 1;
	}
    pdev = platform_device_register_simple("uio_platform_device", 0, NULL, 0);
    if (IS_ERR(pdev)) {
        pr_err("Failed to register platform device.\n");
        return -EINVAL;
    }

    info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);

    if (!info)
        return -ENOMEM;

    info->name = "uio_aurora_driver";
    info->version = "0.1";
    /* Allocate contiguous physical memory in kernel */
    svm_vmem = kmalloc(SVM_SIZE, GFP_KERNEL);
    info->mem[0].addr = (phys_addr_t) svm_vmem;
    if (!info->mem[0].addr)
        goto uiomem;
    info->mem[0].memtype = UIO_MEM_LOGICAL;
    info->mem[0].size = SVM_SIZE;
    info->irq = 13;
    info->handler = uio_irq_handler;
    info->open = uio_open;
    info->release = uio_release;
    info->irqcontrol = uio_irq_control;

    if(uio_register_device(&pdev->dev, info)) {
        pr_err("Unable to register UIO device!\n");
        goto devmem;
    }

    memset((void *)info->mem[0].addr, 0, SVM_SIZE);
//    memset((void *)info->mem[0].addr, 'z', SVM_SIZE-1);
    taint();
    pr_info("Aurora UIO driver loaded; shared memory at 0x%llx\n",
         virt_to_phys(svm_vmem));
    return 0;

devmem:
    kfree((void *)info->mem[0].addr);
uiomem:
    kfree(info);

    return -ENODEV;
}

static void aurora_exit(void)
{
	pci_unregister_driver(&pci_driver);

    if (info) {
        pr_info("Unregistering Aurora uio\n");
        uio_unregister_device(info);
        kfree((void *)info->mem[0].addr);
        kfree(info);
        info = 0;
    }
    if (pdev)
        platform_device_unregister(pdev);
    sweep();
}

module_init(aurora_init);
module_exit(aurora_exit);
MODULE_DEVICE_TABLE(pci, pci_ids);
