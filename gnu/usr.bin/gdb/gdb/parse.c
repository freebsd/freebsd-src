/* Parse expressions for GDB.
   Copyright (C) 1986, 1989, 1990, 1991 Free Software Foundation, Inc.
   Modified from expread.y by the Department of Computer Science at the
   State University of New York at Buffalo, 1991.

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

/* Parse an expression from text in a string,
   and return the result as a  struct expression  pointer.
   That structure contains arithmetic operations in reverse polish,
   with constants represented by operations that are followed by special data.
   See expression.h for the details of the format.
   What is important here is that it can be built up sequentially
   during the process of parsing; the lower levels of the tree always
   come first in the result.  */
   
#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "expression.h"
#include "value.h"
#include "command.h"
#include "language.h"
#include "parser-defs.h"

static void
free_funcalls PARAMS ((void));

static void
prefixify_expression PARAMS ((struct expression *));

static int
length_of_subexp PARAMS ((struct expression *, int));

static void
prefixify_subexp PARAMS ((struct expression *, struct expression *, int, int));

/* Data structure for saving values of arglist_len for function calls whose
   arguments contain other function calls.  */

struct funcall
  {
    struct funcall *next;
    int arglist_len;
  };

static struct funcall *funcall_chain;

/* Assign machine-independent names to certain registers 
   (unless overridden by the REGISTER_NAMES table) */

#ifdef NO_STD_REGS
unsigned num_std_regs = 0;
struct std_regs std_regs[1];
#else
struct std_regs std_regs[] = {

#ifdef PC_REGNUM
	{ "pc", PC_REGNUM },
#endif
#ifdef FP_REGNUM
	{ "fp", FP_REGNUM },
#endif
#ifdef SP_REGNUM
	{ "sp", SP_REGNUM },
#endif
#ifdef PS_REGNUM
	{ "ps", PS_REGNUM },
#endif

};

unsigned num_std_regs = (sizeof std_regs / sizeof std_regs[0]);

#endif


/* Begin counting arguments for a function call,
   saving the data about any containing call.  */

void
start_arglist ()
{
  register struct funcall *new;

  new = (struct funcall *) xmalloc (sizeof (struct funcall));
  new->next = funcall_chain;
  new->arglist_len = arglist_len;
  arglist_len = 0;
  funcall_chain = new;
}

/* Return the number of arguments in a function call just terminated,
   and restore the data for the containing function call.  */

int
end_arglist ()
{
  register int val = arglist_len;
  register struct funcall *call = funcall_chain;
  funcall_chain = call->next;
  arglist_len = call->arglist_len;
  free ((PTR)call);
  return val;
}

/* Free everything in the funcall chain.
   Used when there is an error inside parsing.  */

static void
free_funcalls ()
{
  register struct funcall *call, *next;

  for (call = funcall_chain; call; call = next)
    {
      next = call->next;
      free ((PTR)call);
    }
}

/* This page contains the functions for adding data to the  struct expression
   being constructed.  */

/* Add one element to the end of the expression.  */

/* To avoid a bug in the Sun 4 compiler, we pass things that can fit into
   a register through here */

void
write_exp_elt (expelt)
     union exp_element expelt;
{
  if (expout_ptr >= expout_size)
    {
      expout_size *= 2;
      expout = (struct expression *)
	xrealloc ((char *) expout, sizeof (struct expression)
		  + EXP_ELEM_TO_BYTES (expout_size));
    }
  expout->elts[expout_ptr++] = expelt;
}

void
write_exp_elt_opcode (expelt)
     enum exp_opcode expelt;
{
  union exp_element tmp;

  tmp.opcode = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_sym (expelt)
     struct symbol *expelt;
{
  union exp_element tmp;

  tmp.symbol = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_block (b)
     struct block *b;
{
  union exp_element tmp;
  tmp.block = b;
  write_exp_elt (tmp);
}

void
write_exp_elt_longcst (expelt)
     LONGEST expelt;
{
  union exp_element tmp;

  tmp.longconst = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_dblcst (expelt)
     double expelt;
{
  union exp_element tmp;

  tmp.doubleconst = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_type (expelt)
     struct type *expelt;
{
  union exp_element tmp;

  tmp.type = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_intern (expelt)
     struct internalvar *expelt;
{
  union exp_element tmp;

  tmp.internalvar = expelt;

  write_exp_elt (tmp);
}

/* Add a string constant to the end of the expression.

   String constants are stored by first writing an expression element
   that contains the length of the string, then stuffing the string
   constant itself into however many expression elements are needed
   to hold it, and then writing another expression element that contains
   the length of the string.  I.E. an expression element at each end of
   the string records the string length, so you can skip over the 
   expression elements containing the actual string bytes from either
   end of the string.  Note that this also allows gdb to handle
   strings with embedded null bytes, as is required for some languages.

   Don't be fooled by the fact that the string is null byte terminated,
   this is strictly for the convenience of debugging gdb itself.  Gdb
   Gdb does not depend up the string being null terminated, since the
   actual length is recorded in expression elements at each end of the
   string.  The null byte is taken into consideration when computing how
   many expression elements are required to hold the string constant, of
   course. */


void
write_exp_string (str)
     struct stoken str;
{
  register int len = str.length;
  register int lenelt;
  register char *strdata;

  /* Compute the number of expression elements required to hold the string
     (including a null byte terminator), along with one expression element
     at each end to record the actual string length (not including the
     null byte terminator). */

  lenelt = 2 + BYTES_TO_EXP_ELEM (len + 1);

  /* Ensure that we have enough available expression elements to store
     everything. */

  if ((expout_ptr + lenelt) >= expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + lenelt + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  /* Write the leading length expression element (which advances the current
     expression element index), then write the string constant followed by a
     terminating null byte, and then write the trailing length expression
     element. */

  write_exp_elt_longcst ((LONGEST) len);
  strdata = (char *) &expout->elts[expout_ptr];
  memcpy (strdata, str.ptr, len);
  *(strdata + len) = '\0';
  expout_ptr += lenelt - 2;
  write_exp_elt_longcst ((LONGEST) len);
}

/* Add a bitstring constant to the end of the expression.

   Bitstring constants are stored by first writing an expression element
   that contains the length of the bitstring (in bits), then stuffing the
   bitstring constant itself into however many expression elements are
   needed to hold it, and then writing another expression element that
   contains the length of the bitstring.  I.E. an expression element at
   each end of the bitstring records the bitstring length, so you can skip
   over the expression elements containing the actual bitstring bytes from
   either end of the bitstring. */

void
write_exp_bitstring (str)
     struct stoken str;
{
  register int bits = str.length;	/* length in bits */
  register int len = (bits + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
  register int lenelt;
  register char *strdata;

  /* Compute the number of expression elements required to hold the bitstring,
     along with one expression element at each end to record the actual
     bitstring length in bits. */

  lenelt = 2 + BYTES_TO_EXP_ELEM (len);

  /* Ensure that we have enough available expression elements to store
     everything. */

  if ((expout_ptr + lenelt) >= expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + lenelt + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  /* Write the leading length expression element (which advances the current
     expression element index), then write the bitstring constant, and then
     write the trailing length expression element. */

  write_exp_elt_longcst ((LONGEST) bits);
  strdata = (char *) &expout->elts[expout_ptr];
  memcpy (strdata, str.ptr, len);
  expout_ptr += lenelt - 2;
  write_exp_elt_longcst ((LONGEST) bits);
}

/* Return a null-terminated temporary copy of the name
   of a string token.  */

char *
copy_name (token)
     struct stoken token;
{
  memcpy (namecopy, token.ptr, token.length);
  namecopy[token.length] = 0;
  return namecopy;
}

/* Reverse an expression from suffix form (in which it is constructed)
   to prefix form (in which we can conveniently print or execute it).  */

static void
prefixify_expression (expr)
     register struct expression *expr;
{
  register int len =
    sizeof (struct expression) + EXP_ELEM_TO_BYTES (expr->nelts);
  register struct expression *temp;
  register int inpos = expr->nelts, outpos = 0;

  temp = (struct expression *) alloca (len);

  /* Copy the original expression into temp.  */
  memcpy (temp, expr, len);

  prefixify_subexp (temp, expr, inpos, outpos);
}

/* Return the number of exp_elements in the subexpression of EXPR
   whose last exp_element is at index ENDPOS - 1 in EXPR.  */

static int
length_of_subexp (expr, endpos)
     register struct expression *expr;
     register int endpos;
{
  register int oplen = 1;
  register int args = 0;
  register int i;

  if (endpos < 1)
    error ("?error in length_of_subexp");

  i = (int) expr->elts[endpos - 1].opcode;

  switch (i)
    {
      /* C++  */
    case OP_SCOPE:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = 5 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_LONG:
    case OP_DOUBLE:
    case OP_VAR_VALUE:
      oplen = 4;
      break;

    case OP_TYPE:
    case OP_BOOL:
    case OP_LAST:
    case OP_REGISTER:
    case OP_INTERNALVAR:
      oplen = 3;
      break;

    case OP_FUNCALL:
      oplen = 3;
      args = 1 + longest_to_int (expr->elts[endpos - 2].longconst);
      break;

    case UNOP_MAX:
    case UNOP_MIN:
      oplen = 3;
      break;

   case BINOP_VAL:
   case UNOP_CAST:
   case UNOP_MEMVAL:
      oplen = 3;
      args = 1;
      break;

    case UNOP_ABS:
    case UNOP_CAP:
    case UNOP_CHR:
    case UNOP_FLOAT:
    case UNOP_HIGH:
    case UNOP_ODD:
    case UNOP_ORD:
    case UNOP_TRUNC:
      oplen = 1;
      args = 1;
      break;

    case STRUCTOP_STRUCT:
    case STRUCTOP_PTR:
      args = 1;
      /* fall through */
    case OP_M2_STRING:
    case OP_STRING:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_BITSTRING:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = (oplen + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen);
      break;

    case OP_ARRAY:
      oplen = 4;
      args = longest_to_int (expr->elts[endpos - 2].longconst);
      args -= longest_to_int (expr->elts[endpos - 3].longconst);
      args += 1;
      break;

    case TERNOP_COND:
      args = 3;
      break;

      /* Modula-2 */
   case MULTI_SUBSCRIPT:
      oplen=3;
      args = 1 + longest_to_int (expr->elts[endpos- 2].longconst);
      break;

    case BINOP_ASSIGN_MODIFY:
      oplen = 3;
      args = 2;
      break;

      /* C++ */
    case OP_THIS:
      oplen = 2;
      break;

    default:
      args = 1 + (i < (int) BINOP_END);
    }

  while (args > 0)
    {
      oplen += length_of_subexp (expr, endpos - oplen);
      args--;
    }

  return oplen;
}

/* Copy the subexpression ending just before index INEND in INEXPR
   into OUTEXPR, starting at index OUTBEG.
   In the process, convert it from suffix to prefix form.  */

static void
prefixify_subexp (inexpr, outexpr, inend, outbeg)
     register struct expression *inexpr;
     struct expression *outexpr;
     register int inend;
     int outbeg;
{
  register int oplen = 1;
  register int args = 0;
  register int i;
  int *arglens;
  enum exp_opcode opcode;

  /* Compute how long the last operation is (in OPLEN),
     and also how many preceding subexpressions serve as
     arguments for it (in ARGS).  */

  opcode = inexpr->elts[inend - 1].opcode;
  switch (opcode)
    {
      /* C++  */
    case OP_SCOPE:
      oplen = longest_to_int (inexpr->elts[inend - 2].longconst);
      oplen = 5 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_LONG:
    case OP_DOUBLE:
    case OP_VAR_VALUE:
      oplen = 4;
      break;

    case OP_TYPE:
    case OP_BOOL:
    case OP_LAST:
    case OP_REGISTER:
    case OP_INTERNALVAR:
      oplen = 3;
      break;

    case OP_FUNCALL:
      oplen = 3;
      args = 1 + longest_to_int (inexpr->elts[inend - 2].longconst);
      break;

    case UNOP_MIN:
    case UNOP_MAX:
      oplen = 3;
      break;

    case UNOP_CAST:
    case UNOP_MEMVAL:
      oplen = 3;
      args = 1;
      break;

    case UNOP_ABS:
    case UNOP_CAP:
    case UNOP_CHR:
    case UNOP_FLOAT:
    case UNOP_HIGH:
    case UNOP_ODD:
    case UNOP_ORD:
    case UNOP_TRUNC:
      oplen=1;
      args=1;
      break;

    case STRUCTOP_STRUCT:
    case STRUCTOP_PTR:
      args = 1;
      /* fall through */
    case OP_M2_STRING:
    case OP_STRING:
      oplen = longest_to_int (inexpr->elts[inend - 2].longconst);
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_BITSTRING:
      oplen = longest_to_int (inexpr->elts[inend - 2].longconst);
      oplen = (oplen + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen);
      break;

    case OP_ARRAY:
      oplen = 4;
      args = longest_to_int (inexpr->elts[inend - 2].longconst);
      args -= longest_to_int (inexpr->elts[inend - 3].longconst);
      args += 1;
      break;

    case TERNOP_COND:
      args = 3;
      break;

    case BINOP_ASSIGN_MODIFY:
      oplen = 3;
      args = 2;
      break;

      /* Modula-2 */
   case MULTI_SUBSCRIPT:
      oplen=3;
      args = 1 + longest_to_int (inexpr->elts[inend - 2].longconst);
      break;

      /* C++ */
    case OP_THIS:
      oplen = 2;
      break;

    default:
      args = 1 + ((int) opcode < (int) BINOP_END);
    }

  /* Copy the final operator itself, from the end of the input
     to the beginning of the output.  */
  inend -= oplen;
  memcpy (&outexpr->elts[outbeg], &inexpr->elts[inend],
	  EXP_ELEM_TO_BYTES (oplen));
  outbeg += oplen;

  /* Find the lengths of the arg subexpressions.  */
  arglens = (int *) alloca (args * sizeof (int));
  for (i = args - 1; i >= 0; i--)
    {
      oplen = length_of_subexp (inexpr, inend);
      arglens[i] = oplen;
      inend -= oplen;
    }

  /* Now copy each subexpression, preserving the order of
     the subexpressions, but prefixifying each one.
     In this loop, inend starts at the beginning of
     the expression this level is working on
     and marches forward over the arguments.
     outbeg does similarly in the output.  */
  for (i = 0; i < args; i++)
    {
      oplen = arglens[i];
      inend += oplen;
      prefixify_subexp (inexpr, outexpr, inend, outbeg);
      outbeg += oplen;
    }
}

/* This page contains the two entry points to this file.  */

/* Read an expression from the string *STRINGPTR points to,
   parse it, and return a pointer to a  struct expression  that we malloc.
   Use block BLOCK as the lexical context for variable names;
   if BLOCK is zero, use the block of the selected stack frame.
   Meanwhile, advance *STRINGPTR to point after the expression,
   at the first nonwhite character that is not part of the expression
   (possibly a null character).

   If COMMA is nonzero, stop if a comma is reached.  */

struct expression *
parse_exp_1 (stringptr, block, comma)
     char **stringptr;
     struct block *block;
     int comma;
{
  struct cleanup *old_chain;

  lexptr = *stringptr;

  paren_depth = 0;
  type_stack_depth = 0;

  comma_terminates = comma;

  if (lexptr == 0 || *lexptr == 0)
    error_no_arg ("expression to compute");

  old_chain = make_cleanup (free_funcalls, 0);
  funcall_chain = 0;

  expression_context_block = block ? block : get_selected_block ();

  namecopy = (char *) alloca (strlen (lexptr) + 1);
  expout_size = 10;
  expout_ptr = 0;
  expout = (struct expression *)
    xmalloc (sizeof (struct expression) + EXP_ELEM_TO_BYTES (expout_size));
  expout->language_defn = current_language;
  make_cleanup (free_current_contents, &expout);

  if (current_language->la_parser ())
    current_language->la_error (NULL);

  discard_cleanups (old_chain);

  /* Record the actual number of expression elements, and then
     reallocate the expression memory so that we free up any
     excess elements. */

  expout->nelts = expout_ptr;
  expout = (struct expression *)
    xrealloc ((char *) expout,
	      sizeof (struct expression) + EXP_ELEM_TO_BYTES (expout_ptr));;

  /* Convert expression from postfix form as generated by yacc
     parser, to a prefix form. */

  DUMP_EXPRESSION (expout, stdout, "before conversion to prefix form");
  prefixify_expression (expout);
  DUMP_EXPRESSION (expout, stdout, "after conversion to prefix form");

  *stringptr = lexptr;
  return expout;
}

/* Parse STRING as an expression, and complain if this fails
   to use up all of the contents of STRING.  */

struct expression *
parse_expression (string)
     char *string;
{
  register struct expression *exp;
  exp = parse_exp_1 (&string, 0, 0);
  if (*string)
    error ("Junk after end of expression.");
  return exp;
}

/* Stuff for maintaining a stack of types.  Currently just used by C, but
   probably useful for any language which declares its types "backwards".  */

void 
push_type (tp)
     enum type_pieces tp;
{
  if (type_stack_depth == type_stack_size)
    {
      type_stack_size *= 2;
      type_stack = (union type_stack_elt *)
	xrealloc ((char *) type_stack, type_stack_size * sizeof (*type_stack));
    }
  type_stack[type_stack_depth++].piece = tp;
}

void
push_type_int (n)
     int n;
{
  if (type_stack_depth == type_stack_size)
    {
      type_stack_size *= 2;
      type_stack = (union type_stack_elt *)
	xrealloc ((char *) type_stack, type_stack_size * sizeof (*type_stack));
    }
  type_stack[type_stack_depth++].int_val = n;
}

enum type_pieces 
pop_type ()
{
  if (type_stack_depth)
    return type_stack[--type_stack_depth].piece;
  return tp_end;
}

int
pop_type_int ()
{
  if (type_stack_depth)
    return type_stack[--type_stack_depth].int_val;
  /* "Can't happen".  */
  return 0;
}

/* Pop the type stack and return the type which corresponds to FOLLOW_TYPE
   as modified by all the stuff on the stack.  */
struct type *
follow_types (follow_type)
     struct type *follow_type;
{
  int done = 0;
  int array_size;
  struct type *range_type;

  while (!done)
    switch (pop_type ())
      {
      case tp_end:
	done = 1;
	break;
      case tp_pointer:
	follow_type = lookup_pointer_type (follow_type);
	break;
      case tp_reference:
	follow_type = lookup_reference_type (follow_type);
	break;
      case tp_array:
	array_size = pop_type_int ();
	if (array_size != -1)
	  {
	    range_type =
	      create_range_type ((struct type *) NULL,
				 builtin_type_int, 0,
				 array_size - 1);
	    follow_type =
	      create_array_type ((struct type *) NULL,
				 follow_type, range_type);
	  }
	else
	  follow_type = lookup_pointer_type (follow_type);
	break;
      case tp_function:
	follow_type = lookup_function_type (follow_type);
	break;
      }
  return follow_type;
}

void
_initialize_parse ()
{
  type_stack_size = 80;
  type_stack_depth = 0;
  type_stack = (union type_stack_elt *)
    xmalloc (type_stack_size * sizeof (*type_stack));
}
