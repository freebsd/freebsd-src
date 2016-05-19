/*-
 * Copyright (c) 2012,2013 Kai Wang
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
 */

#include "ld.h"
#include "ld_arch.h"
#include "ld_dynamic.h"
#include "ld_input.h"
#include "ld_layout.h"
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_symbols.h"
#include "ld_utils.h"
#include "amd64.h"

ELFTC_VCSID("$Id: amd64.c 3419 2016-02-19 20:07:15Z emaste $");

static void _create_plt_reloc(struct ld *ld, struct ld_symbol *lsb,
    uint64_t offset);
static void _create_got_reloc(struct ld *ld, struct ld_symbol *lsb,
    uint64_t type, uint64_t offset);
static void _create_copy_reloc(struct ld *ld, struct ld_symbol *lsb);
static void _create_dynamic_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_symbol *lsb, uint64_t type, uint64_t offset, int64_t addend);
static void _scan_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre);
static struct ld_input_section *_find_and_create_got_section(struct ld *ld,
    int create);
static struct ld_input_section *_find_and_create_gotplt_section(struct ld *ld,
    int create);
static struct ld_input_section *_find_and_create_plt_section(struct ld *ld,
    int create);
static void _finalize_got_and_plt(struct ld *ld);
static uint64_t _get_max_page_size(struct ld *ld);
static uint64_t _get_common_page_size(struct ld *ld);
static void _adjust_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf);
static void _process_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf);
static void _reserve_got_entry(struct ld *ld, struct ld_symbol *lsb, int num);
static void _reserve_gotplt_entry(struct ld *ld, struct ld_symbol *lsb);
static void _reserve_plt_entry(struct ld *ld, struct ld_symbol *lsb);
static int _is_absolute_reloc(uint64_t r);
static void _warn_pic(struct ld *ld, struct ld_reloc_entry *lre);
static void _create_tls_gd_reloc(struct ld *ld, struct ld_symbol *lsb);
static void _create_tls_ld_reloc(struct ld *ld, struct ld_symbol *lsb);
static void _create_tls_ie_reloc(struct ld *ld, struct ld_symbol *lsb);
static enum ld_tls_relax _tls_check_relax(struct ld *ld,
    struct ld_reloc_entry *lre);
static uint64_t _got_offset(struct ld *ld, struct ld_symbol *lsb);
static int _tls_verify_gd(uint8_t *buf, uint64_t off);
static int _tls_verify_ld(uint8_t *buf, uint64_t off);
static void _tls_relax_gd_to_ie(struct ld *ld, struct ld_state *ls,
    struct ld_output *lo,struct ld_reloc_entry *lre, uint64_t p, uint64_t g,
    uint8_t *buf);
static void _tls_relax_gd_to_le(struct ld *ld, struct ld_state *ls,
    struct ld_output *lo, struct ld_reloc_entry *lre, struct ld_symbol *lsb,
    uint8_t *buf);
static void _tls_relax_ld_to_le(struct ld *ld, struct ld_state *ls,
    struct ld_reloc_entry *lre, uint8_t *buf);
static void _tls_relax_ie_to_le(struct ld *ld, struct ld_output *lo,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf);
static int32_t _tls_dtpoff(struct ld_output *lo, struct ld_symbol *lsb);
static int32_t _tls_tpoff(struct ld_output *lo, struct ld_symbol *lsb);

static uint64_t
_get_max_page_size(struct ld *ld)
{

	(void) ld;
	return (0x200000);
}

static uint64_t
_get_common_page_size(struct ld *ld)
{

	(void) ld;
	return (0x1000);
}

static int
_is_absolute_reloc(uint64_t r)
{

	if (r == R_X86_64_64 || r == R_X86_64_32 || r == R_X86_64_32S ||
	    r == R_X86_64_16 || r == R_X86_64_8)
		return (1);

	return (0);
}

static int
_is_relative_reloc(uint64_t r)
{

	if (r == R_X86_64_RELATIVE)
		return (1);

	return (0);
}

static void
_warn_pic(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = lre->lre_sym;

	if (lsb->lsb_bind != STB_LOCAL)
		ld_warn(ld, "relocation %s against `%s' can not be used"
		    " by runtime linker; recompile with -fPIC",
		    elftc_reloc_type_str(EM_X86_64,
		    lre->lre_type), lsb->lsb_name);
	else
		ld_warn(ld, "relocation %s can not be used by runtime linker;"
		    " recompile with -fPIC", elftc_reloc_type_str(EM_X86_64,
		    lre->lre_type));
}

static struct ld_input_section *
_find_and_create_got_section(struct ld *ld, int create)
{
	struct ld_input_section *is;

	/* Check if the GOT section is already created. */
	is = ld_input_find_internal_section(ld, ".got");
	if (is != NULL)
		return (is);

	if (create) {
		is = ld_input_add_internal_section(ld, ".got");
		is->is_entsize = 8;
		is->is_align = 8;
		is->is_type = SHT_PROGBITS;
		is->is_flags = SHF_ALLOC | SHF_WRITE;
	}

	return (is);
}

static struct ld_input_section *
_find_and_create_gotplt_section(struct ld *ld, int create)
{
	struct ld_input_section *is;

	/* Check if the GOT (for PLT) section is already created. */
	is = ld_input_find_internal_section(ld, ".got.plt");
	if (is != NULL)
		return (is);

	if (create) {
		is = ld_input_add_internal_section(ld, ".got.plt");
		is->is_entsize = 8;
		is->is_align = 8;
		is->is_type = SHT_PROGBITS;
		is->is_flags = SHF_ALLOC | SHF_WRITE;

		/* Reserve space for the initial entries. */
		(void) ld_input_reserve_ibuf(is, 3);

		/* Create _GLOBAL_OFFSET_TABLE_ symbol. */
		ld_symbols_add_internal(ld, "_GLOBAL_OFFSET_TABLE_", 0, 0,
		    is->is_index, STB_LOCAL, STT_OBJECT, STV_HIDDEN, is, NULL);
	}

	return (is);
}

static struct ld_input_section *
_find_and_create_plt_section(struct ld *ld, int create)
{
	struct ld_input_section *is;

	/* Check if the PLT section is already created. */
	is = ld_input_find_internal_section(ld, ".plt");
	if (is != NULL)
		return (is);

	if (create) {
		is = ld_input_add_internal_section(ld, ".plt");
		is->is_entsize = 16;
		is->is_align = 4;
		is->is_type = SHT_PROGBITS;
		is->is_flags = SHF_ALLOC | SHF_EXECINSTR;

		/* Reserve space for the initial entry. */
		(void) ld_input_reserve_ibuf(is, 1);
	}

	return (is);
}

static void
_reserve_got_entry(struct ld *ld, struct ld_symbol *lsb, int num)
{
	struct ld_input_section *is;

	is = _find_and_create_got_section(ld, 1);

	/* Check if the entry already has a GOT entry. */
	if (lsb->lsb_got)
		return;

	/* Reserve GOT entries. */
	lsb->lsb_got_off = ld_input_reserve_ibuf(is, num);
	lsb->lsb_got = 1;
}

static void
_reserve_gotplt_entry(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input_section *is;

	is = _find_and_create_gotplt_section(ld, 1);

	/* Reserve a GOT entry for PLT. */
	(void) ld_input_reserve_ibuf(is, 1);

	/*
	 * Record a R_X86_64_JUMP_SLOT entry for this symbol. Note that
	 * we don't need to record the offset (relative to the GOT section)
	 * here, since the PLT relocations will be sorted later and we
	 * will generate GOT section according to the new order.
	 */
	_create_plt_reloc(ld, lsb, 0);
}

static void
_reserve_plt_entry(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input_section *is;

	is = _find_and_create_plt_section(ld, 1);

	(void) ld_input_reserve_ibuf(is, 1);
	lsb->lsb_plt = 1;
}

static void
_create_plt_reloc(struct ld *ld, struct ld_symbol *lsb, uint64_t offset)
{

	ld_reloc_create_entry(ld, ".rela.plt", NULL, R_X86_64_JUMP_SLOT,
	    lsb, offset, 0);

	lsb->lsb_dynrel = 1;
}

static void
_create_got_reloc(struct ld *ld, struct ld_symbol *lsb, uint64_t type,
    uint64_t offset)
{
	struct ld_input_section *tis;

	tis = _find_and_create_got_section(ld, 0);
	assert(tis != NULL);

	ld_reloc_create_entry(ld, ".rela.got", tis, type, lsb, offset, 0);

	if (type != R_X86_64_RELATIVE)
		lsb->lsb_dynrel = 1;
}

static void
_create_copy_reloc(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input_section *tis;

	ld_dynamic_reserve_dynbss_entry(ld, lsb);

	tis = ld_input_find_internal_section(ld, ".dynbss");
	assert(tis != NULL);

	ld_reloc_create_entry(ld, ".rela.bss", tis, R_X86_64_COPY, lsb,
	    lsb->lsb_value, 0);

	lsb->lsb_dynrel = 1;
}

static void
_create_dynamic_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_symbol *lsb, uint64_t type, uint64_t offset, int64_t addend)
{

	if (lsb->lsb_bind == STB_LOCAL) {
		if (is->is_flags & SHF_WRITE)
			ld_reloc_create_entry(ld, ".rela.data.rel.local",
			    is, type, lsb, offset, addend);
		else
			ld_reloc_create_entry(ld, ".rela.data.rel.ro.local",
			    is, type, lsb, offset, addend);
	} else {
		if (is->is_flags & SHF_WRITE)
			ld_reloc_create_entry(ld, ".rela.data.rel",
			    is, type, lsb, offset, addend);
		else
			ld_reloc_create_entry(ld, ".rela.data.rel.ro",
			    is, type, lsb, offset, addend);
	}

	if (type != R_X86_64_RELATIVE)
		lsb->lsb_dynrel = 1;
}

static void
_finalize_reloc(struct ld *ld, struct ld_input_section *tis,
    struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	(void) ld;
	(void) tis;

	lsb = ld_symbols_ref(lre->lre_sym);

	switch (lre->lre_type) {
	case R_X86_64_RELATIVE:
		/*
		 * Update the addend stored in the original relocation to
		 * point to the new location, by adding the updated symbol
		 * value.
		 */
		lre->lre_addend += lsb->lsb_value;

		/* R_X86_64_RELATIVE should not associate with a symbol. */
		lre->lre_sym = NULL;
		break;

	case R_X86_64_DTPMOD64:
		/*
		 * Relocation R_X86_64_DTPMOD64 generated for local dynamic
		 * TLS model should not assoicate with a symbol.
		 */
		if (lre->lre_type == R_X86_64_DTPMOD64 &&
		    lsb->lsb_tls_ld)
			lre->lre_sym = NULL;
		break;

	default:
		break;
	}
}

static void
_finalize_got_and_plt(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_input_section *got_is, *rela_got_is, *plt_is, *rela_plt_is;
	struct ld_output_section *got_os, *plt_os, *rela_plt_os;
	struct ld_reloc_entry *lre;
	struct ld_symbol *lsb;
	char dynamic_symbol[] = "_DYNAMIC";
	uint8_t *got, *plt;
	uint64_t u64;
	int32_t s32, pltgot, gotpcrel;
	int i, j;

	lo = ld->ld_output;
	assert(lo != NULL);

	/*
	 * Intiailze all .got section entries to zero.
	 */
	got_is = _find_and_create_got_section(ld, 0);
	if (got_is != NULL)
		memset(got_is->is_ibuf, 0, got_is->is_size);

	/*
	 * Search for GOT relocations that requires filling in symbol
	 * value.
	 */
	rela_got_is = ld_input_find_internal_section(ld, ".rela.got");
	if (rela_got_is != NULL && rela_got_is->is_reloc != NULL) {
		STAILQ_FOREACH(lre, rela_got_is->is_reloc, lre_next) {
			if (lre->lre_type == R_X86_64_RELATIVE) {
				lsb = lre->lre_sym;
				got = (uint8_t *) got_is->is_ibuf +
				    lsb->lsb_got_off;
				WRITE_64(got, lsb->lsb_value);
			}
		}
	}

	/*
	 * Find the .plt section. The buffers should have been allocated
	 * at this point.
	 */
	plt_is = _find_and_create_plt_section(ld, 0);
	if (plt_is == NULL)
		return;
	plt_os = plt_is->is_output;
	plt = plt_is->is_ibuf;
	assert(plt != NULL);

	/*
	 * Find the .got.plt and .rela.plt section. If the .plt section
	 * exists, the .got.plt and .rela.plt section should exist too.
	 */
	got_is = _find_and_create_gotplt_section(ld, 0);
	assert(got_is != NULL);
	got_os = got_is->is_output;
	lo->lo_gotplt = got_os;
	got = got_is->is_ibuf;
	assert(got != NULL);
	rela_plt_is = ld_input_find_internal_section(ld, ".rela.plt");
	assert(rela_plt_is != NULL);
	rela_plt_os = rela_plt_is->is_output;
	lo->lo_rel_plt = rela_plt_os;

	/* Point sh_info field of the .rela.plt to .plt section. */
	rela_plt_os->os_info = plt_os;

	/* Fill in the value of symbol _DYNAMIC in the first GOT entry. */
	ld_symbols_get_value(ld, dynamic_symbol, &u64);
	WRITE_64(got, u64);
	got += 8;

	/* Reserve the second and the third entry for the dynamic linker. */
	memset(got, 0, 16);
	got += 16;

	/*
	 * Write the initial PLT entry.
	 */

	/* Calculate the relative offset from PLT to GOT. */
	pltgot = got_os->os_addr - plt_os->os_addr;

	/*
	 * Push the second GOT entry to the stack for the dynamic
	 * linker. (PUSH reg/memXX [RIP+disp32]) (6 bytes for push)
	 */
	WRITE_8(plt, 0xff);
	WRITE_8(plt + 1, 0x35);
	s32 = pltgot - 6 + 8;
	WRITE_32(plt + 2, s32);
	plt += 6;

	/*
	 * Jump to the address in the third GOT entry (call into
	 * the dynamic linker). (JMP reg/memXX [RIP+disp32])
	 * (6 bytes for jmp)
	 */
	WRITE_8(plt, 0xff);
	WRITE_8(plt + 1, 0x25);
	s32 = pltgot - 12 + 16;
	WRITE_32(plt + 2, s32);
	plt += 6;

	/* Padding: 4-byte nop. (NOP [rAx+disp8]) */
	WRITE_8(plt, 0x0f);
	WRITE_8(plt + 1, 0x1f);
	WRITE_8(plt + 2, 0x40);
	WRITE_8(plt + 3, 0x0);
	plt += 4;

	/*
	 * Walk through the sorted PLT relocations in the output section
	 * and fill in each GOT and PLT entries.
	 */
	i = 3;
	j = 0;
	STAILQ_FOREACH(lre, rela_plt_is->is_reloc, lre_next) {
		lsb = ld_symbols_ref(lre->lre_sym);

		/*
		 * Set symbol's PLT offset to the address of this PLT entry.
		 * The PLT offset is used in relocation processing later.
		 */
		lsb->lsb_plt_off = plt_os->os_addr + (i - 2) * 16;

		/*
		 * Update the offset for the R_X86_64_JUMP_SLOT relocation
		 * entry, pointing to the corresponding GOT entry.
		 */
		lre->lre_offset = got_os->os_addr + i * 8;

		/*
		 * Calculate the IP-relative offset to the GOT entry for
		 * this function. (6 bytes for jmp)
		 */
		gotpcrel = pltgot + i * 8 - (i - 2) * 16 - 6;

		/*
		 * PLT: Jump to the address in the GOT entry for this
		 * function. (JMP reg/memXX [RIP+disp32])
		 */
		WRITE_8(plt, 0xff);
		WRITE_8(plt + 1, 0x25);
		WRITE_32(plt + 2, gotpcrel);
		plt += 6;

		/*
		 * PLT: Symbol is not resolved, push the relocation index to
		 * the stack. (PUSH imm32)
		 */
		WRITE_8(plt, 0x68);
		WRITE_32(plt + 1, j);
		plt += 5;

		/*
		 * PLT: Jump to the first PLT entry, eventually call the
		 * dynamic linker. (JMP rel32off)
		 */
		WRITE_8(plt, 0xe9);
		s32 = - (i - 1) * 16;
		WRITE_32(plt + 1, s32);
		plt += 5;

		/*
		 * GOT: Write the GOT entry for this function, pointing to
		 * the push op.
		 */
		u64 = plt_os->os_addr + (i - 2) * 16 + 6;
		WRITE_64(got, u64);

		/* Increase relocation entry index. */
		j++;

		/* Move to next GOT entry. */
		got += 8;
		i++;
	}

	assert(got == (uint8_t *) got_is->is_ibuf + got_is->is_size);
	assert(plt == (uint8_t *) plt_is->is_ibuf + plt_is->is_size);
}

static void
_scan_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;
	enum ld_tls_relax tr;

	lsb = ld_symbols_ref(lre->lre_sym);

	/*
	 * TODO: We do not yet support "Large Models" and relevant
	 * relocation types R_X86_64_GOT64, R_X86_64_GOTPCREL64,
	 * R_X86_64_GOTPC64, R_X86_64_GOTPLT64 and R_X86_64_PLTOFF64.
	 * Refer to AMD64 ELF ABI for details.
	 */

	switch (lre->lre_type) {
	case R_X86_64_NONE:
		break;

	case R_X86_64_64:
	case R_X86_64_32:
	case R_X86_64_32S:
	case R_X86_64_16:
	case R_X86_64_8:

		/*
		 * For a local symbol, if the linker output a PIE or DSO,
		 * we should generate a R_X86_64_RELATIVE reloc for
		 * R_X86_64_64. We don't know how to generate dynamic reloc
		 * for other reloc types since R_X86_64_RELATIVE is 64 bits.
		 * We can not use them directly either because FreeBSD rtld(1)
		 * (and probably glibc) doesn't accept absolute address
		 * reloction other than R_X86_64_64.
		 */
		if (lsb->lsb_bind == STB_LOCAL) {
			if (ld->ld_pie || ld->ld_dso) {
				if (lre->lre_type == R_X86_64_64)
					_create_dynamic_reloc(ld, is, lsb,
					    R_X86_64_RELATIVE, lre->lre_offset,
					    lre->lre_addend);
				else
					_warn_pic(ld, lre);
			}
			break;
		}

		/*
		 * For a global symbol, we probably need to generate PLT entry
		 * and/or a dynamic relocation.
		 *
		 * Note here, normally the compiler will generate a PC-relative
		 * relocation for function calls. However, if the code retrieve
		 * the address of a function and call it indirectly, assembler
		 * will generate absolute relocation instead. That's why we
		 * should check if we need to create a PLT entry here. Also, if
		 * we're going to create the PLT entry, we should also set the
		 * symbol value to the address of PLT entry just in case the
		 * function address is used to compare with other function
		 * addresses. (If PLT address is used, function will have
		 * unified address in the main executable and DSOs)
		 */
		if (ld_reloc_require_plt(ld, lre)) {
			if (!lsb->lsb_plt) {
				_reserve_gotplt_entry(ld, lsb);
				_reserve_plt_entry(ld, lsb);
			}
			/*
			 * Note here even if we have generated PLT for this
			 * function before, we still need to set this flag.
			 * It's possible that we first see the relative
			 * relocation then this absolute relocation, in
			 * other words, the same function can be called in
			 * different ways.
			 */
			lsb->lsb_func_addr = 1;
		}

		if (ld_reloc_require_copy_reloc(ld, lre) &&
		    !lsb->lsb_copy_reloc)
			_create_copy_reloc(ld, lsb);
		else if (ld_reloc_require_dynamic_reloc(ld, lre)) {
			/* We only support R_X86_64_64. (See above) */
			if (lre->lre_type != R_X86_64_64) {
				_warn_pic(ld, lre);
				break;
			}
			/*
			 * Check if we can relax R_X86_64_64 to
			 * R_X86_64_RELATIVE instead.
			 */
			if (ld_reloc_relative_relax(ld, lre))
				_create_dynamic_reloc(ld, is, lsb,
				    R_X86_64_RELATIVE, lre->lre_offset,
				    lre->lre_addend);
			else
				_create_dynamic_reloc(ld, is, lsb,
				    R_X86_64_64, lre->lre_offset,
				    lre->lre_addend);
		}

		break;

	case R_X86_64_PLT32:
		/*
		 * In some cases we don't really need to generate a PLT
		 * entry, then a R_X86_64_PLT32 relocation can be relaxed
		 * to a R_X86_64_PC32 relocation.
		 */

		if (lsb->lsb_bind == STB_LOCAL ||
		    !ld_reloc_require_plt(ld, lre)) {
			lre->lre_type = R_X86_64_PC32;
			break;
		}

		/*
		 * If linker outputs an normal executable and the symbol is
		 * defined but is not defined inside a DSO, we can generate
		 * a R_X86_64_PC32 relocation instead.
		 */
		if (ld->ld_exec && lsb->lsb_shndx != SHN_UNDEF &&
		    (lsb->lsb_input == NULL ||
		    lsb->lsb_input->li_type != LIT_DSO)) {
			lre->lre_type = R_X86_64_PC32;
			break;
		}

		/* Create an PLT entry otherwise. */
		if (!lsb->lsb_plt) {
			_reserve_gotplt_entry(ld, lsb);
			_reserve_plt_entry(ld, lsb);
		}
		break;

	case R_X86_64_PC64:
	case R_X86_64_PC32:
	case R_X86_64_PC16:
	case R_X86_64_PC8:

		/*
		 * When these relocations apply to a global symbol, we should
		 * check if we need to generate PLT entry and/or a dynamic
		 * relocation.
		 */
		if (lsb->lsb_bind != STB_LOCAL) {
			if (ld_reloc_require_plt(ld, lre) && !lsb->lsb_plt) {
				_reserve_gotplt_entry(ld, lsb);
				_reserve_plt_entry(ld, lsb);
			}

			if (ld_reloc_require_copy_reloc(ld, lre) &&
			    !lsb->lsb_copy_reloc)
				_create_copy_reloc(ld, lsb);
			else if (ld_reloc_require_dynamic_reloc(ld, lre)) {
				/*
				 * We can not generate dynamic relocation for
				 * these PC-relative relocation since they
				 * are probably not supported by the runtime
				 * linkers.
				 *
				 * Note: FreeBSD rtld(1) does support
				 * R_X86_64_PC32.
				 */
				_warn_pic(ld, lre);
			}
		}
		break;

	case R_X86_64_GOTOFF64:
	case R_X86_64_GOTPC32:
		/*
		 * These relocation types use GOT address as a base address
		 * and instruct the linker to build a GOT.
		 */
		(void) _find_and_create_got_section(ld, 1);
		break;

	case R_X86_64_GOT32:
	case R_X86_64_GOTPCREL:
		/*
		 * These relocation types instruct the linker to build a
		 * GOT and generate a GOT entry.
		 */
		if (!lsb->lsb_got) {
			_reserve_got_entry(ld, lsb, 1);
			/*
			 * TODO: For now we always create a R_X86_64_GLOB_DAT
			 * relocation for a GOT entry. There are cases that
			 * the symbol's address is known at link time and
			 * the GOT entry value can be filled in by the program
			 * linker instead.
			 */
			if (ld_reloc_require_glob_dat(ld, lre))
				_create_got_reloc(ld, lsb, R_X86_64_GLOB_DAT,
				    lsb->lsb_got_off);
			else
				_create_got_reloc(ld, lsb, R_X86_64_RELATIVE,
				    lsb->lsb_got_off);
		}
		break;

	case R_X86_64_TLSGD:	/* Global Dynamic */
		tr = _tls_check_relax(ld, lre);
		switch (tr) {
		case TLS_RELAX_NONE:
			_create_tls_gd_reloc(ld, lsb);
			break;
		case TLS_RELAX_INIT_EXEC:
			_create_tls_ie_reloc(ld, lsb);
			break;
		case TLS_RELAX_LOCAL_EXEC:
			break;
		default:
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
			break;
		}
		break;

	case R_X86_64_TLSLD:	/* Local Dynamic */
		tr = _tls_check_relax(ld, lre);
		if (tr == TLS_RELAX_NONE)
			_create_tls_ld_reloc(ld, lsb);
		else if (tr != TLS_RELAX_LOCAL_EXEC)
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
		break;

	case R_X86_64_DTPOFF32:
		/* Handled by R_X86_64_TLSLD case. */
		break;

	case R_X86_64_GOTTPOFF:	/* Initial Exec */
		tr = _tls_check_relax(ld, lre);
		if (tr == TLS_RELAX_NONE)
			_create_tls_ie_reloc(ld, lsb);
		else if (tr != TLS_RELAX_LOCAL_EXEC)
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
		break;

	case R_X86_64_TPOFF32:	/* Local Exec */
		/* No further relaxation possible. */
		break;

	case R_X86_64_GOTPC32_TLSDESC:
	case R_X86_64_TLSDESC_CALL:
		/* TODO. */
		break;

	default:
		ld_warn(ld, "can not handle relocation %ju",
		    lre->lre_type);
		break;
	}
}

static uint64_t
_got_offset(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_output_section *os;

	assert(lsb->lsb_got);

	if (ld->ld_got == NULL) {
		ld->ld_got = _find_and_create_got_section(ld, 0);
		assert(ld->ld_got != NULL);
	}

	os = ld->ld_got->is_output;

	return (os->os_addr + ld->ld_got->is_reloff + lsb->lsb_got_off);
}

static void
_process_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf)
{
	struct ld_state *ls;
	struct ld_output *lo;
	uint64_t u64, s, l, p, g;
	int64_t s64;
	uint32_t u32;
	int32_t s32;
	enum ld_tls_relax tr;

	ls = &ld->ld_state;

	lo = ld->ld_output;
	assert(lo != NULL);

	l = lsb->lsb_plt_off;
	p = lre->lre_offset + is->is_output->os_addr + is->is_reloff;
	s = lsb->lsb_value;

	switch (lre->lre_type) {
	case R_X86_64_NONE:
		break;

	case R_X86_64_64:
		WRITE_64(buf + lre->lre_offset, s + lre->lre_addend);
		break;

	case R_X86_64_PC32:
		if (lsb->lsb_plt)
			s32 = l + lre->lre_addend - p;
		else
			s32 = s + lre->lre_addend - p;
		WRITE_32(buf + lre->lre_offset, s32);
		break;

	case R_X86_64_PLT32:
		if (!ls->ls_ignore_next_plt) {
			s32 = l + lre->lre_addend - p;
			WRITE_32(buf + lre->lre_offset, s32);
		} else
			ls->ls_ignore_next_plt = 0;
		break;

	case R_X86_64_GOTPCREL:
		g = _got_offset(ld, lsb);
		s32 = g + lre->lre_addend - p;
		WRITE_32(buf + lre->lre_offset, s32);
		break;

	case R_X86_64_32:
		u64 = s + lre->lre_addend;
		u32 = u64 & 0xffffffff;
		if (u64 != u32)
			ld_fatal(ld, "R_X86_64_32 relocation failed");
		WRITE_32(buf + lre->lre_offset, u32);
		break;

	case R_X86_64_32S:
		s64 = s + lre->lre_addend;
		s32 = s64 & 0xffffffff;
		if (s64 != s32)
			ld_fatal(ld, "R_X86_64_32S relocation failed");
		WRITE_32(buf + lre->lre_offset, s32);
		break;

	case R_X86_64_TLSGD:	/* Global Dynamic */
		tr = _tls_check_relax(ld, lre);
		switch (tr) {
		case TLS_RELAX_NONE:
			g = _got_offset(ld, lsb);
			s32 = g + lre->lre_addend - p;
			WRITE_32(buf + lre->lre_offset, s32);
			break;
		case TLS_RELAX_INIT_EXEC:
			g = _got_offset(ld, lsb);
			_tls_relax_gd_to_ie(ld, ls, lo, lre, p, g, buf);
			break;
		case TLS_RELAX_LOCAL_EXEC:
			_tls_relax_gd_to_le(ld, ls, lo, lre, lsb, buf);
			break;
		default:
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
			break;
		}
		break;

	case R_X86_64_TLSLD:	/* Local Dynamic */
		tr = _tls_check_relax(ld, lre);
		switch (tr) {
		case TLS_RELAX_NONE:
			g = _got_offset(ld, lsb);
			s32 = g + lre->lre_addend - p;
			WRITE_32(buf + lre->lre_offset, s32);
			break;
		case TLS_RELAX_LOCAL_EXEC:
			_tls_relax_ld_to_le(ld, ls, lre, buf);
			break;
		default:
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
			break;
		}
		break;

	case R_X86_64_DTPOFF32:	/* Local Dynamic (offset) */
		tr = _tls_check_relax(ld, lre);
		switch (tr) {
		case TLS_RELAX_NONE:
			s32 = _tls_dtpoff(lo, lsb);
			WRITE_32(buf + lre->lre_offset, s32);
			break;
		case TLS_RELAX_LOCAL_EXEC:
			s32 = _tls_tpoff(lo, lsb);
			WRITE_32(buf + lre->lre_offset, s32);
			break;
		default:
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
			break;
		}
		break;

	case R_X86_64_GOTTPOFF:	/* Initial Exec */
		tr = _tls_check_relax(ld, lre);
		switch (tr) {
		case TLS_RELAX_NONE:
			g = _got_offset(ld, lsb);
			s32 = g + lre->lre_addend - p;
			WRITE_32(buf + lre->lre_offset, s32);
			break;
		case TLS_RELAX_LOCAL_EXEC:
			_tls_relax_ie_to_le(ld, lo, lre, lsb, buf);
			break;
		default:
			ld_fatal(ld, "Internal: invalid TLS relaxation %d",
			    tr);
			break;
		}
		break;

	case R_X86_64_TPOFF32:	/* Local Exec */
		s32 = _tls_tpoff(lo, lsb);
		WRITE_32(buf + lre->lre_offset, s32);
		break;

	default:
		ld_warn(ld, "Relocation %s not supported",
		    elftc_reloc_type_str(EM_X86_64, lre->lre_type));
		break;
	}
}

static void
_adjust_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf)
{
	struct ld_input_section *_is;

	(void) ld;
	(void) is;
	(void) buf;

	/* Only need to adjust relocation against section symbols. */
	if (lsb->lsb_type != STT_SECTION)
		return;

	if ((_is = lsb->lsb_is) == NULL || _is->is_output == NULL)
		return;

	/*
	 * Update the relocation addend to point to the new location
	 * in the output object.
	 */
	lre->lre_addend += _is->is_reloff;
}

static enum ld_tls_relax
_tls_check_relax(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/*
	 * If the linker is performing -static linking, we should always
	 * use the Local Exec model.
	 */
	if (!ld->ld_dynamic_link)
		return (TLS_RELAX_LOCAL_EXEC);

	/*
	 * If the linker is creating a DSO, we can not perform any TLS
	 * relaxation.
	 */
	if (ld->ld_dso)
		return (TLS_RELAX_NONE);

	/*
	 * The linker is creating an executable, if the symbol is
	 * defined in a regular object, we can use the Local Exec model.
	 */
	if (lsb->lsb_shndx != SHN_UNDEF && ld_symbols_in_regular(lsb))
		return (TLS_RELAX_LOCAL_EXEC);

	/*
	 * If the TLS model is Global Dynamic, we can relax it to Initial
	 * Exec model since the linker is creating an executable.
	 */
	if (lre->lre_type == R_X86_64_TLSGD)
		return (TLS_RELAX_INIT_EXEC);

	/* For all the other cases, no relaxation can be done. */
	return (TLS_RELAX_NONE);
}

static int32_t
_tls_tpoff(struct ld_output *lo, struct ld_symbol *lsb)
{
	int32_t tls_off;

	tls_off = -roundup(lo->lo_tls_size, lo->lo_tls_align);

	return (tls_off + (lsb->lsb_value - lo->lo_tls_addr));
}

static int32_t
_tls_dtpoff(struct ld_output *lo, struct ld_symbol *lsb)
{

	return (lsb->lsb_value - lo->lo_tls_addr);
}

static int
_tls_verify_gd(uint8_t *buf, uint64_t off)
{
	/*
	 * Global Dynamic model:
	 *
	 * 0x00 .byte 0x66
	 * 0x01 leaq x@tlsgd(%rip), %rdi
	 * 0x08 .word 0x6666
	 * 0x0a rex64
	 * 0x0b call _tls_get_addr@plt
	 */
	uint8_t gd[] = "\x66\x48\x8d\x3d\x00\x00\x00\x00"
	    "\x66\x66\x48\xe8\x00\x00\x00\x00";

	if (memcmp(buf + off, gd, sizeof(gd) - 1) == 0)
		return (1);

	return (0);
}

static int
_tls_verify_ld(uint8_t *buf, uint64_t off)
{
	/*
	 * Local Dynamic model:
	 *
	 * 0x00 leaq x@tlsld(%rip), %rdi
	 * 0x07 call _tls_get_addr@plt
	 */
	uint8_t ld[] = "\x48\x8d\x3d\x00\x00\x00\x00"
	    "\xe8\x00\x00\x00\x00";

	if (memcmp(buf + off, ld, sizeof(ld) - 1) == 0)
		return (1);

	return (0);
}

static void
_tls_relax_gd_to_ie(struct ld *ld, struct ld_state *ls, struct ld_output *lo,
    struct ld_reloc_entry *lre, uint64_t p, uint64_t g, uint8_t *buf)
{
	/*
	 * Initial Exec model:
	 *
	 * 0x00 movq %fs:0, %rax
	 * 0x09 addq x@gottpoff(%rip), %rax
	 */
	uint8_t ie[] = "\x64\x48\x8b\x04\x25\x00\x00\x00\x00"
	    "\x48\x03\x05\x00\x00\x00\x00";
	int32_t s32;

	assert(lre->lre_type == R_X86_64_TLSGD);

	if (!_tls_verify_gd(buf, lre->lre_offset - 4))
		ld_warn(ld, "unrecognized TLS global dynamic model code");

	/* Rewrite Global Dynamic to Initial Exec model. */
	memcpy((uint8_t *) buf + lre->lre_offset - 4, ie, sizeof(ie) - 1);

	/*
	 * R_X86_64_TLSGD relocation is applied at gd[4]. After it's relaxed
	 * to Initial Exec model, the resulting R_X86_64_GOTTPOFF relocation
	 * should be applied at ie[12]. The addend should remain the same
	 * since instruction "leaq x@tlsgd(%rip), %rdi" and
	 * "addq x@gottpoff(%rip), %rax" has the same length. `p' is moved
	 * 8 bytes forward.
	 */
	s32 = g + lre->lre_addend - (p + 8);
	WRITE_32(buf + lre->lre_offset + 8, s32);

	/* Ignore the next R_X86_64_PLT32 relocation for _tls_get_addr. */
	ls->ls_ignore_next_plt = 1;
}

static void
_tls_relax_gd_to_le(struct ld *ld, struct ld_state *ls, struct ld_output *lo,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf)
{
	/*
	 * Local Exec model:
	 *
	 * 0x00 movq %fs:0, %rax
	 * 0x09 leaq x@tpoff(%rax), %rax
	 */
	uint8_t le[] = "\x64\x48\x8b\x04\x25\x00\x00\x00\x00"
	    "\x48\x8d\x80\x00\x00\x00\x00";
	int32_t s32;

	if (!_tls_verify_gd(buf, lre->lre_offset - 4))
		ld_warn(ld, "unrecognized TLS global dynamic model code");

	/* Rewrite Global Dynamic to Local Exec model. */
	memcpy((uint8_t *) buf + lre->lre_offset - 4, le, sizeof(le) - 1);

	/*
	 * R_X86_64_TLSGD relocation is applied at gd[4]. After it's relaxed
	 * to Local Exec model, the resulting R_X86_64_TPOFF32 should be
	 * applied at le[12].
	 */
	s32 = _tls_tpoff(lo, lsb);
	WRITE_32(buf + lre->lre_offset + 8, s32);

	/* Ignore the next R_X86_64_PLT32 relocation for _tls_get_addr. */
	ls->ls_ignore_next_plt = 1;
}

static void
_tls_relax_ld_to_le(struct ld *ld, struct ld_state *ls,
    struct ld_reloc_entry *lre, uint8_t *buf)
{
	/*
	 * Local Exec model: (with padding)
	 *
	 * 0x00 .word 0x6666
	 * 0x02 .byte 0x66
	 * 0x03 movq %fs:0, %rax
	 */
	uint8_t le_p[] = "\x66\x66\x66\x64\x48\x8b\x04\x25\x00\x00\x00\x00";

	assert(lre->lre_type == R_X86_64_TLSLD);

	if (!_tls_verify_ld(buf, lre->lre_offset - 3))
		ld_warn(ld, "unrecognized TLS local dynamic model code");

	/* Rewrite Local Dynamic to Local Exec model. */
	memcpy(buf + lre->lre_offset - 3, le_p, sizeof(le_p) - 1);

	/* Ignore the next R_X86_64_PLT32 relocation for _tls_get_addr. */
	ls->ls_ignore_next_plt = 1;
}

static void
_tls_relax_ie_to_le(struct ld *ld, struct ld_output *lo,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf)
{
	int32_t s32;
	uint8_t reg;

	(void) ld;

	assert(lre->lre_type == R_X86_64_GOTTPOFF);

	/*
	 * Rewrite Initial Exec to Local Exec model: rewrite
	 * "movq 0x0(%rip),%reg" to "movq 0x0,%reg". or,
	 * "addq 0x0(%rip),%rsp" to "addq 0x0,%rsp". or,
	 * "addq 0x0(%rip),%reg" to "leaq 0x0(%reg),%reg"
	 */
	reg = buf[lre->lre_offset - 1] >> 3;
	if (buf[lre->lre_offset - 2] == 0x8b) {
		/* movq 0x0(%rip),%reg -> movq 0x0,%reg. */
		buf[lre->lre_offset - 2] = 0xc7;
		buf[lre->lre_offset - 1] = 0xc0 | reg; /* Set r/m to `reg' */
		/*
		 * Set REX.B (high bit for r/m) if REX.R (high bit for reg)
		 * is set.
		 */
		if (buf[lre->lre_offset - 3] == 0x4c)
			buf[lre->lre_offset - 3] = 0x49;
	} else if (reg == 4) {
		/* addq 0x0(%rip),%rsp -> addq 0x0,%rsp */
		buf[lre->lre_offset - 2] = 0x81;
		buf[lre->lre_offset - 1] = 0xc0 | reg; /* Set r/m to `reg' */
		/*
		 * Set REX.B (high bit for r/m) if REX.R (high bit for reg)
		 * is set.
		 */
		if (buf[lre->lre_offset - 3] == 0x4c)
			buf[lre->lre_offset - 3] = 0x49;
	} else {
		/* addq 0x0(%rip),%reg -> leaq 0x0(%reg),%reg */
		buf[lre->lre_offset - 2] = 0x8d;
		/* Both reg and r/m in ModRM should be set to `reg' */
		buf[lre->lre_offset - 1] = 0x80 | reg | (reg << 3);
		/* Set both REX.B and REX.R if REX.R is set */
		if (buf[lre->lre_offset - 3] == 0x4c)
			buf[lre->lre_offset - 3] = 0x4d;
	}
	/*
	 * R_X86_64_GOTTPOFF relocation is applied at ie[12]. After it's
	 * relaxed to Local Exec model, the resulting R_X86_64_TPOFF32
	 * should be applied at le[12]. Thus the offset remains the same.
	 */
	s32 = _tls_tpoff(lo, lsb);
	WRITE_32(buf + lre->lre_offset, s32);
}

static void
_create_tls_gd_reloc(struct ld *ld, struct ld_symbol *lsb)
{

	/*
	 * Reserve 2 GOT entries and generate R_X86_64_DTPMOD64 and
	 * R_X86_64_DTPOFF64 relocations.
	 */
	if (!lsb->lsb_got) {
		_reserve_got_entry(ld, lsb, 2);
		_create_got_reloc(ld, lsb, R_X86_64_DTPMOD64,
		    lsb->lsb_got_off);
		_create_got_reloc(ld, lsb, R_X86_64_DTPOFF64,
		    lsb->lsb_got_off + 8);
	}
}

static void
_create_tls_ld_reloc(struct ld *ld, struct ld_symbol *lsb)
{

	/* Reserve 2 GOT entries and generate R_X86_64_DTPMOD64 reloation. */
	if (!lsb->lsb_got) {
		_reserve_got_entry(ld, lsb, 2);
		_create_got_reloc(ld, lsb, R_X86_64_DTPMOD64,
		    lsb->lsb_got_off);
		lsb->lsb_tls_ld = 1;
	}
}

static void
_create_tls_ie_reloc(struct ld *ld, struct ld_symbol *lsb)
{

	/* Reserve 1 GOT entry and generate R_X86_64_TPOFF64 relocation. */
	if (!lsb->lsb_got) {
		_reserve_got_entry(ld, lsb, 1);
		_create_got_reloc(ld, lsb, R_X86_64_TPOFF64,
		    lsb->lsb_got_off);
	}
}

void
amd64_register(struct ld *ld)
{
	struct ld_arch *amd64, *amd64_alt;

	if ((amd64 = calloc(1, sizeof(*amd64))) == NULL)
		ld_fatal_std(ld, "calloc");

	snprintf(amd64->name, sizeof(amd64->name), "%s", "amd64");

	amd64->script = amd64_script;
	amd64->interp = "/libexec/ld-elf.so.1";
	amd64->get_max_page_size = _get_max_page_size;
	amd64->get_common_page_size = _get_common_page_size;
	amd64->scan_reloc = _scan_reloc;
	amd64->process_reloc = _process_reloc;
	amd64->adjust_reloc = _adjust_reloc;
	amd64->is_absolute_reloc = _is_absolute_reloc;
	amd64->is_relative_reloc = _is_relative_reloc;
	amd64->finalize_reloc = _finalize_reloc;
	amd64->finalize_got_and_plt = _finalize_got_and_plt;
	amd64->reloc_is_64bit = 1;
	amd64->reloc_is_rela = 1;
	amd64->reloc_entsize = sizeof(Elf64_Rela);

	HASH_ADD_STR(ld->ld_arch_list, name, amd64);

	if ((amd64_alt = calloc(1, sizeof(*amd64_alt))) == NULL)
		ld_fatal_std(ld, "calloc");
	memcpy(amd64_alt, amd64, sizeof(struct ld_arch));
	amd64_alt->alias = amd64;
	snprintf(amd64_alt->name, sizeof(amd64_alt->name), "%s", "x86-64");

	HASH_ADD_STR(ld->ld_arch_list, name, amd64_alt);
}
