/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)eval.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* Evaluate expressions for GDB.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.

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

#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "value.h"
#include "expression.h"


/* Parse the string EXP as a C expression, evaluate it,
   and return the result as a number.  */

CORE_ADDR
parse_and_eval_address (exp)
     char *exp;
{
  struct expression *expr = parse_c_expression (exp);
  register CORE_ADDR addr;
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);

  addr = (CORE_ADDR) value_as_long (evaluate_expression (expr));
  do_cleanups (old_chain);
  return addr;
}

/* Like parse_and_eval_address but takes a pointer to a char * variable
   and advanced that variable across the characters parsed.  */

CORE_ADDR
parse_and_eval_address_1 (expptr)
     char **expptr;
{
  struct expression *expr = parse_c_1 (expptr, 0, 0);
  register CORE_ADDR addr;
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);

  addr = value_as_long (evaluate_expression (expr));
  do_cleanups (old_chain);
  return addr;
}

value
parse_and_eval (exp)
     char *exp;
{
  struct expression *expr = parse_c_expression (exp);
  register value val;
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);
  do_cleanups (old_chain);
  return val;
}

/* Parse up to a comma (or to a closeparen)
   in the string EXPP as an expression, evaluate it, and return the value.
   EXPP is advanced to point to the comma.  */

value
parse_to_comma_and_eval (expp)
     char **expp;
{
  struct expression *expr = parse_c_1 (expp, 0, 1);
  register value val;
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);
  do_cleanups (old_chain);
  return val;
}

/* Evaluate an expression in internal prefix form
   such as is constructed by expread.y.

   See expression.h for info on the format of an expression.  */

static value evaluate_subexp ();
static value evaluate_subexp_for_address ();
static value evaluate_subexp_for_sizeof ();
static value evaluate_subexp_with_coercion ();

/* return true if 'var' has an address in inferior's memory. */
static int
value_has_lval(var)
    register struct symbol *var;
{
  switch (SYMBOL_CLASS(var))
    {
    case LOC_STATIC:
    case LOC_LABEL:
    case LOC_ARG:
    case LOC_REF_ARG:
    case LOC_LOCAL:
    case LOC_BLOCK:
      return (1);
    }
  return (0);
}

/* Values of NOSIDE argument to eval_subexp.  */
enum noside
{ EVAL_NORMAL,
  EVAL_SKIP,			/* Only effect is to increment pos.  */
  EVAL_AVOID_SIDE_EFFECTS,	/* Don't modify any variables or
				   call any functions.  The value
				   returned will have the correct
				   type, and will have an
				   approximately correct lvalue
				   type (inaccuracy: anything that is
				   listed as being in a register in
				   the function in which it was
				   declared will be lval_register).  */
};

value
evaluate_expression (exp)
     struct expression *exp;
{
  int pc = 0;
  return evaluate_subexp (0, exp, &pc, EVAL_NORMAL);
}

/* Evaluate an expression, avoiding all memory references
   and getting a value whose type alone is correct.  */

value
evaluate_type (exp)
     struct expression *exp;
{
  int pc = 0;
  return evaluate_subexp (0, exp, &pc, EVAL_AVOID_SIDE_EFFECTS);
}

static value
evaluate_subexp (expect_type, exp, pos, noside)
     struct type *expect_type;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  enum exp_opcode op;
  int tem;
  register int pc, pc2, oldpos;
  register value arg1, arg2, arg3;
  int nargs;
  value *argvec;

  pc = (*pos)++;
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case OP_SCOPE:
      tem = strlen (&exp->elts[pc + 2].string);
      (*pos) += 3 + ((tem + sizeof (union exp_element))
		     / sizeof (union exp_element));
      return value_static_field (exp->elts[pc + 1].type,
				 &exp->elts[pc + 2].string, -1);

    case OP_LONG:
      (*pos) += 3;
      return value_from_long (exp->elts[pc + 1].type,
			      exp->elts[pc + 2].longconst);

    case OP_DOUBLE:
      (*pos) += 3;
      return value_from_double (exp->elts[pc + 1].type,
				exp->elts[pc + 2].doubleconst);

    case OP_VAR_VALUE:
      (*pos) += 2;
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct symbol * sym = exp->elts[pc + 1].symbol;
	  enum lval_type lv;

	  switch (SYMBOL_CLASS (sym))
	    {
	    case LOC_CONST:
	    case LOC_LABEL:
	    case LOC_CONST_BYTES:
	      lv = not_lval;
	    case LOC_REGISTER:
	    case LOC_REGPARM:
	      lv = lval_register;
	    default:
	      lv = lval_memory;
	    }

	  return value_zero (SYMBOL_TYPE (sym), lv);
	}
      else
	return value_of_variable (exp->elts[pc + 1].symbol);

    case OP_LAST:
      (*pos) += 2;
      return access_value_history ((int) exp->elts[pc + 1].longconst);

    case OP_REGISTER:
      (*pos) += 2;
      return value_of_register ((int) exp->elts[pc + 1].longconst);

    case OP_INTERNALVAR:
      (*pos) += 2;
      return value_of_internalvar (exp->elts[pc + 1].internalvar);

    case OP_STRING:
      tem = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + ((tem + sizeof (union exp_element))
		     / sizeof (union exp_element));
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_string (&exp->elts[pc + 1].string, tem);

    case TERNOP_COND:
      /* Skip third and second args to evaluate the first one.  */
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (value_zerop (arg1))
	{
	  evaluate_subexp (0, exp, pos, EVAL_SKIP);
	  return evaluate_subexp (0, exp, pos, noside);
	}
      else
	{
	  arg2 = evaluate_subexp (0, exp, pos, noside);
	  evaluate_subexp (0, exp, pos, EVAL_SKIP);
	  return arg2;
	}

    case OP_FUNCALL:
      (*pos) += 2;
      op = exp->elts[*pos].opcode;
      if (op == STRUCTOP_MEMBER || op == STRUCTOP_MPTR)
	{
	  int fnptr;
	  int tem2;

	  nargs = (int) exp->elts[pc + 1].longconst + 1;
	  /* First, evaluate the structure into arg2 */
	  pc2 = (*pos)++;

	  if (noside == EVAL_SKIP)
	    goto nosideret;

	  if (op == STRUCTOP_MEMBER)
	    {
	      arg2 = evaluate_subexp_for_address (exp, pos, noside);
	    }
	  else
	    {
	      arg2 = evaluate_subexp (0, exp, pos, noside);
	    }

	  /* If the function is a virtual function, then the
	     aggregate value (providing the structure) plays
	     its part by providing the vtable.  Otherwise,
	     it is just along for the ride: call the function
	     directly.  */

	  arg1 = evaluate_subexp (0, exp, pos, noside);

	  fnptr = (int) value_as_long (arg1);
	  if (fnptr < 128)
	    {
	      struct type *basetype;
	      int i, j;
	      basetype = TYPE_TARGET_TYPE (VALUE_TYPE (arg2));
	      basetype = TYPE_VPTR_BASETYPE (basetype);
	      for (i = TYPE_NFN_FIELDS (basetype) - 1; i >= 0; i--)
		{
		  struct fn_field *f = TYPE_FN_FIELDLIST1 (basetype, i);
		  /* If one is virtual, then all are virtual.  */
		  if (TYPE_FN_FIELD_VIRTUAL_P (f, 0))
		    for (j = TYPE_FN_FIELDLIST_LENGTH (basetype, i) - 1; j >= 0; --j)
		      if (TYPE_FN_FIELD_VOFFSET (f, j) == fnptr)
			{
			  value vtbl;
			  value base = value_ind (arg2);
			  struct type *fntype = lookup_pointer_type (TYPE_FN_FIELD_TYPE (f, j));

			  if (TYPE_VPTR_FIELDNO (basetype) < 0)
			    TYPE_VPTR_FIELDNO (basetype)
			      = fill_in_vptr_fieldno (basetype);

			  VALUE_TYPE (base) = basetype;
			  vtbl = value_field (base, TYPE_VPTR_FIELDNO (basetype));
			  VALUE_TYPE (vtbl) = lookup_pointer_type (fntype);
			  VALUE_TYPE (arg1) = builtin_type_int;
			  arg1 = value_subscript (vtbl, arg1);
			  VALUE_TYPE (arg1) = fntype;
			  goto got_it;
			}
		}
	      if (i < 0)
		error ("virtual function at index %d not found", fnptr);
	    }
	  else
	    {
	      VALUE_TYPE (arg1) = lookup_pointer_type (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)));
	    }
	got_it:

	  /* Now, say which argument to start evaluating from */
	  tem = 2;
	}
      else if (op == STRUCTOP_STRUCT || op == STRUCTOP_PTR)
	{
	  /* Hair for method invocations */
	  int tem2;

	  nargs = (int) exp->elts[pc + 1].longconst + 1;
	  /* First, evaluate the structure into arg2 */
	  pc2 = (*pos)++;
	  tem2 = strlen (&exp->elts[pc2 + 1].string);
	  *pos += 2 + (tem2 + sizeof (union exp_element)) / sizeof (union exp_element);
	  if (noside == EVAL_SKIP)
	    goto nosideret;

	  if (op == STRUCTOP_STRUCT)
	    {
	      arg2 = evaluate_subexp_for_address (exp, pos, noside);
	    }
	  else
	    {
	      arg2 = evaluate_subexp (0, exp, pos, noside);
	    }
	  /* Now, say which argument to start evaluating from */
	  tem = 2;
	}
      else
	{
	  nargs = (int) exp->elts[pc + 1].longconst;
	  tem = 0;
	}
      argvec = (value *) alloca (sizeof (value) * (nargs + 2));
      for (; tem <= nargs; tem++)
	/* Ensure that array expressions are coerced into pointer objects. */
	argvec[tem] = evaluate_subexp_with_coercion (exp, pos, noside);

      /* signal end of arglist */
      argvec[tem] = 0;

      if (op == STRUCTOP_STRUCT || op == STRUCTOP_PTR)
	{
	  int static_memfuncp;

	  argvec[1] = arg2;
	  argvec[0] =
	    value_struct_elt (arg2, argvec+1, &exp->elts[pc2 + 1].string,
			      &static_memfuncp,
			      op == STRUCTOP_STRUCT
			      ? "structure" : "structure pointer");
	  if (static_memfuncp)
	    {
	      argvec[1] = argvec[0];
	      nargs--;
	      argvec++;
	    }
	}
      else if (op == STRUCTOP_MEMBER || op == STRUCTOP_MPTR)
	{
	  argvec[1] = arg2;
	  argvec[0] = arg1;
	}

      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  /* If the return type doesn't look like a function type, call an
	     error.  This can happen if somebody tries to turn a variable into
	     a function call. This is here because people often want to
	     call, eg, strcmp, which gdb doesn't know is a function.  If
	     gdb isn't asked for it's opinion (ie. through "whatis"),
	     it won't offer it. */

	  struct type *ftype =
	    TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0]));

	  if (ftype)
	    return allocate_value (TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0])));
	  else
	    error ("Expression of type other than \"Function returning ...\" used as function");
	}
      return call_function (argvec[0], nargs, argvec + 1);

    case STRUCTOP_STRUCT:
      tem = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + ((tem + sizeof (union exp_element))
		     / sizeof (union exp_element));

      /* Try to convert "foo.bar" into "(&foo)->bar" so we won't copy
       * the entire contents of a large struct just to extract one
       * value from it. */
      if (noside == EVAL_NORMAL && exp->elts[*pos].opcode == OP_VAR_VALUE
	  && value_has_lval(exp->elts[*pos + 1].symbol))
	arg1 = evaluate_subexp_for_address(exp, pos, noside);
      else
	arg1 = evaluate_subexp (0, exp, pos, noside);

      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  register struct type *type = VALUE_TYPE (arg1);
	  if (TYPE_CODE (type) == TYPE_CODE_PTR)
	    type = TYPE_TARGET_TYPE (type);
	  return value_zero (lookup_struct_elt_type (type,
						     &exp->elts[pc + 1].string),
			     lval_memory);
	}
      else
	return value_struct_elt (arg1, 0, &exp->elts[pc + 1].string, 0,
				 "structure");

    case STRUCTOP_PTR:
      tem = strlen (&exp->elts[pc + 1].string);
      (*pos) += 2 + (tem + sizeof (union exp_element)) / sizeof (union exp_element);
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (lookup_struct_elt_type (TYPE_TARGET_TYPE
						   (VALUE_TYPE (arg1)),
						   &exp->elts[pc + 1].string),
			   lval_memory);
      else
	return value_struct_elt (arg1, 0, &exp->elts[pc + 1].string, 0,
				 "structure pointer");

    case STRUCTOP_MEMBER:
      arg1 = evaluate_subexp_for_address (exp, pos, noside);
      arg2 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      /* Now, convert these values to an address.  */
      if (TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_PTR
	  || ((TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2)))
	       != TYPE_CODE_MEMBER)
	      && (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2)))
		  != TYPE_CODE_METHOD)))
	error ("non-pointer-to-member value used in pointer-to-member construct");
      arg3 = value_from_long (builtin_type_long,
			      value_as_long (arg1) + value_as_long (arg2));
      VALUE_TYPE (arg3) =
	lookup_pointer_type (TYPE_TARGET_TYPE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))));
      return value_ind (arg3);

    case STRUCTOP_MPTR:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      /* Now, convert these values to an address.  */
      if (TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_PTR
	  || (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))) != TYPE_CODE_MEMBER
	      && TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))) != TYPE_CODE_METHOD))
	error ("non-pointer-to-member value used in pointer-to-member construct");
      arg3 = value_from_long (builtin_type_long,
			      value_as_long (arg1) + value_as_long (arg2));
      VALUE_TYPE (arg3) =
	lookup_pointer_type (TYPE_TARGET_TYPE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))));
      return value_ind (arg3);

    case BINOP_ASSIGN:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, 0);
      else
	return value_assign (arg1, arg2);

    case BINOP_ASSIGN_MODIFY:
      (*pos) += 2;
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      op = exp->elts[pc + 1].opcode;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, BINOP_ASSIGN_MODIFY, op);
      else if (op == BINOP_ADD)
	arg2 = value_add (arg1, arg2);
      else if (op == BINOP_SUB)
	arg2 = value_sub (arg1, arg2);
      else
	arg2 = value_binop (arg1, arg2, op);
      return value_assign (arg1, arg2);

    case BINOP_ADD:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, 0);
      else
	return value_add (arg1, arg2);

    case BINOP_SUB:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, 0);
      else
	return value_sub (arg1, arg2);

    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_REM:
    case BINOP_LSH:
    case BINOP_RSH:
    case BINOP_LOGAND:
    case BINOP_LOGIOR:
    case BINOP_LOGXOR:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, 0);
      else
	if (noside == EVAL_AVOID_SIDE_EFFECTS
	    && op == BINOP_DIV)
	  return value_zero (VALUE_TYPE (arg1), not_lval);
      else
	return value_binop (arg1, arg2, op);

    case BINOP_SUBSCRIPT:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)),
			   VALUE_LVAL (arg1));
			   
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, 0);
      else
	return value_subscript (arg1, arg2);
      
    case BINOP_AND:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	{
	  arg2 = evaluate_subexp (0, exp, pos, noside);
	  goto nosideret;
	}
      
      oldpos = *pos;
      arg2 = evaluate_subexp (0, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      *pos = oldpos;
      
      if (binop_user_defined_p (op, arg1, arg2)) 
	{
	  arg2 = evaluate_subexp (0, exp, pos, noside);
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_zerop (arg1);
	  arg2 = evaluate_subexp (0, exp, pos,
				  (tem ? EVAL_SKIP : noside));
	  return value_from_long (builtin_type_int,
				  (LONGEST) (!tem && !value_zerop (arg2)));
	}

    case BINOP_OR:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	{
	  arg2 = evaluate_subexp (0, exp, pos, noside);
	  goto nosideret;
	}
      
      oldpos = *pos;
      arg2 = evaluate_subexp (0, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      *pos = oldpos;
      
      if (binop_user_defined_p (op, arg1, arg2)) 
	{
	  arg2 = evaluate_subexp (0, exp, pos, noside);
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_zerop (arg1);
	  arg2 = evaluate_subexp (0, exp, pos,
				  (!tem ? EVAL_SKIP : noside));
	  return value_from_long (builtin_type_int,
				  (LONGEST) (!tem || !value_zerop (arg2)));
	}

    case BINOP_EQUAL:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_equal (arg1, arg2);
	  return value_from_long (builtin_type_int, (LONGEST) tem);
	}

    case BINOP_NOTEQUAL:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_equal (arg1, arg2);
	  return value_from_long (builtin_type_int, (LONGEST) ! tem);
	}

    case BINOP_LESS:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_less (arg1, arg2);
	  return value_from_long (builtin_type_int, (LONGEST) tem);
	}

    case BINOP_GTR:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_less (arg2, arg1);
	  return value_from_long (builtin_type_int, (LONGEST) tem);
	}

    case BINOP_GEQ:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else
	{
	  tem = value_less (arg1, arg2);
	  return value_from_long (builtin_type_int, (LONGEST) ! tem);
	}

    case BINOP_LEQ:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, 0);
	}
      else 
	{
	  tem = value_less (arg2, arg1);
	  return value_from_long (builtin_type_int, (LONGEST) ! tem);
	}

    case BINOP_REPEAT:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      arg2 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_INT)
	error ("Non-integral right operand for \"@\" operator.");
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return allocate_repeat_value (VALUE_TYPE (arg1),
				       (int) value_as_long (arg2));
      else
	return value_repeat (arg1, (int) value_as_long (arg2));

    case BINOP_COMMA:
      evaluate_subexp (0, exp, pos, noside);
      return evaluate_subexp (0, exp, pos, noside);

    case UNOP_NEG:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op);
      else
	return value_neg (arg1);

    case UNOP_LOGNOT:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op);
      else
	return value_lognot (arg1);

    case UNOP_ZEROP:
      arg1 = evaluate_subexp (0, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op);
      else
	return value_from_long (builtin_type_int,
				(LONGEST) value_zerop (arg1));

    case UNOP_IND:
      if (expect_type && TYPE_CODE (expect_type) == TYPE_CODE_PTR)
        expect_type = TYPE_TARGET_TYPE (expect_type);
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR
	      || TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_REF
	      /* In C you can dereference an array to get the 1st elt.  */
	      || TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_ARRAY
	      )
	    return value_zero (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)),
			       lval_memory);
	  else if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_INT)
	    /* GDB allows dereferencing an int.  */
	    return value_zero (builtin_type_int, lval_memory);
	  else
	    error ("Attempt to take contents of a non-pointer value.");
	}
      return value_ind (arg1);

    case UNOP_ADDR:
      /* C++: check for and handle pointer to members.  */
      
      op = exp->elts[*pos].opcode;

      if (noside == EVAL_SKIP)
	{
	  if (op == OP_SCOPE)
	    {
	      char *name = &exp->elts[pc+3].string;
	      int tem = strlen (name);
	      (*pos) += 2 + (tem + sizeof (union exp_element)) / sizeof (union exp_element);
	    }
	  else
	    evaluate_subexp (expect_type, exp, pos, EVAL_SKIP);
	  goto nosideret;
	}

      if (op == OP_SCOPE)
	{
	  char *name = &exp->elts[pc+3].string;
	  int tem = strlen (name);
	  struct type *domain = exp->elts[pc+2].type;
	  (*pos) += 2 + (tem + sizeof (union exp_element)) / sizeof (union exp_element);
	  arg1 = value_struct_elt_for_address (domain, expect_type, name);
	  if (arg1)
	    return arg1;
	  error ("no field `%s' in structure", name);
	}
      else
	return evaluate_subexp_for_address (exp, pos, noside);

    case UNOP_SIZEOF:
      if (noside == EVAL_SKIP)
	{
	  evaluate_subexp (0, exp, pos, EVAL_SKIP);
	  goto nosideret;
	}
      return evaluate_subexp_for_sizeof (exp, pos);

    case UNOP_CAST:
      (*pos) += 2;
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_cast (exp->elts[pc + 1].type, arg1);

    case UNOP_MEMVAL:
      (*pos) += 2;
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (exp->elts[pc + 1].type, lval_memory);
      else
	return value_at (exp->elts[pc + 1].type,
			 (CORE_ADDR) value_as_long (arg1));

    case UNOP_PREINCREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op);
	}
      else
	{
	  arg2 = value_add (arg1, value_from_long (builtin_type_char, 
						   (LONGEST) 1));
	  return value_assign (arg1, arg2);
	}

    case UNOP_PREDECREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op);
	}
      else
	{
	  arg2 = value_sub (arg1, value_from_long (builtin_type_char, 
						   (LONGEST) 1));
	  return value_assign (arg1, arg2);
	}

    case UNOP_POSTINCREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op);
	}
      else
	{
	  arg2 = value_add (arg1, value_from_long (builtin_type_char, 
						   (LONGEST) 1));
	  value_assign (arg1, arg2);
	  return arg1;
	}

    case UNOP_POSTDECREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op);
	}
      else
	{
	  arg2 = value_sub (arg1, value_from_long (builtin_type_char, 
						   (LONGEST) 1));
	  value_assign (arg1, arg2);
	  return arg1;
	}
	
    case OP_THIS:
      (*pos) += 1;
      return value_of_this (1);

    default:
      error ("internal error: I do not know how to evaluate what you gave me");
    }

 nosideret:
  return value_from_long (builtin_type_long, (LONGEST) 1);
}

/* Evaluate a subexpression of EXP, at index *POS,
   and return the address of that subexpression.
   Advance *POS over the subexpression.
   If the subexpression isn't an lvalue, get an error.
   NOSIDE may be EVAL_AVOID_SIDE_EFFECTS;
   then only the type of the result need be correct.  */

static value
evaluate_subexp_for_address (exp, pos, noside)
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  enum exp_opcode op;
  register int pc;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case UNOP_IND:
      (*pos)++;
      return evaluate_subexp (0, exp, pos, noside);

    case UNOP_MEMVAL:
      (*pos) += 3;
      return value_cast (lookup_pointer_type (exp->elts[pc + 1].type),
			 evaluate_subexp (0, exp, pos, noside));

    case OP_VAR_VALUE:
      (*pos) += 3;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct type *type =
	    lookup_pointer_type (SYMBOL_TYPE (exp->elts[pc + 1].symbol));
	  enum address_class sym_class =
	    SYMBOL_CLASS (exp->elts[pc + 1].symbol);

	  if (sym_class == LOC_CONST
	      || sym_class == LOC_CONST_BYTES
	      || sym_class == LOC_REGISTER
	      || sym_class == LOC_REGPARM)
	    error ("Attempt to take address of register or constant.");

	return
	  value_zero (type, not_lval);
	}
      else
	return locate_var_value (exp->elts[pc + 1].symbol, (CORE_ADDR) 0);

    default:
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  value x = evaluate_subexp (0, exp, pos, noside);
	  if (VALUE_LVAL (x) == lval_memory)
	    return value_zero (TYPE_POINTER_TYPE (VALUE_TYPE (x)),
			       not_lval);
	  else
	    error ("Attempt to take address of non-lval");
	}
      return value_addr (evaluate_subexp (0, exp, pos, noside));
    }
}

/* Evaluate like `evaluate_subexp' except coercing arrays to pointers.
   When used in contexts where arrays will be coerced anyway,
   this is equivalent to `evaluate_subexp'
   but much faster because it avoids actually fetching array contents.  */

static value
evaluate_subexp_with_coercion (exp, pos, noside)
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  register enum exp_opcode op;
  register int pc;
  register value val;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case OP_VAR_VALUE:
      if (TYPE_CODE (SYMBOL_TYPE (exp->elts[pc + 1].symbol)) == TYPE_CODE_ARRAY)
	{
	  (*pos) += 3;
	  val = locate_var_value (exp->elts[pc + 1].symbol, (CORE_ADDR) 0);
	  return value_cast (lookup_pointer_type (TYPE_TARGET_TYPE (SYMBOL_TYPE (exp->elts[pc + 1].symbol))),
			     val);
	}
    }

  return evaluate_subexp (0, exp, pos, noside);
}

/* Evaluate a subexpression of EXP, at index *POS,
   and return a value for the size of that subexpression.
   Advance *POS over the subexpression.  */

static value
evaluate_subexp_for_sizeof (exp, pos)
     register struct expression *exp;
     register int *pos;
{
  enum exp_opcode op;
  register int pc;
  value val;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
      /* This case is handled specially
	 so that we avoid creating a value for the result type.
	 If the result type is very big, it's desirable not to
	 create a value unnecessarily.  */
    case UNOP_IND:
      (*pos)++;
      val = evaluate_subexp (0, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      return value_from_long (builtin_type_int, (LONGEST)
		      TYPE_LENGTH (TYPE_TARGET_TYPE (VALUE_TYPE (val))));

    case UNOP_MEMVAL:
      (*pos) += 3;
      return value_from_long (builtin_type_int, 
			      (LONGEST) TYPE_LENGTH (exp->elts[pc + 1].type));

    case OP_VAR_VALUE:
      (*pos) += 3;
      return value_from_long (builtin_type_int,
	 (LONGEST) TYPE_LENGTH (SYMBOL_TYPE (exp->elts[pc + 1].symbol)));

    default:
      val = evaluate_subexp (0, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      return value_from_long (builtin_type_int,
			      (LONGEST) TYPE_LENGTH (VALUE_TYPE (val)));
    }
}
