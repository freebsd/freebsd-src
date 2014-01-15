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
 *
 * $Id: ld_input.h 2960 2013-08-25 03:13:07Z kaiwang27 $
 */

struct ld_reloc_entry_head;
struct ld_ehframe_fde_head;

struct ld_section_group {
	char *sg_name;
	UT_hash_handle hh;
};

struct ld_input_section {
	char *is_name;			/* section name */
	struct ld_input *is_input;	/* containing input object */
	struct ld_output_section *is_output; /* containing output section */
	uint64_t is_off;		/* section file offset */
	uint64_t is_reloff;		/* relative offset in output section */
	uint64_t is_addr;		/* section vma */
	uint64_t is_size;		/* section file size */
	uint64_t is_shrink;		/* section shrinked bytes */
	uint64_t is_entsize;		/* seciton entry size */
	uint64_t is_align;		/* section align */
	uint64_t is_type;		/* section type */
	uint64_t is_flags;		/* section flags */
	uint64_t is_link;		/* section link */
	uint64_t is_info;		/* section info */
	uint64_t is_index;		/* section index */
	unsigned char is_discard;	/* dicard section */
	unsigned char is_dynrel;	/* section holds dynamic relocations */
	unsigned char is_pltrel;	/* section holds PLT relocations */
	unsigned char is_refed;		/* should not be gc'ed */
	unsigned char is_need_reloc;	/* need apply relocation */
	void *is_data;			/* output section data descriptor */
	void *is_ibuf;			/* buffer for internal sections */
	void *is_ehframe;		/* temp buffer for ehframe section. */
	struct ld_reloc_entry_head *is_reloc; /* list of relocation entries */
	uint64_t is_num_reloc;		/* number of reloc entries */
	struct ld_input_section *is_tis; /* relocation target */
	struct ld_input_section *is_ris; /* relocation section */
	struct ld_ehframe_fde_head *is_fde; /* list of FDE */
	STAILQ_ENTRY(ld_input_section) is_next; /* next section */
	STAILQ_ENTRY(ld_input_section) is_gc_next; /* next gc search */
	UT_hash_handle hh;		/* hash handle (internal section) */
};

STAILQ_HEAD(ld_input_section_head, ld_input_section);

enum ld_input_type {
	LIT_UNKNOWN,
	LIT_RELOCATABLE,
	LIT_DSO,
};

struct ld_symver_verdef_head;

struct ld_input {
	char *li_name;			/* input object name */
	char *li_fullname;		/* input object and archive name */
	char *li_soname;		/* input object DT_SONAME. */
	Elf *li_elf;			/* input object ELF descriptor */
	enum ld_input_type li_type;	/* input object kind */
	struct ld_file *li_file;	/* containing file */
	size_t li_shnum;		/* num of sections in ELF object */
	struct ld_input_section *li_is;	/* input section list */
	struct ld_input_section *li_istbl; /* internal section hash table */
	struct ld_archive_member *li_lam; /* archive member */
	struct ld_symbol_head *li_local; /* local symbol list */
	struct ld_symbol **li_symindex;	/* symbol index table */
	size_t li_symnum;		/* number of symbols */
	char **li_vername;		/* version name array */
	size_t li_vername_sz;		/* version name array size */
	uint16_t *li_versym;		/* symbol version array */
	size_t li_versym_sz;		/* symbol version array size */
	int li_dso_refcnt;		/* symbol reference count (DSO) */
	struct ld_symver_verdef_head *li_verdef; /* version definition */
	STAILQ_ENTRY(ld_input) li_next;	/* next input object */
};

void	ld_input_init(struct ld *);
void	ld_input_add_symbol(struct ld *, struct ld_input *,
    struct ld_symbol *);
struct ld_input_section *ld_input_add_internal_section(struct ld *,
    const char *);
struct ld_input_section *ld_input_find_internal_section(struct ld *,
    const char *);
void	ld_input_alloc_internal_section_buffers(struct ld *);
struct ld_input *ld_input_alloc(struct ld *, struct ld_file *, const char *);
void	ld_input_alloc_common_symbol(struct ld *, struct ld_symbol *);
void	*ld_input_get_section_rawdata(struct ld *, struct ld_input_section *);
void	ld_input_cleanup(struct ld *);
char	*ld_input_get_fullname(struct ld *, struct ld_input *);
void	ld_input_init_sections(struct ld *, struct ld_input *, Elf *);
void	ld_input_link_objects(struct ld *);
void	ld_input_load(struct ld *, struct ld_input *);
void	ld_input_unload(struct ld *, struct ld_input *);
uint64_t ld_input_reserve_ibuf(struct ld_input_section *, uint64_t);
