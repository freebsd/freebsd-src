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
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/vmparam.h>
#include <machine/vmm.h>
#else	/* !_KERNEL */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/_iovec.h>

#include <machine/vmm.h>

#include <assert.h>
#include <vmmapi.h>
#define	KASSERT(exp,msg)	assert((exp))
#endif	/* _KERNEL */

#include <machine/vmm_instruction_emul.h>
#include <x86/psl.h>
#include <x86/specialreg.h>

/* struct vie_op.op_type */
enum {
	VIE_OP_TYPE_NONE = 0,
	VIE_OP_TYPE_MOV,
	VIE_OP_TYPE_MOVSX,
	VIE_OP_TYPE_MOVZX,
	VIE_OP_TYPE_AND,
	VIE_OP_TYPE_OR,
	VIE_OP_TYPE_TWO_BYTE,
	VIE_OP_TYPE_PUSH,
	VIE_OP_TYPE_CMP,
	VIE_OP_TYPE_LAST
};

/* struct vie_op.op_flags */
#define	VIE_OP_F_IMM		(1 << 0)  /* 16/32-bit immediate operand */
#define	VIE_OP_F_IMM8		(1 << 1)  /* 8-bit immediate operand */
#define	VIE_OP_F_MOFFSET	(1 << 2)  /* 16/32/64-bit immediate moffset */
#define	VIE_OP_F_NO_MODRM	(1 << 3)

static const struct vie_op two_byte_opcodes[256] = {
	[0xB6] = {
		.op_byte = 0xB6,
		.op_type = VIE_OP_TYPE_MOVZX,
	},
	[0xBE] = {
		.op_byte = 0xBE,
		.op_type = VIE_OP_TYPE_MOVSX,
	},
};

static const struct vie_op one_byte_opcodes[256] = {
	[0x0F] = {
		.op_byte = 0x0F,
		.op_type = VIE_OP_TYPE_TWO_BYTE
	},
	[0x3B] = {
		.op_byte = 0x3B,
		.op_type = VIE_OP_TYPE_CMP,
	},
	[0x88] = {
		.op_byte = 0x88,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x89] = {
		.op_byte = 0x89,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8A] = {
		.op_byte = 0x8A,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8B] = {
		.op_byte = 0x8B,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0xA1] = {
		.op_byte = 0xA1,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xA3] = {
		.op_byte = 0xA3,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xC6] = {
		/* XXX Group 11 extended opcode - not just MOV */
		.op_byte = 0xC6,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM8,
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
	[0xFF] = {
		/* XXX Group 5 extended opcode - not just PUSH */
		.op_byte = 0xFF,
		.op_type = VIE_OP_TYPE_PUSH,
	}
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

static void
vie_calc_bytereg(struct vie *vie, enum vm_reg_name *reg, int *lhbr)
{
	*lhbr = 0;
	*reg = gpr_map[vie->reg];

	/*
	 * 64-bit mode imposes limitations on accessing legacy high byte
	 * registers (lhbr).
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
			*lhbr = 1;
			*reg = gpr_map[vie->reg & 0x3];
		}
	}
}

static int
vie_read_bytereg(void *vm, int vcpuid, struct vie *vie, uint8_t *rval)
{
	uint64_t val;
	int error, lhbr;
	enum vm_reg_name reg;

	vie_calc_bytereg(vie, &reg, &lhbr);
	error = vm_get_register(vm, vcpuid, reg, &val);

	/*
	 * To obtain the value of a legacy high byte register shift the
	 * base register right by 8 bits (%ah = %rax >> 8).
	 */
	if (lhbr)
		*rval = val >> 8;
	else
		*rval = val;
	return (error);
}

static int
vie_write_bytereg(void *vm, int vcpuid, struct vie *vie, uint8_t byte)
{
	uint64_t origval, val, mask;
	int error, lhbr;
	enum vm_reg_name reg;

	vie_calc_bytereg(vie, &reg, &lhbr);
	error = vm_get_register(vm, vcpuid, reg, &origval);
	if (error == 0) {
		val = byte;
		mask = 0xff;
		if (lhbr) {
			/*
			 * Shift left by 8 to store 'byte' in a legacy high
			 * byte register.
			 */
			val <<= 8;
			mask <<= 8;
		}
		val |= origval & ~mask;
		error = vm_set_register(vm, vcpuid, reg, val);
	}
	return (error);
}

int
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
 * Return the status flags that would result from doing (x - y).
 */
static u_long
getcc16(uint16_t x, uint16_t y)
{
	u_long rflags;

	__asm __volatile("sub %1,%2; pushfq; popq %0" :
	    "=r" (rflags) : "m" (y), "r" (x));
	return (rflags);
}

static u_long
getcc32(uint32_t x, uint32_t y)
{
	u_long rflags;

	__asm __volatile("sub %1,%2; pushfq; popq %0" :
	    "=r" (rflags) : "m" (y), "r" (x));
	return (rflags);
}

static u_long
getcc64(uint64_t x, uint64_t y)
{
	u_long rflags;

	__asm __volatile("sub %1,%2; pushfq; popq %0" :
	    "=r" (rflags) : "m" (y), "r" (x));
	return (rflags);
}

static u_long
getcc(int opsize, uint64_t x, uint64_t y)
{
	KASSERT(opsize == 2 || opsize == 4 || opsize == 8,
	    ("getcc: invalid operand size %d", opsize));

	if (opsize == 2)
		return (getcc16(x, y));
	else if (opsize == 4)
		return (getcc32(x, y));
	else
		return (getcc64(x, y));
}

static int
emulate_mov(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	    mem_region_read_t memread, mem_region_write_t memwrite, void *arg)
{
	int error, size;
	enum vm_reg_name reg;
	uint8_t byte;
	uint64_t val;

	size = vie->opsize;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x88:
		/*
		 * MOV byte from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 88/r:	mov r/m8, r8
		 * REX + 88/r:	mov r/m8, r8 (%ah, %ch, %dh, %bh not available)
		 */
		size = 1;	/* override for byte operation */
		error = vie_read_bytereg(vm, vcpuid, vie, &byte);
		if (error == 0)
			error = memwrite(vm, vcpuid, gpa, byte, size, arg);
		break;
	case 0x89:
		/*
		 * MOV from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 89/r:	mov r/m16, r16
		 * 89/r:	mov r/m32, r32
		 * REX.W + 89/r	mov r/m64, r64
		 */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vm, vcpuid, reg, &val);
		if (error == 0) {
			val &= size2mask[size];
			error = memwrite(vm, vcpuid, gpa, val, size, arg);
		}
		break;
	case 0x8A:
		/*
		 * MOV byte from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8A/r:	mov r8, r/m8
		 * REX + 8A/r:	mov r8, r/m8
		 */
		size = 1;	/* override for byte operation */
		error = memread(vm, vcpuid, gpa, &val, size, arg);
		if (error == 0)
			error = vie_write_bytereg(vm, vcpuid, vie, val);
		break;
	case 0x8B:
		/*
		 * MOV from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8B/r:	mov r16, r/m16
		 * 8B/r:	mov r32, r/m32
		 * REX.W 8B/r:	mov r64, r/m64
		 */
		error = memread(vm, vcpuid, gpa, &val, size, arg);
		if (error == 0) {
			reg = gpr_map[vie->reg];
			error = vie_update_register(vm, vcpuid, reg, val, size);
		}
		break;
	case 0xA1:
		/*
		 * MOV from seg:moffset to AX/EAX/RAX
		 * A1:		mov AX, moffs16
		 * A1:		mov EAX, moffs32
		 * REX.W + A1:	mov RAX, moffs64
		 */
		error = memread(vm, vcpuid, gpa, &val, size, arg);
		if (error == 0) {
			reg = VM_REG_GUEST_RAX;
			error = vie_update_register(vm, vcpuid, reg, val, size);
		}
		break;
	case 0xA3:
		/*
		 * MOV from AX/EAX/RAX to seg:moffset
		 * A3:		mov moffs16, AX
		 * A3:		mov moffs32, EAX 
		 * REX.W + A3:	mov moffs64, RAX
		 */
		error = vie_read_register(vm, vcpuid, VM_REG_GUEST_RAX, &val);
		if (error == 0) {
			val &= size2mask[size];
			error = memwrite(vm, vcpuid, gpa, val, size, arg);
		}
		break;
	case 0xC6:
		/*
		 * MOV from imm8 to mem (ModRM:r/m)
		 * C6/0		mov r/m8, imm8
		 * REX + C6/0	mov r/m8, imm8
		 */
		size = 1;	/* override for byte operation */
		error = memwrite(vm, vcpuid, gpa, vie->immediate, size, arg);
		break;
	case 0xC7:
		/*
		 * MOV from imm16/imm32 to mem (ModRM:r/m)
		 * C7/0		mov r/m16, imm16
		 * C7/0		mov r/m32, imm32
		 * REX.W + C7/0	mov r/m64, imm32 (sign-extended to 64-bits)
		 */
		val = vie->immediate & size2mask[size];
		error = memwrite(vm, vcpuid, gpa, val, size, arg);
		break;
	default:
		break;
	}

	return (error);
}

static int
emulate_movx(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	     mem_region_read_t memread, mem_region_write_t memwrite,
	     void *arg)
{
	int error, size;
	enum vm_reg_name reg;
	uint64_t val;

	size = vie->opsize;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0xB6:
		/*
		 * MOV and zero extend byte from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F B6/r		movzx r16, r/m8
		 * 0F B6/r		movzx r32, r/m8
		 * REX.W + 0F B6/r	movzx r64, r/m8
		 */

		/* get the first operand */
		error = memread(vm, vcpuid, gpa, &val, 1, arg);
		if (error)
			break;

		/* get the second operand */
		reg = gpr_map[vie->reg];

		/* zero-extend byte */
		val = (uint8_t)val;

		/* write the result */
		error = vie_update_register(vm, vcpuid, reg, val, size);
		break;
	case 0xBE:
		/*
		 * MOV and sign extend byte from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F BE/r		movsx r16, r/m8
		 * 0F BE/r		movsx r32, r/m8
		 * REX.W + 0F BE/r	movsx r64, r/m8
		 */

		/* get the first operand */
		error = memread(vm, vcpuid, gpa, &val, 1, arg);
		if (error)
			break;

		/* get the second operand */
		reg = gpr_map[vie->reg];

		/* sign extend byte */
		val = (int8_t)val;

		/* write the result */
		error = vie_update_register(vm, vcpuid, reg, val, size);
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

	size = vie->opsize;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x23:
		/*
		 * AND reg (ModRM:reg) and mem (ModRM:r/m) and store the
		 * result in reg.
		 *
		 * 23/r		and r16, r/m16
		 * 23/r		and r32, r/m32
		 * REX.W + 23/r	and r64, r/m64
		 */

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
		 * 81 /4		and r/m16, imm16
		 * 81 /4		and r/m32, imm32
		 * REX.W + 81 /4	and r/m64, imm32 sign-extended to 64
		 *
		 * Currently, only the AND operation of the 0x81 opcode
		 * is implemented (ModRM:reg = b100).
		 */
		if ((vie->reg & 7) != 4)
			break;

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

	size = vie->opsize;
	error = EINVAL;

	switch (vie->op.op_byte) {
	case 0x83:
		/*
		 * OR mem (ModRM:r/m) with immediate and store the
		 * result in mem.
		 *
		 * 83 /1		OR r/m16, imm8 sign-extended to 16
		 * 83 /1		OR r/m32, imm8 sign-extended to 32
		 * REX.W + 83/1		OR r/m64, imm8 sign-extended to 64
		 *
		 * Currently, only the OR operation of the 0x83 opcode
		 * is implemented (ModRM:reg = b001).
		 */
		if ((vie->reg & 7) != 1)
			break;

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

#define	RFLAGS_STATUS_BITS    (PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V)

static int
emulate_cmp(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
	    mem_region_read_t memread, mem_region_write_t memwrite, void *arg)
{
	int error, size;
	uint64_t op1, op2, rflags, rflags2;
	enum vm_reg_name reg;

	size = vie->opsize;
	switch (vie->op.op_byte) {
	case 0x3B:
		/*
		 * 3B/r		CMP r16, r/m16
		 * 3B/r		CMP r32, r/m32
		 * REX.W + 3B/r	CMP r64, r/m64
		 *
		 * Compare first operand (reg) with second operand (r/m) and
		 * set status flags in EFLAGS register. The comparison is
		 * performed by subtracting the second operand from the first
		 * operand and then setting the status flags.
		 */

		/* Get the first operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vm, vcpuid, reg, &op1);
		if (error)
			return (error);

		/* Get the second operand */
		error = memread(vm, vcpuid, gpa, &op2, size, arg);
		if (error)
			return (error);

		break;
	default:
		return (EINVAL);
	}
	rflags2 = getcc(size, op1, op2);
	error = vie_read_register(vm, vcpuid, VM_REG_GUEST_RFLAGS, &rflags);
	if (error)
		return (error);
	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & RFLAGS_STATUS_BITS;

	error = vie_update_register(vm, vcpuid, VM_REG_GUEST_RFLAGS, rflags, 8);
	return (error);
}

static int
emulate_push(void *vm, int vcpuid, uint64_t mmio_gpa, struct vie *vie,
    struct vm_guest_paging *paging, mem_region_read_t memread,
    mem_region_write_t memwrite, void *arg)
{
#ifdef _KERNEL
	struct vm_copyinfo copyinfo[2];
#else
	struct iovec copyinfo[2];
#endif
	struct seg_desc ss_desc;
	uint64_t cr0, rflags, rsp, stack_gla, val;
	int error, size, stackaddrsize;

	/*
	 * Table A-6, "Opcode Extensions", Intel SDM, Vol 2.
	 *
	 * PUSH is part of the group 5 extended opcodes and is identified
	 * by ModRM:reg = b110.
	 */
	if ((vie->reg & 7) != 6)
		return (EINVAL);

	size = vie->opsize;
	/*
	 * From "Address-Size Attributes for Stack Accesses", Intel SDL, Vol 1
	 */
	if (paging->cpu_mode == CPU_MODE_REAL)
		stackaddrsize = 2;
	else if (paging->cpu_mode == CPU_MODE_64BIT)
		stackaddrsize = 8;
	else {
		/*
		 * In protected or compability mode the 'B' flag in the
		 * stack-segment descriptor determines the size of the
		 * stack pointer.
		 */
		error = vm_get_seg_desc(vm, vcpuid, VM_REG_GUEST_SS, &ss_desc);
		KASSERT(error == 0, ("%s: error %d getting SS descriptor",
		    __func__, error));
		if (SEG_DESC_DEF32(ss_desc.access))
			stackaddrsize = 4;
		else
			stackaddrsize = 2;
	}

	error = vie_read_register(vm, vcpuid, VM_REG_GUEST_CR0, &cr0);
	KASSERT(error == 0, ("%s: error %d getting cr0", __func__, error));

	error = vie_read_register(vm, vcpuid, VM_REG_GUEST_RFLAGS, &rflags);
	KASSERT(error == 0, ("%s: error %d getting rflags", __func__, error));

	error = vie_read_register(vm, vcpuid, VM_REG_GUEST_RSP, &rsp);
	KASSERT(error == 0, ("%s: error %d getting rsp", __func__, error));

	rsp -= size;
	if (vie_calculate_gla(paging->cpu_mode, VM_REG_GUEST_SS, &ss_desc,
	    rsp, size, stackaddrsize, PROT_WRITE, &stack_gla)) {
		vm_inject_ss(vm, vcpuid, 0);
		return (0);
	}

	if (vie_canonical_check(paging->cpu_mode, stack_gla)) {
		vm_inject_ss(vm, vcpuid, 0);
		return (0);
	}

	if (vie_alignment_check(paging->cpl, size, cr0, rflags, stack_gla)) {
		vm_inject_ac(vm, vcpuid, 0);
		return (0);
	}

	error = vm_copy_setup(vm, vcpuid, paging, stack_gla, size, PROT_WRITE,
	    copyinfo, nitems(copyinfo));
	if (error)
		return (error);

	error = memread(vm, vcpuid, mmio_gpa, &val, size, arg);
	if (error == 0) {
		vm_copyout(vm, vcpuid, &val, copyinfo, size);
		error = vie_update_register(vm, vcpuid, VM_REG_GUEST_RSP, rsp,
		    stackaddrsize);
		KASSERT(error == 0, ("error %d updating rsp", error));
	}
#ifdef _KERNEL
	vm_copy_teardown(vm, vcpuid, copyinfo, nitems(copyinfo));
#endif
	return (error);
}

int
vmm_emulate_instruction(void *vm, int vcpuid, uint64_t gpa, struct vie *vie,
    struct vm_guest_paging *paging, mem_region_read_t memread,
    mem_region_write_t memwrite, void *memarg)
{
	int error;

	if (!vie->decoded)
		return (EINVAL);

	switch (vie->op.op_type) {
	case VIE_OP_TYPE_PUSH:
		error = emulate_push(vm, vcpuid, gpa, vie, paging, memread,
		    memwrite, memarg);
		break;
	case VIE_OP_TYPE_CMP:
		error = emulate_cmp(vm, vcpuid, gpa, vie,
				    memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_MOV:
		error = emulate_mov(vm, vcpuid, gpa, vie,
				    memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_MOVSX:
	case VIE_OP_TYPE_MOVZX:
		error = emulate_movx(vm, vcpuid, gpa, vie,
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

int
vie_alignment_check(int cpl, int size, uint64_t cr0, uint64_t rf, uint64_t gla)
{
	KASSERT(size == 1 || size == 2 || size == 4 || size == 8,
	    ("%s: invalid size %d", __func__, size));
	KASSERT(cpl >= 0 && cpl <= 3, ("%s: invalid cpl %d", __func__, cpl));

	if (cpl != 3 || (cr0 & CR0_AM) == 0 || (rf & PSL_AC) == 0)
		return (0);

	return ((gla & (size - 1)) ? 1 : 0);
}

int
vie_canonical_check(enum vm_cpu_mode cpu_mode, uint64_t gla)
{
	uint64_t mask;

	if (cpu_mode != CPU_MODE_64BIT)
		return (0);

	/*
	 * The value of the bit 47 in the 'gla' should be replicated in the
	 * most significant 16 bits.
	 */
	mask = ~((1UL << 48) - 1);
	if (gla & (1UL << 47))
		return ((gla & mask) != mask);
	else
		return ((gla & mask) != 0);
}

uint64_t
vie_size2mask(int size)
{
	KASSERT(size == 1 || size == 2 || size == 4 || size == 8,
	    ("vie_size2mask: invalid size %d", size));
	return (size2mask[size]);
}

int
vie_calculate_gla(enum vm_cpu_mode cpu_mode, enum vm_reg_name seg,
    struct seg_desc *desc, uint64_t offset, int length, int addrsize,
    int prot, uint64_t *gla)
{
	uint64_t firstoff, low_limit, high_limit, segbase;
	int glasize, type;

	KASSERT(seg >= VM_REG_GUEST_ES && seg <= VM_REG_GUEST_GS,
	    ("%s: invalid segment %d", __func__, seg));
	KASSERT(length == 1 || length == 2 || length == 4 || length == 8,
	    ("%s: invalid operand size %d", __func__, length));
	KASSERT((prot & ~(PROT_READ | PROT_WRITE)) == 0,
	    ("%s: invalid prot %#x", __func__, prot));

	firstoff = offset;
	if (cpu_mode == CPU_MODE_64BIT) {
		KASSERT(addrsize == 4 || addrsize == 8, ("%s: invalid address "
		    "size %d for cpu_mode %d", __func__, addrsize, cpu_mode));
		glasize = 8;
	} else {
		KASSERT(addrsize == 2 || addrsize == 4, ("%s: invalid address "
		    "size %d for cpu mode %d", __func__, addrsize, cpu_mode));
		glasize = 4;
		/*
		 * If the segment selector is loaded with a NULL selector
		 * then the descriptor is unusable and attempting to use
		 * it results in a #GP(0).
		 */
		if (SEG_DESC_UNUSABLE(desc->access))
			return (-1);

		/* 
		 * The processor generates a #NP exception when a segment
		 * register is loaded with a selector that points to a
		 * descriptor that is not present. If this was the case then
		 * it would have been checked before the VM-exit.
		 */
		KASSERT(SEG_DESC_PRESENT(desc->access),
		    ("segment %d not present: %#x", seg, desc->access));

		/*
		 * The descriptor type must indicate a code/data segment.
		 */
		type = SEG_DESC_TYPE(desc->access);
		KASSERT(type >= 16 && type <= 31, ("segment %d has invalid "
		    "descriptor type %#x", seg, type));

		if (prot & PROT_READ) {
			/* #GP on a read access to a exec-only code segment */
			if ((type & 0xA) == 0x8)
				return (-1);
		}

		if (prot & PROT_WRITE) {
			/*
			 * #GP on a write access to a code segment or a
			 * read-only data segment.
			 */
			if (type & 0x8)			/* code segment */
				return (-1);

			if ((type & 0xA) == 0)		/* read-only data seg */
				return (-1);
		}

		/*
		 * 'desc->limit' is fully expanded taking granularity into
		 * account.
		 */
		if ((type & 0xC) == 0x4) {
			/* expand-down data segment */
			low_limit = desc->limit + 1;
			high_limit = SEG_DESC_DEF32(desc->access) ?
			    0xffffffff : 0xffff;
		} else {
			/* code segment or expand-up data segment */
			low_limit = 0;
			high_limit = desc->limit;
		}

		while (length > 0) {
			offset &= vie_size2mask(addrsize);
			if (offset < low_limit || offset > high_limit)
				return (-1);
			offset++;
			length--;
		}
	}

	/*
	 * In 64-bit mode all segments except %fs and %gs have a segment
	 * base address of 0.
	 */
	if (cpu_mode == CPU_MODE_64BIT && seg != VM_REG_GUEST_FS &&
	    seg != VM_REG_GUEST_GS) {
		segbase = 0;
	} else {
		segbase = desc->base;
	}

	/*
	 * Truncate 'firstoff' to the effective address size before adding
	 * it to the segment base.
	 */
	firstoff &= vie_size2mask(addrsize);
	*gla = (segbase + firstoff) & vie_size2mask(glasize);
	return (0);
}

#ifdef _KERNEL
void
vie_init(struct vie *vie)
{

	bzero(vie, sizeof(struct vie));

	vie->base_register = VM_REG_LAST;
	vie->index_register = VM_REG_LAST;
}

static int
pf_error_code(int usermode, int prot, int rsvd, uint64_t pte)
{
	int error_code = 0;

	if (pte & PG_V)
		error_code |= PGEX_P;
	if (prot & VM_PROT_WRITE)
		error_code |= PGEX_W;
	if (usermode)
		error_code |= PGEX_U;
	if (rsvd)
		error_code |= PGEX_RSV;
	if (prot & VM_PROT_EXECUTE)
		error_code |= PGEX_I;

	return (error_code);
}

static void
ptp_release(void **cookie)
{
	if (*cookie != NULL) {
		vm_gpa_release(*cookie);
		*cookie = NULL;
	}
}

static void *
ptp_hold(struct vm *vm, vm_paddr_t ptpphys, size_t len, void **cookie)
{
	void *ptr;

	ptp_release(cookie);
	ptr = vm_gpa_hold(vm, ptpphys, len, VM_PROT_RW, cookie);
	return (ptr);
}

int
vmm_gla2gpa(struct vm *vm, int vcpuid, struct vm_guest_paging *paging,
    uint64_t gla, int prot, uint64_t *gpa)
{
	int nlevels, pfcode, ptpshift, ptpindex, retval, usermode, writable;
	u_int retries;
	uint64_t *ptpbase, ptpphys, pte, pgsize;
	uint32_t *ptpbase32, pte32;
	void *cookie;

	usermode = (paging->cpl == 3 ? 1 : 0);
	writable = prot & VM_PROT_WRITE;
	cookie = NULL;
	retval = 0;
	retries = 0;
restart:
	ptpphys = paging->cr3;		/* root of the page tables */
	ptp_release(&cookie);
	if (retries++ > 0)
		maybe_yield();

	if (vie_canonical_check(paging->cpu_mode, gla)) {
		/*
		 * XXX assuming a non-stack reference otherwise a stack fault
		 * should be generated.
		 */
		vm_inject_gp(vm, vcpuid);
		goto fault;
	}

	if (paging->paging_mode == PAGING_MODE_FLAT) {
		*gpa = gla;
		goto done;
	}

	if (paging->paging_mode == PAGING_MODE_32) {
		nlevels = 2;
		while (--nlevels >= 0) {
			/* Zero out the lower 12 bits. */
			ptpphys &= ~0xfff;

			ptpbase32 = ptp_hold(vm, ptpphys, PAGE_SIZE, &cookie);

			if (ptpbase32 == NULL)
				goto error;

			ptpshift = PAGE_SHIFT + nlevels * 10;
			ptpindex = (gla >> ptpshift) & 0x3FF;
			pgsize = 1UL << ptpshift;

			pte32 = ptpbase32[ptpindex];

			if ((pte32 & PG_V) == 0 ||
			    (usermode && (pte32 & PG_U) == 0) ||
			    (writable && (pte32 & PG_RW) == 0)) {
				pfcode = pf_error_code(usermode, prot, 0,
				    pte32);
				vm_inject_pf(vm, vcpuid, pfcode, gla);
				goto fault;
			}

			/*
			 * Emulate the x86 MMU's management of the accessed
			 * and dirty flags. While the accessed flag is set
			 * at every level of the page table, the dirty flag
			 * is only set at the last level providing the guest
			 * physical address.
			 */
			if ((pte32 & PG_A) == 0) {
				if (atomic_cmpset_32(&ptpbase32[ptpindex],
				    pte32, pte32 | PG_A) == 0) {
					goto restart;
				}
			}

			/* XXX must be ignored if CR4.PSE=0 */
			if (nlevels > 0 && (pte32 & PG_PS) != 0)
				break;

			ptpphys = pte32;
		}

		/* Set the dirty bit in the page table entry if necessary */
		if (writable && (pte32 & PG_M) == 0) {
			if (atomic_cmpset_32(&ptpbase32[ptpindex],
			    pte32, pte32 | PG_M) == 0) {
				goto restart;
			}
		}

		/* Zero out the lower 'ptpshift' bits */
		pte32 >>= ptpshift; pte32 <<= ptpshift;
		*gpa = pte32 | (gla & (pgsize - 1));
		goto done;
	}

	if (paging->paging_mode == PAGING_MODE_PAE) {
		/* Zero out the lower 5 bits and the upper 32 bits */
		ptpphys &= 0xffffffe0UL;

		ptpbase = ptp_hold(vm, ptpphys, sizeof(*ptpbase) * 4, &cookie);
		if (ptpbase == NULL)
			goto error;

		ptpindex = (gla >> 30) & 0x3;

		pte = ptpbase[ptpindex];

		if ((pte & PG_V) == 0) {
			pfcode = pf_error_code(usermode, prot, 0, pte);
			vm_inject_pf(vm, vcpuid, pfcode, gla);
			goto fault;
		}

		ptpphys = pte;

		nlevels = 2;
	} else
		nlevels = 4;
	while (--nlevels >= 0) {
		/* Zero out the lower 12 bits and the upper 12 bits */
		ptpphys >>= 12; ptpphys <<= 24; ptpphys >>= 12;

		ptpbase = ptp_hold(vm, ptpphys, PAGE_SIZE, &cookie);
		if (ptpbase == NULL)
			goto error;

		ptpshift = PAGE_SHIFT + nlevels * 9;
		ptpindex = (gla >> ptpshift) & 0x1FF;
		pgsize = 1UL << ptpshift;

		pte = ptpbase[ptpindex];

		if ((pte & PG_V) == 0 ||
		    (usermode && (pte & PG_U) == 0) ||
		    (writable && (pte & PG_RW) == 0)) {
			pfcode = pf_error_code(usermode, prot, 0, pte);
			vm_inject_pf(vm, vcpuid, pfcode, gla);
			goto fault;
		}

		/* Set the accessed bit in the page table entry */
		if ((pte & PG_A) == 0) {
			if (atomic_cmpset_64(&ptpbase[ptpindex],
			    pte, pte | PG_A) == 0) {
				goto restart;
			}
		}

		if (nlevels > 0 && (pte & PG_PS) != 0) {
			if (pgsize > 1 * GB) {
				pfcode = pf_error_code(usermode, prot, 1, pte);
				vm_inject_pf(vm, vcpuid, pfcode, gla);
				goto fault;
			}
			break;
		}

		ptpphys = pte;
	}

	/* Set the dirty bit in the page table entry if necessary */
	if (writable && (pte & PG_M) == 0) {
		if (atomic_cmpset_64(&ptpbase[ptpindex], pte, pte | PG_M) == 0)
			goto restart;
	}

	/* Zero out the lower 'ptpshift' bits and the upper 12 bits */
	pte >>= ptpshift; pte <<= (ptpshift + 12); pte >>= 12;
	*gpa = pte | (gla & (pgsize - 1));
done:
	ptp_release(&cookie);
	return (retval);
error:
	retval = -1;
	goto done;
fault:
	retval = 1;
	goto done;
}

int
vmm_fetch_instruction(struct vm *vm, int vcpuid, struct vm_guest_paging *paging,
    uint64_t rip, int inst_length, struct vie *vie)
{
	struct vm_copyinfo copyinfo[2];
	int error, prot;

	if (inst_length > VIE_INST_SIZE)
		panic("vmm_fetch_instruction: invalid length %d", inst_length);

	prot = PROT_READ | PROT_EXEC;
	error = vm_copy_setup(vm, vcpuid, paging, rip, inst_length, prot,
	    copyinfo, nitems(copyinfo));
	if (error == 0) {
		vm_copyin(vm, vcpuid, copyinfo, vie->inst, inst_length);
		vm_copy_teardown(vm, vcpuid, copyinfo, nitems(copyinfo));
		vie->num_valid = inst_length;
	}
	return (error);
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
decode_prefixes(struct vie *vie, enum vm_cpu_mode cpu_mode, int cs_d)
{
	uint8_t x;

	while (1) {
		if (vie_peek(vie, &x))
			return (-1);

		if (x == 0x66)
			vie->opsize_override = 1;
		else if (x == 0x67)
			vie->addrsize_override = 1;
		else
			break;

		vie_advance(vie);
	}

	/*
	 * From section 2.2.1, "REX Prefixes", Intel SDM Vol 2:
	 * - Only one REX prefix is allowed per instruction.
	 * - The REX prefix must immediately precede the opcode byte or the
	 *   escape opcode byte.
	 * - If an instruction has a mandatory prefix (0x66, 0xF2 or 0xF3)
	 *   the mandatory prefix must come before the REX prefix.
	 */
	if (cpu_mode == CPU_MODE_64BIT && x >= 0x40 && x <= 0x4F) {
		vie->rex_present = 1;
		vie->rex_w = x & 0x8 ? 1 : 0;
		vie->rex_r = x & 0x4 ? 1 : 0;
		vie->rex_x = x & 0x2 ? 1 : 0;
		vie->rex_b = x & 0x1 ? 1 : 0;
		vie_advance(vie);
	}

	/*
	 * Section "Operand-Size And Address-Size Attributes", Intel SDM, Vol 1
	 */
	if (cpu_mode == CPU_MODE_64BIT) {
		/*
		 * Default address size is 64-bits and default operand size
		 * is 32-bits.
		 */
		vie->addrsize = vie->addrsize_override ? 4 : 8;
		if (vie->rex_w)
			vie->opsize = 8;
		else if (vie->opsize_override)
			vie->opsize = 2;
		else
			vie->opsize = 4;
	} else if (cs_d) {
		/* Default address and operand sizes are 32-bits */
		vie->addrsize = vie->addrsize_override ? 2 : 4;
		vie->opsize = vie->opsize_override ? 2 : 4;
	} else {
		/* Default address and operand sizes are 16-bits */
		vie->addrsize = vie->addrsize_override ? 4 : 2;
		vie->opsize = vie->opsize_override ? 4 : 2;
	}
	return (0);
}

static int
decode_two_byte_opcode(struct vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x))
		return (-1);

	vie->op = two_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE)
		return (-1);

	vie_advance(vie);
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

	if (vie->op.op_type == VIE_OP_TYPE_TWO_BYTE)
		return (decode_two_byte_opcode(vie));

	return (0);
}

static int
decode_modrm(struct vie *vie, enum vm_cpu_mode cpu_mode)
{
	uint8_t x;

	if (cpu_mode == CPU_MODE_REAL)
		return (-1);

	if (vie->op.op_flags & VIE_OP_F_NO_MODRM)
		return (0);

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
		int16_t	signed16;
		int32_t	signed32;
	} u;

	/* Figure out immediate operand size (if any) */
	if (vie->op.op_flags & VIE_OP_F_IMM) {
		/*
		 * Section 2.2.1.5 "Immediates", Intel SDM:
		 * In 64-bit mode the typical size of immediate operands
		 * remains 32-bits. When the operand size if 64-bits, the
		 * processor sign-extends all immediates to 64-bits prior
		 * to their use.
		 */
		if (vie->opsize == 4 || vie->opsize == 8)
			vie->imm_bytes = 4;
		else
			vie->imm_bytes = 2;
	} else if (vie->op.op_flags & VIE_OP_F_IMM8) {
		vie->imm_bytes = 1;
	}

	if ((n = vie->imm_bytes) == 0)
		return (0);

	KASSERT(n == 1 || n == 2 || n == 4,
	    ("%s: invalid number of immediate bytes: %d", __func__, n));

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x))
			return (-1);

		u.buf[i] = x;
		vie_advance(vie);
	}

	/* sign-extend the immediate value before use */
	if (n == 1)
		vie->immediate = u.signed8;
	else if (n == 2)
		vie->immediate = u.signed16;
	else
		vie->immediate = u.signed32;

	return (0);
}

static int
decode_moffset(struct vie *vie)
{
	int i, n;
	uint8_t x;
	union {
		char	buf[8];
		uint64_t u64;
	} u;

	if ((vie->op.op_flags & VIE_OP_F_MOFFSET) == 0)
		return (0);

	/*
	 * Section 2.2.1.4, "Direct Memory-Offset MOVs", Intel SDM:
	 * The memory offset size follows the address-size of the instruction.
	 */
	n = vie->addrsize;
	KASSERT(n == 2 || n == 4 || n == 8, ("invalid moffset bytes: %d", n));

	u.u64 = 0;
	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x))
			return (-1);

		u.buf[i] = x;
		vie_advance(vie);
	}
	vie->displacement = u.u64;
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
	uint64_t base, idx, gla2;

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

	/* XXX assuming that the base address of the segment is 0 */
	gla2 = base + vie->scale * idx + vie->displacement;
	gla2 &= size2mask[vie->addrsize];
	if (gla != gla2) {
		printf("verify_gla mismatch: "
		       "base(0x%0lx), scale(%d), index(0x%0lx), "
		       "disp(0x%0lx), gla(0x%0lx), gla2(0x%0lx)\n",
		       base, vie->scale, idx, vie->displacement, gla, gla2);
		return (-1);
	}

	return (0);
}

int
vmm_decode_instruction(struct vm *vm, int cpuid, uint64_t gla,
		       enum vm_cpu_mode cpu_mode, int cs_d, struct vie *vie)
{

	if (decode_prefixes(vie, cpu_mode, cs_d))
		return (-1);

	if (decode_opcode(vie))
		return (-1);

	if (decode_modrm(vie, cpu_mode))
		return (-1);

	if (decode_sib(vie))
		return (-1);

	if (decode_displacement(vie))
		return (-1);

	if (decode_immediate(vie))
		return (-1);

	if (decode_moffset(vie))
		return (-1);

	if (verify_inst_length(vie))
		return (-1);

	if (verify_gla(vm, cpuid, gla, vie))
		return (-1);

	vie->decoded = 1;	/* success */

	return (0);
}
#endif	/* _KERNEL */
