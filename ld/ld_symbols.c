/*-
 * Copyright (c) 2010-2013 Kai Wang
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
#include "ld_dynamic.h"
#include "ld_file.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_symbols.h"
#include "ld_symver.h"
#include "ld_script.h"
#include "ld_strtab.h"

ELFTC_VCSID("$Id: ld_symbols.c 3281 2015-12-11 21:39:23Z kaiwang27 $");

#define	_INIT_SYMTAB_SIZE	128

static void _load_symbols(struct ld *ld, struct ld_file *lf);
static void _load_archive_symbols(struct ld *ld, struct ld_file *lf);
static void _load_elf_symbols(struct ld *ld, struct ld_input *li, Elf *e);
static void _unload_symbols(struct ld_input *li);
static void _add_elf_symbol(struct ld *ld, struct ld_input *li, Elf *e,
    GElf_Sym *sym, size_t strndx, int i);
static void _add_to_dynsym_table(struct ld *ld, struct ld_symbol *lsb);
static void _write_to_dynsym_table(struct ld *ld, struct ld_symbol *lsb);
static void _add_to_symbol_table(struct ld *ld, struct ld_symbol *lsb);
static void _free_symbol_table(struct ld_symbol_table *symtab);
struct ld_symbol_table *_alloc_symbol_table(struct ld *ld);
static int _archive_member_extracted(struct ld_archive *la, off_t off);
static struct ld_archive_member * _extract_archive_member(struct ld *ld,
    struct ld_file *lf, struct ld_archive *la, off_t off);
static void _print_extracted_member(struct ld *ld,
    struct ld_archive_member *lam, struct ld_symbol *lsb);
static void _resolve_and_add_symbol(struct ld *ld, struct ld_symbol *lsb);
static struct ld_symbol *_alloc_symbol(struct ld *ld);
static void _free_symbol(struct ld_symbol *lsb);
static struct ld_symbol *_find_symbol(struct ld_symbol *tbl, char *name);
static void _update_symbol(struct ld_symbol *lsb);

#define	_add_symbol(tbl, s) do {				\
	HASH_ADD_KEYPTR(hh, (tbl), (s)->lsb_longname,		\
	    strlen((s)->lsb_longname), (s));			\
	} while (0)
#define _remove_symbol(tbl, s) do {				\
	HASH_DEL((tbl), (s));					\
	} while (0)
#define _resolve_symbol(_s, s) do {				\
	assert((_s) != (s));					\
	(s)->lsb_ref_dso |= (_s)->lsb_ref_dso;			\
	(s)->lsb_ref_ndso |= (_s)->lsb_ref_ndso;		\
	if ((s)->lsb_prev != NULL) {				\
		(s)->lsb_prev->lsb_ref = (_s);			\
		(_s)->lsb_prev = (s)->lsb_prev;			\
	}							\
	(s)->lsb_prev = (_s);					\
	(_s)->lsb_ref = (s);					\
	} while (0)

void
ld_symbols_cleanup(struct ld *ld)
{
	struct ld_input *li;
	struct ld_symbol *lsb, *_lsb;

	HASH_CLEAR(hh, ld->ld_sym);

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		_unload_symbols(li);
	}

	if (ld->ld_ext_symbols != NULL) {
		STAILQ_FOREACH_SAFE(lsb, ld->ld_ext_symbols, lsb_next, _lsb) {
			STAILQ_REMOVE(ld->ld_ext_symbols, lsb, ld_symbol,
			    lsb_next);
			_free_symbol(lsb);
		}
		free(ld->ld_ext_symbols);
		ld->ld_ext_symbols = NULL;
	}

	if (ld->ld_var_symbols != NULL) {
		STAILQ_FOREACH_SAFE(lsb, ld->ld_var_symbols, lsb_next, _lsb) {
			STAILQ_REMOVE(ld->ld_var_symbols, lsb, ld_symbol,
			    lsb_next);
			_free_symbol(lsb);
		}
		free(ld->ld_var_symbols);
		ld->ld_var_symbols = NULL;
	}

	if (ld->ld_dyn_symbols != NULL) {
		free(ld->ld_dyn_symbols);
		ld->ld_dyn_symbols = NULL;
	}

	if (ld->ld_symtab != NULL) {
		_free_symbol_table(ld->ld_symtab);
		ld->ld_symtab = NULL;
	}

	if (ld->ld_strtab != NULL) {
		ld_strtab_free(ld->ld_strtab);
		ld->ld_strtab = NULL;
	}
}

void
ld_symbols_add_extern(struct ld *ld, char *name)
{
	struct ld_symbol *lsb;

	/* Check if the extern symbol has been added before. */
	if (_find_symbol(ld->ld_sym, name) != NULL)
		return;

	lsb = _alloc_symbol(ld);
	if ((lsb->lsb_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	if ((lsb->lsb_longname = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	if (ld->ld_ext_symbols == NULL) {
		ld->ld_ext_symbols = malloc(sizeof(*ld->ld_ext_symbols));
		if (ld->ld_ext_symbols == NULL)
			ld_fatal_std(ld, "malloc");
		STAILQ_INIT(ld->ld_ext_symbols);
	}
	STAILQ_INSERT_TAIL(ld->ld_ext_symbols, lsb, lsb_next);

	_add_symbol(ld->ld_sym, lsb);
}

void
ld_symbols_add_variable(struct ld *ld, struct ld_script_variable *ldv,
    unsigned provide, unsigned hidden)
{
	struct ld_symbol *lsb;

	lsb = _alloc_symbol(ld);
	if ((lsb->lsb_name = strdup(ldv->ldv_name)) == NULL)
		ld_fatal_std(ld, "strdup");
	if ((lsb->lsb_longname = strdup(ldv->ldv_name)) == NULL)
		ld_fatal_std(ld, "strdup");
	lsb->lsb_var = ldv;
	lsb->lsb_bind = STB_GLOBAL;
	lsb->lsb_shndx = SHN_ABS;
	lsb->lsb_provide = provide;
	if (hidden)
		lsb->lsb_other = STV_HIDDEN;
	lsb->lsb_ref_ndso = 1;
	ldv->ldv_symbol = lsb;
	ldv->ldv_os_ref = ld->ld_scp->lds_last_os_name;
	ldv->ldv_os_base = ld->ld_scp->lds_base_os_name;

	if (ld->ld_var_symbols == NULL) {
		ld->ld_var_symbols = malloc(sizeof(*ld->ld_var_symbols));
		if (ld->ld_var_symbols == NULL)
			ld_fatal_std(ld, "malloc");
		STAILQ_INIT(ld->ld_var_symbols);
	}
	STAILQ_INSERT_TAIL(ld->ld_var_symbols, lsb, lsb_next);

	_resolve_and_add_symbol(ld, lsb);
}

void
ld_symbols_add_internal(struct ld *ld, const char *name, uint64_t size,
    uint64_t value, uint16_t shndx, unsigned char bind, unsigned char type,
    unsigned char other, struct ld_input_section *is,
    struct ld_output_section *preset_os)
{
	struct ld_symbol *lsb;

	lsb = _alloc_symbol(ld);
	if ((lsb->lsb_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	if ((lsb->lsb_longname = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	lsb->lsb_size = size;
	lsb->lsb_value = value;
	lsb->lsb_shndx = shndx;
	lsb->lsb_bind = bind;
	lsb->lsb_type = type;
	lsb->lsb_other = other;
	lsb->lsb_preset_os = preset_os;
	lsb->lsb_ref_ndso = 1;
	lsb->lsb_input = (is == NULL) ? NULL : is->is_input;
	lsb->lsb_is = is;

	_resolve_and_add_symbol(ld, lsb);
}

int
ld_symbols_get_value(struct ld *ld, char *name, uint64_t *val)
{
	struct ld_symbol *lsb;

	if ((lsb = _find_symbol(ld->ld_sym, name)) != NULL)
		*val = lsb->lsb_value;
	else
		return (-1);

	return (0);
}

void
ld_symbols_resolve(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_file *lf;
	struct ld_symbol *lsb, *_lsb;

	if (TAILQ_EMPTY(&ld->ld_lflist)) {
		if (ld->ld_print_version)
			exit(EXIT_SUCCESS);
		else
			ld_fatal(ld, "no input files");
	}

	ls = &ld->ld_state;
	lf = TAILQ_FIRST(&ld->ld_lflist);
	ls->ls_group_level = lf->lf_group_level;

	while (lf != NULL) {
		/* Process archive groups. */
		if (lf->lf_group_level < ls->ls_group_level &&
		    ls->ls_extracted[ls->ls_group_level]) {
			do {
				lf = TAILQ_PREV(lf, ld_file_head, lf_next);
			} while (lf->lf_group_level >= ls->ls_group_level);
			lf = TAILQ_NEXT(lf, lf_next);
			ls->ls_extracted[ls->ls_group_level] = 0;
		}
		ls->ls_group_level = lf->lf_group_level;

		/* Load symbols. */
		ld_file_load(ld, lf);
		if (ls->ls_arch_conflict) {
			ld_file_unload(ld, lf);
			return;
		}
		_load_symbols(ld, lf);
		ld_file_unload(ld, lf);
		lf = TAILQ_NEXT(lf, lf_next);
	}

	/* Print information regarding space allocated for common symbols. */
	if (ld->ld_print_linkmap) {
		printf("\nCommon symbols:\n");
		printf("%-34s %-10s %s\n", "name", "size", "file");
		HASH_ITER(hh, ld->ld_sym, lsb, _lsb) {
			if (lsb->lsb_shndx != SHN_COMMON)
				continue;
			printf("%-34s", lsb->lsb_name);
			if (strlen(lsb->lsb_name) > 34)
				printf("\n%-34s", "");
			printf(" %#-10jx %s\n", (uintmax_t) lsb->lsb_size,
			    ld_input_get_fullname(ld, lsb->lsb_input));
		}
	}
}

void
ld_symbols_update(struct ld *ld)
{
	struct ld_input *li;
	struct ld_symbol *lsb, *_lsb;

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		if (li->li_local == NULL)
			continue;
		STAILQ_FOREACH(lsb, li->li_local, lsb_next)
			_update_symbol(lsb);
	}

	HASH_ITER(hh, ld->ld_sym, lsb, _lsb) {
		/* Skip symbols from DSOs. */
		if (ld_symbols_in_dso(lsb))
			continue;

		_update_symbol(lsb);
	}
}

void
ld_symbols_build_symtab(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_section *os, *_os;
	struct ld_input *li;
	struct ld_input_section *is;
	struct ld_symbol *lsb, *tmp, _lsb;

	lo = ld->ld_output;

	ld->ld_symtab = _alloc_symbol_table(ld);
	ld->ld_strtab = ld_strtab_alloc(ld, 0);

	/* Create an initial symbol at the beginning of symbol table. */
	_lsb.lsb_name = NULL;
	_lsb.lsb_size = 0;
	_lsb.lsb_value = 0;
	_lsb.lsb_shndx = SHN_UNDEF;
	_lsb.lsb_bind = STB_LOCAL;
	_lsb.lsb_type = STT_NOTYPE;
	_lsb.lsb_other = 0;
	_add_to_symbol_table(ld, &_lsb);

	/* Create STT_SECTION symbols. */
	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_empty)
			continue;
		if (os->os_secsym != NULL)
			continue;
		if (os->os_rel)
			continue;
		os->os_secsym = calloc(1, sizeof(*os->os_secsym));
		if (os->os_secsym == NULL)
			ld_fatal_std(ld, "calloc");
		os->os_secsym->lsb_name = NULL;
		os->os_secsym->lsb_size = 0;
		os->os_secsym->lsb_value = os->os_addr;
		os->os_secsym->lsb_shndx = elf_ndxscn(os->os_scn);
		os->os_secsym->lsb_bind = STB_LOCAL;
		os->os_secsym->lsb_type = STT_SECTION;
		os->os_secsym->lsb_other = 0;
		_add_to_symbol_table(ld, os->os_secsym);

		/* Create STT_SECTION symbols for relocation sections. */
		if (os->os_r != NULL && !ld->ld_reloc) {
			_os = os->os_r;
			_os->os_secsym = calloc(1, sizeof(*_os->os_secsym));
			if (_os->os_secsym == NULL)
				ld_fatal_std(ld, "calloc");
			_os->os_secsym->lsb_name = NULL;
			_os->os_secsym->lsb_size = 0;
			_os->os_secsym->lsb_value = _os->os_addr;
			_os->os_secsym->lsb_shndx = elf_ndxscn(_os->os_scn);
			_os->os_secsym->lsb_bind = STB_LOCAL;
			_os->os_secsym->lsb_type = STT_SECTION;
			_os->os_secsym->lsb_other = 0;
			_add_to_symbol_table(ld, _os->os_secsym);
		}
	}

	/* Copy local symbols from each input object. */
	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		if (li->li_local == NULL)
			continue;
		STAILQ_FOREACH(lsb, li->li_local, lsb_next) {
			if (lsb->lsb_type != STT_SECTION &&
			    lsb->lsb_index != 0)
				_add_to_symbol_table(ld, lsb);

			/*
			 * Set the symbol index of the STT_SECTION symbols
			 * to the index of the section symbol for the
			 * corresponding output section. The updated
			 * symbol index will be used by the relocation
			 * serialization function If the linker generates
			 * relocatable object or option -emit-relocs is
			 * specified.
			 */
			if (lsb->lsb_type == STT_SECTION) {
				is = lsb->lsb_is;
				if (is->is_output != NULL) {
					os = is->is_output;
					assert(os->os_secsym != NULL);
					lsb->lsb_out_index =
					    os->os_secsym->lsb_out_index;
				}
			}
		}
	}

	/* Copy resolved global symbols from hash table. */
	HASH_ITER(hh, ld->ld_sym, lsb, tmp) {

		/* Skip undefined/unreferenced symbols from DSO. */
		if (ld_symbols_in_dso(lsb) &&
		    (lsb->lsb_shndx == SHN_UNDEF || !lsb->lsb_ref_ndso))
			continue;

		/*
		 * Skip linker script defined symbols when creating
		 * relocatable output object.
		 */
		if (lsb->lsb_input == NULL && ld->ld_reloc)
			continue;

		/* Skip "provide" symbols that are not referenced. */
		if (lsb->lsb_provide && lsb->lsb_prev == NULL)
			continue;

		if (lsb->lsb_import) {
			if (lsb->lsb_type == STT_FUNC && lsb->lsb_func_addr)
				lsb->lsb_value = lsb->lsb_plt_off;
			else
				lsb->lsb_value = 0;
			lsb->lsb_shndx = SHN_UNDEF;
		}

		_add_to_symbol_table(ld, lsb);
	}
}

void
ld_symbols_scan(struct ld *ld)
{
	struct ld_symbol *lsb, *tmp;

	ld->ld_dynsym = _alloc_symbol_table(ld);
	if (ld->ld_dynstr == NULL)
		ld->ld_dynstr = ld_strtab_alloc(ld, 0);

	/* Reserve space for the initial symbol. */
	ld->ld_dynsym->sy_size++;

	HASH_ITER(hh, ld->ld_sym, lsb, tmp) {

		/*
		 * Warn undefined symbols if the linker is creating an
		 * executable.
		 */
		if ((ld->ld_exec || ld->ld_pie) &&
		    lsb->lsb_shndx == SHN_UNDEF &&
		    lsb->lsb_bind != STB_WEAK)
			ld_warn(ld, "undefined symbol: %s", lsb->lsb_name);

		/*
		 * Allocate space for common symbols and add them to the
		 * special input section COMMON for section layout later.
		 */
		if (lsb->lsb_shndx == SHN_COMMON)
			ld_input_alloc_common_symbol(ld, lsb);

		/*
		 * The code below handles the dynamic symbol table. If
		 * we are doing a -static linking, we can skip.
		 */
		if (!ld->ld_dynamic_link)
			continue;

		/*
		 * Following symbols should not be added to the dynamic
		 * symbol table:
		 *
		 * 1. Do not add undefined symbols in DSOs.
		 */
		if (ld_symbols_in_dso(lsb) && lsb->lsb_shndx == SHN_UNDEF)
			continue;

		/*
		 * Add following symbols to the dynamic symbol table:
		 *
		 * 1. A symbol that is defined in a regular object and
		 *    referenced by a DSO.
		 *
		 * 2. A symbol that is defined in a DSO and referenced
		 *    by a regular object.
		 *
		 * 3. A symbol that is referenced by a dynamic relocation.
		 *
		 * 4. The linker creates a DSO and the symbol is defined
		 *    in a regular object and is visible externally.
		 *
		 */
		if (lsb->lsb_ref_dso && ld_symbols_in_regular(lsb))
			_add_to_dynsym_table(ld, lsb);
		else if (lsb->lsb_ref_ndso && ld_symbols_in_dso(lsb)) {
			lsb->lsb_import = 1;
			lsb->lsb_input->li_dso_refcnt++;
			ld_symver_add_verdef_refcnt(ld, lsb);
			_add_to_dynsym_table(ld, lsb);
		} else if (lsb->lsb_dynrel)
			_add_to_dynsym_table(ld, lsb);
		else if (ld->ld_dso && ld_symbols_in_regular(lsb) &&
		    lsb->lsb_other == STV_DEFAULT &&
		    ld_symver_search_version_script(ld, lsb) != 0)

			_add_to_dynsym_table(ld, lsb);
	}
}

void
ld_symbols_finalize_dynsym(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_symbol *lsb, _lsb;

	lo = ld->ld_output;
	assert(lo != NULL);

	/* Create an initial symbol at the beginning of symbol table. */
	_lsb.lsb_name = NULL;
	_lsb.lsb_nameindex = 0;
	_lsb.lsb_size = 0;
	_lsb.lsb_value = 0;
	_lsb.lsb_shndx = SHN_UNDEF;
	_lsb.lsb_bind = STB_LOCAL;
	_lsb.lsb_type = STT_NOTYPE;
	_lsb.lsb_other = 0;
	_write_to_dynsym_table(ld, &_lsb);

	assert(ld->ld_dyn_symbols != NULL);

	STAILQ_FOREACH(lsb, ld->ld_dyn_symbols, lsb_dyn) {
		if (lsb->lsb_import) {
			memcpy(&_lsb, lsb, sizeof(_lsb));
			if (lsb->lsb_type == STT_FUNC && lsb->lsb_func_addr)
				_lsb.lsb_value = lsb->lsb_plt_off;
			else
				_lsb.lsb_value = 0;
			_lsb.lsb_shndx = SHN_UNDEF;
			_write_to_dynsym_table(ld, &_lsb);
		} else
			_write_to_dynsym_table(ld, lsb);
	}

	lo->lo_dynsym->os_info_val = ld->ld_dynsym->sy_first_nonlocal;
}

/*
 * Retrieve the resolved symbol.
 */
struct ld_symbol *
ld_symbols_ref(struct ld_symbol *lsb)
{

	while (lsb->lsb_ref != NULL)
		lsb = lsb->lsb_ref;

	return (lsb);
}

/*
 * Check if a symbol can be overriden (by symbols in main executable).
 */
int
ld_symbols_overridden(struct ld *ld, struct ld_symbol *lsb)
{

	/* Symbols can be overridden only when we are creating a DSO. */
	if (!ld->ld_dso)
		return (0);

	/* Only visible symbols can be overriden. */
	if (lsb->lsb_other != STV_DEFAULT)
		return (0);

	/*
	 * Symbols converted to local by version script can not be
	 * overridden.
	 */
	if (ld_symver_search_version_script(ld, lsb) == 0)
		return (0);

	/* TODO: other cases. */

	/* Otherwise symbol can be overridden. */
	return (1);
}

/*
 * Check if a symbol is defined in regular object.
 */
int
ld_symbols_in_regular(struct ld_symbol *lsb)
{

	return (lsb->lsb_input == NULL || lsb->lsb_input->li_type != LIT_DSO);
}

/*
 * Check if a symbol is defined in a DSO.
 */
int
ld_symbols_in_dso(struct ld_symbol *lsb)
{

	return (lsb->lsb_input != NULL && lsb->lsb_input->li_type == LIT_DSO);
}

static struct ld_symbol *
_alloc_symbol(struct ld *ld)
{
	struct ld_symbol *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		ld_fatal_std(ld, "calloc");

	return (s);
}

static struct ld_symbol *
_find_symbol(struct ld_symbol *tbl, char *name)
{
	struct ld_symbol *s;

	HASH_FIND_STR(tbl, name, s);
	return (s);
}

#define _prefer_new()	do {			\
	_resolve_symbol(_lsb, lsb);		\
	_remove_symbol(ld->ld_sym, _lsb);	\
	_add_symbol(ld->ld_sym, lsb);		\
	} while (0)

#define _prefer_old()	_resolve_symbol(lsb, _lsb)

#undef	max
#define	max(a, b) ((a) > (b) ? (a) : (b))

static void
_resolve_and_add_symbol(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_symbol *_lsb;
	struct ld_symbol_defver *dv;
	char *name, *sn;

	/* "long" name is a symbol name plus a symbol version string. */
	name = lsb->lsb_longname;

	/* "sn" stores the bare symbol name. */
	sn = lsb->lsb_name;

	/*
	 * Search in the symbol table for the symbol with the same name and
	 * same version.
	 */
	if ((_lsb = _find_symbol(ld->ld_sym, name)) != NULL)
		goto found;

	/*
	 * If there is a default version recorded for the symbol name:
	 *
	 * 1. If the symbol to resolve doesn't have a version, search the
	 *    symbol with the same name and with a default version.
	 *
	 * 2. If the symbol to resolve has the default version, search the
	 *    symbol with the same name but without a version.
	 */
	HASH_FIND_STR(ld->ld_defver, sn, dv);
	if (dv != NULL) {
		if (!strcmp(name, sn)) {
			if ((_lsb = _find_symbol(ld->ld_sym,
			    dv->dv_longname)) != NULL)
				goto found;
		} else if (!strcmp(name, dv->dv_longname)) {
			if ((_lsb = _find_symbol(ld->ld_sym, sn)) != NULL)
				goto found;
		}
	}

	/*
	 * This is *probably* a new symbol, add it to the symbol table
	 * and proceed.
	 *
	 * Note that if one symbol has a version but another one doesn't,
	 * and they are both undefined, there is still a chance that they are
	 * the same symbol. We will solve that when we see the definition.
	 */
	_add_symbol(ld->ld_sym, lsb);

	return;

found:

	/*
	 * We found the same symbol in the symbol table. Now we should
	 * decide which symbol to resolve and which symbol to keep.
	 */

	/*
	 * Verify both symbol has the same TLS (thread local storage)
	 * characteristics.
	 */
	if ((lsb->lsb_type == STT_TLS || _lsb->lsb_type == STT_TLS) &&
	    lsb->lsb_type != _lsb->lsb_type)
		ld_fatal(ld, "TLS symbol %s is non-TLS in another reference");


	/*
	 * If the symbol to resolve is undefined, we always resolve this
	 * symbol to the symbol that is already in the table, no matter it is
	 * defined or not.
	 */

	if (lsb->lsb_shndx == SHN_UNDEF) {
		_prefer_old();
		return;
	}

	/*
	 * If the symbol to resolve is a common symbol and is defined in
	 * a regular object:
	 *
	 * 1. If the symbol in the table is undefined, we prefer the
	 *    common symbol.
	 *
	 * 2. If both symbols are common symbols, we prefer the symbol
	 *    already in the table. However if the symbol in the table
	 *    is found in a DSO, we prefer the common symbol in regular
	 *    object. The size of the symbol we decided to keep is set to
	 *    the larger one of the two.
	 *
	 * 3. If the symbol in the table is defined, we prefer the
	 *    defined symbol. However if the defined symbol is found
	 *    in a DSO, we prefer the common symbol in regular object.
	 *
	 */

	if (lsb->lsb_shndx == SHN_COMMON && ld_symbols_in_regular(lsb)) {
		if (_lsb->lsb_shndx == SHN_UNDEF)
			_prefer_new();
		else if (_lsb->lsb_shndx == SHN_COMMON) {
			if (ld_symbols_in_dso(_lsb)) {
				_prefer_new();
				lsb->lsb_size = max(lsb->lsb_size,
				    _lsb->lsb_size);
			} else {
				_prefer_old();
				_lsb->lsb_size = max(lsb->lsb_size,
				    _lsb->lsb_size);
			}
		} else {
			if (ld_symbols_in_dso(_lsb))
				_prefer_new();
			else
				_prefer_old();
		}
		return;
	}


	/*
	 * If the symbol to resolve is a common symbol and is defined in
	 * a DSO:
	 *
	 * 1. If the symbol in the table is undefined, we prefer the common
	 *    symbol.
	 *
	 * 2. If the symbol in the table is also a common symbol, we prefer
	 *    the one in the table. The size of the symbol we decided to
	 *    keep is set to the larger one of the two.
	 *
	 * 3. If the symbol in the table is defined, we prefer the defined
	 *    symbol.
	 */

	if (lsb->lsb_shndx == SHN_COMMON && ld_symbols_in_dso(lsb)) {
		if (_lsb->lsb_shndx == SHN_UNDEF)
			_prefer_new();
		else if (_lsb->lsb_shndx == SHN_COMMON) {
			_prefer_old();
			_lsb->lsb_size = max(lsb->lsb_size, _lsb->lsb_size);
		} else
			_prefer_old();
		return;
	}

	/*
	 * Now we know the symbol to resolve is a defined symbol. If it is
	 * defined in a regular object:
	 *
	 * 1. If the symbol in the table is undefined, we prefer the defined
	 *    symbol. (no doubt!)
	 *
	 * 2. If the symbol in the table is a common symbol, we perfer the
	 *    defined symbol.
	 *
	 * 3. If the symbol in the table is also a defined symbol, we need to
	 * consider:
	 *
	 * a) If the symbol in the table is also defined in a regular object,
	 *    and both symbols are strong, we have a multi-definition error.
	 *    If only one of them is strong, we pick that one. If both of them
	 *    are weak, we pick the one that is already in the table. (fisrt
	 *    seen). Another case is that if one of them is a "provide" symbol,
	 *    we prefer the one that is not "provide".
	 *
	 * b) If the symbol in the table is defined in a DSO, we pick the one
	 *    defined in the regular object. (no matter weak or strong!)
	 */

	if (ld_symbols_in_regular(lsb)) {
		if (_lsb->lsb_shndx == SHN_UNDEF ||
		    _lsb->lsb_shndx == SHN_COMMON)
			_prefer_new();
		else {
			if (ld_symbols_in_regular(_lsb)) {
				if (_lsb->lsb_provide && !lsb->lsb_provide)
					_prefer_new();
				else if (lsb->lsb_bind == _lsb->lsb_bind) {
					if (lsb->lsb_bind == STB_WEAK)
						_prefer_old();
					else
						ld_fatal(ld, "multiple "
						    "definition of symbol %s",
						    lsb->lsb_longname);
				} else if (lsb->lsb_bind == STB_WEAK)
					_prefer_old();
				else
					_prefer_new();
			} else
				_prefer_new();
		}
		return;
	}

	/*
	 * Last case, the symbol to resolve is a defined symbol in a DSO.
	 *
	 * 1. If the symbol in the table is undefined, we prefer the defined
	 *    symbol. (no doubt!)
	 *
	 * 2. If the symbol in the table is a common symbol: if it is in a
	 *    regular object and the defined DSO symbol is a function, we
	 *    prefer the common symbol. For all the other cases, we prefer
	 *    the defined symbol in the DSO.
	 *
	 * 3. If the symbol in the table is a defined symbol. We always pick
	 *    the symbol already in the table. (no matter it's in regular
	 *    object or DSO, strong or weak)
	 */

	if (_lsb->lsb_shndx != SHN_UNDEF && _lsb->lsb_shndx != SHN_COMMON)
		_prefer_old();
	else if (_lsb->lsb_shndx == SHN_COMMON &&
	    ld_symbols_in_regular(_lsb) && lsb->lsb_type == STT_FUNC)
		_prefer_old();
	else {
		_prefer_new();

		/*
		 * Now we added a defined symbol from DSO. Here we should
		 * check if the DSO symbol has a default symbol version.
		 * If so, we search the symbol table for the symbol with the
		 * same name but without a symbol version. If there is one,
		 * we resolve the found symbol to this newly added DSO symbol
		 * and remove the found symbol from the table.
		 */
		HASH_FIND_STR(ld->ld_defver, sn, dv);
		if (dv != NULL) {
			if ((_lsb = _find_symbol(ld->ld_sym, sn)) != NULL) {
				_resolve_symbol(_lsb, lsb);
				_remove_symbol(ld->ld_sym, _lsb);
			}
		}
	}
}

static void
_add_elf_symbol(struct ld *ld, struct ld_input *li, Elf *e, GElf_Sym *sym,
    size_t strndx, int i)
{
	struct ld_symbol *lsb;
	struct ld_symbol_defver *dv;
	char *name;
	int j, len, ndx;
	unsigned char st_bind;

	if ((name = elf_strptr(e, strndx, sym->st_name)) == NULL)
		return;

	/*
	 * First check if the section this symbol refers to is belong
	 * to a section group that has been removed.
	 */
	st_bind = GELF_ST_BIND(sym->st_info);
	if (sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_COMMON &&
	    sym->st_shndx != SHN_ABS && sym->st_shndx < li->li_shnum - 1 &&
	    li->li_is[sym->st_shndx].is_discard) {
		st_bind = GELF_ST_BIND(sym->st_info);
		if (st_bind == STB_GLOBAL || st_bind == STB_WEAK) {
			/*
			 * For symbol with STB_GLOBAL or STB_WEAK binding,
			 * we convert it to an undefined symbol.
			 */
			sym->st_shndx = SHN_UNDEF;
		} else {
			/*
			 * Local symbols are discarded, if the section they
			 * refer to are removed.
			 */
			return;
		}
	}

	lsb = _alloc_symbol(ld);

	if ((lsb->lsb_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	lsb->lsb_value = sym->st_value;
	lsb->lsb_size = sym->st_size;
	lsb->lsb_bind = GELF_ST_BIND(sym->st_info);
	lsb->lsb_type = GELF_ST_TYPE(sym->st_info);
	lsb->lsb_other = sym->st_other;
	lsb->lsb_shndx = sym->st_shndx;
	lsb->lsb_index = i;
	lsb->lsb_input = li;
	lsb->lsb_ver = NULL;

	if (lsb->lsb_shndx != SHN_UNDEF && lsb->lsb_shndx != SHN_ABS) {
		if (lsb->lsb_shndx == SHN_COMMON)
			lsb->lsb_is = &li->li_is[li->li_shnum - 1];
		else {
			assert(lsb->lsb_shndx < li->li_shnum - 1);
			lsb->lsb_is = &li->li_is[lsb->lsb_shndx];
		}
	}

	if (li->li_type == LIT_DSO)
		lsb->lsb_ref_dso = 1;
	else
		lsb->lsb_ref_ndso = 1;

	/* Find out symbol version info. */
	j = 0;
	if (li->li_file->lf_type == LFT_DSO && li->li_vername != NULL &&
	    li->li_versym != NULL && (size_t) i < li->li_versym_sz) {
		j = li->li_versym[i];
		ndx = j & ~0x8000;
		if ((size_t) ndx < li->li_vername_sz) {
			lsb->lsb_ver = li->li_vername[ndx];
#if 0
			printf("symbol: %s ver: %s\n", lsb->lsb_name,
			    lsb->lsb_ver);
#endif
			if (j >= 2 && (j & 0x8000) == 0 &&
			    lsb->lsb_shndx != SHN_UNDEF)
				lsb->lsb_default = 1;
		}
	}

	/* Build "long" symbol name which is used for hash key. */
	if (lsb->lsb_ver == NULL || j < 2) {
		lsb->lsb_longname = strdup(lsb->lsb_name);
		if (lsb->lsb_longname == NULL)
			ld_fatal_std(ld, "strdup");
	} else {
		len = strlen(lsb->lsb_name) + strlen(lsb->lsb_ver) + 2;
		if ((lsb->lsb_longname = malloc(len)) == NULL)
			ld_fatal_std(ld, "malloc");
		snprintf(lsb->lsb_longname, len, "%s@%s", lsb->lsb_name,
		    lsb->lsb_ver);
	}

	/* Keep track of default versions. */
	if (lsb->lsb_default) {
		if ((dv = calloc(1, sizeof(*dv))) == NULL)
			ld_fatal(ld, "calloc");
		dv->dv_name = lsb->lsb_name;
		dv->dv_longname = lsb->lsb_longname;
		dv->dv_ver = lsb->lsb_ver;
		HASH_ADD_KEYPTR(hh, ld->ld_defver, dv->dv_name,
		    strlen(dv->dv_name), dv);
	}

	/*
	 * Insert symbol to input object internal symbol list and
	 * perform symbol resolving.
	 */
	ld_input_add_symbol(ld, li, lsb);
	if (lsb->lsb_bind != STB_LOCAL)
		_resolve_and_add_symbol(ld, lsb);
}

static int
_archive_member_extracted(struct ld_archive *la, off_t off)
{
	struct ld_archive_member *_lam;

	HASH_FIND(hh, la->la_m, &off, sizeof(off), _lam);
	if (_lam != NULL)
		return (1);

	return (0);
}

static struct ld_archive_member *
_extract_archive_member(struct ld *ld, struct ld_file *lf,
    struct ld_archive *la, off_t off)
{
	Elf *e;
	Elf_Arhdr *arhdr;
	struct ld_archive_member *lam;
	struct ld_input *li;

	if (elf_rand(lf->lf_elf, off) == 0)
		ld_fatal(ld, "%s: elf_rand failed: %s", lf->lf_name,
		    elf_errmsg(-1));

	if ((e = elf_begin(-1, ELF_C_READ, lf->lf_elf)) == NULL)
		ld_fatal(ld, "%s: elf_begin failed: %s", lf->lf_name,
		    elf_errmsg(-1));

	if ((arhdr = elf_getarhdr(e)) == NULL)
		ld_fatal(ld, "%s: elf_getarhdr failed: %s", lf->lf_name,
		    elf_errmsg(-1));

	/* Keep record of extracted members. */
	if ((lam = calloc(1, sizeof(*lam))) == NULL)
		ld_fatal_std(ld, "calloc");

	lam->lam_ar_name = strdup(lf->lf_name);
	if (lam->lam_ar_name == NULL)
		ld_fatal_std(ld, "strdup");

	lam->lam_name = strdup(arhdr->ar_name);
	if (lam->lam_name == NULL)
		ld_fatal_std(ld, "strdup");

	lam->lam_off = off;

	HASH_ADD(hh, la->la_m, lam_off, sizeof(lam->lam_off), lam);

	/* Allocate input object for this member. */
	li = ld_input_alloc(ld, lf, lam->lam_name);
	li->li_lam = lam;
	lam->lam_input = li;

	/* Load the symbols of this member. */
	_load_elf_symbols(ld, li, e);

	elf_end(e);

	return (lam);
}

static void
_print_extracted_member(struct ld *ld, struct ld_archive_member *lam,
    struct ld_symbol *lsb)
{
	struct ld_state *ls;
	char *c1, *c2;

	ls = &ld->ld_state;

	if (!ls->ls_archive_mb_header) {
		printf("Extracted archive members:\n\n");
		ls->ls_archive_mb_header = 1;
	}

	c1 = ld_input_get_fullname(ld, lam->lam_input);
	c2 = ld_input_get_fullname(ld, lsb->lsb_input);

	printf("%-30s", c1);
	if (strlen(c1) >= 30) {
		printf("\n%-30s", "");
	}
	printf("%s (%s)\n", c2, lsb->lsb_name);
}

static void
_load_archive_symbols(struct ld *ld, struct ld_file *lf)
{
	struct ld_state *ls;
	struct ld_archive *la;
	struct ld_archive_member *lam;
	struct ld_symbol *lsb;
	Elf_Arsym *as;
	size_t c;
	int extracted, i;

	assert(lf != NULL && lf->lf_type == LFT_ARCHIVE);
	assert(lf->lf_ar != NULL);

	ls = &ld->ld_state;
	la = lf->lf_ar;
	if ((as = elf_getarsym(lf->lf_elf, &c)) == NULL)
		ld_fatal(ld, "%s: elf_getarsym failed: %s", lf->lf_name,
		    elf_errmsg(-1));
	do {
		extracted = 0;
		for (i = 0; (size_t) i < c; i++) {
			if (as[i].as_name == NULL)
				break;
			if (_archive_member_extracted(la, as[i].as_off))
				continue;
			if ((lsb = _find_symbol(ld->ld_sym, as[i].as_name)) !=
			    NULL) {
				lam = _extract_archive_member(ld, lf, la,
				    as[i].as_off);
				extracted = 1;
				ls->ls_extracted[ls->ls_group_level] = 1;
				if (ld->ld_print_linkmap)
					_print_extracted_member(ld, lam, lsb);
			}
		}
	} while (extracted);
}

static void
_load_elf_symbols(struct ld *ld, struct ld_input *li, Elf *e)
{
	struct ld_input_section *is;
	Elf_Scn *scn_sym, *scn_dynamic;
	Elf_Scn *scn_versym, *scn_verneed, *scn_verdef;
	Elf_Data *d;
	GElf_Sym sym;
	GElf_Shdr shdr;
	size_t dyn_strndx, strndx;
	int elferr, i;

	/* Load section list from input object. */
	ld_input_init_sections(ld, li, e);

	strndx = dyn_strndx = SHN_UNDEF;
	scn_sym = scn_versym = scn_verneed = scn_verdef = scn_dynamic = NULL;

	for (i = 0; (uint64_t) i < li->li_shnum - 1; i++) {
		is = &li->li_is[i];
		if (li->li_type == LIT_DSO) {
			if (is->is_type == SHT_DYNSYM) {
				scn_sym = elf_getscn(e, is->is_index);
				strndx = is->is_link;
			} else if (is->is_type == SHT_SUNW_versym)
				scn_versym = elf_getscn(e, is->is_index);
			else if (is->is_type == SHT_SUNW_verneed)
				scn_verneed = elf_getscn(e, is->is_index);
			else if (is->is_type == SHT_SUNW_verdef)
				scn_verdef = elf_getscn(e, is->is_index);
			else if (is->is_type == SHT_DYNAMIC) {
				scn_dynamic = elf_getscn(e, is->is_index);
				dyn_strndx = is->is_link;
			}
		} else {
			if (is->is_type == SHT_SYMTAB) {
				scn_sym = elf_getscn(e, is->is_index);
				strndx = is->is_link;
			}
		}
	}

	if (scn_sym == NULL || strndx == SHN_UNDEF)
		return;

	ld_symver_load_symbol_version_info(ld, li, e, scn_versym, scn_verneed,
	    scn_verdef);

	if (scn_dynamic != NULL)
		ld_dynamic_load_dso_dynamic(ld, li, e, scn_dynamic,
		    dyn_strndx);

	if (gelf_getshdr(scn_sym, &shdr) != &shdr) {
		ld_warn(ld, "%s: gelf_getshdr failed: %s", li->li_name,
		    elf_errmsg(-1));
		return;
	}

	(void) elf_errno();
	if ((d = elf_getdata(scn_sym, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_warn(ld, "%s: elf_getdata failed: %s", li->li_name,
			    elf_errmsg(elferr));
		/* Empty symbol table section? */
		return;
	}

	li->li_symnum = d->d_size / shdr.sh_entsize;
	for (i = 0; (uint64_t) i < li->li_symnum; i++) {
		if (gelf_getsym(d, i, &sym) != &sym)
			ld_warn(ld, "%s: gelf_getsym failed: %s", li->li_name,
			    elf_errmsg(-1));
		_add_elf_symbol(ld, li, e, &sym, strndx, i);
	}

}

static void
_load_symbols(struct ld *ld, struct ld_file *lf)
{

	if (lf->lf_type == LFT_ARCHIVE)
		_load_archive_symbols(ld, lf);
	else {
		lf->lf_input = ld_input_alloc(ld, lf, lf->lf_name);
		_load_elf_symbols(ld, lf->lf_input, lf->lf_elf);
	}
}

static void
_unload_symbols(struct ld_input *li)
{
	int i;

	if (li->li_symindex == NULL)
		return;

	for (i = 0; (uint64_t) i < li->li_symnum; i++)
		_free_symbol(li->li_symindex[i]);
}

static void
_free_symbol(struct ld_symbol *lsb)
{

	if (lsb == NULL)
		return;

	free(lsb->lsb_name);
	free(lsb->lsb_longname);
	free(lsb);
}

static void
_update_symbol(struct ld_symbol *lsb)
{
	struct ld_input_section *is;
	struct ld_output_section *os;

	if (lsb->lsb_preset_os != NULL) {
		lsb->lsb_value = lsb->lsb_preset_os->os_addr;
		lsb->lsb_shndx = elf_ndxscn(lsb->lsb_preset_os->os_scn);
		return;
	}

	if (lsb->lsb_shndx == SHN_ABS)
		return;

	if (lsb->lsb_input != NULL) {
		is = lsb->lsb_is;
		if (is == NULL || (os = is->is_output) == NULL)
			return;
		lsb->lsb_value += os->os_addr + is->is_reloff;
		lsb->lsb_shndx = elf_ndxscn(os->os_scn);
	}
}

struct ld_symbol_table *
_alloc_symbol_table(struct ld *ld)
{
	struct ld_symbol_table *symtab;

	if ((symtab = calloc(1, sizeof(*ld->ld_symtab))) == NULL)
		ld_fatal_std(ld, "calloc");

	return (symtab);
}

static void
_add_to_dynsym_table(struct ld *ld, struct ld_symbol *lsb)
{

	assert(ld->ld_dynsym != NULL && ld->ld_dynstr != NULL);

	if (ld->ld_dyn_symbols == NULL) {
		ld->ld_dyn_symbols = malloc(sizeof(*ld->ld_dyn_symbols));
		if (ld->ld_dyn_symbols == NULL)
			ld_fatal_std(ld, "malloc");
		STAILQ_INIT(ld->ld_dyn_symbols);
	}
	STAILQ_INSERT_TAIL(ld->ld_dyn_symbols, lsb, lsb_dyn);

	lsb->lsb_nameindex = ld_strtab_insert_no_suffix(ld, ld->ld_dynstr,
	    lsb->lsb_name);

	lsb->lsb_dyn_index = ld->ld_dynsym->sy_size++;
}

static void
_write_to_dynsym_table(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_output *lo;
	struct ld_symbol_table *symtab;
	Elf32_Sym *s32;
	Elf64_Sym *s64;
	size_t es;

	assert(lsb != NULL);
	assert(ld->ld_dynsym != NULL && ld->ld_dynstr != NULL);
	symtab = ld->ld_dynsym;

	lo = ld->ld_output;
	assert(lo != NULL);

	es = (lo->lo_ec == ELFCLASS32) ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym);

	/* Allocate buffer for the dynsym table. */
	if (symtab->sy_buf == NULL) {
		symtab->sy_buf = malloc(symtab->sy_size * es);
		symtab->sy_write_pos = 0;
	}

	if (lo->lo_ec == ELFCLASS32) {
		s32 = symtab->sy_buf;
		s32 += symtab->sy_write_pos;
		s32->st_name = lsb->lsb_nameindex;
		s32->st_info = ELF32_ST_INFO(lsb->lsb_bind, lsb->lsb_type);
		s32->st_other = lsb->lsb_other;
		s32->st_shndx = lsb->lsb_shndx;
		s32->st_value = lsb->lsb_value;
		s32->st_size = lsb->lsb_size;
	} else {
		s64 = symtab->sy_buf;
		s64 += symtab->sy_write_pos;
		s64->st_name = lsb->lsb_nameindex;
		s64->st_info = ELF64_ST_INFO(lsb->lsb_bind, lsb->lsb_type);
		s64->st_other = lsb->lsb_other;
		s64->st_shndx = lsb->lsb_shndx;
		s64->st_value = lsb->lsb_value;
		s64->st_size = lsb->lsb_size;
	}

	/* Remember the index for the first non-local symbol. */
	if (symtab->sy_first_nonlocal == 0 && lsb->lsb_bind != STB_LOCAL)
		symtab->sy_first_nonlocal = symtab->sy_write_pos;

	symtab->sy_write_pos++;
}

static void
_add_to_symbol_table(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_output *lo;
	struct ld_symbol_table *symtab;
	struct ld_strtab *strtab;
	Elf32_Sym *s32;
	Elf64_Sym *s64;
	size_t es;

	assert(lsb != NULL);
	assert(ld->ld_symtab != NULL && ld->ld_strtab != NULL);
	symtab = ld->ld_symtab;
	strtab = ld->ld_strtab;

	lo = ld->ld_output;
	assert(lo != NULL);

	es = (lo->lo_ec == ELFCLASS32) ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym);

	/* Allocate/Reallocate buffer for the symbol table. */
	if (symtab->sy_buf == NULL) {
		symtab->sy_size = 0;
		symtab->sy_cap = _INIT_SYMTAB_SIZE;
		symtab->sy_buf = malloc(symtab->sy_cap * es);
		if (symtab->sy_buf == NULL)
			ld_fatal_std(ld, "malloc");
	} else if (symtab->sy_size >= symtab->sy_cap) {
		symtab->sy_cap *= 2;
		symtab->sy_buf = realloc(symtab->sy_buf, symtab->sy_cap * es);
		if (symtab->sy_buf == NULL)
			ld_fatal_std(ld, "relloc");
	}

	/*
	 * Insert the symbol into the symbol table and the symbol name to
	 * the assoicated name string table.
	 */
	lsb->lsb_nameindex = ld_strtab_insert_no_suffix(ld, strtab,
	    lsb->lsb_name);
	if (lo->lo_ec == ELFCLASS32) {
		s32 = symtab->sy_buf;
		s32 += symtab->sy_size;
		s32->st_name = lsb->lsb_nameindex;
		s32->st_info = ELF32_ST_INFO(lsb->lsb_bind, lsb->lsb_type);
		s32->st_other = lsb->lsb_other;
		s32->st_shndx = lsb->lsb_shndx;
		s32->st_value = lsb->lsb_value;
		s32->st_size = lsb->lsb_size;
	} else {
		s64 = symtab->sy_buf;
		s64 += symtab->sy_size;
		s64->st_name = lsb->lsb_nameindex;
		s64->st_info = ELF64_ST_INFO(lsb->lsb_bind, lsb->lsb_type);
		s64->st_other = lsb->lsb_other;
		s64->st_shndx = lsb->lsb_shndx;
		s64->st_value = lsb->lsb_value;
		s64->st_size = lsb->lsb_size;
	}

	/* Remember the index for the first non-local symbol. */
	if (symtab->sy_first_nonlocal == 0 && lsb->lsb_bind != STB_LOCAL)
		symtab->sy_first_nonlocal = symtab->sy_size;

	lsb->lsb_out_index = symtab->sy_size;
	symtab->sy_size++;
}

static void
_free_symbol_table(struct ld_symbol_table *symtab)
{

	if (symtab == NULL)
		return;

	free(symtab->sy_buf);
	free(symtab);
}
