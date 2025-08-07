/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>

#include <dev/vmm/vmm_ktr.h>

#include "vatpic.h"
#include "vatpit.h"
#include "vpmtmr.h"
#include "vrtc.h"
#include "vmm_ioport.h"

#define	MAX_IOPORTS		1280

ioport_handler_func_t ioport_handler[MAX_IOPORTS] = {
	[TIMER_MODE] = vatpit_handler,
	[TIMER_CNTR0] = vatpit_handler,
	[TIMER_CNTR1] = vatpit_handler,
	[TIMER_CNTR2] = vatpit_handler,
	[NMISC_PORT] = vatpit_nmisc_handler,
	[IO_ICU1] = vatpic_master_handler,
	[IO_ICU1 + ICU_IMR_OFFSET] = vatpic_master_handler,
	[IO_ICU2] = vatpic_slave_handler,
	[IO_ICU2 + ICU_IMR_OFFSET] = vatpic_slave_handler,
	[IO_ELCR1] = vatpic_elc_handler,
	[IO_ELCR2] = vatpic_elc_handler,
	[IO_PMTMR] = vpmtmr_handler,
	[IO_RTC] = vrtc_addr_handler,
	[IO_RTC + 1] = vrtc_data_handler,
};

#ifdef KTR
static const char *
inout_instruction(struct vm_exit *vmexit)
{
	int index;

	static const char *iodesc[] = {
		"outb", "outw", "outl",
		"inb", "inw", "inl",
		"outsb", "outsw", "outsd",
		"insb", "insw", "insd",
	};

	switch (vmexit->u.inout.bytes) {
	case 1:
		index = 0;
		break;
	case 2:
		index = 1;
		break;
	default:
		index = 2;
		break;
	}

	if (vmexit->u.inout.in)
		index += 3;

	if (vmexit->u.inout.string)
		index += 6;

	KASSERT(index < nitems(iodesc), ("%s: invalid index %d",
	    __func__, index));

	return (iodesc[index]);
}
#endif	/* KTR */

static int
emulate_inout_port(struct vcpu *vcpu, struct vm_exit *vmexit, bool *retu)
{
	ioport_handler_func_t handler;
	uint32_t mask, val = 0;
	int error;

	/*
	 * If there is no handler for the I/O port then punt to userspace.
	 */
	if (vmexit->u.inout.port >= MAX_IOPORTS ||
	    (handler = ioport_handler[vmexit->u.inout.port]) == NULL) {
		*retu = true;
		return (0);
	}

	mask = vie_size2mask(vmexit->u.inout.bytes);

	if (!vmexit->u.inout.in) {
		val = vmexit->u.inout.eax & mask;
	}

	error = (*handler)(vcpu_vm(vcpu), vmexit->u.inout.in,
	    vmexit->u.inout.port, vmexit->u.inout.bytes, &val);
	if (error) {
		/*
		 * The value returned by this function is also the return value
		 * of vm_run(). This needs to be a positive number otherwise it
		 * can be interpreted as a "pseudo-error" like ERESTART.
		 *
		 * Enforce this by mapping all errors to EIO.
		 */
		return (EIO);
	}

	if (vmexit->u.inout.in) {
		vmexit->u.inout.eax &= ~mask;
		vmexit->u.inout.eax |= val & mask;
		error = vm_set_register(vcpu, VM_REG_GUEST_RAX,
		    vmexit->u.inout.eax);
		KASSERT(error == 0, ("emulate_ioport: error %d setting guest "
		    "rax register", error));
	}
	*retu = false;
	return (0);
}

static int
decode_segment(struct vcpu *vcpu, enum vm_reg_name *segment)
{
	struct vm_guest_paging *paging;
	struct vie vie;
	struct vm_exit *vme;
	int err;
	int fault;

	vme = vm_exitinfo(vcpu);
	paging = &vme->u.inout_str.paging;

	vie_init(&vie, NULL, 0);
	err = vmm_fetch_instruction(vcpu, paging,
	    vme->rip + vme->u.inout_str.cs_base, VIE_INST_SIZE, &vie, &fault);
	if (err || fault)
		return (err);

	err = vmm_decode_instruction(vcpu, VIE_INVALID_GLA, paging->cpu_mode,
	    vme->u.inout_str.cs_d, &vie);

	if (err || vie.op.op_type != VIE_OP_TYPE_OUTS)
		return (EINVAL);
	if (vie.segment_override)
		*segment = vie.segment_register;
	else
		*segment = VM_REG_GUEST_DS;

	return (0);
}

static int
emulate_inout_str(struct vcpu *vcpu, struct vm_exit *vmexit, bool *retu)
{
	int err;

	*retu = true;
	if (vmexit->u.inout_str.seg_name == VM_REG_LAST) {
		err = decode_segment(vcpu, &vmexit->u.inout_str.seg_name);
		if (err)
			return (err);
		return (vm_get_seg_desc(vcpu, vmexit->u.inout_str.seg_name,
		    &vmexit->u.inout_str.seg_desc));
	}
	return (0);	/* Return to userspace to finish emulation */
}

int
vm_handle_inout(struct vcpu *vcpu, struct vm_exit *vmexit, bool *retu)
{
	int bytes __diagused, error;

	bytes = vmexit->u.inout.bytes;
	KASSERT(bytes == 1 || bytes == 2 || bytes == 4,
	    ("vm_handle_inout: invalid operand size %d", bytes));

	if (vmexit->u.inout.string)
		error = emulate_inout_str(vcpu, vmexit, retu);
	else
		error = emulate_inout_port(vcpu, vmexit, retu);

	VCPU_CTR4(vcpu_vm(vcpu), vcpu_vcpuid(vcpu), "%s%s 0x%04x: %s",
	    vmexit->u.inout.rep ? "rep " : "",
	    inout_instruction(vmexit),
	    vmexit->u.inout.port,
	    error ? "error" : (*retu ? "userspace" : "handled"));

	return (error);
}
