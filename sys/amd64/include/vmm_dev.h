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

#ifndef	_VMM_DEV_H_
#define	_VMM_DEV_H_

#ifdef _KERNEL
void	vmmdev_init(void);
int	vmmdev_cleanup(void);
#endif

struct vm_memory_segment {
	vm_paddr_t	gpa;	/* in */
	size_t		len;
	int		wired;
};

struct vm_register {
	int		cpuid;
	int		regnum;		/* enum vm_reg_name */
	uint64_t	regval;
};

struct vm_seg_desc {			/* data or code segment */
	int		cpuid;
	int		regnum;		/* enum vm_reg_name */
	struct seg_desc desc;
};

struct vm_run {
	int		cpuid;
	uint64_t	rip;		/* start running here */
	struct vm_exit	vm_exit;
};

struct vm_event {
	int		cpuid;
	enum vm_event_type type;
	int		vector;
	uint32_t	error_code;
	int		error_code_valid;
};

struct vm_lapic_msi {
	uint64_t	msg;
	uint64_t	addr;
};

struct vm_lapic_irq {
	int		cpuid;
	int		vector;
};

struct vm_ioapic_irq {
	int		irq;
};

struct vm_capability {
	int		cpuid;
	enum vm_cap_type captype;
	int		capval;
	int		allcpus;
};

struct vm_pptdev {
	int		bus;
	int		slot;
	int		func;
};

struct vm_pptdev_mmio {
	int		bus;
	int		slot;
	int		func;
	vm_paddr_t	gpa;
	vm_paddr_t	hpa;
	size_t		len;
};

struct vm_pptdev_msi {
	int		vcpu;
	int		bus;
	int		slot;
	int		func;
	int		numvec;		/* 0 means disabled */
	uint64_t	msg;
	uint64_t	addr;
};

struct vm_pptdev_msix {
	int		vcpu;
	int		bus;
	int		slot;
	int		func;
	int		idx;
	uint64_t	msg;
	uint32_t	vector_control;
	uint64_t	addr;
};

struct vm_nmi {
	int		cpuid;
};

#define	MAX_VM_STATS	64
struct vm_stats {
	int		cpuid;				/* in */
	int		num_entries;			/* out */
	struct timeval	tv;
	uint64_t	statbuf[MAX_VM_STATS];
};

struct vm_stat_desc {
	int		index;				/* in */
	char		desc[128];			/* out */
};

struct vm_x2apic {
	int			cpuid;
	enum x2apic_state	state;
};

struct vm_gpa_pte {
	uint64_t	gpa;				/* in */
	uint64_t	pte[4];				/* out */
	int		ptenum;
};

struct vm_hpet_cap {
	uint32_t	capabilities;	/* lower 32 bits of HPET capabilities */
};

enum {
	/* general routines */
	IOCNUM_ABIVERS = 0,
	IOCNUM_RUN = 1,
	IOCNUM_SET_CAPABILITY = 2,
	IOCNUM_GET_CAPABILITY = 3,

	/* memory apis */
	IOCNUM_MAP_MEMORY = 10,
	IOCNUM_GET_MEMORY_SEG = 11,
	IOCNUM_GET_GPA_PMAP = 12,

	/* register/state accessors */
	IOCNUM_SET_REGISTER = 20,
	IOCNUM_GET_REGISTER = 21,
	IOCNUM_SET_SEGMENT_DESCRIPTOR = 22,
	IOCNUM_GET_SEGMENT_DESCRIPTOR = 23,

	/* interrupt injection */
	IOCNUM_INJECT_EVENT = 30,
	IOCNUM_LAPIC_IRQ = 31,
	IOCNUM_INJECT_NMI = 32,
	IOCNUM_IOAPIC_ASSERT_IRQ = 33,
	IOCNUM_IOAPIC_DEASSERT_IRQ = 34,
	IOCNUM_IOAPIC_PULSE_IRQ = 35,
	IOCNUM_LAPIC_MSI = 36,

	/* PCI pass-thru */
	IOCNUM_BIND_PPTDEV = 40,
	IOCNUM_UNBIND_PPTDEV = 41,
	IOCNUM_MAP_PPTDEV_MMIO = 42,
	IOCNUM_PPTDEV_MSI = 43,
	IOCNUM_PPTDEV_MSIX = 44,

	/* statistics */
	IOCNUM_VM_STATS = 50, 
	IOCNUM_VM_STAT_DESC = 51,

	/* kernel device state */
	IOCNUM_SET_X2APIC_STATE = 60,
	IOCNUM_GET_X2APIC_STATE = 61,
	IOCNUM_GET_HPET_CAPABILITIES = 62,
};

#define	VM_RUN		\
	_IOWR('v', IOCNUM_RUN, struct vm_run)
#define	VM_MAP_MEMORY	\
	_IOWR('v', IOCNUM_MAP_MEMORY, struct vm_memory_segment)
#define	VM_GET_MEMORY_SEG \
	_IOWR('v', IOCNUM_GET_MEMORY_SEG, struct vm_memory_segment)
#define	VM_SET_REGISTER \
	_IOW('v', IOCNUM_SET_REGISTER, struct vm_register)
#define	VM_GET_REGISTER \
	_IOWR('v', IOCNUM_GET_REGISTER, struct vm_register)
#define	VM_SET_SEGMENT_DESCRIPTOR \
	_IOW('v', IOCNUM_SET_SEGMENT_DESCRIPTOR, struct vm_seg_desc)
#define	VM_GET_SEGMENT_DESCRIPTOR \
	_IOWR('v', IOCNUM_GET_SEGMENT_DESCRIPTOR, struct vm_seg_desc)
#define	VM_INJECT_EVENT	\
	_IOW('v', IOCNUM_INJECT_EVENT, struct vm_event)
#define	VM_LAPIC_IRQ 		\
	_IOW('v', IOCNUM_LAPIC_IRQ, struct vm_lapic_irq)
#define	VM_LAPIC_MSI		\
	_IOW('v', IOCNUM_LAPIC_MSI, struct vm_lapic_msi)
#define	VM_IOAPIC_ASSERT_IRQ	\
	_IOW('v', IOCNUM_IOAPIC_ASSERT_IRQ, struct vm_ioapic_irq)
#define	VM_IOAPIC_DEASSERT_IRQ	\
	_IOW('v', IOCNUM_IOAPIC_DEASSERT_IRQ, struct vm_ioapic_irq)
#define	VM_IOAPIC_PULSE_IRQ	\
	_IOW('v', IOCNUM_IOAPIC_PULSE_IRQ, struct vm_ioapic_irq)
#define	VM_SET_CAPABILITY \
	_IOW('v', IOCNUM_SET_CAPABILITY, struct vm_capability)
#define	VM_GET_CAPABILITY \
	_IOWR('v', IOCNUM_GET_CAPABILITY, struct vm_capability)
#define	VM_BIND_PPTDEV \
	_IOW('v', IOCNUM_BIND_PPTDEV, struct vm_pptdev)
#define	VM_UNBIND_PPTDEV \
	_IOW('v', IOCNUM_UNBIND_PPTDEV, struct vm_pptdev)
#define	VM_MAP_PPTDEV_MMIO \
	_IOW('v', IOCNUM_MAP_PPTDEV_MMIO, struct vm_pptdev_mmio)
#define	VM_PPTDEV_MSI \
	_IOW('v', IOCNUM_PPTDEV_MSI, struct vm_pptdev_msi)
#define	VM_PPTDEV_MSIX \
	_IOW('v', IOCNUM_PPTDEV_MSIX, struct vm_pptdev_msix)
#define VM_INJECT_NMI \
	_IOW('v', IOCNUM_INJECT_NMI, struct vm_nmi)
#define	VM_STATS \
	_IOWR('v', IOCNUM_VM_STATS, struct vm_stats)
#define	VM_STAT_DESC \
	_IOWR('v', IOCNUM_VM_STAT_DESC, struct vm_stat_desc)
#define	VM_SET_X2APIC_STATE \
	_IOW('v', IOCNUM_SET_X2APIC_STATE, struct vm_x2apic)
#define	VM_GET_X2APIC_STATE \
	_IOWR('v', IOCNUM_GET_X2APIC_STATE, struct vm_x2apic)
#define	VM_GET_HPET_CAPABILITIES \
	_IOR('v', IOCNUM_GET_HPET_CAPABILITIES, struct vm_hpet_cap)
#define	VM_GET_GPA_PMAP \
	_IOWR('v', IOCNUM_GET_GPA_PMAP, struct vm_gpa_pte)
#endif
