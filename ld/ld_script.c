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
#include "ld_exp.h"
#include "ld_options.h"
#include "ld_script.h"
#include "ld_file.h"
#include "ld_symbols.h"
#include "ld_output.h"

ELFTC_VCSID("$Id: ld_script.c 3281 2015-12-11 21:39:23Z kaiwang27 $");

static void _input_file_add(struct ld *ld, struct ld_script_input_file *ldif);
static void _overlay_section_free(void *ptr);
static struct ld_script_variable *_variable_find(struct ld *ld, char *name);

#define _variable_add(v) \
	HASH_ADD_KEYPTR(hh, ld->ld_scp->lds_v, (v)->ldv_name, \
	    strlen((v)->ldv_name), (v))

struct ld_script_cmd *
ld_script_assert(struct ld *ld, struct ld_exp *exp, char *msg)
{
	struct ld_script_assert *a;

	if ((a = calloc(1, sizeof(*a))) == NULL)
		ld_fatal_std(ld, "calloc");
	a->lda_exp = exp;
	a->lda_msg = msg;

	return (ld_script_cmd(ld, LSC_ASSERT, a));
}

struct ld_script_assign *
ld_script_assign(struct ld *ld, struct ld_exp *var, enum ld_script_assign_op op,
    struct ld_exp *val, unsigned provide, unsigned hidden)
{
	struct ld_script_assign *lda;
	struct ld_script_variable *ldv;

	if ((lda = calloc(1, sizeof(*lda))) == NULL)
		ld_fatal_std(ld, "calloc");

	lda->lda_var = var;
	lda->lda_op = op;
	lda->lda_val = val;
	lda->lda_provide = provide;

	if ((ldv = _variable_find(ld, var->le_name)) == NULL) {
		ldv = calloc(1, sizeof(*ldv));
		if ((ldv->ldv_name = strdup(var->le_name)) == NULL)
			ld_fatal_std(ld, "strdup");
		_variable_add(ldv);
		if (*var->le_name != '.')
			ld_symbols_add_variable(ld, ldv, provide, hidden);
	}

	return (lda);
}

void
ld_script_assign_dump(struct ld *ld, struct ld_script_assign *lda)
{

	printf("%16s", "");
	printf("0x%016jx ", (uintmax_t) lda->lda_res);

	if (lda->lda_provide)
		printf("PROVIDE(");

	ld_exp_dump(ld, lda->lda_var);

	switch (lda->lda_op) {
	case LSAOP_ADD_E:
		printf(" += ");
		break;
	case LSAOP_AND_E:
		printf(" &= ");
		break;
	case LSAOP_DIV_E:
		printf(" /= ");
		break;
	case LSAOP_E:
		printf(" = ");
		break;
	case LSAOP_LSHIFT_E:
		printf(" <<= ");
		break;
	case LSAOP_MUL_E:
		printf(" *= ");
		break;
	case LSAOP_OR_E:
		printf(" |= ");
		break;
	case LSAOP_RSHIFT_E:
		printf(" >>= ");
		break;
	case LSAOP_SUB_E:
		printf(" -= ");
		break;
	default:
		ld_fatal(ld, "internal: unknown assignment op: %d",
		    lda->lda_op);
	}

	ld_exp_dump(ld, lda->lda_val);

	if (lda->lda_provide)
		printf(")");

	printf("\n");
}

void
ld_script_assign_free(struct ld_script_assign *lda)
{

	if (lda == NULL)
		return;
	ld_exp_free(lda->lda_var);
	ld_exp_free(lda->lda_val);
	free(lda);
}

static void
_update_variable_section(struct ld *ld, struct ld_script_variable *ldv)
{
	struct ld_output_section *os, *last;

	if (ldv->ldv_os_base) {
		/* Get base address of the section. */
		STAILQ_FOREACH(os, &ld->ld_output->lo_oslist, os_next) {
			if (strcmp(os->os_name, ldv->ldv_os_base) == 0) {
				ldv->ldv_base = os->os_addr;
				ldv->ldv_os_ref = ldv->ldv_os_base;
				ldv->ldv_os_base = 0;
				break;
			}
		}
	}

	if (ldv->ldv_os_ref) {
		/* Bind the symbol to the last section. */
		last = 0;
		STAILQ_FOREACH(os, &ld->ld_output->lo_oslist, os_next) {
			if (! os->os_empty)
				last = os;
			if (strcmp(os->os_name, ldv->ldv_os_ref) == 0) {
				if (last) {
					ldv->ldv_symbol->lsb_shndx = elf_ndxscn(last->os_scn);
				}
				ldv->ldv_os_ref = 0;
				break;
			}
		}
	}
}

void
ld_script_process_assign(struct ld *ld, struct ld_script_assign *lda)
{
	struct ld_state *ls;
	struct ld_exp *var;
	struct ld_script_variable *ldv;

	ls = &ld->ld_state;
	var = lda->lda_var;
	ldv = _variable_find(ld, var->le_name);
	assert(ldv != NULL);

	ldv->ldv_val = ld_exp_eval(ld, lda->lda_val);
	if (*var->le_name == '.') {
		/*
		 * TODO: location counter is allowed to move backwards
		 * outside output section descriptor, as long as the
		 * move will not cause overlapping LMA's.
		 */
		if ((uint64_t) ldv->ldv_val < ls->ls_loc_counter)
			ld_fatal(ld, "cannot move location counter backwards"
			    " from %#jx to %#jx",
			    (uintmax_t) ls->ls_loc_counter,
			    (uintmax_t) ldv->ldv_val);
		ls->ls_loc_counter = (uint64_t) ldv->ldv_val;

	} else if (ldv->ldv_symbol != NULL) {
		_update_variable_section(ld, ldv);
		ldv->ldv_symbol->lsb_value = ldv->ldv_val + ldv->ldv_base;
	}
	lda->lda_res = ldv->ldv_val;
}

void
ld_script_process_entry(struct ld *ld, char *name)
{

	if (ld->ld_scp->lds_entry_point != NULL) {
		free(ld->ld_scp->lds_entry_point);
		ld->ld_scp->lds_entry_point = NULL;
	}

	ld->ld_scp->lds_entry_point = strdup(name);
	if (ld->ld_scp->lds_entry_point == NULL)
		ld_fatal_std(ld, "strdup");
}

int64_t
ld_script_variable_value(struct ld *ld, char *name)
{
	struct ld_script_variable *ldv;
	struct ld_state *ls;

	ls = &ld->ld_state;
	if (*name == '.')
		return (ls->ls_loc_counter);

	ldv = _variable_find(ld, name);
	assert(ldv != NULL);

	return (ldv->ldv_val);
}

struct ld_script_cmd *
ld_script_cmd(struct ld *ld, enum ld_script_cmd_type type, void *cmd)
{
	struct ld_script_cmd *ldc;

	if ((ldc = calloc(1, sizeof(*ldc))) == NULL)
		ld_fatal_std(ld, "calloc");
	ldc->ldc_type = type;
	ldc->ldc_cmd = cmd;

	return (ldc);
}

void
ld_script_cmd_insert(struct ld_script_cmd_head *head, struct ld_script_cmd *ldc)
{

	STAILQ_INSERT_TAIL(head, ldc, ldc_next);
}

static void
_overlay_section_free(void *ptr)
{
	struct ld_script_cmd *c, *_c;
	struct ld_script_sections_overlay_section *ldos;

	ldos = ptr;
	if (ldos == NULL)
		return;
	free(ldos->ldos_name);
	ld_script_list_free(ldos->ldos_phdr, free);
	ld_exp_free(ldos->ldos_fill);
	STAILQ_FOREACH_SAFE(c, &ldos->ldos_c, ldc_next, _c) {
		STAILQ_REMOVE(&ldos->ldos_c, c, ld_script_cmd, ldc_next);
		ld_script_cmd_free(c);
	}
	free(ldos);
}

void
ld_script_cmd_free(struct ld_script_cmd *ldc)
{
	struct ld_script_cmd *c, *_c;
	struct ld_script_assert *lda;
	struct ld_script_sections *ldss;
	struct ld_script_sections_output *ldso;
	struct ld_script_sections_output_data *ldod;
	struct ld_script_sections_output_input *ldoi;
	struct ld_script_sections_overlay *ldso2;

	switch (ldc->ldc_type) {
	case LSC_ASSERT:
		lda = ldc->ldc_cmd;
		ld_exp_free(lda->lda_exp);
		free(lda->lda_msg);
		free(lda);
		break;

	case LSC_ASSIGN:
		ld_script_assign_free(ldc->ldc_cmd);
		break;

	case LSC_ENTRY:
		free(ldc->ldc_cmd);
		break;

	case LSC_SECTIONS:
		ldss = ldc->ldc_cmd;
		STAILQ_FOREACH_SAFE(c, &ldss->ldss_c, ldc_next, _c) {
			STAILQ_REMOVE(&ldss->ldss_c, c, ld_script_cmd,
			    ldc_next);
			ld_script_cmd_free(c);
		}
		free(ldss);
		break;

	case LSC_SECTIONS_OUTPUT:
		ldso = ldc->ldc_cmd;
		free(ldso->ldso_name);
		free(ldso->ldso_type);
		ld_exp_free(ldso->ldso_vma);
		ld_exp_free(ldso->ldso_lma);
		ld_exp_free(ldso->ldso_align);
		ld_exp_free(ldso->ldso_subalign);
		free(ldso->ldso_constraint);
		free(ldso->ldso_region);
		free(ldso->ldso_lma_region);
		ld_script_list_free(ldso->ldso_phdr, free);
		ld_exp_free(ldso->ldso_fill);
		STAILQ_FOREACH_SAFE(c, &ldso->ldso_c, ldc_next, _c) {
			STAILQ_REMOVE(&ldso->ldso_c, c, ld_script_cmd,
			    ldc_next);
			ld_script_cmd_free(c);
		}
		free(ldso);
		break;

	case LSC_SECTIONS_OUTPUT_DATA:
		ldod = ldc->ldc_cmd;
		ld_exp_free(ldod->ldod_exp);
		free(ldod);
		break;

	case LSC_SECTIONS_OUTPUT_INPUT:
		ldoi = ldc->ldc_cmd;
		ld_wildcard_free(ldoi->ldoi_ar);
		ld_wildcard_free(ldoi->ldoi_file);
		ld_script_list_free(ldoi->ldoi_exclude, ld_wildcard_free);
		ld_script_list_free(ldoi->ldoi_sec, ld_wildcard_free);
		free(ldoi);
		break;
		
	case LSC_SECTIONS_OVERLAY:
		ldso2 = ldc->ldc_cmd;
		ld_exp_free(ldso2->ldso_vma);
		ld_exp_free(ldso2->ldso_lma);
		free(ldso2->ldso_region);
		ld_script_list_free(ldso2->ldso_phdr, free);
		ld_exp_free(ldso2->ldso_fill);
		ld_script_list_free(ldso2->ldso_s, _overlay_section_free);
		free(ldso2);
		break;

	default:
		break;
	}

	free(ldc);
}

void
ld_script_extern(struct ld *ld, struct ld_script_list *list)
{
	struct ld_script_list *ldl;

	ldl = list;
	while (ldl != NULL) {
		ld_symbols_add_extern(ld, ldl->ldl_entry);
		ldl = ldl->ldl_next;
	}
	ld_script_list_free(list, free);
}

void
ld_script_group(struct ld *ld, struct ld_script_list *list)
{
	struct ld_script_list *ldl;

	ld->ld_state.ls_group_level++;
	if (ld->ld_state.ls_group_level > LD_MAX_NESTED_GROUP)
		ld_fatal(ld, "too many nested archive groups");
	ldl = list;
	while (ldl != NULL) {
		_input_file_add(ld, ldl->ldl_entry);
		ldl = ldl->ldl_next;
	}
	ld->ld_state.ls_group_level--;
	ld_script_list_free(list, free);
}

void
ld_script_init(struct ld *ld)
{

	ld->ld_scp = calloc(1, sizeof(*ld->ld_scp));
	if (ld->ld_scp == NULL)
		ld_fatal_std(ld, "calloc");

	STAILQ_INIT(&ld->ld_scp->lds_a);
	STAILQ_INIT(&ld->ld_scp->lds_c);
	STAILQ_INIT(&ld->ld_scp->lds_n);
	STAILQ_INIT(&ld->ld_scp->lds_p);
	STAILQ_INIT(&ld->ld_scp->lds_r);
	STAILQ_INIT(&ld->ld_scp->lds_vn);

	ld_script_parse_internal();
}

void
ld_script_cleanup(struct ld *ld)
{
	struct ld_script *lds;
	struct ld_script_phdr *p, *_p;
	struct ld_script_region *r, *_r;
	struct ld_script_region_alias *a, *_a;
	struct ld_script_nocrossref *n, *_n;
	struct ld_script_cmd *c, *_c;
	struct ld_script_variable *v, *_v;

	if (ld->ld_scp == NULL)
		return;

	lds = ld->ld_scp;

	if (lds->lds_entry_point != NULL) {
		free(lds->lds_entry_point);
		lds->lds_entry_point = NULL;
	}

	STAILQ_FOREACH_SAFE(p, &lds->lds_p, ldsp_next, _p) {
		STAILQ_REMOVE(&lds->lds_p, p, ld_script_phdr, ldsp_next);
		free(p->ldsp_name);
		free(p->ldsp_type);
		ld_exp_free(p->ldsp_addr);
		free(p);
	}

	STAILQ_FOREACH_SAFE(r, &lds->lds_r, ldsr_next, _r) {
		STAILQ_REMOVE(&lds->lds_r, r, ld_script_region, ldsr_next);
		free(r->ldsr_name);
		free(r->ldsr_attr);
		ld_exp_free(r->ldsr_origin);
		ld_exp_free(r->ldsr_len);
		free(r);
	}

	STAILQ_FOREACH_SAFE(a, &lds->lds_a, ldra_next, _a) {
		STAILQ_REMOVE(&lds->lds_a, a, ld_script_region_alias,
		    ldra_next);
		free(a->ldra_alias);
		free(a->ldra_region);
		free(a);
	}

	STAILQ_FOREACH_SAFE(n, &lds->lds_n, ldn_next, _n) {
		STAILQ_REMOVE(&lds->lds_n, n, ld_script_nocrossref, ldn_next);
		ld_script_list_free(n->ldn_l, free);
		free(n);
	}

	STAILQ_FOREACH_SAFE(c, &lds->lds_c, ldc_next, _c) {
		STAILQ_REMOVE(&lds->lds_c, c, ld_script_cmd, ldc_next);
		ld_script_cmd_free(c);
	}

	if (lds->lds_v != NULL) {
		HASH_ITER(hh, lds->lds_v, v, _v) {
			HASH_DEL(lds->lds_v, v);
			free(v->ldv_name);
			free(v);
		}
		lds->lds_v = NULL;
	}
}

void
ld_script_input(struct ld *ld, struct ld_script_list *list)
{
	struct ld_script_list *ldl;

	ld->ld_state.ls_search_dir = 1;
	ldl = list;
	while (ldl != NULL) {
		_input_file_add(ld, ldl->ldl_entry);
		ldl = ldl->ldl_next;
	}
	ld->ld_state.ls_search_dir = 0;
	ld_script_list_free(list, free);
}

struct ld_script_input_file *
ld_script_input_file(struct ld *ld, unsigned as_needed, void *in)
{
	struct ld_script_input_file *ldif;

	if ((ldif = calloc(1, sizeof(*ldif))) == NULL)
		ld_fatal_std(ld, "calloc");
	ldif->ldif_as_needed = as_needed;
	if (as_needed)
		ldif->ldif_u.ldif_ldl = in;
	else
		ldif->ldif_u.ldif_name = in;

	return (ldif);
}

struct ld_script_list *
ld_script_list(struct ld *ld, struct ld_script_list *list, void *entry)
{
	struct ld_script_list *ldl;

	if ((ldl = malloc(sizeof(*ldl))) == NULL)
		ld_fatal_std(ld, "malloc");
	ldl->ldl_entry = entry;
	ldl->ldl_next = list;

	return (ldl);
}

void
ld_script_list_free(struct ld_script_list *list, void (*_free)(void *ptr))
{
	struct ld_script_list *ldl;

	if (list == NULL)
		return;

	do {
		ldl = list;
		list = ldl->ldl_next;
		if (ldl->ldl_entry)
			_free(ldl->ldl_entry);
		free(ldl);
	} while (list != NULL);
}

struct ld_script_list *
ld_script_list_reverse(struct ld_script_list *list)
{
	struct ld_script_list *root, *next;

	root = NULL;
	while (list != NULL) {
		next = list->ldl_next;
		list->ldl_next = root;
		root = list;
		list = next;
	}

	return (root);
}

void
ld_script_nocrossrefs(struct ld *ld, struct ld_script_list *list)
{
	struct ld_script_nocrossref *ldn;

	if ((ldn = calloc(1, sizeof(*ldn))) == NULL)
		ld_fatal_std(ld, "calloc");
	ldn->ldn_l = list;
	STAILQ_INSERT_TAIL(&ld->ld_scp->lds_n, ldn, ldn_next);
}

struct ld_script_phdr *
ld_script_phdr(struct ld *ld, char *name, char *type, unsigned filehdr,
    unsigned phdrs, struct ld_exp *addr, unsigned flags)
{
	struct ld_script_phdr *ldsp;

	if ((ldsp = calloc(1, sizeof(*ldsp))) == NULL)
		ld_fatal_std(ld, "calloc");

	ldsp->ldsp_name = name;
	ldsp->ldsp_type = type;
	ldsp->ldsp_filehdr = filehdr;
	ldsp->ldsp_phdrs = phdrs;
	ldsp->ldsp_addr = addr;
	ldsp->ldsp_flags = flags;

	return (ldsp);
}

struct ld_script_region *
ld_script_region(struct ld *ld, char *name, char *attr, struct ld_exp *origin,
    struct ld_exp *len)
{
	struct ld_script_region *ldsr;

	if ((ldsr = malloc(sizeof(*ldsr))) == NULL)
		ld_fatal_std(ld, "malloc");

	ldsr->ldsr_name = name;
	ldsr->ldsr_attr = attr;
	ldsr->ldsr_origin = origin;
	ldsr->ldsr_len = len;

	return (ldsr);
}

void
ld_script_region_alias(struct ld *ld, char *alias, char *region)
{
	struct ld_script_region_alias *ldra;

	if ((ldra = calloc(1, sizeof(*ldra))) == NULL)
		ld_fatal_std(ld, "calloc");

	ldra->ldra_alias = alias;
	ldra->ldra_region = region;

	STAILQ_INSERT_TAIL(&ld->ld_scp->lds_a, ldra, ldra_next);
}

void
ld_script_version_add_node(struct ld *ld, char *ver, void *head, char *depend)
{
	struct ld_script_version_node *ldvn;

	if ((ldvn = calloc(1, sizeof(*ldvn))) == NULL)
		ld_fatal_std(ld, "calloc");
	
	ldvn->ldvn_name = ver;
	if (ldvn->ldvn_name == NULL) {
		/*
		 * Version name can be omitted only when this is the only
		 * node in the version script.
		 */
		if (ld->ld_scp->lds_vn_name_omitted ||
		    !STAILQ_EMPTY(&ld->ld_scp->lds_vn))
			ld_fatal(ld, "version script can only have one "
			    "version node that is without a version name");
		ld->ld_scp->lds_vn_name_omitted = 1;
	}

	ldvn->ldvn_dep = depend;
	ldvn->ldvn_e = head;

	STAILQ_INSERT_TAIL(&ld->ld_scp->lds_vn, ldvn, ldvn_next);
}

struct ld_script_version_entry *
ld_script_version_alloc_entry(struct ld *ld, char *sym, void *extern_block)
{
	struct ld_state *ls;
	struct ld_script_version_entry *ldve;
	int ignore;
	char *p;

	ls = &ld->ld_state;

	if ((ldve = calloc(1, sizeof(*ldve))) == NULL)
		ld_fatal_std(ld, "calloc");

	ldve->ldve_sym = sym;
	ldve->ldve_local = ls->ls_version_local;
	ldve->ldve_list = extern_block;

	if (ldve->ldve_sym == NULL)
		return (ldve);

	ignore = 0;
	for (p = ldve->ldve_sym; *p != '\0'; p++) {
		switch (*p) {
		case '\\':
			/* Ignore the next char */
			ignore = 1;
			break;
		case '?':
		case '*':
		case '[':
			if (!ignore) {
				ldve->ldve_glob = 1;
				goto done;
			} else
				ignore = 0;
		}
	}

done:
	return (ldve);
}

void *
ld_script_version_link_entry(struct ld *ld,
    struct ld_script_version_entry_head *head,
    struct ld_script_version_entry *ldve)
{

	if (ldve == NULL)
		return (head);

	if (head == NULL) {
		if ((head = calloc(1, sizeof(*head))) == NULL)
			ld_fatal_std(ld, "calloc");
		STAILQ_INIT(head);
	}

	if (ldve->ldve_list != NULL) {
		STAILQ_CONCAT(head, ldve->ldve_list);
		free(ldve->ldve_list);
		free(ldve);
	} else
		STAILQ_INSERT_TAIL(head, ldve, ldve_next);

	return (head);
}

void
ld_script_version_set_lang(struct ld * ld,
    struct ld_script_version_entry_head *head, char *lang)
{
	struct ld_script_version_entry *ldve;
	enum ld_script_version_lang vl;

	vl = VL_C;

	if (!strcasecmp(lang, "c"))
		vl = VL_C;
	else if (!strcasecmp(lang, "c++"))
		vl = VL_CPP;
	else if (!strcasecmp(lang, "java"))
		vl = VL_JAVA;
	else
		ld_warn(ld, "unrecognized language `%s' in version script",
		    lang);

	STAILQ_FOREACH(ldve, head, ldve_next) {
		/* Do not overwrite lang set by inner extern blocks. */
		if (!ldve->ldve_lang_set) {
			ldve->ldve_lang = vl;
			ldve->ldve_lang_set = 1;
		}
	}
}
    

static void
_input_file_add(struct ld *ld, struct ld_script_input_file *ldif)
{
	struct ld_state *ls;
	struct ld_script_list *ldl;
	unsigned old_as_needed;

	ls = &ld->ld_state;

	if (!ldif->ldif_as_needed) {
		ld_file_add(ld, ldif->ldif_u.ldif_name, LFT_UNKNOWN);
		free(ldif->ldif_u.ldif_name);
	} else {
		old_as_needed = ls->ls_as_needed;
		ls->ls_as_needed = 1;
		ldl = ldif->ldif_u.ldif_ldl;
		while (ldl != NULL) {
			ld_file_add(ld, ldl->ldl_entry, LFT_UNKNOWN);
			ldl = ldl->ldl_next;
		}
		ls->ls_as_needed = old_as_needed;
		ld_script_list_free(ldif->ldif_u.ldif_ldl, free);
	}
}

static struct ld_script_variable *
_variable_find(struct ld *ld, char *name)
{
	struct ld_script_variable *ldv;

	HASH_FIND_STR(ld->ld_scp->lds_v, name, ldv);

	return (ldv);
}
