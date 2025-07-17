/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Anish Gupta (akgupt3@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_bhyve_snapshot.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/vmm.h>
#include <machine/vmm_snapshot.h>

#include <dev/vmm/vmm_ktr.h>

#include "vlapic.h"
#include "vmcb.h"
#include "svm.h"
#include "svm_softc.h"

/*
 * The VMCB aka Virtual Machine Control Block is a 4KB aligned page
 * in memory that describes the virtual machine.
 *
 * The VMCB contains:
 * - instructions or events in the guest to intercept
 * - control bits that modify execution environment of the guest
 * - guest processor state (e.g. general purpose registers)
 */

/*
 * Return VMCB segment area.
 */
static struct vmcb_segment *
vmcb_segptr(struct vmcb *vmcb, int type)
{
	struct vmcb_state *state;
	struct vmcb_segment *seg;

	state = &vmcb->state;

	switch (type) {
	case VM_REG_GUEST_CS:
		seg = &state->cs;
		break;

	case VM_REG_GUEST_DS:
		seg = &state->ds;
		break;

	case VM_REG_GUEST_ES:
		seg = &state->es;
		break;

	case VM_REG_GUEST_FS:
		seg = &state->fs;
		break;

	case VM_REG_GUEST_GS:
		seg = &state->gs;
		break;

	case VM_REG_GUEST_SS:
		seg = &state->ss;
		break;

	case VM_REG_GUEST_GDTR:
		seg = &state->gdt;
		break;

	case VM_REG_GUEST_IDTR:
		seg = &state->idt;
		break;

	case VM_REG_GUEST_LDTR:
		seg = &state->ldt;
		break;

	case VM_REG_GUEST_TR:
		seg = &state->tr;
		break;

	default:
		seg = NULL;
		break;
	}

	return (seg);
}

static int
vmcb_access(struct svm_vcpu *vcpu, int write, int ident, uint64_t *val)
{
	struct vmcb *vmcb;
	int off, bytes;
	char *ptr;

	vmcb	= svm_get_vmcb(vcpu);
	off	= VMCB_ACCESS_OFFSET(ident);
	bytes	= VMCB_ACCESS_BYTES(ident);

	if ((off + bytes) >= sizeof (struct vmcb))
		return (EINVAL);

	ptr = (char *)vmcb;

	if (!write)
		*val = 0;

	switch (bytes) {
	case 8:
	case 4:
	case 2:
	case 1:
		if (write)
			memcpy(ptr + off, val, bytes);
		else
			memcpy(val, ptr + off, bytes);
		break;
	default:
		SVM_CTR1(vcpu, "Invalid size %d for VMCB access: %d", bytes);
		return (EINVAL);
	}

	/* Invalidate all VMCB state cached by h/w. */
	if (write)
		svm_set_dirty(vcpu, 0xffffffff);

	return (0);
}

/*
 * Read from segment selector, control and general purpose register of VMCB.
 */
int
vmcb_read(struct svm_vcpu *vcpu, int ident, uint64_t *retval)
{
	struct vmcb *vmcb;
	struct vmcb_state *state;
	struct vmcb_segment *seg;
	int err;

	vmcb = svm_get_vmcb(vcpu);
	state = &vmcb->state;
	err = 0;

	if (VMCB_ACCESS_OK(ident))
		return (vmcb_access(vcpu, 0, ident, retval));

	switch (ident) {
	case VM_REG_GUEST_CR0:
		*retval = state->cr0;
		break;

	case VM_REG_GUEST_CR2:
		*retval = state->cr2;
		break;

	case VM_REG_GUEST_CR3:
		*retval = state->cr3;
		break;

	case VM_REG_GUEST_CR4:
		*retval = state->cr4;
		break;

	case VM_REG_GUEST_DR6:
		*retval = state->dr6;
		break;

	case VM_REG_GUEST_DR7:
		*retval = state->dr7;
		break;

	case VM_REG_GUEST_EFER:
		*retval = state->efer;
		break;

	case VM_REG_GUEST_RAX:
		*retval = state->rax;
		break;

	case VM_REG_GUEST_RFLAGS:
		*retval = state->rflags;
		break;

	case VM_REG_GUEST_RIP:
		*retval = state->rip;
		break;

	case VM_REG_GUEST_RSP:
		*retval = state->rsp;
		break;

	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_LDTR:
	case VM_REG_GUEST_TR:
		seg = vmcb_segptr(vmcb, ident);
		KASSERT(seg != NULL, ("%s: unable to get segment %d from VMCB",
		    __func__, ident));
		*retval = seg->selector;
		break;

	case VM_REG_GUEST_FS_BASE:
	case VM_REG_GUEST_GS_BASE:
		seg = vmcb_segptr(vmcb, ident == VM_REG_GUEST_FS_BASE ?
		    VM_REG_GUEST_FS : VM_REG_GUEST_GS);
		KASSERT(seg != NULL, ("%s: unable to get segment %d from VMCB",
		    __func__, ident));
		*retval = seg->base;
		break;
	case VM_REG_GUEST_KGS_BASE:
		*retval = state->kernelgsbase;
		break;

	case VM_REG_GUEST_TPR:
		*retval = vlapic_get_cr8(vm_lapic(vcpu->vcpu));
		break;

	case VM_REG_GUEST_GDTR:
	case VM_REG_GUEST_IDTR:
		/* GDTR and IDTR don't have segment selectors */
		err = EINVAL;
		break;
	default:
		err =  EINVAL;
		break;
	}

	return (err);
}

/*
 * Write to segment selector, control and general purpose register of VMCB.
 */
int
vmcb_write(struct svm_vcpu *vcpu, int ident, uint64_t val)
{
	struct vmcb *vmcb;
	struct vmcb_state *state;
	struct vmcb_segment *seg;
	int err, dirtyseg;

	vmcb = svm_get_vmcb(vcpu);
	state = &vmcb->state;
	dirtyseg = 0;
	err = 0;

	if (VMCB_ACCESS_OK(ident))
		return (vmcb_access(vcpu, 1, ident, &val));

	switch (ident) {
	case VM_REG_GUEST_CR0:
		state->cr0 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_CR);
		break;

	case VM_REG_GUEST_CR2:
		state->cr2 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_CR2);
		break;

	case VM_REG_GUEST_CR3:
		state->cr3 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_CR);
		break;

	case VM_REG_GUEST_CR4:
		state->cr4 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_CR);
		break;

	case VM_REG_GUEST_DR6:
		state->dr6 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_DR);
		break;

	case VM_REG_GUEST_DR7:
		state->dr7 = val;
		svm_set_dirty(vcpu, VMCB_CACHE_DR);
		break;

	case VM_REG_GUEST_EFER:
		/* EFER_SVM must always be set when the guest is executing */
		state->efer = val | EFER_SVM;
		svm_set_dirty(vcpu, VMCB_CACHE_CR);
		break;

	case VM_REG_GUEST_RAX:
		state->rax = val;
		break;

	case VM_REG_GUEST_RFLAGS:
		state->rflags = val;
		break;

	case VM_REG_GUEST_RIP:
		state->rip = val;
		break;

	case VM_REG_GUEST_RSP:
		state->rsp = val;
		break;

	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_SS:
		dirtyseg = 1;		/* FALLTHROUGH */
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_LDTR:
	case VM_REG_GUEST_TR:
		seg = vmcb_segptr(vmcb, ident);
		KASSERT(seg != NULL, ("%s: unable to get segment %d from VMCB",
		    __func__, ident));
		seg->selector = val;
		if (dirtyseg)
			svm_set_dirty(vcpu, VMCB_CACHE_SEG);
		break;

	case VM_REG_GUEST_GDTR:
	case VM_REG_GUEST_IDTR:
		/* GDTR and IDTR don't have segment selectors */
		err = EINVAL;
		break;
	default:
		err = EINVAL;
		break;
	}

	return (err);
}

int
vmcb_seg(struct vmcb *vmcb, int ident, struct vmcb_segment *seg2)
{
	struct vmcb_segment *seg;

	seg = vmcb_segptr(vmcb, ident);
	if (seg != NULL) {
		bcopy(seg, seg2, sizeof(struct vmcb_segment));
		return (0);
	} else {
		return (EINVAL);
	}
}

int
vmcb_setdesc(struct svm_vcpu *vcpu, int reg, struct seg_desc *desc)
{
	struct vmcb *vmcb;
	struct vmcb_segment *seg;
	uint16_t attrib;

	vmcb = svm_get_vmcb(vcpu);

	seg = vmcb_segptr(vmcb, reg);
	KASSERT(seg != NULL, ("%s: invalid segment descriptor %d",
	    __func__, reg));

	seg->base = desc->base;
	seg->limit = desc->limit;
	if (reg != VM_REG_GUEST_GDTR && reg != VM_REG_GUEST_IDTR) {
		/*
		 * Map seg_desc access to VMCB attribute format.
		 *
		 * SVM uses the 'P' bit in the segment attributes to indicate a
		 * NULL segment so clear it if the segment is marked unusable.
		 */
		attrib = ((desc->access & 0xF000) >> 4) | (desc->access & 0xFF);
		if (SEG_DESC_UNUSABLE(desc->access)) {
			attrib &= ~0x80;
		}
		seg->attrib = attrib;
	}

	SVM_CTR4(vcpu, "Setting desc %d: base (%#lx), limit (%#x), "
	    "attrib (%#x)", reg, seg->base, seg->limit, seg->attrib);

	switch (reg) {
	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_SS:
		svm_set_dirty(vcpu, VMCB_CACHE_SEG);
		break;
	case VM_REG_GUEST_GDTR:
	case VM_REG_GUEST_IDTR:
		svm_set_dirty(vcpu, VMCB_CACHE_DT);
		break;
	default:
		break;
	}

	return (0);
}

int
vmcb_getdesc(struct svm_vcpu *vcpu, int reg, struct seg_desc *desc)
{
	struct vmcb *vmcb;
	struct vmcb_segment *seg;

	vmcb = svm_get_vmcb(vcpu);
	seg = vmcb_segptr(vmcb, reg);
	KASSERT(seg != NULL, ("%s: invalid segment descriptor %d",
	    __func__, reg));

	desc->base = seg->base;
	desc->limit = seg->limit;
	desc->access = 0;

	if (reg != VM_REG_GUEST_GDTR && reg != VM_REG_GUEST_IDTR) {
		/* Map seg_desc access to VMCB attribute format */
		desc->access = ((seg->attrib & 0xF00) << 4) |
		    (seg->attrib & 0xFF);

		/*
		 * VT-x uses bit 16 to indicate a segment that has been loaded
		 * with a NULL selector (aka unusable). The 'desc->access'
		 * field is interpreted in the VT-x format by the
		 * processor-independent code.
		 *
		 * SVM uses the 'P' bit to convey the same information so
		 * convert it into the VT-x format. For more details refer to
		 * section "Segment State in the VMCB" in APMv2.
		 */
		if (reg != VM_REG_GUEST_CS && reg != VM_REG_GUEST_TR) {
			if ((desc->access & 0x80) == 0)
				desc->access |= 0x10000;  /* Unusable segment */
		}
	}

	return (0);
}

#ifdef BHYVE_SNAPSHOT
int
vmcb_getany(struct svm_vcpu *vcpu, int ident, uint64_t *val)
{
	int error = 0;

	if (ident >= VM_REG_LAST) {
		error = EINVAL;
		goto err;
	}

	error = vmcb_read(vcpu, ident, val);

err:
	return (error);
}

int
vmcb_setany(struct svm_vcpu *vcpu, int ident, uint64_t val)
{
	int error = 0;

	if (ident >= VM_REG_LAST) {
		error = EINVAL;
		goto err;
	}

	error = vmcb_write(vcpu, ident, val);

err:
	return (error);
}

int
vmcb_snapshot_desc(struct svm_vcpu *vcpu, int reg,
    struct vm_snapshot_meta *meta)
{
	int ret;
	struct seg_desc desc;

	if (meta->op == VM_SNAPSHOT_SAVE) {
		ret = vmcb_getdesc(vcpu, reg, &desc);
		if (ret != 0)
			goto done;

		SNAPSHOT_VAR_OR_LEAVE(desc.base, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(desc.limit, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(desc.access, meta, ret, done);
	} else if (meta->op == VM_SNAPSHOT_RESTORE) {
		SNAPSHOT_VAR_OR_LEAVE(desc.base, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(desc.limit, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(desc.access, meta, ret, done);

		ret = vmcb_setdesc(vcpu, reg, &desc);
		if (ret != 0)
			goto done;
	} else {
		ret = EINVAL;
		goto done;
	}

done:
	return (ret);
}

int
vmcb_snapshot_any(struct svm_vcpu *vcpu, int ident,
    struct vm_snapshot_meta *meta)
{
	int ret;
	uint64_t val;

	if (meta->op == VM_SNAPSHOT_SAVE) {
		ret = vmcb_getany(vcpu, ident, &val);
		if (ret != 0)
			goto done;

		SNAPSHOT_VAR_OR_LEAVE(val, meta, ret, done);
	} else if (meta->op == VM_SNAPSHOT_RESTORE) {
		SNAPSHOT_VAR_OR_LEAVE(val, meta, ret, done);

		ret = vmcb_setany(vcpu, ident, val);
		if (ret != 0)
			goto done;
	} else {
		ret = EINVAL;
		goto done;
	}

done:
	return (ret);
}
#endif
