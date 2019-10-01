/*
 * QEMU KVM support -- x86 specific functions.
 *
 * Copyright (c) 2012 Linaro Limited
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_KVM_I386_H
#define QEMU_KVM_I386_H

#include "sysemu/kvm.h"

#define kvm_apic_in_kernel() (kvm_irqchip_in_kernel())

bool kvm_allows_irq0_override(void);
bool kvm_has_smm(void);
void kvm_synchronize_all_tsc(void);
void kvm_arch_reset_vcpu(X86CPU *cs);
void kvm_arch_do_init_vcpu(X86CPU *cs);

int kvm_device_pci_assign(KVMState *s, PCIHostDeviceAddress *dev_addr,
                          uint32_t flags, uint32_t *dev_id);
int kvm_device_pci_deassign(KVMState *s, uint32_t dev_id);

int kvm_device_intx_assign(KVMState *s, uint32_t dev_id,
                           bool use_host_msi, uint32_t guest_irq);
int kvm_device_intx_set_mask(KVMState *s, uint32_t dev_id, bool masked);
int kvm_device_intx_deassign(KVMState *s, uint32_t dev_id, bool use_host_msi);

int kvm_device_msi_assign(KVMState *s, uint32_t dev_id, int virq);
int kvm_device_msi_deassign(KVMState *s, uint32_t dev_id);

bool kvm_device_msix_supported(KVMState *s);
int kvm_device_msix_init_vectors(KVMState *s, uint32_t dev_id,
                                 uint32_t nr_vectors);
int kvm_device_msix_set_vector(KVMState *s, uint32_t dev_id, uint32_t vector,
                               int virq);
int kvm_device_msix_assign(KVMState *s, uint32_t dev_id);
int kvm_device_msix_deassign(KVMState *s, uint32_t dev_id);

void kvm_put_apicbase(X86CPU *cpu, uint64_t value);

bool kvm_enable_x2apic(void);
bool kvm_has_x2apic_api(void);

struct SGXinfo {
    uint64_t epc_sz;    /* epc_sz == 0 also means SGX not supported */
    uint64_t epc_base;  /* Calculated by Qemu */
    uint64_t ia32_sgxlepubkeyhash[4];
    bool ia32_sgxlepubkeyhash_writable;
    bool le_wr;
};

bool sgx_lcp_is_intel_lehash(void);

extern struct SGXinfo *sgx_state;
void parse_sgx_options(void);

bool kvm_cpu_has_sgx(X86CPU *cpu);
bool kvm_cpu_has_sgx_lcp(X86CPU *cpu);
bool kvm_ia32_sgxlepubkeyhash_writable(X86CPU *cpu);

#endif
