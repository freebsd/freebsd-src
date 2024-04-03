/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <machine/specialreg.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_snapshot.h>

#include <string.h>

#include "vmmapi.h"
#include "internal.h"

const char *vm_capstrmap[] = {
	[VM_CAP_HALT_EXIT]  = "hlt_exit",
	[VM_CAP_MTRAP_EXIT] = "mtrap_exit",
	[VM_CAP_PAUSE_EXIT] = "pause_exit",
	[VM_CAP_UNRESTRICTED_GUEST] = "unrestricted_guest",
	[VM_CAP_ENABLE_INVPCID] = "enable_invpcid",
	[VM_CAP_BPT_EXIT] = "bpt_exit",
	[VM_CAP_RDPID] = "rdpid",
	[VM_CAP_RDTSCP] = "rdtscp",
	[VM_CAP_IPI_EXIT] = "ipi_exit",
	[VM_CAP_MASK_HWINTR] = "mask_hwintr",
	[VM_CAP_RFLAGS_TF] = "rflags_tf",
	[VM_CAP_MAX] = NULL,
};

#define	VM_MD_IOCTLS			\
	VM_SET_SEGMENT_DESCRIPTOR,	\
	VM_GET_SEGMENT_DESCRIPTOR,	\
	VM_SET_KERNEMU_DEV,		\
	VM_GET_KERNEMU_DEV,		\
	VM_LAPIC_IRQ,			\
	VM_LAPIC_LOCAL_IRQ,		\
	VM_LAPIC_MSI,			\
	VM_IOAPIC_ASSERT_IRQ,		\
	VM_IOAPIC_DEASSERT_IRQ,		\
	VM_IOAPIC_PULSE_IRQ,		\
	VM_IOAPIC_PINCOUNT,		\
	VM_ISA_ASSERT_IRQ,		\
	VM_ISA_DEASSERT_IRQ,		\
	VM_ISA_PULSE_IRQ,		\
	VM_ISA_SET_IRQ_TRIGGER,		\
	VM_INJECT_NMI,			\
	VM_SET_X2APIC_STATE,		\
	VM_GET_X2APIC_STATE,		\
	VM_GET_HPET_CAPABILITIES,	\
	VM_RTC_WRITE,			\
	VM_RTC_READ,			\
	VM_RTC_SETTIME,			\
	VM_RTC_GETTIME

const cap_ioctl_t vm_ioctl_cmds[] = {
	VM_COMMON_IOCTLS,
	VM_PPT_IOCTLS,
	VM_MD_IOCTLS,
};
size_t vm_ioctl_ncmds = nitems(vm_ioctl_cmds);

int
vm_set_desc(struct vcpu *vcpu, int reg,
	    uint64_t base, uint32_t limit, uint32_t access)
{
	int error;
	struct vm_seg_desc vmsegdesc;

	bzero(&vmsegdesc, sizeof(vmsegdesc));
	vmsegdesc.regnum = reg;
	vmsegdesc.desc.base = base;
	vmsegdesc.desc.limit = limit;
	vmsegdesc.desc.access = access;

	error = vcpu_ioctl(vcpu, VM_SET_SEGMENT_DESCRIPTOR, &vmsegdesc);
	return (error);
}

int
vm_get_desc(struct vcpu *vcpu, int reg, uint64_t *base, uint32_t *limit,
    uint32_t *access)
{
	int error;
	struct vm_seg_desc vmsegdesc;

	bzero(&vmsegdesc, sizeof(vmsegdesc));
	vmsegdesc.regnum = reg;

	error = vcpu_ioctl(vcpu, VM_GET_SEGMENT_DESCRIPTOR, &vmsegdesc);
	if (error == 0) {
		*base = vmsegdesc.desc.base;
		*limit = vmsegdesc.desc.limit;
		*access = vmsegdesc.desc.access;
	}
	return (error);
}

int
vm_get_seg_desc(struct vcpu *vcpu, int reg, struct seg_desc *seg_desc)
{
	int error;

	error = vm_get_desc(vcpu, reg, &seg_desc->base, &seg_desc->limit,
	    &seg_desc->access);
	return (error);
}

int
vm_lapic_irq(struct vcpu *vcpu, int vector)
{
	struct vm_lapic_irq vmirq;

	bzero(&vmirq, sizeof(vmirq));
	vmirq.vector = vector;

	return (vcpu_ioctl(vcpu, VM_LAPIC_IRQ, &vmirq));
}

int
vm_lapic_local_irq(struct vcpu *vcpu, int vector)
{
	struct vm_lapic_irq vmirq;

	bzero(&vmirq, sizeof(vmirq));
	vmirq.vector = vector;

	return (vcpu_ioctl(vcpu, VM_LAPIC_LOCAL_IRQ, &vmirq));
}

int
vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg)
{
	struct vm_lapic_msi vmmsi;

	bzero(&vmmsi, sizeof(vmmsi));
	vmmsi.addr = addr;
	vmmsi.msg = msg;

	return (ioctl(ctx->fd, VM_LAPIC_MSI, &vmmsi));
}

int
vm_raise_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg,
    int bus __unused, int slot __unused, int func __unused)
{
	return (vm_lapic_msi(ctx, addr, msg));
}

int
vm_apicid2vcpu(struct vmctx *ctx __unused, int apicid)
{
	/*
	 * The apic id associated with the 'vcpu' has the same numerical value
	 * as the 'vcpu' itself.
	 */
	return (apicid);
}

int
vm_ioapic_assert_irq(struct vmctx *ctx, int irq)
{
	struct vm_ioapic_irq ioapic_irq;

	bzero(&ioapic_irq, sizeof(struct vm_ioapic_irq));
	ioapic_irq.irq = irq;

	return (ioctl(ctx->fd, VM_IOAPIC_ASSERT_IRQ, &ioapic_irq));
}

int
vm_ioapic_deassert_irq(struct vmctx *ctx, int irq)
{
	struct vm_ioapic_irq ioapic_irq;

	bzero(&ioapic_irq, sizeof(struct vm_ioapic_irq));
	ioapic_irq.irq = irq;

	return (ioctl(ctx->fd, VM_IOAPIC_DEASSERT_IRQ, &ioapic_irq));
}

int
vm_ioapic_pulse_irq(struct vmctx *ctx, int irq)
{
	struct vm_ioapic_irq ioapic_irq;

	bzero(&ioapic_irq, sizeof(struct vm_ioapic_irq));
	ioapic_irq.irq = irq;

	return (ioctl(ctx->fd, VM_IOAPIC_PULSE_IRQ, &ioapic_irq));
}

int
vm_ioapic_pincount(struct vmctx *ctx, int *pincount)
{

	return (ioctl(ctx->fd, VM_IOAPIC_PINCOUNT, pincount));
}

int
vm_isa_assert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq)
{
	struct vm_isa_irq isa_irq;

	bzero(&isa_irq, sizeof(struct vm_isa_irq));
	isa_irq.atpic_irq = atpic_irq;
	isa_irq.ioapic_irq = ioapic_irq;

	return (ioctl(ctx->fd, VM_ISA_ASSERT_IRQ, &isa_irq));
}

int
vm_isa_deassert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq)
{
	struct vm_isa_irq isa_irq;

	bzero(&isa_irq, sizeof(struct vm_isa_irq));
	isa_irq.atpic_irq = atpic_irq;
	isa_irq.ioapic_irq = ioapic_irq;

	return (ioctl(ctx->fd, VM_ISA_DEASSERT_IRQ, &isa_irq));
}

int
vm_isa_pulse_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq)
{
	struct vm_isa_irq isa_irq;

	bzero(&isa_irq, sizeof(struct vm_isa_irq));
	isa_irq.atpic_irq = atpic_irq;
	isa_irq.ioapic_irq = ioapic_irq;

	return (ioctl(ctx->fd, VM_ISA_PULSE_IRQ, &isa_irq));
}

int
vm_isa_set_irq_trigger(struct vmctx *ctx, int atpic_irq,
    enum vm_intr_trigger trigger)
{
	struct vm_isa_irq_trigger isa_irq_trigger;

	bzero(&isa_irq_trigger, sizeof(struct vm_isa_irq_trigger));
	isa_irq_trigger.atpic_irq = atpic_irq;
	isa_irq_trigger.trigger = trigger;

	return (ioctl(ctx->fd, VM_ISA_SET_IRQ_TRIGGER, &isa_irq_trigger));
}

int
vm_inject_nmi(struct vcpu *vcpu)
{
	struct vm_nmi vmnmi;

	bzero(&vmnmi, sizeof(vmnmi));

	return (vcpu_ioctl(vcpu, VM_INJECT_NMI, &vmnmi));
}

int
vm_inject_exception(struct vcpu *vcpu, int vector, int errcode_valid,
    uint32_t errcode, int restart_instruction)
{
	struct vm_exception exc;

	exc.vector = vector;
	exc.error_code = errcode;
	exc.error_code_valid = errcode_valid;
	exc.restart_instruction = restart_instruction;

	return (vcpu_ioctl(vcpu, VM_INJECT_EXCEPTION, &exc));
}

int
vm_readwrite_kernemu_device(struct vcpu *vcpu, vm_paddr_t gpa,
    bool write, int size, uint64_t *value)
{
	struct vm_readwrite_kernemu_device irp = {
		.access_width = fls(size) - 1,
		.gpa = gpa,
		.value = write ? *value : ~0ul,
	};
	long cmd = (write ? VM_SET_KERNEMU_DEV : VM_GET_KERNEMU_DEV);
	int rc;

	rc = vcpu_ioctl(vcpu, cmd, &irp);
	if (rc == 0 && !write)
		*value = irp.value;
	return (rc);
}

int
vm_get_x2apic_state(struct vcpu *vcpu, enum x2apic_state *state)
{
	int error;
	struct vm_x2apic x2apic;

	bzero(&x2apic, sizeof(x2apic));

	error = vcpu_ioctl(vcpu, VM_GET_X2APIC_STATE, &x2apic);
	*state = x2apic.state;
	return (error);
}

int
vm_set_x2apic_state(struct vcpu *vcpu, enum x2apic_state state)
{
	int error;
	struct vm_x2apic x2apic;

	bzero(&x2apic, sizeof(x2apic));
	x2apic.state = state;

	error = vcpu_ioctl(vcpu, VM_SET_X2APIC_STATE, &x2apic);

	return (error);
}

int
vm_get_hpet_capabilities(struct vmctx *ctx, uint32_t *capabilities)
{
	int error;
	struct vm_hpet_cap cap;

	bzero(&cap, sizeof(struct vm_hpet_cap));
	error = ioctl(ctx->fd, VM_GET_HPET_CAPABILITIES, &cap);
	if (capabilities != NULL)
		*capabilities = cap.capabilities;
	return (error);
}

int
vm_rtc_write(struct vmctx *ctx, int offset, uint8_t value)
{
	struct vm_rtc_data rtcdata;
	int error;

	bzero(&rtcdata, sizeof(struct vm_rtc_data));
	rtcdata.offset = offset;
	rtcdata.value = value;
	error = ioctl(ctx->fd, VM_RTC_WRITE, &rtcdata);
	return (error);
}

int
vm_rtc_read(struct vmctx *ctx, int offset, uint8_t *retval)
{
	struct vm_rtc_data rtcdata;
	int error;

	bzero(&rtcdata, sizeof(struct vm_rtc_data));
	rtcdata.offset = offset;
	error = ioctl(ctx->fd, VM_RTC_READ, &rtcdata);
	if (error == 0)
		*retval = rtcdata.value;
	return (error);
}

int
vm_rtc_settime(struct vmctx *ctx, time_t secs)
{
	struct vm_rtc_time rtctime;
	int error;

	bzero(&rtctime, sizeof(struct vm_rtc_time));
	rtctime.secs = secs;
	error = ioctl(ctx->fd, VM_RTC_SETTIME, &rtctime);
	return (error);
}

int
vm_rtc_gettime(struct vmctx *ctx, time_t *secs)
{
	struct vm_rtc_time rtctime;
	int error;

	bzero(&rtctime, sizeof(struct vm_rtc_time));
	error = ioctl(ctx->fd, VM_RTC_GETTIME, &rtctime);
	if (error == 0)
		*secs = rtctime.secs;
	return (error);
}

/*
 * From Intel Vol 3a:
 * Table 9-1. IA-32 Processor States Following Power-up, Reset or INIT
 */
int
vcpu_reset(struct vcpu *vcpu)
{
	int error;
	uint64_t rflags, rip, cr0, cr4, zero, desc_base, rdx;
	uint32_t desc_access, desc_limit;
	uint16_t sel;

	zero = 0;

	rflags = 0x2;
	error = vm_set_register(vcpu, VM_REG_GUEST_RFLAGS, rflags);
	if (error)
		goto done;

	rip = 0xfff0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RIP, rip)) != 0)
		goto done;

	/*
	 * According to Intels Software Developer Manual CR0 should be
	 * initialized with CR0_ET | CR0_NW | CR0_CD but that crashes some
	 * guests like Windows.
	 */
	cr0 = CR0_NE;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR0, cr0)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR2, zero)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR3, zero)) != 0)
		goto done;

	cr4 = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CR4, cr4)) != 0)
		goto done;

	/*
	 * CS: present, r/w, accessed, 16-bit, byte granularity, usable
	 */
	desc_base = 0xffff0000;
	desc_limit = 0xffff;
	desc_access = 0x0093;
	error = vm_set_desc(vcpu, VM_REG_GUEST_CS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0xf000;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_CS, sel)) != 0)
		goto done;

	/*
	 * SS,DS,ES,FS,GS: present, r/w, accessed, 16-bit, byte granularity
	 */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x0093;
	error = vm_set_desc(vcpu, VM_REG_GUEST_SS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_DS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_ES,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_FS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_GS,
			    desc_base, desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_SS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_ES, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_FS, sel)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_GS, sel)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_EFER, zero)) != 0)
		goto done;

	/* General purpose registers */
	rdx = 0xf00;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RAX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RBX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RCX, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RDX, rdx)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RSI, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RDI, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RBP, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_RSP, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R8, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R9, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R10, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R11, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R12, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R13, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R14, zero)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_R15, zero)) != 0)
		goto done;

	/* GDTR, IDTR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0;
	error = vm_set_desc(vcpu, VM_REG_GUEST_GDTR,
			    desc_base, desc_limit, desc_access);
	if (error != 0)
		goto done;

	error = vm_set_desc(vcpu, VM_REG_GUEST_IDTR,
			    desc_base, desc_limit, desc_access);
	if (error != 0)
		goto done;

	/* TR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x0000008b;
	error = vm_set_desc(vcpu, VM_REG_GUEST_TR, 0, 0, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_TR, sel)) != 0)
		goto done;

	/* LDTR */
	desc_base = 0;
	desc_limit = 0xffff;
	desc_access = 0x00000082;
	error = vm_set_desc(vcpu, VM_REG_GUEST_LDTR, desc_base,
			    desc_limit, desc_access);
	if (error)
		goto done;

	sel = 0;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_LDTR, 0)) != 0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DR6,
		 0xffff0ff0)) != 0)
		goto done;
	if ((error = vm_set_register(vcpu, VM_REG_GUEST_DR7, 0x400)) !=
	    0)
		goto done;

	if ((error = vm_set_register(vcpu, VM_REG_GUEST_INTR_SHADOW,
		 zero)) != 0)
		goto done;

	error = 0;
done:
	return (error);
}
