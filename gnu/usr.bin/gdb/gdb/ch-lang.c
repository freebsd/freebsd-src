/* Chill language support routines for GDB, the GNU debugger.
   Copyright 1992 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "ch-lang.h"


/* For now, Chill uses a simple mangling algorithm whereby you simply
   discard everything after the occurance of two successive CPLUS_MARKER
   characters to derive the demangled form. */

char *
chill_demangle (mangled)
     const char *mangled;
{
  char *joiner;
  char *demangled;

  joiner = strchr (mangled, CPLUS_MARKER);
  if (joiner != NULL && *(joiner + 1) == CPLUS_MARKER)
    {
      demangled = savestring (mangled, joiner - mangled);
    }
  else
    {
      demangled = NULL;
    }
  return (demangled);
}

static void
chill_printchar (c, stream)
     register int c;
     FILE *stream;
{
  c &= 0xFF;			/* Avoid sign bit follies */

  if (PRINT_LITERAL_FORM (c))
    {
      fprintf_filtered (stream, "'%c'", c);
    }
  else
    {
      fprintf_filtered (stream, "C'%.2x'", (unsigned int) c);
    }
}

/* Print the character string STRING, printing at most LENGTH characters.
   Printing stops early if the number hits print_max; repeat counts
   are printed as appropriate.  Print ellipses at the end if we
   had to stop before printing LENGTH characters, or if FORCE_ELLIPSES.
   Note that gdb maintains the length of strings without counting the
   terminating null byte, while chill strings are typically written with
   an explicit null byte.  So we always assume an implied null byte
   until gdb is able to maintain non-null terminated strings as well
   as null terminated strings (FIXME).
  */

static void
chill_printstr (stream, string, length, force_ellipses)
     FILE *stream;
     char *string;
     unsigned int length;
     int force_ellipses;
{
  register unsigned int i;
  unsigned int things_printed = 0;
  int in_literal_form = 0;
  int in_control_form = 0;
  int need_slashslash = 0;
  unsigned int c;
  extern int repeat_count_threshold;
  extern int print_max;

  if (length == 0)
    {
      chill_printchar ('\0', stream);
      return;
    }

  for (i = 0; i < length && things_printed < print_max; ++i)
    {
      /* Position of the character we are examining
	 to see whether it is repeated.  */
      unsigned int rep1;
      /* Number of repetitions we have detected so far.  */
      unsigned int reps;

      QUIT;

      if (need_slashslash)
	{
	  fputs_filtered ("//", stream);
	  need_slashslash = 0;
	}

      rep1 = i + 1;
      reps = 1;
      while (rep1 < length && string[rep1] == string[i])
	{
	  ++rep1;
	  ++reps;
	}

      c = string[i];
      if (reps > repeat_count_threshold)
	{
	  if (in_control_form || in_literal_form)
	    {
	      fputs_filtered ("'//", stream);
	      in_control_form = in_literal_form = 0;
	    }
	  chill_printchar (c, stream);
	  fprintf_filtered (stream, "<repeats %u times>", reps);
	  i = rep1 - 1;
	  things_printed += repeat_count_threshold;
	  need_slashslash = 1;
	}
      else
	{
	  if (PRINT_LITERAL_FORM (c))
	    {
	      if (!in_literal_form)
		{
		  if (in_control_form)
		    {
		      fputs_filtered ("'//", stream);
		      in_control_form = 0;
		    }
		  fputs_filtered ("'", stream);
		  in_literal_form = 1;
		}
	      fprintf_filtered (stream, "%c", c);
	    }
	  else
	    {
	      if (!in_control_form)
		{
		  if (in_literal_form)
		    {
		      fputs_filtered ("'//", stream);
		      in_literal_form = 0;
		    }
		  fputs_filtered ("c'", stream);
		  in_control_form = 1;
		}
	      fprintf_filtered (stream, "%.2x", c);
	    }
	  ++things_printed;
	}
    }

  /* Terminate the quotes if necessary.  */
  if (in_literal_form || in_control_form)
    {
      fputs_filtered ("'", stream);
    }
  if (force_ellipses || (i < length))
    {
      fputs_filtered ("...", stream);
    }
}

static struct type *
chill_create_fundamental_type (objfile, typeid)
     struct objfile *objfile;
     int typeid;
{
  register struct type *type = NULL;

  switch (typeid)
    {
      default:
	/* FIXME:  For now, if we are asked to produce a type not in this
	   language, create the equivalent of a C integer type with the
	   name "<?type?>".  When all the dust settles from the type
	   reconstruction work, this should probably become an error. */
	type = init_type (TYPE_CODE_INT, 2, 0, "<?type?>", objfile);
        warning ("internal error: no chill fundamental type %d", typeid);
	break;
      case FT_VOID:
	/* FIXME:  Currently the GNU Chill compiler emits some DWARF entries for
	   typedefs, unrelated to anything directly in the code being compiled,
	   that have some FT_VOID types.  Just fake it for now. */
	type = init_type (TYPE_CODE_VOID, 0, 0, "<?VOID?>", objfile);
	break;
      case FT_BOOLEAN:
	type = init_type (TYPE_CODE_BOOL, 1, TYPE_FLAG_UNSIGNED, "BOOL", objfile);
	break;
      case FT_CHAR:
	type = init_type (TYPE_CODE_CHAR, 1, TYPE_FLAG_UNSIGNED, "CHAR", objfile);
	break;
      case FT_SIGNED_CHAR:
	type = init_type (TYPE_CODE_INT, 1, TYPE_FLAG_SIGNED, "BYTE", objfile);
	break;
      case FT_UNSIGNED_CHAR:
	type = init_type (TYPE_CODE_INT, 1, TYPE_FLAG_UNSIGNED, "UBYTE", objfile);
	break;
      case FT_SHORT:			/* Chill ints are 2 bytes */
	type = init_type (TYPE_CODE_INT, 2, TYPE_FLAG_SIGNED, "INT", objfile);
	break;
      case FT_UNSIGNED_SHORT:		/* Chill ints are 2 bytes */
	type = init_type (TYPE_CODE_INT, 2, TYPE_FLAG_UNSIGNED, "UINT", objfile);
	break;
      case FT_INTEGER:			/* FIXME? */
      case FT_SIGNED_INTEGER:		/* FIXME? */
      case FT_LONG:			/* Chill longs are 4 bytes */
      case FT_SIGNED_LONG:		/* Chill longs are 4 bytes */
	type = init_type (TYPE_CODE_INT, 4, TYPE_FLAG_SIGNED, "LONG", objfile);
	break;
      case FT_UNSIGNED_INTEGER:		/* FIXME? */
      case FT_UNSIGNED_LONG:		/* Chill longs are 4 bytes */
	type = init_type (TYPE_CODE_INT, 4, TYPE_FLAG_UNSIGNED, "ULONG", objfile);
	break;
      case FT_FLOAT:
	type = init_type (TYPE_CODE_FLT, 4, 0, "REAL", objfile);
	break;
      case FT_DBL_PREC_FLOAT:
	type = init_type (TYPE_CODE_FLT, 8, 0, "LONG_REAL", objfile);
	break;
      }
  return (type);
}


/* Table of operators and their precedences for printing expressions.  */

static const struct op_print chill_op_print_tab[] = {
    {"AND", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
    {"OR",  BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
    {"NOT", UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
    {"MOD", BINOP_MOD, PREC_MUL, 0},
    {"REM", BINOP_REM, PREC_MUL, 0},
    {":=",  BINOP_ASSIGN, PREC_ASSIGN, 1},
    {"=",   BINOP_EQUAL, PREC_EQUAL, 0},
    {"/=",  BINOP_NOTEQUAL, PREC_EQUAL, 0},
    {"<=",  BINOP_LEQ, PREC_ORDER, 0},
    {">=",  BINOP_GEQ, PREC_ORDER, 0},
    {">",   BINOP_GTR, PREC_ORDER, 0},
    {"<",   BINOP_LESS, PREC_ORDER, 0},
    {"+",   BINOP_ADD, PREC_ADD, 0},
    {"-",   BINOP_SUB, PREC_ADD, 0},
    {"*",   BINOP_MUL, PREC_MUL, 0},
    {"/",   BINOP_DIV, PREC_MUL, 0},
    {"//",  BINOP_CONCAT, PREC_PREFIX, 0},	/* FIXME: precedence? */
    {"-",   UNOP_NEG, PREC_PREFIX, 0},
    {NULL,  0, 0, 0}
};


/* The built-in types of Chill.  */

struct type *builtin_type_chill_bool;
struct type *builtin_type_chill_char;
struct type *builtin_type_chill_long;
struct type *builtin_type_chill_ulong;
struct type *builtin_type_chill_real;

struct type ** const (chill_builtin_types[]) = 
{
  &builtin_type_chill_bool,
  &builtin_type_chill_char,
  &builtin_type_chill_long,
  &builtin_type_chill_ulong,
  &builtin_type_chill_real,
  0
};

const struct language_defn chill_language_defn = {
  "chill",
  language_chill,
  chill_builtin_types,
  range_check_on,
  type_check_on,
  chill_parse,			/* parser */
  chill_error,			/* parser error function */
  chill_printchar,		/* print a character constant */
  chill_printstr,		/* function to print a string constant */
  chill_create_fundamental_type,/* Create fundamental type in this language */
  chill_print_type,		/* Print a type using appropriate syntax */
  chill_val_print,		/* Print a value using appropriate syntax */
  &BUILTIN_TYPE_LONGEST,	/* longest signed   integral type */
  &BUILTIN_TYPE_UNSIGNED_LONGEST,/* longest unsigned integral type */
  &builtin_type_chill_real,	/* longest floating point type */
  {"",      "B'",  "",   ""},	/* Binary format info */
  {"O'%lo",  "O'",  "o",  ""},	/* Octal format info */
  {"D'%ld",  "D'",  "d",  ""},	/* Decimal format info */
  {"H'%lx",  "H'",  "x",  ""},	/* Hex format info */
  chill_op_print_tab,		/* expression operators for printing */
  LANG_MAGIC
};

/* Initialization for Chill */

void
_initialize_chill_language ()
{
  builtin_type_chill_bool =
    init_type (TYPE_CODE_BOOL, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "BOOL", (struct objfile *) NULL);
  builtin_type_chill_char =
    init_type (TYPE_CODE_CHAR, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "CHAR", (struct objfile *) NULL);
  builtin_type_chill_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "LONG", (struct objfile *) NULL);
  builtin_type_chill_ulong =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "ULONG", (struct objfile *) NULL);
  builtin_type_chill_real =
    init_type (TYPE_CODE_FLT, TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "LONG_REAL", (struct objfile *) NULL);

  add_language (&chill_language_defn);
}
