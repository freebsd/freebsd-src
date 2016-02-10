%{
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
#include "ld_options.h"
#include "ld_output.h"
#include "ld_script.h"
#include "ld_file.h"
#include "ld_path.h"
#include "ld_exp.h"

ELFTC_VCSID("$Id: ld_script_parser.y 3281 2015-12-11 21:39:23Z kaiwang27 $");

struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;

#ifndef	YY_BUF_SIZE
#define	YY_BUF_SIZE 16384
#endif

extern int yylex(void);
extern int yyparse(void);
extern YY_BUFFER_STATE yy_create_buffer(FILE *file, int size);
extern YY_BUFFER_STATE yy_scan_string(char *yy_str);
extern void yy_switch_to_buffer(YY_BUFFER_STATE b);
extern void yy_delete_buffer(YY_BUFFER_STATE b);
extern int lineno;
extern FILE *yyin;
extern struct ld *ld;

static void yyerror(const char *s);
static void _init_script(void);
static struct ld_script_cmd_head ldss_c, ldso_c;

%}

%token T_ABSOLUTE
%token T_ADDR
%token T_ALIGN
%token T_ALIGNOF
%token T_ASSERT
%token T_AS_NEEDED
%token T_AT
%token T_BIND
%token T_BLOCK
%token T_BYTE
%token T_CONSTANT
%token T_CONSTRUCTORS
%token T_CREATE_OBJECT_SYMBOLS
%token T_DATA_SEGMENT_ALIGN
%token T_DATA_SEGMENT_END
%token T_DATA_SEGMENT_RELRO_END
%token T_DEFINED
%token T_ENTRY
%token T_EXCLUDE_FILE
%token T_EXTERN
%token T_FILEHDR
%token T_FILL
%token T_FLAGS
%token T_FLOAT
%token T_FORCE_COMMON_ALLOCATION
%token T_GROUP
%token T_HLL
%token T_INCLUDE
%token T_INHIBIT_COMMON_ALLOCATION
%token T_INPUT
%token T_KEEP
%token T_LENGTH
%token T_LOADADDR
%token T_LONG
%token T_MAP
%token T_MAX
%token T_MEMORY
%token T_MIN
%token T_NEXT
%token T_NOCROSSREFS
%token T_NOFLOAT
%token T_OPTION
%token T_ORIGIN
%token T_OUTPUT
%token T_OUTPUT_ARCH
%token T_OUTPUT_FORMAT
%token T_PHDRS
%token T_PROVIDE
%token T_PROVIDE_HIDDEN
%token T_QUAD
%token T_REGION_ALIAS
%token T_SEARCH_DIR
%token T_SECTIONS
%token T_SEGMENT_START
%token T_SHORT
%token T_SIZEOF
%token T_SIZEOF_HEADERS
%token T_SORT_BY_ALIGNMENT
%token T_SORT_BY_NAME
%token T_SPECIAL
%token T_SQUAD
%token T_STARTUP
%token T_SUBALIGN
%token T_SYSLIB
%token T_TARGET
%token T_TRUNCATE
%token T_VER_EXTERN
%token T_VER_GLOBAL
%token T_VER_LOCAL

%token T_LSHIFT_E
%token T_RSHIFT_E
%token T_LSHIFT
%token T_RSHIFT
%token T_EQ
%token T_NE
%token T_GE
%token T_LE
%token T_ADD_E
%token T_SUB_E
%token T_MUL_E
%token T_DIV_E
%token T_AND_E
%token T_OR_E
%token T_LOGICAL_AND
%token T_LOGICAL_OR

%right '=' T_AND_E T_OR_E T_MUL_E T_DIV_E T_ADD_E T_SUB_E T_LSHIFT_E T_RSHIFT_E
%right '?' ':'
%left T_LOGICAL_OR
%left T_LOGICAL_AND
%left '|'
%left '&'
%left T_EQ T_NE T_GE T_LE '>' '<'
%left T_LSHIFT T_RSHIFT
%left '+' '-'
%left '*' '/' '%'
%left UNARY

%token <num> T_NUM
%token <str> T_COMMONPAGESIZE
%token <str> T_COPY
%token <str> T_DSECT
%token <str> T_IDENT
%token <str> T_INFO
%token <str> T_MAXPAGESIZE
%token <str> T_MEMORY_ATTR
%token <str> T_NOLOAD
%token <str> T_ONLY_IF_RO
%token <str> T_ONLY_IF_RW
%token <str> T_OVERLAY
%token <str> T_STRING
%token <str> T_WILDCARD

%type <assign> assignment
%type <assign> provide_assignment
%type <assign> provide_hidden_assignment
%type <assign> simple_assignment
%type <cmd> assert_command
%type <cmd> entry_command
%type <cmd> ldscript_command
%type <cmd> output_section_command
%type <cmd> sections_command
%type <cmd> sections_sub_command
%type <exp> expression
%type <exp> function
%type <exp> constant
%type <exp> variable
%type <exp> absolute_function
%type <exp> addr_function
%type <exp> align_function
%type <exp> alignof_function
%type <exp> block_function
%type <exp> data_segment_align_function
%type <exp> data_segment_end_function
%type <exp> data_segment_relro_end_function
%type <exp> defined_function
%type <exp> length_function
%type <exp> loadaddr_function
%type <exp> max_function
%type <exp> min_function
%type <exp> next_function
%type <exp> origin_function
%type <exp> output_section_addr
%type <exp> output_section_align
%type <exp> output_section_fillexp
%type <exp> output_section_lma
%type <exp> output_section_subalign
%type <exp> overlay_vma
%type <exp> phdr_at
%type <exp> segment_start_function
%type <exp> sizeof_function
%type <exp> sizeof_headers_function
%type <input_file> input_file
%type <input_section> input_section
%type <input_section> input_section_desc
%type <input_section> input_section_desc_no_keep
%type <list> as_needed_list
%type <list> ident_list
%type <list> ident_list_nosep
%type <list> input_file_list
%type <list> output_section_addr_and_type
%type <list> output_section_phdr
%type <list> overlay_section_list
%type <list> wildcard_list
%type <num> assign_op
%type <num> data_type
%type <num> output_section_keywords
%type <num> overlay_nocref
%type <num> phdr_filehdr
%type <num> phdr_flags
%type <num> phdr_phdrs
%type <output_data> output_section_data
%type <output_desc> output_sections_desc
%type <overlay_desc> overlay_desc
%type <overlay_section> overlay_section
%type <phdr> phdr
%type <region> memory_region
%type <str> ident
%type <str> memory_attr
%type <str> output_section_constraint
%type <str> output_section_lma_region
%type <str> output_section_region
%type <str> output_section_type
%type <str> output_section_type_keyword
%type <str> symbolic_constant
%type <str> wildcard
%type <wildcard> wildcard_sort
%type <version_entry> version_entry
%type <version_entry_head> version_block
%type <version_entry_head> version_entry_list
%type <version_entry_head> extern_block
%type <str> version_dependency

%union {
	struct ld_exp *exp;
	struct ld_script_assign *assign;
	struct ld_script_cmd *cmd;
	struct ld_script_list *list;
	struct ld_script_input_file *input_file;
	struct ld_script_phdr *phdr;
	struct ld_script_region *region;
	struct ld_script_sections_output *output_desc;
	struct ld_script_sections_output_data *output_data;
	struct ld_script_sections_output_input *input_section;
	struct ld_script_sections_overlay *overlay_desc;
	struct ld_script_sections_overlay_section *overlay_section;
	struct ld_script_version_entry *version_entry;
	struct ld_script_version_entry_head *version_entry_head;
	struct ld_wildcard *wildcard;
	char *str;
	int64_t num;
}

%%

script
	: ldscript
	|
	;

ldscript
	: ldscript_command {
		if ($1 != NULL)
			ld_script_cmd_insert(&ld->ld_scp->lds_c, $1);
	}
	| ldscript ldscript_command {
		if ($2 != NULL)
			ld_script_cmd_insert(&ld->ld_scp->lds_c, $2);
	}
	;

expression
	: expression '+' expression {
		$$ = ld_exp_binary(ld, LEOP_ADD, $1, $3);
	}
	| expression '-' expression {
		$$ = ld_exp_binary(ld, LEOP_SUBSTRACT, $1, $3);
	}
	| expression '*' expression {
		$$ = ld_exp_binary(ld, LEOP_MUL, $1, $3);
	}
	| expression '/' expression {
		$$ = ld_exp_binary(ld, LEOP_DIV, $1, $3);
	}
	| expression '%' expression {
		$$ = ld_exp_binary(ld, LEOP_MOD, $1, $3);
	}
	| expression '&' expression {
		$$ = ld_exp_binary(ld, LEOP_AND, $1, $3);
	}
	| expression '|' expression {
		$$ = ld_exp_binary(ld, LEOP_OR, $1, $3);
	}
	| expression '>' expression {
		$$ = ld_exp_binary(ld, LEOP_GREATER, $1, $3);
	}
	| expression '<' expression {
		$$ = ld_exp_binary(ld, LEOP_LESSER, $1, $3);
	}
	| expression T_EQ expression {
		$$ = ld_exp_binary(ld, LEOP_EQUAL, $1, $3);
	}
	| expression T_NE expression {
		$$ = ld_exp_binary(ld, LEOP_NE, $1, $3);
	}
	| expression T_GE expression {
		$$ = ld_exp_binary(ld, LEOP_GE, $1, $3);
	}
	| expression T_LE expression {
		$$ = ld_exp_binary(ld, LEOP_LE, $1, $3);
	}
	| expression T_LSHIFT expression {
		$$ = ld_exp_binary(ld, LEOP_LSHIFT, $1, $3);
	}
	| expression T_RSHIFT expression {
		$$ = ld_exp_binary(ld, LEOP_RSHIFT, $1, $3);
	}
	| expression T_LOGICAL_AND expression {
		$$ = ld_exp_binary(ld, LEOP_LOGICAL_AND, $1, $3);
	}
	| expression T_LOGICAL_OR expression {
		$$ = ld_exp_binary(ld, LEOP_LOGICAL_OR, $1, $3);
	}
	| '!' expression %prec UNARY {
		$$ = ld_exp_unary(ld, LEOP_NOT, $2);
	}
	| '-' expression %prec UNARY {
		$$ = ld_exp_unary(ld, LEOP_MINUS, $2);
	}
	| '~' expression %prec UNARY {
		$$ = ld_exp_unary(ld, LEOP_NEGATION, $2);
	}
	| expression '?' expression ':' expression {
		$$ = ld_exp_trinary(ld, $1, $3, $5);
	}
	| simple_assignment {
		$$ = ld_exp_assign(ld, $1);
	}
	| function
	| constant
	| variable
	| '(' expression ')' { $$ = $2;	$$->le_par = 1; }
	;

function
	: absolute_function
	| addr_function
	| align_function
	| alignof_function
	| block_function
	| data_segment_align_function
	| data_segment_end_function
	| data_segment_relro_end_function
	| defined_function
	| length_function
	| loadaddr_function
	| max_function
	| min_function
	| next_function
	| origin_function
	| segment_start_function
	| sizeof_function
	| sizeof_headers_function
	;

absolute_function
	: T_ABSOLUTE '(' expression ')' {
		$$ = ld_exp_unary(ld, LEOP_ABS, $3);
	}
	;

addr_function
	: T_ADDR '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_ADDR, ld_exp_name(ld, $3));
	}
	;

align_function
	: T_ALIGN '(' expression ')' {
		$$ = ld_exp_unary(ld, LEOP_ALIGN, $3);
	}
	| T_ALIGN '(' expression ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_ALIGN, $3, $5);
	}
	;

alignof_function
	: T_ALIGNOF '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_ALIGNOF, ld_exp_name(ld, $3));
	}
	;

block_function
	: T_BLOCK '(' expression ')' {
		$$ = ld_exp_unary(ld, LEOP_BLOCK, $3);
	}
	;

data_segment_align_function
	: T_DATA_SEGMENT_ALIGN '(' expression ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_DSA, $3, $5);
	}
	;

data_segment_end_function
	: T_DATA_SEGMENT_END '(' expression ')' {
		$$ = ld_exp_unary(ld, LEOP_DSE, $3);
	}
	;

data_segment_relro_end_function
	: T_DATA_SEGMENT_RELRO_END '(' expression ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_DSRE, $3, $5);
	}
	;

defined_function
	: T_DEFINED '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_DEFINED, ld_exp_symbol(ld, $3));
	}
	;

length_function
	: T_LENGTH '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_LENGTH, ld_exp_name(ld, $3));
	}
	;

loadaddr_function
	: T_LOADADDR '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_LOADADDR, ld_exp_name(ld, $3));
	}
	;

max_function
	: T_MAX '(' expression ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_MAX, $3, $5);
	}
	;

min_function
	: T_MIN '(' expression ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_MIN, $3, $5);
	}
	;

next_function
	: T_NEXT '(' expression ')' {
		$$ = ld_exp_unary(ld, LEOP_NEXT, $3);
	}
	;

origin_function
	: T_ORIGIN '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_ORIGIN, ld_exp_name(ld, $3));
	}
	;

segment_start_function
	: T_SEGMENT_START '(' ident ',' expression ')' {
		$$ = ld_exp_binary(ld, LEOP_MIN, ld_exp_name(ld, $3), $5);
	}
	;

sizeof_function
	: T_SIZEOF '(' ident ')' {
		$$ = ld_exp_unary(ld, LEOP_SIZEOF, ld_exp_name(ld, $3));
	}
	;

sizeof_headers_function
	: T_SIZEOF_HEADERS {
		$$ = ld_exp_sizeof_headers(ld);
	}
	;

constant
	: T_NUM {
		$$ = ld_exp_constant(ld, $1);
	}
	| symbolic_constant {
		$$ = ld_exp_symbolic_constant(ld, $1);
	}
	;

symbolic_constant
	: T_CONSTANT '(' T_COMMONPAGESIZE ')' { $$ = $3; }
	| T_CONSTANT '(' T_MAXPAGESIZE ')' { $$ = $3; }
	;

ldscript_command
	: assert_command
	| assignment {
		if (*$1->lda_var->le_name == '.')
			ld_fatal(ld, "variable . can only be used inside"
			    " SECTIONS command");
		$$ = ld_script_cmd(ld, LSC_ASSIGN, $1);
	}
	| entry_command
	| extern_command { $$ = NULL; }
	| force_common_allocation_command { $$ = NULL; }
	| group_command { $$ = NULL; }
	| inhibit_common_allocation_command { $$ = NULL; }
	| input_command { $$ = NULL; }
	| memory_command { $$ = NULL; }
	| nocrossrefs_command { $$ = NULL; }
	| output_command { $$ = NULL; }
	| output_arch_command { $$ = NULL; }
	| output_format_command { $$ = NULL; }
	| phdrs_command { $$ = NULL; }
	| region_alias_command { $$ = NULL; }
	| search_dir_command { $$ = NULL; }
	| sections_command
	| startup_command { $$ = NULL; }
	| target_command { $$ = NULL; }
	| version_script_node { $$ = NULL; }
	| ';' { $$ = NULL; }
	;

assignment
	: simple_assignment
	| provide_assignment
	| provide_hidden_assignment
	;

simple_assignment
	: variable assign_op expression %prec '=' {
		$$ = ld_script_assign(ld, $1, $2, $3, 0, 0);
	}
	;

provide_assignment
	: T_PROVIDE '(' variable '=' expression ')' {
		$$ = ld_script_assign(ld, $3, LSAOP_E, $5, 1, 0);
	}
	;

provide_hidden_assignment
	: T_PROVIDE_HIDDEN '(' variable '=' expression ')' {
		$$ = ld_script_assign(ld, $3, LSAOP_E, $5, 1, 1);
	}
	;

assign_op
	: T_LSHIFT_E { $$ = LSAOP_LSHIFT_E; }
	| T_RSHIFT_E { $$ = LSAOP_RSHIFT_E; }
	| T_ADD_E { $$ = LSAOP_ADD_E; }
	| T_SUB_E { $$ = LSAOP_SUB_E; }
	| T_MUL_E { $$ = LSAOP_MUL_E; }
	| T_DIV_E { $$ = LSAOP_DIV_E; }
	| T_AND_E { $$ = LSAOP_AND_E; }
	| T_OR_E { $$ = LSAOP_OR_E; }
	| '=' { $$ = LSAOP_E; }
	;

assert_command
	: T_ASSERT '(' expression ',' T_STRING ')' {
		$$ = ld_script_assert(ld, $3, $5);
	}
	;

entry_command
	: T_ENTRY '(' ident ')' {
		$$ = ld_script_cmd(ld, LSC_ENTRY, $3);
	}
	;

extern_command
	: T_EXTERN '(' ident_list_nosep ')' { ld_script_extern(ld, $3); }
	;

force_common_allocation_command
	: T_FORCE_COMMON_ALLOCATION { ld->ld_common_alloc = 1; }
	;

group_command
	: T_GROUP '(' input_file_list ')' {
		 ld_script_group(ld, ld_script_list_reverse($3));
	}
	;

inhibit_common_allocation_command
	: T_INHIBIT_COMMON_ALLOCATION { ld->ld_common_no_alloc = 1; }
	;

input_command
	: T_INPUT '(' input_file_list ')' {
		ld_script_input(ld, ld_script_list_reverse($3));
	}
	;

memory_command
	: T_MEMORY '{' memory_region_list '}'
	;

memory_region_list
	: memory_region {
		STAILQ_INSERT_TAIL(&ld->ld_scp->lds_r, $1, ldsr_next);
	}
	| memory_region_list memory_region {
		STAILQ_INSERT_TAIL(&ld->ld_scp->lds_r, $2, ldsr_next);
	}
	;

memory_region
	: ident memory_attr ':' T_ORIGIN '=' expression ',' T_LENGTH '='
	expression {
		ld_script_region(ld, $1, $2, $6, $10);
	}
	;

memory_attr
	: T_MEMORY_ATTR
	| { $$ = NULL; }
	;

nocrossrefs_command
	: T_NOCROSSREFS '(' ident_list_nosep ')' {
		ld_script_nocrossrefs(ld, $3);
	}
	;

output_command
	: T_OUTPUT '(' ident ')' {
		if (ld->ld_output == NULL)
			ld->ld_output_file = $3;
		else
			free($3);
	}
	;

output_arch_command
	: T_OUTPUT_ARCH '(' ident ')' {
		ld_arch_set(ld, $3);
		free($3);
	}
	;

output_format_command
	: T_OUTPUT_FORMAT '(' ident ')' {
		ld_output_format(ld, $3, $3, $3);
	}
	| T_OUTPUT_FORMAT '(' ident ',' ident ',' ident ')' {
		ld_output_format(ld, $3, $5, $7);
	}
	;

phdrs_command
	: T_PHDRS '{' phdr_list '}'
	;

phdr_list
	: phdr {
		STAILQ_INSERT_TAIL(&ld->ld_scp->lds_p, $1, ldsp_next);
	}
	| phdr_list phdr {
		STAILQ_INSERT_TAIL(&ld->ld_scp->lds_p, $2, ldsp_next);
	}

phdr
	: ident ident phdr_filehdr phdr_phdrs phdr_at phdr_flags ';' {
		$$ = ld_script_phdr(ld, $1, $2, $3, $4, $5, $6);
	}
	;

phdr_filehdr
	: T_FILEHDR { $$ = 1; }
	| { $$ = 0; }
	;

phdr_phdrs
	: T_PHDRS { $$ = 1; }
	| { $$ = 0; }
	;

phdr_at
	: T_AT '(' expression ')' { $$ = $3; }
	| { $$ = NULL; }
	;

phdr_flags
	: T_FLAGS '(' T_NUM ')' { $$ = $3; }
	| { $$ = 0; }
	;

region_alias_command
	: T_REGION_ALIAS '(' ident ',' ident ')' {
		ld_script_region_alias(ld, $3, $5);
	}
	;

search_dir_command
	: T_SEARCH_DIR '(' ident ')' {
		ld_path_add(ld, $3, LPT_L);
		free($3);
	}
	;

sections_command
	: T_SECTIONS '{' sections_command_list '}' {
		struct ld_script_sections *ldss;
		ldss = malloc(sizeof(struct ld_script_sections));
		if (ldss == NULL)
			ld_fatal_std(ld, "malloc");
		memcpy(&ldss->ldss_c, &ldss_c, sizeof(ldss_c));
		$$ = ld_script_cmd(ld, LSC_SECTIONS, ldss);
		STAILQ_INIT(&ldss_c);
	}
	;

sections_command_list
	: sections_sub_command {
		if ($1 != NULL)
			ld_script_cmd_insert(&ldss_c, $1);
	}
	| sections_command_list sections_sub_command {
		if ($2 != NULL)
			ld_script_cmd_insert(&ldss_c, $2);
	}
	;

sections_sub_command
	: entry_command
	| assignment {
		$$ = ld_script_cmd(ld, LSC_ASSIGN, $1);
	}
	| output_sections_desc {
		$$ = ld_script_cmd(ld, LSC_SECTIONS_OUTPUT, $1);
	}
	| overlay_desc {
		$$ = ld_script_cmd(ld, LSC_SECTIONS_OVERLAY, $1);
	}
	| ';' { $$ = NULL; }
	;

output_sections_desc
	: ident output_section_addr_and_type ':' {
		/* Remember the name of last output section, needed later for assignment. */
		ld->ld_scp->lds_base_os_name = $1;
	}
	output_section_lma
	output_section_align
	output_section_subalign
	output_section_constraint
	'{' output_section_command_list '}'
	output_section_region
	output_section_lma_region
	output_section_phdr
	output_section_fillexp {
		$$ = calloc(1, sizeof(struct ld_script_sections_output));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldso_name = $1;
		$$->ldso_vma = $2->ldl_entry;
		$$->ldso_type = $2->ldl_next->ldl_entry;
		$$->ldso_lma = $5;
		$$->ldso_align = $6;
		$$->ldso_subalign = $7;
		$$->ldso_constraint = $8;
		memcpy(&$$->ldso_c, &ldso_c, sizeof(ldso_c));
		$$->ldso_region = $12;
		$$->ldso_lma_region = $13;
		$$->ldso_phdr = ld_script_list_reverse($14);
		$$->ldso_fill = $15;
		STAILQ_INIT(&ldso_c);
		ld->ld_scp->lds_base_os_name = 0;
		ld->ld_scp->lds_last_os_name = $1;
	}
	;

output_section_addr_and_type
	: output_section_addr output_section_type {
		$$ = ld_script_list(ld, NULL, $2);
		$$ = ld_script_list(ld, $$, $1);
	}
	| output_section_type {
		$$ = ld_script_list(ld, NULL, NULL);
		$$ = ld_script_list(ld, $$, $1);
	}
	;

output_section_addr
	: expression
	;

output_section_type
	: '(' output_section_type_keyword ')' { $$ = $2; }
	| '(' ')' { $$ = NULL; }
	| { $$ = NULL; }
	;

output_section_type_keyword
	: T_COPY
	| T_DSECT
	| T_INFO
	| T_NOLOAD
	| T_OVERLAY
	;

output_section_lma
	: T_AT '(' expression ')' { $$ = $3; }
	| { $$ = NULL; }
	;

output_section_align
	: T_ALIGN '(' expression ')' { $$ = $3; }
	| { $$ = NULL; }
	;

output_section_subalign
	: T_SUBALIGN '(' expression ')' { $$ = $3; }
	| { $$ = NULL; }
	;

output_section_constraint
	: T_ONLY_IF_RO
	| T_ONLY_IF_RW
	| { $$ = NULL; }
	;

output_section_command_list
	: output_section_command {
		if ($1 != NULL)
			ld_script_cmd_insert(&ldso_c, $1);
	}
	| output_section_command_list output_section_command {
		if ($2 != NULL)
			ld_script_cmd_insert(&ldso_c, $2);
	}
	;

output_section_command
	: assignment {
		$$ = ld_script_cmd(ld, LSC_ASSIGN, $1);
	}
	| input_section_desc {
		$$ = ld_script_cmd(ld, LSC_SECTIONS_OUTPUT_INPUT, $1);
	}
	| output_section_data {
		$$ = ld_script_cmd(ld, LSC_SECTIONS_OUTPUT_DATA, $1);
	}
	| output_section_keywords {
		$$ = ld_script_cmd(ld, LSC_SECTIONS_OUTPUT_KEYWORD,
		    (void *) (uintptr_t) $1);
	}
	| ';' { $$ = NULL; }
	;

input_section_desc
	: input_section_desc_no_keep {
		$1->ldoi_keep = 0;
		$$ = $1;
	}
	| T_KEEP '(' input_section_desc_no_keep ')' {
		$3->ldoi_keep = 0;
		$$ = $3;
	}
	;

input_section_desc_no_keep
	: wildcard_sort input_section {
		$2->ldoi_ar = NULL;
		$2->ldoi_file = $1;
		$$ = $2;
	}
	| wildcard_sort ':' wildcard_sort input_section {
		$4->ldoi_ar = $1;
		$4->ldoi_ar = $3;
		$$ = $4;
	}
	;

input_section
	: '(' T_EXCLUDE_FILE '(' wildcard_list ')' wildcard_list ')' {
		$$ = calloc(1, sizeof(struct ld_script_sections_output_input));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldoi_exclude = ld_script_list_reverse($4);
		$$->ldoi_sec = ld_script_list_reverse($6);
	}
	| '(' wildcard_list ')' {
		$$ = calloc(1, sizeof(struct ld_script_sections_output_input));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldoi_exclude = NULL;
		$$->ldoi_sec = ld_script_list_reverse($2);
	}
	;

output_section_data
	: data_type '(' expression ')' {
		$$ = calloc(1, sizeof(struct ld_script_sections_output_data));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldod_type = $1;
		$$->ldod_exp = $3;
	}
	;

data_type
	: T_BYTE { $$ = LSODT_BYTE; }
	| T_SHORT { $$ = LSODT_SHORT; }
	| T_LONG { $$ = LSODT_LONG; }
	| T_QUAD { $$ = LSODT_QUAD; }
	| T_SQUAD { $$ = LSODT_SQUAD; }
	| T_FILL { $$ = LSODT_FILL; }
	;

output_section_keywords
	: T_CREATE_OBJECT_SYMBOLS {
		$$ = LSOK_CREATE_OBJECT_SYMBOLS;
	}
	| T_CONSTRUCTORS {
		$$ = LSOK_CONSTRUCTORS;
	}
	| T_SORT_BY_NAME '(' T_CONSTRUCTORS ')' {
		$$ = LSOK_CONSTRUCTORS_SORT_BY_NAME;
	}
	;

output_section_region
	: '>' ident { $$ = $2; }
	| { $$ = NULL; }
	;

output_section_lma_region
	: T_AT '>' ident { $$ = $3; }
	| { $$ = NULL; }
	;

output_section_phdr
	: output_section_phdr ':' ident {
		$$ = ld_script_list(ld, $$, $3);
	}
	| { $$ = NULL; }
	;


output_section_fillexp
	: '=' expression { $$ = $2; }
	| { $$ = NULL; }
	;

overlay_desc
	: T_OVERLAY
	overlay_vma ':'
	overlay_nocref
	output_section_lma
	'{' overlay_section_list '}'
	output_section_region
	output_section_phdr
	output_section_fillexp {
		$$ = calloc(1, sizeof(struct ld_script_sections_overlay));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldso_vma = $2;
		$$->ldso_nocrossref = !!$4;
		$$->ldso_lma = $5;
		$$->ldso_s = $7;
		$$->ldso_region = $9;
		$$->ldso_phdr = $10;
		$$->ldso_fill = $11;
	}
	;

overlay_vma
	: expression
	| { $$ = NULL; }
	;

overlay_nocref
	: T_NOCROSSREFS { $$ = 1; }
	| { $$ = 0; }
	;

overlay_section_list
	: overlay_section {
		$$ = ld_script_list(ld, NULL, $1);
	}
	| overlay_section_list overlay_section {
		$$ = ld_script_list(ld, $1, $2);
	}
	;

overlay_section
	: ident
	'{' output_section_command_list '}'
	output_section_phdr
	output_section_fillexp {
		$$ = calloc(1,
		    sizeof(struct ld_script_sections_overlay_section));
		if ($$ == NULL)
			ld_fatal_std(ld, "calloc");
		$$->ldos_name = $1;
		memcpy(&$$->ldos_c, &ldso_c, sizeof(ldso_c));
		$$->ldos_phdr = $5;
		$$->ldos_fill = $6;
		STAILQ_INIT(&ldso_c);
	}
	;

startup_command
	: T_STARTUP '(' ident ')' {
		ld_file_add_first(ld, $3, LFT_UNKNOWN);
		free($3);
	}
	;

target_command
	: T_TARGET '(' ident ')'
	;

version_script_node
	: ident extern_block version_dependency ';' {
		ld_script_version_add_node(ld, $1, $2, $3);
	}
	| ident version_block version_dependency ';' {
		ld_script_version_add_node(ld, $1, $2, $3);
	}
	| extern_block version_dependency ';' {
		ld_script_version_add_node(ld, NULL, $1, $2);
	}
	| version_block version_dependency ';' {
		ld_script_version_add_node(ld, NULL, $1, $2);
	}
	;

extern_block
	: T_VER_EXTERN T_STRING version_block {
		ld_script_version_set_lang(ld, $3, $2);
		$$ = $3;
	}
	;

version_block
	: '{' version_entry_list '}' {
		$$ = $2;
		ld->ld_state.ls_version_local = 0;
	}
	;

version_entry_list
	: version_entry {
		$$ = ld_script_version_link_entry(ld, NULL, $1);
	}
	| version_entry_list version_entry {
		$$ = ld_script_version_link_entry(ld, $1, $2);
	}
	;

version_entry
	: T_VER_GLOBAL {
		ld->ld_state.ls_version_local = 0;
		$$ = NULL;
	}
	| T_VER_LOCAL {
		ld->ld_state.ls_version_local = 1;
		$$ = NULL;
	}
	| wildcard ';' {
		$$ = ld_script_version_alloc_entry(ld, $1, NULL);
	}
	| extern_block ';' {
		$$ = ld_script_version_alloc_entry(ld, NULL, $1);
	}
	;

version_dependency
	: ident
	| { $$ = NULL; }
	;

ident
	: T_IDENT
	| T_STRING
	;

variable
	: ident { $$ = ld_exp_symbol(ld, $1); }
	| '.'  { $$ = ld_exp_symbol(ld, "."); }
	;

wildcard
	: ident
	| T_WILDCARD
	| '*' { $$ = strdup("*"); }
	| '?' { $$ = strdup("?"); }
	;

wildcard_sort
	: wildcard {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $1;
		$$->lw_sort = LWS_NONE;
	}
	| T_SORT_BY_NAME '(' wildcard ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $3;
		$$->lw_sort = LWS_NAME;
	}
	| T_SORT_BY_NAME '(' T_SORT_BY_NAME '(' wildcard ')' ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $5;
		$$->lw_sort = LWS_NAME;
	}
	| T_SORT_BY_NAME '(' T_SORT_BY_ALIGNMENT '(' wildcard ')' ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $5;
		$$->lw_sort = LWS_NAME_ALIGN;
	}
	| T_SORT_BY_ALIGNMENT '(' wildcard ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $3;
		$$->lw_sort = LWS_ALIGN;
	}
	| T_SORT_BY_ALIGNMENT '(' T_SORT_BY_NAME '(' wildcard ')' ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $5;
		$$->lw_sort = LWS_ALIGN_NAME;
	}
	| T_SORT_BY_ALIGNMENT '(' T_SORT_BY_ALIGNMENT '(' wildcard ')' ')' {
		$$ = ld_wildcard_alloc(ld);
		$$->lw_name = $5;
		$$->lw_sort = LWS_ALIGN;
	}
	;

ident_list
	: ident { $$ = ld_script_list(ld, NULL, $1); }
	| ident_list separator ident { $$ = ld_script_list(ld, $1, $3); }
	;

ident_list_nosep
	: ident { $$ = ld_script_list(ld, NULL, $1); }
	| ident_list_nosep ident { $$ = ld_script_list(ld, $1, $2); }
	;

input_file_list
	: input_file { $$ = ld_script_list(ld, NULL, $1); }
	| input_file_list separator input_file { $$ = ld_script_list(ld, $1, $3); }
	;

input_file
	: ident { $$ = ld_script_input_file(ld, 0, $1); }
	| as_needed_list { $$ = ld_script_input_file(ld, 1, $1); }
	;

as_needed_list
	: T_AS_NEEDED '(' ident_list ')' { $$ = $3; }
	;

wildcard_list
	: wildcard_sort { $$ = ld_script_list(ld, NULL, $1); }
	| wildcard_list wildcard_sort { $$ = ld_script_list(ld, $1, $2); }
	;

separator
	: ','
	|
	;

%%

/* ARGSUSED */
static void
yyerror(const char *s)
{

	(void) s;
	errx(1, "Syntax error in ld script, line %d\n", lineno);
}

static void
_init_script(void)
{

	STAILQ_INIT(&ldss_c);
	STAILQ_INIT(&ldso_c);
}

void
ld_script_parse(const char *name)
{
	YY_BUFFER_STATE b;

	_init_script();

	if ((yyin = fopen(name, "r")) == NULL)
		ld_fatal_std(ld, "fopen %s name failed", name);
	b = yy_create_buffer(yyin, YY_BUF_SIZE);
	yy_switch_to_buffer(b);
	if (yyparse() < 0)
		ld_fatal(ld, "unable to parse linker script %s", name);
	yy_delete_buffer(b);
}

void
ld_script_parse_internal(void)
{
	YY_BUFFER_STATE b;

	_init_script();

	assert(ld->ld_arch != NULL && ld->ld_arch->script != NULL);
	b = yy_scan_string(ld->ld_arch->script);
	yy_switch_to_buffer(b);
	if (yyparse() < 0)
		ld_fatal(ld, "unable to parse internal linker script");
	yy_delete_buffer(b);
}
