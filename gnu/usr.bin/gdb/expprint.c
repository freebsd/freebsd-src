/* Print in infix form a struct expression.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "defs.h"
#include "symtab.h"
#include "param.h"
#include "expression.h"
#include "value.h"


/* These codes indicate operator precedences, least tightly binding first.  */
/* Adding 1 to a precedence value is done for binary operators,
   on the operand which is more tightly bound, so that operators
   of equal precedence within that operand will get parentheses.  */
/* PREC_HYPER and PREC_ABOVE_COMMA are not the precedence of any operator;
   they are used as the "surrounding precedence" to force
   various kinds of things to be parenthesized.  */
enum precedence
{ PREC_NULL, PREC_COMMA, PREC_ABOVE_COMMA, PREC_ASSIGN, PREC_OR, PREC_AND,
  PREC_LOGIOR, PREC_LOGAND, PREC_LOGXOR, PREC_EQUAL, PREC_ORDER,
  PREC_SHIFT, PREC_ADD, PREC_MUL, PREC_REPEAT,
  PREC_HYPER, PREC_PREFIX, PREC_SUFFIX };

/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

struct op_print
{
  char *string;
  enum exp_opcode opcode;
  /* Precedence of operator.  These values are used only by comparisons.  */
  enum precedence precedence;
  int right_assoc;
};

static struct op_print op_print_tab[] =
  {
    {",", BINOP_COMMA, PREC_COMMA, 0},
    {"=", BINOP_ASSIGN, PREC_ASSIGN, 1},
    {"||", BINOP_OR, PREC_OR, 0},
    {"&&", BINOP_AND, PREC_AND, 0},
    {"|", BINOP_LOGIOR, PREC_LOGIOR, 0},
    {"&", BINOP_LOGAND, PREC_LOGAND, 0},
    {"^", BINOP_LOGXOR, PREC_LOGXOR, 0},
    {"==", BINOP_EQUAL, PREC_EQUAL, 0},
    {"!=", BINOP_NOTEQUAL, PREC_EQUAL, 0},
    {"<=", BINOP_LEQ, PREC_ORDER, 0},
    {">=", BINOP_GEQ, PREC_ORDER, 0},
    {">", BINOP_GTR, PREC_ORDER, 0},
    {"<", BINOP_LESS, PREC_ORDER, 0},
    {">>", BINOP_RSH, PREC_SHIFT, 0},
    {"<<", BINOP_LSH, PREC_SHIFT, 0},
    {"+", BINOP_ADD, PREC_ADD, 0},
    {"-", BINOP_SUB, PREC_ADD, 0},
    {"*", BINOP_MUL, PREC_MUL, 0},
    {"/", BINOP_DIV, PREC_MUL, 0},
    {"%", BINOP_REM, PREC_MUL, 0},
    {"@", BINOP_REPEAT, PREC_REPEAT, 0},
    {"-", UNOP_NEG, PREC_PREFIX, 0},
    {"!", UNOP_ZEROP, PREC_PREFIX, 0},
    {"~", UNOP_LOGNOT, PREC_PREFIX, 0},
    {"*", UNOP_IND, PREC_PREFIX, 0},
    {"&", UNOP_ADDR, PREC_PREFIX, 0},
    {"sizeof ", UNOP_SIZEOF, PREC_PREFIX, 0},
    {"++", UNOP_PREINCREMENT, PREC_PREFIX, 0},
    {"--", UNOP_PREDECREMENT, PREC_PREFIX, 0},
    /* C++  */
    {"::", BINOP_SCOPE, PREC_PREFIX, 0},
  };

static void print_subexp ();

void
print_expression (exp, stream)
     struct expression *exp;
     FILE *stream;
{
  int pc = 0;
  print_subexp (exp, &pc, stream, PREC_NULL);
}

/* Print the subexpression of EXP that starts in position POS, on STREAM.
   PREC is the precedence of the surrounding operator;
   if the precedence of the main operator of this subexpression is less,
   parentheses are needed here.  */

static void
print_subexp (exp, pos, stream, prec)
     register struct expression *exp;
     register int *pos;
     FILE *stream;
     enum precedence prec;
{
  register int tem;
  register int pc;
  int nargs;
  register char *op_str;
  int assign_modify = 0;
  enum exp_opcode opcode;
  enum precedence myprec;
  /* Set to 1 for a right-associative operator.  */
  int assoc;

  pc = (*pos)++;
  opcode = exp->elts[pc].opcode;
  switch (opcode)
    {
    case OP_SCOPE:
      myprec = PREC_PREFIX;
      assoc = 0;
      (*pos) += 2;
      print_subexp (exp, pos, stream, (int) myprec + assoc);
      fprintf (stream, " :: ");
      nargs = strlen (&exp->elts[pc + 2].string);
      (*pos) += 1 + (nargs + sizeof (union exp_element)) / sizeof (union exp_element);

      fprintf (stream, &exp->elts[pc + 2].string);
      return;

    case OP_LONG:
      (*pos) += 3;
      value_print (value_from_long (exp->elts[pc + 1].type,
				    exp->elts[pc + 2].longconst),
		   stream, 0, Val_no_prettyprint);
      return;

    case OP_DOUBLE:
      (*pos) += 3;
      value_print (value_from_double (exp->elts[pc + 1].type,
				      exp->elts[pc + 2].doubleconst),
		   stream, 0, Val_no_prettyprint);
      return;

    case OP_VAR_VALUE:
      (*pos) += 2;
      fprintf (stream, "%s", SYMBOL_NAME (exp->elts[pc + 1].symbol));
      return;

    case OP_LAST:
      (*pos) += 2;
      fprintf (stream, "$%d", (int) exp->elts[pc + 1].longconst);
      return;

    case OP_REGISTER:
      (*pos) += 2;
      fprintf (stream, "$%s", reg_names[exp->elts[pc + 1].longconst]);
      return;

    case OP_INTERNALVAR:
      (*pos) += 2;
      fprintf (stream, "$%s",
	       internalvar_name (exp->elts[pc + 1].internalvar));
      return;

    case OP_FUNCALL:
      (*pos) += 2;
      nargs = exp->elts[pc + 1].longconst;
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, " (");
      for (tem = 0; tem < nargs; tem++)
	{
	  if (tem > 0)
	    fprintf (stream, ", ");
	  print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
	}
      fprintf (stream, ")");
      return;

    case OP_STRING:
      nargs = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + (nargs + sizeof (union exp_element)) / sizeof (union exp_element);
      fprintf (stream, "\"");
      for (tem = 0; tem < nargs; tem++)
	printchar ((&exp->elts[pc + 1].string)[tem], stream, '"');
      fprintf (stream, "\"");
      return;

    case TERNOP_COND:
      if ((int) prec > (int) PREC_COMMA)
	fprintf (stream, "(");
      /* Print the subexpressions, forcing parentheses
	 around any binary operations within them.
	 This is more parentheses than are strictly necessary,
	 but it looks clearer.  */
      print_subexp (exp, pos, stream, PREC_HYPER);
      fprintf (stream, " ? ");
      print_subexp (exp, pos, stream, PREC_HYPER);
      fprintf (stream, " : ");
      print_subexp (exp, pos, stream, PREC_HYPER);
      if ((int) prec > (int) PREC_COMMA)
	fprintf (stream, ")");
      return;

    case STRUCTOP_STRUCT:
      tem = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + (tem + sizeof (union exp_element)) / sizeof (union exp_element);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, ".%s", &exp->elts[pc + 1].string);
      return;

    case STRUCTOP_PTR:
      tem = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + (tem + sizeof (union exp_element)) / sizeof (union exp_element);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, "->%s", &exp->elts[pc + 1].string);
      return;

    case BINOP_SUBSCRIPT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, "[");
      print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
      fprintf (stream, "]");
      return;

    case UNOP_POSTINCREMENT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, "++");
      return;

    case UNOP_POSTDECREMENT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf (stream, "--");
      return;

    case UNOP_CAST:
      (*pos) += 2;
      if ((int) prec > (int) PREC_PREFIX)
	fprintf (stream, "(");
      fprintf (stream, "(");
      type_print (exp->elts[pc + 1].type, "", stream, 0);
      fprintf (stream, ") ");
      print_subexp (exp, pos, stream, PREC_PREFIX);
      if ((int) prec > (int) PREC_PREFIX)
	fprintf (stream, ")");
      return;

    case UNOP_MEMVAL:
      (*pos) += 2;
      if ((int) prec > (int) PREC_PREFIX)
	fprintf (stream, "(");
      fprintf (stream, "{");
      type_print (exp->elts[pc + 1].type, "", stream, 0);
      fprintf (stream, "} ");
      print_subexp (exp, pos, stream, PREC_PREFIX);
      if ((int) prec > (int) PREC_PREFIX)
	fprintf (stream, ")");
      return;

    case BINOP_ASSIGN_MODIFY:
      opcode = exp->elts[pc + 1].opcode;
      (*pos) += 2;
      myprec = PREC_ASSIGN;
      assoc = 1;
      assign_modify = 1;
      for (tem = 0; tem < sizeof op_print_tab / sizeof op_print_tab[0]; tem++)
	if (op_print_tab[tem].opcode == opcode)
	  {
	    op_str = op_print_tab[tem].string;
	    break;
	  }

    case OP_THIS:
      ++(*pos);
      fprintf (stream, "this");
      return;

    default:
      for (tem = 0; tem < sizeof op_print_tab / sizeof op_print_tab[0]; tem++)
	if (op_print_tab[tem].opcode == opcode)
	  {
	    op_str = op_print_tab[tem].string;
	    myprec = op_print_tab[tem].precedence;
	    assoc = op_print_tab[tem].right_assoc;
	    break;
	  }
    }

  if ((int) myprec < (int) prec)
    fprintf (stream, "(");
  if ((int) opcode > (int) BINOP_END)
    {
      /* Unary prefix operator.  */
      fprintf (stream, "%s", op_str);
      print_subexp (exp, pos, stream, PREC_PREFIX);
    }
  else
    {
      /* Binary operator.  */
      /* Print left operand.
	 If operator is right-associative,
	 increment precedence for this operand.  */
      print_subexp (exp, pos, stream, (int) myprec + assoc);
      /* Print the operator itself.  */
      if (assign_modify)
	fprintf (stream, " %s= ", op_str);
      else if (op_str[0] == ',')
	fprintf (stream, "%s ", op_str);
      else
	fprintf (stream, " %s ", op_str);
      /* Print right operand.
	 If operator is left-associative,
	 increment precedence for this operand.  */
      print_subexp (exp, pos, stream, (int) myprec + !assoc);
    }
  if ((int) myprec < (int) prec)
    fprintf (stream, ")");
}
