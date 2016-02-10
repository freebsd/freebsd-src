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
#include "ld_arch.h"
#include "ld_file.h"
#include "ld_path.h"

ELFTC_VCSID("$Id: ld_file.c 3281 2015-12-11 21:39:23Z kaiwang27 $");

/*
 * Support routines for input file handling.
 */

static void _add_file(struct ld *ld, const char *name, enum ld_file_type type,
    int first, struct ld_file *after);

void
ld_file_cleanup(struct ld *ld)
{
	struct ld_file *lf, *_lf;
	struct ld_archive_member *lam, *_lam;

	TAILQ_FOREACH_SAFE(lf, &ld->ld_lflist, lf_next, _lf) {
		TAILQ_REMOVE(&ld->ld_lflist, lf, lf_next);
		free(lf->lf_name);
		if (lf->lf_ar != NULL) {
			HASH_ITER(hh, lf->lf_ar->la_m, lam, _lam) {
				HASH_DEL(lf->lf_ar->la_m, lam);
				free(lam->lam_name);
				free(lam);
			}
			free(lf->lf_ar);
		}
		free(lf);
	}
}

void
ld_file_add(struct ld *ld, const char *name, enum ld_file_type type)
{

	_add_file(ld, name, type, 0, NULL);
}

void
ld_file_add_first(struct ld *ld, const char *name, enum ld_file_type type)
{

	_add_file(ld, name, type, 1, NULL);
}

void
ld_file_add_after(struct ld *ld, const char *name, enum ld_file_type type,
    struct ld_file *after)
{

	_add_file(ld, name, type, 0, after);
}

void
ld_file_load(struct ld *ld, struct ld_file *lf)
{
	struct ld_archive *la;
	struct ld_state *ls;
	struct stat sb;
	Elf_Kind k;
	GElf_Ehdr ehdr;
	int fd;

	assert(lf != NULL && lf->lf_name != NULL);

	ls = &ld->ld_state;
	if (ls->ls_file == lf)
		return;

	if ((fd = open(lf->lf_name, O_RDONLY)) < 0)
		ld_fatal_std(ld, "%s: open", lf->lf_name);

	if (fstat(fd, &sb) < 0)
		ld_fatal_std(ld, "%s: stat", lf->lf_name);
	if (sb.st_size == 0)
		ld_fatal(ld, "%s: File truncated", lf->lf_name);

	lf->lf_size = sb.st_size;
	if ((lf->lf_mmap = mmap(NULL, lf->lf_size, PROT_READ, MAP_PRIVATE, fd,
	    (off_t) 0)) == MAP_FAILED)
		ld_fatal_std(ld, "%s: mmap", lf->lf_name);
	close(fd);

	if (lf->lf_type == LFT_BINARY)
		return;

	if ((lf->lf_elf = elf_memory(lf->lf_mmap, lf->lf_size)) == NULL)
		ld_fatal(ld, "%s: elf_memory failed: %s", lf->lf_name,
		    elf_errmsg(-1));

	k = elf_kind(lf->lf_elf);

	if (k == ELF_K_AR) {
		lf->lf_type = LFT_ARCHIVE;
		if (lf->lf_ar == NULL) {
			if ((la = calloc(1, sizeof(*la))) == NULL)
				ld_fatal_std(ld, "calloc");
			lf->lf_ar = la;
		}
		return;
	}

	assert(k != ELF_K_AR);
	if (k == ELF_K_NONE)
		ld_fatal(ld, "%s: File format not recognized", lf->lf_name);

	if (gelf_getehdr(lf->lf_elf, &ehdr) == NULL)
		ld_fatal(ld, "%s: gelf_getehdr failed: %s", lf->lf_name,
		    elf_errmsg(-1));

	switch (ehdr.e_type) {
	case ET_NONE:
		ld_fatal(ld, "%s: ELF type ET_NONE not supported", lf->lf_name);
	case ET_REL:
		lf->lf_type = LFT_RELOCATABLE;
		break;
	case ET_EXEC:
		ld_fatal(ld, "%s: ELF type ET_EXEC not supported yet",
		    lf->lf_name);
	case ET_DYN:
		lf->lf_type = LFT_DSO;
		break;
	case ET_CORE:
		ld_fatal(ld, "%s: ELF type ET_NONE not supported", lf->lf_name);
	default:
		ld_fatal(ld, "%s: unknown ELF type %u", ehdr.e_type);
	}

	ld_arch_verify(ld, lf->lf_name, ehdr.e_machine, ehdr.e_ident[EI_DATA],
	    ehdr.e_flags);
}

void
ld_file_unload(struct ld *ld, struct ld_file *lf)
{
	struct ld_state *ls;

	ls = &ld->ld_state;

	if (lf->lf_type != LFT_BINARY)
		elf_end(lf->lf_elf);

	if (lf->lf_mmap != NULL) {
		if (munmap(lf->lf_mmap, lf->lf_size) < 0)
			ld_fatal_std(ld, "%s: munmap", lf->lf_name);
	}

	if (ls->ls_file == lf)
		ls->ls_file = NULL;
}

static void
_add_file(struct ld *ld, const char *name, enum ld_file_type type,
    int first, struct ld_file *after)
{
	struct ld_state *ls;
	struct ld_file *lf;
	int fd;

	assert(ld != NULL && name != NULL);

	if (!strncmp(name, "-l", 2)) {
		ld_path_search_library(ld, &name[2]);
		return;
	}

	ls = &ld->ld_state;

	if ((lf = calloc(1, sizeof(*lf))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((lf->lf_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	lf->lf_type = type;
	lf->lf_whole_archive = ls->ls_whole_archive;
	lf->lf_as_needed = ls->ls_as_needed;
	lf->lf_group_level = ls->ls_group_level;
	lf->lf_search_dir = ls->ls_search_dir;

	if ((fd = open(lf->lf_name, O_RDONLY)) < 0) {
		if (!lf->lf_search_dir)
			ld_fatal_std(ld, "%s: open", lf->lf_name);

		/* Search library path for this file. */
		ld_path_search_file(ld, lf);
	} else
		(void) close(fd);

	if (lf->lf_type == LFT_UNKNOWN && ls->ls_itgt != NULL) {
		if (elftc_bfd_target_flavor(ls->ls_itgt) == ETF_BINARY)
			lf->lf_type = LFT_BINARY;
	}

	if (lf->lf_type == LFT_DSO)
		ld->ld_dynamic_link = 1;

	if (after != NULL)
		TAILQ_INSERT_AFTER(&ld->ld_lflist, after, lf, lf_next);
	else if (first)
		TAILQ_INSERT_HEAD(&ld->ld_lflist, lf, lf_next);
	else
		TAILQ_INSERT_TAIL(&ld->ld_lflist, lf, lf_next);
}
