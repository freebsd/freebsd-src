/* Parser for GNU CHILL (CCITT High-Level Language)  -*- C -*-
   Copyright (C) 1992, 1993, 1995 Free Software Foundation, Inc.

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

/* Parse a Chill expression from text in a string,
   and return the result as a  struct expression  pointer.
   That structure contains arithmetic operations in reverse polish,
   with constants represented by operations that are followed by special data.
   See expression.h for the details of the format.
   What is important here is that it can be built up sequentially
   during the process of parsing; the lower levels of the tree always
   come first in the result.

   Note that the language accepted by this parser is more liberal
   than the one accepted by an actual Chill compiler.  For example, the
   language rule that a simple name string can not be one of the reserved
   simple name strings is not enforced (e.g "case" is not treated as a
   reserved name).  Another example is that Chill is a strongly typed
   language, and certain expressions that violate the type constraints
   may still be evaluated if gdb can do so in a meaningful manner, while
   such expressions would be rejected by the compiler.  The reason for
   this more liberal behavior is the philosophy that the debugger
   is intended to be a tool that is used by the programmer when things
   go wrong, and as such, it should provide as few artificial barriers
   to it's use as possible.  If it can do something meaningful, even
   something that violates language contraints that are enforced by the
   compiler, it should do so without complaint.

 */

#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "language.h"
#include "value.h"
#include "parser-defs.h"
#include "ch-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */

#ifdef __GNUC__
#define INLINE __inline__
#endif

typedef union

  {
    LONGEST lval;
    ULONGEST ulval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    double dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
  }YYSTYPE;

enum ch_terminal {
  END_TOKEN = 0,
  /* '\001' ... '\xff' come first. */
  OPEN_PAREN = '(',
  TOKEN_NOT_READ = 999,
  INTEGER_LITERAL,
  BOOLEAN_LITERAL,
  CHARACTER_LITERAL,
  FLOAT_LITERAL,
  GENERAL_PROCEDURE_NAME,
  LOCATION_NAME,
  EMPTINESS_LITERAL,
  CHARACTER_STRING_LITERAL,
  BIT_STRING_LITERAL,
  TYPENAME,
  DOT_FIELD_NAME, /* '.' followed by <field name> */
  CASE,
  OF,
  ESAC,
  LOGIOR,
  ORIF,
  LOGXOR,
  LOGAND,
  ANDIF,
  NOTEQUAL,
  GEQ,
  LEQ,
  IN,
  SLASH_SLASH,
  MOD,
  REM,
  NOT,
  POINTER,
  RECEIVE,
  UP,
  IF,
  THEN,
  ELSE,
  FI,
  ELSIF,
  ILLEGAL_TOKEN,
  NUM,
  PRED,
  SUCC,
  ABS,
  CARD,
  MAX_TOKEN,
  MIN_TOKEN,
  ADDR_TOKEN,
  SIZE,
  UPPER,
  LOWER,
  LENGTH,
  ARRAY,
  GDB_VARIABLE,
  GDB_ASSIGNMENT
};

/* Forward declarations. */

static void write_lower_upper_value PARAMS ((enum exp_opcode, struct type *));
static enum ch_terminal match_bitstring_literal PARAMS ((void));
static enum ch_terminal match_integer_literal PARAMS ((void));
static enum ch_terminal match_character_literal PARAMS ((void));
static enum ch_terminal match_string_literal PARAMS ((void));
static enum ch_terminal match_float_literal PARAMS ((void));
static enum ch_terminal match_float_literal PARAMS ((void));
static int decode_integer_literal PARAMS ((LONGEST *, char **));
static int decode_integer_value PARAMS ((int, char **, LONGEST *));
static char *match_simple_name_string PARAMS ((void));
static void growbuf_by_size PARAMS ((int));
static void parse_untyped_expr PARAMS ((void));
static void parse_if_expression PARAMS ((void));
static void parse_else_alternative PARAMS ((void));
static void parse_then_alternative PARAMS ((void));
static void parse_expr PARAMS ((void));
static void parse_operand0 PARAMS ((void));
static void parse_operand1 PARAMS ((void));
static void parse_operand2 PARAMS ((void));
static void parse_operand3 PARAMS ((void));
static void parse_operand4 PARAMS ((void));
static void parse_operand5 PARAMS ((void));
static void parse_operand6 PARAMS ((void));
static void parse_primval PARAMS ((void));
static void parse_tuple PARAMS ((struct type *));
static void parse_opt_element_list PARAMS ((struct type *));
static void parse_tuple_element PARAMS ((struct type *));
static void parse_named_record_element PARAMS ((void));
static void parse_call PARAMS ((void));
static struct type *parse_mode_or_normal_call PARAMS ((void));
#if 0
static struct type *parse_mode_call PARAMS ((void));
#endif
static void parse_unary_call PARAMS ((void));
static int parse_opt_untyped_expr PARAMS ((void));
static void parse_case_label PARAMS ((void));
static int expect PARAMS ((enum ch_terminal, char *));
static void parse_expr PARAMS ((void));
static void parse_primval PARAMS ((void));
static void parse_untyped_expr PARAMS ((void));
static int parse_opt_untyped_expr PARAMS ((void));
static void parse_if_expression_body PARAMS((void));
static enum ch_terminal ch_lex PARAMS ((void));
INLINE static enum ch_terminal PEEK_TOKEN PARAMS ((void));
static enum ch_terminal peek_token_ PARAMS ((int));
static void forward_token_ PARAMS ((void));
static void require PARAMS ((enum ch_terminal));
static int check_token PARAMS ((enum ch_terminal));

#define MAX_LOOK_AHEAD 2
static enum ch_terminal terminal_buffer[MAX_LOOK_AHEAD+1] = {
  TOKEN_NOT_READ, TOKEN_NOT_READ, TOKEN_NOT_READ};
static YYSTYPE yylval;
static YYSTYPE val_buffer[MAX_LOOK_AHEAD+1];

/*int current_token, lookahead_token;*/

INLINE static enum ch_terminal
PEEK_TOKEN()
{
  if (terminal_buffer[0] == TOKEN_NOT_READ)
    {
      terminal_buffer[0] = ch_lex ();
      val_buffer[0] = yylval;
    }
  return terminal_buffer[0];
}
#define PEEK_LVAL() val_buffer[0]
#define PEEK_TOKEN1() peek_token_(1)
#define PEEK_TOKEN2() peek_token_(2)
static enum ch_terminal
peek_token_ (i)
     int i;
{
  if (i > MAX_LOOK_AHEAD)
    fatal ("internal error - too much lookahead");
  if (terminal_buffer[i] == TOKEN_NOT_READ)
    {
      terminal_buffer[i] = ch_lex ();
      val_buffer[i] = yylval;
    }
  return terminal_buffer[i];
}

#if 0

static void
pushback_token (code, node)
     enum ch_terminal code;
     YYSTYPE node;
{
  int i;
  if (terminal_buffer[MAX_LOOK_AHEAD] != TOKEN_NOT_READ)
    fatal ("internal error - cannot pushback token");
  for (i = MAX_LOOK_AHEAD; i > 0; i--)
    { 
      terminal_buffer[i] = terminal_buffer[i - 1]; 
      val_buffer[i] = val_buffer[i - 1];
  }
  terminal_buffer[0] = code;
  val_buffer[0] = node;
}

#endif

static void
forward_token_()
{
  int i;
  for (i = 0; i < MAX_LOOK_AHEAD; i++)
    {
      terminal_buffer[i] = terminal_buffer[i+1];
      val_buffer[i] = val_buffer[i+1];
    }
  terminal_buffer[MAX_LOOK_AHEAD] = TOKEN_NOT_READ;
}
#define FORWARD_TOKEN() forward_token_()

/* Skip the next token.
   if it isn't TOKEN, the parser is broken. */

static void
require(token)
     enum ch_terminal token;
{
  if (PEEK_TOKEN() != token)
    {
      char buf[80];
      sprintf (buf, "internal parser error - expected token %d", (int)token);
      fatal(buf);
    }
  FORWARD_TOKEN();
}

static int
check_token (token)
     enum ch_terminal token;
{
  if (PEEK_TOKEN() != token)
    return 0;
  FORWARD_TOKEN ();
  return 1;
}

/* return 0 if expected token was not found,
   else return 1.
*/
static int
expect (token, message)
     enum ch_terminal token;
     char *message;
{
  if (PEEK_TOKEN() != token)
    {
      if (message)
	error (message);
      else if (token < 256)
	error ("syntax error - expected a '%c' here \"%s\"", token, lexptr);
      else
	error ("syntax error");
      return 0;
    }
  else
    FORWARD_TOKEN();
  return 1;
}

#if 0
static tree
parse_opt_name_string (allow_all)
     int allow_all; /* 1 if ALL is allowed as a postfix */
{
  int token = PEEK_TOKEN();
  tree name;
  if (token != NAME)
    {
      if (token == ALL && allow_all)
	{
	  FORWARD_TOKEN ();
	  return ALL_POSTFIX;
	}
      return NULL_TREE;
    }
  name = PEEK_LVAL();
  for (;;)
    {
      FORWARD_TOKEN ();
      token = PEEK_TOKEN();
      if (token != '!')
	return name;
      FORWARD_TOKEN();
      token = PEEK_TOKEN();
      if (token == ALL && allow_all)
	return get_identifier3(IDENTIFIER_POINTER (name), "!", "*");
      if (token != NAME)
	{
	  if (pass == 1)
	    error ("'%s!' is not followed by an identifier",
		   IDENTIFIER_POINTER (name));
	  return name;
	}
      name = get_identifier3(IDENTIFIER_POINTER(name),
			     "!", IDENTIFIER_POINTER(PEEK_LVAL()));
    }
}

static tree
parse_simple_name_string ()
{
  int token = PEEK_TOKEN();
  tree name;
  if (token != NAME)
    {
      error ("expected a name here");
      return error_mark_node;
    }
  name = PEEK_LVAL ();
  FORWARD_TOKEN ();
  return name;
}

static tree
parse_name_string ()
{
  tree name = parse_opt_name_string (0);
  if (name)
    return name;
  if (pass == 1)
    error ("expected a name string here");
  return error_mark_node;
}

/* Matches: <name_string>
   Returns if pass 1: the identifier.
   Returns if pass 2: a decl or value for identifier. */

static tree
parse_name ()
{
  tree name = parse_name_string ();
  if (pass == 1 || ignoring)
    return name;
  else
    {
      tree decl = lookup_name (name);
      if (decl == NULL_TREE)
	{
	  error ("`%s' undeclared", IDENTIFIER_POINTER (name));
	  return error_mark_node;
	}
      else if (TREE_CODE (TREE_TYPE (decl)) == ERROR_MARK)
	return error_mark_node;
      else if (TREE_CODE (decl) == CONST_DECL)
	return DECL_INITIAL (decl);
      else if (TREE_CODE (TREE_TYPE (decl)) == REFERENCE_TYPE)
	return convert_from_reference (decl);
      else
	return decl;
    } 
}
#endif

#if 0
static void
pushback_paren_expr (expr)
     tree expr;
{
  if (pass == 1 && !ignoring)
    expr = build1 (PAREN_EXPR, NULL_TREE, expr);
  pushback_token (EXPR, expr);
}
#endif

/* Matches: <case label> */

static void
parse_case_label ()
{
  if (check_token (ELSE))
    error ("ELSE in tuples labels not implemented");
  /* Does not handle the case of a mode name.  FIXME */
  parse_expr ();
  if (check_token (':'))
    {
      parse_expr ();
      write_exp_elt_opcode (BINOP_RANGE);
    }
}

static int
parse_opt_untyped_expr ()
{
  switch (PEEK_TOKEN ())
    {
    case ',':
    case ':':
    case ')':
      return 0;
    default:
      parse_untyped_expr ();
      return 1;
    }
}

static void
parse_unary_call ()
{
  FORWARD_TOKEN ();
  expect ('(', NULL);
  parse_expr ();
  expect (')', NULL);
}

/* Parse NAME '(' MODENAME ')'. */

#if 0

static struct type *
parse_mode_call ()
{
  struct type *type;
  FORWARD_TOKEN ();
  expect ('(', NULL);
  if (PEEK_TOKEN () != TYPENAME)
    error ("expect MODENAME here `%s'", lexptr);
  type = PEEK_LVAL().tsym.type;
  FORWARD_TOKEN ();
  expect (')', NULL);
  return type;
}

#endif

static struct type *
parse_mode_or_normal_call ()
{
  struct type *type;
  FORWARD_TOKEN ();
  expect ('(', NULL);
  if (PEEK_TOKEN () == TYPENAME)
    {
      type = PEEK_LVAL().tsym.type;
      FORWARD_TOKEN ();
    }
  else
    {
      parse_expr ();
      type = NULL;
    }
  expect (')', NULL);
  return type;
}

/* Parse something that looks like a function call.
   Assume we have parsed the function, and are at the '('. */

static void
parse_call ()
{
  int arg_count;
  require ('(');
  /* This is to save the value of arglist_len
     being accumulated for each dimension. */
  start_arglist ();
  if (parse_opt_untyped_expr ())
    {
      int tok = PEEK_TOKEN ();
      arglist_len = 1;
      if (tok == UP || tok == ':')
	{
	  FORWARD_TOKEN ();
	  parse_expr ();
	  expect (')', "expected ')' to terminate slice");
	  end_arglist ();
	  write_exp_elt_opcode (tok == UP ? TERNOP_SLICE_COUNT
				: TERNOP_SLICE);
	  return;
	}
      while (check_token (','))
	{
	  parse_untyped_expr ();
	  arglist_len++;
	}
    }
  else
    arglist_len = 0;
  expect (')', NULL);
  arg_count = end_arglist ();
  write_exp_elt_opcode (MULTI_SUBSCRIPT);
  write_exp_elt_longcst (arg_count);
  write_exp_elt_opcode (MULTI_SUBSCRIPT);
}

static void
parse_named_record_element ()
{
  struct stoken label;
  char buf[256];

  label = PEEK_LVAL ().sval;
  sprintf (buf, "expected a field name here `%s'", lexptr);
  expect (DOT_FIELD_NAME, buf);
  if (check_token (','))
    parse_named_record_element ();
  else if (check_token (':'))
    parse_expr ();
  else
    error ("syntax error near `%s' in named record tuple element", lexptr);
  write_exp_elt_opcode (OP_LABELED);
  write_exp_string (label);
  write_exp_elt_opcode (OP_LABELED);
}

/* Returns one or more TREE_LIST nodes, in reverse order. */

static void
parse_tuple_element (type)
     struct type *type;
{
  if (PEEK_TOKEN () == DOT_FIELD_NAME)
    {
      /* Parse a labelled structure tuple. */
      parse_named_record_element ();
      return;
    }

  if (check_token ('('))
    {
      if (check_token ('*'))
	{
	  expect (')', "missing ')' after '*' case label list");
	  if (type)
	    {
	      if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
		{
		  /* do this as a range from low to high */
		  struct type *range_type = TYPE_FIELD_TYPE (type, 0);
		  LONGEST low_bound, high_bound;
		  if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
		    error ("cannot determine bounds for (*)");
		  /* lower bound */
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (range_type);
		  write_exp_elt_longcst (low_bound);
		  write_exp_elt_opcode (OP_LONG);
		  /* upper bound */
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (range_type);
		  write_exp_elt_longcst (high_bound);
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_opcode (BINOP_RANGE);
		}
	      else
		error ("(*) in invalid context");
	    }
	  else
	    error ("(*) only possible with modename in front of tuple (mode[..])");
	}
      else
	{
	  parse_case_label ();
	  while (check_token (','))
	    {
	      parse_case_label ();
	      write_exp_elt_opcode (BINOP_COMMA);
	    }
	  expect (')', NULL);
	}
    }
  else
    parse_untyped_expr ();
  if (check_token (':'))
    {
      /* A powerset range or a labeled Array. */
      parse_untyped_expr ();
      write_exp_elt_opcode (BINOP_RANGE);
    }
}

/* Matches:  a COMMA-separated list of tuple elements.
   Returns a list (of TREE_LIST nodes). */
static void
parse_opt_element_list (type)
     struct type *type;
{
  arglist_len = 0;
  if (PEEK_TOKEN () == ']')
    return;
  for (;;)
    {
      parse_tuple_element (type);
      arglist_len++;
      if (PEEK_TOKEN () == ']')
	break;
      if (!check_token (','))
	error ("bad syntax in tuple");
    }
}

/* Parses: '[' elements ']'
   If modename is non-NULL it prefixed the tuple.  */

static void
parse_tuple (mode)
     struct type *mode;
{
  struct type *type;
  if (mode)
    type = check_typedef (mode);
  else
    type = 0;
  require ('[');
  start_arglist ();
  parse_opt_element_list (type);
  expect (']', "missing ']' after tuple");
  write_exp_elt_opcode (OP_ARRAY);
  write_exp_elt_longcst ((LONGEST) 0);
  write_exp_elt_longcst ((LONGEST) end_arglist () - 1);
  write_exp_elt_opcode (OP_ARRAY);
  if (type)
    {
      if (TYPE_CODE (type) != TYPE_CODE_ARRAY
	  && TYPE_CODE (type) != TYPE_CODE_STRUCT
	  && TYPE_CODE (type) != TYPE_CODE_SET)
	error ("invalid tuple mode");
      write_exp_elt_opcode (UNOP_CAST);
      write_exp_elt_type (mode);
      write_exp_elt_opcode (UNOP_CAST);
    }
}

static void
parse_primval ()
{
  struct type *type;
  enum exp_opcode op;
  char *op_name;
  switch (PEEK_TOKEN ())
    {
    case INTEGER_LITERAL: 
    case CHARACTER_LITERAL:
      write_exp_elt_opcode (OP_LONG);
      write_exp_elt_type (PEEK_LVAL ().typed_val.type);
      write_exp_elt_longcst (PEEK_LVAL ().typed_val.val);
      write_exp_elt_opcode (OP_LONG);
      FORWARD_TOKEN ();
      break;
    case BOOLEAN_LITERAL:
      write_exp_elt_opcode (OP_BOOL);
      write_exp_elt_longcst ((LONGEST) PEEK_LVAL ().ulval);
      write_exp_elt_opcode (OP_BOOL);
      FORWARD_TOKEN ();
      break;
    case FLOAT_LITERAL:
      write_exp_elt_opcode (OP_DOUBLE);
      write_exp_elt_type (builtin_type_double);
      write_exp_elt_dblcst (PEEK_LVAL ().dval);
      write_exp_elt_opcode (OP_DOUBLE);
      FORWARD_TOKEN ();
      break;
    case EMPTINESS_LITERAL:
      write_exp_elt_opcode (OP_LONG);
      write_exp_elt_type (lookup_pointer_type (builtin_type_void));
      write_exp_elt_longcst (0);
      write_exp_elt_opcode (OP_LONG);
      FORWARD_TOKEN ();
      break;
    case CHARACTER_STRING_LITERAL:
      write_exp_elt_opcode (OP_STRING);
      write_exp_string (PEEK_LVAL ().sval);
      write_exp_elt_opcode (OP_STRING);
      FORWARD_TOKEN ();
      break;
    case BIT_STRING_LITERAL:
      write_exp_elt_opcode (OP_BITSTRING);
      write_exp_bitstring (PEEK_LVAL ().sval);
      write_exp_elt_opcode (OP_BITSTRING);
      FORWARD_TOKEN ();
      break;
    case ARRAY:
      FORWARD_TOKEN ();
      /* This is pseudo-Chill, similar to C's '(TYPE[])EXPR'
	 which casts to an artificial array. */
      expect ('(', NULL);
      expect (')', NULL);
      if (PEEK_TOKEN () != TYPENAME)
	error ("missing MODENAME after ARRAY()");
      type = PEEK_LVAL().tsym.type;
      FORWARD_TOKEN ();
      expect ('(', NULL);
      parse_expr ();
      expect (')', "missing right parenthesis");
      type = create_array_type ((struct type *) NULL, type,
				create_range_type ((struct type *) NULL,
						   builtin_type_int, 0, 0));
      TYPE_ARRAY_UPPER_BOUND_TYPE(type) = BOUND_CANNOT_BE_DETERMINED;
      write_exp_elt_opcode (UNOP_CAST);
      write_exp_elt_type (type);
      write_exp_elt_opcode (UNOP_CAST);
      break;
#if 0
    case CONST:
    case EXPR:
      val = PEEK_LVAL();
      FORWARD_TOKEN ();
      break;
#endif
    case '(':
      FORWARD_TOKEN ();
      parse_expr ();
      expect (')', "missing right parenthesis");
      break;
    case '[':
      parse_tuple (NULL);
      break;
    case GENERAL_PROCEDURE_NAME:
    case LOCATION_NAME:
      write_exp_elt_opcode (OP_VAR_VALUE);
      write_exp_elt_block (NULL);
      write_exp_elt_sym (PEEK_LVAL ().ssym.sym);
      write_exp_elt_opcode (OP_VAR_VALUE);
      FORWARD_TOKEN ();
      break;
    case GDB_VARIABLE:	/* gdb specific */
      FORWARD_TOKEN ();
      break;
    case NUM:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_CAST);
      write_exp_elt_type (builtin_type_int);
      write_exp_elt_opcode (UNOP_CAST);
      break;
    case CARD:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_CARD);
      break;
    case MAX_TOKEN:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_CHMAX);
      break;
    case MIN_TOKEN:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_CHMIN);
      break;
    case PRED:      op_name = "PRED"; goto unimplemented_unary_builtin;
    case SUCC:      op_name = "SUCC"; goto unimplemented_unary_builtin;
    case ABS:       op_name = "ABS";  goto unimplemented_unary_builtin;
    unimplemented_unary_builtin:
      parse_unary_call ();
      error ("not implemented:  %s builtin function", op_name);
      break;
    case ADDR_TOKEN:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_ADDR);
      break;
    case SIZE:
      type = parse_mode_or_normal_call ();
      if (type)
	{ write_exp_elt_opcode (OP_LONG);
	  write_exp_elt_type (builtin_type_int);
	  CHECK_TYPEDEF (type);
	  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH (type));
	  write_exp_elt_opcode (OP_LONG);
	}
      else
	write_exp_elt_opcode (UNOP_SIZEOF);
      break;
    case LOWER:
      op = UNOP_LOWER;
      goto lower_upper;
    case UPPER:
      op = UNOP_UPPER;
      goto lower_upper;
    lower_upper:
      type = parse_mode_or_normal_call ();
      write_lower_upper_value (op, type);
      break;
    case LENGTH:
      parse_unary_call ();
      write_exp_elt_opcode (UNOP_LENGTH);
      break;
    case TYPENAME:
      type = PEEK_LVAL ().tsym.type;
      FORWARD_TOKEN ();
      switch (PEEK_TOKEN())
	{
	case '[':
	  parse_tuple (type);
	  break;
	case '(':
	  FORWARD_TOKEN ();
	  parse_expr ();
	  expect (')', "missing right parenthesis");
	  write_exp_elt_opcode (UNOP_CAST);
	  write_exp_elt_type (type);
	  write_exp_elt_opcode (UNOP_CAST);
	  break;
	default:
	  error ("typename in invalid context");
	}
      break;
      
    default: 
      error ("invalid expression syntax at `%s'", lexptr);
    }
  for (;;)
    {
      switch (PEEK_TOKEN ())
	{
	case DOT_FIELD_NAME:
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  write_exp_string (PEEK_LVAL ().sval);
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  FORWARD_TOKEN ();
	  continue;
	case POINTER:
	  FORWARD_TOKEN ();
	  if (PEEK_TOKEN () == TYPENAME)
	    {
	      type = PEEK_LVAL ().tsym.type;
	      write_exp_elt_opcode (UNOP_CAST);
	      write_exp_elt_type (lookup_pointer_type (type));
	      write_exp_elt_opcode (UNOP_CAST);
	      FORWARD_TOKEN ();
	    }
	  write_exp_elt_opcode (UNOP_IND);
	  continue;
	case OPEN_PAREN:
	  parse_call ();
	  continue;
	case CHARACTER_STRING_LITERAL:
	case CHARACTER_LITERAL:
	case BIT_STRING_LITERAL:
	  /* Handle string repetition. (See comment in parse_operand5.) */
	  parse_primval ();
	  write_exp_elt_opcode (MULTI_SUBSCRIPT);
	  write_exp_elt_longcst (1);
	  write_exp_elt_opcode (MULTI_SUBSCRIPT);
	  continue;
	case END_TOKEN:
	case TOKEN_NOT_READ:
	case INTEGER_LITERAL:
	case BOOLEAN_LITERAL:
	case FLOAT_LITERAL:
	case GENERAL_PROCEDURE_NAME:
	case LOCATION_NAME:
	case EMPTINESS_LITERAL:
	case TYPENAME:
	case CASE:
	case OF:
	case ESAC:
	case LOGIOR:
	case ORIF:
	case LOGXOR:
	case LOGAND:
	case ANDIF:
	case NOTEQUAL:
	case GEQ:
	case LEQ:
	case IN:
	case SLASH_SLASH:
	case MOD:
	case REM:
	case NOT:
	case RECEIVE:
	case UP:
	case IF:
	case THEN:
	case ELSE:
	case FI:
	case ELSIF:
	case ILLEGAL_TOKEN:
	case NUM:
	case PRED:
	case SUCC:
	case ABS:
	case CARD:
	case MAX_TOKEN:
	case MIN_TOKEN:
	case ADDR_TOKEN:
	case SIZE:
	case UPPER:
	case LOWER:
	case LENGTH:
	case ARRAY:
	case GDB_VARIABLE:
	case GDB_ASSIGNMENT:
	  break;
	}
      break;
    }
  return;
}

static void
parse_operand6 ()
{
  if (check_token (RECEIVE))
    {
      parse_primval ();
      error ("not implemented:  RECEIVE expression");
    }
  else if (check_token (POINTER))
    {
      parse_primval ();
      write_exp_elt_opcode (UNOP_ADDR);
    }
  else
    parse_primval();
}

static void
parse_operand5()
{
  enum exp_opcode op;
  /* We are supposed to be looking for a <string repetition operator>,
     but in general we can't distinguish that from a parenthesized
     expression.  This is especially difficult if we allow the
     string operand to be a constant expression (as requested by
     some users), and not just a string literal.
     Consider:  LPRN expr RPRN LPRN expr RPRN
     Is that a function call or string repetition?
     Instead, we handle string repetition in parse_primval,
     and build_generalized_call. */
  switch (PEEK_TOKEN())
    {
    case NOT:  op = UNOP_LOGICAL_NOT; break;
    case '-':  op = UNOP_NEG; break;
    default:
      op = OP_NULL;
    }
  if (op != OP_NULL)
    FORWARD_TOKEN();
  parse_operand6();
  if (op != OP_NULL)
    write_exp_elt_opcode (op);
}

static void
parse_operand4 ()
{
  enum exp_opcode op;
  parse_operand5();
  for (;;)
    {
      switch (PEEK_TOKEN())
	{
	case '*':  op = BINOP_MUL; break;
	case '/':  op = BINOP_DIV; break;
	case MOD:  op = BINOP_MOD; break;
	case REM:  op = BINOP_REM; break;
	default:
	  return;
	}
      FORWARD_TOKEN();
      parse_operand5();
      write_exp_elt_opcode (op);
    }
}

static void
parse_operand3 ()
{
  enum exp_opcode op;
  parse_operand4 ();
  for (;;)
    {
      switch (PEEK_TOKEN())
	{
	case '+':    op = BINOP_ADD; break;
	case '-':    op = BINOP_SUB; break;
	case SLASH_SLASH: op = BINOP_CONCAT; break;
	default:
	  return;
	}
      FORWARD_TOKEN();
      parse_operand4();
      write_exp_elt_opcode (op);
    }
}

static void
parse_operand2 ()
{
  enum exp_opcode op;
  parse_operand3 ();
  for (;;)
    {
      if (check_token (IN))
	{
	  parse_operand3();
	  write_exp_elt_opcode (BINOP_IN);
	}
      else
	{
	  switch (PEEK_TOKEN())
	    {
	    case '>':      op = BINOP_GTR; break;
	    case GEQ:      op = BINOP_GEQ; break;
	    case '<':      op = BINOP_LESS; break;
	    case LEQ:      op = BINOP_LEQ; break;
	    case '=':      op = BINOP_EQUAL; break;
	    case NOTEQUAL: op = BINOP_NOTEQUAL; break;
	    default:
	      return;
	    }
	  FORWARD_TOKEN();
	  parse_operand3();
	  write_exp_elt_opcode (op);
	}
    }
}

static void
parse_operand1 ()
{
  enum exp_opcode op;
  parse_operand2 ();
  for (;;)
    {
      switch (PEEK_TOKEN())
	{
	case LOGAND: op = BINOP_BITWISE_AND; break;
	case ANDIF:  op = BINOP_LOGICAL_AND; break;
	default:
	  return;
	}
      FORWARD_TOKEN();
      parse_operand2();
      write_exp_elt_opcode (op);
    }
}

static void
parse_operand0 ()
{ 
  enum exp_opcode op;
  parse_operand1();
  for (;;)
    {
      switch (PEEK_TOKEN())
	{
	case LOGIOR:  op = BINOP_BITWISE_IOR; break;
	case LOGXOR:  op = BINOP_BITWISE_XOR; break;
	case ORIF:    op = BINOP_LOGICAL_OR; break;
	default:
	  return;
	}
      FORWARD_TOKEN();
      parse_operand1();
      write_exp_elt_opcode (op);
    }
}

static void
parse_expr ()
{
  parse_operand0 ();
  if (check_token (GDB_ASSIGNMENT))
    {
      parse_expr ();
      write_exp_elt_opcode (BINOP_ASSIGN);
    }
}

static void
parse_then_alternative ()
{
  expect (THEN, "missing 'THEN' in 'IF' expression");
  parse_expr ();
}

static void
parse_else_alternative ()
{
  if (check_token (ELSIF))
    parse_if_expression_body ();
  else if (check_token (ELSE))
    parse_expr ();
  else
    error ("missing ELSE/ELSIF in IF expression");
}

/* Matches: <boolean expression> <then alternative> <else alternative> */

static void
parse_if_expression_body ()
{
  parse_expr ();
  parse_then_alternative ();
  parse_else_alternative ();
  write_exp_elt_opcode (TERNOP_COND);
}

static void
parse_if_expression ()
{
  require (IF);
  parse_if_expression_body ();
  expect (FI, "missing 'FI' at end of conditional expression");
}

/* An <untyped_expr> is a superset of <expr>.  It also includes
   <conditional expressions> and untyped <tuples>, whose types
   are not given by their constituents.  Hence, these are only
   allowed in certain contexts that expect a certain type.
   You should call convert() to fix up the <untyped_expr>. */

static void
parse_untyped_expr ()
{
  switch (PEEK_TOKEN())
    {
    case IF:
      parse_if_expression ();
      return;
    case CASE:
      error ("not implemented:  CASE expression");
    case '(':
      switch (PEEK_TOKEN1())
	{
	case IF:
	case CASE:
	  goto skip_lprn;
	case '[':
	skip_lprn:
	  FORWARD_TOKEN ();
	  parse_untyped_expr ();
	  expect (')', "missing ')'");
	  return;
	default: ;
	  /* fall through */
	}
    default:
      parse_operand0 ();
    }
}

int
chill_parse ()
{
  terminal_buffer[0] = TOKEN_NOT_READ;
  if (PEEK_TOKEN () == TYPENAME && PEEK_TOKEN1 () == END_TOKEN)
    {
      write_exp_elt_opcode(OP_TYPE);
      write_exp_elt_type(PEEK_LVAL ().tsym.type);
      write_exp_elt_opcode(OP_TYPE);
      FORWARD_TOKEN ();
    }
  else
    parse_expr ();
  if (terminal_buffer[0] != END_TOKEN)
    {
      if (comma_terminates && terminal_buffer[0] == ',')
	lexptr--;  /* Put the comma back.  */
      else
	error ("Junk after end of expression.");
    }
  return 0;
}


/* Implementation of a dynamically expandable buffer for processing input
   characters acquired through lexptr and building a value to return in
   yylval. */

static char *tempbuf;		/* Current buffer contents */
static int tempbufsize;		/* Size of allocated buffer */
static int tempbufindex;	/* Current index into buffer */

#define GROWBY_MIN_SIZE 64	/* Minimum amount to grow buffer by */

#define CHECKBUF(size) \
  do { \
    if (tempbufindex + (size) >= tempbufsize) \
      { \
	growbuf_by_size (size); \
      } \
  } while (0);

/* Grow the static temp buffer if necessary, including allocating the first one
   on demand. */

static void
growbuf_by_size (count)
     int count;
{
  int growby;

  growby = max (count, GROWBY_MIN_SIZE);
  tempbufsize += growby;
  if (tempbuf == NULL)
    {
      tempbuf = (char *) xmalloc (tempbufsize);
    }
  else
    {
      tempbuf = (char *) xrealloc (tempbuf, tempbufsize);
    }
}

/* Try to consume a simple name string token.  If successful, returns
   a pointer to a nullbyte terminated copy of the name that can be used
   in symbol table lookups.  If not successful, returns NULL. */

static char *
match_simple_name_string ()
{
  char *tokptr = lexptr;

  if (isalpha (*tokptr) || *tokptr == '_')
    {
      char *result;
      do {
	tokptr++;
      } while (isalnum (*tokptr) || (*tokptr == '_'));
      yylval.sval.ptr = lexptr;
      yylval.sval.length = tokptr - lexptr;
      lexptr = tokptr;
      result = copy_name (yylval.sval);
      return result;
    }
  return (NULL);
}

/* Start looking for a value composed of valid digits as set by the base
   in use.  Note that '_' characters are valid anywhere, in any quantity,
   and are simply ignored.  Since we must find at least one valid digit,
   or reject this token as an integer literal, we keep track of how many
   digits we have encountered. */
  
static int
decode_integer_value (base, tokptrptr, ivalptr)
  int base;
  char **tokptrptr;
  LONGEST *ivalptr;
{
  char *tokptr = *tokptrptr;
  int temp;
  int digits = 0;

  while (*tokptr != '\0')
    {
      temp = *tokptr;
      if (isupper (temp))
        temp = tolower (temp);
      tokptr++;
      switch (temp)
	{
	case '_':
	  continue;
	case '0':  case '1':  case '2':  case '3':  case '4':
	case '5':  case '6':  case '7':  case '8':  case '9':
	  temp -= '0';
	  break;
	case 'a':  case 'b':  case 'c':  case 'd':  case 'e': case 'f':
	  temp -= 'a';
	  temp += 10;
	  break;
	default:
	  temp = base;
	  break;
	}
      if (temp < base)
	{
	  digits++;
	  *ivalptr *= base;
	  *ivalptr += temp;
	}
      else
	{
	  /* Found something not in domain for current base. */
	  tokptr--;	/* Unconsume what gave us indigestion. */
	  break;
	}
    }
  
  /* If we didn't find any digits, then we don't have a valid integer
     value, so reject the entire token.  Otherwise, update the lexical
     scan pointer, and return non-zero for success. */
  
  if (digits == 0)
    {
      return (0);
    }
  else
    {
      *tokptrptr = tokptr;
      return (1);
    }
}

static int
decode_integer_literal (valptr, tokptrptr)
  LONGEST *valptr;
  char **tokptrptr;
{
  char *tokptr = *tokptrptr;
  int base = 0;
  LONGEST ival = 0;
  int explicit_base = 0;
  
  /* Look for an explicit base specifier, which is optional. */
  
  switch (*tokptr)
    {
    case 'd':
    case 'D':
      explicit_base++;
      base = 10;
      tokptr++;
      break;
    case 'b':
    case 'B':
      explicit_base++;
      base = 2;
      tokptr++;
      break;
    case 'h':
    case 'H':
      explicit_base++;
      base = 16;
      tokptr++;
      break;
    case 'o':
    case 'O':
      explicit_base++;
      base = 8;
      tokptr++;
      break;
    default:
      base = 10;
      break;
    }
  
  /* If we found an explicit base ensure that the character after the
     explicit base is a single quote. */
  
  if (explicit_base && (*tokptr++ != '\''))
    {
      return (0);
    }
  
  /* Attempt to decode whatever follows as an integer value in the
     indicated base, updating the token pointer in the process and
     computing the value into ival.  Also, if we have an explicit
     base, then the next character must not be a single quote, or we
     have a bitstring literal, so reject the entire token in this case.
     Otherwise, update the lexical scan pointer, and return non-zero
     for success. */

  if (!decode_integer_value (base, &tokptr, &ival))
    {
      return (0);
    }
  else if (explicit_base && (*tokptr == '\''))
    {
      return (0);
    }
  else
    {
      *valptr = ival;
      *tokptrptr = tokptr;
      return (1);
    }
}

/*  If it wasn't for the fact that floating point values can contain '_'
    characters, we could just let strtod do all the hard work by letting it
    try to consume as much of the current token buffer as possible and
    find a legal conversion.  Unfortunately we need to filter out the '_'
    characters before calling strtod, which we do by copying the other
    legal chars to a local buffer to be converted.  However since we also
    need to keep track of where the last unconsumed character in the input
    buffer is, we have transfer only as many characters as may compose a
    legal floating point value. */
    
static enum ch_terminal
match_float_literal ()
{
  char *tokptr = lexptr;
  char *buf;
  char *copy;
  double dval;
  extern double strtod ();
  
  /* Make local buffer in which to build the string to convert.  This is
     required because underscores are valid in chill floating point numbers
     but not in the string passed to strtod to convert.  The string will be
     no longer than our input string. */
     
  copy = buf = (char *) alloca (strlen (tokptr) + 1);

  /* Transfer all leading digits to the conversion buffer, discarding any
     underscores. */

  while (isdigit (*tokptr) || *tokptr == '_')
    {
      if (*tokptr != '_')
	{
	  *copy++ = *tokptr;
	}
      tokptr++;
    }

  /* Now accept either a '.', or one of [eEdD].  Dot is legal regardless
     of whether we found any leading digits, and we simply accept it and
     continue on to look for the fractional part and/or exponent.  One of
     [eEdD] is legal only if we have seen digits, and means that there
     is no fractional part.  If we find neither of these, then this is
     not a floating point number, so return failure. */

  switch (*tokptr++)
    {
      case '.':
        /* Accept and then look for fractional part and/or exponent. */
	*copy++ = '.';
	break;

      case 'e':
      case 'E':
      case 'd':
      case 'D':
	if (copy == buf)
	  {
	    return (0);
	  }
	*copy++ = 'e';
	goto collect_exponent;
	break;

      default:
	return (0);
        break;
    }

  /* We found a '.', copy any fractional digits to the conversion buffer, up
     to the first nondigit, non-underscore character. */

  while (isdigit (*tokptr) || *tokptr == '_')
    {
      if (*tokptr != '_')
	{
	  *copy++ = *tokptr;
	}
      tokptr++;
    }

  /* Look for an exponent, which must start with one of [eEdD].  If none
     is found, jump directly to trying to convert what we have collected
     so far. */

  switch (*tokptr)
    {
      case 'e':
      case 'E':
      case 'd':
      case 'D':
	*copy++ = 'e';
	tokptr++;
	break;
      default:
	goto convert_float;
	break;
    }

  /* Accept an optional '-' or '+' following one of [eEdD]. */

  collect_exponent:
  if (*tokptr == '+' || *tokptr == '-')
    {
      *copy++ = *tokptr++;
    }

  /* Now copy an exponent into the conversion buffer.  Note that at the 
     moment underscores are *not* allowed in exponents. */

  while (isdigit (*tokptr))
    {
      *copy++ = *tokptr++;
    }

  /* If we transfered any chars to the conversion buffer, try to interpret its
     contents as a floating point value.  If any characters remain, then we
     must not have a valid floating point string. */

  convert_float:
  *copy = '\0';
  if (copy != buf)
      {
        dval = strtod (buf, &copy);
        if (*copy == '\0')
	  {
	    yylval.dval = dval;
	    lexptr = tokptr;
	    return (FLOAT_LITERAL);
	  }
      }
  return (0);
}

/* Recognize a string literal.  A string literal is a sequence
   of characters enclosed in matching single or double quotes, except that
   a single character inside single quotes is a character literal, which
   we reject as a string literal.  To embed the terminator character inside
   a string, it is simply doubled (I.E. "this""is""one""string") */

static enum ch_terminal
match_string_literal ()
{
  char *tokptr = lexptr;
  int in_ctrlseq = 0;
  LONGEST ival;

  for (tempbufindex = 0, tokptr++; *tokptr != '\0'; tokptr++)
    {
      CHECKBUF (1);
    tryagain: ;
      if (in_ctrlseq)
	{
	  /* skip possible whitespaces */
	  while ((*tokptr == ' ' || *tokptr == '\t') && *tokptr)
	    tokptr++;
	  if (*tokptr == ')')
	    {
	      in_ctrlseq = 0;
	      tokptr++;
	      goto tryagain;
	    }
	  else if (*tokptr != ',')
	    error ("Invalid control sequence");
	  tokptr++;
	  /* skip possible whitespaces */
	  while ((*tokptr == ' ' || *tokptr == '\t') && *tokptr)
	    tokptr++;
	  if (!decode_integer_literal (&ival, &tokptr))
	    error ("Invalid control sequence");
	  tokptr--;
	}
      else if (*tokptr == *lexptr)
	{
	  if (*(tokptr + 1) == *lexptr)
	    {
	      ival = *tokptr++;
	    }
	  else
	    {
	      break;
	    }
	}
      else if (*tokptr == '^')
	{
	  if (*(tokptr + 1) == '(')
	    {
	      in_ctrlseq = 1;
	      tokptr += 2;
	      if (!decode_integer_literal (&ival, &tokptr))
		error ("Invalid control sequence");
	      tokptr--;
	    }
	  else if (*(tokptr + 1) == '^')
	    ival = *tokptr++;
	  else
	    error ("Invalid control sequence");
	}
      else
	ival = *tokptr;
      tempbuf[tempbufindex++] = ival;
    }
  if (in_ctrlseq)
    error ("Invalid control sequence");

  if (*tokptr == '\0'					/* no terminator */
      || (tempbufindex == 1 && *tokptr == '\''))	/* char literal */
    {
      return (0);
    }
  else
    {
      tempbuf[tempbufindex] = '\0';
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = ++tokptr;
      return (CHARACTER_STRING_LITERAL);
    }
}

/* Recognize a character literal.  A character literal is single character
   or a control sequence, enclosed in single quotes.  A control sequence
   is a comma separated list of one or more integer literals, enclosed
   in parenthesis and introduced with a circumflex character.

   EX:  'a'  '^(7)'  '^(7,8)'

   As a GNU chill extension, the syntax C'xx' is also recognized as a 
   character literal, where xx is a hex value for the character.

   Note that more than a single character, enclosed in single quotes, is
   a string literal.

   Returns CHARACTER_LITERAL if a match is found.
   */

static enum ch_terminal
match_character_literal ()
{
  char *tokptr = lexptr;
  LONGEST ival = 0;
  
  if ((*tokptr == 'c' || *tokptr == 'C') && (*(tokptr + 1) == '\''))
    {
      /* We have a GNU chill extension form, so skip the leading "C'",
	 decode the hex value, and then ensure that we have a trailing
	 single quote character. */
      tokptr += 2;
      if (!decode_integer_value (16, &tokptr, &ival) || (*tokptr != '\''))
	{
	  return (0);
	}
      tokptr++;
    }
  else if (*tokptr == '\'')
    {
      tokptr++;

      /* Determine which form we have, either a control sequence or the
	 single character form. */
      
      if (*tokptr == '^')
	{
	  if (*(tokptr + 1) == '(')
	    {
	      /* Match and decode a control sequence.  Return zero if we don't
		 find a valid integer literal, or if the next unconsumed character
		 after the integer literal is not the trailing ')'. */
	      tokptr += 2;
	      if (!decode_integer_literal (&ival, &tokptr) || (*tokptr++ != ')'))
		{
		  return (0);
		}
	    }
	  else if (*(tokptr + 1) == '^')
	    {
	      ival = *tokptr;
	      tokptr += 2;
	    }
	  else
	    /* fail */
	    error ("Invalid control sequence");
	}
      else if (*tokptr == '\'')
	{
	  /* this must be duplicated */
	  ival = *tokptr;
	  tokptr += 2;
	}
      else
	{
	  ival = *tokptr++;
	}

      /* The trailing quote has not yet been consumed.  If we don't find
	 it, then we have no match. */
      
      if (*tokptr++ != '\'')
	{
	  return (0);
	}
    }
  else
    {
      /* Not a character literal. */
      return (0);
    }
  yylval.typed_val.val = ival;
  yylval.typed_val.type = builtin_type_chill_char;
  lexptr = tokptr;
  return (CHARACTER_LITERAL);
}

/* Recognize an integer literal, as specified in Z.200 sec 5.2.4.2.
   Note that according to 5.2.4.2, a single "_" is also a valid integer
   literal, however GNU-chill requires there to be at least one "digit"
   in any integer literal. */

static enum ch_terminal
match_integer_literal ()
{
  char *tokptr = lexptr;
  LONGEST ival;
  
  if (!decode_integer_literal (&ival, &tokptr))
    {
      return (0);
    }
  else 
    {
      yylval.typed_val.val = ival;
#if defined(CC_HAS_LONG_LONG) && defined(__STDC__)
      if (ival > (LONGEST)2147483647U || ival < -(LONGEST)2147483648U)
	yylval.typed_val.type = builtin_type_long_long;
      else
#endif
	yylval.typed_val.type = builtin_type_int;
      lexptr = tokptr;
      return (INTEGER_LITERAL);
    }
}

/* Recognize a bit-string literal, as specified in Z.200 sec 5.2.4.8
   Note that according to 5.2.4.8, a single "_" is also a valid bit-string
   literal, however GNU-chill requires there to be at least one "digit"
   in any bit-string literal. */

static enum ch_terminal
match_bitstring_literal ()
{
  register char *tokptr = lexptr;
  int bitoffset = 0;
  int bitcount = 0;
  int bits_per_char;
  int digit;
  
  tempbufindex = 0;
  CHECKBUF (1);
  tempbuf[0] = 0;

  /* Look for the required explicit base specifier. */
  
  switch (*tokptr++)
    {
    case 'b':
    case 'B':
      bits_per_char = 1;
      break;
    case 'o':
    case 'O':
      bits_per_char = 3;
      break;
    case 'h':
    case 'H':
      bits_per_char = 4;
      break;
    default:
      return (0);
      break;
    }

  /* Ensure that the character after the explicit base is a single quote. */
  
  if (*tokptr++ != '\'')
    {
      return (0);
    }
  
  while (*tokptr != '\0' && *tokptr != '\'')
    {
      digit = *tokptr;
      if (isupper (digit))
        digit = tolower (digit);
      tokptr++;
      switch (digit)
	{
	  case '_':
	    continue;
	  case '0':  case '1':  case '2':  case '3':  case '4':
	  case '5':  case '6':  case '7':  case '8':  case '9':
	    digit -= '0';
	    break;
	  case 'a':  case 'b':  case 'c':  case 'd':  case 'e': case 'f':
	    digit -= 'a';
	    digit += 10;
	    break;
	  default:
	    /* this is not a bitstring literal, probably an integer */
	    return 0;
	}
      if (digit >= 1 << bits_per_char)
	{
	  /* Found something not in domain for current base. */
	  error ("Too-large digit in bitstring or integer.");
	}
      else
	{
	  /* Extract bits from digit, packing them into the bitstring byte. */
	  int k = TARGET_BYTE_ORDER == BIG_ENDIAN ? bits_per_char - 1 : 0;
	  for (; TARGET_BYTE_ORDER == BIG_ENDIAN ? k >= 0 : k < bits_per_char;
	       TARGET_BYTE_ORDER == BIG_ENDIAN ? k-- : k++)
	    {
	      bitcount++;
	      if (digit & (1 << k))
		{
		  tempbuf[tempbufindex] |=
		    (TARGET_BYTE_ORDER == BIG_ENDIAN)
		      ? (1 << (HOST_CHAR_BIT - 1 - bitoffset))
			: (1 << bitoffset);
		}
	      bitoffset++;
	      if (bitoffset == HOST_CHAR_BIT)
		{
		  bitoffset = 0;
		  tempbufindex++;
		  CHECKBUF(1);
		  tempbuf[tempbufindex] = 0;
		}
	    }
	}
    }
  
  /* Verify that we consumed everything up to the trailing single quote,
     and that we found some bits (IE not just underbars). */

  if (*tokptr++ != '\'')
    {
      return (0);
    }
  else 
    {
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = bitcount;
      lexptr = tokptr;
      return (BIT_STRING_LITERAL);
    }
}

struct token
{
  char *operator;
  int token;
};

static const struct token idtokentab[] =
{
    { "array", ARRAY },
    { "length", LENGTH },
    { "lower", LOWER },
    { "upper", UPPER },
    { "andif", ANDIF },
    { "pred", PRED },
    { "succ", SUCC },
    { "card", CARD },
    { "size", SIZE },
    { "orif", ORIF },
    { "num", NUM },
    { "abs", ABS },
    { "max", MAX_TOKEN },
    { "min", MIN_TOKEN },
    { "mod", MOD },
    { "rem", REM },
    { "not", NOT },
    { "xor", LOGXOR },
    { "and", LOGAND },
    { "in", IN },
    { "or", LOGIOR },
    { "up", UP },
    { "addr", ADDR_TOKEN },
    { "null", EMPTINESS_LITERAL }
};

static const struct token tokentab2[] =
{
    { ":=", GDB_ASSIGNMENT },
    { "//", SLASH_SLASH },
    { "->", POINTER },
    { "/=", NOTEQUAL },
    { "<=", LEQ },
    { ">=", GEQ }
};

/* Read one token, getting characters through lexptr.  */
/* This is where we will check to make sure that the language and the
   operators used are compatible.  */

static enum ch_terminal
ch_lex ()
{
    unsigned int i;
    enum ch_terminal token;
    char *inputname;
    struct symbol *sym;

    /* Skip over any leading whitespace. */
    while (isspace (*lexptr))
	{
	    lexptr++;
	}
    /* Look for special single character cases which can't be the first
       character of some other multicharacter token. */
    switch (*lexptr)
	{
	    case '\0':
	        return END_TOKEN;
	    case ',':
	    case '=':
	    case ';':
	    case '!':
	    case '+':
	    case '*':
	    case '(':
	    case ')':
	    case '[':
	    case ']':
		return (*lexptr++);
	}
    /* Look for characters which start a particular kind of multicharacter
       token, such as a character literal, register name, convenience
       variable name, string literal, etc. */
    switch (*lexptr)
      {
	case '\'':
	case '\"':
	  /* First try to match a string literal, which is any
	     sequence of characters enclosed in matching single or double
	     quotes, except that a single character inside single quotes
	     is a character literal, so we have to catch that case also. */
	  token = match_string_literal ();
	  if (token != 0)
	    {
	      return (token);
	    }
	  if (*lexptr == '\'')
	    {
	      token = match_character_literal ();
	      if (token != 0)
		{
		  return (token);
		}
	    }
	  break;
        case 'C':
        case 'c':
	  token = match_character_literal ();
	  if (token != 0)
	    {
	      return (token);
	    }
	  break;
	case '$':
	  yylval.sval.ptr = lexptr;
	  do {
	    lexptr++;
	  } while (isalnum (*lexptr) || *lexptr == '_' || *lexptr == '$');
	  yylval.sval.length = lexptr - yylval.sval.ptr;
	  write_dollar_variable (yylval.sval);
	  return GDB_VARIABLE;
	  break;
      }
    /* See if it is a special token of length 2.  */
    for (i = 0; i < sizeof (tokentab2) / sizeof (tokentab2[0]); i++)
	{
	    if (STREQN (lexptr, tokentab2[i].operator, 2))
		{
		    lexptr += 2;
		    return (tokentab2[i].token);
		}
	}
    /* Look for single character cases which which could be the first
       character of some other multicharacter token, but aren't, or we
       would already have found it. */
    switch (*lexptr)
	{
	    case '-':
	    case ':':
	    case '/':
	    case '<':
	    case '>':
		return (*lexptr++);
	}
    /* Look for a float literal before looking for an integer literal, so
       we match as much of the input stream as possible. */
    token = match_float_literal ();
    if (token != 0)
	{
	    return (token);
	}
    token = match_bitstring_literal ();
    if (token != 0)
	{
	    return (token);
	}
    token = match_integer_literal ();
    if (token != 0)
	{
	    return (token);
	}

    /* Try to match a simple name string, and if a match is found, then
       further classify what sort of name it is and return an appropriate
       token.  Note that attempting to match a simple name string consumes
       the token from lexptr, so we can't back out if we later find that
       we can't classify what sort of name it is. */

    inputname = match_simple_name_string ();

    if (inputname != NULL)
      {
	char *simplename = (char*) alloca (strlen (inputname) + 1);

	char *dptr = simplename, *sptr = inputname;
	for (; *sptr; sptr++)
	  *dptr++ = isupper (*sptr) ? tolower(*sptr) : *sptr;
	*dptr = '\0';

	/* See if it is a reserved identifier. */
	for (i = 0; i < sizeof (idtokentab) / sizeof (idtokentab[0]); i++)
	    {
		if (STREQ (simplename, idtokentab[i].operator))
		    {
			return (idtokentab[i].token);
		    }
	    }

	/* Look for other special tokens. */
	if (STREQ (simplename, "true"))
	    {
		yylval.ulval = 1;
		return (BOOLEAN_LITERAL);
	    }
	if (STREQ (simplename, "false"))
	    {
		yylval.ulval = 0;
		return (BOOLEAN_LITERAL);
	    }

	sym = lookup_symbol (inputname, expression_context_block,
			     VAR_NAMESPACE, (int *) NULL,
			     (struct symtab **) NULL);
	if (sym == NULL && strcmp (inputname, simplename) != 0)
	  {
	    sym = lookup_symbol (simplename, expression_context_block,
				 VAR_NAMESPACE, (int *) NULL,
				 (struct symtab **) NULL);
	  }
	if (sym != NULL)
	  {
	    yylval.ssym.stoken.ptr = NULL;
	    yylval.ssym.stoken.length = 0;
	    yylval.ssym.sym = sym;
	    yylval.ssym.is_a_field_of_this = 0;	/* FIXME, C++'ism */
	    switch (SYMBOL_CLASS (sym))
	      {
	      case LOC_BLOCK:
		/* Found a procedure name. */
		return (GENERAL_PROCEDURE_NAME);
	      case LOC_STATIC:
		/* Found a global or local static variable. */
		return (LOCATION_NAME);
	      case LOC_REGISTER:
	      case LOC_ARG:
	      case LOC_REF_ARG:
	      case LOC_REGPARM:
	      case LOC_REGPARM_ADDR:
	      case LOC_LOCAL:
	      case LOC_LOCAL_ARG:
	      case LOC_BASEREG:
	      case LOC_BASEREG_ARG:
		if (innermost_block == NULL
		    || contained_in (block_found, innermost_block))
		  {
		    innermost_block = block_found;
		  }
		return (LOCATION_NAME);
		break;
	      case LOC_CONST:
	      case LOC_LABEL:
		return (LOCATION_NAME);
		break;
	      case LOC_TYPEDEF:
		yylval.tsym.type = SYMBOL_TYPE (sym);
		return TYPENAME;
	      case LOC_UNDEF:
	      case LOC_CONST_BYTES:
	      case LOC_OPTIMIZED_OUT:
		error ("Symbol \"%s\" names no location.", inputname);
		break;
	      case LOC_UNRESOLVED:
		error ("unhandled SYMBOL_CLASS in ch_lex()");
		break;
	      }
	  }
	else if (!have_full_symbols () && !have_partial_symbols ())
	  {
	    error ("No symbol table is loaded.  Use the \"file\" command.");
	  }
	else
	  {
	    error ("No symbol \"%s\" in current context.", inputname);
	  }
      }

    /* Catch single character tokens which are not part of some
       longer token. */

    switch (*lexptr)
      {
	case '.':			/* Not float for example. */
	  lexptr++;
	  while (isspace (*lexptr)) lexptr++;
	  inputname = match_simple_name_string ();
	  if (!inputname)
	    return '.';
	  return DOT_FIELD_NAME;
      }

    return (ILLEGAL_TOKEN);
}

static void
write_lower_upper_value (opcode, type)
     enum exp_opcode opcode;  /* Either UNOP_LOWER or UNOP_UPPER */
     struct type *type;
{
  if (type == NULL)
    write_exp_elt_opcode (opcode);
  else
    {
      struct type *result_type;
      LONGEST val = type_lower_upper (opcode, type, &result_type);
      write_exp_elt_opcode (OP_LONG);
      write_exp_elt_type (result_type);
      write_exp_elt_longcst (val);
      write_exp_elt_opcode (OP_LONG);
    }
}

void
chill_error (msg)
     char *msg;
{
  /* Never used. */
}
