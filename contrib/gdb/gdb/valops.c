/* Perform non-arithmetic operations on values, for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "demangle.h"
#include "language.h"

#include <errno.h>
#include "gdb_string.h"

/* Default to coercing float to double in function calls only when there is
   no prototype.  Otherwise on targets where the debug information is incorrect
   for either the prototype or non-prototype case, we can force it by defining
   COERCE_FLOAT_TO_DOUBLE in the target configuration file. */

#ifndef COERCE_FLOAT_TO_DOUBLE
#define COERCE_FLOAT_TO_DOUBLE (param_type == NULL)
#endif

/* Local functions.  */

static int typecmp PARAMS ((int staticp, struct type *t1[], value_ptr t2[]));

static CORE_ADDR find_function_addr PARAMS ((value_ptr, struct type **));

static CORE_ADDR value_push PARAMS ((CORE_ADDR, value_ptr));

static value_ptr search_struct_field PARAMS ((char *, value_ptr, int,
					      struct type *, int));

static value_ptr search_struct_method PARAMS ((char *, value_ptr *,
					       value_ptr *,
					       int, int *, struct type *));

static int check_field_in PARAMS ((struct type *, const char *));

static CORE_ADDR allocate_space_in_inferior PARAMS ((int));

static value_ptr cast_into_complex PARAMS ((struct type *, value_ptr));

#define VALUE_SUBSTRING_START(VAL) VALUE_FRAME(VAL)

/* Flag for whether we want to abandon failed expression evals by default.  */

#if 0
static int auto_abandon = 0;
#endif


/* Find the address of function name NAME in the inferior.  */

value_ptr
find_function_in_inferior (name)
     char *name;
{
  register struct symbol *sym;
  sym = lookup_symbol (name, 0, VAR_NAMESPACE, 0, NULL);
  if (sym != NULL)
    {
      if (SYMBOL_CLASS (sym) != LOC_BLOCK)
	{
	  error ("\"%s\" exists in this program but is not a function.",
		 name);
	}
      return value_of_variable (sym, NULL);
    }
  else
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol(name, NULL, NULL);
      if (msymbol != NULL)
	{
	  struct type *type;
	  LONGEST maddr;
	  type = lookup_pointer_type (builtin_type_char);
	  type = lookup_function_type (type);
	  type = lookup_pointer_type (type);
	  maddr = (LONGEST) SYMBOL_VALUE_ADDRESS (msymbol);
	  return value_from_longest (type, maddr);
	}
      else
	{
	  error ("evaluation of this expression requires the program to have a function \"%s\".", name);
	}
    }
}

/* Allocate NBYTES of space in the inferior using the inferior's malloc
   and return a value that is a pointer to the allocated space. */

value_ptr
value_allocate_space_in_inferior (len)
     int len;
{
  value_ptr blocklen;
  register value_ptr val = find_function_in_inferior ("malloc");

  blocklen = value_from_longest (builtin_type_int, (LONGEST) len);
  val = call_function_by_hand (val, 1, &blocklen);
  if (value_logical_not (val))
    {
      error ("No memory available to program.");
    }
  return val;
}

static CORE_ADDR
allocate_space_in_inferior (len)
     int len;
{
  return value_as_long (value_allocate_space_in_inferior (len));
}

/* Cast value ARG2 to type TYPE and return as a value.
   More general than a C cast: accepts any two types of the same length,
   and if ARG2 is an lvalue it can be cast into anything at all.  */
/* In C++, casts may change pointer or object representations.  */

value_ptr
value_cast (type, arg2)
     struct type *type;
     register value_ptr arg2;
{
  register enum type_code code1;
  register enum type_code code2;
  register int scalar;
  struct type *type2;

  if (VALUE_TYPE (arg2) == type)
    return arg2;

  CHECK_TYPEDEF (type);
  code1 = TYPE_CODE (type);
  COERCE_REF(arg2);
  type2 = check_typedef (VALUE_TYPE (arg2));

  /* A cast to an undetermined-length array_type, such as (TYPE [])OBJECT,
     is treated like a cast to (TYPE [N])OBJECT,
     where N is sizeof(OBJECT)/sizeof(TYPE). */
  if (code1 == TYPE_CODE_ARRAY)
    {
      struct type *element_type = TYPE_TARGET_TYPE (type);
      unsigned element_length = TYPE_LENGTH (check_typedef (element_type));
      if (element_length > 0
	  && TYPE_ARRAY_UPPER_BOUND_TYPE (type) == BOUND_CANNOT_BE_DETERMINED)
	{
	  struct type *range_type = TYPE_INDEX_TYPE (type);
	  int val_length = TYPE_LENGTH (type2);
	  LONGEST low_bound, high_bound, new_length;
	  if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
	    low_bound = 0, high_bound = 0;
	  new_length = val_length / element_length;
	  if (val_length % element_length != 0)
       warning("array element type size does not divide object size in cast");
	  /* FIXME-type-allocation: need a way to free this type when we are
	     done with it.  */
	  range_type = create_range_type ((struct type *) NULL,
					  TYPE_TARGET_TYPE (range_type),
					  low_bound,
					  new_length + low_bound - 1);
	  VALUE_TYPE (arg2) = create_array_type ((struct type *) NULL,
						 element_type, range_type);
	  return arg2;
	}
    }

  if (current_language->c_style_arrays
      && TYPE_CODE (type2) == TYPE_CODE_ARRAY)
    arg2 = value_coerce_array (arg2);

  if (TYPE_CODE (type2) == TYPE_CODE_FUNC)
    arg2 = value_coerce_function (arg2);

  type2 = check_typedef (VALUE_TYPE (arg2));
  COERCE_VARYING_ARRAY (arg2, type2);
  code2 = TYPE_CODE (type2);

  if (code1 == TYPE_CODE_COMPLEX)
    return cast_into_complex (type, arg2);
  if (code1 == TYPE_CODE_BOOL || code1 == TYPE_CODE_CHAR)
    code1 = TYPE_CODE_INT; 
  if (code2 == TYPE_CODE_BOOL || code2 == TYPE_CODE_CHAR)
    code2 = TYPE_CODE_INT;

  scalar = (code2 == TYPE_CODE_INT || code2 == TYPE_CODE_FLT
	    || code2 == TYPE_CODE_ENUM || code2 == TYPE_CODE_RANGE);

  if (   code1 == TYPE_CODE_STRUCT
      && code2 == TYPE_CODE_STRUCT
      && TYPE_NAME (type) != 0)
    {
      /* Look in the type of the source to see if it contains the
	 type of the target as a superclass.  If so, we'll need to
	 offset the object in addition to changing its type.  */
      value_ptr v = search_struct_field (type_name_no_tag (type),
					 arg2, 0, type2, 1);
      if (v)
	{
	  VALUE_TYPE (v) = type;
	  return v;
	}
    }
  if (code1 == TYPE_CODE_FLT && scalar)
    return value_from_double (type, value_as_double (arg2));
  else if ((code1 == TYPE_CODE_INT || code1 == TYPE_CODE_ENUM
	    || code1 == TYPE_CODE_RANGE)
	   && (scalar || code2 == TYPE_CODE_PTR))
    return value_from_longest (type, value_as_long (arg2));
  else if (TYPE_LENGTH (type) == TYPE_LENGTH (type2))
    {
      if (code1 == TYPE_CODE_PTR && code2 == TYPE_CODE_PTR)
	{
	  /* Look in the type of the source to see if it contains the
	     type of the target as a superclass.  If so, we'll need to
	     offset the pointer rather than just change its type.  */
	  struct type *t1 = check_typedef (TYPE_TARGET_TYPE (type));
	  struct type *t2 = check_typedef (TYPE_TARGET_TYPE (type2));
	  if (   TYPE_CODE (t1) == TYPE_CODE_STRUCT
	      && TYPE_CODE (t2) == TYPE_CODE_STRUCT
	      && TYPE_NAME (t1) != 0) /* if name unknown, can't have supercl */
	    {
	      value_ptr v = search_struct_field (type_name_no_tag (t1),
						 value_ind (arg2), 0, t2, 1);
	      if (v)
		{
		  v = value_addr (v);
		  VALUE_TYPE (v) = type;
		  return v;
		}
	    }
	  /* No superclass found, just fall through to change ptr type.  */
	}
      VALUE_TYPE (arg2) = type;
      return arg2;
    }
  else if (chill_varying_type (type))
    {
      struct type *range1, *range2, *eltype1, *eltype2;
      value_ptr val;
      int count1, count2;
      LONGEST low_bound, high_bound;
      char *valaddr, *valaddr_data;
      if (code2 == TYPE_CODE_BITSTRING)
	error ("not implemented: converting bitstring to varying type");
      if ((code2 != TYPE_CODE_ARRAY && code2 != TYPE_CODE_STRING)
	  || (eltype1 = check_typedef (TYPE_TARGET_TYPE (TYPE_FIELD_TYPE (type, 1))),
	      eltype2 = check_typedef (TYPE_TARGET_TYPE (type2)),
	      (TYPE_LENGTH (eltype1) != TYPE_LENGTH (eltype2)
	       /* || TYPE_CODE (eltype1) != TYPE_CODE (eltype2) */ )))
	error ("Invalid conversion to varying type");
      range1 = TYPE_FIELD_TYPE (TYPE_FIELD_TYPE (type, 1), 0);
      range2 = TYPE_FIELD_TYPE (type2, 0);
      if (get_discrete_bounds (range1, &low_bound, &high_bound) < 0)
	count1 = -1;
      else
	count1 = high_bound - low_bound + 1;
      if (get_discrete_bounds (range2, &low_bound, &high_bound) < 0)
	count1 = -1, count2 = 0;  /* To force error before */
      else
	count2 = high_bound - low_bound + 1;
      if (count2 > count1)
	error ("target varying type is too small");
      val = allocate_value (type);
      valaddr = VALUE_CONTENTS_RAW (val);
      valaddr_data = valaddr + TYPE_FIELD_BITPOS (type, 1) / 8;
      /* Set val's __var_length field to count2. */
      store_signed_integer (valaddr, TYPE_LENGTH (TYPE_FIELD_TYPE (type, 0)),
			    count2);
      /* Set the __var_data field to count2 elements copied from arg2. */
      memcpy (valaddr_data, VALUE_CONTENTS (arg2),
	      count2 * TYPE_LENGTH (eltype2));
      /* Zero the rest of the __var_data field of val. */
      memset (valaddr_data + count2 * TYPE_LENGTH (eltype2), '\0',
	      (count1 - count2) * TYPE_LENGTH (eltype2));
      return val;
    }
  else if (VALUE_LVAL (arg2) == lval_memory)
    {
      return value_at_lazy (type, VALUE_ADDRESS (arg2) + VALUE_OFFSET (arg2));
    }
  else if (code1 == TYPE_CODE_VOID)
    {
      return value_zero (builtin_type_void, not_lval);
    }
  else
    {
      error ("Invalid cast.");
      return 0;
    }
}

/* Create a value of type TYPE that is zero, and return it.  */

value_ptr
value_zero (type, lv)
     struct type *type;
     enum lval_type lv;
{
  register value_ptr val = allocate_value (type);

  memset (VALUE_CONTENTS (val), 0, TYPE_LENGTH (check_typedef (type)));
  VALUE_LVAL (val) = lv;

  return val;
}

/* Return a value with type TYPE located at ADDR.  

   Call value_at only if the data needs to be fetched immediately;
   if we can be 'lazy' and defer the fetch, perhaps indefinately, call
   value_at_lazy instead.  value_at_lazy simply records the address of
   the data and sets the lazy-evaluation-required flag.  The lazy flag 
   is tested in the VALUE_CONTENTS macro, which is used if and when 
   the contents are actually required.  */

value_ptr
value_at (type, addr)
     struct type *type;
     CORE_ADDR addr;
{
  register value_ptr val;

  if (TYPE_CODE (check_typedef (type)) == TYPE_CODE_VOID)
    error ("Attempt to dereference a generic pointer.");

  val = allocate_value (type);

  read_memory (addr, VALUE_CONTENTS_RAW (val), TYPE_LENGTH (type));

  VALUE_LVAL (val) = lval_memory;
  VALUE_ADDRESS (val) = addr;

  return val;
}

/* Return a lazy value with type TYPE located at ADDR (cf. value_at).  */

value_ptr
value_at_lazy (type, addr)
     struct type *type;
     CORE_ADDR addr;
{
  register value_ptr val;

  if (TYPE_CODE (check_typedef (type)) == TYPE_CODE_VOID)
    error ("Attempt to dereference a generic pointer.");

  val = allocate_value (type);

  VALUE_LVAL (val) = lval_memory;
  VALUE_ADDRESS (val) = addr;
  VALUE_LAZY (val) = 1;

  return val;
}

/* Called only from the VALUE_CONTENTS macro, if the current data for
   a variable needs to be loaded into VALUE_CONTENTS(VAL).  Fetches the
   data from the user's process, and clears the lazy flag to indicate
   that the data in the buffer is valid.

   If the value is zero-length, we avoid calling read_memory, which would
   abort.  We mark the value as fetched anyway -- all 0 bytes of it.

   This function returns a value because it is used in the VALUE_CONTENTS
   macro as part of an expression, where a void would not work.  The
   value is ignored.  */

int
value_fetch_lazy (val)
     register value_ptr val;
{
  CORE_ADDR addr = VALUE_ADDRESS (val) + VALUE_OFFSET (val);
  int length = TYPE_LENGTH (VALUE_TYPE (val));

  if (length)
    read_memory (addr, VALUE_CONTENTS_RAW (val), length);
  VALUE_LAZY (val) = 0;
  return 0;
}


/* Store the contents of FROMVAL into the location of TOVAL.
   Return a new value with the location of TOVAL and contents of FROMVAL.  */

value_ptr
value_assign (toval, fromval)
     register value_ptr toval, fromval;
{
  register struct type *type;
  register value_ptr val;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  int use_buffer = 0;

  if (!toval->modifiable)
    error ("Left operand of assignment is not a modifiable lvalue.");

  COERCE_REF (toval);

  type = VALUE_TYPE (toval);
  if (VALUE_LVAL (toval) != lval_internalvar)
    fromval = value_cast (type, fromval);
  else
    COERCE_ARRAY (fromval);
  CHECK_TYPEDEF (type);

  /* If TOVAL is a special machine register requiring conversion
     of program values to a special raw format,
     convert FROMVAL's contents now, with result in `raw_buffer',
     and set USE_BUFFER to the number of bytes to write.  */

#ifdef REGISTER_CONVERTIBLE
  if (VALUE_REGNO (toval) >= 0
      && REGISTER_CONVERTIBLE (VALUE_REGNO (toval)))
    {
      int regno = VALUE_REGNO (toval);
      if (REGISTER_CONVERTIBLE (regno))
	{
	  struct type *fromtype = check_typedef (VALUE_TYPE (fromval));
	  REGISTER_CONVERT_TO_RAW (fromtype, regno,
				   VALUE_CONTENTS (fromval), raw_buffer);
	  use_buffer = REGISTER_RAW_SIZE (regno);
	}
    }
#endif

  switch (VALUE_LVAL (toval))
    {
    case lval_internalvar:
      set_internalvar (VALUE_INTERNALVAR (toval), fromval);
      return value_copy (VALUE_INTERNALVAR (toval)->value);

    case lval_internalvar_component:
      set_internalvar_component (VALUE_INTERNALVAR (toval),
				 VALUE_OFFSET (toval),
				 VALUE_BITPOS (toval),
				 VALUE_BITSIZE (toval),
				 fromval);
      break;

    case lval_memory:
      if (VALUE_BITSIZE (toval))
	{
	  char buffer[sizeof (LONGEST)];
	  /* We assume that the argument to read_memory is in units of
	     host chars.  FIXME:  Is that correct?  */
	  int len = (VALUE_BITPOS (toval)
		     + VALUE_BITSIZE (toval)
		     + HOST_CHAR_BIT - 1)
		    / HOST_CHAR_BIT;

	  if (len > (int) sizeof (LONGEST))
	    error ("Can't handle bitfields which don't fit in a %d bit word.",
		   sizeof (LONGEST) * HOST_CHAR_BIT);

	  read_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
		       buffer, len);
	  modify_field (buffer, value_as_long (fromval),
			VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
	  write_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			buffer, len);
	}
      else if (use_buffer)
	write_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
		      raw_buffer, use_buffer);
      else
	write_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
		      VALUE_CONTENTS (fromval), TYPE_LENGTH (type));
      break;

    case lval_register:
      if (VALUE_BITSIZE (toval))
	{
	  char buffer[sizeof (LONGEST)];
          int len = REGISTER_RAW_SIZE (VALUE_REGNO (toval));

	  if (len > (int) sizeof (LONGEST))
	    error ("Can't handle bitfields in registers larger than %d bits.",
		   sizeof (LONGEST) * HOST_CHAR_BIT);

	  if (VALUE_BITPOS (toval) + VALUE_BITSIZE (toval)
	      > len * HOST_CHAR_BIT)
	    /* Getting this right would involve being very careful about
	       byte order.  */
	    error ("\
Can't handle bitfield which doesn't fit in a single register.");

          read_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
                               buffer, len);
          modify_field (buffer, value_as_long (fromval),
                        VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
          write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
                                buffer, len);
	}
      else if (use_buffer)
	write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			      raw_buffer, use_buffer);
      else
        {
	  /* Do any conversion necessary when storing this type to more
	     than one register.  */
#ifdef REGISTER_CONVERT_FROM_TYPE
	  memcpy (raw_buffer, VALUE_CONTENTS (fromval), TYPE_LENGTH (type));
	  REGISTER_CONVERT_FROM_TYPE(VALUE_REGNO (toval), type, raw_buffer);
	  write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
				raw_buffer, TYPE_LENGTH (type));
#else
	  write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			        VALUE_CONTENTS (fromval), TYPE_LENGTH (type));
#endif
	}
      /* Assigning to the stack pointer, frame pointer, and other
	 (architecture and calling convention specific) registers may
	 cause the frame cache to be out of date.  We just do this
	 on all assignments to registers for simplicity; I doubt the slowdown
	 matters.  */
      reinit_frame_cache ();
      break;

    case lval_reg_frame_relative:
      {
	/* value is stored in a series of registers in the frame
	   specified by the structure.  Copy that value out, modify
	   it, and copy it back in.  */
	int amount_to_copy = (VALUE_BITSIZE (toval) ? 1 : TYPE_LENGTH (type));
	int reg_size = REGISTER_RAW_SIZE (VALUE_FRAME_REGNUM (toval));
	int byte_offset = VALUE_OFFSET (toval) % reg_size;
	int reg_offset = VALUE_OFFSET (toval) / reg_size;
	int amount_copied;

	/* Make the buffer large enough in all cases.  */
	char *buffer = (char *) alloca (amount_to_copy
					+ sizeof (LONGEST)
					+ MAX_REGISTER_RAW_SIZE);

	int regno;
	struct frame_info *frame;

	/* Figure out which frame this is in currently.  */
	for (frame = get_current_frame ();
	     frame && FRAME_FP (frame) != VALUE_FRAME (toval);
	     frame = get_prev_frame (frame))
	  ;

	if (!frame)
	  error ("Value being assigned to is no longer active.");

	amount_to_copy += (reg_size - amount_to_copy % reg_size);

	/* Copy it out.  */
	for ((regno = VALUE_FRAME_REGNUM (toval) + reg_offset,
	      amount_copied = 0);
	     amount_copied < amount_to_copy;
	     amount_copied += reg_size, regno++)
	  {
	    get_saved_register (buffer + amount_copied,
				(int *)NULL, (CORE_ADDR *)NULL,
				frame, regno, (enum lval_type *)NULL);
	  }

	/* Modify what needs to be modified.  */
	if (VALUE_BITSIZE (toval))
	  modify_field (buffer + byte_offset,
			value_as_long (fromval),
			VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
	else if (use_buffer)
	  memcpy (buffer + byte_offset, raw_buffer, use_buffer);
	else
	  memcpy (buffer + byte_offset, VALUE_CONTENTS (fromval),
		  TYPE_LENGTH (type));

	/* Copy it back.  */
	for ((regno = VALUE_FRAME_REGNUM (toval) + reg_offset,
	      amount_copied = 0);
	     amount_copied < amount_to_copy;
	     amount_copied += reg_size, regno++)
	  {
	    enum lval_type lval;
	    CORE_ADDR addr;
	    int optim;

	    /* Just find out where to put it.  */
	    get_saved_register ((char *)NULL,
			        &optim, &addr, frame, regno, &lval);
	    
	    if (optim)
	      error ("Attempt to assign to a value that was optimized out.");
	    if (lval == lval_memory)
	      write_memory (addr, buffer + amount_copied, reg_size);
	    else if (lval == lval_register)
	      write_register_bytes (addr, buffer + amount_copied, reg_size);
	    else
	      error ("Attempt to assign to an unmodifiable value.");
	  }
      }
      break;
	

    default:
      error ("Left operand of assignment is not an lvalue.");
    }

  /* If the field does not entirely fill a LONGEST, then zero the sign bits.
     If the field is signed, and is negative, then sign extend. */
  if ((VALUE_BITSIZE (toval) > 0)
      && (VALUE_BITSIZE (toval) < 8 * (int) sizeof (LONGEST)))
    {
      LONGEST fieldval = value_as_long (fromval);
      LONGEST valmask = (((unsigned LONGEST) 1) << VALUE_BITSIZE (toval)) - 1;

      fieldval &= valmask;
      if (!TYPE_UNSIGNED (type) && (fieldval & (valmask ^ (valmask >> 1))))
	fieldval |= ~valmask;

      fromval = value_from_longest (type, fieldval);
    }

  val = value_copy (toval);
  memcpy (VALUE_CONTENTS_RAW (val), VALUE_CONTENTS (fromval),
	  TYPE_LENGTH (type));
  VALUE_TYPE (val) = type;
  
  return val;
}

/* Extend a value VAL to COUNT repetitions of its type.  */

value_ptr
value_repeat (arg1, count)
     value_ptr arg1;
     int count;
{
  register value_ptr val;

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Only values in memory can be extended with '@'.");
  if (count < 1)
    error ("Invalid number %d of repetitions.", count);

  val = allocate_repeat_value (VALUE_TYPE (arg1), count);

  read_memory (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1),
	       VALUE_CONTENTS_RAW (val),
	       TYPE_LENGTH (VALUE_TYPE (val)));
  VALUE_LVAL (val) = lval_memory;
  VALUE_ADDRESS (val) = VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1);

  return val;
}

value_ptr
value_of_variable (var, b)
     struct symbol *var;
     struct block *b;
{
  value_ptr val;
  struct frame_info *frame;

  if (b == NULL)
    /* Use selected frame.  */
    frame = NULL;
  else
    {
      frame = block_innermost_frame (b);
      if (frame == NULL && symbol_read_needs_frame (var))
	{
	  if (BLOCK_FUNCTION (b) != NULL
	      && SYMBOL_NAME (BLOCK_FUNCTION (b)) != NULL)
	    error ("No frame is currently executing in block %s.",
		   SYMBOL_NAME (BLOCK_FUNCTION (b)));
	  else
	    error ("No frame is currently executing in specified block");
	}
    }
  val = read_var_value (var, frame);
  if (val == 0)
    error ("Address of symbol \"%s\" is unknown.", SYMBOL_SOURCE_NAME (var));
  return val;
}

/* Given a value which is an array, return a value which is a pointer to its
   first element, regardless of whether or not the array has a nonzero lower
   bound.

   FIXME:  A previous comment here indicated that this routine should be
   substracting the array's lower bound.  It's not clear to me that this
   is correct.  Given an array subscripting operation, it would certainly
   work to do the adjustment here, essentially computing:

   (&array[0] - (lowerbound * sizeof array[0])) + (index * sizeof array[0])

   However I believe a more appropriate and logical place to account for
   the lower bound is to do so in value_subscript, essentially computing:

   (&array[0] + ((index - lowerbound) * sizeof array[0]))

   As further evidence consider what would happen with operations other
   than array subscripting, where the caller would get back a value that
   had an address somewhere before the actual first element of the array,
   and the information about the lower bound would be lost because of
   the coercion to pointer type.
   */

value_ptr
value_coerce_array (arg1)
     value_ptr arg1;
{
  register struct type *type = check_typedef (VALUE_TYPE (arg1));

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Attempt to take address of value not located in memory.");

  return value_from_longest (lookup_pointer_type (TYPE_TARGET_TYPE (type)),
		       (LONGEST) (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1)));
}

/* Given a value which is a function, return a value which is a pointer
   to it.  */

value_ptr
value_coerce_function (arg1)
     value_ptr arg1;
{

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Attempt to take address of value not located in memory.");

  return value_from_longest (lookup_pointer_type (VALUE_TYPE (arg1)),
		(LONGEST) (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1)));
}  

/* Return a pointer value for the object for which ARG1 is the contents.  */

value_ptr
value_addr (arg1)
     value_ptr arg1;
{
  struct type *type = check_typedef (VALUE_TYPE (arg1));
  if (TYPE_CODE (type) == TYPE_CODE_REF)
    {
      /* Copy the value, but change the type from (T&) to (T*).
	 We keep the same location information, which is efficient,
	 and allows &(&X) to get the location containing the reference. */
      value_ptr arg2 = value_copy (arg1);
      VALUE_TYPE (arg2) = lookup_pointer_type (TYPE_TARGET_TYPE (type));
      return arg2;
    }
  if (TYPE_CODE (type) == TYPE_CODE_FUNC)
    return value_coerce_function (arg1);

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Attempt to take address of value not located in memory.");

  return value_from_longest (lookup_pointer_type (VALUE_TYPE (arg1)),
		(LONGEST) (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1)));
}

/* Given a value of a pointer type, apply the C unary * operator to it.  */

value_ptr
value_ind (arg1)
     value_ptr arg1;
{
  struct type *type1;
  COERCE_ARRAY (arg1);
  type1 = check_typedef (VALUE_TYPE (arg1));

  if (TYPE_CODE (type1) == TYPE_CODE_MEMBER)
    error ("not implemented: member types in value_ind");

  /* Allow * on an integer so we can cast it to whatever we want.
     This returns an int, which seems like the most C-like thing
     to do.  "long long" variables are rare enough that
     BUILTIN_TYPE_LONGEST would seem to be a mistake.  */
  if (TYPE_CODE (type1) == TYPE_CODE_INT)
    return value_at (builtin_type_int,
		     (CORE_ADDR) value_as_long (arg1));
  else if (TYPE_CODE (type1) == TYPE_CODE_PTR)
    return value_at_lazy (TYPE_TARGET_TYPE (type1), value_as_pointer (arg1));
  error ("Attempt to take contents of a non-pointer value.");
  return 0;  /* For lint -- never reached */
}

/* Pushing small parts of stack frames.  */

/* Push one word (the size of object that a register holds).  */

CORE_ADDR
push_word (sp, word)
     CORE_ADDR sp;
     unsigned LONGEST word;
{
  register int len = REGISTER_SIZE;
  char buffer[MAX_REGISTER_RAW_SIZE];

  store_unsigned_integer (buffer, len, word);
#if 1 INNER_THAN 2
  sp -= len;
  write_memory (sp, buffer, len);
#else /* stack grows upward */
  write_memory (sp, buffer, len);
  sp += len;
#endif /* stack grows upward */

  return sp;
}

/* Push LEN bytes with data at BUFFER.  */

CORE_ADDR
push_bytes (sp, buffer, len)
     CORE_ADDR sp;
     char *buffer;
     int len;
{
#if 1 INNER_THAN 2
  sp -= len;
  write_memory (sp, buffer, len);
#else /* stack grows upward */
  write_memory (sp, buffer, len);
  sp += len;
#endif /* stack grows upward */

  return sp;
}

/* Push onto the stack the specified value VALUE.  */

static CORE_ADDR
value_push (sp, arg)
     register CORE_ADDR sp;
     value_ptr arg;
{
  register int len = TYPE_LENGTH (VALUE_TYPE (arg));

#if 1 INNER_THAN 2
  sp -= len;
  write_memory (sp, VALUE_CONTENTS (arg), len);
#else /* stack grows upward */
  write_memory (sp, VALUE_CONTENTS (arg), len);
  sp += len;
#endif /* stack grows upward */

  return sp;
}

/* Perform the standard coercions that are specified
   for arguments to be passed to C functions.

   If PARAM_TYPE is non-NULL, it is the expected parameter type. */

static value_ptr
value_arg_coerce (arg, param_type)
     value_ptr arg;
     struct type *param_type;
{
  register struct type *arg_type = check_typedef (VALUE_TYPE (arg));
  register struct type *type
    = param_type ? check_typedef (param_type) : arg_type;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_REF:
      if (TYPE_CODE (arg_type) != TYPE_CODE_REF)
	{
	  arg = value_addr (arg);
	  VALUE_TYPE (arg) = param_type;
	  return arg;
	}
      break;
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_ENUM:
      if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
	type = builtin_type_int;
      break;
   case TYPE_CODE_FLT:
     /* coerce float to double, unless the function prototype specifies float */
     if (COERCE_FLOAT_TO_DOUBLE)
       {
	 if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_double))
	   type = builtin_type_double;
	 else if (TYPE_LENGTH (type) > TYPE_LENGTH (builtin_type_double))
	   type = builtin_type_long_double;
       }
     break;
    case TYPE_CODE_FUNC:
      type = lookup_pointer_type (type);
      break;
    case TYPE_CODE_ARRAY:
      if (current_language->c_style_arrays)
	type = lookup_pointer_type (TYPE_TARGET_TYPE (type));
      break;
    case TYPE_CODE_UNDEF:
    case TYPE_CODE_PTR:
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_VOID:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_STRING:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_MEMBER:
    case TYPE_CODE_METHOD:
    case TYPE_CODE_COMPLEX:
    default:
      break;
    }

  return value_cast (type, arg);
}

/* Determine a function's address and its return type from its value. 
   Calls error() if the function is not valid for calling.  */

static CORE_ADDR
find_function_addr (function, retval_type)
     value_ptr function;
     struct type **retval_type;
{
  register struct type *ftype = check_typedef (VALUE_TYPE (function));
  register enum type_code code = TYPE_CODE (ftype);
  struct type *value_type;
  CORE_ADDR funaddr;

  /* If it's a member function, just look at the function
     part of it.  */

  /* Determine address to call.  */
  if (code == TYPE_CODE_FUNC || code == TYPE_CODE_METHOD)
    {
      funaddr = VALUE_ADDRESS (function);
      value_type = TYPE_TARGET_TYPE (ftype);
    }
  else if (code == TYPE_CODE_PTR)
    {
      funaddr = value_as_pointer (function);
      ftype = check_typedef (TYPE_TARGET_TYPE (ftype));
      if (TYPE_CODE (ftype) == TYPE_CODE_FUNC
	  || TYPE_CODE (ftype) == TYPE_CODE_METHOD)
	{
#ifdef CONVERT_FROM_FUNC_PTR_ADDR
	  /* FIXME: This is a workaround for the unusual function
	     pointer representation on the RS/6000, see comment
	     in config/rs6000/tm-rs6000.h  */
	  funaddr = CONVERT_FROM_FUNC_PTR_ADDR (funaddr);
#endif
	  value_type = TYPE_TARGET_TYPE (ftype);
	}
      else
	value_type = builtin_type_int;
    }
  else if (code == TYPE_CODE_INT)
    {
      /* Handle the case of functions lacking debugging info.
	 Their values are characters since their addresses are char */
      if (TYPE_LENGTH (ftype) == 1)
	funaddr = value_as_pointer (value_addr (function));
      else
	/* Handle integer used as address of a function.  */
	funaddr = (CORE_ADDR) value_as_long (function);

      value_type = builtin_type_int;
    }
  else
    error ("Invalid data type for function to be called.");

  *retval_type = value_type;
  return funaddr;
}

#if defined (CALL_DUMMY)
/* All this stuff with a dummy frame may seem unnecessarily complicated
   (why not just save registers in GDB?).  The purpose of pushing a dummy
   frame which looks just like a real frame is so that if you call a
   function and then hit a breakpoint (get a signal, etc), "backtrace"
   will look right.  Whether the backtrace needs to actually show the
   stack at the time the inferior function was called is debatable, but
   it certainly needs to not display garbage.  So if you are contemplating
   making dummy frames be different from normal frames, consider that.  */

/* Perform a function call in the inferior.
   ARGS is a vector of values of arguments (NARGS of them).
   FUNCTION is a value, the function to be called.
   Returns a value representing what the function returned.
   May fail to return, if a breakpoint or signal is hit
   during the execution of the function.

   ARGS is modified to contain coerced values. */

value_ptr
call_function_by_hand (function, nargs, args)
     value_ptr function;
     int nargs;
     value_ptr *args;
{
  register CORE_ADDR sp;
  register int i;
  CORE_ADDR start_sp;
  /* CALL_DUMMY is an array of words (REGISTER_SIZE), but each word
     is in host byte order.  Before calling FIX_CALL_DUMMY, we byteswap it
     and remove any extra bytes which might exist because unsigned LONGEST is
     bigger than REGISTER_SIZE.  */
  static unsigned LONGEST dummy[] = CALL_DUMMY;
  char dummy1[REGISTER_SIZE * sizeof dummy / sizeof (unsigned LONGEST)];
  CORE_ADDR old_sp;
  struct type *value_type;
  unsigned char struct_return;
  CORE_ADDR struct_addr;
  struct inferior_status inf_status;
  struct cleanup *old_chain;
  CORE_ADDR funaddr;
  int using_gcc;
  CORE_ADDR real_pc;
  struct type *ftype = check_typedef (SYMBOL_TYPE (function));

  if (!target_has_execution)
    noprocess();

  save_inferior_status (&inf_status, 1);
  old_chain = make_cleanup (restore_inferior_status, &inf_status);

  /* PUSH_DUMMY_FRAME is responsible for saving the inferior registers
     (and POP_FRAME for restoring them).  (At least on most machines)
     they are saved on the stack in the inferior.  */
  PUSH_DUMMY_FRAME;

  old_sp = sp = read_sp ();

#if 1 INNER_THAN 2		/* Stack grows down */
  sp -= sizeof dummy1;
  start_sp = sp;
#else				/* Stack grows up */
  start_sp = sp;
  sp += sizeof dummy1;
#endif

  funaddr = find_function_addr (function, &value_type);
  CHECK_TYPEDEF (value_type);

  {
    struct block *b = block_for_pc (funaddr);
    /* If compiled without -g, assume GCC.  */
    using_gcc = b == NULL ? 0 : BLOCK_GCC_COMPILED (b);
  }

  /* Are we returning a value using a structure return or a normal
     value return? */

  struct_return = using_struct_return (function, funaddr, value_type,
				       using_gcc);

  /* Create a call sequence customized for this function
     and the number of arguments for it.  */
  for (i = 0; i < (int) (sizeof (dummy) / sizeof (dummy[0])); i++)
    store_unsigned_integer (&dummy1[i * REGISTER_SIZE],
			    REGISTER_SIZE,
			    (unsigned LONGEST)dummy[i]);

#ifdef GDB_TARGET_IS_HPPA
  real_pc = FIX_CALL_DUMMY (dummy1, start_sp, funaddr, nargs, args,
			    value_type, using_gcc);
#else
  FIX_CALL_DUMMY (dummy1, start_sp, funaddr, nargs, args,
		  value_type, using_gcc);
  real_pc = start_sp;
#endif

#if CALL_DUMMY_LOCATION == ON_STACK
  write_memory (start_sp, (char *)dummy1, sizeof dummy1);
#endif /* On stack.  */

#if CALL_DUMMY_LOCATION == BEFORE_TEXT_END
  /* Convex Unix prohibits executing in the stack segment. */
  /* Hope there is empty room at the top of the text segment. */
  {
    extern CORE_ADDR text_end;
    static checked = 0;
    if (!checked)
      for (start_sp = text_end - sizeof dummy1; start_sp < text_end; ++start_sp)
	if (read_memory_integer (start_sp, 1) != 0)
	  error ("text segment full -- no place to put call");
    checked = 1;
    sp = old_sp;
    real_pc = text_end - sizeof dummy1;
    write_memory (real_pc, (char *)dummy1, sizeof dummy1);
  }
#endif /* Before text_end.  */

#if CALL_DUMMY_LOCATION == AFTER_TEXT_END
  {
    extern CORE_ADDR text_end;
    int errcode;
    sp = old_sp;
    real_pc = text_end;
    errcode = target_write_memory (real_pc, (char *)dummy1, sizeof dummy1);
    if (errcode != 0)
      error ("Cannot write text segment -- call_function failed");
  }
#endif /* After text_end.  */

#if CALL_DUMMY_LOCATION == AT_ENTRY_POINT
  real_pc = funaddr;
#endif /* At entry point.  */

#ifdef lint
  sp = old_sp;		/* It really is used, for some ifdef's... */
#endif

  if (nargs < TYPE_NFIELDS (ftype))
    error ("too few arguments in function call");

  for (i = nargs - 1; i >= 0; i--)
    {
      struct type *param_type;
      if (TYPE_NFIELDS (ftype) > i)
	param_type = TYPE_FIELD_TYPE (ftype, i);
      else
	param_type = 0;
      args[i] = value_arg_coerce (args[i], param_type);
    }

#if defined (REG_STRUCT_HAS_ADDR)
  {
    /* This is a machine like the sparc, where we may need to pass a pointer
       to the structure, not the structure itself.  */
    for (i = nargs - 1; i >= 0; i--)
      {
	struct type *arg_type = check_typedef (VALUE_TYPE (args[i]));
	if ((TYPE_CODE (arg_type) == TYPE_CODE_STRUCT
	     || TYPE_CODE (arg_type) == TYPE_CODE_UNION
	     || TYPE_CODE (arg_type) == TYPE_CODE_ARRAY
	     || TYPE_CODE (arg_type) == TYPE_CODE_STRING
	     || TYPE_CODE (arg_type) == TYPE_CODE_BITSTRING
	     || TYPE_CODE (arg_type) == TYPE_CODE_SET
	     || (TYPE_CODE (arg_type) == TYPE_CODE_FLT
		 && TYPE_LENGTH (arg_type) > 8)
	     )
	  && REG_STRUCT_HAS_ADDR (using_gcc, arg_type))
	  {
	    CORE_ADDR addr;
	    int len = TYPE_LENGTH (arg_type);
#ifdef STACK_ALIGN
	    int aligned_len = STACK_ALIGN (len);
#else
	    int aligned_len = len;
#endif
#if !(1 INNER_THAN 2)
	    /* The stack grows up, so the address of the thing we push
	       is the stack pointer before we push it.  */
	    addr = sp;
#else
	    sp -= aligned_len;
#endif
	    /* Push the structure.  */
	    write_memory (sp, VALUE_CONTENTS (args[i]), len);
#if 1 INNER_THAN 2
	    /* The stack grows down, so the address of the thing we push
	       is the stack pointer after we push it.  */
	    addr = sp;
#else
	    sp += aligned_len;
#endif
	    /* The value we're going to pass is the address of the thing
	       we just pushed.  */
	    args[i] = value_from_longest (lookup_pointer_type (value_type),
					  (LONGEST) addr);
	  }
      }
  }
#endif /* REG_STRUCT_HAS_ADDR.  */

  /* Reserve space for the return structure to be written on the
     stack, if necessary */

  if (struct_return)
    {
      int len = TYPE_LENGTH (value_type);
#ifdef STACK_ALIGN
      len = STACK_ALIGN (len);
#endif
#if 1 INNER_THAN 2
      sp -= len;
      struct_addr = sp;
#else
      struct_addr = sp;
      sp += len;
#endif
    }

#ifdef STACK_ALIGN
  /* If stack grows down, we must leave a hole at the top. */
  {
    int len = 0;

    for (i = nargs - 1; i >= 0; i--)
      len += TYPE_LENGTH (VALUE_TYPE (args[i]));
#ifdef CALL_DUMMY_STACK_ADJUST
    len += CALL_DUMMY_STACK_ADJUST;
#endif
#if 1 INNER_THAN 2
    sp -= STACK_ALIGN (len) - len;
#else
    sp += STACK_ALIGN (len) - len;
#endif
  }
#endif /* STACK_ALIGN */

#ifdef PUSH_ARGUMENTS
  PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr);
#else /* !PUSH_ARGUMENTS */
  for (i = nargs - 1; i >= 0; i--)
    sp = value_push (sp, args[i]);
#endif /* !PUSH_ARGUMENTS */

#ifdef CALL_DUMMY_STACK_ADJUST
#if 1 INNER_THAN 2
  sp -= CALL_DUMMY_STACK_ADJUST;
#else
  sp += CALL_DUMMY_STACK_ADJUST;
#endif
#endif /* CALL_DUMMY_STACK_ADJUST */

  /* Store the address at which the structure is supposed to be
     written.  Note that this (and the code which reserved the space
     above) assumes that gcc was used to compile this function.  Since
     it doesn't cost us anything but space and if the function is pcc
     it will ignore this value, we will make that assumption.

     Also note that on some machines (like the sparc) pcc uses a 
     convention like gcc's.  */

  if (struct_return)
    STORE_STRUCT_RETURN (struct_addr, sp);

  /* Write the stack pointer.  This is here because the statements above
     might fool with it.  On SPARC, this write also stores the register
     window into the right place in the new stack frame, which otherwise
     wouldn't happen.  (See store_inferior_registers in sparc-nat.c.)  */
  write_sp (sp);

  {
    char retbuf[REGISTER_BYTES];
    char *name;
    struct symbol *symbol;

    name = NULL;
    symbol = find_pc_function (funaddr);
    if (symbol)
      {
	name = SYMBOL_SOURCE_NAME (symbol);
      }
    else
      {
	/* Try the minimal symbols.  */
	struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (funaddr);

	if (msymbol)
	  {
	    name = SYMBOL_SOURCE_NAME (msymbol);
	  }
      }
    if (name == NULL)
      {
	char format[80];
	sprintf (format, "at %s", local_hex_format ());
	name = alloca (80);
	/* FIXME-32x64: assumes funaddr fits in a long.  */
	sprintf (name, format, (unsigned long) funaddr);
      }

    /* Execute the stack dummy routine, calling FUNCTION.
       When it is done, discard the empty frame
       after storing the contents of all regs into retbuf.  */
    if (run_stack_dummy (real_pc + CALL_DUMMY_START_OFFSET, retbuf))
      {
	/* We stopped somewhere besides the call dummy.  */

	/* If we did the cleanups, we would print a spurious error message
	   (Unable to restore previously selected frame), would write the
	   registers from the inf_status (which is wrong), and would do other
	   wrong things (like set stop_bpstat to the wrong thing).  */
	discard_cleanups (old_chain);
	/* Prevent memory leak.  */
	bpstat_clear (&inf_status.stop_bpstat);

	/* The following error message used to say "The expression
	   which contained the function call has been discarded."  It
	   is a hard concept to explain in a few words.  Ideally, GDB
	   would be able to resume evaluation of the expression when
	   the function finally is done executing.  Perhaps someday
	   this will be implemented (it would not be easy).  */

	/* FIXME: Insert a bunch of wrap_here; name can be very long if it's
	   a C++ name with arguments and stuff.  */
	error ("\
The program being debugged stopped while in a function called from GDB.\n\
When the function (%s) is done executing, GDB will silently\n\
stop (instead of continuing to evaluate the expression containing\n\
the function call).", name);
      }

    do_cleanups (old_chain);

    /* Figure out the value returned by the function.  */
    return value_being_returned (value_type, retbuf, struct_return);
  }
}
#else /* no CALL_DUMMY.  */
value_ptr
call_function_by_hand (function, nargs, args)
     value_ptr function;
     int nargs;
     value_ptr *args;
{
  error ("Cannot invoke functions on this machine.");
}
#endif /* no CALL_DUMMY.  */


/* Create a value for an array by allocating space in the inferior, copying
   the data into that space, and then setting up an array value.

   The array bounds are set from LOWBOUND and HIGHBOUND, and the array is
   populated from the values passed in ELEMVEC.

   The element type of the array is inherited from the type of the
   first element, and all elements must have the same size (though we
   don't currently enforce any restriction on their types). */

value_ptr
value_array (lowbound, highbound, elemvec)
     int lowbound;
     int highbound;
     value_ptr *elemvec;
{
  int nelem;
  int idx;
  unsigned int typelength;
  value_ptr val;
  struct type *rangetype;
  struct type *arraytype;
  CORE_ADDR addr;

  /* Validate that the bounds are reasonable and that each of the elements
     have the same size. */

  nelem = highbound - lowbound + 1;
  if (nelem <= 0)
    {
      error ("bad array bounds (%d, %d)", lowbound, highbound);
    }
  typelength = TYPE_LENGTH (VALUE_TYPE (elemvec[0]));
  for (idx = 1; idx < nelem; idx++)
    {
      if (TYPE_LENGTH (VALUE_TYPE (elemvec[idx])) != typelength)
	{
	  error ("array elements must all be the same size");
	}
    }

  rangetype = create_range_type ((struct type *) NULL, builtin_type_int,
				 lowbound, highbound);
  arraytype = create_array_type ((struct type *) NULL, 
				 VALUE_TYPE (elemvec[0]), rangetype);

  if (!current_language->c_style_arrays)
    {
      val = allocate_value (arraytype);
      for (idx = 0; idx < nelem; idx++)
	{
	  memcpy (VALUE_CONTENTS_RAW (val) + (idx * typelength),
		  VALUE_CONTENTS (elemvec[idx]),
		  typelength);
	}
      return val;
    }

  /* Allocate space to store the array in the inferior, and then initialize
     it by copying in each element.  FIXME:  Is it worth it to create a
     local buffer in which to collect each value and then write all the
     bytes in one operation? */

  addr = allocate_space_in_inferior (nelem * typelength);
  for (idx = 0; idx < nelem; idx++)
    {
      write_memory (addr + (idx * typelength), VALUE_CONTENTS (elemvec[idx]),
		    typelength);
    }

  /* Create the array type and set up an array value to be evaluated lazily. */

  val = value_at_lazy (arraytype, addr);
  return (val);
}

/* Create a value for a string constant by allocating space in the inferior,
   copying the data into that space, and returning the address with type
   TYPE_CODE_STRING.  PTR points to the string constant data; LEN is number
   of characters.
   Note that string types are like array of char types with a lower bound of
   zero and an upper bound of LEN - 1.  Also note that the string may contain
   embedded null bytes. */

value_ptr
value_string (ptr, len)
     char *ptr;
     int len;
{
  value_ptr val;
  int lowbound = current_language->string_lower_bound;
  struct type *rangetype = create_range_type ((struct type *) NULL,
					      builtin_type_int,
					      lowbound, len + lowbound - 1);
  struct type *stringtype
    = create_string_type ((struct type *) NULL, rangetype);
  CORE_ADDR addr;

  if (current_language->c_style_arrays == 0)
    {
      val = allocate_value (stringtype);
      memcpy (VALUE_CONTENTS_RAW (val), ptr, len);
      return val;
    }


  /* Allocate space to store the string in the inferior, and then
     copy LEN bytes from PTR in gdb to that address in the inferior. */

  addr = allocate_space_in_inferior (len);
  write_memory (addr, ptr, len);

  val = value_at_lazy (stringtype, addr);
  return (val);
}

value_ptr
value_bitstring (ptr, len)
     char *ptr;
     int len;
{
  value_ptr val;
  struct type *domain_type = create_range_type (NULL, builtin_type_int,
						0, len - 1);
  struct type *type = create_set_type ((struct type*) NULL, domain_type);
  TYPE_CODE (type) = TYPE_CODE_BITSTRING;
  val = allocate_value (type);
  memcpy (VALUE_CONTENTS_RAW (val), ptr, TYPE_LENGTH (type));
  return val;
}

/* See if we can pass arguments in T2 to a function which takes arguments
   of types T1.  Both t1 and t2 are NULL-terminated vectors.  If some
   arguments need coercion of some sort, then the coerced values are written
   into T2.  Return value is 0 if the arguments could be matched, or the
   position at which they differ if not.

   STATICP is nonzero if the T1 argument list came from a
   static member function.

   For non-static member functions, we ignore the first argument,
   which is the type of the instance variable.  This is because we want
   to handle calls with objects from derived classes.  This is not
   entirely correct: we should actually check to make sure that a
   requested operation is type secure, shouldn't we?  FIXME.  */

static int
typecmp (staticp, t1, t2)
     int staticp;
     struct type *t1[];
     value_ptr t2[];
{
  int i;

  if (t2 == 0)
    return 1;
  if (staticp && t1 == 0)
    return t2[1] != 0;
  if (t1 == 0)
    return 1;
  if (TYPE_CODE (t1[0]) == TYPE_CODE_VOID) return 0;
  if (t1[!staticp] == 0) return 0;
  for (i = !staticp; t1[i] && TYPE_CODE (t1[i]) != TYPE_CODE_VOID; i++)
    {
    struct type *tt1, *tt2;
      if (! t2[i])
	return i+1;
      tt1 = check_typedef (t1[i]);
      tt2 = check_typedef (VALUE_TYPE(t2[i]));
      if (TYPE_CODE (tt1) == TYPE_CODE_REF
	  /* We should be doing hairy argument matching, as below.  */
	  && (TYPE_CODE (check_typedef (TYPE_TARGET_TYPE (tt1))) == TYPE_CODE (tt2)))
	{
	  if (TYPE_CODE (tt2) == TYPE_CODE_ARRAY)
	    t2[i] = value_coerce_array (t2[i]);
	  else
	    t2[i] = value_addr (t2[i]);
	  continue;
	}

      while (TYPE_CODE (tt1) == TYPE_CODE_PTR
	  && (   TYPE_CODE (tt2) == TYPE_CODE_ARRAY
	      || TYPE_CODE (tt2) == TYPE_CODE_PTR))
	{
	   tt1 = check_typedef (TYPE_TARGET_TYPE(tt1)); 
	   tt2 = check_typedef (TYPE_TARGET_TYPE(tt2));
	}
      if (TYPE_CODE(tt1) == TYPE_CODE(tt2)) continue;
      /* Array to pointer is a `trivial conversion' according to the ARM.  */

      /* We should be doing much hairier argument matching (see section 13.2
	 of the ARM), but as a quick kludge, just check for the same type
	 code.  */
      if (TYPE_CODE (t1[i]) != TYPE_CODE (VALUE_TYPE (t2[i])))
	return i+1;
    }
  if (!t1[i]) return 0;
  return t2[i] ? i+1 : 0;
}

/* Helper function used by value_struct_elt to recurse through baseclasses.
   Look for a field NAME in ARG1. Adjust the address of ARG1 by OFFSET bytes,
   and search in it assuming it has (class) type TYPE.
   If found, return value, else return NULL.

   If LOOKING_FOR_BASECLASS, then instead of looking for struct fields,
   look for a baseclass named NAME.  */

static value_ptr
search_struct_field (name, arg1, offset, type, looking_for_baseclass)
     char *name;
     register value_ptr arg1;
     int offset;
     register struct type *type;
     int looking_for_baseclass;
{
  int i;

  CHECK_TYPEDEF (type);

  if (! looking_for_baseclass)
    for (i = TYPE_NFIELDS (type) - 1; i >= TYPE_N_BASECLASSES (type); i--)
      {
	char *t_field_name = TYPE_FIELD_NAME (type, i);

	if (t_field_name && STREQ (t_field_name, name))
	  {
	    value_ptr v;
	    if (TYPE_FIELD_STATIC (type, i))
	      {
		char *phys_name = TYPE_FIELD_STATIC_PHYSNAME (type, i);
		struct symbol *sym =
		    lookup_symbol (phys_name, 0, VAR_NAMESPACE, 0, NULL);
		if (sym == NULL)
		    error ("Internal error: could not find physical static variable named %s",
			   phys_name);
		v = value_at (TYPE_FIELD_TYPE (type, i),
			      (CORE_ADDR)SYMBOL_BLOCK_VALUE (sym));
	      }
	    else
	      v = value_primitive_field (arg1, offset, i, type);
	    if (v == 0)
	      error("there is no field named %s", name);
	    return v;
	  }

	if (t_field_name
	    && (t_field_name[0] == '\0'
		|| (TYPE_CODE (type) == TYPE_CODE_UNION
		    && STREQ (t_field_name, "else"))))
	  {
	    struct type *field_type = TYPE_FIELD_TYPE (type, i);
	    if (TYPE_CODE (field_type) == TYPE_CODE_UNION
		|| TYPE_CODE (field_type) == TYPE_CODE_STRUCT)
	      {
		/* Look for a match through the fields of an anonymous union,
		   or anonymous struct.  C++ provides anonymous unions.

		   In the GNU Chill implementation of variant record types,
		   each <alternative field> has an (anonymous) union type,
		   each member of the union represents a <variant alternative>.
		   Each <variant alternative> is represented as a struct,
		   with a member for each <variant field>.  */
		   
		value_ptr v;
		int new_offset = offset;

		/* This is pretty gross.  In G++, the offset in an anonymous
		   union is relative to the beginning of the enclosing struct.
		   In the GNU Chill implementation of variant records,
		   the bitpos is zero in an anonymous union field, so we
		   have to add the offset of the union here. */
		if (TYPE_CODE (field_type) == TYPE_CODE_STRUCT
		    || (TYPE_NFIELDS (field_type) > 0
			&& TYPE_FIELD_BITPOS (field_type, 0) == 0))
		  new_offset += TYPE_FIELD_BITPOS (type, i) / 8;

		v = search_struct_field (name, arg1, new_offset, field_type,
					 looking_for_baseclass);
		if (v)
		  return v;
	      }
	  }
      }

  for (i = TYPE_N_BASECLASSES (type) - 1; i >= 0; i--)
    {
      value_ptr v;
      struct type *basetype = check_typedef (TYPE_BASECLASS (type, i));
      /* If we are looking for baseclasses, this is what we get when we
	 hit them.  But it could happen that the base part's member name
	 is not yet filled in.  */
      int found_baseclass = (looking_for_baseclass
			     && TYPE_BASECLASS_NAME (type, i) != NULL
			     && STREQ (name, TYPE_BASECLASS_NAME (type, i)));

      if (BASETYPE_VIA_VIRTUAL (type, i))
	{
	  int boffset = VALUE_OFFSET (arg1) + offset;
	  boffset = baseclass_offset (type, i,
				      VALUE_CONTENTS (arg1) + boffset,
				      VALUE_ADDRESS (arg1) + boffset);
	  if (boffset == -1)
	    error ("virtual baseclass botch");
	  if (found_baseclass)
	    {
	      value_ptr v2 = allocate_value (basetype);
	      VALUE_LVAL (v2) = VALUE_LVAL (arg1);
	      VALUE_ADDRESS (v2) = VALUE_ADDRESS (arg1);
	      VALUE_OFFSET (v2) = VALUE_OFFSET (arg1) + offset + boffset;
	      if (VALUE_LAZY (arg1))
		VALUE_LAZY (v2) = 1;
	      else
		memcpy (VALUE_CONTENTS_RAW (v2),
			VALUE_CONTENTS_RAW (arg1) + offset + boffset,
			TYPE_LENGTH (basetype));
	      return v2;
	    }
	  v = search_struct_field (name, arg1, offset + boffset,
				   TYPE_BASECLASS (type, i),
				   looking_for_baseclass);
	}
      else if (found_baseclass)
	v = value_primitive_field (arg1, offset, i, type);
      else
	v = search_struct_field (name, arg1,
				 offset + TYPE_BASECLASS_BITPOS (type, i) / 8,
				 basetype, looking_for_baseclass);
      if (v) return v;
    }
  return NULL;
}

/* Helper function used by value_struct_elt to recurse through baseclasses.
   Look for a field NAME in ARG1. Adjust the address of ARG1 by OFFSET bytes,
   and search in it assuming it has (class) type TYPE.
   If found, return value, else if name matched and args not return (value)-1,
   else return NULL. */

static value_ptr
search_struct_method (name, arg1p, args, offset, static_memfuncp, type)
     char *name;
     register value_ptr *arg1p, *args;
     int offset, *static_memfuncp;
     register struct type *type;
{
  int i;
  value_ptr v;
  int name_matched = 0;
  char dem_opname[64];

  CHECK_TYPEDEF (type);
  for (i = TYPE_NFN_FIELDS (type) - 1; i >= 0; i--)
    {
      char *t_field_name = TYPE_FN_FIELDLIST_NAME (type, i);
      if (strncmp(t_field_name, "__", 2)==0 ||
	strncmp(t_field_name, "op", 2)==0 ||
	strncmp(t_field_name, "type", 4)==0 )
	{
	  if (cplus_demangle_opname(t_field_name, dem_opname, DMGL_ANSI))
	    t_field_name = dem_opname;
	  else if (cplus_demangle_opname(t_field_name, dem_opname, 0))
	    t_field_name = dem_opname; 
	}
      if (t_field_name && STREQ (t_field_name, name))
	{
	  int j = TYPE_FN_FIELDLIST_LENGTH (type, i) - 1;
	  struct fn_field *f = TYPE_FN_FIELDLIST1 (type, i);
 	  name_matched = 1; 

	  if (j > 0 && args == 0)
	    error ("cannot resolve overloaded method `%s'", name);
	  while (j >= 0)
	    {
	      if (TYPE_FN_FIELD_STUB (f, j))
		check_stub_method (type, i, j);
	      if (!typecmp (TYPE_FN_FIELD_STATIC_P (f, j),
			    TYPE_FN_FIELD_ARGS (f, j), args))
		{
		  if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
		    return value_virtual_fn_field (arg1p, f, j, type, offset);
		  if (TYPE_FN_FIELD_STATIC_P (f, j) && static_memfuncp)
		    *static_memfuncp = 1;
		  v = value_fn_field (arg1p, f, j, type, offset);
		  if (v != NULL) return v;
		}
	      j--;
	    }
	}
    }

  for (i = TYPE_N_BASECLASSES (type) - 1; i >= 0; i--)
    {
      int base_offset;

      if (BASETYPE_VIA_VIRTUAL (type, i))
	{
	  base_offset = VALUE_OFFSET (*arg1p) + offset;
	  base_offset =
	    baseclass_offset (type, i,
			      VALUE_CONTENTS (*arg1p) + base_offset,
			      VALUE_ADDRESS (*arg1p) + base_offset);
	  if (base_offset == -1)
	    error ("virtual baseclass botch");
	}
      else
	{
	  base_offset = TYPE_BASECLASS_BITPOS (type, i) / 8;
        }
      v = search_struct_method (name, arg1p, args, base_offset + offset,
				static_memfuncp, TYPE_BASECLASS (type, i));
      if (v == (value_ptr) -1)
	{
	  name_matched = 1;
	}
      else if (v)
	{
/* FIXME-bothner:  Why is this commented out?  Why is it here?  */
/*	  *arg1p = arg1_tmp;*/
	  return v;
        }
    }
  if (name_matched) return (value_ptr) -1;
  else return NULL;
}

/* Given *ARGP, a value of type (pointer to a)* structure/union,
   extract the component named NAME from the ultimate target structure/union
   and return it as a value with its appropriate type.
   ERR is used in the error message if *ARGP's type is wrong.

   C++: ARGS is a list of argument types to aid in the selection of
   an appropriate method. Also, handle derived types.

   STATIC_MEMFUNCP, if non-NULL, points to a caller-supplied location
   where the truthvalue of whether the function that was resolved was
   a static member function or not is stored.

   ERR is an error message to be printed in case the field is not found.  */

value_ptr
value_struct_elt (argp, args, name, static_memfuncp, err)
     register value_ptr *argp, *args;
     char *name;
     int *static_memfuncp;
     char *err;
{
  register struct type *t;
  value_ptr v;

  COERCE_ARRAY (*argp);

  t = check_typedef (VALUE_TYPE (*argp));

  /* Follow pointers until we get to a non-pointer.  */

  while (TYPE_CODE (t) == TYPE_CODE_PTR || TYPE_CODE (t) == TYPE_CODE_REF)
    {
      *argp = value_ind (*argp);
      /* Don't coerce fn pointer to fn and then back again!  */
      if (TYPE_CODE (VALUE_TYPE (*argp)) != TYPE_CODE_FUNC)
	COERCE_ARRAY (*argp);
      t = check_typedef (VALUE_TYPE (*argp));
    }

  if (TYPE_CODE (t) == TYPE_CODE_MEMBER)
    error ("not implemented: member type in value_struct_elt");

  if (   TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Attempt to extract a component of a value that is not a %s.", err);

  /* Assume it's not, unless we see that it is.  */
  if (static_memfuncp)
    *static_memfuncp =0;

  if (!args)
    {
      /* if there are no arguments ...do this...  */

      /* Try as a field first, because if we succeed, there
	 is less work to be done.  */
      v = search_struct_field (name, *argp, 0, t, 0);
      if (v)
	return v;

      /* C++: If it was not found as a data field, then try to
         return it as a pointer to a method.  */

      if (destructor_name_p (name, t))
	error ("Cannot get value of destructor");

      v = search_struct_method (name, argp, args, 0, static_memfuncp, t);

      if (v == (value_ptr) -1)
	error ("Cannot take address of a method");
      else if (v == 0)
	{
	  if (TYPE_NFN_FIELDS (t))
	    error ("There is no member or method named %s.", name);
	  else
	    error ("There is no member named %s.", name);
	}
      return v;
    }

  if (destructor_name_p (name, t))
    {
      if (!args[1])
	{
	  /* destructors are a special case.  */
	  v = value_fn_field (NULL, TYPE_FN_FIELDLIST1 (t, 0),
			      TYPE_FN_FIELDLIST_LENGTH (t, 0), 0, 0);
	  if (!v) error("could not find destructor function named %s.", name);
	  else return v;
	}
      else
	{
	  error ("destructor should not have any argument");
	}
    }
  else
    v = search_struct_method (name, argp, args, 0, static_memfuncp, t);

  if (v == (value_ptr) -1)
    {
	error("Argument list of %s mismatch with component in the structure.", name);
    }
  else if (v == 0)
    {
      /* See if user tried to invoke data as function.  If so,
	 hand it back.  If it's not callable (i.e., a pointer to function),
	 gdb should give an error.  */
      v = search_struct_field (name, *argp, 0, t, 0);
    }

  if (!v)
    error ("Structure has no component named %s.", name);
  return v;
}

/* C++: return 1 is NAME is a legitimate name for the destructor
   of type TYPE.  If TYPE does not have a destructor, or
   if NAME is inappropriate for TYPE, an error is signaled.  */
int
destructor_name_p (name, type)
     const char *name;
     const struct type *type;
{
  /* destructors are a special case.  */

  if (name[0] == '~')
    {
      char *dname = type_name_no_tag (type);
      char *cp = strchr (dname, '<');
      unsigned int len;

      /* Do not compare the template part for template classes.  */
      if (cp == NULL)
	len = strlen (dname);
      else
	len = cp - dname;
      if (strlen (name + 1) != len || !STREQN (dname, name + 1, len))
	error ("name of destructor must equal name of class");
      else
	return 1;
    }
  return 0;
}

/* Helper function for check_field: Given TYPE, a structure/union,
   return 1 if the component named NAME from the ultimate
   target structure/union is defined, otherwise, return 0. */

static int
check_field_in (type, name)
     register struct type *type;
     const char *name;
{
  register int i;

  for (i = TYPE_NFIELDS (type) - 1; i >= TYPE_N_BASECLASSES (type); i--)
    {
      char *t_field_name = TYPE_FIELD_NAME (type, i);
      if (t_field_name && STREQ (t_field_name, name))
	return 1;
    }

  /* C++: If it was not found as a data field, then try to
     return it as a pointer to a method.  */

  /* Destructors are a special case.  */
  if (destructor_name_p (name, type))
    return 1;

  for (i = TYPE_NFN_FIELDS (type) - 1; i >= 0; --i)
    {
      if (STREQ (TYPE_FN_FIELDLIST_NAME (type, i), name))
	return 1;
    }

  for (i = TYPE_N_BASECLASSES (type) - 1; i >= 0; i--)
    if (check_field_in (TYPE_BASECLASS (type, i), name))
      return 1;
      
  return 0;
}


/* C++: Given ARG1, a value of type (pointer to a)* structure/union,
   return 1 if the component named NAME from the ultimate
   target structure/union is defined, otherwise, return 0.  */

int
check_field (arg1, name)
     register value_ptr arg1;
     const char *name;
{
  register struct type *t;

  COERCE_ARRAY (arg1);

  t = VALUE_TYPE (arg1);

  /* Follow pointers until we get to a non-pointer.  */

  for (;;)
    {
      CHECK_TYPEDEF (t);
      if (TYPE_CODE (t) != TYPE_CODE_PTR && TYPE_CODE (t) != TYPE_CODE_REF)
	break;
      t = TYPE_TARGET_TYPE (t);
    }

  if (TYPE_CODE (t) == TYPE_CODE_MEMBER)
    error ("not implemented: member type in check_field");

  if (   TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Internal error: `this' is not an aggregate");

  return check_field_in (t, name);
}

/* C++: Given an aggregate type CURTYPE, and a member name NAME,
   return the address of this member as a "pointer to member"
   type.  If INTYPE is non-null, then it will be the type
   of the member we are looking for.  This will help us resolve
   "pointers to member functions".  This function is used
   to resolve user expressions of the form "DOMAIN::NAME".  */

value_ptr
value_struct_elt_for_reference (domain, offset, curtype, name, intype)
     struct type *domain, *curtype, *intype;
     int offset;
     char *name;
{
  register struct type *t = curtype;
  register int i;
  value_ptr v;

  if (   TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Internal error: non-aggregate type to value_struct_elt_for_reference");

  for (i = TYPE_NFIELDS (t) - 1; i >= TYPE_N_BASECLASSES (t); i--)
    {
      char *t_field_name = TYPE_FIELD_NAME (t, i);
      
      if (t_field_name && STREQ (t_field_name, name))
	{
	  if (TYPE_FIELD_STATIC (t, i))
	    {
	      char *phys_name = TYPE_FIELD_STATIC_PHYSNAME (t, i);
	      struct symbol *sym =
		lookup_symbol (phys_name, 0, VAR_NAMESPACE, 0, NULL);
	      if (sym == NULL)
		error ("Internal error: could not find physical static variable named %s",
		       phys_name);
	      return value_at (SYMBOL_TYPE (sym),
			       (CORE_ADDR)SYMBOL_BLOCK_VALUE (sym));
	    }
	  if (TYPE_FIELD_PACKED (t, i))
	    error ("pointers to bitfield members not allowed");
	  
	  return value_from_longest
	    (lookup_reference_type (lookup_member_type (TYPE_FIELD_TYPE (t, i),
							domain)),
	     offset + (LONGEST) (TYPE_FIELD_BITPOS (t, i) >> 3));
	}
    }

  /* C++: If it was not found as a data field, then try to
     return it as a pointer to a method.  */

  /* Destructors are a special case.  */
  if (destructor_name_p (name, t))
    {
      error ("member pointers to destructors not implemented yet");
    }

  /* Perform all necessary dereferencing.  */
  while (intype && TYPE_CODE (intype) == TYPE_CODE_PTR)
    intype = TYPE_TARGET_TYPE (intype);

  for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; --i)
    {
      char *t_field_name = TYPE_FN_FIELDLIST_NAME (t, i);
      char dem_opname[64];

      if (strncmp(t_field_name, "__", 2)==0 ||
	strncmp(t_field_name, "op", 2)==0 ||
	strncmp(t_field_name, "type", 4)==0 )
	{
	  if (cplus_demangle_opname(t_field_name, dem_opname, DMGL_ANSI))
	    t_field_name = dem_opname;
	  else if (cplus_demangle_opname(t_field_name, dem_opname, 0))
	    t_field_name = dem_opname; 
	}
      if (t_field_name && STREQ (t_field_name, name))
	{
	  int j = TYPE_FN_FIELDLIST_LENGTH (t, i);
	  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, i);
	  
	  if (intype == 0 && j > 1)
	    error ("non-unique member `%s' requires type instantiation", name);
	  if (intype)
	    {
	      while (j--)
		if (TYPE_FN_FIELD_TYPE (f, j) == intype)
		  break;
	      if (j < 0)
		error ("no member function matches that type instantiation");
	    }
	  else
	    j = 0;
	  
	  if (TYPE_FN_FIELD_STUB (f, j))
	    check_stub_method (t, i, j);
	  if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
	    {
	      return value_from_longest
		(lookup_reference_type
		 (lookup_member_type (TYPE_FN_FIELD_TYPE (f, j),
				      domain)),
		 (LONGEST) METHOD_PTR_FROM_VOFFSET (TYPE_FN_FIELD_VOFFSET (f, j)));
	    }
	  else
	    {
	      struct symbol *s = lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, j),
						0, VAR_NAMESPACE, 0, NULL);
	      if (s == NULL)
		{
		  v = 0;
		}
	      else
		{
		  v = read_var_value (s, 0);
#if 0
		  VALUE_TYPE (v) = lookup_reference_type
		    (lookup_member_type (TYPE_FN_FIELD_TYPE (f, j),
					 domain));
#endif
		}
	      return v;
	    }
	}
    }
  for (i = TYPE_N_BASECLASSES (t) - 1; i >= 0; i--)
    {
      value_ptr v;
      int base_offset;

      if (BASETYPE_VIA_VIRTUAL (t, i))
	base_offset = 0;
      else
	base_offset = TYPE_BASECLASS_BITPOS (t, i) / 8;
      v = value_struct_elt_for_reference (domain,
					  offset + base_offset,
					  TYPE_BASECLASS (t, i),
					  name,
					  intype);
      if (v)
	return v;
    }
  return 0;
}

/* C++: return the value of the class instance variable, if one exists.
   Flag COMPLAIN signals an error if the request is made in an
   inappropriate context.  */

value_ptr
value_of_this (complain)
     int complain;
{
  struct symbol *func, *sym;
  struct block *b;
  int i;
  static const char funny_this[] = "this";
  value_ptr this;

  if (selected_frame == 0)
    if (complain)
      error ("no frame selected");
    else return 0;

  func = get_frame_function (selected_frame);
  if (!func)
    {
      if (complain)
	error ("no `this' in nameless context");
      else return 0;
    }

  b = SYMBOL_BLOCK_VALUE (func);
  i = BLOCK_NSYMS (b);
  if (i <= 0)
    if (complain)
      error ("no args, no `this'");
    else return 0;

  /* Calling lookup_block_symbol is necessary to get the LOC_REGISTER
     symbol instead of the LOC_ARG one (if both exist).  */
  sym = lookup_block_symbol (b, funny_this, VAR_NAMESPACE);
  if (sym == NULL)
    {
      if (complain)
	error ("current stack frame not in method");
      else
	return NULL;
    }

  this = read_var_value (sym, selected_frame);
  if (this == 0 && complain)
    error ("`this' argument at unknown address");
  return this;
}

/* Create a slice (sub-string, sub-array) of ARRAY, that is LENGTH elements
   long, starting at LOWBOUND.  The result has the same lower bound as
   the original ARRAY.  */

value_ptr
value_slice (array, lowbound, length)
     value_ptr array;
     int lowbound, length;
{
  struct type *slice_range_type, *slice_type, *range_type;
  LONGEST lowerbound, upperbound, offset;
  value_ptr slice;
  struct type *array_type;
  array_type = check_typedef (VALUE_TYPE (array));
  COERCE_VARYING_ARRAY (array, array_type);
  if (TYPE_CODE (array_type) != TYPE_CODE_ARRAY
      && TYPE_CODE (array_type) != TYPE_CODE_STRING
      && TYPE_CODE (array_type) != TYPE_CODE_BITSTRING)
    error ("cannot take slice of non-array");
  range_type = TYPE_INDEX_TYPE (array_type);
  if (get_discrete_bounds (range_type, &lowerbound, &upperbound) < 0)
    error ("slice from bad array or bitstring");
  if (lowbound < lowerbound || length < 0
      || lowbound + length - 1 > upperbound
      /* Chill allows zero-length strings but not arrays. */
      || (current_language->la_language == language_chill
	  && length == 0 && TYPE_CODE (array_type) == TYPE_CODE_ARRAY))
    error ("slice out of range");
  /* FIXME-type-allocation: need a way to free this type when we are
     done with it.  */
  slice_range_type = create_range_type ((struct type*) NULL,
					TYPE_TARGET_TYPE (range_type),
					lowerbound, lowerbound + length - 1);
  if (TYPE_CODE (array_type) == TYPE_CODE_BITSTRING)
    {
      int i;
      slice_type = create_set_type ((struct type*) NULL, slice_range_type);
      TYPE_CODE (slice_type) = TYPE_CODE_BITSTRING;
      slice = value_zero (slice_type, not_lval);
      for (i = 0; i < length; i++)
	{
	  int element = value_bit_index (array_type,
					 VALUE_CONTENTS (array),
					 lowbound + i);
	  if (element < 0)
	    error ("internal error accessing bitstring");
	  else if (element > 0)
	    {
	      int j = i % TARGET_CHAR_BIT;
	      if (BITS_BIG_ENDIAN)
		j = TARGET_CHAR_BIT - 1 - j;
	      VALUE_CONTENTS_RAW (slice)[i / TARGET_CHAR_BIT] |= (1 << j);
	    }
	}
      /* We should set the address, bitssize, and bitspos, so the clice
	 can be used on the LHS, but that may require extensions to
	 value_assign.  For now, just leave as a non_lval.  FIXME.  */
    }
  else
    {
      struct type *element_type = TYPE_TARGET_TYPE (array_type);
      offset
	= (lowbound - lowerbound) * TYPE_LENGTH (check_typedef (element_type));
      slice_type = create_array_type ((struct type*) NULL, element_type,
				      slice_range_type);
      TYPE_CODE (slice_type) = TYPE_CODE (array_type);
      slice = allocate_value (slice_type);
      if (VALUE_LAZY (array))
	VALUE_LAZY (slice) = 1;
      else
	memcpy (VALUE_CONTENTS (slice), VALUE_CONTENTS (array) + offset,
		TYPE_LENGTH (slice_type));
      if (VALUE_LVAL (array) == lval_internalvar)
	VALUE_LVAL (slice) = lval_internalvar_component;
      else
	VALUE_LVAL (slice) = VALUE_LVAL (array);
      VALUE_ADDRESS (slice) = VALUE_ADDRESS (array);
      VALUE_OFFSET (slice) = VALUE_OFFSET (array) + offset;
    }
  return slice;
}

/* Assuming chill_varying_type (VARRAY) is true, return an equivalent
   value as a fixed-length array. */

value_ptr
varying_to_slice (varray)
     value_ptr varray;
{
  struct type *vtype = check_typedef (VALUE_TYPE (varray));
  LONGEST length = unpack_long (TYPE_FIELD_TYPE (vtype, 0),
				VALUE_CONTENTS (varray)
				+ TYPE_FIELD_BITPOS (vtype, 0) / 8);
  return value_slice (value_primitive_field (varray, 0, 1, vtype), 0, length);
}

/* Create a value for a FORTRAN complex number.  Currently most of 
   the time values are coerced to COMPLEX*16 (i.e. a complex number 
   composed of 2 doubles.  This really should be a smarter routine 
   that figures out precision inteligently as opposed to assuming 
   doubles. FIXME: fmb */ 

value_ptr
value_literal_complex (arg1, arg2, type)
     value_ptr arg1;
     value_ptr arg2;
     struct type *type;
{
  register value_ptr val;
  struct type *real_type = TYPE_TARGET_TYPE (type);

  val = allocate_value (type);
  arg1 = value_cast (real_type, arg1);
  arg2 = value_cast (real_type, arg2);

  memcpy (VALUE_CONTENTS_RAW (val),
	  VALUE_CONTENTS (arg1), TYPE_LENGTH (real_type));
  memcpy (VALUE_CONTENTS_RAW (val) + TYPE_LENGTH (real_type),
	  VALUE_CONTENTS (arg2), TYPE_LENGTH (real_type));
  return val;
}

/* Cast a value into the appropriate complex data type. */

static value_ptr
cast_into_complex (type, val)
     struct type *type;
     register value_ptr val;
{
  struct type *real_type = TYPE_TARGET_TYPE (type);
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_COMPLEX)
    {
      struct type *val_real_type = TYPE_TARGET_TYPE (VALUE_TYPE (val));
      value_ptr re_val = allocate_value (val_real_type);
      value_ptr im_val = allocate_value (val_real_type);

      memcpy (VALUE_CONTENTS_RAW (re_val),
	      VALUE_CONTENTS (val), TYPE_LENGTH (val_real_type));
      memcpy (VALUE_CONTENTS_RAW (im_val),
	      VALUE_CONTENTS (val) + TYPE_LENGTH (val_real_type),
	       TYPE_LENGTH (val_real_type));

      return value_literal_complex (re_val, im_val, type);
    }
  else if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FLT
	   || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_INT)
    return value_literal_complex (val, value_zero (real_type, not_lval), type);
  else
    error ("cannot cast non-number to complex");
}

void
_initialize_valops ()
{
#if 0
  add_show_from_set
    (add_set_cmd ("abandon", class_support, var_boolean, (char *)&auto_abandon,
		  "Set automatic abandonment of expressions upon failure.",
		  &setlist),
     &showlist);
#endif
}
