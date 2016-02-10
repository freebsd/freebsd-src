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
 * $Id: ld_script.h 3281 2015-12-11 21:39:23Z kaiwang27 $
 */

enum ld_script_cmd_type {
	LSC_ASSERT,
	LSC_ASSIGN,
	LSC_AS_NEEDED,
	LSC_ENTRY,
	LSC_EXTERN,
	LSC_FCA,
	LSC_HIDDEN_ASSIGN,
	LSC_ICA,
	LSC_INPUT,
	LSC_MEMORY,
	LSC_NOCROSSREFS,
	LSC_OUTPUT,
	LSC_OUTPUT_ARCH,
	LSC_OUTPUT_FORMAT,
	LSC_PHDRS,
	LSC_PROVIDE_ASSIGN,
	LSC_REGION_ALIAS,
	LSC_SEARCH_DIR,
	LSC_SECTIONS,
	LSC_SECTIONS_OUTPUT,
	LSC_SECTIONS_OUTPUT_DATA,
	LSC_SECTIONS_OUTPUT_INPUT,
	LSC_SECTIONS_OUTPUT_KEYWORD,
	LSC_SECTIONS_OVERLAY,
	LSC_STARTUP,
	LSC_TARGET,
	LSC_VERSION,
};

struct ld_script_cmd {
	enum ld_script_cmd_type ldc_type; /* ldscript cmd type */
	void *ldc_cmd;			/* ldscript cmd */
	STAILQ_ENTRY(ld_script_cmd) ldc_next; /* next cmd */
};

STAILQ_HEAD(ld_script_cmd_head, ld_script_cmd);

struct ld_script_list {
	void *ldl_entry;		/* list entry */
	struct ld_script_list *ldl_next; /* next entry */
};

struct ld_script_assert {
	struct ld_exp *lda_exp;		/* expression to assert */
	char *lda_msg;			/* assertion message */
};

enum ld_script_assign_op {
	LSAOP_ADD_E,
	LSAOP_AND_E,
	LSAOP_DIV_E,
	LSAOP_E,
	LSAOP_LSHIFT_E,
	LSAOP_MUL_E,
	LSAOP_OR_E,
	LSAOP_RSHIFT_E,
	LSAOP_SUB_E,
};

struct ld_script_assign {
	struct ld_exp *lda_var;		/* symbol */
	struct ld_exp *lda_val;		/* value */
	enum ld_script_assign_op lda_op; /* assign op */
	unsigned lda_provide;		/* provide assign */
	int64_t lda_res;		/* assign result */
};

struct ld_script_input_file {
	unsigned ldif_as_needed;	/* as_needed list */
	union {
		char *ldif_name;	/* input file name */
		struct ld_script_list *ldif_ldl; /* input file list */
	} ldif_u;
};

struct ld_script_nocrossref {
	struct ld_script_list *ldn_l;	/* nocrossref sections */
	STAILQ_ENTRY(ld_script_nocrossref) ldn_next; /* next nocrossref */
};

struct ld_script_region {
	char *ldsr_name;		/* memory region name */
	char *ldsr_attr;		/* memory region attribute */
	struct ld_exp *ldsr_origin;	/* memroy region start address */
	struct ld_exp *ldsr_len;	/* memroy region length */
	STAILQ_ENTRY(ld_script_region) ldsr_next; /* next memory region */
};

struct ld_script_region_alias {
	char *ldra_alias;		/* memory region alias name */
	char *ldra_region; 		/* memory region */
	STAILQ_ENTRY(ld_script_region_alias) ldra_next; /* next region alias */
};

struct ld_script_phdr {
	char *ldsp_name;		/* phdr name */
	char *ldsp_type;		/* phdr type */
	unsigned ldsp_filehdr;		/* FILEHDR keyword */
	unsigned ldsp_phdrs;		/* PHDRS keyword */
	struct ld_exp *ldsp_addr;	/* segment address */
	unsigned ldsp_flags;		/* segment flags */
	STAILQ_ENTRY(ld_script_phdr) ldsp_next; /* next phdr */
};

enum ld_script_sections_output_data_type {
	LSODT_BYTE,
	LSODT_SHORT,
	LSODT_LONG,
	LSODT_QUAD,
	LSODT_SQUAD,
	LSODT_FILL,
};

struct ld_script_sections_output_data {
	enum ld_script_sections_output_data_type ldod_type; /* data type */
	struct ld_exp *ldod_exp;	/* data expression */
};

struct ld_script_sections_output_input {
	struct ld_wildcard *ldoi_ar;	/* archive name */
	struct ld_wildcard *ldoi_file;	/* file/member name */
	struct ld_script_list *ldoi_exclude; /* exclude file list */
	struct ld_script_list *ldoi_sec; /* section name list */
	unsigned ldoi_flags;		/* input section flags */
	unsigned ldoi_keep;		/* keep input section */
};

enum ld_script_sections_output_keywords {
	LSOK_CONSTRUCTORS,
	LSOK_CONSTRUCTORS_SORT_BY_NAME,
	LSOK_CREATE_OBJECT_SYMBOLS,
};

struct ld_script_sections_output {
	char *ldso_name;		/* output section name */
	char *ldso_type;		/* output section type */
	struct ld_exp *ldso_vma;	/* output section vma */
	struct ld_exp *ldso_lma;	/* output section lma */
	struct ld_exp *ldso_align;	/* output section align */
	struct ld_exp *ldso_subalign;	/* output sectino subalign */
	char *ldso_constraint;		/* output section constraint */
	char *ldso_region; 		/* output section region */
	char *ldso_lma_region;		/* output section lma region */
	struct ld_script_list *ldso_phdr; /* output section segment list */
	struct ld_exp *ldso_fill;	/* output section fill exp */
	struct ld_script_cmd_head ldso_c; /* output section cmd list */
};

struct ld_script_sections_overlay_section {
	char *ldos_name;		/* overlay section name */
	struct ld_script_list *ldos_phdr; /* overlay section segment */
	struct ld_exp *ldos_fill;	/* overlay section fill exp */
	struct ld_script_cmd_head ldos_c; /* output section cmd list */
};

struct ld_script_sections_overlay {
	struct ld_exp *ldso_vma;	/* overlay vma */
	struct ld_exp *ldso_lma;	/* overlay lma */
	unsigned ldso_nocrossref;	/* no corss-ref between sections */
	char *ldso_region; 		/* overlay region */
	struct ld_script_list *ldso_phdr; /* overlay segment */
	struct ld_exp *ldso_fill;	/* overlay fill exp */
	struct ld_script_list *ldso_s;	/* overlay section list */
};

struct ld_script_sections {
	struct ld_script_cmd_head ldss_c; /* section cmd list */
};

struct ld_script_variable {
	char *ldv_name;			/* variable name */
	char *ldv_os_base;		/* add base address of this section */
	char *ldv_os_ref;		/* link symbol to this section */
	struct ld_symbol *ldv_symbol;	/* assoicated symbol */
	int64_t ldv_val;		/* variable value */
	int64_t ldv_base;		/* base value */
	UT_hash_handle hh;		/* hash handle */
};

enum ld_script_version_lang {
	VL_C = 0,
	VL_CPP,
	VL_JAVA
};

struct ld_script_version_entry_head;

struct ld_script_version_entry {
	enum ld_script_version_lang ldve_lang; /* version entry lanauage */
	char *ldve_sym; /* symbol wildcard */
	unsigned char ldve_local; /* symbol scope */
	unsigned char ldve_glob;  /* ldve_sym contains glob chars. */
	STAILQ_ENTRY(ld_script_version_entry) ldve_next;

	/* Following fields are only used during script parsing. */
	struct ld_script_version_entry_head *ldve_list; /* extern block */
	unsigned char ldve_lang_set; /* lang is set  */
};

STAILQ_HEAD(ld_script_version_entry_head, ld_script_version_entry);

struct ld_script_version_node {
	char *ldvn_name; /* version name */
	char *ldvn_dep; /* version dependency */
	struct ld_script_version_entry_head *ldvn_e; /* version entries */
	STAILQ_ENTRY(ld_script_version_node) ldvn_next;
};

struct ld_script {
	char *lds_entry_point;		/* entry point symbol */
	STAILQ_HEAD(, ld_script_phdr) lds_p; /* phdr table */
	STAILQ_HEAD(, ld_script_region_alias) lds_a; /* region aliases list */
	STAILQ_HEAD(, ld_script_region) lds_r; /* memory region list */
	STAILQ_HEAD(, ld_script_nocrossref) lds_n; /* nocrossref list */
	STAILQ_HEAD(, ld_script_version_node) lds_vn; /* version node list */
	unsigned char lds_vn_name_omitted; /* version node w/o name exists */
	struct ld_script_cmd_head lds_c; /* other ldscript cmd list */
	struct ld_script_variable *lds_v; /* variable table */
	char *lds_last_os_name;		/* last output section */
	char *lds_base_os_name;		/* current output section */
};

struct ld_script_cmd *ld_script_assert(struct ld *, struct ld_exp *, char *);
struct ld_script_assign *ld_script_assign(struct ld *, struct ld_exp *,
    enum ld_script_assign_op, struct ld_exp *, unsigned, unsigned);
void	ld_script_assign_dump(struct ld *, struct ld_script_assign *);
void	ld_script_assign_free(struct ld_script_assign *);
void	ld_script_cleanup(struct ld *);
struct ld_script_cmd *ld_script_cmd(struct ld *, enum ld_script_cmd_type,
    void *);
void	ld_script_cmd_free(struct ld_script_cmd *);
void	ld_script_cmd_insert(struct ld_script_cmd_head *,
    struct ld_script_cmd *);
void	ld_script_extern(struct ld *, struct ld_script_list *);
void	ld_script_group(struct ld *, struct ld_script_list *);
void	ld_script_init(struct ld *);
void	ld_script_input(struct ld *, struct ld_script_list *);
struct ld_script_input_file *ld_script_input_file(struct ld *, unsigned,
    void *);
struct ld_script_list *ld_script_list(struct ld *, struct ld_script_list *,
    void *);
void	ld_script_list_free(struct ld_script_list *, void (*)(void *));
struct ld_script_list *ld_script_list_reverse(struct ld_script_list *);
void	ld_script_nocrossrefs(struct ld *, struct ld_script_list *);
struct ld_script_phdr *ld_script_phdr(struct ld *, char *, char *, unsigned,
    unsigned, struct ld_exp *, unsigned);
void	ld_script_parse(const char *);
void	ld_script_parse_internal(void);
struct ld_script_region *ld_script_region(struct ld *, char *, char *,
    struct ld_exp *, struct ld_exp *);
void	ld_script_process_assign(struct ld *, struct ld_script_assign *);
void	ld_script_process_entry(struct ld *, char *);
void	ld_script_region_alias(struct ld *, char *, char *);
int64_t ld_script_variable_value(struct ld *, char *);
void	ld_script_version_add_node(struct ld *, char *, void *, char *);
struct ld_script_version_entry *ld_script_version_alloc_entry(struct ld *,
    char *, void *);
void	*ld_script_version_link_entry(struct ld *,
    struct ld_script_version_entry_head *, struct ld_script_version_entry *);
void	ld_script_version_set_lang(struct ld *,
    struct ld_script_version_entry_head *, char *);
