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
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_symbols.h"
#include "ld_utils.h"
#include "i386.h"

ELFTC_VCSID("$Id: i386.c 3419 2016-02-19 20:07:15Z emaste $");

static void _create_plt_reloc(struct ld *ld, struct ld_symbol *lsb,
    uint64_t offset);
static void _create_got_reloc(struct ld *ld, struct ld_symbol *lsb,
    uint64_t type, uint64_t offset);
static void _create_copy_reloc(struct ld *ld, struct ld_symbol *lsb);
static void _create_dynamic_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_symbol *lsb, uint64_t type, uint64_t offset);
static void _scan_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre);
static struct ld_input_section *_find_and_create_got_section(struct ld *ld,
    int create);
static struct ld_input_section *_find_and_create_gotplt_section(struct ld *ld,
    int create);
static struct ld_input_section *_find_and_create_plt_section(struct ld *ld,
    int create);
static uint64_t _get_max_page_size(struct ld *ld);
static uint64_t _get_common_page_size(struct ld *ld);
static void _process_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre, struct ld_symbol *lsb, uint8_t *buf);
static void _reserve_got_entry(struct ld *ld, struct ld_symbol *lsb, int num);
static void _reserve_gotplt_entry(struct ld *ld, struct ld_symbol *lsb);
static void _reserve_plt_entry(struct ld *ld, struct ld_symbol *lsb);
static int _is_absolute_reloc(uint64_t r);
static int _is_relative_reloc(uint64_t r);
static void _warn_pic(struct ld *ld, struct ld_reloc_entry *lre);
static uint32_t _got_offset(struct ld *ld, struct ld_symbol *lsb);

static uint64_t
_get_max_page_size(struct ld *ld)
{

	(void) ld;
	return (0x1000);
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

	if (r == R_386_32)
		return (1);

	return (0);
}

static int
_is_relative_reloc(uint64_t r)
{

	if (r == R_386_RELATIVE)
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
		    elftc_reloc_type_str(EM_386, lre->lre_type), lsb->lsb_name);
	else
		ld_warn(ld, "relocation %s can not be used by runtime linker;"
		    " recompile with -fPIC",
		    elftc_reloc_type_str(EM_386, lre->lre_type));
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
		is->is_entsize = 4;
		is->is_align = 4;
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
		is->is_entsize = 4;
		is->is_align = 4;
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
		is->is_entsize = 4;
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
	 * Record a R_386_JUMP_SLOT entry for this symbol. Note that
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

	ld_reloc_create_entry(ld, ".rel.plt", NULL, R_386_JUMP_SLOT,
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

	ld_reloc_create_entry(ld, ".rel.got", tis, type, lsb, offset, 0);

	if (type != R_386_RELATIVE)
		lsb->lsb_dynrel = 1;
}

static void
_create_copy_reloc(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input_section *tis;

	ld_dynamic_reserve_dynbss_entry(ld, lsb);

	tis = ld_input_find_internal_section(ld, ".dynbss");
	assert(tis != NULL);

	ld_reloc_create_entry(ld, ".rel.bss", tis, R_386_COPY, lsb,
	    lsb->lsb_value, 0);

	lsb->lsb_dynrel = 1;
}

static void
_create_dynamic_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_symbol *lsb, uint64_t type, uint64_t offset)
{

	if (lsb->lsb_bind == STB_LOCAL) {
		if (is->is_flags & SHF_WRITE)
			ld_reloc_create_entry(ld, ".rel.data.rel.local",
			    is, type, lsb, offset, 0);
		else
			ld_reloc_create_entry(ld, ".rel.data.rel.ro.local",
			    is, type, lsb, offset, 0);
	} else {
		if (is->is_flags & SHF_WRITE)
			ld_reloc_create_entry(ld, ".rel.data.rel",
			    is, type, lsb, offset, 0);
		else
			ld_reloc_create_entry(ld, ".rel.data.rel.ro",
			    is, type, lsb, offset, 0);
	}

	if (type != R_386_RELATIVE)
		lsb->lsb_dynrel = 1;
}

static void
_scan_reloc(struct ld *ld, struct ld_input_section *is,
    struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	switch (lre->lre_type) {
	case R_386_NONE:
		break;

	case R_386_32:
		/*
		 * For a local symbol, if te linker output a PIE or DSO,
		 * we should generate a R_386_RELATIVE reloc for R_386_32.
		 */
		if (lsb->lsb_bind == STB_LOCAL) {
			if (ld->ld_pie || ld->ld_dso)
				_create_dynamic_reloc(ld, is, lsb,
				    R_386_RELATIVE, lre->lre_offset);
			break;
		}

		/*
		 * For a global symbol, we probably need to generate PLE entry
		 * and/ore a dynamic relocation.
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
			/*
			 * Check if we can relax R_386_32 to
			 * R_386_RELATIVE instead.
			 */
			if (ld_reloc_relative_relax(ld, lre))
				_create_dynamic_reloc(ld, is, lsb,
				    R_386_RELATIVE, lre->lre_offset);
			else
				_create_dynamic_reloc(ld, is, lsb,
				    R_386_32, lre->lre_offset);
		}

		break;

	case R_386_PLT32:
		/*
		 * In some cases we don't really need to generate a PLT
		 * entry, then a R_386_PLT32 relocation can be relaxed
		 * to a R_386_PC32 relocation.
		 */
		if (lsb->lsb_bind == STB_LOCAL ||
		    !ld_reloc_require_plt(ld, lre)) {
			lre->lre_type = R_386_PC32;
			break;
		}

		/*
		 * If linker outputs an normal executable and the symbol is
		 * defined but is not defined inside a DSO, we can generate
		 * a R_386_PC32 relocation instead.
		 */
		if (ld->ld_exec && lsb->lsb_shndx != SHN_UNDEF &&
		    (lsb->lsb_input == NULL ||
		    lsb->lsb_input->li_type != LIT_DSO)) {
			lre->lre_type = R_386_PC32;
			break;
		}

		/* Create an PLT entry otherwise. */
		if (!lsb->lsb_plt) {
			_reserve_gotplt_entry(ld, lsb);
			_reserve_plt_entry(ld, lsb);
		}
		break;

	case R_386_PC32:
		/*
		 * When R_386_PC32 apply to a global symbol, we should
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
				 */
				_warn_pic(ld, lre);
			}
		}
		break;

	case R_386_GOTOFF:
	case R_386_GOTPC:
		/*
		 * These relocation types use GOT address as a base address
		 * and instruct the linker to build a GOT.
		 */
		(void) _find_and_create_got_section(ld, 1);
		break;

	case R_386_GOT32:
		/*
		 * R_386_GOT32 relocation instructs the linker to build a
		 * GOT and generate a GOT entry.
		 */
		if (!lsb->lsb_got) {
			_reserve_got_entry(ld, lsb, 1);
			/*
			 * TODO: For now we always create a R_386_GLOB_DAT
			 * relocation for a GOT entry. There are cases that
			 * the symbol's address is known at link time and
			 * the GOT entry value can be filled in by the program
			 * linker instead.
			 */
			if (ld_reloc_require_glob_dat(ld, lre))
				_create_got_reloc(ld, lsb, R_386_GLOB_DAT,
				    lsb->lsb_got_off);
			else
				_create_got_reloc(ld, lsb, R_386_RELATIVE,
				    lsb->lsb_got_off);
		}

	default:
		ld_warn(ld, "can not handle relocation %ju",
		    lre->lre_type);
		break;
	}
}

static uint32_t
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
	uint32_t p, s, l, g, got;
	int32_t a, v;

	ls = &ld->ld_state;

	lo = ld->ld_output;
	assert(lo != NULL);

	l = lsb->lsb_plt_off;
	p = lre->lre_offset + is->is_output->os_addr + is->is_reloff;
	got = ld->ld_got->is_output->os_addr;
	s = (uint32_t) lsb->lsb_value;
	READ_32(buf + lre->lre_offset, a);

	switch (lre->lre_type) {
	case R_386_NONE:
		break;

	case R_386_32:
		v = s + a;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_386_PC32:
		if (lsb->lsb_plt)
			v = l + a - p;
		else
			v = s + a - p;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_386_PLT32:
		if (!ls->ls_ignore_next_plt) {
			v = l + a - p;
			WRITE_32(buf + lre->lre_offset, v);
		} else
			ls->ls_ignore_next_plt = 0;
		break;

	case R_386_GOT32:
		g = _got_offset(ld, lsb);
		v = g + a;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_386_GOTOFF:
		v = s + a - got;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	case R_386_GOTPC:
		v = got + a - p;
		WRITE_32(buf + lre->lre_offset, v);
		break;

	default:
		ld_fatal(ld, "Relocation %d not supported", lre->lre_type);
		break;
	}
}

void
i386_register(struct ld *ld)
{
	struct ld_arch *i386_arch;

	if ((i386_arch = calloc(1, sizeof(*i386_arch))) == NULL)
		ld_fatal_std(ld, "calloc");
	
	snprintf(i386_arch->name, sizeof(i386_arch->name), "%s", "i386");

	i386_arch->script = i386_script;
	i386_arch->get_max_page_size = _get_max_page_size;
	i386_arch->get_common_page_size = _get_common_page_size;
	i386_arch->scan_reloc = _scan_reloc;
	i386_arch->process_reloc = _process_reloc;
	i386_arch->is_absolute_reloc = _is_absolute_reloc;
	i386_arch->is_relative_reloc = _is_relative_reloc;
	i386_arch->reloc_is_64bit = 0;
	i386_arch->reloc_is_rela = 0;
	i386_arch->reloc_entsize = sizeof(Elf32_Rel);

	HASH_ADD_STR(ld->ld_arch_list, name, i386_arch);
}
