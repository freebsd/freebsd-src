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
#include "ld_arch.h"
#include "ld_dynamic.h"
#include "ld_ehframe.h"
#include "ld_exp.h"
#include "ld_file.h"
#include "ld_script.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_layout.h"
#include "ld_options.h"
#include "ld_symbols.h"
#include "ld_strtab.h"

ELFTC_VCSID("$Id: ld_layout.c 3276 2015-12-11 21:39:06Z kaiwang27 $");

struct ld_wildcard_match {
	char *wm_name;
	unsigned wm_no_match;
	struct ld_output_section *wm_os;
	struct ld_input_section_head *wm_islist;
	struct ld_script_sections_output_input *wm_ldoi;
	struct ld_wildcard_match *wm_next;
	UT_hash_handle hh;
};

/*
 * Support routines for output section layout.
 */

static void _calc_offset(struct ld *ld);
static void _calc_output_section_offset(struct ld *ld,
    struct ld_output_section *os);
static void _calc_reloc_section_offset(struct ld *ld, struct ld_output *lo);
static void _calc_shdr_offset(struct ld *ld);
static int _check_filename_constraint(struct ld_input *li,
    struct ld_script_sections_output_input *ldoi);
static void _insert_input_to_output(struct ld *ld, struct ld_output *lo,
    struct ld_output_section *os, struct ld_input_section *is,
    struct ld_input_section_head *islist);
static void _layout_input_sections(struct ld *ld, struct ld_input *li);
static void _layout_orphan_section(struct ld *ld, struct ld_input_section *is);
static void _layout_sections(struct ld *ld, struct ld_script_sections *ldss);
static void _parse_output_section_descriptor(struct ld *ld,
    struct ld_output_section *os);
static void _prepare_output_section(struct ld *ld,
    struct ld_script_sections_output *ldso);
static void _print_section_layout(struct ld *ld, struct ld_output_section *os);
static void _print_wildcard(struct ld_wildcard *lw);
static void _print_wildcard_list(struct ld_script_list *ldl);
static void _record_wildcard_match(struct ld *ld, char *name,
    struct ld_output_section *os, struct ld_output_element *oe);
static void _record_wildcard_no_match(struct ld *ld, char *name);
static void _set_output_section_loadable_flag(struct ld_output_section *os);
static int _wildcard_match(struct ld_wildcard *lw, const char *string);
static int _wildcard_list_match(struct ld_script_list *list,
    const char *string);

void
ld_layout_sections(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_script *lds;
	struct ld_script_cmd *ldc;
	int sections_cmd_exist;

	lo = ld->ld_output;
	lds = ld->ld_scp;

	sections_cmd_exist = 0;
	STAILQ_FOREACH(ldc, &lds->lds_c, ldc_next) {
		switch (ldc->ldc_type) {
		case LSC_ASSERT:
			ld_output_create_element(ld, &lo->lo_oelist, OET_ASSERT,
			    ldc->ldc_cmd, NULL);
			break;
		case LSC_ASSIGN:
			ld_output_create_element(ld, &lo->lo_oelist, OET_ASSIGN,
			    ldc->ldc_cmd, NULL);
			break;
		case LSC_ENTRY:
			ld_output_create_element(ld, &lo->lo_oelist, OET_ENTRY,
			    ldc->ldc_cmd, NULL);
			break;
		case LSC_SECTIONS:
			if (sections_cmd_exist)
				ld_fatal(ld, "found multiple SECTIONS commands"
				    " in the linker script");
			sections_cmd_exist = 1;
			_layout_sections(ld, ldc->ldc_cmd);
			break;
		default:
			break;
		}
	}

	if (!sections_cmd_exist)
		_layout_sections(ld, NULL);

	/* Scan and optimize .eh_frame section. */
	ld_ehframe_scan(ld);

	/* Initialise sections for dyanmically linked output object. */
	ld_dynamic_create(ld);

	/* Create ELF sections. */
	ld_output_create_elf_sections(ld);

	/* Calculate section offsets of the output object. */
	_calc_offset(ld);

	/* Calculate symbol values and indices of the output object. */
	ld_symbols_update(ld);

	/* Print out link map if requested. */
	if (ld->ld_print_linkmap)
		ld_layout_print_linkmap(ld);
}

void
ld_layout_print_linkmap(struct ld *ld)
{
	struct ld_input *li;
	struct ld_input_section *is;
	struct ld_output *lo;
	struct ld_output_element *oe;
	struct ld_script *lds;
	int i;

	lo = ld->ld_output;
	assert(lo != NULL);

	/* Print out the list of discarded sections. */
	printf("\nDiscarded input sections:\n\n");
	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		for (i = 0; (size_t) i < li->li_shnum; i++) {
			is = &li->li_is[i];
			if (is->is_discard) {
				printf(" %-20s ", is->is_name);
				if (lo->lo_ec == ELFCLASS32)
					printf("0x%08jx ",
					    (uintmax_t) is->is_addr);
				else
					printf("0x%016jx ",
					    (uintmax_t) is->is_addr);
				printf("0x%jx ", (uintmax_t) is->is_size);
				printf("%s\n", ld_input_get_fullname(ld, li));
			}
		}
	}


	lds = ld->ld_scp;
	if (lds == NULL)
		return;

	/* TODO: Dump memory configuration */

	printf("\nLinker script and memory map\n\n");

	/* TODO: Dump loaded objects. */

	STAILQ_FOREACH(oe, &lo->lo_oelist, oe_next) {

		switch (oe->oe_type) {
		case OET_ASSERT:
			/* TODO */
			break;
		case OET_ASSIGN:
			ld_script_assign_dump(ld, oe->oe_entry);
			break;
		case OET_ENTRY:
			/* TODO */
			break;
 		case OET_OUTPUT_SECTION:
			_print_section_layout(ld, oe->oe_entry);
			break;
		default:
			break;
		}
	}
}

static void
_print_section_layout(struct ld *ld, struct ld_output_section *os)
{
	struct ld_input_section *is;
	struct ld_input_section_head *islist;
	struct ld_output *lo;
	struct ld_output_element *oe;
	struct ld_script_sections_output_input *ldoi;

	lo = ld->ld_output;

	if (os->os_empty)
		printf("\n%s\n", os->os_name);
	else {
		printf("\n%-15s", os->os_name);
		if (lo->lo_ec == ELFCLASS32)
			printf(" 0x%08jx", (uintmax_t) os->os_addr);
		else
			printf(" 0x%016jx", (uintmax_t) os->os_addr);
		printf(" %#10jx\n", (uintmax_t) os->os_size);
	}

	STAILQ_FOREACH(oe, &os->os_e, oe_next) {
		switch (oe->oe_type) {
		case OET_ASSIGN:
			ld_script_assign_dump(ld, oe->oe_entry);
			break;
		case OET_INPUT_SECTION_LIST:
			/*
			 * Print out wildcard patterns and input sections
			 * matched by these patterns.
			 */
			ldoi = oe->oe_entry;
			if (ldoi == NULL)
				break;
			putchar(' ');
			if (ldoi->ldoi_ar) {
				_print_wildcard(ldoi->ldoi_ar);
				putchar(':');
			}
			_print_wildcard(ldoi->ldoi_file);
			putchar('(');
			if (ldoi->ldoi_exclude) {
				printf("(EXCLUDE_FILE(");
				_print_wildcard_list(ldoi->ldoi_exclude);
				putchar(')');
				putchar(' ');
			}
			_print_wildcard_list(ldoi->ldoi_sec);
			putchar(')');
			putchar('\n');
			if ((islist = oe->oe_islist) == NULL)
				break;
			STAILQ_FOREACH(is, islist, is_next) {
				if (!strcmp(is->is_name, "COMMON") &&
				    is->is_size == 0)
					continue;
				printf(" %-14s", is->is_name);
				if (lo->lo_ec == ELFCLASS32)
					printf(" 0x%08jx", (uintmax_t)
					    (os->os_addr + is->is_reloff));
				else
					printf(" 0x%016jx", (uintmax_t)
					    (os->os_addr + is->is_reloff));
				if (is->is_size == 0)
					printf(" %10s", "0x0");
				else
					printf(" %#10jx", (uintmax_t)
					    is->is_size);
				printf(" %s\n", ld_input_get_fullname(ld,
				    is->is_input));
			}
			break;
		default:
			break;
		}
	}
}

static void
_print_wildcard(struct ld_wildcard *lw)
{

	switch (lw->lw_sort) {
	case LWS_NONE:
		printf("%s", lw->lw_name);
		break;
	case LWS_NAME:
		printf("SORT_BY_NAME(%s)", lw->lw_name);
		break;
	case LWS_ALIGN:
		printf("SORT_BY_ALIGNMENT(%s)", lw->lw_name);
		break;
	case LWS_NAME_ALIGN:
		printf("SORT_BY_NAME(SORT_BY_ALIGNMENT(%s))", lw->lw_name);
		break;
	case LWS_ALIGN_NAME:
		printf("SORT_BY_ALIGNMENT(SORT_BY_NAME(%s))", lw->lw_name);
		break;
	default:
		break;
	}
}

static void
_print_wildcard_list(struct ld_script_list *ldl)
{

	_print_wildcard(ldl->ldl_entry);
	if (ldl->ldl_next != NULL) {
		putchar(' ');
		_print_wildcard_list(ldl->ldl_next);
	}
}

off_t
ld_layout_calc_header_size(struct ld *ld)
{
	struct ld_script_phdr *ldsp;
	struct ld_output *lo;
	struct ld_output_section *os;
	off_t header_size;
	unsigned ec, w, num_phdrs;
	int new, tls;

	lo = ld->ld_output;
	assert(lo != NULL);

	header_size = 0;

	ec = elftc_bfd_target_class(ld->ld_otgt);

	if (ec == ELFCLASS32)
		header_size += sizeof(Elf32_Ehdr);
	else
		header_size += sizeof(Elf64_Ehdr);

	/* Do not generate segments for relocatable output. */
	if (ld->ld_reloc) {
		lo->lo_phdr_num = 0;
		return (header_size);
	}

	if (!STAILQ_EMPTY(&ld->ld_scp->lds_p)) {
		num_phdrs = 0;
		STAILQ_FOREACH(ldsp, &ld->ld_scp->lds_p, ldsp_next)
			num_phdrs++;
	} else {
		if (lo->lo_phdr_num > 0)
			num_phdrs = lo->lo_phdr_num;
		else {
			num_phdrs = 0;
			new = 1;
			tls = 0;
			w = 0;
			STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
				if (os->os_empty)
					continue;

				if ((os->os_flags & SHF_ALLOC) == 0) {
					new = 1;
					continue;
				}

				if ((os->os_flags & SHF_WRITE) != w || new) {
					new = 0;
					num_phdrs++;
					w = os->os_flags & SHF_WRITE;
				}

				if ((os->os_flags & SHF_TLS) != 0 && !tls) {
					tls = 1;
					num_phdrs++;
				}
			}

			/*
			 * PT_PHDR and PT_DYNAMIC for dynamic linking. But
			 * do not create PT_PHDR for shared libraries.
			 */
			if (lo->lo_dso_needed > 0) {
				num_phdrs++;
				if (!ld->ld_dso)
					num_phdrs++;
			}

			if (lo->lo_interp != NULL)
				num_phdrs++;

			if (lo->lo_phdr_note)
				num_phdrs++;

			if (ld->ld_ehframe_hdr)
				num_phdrs++;

			if (ld->ld_gen_gnustack)
				num_phdrs++;
		}
	}

	if (ec == ELFCLASS32)
		header_size += num_phdrs * sizeof(Elf32_Phdr);
	else
		header_size += num_phdrs * sizeof(Elf64_Phdr);

	lo->lo_phdr_num = num_phdrs;

	return (header_size);
}

static void
_layout_sections(struct ld *ld, struct ld_script_sections *ldss)
{
	struct ld_input *li;
	struct ld_output *lo;
	struct ld_script_cmd *ldc;

	lo = ld->ld_output;

	/*
	 * Process commands inside the SECTIONS command and create
	 * output elements.
	 */
	STAILQ_FOREACH(ldc, &ldss->ldss_c, ldc_next) {
		switch (ldc->ldc_type) {
		case LSC_ASSERT:
			ld_output_create_element(ld, &lo->lo_oelist,
			    OET_ASSIGN, ldc->ldc_cmd, NULL);
		case LSC_ASSIGN:
			ld_output_create_element(ld, &lo->lo_oelist,
			    OET_ASSIGN, ldc->ldc_cmd, NULL);
			break;
		case LSC_ENTRY:
			ld_output_create_element(ld, &lo->lo_oelist,
			    OET_ENTRY, ldc->ldc_cmd, NULL);
			break;
		case LSC_SECTIONS_OUTPUT:
			_prepare_output_section(ld, ldc->ldc_cmd);
			break;
		case LSC_SECTIONS_OVERLAY:
			/* TODO */
			break;
		default:
			break;
		}
	}

	/* Lay out each input object. */
	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {

		/* Only lay out relocatable input objects. */
		if (li->li_type != LIT_RELOCATABLE)
			continue;

		/* Lay out sections for the input object. */
		_layout_input_sections(ld, li);
	}
}

static void
_prepare_output_section(struct ld *ld, struct ld_script_sections_output *ldso)
{
	struct ld_script_cmd *ldc;
	struct ld_input_section_head *islist;
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_element *oe;

	lo = ld->ld_output;

	HASH_FIND_STR(lo->lo_ostbl, ldso->ldso_name, os);
	if (os != NULL)
		return;

	os = ld_output_alloc_section(ld, ldso->ldso_name, NULL, NULL);
	os->os_ldso = ldso;
	_set_output_section_loadable_flag(os);

	STAILQ_FOREACH(ldc, &ldso->ldso_c, ldc_next) {
		switch (ldc->ldc_type) {
		case LSC_ASSERT:
			oe = ld_output_create_section_element(ld, os,
			    OET_ASSERT, ldc->ldc_cmd, NULL);
			break;
		case LSC_ASSIGN:
			oe = ld_output_create_section_element(ld, os,
			    OET_ASSIGN, ldc->ldc_cmd, NULL);
			break;
		case LSC_SECTIONS_OUTPUT_DATA:
			oe = ld_output_create_section_element(ld, os,
			    OET_DATA, ldc->ldc_cmd, NULL);
			break;
		case LSC_SECTIONS_OUTPUT_INPUT:
			islist = calloc(1, sizeof(*islist));
			if (islist == NULL)
				ld_fatal_std(ld, "calloc");
			STAILQ_INIT(islist);
			oe = ld_output_create_section_element(ld, os,
			    OET_INPUT_SECTION_LIST, ldc->ldc_cmd, NULL);
			oe->oe_islist = islist;
			break;
		case LSC_SECTIONS_OUTPUT_KEYWORD:
			ld_output_create_section_element(ld, os,
			    OET_KEYWORD, ldc->ldc_cmd, NULL);
			break;
		default:
			ld_fatal(ld, "internal: invalid output section "
			    "command: %d", ldc->ldc_type);
		}
	}
}

static int
_wildcard_match(struct ld_wildcard *lw, const char *string)
{

	return (fnmatch(lw->lw_name, string, 0) == 0);
}

static int
_wildcard_list_match(struct ld_script_list *list, const char *string)
{
	struct ld_script_list *ldl;

	for (ldl = list; ldl != NULL; ldl = ldl->ldl_next)
		if (_wildcard_match(ldl->ldl_entry, string))
			return (1);

	return (0);
}

static void
_set_output_section_loadable_flag(struct ld_output_section *os)
{
	struct ld_script_sections_output *ldso;
	struct ld_exp *le;

	if ((ldso = os->os_ldso) == NULL)
		return;

	if (ldso->ldso_vma == NULL)
		os->os_flags |= SHF_ALLOC;
	else {
		le = ldso->ldso_vma;
		if (le->le_op != LEOP_CONSTANT || le->le_val != 0)
			os->os_flags |= SHF_ALLOC;
	}

	if (ldso->ldso_type != NULL && strcmp(ldso->ldso_type, "NOLOAD") == 0)
		os->os_flags &= ~SHF_ALLOC;
}

static int
_check_filename_constraint(struct ld_input *li,
    struct ld_script_sections_output_input *ldoi)
{
	struct ld_file *lf;

	/* Internal sections always suffice any constraint. */
	if (li->li_name == NULL)
		return (1);

	lf = li->li_file;

	if (ldoi->ldoi_ar != NULL && li->li_lam != NULL &&
	    !_wildcard_match(ldoi->ldoi_ar, lf->lf_name))
		return (0);

	assert(ldoi->ldoi_file != NULL);
	if (!_wildcard_match(ldoi->ldoi_file, li->li_name))
		return (0);

	if (ldoi->ldoi_exclude != NULL &&
	    _wildcard_list_match(ldoi->ldoi_exclude, li->li_name))
		return (0);

	return (1);
}

static void
_record_wildcard_match(struct ld *ld, char *name, struct ld_output_section *os,
    struct ld_output_element *oe)
{
	struct ld_wildcard_match *wm, *_wm;

	assert(name != NULL && os != NULL);
	assert(oe != NULL && oe->oe_type == OET_INPUT_SECTION_LIST);

	HASH_FIND_STR(ld->ld_wm, name, wm);

	/* Create a new wildcard match. */
	if (wm == NULL) {
		if ((wm = calloc(1, sizeof(*wm))) == NULL)
			ld_fatal_std(ld, "calloc");
		if ((wm->wm_name = strdup(name)) == NULL)
			ld_fatal_std(ld, "strdup");
		wm->wm_os = os;
		wm->wm_islist = oe->oe_islist;
		wm->wm_ldoi = oe->oe_entry;
		wm->wm_next = NULL;
		HASH_ADD_KEYPTR(hh, ld->ld_wm, wm->wm_name,
		    strlen(wm->wm_name), wm);
		return;
	}

	/*
	 * Wildcard match already exist, compare the "ldoi" to check
	 * if this is a new wildcard match with a different file/archive
	 * constraint. If so, Insert it to the tail of the wildcard match
	 * list.
	 */
	do {
		if (oe->oe_entry == (void *) wm->wm_ldoi)
			return;
	} while (wm->wm_next != NULL && (wm = wm->wm_next));

	if ((_wm = calloc(1, sizeof(*_wm))) == NULL)
		ld_fatal_std(ld, "calloc");
	_wm->wm_os = os;
	_wm->wm_islist = oe->oe_islist;
	_wm->wm_ldoi = oe->oe_entry;
	_wm->wm_next = NULL;
	wm->wm_next = _wm;
}

static void
_record_wildcard_no_match(struct ld *ld, char *name)
{
	struct ld_wildcard_match *wm;

	assert(name != NULL);

	HASH_FIND_STR(ld->ld_wm, name, wm);

	/*
	 * Unfortunately this section is an orphan section because
	 * it doesn't satisfy the file/archive constraint but does
	 * match certain section name wildcard. We can not record this.
	 */
	if (wm != NULL)
		return;

	/* Create the wildcard "no-match" for the orphan. */
	if ((wm = calloc(1, sizeof(*wm))) == NULL)
		ld_fatal_std(ld, "calloc");
	if ((wm->wm_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");
	wm->wm_no_match = 1;
}

static void
_layout_input_sections(struct ld *ld, struct ld_input *li)
{
	struct ld_input_section *is;
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_element *oe;
	struct ld_wildcard_match *wm;
	struct ld_script_sections_output_input *ldoi;
	int i;

	lo = ld->ld_output;

	for (i = 0; (size_t) i < li->li_shnum; i++) {

		is = &li->li_is[i];

		if (is->is_type == SHT_NULL)
			continue;

		/* Ignore discarded section groups. */
		if (is->is_discard)
			continue;

		if (strcmp(is->is_name, ".shstrtab") == 0 ||
		    strcmp(is->is_name, ".symtab") == 0 ||
		    strcmp(is->is_name, ".strtab") == 0)
			continue;

		/* Search the wildcard match table for a quick match. */
		HASH_FIND_STR(ld->ld_wm, is->is_name, wm);
		if (wm != NULL) {
			if (wm->wm_no_match) {
				/*
				 * We found a "no-match". This is certainly
				 * an orphan section.
				 */
				_layout_orphan_section(ld, is);
				continue;
			}
		} else
			goto full_search;

		/* There is a match! Verify file/archive constraint. */
		while (wm != NULL) {
			ldoi = wm->wm_ldoi;

			if (!_check_filename_constraint(li, ldoi))
				goto next_wm;

			if (strcmp(wm->wm_os->os_name, "/DISCARD/") == 0) {
				is->is_discard = 1;
				break;
			}

			/*
			 * File/archive constraint satisfied. Insert the
			 * this section to the input section list of the
			 * output section element.
			 */
			_insert_input_to_output(ld, lo, wm->wm_os, is,
			    wm->wm_islist);
			break;

		next_wm:
			wm = wm->wm_next;
		}

		if (wm != NULL)
			continue;

	full_search:

		/*
		 * Otherwise, we have to do a full search for the section
		 * name in all the wildcard list.
		 */
		STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
			STAILQ_FOREACH(oe, &os->os_e, oe_next) {

				if (oe->oe_type != OET_INPUT_SECTION_LIST)
					continue;

				/*
				 * Skip output sections created for orphan
				 * input sections. They don't have wildcard
				 * list.
				 */
				if ((ldoi = oe->oe_entry) == NULL)
					continue;

				/* Check if the section name match wildcard */
				assert(ldoi->ldoi_sec != NULL);
				if (!_wildcard_list_match(ldoi->ldoi_sec,
				    is->is_name))
					continue;

				/*
				 * Record this wildcard match to speed up
				 * wildcard match for sections with the same
				 * name.
				 */
				_record_wildcard_match(ld, is->is_name, os,
				    oe);

				/* Check file/archive constraint.  */
				if (!_check_filename_constraint(li, ldoi)) {
					continue;
				}

				/* Check if we should discard the section. */
				if (strcmp(os->os_name, "/DISCARD/") == 0) {
					is->is_discard = 1;
					goto next_input_section;
				}

				/* Match! Insert to the input section list. */
				_insert_input_to_output(ld, lo, os, is,
				    oe->oe_islist);
				goto next_input_section;
			}
		}

		/*
		 * We found an orphan section. Record this so we can quickly
		 * identify other orphan sections with the same name.
		 */
		_record_wildcard_no_match(ld, is->is_name);

		/* Lay out the orphan section. */
		_layout_orphan_section(ld, is);

	next_input_section:
		;
	}
}

static void
_layout_orphan_section(struct ld *ld, struct ld_input_section *is)
{
	struct ld_input_section_head *islist;
	struct ld_output *lo;
	struct ld_output_element *oe;
	struct ld_output_section *os, *_os;

	/*
	 * Layout the input sections that are not listed in the output
	 * section descriptor in the linker script.
	 */

	lo = ld->ld_output;

	if (is->is_discard)
		return;

	if (strcmp(is->is_name, ".shstrtab") == 0 ||
	    strcmp(is->is_name, ".symtab") == 0 ||
	    strcmp(is->is_name, ".strtab") == 0)
		return;

	if ((is->is_type == SHT_REL || is->is_type == SHT_RELA) &&
	    !is->is_dynrel)
		return;

	/*
	 * When garbage collection is enabled (option `-gc-sections'
	 * specified), remove sections that are not used.
	 */
	if (ld->ld_gc) {
		if ((is->is_flags & SHF_ALLOC) != 0 && !is->is_refed) {
			if (ld->ld_gc_print)
				ld_info(ld, "Remove unused ection `%s' in "
				    "file %s", is->is_name,
				    ld_input_get_fullname(ld, is->is_input));
			return;
		}
	}

	HASH_FIND_STR(lo->lo_ostbl, is->is_name, os);
	if (os != NULL) {
		oe = STAILQ_FIRST(&os->os_e);
		assert(oe != NULL &&
		    oe->oe_type == OET_INPUT_SECTION_LIST);
		_insert_input_to_output(ld, lo, os, is, oe->oe_islist);
		return;
	}

	/*
	 * Create a new output secton and put it in a proper place,
	 * based on the section flag.
	 */
	_os = ld_layout_insert_output_section(ld, is->is_name,
	    is->is_flags);

	if ((islist = calloc(1, sizeof(*islist))) == NULL)
		ld_fatal_std(ld, "calloc");
	STAILQ_INIT(islist);

	oe = ld_output_create_section_element(ld, _os, OET_INPUT_SECTION_LIST,
	    NULL, NULL);
	oe->oe_islist = islist;
	_insert_input_to_output(ld, lo, _os, is, oe->oe_islist);
}

struct ld_output_section *
ld_layout_insert_output_section(struct ld *ld, const char *name,
    uint64_t flags)
{
	struct ld_output *lo;
	struct ld_output_section *os, *_os;

	lo = ld->ld_output;
	assert(lo != NULL);

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if ((os->os_flags & SHF_ALLOC) != (flags & SHF_ALLOC))
			continue;

		if (os->os_flags == flags) {
			_os = STAILQ_NEXT(os, os_next);
			if (_os == NULL || _os->os_flags != flags)
				break;
		}

		_os = STAILQ_NEXT(os, os_next);
		if (_os != NULL &&
		    (_os->os_flags & SHF_ALLOC) != (flags & SHF_ALLOC))
			break;
	}

	_os = ld_output_alloc_section(ld, name, os, NULL);
	_os->os_flags |= flags & SHF_ALLOC;

	return (_os);
}

static void
_insert_input_to_output(struct ld *ld, struct ld_output *lo,
    struct ld_output_section *os, struct ld_input_section *is,
    struct ld_input_section_head *islist)
{
	struct ld_output_section *_os;
	char *name;
	int len;

	/*
	 * Relocation sections is handled separately.
	 */
	if ((is->is_type == SHT_REL || is->is_type == SHT_RELA) &&
	    !is->is_dynrel)
		return;

	os->os_empty = 0;

	os->os_flags |= is->is_flags & (SHF_EXECINSTR | SHF_WRITE | SHF_TLS);
	os->os_dynrel |= is->is_dynrel;
	os->os_pltrel |= is->is_pltrel;

	if (!is->is_dynrel && !is->is_pltrel && is->is_type != SHT_NOBITS &&
	    is->is_size != 0)
		is->is_need_reloc = 1;

	if (is->is_align > os->os_align)
		os->os_align = is->is_align;

	/*
	 * The entsize of the output section is determined by the
	 * input sections it contains. If all the input sections has
	 * the same entsize, the output section will also have that
	 * entsize. If any input section has a different entsize,
	 * the entsize for output section is set to 0, meaning that
	 * it has variable entry sizes.
	 */
	if (!os->os_entsize_set) {
		os->os_entsize = is->is_entsize;
		os->os_entsize_set = 1;
	} else if (os->os_entsize != is->is_entsize)
		os->os_entsize = 0;

	if (os->os_type == SHT_NULL)
		os->os_type = is->is_type;
	if (is->is_type == SHT_NOTE)
		lo->lo_phdr_note = 1;

	is->is_output = os;

	STAILQ_INSERT_TAIL(islist, is, is_next);

	/*
	 * Lay out relocation section for this input section if the linker
	 * creates relocatable output object or if -emit-relocs option is
	 * sepcified.
	 */
	if ((ld->ld_reloc || ld->ld_emit_reloc) && is->is_ris != NULL &&
	    is->is_ris->is_num_reloc > 0) {
		if (os->os_r == NULL) {
			/*
			 * Create relocation section for output sections.
			 */
			if (ld->ld_arch->reloc_is_rela) {
				len = strlen(os->os_name) + 6;
				if ((name = malloc(len)) == NULL)
					ld_fatal_std(ld, "malloc");
				snprintf(name, len, ".rela%s", os->os_name);
			} else {
				len = strlen(os->os_name) + 5;
				if ((name = malloc(len)) == NULL)
					ld_fatal_std(ld, "malloc");
				snprintf(name, len, ".rel%s", os->os_name);
			}
			_os = ld_output_alloc_section(ld, name, NULL, os);
			_os->os_rel = 1;

			/*
			 * Fill in entry size, alignment and type for output
			 * relocation sections.
			 */
			_os->os_entsize = ld->ld_arch->reloc_entsize;
			_os->os_type = ld->ld_arch->reloc_is_rela ? SHT_RELA :
			    SHT_REL;
			_os->os_align = ld->ld_arch->reloc_is_64bit ? 8 : 4;

			/* Setup sh_link and sh_info. */
			if ((_os->os_link = strdup(".symtab")) == NULL)
				ld_fatal_std(ld, "strdup");
			_os->os_info = os;

			/* Relocation sections are not allocated in memory. */
			_os->os_addr = 0;
		} else
			_os = os->os_r;

		_os->os_size += is->is_ris->is_num_reloc * _os->os_entsize;
	}

}

static void
_parse_output_section_descriptor(struct ld *ld, struct ld_output_section *os)
{
	struct ld_script_sections_output *ldso;

	if ((ldso = os->os_ldso) == NULL)
		return;

	if (ldso->ldso_vma != NULL)
		os->os_addr = ld_exp_eval(ld, ldso->ldso_vma);

	if (ldso->ldso_lma != NULL)
		os->os_lma = ld_exp_eval(ld, ldso->ldso_lma);

	if (ldso->ldso_align != NULL)
		os->os_align = ld_exp_eval(ld, ldso->ldso_align);

	/* TODO: handle other output section parameters. */
}

static void
_calc_offset(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_output *lo;
	struct ld_output_element *oe;

	ls = &ld->ld_state;
	lo = ld->ld_output;
	ls->ls_loc_counter = 0;
	ls->ls_offset = ld_layout_calc_header_size(ld);
	ls->ls_first_output_sec = 1;

	STAILQ_FOREACH(oe, &lo->lo_oelist, oe_next) {
		switch (oe->oe_type) {
		case OET_ASSERT:
			/* TODO */
			break;
		case OET_ASSIGN:
			ld_script_process_assign(ld, oe->oe_entry);
			break;
		case OET_ENTRY:
			ld_script_process_entry(ld, oe->oe_entry);
			break;
 		case OET_OUTPUT_SECTION:
			_parse_output_section_descriptor(ld, oe->oe_entry);
			_calc_output_section_offset(ld, oe->oe_entry);
			break;
		default:
			break;
		}
	}

	/* Emit .note.GNU-stack section for reloctable output object. */
	if (ld->ld_gen_gnustack && ld->ld_reloc)
		ld_output_emit_gnu_stack_section(ld);

	/* Lay out section header table after normal input sections. */
	_calc_shdr_offset(ld);

	/* Create .shstrtab section and put it after section header table. */
	ld_output_create_string_table_section(ld, ".shstrtab",
	    ld->ld_shstrtab, NULL);

	/* Lay out relocation sections. */
	if (ld->ld_reloc || ld->ld_emit_reloc)
		_calc_reloc_section_offset(ld, lo);
}

static void
_calc_output_section_offset(struct ld *ld, struct ld_output_section *os)
{
	struct ld_state *ls;
	struct ld_output_element *oe;
	struct ld_output_data_buffer *odb;
	struct ld_input_section *is;
	struct ld_input_section_head *islist;
	struct ld_symbol_table *sy;
	struct ld_strtab *st;
	uint64_t addr;

	/* Relocation sections are handled separately. */
	if (os->os_rel)
		return;

	ls = &ld->ld_state;

	/*
	 * Position independent output object should have VMA from 0.
	 * So if we are building a DSO or PIE, and this output section is
	 * the first one, we should set current VMA to SIZEOF_HEADERS
	 * and ignore all the previous assignments to the location counter.
	 */
	if ((ld->ld_dso || ld->ld_pie) && ls->ls_first_output_sec) {
		ls->ls_loc_counter = ld_layout_calc_header_size(ld);
		if (!os->os_empty)
			ls->ls_first_output_sec = 0;
	}

	/*
	 * Location counter stores the end VMA offset of the previous output
	 * section. We use that value as the base VMA offset for this output
	 * section.
	 */
	addr = ls->ls_loc_counter;

	/*
	 * Location counter when refered inside an output section descriptor,
	 * is an offset relative to the start of the section.
	 */
	ls->ls_loc_counter = 0;

	STAILQ_FOREACH(oe, &os->os_e, oe_next) {
		switch (oe->oe_type) {
		case OET_ASSERT:
			/* TODO */
			break;
		case OET_ASSIGN:
			ld_script_process_assign(ld, oe->oe_entry);
			break;
		case OET_DATA:
			/* TODO */
			break;
		case OET_DATA_BUFFER:
			odb = oe->oe_entry;
			odb->odb_off = roundup(ls->ls_loc_counter,
			    odb->odb_align);
			ls->ls_loc_counter = odb->odb_off + odb->odb_size;
			break;
		case OET_ENTRY:
			ld_script_process_entry(ld, oe->oe_entry);
			break;
		case OET_INPUT_SECTION_LIST:
			islist = oe->oe_islist;
			STAILQ_FOREACH(is, islist, is_next) {
				if (is->is_size == 0)
					continue;
				is->is_reloff = roundup(ls->ls_loc_counter,
				    is->is_align);
#if 0
				printf("\t%s(%s): %#jx,%#jx(%#jx)\n",
				    is->is_input->li_name,
				    is->is_name, is->is_reloff,
				    is->is_size, is->is_align);
#endif
				ls->ls_loc_counter = is->is_reloff +
				    is->is_size;
			}
			break;
		case OET_KEYWORD:
			/* TODO */
			break;
		case OET_SYMTAB:
			assert(ls->ls_loc_counter == 0);
			sy = oe->oe_entry;
			ls->ls_loc_counter = sy->sy_size * os->os_entsize;
			break;
		case OET_STRTAB:
			assert(ls->ls_loc_counter == 0);
			st = oe->oe_entry;
			ls->ls_loc_counter = ld_strtab_getsize(st);
			break;
		default:
			break;
		}
	}

	/*
	 * Properly align section vma and offset to the required section
	 * alignment.
	 */

	if ((os->os_flags & SHF_ALLOC) != 0 && !ld->ld_reloc) {
		if (os->os_ldso == NULL || os->os_ldso->ldso_vma == NULL)
			os->os_addr = roundup(addr, os->os_align);
	} else
		os->os_addr = 0;

	os->os_off = roundup(ls->ls_offset, os->os_align);
	os->os_size = ls->ls_loc_counter;

#if 0
	printf("layout output section %s: (off:%#jx,size:%#jx) "
	    "vma:%#jx,align:%#jx\n", os->os_name, os->os_off, os->os_size,
	    os->os_addr, os->os_align);
#endif

	/*
	 * Calculate the file offset for the next output section. Note that
	 * only sections with type other than SHT_NOBITS consume file space.
	 */
	ls->ls_offset = os->os_off;
	if (os->os_type != SHT_NOBITS)
		ls->ls_offset += os->os_size;

	/* Reset location counter to the current VMA. */
	if (os->os_flags & SHF_ALLOC) {
		ls->ls_loc_counter = os->os_addr;
		/*
		 * Do not allocate VMA for TLS .tbss sections. TLS sections
		 * are only used as an initialization image and .tbss section
		 * will not be allocated in memory.
		 */
		if (os->os_type != SHT_NOBITS || (os->os_flags & SHF_TLS) == 0)
			ls->ls_loc_counter += os->os_size;
	}
}

static void
_calc_reloc_section_offset(struct ld *ld, struct ld_output *lo)
{
	struct ld_state *ls;
	struct ld_output_section *os, *_os;

	ls = &ld->ld_state;

	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_r != NULL) {
			_os = os->os_r;
			_os->os_off = roundup(ls->ls_offset, _os->os_align);
			ls->ls_offset = _os->os_off + _os->os_size;
		}
	}
}

static void
_calc_shdr_offset(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_output *lo;
	struct ld_output_section *os;
	uint64_t shoff;
	int n;

	ls = &ld->ld_state;
	lo = ld->ld_output;

	if (lo->lo_ec == ELFCLASS32)
		shoff = roundup(ls->ls_offset, 4);
	else
		shoff = roundup(ls->ls_offset, 8);

	ls->ls_offset = shoff;

	n = 0;
	STAILQ_FOREACH(os, &lo->lo_oslist, os_next) {
		if (os->os_scn != NULL)
			n++;
	}

	/* TODO: n + 2 if ld(1) will not create symbol table. */
	ls->ls_offset += gelf_fsize(lo->lo_elf, ELF_T_SHDR, n + 4, EV_CURRENT);

	lo->lo_shoff = shoff;
}
