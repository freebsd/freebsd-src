/* yyscript.y -- linker script grammer for gold.  */

/* This is a bison grammar to parse a subset of the original GNU ld
   linker script language.  */

%{

#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "script-c.h"

%}

/* We need to use a pure parser because we might be multi-threaded.
   We pass some arguments through the parser to the lexer.  */

%pure-parser

%parse-param {void* closure}
%lex-param {void* closure}

/* Since we require bison anyhow, we take advantage of it.  */

%error-verbose

/* The values associated with tokens.  */

%union {
  const char* string;
  int64_t integer;
}

/* Operators, including a precedence table for expressions.  */

%right PLUSEQ MINUSEQ MULTEQ DIVEQ '=' LSHIFTEQ RSHIFTEQ ANDEQ OREQ
%right '?' ':'
%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'

/* Constants.  */

%token <string> STRING
%token <integer> INTEGER

/* Keywords.  This list is taken from ldgram.y and ldlex.l in the old
   GNU linker, with the keywords which only appear in MRI mode
   removed.  Not all these keywords are actually used in this grammar.
   In most cases the keyword is recognized as the token name in upper
   case.  The comments indicate where this is not the case.  */

%token ABSOLUTE
%token ADDR
%token ALIGN_K		/* ALIGN */
%token ASSERT_K		/* ASSERT */
%token AS_NEEDED
%token AT
%token BIND
%token BLOCK
%token BYTE
%token CONSTANT
%token CONSTRUCTORS
%token COPY
%token CREATE_OBJECT_SYMBOLS
%token DATA_SEGMENT_ALIGN
%token DATA_SEGMENT_END
%token DATA_SEGMENT_RELRO_END
%token DEFINED
%token DSECT
%token ENTRY
%token EXCLUDE_FILE
%token EXTERN
%token FILL
%token FLOAT
%token FORCE_COMMON_ALLOCATION
%token GLOBAL		/* global */
%token GROUP
%token HLL
%token INCLUDE
%token INFO
%token INHIBIT_COMMON_ALLOCATION
%token INPUT
%token KEEP
%token LENGTH		/* LENGTH, l, len */
%token LOADADDR
%token LOCAL		/* local */
%token LONG
%token MAP
%token MAX_K		/* MAX */
%token MEMORY
%token MIN_K		/* MIN */
%token NEXT
%token NOCROSSREFS
%token NOFLOAT
%token NOLOAD
%token ONLY_IF_RO
%token ONLY_IF_RW
%token ORIGIN		/* ORIGIN, o, org */
%token OUTPUT
%token OUTPUT_ARCH
%token OUTPUT_FORMAT
%token OVERLAY
%token PHDRS
%token PROVIDE
%token PROVIDE_HIDDEN
%token QUAD
%token SEARCH_DIR
%token SECTIONS
%token SEGMENT_START
%token SHORT
%token SIZEOF
%token SIZEOF_HEADERS	/* SIZEOF_HEADERS, sizeof_headers */
%token SORT_BY_ALIGNMENT
%token SORT_BY_NAME
%token SPECIAL
%token SQUAD
%token STARTUP
%token SUBALIGN
%token SYSLIB
%token TARGET_K		/* TARGET */
%token TRUNCATE
%token VERSIONK		/* VERSION */

%%

file_list:
	  file_list file_cmd
	| /* empty */
	;

file_cmd:
	  OUTPUT_FORMAT '(' STRING ')'
	| GROUP
	    { script_start_group(closure); }
	  '(' input_list ')'
	    { script_end_group(closure); }
	;

input_list:
	  input_list_element
	| input_list opt_comma input_list_element
	;

input_list_element:
	  STRING
	    { script_add_file(closure, $1); }
	| AS_NEEDED
	    { script_start_as_needed(closure); }
	  '(' input_list ')'
	    { script_end_as_needed(closure); }
	;

opt_comma:
	  ','
	| /* empty */
	;

%%
