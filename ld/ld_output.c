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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include "ld_ehframe.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_layout.h"
#include "ld_reloc.h"
#include "ld_script.h"
#include "ld_strtab.h"
#include "ld_symbols.h"

ELFTC_VCSID("$Id: ld_output.c 3281 2015-12-11 21:39:23Z kaiwang27 $");

static void _alloc_input_section_data(struct ld *ld, Elf_Scn *scn,
    struct ld_input_section *is);
static void _alloc_section_data_from_buffer(struct ld *ld, Elf_Scn *scn,
    struct ld_output_data_buffer *odb);
static void _alloc_section_data_for_symtab(struct ld *ld,
    struct ld_output_section *os, Elf_Scn *scn,
    struct ld_symbol_table *symtab);
static void _alloc_section_data_for_strtab(struct ld *ld, Elf_Scn *scn,
    struct ld_strtab *strtab);
static void _add_to_shstrtab(struct ld *ld, const char *name);
static void _copy_and_reloc_input_sections(struct ld *ld);
static Elf_Scn *_create_elf_scn(struct ld *ld, struct ld_output *lo,
    struct ld_output_section *os);
static void _create_elf_section(struct ld *ld, struct ld_output_section *os);
static void _create_phdr(struct ld *ld);
static void _create_symbol_table(struct ld *ld);
static uint64_t _find_entry_point(struct ld *ld);
static void _produce_reloc_sections(struct ld *ld, struct ld_output *lo);
static void _join_and_finalize_dynamic_reloc_sections(struct ld *ld,
    struct ld_output *lo);
static void _join_normal_reloc_sections(struct ld *ld, struct ld_output *lo);
static void _update_section_header(struct ld *ld);

void
ld_output_early_init(struct ld *ld)
{
	struct ld_output *lo;

	if (ld->ld_output == NULL) {
		if ((lo = calloc(1, sizeof(*lo))) == NULL)
			ld_fatal_std(ld, "calloc");

		STAILQ_INIT(&lo->lo_oelist);
		STAILQ_INIT(&lo->lo_oslist);
		ld->ld_output = lo;
	} else
		lo = ld->ld_output;

	assert(ld->ld_otgt != NULL);
	lo->lo_ec = elftc_bfd_target_class(ld->ld_otgt);
	lo->lo_endian = elftc_bfd_target_byteorder(ld->ld_otgt);
}

void
ld_output_init(struct ld *ld)
{
	struct ld_output *lo;
	const char *fn;
	GElf_Ehdr eh;

	lo = ld->ld_output;
	assert(lo != NULL);

	if (ld->ld_output_file == NULL)
		fn = "a.out";
	else
		fn = ld->ld_output_file;

	lo->lo_fd = open(fn, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (lo->lo_fd < 0)
		ld_fatal_std(ld, "can not create output file: open %s", fn);

	if ((lo->lo_elf = elf_begin(lo->lo_fd, ELF_C_WRITE, NULL)) == NULL)
		ld_fatal(ld, "elf_begin failed: %s", elf_errmsg(-1));

	elf_flagelf(lo->lo_elf, ELF_C_SET, ELF_F_LAYOUT);

	if (gelf_newehdr(lo->lo_elf, lo->lo_ec) == NULL)
		ld_fatal(ld, "gelf_newehdr failed: %s", elf_errmsg(-1));

	if (gelf_getehdr(lo->lo_elf, &eh) == NULL)
		ld_fatal(ld, "gelf_getehdr failed: %s", elf_errmsg(-1));

	eh.e_ident[EI_CLASS] = lo->lo_ec;
	eh.e_ident[EI_DATA] = lo->lo_endian;
	eh.e_flags = ld->ld_arch->flags;
	eh.e_machine = elftc_bfd_target_machine(ld->ld_otgt);
	if (ld->ld_dso || ld->ld_pie)
		eh.e_type = ET_DYN;
	else if (ld->ld_reloc)
		eh.e_type = ET_REL;
	else
		eh.e_type = ET_EXEC;
	eh.e_version = EV_CURRENT;

	/* Save updated ELF header. */
	if (gelf_update_ehdr(lo->lo_elf, &eh) == 0)
		ld_fatal(ld, "gelf_update_ehdr failed: %s", elf_errmsg(-1));
}

void
ld_output_format(struct ld *ld, char *def, char *be, char *le)
{

	ld->ld_otgt_name = def;
	if ((ld->ld_otgt = elftc_bfd_find_target(def)) == NULL)
		ld_fatal(ld, "invalid BFD format %s", def);

	ld->ld_otgt_be_name = be;
	if ((ld->ld_otgt_be = elftc_bfd_find_target(be)) == NULL)
		ld_fatal(ld, "invalid BFD format %s", be);

	ld->ld_otgt_le_name = le;
	if ((ld->ld_otgt_le = elftc_bfd_find_target(le)) == NULL)
		ld_fatal(ld, "invalid BFD format %s", le);

	ld_arch_set_from_target(ld);
}

struct ld_output_element *
ld_output_create_element(struct ld *ld, struct ld_output_element_head *head,
    enum ld_output_element_type type, void *entry,
    struct ld_output_element *after)
{
	struct ld_output_element *oe;

	if ((oe = calloc(1, sizeof(*oe))) == NULL)
		ld_fatal_std(ld, "calloc");

	oe->oe_type = type;
	oe->oe_entry = entry;

	if (after == NULL)
		STAILQ_INSERT_TAIL(head, oe, oe_next);
	else
		STAILQ_INSERT_AFTER(head, after, oe, oe_next);

	return (oe);
}

struct ld_output_element *
ld_output_create_section_element(struct ld *ld, struct ld_output_section *os,
    enum ld_output_element_type type, void *entry,
    struct ld_output_element *after)
{
	struct ld_output_element *oe;

	oe = ld_output_create_element(ld, &os->os_e, type, entry, after);

	switch (type) {
	case OET_DATA:
	case OET_DATA_BUFFER:
	case OET_SYMTAB:
	case OET_STRTAB:
		os->os_empty = 0;
		break;
	default:
		break;
	}

	return (oe);
}

struct ld_output_section *
ld_output_alloc_section(struct ld *ld, const char *name,
    struct ld_output_section *after, struct ld_output_section *ros)
{
	struct ld_output *lo;
	struct ld_output_section *os;

	lo = ld->ld_output;

	if ((os = calloc(1, sizeof(*os))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((os->os_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	os->os_align = 1;
	os->os_empty = 1;

	STAILQ_INIT(&os->os_e);

	HASH_ADD_KEYPTR(hh, lo->lo_ostbl, os->os_name, strlen(os->os_name), os);

	if (after == NULL) {
		STAILQ_INSERT_TAIL(&lo->lo_oslist, os, os_next);
		os->os_pe = ld_output_create_element(ld, &lo->lo_oelist,
		    OET_OUTPUT_SECTION, os, NULL);
	} else {
		STAILQ_INSERT_AFTER(&lo->lo_oslist, after, os, os_next);
		os->os_pe = ld_output_create_element(ld, &lo->lo_oelist,
		    OET_OUTPUT_SECTION, os, after->os_pe);
	}

	if (ros != NULL)
		ros->os_r = os;

	return (os);
}

static Elf_Scn *
_create_elf_scn(struct ld *ld, struct ld_output *lo,
    struct ld_output_section *os)
{
	Elf_Scn *scn;

	assert(lo->lo_elf != NULL);

	if ((scn = elf_newscn(lo->lo_elf)) == NULL)
		ld_fatal(ld, "elf_newscn failed: %s", elf_errmsg(-1));

	if (os != NULL)
		os->os_scn = scn;

	return (scn);
}

static void
_create_elf_section(struct ld *ld, struct ld_output_section *os)
{
	struct ld_output *lo;
	struct ld_output_element *oe;
	struct ld_input_section *is;
	struct ld_input_section_head *islist;
	Elf_Data *d;
	Elf_Scn *scn;

	lo = ld->ld_output;
	assert(lo->lo_elf != NULL);

	/* Create section data. */
	scn = NULL;
	STAILQ_FOREACH(oe, &os->os_e, oe_next) {
		switch (oe->oe_type) {
		case OET_DATA:
			if (scn == NULL)
				scn = _create_elf_scn(ld, lo, os);
			/* TODO */
			break;
		case OET_DATA_BUFFER:
			if (scn == NULL)
				scn = _create_elf_scn(ld, lo, os);
			_alloc_section_data_from_buffer(ld, scn, oe->oe_entry);
			break;
		case OET_INPUT_SECTION_LIST:
			islist = oe->oe_islist;
			STAILQ_FOREACH(is, islist, is_next) {
				if (scn == NULL)
					scn = _create_elf_scn(ld, lo, os);
				if (os->os_type != SHT_NOBITS &&
				    !os->os_dynrel)
					_alloc_input_section_data(ld, scn, is);
			}
			if ((ld->ld_reloc || ld->ld_emit_reloc) &&
			    os->os_r != NULL) {
				/* Create Scn for relocation section. */
				if (os->os_r->os_scn == NULL) {
					os->os_r->os_scn = _create_elf_scn(ld,
					    lo, os->os_r);
					_add_to_shstrtab(ld,
					    os->os_r->os_name);
				}
			}
			break;
		case OET_KEYWORD:
			if (scn == NULL)
				scn = _create_elf_scn(ld, lo, os);
			/* TODO */
			break;
		case OET_SYMTAB:
			/* TODO: Check symtab size. */
			if (scn == NULL)
				scn = _create_elf_scn(ld, lo, os);
			break;
		case OET_STRTAB:
			/* TODO: Check strtab size. */
			if (scn == NULL)
				scn = _create_elf_scn(ld, lo, os);
			_alloc_section_data_for_strtab(ld, scn, oe->oe_entry);
			break;
		default:
			break;
		}
	}

	if (scn == NULL)
		return;

	if (os->os_type == SHT_NOBITS) {
		if ((d = elf_newdata(scn)) == NULL)
			ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

		d->d_align = os->os_align;
		d->d_off = 0;
		d->d_type = ELF_T_BYTE;
		d->d_size = os->os_size;
		d->d_version = EV_CURRENT;
		d->d_buf = NULL;
	}

	_add_to_shstrtab(ld, os->os_name);
}

static void
_alloc_input_section_data(struct ld *ld, Elf_Scn *scn,
    struct ld_input_section *is)
{
	Elf_Data *d;

	if (is->is_type == SHT_NOBITS || is->is_size == 0)
		return;

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	is->is_data = d;
}

static void
_alloc_section_data_from_buffer(struct ld *ld, Elf_Scn *scn,
    struct ld_output_data_buffer *odb)
{
	Elf_Data *d;

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = odb->odb_align;
	d->d_off = odb->odb_off;
	d->d_type = odb->odb_type;
	d->d_size = odb->odb_size;
	d->d_version = EV_CURRENT;
	d->d_buf = odb->odb_buf;
}

static void
_alloc_section_data_from_reloc_buffer(struct ld *ld, Elf_Scn *scn,
    void *buf, size_t sz)
{
	Elf_Data *d;

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = ld->ld_arch->reloc_is_64bit ? 8 : 4;
	d->d_off = 0;		/* has to be the only data descriptor */
	d->d_type = ld->ld_arch->reloc_is_rela ? ELF_T_RELA : ELF_T_REL;
	d->d_size = sz;
	d->d_version = EV_CURRENT;
	d->d_buf = buf;
}

static void
_alloc_section_data_for_symtab(struct ld *ld, struct ld_output_section *os,
    Elf_Scn *scn, struct ld_symbol_table *symtab)
{
	Elf_Data *d;

	if (symtab->sy_buf == NULL || symtab->sy_size == 0)
		return;

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = os->os_align;
	d->d_off = 0;
	d->d_type = ELF_T_SYM;
	d->d_size = os->os_entsize * symtab->sy_size;
	d->d_version = EV_CURRENT;
	d->d_buf = symtab->sy_buf;
}

static void
_alloc_section_data_for_strtab(struct ld *ld, Elf_Scn *scn,
    struct ld_strtab *strtab)
{
	Elf_Data *d;
	void *buf;
	size_t sz;

	buf = ld_strtab_getbuf(ld, strtab);
	sz = ld_strtab_getsize(strtab);
	if (buf == NULL || sz == 0)
		return;

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = 1;
	d->d_off = 0;
	d->d_type = ELF_T_BYTE;
	d->d_size = sz;
	d->d_version = EV_CURRENT;
	d->d_buf = buf;
}

static void
_copy_and_reloc_input_sections(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;
	Elf_Data *d;
	int i;

	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		ld_input_load(ld, li);
		for (i = 0; (uint64_t) i < li->li_shnum; i++) {
			is = &li->li_is[i];

			if (is->is_discard || !is->is_need_reloc)
				continue;

			d = is->is_data;

			d->d_align = is->is_align;
			d->d_off = is->is_reloff;
			d->d_type = ELF_T_BYTE;
			d->d_size = is->is_size;
			d->d_version = EV_CURRENT;

			/*
			 * Take different actions depending on different types
			 * of input sections:
			 *
			 * For internal input sections, assign the internal
			 * buffer directly to the data descriptor.
			 * For relocation sections, they should be ignored
			 * since they are handled elsewhere.
			 * For other input sections, load the raw data from
			 * input object and preform relocation.
			 */
			if (is->is_ibuf != NULL) {
				d->d_buf = is->is_ibuf;
				/* .eh_frame section needs relocation */
				if (strcmp(is->is_name, ".eh_frame") == 0)
					ld_reloc_process_input_section(ld, is,
					    d->d_buf);
			} else if (is->is_reloc == NULL) {
				d->d_buf = ld_input_get_section_rawdata(ld,
				    is);
				ld_reloc_process_input_section(ld, is,
				    d->d_buf);
			}
		}
		ld_input_unload(ld, li);
	}
}

static void
_produce_reloc_sections(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os;
	void *buf;
	size_t sz;

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_reloc != NULL) {
			/* Serialize relocation records. */
			buf = ld_reloc_serialize(ld, os, &sz);
			_alloc_section_data_from_reloc_buffer(ld, os->os_scn,
			    buf, sz);

			/*
			 * Link dynamic relocation sections to .dynsym
			 * section.
			 */
			if (os->os_dynrel) {
				if ((os->os_link = strdup(".dynsym")) == NULL)
					ld_fatal_std(ld, "strdup");
			}
		}
	}
}

static void
_join_and_finalize_dynamic_reloc_sections(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os;
	struct ld_output_element *oe;
	struct ld_input_section *is;
	struct ld_input_section_head *islist;

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {

		if (!os->os_dynrel)
			continue;

		STAILQ_FOREACH(oe, &os->os_e, oe_next) {
			switch (oe->oe_type) {
			case OET_INPUT_SECTION_LIST:
				islist = oe->oe_islist;
				STAILQ_FOREACH(is, islist, is_next)
					ld_reloc_join(ld, os, is);
				break;
			default:
				break;
			}
		}

		/* Sort dynamic relocations for the runtime linker. */
		if (os->os_reloc != NULL && os->os_dynrel && !os->os_pltrel)
			ld_reloc_sort(ld, os);

		/* Finalize relocations. */
		ld_reloc_finalize_dynamic(ld, lo, os);
	}
}

static void
_join_normal_reloc_sections(struct ld *ld, struct ld_output *lo)
{
	struct ld_output_section *os;
	struct ld_output_element *oe;
	struct ld_input_section *is;
	struct ld_input_section_head *islist;

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {

		if (os->os_r == NULL)
			continue;

		STAILQ_FOREACH(oe, &os->os_e, oe_next) {
			switch (oe->oe_type) {
			case OET_INPUT_SECTION_LIST:
				islist = oe->oe_islist;
				STAILQ_FOREACH(is, islist, is_next) {
					if (is->is_ris == NULL)
						continue;
					ld_reloc_join(ld, os->os_r,
					    is->is_ris);
				}
				break;
			default:
				break;
			}
		}
	}
}

void
ld_output_create_elf_sections(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_element *oe;

	lo = ld->ld_output;
	assert(lo->lo_elf != NULL);

	STAILQ_FOREACH(oe, &lo->lo_oelist, oe_next) {
		switch (oe->oe_type) {
		case OET_OUTPUT_SECTION:
			_create_elf_section(ld, oe->oe_entry);
			break;
		default:
			break;
		}
	}
}

void
ld_output_emit_gnu_stack_section(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_output *lo;
	struct ld_output_section *os;

	ls = &ld->ld_state;
	lo = ld->ld_output;

	os = ld_output_alloc_section(ld, ".note.GNU-stack", NULL, NULL);
	os->os_empty = 0;
	os->os_addr = 0;
	os->os_type = SHT_PROGBITS;
	os->os_align = 1;
	os->os_entsize = 0;
	os->os_off = ls->ls_offset;
	os->os_size = 0;
	if (ld->ld_stack_exec)
		os->os_flags = SHF_EXECINSTR;

	(void) _create_elf_scn(ld, lo, os);

	_add_to_shstrtab(ld, ".note.GNU-stack");

	/*
	 * .note.GNU-stack is an empty section so we don't allocate any
	 * Elf_Data descriptors.
	 */
}

static uint64_t
_find_entry_point(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_section *os;
	char entry_symbol[] = "start";
	uint64_t entry;

	lo = ld->ld_output;

	if (ld->ld_entry != NULL) {
		if (ld_symbols_get_value(ld, ld->ld_entry, &entry) < 0)
			ld_fatal(ld, "symbol %s is undefined", ld->ld_entry);
		return (entry);
	}

	if (ld->ld_scp->lds_entry_point != NULL) {
		if (ld_symbols_get_value(ld, ld->ld_scp->lds_entry_point,
		    &entry) == 0)
			return (entry);
	}

	if (ld_symbols_get_value(ld, entry_symbol, &entry) == 0)
		return (entry);

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_empty)
			continue;
		if (strcmp(os->os_name, ".text") == 0)
			return (os->os_addr);
	}

	return (0);
}

static void
_create_phdr(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_section *os;
	Elf32_Phdr *p32;
	Elf64_Phdr *p64;
	void *phdrs;
	uint64_t addr, off, align, flags, filesz, memsz, phdr_addr;
	uint64_t tls_addr, tls_off, tls_align, tls_flags;
	uint64_t tls_filesz, tls_memsz;
	unsigned w;
	int i, new, first, tls;

	/* TODO: support segments created by linker script command PHDR. */

#define	_WRITE_PHDR(T,O,A,FSZ,MSZ,FL,AL)		\
	do {						\
		if (lo->lo_ec == ELFCLASS32) {		\
			p32[i].p_type = (T);		\
			p32[i].p_offset = (O);		\
			p32[i].p_vaddr = (A);		\
			p32[i].p_paddr = (A);		\
			p32[i].p_filesz = (FSZ);	\
			p32[i].p_memsz = (MSZ);		\
			p32[i].p_flags = (FL);		\
			p32[i].p_align = (AL);		\
		} else {				\
			p64[i].p_type = (T);		\
			p64[i].p_offset = (O);		\
			p64[i].p_vaddr = (A);		\
			p64[i].p_paddr = (A);		\
			p64[i].p_filesz = (FSZ);	\
			p64[i].p_memsz = (MSZ);		\
			p64[i].p_flags = (FL);		\
			p64[i].p_align = (AL);		\
		}					\
	} while(0)

	lo = ld->ld_output;
	assert(lo->lo_elf != NULL);
	assert(lo->lo_phdr_num != 0);
	assert(ld->ld_arch != NULL);

	if ((phdrs = gelf_newphdr(lo->lo_elf, lo->lo_phdr_num)) == NULL)
		ld_fatal(ld, "gelf_newphdr failed: %s", elf_errmsg(-1));

	p32 = NULL;
	p64 = NULL;
	if (lo->lo_ec == ELFCLASS32)
		p32 = phdrs;
	else
		p64 = phdrs;

	i = -1;

	/* Calculate the start vma of output object. */
	os = STAILQ_FIRST(&lo->lo_oslist);
	addr = os->os_addr - os->os_off;

	/* Create PT_PHDR segment for dynamically linked output object */
	if (lo->lo_dso_needed > 0 && !ld->ld_dso) {
		i++;
		off = gelf_fsize(lo->lo_elf, ELF_T_EHDR, 1, EV_CURRENT);
		phdr_addr = addr + off;
		filesz = memsz = gelf_fsize(lo->lo_elf, ELF_T_PHDR,
		    lo->lo_phdr_num, EV_CURRENT);
		align = lo->lo_ec == ELFCLASS32 ? 4 : 8;
		flags = PF_R | PF_X;
		_WRITE_PHDR(PT_PHDR, off, phdr_addr, filesz, memsz, flags,
		    align);
	}

	/* Create PT_INTERP segment for dynamically linked output object */
	if (lo->lo_interp != NULL) {
		i++;
		os = lo->lo_interp;
		_WRITE_PHDR(PT_INTERP, os->os_off, os->os_addr, os->os_size,
		    os->os_size, PF_R, 1);
	}

	/*
	 * Create PT_LOAD segments.
	 */

	align = ld->ld_arch->get_max_page_size(ld);
	new = 1;
	w = 0;
	off = filesz = memsz = 0;
	flags = PF_R;
	first = 1;

	tls = 0;
	tls_off = tls_addr = tls_filesz = tls_memsz = tls_align = 0;
	tls_flags = PF_R;	/* TLS segment is a read-only image */

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_empty)
			continue;

		if ((os->os_flags & SHF_ALLOC) == 0) {
			new = 1;
			continue;
		}

		if ((os->os_flags & SHF_TLS) != 0) {
			if (tls < 0)
				ld_warn(ld, "can not have multiple TLS "
				    "segments");
			else {
				if (tls == 0) {
					tls = 1;
					tls_addr = os->os_addr;
					tls_off = os->os_off;
				}

				if (os->os_align > tls_align)
					tls_align = os->os_align;
			}

		} else if (tls > 0)
			tls = -1;

		if ((os->os_flags & SHF_WRITE) != w || new) {
			new = 0;
			w = os->os_flags & SHF_WRITE;

			if (!first)
				_WRITE_PHDR(PT_LOAD, off, addr, filesz, memsz,
				    flags, align);

			i++;
			if ((unsigned) i >= lo->lo_phdr_num)
				ld_fatal(ld, "not enough room for program"
				    " headers");
			if (!first) {
				addr = os->os_addr;
				off = os->os_off;
			}
			first = 0;
			flags = PF_R;
			filesz = 0;
			memsz = 0;
		}

		memsz = os->os_addr + os->os_size - addr;
		if (tls > 0)
			tls_memsz = memsz;

		if (os->os_type != SHT_NOBITS) {
			filesz = memsz;
			if (tls > 0)
				tls_filesz = tls_memsz;
		}

		if (os->os_flags & SHF_WRITE)
			flags |= PF_W;

		if (os->os_flags & SHF_EXECINSTR)
			flags |= PF_X;
	}
	if (i >= 0)
		_WRITE_PHDR(PT_LOAD, off, addr, filesz, memsz, flags, align);

	/*
	 * Create PT_DYNAMIC segment.
	 */
	if (lo->lo_dynamic != NULL) {
		i++;
		os = lo->lo_dynamic;
		_WRITE_PHDR(PT_DYNAMIC, os->os_off, os->os_addr, os->os_size,
		    os->os_size, PF_R | PF_W, lo->lo_ec == ELFCLASS32 ? 4 : 8);
	}

	/*
	 * Create PT_NOTE segment.
	 */

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_type == SHT_NOTE) {
			i++;
			if ((unsigned) i >= lo->lo_phdr_num)
				ld_fatal(ld, "not enough room for program"
				    " headers");
			_WRITE_PHDR(PT_NOTE, os->os_off, os->os_addr,
			    os->os_size, os->os_size, PF_R, os->os_align);
			break;
		}
	}

	/*
	 * Create PT_TLS segment.
	 */

	if (tls != 0) {
		i++;
		lo->lo_tls_size = tls_memsz;
		lo->lo_tls_align = tls_align;
		lo->lo_tls_addr = tls_addr;
		_WRITE_PHDR(PT_TLS, tls_off, tls_addr, tls_filesz, tls_memsz,
		    tls_flags, tls_align);
	}

	/*
	 * Create PT_GNU_EH_FRAME segment.
	 */
	if (ld->ld_ehframe_hdr) {
		i++;
		os = lo->lo_ehframe_hdr;
		assert(os != NULL);
		_WRITE_PHDR(PT_GNU_EH_FRAME, os->os_off, os->os_addr,
		    os->os_size, os->os_size, PF_R, 4);
	}

	/*
	 * Create PT_GNU_STACK segment.
	 */

	if (ld->ld_gen_gnustack) {
		i++;
		flags = PF_R | PF_W;
		if (ld->ld_stack_exec)
			flags |= PF_X;
		align = (lo->lo_ec == ELFCLASS32) ? 4 : 8;
		_WRITE_PHDR(PT_GNU_STACK, 0, 0, 0, 0, flags, align);
	}

	assert((unsigned) i + 1 == lo->lo_phdr_num);

#undef	_WRITE_PHDR
}

void
ld_output_create(struct ld *ld)
{
	struct ld_output *lo;
	GElf_Ehdr eh;

	lo = ld->ld_output;

	if (gelf_getehdr(lo->lo_elf, &eh) == NULL)
		ld_fatal(ld, "gelf_getehdr failed: %s", elf_errmsg(-1));

	/* Set program header table offset. */
	eh.e_phoff = gelf_fsize(lo->lo_elf, ELF_T_EHDR, 1, EV_CURRENT);
	if (eh.e_phoff == 0)
		ld_fatal(ld, "gelf_fsize failed: %s", elf_errmsg(-1));

	/* Set section headers table offset. */
	eh.e_shoff = lo->lo_shoff;

	/* Set executable entry point. */
	eh.e_entry = _find_entry_point(ld);

	/* Save updated ELF header. */
	if (gelf_update_ehdr(lo->lo_elf, &eh) == 0)
		ld_fatal(ld, "gelf_update_ehdr failed: %s", elf_errmsg(-1));

	/* Allocate space for internal sections. */
	ld_input_alloc_internal_section_buffers(ld);

	/* Finalize PLT and GOT sections. */
	if (ld->ld_arch->finalize_got_and_plt)
		ld->ld_arch->finalize_got_and_plt(ld);

	/* Join and sort dynamic relocation sections. */
	_join_and_finalize_dynamic_reloc_sections(ld, lo);

	/* Finalize sections for dynamically linked output object. */
	ld_dynamic_finalize(ld);

	/* Finalize dynamic symbol section. */
	if (lo->lo_dynsym != NULL) {
		ld_symbols_finalize_dynsym(ld);
		_alloc_section_data_for_symtab(ld, lo->lo_dynsym,
		    lo->lo_dynsym->os_scn, ld->ld_dynsym);
	}

	/* Generate symbol table. */
	_create_symbol_table(ld);

	/* Copy and relocate input section data to output section. */
	_copy_and_reloc_input_sections(ld);

	/* Finalize .eh_frame_hdr section. */
	if (ld->ld_ehframe_hdr)
		ld_ehframe_finalize_hdr(ld);

	/*
	 * Join normal relocation sections if the linker is creating a
	 * relocatable object or if option -emit-relocs is specified.
	 */
	if (ld->ld_reloc || ld->ld_emit_reloc)
		_join_normal_reloc_sections(ld, lo);

	/* Produce relocation entries. */
	_produce_reloc_sections(ld, lo);

	/* Update section headers for the output sections. */
	_update_section_header(ld);

	/* Create program headers. */
	if (!ld->ld_reloc)
		_create_phdr(ld);

	/* Finally write out the output ELF object. */
	if (elf_update(lo->lo_elf, ELF_C_WRITE) < 0)
		ld_fatal(ld, "elf_update failed: %s", elf_errmsg(-1));
}

static void
_add_to_shstrtab(struct ld *ld, const char *name)
{

	if (ld->ld_shstrtab == NULL) {
		ld->ld_shstrtab = ld_strtab_alloc(ld, 1);
		ld_strtab_insert(ld, ld->ld_shstrtab, ".symtab");
		ld_strtab_insert(ld, ld->ld_shstrtab, ".strtab");
		ld_strtab_insert(ld, ld->ld_shstrtab, ".shstrtab");
	}

	ld_strtab_insert(ld, ld->ld_shstrtab, name);
}

static void
_update_section_header(struct ld *ld)
{
	struct ld_strtab *st;
	struct ld_output *lo;
	struct ld_output_section *os, *_os;
	GElf_Shdr sh;

	lo = ld->ld_output;
	st = ld->ld_shstrtab;
	assert(st != NULL);

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_scn == NULL)
			continue;

		if (gelf_getshdr(os->os_scn, &sh) == NULL)
			ld_fatal(ld, "gelf_getshdr failed: %s",
			    elf_errmsg(-1));

		sh.sh_name = ld_strtab_lookup(st, os->os_name);
		sh.sh_flags = os->os_flags;
		sh.sh_addr = os->os_addr;
		sh.sh_addralign = os->os_align;
		sh.sh_offset = os->os_off;
		sh.sh_size = os->os_size;
		sh.sh_type = os->os_type;
		sh.sh_entsize = os->os_entsize;

		/* Update "sh_link" field. */
		if (os->os_link != NULL) {
			if (!strcmp(os->os_link, ".symtab"))
				sh.sh_link = lo->lo_symtab_shndx;
			else {
				HASH_FIND_STR(lo->lo_ostbl, os->os_link, _os);
				if (_os == NULL)
					ld_fatal(ld, "Internal: can not find"
					    " link section %s", os->os_link);
				sh.sh_link = elf_ndxscn(_os->os_scn);
			}
		}

		/* Update "sh_info" field. */
		if (os->os_info != NULL)
			sh.sh_info = elf_ndxscn(os->os_info->os_scn);
		else
			sh.sh_info = os->os_info_val;

#if 0
		printf("name=%s, shname=%#jx, offset=%#jx, size=%#jx, type=%#jx\n",
		    os->os_name, (uint64_t) sh.sh_name, (uint64_t) sh.sh_offset,
		    (uint64_t) sh.sh_size, (uint64_t) sh.sh_type);
#endif

		if (!gelf_update_shdr(os->os_scn, &sh))
			ld_fatal(ld, "gelf_update_shdr failed: %s",
			    elf_errmsg(-1));
	}
}

static void
_create_symbol_table(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_strtab *st;
	struct ld_output *lo;
	Elf_Scn *scn_symtab, *scn_strtab;
	Elf_Data *d;
	GElf_Shdr sh;
	size_t strndx;

	ld_symbols_build_symtab(ld);

	ls = &ld->ld_state;
	lo = ld->ld_output;
	st = ld->ld_shstrtab;
	assert(st != NULL);

	/*
	 * Create .symtab section.
	 */
	scn_symtab = _create_elf_scn(ld, lo, NULL);
	scn_strtab = _create_elf_scn(ld, lo, NULL);
	lo->lo_symtab_shndx = elf_ndxscn(scn_symtab);
	strndx = elf_ndxscn(scn_strtab);

	if (gelf_getshdr(scn_symtab, &sh) == NULL)
		ld_fatal(ld, "gelf_getshdr failed: %s", elf_errmsg(-1));

	sh.sh_name = ld_strtab_lookup(st, ".symtab");
	sh.sh_flags = 0;
	sh.sh_addr = 0;
	sh.sh_addralign = (lo->lo_ec == ELFCLASS32) ? 4 : 8;
	sh.sh_offset = roundup(ls->ls_offset, sh.sh_addralign);
	sh.sh_entsize = (lo->lo_ec == ELFCLASS32) ? sizeof(Elf32_Sym) :
	    sizeof(Elf64_Sym);
	sh.sh_size = ld->ld_symtab->sy_size * sh.sh_entsize;
	sh.sh_type = SHT_SYMTAB;
	sh.sh_link = strndx;
	sh.sh_info = ld->ld_symtab->sy_first_nonlocal;

	if (!gelf_update_shdr(scn_symtab, &sh))
		ld_fatal(ld, "gelf_update_shdr failed: %s", elf_errmsg(-1));

	if ((d = elf_newdata(scn_symtab)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = sh.sh_addralign;
	d->d_off = 0;
	d->d_type = ELF_T_SYM;
	d->d_size = sh.sh_size;
	d->d_version = EV_CURRENT;
	d->d_buf = ld->ld_symtab->sy_buf;

	ls->ls_offset = sh.sh_offset + sh.sh_size;

	/*
	 * Create .strtab section.
	 */
	ld_output_create_string_table_section(ld, ".strtab", ld->ld_strtab,
	    scn_strtab);
}

void
ld_output_create_string_table_section(struct ld *ld, const char *name,
    struct ld_strtab *st, Elf_Scn *scn)
{
	struct ld_state *ls;
	struct ld_output *lo;
	Elf_Data *d;
	GElf_Shdr sh;
	size_t sz;

	assert(st != NULL && name != NULL);

	ls = &ld->ld_state;
	lo = ld->ld_output;

	if (scn == NULL)
		scn = _create_elf_scn(ld, lo, NULL);

	if (strcmp(name, ".shstrtab") == 0) {
		if (!elf_setshstrndx(lo->lo_elf, elf_ndxscn(scn)))
			ld_fatal(ld, "elf_setshstrndx failed: %s",
			    elf_errmsg(-1));
	}

	if (gelf_getshdr(scn, &sh) == NULL)
		ld_fatal(ld, "gelf_getshdr failed: %s", elf_errmsg(-1));

	sh.sh_name = ld_strtab_lookup(ld->ld_shstrtab, name);
	sh.sh_flags = 0;
	sh.sh_addr = 0;
	sh.sh_addralign = 1;
	sh.sh_offset = ls->ls_offset;
	sh.sh_size = ld_strtab_getsize(st);
	sh.sh_type = SHT_STRTAB;

	if (!gelf_update_shdr(scn, &sh))
		ld_fatal(ld, "gelf_update_shdr failed: %s", elf_errmsg(-1));

	sz = ld_strtab_getsize(st);

	if ((d = elf_newdata(scn)) == NULL)
		ld_fatal(ld, "elf_newdata failed: %s", elf_errmsg(-1));

	d->d_align = 1;
	d->d_off = 0;
	d->d_type = ELF_T_BYTE;
	d->d_size = sz;
	d->d_version = EV_CURRENT;
	d->d_buf = ld_strtab_getbuf(ld, st);

	ls->ls_offset += sz;
}
