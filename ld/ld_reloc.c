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
#include "ld_ehframe.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_script.h"
#include "ld_symbols.h"
#include "ld_utils.h"

ELFTC_VCSID("$Id: ld_reloc.c 2962 2013-08-25 16:34:57Z kaiwang27 $");

static struct ld *_ld;

/*
 * Support routines for relocation handling.
 */

static int _discard_reloc(struct ld *ld, struct ld_input_section *is,
    uint64_t sym, uint64_t off, uint64_t *reloc_adjust);
static void _scan_reloc(struct ld *ld, struct ld_input_section *is,
    uint64_t sym, struct ld_reloc_entry *lre);
static void _read_rel(struct ld *ld, struct ld_input_section *is,
    Elf_Data *d);
static void _read_rela(struct ld *ld, struct ld_input_section *is,
    Elf_Data *d);
static void _add_to_gc_search_list(struct ld_state *ls,
    struct ld_input_section *is);
static uint64_t _reloc_addr(struct ld_reloc_entry *lre);
static int _cmp_reloc(struct ld_reloc_entry *a, struct ld_reloc_entry *b);

void
ld_reloc_load(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;
	Elf *e;
	Elf_Scn *scn;
	Elf_Data *d;
	int elferr, i;

	ld_input_link_objects(ld);

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {

		if (li->li_name == NULL || li->li_type == LIT_DSO)
			continue;

		ld_input_load(ld, li);
		e = li->li_elf;

		for (i = 0; (uint64_t) i < li->li_shnum - 1; i++) {
			is = &li->li_is[i];

			if (is->is_type != SHT_REL && is->is_type != SHT_RELA)
				continue;

			if ((scn = elf_getscn(e, is->is_index)) == NULL) {
				ld_warn(ld, "%s(%s): elf_getscn failed: %s",
				    li->li_name, is->is_name, elf_errmsg(-1));
				continue;
			}

			(void) elf_errno();
			if ((d = elf_getdata(scn, NULL)) == NULL) {
				elferr = elf_errno();
				if (elferr != 0)
					ld_warn(ld, "%s(%s): elf_getdata "
					    "failed: %s", li->li_name,
					    is->is_name, elf_errmsg(elferr));
				continue;
			}

			/*
			 * Find out the section to which this relocation
			 * section applies.
			 */
			if (is->is_info < li->li_shnum) {
				is->is_tis = &li->li_is[is->is_info];
				li->li_is[is->is_info].is_ris = is;
			} else {
				ld_warn(ld, "%s(%s): invalid relocation"
				    " section", li->li_name, is->is_name);
				continue;
			}

			/*
			 * Load and process relocation entries.
			 */
			if ((is->is_reloc = malloc(sizeof(*is->is_reloc))) ==
			    NULL)
				ld_fatal(ld, "malloc");
			STAILQ_INIT(is->is_reloc);

			if (is->is_type == SHT_REL)
				_read_rel(ld, is, d);
			else
				_read_rela(ld, is, d);

			if (!strcmp(is->is_tis->is_name, ".eh_frame"))
				ld_ehframe_adjust(ld, is->is_tis);
		}

		ld_input_unload(ld, li);
	}
}

void
ld_reloc_deferred_scan(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;
	struct ld_reloc_entry *lre;
	int i;

	if (ld->ld_reloc)
		return;

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {

		if (li->li_name == NULL || li->li_type == LIT_DSO)
			continue;

		for (i = 0; (uint64_t) i < li->li_shnum - 1; i++) {
			is = &li->li_is[i];

			if (is->is_type != SHT_REL && is->is_type != SHT_RELA)
				continue;

			if (is->is_reloc == NULL)
				continue;

			STAILQ_FOREACH(lre, is->is_reloc, lre_next) {
				ld->ld_arch->scan_reloc(ld, is->is_tis, lre);
			}
		}
	}
}

static int
_discard_reloc(struct ld *ld, struct ld_input_section *is, uint64_t sym,
    uint64_t off, uint64_t *reloc_adjust)
{
	struct ld_output *lo;
	uint8_t *p;
	uint64_t length;
	uint32_t cie_id;

	lo = ld->ld_output;
	assert(lo != NULL);

	/*
	 * Relocation entry should be discarded if the symbol it refers
	 * to was discarded.
	 */
	if (is->is_input->li_symindex[sym] != NULL)
		return (0);

	if (strcmp(is->is_tis->is_name, ".eh_frame"))
		goto discard_reloc;

	/*
	 * If we discard a relocation entry for a FDE in the .eh_frame
	 * section, we need also to remove the FDE entry and adjust the
	 * relocation offset of the following relocation entries for
	 * the .eh_frame section.
	 */

	assert(is->is_tis->is_ehframe != NULL);
	p = is->is_tis->is_ehframe;
	p += off - 8;		/* XXX extended length unsupported */

	/* Read CIE/FDE length field. */
	READ_32(p, length);
	p += 4;

	/* Check for terminator. (Shouldn't happen) */
	if (length == 0)
		goto discard_reloc;

	/* Read CIE ID/Pointer field. */
	READ_32(p, cie_id);
	if (cie_id == 0)
		goto discard_reloc;	/* Shouldn't happen */

	/* Set CIE ID to 0xFFFFFFFF to mark this FDE to be discarded */
	WRITE_32(p, 0xFFFFFFFF);

	/* Update relocation offset adjustment. */
	*reloc_adjust += length + 4;

	/* Reduce the size of the .eh_frame section. */
	is->is_tis->is_shrink += length + 4;

discard_reloc:

	/* Reduce the size of the relocation section accordingly */
	is->is_size -= ld->ld_arch->reloc_entsize;

	return (1);
}

static void
_read_rel(struct ld *ld, struct ld_input_section *is, Elf_Data *d)
{
	struct ld_reloc_entry *lre;
	GElf_Rel r;
	uint64_t reloc_adjust, sym;
	int i, len;

	assert(is->is_reloc != NULL);

	reloc_adjust = 0;
	len = d->d_size / is->is_entsize;
	for (i = 0; i < len; i++) {
		if (gelf_getrel(d, i, &r) != &r) {
			ld_warn(ld, "gelf_getrel failed: %s", elf_errmsg(-1));
			continue;
		}
		sym = GELF_R_SYM(r.r_info);
		if (_discard_reloc(ld, is, sym, r.r_offset, &reloc_adjust))
			continue;
		if ((lre = calloc(1, sizeof(*lre))) == NULL)
			ld_fatal(ld, "calloc");
		assert(r.r_offset >= reloc_adjust);
		lre->lre_offset = r.r_offset - reloc_adjust;
		lre->lre_type = GELF_R_TYPE(r.r_info);
		lre->lre_tis = is->is_tis;
		_scan_reloc(ld, is, sym, lre);
		STAILQ_INSERT_TAIL(is->is_reloc, lre, lre_next);
		is->is_num_reloc++;
	}
	is->is_tis->is_shrink = reloc_adjust;
}

static void
_read_rela(struct ld *ld, struct ld_input_section *is, Elf_Data *d)
{
	struct ld_reloc_entry *lre;
	GElf_Rela r;
	uint64_t reloc_adjust, sym;
	int i, len;

	assert(is->is_reloc != NULL);

	reloc_adjust = 0;
	len = d->d_size / is->is_entsize;
	for (i = 0; i < len; i++) {
		if (gelf_getrela(d, i, &r) != &r) {
			ld_warn(ld, "gelf_getrel failed: %s", elf_errmsg(-1));
			continue;
		}
		sym = GELF_R_SYM(r.r_info);
		if (_discard_reloc(ld, is, sym, r.r_offset, &reloc_adjust))
			continue;
		if ((lre = calloc(1, sizeof(*lre))) == NULL)
			ld_fatal(ld, "calloc");
		assert(r.r_offset >= reloc_adjust);
		lre->lre_offset = r.r_offset - reloc_adjust;
		lre->lre_type = GELF_R_TYPE(r.r_info);
		lre->lre_addend = r.r_addend;
		lre->lre_tis = is->is_tis;
		_scan_reloc(ld, is, sym, lre);
		STAILQ_INSERT_TAIL(is->is_reloc, lre, lre_next);
		is->is_num_reloc++;
	}
	is->is_tis->is_shrink = reloc_adjust;
}

static void
_scan_reloc(struct ld *ld, struct ld_input_section *is, uint64_t sym,
    struct ld_reloc_entry *lre)
{
	struct ld_input *li;

	(void) ld;

	li = is->is_input;

	lre->lre_sym = li->li_symindex[sym];

	if (!ld->ld_reloc && !ld->ld_gc)
		ld->ld_arch->scan_reloc(ld, is->is_tis, lre);
}

static void
_add_to_gc_search_list(struct ld_state *ls, struct ld_input_section *is)
{

	assert(is != NULL);

	/* Only add allocated sections. */
	if ((is->is_flags & SHF_ALLOC) == 0)
		return;

	/*
	 * Do not add sections that are already exist in the search list,
	 * or sections that don't have assoicated relocations.
	 */
	if (is->is_refed || is->is_ris == NULL || is->is_ris->is_reloc == NULL)
		return;

	STAILQ_INSERT_TAIL(ls->ls_gc, is, is_gc_next);
}

void
ld_reloc_gc_sections(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_symbol *lsb;
	struct ld_input_section *is;
	struct ld_reloc_entry *lre;
	char *entry;

	/*
	 * Initialise search list. Initial search list consists of sections
	 * contains the entry and extern symbols.
	 */
	ls = &ld->ld_state;
	if ((ls->ls_gc = calloc(1, sizeof(*ls->ls_gc))) == NULL)
		ld_fatal_std(ld, "calloc");
	STAILQ_INIT(ls->ls_gc);

	/*
	 * Add the section that contains the entry symbol to the initial
	 * search list.
	 */
	entry = ld->ld_entry != NULL ? ld->ld_entry :
	    ld->ld_scp->lds_entry_point;
	if (entry != NULL) {
		HASH_FIND_STR(ld->ld_sym, entry, lsb);
		if (lsb != NULL && lsb->lsb_is != NULL)
			_add_to_gc_search_list(ls, lsb->lsb_is);
	}

	/*
	 * Add sections that contain the symbols specified by command line
	 * option `-u' (extern symbols) to the initial search list.
	 */
	if (ld->ld_ext_symbols != NULL) {
		STAILQ_FOREACH(lsb, ld->ld_ext_symbols, lsb_next) {
			if (lsb->lsb_is != NULL)
				_add_to_gc_search_list(ls, lsb->lsb_is);
		}
	}

	/*
	 * Breadth-first search for sections referenced by relocations
	 * assoicated with the initial sections. The search is recusive,
	 * the relocations assoicated with the found sections are again
	 * used to search for more referenced sections.
	 */
	STAILQ_FOREACH(is, ls->ls_gc, is_gc_next) {
		assert(is->is_ris != NULL);
		STAILQ_FOREACH(lre, is->is_ris->is_reloc, lre_next) {
			if (lre->lre_sym == NULL)
				continue;
			lsb = ld_symbols_ref(lre->lre_sym);
			if (lsb->lsb_is != NULL)
				_add_to_gc_search_list(ls, lsb->lsb_is);
		}
	}
}

void *
ld_reloc_serialize(struct ld *ld, struct ld_output_section *os, size_t *sz)
{
	struct ld_reloc_entry *lre;
	struct ld_symbol *lsb;
	Elf32_Rel *r32;
	Elf64_Rel *r64;
	Elf32_Rela *ra32;
	Elf64_Rela *ra64;
	uint8_t *p;
	void *b;
	size_t entsize;
	uint64_t sym;
	unsigned char is_64;
	unsigned char is_rela;

	is_64 = ld->ld_arch->reloc_is_64bit;
	is_rela = ld->ld_arch->reloc_is_rela;
	entsize = ld->ld_arch->reloc_entsize;

	b = malloc(ld->ld_arch->reloc_entsize * os->os_num_reloc);
	if (b == NULL)
		ld_fatal_std(ld, "malloc");

	p = b;
	STAILQ_FOREACH(lre, os->os_reloc, lre_next) {
		if (lre->lre_sym != NULL) {
			lsb = ld_symbols_ref(lre->lre_sym);
			if (os->os_dynrel)
				sym = lsb->lsb_dyn_index;
			else
				sym = lsb->lsb_out_index;
		} else
			sym = 0;

		if (is_64 && is_rela) {
			ra64 = (Elf64_Rela *) (uintptr_t) p;
			ra64->r_offset = lre->lre_offset;
			ra64->r_info = ELF64_R_INFO(sym, lre->lre_type);
			ra64->r_addend = lre->lre_addend;
		} else if (!is_64 && !is_rela) {
			r32 = (Elf32_Rel *) (uintptr_t) p;
			r32->r_offset = (uint32_t) lre->lre_offset;
			r32->r_info = (uint32_t) ELF32_R_INFO(sym,
			    lre->lre_type);
		} else if (!is_64 && is_rela) {
			ra32 = (Elf32_Rela *) (uintptr_t) p;
			ra32->r_offset = (uint32_t) lre->lre_offset;
			ra32->r_info = (uint32_t) ELF32_R_INFO(sym,
			    lre->lre_type);
			ra32->r_addend = (int32_t) lre->lre_addend;
		} else if (is_64 && !is_rela) {
			r64 = (Elf64_Rel *) (uintptr_t) p;
			r64->r_offset = lre->lre_offset;
			r64->r_info = ELF64_R_INFO(sym, lre->lre_type);
		}

		p += entsize;
	}

	*sz = entsize * os->os_num_reloc;
	assert((size_t) (p - (uint8_t *) b) == *sz);

	return (b);
}

void
ld_reloc_create_entry(struct ld *ld, const char *name,
    struct ld_input_section *tis, uint64_t type, struct ld_symbol *lsb,
    uint64_t offset, int64_t addend)
{
	struct ld_input_section *is;
	struct ld_reloc_entry *lre;
	int len;

	/*
	 * List of internal sections to hold dynamic relocations:
	 *
	 * .rel.bss      contains copy relocations
	 * .rel.plt      contains PLT (*_JMP_SLOT) relocations
	 * .rel.got      contains GOT (*_GLOB_DATA) relocations
	 * .rel.data.*   contains *_RELATIVE and absolute relocations
	 */

	is = ld_input_find_internal_section(ld, name);
	if (is == NULL) {
		is = ld_input_add_internal_section(ld, name);
		is->is_dynrel = 1;
		is->is_type = ld->ld_arch->reloc_is_rela ? SHT_RELA : SHT_REL;
		is->is_align = ld->ld_arch->reloc_is_64bit ? 8 : 4;
		is->is_entsize = ld->ld_arch->reloc_entsize;

		len = strlen(name);
		if (len > 3 && name[len - 1] == 't' && name[len - 2] == 'l' &&
		    name[len - 3] == 'p')
			is->is_pltrel = 1;
	}

	if (is->is_reloc == NULL) {
		is->is_reloc = calloc(1, sizeof(*is->is_reloc));
		if (is->is_reloc == NULL)
			ld_fatal_std(ld, "calloc");
		STAILQ_INIT(is->is_reloc);
	}

	if ((lre = malloc(sizeof(*lre))) == NULL)
		ld_fatal_std(ld, "calloc");

	lre->lre_tis = tis;
	lre->lre_type = type;
	lre->lre_sym = lsb;
	lre->lre_offset = offset;
	lre->lre_addend = addend;

	STAILQ_INSERT_TAIL(is->is_reloc, lre, lre_next);
	is->is_num_reloc++;
	is->is_size += ld->ld_arch->reloc_entsize;

	/* Keep track of the total number of *_RELATIVE relocations. */
	if (ld->ld_arch->is_relative_reloc(type))
		ld->ld_state.ls_relative_reloc++;
}

void
ld_reloc_finalize_dynamic(struct ld *ld, struct ld_output *lo,
    struct ld_output_section *os)
{
	struct ld_input_section *is;
	struct ld_output_section *_os;
	struct ld_reloc_entry *lre;

	if (!os->os_dynrel || os->os_reloc == NULL)
		return;

	/* PLT relocation is handled in arch-specified code. */
	if (os->os_pltrel)
		return;

	/*
	 * Set the lo->lo_rel_dyn here so that the DT_* entries needed for
	 * dynamic relocation will be generated.
	 *
	 * Note that besides the PLT relocation section, we can only have one
	 * dynamic relocation section in the output object.
	 */
	if (lo->lo_rel_dyn == NULL)
		lo->lo_rel_dyn = os;

	STAILQ_FOREACH(lre, os->os_reloc, lre_next) {
		/*
		 * Found out the corresponding output section for the input
		 * section which the relocation applies to.
		 */
		is = lre->lre_tis;
		assert(is != NULL);
		if ((_os = is->is_output) == NULL)
			continue;

		/*
		 * Update the relocation offset to make it point to the
		 * correct place in the output section.
		 */
		lre->lre_offset += _os->os_addr + is->is_reloff;

		/*
		 * Perform arch-specific dynamic relocation
		 * finalization.
		 */
		ld->ld_arch->finalize_reloc(ld, is, lre);
	}
}

void
ld_reloc_join(struct ld *ld, struct ld_output_section *os,
    struct ld_input_section *is)
{

	assert(is->is_reloc != NULL);

	if (os->os_reloc == NULL) {
		if ((os->os_reloc = malloc(sizeof(*os->os_reloc))) == NULL)
			ld_fatal_std(ld, "malloc");
		STAILQ_INIT(os->os_reloc);
	}

	STAILQ_CONCAT(os->os_reloc, is->is_reloc);
	os->os_num_reloc += is->is_num_reloc;

	is->is_num_reloc = 0;
	free(is->is_reloc);
	is->is_reloc = NULL;
}

static uint64_t
_reloc_addr(struct ld_reloc_entry *lre)
{

	return (lre->lre_tis->is_output->os_addr + lre->lre_tis->is_reloff +
	    lre->lre_offset);
}

static int
_cmp_reloc(struct ld_reloc_entry *a, struct ld_reloc_entry *b)
{
	struct ld *ld;

	ld = _ld;

	/*
	 * Sort dynamic relocation entries to make the runtime linker
	 * run faster. *_RELATIVE relocations should be sorted to the
	 * front. Between two *_RELATIVE relocations, the one with
	 * lower address should appear first. For other relocations
	 * we sort them by assoicated dynamic symbol index, then
	 * by relocation type.
	 */

	if (ld->ld_arch->is_relative_reloc(a->lre_type) &&
	    !ld->ld_arch->is_relative_reloc(b->lre_type))
		return (-1);

	if (!ld->ld_arch->is_relative_reloc(a->lre_type) &&
	    ld->ld_arch->is_relative_reloc(b->lre_type))
		return (1);

	if (ld->ld_arch->is_relative_reloc(a->lre_type) &&
	    ld->ld_arch->is_relative_reloc(b->lre_type)) {
		if (_reloc_addr(a) < _reloc_addr(b))
			return (-1);
		else if (_reloc_addr(a) > _reloc_addr(b))
			return (1);
		else
			return (0);
	}

	if (a->lre_sym->lsb_dyn_index < b->lre_sym->lsb_dyn_index)
		return (-1);
	else if (a->lre_sym->lsb_dyn_index > b->lre_sym->lsb_dyn_index)
		return (1);

	if (a->lre_type < b->lre_type)
		return (-1);
	else if (a->lre_type > b->lre_type)
		return (1);

	return (0);
}

void
ld_reloc_sort(struct ld *ld, struct ld_output_section *os)
{

	_ld = ld;

	if (os->os_reloc == NULL)
		return;

	STAILQ_SORT(os->os_reloc, ld_reloc_entry, lre_next, _cmp_reloc);
}

int
ld_reloc_require_plt(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/* Only need PLT for functions. */
	if (lsb->lsb_type != STT_FUNC)
		return (0);

	/* Create PLT for functions in DSOs. */
	if (ld_symbols_in_dso(lsb))
		return (1);

	/*
	 * If the linker outputs a DSO, PLT entry is needed if the symbol
	 * if undefined or it can be overridden.
	 */
	if (ld->ld_dso &&
	    (lsb->lsb_shndx == SHN_UNDEF || ld_symbols_overridden(ld, lsb)))
		return (1);

	/* Otherwise, we do not create PLT entry. */
	return (0);
}

int
ld_reloc_require_copy_reloc(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/* Functions do not need copy reloc. */
	if (lsb->lsb_type == STT_FUNC)
		return (0);

	/*
	 * If we are generating a normal executable and the symbol is
	 * defined in a DSO, we need a copy reloc.
	 */
	if (ld->ld_exec && ld_symbols_in_dso(lsb))
		return (1);

	return (0);
}

int
ld_reloc_require_glob_dat(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/*
	 * If the symbol is undefined or if it's defined in a DSO,
	 * GLOB_DAT relocation is required.
	 */
	if (lsb->lsb_shndx == SHN_UNDEF || ld_symbols_in_dso(lsb))
		return (1);

	/*
	 * If the linker creates a DSO and the symbol can be overridden
	 * GLOB_DAT relocation is required.
	 */
	if (ld->ld_dso && ld_symbols_overridden(ld, lsb))
		return (1);

	/*
	 * If the linker creates a DSO and the symbol visibility is
	 * STV_PROTECTED, GLOB_DAT relocation is required for function
	 * address comparsion to work.
	 */
	if (ld->ld_dso && lsb->lsb_other == STV_PROTECTED)
		return (1);

	/*
	 * Otherwise GLOB_DAT relocation is not required, RELATIVE
	 * relocation can be used instead.
	 */
	return (0);
}

int
ld_reloc_require_dynamic_reloc(struct ld *ld, struct ld_reloc_entry *lre)
{
	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/*
	 * If the symbol is defined in a DSO, we create specific dynamic
	 * relocations when we create PLT, GOT or copy reloc.
	 */
	if (ld_symbols_in_dso(lsb))
		return (0);

	/*
	 * When we are creating a DSO, we create dynamic relocation if
	 * the symbol is undefined, or if the symbol can be overridden.
	 */
	if (ld->ld_dso && (lsb->lsb_shndx == SHN_UNDEF ||
	    ld_symbols_overridden(ld, lsb)))
		return (1);

	/*
	 * When we are creating a PIE/DSO (position-independent), if the
	 * relocation is referencing the absolute address of a symbol,
	 * we should create dynamic relocation.
	 */
	if ((ld->ld_pie || ld->ld_dso) &&
	    ld->ld_arch->is_absolute_reloc(lre->lre_type))
		return (1);

	/* Otherwise we do not generate dynamic relocation. */
	return (0);
}

int
ld_reloc_relative_relax(struct ld *ld, struct ld_reloc_entry *lre)
{

	struct ld_symbol *lsb;

	lsb = ld_symbols_ref(lre->lre_sym);

	/*
	 * We only use *_RELATIVE relocation when we create PIE/DSO.
	 */
	if (!ld->ld_pie && !ld->ld_dso)
		return (0);

	/*
	 * If the symbol is defined in a DSO, we can not relax the
	 * relocation.
	 */
	if (ld_symbols_in_dso(lsb))
		return (0);

	/*
	 * When we are creating a DSO, we can not relax dynamic relocation
	 * to *_RELATIVE relocation if the symbol is undefined, or if the
	 * symbol can be overridden.
	 */
	if (ld->ld_dso && (lsb->lsb_shndx == SHN_UNDEF ||
	    ld_symbols_overridden(ld, lsb)))
		return (0);

	/* Otherwise it's ok to use *_RELATIVE. */
	return (1);
}

void
ld_reloc_process_input_section(struct ld *ld, struct ld_input_section *is,
    void *buf)
{
	struct ld_input *li;
	struct ld_input_section *ris;
	struct ld_output_section *os;
	struct ld_reloc_entry *lre;
	struct ld_symbol *lsb;
	int i;

	if (is->is_type == SHT_REL || is->is_type == SHT_RELA)
		return;

	os = is->is_output;

	li = is->is_input;
	if (is->is_ris != NULL)
		ris = is->is_ris;
	else {
		ris = NULL;
		for (i = 0; (uint64_t) i < li->li_shnum; i++) {
			if (li->li_is[i].is_type != SHT_REL &&
			    li->li_is[i].is_type != SHT_RELA)
				continue;
			if (li->li_is[i].is_info == is->is_index) {
				ris = &li->li_is[i];
				break;
			}
		}
	}

	if (ris == NULL)
		return;

	assert(ris->is_reloc != NULL);

	STAILQ_FOREACH(lre, ris->is_reloc, lre_next) {
		lsb = ld_symbols_ref(lre->lre_sym);

		/*
		 * Arch-specific relocation handling for non-relocatable
		 * output object.
		 */
		if (!ld->ld_reloc)
			ld->ld_arch->process_reloc(ld, is, lre, lsb, buf);

		/*
		 * Arch-specific relocation handling for relocatable output
		 * object and -emit-relocs option.
		 *
		 * Note that for SHT_REL relocation sections, relocation
		 * addend (in-place) is not adjusted since it will overwrite
		 * the already applied relocation.
		 */
		if (ld->ld_reloc ||
		    (ld->ld_emit_reloc && ld->ld_arch->reloc_is_rela))
			ld->ld_arch->adjust_reloc(ld, is, lre, lsb, buf);

		/*
		 * Update the relocation offset to make it point to the
		 * correct place in the output section. For -emit-relocs
		 * option, the section VMA is used. For relocatable output
		 * object, the section relative offset is added to the
		 * relocation offset.
		 */
		if (ld->ld_reloc)
			lre->lre_offset += is->is_reloff;
		else if (ld->ld_emit_reloc)
			lre->lre_offset += os->os_addr + is->is_reloff;
	}
}
