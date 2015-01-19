/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMMAPI_H_
#define	_VMMAPI_H_

#include <sys/param.h>
#include <sys/cpuset.h>

/*
 * API version for out-of-tree consumers like grub-bhyve for making compile
 * time decisions.
 */
#define	VMMAPI_VERSION	0101	/* 2 digit major followed by 2 digit minor */

struct iovec;
struct vmctx;
enum x2apic_state;

/*
 * Different styles of mapping the memory assigned to a VM into the address
 * space of the controlling process.
 */
enum vm_mmap_style {
	VM_MMAP_NONE,		/* no mapping */
	VM_MMAP_ALL,		/* fully and statically mapped */
	VM_MMAP_SPARSE,		/* mappings created on-demand */
};

#define	VM_MEM_F_INCORE	0x01	/* include guest memory in core file */

int	vm_create(const char *name);
struct vmctx *vm_open(const char *name);
void	vm_destroy(struct vmctx *ctx);
int	vm_parse_memsize(const char *optarg, size_t *memsize);
int	vm_get_memory_seg(struct vmctx *ctx, vm_paddr_t gpa, size_t *ret_len,
			  int *wired);
int	vm_setup_memory(struct vmctx *ctx, size_t len, enum vm_mmap_style s);
void	*vm_map_gpa(struct vmctx *ctx, vm_paddr_t gaddr, size_t len);
int	vm_get_gpa_pmap(struct vmctx *, uint64_t gpa, uint64_t *pte, int *num);
uint32_t vm_get_lowmem_limit(struct vmctx *ctx);
void	vm_set_lowmem_limit(struct vmctx *ctx, uint32_t limit);
void	vm_set_memflags(struct vmctx *ctx, int flags);
size_t	vm_get_lowmem_size(struct vmctx *ctx);
size_t	vm_get_highmem_size(struct vmctx *ctx);
int	vm_set_desc(struct vmctx *ctx, int vcpu, int reg,
		    uint64_t base, uint32_t limit, uint32_t access);
int	vm_get_desc(struct vmctx *ctx, int vcpu, int reg,
		    uint64_t *base, uint32_t *limit, uint32_t *access);
int	vm_get_seg_desc(struct vmctx *ctx, int vcpu, int reg,
			struct seg_desc *seg_desc);
int	vm_set_register(struct vmctx *ctx, int vcpu, int reg, uint64_t val);
int	vm_get_register(struct vmctx *ctx, int vcpu, int reg, uint64_t *retval);
int	vm_run(struct vmctx *ctx, int vcpu, struct vm_exit *ret_vmexit);
int	vm_suspend(struct vmctx *ctx, enum vm_suspend_how how);
int	vm_reinit(struct vmctx *ctx);
int	vm_apicid2vcpu(struct vmctx *ctx, int apicid);
int	vm_inject_exception(struct vmctx *ctx, int vcpu, int vector,
    int errcode_valid, uint32_t errcode, int restart_instruction);
int	vm_lapic_irq(struct vmctx *ctx, int vcpu, int vector);
int	vm_lapic_local_irq(struct vmctx *ctx, int vcpu, int vector);
int	vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg);
int	vm_ioapic_assert_irq(struct vmctx *ctx, int irq);
int	vm_ioapic_deassert_irq(struct vmctx *ctx, int irq);
int	vm_ioapic_pulse_irq(struct vmctx *ctx, int irq);
int	vm_ioapic_pincount(struct vmctx *ctx, int *pincount);
int	vm_isa_assert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_isa_deassert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_isa_pulse_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_isa_set_irq_trigger(struct vmctx *ctx, int atpic_irq,
	    enum vm_intr_trigger trigger);
int	vm_inject_nmi(struct vmctx *ctx, int vcpu);
int	vm_capability_name2type(const char *capname);
const char *vm_capability_type2name(int type);
int	vm_get_capability(struct vmctx *ctx, int vcpu, enum vm_cap_type cap,
			  int *retval);
int	vm_set_capability(struct vmctx *ctx, int vcpu, enum vm_cap_type cap,
			  int val);
int	vm_assign_pptdev(struct vmctx *ctx, int bus, int slot, int func);
int	vm_unassign_pptdev(struct vmctx *ctx, int bus, int slot, int func);
int	vm_map_pptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
			   vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int	vm_setup_pptdev_msi(struct vmctx *ctx, int vcpu, int bus, int slot,
	    int func, uint64_t addr, uint64_t msg, int numvec);
int	vm_setup_pptdev_msix(struct vmctx *ctx, int vcpu, int bus, int slot,
	    int func, int idx, uint64_t addr, uint64_t msg,
	    uint32_t vector_control);

int	vm_get_intinfo(struct vmctx *ctx, int vcpu, uint64_t *i1, uint64_t *i2);
int	vm_set_intinfo(struct vmctx *ctx, int vcpu, uint64_t exit_intinfo);

/*
 * Return a pointer to the statistics buffer. Note that this is not MT-safe.
 */
uint64_t *vm_get_stats(struct vmctx *ctx, int vcpu, struct timeval *ret_tv,
		       int *ret_entries);
const char *vm_get_stat_desc(struct vmctx *ctx, int index);

int	vm_get_x2apic_state(struct vmctx *ctx, int vcpu, enum x2apic_state *s);
int	vm_set_x2apic_state(struct vmctx *ctx, int vcpu, enum x2apic_state s);

int	vm_get_hpet_capabilities(struct vmctx *ctx, uint32_t *capabilities);

/*
 * Translate the GLA range [gla,gla+len) into GPA segments in 'iov'.
 * The 'iovcnt' should be big enough to accomodate all GPA segments.
 * Returns 0 on success, 1 on a guest fault condition and -1 otherwise.
 */
int	vm_copy_setup(struct vmctx *ctx, int vcpu, struct vm_guest_paging *pg,
	    uint64_t gla, size_t len, int prot, struct iovec *iov, int iovcnt);
void	vm_copyin(struct vmctx *ctx, int vcpu, struct iovec *guest_iov,
	    void *host_dst, size_t len);
void	vm_copyout(struct vmctx *ctx, int vcpu, const void *host_src,
	    struct iovec *guest_iov, size_t len);
void	vm_copy_teardown(struct vmctx *ctx, int vcpu, struct iovec *iov,
	    int iovcnt);

/* RTC */
int	vm_rtc_write(struct vmctx *ctx, int offset, uint8_t value);
int	vm_rtc_read(struct vmctx *ctx, int offset, uint8_t *retval);
int	vm_rtc_settime(struct vmctx *ctx, time_t secs);
int	vm_rtc_gettime(struct vmctx *ctx, time_t *secs);

/* Reset vcpu register state */
int	vcpu_reset(struct vmctx *ctx, int vcpu);

int	vm_active_cpus(struct vmctx *ctx, cpuset_t *cpus);
int	vm_suspended_cpus(struct vmctx *ctx, cpuset_t *cpus);
int	vm_activate_cpu(struct vmctx *ctx, int vcpu);

/*
 * FreeBSD specific APIs
 */
int	vm_setup_freebsd_registers(struct vmctx *ctx, int vcpu,
				uint64_t rip, uint64_t cr3, uint64_t gdtbase,
				uint64_t rsp);
int	vm_setup_freebsd_registers_i386(struct vmctx *vmctx, int vcpu,
					uint32_t eip, uint32_t gdtbase,
					uint32_t esp);
void	vm_setup_freebsd_gdt(uint64_t *gdtr);
#endif	/* _VMMAPI_H_ */
