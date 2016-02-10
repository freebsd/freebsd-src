/*-
 * Copyright (c) 2015 Serge Vakulenko
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
 */

#include "ld.h"
#include "ld_arch.h"
#include "ld_dynamic.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_symbols.h"
#include "ld_utils.h"
#include "mips.h"

#define EF_MIPS_ABI2		0x00000020	/* n32 abi */
#define EF_MIPS_ABI		0x00007000	/* Application binary interface */
#define E_MIPS_ABI_O32		0x00001000	/* MIPS 32 bit ABI (UCODE) */
#define E_MIPS_ABI_O64		0x00002000	/* UCODE MIPS 64 bit ABI */
#define E_MIPS_ABI_EABI32	0x00003000	/* Embedded ABI for 32-bit */
#define E_MIPS_ABI_EABI64	0x00004000	/* Embedded ABI for 64-bit */

#define EF_MIPS_ASE_MDMX	0x08000000	/* MDMX multimedia extensions */
#define EF_MIPS_ASE_M16		0x04000000	/* MIPS16e ISA extensions */
#define EF_MIPS_ASE_MICROMIPS	0x02000000	/* MicroMIPS architecture */

#define EF_MIPS_ARCH_1		0x00000000	/* MIPS I instruction set */
#define EF_MIPS_ARCH_2		0x10000000	/* MIPS II instruction set */
#define EF_MIPS_ARCH_3		0x20000000	/* MIPS III instruction set */
#define EF_MIPS_ARCH_4		0x30000000	/* MIPS IV instruction set */
#define EF_MIPS_ARCH_5		0x40000000	/* Never introduced */
#define EF_MIPS_ARCH_32		0x50000000	/* Mips32 Revision 1 */
#define EF_MIPS_ARCH_64		0x60000000	/* Mips64 Revision 1 */
#define EF_MIPS_ARCH_32R2	0x70000000	/* Mips32 Revision 2 */
#define EF_MIPS_ARCH_64R2	0x80000000	/* Mips64 Revision 2 */

ELFTC_VCSID("$Id$");

static void
_scan_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre)
{

	(void) is;

	switch (lre->lre_type) {

	case R_MIPS_NONE:
	case R_MIPS_32:
	case R_MIPS_26:
	case R_MIPS_PC16:
	case R_MIPS_GPREL16:
	case R_MIPS_HI16:
	case R_MIPS_LO16:
		break;

	default:
		ld_warn(ld, "can not handle relocation %ju",
		    lre->lre_type);
		break;
	}
}

static void
_process_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf)
{
	struct ld_output *lo = ld->ld_output;
	uint32_t pc, s;
	int32_t a, v, la;
	static uint64_t gp;
	static char gp_name[] = "_gp";

	assert(lo != NULL);

	pc = lre->lre_offset + is->is_output->os_addr + is->is_reloff;
	s = (uint32_t) lsb->lsb_value;
	READ_32(buf + lre->lre_offset, a);

	switch (lre->lre_type) {

	case R_MIPS_NONE:
		break;

	case R_MIPS_32:
		/* 32-bit byte address. */
		v = s + a;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_MIPS_26:
		/* Word address at lower 26 bits. */
		s += (a & 0x3ffffff) << 2;
		v = (a & ~0x3ffffff) | ((s >> 2) & 0x3ffffff);
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_MIPS_PC16:
		/* PC-relative word address at lower 16 bits. */
		s += ((a & 0xffff) << 2) - pc;
		v = (a & ~0xffff) | ((s >> 2) & 0xffff);
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_MIPS_GPREL16:
		/* GP-relative byte address at lower 16 bits. */
		if (! gp && ld_symbols_get_value(ld, gp_name, &gp) < 0)
			ld_fatal(ld, "symbol _gp is undefined");

		s += (int16_t)(a & 0xffff) - gp;
		v = (a & ~0xffff) | (s & 0xffff);
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_MIPS_HI16:
		/* 16-bit high part of address pair. */
		if (! STAILQ_NEXT(lre, lre_next) ||
		    STAILQ_NEXT(lre, lre_next)->lre_type != R_MIPS_LO16)
			ld_fatal(ld, "no LO16 after HI16 relocation");
		READ_32(buf + STAILQ_NEXT(lre, lre_next)->lre_offset, la);
		s += (a << 16) + (int16_t)la;
		v = (a & ~0xffff) | (((s - (int16_t)s) >> 16) & 0xffff);
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_MIPS_LO16:
		/* 16-bit low part of address pair. */
		s += (int16_t)a;
		v = (a & ~0xffff) | (s & 0xffff);
		WRITE_32(buf + lre->lre_offset, v);
		break;

	default:
		ld_fatal(ld, "Relocation %d not supported", lre->lre_type);
		break;
	}
}

/*
 * Map flags into a valid MIPS architecture level value.
 */
static unsigned
_map_arch(unsigned flags)
{
	flags &= EF_MIPS_ARCH;

	switch (flags) {
	default:
	case EF_MIPS_ARCH_1:
		return EF_MIPS_ARCH_1;
	case EF_MIPS_ARCH_2:
	case EF_MIPS_ARCH_3:
	case EF_MIPS_ARCH_4:
	case EF_MIPS_ARCH_5:
	case EF_MIPS_ARCH_32:
	case EF_MIPS_ARCH_64:
	case EF_MIPS_ARCH_32R2:
	case EF_MIPS_ARCH_64R2:
		return flags;
	}
}

/*
 * Merge architecture levels of two files.
 */
static unsigned
_merge_arch(unsigned old_arch, unsigned new_arch)
{
	unsigned base, extended;

	if (old_arch < new_arch) {
		base = old_arch;
		extended = new_arch;
	} else if (old_arch > new_arch) {
		base = new_arch;
		extended = old_arch;
	} else
		return old_arch;

	switch (extended) {
	default:
	case EF_MIPS_ARCH_1:
	case EF_MIPS_ARCH_2:
	case EF_MIPS_ARCH_3:
	case EF_MIPS_ARCH_4:
	case EF_MIPS_ARCH_5:
		return extended;

	case EF_MIPS_ARCH_32:
		if (base <= EF_MIPS_ARCH_2)
			return EF_MIPS_ARCH_32;
		return EF_MIPS_ARCH_64;

	case EF_MIPS_ARCH_64:
		return EF_MIPS_ARCH_64;

	case EF_MIPS_ARCH_32R2:
		if (base <= EF_MIPS_ARCH_2 || base == EF_MIPS_ARCH_32)
			return EF_MIPS_ARCH_32R2;
		return EF_MIPS_ARCH_64R2;

	case EF_MIPS_ARCH_64R2:
		return EF_MIPS_ARCH_64R2;
	}
}

static const char*
_abi_name(int flags)
{
	switch (flags & EF_MIPS_ABI) {
	case 0:
		return (flags & EF_MIPS_ABI2) ? "N32" : "none";
	case E_MIPS_ABI_O32:
		return "O32";
	case E_MIPS_ABI_O64:
		return "O64";
	case E_MIPS_ABI_EABI32:
		return "EABI32";
	case E_MIPS_ABI_EABI64:
		return "EABI64";
	default:
		return "Unknown";
    }
}

/*
 * Merge options of application binary interface.
 */
static unsigned
_merge_abi(struct ld *ld, unsigned new_flags)
{
	int old = ld->ld_arch->flags & EF_MIPS_ABI;
	int new = new_flags & EF_MIPS_ABI;

	if (old == 0)
		return new;

	if (new != old && new != 0)
		ld_fatal(ld, "ABI mismatch: linking '%s' module with previous '%s' modules",
			_abi_name(new_flags), _abi_name(ld->ld_arch->flags));

	return old;
}

/*
 * Merge options of application-specific extensions.
 */
static unsigned
_merge_ase(struct ld *ld, unsigned new_flags)
{
	int old_micro = ld->ld_arch->flags & EF_MIPS_ASE_MICROMIPS;
	int new_micro = new_flags & EF_MIPS_ASE_MICROMIPS;
	int old_m16 = ld->ld_arch->flags & EF_MIPS_ASE_M16;
	int new_m16 = new_flags & EF_MIPS_ASE_M16;

	if ((old_m16 && new_micro) || (old_micro && new_m16))
		ld_fatal(ld, "ASE mismatch: linking '%s' module with previous '%s' modules",
			new_m16 ? "MIPS16" : "microMIPS",
			old_micro ? "microMIPS" : "MIPS16");
	return old_micro | new_micro | old_m16 | new_m16;
}

/*
 * Merge architecture-specific flags of the file to be linked
 * into a resulting value for output file.
 */
static void
_merge_flags(struct ld *ld, unsigned new_flags)
{
	struct ld_arch *la = ld->ld_arch;
	unsigned value;

	/* At least one .noreorder directive appeared in the source. */
	la->flags |= new_flags & EF_MIPS_NOREORDER;

	/* Merge position-independent flags. */
	if (((new_flags & (EF_MIPS_PIC | EF_MIPS_CPIC)) != 0) !=
	    ((la->flags & (EF_MIPS_PIC | EF_MIPS_CPIC)) != 0))
		ld_warn(ld, "linking PIC files with non-PIC files");
	if (new_flags & (EF_MIPS_PIC | EF_MIPS_CPIC))
		la->flags |= EF_MIPS_CPIC;
	if (! (new_flags & EF_MIPS_PIC))
		la->flags &= ~EF_MIPS_PIC;

	/* Merge architecture level. */
	value = _merge_arch(_map_arch(la->flags), _map_arch(new_flags));
	la->flags &= ~EF_MIPS_ARCH;
	la->flags |= value;

	/* Merge ABI options. */
	value = _merge_abi(ld, new_flags);
	la->flags &= ~EF_MIPS_ABI;
	la->flags |= value;

	/* Merge application-specific extensions. */
	value = _merge_ase(ld, new_flags);
	la->flags &= ~EF_MIPS_ARCH_ASE;
	la->flags |= value;
}

static uint64_t
_get_max_page_size(struct ld *ld)
{

	(void) ld;

	return 0x1000;
}

static uint64_t
_get_common_page_size(struct ld *ld)
{

	(void) ld;

	return 0x1000;
}

static int
_is_absolute_reloc(uint64_t r)
{
	if (r == R_MIPS_32)
		return 1;

	return 0;
}

static int
_is_relative_reloc(uint64_t r)
{
	if (r == R_MIPS_REL32)
		return 1;

	return 0;
}

void
mips_register(struct ld *ld)
{
	struct ld_arch *mips_little_endian, *mips_big_endian;

	if ((mips_little_endian = calloc(1, sizeof(*mips_little_endian))) == NULL)
		ld_fatal_std(ld, "calloc");
	if ((mips_big_endian = calloc(1, sizeof(*mips_big_endian))) == NULL)
		ld_fatal_std(ld, "calloc");

	/*
	 * Little endian.
	 */
	snprintf(mips_little_endian->name, sizeof(mips_little_endian->name), "%s", "littlemips");

	mips_little_endian->script = littlemips_script;
	mips_little_endian->get_max_page_size = _get_max_page_size;
	mips_little_endian->get_common_page_size = _get_common_page_size;
	mips_little_endian->scan_reloc = _scan_reloc;
	mips_little_endian->process_reloc = _process_reloc;
	mips_little_endian->is_absolute_reloc = _is_absolute_reloc;
	mips_little_endian->is_relative_reloc = _is_relative_reloc;
	mips_little_endian->merge_flags = _merge_flags;
	mips_little_endian->reloc_is_64bit = 0;
	mips_little_endian->reloc_is_rela = 0;
	mips_little_endian->reloc_entsize = sizeof(Elf32_Rel);

	/*
	 * Big endian.
	 */
	snprintf(mips_big_endian->name, sizeof(mips_big_endian->name), "%s", "bigmips");

	mips_big_endian->script = bigmips_script;
	mips_big_endian->get_max_page_size = _get_max_page_size;
	mips_big_endian->get_common_page_size = _get_common_page_size;
	mips_big_endian->scan_reloc = _scan_reloc;
	mips_big_endian->process_reloc = _process_reloc;
	mips_big_endian->is_absolute_reloc = _is_absolute_reloc;
	mips_big_endian->is_relative_reloc = _is_relative_reloc;
	mips_little_endian->merge_flags = _merge_flags;
	mips_big_endian->reloc_is_64bit = 0;
	mips_big_endian->reloc_is_rela = 0;
	mips_big_endian->reloc_entsize = sizeof(Elf32_Rel);

	HASH_ADD_STR(ld->ld_arch_list, name, mips_little_endian);
	HASH_ADD_STR(ld->ld_arch_list, name, mips_big_endian);
}
