/* A YACC grammar to parse a superset of the AT&T linker scripting language.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support (steve@cygnus.com).

This file is part of GNU ld.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

%{
/*

 */

#define DONTDECLARE_MALLOC

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "ld.h"    
#include "ldexp.h"
#include "ldver.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"
#include "ldmisc.h"
#include "ldmain.h"
#include "mri.h"
#include "ldctor.h"
#include "ldlex.h"

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

static enum section_type sectype;

lang_memory_region_type *region;

struct wildcard_spec current_file;
boolean ldgram_want_filename = true;
boolean had_script = false;
boolean force_make_executable = false;

boolean ldgram_in_script = false;
boolean ldgram_had_equals = false;
boolean ldgram_had_keep = false;
char *ldgram_vers_current_lang = NULL;

#define ERROR_NAME_MAX 20
static char *error_names[ERROR_NAME_MAX];
static int error_index;
#define PUSH_ERROR(x) if (error_index < ERROR_NAME_MAX) error_names[error_index] = x; error_index++;
#define POP_ERROR()   error_index--;
%}
%union {
  bfd_vma integer;
  char *name;
  const char *cname;
  struct wildcard_spec wildcard;
  struct name_list *name_list;
  int token;
  union etree_union *etree;
  struct phdr_info
    {
      boolean filehdr;
      boolean phdrs;
      union etree_union *at;
      union etree_union *flags;
    } phdr;
  struct lang_nocrossref *nocrossref;
  struct lang_output_section_phdr_list *section_phdr;
  struct bfd_elf_version_deps *deflist;
  struct bfd_elf_version_expr *versyms;
  struct bfd_elf_version_tree *versnode;
}

%type <etree> exp opt_exp_with_type mustbe_exp opt_at phdr_type phdr_val
%type <etree> opt_exp_without_type
%type <integer> fill_opt
%type <name_list> exclude_name_list
%type <name> memspec_opt casesymlist
%type <name> memspec_at_opt
%type <cname> wildcard_name
%type <wildcard> wildcard_spec
%token <integer> INT  
%token <name> NAME LNAME
%type <integer> length
%type <phdr> phdr_qualifiers
%type <nocrossref> nocrossref_list
%type <section_phdr> phdr_opt
%type <integer> opt_nocrossrefs

%right <token> PLUSEQ MINUSEQ MULTEQ DIVEQ  '=' LSHIFTEQ RSHIFTEQ   ANDEQ OREQ 
%right <token> '?' ':'
%left <token> OROR
%left <token>  ANDAND
%left <token> '|'
%left <token>  '^'
%left  <token> '&'
%left <token>  EQ NE
%left  <token> '<' '>' LE GE
%left  <token> LSHIFT RSHIFT

%left  <token> '+' '-'
%left  <token> '*' '/' '%'

%right UNARY
%token END 
%left <token> '('
%token <token> ALIGN_K BLOCK BIND QUAD SQUAD LONG SHORT BYTE
%token SECTIONS PHDRS SORT
%token '{' '}'
%token SIZEOF_HEADERS OUTPUT_FORMAT FORCE_COMMON_ALLOCATION OUTPUT_ARCH
%token SIZEOF_HEADERS
%token INCLUDE
%token MEMORY DEFSYMEND
%token NOLOAD DSECT COPY INFO OVERLAY
%token NAME LNAME DEFINED TARGET_K SEARCH_DIR MAP ENTRY
%token <integer> NEXT
%token SIZEOF ADDR LOADADDR MAX_K MIN_K
%token STARTUP HLL SYSLIB FLOAT NOFLOAT NOCROSSREFS
%token ORIGIN FILL
%token LENGTH CREATE_OBJECT_SYMBOLS INPUT GROUP OUTPUT CONSTRUCTORS
%token ALIGNMOD AT PROVIDE
%type <token> assign_op atype attributes_opt
%type <name>  filename
%token CHIP LIST SECT ABSOLUTE  LOAD NEWLINE ENDWORD ORDER NAMEWORD ASSERT_K
%token FORMAT PUBLIC DEFSYMEND BASE ALIAS TRUNCATE REL
%token INPUT_SCRIPT INPUT_MRI_SCRIPT INPUT_DEFSYM CASE EXTERN START
%token <name> VERS_TAG VERS_IDENTIFIER
%token GLOBAL LOCAL VERSIONK INPUT_VERSION_SCRIPT
%token KEEP
%token EXCLUDE_FILE
%type <versyms> vers_defns
%type <versnode> vers_tag
%type <deflist> verdep

%%

file:	
		INPUT_SCRIPT script_file
	|	INPUT_MRI_SCRIPT mri_script_file
	|	INPUT_VERSION_SCRIPT version_script_file
	|	INPUT_DEFSYM defsym_expr
	;


filename:  NAME;


defsym_expr:
		{ ldlex_defsym(); }
		NAME '=' exp
		{
		  ldlex_popstate();
		  lang_add_assignment(exp_assop($3,$2,$4));
		}

/* SYNTAX WITHIN AN MRI SCRIPT FILE */  
mri_script_file:
		{
		  ldlex_mri_script ();
		  PUSH_ERROR (_("MRI style script"));
		}
	     mri_script_lines
		{
		  ldlex_popstate ();
		  mri_draw_tree ();
		  POP_ERROR ();
		}
	;

mri_script_lines:
		mri_script_lines mri_script_command NEWLINE
          |
	;

mri_script_command:
		CHIP  exp 
	|	CHIP  exp ',' exp
	|	NAME 	{
			einfo(_("%P%F: unrecognised keyword in MRI style script '%s'\n"),$1);
			}
	|	LIST  	{
			config.map_filename = "-";
			}
        |       ORDER ordernamelist
	|       ENDWORD 
        |       PUBLIC NAME '=' exp
 			{ mri_public($2, $4); }
        |       PUBLIC NAME ',' exp
 			{ mri_public($2, $4); }
        |       PUBLIC NAME  exp 
 			{ mri_public($2, $3); }
	| 	FORMAT NAME
			{ mri_format($2); }
	|	SECT NAME ',' exp
			{ mri_output_section($2, $4);}
	|	SECT NAME  exp
			{ mri_output_section($2, $3);}
	|	SECT NAME '=' exp
			{ mri_output_section($2, $4);}
	|	ALIGN_K NAME '=' exp
			{ mri_align($2,$4); }
	|	ALIGN_K NAME ',' exp
			{ mri_align($2,$4); }
	|	ALIGNMOD NAME '=' exp
			{ mri_alignmod($2,$4); }
	|	ALIGNMOD NAME ',' exp
			{ mri_alignmod($2,$4); }
	|	ABSOLUTE mri_abs_name_list
	|	LOAD	 mri_load_name_list
	|       NAMEWORD NAME 
			{ mri_name($2); }   
	|	ALIAS NAME ',' NAME
			{ mri_alias($2,$4,0);}
	|	ALIAS NAME ',' INT
			{ mri_alias($2,0,(int) $4);}
	|	BASE     exp
			{ mri_base($2); }
        |       TRUNCATE INT
		{  mri_truncate((unsigned int) $2); }
	|	CASE casesymlist
	|	EXTERN extern_name_list
	|	INCLUDE filename
		{ ldfile_open_command_file ($2); } mri_script_lines END
	|	START NAME
		{ lang_add_entry ($2, false); }
        |
	;

ordernamelist:
	      ordernamelist ',' NAME         { mri_order($3); }
	|     ordernamelist  NAME         { mri_order($2); }
      	|
	;

mri_load_name_list:
		NAME
			{ mri_load($1); }
	|	mri_load_name_list ',' NAME { mri_load($3); }
	;

mri_abs_name_list:
 		NAME
 			{ mri_only_load($1); }
	|	mri_abs_name_list ','  NAME
 			{ mri_only_load($3); }
	;

casesymlist:
	  /* empty */ { $$ = NULL; }
	| NAME
	| casesymlist ',' NAME
	;

extern_name_list:
	  NAME
			{ ldlang_add_undef ($1); }
	| extern_name_list NAME
			{ ldlang_add_undef ($2); }
	| extern_name_list ',' NAME
			{ ldlang_add_undef ($3); }
	;

script_file:
	{
	 ldlex_both();
	}
       ifile_list
	{
	ldlex_popstate();
	}
        ;


ifile_list:
       ifile_list ifile_p1
        |
	;



ifile_p1:
		memory
	|	sections
	|	phdrs
	|	startup
	|	high_level_library
	|	low_level_library
	|	floating_point_support
	|	statement_anywhere
	|	version
        |	 ';'
	|	TARGET_K '(' NAME ')'
		{ lang_add_target($3); }
	|	SEARCH_DIR '(' filename ')'
		{ ldfile_add_library_path ($3, false); }
	|	OUTPUT '(' filename ')'
		{ lang_add_output($3, 1); }
        |	OUTPUT_FORMAT '(' NAME ')'
		  { lang_add_output_format ($3, (char *) NULL,
					    (char *) NULL, 1); }
	|	OUTPUT_FORMAT '(' NAME ',' NAME ',' NAME ')'
		  { lang_add_output_format ($3, $5, $7, 1); }
        |	OUTPUT_ARCH '(' NAME ')'
		  { ldfile_set_output_arch($3); }
	|	FORCE_COMMON_ALLOCATION
		{ command_line.force_common_definition = true ; }
	|	INPUT '(' input_list ')'
	|	GROUP
		  { lang_enter_group (); }
		    '(' input_list ')'
		  { lang_leave_group (); }
     	|	MAP '(' filename ')'
		{ lang_add_map($3); }
	|	INCLUDE filename 
		{ ldfile_open_command_file($2); } ifile_list END
	|	NOCROSSREFS '(' nocrossref_list ')'
		{
		  lang_add_nocrossref ($3);
		}
	|	EXTERN '(' extern_name_list ')'
	;

input_list:
		NAME
		{ lang_add_input_file($1,lang_input_file_is_search_file_enum,
				 (char *)NULL); }
	|	input_list ',' NAME
		{ lang_add_input_file($3,lang_input_file_is_search_file_enum,
				 (char *)NULL); }
	|	input_list NAME
		{ lang_add_input_file($2,lang_input_file_is_search_file_enum,
				 (char *)NULL); }
	|	LNAME
		{ lang_add_input_file($1,lang_input_file_is_l_enum,
				 (char *)NULL); }
	|	input_list ',' LNAME
		{ lang_add_input_file($3,lang_input_file_is_l_enum,
				 (char *)NULL); }
	|	input_list LNAME
		{ lang_add_input_file($2,lang_input_file_is_l_enum,
				 (char *)NULL); }
	;

sections:
		SECTIONS '{' sec_or_group_p1 '}'
	;

sec_or_group_p1:
		sec_or_group_p1 section
	|	sec_or_group_p1 statement_anywhere
	|
	;

statement_anywhere:
		ENTRY '(' NAME ')'
		{ lang_add_entry ($3, false); }
	|	assignment end
	;

/* The '*' and '?' cases are there because the lexer returns them as
   separate tokens rather than as NAME.  */
wildcard_name:
		NAME
			{
			  $$ = $1;
			}
	|	'*'
			{
			  $$ = "*";
			}
	|	'?'
			{
			  $$ = "?";
			}
	;

wildcard_spec:
		wildcard_name
			{
			  $$.name = $1;
			  $$.sorted = false;
			  $$.exclude_name_list = NULL;
			}
	| 	EXCLUDE_FILE '(' exclude_name_list ')' wildcard_name
			{
			  $$.name = $5;
			  $$.sorted = false;
			  $$.exclude_name_list = $3;
			}
	|	SORT '(' wildcard_name ')'
			{
			  $$.name = $3;
			  $$.sorted = true;
			  $$.exclude_name_list = NULL;
			}
	|	SORT '(' EXCLUDE_FILE '(' exclude_name_list ')' wildcard_name ')'
			{
			  $$.name = $7;
			  $$.sorted = true;
			  $$.exclude_name_list = $5;
			}
	;



exclude_name_list:
		exclude_name_list wildcard_name
			{
			  struct name_list *tmp;
			  tmp = (struct name_list *) xmalloc (sizeof *tmp);
			  tmp->name = $2;
			  tmp->next = $1;
			  $$ = tmp;	
			}
	|
		wildcard_name
			{
			  struct name_list *tmp;
			  tmp = (struct name_list *) xmalloc (sizeof *tmp);
			  tmp->name = $1;
			  tmp->next = NULL;
			  $$ = tmp;
			}
	;

file_NAME_list:
		wildcard_spec
			{
			  lang_add_wild ($1.name, $1.sorted,
					 current_file.name,
					 current_file.sorted,
					 ldgram_had_keep, $1.exclude_name_list);
			}
	|	file_NAME_list opt_comma wildcard_spec
			{
			  lang_add_wild ($3.name, $3.sorted,
					 current_file.name,
					 current_file.sorted,
					 ldgram_had_keep, $3.exclude_name_list);
			}
	;

input_section_spec_no_keep:
		NAME
			{
			  lang_add_wild (NULL, false, $1, false,
					 ldgram_had_keep, NULL);
			}
        |	'['
			{
			  current_file.name = NULL;
			  current_file.sorted = false;
			}
		file_NAME_list ']'
	|	wildcard_spec
			{
			  current_file = $1;
			  /* '*' matches any file name.  */
			  if (strcmp (current_file.name, "*") == 0)
			    current_file.name = NULL;
			}
		'(' file_NAME_list ')'
	;

input_section_spec:
		input_section_spec_no_keep
	|	KEEP '('
			{ ldgram_had_keep = true; }
		input_section_spec_no_keep ')'
			{ ldgram_had_keep = false; }
	;

statement:
	  	assignment end
	|	CREATE_OBJECT_SYMBOLS
		{
 		lang_add_attribute(lang_object_symbols_statement_enum); 
	      	}
        |	';'
        |	CONSTRUCTORS
		{
 		
		  lang_add_attribute(lang_constructors_statement_enum); 
		}
	| SORT '(' CONSTRUCTORS ')'
		{
		  constructors_sorted = true;
		  lang_add_attribute (lang_constructors_statement_enum);
		}
	| input_section_spec
        | length '(' mustbe_exp ')'
        	        {
			lang_add_data((int) $1,$3);
			}
  
	| FILL '(' mustbe_exp ')'
			{
			  lang_add_fill
			    (exp_get_value_int($3,
					       0,
					       "fill value",
					       lang_first_phase_enum));
			}
	;

statement_list:
		statement_list statement
  	|  	statement
	;
  
statement_list_opt:
		/* empty */
	|	statement_list
	;

length:
		QUAD
			{ $$ = $1; }
	|	SQUAD
			{ $$ = $1; }
	|	LONG
			{ $$ = $1; }
	| 	SHORT
			{ $$ = $1; }
	|	BYTE
			{ $$ = $1; }
	;

fill_opt:
          '=' mustbe_exp
		{
		  $$ =	 exp_get_value_int($2,
					   0,
					   "fill value",
					   lang_first_phase_enum);
		}
	| 	{ $$ = 0; }
	;

		

assign_op:
		PLUSEQ
			{ $$ = '+'; }
	|	MINUSEQ
			{ $$ = '-'; }
	| 	MULTEQ
			{ $$ = '*'; }
	| 	DIVEQ
			{ $$ = '/'; }
	| 	LSHIFTEQ
			{ $$ = LSHIFT; }
	| 	RSHIFTEQ
			{ $$ = RSHIFT; }
	| 	ANDEQ
			{ $$ = '&'; }
	| 	OREQ
			{ $$ = '|'; }

	;

end:	';' | ','
	;


assignment:
		NAME '=' mustbe_exp
		{
		  lang_add_assignment (exp_assop ($2, $1, $3));
		}
	|	NAME assign_op mustbe_exp
		{
		  lang_add_assignment (exp_assop ('=', $1,
						  exp_binop ($2,
							     exp_nameop (NAME,
									 $1),
							     $3)));
		}
	|	PROVIDE '(' NAME '=' mustbe_exp ')'
		{
		  lang_add_assignment (exp_provide ($3, $5));
		}
	;


opt_comma:
		','	|	;


memory:
		MEMORY '{' memory_spec memory_spec_list '}'
	;

memory_spec_list:
		memory_spec_list memory_spec
	|	memory_spec_list ',' memory_spec
	|
	;


memory_spec: 		NAME
			{ region = lang_memory_region_lookup($1); }
		attributes_opt ':'
		origin_spec opt_comma length_spec

	;

origin_spec:
	ORIGIN '=' mustbe_exp
		{ region->current =
		 region->origin =
		 exp_get_vma($3, 0L,"origin", lang_first_phase_enum);
}
	;

length_spec:
             LENGTH '=' mustbe_exp
               { region->length = exp_get_vma($3,
					       ~((bfd_vma)0),
					       "length",
					       lang_first_phase_enum);
		}
	;

attributes_opt:
		/* empty */
		  { /* dummy action to avoid bison 1.25 error message */ }
	|	'(' attributes_list ')'
	;

attributes_list:
		attributes_string
	|	attributes_list attributes_string
	;

attributes_string:
		NAME
		  { lang_set_flags (region, $1, 0); }
	|	'!' NAME
		  { lang_set_flags (region, $2, 1); }
	;

startup:
	STARTUP '(' filename ')'
		{ lang_startup($3); }
	;

high_level_library:
		HLL '(' high_level_library_NAME_list ')'
	|	HLL '(' ')'
			{ ldemul_hll((char *)NULL); }
	;

high_level_library_NAME_list:
		high_level_library_NAME_list opt_comma filename
			{ ldemul_hll($3); }
	|	filename
			{ ldemul_hll($1); }

	;

low_level_library:
	SYSLIB '(' low_level_library_NAME_list ')'
	; low_level_library_NAME_list:
		low_level_library_NAME_list opt_comma filename
			{ ldemul_syslib($3); }
	|
	;

floating_point_support:
		FLOAT
			{ lang_float(true); }
	|	NOFLOAT
			{ lang_float(false); }
	;
		
nocrossref_list:
		/* empty */
		{
		  $$ = NULL;
		}
	|	NAME nocrossref_list
		{
		  struct lang_nocrossref *n;

		  n = (struct lang_nocrossref *) xmalloc (sizeof *n);
		  n->name = $1;
		  n->next = $2;
		  $$ = n;
		}
	|	NAME ',' nocrossref_list
		{
		  struct lang_nocrossref *n;

		  n = (struct lang_nocrossref *) xmalloc (sizeof *n);
		  n->name = $1;
		  n->next = $3;
		  $$ = n;
		}
	;

mustbe_exp:		 { ldlex_expression(); }
		exp
			 { ldlex_popstate(); $$=$2;}
	;

exp	:
		'-' exp %prec UNARY
			{ $$ = exp_unop('-', $2); }
	|	'(' exp ')'
			{ $$ = $2; }
	|	NEXT '(' exp ')' %prec UNARY
			{ $$ = exp_unop((int) $1,$3); }
	|	'!' exp %prec UNARY
			{ $$ = exp_unop('!', $2); }
	|	'+' exp %prec UNARY
			{ $$ = $2; }
	|	'~' exp %prec UNARY
			{ $$ = exp_unop('~', $2);}

	|	exp '*' exp
			{ $$ = exp_binop('*', $1, $3); }
	|	exp '/' exp
			{ $$ = exp_binop('/', $1, $3); }
	|	exp '%' exp
			{ $$ = exp_binop('%', $1, $3); }
	|	exp '+' exp
			{ $$ = exp_binop('+', $1, $3); }
	|	exp '-' exp
			{ $$ = exp_binop('-' , $1, $3); }
	|	exp LSHIFT exp
			{ $$ = exp_binop(LSHIFT , $1, $3); }
	|	exp RSHIFT exp
			{ $$ = exp_binop(RSHIFT , $1, $3); }
	|	exp EQ exp
			{ $$ = exp_binop(EQ , $1, $3); }
	|	exp NE exp
			{ $$ = exp_binop(NE , $1, $3); }
	|	exp LE exp
			{ $$ = exp_binop(LE , $1, $3); }
  	|	exp GE exp
			{ $$ = exp_binop(GE , $1, $3); }
	|	exp '<' exp
			{ $$ = exp_binop('<' , $1, $3); }
	|	exp '>' exp
			{ $$ = exp_binop('>' , $1, $3); }
	|	exp '&' exp
			{ $$ = exp_binop('&' , $1, $3); }
	|	exp '^' exp
			{ $$ = exp_binop('^' , $1, $3); }
	|	exp '|' exp
			{ $$ = exp_binop('|' , $1, $3); }
	|	exp '?' exp ':' exp
			{ $$ = exp_trinop('?' , $1, $3, $5); }
	|	exp ANDAND exp
			{ $$ = exp_binop(ANDAND , $1, $3); }
	|	exp OROR exp
			{ $$ = exp_binop(OROR , $1, $3); }
	|	DEFINED '(' NAME ')'
			{ $$ = exp_nameop(DEFINED, $3); }
	|	INT
			{ $$ = exp_intop($1); }
        |	SIZEOF_HEADERS
			{ $$ = exp_nameop(SIZEOF_HEADERS,0); }

	|	SIZEOF '(' NAME ')'
			{ $$ = exp_nameop(SIZEOF,$3); }
	|	ADDR '(' NAME ')'
			{ $$ = exp_nameop(ADDR,$3); }
	|	LOADADDR '(' NAME ')'
			{ $$ = exp_nameop(LOADADDR,$3); }
	|	ABSOLUTE '(' exp ')'
			{ $$ = exp_unop(ABSOLUTE, $3); }
	|	ALIGN_K '(' exp ')'
			{ $$ = exp_unop(ALIGN_K,$3); }
	|	BLOCK '(' exp ')'
			{ $$ = exp_unop(ALIGN_K,$3); }
	|	NAME
			{ $$ = exp_nameop(NAME,$1); }
	|	MAX_K '(' exp ',' exp ')'
			{ $$ = exp_binop (MAX_K, $3, $5 ); }
	|	MIN_K '(' exp ',' exp ')'
			{ $$ = exp_binop (MIN_K, $3, $5 ); }
	|	ASSERT_K '(' exp ',' NAME ')'
			{ $$ = exp_assert ($3, $5); }
	;


memspec_at_opt:
                AT '>' NAME { $$ = $3; }
        |       { $$ = "*default*"; }
        ;

opt_at:
		AT '(' exp ')' { $$ = $3; }
	|	{ $$ = 0; }
	;

section:	NAME 		{ ldlex_expression(); }
		opt_exp_with_type 
		opt_at   	{ ldlex_popstate (); ldlex_script (); }
		'{'
			{
			  lang_enter_output_section_statement($1, $3,
							      sectype,
							      0, 0, 0, $4);
			}
		statement_list_opt 	
 		'}' { ldlex_popstate (); ldlex_expression (); }
		memspec_opt memspec_at_opt phdr_opt fill_opt
		{
		  ldlex_popstate ();
		  lang_leave_output_section_statement ($14, $11, $13, $12);
		}
		opt_comma
	|	OVERLAY
			{ ldlex_expression (); }
		opt_exp_without_type opt_nocrossrefs opt_at
			{ ldlex_popstate (); ldlex_script (); }
		'{' 
			{
			  lang_enter_overlay ($3, $5, (int) $4);
			}
		overlay_section
		'}'
			{ ldlex_popstate (); ldlex_expression (); }
		memspec_opt memspec_at_opt phdr_opt fill_opt
			{
			  ldlex_popstate ();
			  lang_leave_overlay ($15, $12, $14, $13);
			}
		opt_comma
	|	/* The GROUP case is just enough to support the gcc
		   svr3.ifile script.  It is not intended to be full
		   support.  I'm not even sure what GROUP is supposed
		   to mean.  */
		GROUP { ldlex_expression (); }
		opt_exp_with_type
		{
		  ldlex_popstate ();
		  lang_add_assignment (exp_assop ('=', ".", $3));
		}
		'{' sec_or_group_p1 '}'
	;

type:
	   NOLOAD  { sectype = noload_section; }
	|  DSECT   { sectype = dsect_section; }
	|  COPY    { sectype = copy_section; }
	|  INFO    { sectype = info_section; }
	|  OVERLAY { sectype = overlay_section; }
	;

atype:
	 	'(' type ')'
  	| 	/* EMPTY */ { sectype = normal_section; }
  	| 	'(' ')' { sectype = normal_section; }
	;

opt_exp_with_type:
		exp atype ':'		{ $$ = $1; }
	|	atype ':'		{ $$ = (etree_type *)NULL;  }
	|	/* The BIND cases are to support the gcc svr3.ifile
		   script.  They aren't intended to implement full
		   support for the BIND keyword.  I'm not even sure
		   what BIND is supposed to mean.  */
		BIND '(' exp ')' atype ':' { $$ = $3; }
	|	BIND '(' exp ')' BLOCK '(' exp ')' atype ':'
		{ $$ = $3; }
	;

opt_exp_without_type:
		exp ':'		{ $$ = $1; }
	|	':'		{ $$ = (etree_type *) NULL;  }
	;

opt_nocrossrefs:
		/* empty */
			{ $$ = 0; }
	|	NOCROSSREFS
			{ $$ = 1; }
	;

memspec_opt:
		'>' NAME
		{ $$ = $2; }
	|	{ $$ = "*default*"; }
	;

phdr_opt:
		/* empty */
		{
		  $$ = NULL;
		}
	|	phdr_opt ':' NAME
		{
		  struct lang_output_section_phdr_list *n;

		  n = ((struct lang_output_section_phdr_list *)
		       xmalloc (sizeof *n));
		  n->name = $3;
		  n->used = false;
		  n->next = $1;
		  $$ = n;
		}
	;

overlay_section:
		/* empty */
	|	overlay_section
		NAME
			{
			  ldlex_script ();
			  lang_enter_overlay_section ($2);
			}
		'{' statement_list_opt '}'
			{ ldlex_popstate (); ldlex_expression (); }
		phdr_opt fill_opt
			{
			  ldlex_popstate ();
			  lang_leave_overlay_section ($9, $8);
			}
		opt_comma
	;

phdrs:
		PHDRS '{' phdr_list '}'
	;

phdr_list:
		/* empty */
	|	phdr_list phdr
	;

phdr:
		NAME { ldlex_expression (); }
		  phdr_type phdr_qualifiers { ldlex_popstate (); }
		  ';'
		{
		  lang_new_phdr ($1, $3, $4.filehdr, $4.phdrs, $4.at,
				 $4.flags);
		}
	;

phdr_type:
		exp
		{
		  $$ = $1;

		  if ($1->type.node_class == etree_name
		      && $1->type.node_code == NAME)
		    {
		      const char *s;
		      unsigned int i;
		      static const char * const phdr_types[] =
			{
			  "PT_NULL", "PT_LOAD", "PT_DYNAMIC",
			  "PT_INTERP", "PT_NOTE", "PT_SHLIB",
			  "PT_PHDR"
			};

		      s = $1->name.name;
		      for (i = 0;
			   i < sizeof phdr_types / sizeof phdr_types[0];
			   i++)
			if (strcmp (s, phdr_types[i]) == 0)
			  {
			    $$ = exp_intop (i);
			    break;
			  }
		    }
		}
	;

phdr_qualifiers:
		/* empty */
		{
		  memset (&$$, 0, sizeof (struct phdr_info));
		}
	|	NAME phdr_val phdr_qualifiers
		{
		  $$ = $3;
		  if (strcmp ($1, "FILEHDR") == 0 && $2 == NULL)
		    $$.filehdr = true;
		  else if (strcmp ($1, "PHDRS") == 0 && $2 == NULL)
		    $$.phdrs = true;
		  else if (strcmp ($1, "FLAGS") == 0 && $2 != NULL)
		    $$.flags = $2;
		  else
		    einfo (_("%X%P:%S: PHDRS syntax error at `%s'\n"), $1);
		}
	|	AT '(' exp ')' phdr_qualifiers
		{
		  $$ = $5;
		  $$.at = $3;
		}
	;

phdr_val:
		/* empty */
		{
		  $$ = NULL;
		}
	| '(' exp ')'
		{
		  $$ = $2;
		}
	;

/* This syntax is used within an external version script file.  */

version_script_file:
		{
		  ldlex_version_file ();
		  PUSH_ERROR (_("VERSION script"));
		}
		vers_nodes
		{
		  ldlex_popstate ();
		  POP_ERROR ();
		}
	;

/* This is used within a normal linker script file.  */

version:
		{
		  ldlex_version_script ();
		}
		VERSIONK '{' vers_nodes '}'
		{
		  ldlex_popstate ();
		}
	;

vers_nodes:
		vers_node
	|	vers_nodes vers_node
	;

vers_node:
		VERS_TAG '{' vers_tag '}' ';'
		{
		  lang_register_vers_node ($1, $3, NULL);
		}
	|	VERS_TAG '{' vers_tag '}' verdep ';'
		{
		  lang_register_vers_node ($1, $3, $5);
		}
	;

verdep:
		VERS_TAG
		{
		  $$ = lang_add_vers_depend (NULL, $1);
		}
	|	verdep VERS_TAG
		{
		  $$ = lang_add_vers_depend ($1, $2);
		}
	;

vers_tag:
		/* empty */
		{
		  $$ = lang_new_vers_node (NULL, NULL);
		}
	|	vers_defns ';'
		{
		  $$ = lang_new_vers_node ($1, NULL);
		}
	|	GLOBAL ':' vers_defns ';'
		{
		  $$ = lang_new_vers_node ($3, NULL);
		}
	|	LOCAL ':' vers_defns ';'
		{
		  $$ = lang_new_vers_node (NULL, $3);
		}
	|	GLOBAL ':' vers_defns ';' LOCAL ':' vers_defns ';'
		{
		  $$ = lang_new_vers_node ($3, $7);
		}
	;

vers_defns:
		VERS_IDENTIFIER
		{
		  $$ = lang_new_vers_regex (NULL, $1, ldgram_vers_current_lang);
		}
	|	vers_defns ';' VERS_IDENTIFIER
		{
		  $$ = lang_new_vers_regex ($1, $3, ldgram_vers_current_lang);
		}
	|	EXTERN NAME '{'
			{
			  $<name>$ = ldgram_vers_current_lang;
			  ldgram_vers_current_lang = $2;
			}
		vers_defns '}'
			{
			  $$ = $5;
			  ldgram_vers_current_lang = $<name>4;
			}
	;

%%
void
yyerror(arg) 
     const char *arg;
{ 
  if (ldfile_assumed_script)
    einfo (_("%P:%s: file format not recognized; treating as linker script\n"),
	   ldfile_input_filename);
  if (error_index > 0 && error_index < ERROR_NAME_MAX)
     einfo ("%P%F:%S: %s in %s\n", arg, error_names[error_index-1]);
  else
     einfo ("%P%F:%S: %s\n", arg);
}
