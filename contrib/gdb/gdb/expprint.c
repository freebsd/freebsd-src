/* Print in infix form a struct expression.
   Copyright (C) 1986, 1989, 1991 Free Software Foundation, Inc.

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
#include "expression.h"
#include "value.h"
#include "language.h"
#include "parser-defs.h"

/* Prototypes for local functions */

static void
print_subexp PARAMS ((struct expression *, int *, GDB_FILE *, enum precedence));

void
print_expression (exp, stream)
     struct expression *exp;
     GDB_FILE *stream;
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
     GDB_FILE *stream;
     enum precedence prec;
{
  register unsigned tem;
  register const struct op_print *op_print_tab;
  register int pc;
  unsigned nargs;
  register char *op_str;
  int assign_modify = 0;
  enum exp_opcode opcode;
  enum precedence myprec = PREC_NULL;
  /* Set to 1 for a right-associative operator.  */
  int assoc = 0;
  value_ptr val;
  char *tempstr = NULL;

  op_print_tab = exp->language_defn->la_op_print_tab;
  pc = (*pos)++;
  opcode = exp->elts[pc].opcode;
  switch (opcode)
    {
    /* Common ops */

    case OP_SCOPE:
      myprec = PREC_PREFIX;
      assoc = 0;
      fputs_filtered (type_name_no_tag (exp->elts[pc + 1].type), stream);
      fputs_filtered ("::", stream);
      nargs = longest_to_int (exp->elts[pc + 2].longconst);
      (*pos) += 4 + BYTES_TO_EXP_ELEM (nargs + 1);
      fputs_filtered (&exp->elts[pc + 3].string, stream);
      return;

    case OP_LONG:
      (*pos) += 3;
      value_print (value_from_longest (exp->elts[pc + 1].type,
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
      {
	struct block *b;
	(*pos) += 3;
	b = exp->elts[pc + 1].block;
	if (b != NULL
	    && BLOCK_FUNCTION (b) != NULL
	    && SYMBOL_SOURCE_NAME (BLOCK_FUNCTION (b)) != NULL)
	  {
	    fputs_filtered (SYMBOL_SOURCE_NAME (BLOCK_FUNCTION (b)), stream);
	    fputs_filtered ("::", stream);
	  }
	fputs_filtered (SYMBOL_SOURCE_NAME (exp->elts[pc + 2].symbol), stream);
      }
      return;

    case OP_LAST:
      (*pos) += 2;
      fprintf_filtered (stream, "$%d",
			longest_to_int (exp->elts[pc + 1].longconst));
      return;

    case OP_REGISTER:
      (*pos) += 2;
      fprintf_filtered (stream, "$%s",
	       reg_names[longest_to_int (exp->elts[pc + 1].longconst)]);
      return;

    case OP_BOOL:
      (*pos) += 2;
      fprintf_filtered (stream, "%s",
			longest_to_int (exp->elts[pc + 1].longconst)
			? "TRUE" : "FALSE");
      return;

    case OP_INTERNALVAR:
      (*pos) += 2;
      fprintf_filtered (stream, "$%s",
	       internalvar_name (exp->elts[pc + 1].internalvar));
      return;

    case OP_FUNCALL:
      (*pos) += 2;
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered (" (", stream);
      for (tem = 0; tem < nargs; tem++)
	{
	  if (tem != 0)
	    fputs_filtered (", ", stream);
	  print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
	}
      fputs_filtered (")", stream);
      return;

    case OP_NAME:
    case OP_EXPRSTRING:
      nargs = longest_to_int (exp -> elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (nargs + 1);
      fputs_filtered (&exp->elts[pc + 2].string, stream);
      return;

    case OP_STRING:
      nargs = longest_to_int (exp -> elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (nargs + 1);
      /* LA_PRINT_STRING will print using the current repeat count threshold.
	 If necessary, we can temporarily set it to zero, or pass it as an
	 additional parameter to LA_PRINT_STRING.  -fnf */
      LA_PRINT_STRING (stream, &exp->elts[pc + 2].string, nargs, 0);
      return;

    case OP_BITSTRING:
      nargs = longest_to_int (exp -> elts[pc + 1].longconst);
      (*pos)
	+= 3 + BYTES_TO_EXP_ELEM ((nargs + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT);
      fprintf (stream, "B'<unimplemented>'");
      return;

    case OP_ARRAY:
      (*pos) += 3;
      nargs = longest_to_int (exp->elts[pc + 2].longconst);
      nargs -= longest_to_int (exp->elts[pc + 1].longconst);
      nargs++;
      tem = 0;
      if (exp->elts[pc + 4].opcode == OP_LONG
	  && exp->elts[pc + 5].type == builtin_type_char
	  && exp->language_defn->la_language == language_c)
	{
	  /* Attempt to print C character arrays using string syntax.
	     Walk through the args, picking up one character from each
	     of the OP_LONG expression elements.  If any array element
	     does not match our expection of what we should find for
	     a simple string, revert back to array printing.  Note that
	     the last expression element is an explicit null terminator
	     byte, which doesn't get printed. */
	  tempstr = alloca (nargs);
	  pc += 4;
	  while (tem < nargs)
	    {
	      if (exp->elts[pc].opcode != OP_LONG
		  || exp->elts[pc + 1].type != builtin_type_char)
		{
		  /* Not a simple array of char, use regular array printing. */
		  tem = 0;
		  break;
		}
	      else
		{
		  tempstr[tem++] =
		    longest_to_int (exp->elts[pc + 2].longconst);
		  pc += 4;
		}
	    }
	}
      if (tem > 0)
	{
	  LA_PRINT_STRING (stream, tempstr, nargs - 1, 0);
	  (*pos) = pc;
	}
      else
	{
	  int is_chill = exp->language_defn->la_language == language_chill;
	  fputs_filtered (is_chill ? " [" : " {", stream);
	  for (tem = 0; tem < nargs; tem++)
	    {
	      if (tem != 0)
		{
		  fputs_filtered (", ", stream);
		}
	      print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
	    }
	  fputs_filtered (is_chill ? "]" : "}", stream);
	}
      return;

    case OP_LABELED:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);

      if (exp->language_defn->la_language == language_chill)
	{
	  fputs_filtered (".", stream);
	  fputs_filtered (&exp->elts[pc + 2].string, stream);
	  fputs_filtered (exp->elts[*pos].opcode == OP_LABELED ? ", "
			  : ": ",
			  stream);
	}
      else
	{
	  /* Gcc support both these syntaxes.  Unsure which is preferred.  */
#if 1
	  fputs_filtered (&exp->elts[pc + 2].string, stream);
	  fputs_filtered (": ", stream);
#else
	  fputs_filtered (".", stream);
	  fputs_filtered (&exp->elts[pc + 2].string, stream);
	  fputs_filtered ("=", stream);
#endif
	}
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      return;

    case TERNOP_COND:
      if ((int) prec > (int) PREC_COMMA)
	fputs_filtered ("(", stream);
      /* Print the subexpressions, forcing parentheses
	 around any binary operations within them.
	 This is more parentheses than are strictly necessary,
	 but it looks clearer.  */
      print_subexp (exp, pos, stream, PREC_HYPER);
      fputs_filtered (" ? ", stream);
      print_subexp (exp, pos, stream, PREC_HYPER);
      fputs_filtered (" : ", stream);
      print_subexp (exp, pos, stream, PREC_HYPER);
      if ((int) prec > (int) PREC_COMMA)
	fputs_filtered (")", stream);
      return;

    case TERNOP_SLICE:
    case TERNOP_SLICE_COUNT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("(", stream);
      print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
      fputs_filtered (opcode == TERNOP_SLICE ? " : " : " UP ", stream);
      print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
      fputs_filtered (")", stream);
      return;

    case STRUCTOP_STRUCT:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered (".", stream);
      fputs_filtered (&exp->elts[pc + 2].string, stream);
      return;

    /* Will not occur for Modula-2 */
    case STRUCTOP_PTR:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("->", stream);
      fputs_filtered (&exp->elts[pc + 2].string, stream);
      return;


    case BINOP_SUBSCRIPT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("[", stream);
      print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
      fputs_filtered ("]", stream);
      return;

    case UNOP_POSTINCREMENT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("++", stream);
      return;

    case UNOP_POSTDECREMENT:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("--", stream);
      return;

    case UNOP_CAST:
      (*pos) += 2;
      if ((int) prec > (int) PREC_PREFIX)
        fputs_filtered ("(", stream);
      fputs_filtered ("(", stream);
      type_print (exp->elts[pc + 1].type, "", stream, 0);
      fputs_filtered (") ", stream);
      print_subexp (exp, pos, stream, PREC_PREFIX);
      if ((int) prec > (int) PREC_PREFIX)
        fputs_filtered (")", stream);
      return;

    case UNOP_MEMVAL:
      (*pos) += 2;
      if ((int) prec > (int) PREC_PREFIX)
        fputs_filtered ("(", stream);
      if (exp->elts[pc + 1].type->code == TYPE_CODE_FUNC &&
	  exp->elts[pc + 3].opcode == OP_LONG) {
	/* We have a minimal symbol fn, probably.  It's encoded
	   as a UNOP_MEMVAL (function-type) of an OP_LONG (int, address).
	   Swallow the OP_LONG (including both its opcodes); ignore
	   its type; print the value in the type of the MEMVAL.  */
	(*pos) += 4;
	val = value_at_lazy (exp->elts[pc + 1].type,
			     (CORE_ADDR) exp->elts[pc + 5].longconst);
	value_print (val, stream, 0, Val_no_prettyprint);
      } else {
	fputs_filtered ("{", stream);
	type_print (exp->elts[pc + 1].type, "", stream, 0);
	fputs_filtered ("} ", stream);
        print_subexp (exp, pos, stream, PREC_PREFIX);
      }
      if ((int) prec > (int) PREC_PREFIX)
        fputs_filtered (")", stream);
      return;

    case BINOP_ASSIGN_MODIFY:
      opcode = exp->elts[pc + 1].opcode;
      (*pos) += 2;
      myprec = PREC_ASSIGN;
      assoc = 1;
      assign_modify = 1;
      op_str = "???";
      for (tem = 0; op_print_tab[tem].opcode != OP_NULL; tem++)
	if (op_print_tab[tem].opcode == opcode)
	  {
	    op_str = op_print_tab[tem].string;
	    break;
	  }
      if (op_print_tab[tem].opcode != opcode)
	/* Not found; don't try to keep going because we don't know how
	   to interpret further elements.  */
	error ("Invalid expression");
      break;

    /* C++ ops */

    case OP_THIS:
      ++(*pos);
      fputs_filtered ("this", stream);
      return;

    /* Modula-2 ops */

    case MULTI_SUBSCRIPT:
      (*pos) += 2;
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf_unfiltered (stream, " [");
      for (tem = 0; tem < nargs; tem++)
	{
	  if (tem != 0)
	    fprintf_unfiltered (stream, ", ");
	  print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
	}
      fprintf_unfiltered (stream, "]");
      return;

    case BINOP_VAL:
      (*pos)+=2;
      fprintf_unfiltered(stream,"VAL(");
      type_print(exp->elts[pc+1].type,"",stream,0);
      fprintf_unfiltered(stream,",");
      print_subexp(exp,pos,stream,PREC_PREFIX);
      fprintf_unfiltered(stream,")");
      return;
      
    case BINOP_INCL:
    case BINOP_EXCL:
      error("print_subexp:  Not implemented.");

    /* Default ops */

    default:
      op_str = "???";
      for (tem = 0; op_print_tab[tem].opcode != OP_NULL; tem++)
	if (op_print_tab[tem].opcode == opcode)
	  {
	    op_str = op_print_tab[tem].string;
	    myprec = op_print_tab[tem].precedence;
	    assoc = op_print_tab[tem].right_assoc;
	    break;
	  }
      if (op_print_tab[tem].opcode != opcode)
	/* Not found; don't try to keep going because we don't know how
	   to interpret further elements.  For example, this happens
	   if opcode is OP_TYPE.  */
	error ("Invalid expression");
   }

  /* Note that PREC_BUILTIN will always emit parentheses. */
  if ((int) myprec < (int) prec)
    fputs_filtered ("(", stream);
  if ((int) opcode > (int) BINOP_END)
    {
      if (assoc)
	{
	  /* Unary postfix operator.  */
	  print_subexp (exp, pos, stream, PREC_SUFFIX);
	  fputs_filtered (op_str, stream);
	}
      else
	{
	  /* Unary prefix operator.  */
	  fputs_filtered (op_str, stream);
	  if (myprec == PREC_BUILTIN_FUNCTION)
	    fputs_filtered ("(", stream);
	  print_subexp (exp, pos, stream, PREC_PREFIX);
	  if (myprec == PREC_BUILTIN_FUNCTION)
	    fputs_filtered (")", stream);
	}
    }
  else
    {
      /* Binary operator.  */
      /* Print left operand.
	 If operator is right-associative,
	 increment precedence for this operand.  */
      print_subexp (exp, pos, stream,
		    (enum precedence) ((int) myprec + assoc));
      /* Print the operator itself.  */
      if (assign_modify)
	fprintf_filtered (stream, " %s= ", op_str);
      else if (op_str[0] == ',')
	fprintf_filtered (stream, "%s ", op_str);
      else
	fprintf_filtered (stream, " %s ", op_str);
      /* Print right operand.
	 If operator is left-associative,
	 increment precedence for this operand.  */
      print_subexp (exp, pos, stream,
		    (enum precedence) ((int) myprec + !assoc));
    }

  if ((int) myprec < (int) prec)
    fputs_filtered (")", stream);
}

/* Return the operator corresponding to opcode OP as
   a string.   NULL indicates that the opcode was not found in the
   current language table.  */
char *
op_string(op)
   enum exp_opcode op;
{
  int tem;
  register const struct op_print *op_print_tab;

  op_print_tab = current_language->la_op_print_tab;
  for (tem = 0; op_print_tab[tem].opcode != OP_NULL; tem++)
    if (op_print_tab[tem].opcode == op)
      return op_print_tab[tem].string;
  return NULL;
}

#ifdef DEBUG_EXPRESSIONS

/* Support for dumping the raw data from expressions in a human readable
   form.  */

void
dump_expression (exp, stream, note)
     struct expression *exp;
     GDB_FILE *stream;
     char *note;
{
  int elt;
  char *opcode_name;
  char *eltscan;
  int eltsize;

  fprintf_filtered (stream, "Dump of expression @ ");
  gdb_print_address (exp, stream);
  fprintf_filtered (stream, ", %s:\n", note);
  fprintf_filtered (stream, "\tLanguage %s, %d elements, %d bytes each.\n",
		    exp->language_defn->la_name, exp -> nelts,
		    sizeof (union exp_element));
  fprintf_filtered (stream, "\t%5s  %20s  %16s  %s\n", "Index", "Opcode",
		    "Hex Value", "String Value");
  for (elt = 0; elt < exp -> nelts; elt++)
    {
      fprintf_filtered (stream, "\t%5d  ", elt);
      switch (exp -> elts[elt].opcode)
	{
	  default: opcode_name = "<unknown>"; break;
	  case OP_NULL: opcode_name = "OP_NULL"; break;
	  case BINOP_ADD: opcode_name = "BINOP_ADD"; break;
	  case BINOP_SUB: opcode_name = "BINOP_SUB"; break;
	  case BINOP_MUL: opcode_name = "BINOP_MUL"; break;
	  case BINOP_DIV: opcode_name = "BINOP_DIV"; break;
	  case BINOP_REM: opcode_name = "BINOP_REM"; break;
	  case BINOP_MOD: opcode_name = "BINOP_MOD"; break;
	  case BINOP_LSH: opcode_name = "BINOP_LSH"; break;
	  case BINOP_RSH: opcode_name = "BINOP_RSH"; break;
	  case BINOP_LOGICAL_AND: opcode_name = "BINOP_LOGICAL_AND"; break;
	  case BINOP_LOGICAL_OR: opcode_name = "BINOP_LOGICAL_OR"; break;
	  case BINOP_BITWISE_AND: opcode_name = "BINOP_BITWISE_AND"; break;
	  case BINOP_BITWISE_IOR: opcode_name = "BINOP_BITWISE_IOR"; break;
	  case BINOP_BITWISE_XOR: opcode_name = "BINOP_BITWISE_XOR"; break;
	  case BINOP_EQUAL: opcode_name = "BINOP_EQUAL"; break;
	  case BINOP_NOTEQUAL: opcode_name = "BINOP_NOTEQUAL"; break;
	  case BINOP_LESS: opcode_name = "BINOP_LESS"; break;
	  case BINOP_GTR: opcode_name = "BINOP_GTR"; break;
	  case BINOP_LEQ: opcode_name = "BINOP_LEQ"; break;
	  case BINOP_GEQ: opcode_name = "BINOP_GEQ"; break;
	  case BINOP_REPEAT: opcode_name = "BINOP_REPEAT"; break;
	  case BINOP_ASSIGN: opcode_name = "BINOP_ASSIGN"; break;
	  case BINOP_COMMA: opcode_name = "BINOP_COMMA"; break;
	  case BINOP_SUBSCRIPT: opcode_name = "BINOP_SUBSCRIPT"; break;
	  case MULTI_SUBSCRIPT: opcode_name = "MULTI_SUBSCRIPT"; break;
	  case BINOP_EXP: opcode_name = "BINOP_EXP"; break;
	  case BINOP_MIN: opcode_name = "BINOP_MIN"; break;
	  case BINOP_MAX: opcode_name = "BINOP_MAX"; break;
	  case BINOP_SCOPE: opcode_name = "BINOP_SCOPE"; break;
	  case STRUCTOP_MEMBER: opcode_name = "STRUCTOP_MEMBER"; break;
	  case STRUCTOP_MPTR: opcode_name = "STRUCTOP_MPTR"; break;
	  case BINOP_INTDIV: opcode_name = "BINOP_INTDIV"; break;
	  case BINOP_ASSIGN_MODIFY: opcode_name = "BINOP_ASSIGN_MODIFY"; break;
	  case BINOP_VAL: opcode_name = "BINOP_VAL"; break;
	  case BINOP_INCL: opcode_name = "BINOP_INCL"; break;
	  case BINOP_EXCL: opcode_name = "BINOP_EXCL"; break;
	  case BINOP_CONCAT: opcode_name = "BINOP_CONCAT"; break;
	  case BINOP_RANGE: opcode_name = "BINOP_RANGE"; break;
	  case BINOP_END: opcode_name = "BINOP_END"; break;
	  case TERNOP_COND: opcode_name = "TERNOP_COND"; break;
	  case TERNOP_SLICE: opcode_name = "TERNOP_SLICE"; break;
	  case TERNOP_SLICE_COUNT: opcode_name = "TERNOP_SLICE_COUNT"; break;
	  case OP_LONG: opcode_name = "OP_LONG"; break;
	  case OP_DOUBLE: opcode_name = "OP_DOUBLE"; break;
	  case OP_VAR_VALUE: opcode_name = "OP_VAR_VALUE"; break;
	  case OP_LAST: opcode_name = "OP_LAST"; break;
	  case OP_REGISTER: opcode_name = "OP_REGISTER"; break;
	  case OP_INTERNALVAR: opcode_name = "OP_INTERNALVAR"; break;
	  case OP_FUNCALL: opcode_name = "OP_FUNCALL"; break;
	  case OP_STRING: opcode_name = "OP_STRING"; break;
	  case OP_BITSTRING: opcode_name = "OP_BITSTRING"; break;
	  case OP_ARRAY: opcode_name = "OP_ARRAY"; break;
	  case UNOP_CAST: opcode_name = "UNOP_CAST"; break;
	  case UNOP_MEMVAL: opcode_name = "UNOP_MEMVAL"; break;
	  case UNOP_NEG: opcode_name = "UNOP_NEG"; break;
	  case UNOP_LOGICAL_NOT: opcode_name = "UNOP_LOGICAL_NOT"; break;
	  case UNOP_COMPLEMENT: opcode_name = "UNOP_COMPLEMENT"; break;
	  case UNOP_IND: opcode_name = "UNOP_IND"; break;
	  case UNOP_ADDR: opcode_name = "UNOP_ADDR"; break;
	  case UNOP_PREINCREMENT: opcode_name = "UNOP_PREINCREMENT"; break;
	  case UNOP_POSTINCREMENT: opcode_name = "UNOP_POSTINCREMENT"; break;
	  case UNOP_PREDECREMENT: opcode_name = "UNOP_PREDECREMENT"; break;
	  case UNOP_POSTDECREMENT: opcode_name = "UNOP_POSTDECREMENT"; break;
	  case UNOP_SIZEOF: opcode_name = "UNOP_SIZEOF"; break;
	  case UNOP_LOWER: opcode_name = "UNOP_LOWER"; break;
	  case UNOP_UPPER: opcode_name = "UNOP_UPPER"; break;
	  case UNOP_LENGTH: opcode_name = "UNOP_LENGTH"; break;
	  case UNOP_PLUS: opcode_name = "UNOP_PLUS"; break;
	  case UNOP_CAP: opcode_name = "UNOP_CAP"; break;
	  case UNOP_CHR: opcode_name = "UNOP_CHR"; break;
	  case UNOP_ORD: opcode_name = "UNOP_ORD"; break;
	  case UNOP_ABS: opcode_name = "UNOP_ABS"; break;
	  case UNOP_FLOAT: opcode_name = "UNOP_FLOAT"; break;
	  case UNOP_HIGH: opcode_name = "UNOP_HIGH"; break;
	  case UNOP_MAX: opcode_name = "UNOP_MAX"; break;
	  case UNOP_MIN: opcode_name = "UNOP_MIN"; break;
	  case UNOP_ODD: opcode_name = "UNOP_ODD"; break;
	  case UNOP_TRUNC: opcode_name = "UNOP_TRUNC"; break;
	  case OP_BOOL: opcode_name = "OP_BOOL"; break;
	  case OP_M2_STRING: opcode_name = "OP_M2_STRING"; break;
	  case STRUCTOP_STRUCT: opcode_name = "STRUCTOP_STRUCT"; break;
	  case STRUCTOP_PTR: opcode_name = "STRUCTOP_PTR"; break;
	  case OP_THIS: opcode_name = "OP_THIS"; break;
	  case OP_SCOPE: opcode_name = "OP_SCOPE"; break;
	  case OP_TYPE: opcode_name = "OP_TYPE"; break;
	  case OP_LABELED: opcode_name = "OP_LABELED"; break;
	}
      fprintf_filtered (stream, "%20s  ", opcode_name);
      fprintf_filtered (stream,
#if defined (PRINTF_HAS_LONG_LONG)
			"%ll16x  ",
#else
			"%l16x  ",
#endif
			exp -> elts[elt].longconst);

      for (eltscan = (char *) &exp->elts[elt],
	     eltsize = sizeof (union exp_element) ;
	   eltsize-- > 0;
	   eltscan++)
	{
	  fprintf_filtered (stream, "%c",
			    isprint (*eltscan) ? (*eltscan & 0xFF) : '.');
	}
      fprintf_filtered (stream, "\n");
    }
}

#endif	/* DEBUG_EXPRESSIONS */
