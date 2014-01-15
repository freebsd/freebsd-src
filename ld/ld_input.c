/*-
 * Copyright (c) 2011-2013 Kai Wang
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
#include "ld_file.h"
#include "ld_input.h"
#include "ld_symbols.h"

ELFTC_VCSID("$Id: ld_input.c 2960 2013-08-25 03:13:07Z kaiwang27 $");

/*
 * Support routines for input section handling.
 */

static void _discard_section_group(struct ld *ld, struct ld_input *li,
    Elf_Scn *scn);
static off_t _offset_sort(struct ld_archive_member *a,
    struct ld_archive_member *b);

#define _MAX_INTERNAL_SECTIONS		16

void
ld_input_init(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;

	assert(STAILQ_EMPTY(&ld->ld_lilist));

	/*
	 * Create an internal pseudo input object to hold internal
	 * input sections.
	 */

	li = ld_input_alloc(ld, NULL, NULL);

	li->li_is = calloc(_MAX_INTERNAL_SECTIONS,
	    sizeof(struct ld_input_section));
	if (li->li_is == NULL)
		ld_fatal_std(ld, "calloc");

	STAILQ_INSERT_TAIL(&ld->ld_lilist, li, li_next);

	/*
	 * Create an initial SHT_NULL section for the pseudo input object,
	 * so all the internal sections will have valid section index.
	 * (other than SHN_UNDEF)
	 */
	is = &li->li_is[li->li_shnum];
	if ((is->is_name = strdup("")) == NULL)
		ld_fatal_std(ld, "strdup");
	is->is_input = li;
	is->is_type = SHT_NULL;
	is->is_index = li->li_shnum;
	li->li_shnum++;
}

struct ld_input_section *
ld_input_add_internal_section(struct ld *ld, const char *name)
{
	struct ld_input *li;
	struct ld_input_section *is;

	li = STAILQ_FIRST(&ld->ld_lilist);
	assert(li != NULL);

	if (li->li_shnum >= _MAX_INTERNAL_SECTIONS)
		ld_fatal(ld, "Internal: not enough buffer for internal "
		    "sections");

	is = &li->li_is[li->li_shnum];
	if ((is->is_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	is->is_input = li;
	is->is_index = li->li_shnum;

	/* Use a hash table to accelerate lookup for internal sections. */
	HASH_ADD_KEYPTR(hh, li->li_istbl, is->is_name, strlen(is->is_name),
	    is);

	li->li_shnum++;

	return (is);
}

struct ld_input_section *
ld_input_find_internal_section(struct ld *ld, const char *name)
{
	struct ld_input *li;
	struct ld_input_section *is;
	char _name[32];

	li = STAILQ_FIRST(&ld->ld_lilist);
	assert(li != NULL);

	snprintf(_name, sizeof(_name), "%s", name);
	HASH_FIND_STR(li->li_istbl, _name, is);

	return (is);
}

uint64_t
ld_input_reserve_ibuf(struct ld_input_section *is, uint64_t n)
{
	uint64_t off;

	assert(is->is_entsize != 0);

	off = is->is_size;
	is->is_size += n * is->is_entsize;

	return (off);
}

void
ld_input_alloc_internal_section_buffers(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;
	int i;

	li = STAILQ_FIRST(&ld->ld_lilist);
	assert(li != NULL);

	for (i = 0; (uint64_t) i < li->li_shnum; i++) {
		is = &li->li_is[i];

		if (is->is_type == SHT_NOBITS || is->is_size == 0 ||
		    is->is_dynrel)
			continue;

		if ((is->is_ibuf = malloc(is->is_size)) == NULL)
			ld_fatal_std(ld, "malloc");
	}
}

void
ld_input_cleanup(struct ld *ld)
{
	struct ld_input *li, *_li;
	int i;

	STAILQ_FOREACH_SAFE(li, &ld->ld_lilist, li_next, _li) {
		STAILQ_REMOVE(&ld->ld_lilist, li, ld_input, li_next);
		if (li->li_symindex)
			free(li->li_symindex);
		if (li->li_local)
			free(li->li_local);
		if (li->li_versym)
			free(li->li_versym);
		if (li->li_vername) {
			for (i = 0; (size_t) i < li->li_vername_sz; i++)
				if (li->li_vername[i])
					free(li->li_vername[i]);
			free(li->li_vername);
		}
		if (li->li_is)
			free(li->li_is);
		if (li->li_fullname)
			free(li->li_fullname);
		if (li->li_name)
			free(li->li_name);
		if (li->li_soname)
			free(li->li_soname);
		free(li);
	}
}

void
ld_input_add_symbol(struct ld *ld, struct ld_input *li, struct ld_symbol *lsb)
{

	if (li->li_symindex == NULL) {
		assert(li->li_symnum != 0);
		li->li_symindex = calloc(li->li_symnum,
		    sizeof(*li->li_symindex));
		if (li->li_symindex == NULL)
			ld_fatal_std(ld, "calloc");
	}

	li->li_symindex[lsb->lsb_index] = lsb;

	if (lsb->lsb_bind == STB_LOCAL) {
		if (li->li_local == NULL) {
			li->li_local = calloc(1, sizeof(*li->li_local));
			if (li->li_local == NULL)
				ld_fatal_std(ld, "calloc");
			STAILQ_INIT(li->li_local);
		}
		STAILQ_INSERT_TAIL(li->li_local, lsb, lsb_next);
	}
}

struct ld_input *
ld_input_alloc(struct ld *ld, struct ld_file *lf, const char *name)
{
	struct ld_input *li;

	if ((li = calloc(1, sizeof(*li))) == NULL)
		ld_fatal_std(ld, "calloc");

	if (name != NULL && (li->li_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	li->li_file = lf;

	if (lf != NULL) {
		switch (lf->lf_type) {
		case LFT_ARCHIVE:
		case LFT_RELOCATABLE:
			li->li_type = LIT_RELOCATABLE;
			break;
		case LFT_DSO:
			li->li_type = LIT_DSO;
			break;
		case LFT_BINARY:
		case LFT_UNKNOWN:
		default:
			li->li_type = LIT_UNKNOWN;
			break;
		}
	} else
		li->li_type = LIT_RELOCATABLE;

	return (li);
}

char *
ld_input_get_fullname(struct ld *ld, struct ld_input *li)
{
	struct ld_archive_member *lam;
	size_t len;

	if (li->li_fullname != NULL)
		return (li->li_fullname);

	if (li->li_lam == NULL)
		return (li->li_name);

	lam = li->li_lam;
	len = strlen(lam->lam_ar_name) + strlen(lam->lam_name) + 3;
	if ((li->li_fullname = malloc(len)) == NULL)
		ld_fatal_std(ld, "malloc");
	snprintf(li->li_fullname, len, "%s(%s)", lam->lam_ar_name,
	    lam->lam_name);

	return  (li->li_fullname);
}

void
ld_input_link_objects(struct ld *ld)
{
	struct ld_file *lf;
	struct ld_archive_member *lam, *tmp;
	struct ld_input *li;

	TAILQ_FOREACH(lf, &ld->ld_lflist, lf_next) {
		if (lf->lf_ar != NULL) {
			HASH_SORT(lf->lf_ar->la_m, _offset_sort);
			HASH_ITER(hh, lf->lf_ar->la_m, lam, tmp) {
				li = lam->lam_input;
				if (li != NULL)
					STAILQ_INSERT_TAIL(&ld->ld_lilist, li,
					    li_next);
			}
		} else {
			li = lf->lf_input;
			if (li != NULL)
				STAILQ_INSERT_TAIL(&ld->ld_lilist, li, li_next);
		}
	}
}

void *
ld_input_get_section_rawdata(struct ld *ld, struct ld_input_section *is)
{
	Elf *e;
	Elf_Scn *scn;
	Elf_Data *d;
	struct ld_input *li;
	char *buf;
	int elferr;

	li = is->is_input;
	e = li->li_elf;
	assert(e != NULL);

	if ((scn = elf_getscn(e, is->is_index)) == NULL)
		ld_fatal(ld, "%s(%s): elf_getscn failed: %s", li->li_name,
		    is->is_name, elf_errmsg(-1));

	(void) elf_errno();
	if ((d = elf_rawdata(scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_warn(ld, "%s(%s): elf_rawdata failed: %s",
			    li->li_name, is->is_name, elf_errmsg(elferr));
		return (NULL);
	}

	if (d->d_buf == NULL || d->d_size == 0)
		return (NULL);

	if ((buf = malloc(d->d_size)) == NULL)
		ld_fatal_std(ld, "malloc");

	memcpy(buf, d->d_buf, d->d_size);

	return (buf);
}

void
ld_input_load(struct ld *ld, struct ld_input *li)
{
	struct ld_state *ls;
	struct ld_file *lf;
	struct ld_archive_member *lam;

	if (li->li_file == NULL)
		return;

	assert(li->li_elf == NULL);
	ls = &ld->ld_state;
	if (li->li_file != ls->ls_file) {
		if (ls->ls_file != NULL)
			ld_file_unload(ld, ls->ls_file);
		ld_file_load(ld, li->li_file);
	}
	lf = li->li_file;
	if (lf->lf_ar != NULL) {
		assert(li->li_lam != NULL);
		lam = li->li_lam;
		if (elf_rand(lf->lf_elf, lam->lam_off) != lam->lam_off)
			ld_fatal(ld, "%s: elf_rand: %s", lf->lf_name,
			    elf_errmsg(-1));
		if ((li->li_elf = elf_begin(-1, ELF_C_READ, lf->lf_elf)) ==
		    NULL)
			ld_fatal(ld, "%s: elf_begin: %s", lf->lf_name,
			    elf_errmsg(-1));
	} else
		li->li_elf = lf->lf_elf;
}

void
ld_input_unload(struct ld *ld, struct ld_input *li)
{
	struct ld_file *lf;

	(void) ld;

	if (li->li_file == NULL)
		return;

	assert(li->li_elf != NULL);
	lf = li->li_file;
	if (lf->lf_ar != NULL)
		(void) elf_end(li->li_elf);
	li->li_elf = NULL;
}

void
ld_input_init_sections(struct ld *ld, struct ld_input *li, Elf *e)
{
	struct ld_input_section *is;
	struct ld_section_group *sg;
	Elf_Scn *scn, *_scn;
	Elf_Data *d, *_d;
	char *name;
	GElf_Shdr sh;
	GElf_Sym sym;
	size_t shstrndx, strndx, ndx;
	int elferr;

	_d = NULL;
	strndx = 0;

	if (elf_getshdrnum(e, &li->li_shnum) < 0)
		ld_fatal(ld, "%s: elf_getshdrnum: %s", li->li_name,
		    elf_errmsg(-1));

	/* Allocate one more pseudo section to hold common symbols */
	li->li_shnum++;

	assert(li->li_is == NULL);
	if ((li->li_is = calloc(li->li_shnum, sizeof(*is))) == NULL)
		ld_fatal_std(ld, "%s: calloc: %s", li->li_name);

	if (elf_getshdrstrndx(e, &shstrndx) < 0)
		ld_fatal(ld, "%s: elf_getshdrstrndx: %s", li->li_name,
		    elf_errmsg(-1));

	(void) elf_errno();
	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &sh) != &sh)
			ld_fatal(ld, "%s: gelf_getshdr: %s", li->li_name,
			    elf_errmsg(-1));

		if ((name = elf_strptr(e, shstrndx, sh.sh_name)) == NULL)
			ld_fatal(ld, "%s: elf_strptr: %s", li->li_name,
			    elf_errmsg(-1));

		if ((ndx = elf_ndxscn(scn)) == SHN_UNDEF)
			ld_fatal(ld, "%s: elf_ndxscn: %s", li->li_name,
			    elf_errmsg(-1));

		if (ndx >= li->li_shnum - 1)
			ld_fatal(ld, "%s: section index of '%s' section is"
			    " invalid", li->li_name, name);

		is = &li->li_is[ndx];
		if ((is->is_name = strdup(name)) == NULL)
			ld_fatal_std(ld, "%s: calloc", li->li_name);
		is->is_off = sh.sh_offset;
		is->is_size = sh.sh_size;
		is->is_entsize = sh.sh_entsize;
		is->is_addr = sh.sh_addr;
		is->is_align = sh.sh_addralign;
		is->is_type = sh.sh_type;
		is->is_flags = sh.sh_flags;
		is->is_link = sh.sh_link;
		is->is_info = sh.sh_info;
		is->is_index = elf_ndxscn(scn);
		is->is_shrink = 0;
		is->is_input = li;

		/*
		 * Section groups are identified by their signatures.
		 * A section group's signature is used to compare with the
		 * the section groups that are already added. If a match
		 * is found, the sections included in this section group
		 * should be discarded.
		 *
		 * Note that since signatures are stored in the symbol
		 * table, in order to handle that here we have to load
		 * the symbol table earlier.
		 */
		if (is->is_type == SHT_GROUP) {
			is->is_discard = 1;
			if (_d == NULL) {
				_scn = elf_getscn(e, is->is_link);
				if (_scn == NULL) {
					ld_warn(ld, "%s: elf_getscn failed"
					    " with the `sh_link' of group"
					    " section %ju as index: %s",
					    li->li_name, ndx, elf_errmsg(-1));
					continue;
				}
				if (gelf_getshdr(_scn, &sh) != &sh) {
					ld_warn(ld, "%s: gelf_getshdr: %s",
					    li->li_name, elf_errmsg(-1));
					continue;
				}
				strndx = sh.sh_link;
				(void) elf_errno();
				_d = elf_getdata(_scn, NULL);
				if (_d == NULL) {
					elferr = elf_errno();
					if (elferr != 0)
						ld_warn(ld, "%s: elf_getdata"
						    " failed: %s", li->li_name,
						    elf_errmsg(elferr));
					continue;
				}
			}
			if (gelf_getsym(_d, is->is_info, &sym) != &sym) {
				ld_warn(ld, "%s: gelf_getsym failed (section"
				    " group signature): %s", li->li_name,
				    elf_errmsg(-1));
				continue;
			}
			if ((name = elf_strptr(e, strndx, sym.st_name)) ==
			    NULL) {
				ld_warn(ld, "%s: elf_strptr failed (section"
				    " group signature): %s", li->li_name,
				    elf_errmsg(-1));
				continue;
			}

			/*
			 * Search the currently added section groups for the
			 * signature. If found, this section group should not
			 * be added and the sections it contains should be
			 * discarded. If not found, we add this section group
			 * to the set.
			 */
			HASH_FIND_STR(ld->ld_sg, name, sg);
			if (sg != NULL)
				_discard_section_group(ld, li, scn);
			else {
				if ((sg = calloc(1, sizeof(*sg))) == NULL)
					ld_fatal_std(ld, "%s: calloc",
					    li->li_name);
				if ((sg->sg_name = strdup(name)) == NULL)
					ld_fatal_std(ld, "%s: strdup",
					    li->li_name);
				HASH_ADD_KEYPTR(hh, ld->ld_sg, sg->sg_name,
				    strlen(sg->sg_name), sg);
			}
		}

		/*
		 * Check for informational sections which should not
		 * be included in the output object, process them
		 * and mark them as discarded if need.
		 */

		if (strcmp(is->is_name, ".note.GNU-stack") == 0) {
			ld->ld_gen_gnustack = 1;
			if (is->is_flags & SHF_EXECINSTR)
				ld->ld_stack_exec = 1;
			is->is_discard = 1;
			continue;
		}

		/*
		 * The content of input .eh_frame section is preloaded for
		 * output .eh_frame optimization.
		 */
		if (strcmp(is->is_name, ".eh_frame") == 0) {
			if ((d = elf_rawdata(scn, NULL)) == NULL) {
				elferr = elf_errno();
				if (elferr != 0)
					ld_warn(ld, "%s(%s): elf_rawdata "
					    "failed: %s", li->li_name,
					    is->is_name, elf_errmsg(elferr));
				continue;
			}

			if (d->d_buf == NULL || d->d_size == 0)
				continue;

			if ((is->is_ehframe = malloc(d->d_size)) == NULL)
				ld_fatal_std(ld, "malloc");

			memcpy(is->is_ehframe, d->d_buf, d->d_size);
			is->is_ibuf = is->is_ehframe;
		}
	}
	elferr = elf_errno();
	if (elferr != 0)
		ld_fatal(ld, "%s: elf_nextscn failed: %s", li->li_name,
		    elf_errmsg(elferr));
}

void
ld_input_alloc_common_symbol(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_input *li;
	struct ld_input_section *is;

	li = lsb->lsb_input;
	if (li == NULL)
		return;		/* unlikely */

	/*
	 * Do not allocate memory for common symbols when the linker
	 * creates a relocatable output object, unless option -d is
	 * specified.
	 */
	if (ld->ld_reloc && !ld->ld_common_alloc)
		return;

	is = &li->li_is[li->li_shnum - 1];
	if (is->is_name == NULL) {
		/*
		 * Create a pseudo section named COMMON to keep track of
		 * common symbols.
		 */
		if ((is->is_name = strdup("COMMON")) == NULL)
			ld_fatal_std(ld, "%s: calloc", li->li_name);
		is->is_off = 0;
		is->is_size = 0;
		is->is_entsize = 0;
		is->is_align = 1;
		is->is_type = SHT_NOBITS;
		is->is_flags = SHF_ALLOC | SHF_WRITE;
		is->is_link = 0;
		is->is_info = 0;
		is->is_index = SHN_COMMON;
		is->is_input = li;
	}

	/*
	 * Allocate space for this symbol in the pseudo COMMON section.
	 * Properly handle the alignment. (For common symbols, symbol
	 * value stores the required alignment)
	 */
	if (lsb->lsb_value > is->is_align)
		is->is_align = lsb->lsb_value;
	is->is_size = roundup(is->is_size, is->is_align);
	lsb->lsb_value = is->is_size;
	is->is_size += lsb->lsb_size;
}

static void
_discard_section_group(struct ld *ld, struct ld_input *li, Elf_Scn *scn)
{
	Elf_Data *d;
	uint32_t *w;
	int elferr, i;

	(void) elf_errno();
	if ((d = elf_getdata(scn, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_warn(ld, "%s: elf_getdata failed (section group):"
			    " %s", li->li_name, elf_errmsg(elferr));
		return;
	}

	if (d->d_buf == NULL || d->d_size == 0)
		return;

	w = d->d_buf;
	if ((*w & GRP_COMDAT) == 0)
		return;

	for (i = 1; (size_t) i < d->d_size / 4; i++) {
		if (w[i] < li->li_shnum - 1)
			li->li_is[w[i]].is_discard = 1;
	}
}

static off_t
_offset_sort(struct ld_archive_member *a, struct ld_archive_member *b)
{

	return (a->lam_off - b->lam_off);
}
