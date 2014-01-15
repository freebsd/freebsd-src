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
 *
 * $Id: ld_file.h 2930 2013-03-17 22:54:26Z kaiwang27 $
 */

enum ld_file_type {
	LFT_UNKNOWN,
	LFT_RELOCATABLE,
	LFT_DSO,
	LFT_ARCHIVE,
	LFT_BINARY
};

struct ld_archive_member {
	char *lam_ar_name;		/* archive name */
	char *lam_name;			/* archive member name */
	off_t lam_off;			/* archive member offset */
	struct ld_input *lam_input;	/* input object */
	UT_hash_handle hh;		/* hash handle */
};

struct ld_archive {
	struct ld_archive_member *la_m;	/* extracted member list. */
};

struct ld_file {
	char *lf_name;			/* input file name */
	enum ld_file_type lf_type;	/* input file type */
	void *lf_mmap;			/* input file image */
	size_t lf_size;			/* input file size */
	Elf *lf_elf;			/* input file ELF descriptor */
	struct ld_archive *lf_ar;	/* input archive */
	struct ld_input *lf_input;	/* input object */
	unsigned lf_whole_archive;	/* include whole archive */
	unsigned lf_as_needed;		/* DT_NEEDED */
	unsigned lf_group_level;	/* archive group level */
	unsigned lf_search_dir;		/* search library directories */
	TAILQ_ENTRY(ld_file) lf_next;	/* next input file */
};

void	ld_file_add(struct ld *, const char *, enum ld_file_type);
void	ld_file_add_first(struct ld *, const char *, enum ld_file_type);
void	ld_file_add_after(struct ld *, const char *, enum ld_file_type,
    struct ld_file *);
void	ld_file_cleanup(struct ld *);
void	ld_file_load(struct ld *, struct ld_file *);
void	ld_file_unload(struct ld *, struct ld_file *);
