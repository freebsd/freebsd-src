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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>
#else	/* !_KERNEL */
#include <sys/types.h>
#include <sys/errno.h>

#include <machine/vmm.h>

#include <vmmapi.h>
#endif	/* _KERNEL */

enum cpu_mode {
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

/* struct vie_op.op_type */
enum {
	VIE_OP_TYPE_NONE = 0,
	VIE_OP_TYPE_MOV,
	VIE_OP_TYPE_AND,
	VIE_OP_TYPE_OR,
	VIE_OP_TYPE_LAST
};

/* struct vie_op.op_flags */
#define	VIE_OP_F_IMM		(1 << 0)	/* immediate operand present */
#define	VIE_OP_F_IMM8		(1 << 1)	/* 8-bit immediate operand */

static const struct vie_op one_byte_opcodes[256] = {
	[0x88] = {
		.op_byte = 0x88,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x89] = {
		.op_byte = 0x89,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8B] = {
		.op_byte = 0x8B,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0xC7] = {
		.op_byte = 0xC7,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x23] = {
		.op_byte = 0x23,
		.op_type = VIE_OP_TYPE_AND,
	},
	[0x81] = {
		/* XXX Group 1 extended opcode - not just AND */
		.op_byte = 0x81,
		.op_type = VIE_OP_TYPE_AND,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x83] = {
		/* XXX Group 1 extended opcode - not just OR */
		.op_byte = 0x83,
		.op_type = VIE_OP_TYPE_OR,
		.op_flags = VIE_OP_F_IMM8,
	},
};

/* struct vie.mod */
#define	VIE_MOD_INDIRECT		0
#define	VIE_MOD_INDIRECT_DISP8		1
#define	VIE_MOD_INDIRECT_DISP32		2
#define	VIE_MOD_DIRECT			3

/* struct vie.rm */
#define	VIE_RM_SIB			4
#define	VIE_RM_DISP32			5

#define	GB				(1024 * 1024 * 1024)

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

static uint64_t size2mask[] = {
	[1] = 0xff,
	[2] = 0xffff,
	[4] = 0xffffffff,
	[8] = 0xffffffffffffffff,
};

static int
vie_read_register(void *vm, int vcpuid, enum vm_reg_name reg, uint64_t *rval)
{
	int error;

	error = vm_get_register(vm, vcpuid, reg, rval);

	return (error);
}

static int
vie_read_bytereg(void *vm, int vcpuid, struct vie *vie, uint8_t *rval)
{
	uint64_t val;
	int error, rshift;
	enum vm_reg_name reg;

	rshift = 0;
	reg = gpr_map[vie->reg];

	/*
	 * 64-bit mode imposes limitations on accessing legacy byte registers.
	 *
	 * The legacy high-byte registers cannot be addressed if the REX
	 * prefix is present. In this case the values 4, 5, 6 and 7 of the
	 * 'ModRM:reg' field address %spl, %bpl, %sil and %dil respectively.
	 *
	 * If the REX prefix is not present then the values 4, 5, 6 and 7
	 * of the 'ModRM:reg' field address the legacy high-byte registers,
	 * %ah, %ch, %dh and %bh respectively.
	 */
	if (!vie->rex_present) {
		if (vie->reg & 0x4) {
			/*
			 * Obtain the value of %ah by reading %rax and shifting
			 * right by 8 bits (same for %bh, %ch and %dh).
			 */
			rshift = 8;
			reg = gpr_map[vie->reg & 0x3];
		}
	}

	error = vm_get_register(vm, vcpuid, reg, &val);
	*rval = val >> rshift;
	return (error);
}

static int
vie_update_register(void *vm, int vcpuid, enum vm_reg_name reg,
		    uint64_t val, int size)
{
	int error;
	uint64_t origval;

	switch (size) {
	case 1:
	case 2:
		error = vie_read_register(vm, vcpuid, reg, &origval);
		if (error)
			return (error);
		val &= size2mask[size];
		val |= origval & ~size2mask[size];
		break;
	case 4:
		val &= 0xffffffffUL;
		break;
	case 8:
		break;
	default:
		return (EINVAL);
	}

	error = vm_set_register(vm, vcpuid, reg, val);
	return (error);
}

/*
 * The following simplifying assumptions are made during emulation:
 *
 * - guest is in 64-bit mode
 *   - default address size is 64-bits
 *   - default operand size is 32-bits
 *
 * - operand size override is not supported
 *
 * - address size override is not supported
 */
static int
emulate_mov(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	    mem_region_read_t memread, mem_region_write_t memwrite, void *arg)
{
	int error, size;
	enum vm_reg_name reg;
	uint8_t byte;
	uint64_t val;

	size = 4;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x88:
		/*
		 * MOV byte from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 88/r:	mov r/m8, r8
		 * REX + 88/r:	mov r/m8, r8 (%ah, %ch, %dh, %bh not available)
		 */
		size = 1;
		error = vie_read_bytereg(vm, vcpuid, vie, &byte);
		if (error == 0)
			error = memwrite(vm, vcpuid, gpa, byte, size, arg);
		break;
	case 0x89:
		/*
		 * MOV from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 89/r:	mov r/m32, r32
		 * REX.W + 89/r	mov r/m64, r64
		 */
		if (vie->rex_w)
			size = 8;
		reg = gpr_map[vie->reg];
		error = vie_read_register(vm, vcpuid, reg, &val);
		if (error == 0) {
			val &= size2mask[size];
			error = memwrite(vm, vcpuid, gpa, val, size, arg);
		}
		break;
	case 0x8B:
		/*
		 * MOV from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8B/r:	mov r32, r/m32
		 * REX.W 8B/r:	mov r64, r/m64
		 */
		if (vie->rex_w)
			size = 8;
		error = memread(vm, vcpuid, gpa, &val, size, arg);
		if (error == 0) {
			reg = gpr_map[vie->reg];
			error = vie_update_register(vm, vcpuid, reg, val, size);
		}
		break;
	case 0xC7:
		/*
		 * MOV from imm32 to mem (ModRM:r/m)
		 * C7/0		mov r/m32, imm32
		 * REX.W + C7/0	mov r/m64, imm32 (sign-extended to 64-bits)
		 */
		val = vie->immediate;		/* already sign-extended */

		if (vie->rex_w)
			size = 8;

		if (size != 8)
			val &= size2mask[size];

		error = memwrite(vm, vcpuid, gpa, val, size, arg);
		break;
	default:
		break;
	}

	return (error);
}

static int
emulate_and(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	    mem_region_read_t memread, mem_region_write_t memwrite, void *arg)
{
	int error, size;
	enum vm_reg_name reg;
	uint64_t val1, val2;

	size = 4;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x23:
		/*
		 * AND reg (ModRM:reg) and mem (ModRM:r/m) and store the
		 * result in reg.
		 *
		 * 23/r		and r32, r/m32
		 * REX.W + 23/r	and r64, r/m64
		 */
		if (vie->rex_w)
			size = 8;

		/* get the first operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vm, vcpuid, reg, &val1);
		if (error)
			break;

		/* get the second operand */
		error = memread(vm, vcpuid, gpa, &val2, size, arg);
		if (error)
			break;

		/* perform the operation and write the result */
		val1 &= val2;
		error = vie_update_register(vm, vcpuid, reg, val1, size);
		break;
	case 0x81:
		/*
		 * AND mem (ModRM:r/m) with immediate and store the
		 * result in mem.
		 *
		 * 81/          and r/m32, imm32
		 * REX.W + 81/  and r/m64, imm32 sign-extended to 64
		 *
		 * Currently, only the AND operation of the 0x81 opcode
		 * is implemented (ModRM:reg = b100).
		 */
		if ((vie->reg & 7) != 4)
			break;

		if (vie->rex_w)
			size = 8;
		
		/* get the first operand */
                error = memread(vm, vcpuid, gpa, &val1, size, arg);
                if (error)
			break;

                /*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
                val1 &= vie->immediate;
                error = memwrite(vm, vcpuid, gpa, val1, size, arg);
		break;
	default:
		break;
	}
	return (error);
}

static int
emulate_or(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	    mem_region_read_t memread, mem_region_write_t memwrite, void *arg)
{
	int error, size;
	uint64_t val1;

	size = 4;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x83:
		/*
		 * OR mem (ModRM:r/m) with immediate and store the
		 * result in mem.
		 *
		 * 83/          OR r/m32, imm8 sign-extended to 32
		 * REX.W + 83/  OR r/m64, imm8 sign-extended to 64
		 *
		 * Currently, only the OR operation of the 0x83 opcode
		 * is implemented (ModRM:reg = b001).
		 */
		if ((vie->reg & 7) != 1)
			break;

		if (vie->rex_w)
			size = 8;
		
		/* get the first operand */
                error = memread(vm, vcpuid, gpa, &val1, size, arg);
                if (error)
			break;

                /*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
                val1 |= vie->immediate;
                error = memwrite(vm, vcpuid, gpa, val1, size, arg);
		break;
	default:
		break;
	}
	return (error);
}

int
vmm_emulate_instruction(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
			mem_region_read_t memread, mem_region_write_t memwrite,
			void *memarg)
{
	int error;

	if (!vie->decoded)
		return (EINVAL);

	switch (vie->op.op_type) {
	case VIE_OP_TYPE_MOV:
		error = emulate_mov(vm, vcpuid, gpa, vie,
				    memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_AND:
		error = emulate_and(vm, vcpuid, gpa, vie,
				    memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_OR:
		error = emulate_or(vm, vcpuid, gpa, vie,
				    memread, memwrite, memarg);
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

#ifdef _KERNEL
static void
vie_init(struct vie *vie)
{

	bzero(vie, sizeof(struct vie));

	vie->base_register = VM_REG_LAST;
	vie->index_register = VM_REG_LAST;
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
vmm_fetch_instruction(struct vm *vm, int cpuid, uint64_t rip, int inst_length,
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

	vie->num_processed++;
}

static int
decode_rex(struct vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x))
		return (-1);

	if (x >= 0x40 && x <= 0x4F) {
		vie->rex_present = 1;

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

	if (vie_peek(vie, &x))
		return (-1);

	vie->op = one_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE)
		return (-1);

	vie_advance(vie);
	return (0);
}

static int
decode_modrm(struct vie *vie)
{
	uint8_t x;
	enum cpu_mode cpu_mode;

	/*
	 * XXX assuming that guest is in IA-32E 64-bit mode
	 */
	cpu_mode = CPU_MODE_64BIT;

	if (vie_peek(vie, &x))
		return (-1);

	vie->mod = (x >> 6) & 0x3;
	vie->rm =  (x >> 0) & 0x7;
	vie->reg = (x >> 3) & 0x7;

	/*
	 * A direct addressing mode makes no sense in the context of an EPT
	 * fault. There has to be a memory access involved to cause the
	 * EPT fault.
	 */
	if (vie->mod == VIE_MOD_DIRECT)
		return (-1);

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

	/* SIB */
	if (vie->mod != VIE_MOD_DIRECT && vie->rm == VIE_RM_SIB)
		goto done;

	vie->base_register = gpr_map[vie->rm];

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
			/*
			 * Table 2-7. RIP-Relative Addressing
			 *
			 * In 64-bit mode mod=00 r/m=101 implies [rip] + disp32
			 * whereas in compatibility mode it just implies disp32.
			 */

			if (cpu_mode == CPU_MODE_64BIT)
				vie->base_register = VM_REG_GUEST_RIP;
			else
				vie->base_register = VM_REG_LAST;
				
		}
		break;
	}

	/* Figure out immediate operand size (if any) */
	if (vie->op.op_flags & VIE_OP_F_IMM)
		vie->imm_bytes = 4;
	else if (vie->op.op_flags & VIE_OP_F_IMM8)
		vie->imm_bytes = 1;

done:
	vie_advance(vie);

	return (0);
}

static int
decode_sib(struct vie *vie)
{
	uint8_t x;

	/* Proceed only if SIB byte is present */
	if (vie->mod == VIE_MOD_DIRECT || vie->rm != VIE_RM_SIB)
		return (0);

	if (vie_peek(vie, &x))
		return (-1);

	/* De-construct the SIB byte */
	vie->ss = (x >> 6) & 0x3;
	vie->index = (x >> 3) & 0x7;
	vie->base = (x >> 0) & 0x7;

	/* Apply the REX prefix modifiers */
	vie->index |= vie->rex_x << 3;
	vie->base |= vie->rex_b << 3;

	switch (vie->mod) {
	case VIE_MOD_INDIRECT_DISP8:
		vie->disp_bytes = 1;
		break;
	case VIE_MOD_INDIRECT_DISP32:
		vie->disp_bytes = 4;
		break;
	}

	if (vie->mod == VIE_MOD_INDIRECT &&
	    (vie->base == 5 || vie->base == 13)) {
		/*
		 * Special case when base register is unused if mod = 0
		 * and base = %rbp or %r13.
		 *
		 * Documented in:
		 * Table 2-3: 32-bit Addressing Forms with the SIB Byte
		 * Table 2-5: Special Cases of REX Encodings
		 */
		vie->disp_bytes = 4;
	} else {
		vie->base_register = gpr_map[vie->base];
	}

	/*
	 * All encodings of 'index' are valid except for %rsp (4).
	 *
	 * Documented in:
	 * Table 2-3: 32-bit Addressing Forms with the SIB Byte
	 * Table 2-5: Special Cases of REX Encodings
	 */
	if (vie->index != 4)
		vie->index_register = gpr_map[vie->index];

	/* 'scale' makes sense only in the context of an index register */
	if (vie->index_register < VM_REG_LAST)
		vie->scale = 1 << vie->ss;

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
		int8_t	signed8;
		int32_t	signed32;
	} u;

	if ((n = vie->imm_bytes) == 0)
		return (0);

	if (n != 1 && n != 4)
		panic("decode_immediate: invalid imm_bytes %d", n);

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x))
			return (-1);

		u.buf[i] = x;
		vie_advance(vie);
	}
	
	if (n == 1)
		vie->immediate = u.signed8;		/* sign-extended */
	else
		vie->immediate = u.signed32;		/* sign-extended */

	return (0);
}

/*
 * Verify that all the bytes in the instruction buffer were consumed.
 */
static int
verify_inst_length(struct vie *vie)
{

	if (vie->num_processed == vie->num_valid)
		return (0);
	else
		return (-1);
}

/*
 * Verify that the 'guest linear address' provided as collateral of the nested
 * page table fault matches with our instruction decoding.
 */
static int
verify_gla(struct vm *vm, int cpuid, uint64_t gla, struct vie *vie)
{
	int error;
	uint64_t base, idx;

	/* Skip 'gla' verification */
	if (gla == VIE_INVALID_GLA)
		return (0);

	base = 0;
	if (vie->base_register != VM_REG_LAST) {
		error = vm_get_register(vm, cpuid, vie->base_register, &base);
		if (error) {
			printf("verify_gla: error %d getting base reg %d\n",
				error, vie->base_register);
			return (-1);
		}

		/*
		 * RIP-relative addressing starts from the following
		 * instruction
		 */
		if (vie->base_register == VM_REG_GUEST_RIP)
			base += vie->num_valid;
	}

	idx = 0;
	if (vie->index_register != VM_REG_LAST) {
		error = vm_get_register(vm, cpuid, vie->index_register, &idx);
		if (error) {
			printf("verify_gla: error %d getting index reg %d\n",
				error, vie->index_register);
			return (-1);
		}
	}

	if (base + vie->scale * idx + vie->displacement != gla) {
		printf("verify_gla mismatch: "
		       "base(0x%0lx), scale(%d), index(0x%0lx), "
		       "disp(0x%0lx), gla(0x%0lx)\n",
		       base, vie->scale, idx, vie->displacement, gla);
		return (-1);
	}

	return (0);
}

int
vmm_decode_instruction(struct vm *vm, int cpuid, uint64_t gla, struct vie *vie)
{

	if (decode_rex(vie))
		return (-1);

	if (decode_opcode(vie))
		return (-1);

	if (decode_modrm(vie))
		return (-1);

	if (decode_sib(vie))
		return (-1);

	if (decode_displacement(vie))
		return (-1);
	
	if (decode_immediate(vie))
		return (-1);

	if (verify_inst_length(vie))
		return (-1);

	if (verify_gla(vm, cpuid, gla, vie))
		return (-1);

	vie->decoded = 1;	/* success */

	return (0);
}
#endif	/* _KERNEL */
