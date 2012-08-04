/*-
 * Copyright (c) 2012 Sandvine, Inc.
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

#include <strings.h>
#include <unistd.h>
#include <machine/vmm.h>
#include <vmmapi.h>

#include "fbsdrun.h"
#include "instruction_emul.h"

#define PREFIX_LOCK 		0xF0
#define PREFIX_REPNE 		0xF2
#define PREFIX_REPE		0xF3
#define PREFIX_CS_OVERRIDE	0x2E
#define PREFIX_SS_OVERRIDE	0x36
#define PREFIX_DS_OVERRIDE	0x3E
#define PREFIX_ES_OVERRIDE	0x26
#define PREFIX_FS_OVERRIDE	0x64
#define PREFIX_GS_OVERRIDE	0x65
#define PREFIX_BRANCH_NOT_TAKEN	0x2E
#define PREFIX_BRANCH_TAKEN	0x3E
#define PREFIX_OPSIZE		0x66
#define PREFIX_ADDRSIZE 	0x67

#define OPCODE_2BYTE_ESCAPE	0x0F
#define OPCODE_3BYTE_ESCAPE	0x38

#define MODRM_MOD_MASK		0xC0
#define MODRM_MOD_SHIFT		6
#define MODRM_RM_MASK		0x07
#define MODRM_RM_SHIFT		0
#define MODRM_REG_MASK		0x38
#define MODRM_REG_SHIFT		3

#define MOD_INDIRECT		0x0
#define MOD_INDIRECT_DISP8	0x1
#define MOD_INDIRECT_DISP32	0x2
#define MOD_DIRECT		0x3

#define RM_EAX			0x0
#define RM_ECX			0x1
#define RM_EDX			0x2
#define RM_EBX			0x3
#define RM_SIB			0x4
#define RM_DISP32		0x5
#define RM_EBP			RM_DISP32
#define RM_ESI			0x6
#define RM_EDI			0x7

#define REG_EAX			0x0
#define REG_ECX			0x1
#define REG_EDX			0x2
#define REG_EBX			0x3
#define REG_ESP			0x4
#define REG_EBP			0x5
#define REG_ESI			0x6
#define REG_EDI			0x7
#define REG_R8			0x8
#define REG_R9			0x9
#define REG_R10			0xA
#define REG_R11			0xB
#define REG_R12			0xC
#define REG_R13			0xD
#define REG_R14			0xE
#define REG_R15			0xF

#define HAS_MODRM		1
#define FROM_RM			(1<<1)
#define FROM_REG		(1<<2)
#define TO_RM			(1<<3)
#define TO_REG			(1<<4)

#define REX_MASK		0xF0
#define REX_PREFIX		0x40
#define is_rex_prefix(x) ( ((x) & REX_MASK) == REX_PREFIX )
#define REX_W_MASK		0x8
#define REX_R_MASK		0x4
#define REX_X_MASK		0x2
#define REX_B_MASK		0x1

#define is_prefix(x) ((x) == PREFIX_LOCK || (x) == PREFIX_REPNE || \
		      (x) == PREFIX_REPE || (x) == PREFIX_CS_OVERRIDE || \
		      (x) == PREFIX_SS_OVERRIDE || (x) == PREFIX_DS_OVERRIDE || \
		      (x) == PREFIX_ES_OVERRIDE || (x) == PREFIX_FS_OVERRIDE || \
		      (x) == PREFIX_GS_OVERRIDE || (x) == PREFIX_BRANCH_NOT_TAKEN || \
		      (x) == PREFIX_BRANCH_TAKEN || (x) == PREFIX_OPSIZE || \
		      (x) == PREFIX_ADDRSIZE || is_rex_prefix((x)))

#define PAGE_FRAME_MASK		0x80
#define PAGE_OFFSET_MASK	0xFFF
#define PAGE_TABLE_ENTRY_MASK	(~PAGE_OFFSET_MASK)
#define PML4E_OFFSET_MASK	0x0000FF8000000000
#define PML4E_SHIFT		39

#define MAX_EMULATED_REGIONS 8
int registered_regions = 0;
struct memory_region
{
	uintptr_t start;
	uintptr_t end;
	emulated_read_func_t memread;
	emulated_write_func_t memwrite;
	void *arg;
} emulated_regions[MAX_EMULATED_REGIONS];

struct decoded_instruction
{
	void *instruction;
	uint8_t *opcode;
	uint8_t *modrm;
	uint8_t *sib;
	uint8_t  *displacement;
	uint8_t *immediate;

	uint8_t opcode_flags;

	uint8_t addressing_mode;
	uint8_t rm;
	uint8_t reg;
	uint8_t rex_r;
	uint8_t rex_w;
	uint8_t rex_b;
	uint8_t rex_x;

	int32_t disp;
};

static enum vm_reg_name vm_reg_name_mappings[] = {
	[REG_EAX] = VM_REG_GUEST_RAX,
	[REG_EBX] = VM_REG_GUEST_RBX,
	[REG_ECX] = VM_REG_GUEST_RCX,
	[REG_EDX] = VM_REG_GUEST_RDX,
	[REG_ESP] = VM_REG_GUEST_RSP,
	[REG_EBP] = VM_REG_GUEST_RBP,
	[REG_ESI] = VM_REG_GUEST_RSI,
	[REG_EDI] = VM_REG_GUEST_RDI,
	[REG_R8]  = VM_REG_GUEST_R8,
	[REG_R9]  = VM_REG_GUEST_R9,
	[REG_R10] = VM_REG_GUEST_R10,
	[REG_R11] = VM_REG_GUEST_R11,
	[REG_R12] = VM_REG_GUEST_R12,
	[REG_R13] = VM_REG_GUEST_R13,
	[REG_R14] = VM_REG_GUEST_R14,
	[REG_R15] = VM_REG_GUEST_R15
};

uint8_t one_byte_opcodes[256] = {
	[0x89]  = HAS_MODRM | FROM_REG | TO_RM,
	[0x8B]	= HAS_MODRM | FROM_RM | TO_REG,
};

static uintptr_t 
gla2gpa(uint64_t gla, uint64_t guest_cr3)
{
	uint64_t *table;
	uint64_t mask, entry;
	int level, shift;
	uintptr_t page_frame;

        table = paddr_guest2host(guest_cr3 & PAGE_TABLE_ENTRY_MASK);
        mask = PML4E_OFFSET_MASK;
        shift = PML4E_SHIFT;
        for (level = 0; level < 4; ++level)
        {
		entry = table[(gla & mask) >> shift];
		table = (uint64_t*)(entry & PAGE_TABLE_ENTRY_MASK);

		/* This entry does not point to another page table */
		if (entry & PAGE_FRAME_MASK || level >= 3) 
			break;
		
		table = paddr_guest2host((uintptr_t)table);
		mask >>= 9;
		shift -= 9;
        }

	mask = (1 << shift) - 1;
	page_frame = ((uintptr_t)table & ~mask);
	return (page_frame | (gla & mask));
}

static void *
gla2hla(uint64_t gla, uint64_t guest_cr3)
{
	uintptr_t gpa;

	gpa = gla2gpa(gla, guest_cr3);
	return paddr_guest2host(gpa);
}

/*
 * Decodes all of the prefixes of the instruction. Only a subset of REX 
 * prefixes are currently supported. If any unsupported prefix is 
 * encountered, returns -1.
 */
static int 
decode_prefixes(struct decoded_instruction *decoded)
{
	uint8_t *current_prefix;

	current_prefix = decoded->instruction;

	if (is_rex_prefix(*current_prefix)) {
		decoded->rex_w = *current_prefix & REX_W_MASK;
		decoded->rex_r = *current_prefix & REX_R_MASK;
		decoded->rex_x = *current_prefix & REX_X_MASK;
		decoded->rex_b = *current_prefix & REX_B_MASK;
		current_prefix++;
	} else if (is_prefix(*current_prefix)) {
		return (-1);
	}

	decoded->opcode = current_prefix;
	return (0);
}

/*
 * Decodes the instruction's opcode. If the opcode is not understood, returns
 * -1 indicating an error. Sets the instruction's mod_rm pointer to the 
 * location of the ModR/M field.
 */
static int 
decode_opcode(struct decoded_instruction *decoded)
{
	uint8_t opcode, flags;

	opcode = *decoded->opcode;
	flags = one_byte_opcodes[opcode];

	if (!flags) 
		return (-1);

	if (flags & HAS_MODRM) {
		decoded->modrm = decoded->opcode + 1;
	}

	decoded->opcode_flags = flags;

	return (0);
}

/*
 * Decodes the instruction's ModR/M field. Sets the instruction's sib pointer
 * to the location of the SIB if one is expected to be present, or 0 if not.
 */
static int 
decode_mod_rm(struct decoded_instruction *decoded)
{
	uint8_t modrm;
	uint8_t *extension_operands;

	if (decoded->modrm) {
		modrm = *decoded->modrm;
	
		decoded->addressing_mode = (modrm & MODRM_MOD_MASK) >> MODRM_MOD_SHIFT;
		decoded->rm = (modrm & MODRM_RM_MASK) >> MODRM_RM_SHIFT;
		decoded->reg = (modrm & MODRM_REG_MASK) >> MODRM_REG_SHIFT;

		if (decoded->rex_b) 
			decoded->rm |= (1<<3);

		if (decoded->rex_r) 
			decoded->reg |= (1<<3);

		extension_operands = decoded->modrm + 1;
	
		if (decoded->rm == RM_SIB) {
			decoded->sib = decoded->modrm + 1;
			extension_operands = decoded->sib + 1;
		}

		switch (decoded->addressing_mode) {
		case MOD_INDIRECT:
		case MOD_DIRECT:
			decoded->displacement = 0;
			break;
		case MOD_INDIRECT_DISP8:
			decoded->displacement = extension_operands;
			break;
		case MOD_INDIRECT_DISP32:
			decoded->displacement = extension_operands;
			break;
		}
	}

	return (0);
}

/*
 * Decodes the instruction's SIB field. No such instructions are currently
 * supported, so do nothing and return -1 if there is a SIB field, 0 otherwise.
 */
static int
decode_sib(struct decoded_instruction *decoded)
{

	if (decoded->sib) 
		return (-1);

	return (0);
}

/*
 * Grabs and saves the instruction's immediate operand and displacement if
 * they are present. Immediates are not currently supported, so if an 
 * immediate is present it will return -1 indicating an error.
 */
static int
decode_extension_operands(struct decoded_instruction *decoded)
{

	if (decoded->displacement) {
		if (decoded->addressing_mode == MOD_INDIRECT_DISP8) {
			decoded->disp = *((int8_t *)decoded->displacement);
		} else if (decoded->addressing_mode == MOD_INDIRECT_DISP32) {
			decoded->disp = *((int32_t *)decoded->displacement);
		}
	}

	if (decoded->immediate) {
		return (-1);
	}

	return (0);
}

static int
decode_instruction(void *instr, struct decoded_instruction *decoded)
{
	int error;

	bzero(decoded, sizeof(*decoded));
	decoded->instruction = instr;

	error = decode_prefixes(decoded);
	if (error)
                return (error);

	error = decode_opcode(decoded);
	if (error) 
		return (error);

	error = decode_mod_rm(decoded);
	if (error)
		return (error);

	error = decode_sib(decoded);
	if (error) 
		return (error);

	error = decode_extension_operands(decoded);
	if (error) 
		return (error);

	return (0);
}

static struct memory_region * 
find_region(uintptr_t addr)
{
	int i;

	for (i = 0; i < registered_regions; ++i) {
		if (emulated_regions[i].start <= addr && 
		   emulated_regions[i].end >= addr) {
			return &emulated_regions[i];
		}
	}

	return (0);
}

static enum vm_reg_name
get_vm_reg_name(uint8_t reg)
{
	return vm_reg_name_mappings[reg];
}

static int 
get_operand(struct vmctx *vm, int vcpu, uint64_t guest_cr3,
	    const struct decoded_instruction *instruction, uint64_t *operand)
{
	enum vm_reg_name regname;
	uint64_t reg;
	uintptr_t target;
	int error;
	uint8_t rm, addressing_mode;
	struct memory_region *emulated_memory;

	if (instruction->opcode_flags & FROM_RM) {
		rm = instruction->rm;
		addressing_mode = instruction->addressing_mode;
	} else if (instruction->opcode_flags & FROM_REG) {
		rm = instruction->reg;
		addressing_mode = MOD_DIRECT;
	} else 
		return (-1);

	regname = get_vm_reg_name(rm);
	error = vm_get_register(vm, vcpu, regname, &reg);
	if (error) 
		return (error);

	switch (addressing_mode) {
	case MOD_DIRECT:
		*operand = reg;
		return (0);
	case MOD_INDIRECT:
	case MOD_INDIRECT_DISP8:
	case MOD_INDIRECT_DISP32:
		target = gla2gpa(reg, guest_cr3);
		target += instruction->disp;
		emulated_memory = find_region(target);
		if (emulated_memory) {
			return emulated_memory->memread(vm, vcpu, target, 
							4, operand, 
							emulated_memory->arg);
		}
                return (-1);
	default:
		return (-1);
	}
}

static int 
perform_write(struct vmctx *vm, int vcpu, uint64_t guest_cr3,
	      const struct decoded_instruction *instruction, uint64_t operand)
{
	enum vm_reg_name regname;
	uintptr_t target;
	int error;
	uint64_t reg;
	struct memory_region *emulated_memory;
	uint8_t addressing_mode;

	if (instruction->opcode_flags & TO_RM) {
		reg = instruction->rm;
		addressing_mode = instruction->addressing_mode;
	} else if (instruction->opcode_flags & TO_REG) {
		reg = instruction->reg;
		addressing_mode = MOD_DIRECT;
	} else
		return (-1);

	regname = get_vm_reg_name(reg);
	error = vm_get_register(vm, vcpu, regname, &reg);
	if (error)
		return (error);

	switch(addressing_mode) {
	case MOD_DIRECT:
		return vm_set_register(vm, vcpu, regname, operand);
	case MOD_INDIRECT:
	case MOD_INDIRECT_DISP8:
	case MOD_INDIRECT_DISP32:
		target = gla2gpa(reg, guest_cr3);
		target += instruction->disp;
		emulated_memory = find_region(target);
		if (emulated_memory) {
			return emulated_memory->memwrite(vm, vcpu, target, 
							 4, operand, 
							 emulated_memory->arg);
		}
		return (-1);
	default:
		return (-1);
	}
}

static int 
emulate_decoded_instruction(struct vmctx *vm, int vcpu, uint64_t cr3,
			    const struct decoded_instruction *instruction)
{
	uint64_t operand;
	int error;

	error = get_operand(vm, vcpu, cr3, instruction, &operand);
	if (error)
		return (error);

	return perform_write(vm, vcpu, cr3, instruction, operand);
}

int 
emulate_instruction(struct vmctx *vm, int vcpu, uint64_t rip, uint64_t cr3)
{
	struct decoded_instruction instr;
	int error;
	void *instruction = gla2hla(rip, cr3);

	if ((error = decode_instruction(instruction, &instr)) != 0)
		return (error);

	return emulate_decoded_instruction(vm, vcpu, cr3, &instr);
}

struct memory_region *
register_emulated_memory(uintptr_t start, size_t len, emulated_read_func_t memread,
			 emulated_write_func_t memwrite, void *arg)
{
	if (registered_regions >= MAX_EMULATED_REGIONS) 
		return (NULL);

	struct memory_region *region = &emulated_regions[registered_regions];
	region->start = start;
	region->end = start + len;
	region->memread = memread;
	region->memwrite = memwrite;
	region->arg = arg;

	registered_regions++;
	return (region);
}

void 
move_memory_region(struct memory_region *region, uintptr_t start)
{
	size_t len;

	len = region->end - region->start;
	region->start = start;
	region->end = start + len;
}
