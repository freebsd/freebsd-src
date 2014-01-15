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
#include "ld_file.h"
#include "ld_hash.h"
#include "ld_input.h"
#include "ld_layout.h"
#include "ld_output.h"
#include "ld_path.h"
#include "ld_symbols.h"
#include "ld_symver.h"
#include "ld_strtab.h"

ELFTC_VCSID("$Id: ld_dynamic.c 2965 2013-09-10 02:46:29Z kaiwang27 $");

static void _check_dso_needed(struct ld *ld, struct ld_output *lo);
static void _create_dynamic(struct ld *ld, struct ld_output *lo);
static void _create_interp(struct ld *ld, struct ld_output *lo);
static void _create_dynsym_and_dynstr_section(struct ld *ld,
    struct ld_output *lo);
static void _finalize_dynamic(struct ld *ld, struct ld_output *lo);

void
ld_dynamic_create(struct ld *ld)
{
	struct ld_output *lo;

	lo = ld->ld_output;
	assert(lo != NULL);

	/* Check how many DSOs is needed for output object. */
	_check_dso_needed(ld, lo);

	/* Link statically if we don't use DSOs? */
	if (lo->lo_dso_needed == 0)
		return;

	ld->ld_dynamic_link = 1;

	/* Create .interp section. */
	if (!ld->ld_dso)
		_create_interp(ld, lo);

	/* Create .dynamic section. */
	_create_dynamic(ld, lo);

	/* Create .dynsym and .dynstr sections. */
	_create_dynsym_and_dynstr_section(ld, lo);

	/* Create .hash section. */
	ld_hash_create_svr4_hash_section(ld);

	/*
	 * Create .gnu.version_d section if the linker creats a shared
	 * library and version script is provided.
	 */
	lo->lo_version_index = 2;
	if (ld->ld_dso)
		ld_symver_create_verdef_section(ld);

	/* Create .gnu.version_r section. */
	ld_symver_create_verneed_section(ld);

	/* Create .gnu.version section. */
	ld_symver_create_versym_section(ld);
}

void
ld_dynamic_finalize(struct ld *ld)
{
	struct ld_output *lo;

	lo = ld->ld_output;
	assert(lo != NULL);

	if (lo->lo_dso_needed == 0)
		return;

	/* Finalize .dynamic section */
	_finalize_dynamic(ld, lo);
}

void
ld_dynamic_load_dso_dynamic(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *scn, size_t strndx)
{
	GElf_Shdr shdr;
	GElf_Dyn dyn;
	Elf_Data *d;
	int elferr, i, len;
	const char *name;

	if (strndx == SHN_UNDEF)
		return;

	if (gelf_getshdr(scn, &shdr) != &shdr) {
		ld_warn(ld, "%s: gelf_getshdr failed: %s", li->li_name,
		    elf_errmsg(-1));
		return;
	}

	(void) elf_errno();
	if ((d = elf_getdata(scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_warn(ld, "%s: elf_getdata failed: %s", li->li_name,
			    elf_errmsg(elferr));
		return;
	}

	len = d->d_size / shdr.sh_entsize;
	for (i = 0; i < len; i++) {
		if (gelf_getdyn(d, i, &dyn) != &dyn) {
			ld_warn(ld, "%s: gelf_getdyn failed: %s", li->li_name,
			    elf_errmsg(-1));
			continue;
		}
		switch (dyn.d_tag) {
		case DT_SONAME:
			name = elf_strptr(e, strndx, dyn.d_un.d_ptr);
			if (name != NULL &&
			    (li->li_soname = strdup(name)) == NULL)
				ld_fatal_std(ld, "strdup");
			break;
		case DT_NEEDED:
			name = elf_strptr(e, strndx, dyn.d_un.d_ptr);
			if (name != NULL)
				ld_path_search_dso_needed(ld, li->li_file,
				    name);
			break;
		default:
			break;
		}
	}
}

void
ld_dynamic_reserve_dynbss_entry(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input *li;
	struct ld_input_section *dynbss, *is;
	uint64_t a;

	/* Create .dynbss section if it doesn't yet exist. */
	dynbss = ld_input_find_internal_section(ld, ".dynbss");
	if (dynbss == NULL) {
		dynbss = ld_input_add_internal_section(ld, ".dynbss");
		dynbss->is_type = SHT_NOBITS;
	}

	li = lsb->lsb_input;
	assert(li != NULL && li->li_type == LIT_DSO);

	/*
	 * TODO: we don't have to create copy relocation
	 * for every import object. Some import objects
	 * are read-only, in that case we can create other
	 * dynamic relocations for them.
	 */

	/*
	 * If the section to which the symbols belong has a larger
	 * alignment requirement, we increase .dynbss section alignment
	 * accordingly. XXX What if it is a DSO common symbol?
	 */
	is = NULL;
	if (lsb->lsb_shndx != SHN_COMMON) {
		assert(lsb->lsb_shndx < li->li_shnum - 1);
		is = &li->li_is[lsb->lsb_shndx];
		if (is->is_align > dynbss->is_align)
			dynbss->is_align = is->is_align;
	}

	/*
	 * Calculate the alignment for this object.
	 */
	if (is != NULL) {
		for (a = is->is_align; a > 1; a >>= 1) {
			if ((lsb->lsb_value - is->is_off) % a == 0)
				break;
		}
	} else
		a = 1;

	if (a > 1)
		dynbss->is_size = roundup(dynbss->is_size, a);

	lsb->lsb_value = dynbss->is_size;
	lsb->lsb_copy_reloc = 1;
	lsb->lsb_input = dynbss->is_input;
	lsb->lsb_shndx = dynbss->is_index;
	lsb->lsb_is = dynbss;

	dynbss->is_size += lsb->lsb_size;
}

static void
_create_interp(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os;
	struct ld_output_data_buffer *odb;
	const char *interp;
	char interp_name[] = ".interp";

	HASH_FIND_STR(lo->lo_ostbl, interp_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, interp_name,
		    SHF_ALLOC);
	os->os_type = SHT_PROGBITS;
	os->os_align = 1;
	os->os_entsize = 0;
	os->os_flags = SHF_ALLOC;

	lo->lo_interp = os;

	if (ld->ld_interp != NULL)
		interp = ld->ld_interp;
	else
		interp = ld->ld_arch->interp;
	assert(interp != NULL);

	if ((odb = calloc(1, sizeof(*odb))) == NULL)
		ld_fatal_std(ld, "calloc");
	odb->odb_size = strlen(interp) + 1;
	odb->odb_align = 1;
	odb->odb_type = ELF_T_BYTE;

	if ((odb->odb_buf = calloc(odb->odb_size, 1)) == NULL)
		ld_fatal_std(ld, "calloc");
	strncpy(odb->odb_buf, interp, strlen(interp));
	odb->odb_buf[strlen(interp)] = '\0';

	(void) ld_output_create_section_element(ld, os, OET_DATA_BUFFER, odb,
	    NULL);
}

static void
_create_dynamic(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os, *_os;
	struct ld_output_data_buffer *odb;
	char dynamic_name[] = ".dynamic";
	char init_name[] = ".init";
	char fini_name[] = ".fini";
	char *rpath;
	int entries;

	HASH_FIND_STR(lo->lo_ostbl, dynamic_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, dynamic_name,
		    SHF_ALLOC | SHF_WRITE);
	os->os_type = SHT_DYNAMIC;
	os->os_flags = SHF_ALLOC | SHF_WRITE;
	if (lo->lo_ec == ELFCLASS32) {
		os->os_entsize = 8;
		os->os_align = 4;
	} else {
		os->os_entsize = 16;
		os->os_align = 8;
	}

	lo->lo_dynamic = os;

	/* .dynamic section should link to .dynstr section. */
	if ((os->os_link = strdup(".dynstr")) == NULL)
		ld_fatal_std(ld, "strdup");

	/* DT_NEEDED */
	entries = lo->lo_dso_needed;

	/* DT_SONAME. */
	if (ld->ld_soname != NULL) {
		lo->lo_soname_nameindex = ld_strtab_insert_no_suffix(ld,
		    ld->ld_dynstr, ld->ld_soname);
		entries++;
	}

	/* DT_INIT */
	HASH_FIND_STR(lo->lo_ostbl, init_name, _os);
	if (_os != NULL && !_os->os_empty) {
		lo->lo_init = _os;
		entries++;
	}

	/* DT_FINI */
	HASH_FIND_STR(lo->lo_ostbl, fini_name, _os);
	if (_os != NULL && !_os->os_empty) {
		lo->lo_fini = _os;
		entries++;
	}

	/* DT_HASH, DT_STRTAB, DT_SYMTAB, DT_STRSZ and DT_SYMENT */
	if (ld->ld_dynsym)
		entries += 5;

	/* DT_RPATH. */
	if (!STAILQ_EMPTY(&ld->ld_state.ls_rplist)) {
		rpath = ld_path_join_rpath(ld);
		lo->lo_rpath_nameindex = ld_strtab_insert_no_suffix(ld,
		    ld->ld_dynstr, rpath);
		entries++;
	}

	/*
	 * DT_DEBUG. dynamic linker changes this at runtime, gdb uses
	 * it to find all the loaded DSOs. (thus .dynamic has to be
	 * writable)
	 */
	if (!ld->ld_dso)
		entries++;

	/* DT_PLTGOT, DT_PLTRELSZ, DT_PLTREL and DT_JMPREL. */
	entries += 4;

	/* DT_REL/DT_RELA, DT_RELSZ/DT_RELASZ and DT_RELENT/DT_RELAENT */
	entries += 3;

	/*
	 * DT_VERNEED, DT_VERNEEDNUM, DT_VERDEF, DT_VERDEFNUM and DT_VERSYM.
	 */
	entries += 5;

	/* DT_RELCOUNT/DT_RELACOUNT. */
	if (ld->ld_state.ls_relative_reloc > 0)
		entries++;

	/* DT_NULL. TODO: Reserve multiple DT_NULL entries for DT_RPATH? */
	entries++;

	/*
	 * Reserve space for .dynamic section, based on number of entries.
	 */
	if ((odb = calloc(1, sizeof(*odb))) == NULL)
		ld_fatal_std(ld, "calloc");
	odb->odb_size = entries * os->os_entsize;
	if ((odb->odb_buf = malloc(odb->odb_size)) == NULL)
		ld_fatal_std(ld, "malloc");
	odb->odb_align = os->os_align;
	odb->odb_type = ELF_T_DYN;

	(void) ld_output_create_section_element(ld, os, OET_DATA_BUFFER, odb,
	    NULL);

	lo->lo_dynamic_odb = odb;

	/* Create _DYNAMIC symobl. */
	ld_symbols_add_internal(ld, "_DYNAMIC", 0, 0, SHN_ABS, STB_LOCAL,
	    STT_OBJECT, STV_HIDDEN, NULL, os);
}

#define	DT_ENTRY_VAL(tag,val)					\
	do {							\
		if (lo->lo_ec == ELFCLASS32) {			\
			assert(dt32 < end32); 			\
			dt32->d_tag = (int32_t) (tag);		\
			dt32->d_un.d_val = (uint32_t) (val);	\
			dt32++;					\
		} else {					\
			assert(dt64 < end64); 			\
			dt64->d_tag = (tag);			\
			dt64->d_un.d_val = (val);		\
			dt64++;					\
		}						\
	} while(0)

#define DT_ENTRY_PTR(tag,ptr)					\
	do {							\
		if (lo->lo_ec == ELFCLASS32) {			\
			assert(dt32 < end32); 			\
			dt32->d_tag = (int32_t) (tag);		\
			dt32->d_un.d_ptr = (uint32_t) (ptr);	\
			dt32++;					\
		} else {					\
			assert(dt64 < end64); 			\
			dt64->d_tag = (tag);			\
			dt64->d_un.d_ptr = (ptr);		\
			dt64++;					\
		}						\
	} while(0)

#define DT_ENTRY_NULL						\
	do {							\
		if (lo->lo_ec == ELFCLASS32) {			\
			assert(dt32 < end32); 			\
			while (dt32 < end32)			\
				DT_ENTRY_VAL(DT_NULL, 0);	\
			assert(dt32 == end32);			\
		} else {					\
			assert(dt64 < end64); 			\
			while (dt64 < end64)			\
				DT_ENTRY_VAL(DT_NULL, 0);	\
			assert(dt64 == end64);			\
		}						\
	} while(0)

static void
_finalize_dynamic(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_data_buffer *odb;
	Elf32_Dyn *dt32, *end32;
	Elf64_Dyn *dt64, *end64;
	int *p;

	odb = lo->lo_dynamic_odb;
	assert(odb != NULL);

	dt32 = (Elf32_Dyn *) (uintptr_t) odb->odb_buf;
	dt64 = (Elf64_Dyn *) (uintptr_t) odb->odb_buf;
	end32 = (Elf32_Dyn *) (uintptr_t) (odb->odb_buf + odb->odb_size);
	end64 = (Elf64_Dyn *) (uintptr_t) (odb->odb_buf + odb->odb_size);

	/* DT_NEEDED. */
	for (p = (int *) (uintptr_t) utarray_front(lo->lo_dso_nameindex);
	     p != NULL;
	     p = (int *) (uintptr_t) utarray_next(lo->lo_dso_nameindex, p))
		DT_ENTRY_VAL(DT_NEEDED, *p);

	/* DT_SONAME. */
	if (ld->ld_soname != NULL)
		DT_ENTRY_VAL(DT_SONAME, lo->lo_soname_nameindex);

	/* DT_INIT and DT_FINI */
	if (lo->lo_init != NULL)
		DT_ENTRY_PTR(DT_INIT, lo->lo_init->os_addr);
	if (lo->lo_fini != NULL)
		DT_ENTRY_PTR(DT_FINI, lo->lo_fini->os_addr);

	/* DT_HASH */
	if (lo->lo_hash != NULL)
		DT_ENTRY_PTR(DT_HASH, lo->lo_hash->os_addr);

	/* DT_HASH, DT_STRTAB, DT_SYMTAB, DT_STRSZ and DT_SYMENT */
	if (lo->lo_dynsym != NULL && lo->lo_dynstr != NULL) {
		DT_ENTRY_PTR(DT_STRTAB, lo->lo_dynstr->os_addr);
		DT_ENTRY_PTR(DT_SYMTAB, lo->lo_dynsym->os_addr);
		DT_ENTRY_VAL(DT_STRSZ, ld_strtab_getsize(ld->ld_dynstr));
		DT_ENTRY_VAL(DT_SYMENT,
		    lo->lo_ec == ELFCLASS32 ? sizeof(Elf32_Sym) :
		    sizeof(Elf64_Sym));
	}

	/* DT_RPATH */
	if (!STAILQ_EMPTY(&ld->ld_state.ls_rplist))
		DT_ENTRY_VAL(DT_RPATH, lo->lo_rpath_nameindex);

	/* DT_DEBUG */
	if (!ld->ld_dso)
		DT_ENTRY_VAL(DT_DEBUG, 0);

	/* DT_PLTGOT, DT_PLTRELSZ, DT_PLTREL and DT_JMPREL. */
	if (lo->lo_gotplt != NULL)
		DT_ENTRY_PTR(DT_PLTGOT, lo->lo_gotplt->os_addr);
	if (lo->lo_rel_plt != NULL) {
		DT_ENTRY_VAL(DT_PLTRELSZ, lo->lo_rel_plt->os_size);
		DT_ENTRY_VAL(DT_PLTREL,
		    ld->ld_arch->reloc_is_rela ? DT_RELA : DT_REL);
		DT_ENTRY_PTR(DT_JMPREL, lo->lo_rel_plt->os_addr);
	}

	/* DT_REL/DT_RELA, DT_RELSZ/DT_RELASZ and DT_RELENT/DT_RELAENT */
	if (lo->lo_rel_dyn != NULL) {
		if (!ld->ld_arch->reloc_is_rela) {
			DT_ENTRY_PTR(DT_REL, lo->lo_rel_dyn->os_addr);
			DT_ENTRY_VAL(DT_RELSZ, lo->lo_rel_dyn->os_size);
			DT_ENTRY_VAL(DT_RELENT, ld->ld_arch->reloc_entsize);
		} else {
			DT_ENTRY_PTR(DT_RELA, lo->lo_rel_dyn->os_addr);
			DT_ENTRY_VAL(DT_RELASZ, lo->lo_rel_dyn->os_size);
			DT_ENTRY_VAL(DT_RELAENT, ld->ld_arch->reloc_entsize);
		}
	}

	/*
	 * DT_VERNEED, DT_VERNEEDNUM, DT_VERDEF, DT_VERDEFNUM and
	 * DT_VERSYM.
	 */
	if (lo->lo_verdef != NULL) {
		DT_ENTRY_PTR(DT_VERDEF, lo->lo_verdef->os_addr);
		DT_ENTRY_VAL(DT_VERDEFNUM, lo->lo_verdef_num);
	}
	if (lo->lo_verneed != NULL) {
		DT_ENTRY_PTR(DT_VERNEED, lo->lo_verneed->os_addr);
		DT_ENTRY_VAL(DT_VERNEEDNUM, lo->lo_verneed_num);
	}
	if (lo->lo_versym != NULL)
		DT_ENTRY_PTR(DT_VERSYM, lo->lo_versym->os_addr);

	/* DT_RELCOUNT/DT_RELACOUNT. */
	if (ld->ld_state.ls_relative_reloc > 0)
		DT_ENTRY_VAL(ld->ld_arch->reloc_is_rela ? DT_RELACOUNT :
		    DT_RELCOUNT, ld->ld_state.ls_relative_reloc);

	/* Fill in the space left with DT_NULL entries */
	DT_ENTRY_NULL;
}

static void
_create_dynsym_and_dynstr_section(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os;
	char dynsym_name[] = ".dynsym";
	char dynstr_name[] = ".dynstr";

	/*
	 * Create .dynsym section.
	 */

	HASH_FIND_STR(lo->lo_ostbl, dynsym_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, dynsym_name,
		    SHF_ALLOC);
	os->os_type = SHT_DYNSYM;
	os->os_flags = SHF_ALLOC;
	if (lo->lo_ec == ELFCLASS32) {
		os->os_entsize = sizeof(Elf32_Sym);
		os->os_align = 4;
	} else {
		os->os_entsize = sizeof(Elf64_Sym);
		os->os_align = 8;
	}
	lo->lo_dynsym = os;

	(void) ld_output_create_section_element(ld, os, OET_SYMTAB,
	    ld->ld_dynsym, NULL);

	/*
	 * Create .dynstr section.
	 */

	HASH_FIND_STR(lo->lo_ostbl, dynstr_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, dynstr_name,
		    SHF_ALLOC);
	os->os_type = SHT_STRTAB;
	os->os_flags = SHF_ALLOC;
	os->os_entsize = 0;
	os->os_align = 1;
	lo->lo_dynstr = os;

	(void) ld_output_create_section_element(ld, os, OET_STRTAB,
	    ld->ld_dynstr, NULL);

	if ((lo->lo_dynsym->os_link = strdup(".dynstr")) == NULL)
		ld_fatal_std(ld, "strdup");
}

static void
_check_dso_needed(struct ld *ld, struct ld_output *lo)
{
	struct ld_input *li;
	char *bn;
	int ndx;

	lo->lo_dso_needed = 0;

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		if (li->li_type != LIT_DSO)
			continue;

		if (li->li_dso_refcnt > 0 || !li->li_file->lf_as_needed) {
			lo->lo_dso_needed++;

			if (ld->ld_dynstr == NULL)
				ld->ld_dynstr = ld_strtab_alloc(ld, 0);

			/* Insert DSO name to the .dynstr string table. */
			if (li->li_soname != NULL)
				bn = li->li_soname;
			else {
				if ((bn = strrchr(li->li_name, '/')) == NULL)
					bn = li->li_name;
				else
					bn++;
			}
			ndx = ld_strtab_insert_no_suffix(ld, ld->ld_dynstr,
			    bn);

			/* Save the generated name index for later use. */
			if (lo->lo_dso_nameindex == NULL)
				utarray_new(lo->lo_dso_nameindex, &ut_int_icd);
			utarray_push_back(lo->lo_dso_nameindex, &ndx);
		}
	}
}
