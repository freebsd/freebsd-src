/*-
 * Copyright (c) 2012 Sandvine, Inc.
 * Copyright (c) 2012 NetApp, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>

#include "vmm_instruction_emul.h"

#define	GB	(1024 * 1024 * 1024)

static enum vm_reg_name gpr_map[16] = {
	VM_REG_GUEST_RAX,
	VM_REG_GUEST_RCX,
	VM_REG_GUEST_RDX,
	VM_REG_GUEST_RBX,
	VM_REG_GUEST_RSP,
	VM_REG_GUEST_RBP,
	VM_REG_GUEST_RSI,
	VM_REG_GUEST_RDI,
	VM_REG_GUEST_R8,
	VM_REG_GUEST_R9,
	VM_REG_GUEST_R10,
	VM_REG_GUEST_R11,
	VM_REG_GUEST_R12,
	VM_REG_GUEST_R13,
	VM_REG_GUEST_R14,
	VM_REG_GUEST_R15
};

static void
vie_init(struct vie *vie)
{

	bzero(vie, sizeof(struct vie));

	vie->op_size = VIE_OP_SIZE_32BIT;

	vie->base_register = VM_REG_LAST;
	vie->index_register = VM_REG_LAST;
	vie->operand_register = VM_REG_LAST;
}

static int
gla2gpa(struct vm *vm, uint64_t gla, uint64_t ptpphys,
	uint64_t *gpa, uint64_t *gpaend)
{
	vm_paddr_t hpa;
	int nlevels, ptpshift, ptpindex;
	uint64_t *ptpbase, pte, pgsize;

	/*
	 * XXX assumes 64-bit guest with 4 page walk levels
	 */
	nlevels = 4;
	while (--nlevels >= 0) {
		/* Zero out the lower 12 bits and the upper 12 bits */
		ptpphys >>= 12; ptpphys <<= 24; ptpphys >>= 12;

		hpa = vm_gpa2hpa(vm, ptpphys, PAGE_SIZE);
		if (hpa == -1)
			goto error;

		ptpbase = (uint64_t *)PHYS_TO_DMAP(hpa);

		ptpshift = PAGE_SHIFT + nlevels * 9;
		ptpindex = (gla >> ptpshift) & 0x1FF;
		pgsize = 1UL << ptpshift;

		pte = ptpbase[ptpindex];

		if ((pte & PG_V) == 0)
			goto error;

		if (pte & PG_PS) {
			if (pgsize > 1 * GB)
				goto error;
			else
				break;
		}

		ptpphys = pte;
	}

	/* Zero out the lower 'ptpshift' bits and the upper 12 bits */
	pte >>= ptpshift; pte <<= (ptpshift + 12); pte >>= 12;
	*gpa = pte | (gla & (pgsize - 1));
	*gpaend = pte + pgsize;
	return (0);

error:
	return (-1);
}

int
vmm_fetch_instruction(struct vm *vm, uint64_t rip, int inst_length,
		      uint64_t cr3, struct vie *vie)
{
	int n, err;
	uint64_t hpa, gpa, gpaend, off;

	/*
	 * XXX cache previously fetched instructions using 'rip' as the tag
	 */

	if (inst_length > VIE_INST_SIZE)
		panic("vmm_fetch_instruction: invalid length %d", inst_length);

	vie_init(vie);

	/* Copy the instruction into 'vie' */
	while (vie->num_valid < inst_length) {
		err = gla2gpa(vm, rip, cr3, &gpa, &gpaend);
		if (err)
			break;

		off = gpa & PAGE_MASK;
		n = min(inst_length - vie->num_valid, PAGE_SIZE - off);

		hpa = vm_gpa2hpa(vm, gpa, n);
		if (hpa == -1)
			break;

		bcopy((void *)PHYS_TO_DMAP(hpa), &vie->inst[vie->num_valid], n);

		rip += n;
		vie->num_valid += n;
	}

	if (vie->num_valid == inst_length)
		return (0);
	else
		return (-1);
}

static int
vie_peek(struct vie *vie, uint8_t *x)
{
	if (vie->num_processed < vie->num_valid) {
		*x = vie->inst[vie->num_processed];
		return (0);
	} else
		return (-1);
}

static void
vie_advance(struct vie *vie)
{
	if (vie->num_processed >= vie->num_valid)
		panic("vie_advance: %d/%d", vie->num_processed, vie->num_valid);

	vie->num_processed++;
}

static int
decode_rex(struct vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x))
		return (-1);

	if (x >= 0x40 && x <= 0x4F) {
		vie->rex_w = x & 0x8 ? 1 : 0;
		vie->rex_r = x & 0x4 ? 1 : 0;
		vie->rex_x = x & 0x2 ? 1 : 0;
		vie->rex_b = x & 0x1 ? 1 : 0;

		vie_advance(vie);
	}

	return (0);
}

static int
decode_opcode(struct vie *vie)
{
	uint8_t x;

	static const uint8_t flags[256] = {
		[0x89] = VIE_F_HAS_MODRM | VIE_F_FROM_REG | VIE_F_TO_RM,
		[0x8B] = VIE_F_HAS_MODRM | VIE_F_FROM_RM | VIE_F_TO_REG,
		[0xC7] = VIE_F_HAS_MODRM | VIE_F_FROM_IMM | VIE_F_TO_RM,
	};

	if (vie_peek(vie, &x))
		return (-1);

	vie->opcode_byte = x;
	vie->opcode_flags = flags[x];

	vie_advance(vie);

	if (vie->opcode_flags == 0)
		return (-1);
	else
		return (0);
}

/*
 * XXX assuming 32-bit or 64-bit guest
 */
static int
decode_modrm(struct vie *vie)
{
	uint8_t x;

	if ((vie->opcode_flags & VIE_F_HAS_MODRM) == 0)
		return (0);

	if (vie_peek(vie, &x))
		return (-1);

	vie->mod = (x >> 6) & 0x3;
	vie->rm =  (x >> 0) & 0x7;
	vie->reg = (x >> 3) & 0x7;

	if ((vie->mod == VIE_MOD_INDIRECT && vie->rm == VIE_RM_DISP32) ||
	    (vie->mod != VIE_MOD_DIRECT && vie->rm == VIE_RM_SIB)) {
			/*
			 * Table 2-5: Special Cases of REX Encodings
			 *
			 * mod=0, r/m=5 is used in the compatibility mode to
			 * indicate a disp32 without a base register.
			 *
			 * mod!=3, r/m=4 is used in the compatibility mode to
			 * indicate that the SIB byte is present.
			 *
			 * The 'b' bit in the REX prefix is don't care in
			 * this case.
			 */
	} else {
		vie->rm |= (vie->rex_b << 3);
	}

	vie->reg |= (vie->rex_r << 3);

	/* SIB addressing not supported yet */
	if (vie->mod != VIE_MOD_DIRECT && vie->rm == VIE_RM_SIB)
		return (-1);

	vie->base_register = gpr_map[vie->rm];

	if (vie->opcode_flags & (VIE_F_FROM_REG | VIE_F_TO_REG))
		vie->operand_register = gpr_map[vie->reg];

	switch (vie->mod) {
	case VIE_MOD_INDIRECT_DISP8:
		vie->disp_bytes = 1;
		break;
	case VIE_MOD_INDIRECT_DISP32:
		vie->disp_bytes = 4;
		break;
	case VIE_MOD_INDIRECT:
		if (vie->rm == VIE_RM_DISP32) {
			vie->disp_bytes = 4;
			vie->base_register = VM_REG_LAST;	/* no base */
		}
		break;
	}

	/* calculate the operand size */
	if (vie->rex_w)
		vie->op_size = VIE_OP_SIZE_64BIT;

	if (vie->opcode_flags & VIE_F_FROM_IMM)
		vie->imm_bytes = 4;

	vie_advance(vie);

	return (0);
}

static int
decode_displacement(struct vie *vie)
{
	int n, i;
	uint8_t x;

	union {
		char	buf[4];
		int8_t	signed8;
		int32_t	signed32;
	} u;

	if ((n = vie->disp_bytes) == 0)
		return (0);

	if (n != 1 && n != 4)
		panic("decode_displacement: invalid disp_bytes %d", n);

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x))
			return (-1);

		u.buf[i] = x;
		vie_advance(vie);
	}

	if (n == 1)
		vie->displacement = u.signed8;		/* sign-extended */
	else
		vie->displacement = u.signed32;		/* sign-extended */

	return (0);
}

static int
decode_immediate(struct vie *vie)
{
	int i, n;
	uint8_t x;
	union {
		char	buf[4];
		int32_t	signed32;
	} u;

	if ((n = vie->imm_bytes) == 0)
		return (0);

	if (n != 4)
		panic("decode_immediate: invalid imm_bytes %d", n);

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x))
			return (-1);

		u.buf[i] = x;
		vie_advance(vie);
	}
	
	vie->immediate = u.signed32;		/* sign-extended */

	return (0);
}

int
vmm_decode_instruction(struct vie *vie)
{
	if (decode_rex(vie))
		return (-1);

	if (decode_opcode(vie))
		return (-1);

	if (decode_modrm(vie))
		return (-1);

	if (decode_displacement(vie))
		return (-1);
	
	if (decode_immediate(vie))
		return (-1);

	return (0);
}
