/* Perform arithmetic and other operations on values, for GDB.
   Copyright 1986, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "value.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "target.h"
#include "language.h"
#include <string.h>

/* Define whether or not the C operator '/' truncates towards zero for
   differently signed operands (truncation direction is undefined in C). */

#ifndef TRUNCATION_TOWARDS_ZERO
#define TRUNCATION_TOWARDS_ZERO ((-5 / 2) == -2)
#endif

static value
value_subscripted_rvalue PARAMS ((value, value));


value
value_add (arg1, arg2)
	value arg1, arg2;
{
  register value valint, valptr;
  register int len;

  COERCE_ARRAY (arg1);
  COERCE_ARRAY (arg2);

  if ((TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR
       || TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_PTR)
      &&
      (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_INT
       || TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_INT))
    /* Exactly one argument is a pointer, and one is an integer.  */
    {
      if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR)
	{
	  valptr = arg1;
	  valint = arg2;
	}
      else
	{
	  valptr = arg2;
	  valint = arg1;
	}
      len = TYPE_LENGTH (TYPE_TARGET_TYPE (VALUE_TYPE (valptr)));
      if (len == 0) len = 1;	/* For (void *) */
      return value_from_longest (VALUE_TYPE (valptr),
			      value_as_long (valptr)
			      + (len * value_as_long (valint)));
    }

  return value_binop (arg1, arg2, BINOP_ADD);
}

value
value_sub (arg1, arg2)
	value arg1, arg2;
{

  COERCE_ARRAY (arg1);
  COERCE_ARRAY (arg2);

  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR)
    {
      if (TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_INT)
	{
	  /* pointer - integer.  */
	  return value_from_longest
	    (VALUE_TYPE (arg1),
	     value_as_long (arg1)
	     - (TYPE_LENGTH (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)))
		* value_as_long (arg2)));
	}
      else if (VALUE_TYPE (arg1) == VALUE_TYPE (arg2))
	{
	  /* pointer to <type x> - pointer to <type x>.  */
	  return value_from_longest
	    (builtin_type_long,		/* FIXME -- should be ptrdiff_t */
	     (value_as_long (arg1) - value_as_long (arg2))
	     / TYPE_LENGTH (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))));
	}
      else
	{
	  error ("\
First argument of `-' is a pointer and second argument is neither\n\
an integer nor a pointer of the same type.");
	}
    }

  return value_binop (arg1, arg2, BINOP_SUB);
}

/* Return the value of ARRAY[IDX].
   See comments in value_coerce_array() for rationale for reason for
   doing lower bounds adjustment here rather than there.
   FIXME:  Perhaps we should validate that the index is valid and if
   verbosity is set, warn about invalid indices (but still use them). */

value
value_subscript (array, idx)
     value array, idx;
{
  int lowerbound;
  value bound;
  struct type *range_type;

  COERCE_REF (array);

  if (TYPE_CODE (VALUE_TYPE (array)) == TYPE_CODE_ARRAY)
    {
      range_type = TYPE_FIELD_TYPE (VALUE_TYPE (array), 0);
      lowerbound = TYPE_FIELD_BITPOS (range_type, 0);
      if (lowerbound != 0)
	{
	  bound = value_from_longest (builtin_type_int, (LONGEST) lowerbound);
	  idx = value_sub (idx, bound);
	}
      if (VALUE_LVAL (array) != lval_memory)
	{
	  return value_subscripted_rvalue (array, idx);
	}
    }
  return value_ind (value_add (array, idx));
}

/* Return the value of EXPR[IDX], expr an aggregate rvalue
   (eg, a vector register).  This routine used to promote floats
   to doubles, but no longer does.  */

static value
value_subscripted_rvalue (array, idx)
     value array, idx;
{
  struct type *elt_type = TYPE_TARGET_TYPE (VALUE_TYPE (array));
  int elt_size = TYPE_LENGTH (elt_type);
  int elt_offs = elt_size * longest_to_int (value_as_long (idx));
  value v;

  if (elt_offs >= TYPE_LENGTH (VALUE_TYPE (array)))
    error ("no such vector element");

  v = allocate_value (elt_type);
  memcpy (VALUE_CONTENTS (v), VALUE_CONTENTS (array) + elt_offs, elt_size);

  if (VALUE_LVAL (array) == lval_internalvar)
    VALUE_LVAL (v) = lval_internalvar_component;
  else
    VALUE_LVAL (v) = not_lval;
  VALUE_ADDRESS (v) = VALUE_ADDRESS (array);
  VALUE_OFFSET (v) = VALUE_OFFSET (array) + elt_offs;
  VALUE_BITSIZE (v) = elt_size * 8;
  return v;
}

/* Check to see if either argument is a structure.  This is called so
   we know whether to go ahead with the normal binop or look for a 
   user defined function instead.

   For now, we do not overload the `=' operator.  */

int
binop_user_defined_p (op, arg1, arg2)
     enum exp_opcode op;
     value arg1, arg2;
{
  if (op == BINOP_ASSIGN)
    return 0;
  return (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_STRUCT
	  || TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_STRUCT
	  || (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_REF
	      && TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_STRUCT)
	  || (TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_REF
	      && TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))) == TYPE_CODE_STRUCT));
}

/* Check to see if argument is a structure.  This is called so
   we know whether to go ahead with the normal unop or look for a 
   user defined function instead.

   For now, we do not overload the `&' operator.  */

int unop_user_defined_p (op, arg1)
     enum exp_opcode op;
     value arg1;
{
  if (op == UNOP_ADDR)
    return 0;
  return (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_STRUCT
	  || (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_REF
	      && TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_STRUCT));
}

/* We know either arg1 or arg2 is a structure, so try to find the right
   user defined function.  Create an argument vector that calls 
   arg1.operator @ (arg1,arg2) and return that value (where '@' is any
   binary operator which is legal for GNU C++).

   OP is the operatore, and if it is BINOP_ASSIGN_MODIFY, then OTHEROP
   is the opcode saying how to modify it.  Otherwise, OTHEROP is
   unused.  */

value
value_x_binop (arg1, arg2, op, otherop)
     value arg1, arg2;
     enum exp_opcode op, otherop;
{
  value * argvec;
  char *ptr;
  char tstr[13];
  int static_memfuncp;

  COERCE_REF (arg1);
  COERCE_REF (arg2);
  COERCE_ENUM (arg1);
  COERCE_ENUM (arg2);

  /* now we know that what we have to do is construct our
     arg vector and find the right function to call it with.  */

  if (TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_STRUCT)
    error ("Can't do that binary op on that type");  /* FIXME be explicit */

  argvec = (value *) alloca (sizeof (value) * 4);
  argvec[1] = value_addr (arg1);
  argvec[2] = arg2;
  argvec[3] = 0;

  /* make the right function name up */  
  strcpy(tstr, "operator__");
  ptr = tstr+8;
  switch (op)
    {
    case BINOP_ADD:		strcpy(ptr,"+"); break;
    case BINOP_SUB:		strcpy(ptr,"-"); break;
    case BINOP_MUL:		strcpy(ptr,"*"); break;
    case BINOP_DIV:		strcpy(ptr,"/"); break;
    case BINOP_REM:		strcpy(ptr,"%"); break;
    case BINOP_LSH:		strcpy(ptr,"<<"); break;
    case BINOP_RSH:		strcpy(ptr,">>"); break;
    case BINOP_BITWISE_AND:	strcpy(ptr,"&"); break;
    case BINOP_BITWISE_IOR:	strcpy(ptr,"|"); break;
    case BINOP_BITWISE_XOR:	strcpy(ptr,"^"); break;
    case BINOP_LOGICAL_AND:	strcpy(ptr,"&&"); break;
    case BINOP_LOGICAL_OR:	strcpy(ptr,"||"); break;
    case BINOP_MIN:		strcpy(ptr,"<?"); break;
    case BINOP_MAX:		strcpy(ptr,">?"); break;
    case BINOP_ASSIGN:		strcpy(ptr,"="); break;
    case BINOP_ASSIGN_MODIFY:	
      switch (otherop)
	{
	case BINOP_ADD:		strcpy(ptr,"+="); break;
	case BINOP_SUB:		strcpy(ptr,"-="); break;
	case BINOP_MUL:		strcpy(ptr,"*="); break;
	case BINOP_DIV:		strcpy(ptr,"/="); break;
	case BINOP_REM:		strcpy(ptr,"%="); break;
	case BINOP_BITWISE_AND:	strcpy(ptr,"&="); break;
	case BINOP_BITWISE_IOR:	strcpy(ptr,"|="); break;
	case BINOP_BITWISE_XOR:	strcpy(ptr,"^="); break;
	case BINOP_MOD:		/* invalid */
	default:
	  error ("Invalid binary operation specified.");
	}
      break;
    case BINOP_SUBSCRIPT: strcpy(ptr,"[]"); break;
    case BINOP_EQUAL:	  strcpy(ptr,"=="); break;
    case BINOP_NOTEQUAL:  strcpy(ptr,"!="); break;
    case BINOP_LESS:      strcpy(ptr,"<"); break;
    case BINOP_GTR:       strcpy(ptr,">"); break;
    case BINOP_GEQ:       strcpy(ptr,">="); break;
    case BINOP_LEQ:       strcpy(ptr,"<="); break;
    case BINOP_MOD:	  /* invalid */
    default:
      error ("Invalid binary operation specified.");
    }
  argvec[0] = value_struct_elt (&arg1, argvec+1, tstr, &static_memfuncp, "structure");
  if (argvec[0])
    {
      if (static_memfuncp)
	{
	  argvec[1] = argvec[0];
	  argvec++;
	}
      return call_function_by_hand (argvec[0], 2 - static_memfuncp, argvec + 1);
    }
  error ("member function %s not found", tstr);
#ifdef lint
  return call_function_by_hand (argvec[0], 2 - static_memfuncp, argvec + 1);
#endif
}

/* We know that arg1 is a structure, so try to find a unary user
   defined operator that matches the operator in question.  
   Create an argument vector that calls arg1.operator @ (arg1)
   and return that value (where '@' is (almost) any unary operator which
   is legal for GNU C++).  */

value
value_x_unop (arg1, op)
     value arg1;
     enum exp_opcode op;
{
  value * argvec;
  char *ptr;
  char tstr[13];
  int static_memfuncp;

  COERCE_ENUM (arg1);

  /* now we know that what we have to do is construct our
     arg vector and find the right function to call it with.  */

  if (TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_STRUCT)
    error ("Can't do that unary op on that type");  /* FIXME be explicit */

  argvec = (value *) alloca (sizeof (value) * 3);
  argvec[1] = value_addr (arg1);
  argvec[2] = 0;

  /* make the right function name up */  
  strcpy(tstr,"operator__");
  ptr = tstr+8;
  switch (op)
    {
    case UNOP_PREINCREMENT:	strcpy(ptr,"++"); break;
    case UNOP_PREDECREMENT:	strcpy(ptr,"++"); break;
    case UNOP_POSTINCREMENT:	strcpy(ptr,"++"); break;
    case UNOP_POSTDECREMENT:	strcpy(ptr,"++"); break;
    case UNOP_LOGICAL_NOT:	strcpy(ptr,"!"); break;
    case UNOP_COMPLEMENT:	strcpy(ptr,"~"); break;
    case UNOP_NEG:		strcpy(ptr,"-"); break;
    default:
      error ("Invalid binary operation specified.");
    }
  argvec[0] = value_struct_elt (&arg1, argvec+1, tstr, &static_memfuncp, "structure");
  if (argvec[0])
    {
      if (static_memfuncp)
	{
	  argvec[1] = argvec[0];
	  argvec++;
	}
      return call_function_by_hand (argvec[0], 1 - static_memfuncp, argvec + 1);
    }
  error ("member function %s not found", tstr);
  return 0;  /* For lint -- never reached */
}


/* Concatenate two values with the following conditions:

   (1)	Both values must be either bitstring values or character string
	values and the resulting value consists of the concatenation of
	ARG1 followed by ARG2.

	or

	One value must be an integer value and the other value must be
	either a bitstring value or character string value, which is
	to be repeated by the number of times specified by the integer
	value.


    (2)	Boolean values are also allowed and are treated as bit string
    	values of length 1.

    (3)	Character values are also allowed and are treated as character
    	string values of length 1.
*/

value
value_concat (arg1, arg2)
     value arg1, arg2;
{
  register value inval1, inval2, outval;
  int inval1len, inval2len;
  int count, idx;
  char *ptr;
  char inchar;

  /* First figure out if we are dealing with two values to be concatenated
     or a repeat count and a value to be repeated.  INVAL1 is set to the
     first of two concatenated values, or the repeat count.  INVAL2 is set
     to the second of the two concatenated values or the value to be 
     repeated. */

  if (TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_INT)
    {
      inval1 = arg2;
      inval2 = arg1;
    }
  else
    {
      inval1 = arg1;
      inval2 = arg2;
    }

  /* Now process the input values. */

  if (TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_INT)
    {
      /* We have a repeat count.  Validate the second value and then
	 construct a value repeated that many times. */
      if (TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_STRING
	  || TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_CHAR)
	{
	  count = longest_to_int (value_as_long (inval1));
	  inval2len = TYPE_LENGTH (VALUE_TYPE (inval2));
	  ptr = (char *) alloca (count * inval2len);
	  if (TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_CHAR)
	    {
	      inchar = (char) unpack_long (VALUE_TYPE (inval2),
					   VALUE_CONTENTS (inval2));
	      for (idx = 0; idx < count; idx++)
		{
		  *(ptr + idx) = inchar;
		}
	    }
	  else
	    {
	      for (idx = 0; idx < count; idx++)
		{
		  memcpy (ptr + (idx * inval2len), VALUE_CONTENTS (inval2),
			  inval2len);
		}
	    }
	  outval = value_string (ptr, count * inval2len);
	}
      else if (TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_BITSTRING
	       || TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_BOOL)
	{
	  error ("unimplemented support for bitstring/boolean repeats");
	}
      else
	{
	  error ("can't repeat values of that type");
	}
    }
  else if (TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_STRING
      || TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_CHAR)
    {
      /* We have two character strings to concatenate. */
      if (TYPE_CODE (VALUE_TYPE (inval2)) != TYPE_CODE_STRING
	  && TYPE_CODE (VALUE_TYPE (inval2)) != TYPE_CODE_CHAR)
	{
	  error ("Strings can only be concatenated with other strings.");
	}
      inval1len = TYPE_LENGTH (VALUE_TYPE (inval1));
      inval2len = TYPE_LENGTH (VALUE_TYPE (inval2));
      ptr = (char *) alloca (inval1len + inval2len);
      if (TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_CHAR)
	{
	  *ptr = (char) unpack_long (VALUE_TYPE (inval1), VALUE_CONTENTS (inval1));
	}
      else
	{
	  memcpy (ptr, VALUE_CONTENTS (inval1), inval1len);
	}
      if (TYPE_CODE (VALUE_TYPE (inval2)) == TYPE_CODE_CHAR)
	{
	  *(ptr + inval1len) = 
	    (char) unpack_long (VALUE_TYPE (inval2), VALUE_CONTENTS (inval2));
	}
      else
	{
	  memcpy (ptr + inval1len, VALUE_CONTENTS (inval2), inval2len);
	}
      outval = value_string (ptr, inval1len + inval2len);
    }
  else if (TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_BITSTRING
	   || TYPE_CODE (VALUE_TYPE (inval1)) == TYPE_CODE_BOOL)
    {
      /* We have two bitstrings to concatenate. */
      if (TYPE_CODE (VALUE_TYPE (inval2)) != TYPE_CODE_BITSTRING
	  && TYPE_CODE (VALUE_TYPE (inval2)) != TYPE_CODE_BOOL)
	{
	  error ("Bitstrings or booleans can only be concatenated with other bitstrings or booleans.");
	}
      error ("unimplemented support for bitstring/boolean concatenation.");
    }      
  else
    {
      /* We don't know how to concatenate these operands. */
      error ("illegal operands for concatenation.");
    }
  return (outval);
}


/* Perform a binary operation on two operands which have reasonable
   representations as integers or floats.  This includes booleans,
   characters, integers, or floats.
   Does not support addition and subtraction on pointers;
   use value_add or value_sub if you want to handle those possibilities.  */

value
value_binop (arg1, arg2, op)
     value arg1, arg2;
     enum exp_opcode op;
{
  register value val;

  COERCE_ENUM (arg1);
  COERCE_ENUM (arg2);

  if ((TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_FLT
       &&
       TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_CHAR
       &&
       TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_INT
       &&
       TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_BOOL)
      ||
      (TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_FLT
       &&
       TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_CHAR
       &&
       TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_INT
       &&
       TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_BOOL))
    error ("Argument to arithmetic operation not a number or boolean.");

  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_FLT
      ||
      TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_FLT)
    {
      double v1, v2, v;
      v1 = value_as_double (arg1);
      v2 = value_as_double (arg2);
      switch (op)
	{
	case BINOP_ADD:
	  v = v1 + v2;
	  break;

	case BINOP_SUB:
	  v = v1 - v2;
	  break;

	case BINOP_MUL:
	  v = v1 * v2;
	  break;

	case BINOP_DIV:
	  v = v1 / v2;
	  break;

	default:
	  error ("Integer-only operation on floating point number.");
	}

      val = allocate_value (builtin_type_double);
      SWAP_TARGET_AND_HOST (&v, sizeof (v));
      *(double *) VALUE_CONTENTS_RAW (val) = v;
    }
  else if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_BOOL
	   &&
	   TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_BOOL)
      {
	  LONGEST v1, v2, v;
	  v1 = value_as_long (arg1);
	  v2 = value_as_long (arg2);
	  
	  switch (op)
	    {
	    case BINOP_BITWISE_AND:
	      v = v1 & v2;
	      break;
	      
	    case BINOP_BITWISE_IOR:
	      v = v1 | v2;
	      break;
	      
	    case BINOP_BITWISE_XOR:
	      v = v1 ^ v2;
	      break;
	      
	    default:
	      error ("Invalid operation on booleans.");
	    }
	  
	  val = allocate_value (builtin_type_chill_bool);
	  store_signed_integer (VALUE_CONTENTS_RAW (val),
				TYPE_LENGTH (VALUE_TYPE (val)),
				v);
      }
  else
    /* Integral operations here.  */
    /* FIXME:  Also mixed integral/booleans, with result an integer. */
    {
      /* Should we promote to unsigned longest?  */
      if ((TYPE_UNSIGNED (VALUE_TYPE (arg1))
	   || TYPE_UNSIGNED (VALUE_TYPE (arg2)))
	  && (TYPE_LENGTH (VALUE_TYPE (arg1)) >= sizeof (unsigned LONGEST)
	      || TYPE_LENGTH (VALUE_TYPE (arg2)) >= sizeof (unsigned LONGEST)))
	{
	  unsigned LONGEST v1, v2, v;
	  v1 = (unsigned LONGEST) value_as_long (arg1);
	  v2 = (unsigned LONGEST) value_as_long (arg2);
	  
	  switch (op)
	    {
	    case BINOP_ADD:
	      v = v1 + v2;
	      break;
	      
	    case BINOP_SUB:
	      v = v1 - v2;
	      break;
	      
	    case BINOP_MUL:
	      v = v1 * v2;
	      break;
	      
	    case BINOP_DIV:
	      v = v1 / v2;
	      break;
	      
	    case BINOP_REM:
	      v = v1 % v2;
	      break;
	      
	    case BINOP_MOD:
	      /* Knuth 1.2.4, integer only.  Note that unlike the C '%' op,
	         v1 mod 0 has a defined value, v1. */
	      /* Chill specifies that v2 must be > 0, so check for that. */
	      if (current_language -> la_language == language_chill
		  && value_as_long (arg2) <= 0)
		{
		  error ("Second operand of MOD must be greater than zero.");
		}
	      if (v2 == 0)
		{
		  v = v1;
		}
	      else
		{
		  v = v1/v2;
		  /* Note floor(v1/v2) == v1/v2 for unsigned. */
		  v = v1 - (v2 * v);
		}
	      break;
	      
	    case BINOP_LSH:
	      v = v1 << v2;
	      break;
	      
	    case BINOP_RSH:
	      v = v1 >> v2;
	      break;
	      
	    case BINOP_BITWISE_AND:
	      v = v1 & v2;
	      break;
	      
	    case BINOP_BITWISE_IOR:
	      v = v1 | v2;
	      break;
	      
	    case BINOP_BITWISE_XOR:
	      v = v1 ^ v2;
	      break;
	      
	    case BINOP_LOGICAL_AND:
	      v = v1 && v2;
	      break;
	      
	    case BINOP_LOGICAL_OR:
	      v = v1 || v2;
	      break;
	      
	    case BINOP_MIN:
	      v = v1 < v2 ? v1 : v2;
	      break;
	      
	    case BINOP_MAX:
	      v = v1 > v2 ? v1 : v2;
	      break;
	      
	    default:
	      error ("Invalid binary operation on numbers.");
	    }

	  val = allocate_value (BUILTIN_TYPE_UNSIGNED_LONGEST);
	  store_unsigned_integer (VALUE_CONTENTS_RAW (val),
				  TYPE_LENGTH (VALUE_TYPE (val)),
				  v);
	}
      else
	{
	  LONGEST v1, v2, v;
	  v1 = value_as_long (arg1);
	  v2 = value_as_long (arg2);
	  
	  switch (op)
	    {
	    case BINOP_ADD:
	      v = v1 + v2;
	      break;
	      
	    case BINOP_SUB:
	      v = v1 - v2;
	      break;
	      
	    case BINOP_MUL:
	      v = v1 * v2;
	      break;
	      
	    case BINOP_DIV:
	      v = v1 / v2;
	      break;
	      
	    case BINOP_REM:
	      v = v1 % v2;
	      break;
	      
	    case BINOP_MOD:
	      /* Knuth 1.2.4, integer only.  Note that unlike the C '%' op,
	         X mod 0 has a defined value, X. */
	      /* Chill specifies that v2 must be > 0, so check for that. */
	      if (current_language -> la_language == language_chill
		  && v2 <= 0)
		{
		  error ("Second operand of MOD must be greater than zero.");
		}
	      if (v2 == 0)
		{
		  v = v1;
		}
	      else
		{
		  v = v1/v2;
		  /* Compute floor. */
		  if (TRUNCATION_TOWARDS_ZERO && (v < 0) && ((v1 % v2) != 0))
		    {
		      v--;
		    }
		  v = v1 - (v2 * v);
		}
	      break;
	      
	    case BINOP_LSH:
	      v = v1 << v2;
	      break;
	      
	    case BINOP_RSH:
	      v = v1 >> v2;
	      break;
	      
	    case BINOP_BITWISE_AND:
	      v = v1 & v2;
	      break;
	      
	    case BINOP_BITWISE_IOR:
	      v = v1 | v2;
	      break;
	      
	    case BINOP_BITWISE_XOR:
	      v = v1 ^ v2;
	      break;
	      
	    case BINOP_LOGICAL_AND:
	      v = v1 && v2;
	      break;
	      
	    case BINOP_LOGICAL_OR:
	      v = v1 || v2;
	      break;
	      
	    case BINOP_MIN:
	      v = v1 < v2 ? v1 : v2;
	      break;
	      
	    case BINOP_MAX:
	      v = v1 > v2 ? v1 : v2;
	      break;
	      
	    default:
	      error ("Invalid binary operation on numbers.");
	    }
	  
	  val = allocate_value (BUILTIN_TYPE_LONGEST);
	  store_signed_integer (VALUE_CONTENTS_RAW (val),
				TYPE_LENGTH (VALUE_TYPE (val)),
				v);
	}
    }

  return val;
}

/* Simulate the C operator ! -- return 1 if ARG1 contains zero.  */

int
value_logical_not (arg1)
     value arg1;
{
  register int len;
  register char *p;

  COERCE_ARRAY (arg1);

  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_FLT)
    return 0 == value_as_double (arg1);

  len = TYPE_LENGTH (VALUE_TYPE (arg1));
  p = VALUE_CONTENTS (arg1);

  while (--len >= 0)
    {
      if (*p++)
	break;
    }

  return len < 0;
}

/* Simulate the C operator == by returning a 1
   iff ARG1 and ARG2 have equal contents.  */

int
value_equal (arg1, arg2)
     register value arg1, arg2;

{
  register int len;
  register char *p1, *p2;
  enum type_code code1;
  enum type_code code2;

  COERCE_ARRAY (arg1);
  COERCE_ARRAY (arg2);

  code1 = TYPE_CODE (VALUE_TYPE (arg1));
  code2 = TYPE_CODE (VALUE_TYPE (arg2));

  if (code1 == TYPE_CODE_INT && code2 == TYPE_CODE_INT)
    return value_as_long (arg1) == value_as_long (arg2);
  else if ((code1 == TYPE_CODE_FLT || code1 == TYPE_CODE_INT)
	   && (code2 == TYPE_CODE_FLT || code2 == TYPE_CODE_INT))
    return value_as_double (arg1) == value_as_double (arg2);

  /* FIXME: Need to promote to either CORE_ADDR or LONGEST, whichever
     is bigger.  */
  else if (code1 == TYPE_CODE_PTR && code2 == TYPE_CODE_INT)
    return value_as_pointer (arg1) == (CORE_ADDR) value_as_long (arg2);
  else if (code2 == TYPE_CODE_PTR && code1 == TYPE_CODE_INT)
    return (CORE_ADDR) value_as_long (arg1) == value_as_pointer (arg2);

  else if (code1 == code2
	   && ((len = TYPE_LENGTH (VALUE_TYPE (arg1)))
	       == TYPE_LENGTH (VALUE_TYPE (arg2))))
    {
      p1 = VALUE_CONTENTS (arg1);
      p2 = VALUE_CONTENTS (arg2);
      while (--len >= 0)
	{
	  if (*p1++ != *p2++)
	    break;
	}
      return len < 0;
    }
  else
    {
      error ("Invalid type combination in equality test.");
      return 0;  /* For lint -- never reached */
    }
}

/* Simulate the C operator < by returning 1
   iff ARG1's contents are less than ARG2's.  */

int
value_less (arg1, arg2)
     register value arg1, arg2;
{
  register enum type_code code1;
  register enum type_code code2;

  COERCE_ARRAY (arg1);
  COERCE_ARRAY (arg2);

  code1 = TYPE_CODE (VALUE_TYPE (arg1));
  code2 = TYPE_CODE (VALUE_TYPE (arg2));

  if (code1 == TYPE_CODE_INT && code2 == TYPE_CODE_INT)
    {
      if (TYPE_UNSIGNED (VALUE_TYPE (arg1))
       || TYPE_UNSIGNED (VALUE_TYPE (arg2)))
	return ((unsigned LONGEST) value_as_long (arg1)
		< (unsigned LONGEST) value_as_long (arg2));
      else
	return value_as_long (arg1) < value_as_long (arg2);
    }
  else if ((code1 == TYPE_CODE_FLT || code1 == TYPE_CODE_INT)
	   && (code2 == TYPE_CODE_FLT || code2 == TYPE_CODE_INT))
    return value_as_double (arg1) < value_as_double (arg2);
  else if (code1 == TYPE_CODE_PTR && code2 == TYPE_CODE_PTR)
    return value_as_pointer (arg1) < value_as_pointer (arg2);

  /* FIXME: Need to promote to either CORE_ADDR or LONGEST, whichever
     is bigger.  */
  else if (code1 == TYPE_CODE_PTR && code2 == TYPE_CODE_INT)
    return value_as_pointer (arg1) < (CORE_ADDR) value_as_long (arg2);
  else if (code2 == TYPE_CODE_PTR && code1 == TYPE_CODE_INT)
    return (CORE_ADDR) value_as_long (arg1) < value_as_pointer (arg2);

  else
    {
      error ("Invalid type combination in ordering comparison.");
      return 0;
    }
}

/* The unary operators - and ~.  Both free the argument ARG1.  */

value
value_neg (arg1)
     register value arg1;
{
  register struct type *type;

  COERCE_ENUM (arg1);

  type = VALUE_TYPE (arg1);

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    return value_from_double (type, - value_as_double (arg1));
  else if (TYPE_CODE (type) == TYPE_CODE_INT)
    return value_from_longest (type, - value_as_long (arg1));
  else {
    error ("Argument to negate operation not a number.");
    return 0;  /* For lint -- never reached */
  }
}

value
value_complement (arg1)
     register value arg1;
{
  COERCE_ENUM (arg1);

  if (TYPE_CODE (VALUE_TYPE (arg1)) != TYPE_CODE_INT)
    error ("Argument to complement operation not an integer.");

  return value_from_longest (VALUE_TYPE (arg1), ~ value_as_long (arg1));
}

