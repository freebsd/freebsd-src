/* Chill language support routines for GDB, the GNU debugger.
   Copyright 1992, 1995, 1996 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
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
  const char *joiner = NULL;
  char *demangled;
  const char *cp = mangled;

  while (*cp)
    {
      if (is_cplus_marker (*cp))
	{
	  joiner = cp;
	  break;
	}
      cp++;
    }
  if (joiner != NULL && *(joiner + 1) == *joiner)
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
     GDB_FILE *stream;
{
  c &= 0xFF;			/* Avoid sign bit follies */

  if (PRINT_LITERAL_FORM (c))
    {
      if (c == '\'' || c == '^')
	fprintf_filtered (stream, "'%c%c'", c, c);
      else
	fprintf_filtered (stream, "'%c'", c);
    }
  else
    {
      fprintf_filtered (stream, "'^(%u)'", (unsigned int) c);
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
     GDB_FILE *stream;
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
      fputs_filtered ("\"\"", stream);
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
	      if (in_control_form)
		fputs_filtered (")", stream);
	      fputs_filtered ("\"//", stream);
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
	  if (! in_literal_form && ! in_control_form)
	    fputs_filtered ("\"", stream);
	  if (PRINT_LITERAL_FORM (c))
	    {
	      if (!in_literal_form)
		{
		  if (in_control_form)
		    {
		      fputs_filtered (")", stream);
		      in_control_form = 0;
		    }
		  in_literal_form = 1;
		}
	      fprintf_filtered (stream, "%c", c);
	      if (c == '"' || c == '^')
		/* duplicate this one as must be done at input */
		fprintf_filtered (stream, "%c", c);
	    }
	  else
	    {
	      if (!in_control_form)
		{
		  if (in_literal_form)
		    {
		      in_literal_form = 0;
		    }
		  fputs_filtered ("^(", stream);
		  in_control_form = 1;
		}
	      else
		fprintf_filtered (stream, ",");
	      c = c & 0xff;
	      fprintf_filtered (stream, "%u", (unsigned int) c);
	    }
	  ++things_printed;
	}
    }

  /* Terminate the quotes if necessary.  */
  if (in_control_form)
    {
      fputs_filtered (")", stream);
    }
  if (in_literal_form || in_control_form)
    {
      fputs_filtered ("\"", stream);
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
	type = init_type (TYPE_CODE_INT, 1, 0, "BYTE", objfile);
	break;
      case FT_UNSIGNED_CHAR:
	type = init_type (TYPE_CODE_INT, 1, TYPE_FLAG_UNSIGNED, "UBYTE", objfile);
	break;
      case FT_SHORT:			/* Chill ints are 2 bytes */
	type = init_type (TYPE_CODE_INT, 2, 0, "INT", objfile);
	break;
      case FT_UNSIGNED_SHORT:		/* Chill ints are 2 bytes */
	type = init_type (TYPE_CODE_INT, 2, TYPE_FLAG_UNSIGNED, "UINT", objfile);
	break;
      case FT_INTEGER:			/* FIXME? */
      case FT_SIGNED_INTEGER:		/* FIXME? */
      case FT_LONG:			/* Chill longs are 4 bytes */
      case FT_SIGNED_LONG:		/* Chill longs are 4 bytes */
	type = init_type (TYPE_CODE_INT, 4, 0, "LONG", objfile);
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
    {"SIZE",UNOP_SIZEOF, PREC_BUILTIN_FUNCTION, 0},
    {"LOWER",UNOP_LOWER, PREC_BUILTIN_FUNCTION, 0},
    {"UPPER",UNOP_UPPER, PREC_BUILTIN_FUNCTION, 0},
    {"CARD",UNOP_CARD, PREC_BUILTIN_FUNCTION, 0},
    {"MAX",UNOP_CHMAX, PREC_BUILTIN_FUNCTION, 0},
    {"MIN",UNOP_CHMIN, PREC_BUILTIN_FUNCTION, 0},
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
    {"->",  UNOP_IND, PREC_SUFFIX, 1},
    {"->",  UNOP_ADDR, PREC_PREFIX, 0},
    {":",   BINOP_RANGE, PREC_ASSIGN, 0},
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

/* Calculate LOWER or UPPER of TYPE.
   Returns the result as an integer.
   *RESULT_TYPE is the appropriate type for the result. */

LONGEST
type_lower_upper (op, type, result_type)
     enum exp_opcode op;  /* Either UNOP_LOWER or UNOP_UPPER */
     struct type *type;
     struct type **result_type;
{
  LONGEST low, high;
  *result_type = type;
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
      *result_type = builtin_type_int;
      if (chill_varying_type (type))
	return type_lower_upper (op, TYPE_FIELD_TYPE (type, 1), result_type);
      break;
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_STRING:
      type = TYPE_FIELD_TYPE (type, 0);  /* Get index type */

      /* ... fall through ... */
    case TYPE_CODE_RANGE:
      *result_type = TYPE_TARGET_TYPE (type);
      return op == UNOP_LOWER ? TYPE_LOW_BOUND (type) : TYPE_HIGH_BOUND (type);

    case TYPE_CODE_ENUM:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
      if (get_discrete_bounds (type, &low, &high) >= 0)
	{
	  *result_type = type;
	  return op == UNOP_LOWER ? low : high;
	}
      break;
    case TYPE_CODE_UNDEF:
    case TYPE_CODE_PTR:
    case TYPE_CODE_UNION:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_FLT:
    case TYPE_CODE_VOID:
    case TYPE_CODE_SET:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_MEMBER:
    case TYPE_CODE_METHOD:
    case TYPE_CODE_REF:
    case TYPE_CODE_COMPLEX:
    default:
      break;
    }
  error ("unknown mode for LOWER/UPPER builtin");
}

static value_ptr
value_chill_length (val)
     value_ptr val;
{
  LONGEST tmp;
  struct type *type = VALUE_TYPE (val);
  struct type *ttype;
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_STRING:
      tmp = type_lower_upper (UNOP_UPPER, type, &ttype)
	- type_lower_upper (UNOP_LOWER, type, &ttype) + 1;
      break;
    case TYPE_CODE_STRUCT:
      if (chill_varying_type (type))
	{
	  tmp = unpack_long (TYPE_FIELD_TYPE (type, 0), VALUE_CONTENTS (val));
	  break;
	}
      /* ... else fall through ... */
    default:
      error ("bad argument to LENGTH builtin");
    }
  return value_from_longest (builtin_type_int, tmp);
}

static value_ptr
value_chill_card (val)
     value_ptr val;
{
  LONGEST tmp = 0;
  struct type *type = VALUE_TYPE (val);
  CHECK_TYPEDEF (type);

  if (TYPE_CODE (type) == TYPE_CODE_SET)
    {
      struct type *range_type = TYPE_INDEX_TYPE (type);
      LONGEST lower_bound, upper_bound;
      int i;

      get_discrete_bounds (range_type, &lower_bound, &upper_bound);
      for (i = lower_bound; i <= upper_bound; i++)
	if (value_bit_index (type, VALUE_CONTENTS (val), i) > 0)
	  tmp++;
    }
  else
    error ("bad argument to CARD builtin");

  return value_from_longest (builtin_type_int, tmp);
}

static value_ptr
value_chill_max_min (op, val)
     enum exp_opcode op;
     value_ptr val;
{
  LONGEST tmp = 0;
  struct type *type = VALUE_TYPE (val);
  struct type *elttype;
  CHECK_TYPEDEF (type);

  if (TYPE_CODE (type) == TYPE_CODE_SET)
    {
      LONGEST lower_bound, upper_bound;
      int i, empty = 1;

      elttype = TYPE_INDEX_TYPE (type);
      CHECK_TYPEDEF (elttype);
      get_discrete_bounds (elttype, &lower_bound, &upper_bound);

      if (op == UNOP_CHMAX)
	{
	  for (i = upper_bound; i >= lower_bound; i--)
	    {
	      if (value_bit_index (type, VALUE_CONTENTS (val), i) > 0)
		{
		  tmp = i;
		  empty = 0;
		  break;
		}
	    }
	}
      else
	{
	  for (i = lower_bound; i <= upper_bound; i++)
	    {
	      if (value_bit_index (type, VALUE_CONTENTS (val), i) > 0)
		{
		  tmp = i;
		  empty = 0;
		  break;
		}
	    }
	}
      if (empty)
	error ("%s for empty powerset", op == UNOP_CHMAX ? "MAX" : "MIN");
    }
  else
    error ("bad argument to %s builtin", op == UNOP_CHMAX ? "MAX" : "MIN");

  return value_from_longest (TYPE_CODE (elttype) == TYPE_CODE_RANGE
			       ? TYPE_TARGET_TYPE (elttype)
			       : elttype,
			     tmp);
}

static value_ptr
evaluate_subexp_chill (expect_type, exp, pos, noside)
     struct type *expect_type;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  int pc = *pos;
  struct type *type;
  int tem, nargs;
  value_ptr arg1;
  value_ptr *argvec;
  enum exp_opcode op = exp->elts[*pos].opcode;
  switch (op)
    {
    case MULTI_SUBSCRIPT:
      if (noside == EVAL_SKIP)
	break;
      (*pos) += 3;
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      type = check_typedef (VALUE_TYPE (arg1));

      if (nargs == 1 && TYPE_CODE (type) == TYPE_CODE_INT)
	{
	  /* Looks like string repetition. */
	  value_ptr string = evaluate_subexp_with_coercion (exp, pos, noside);
	  return value_concat (arg1, string);
	}

      switch (TYPE_CODE (type))
	{
	case TYPE_CODE_PTR:
	  type = check_typedef (TYPE_TARGET_TYPE (type));
	  if (!type || TYPE_CODE (type) != TYPE_CODE_FUNC)
	    error ("reference value used as function");
	  /* ... fall through ... */
	case TYPE_CODE_FUNC:
	  /* It's a function call. */
	  if (noside == EVAL_AVOID_SIDE_EFFECTS)
	    break;

	  /* Allocate arg vector, including space for the function to be
	     called in argvec[0] and a terminating NULL */
	  argvec = (value_ptr *) alloca (sizeof (value_ptr) * (nargs + 2));
	  argvec[0] = arg1;
	  tem = 1;
	  for (; tem <= nargs && tem <= TYPE_NFIELDS (type); tem++)
	    {
	      argvec[tem]
		= evaluate_subexp_chill (TYPE_FIELD_TYPE (type, tem-1),
					 exp, pos, noside);
	    }
	  for (; tem <= nargs; tem++)
	    argvec[tem] = evaluate_subexp_with_coercion (exp, pos, noside);
	  argvec[tem] = 0; /* signal end of arglist */

	  return call_function_by_hand (argvec[0], nargs, argvec + 1);
	default:
	  break;
	}

      while (nargs-- > 0)
	{
	  value_ptr index = evaluate_subexp_with_coercion (exp, pos, noside);
	  arg1 = value_subscript (arg1, index);
	}
      return (arg1);

    case UNOP_LOWER:
    case UNOP_UPPER:
      (*pos)++;
      if (noside == EVAL_SKIP)
	{
	  (*exp->language_defn->evaluate_exp) (NULL_TYPE, exp, pos, EVAL_SKIP);
	  goto nosideret;
	}
      arg1 = (*exp->language_defn->evaluate_exp) (NULL_TYPE, exp, pos,
						  EVAL_AVOID_SIDE_EFFECTS);
      tem = type_lower_upper (op, VALUE_TYPE (arg1), &type);
      return value_from_longest (type, tem);

    case UNOP_LENGTH:
      (*pos)++;
      arg1 = (*exp->language_defn->evaluate_exp) (NULL_TYPE, exp, pos, noside);
      return value_chill_length (arg1);

    case UNOP_CARD:
      (*pos)++;
      arg1 = (*exp->language_defn->evaluate_exp) (NULL_TYPE, exp, pos, noside);
      return value_chill_card (arg1);

    case UNOP_CHMAX:
    case UNOP_CHMIN:
      (*pos)++;
      arg1 = (*exp->language_defn->evaluate_exp) (NULL_TYPE, exp, pos, noside);
      return value_chill_max_min (op, arg1);

    case BINOP_COMMA:
      error ("',' operator used in invalid context");

    default:
      break;
    }

  return evaluate_subexp_standard (expect_type, exp, pos, noside);
 nosideret:
  return value_from_longest (builtin_type_long, (LONGEST) 1);
}

const struct language_defn chill_language_defn = {
  "chill",
  language_chill,
  chill_builtin_types,
  range_check_on,
  type_check_on,
  chill_parse,			/* parser */
  chill_error,			/* parser error function */
  evaluate_subexp_chill,
  chill_printchar,		/* print a character constant */
  chill_printstr,		/* function to print a string constant */
  chill_create_fundamental_type,/* Create fundamental type in this language */
  chill_print_type,		/* Print a type using appropriate syntax */
  chill_val_print,		/* Print a value using appropriate syntax */
  chill_value_print,		/* Print a top-levl value */
  {"",      "B'",  "",   ""},	/* Binary format info */
  {"O'%lo",  "O'",  "o",  ""},	/* Octal format info */
  {"D'%ld",  "D'",  "d",  ""},	/* Decimal format info */
  {"H'%lx",  "H'",  "x",  ""},	/* Hex format info */
  chill_op_print_tab,		/* expression operators for printing */
  0,				/* arrays are first-class (not c-style) */
  0,				/* String lower bound */
  &builtin_type_chill_char,	/* Type of string elements */ 
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
