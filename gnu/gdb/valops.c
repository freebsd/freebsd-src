/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)valops.c	6.4 (Berkeley) 5/8/91";
#endif /* not lint */

/* Perform non-arithmetic operations on values, for GDB.
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

#include "stdio.h"
#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "value.h"
#include "frame.h"
#include "inferior.h"

/* Cast value ARG2 to type TYPE and return as a value.
   More general than a C cast: accepts any two types of the same length,
   and if ARG2 is an lvalue it can be cast into anything at all.  */

value
value_cast (type, arg2)
     struct type *type;
     register value arg2;
{
  register enum type_code code1;
  register enum type_code code2;
  register int scalar;

  /* Coerce arrays but not enums.  Enums will work as-is
     and coercing them would cause an infinite recursion.  */
  if (TYPE_CODE (VALUE_TYPE (arg2)) != TYPE_CODE_ENUM)
    COERCE_ARRAY (arg2);

  code1 = TYPE_CODE (type);
  code2 = TYPE_CODE (VALUE_TYPE (arg2));
  scalar = (code2 == TYPE_CODE_INT || code2 == TYPE_CODE_FLT
	    || code2 == TYPE_CODE_ENUM);

  if (code1 == TYPE_CODE_FLT && scalar)
    return value_from_double (type, value_as_double (arg2));
  else if ((code1 == TYPE_CODE_INT || code1 == TYPE_CODE_ENUM)
	   && (scalar || code2 == TYPE_CODE_PTR))
    return value_from_long (type, value_as_long (arg2));
  else if (TYPE_LENGTH (type) == TYPE_LENGTH (VALUE_TYPE (arg2)))
    {
      VALUE_TYPE (arg2) = type;
      return arg2;
    }
  else if (VALUE_LVAL (arg2) == lval_memory)
    {
      return value_at (type, VALUE_ADDRESS (arg2) + VALUE_OFFSET (arg2));
    }
  else
    error ("Invalid cast.");
}

/* Create a value of type TYPE that is zero, and return it.  */

value
value_zero (type, lv)
     struct type *type;
     enum lval_type lv;
{
  register value val = allocate_value (type);

  bzero (VALUE_CONTENTS (val), TYPE_LENGTH (type));
  VALUE_LVAL (val) = lv;

  return val;
}

/* Return the value with a specified type located at specified address.  */

value
value_at (type, addr)
     struct type *type;
     CORE_ADDR addr;
{
  register value val = allocate_value (type);
  int temp;

  temp = read_memory (addr, VALUE_CONTENTS (val), TYPE_LENGTH (type));
  if (temp)
    {
      if (have_inferior_p () && !remote_debugging)
	print_sys_errmsg ("ptrace", temp);
      /* Actually, address between addr and addr + len was out of bounds. */
      error ("Cannot read memory: address 0x%x out of bounds.", addr);
    }

  VALUE_LVAL (val) = lval_memory;
  VALUE_ADDRESS (val) = addr;

  return val;
}

/* Store the contents of FROMVAL into the location of TOVAL.
   Return a new value with the location of TOVAL and contents of FROMVAL.  */

value
value_assign (toval, fromval)
     register value toval, fromval;
{
  register struct type *type = VALUE_TYPE (toval);
  register value val;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  int use_buffer = 0;

  extern CORE_ADDR find_saved_register ();

  COERCE_ARRAY (fromval);

  if (VALUE_LVAL (toval) != lval_internalvar)
    fromval = value_cast (type, fromval);

  /* If TOVAL is a special machine register requiring conversion
     of program values to a special raw format,
     convert FROMVAL's contents now, with result in `raw_buffer',
     and set USE_BUFFER to the number of bytes to write.  */

  if (VALUE_REGNO (toval) >= 0
      && REGISTER_CONVERTIBLE (VALUE_REGNO (toval)))
    {
      int regno = VALUE_REGNO (toval);
      if (VALUE_TYPE (fromval) != REGISTER_VIRTUAL_TYPE (regno))
	fromval = value_cast (REGISTER_VIRTUAL_TYPE (regno), fromval);
      bcopy (VALUE_CONTENTS (fromval), virtual_buffer,
	     REGISTER_VIRTUAL_SIZE (regno));
      REGISTER_CONVERT_TO_RAW (regno, virtual_buffer, raw_buffer);
      use_buffer = REGISTER_RAW_SIZE (regno);
    }

  switch (VALUE_LVAL (toval))
    {
    case lval_internalvar:
      set_internalvar (VALUE_INTERNALVAR (toval), fromval);
      break;

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
	  int val;
	  read_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
		       &val, sizeof val);
	  modify_field (&val, (int) value_as_long (fromval),
			VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
	  write_memory (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			&val, sizeof val);
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
	  int val;

	  read_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			       &val, sizeof val);
	  modify_field (&val, (int) value_as_long (fromval),
			VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
	  write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
				&val, sizeof val);
	}
      else if (use_buffer)
	write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			      raw_buffer, use_buffer);
      else
	write_register_bytes (VALUE_ADDRESS (toval) + VALUE_OFFSET (toval),
			      VALUE_CONTENTS (fromval), TYPE_LENGTH (type));
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
	char *buffer = (char *) alloca (amount_to_copy);
	int regno;
	FRAME frame;
	CORE_ADDR addr;

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
	    addr = find_saved_register (frame, regno);
	    if (addr == 0)
	      read_register_bytes (REGISTER_BYTE (regno),
				   buffer + amount_copied,
				   reg_size);
	    else
	      read_memory (addr, buffer + amount_copied, reg_size);
	  }

	/* Modify what needs to be modified.  */
	if (VALUE_BITSIZE (toval))
	  modify_field (buffer + byte_offset,
			(int) value_as_long (fromval),
			VALUE_BITPOS (toval), VALUE_BITSIZE (toval));
	else if (use_buffer)
	  bcopy (raw_buffer, buffer + byte_offset, use_buffer);
	else
	  bcopy (VALUE_CONTENTS (fromval), buffer + byte_offset,
		 TYPE_LENGTH (type));

	/* Copy it back.  */
	for ((regno = VALUE_FRAME_REGNUM (toval) + reg_offset,
	      amount_copied = 0);
	     amount_copied < amount_to_copy;
	     amount_copied += reg_size, regno++)
	  {
	    addr = find_saved_register (frame, regno);
	    if (addr == 0)
	      write_register_bytes (REGISTER_BYTE (regno),
				    buffer + amount_copied,
				    reg_size);
	    else
	      write_memory (addr, buffer + amount_copied, reg_size);
	  }
      }
      break;
	

    default:
      error ("Left side of = operation is not an lvalue.");
    }

  /* Return a value just like TOVAL except with the contents of FROMVAL
     (except in the case of the type if TOVAL is an internalvar).  */

  if (VALUE_LVAL (toval) == lval_internalvar
      || VALUE_LVAL (toval) == lval_internalvar_component)
    {
      type = VALUE_TYPE (fromval);
    }

  val = allocate_value (type);
  bcopy (toval, val, VALUE_CONTENTS (val) - (char *) val);
  bcopy (VALUE_CONTENTS (fromval), VALUE_CONTENTS (val), TYPE_LENGTH (type));
  VALUE_TYPE (val) = type;
  
  return val;
}

/* Extend a value VAL to COUNT repetitions of its type.  */

value
value_repeat (arg1, count)
     value arg1;
     int count;
{
  register value val;

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Only values in memory can be extended with '@'.");
  if (count < 1)
    error ("Invalid number %d of repetitions.", count);

  val = allocate_repeat_value (VALUE_TYPE (arg1), count);

  read_memory (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1),
	       VALUE_CONTENTS (val),
	       TYPE_LENGTH (VALUE_TYPE (val)) * count);
  VALUE_LVAL (val) = lval_memory;
  VALUE_ADDRESS (val) = VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1);

  return val;
}

value
value_of_variable (var)
     struct symbol *var;
{
  return read_var_value (var, (FRAME) 0);
}

/* Given a value which is an array, return a value which is
   a pointer to its first element.  */

value
value_coerce_array (arg1)
     value arg1;
{
  register struct type *type;
  register value val;

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Attempt to take address of value not located in memory.");

  /* Get type of elements.  */
  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_ARRAY)
    type = TYPE_TARGET_TYPE (VALUE_TYPE (arg1));
  else
    /* A phony array made by value_repeat.
       Its type is the type of the elements, not an array type.  */
    type = VALUE_TYPE (arg1);

  /* Get the type of the result.  */
  type = lookup_pointer_type (type);
  val = value_from_long (builtin_type_long,
		       (LONGEST) (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1)));
  VALUE_TYPE (val) = type;
  return val;
}

/* Return a pointer value for the object for which ARG1 is the contents.  */

value
value_addr (arg1)
     value arg1;
{
  register struct type *type;
  register value val, arg1_coerced;

  /* Taking the address of an array is really a no-op
     once the array is coerced to a pointer to its first element.  */
  arg1_coerced = arg1;
  COERCE_ARRAY (arg1_coerced);
  if (arg1 != arg1_coerced)
    return arg1_coerced;

  if (VALUE_LVAL (arg1) != lval_memory)
    error ("Attempt to take address of value not located in memory.");

  /* Get the type of the result.  */
  type = lookup_pointer_type (VALUE_TYPE (arg1));
  val = value_from_long (builtin_type_long,
		(LONGEST) (VALUE_ADDRESS (arg1) + VALUE_OFFSET (arg1)));
  VALUE_TYPE (val) = type;
  return val;
}

/* Given a value of a pointer type, apply the C unary * operator to it.  */

value
value_ind (arg1)
     value arg1;
{
  /* Must do this before COERCE_ARRAY, otherwise an infinite loop
     will result */
  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_REF)
    return value_at (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)),
		     (CORE_ADDR) value_as_long (arg1));

  COERCE_ARRAY (arg1);

  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_MEMBER)
    error ("not implemented: member types in value_ind");

  /* Allow * on an integer so we can cast it to whatever we want.
     This returns an int, which seems like the most C-like thing
     to do.  "long long" variables are rare enough that
     BUILTIN_TYPE_LONGEST would seem to be a mistake.  */
  if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_INT)
    return value_at (builtin_type_int,
		     (CORE_ADDR) value_as_long (arg1));
  else if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR)
    return value_at (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)),
		     (CORE_ADDR) value_as_long (arg1));
  else if (TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_REF)
    return value_at (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)),
		     (CORE_ADDR) value_as_long (arg1));
  error ("Attempt to take contents of a non-pointer value.");
}

/* Pushing small parts of stack frames.  */

/* Push one word (the size of object that a register holds).  */

CORE_ADDR
push_word (sp, buffer)
     CORE_ADDR sp;
     REGISTER_TYPE buffer;
{
  register int len = sizeof (REGISTER_TYPE);

#if 1 INNER_THAN 2
  sp -= len;
  write_memory (sp, &buffer, len);
#else /* stack grows upward */
  write_memory (sp, &buffer, len);
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

CORE_ADDR
value_push (sp, arg)
     register CORE_ADDR sp;
     value arg;
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
   for arguments to be passed to C functions.  */

value
value_arg_coerce (arg)
     value arg;
{
  register struct type *type;

  COERCE_ENUM (arg);

  type = VALUE_TYPE (arg);

  if (TYPE_CODE (type) == TYPE_CODE_INT
      && TYPE_LENGTH (type) < sizeof (int))
    return value_cast (builtin_type_int, arg);

  if (type == builtin_type_float)
    return value_cast (builtin_type_double, arg);

  return arg;
}

/* Push the value ARG, first coercing it as an argument
   to a C function.  */

CORE_ADDR
value_arg_push (sp, arg)
     register CORE_ADDR sp;
     value arg;
{
  return value_push (sp, value_arg_coerce (arg));
}

#ifdef NEW_CALL_FUNCTION

int
arg_stacklen(nargs, args)
	int nargs;
	value *args;
{
	int len = 0;

	while (--nargs >= 0)
		len += TYPE_LENGTH(VALUE_TYPE(value_arg_coerce(args[nargs])));

	return len;
}

CORE_ADDR
function_address(function, type)
	value function;
	struct type **type;
{
	register CORE_ADDR funaddr;
	register struct type *ftype = VALUE_TYPE(function);
	register enum type_code code = TYPE_CODE(ftype);

	/*
	 * If it's a member function, just look at the function part
	 * of it.
	 */

	/* Determine address to call.  */
	if (code == TYPE_CODE_FUNC || code == TYPE_CODE_METHOD) {
		funaddr = VALUE_ADDRESS(function);
		*type = TYPE_TARGET_TYPE(ftype);
	} else if (code == TYPE_CODE_PTR) {
		funaddr = value_as_long(function);
		if (TYPE_CODE(TYPE_TARGET_TYPE(ftype)) == TYPE_CODE_FUNC
		    || TYPE_CODE(TYPE_TARGET_TYPE(ftype)) == TYPE_CODE_METHOD)
			*type = TYPE_TARGET_TYPE(TYPE_TARGET_TYPE(ftype));
		else
			*type = builtin_type_int;
	} else if (code == TYPE_CODE_INT) {
		/*
		 * Handle the case of functions lacking debugging
		 * info. Their values are characters since their
		 * addresses are char
		 */
		if (TYPE_LENGTH(ftype) == 1)

			funaddr = value_as_long(value_addr(function));
		else
			/*
			 * Handle integer used as address of a
			 * function.
			 */
			funaddr = value_as_long(function);
		
		*type = builtin_type_int;
	} else
		error("Invalid data type for function to be called.");

	return funaddr;
}
	
/* Perform a function call in the inferior.
   ARGS is a vector of values of arguments (NARGS of them).
   FUNCTION is a value, the function to be called.
   Returns a value representing what the function returned.
   May fail to return, if a breakpoint or signal is hit
   during the execution of the function.  */

value
call_function(function, nargs, args)
	value function;
	int nargs;
	value *args;
{
	register CORE_ADDR sp, pc;
	struct type *value_type;
	struct inferior_status inf_status;
	struct cleanup *old_chain;
	register CORE_ADDR funaddr;
	int struct_return_bytes;
	char retbuf[REGISTER_BYTES];

	if (!have_inferior_p())
	    error("Cannot invoke functions if the inferior is not running.");

	save_inferior_status(&inf_status, 1);
	old_chain = make_cleanup(restore_inferior_status, &inf_status);

	sp = read_register(SP_REGNUM);
	funaddr = function_address(function, &value_type);
	/*
	 * Are we returning a value using a structure return or a
	 * normal value return?
	 */
	if (using_struct_return(function, funaddr, value_type))
		struct_return_bytes = TYPE_LENGTH(value_type);
	else 
		struct_return_bytes = 0;
	/*
	 * Create a call sequence customized for this function and
	 * the number of arguments for it.
	 */
	pc = setup_dummy(sp, funaddr, nargs, args, 
			 struct_return_bytes, value_arg_push);

	/*
	 * Execute the stack dummy stub.  The register state will be
	 * returned in retbuf.  It is restored below.
	 */
	run_stack_dummy(pc, retbuf);

	/*
	 * This will restore the register context that existed before
	 * we called the dummy function.
	 */
	do_cleanups(old_chain);

	return value_being_returned(value_type, retbuf, struct_return_bytes);
}
#else

/* Perform a function call in the inferior.
   ARGS is a vector of values of arguments (NARGS of them).
   FUNCTION is a value, the function to be called.
   Returns a value representing what the function returned.
   May fail to return, if a breakpoint or signal is hit
   during the execution of the function.  */

value
call_function (function, nargs, args)
     value function;
     int nargs;
     value *args;
{
  register CORE_ADDR sp;
  register int i;
  CORE_ADDR start_sp;
  static REGISTER_TYPE dummy[] = CALL_DUMMY;
  REGISTER_TYPE dummy1[sizeof dummy / sizeof (REGISTER_TYPE)];
  CORE_ADDR old_sp;
  struct type *value_type;
  unsigned char struct_return;
  CORE_ADDR struct_addr;
  struct inferior_status inf_status;
  struct cleanup *old_chain;

  if (!have_inferior_p ())
    error ("Cannot invoke functions if the inferior is not running.");

  save_inferior_status (&inf_status, 1);
  old_chain = make_cleanup (restore_inferior_status, &inf_status);

  /* PUSH_DUMMY_FRAME is responsible for saving the inferior registers
     (and POP_FRAME for restoring them).  (At least on most machines)
     they are saved on the stack in the inferior.  */
  PUSH_DUMMY_FRAME;

  old_sp = sp = read_register (SP_REGNUM);

#if 1 INNER_THAN 2		/* Stack grows down */
  sp -= sizeof dummy;
  start_sp = sp;
#else				/* Stack grows up */
  start_sp = sp;
  sp += sizeof dummy;
#endif

  {
    register CORE_ADDR funaddr;
    register struct type *ftype = VALUE_TYPE (function);
    register enum type_code code = TYPE_CODE (ftype);

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
	funaddr = value_as_long (function);
	if (TYPE_CODE (TYPE_TARGET_TYPE (ftype)) == TYPE_CODE_FUNC
	    || TYPE_CODE (TYPE_TARGET_TYPE (ftype)) == TYPE_CODE_METHOD)
	  value_type = TYPE_TARGET_TYPE (TYPE_TARGET_TYPE (ftype));
	else
	  value_type = builtin_type_int;
      }
    else if (code == TYPE_CODE_INT)
      {
	/* Handle the case of functions lacking debugging info.
	   Their values are characters since their addresses are char */
	if (TYPE_LENGTH (ftype) == 1)
	  funaddr = value_as_long (value_addr (function));
	else
	  /* Handle integer used as address of a function.  */
	  funaddr = value_as_long (function);

	value_type = builtin_type_int;
      }
    else
      error ("Invalid data type for function to be called.");

    /* Are we returning a value using a structure return or a normal
       value return? */

    struct_return = using_struct_return (function, funaddr, value_type);

    /* Create a call sequence customized for this function
       and the number of arguments for it.  */
    bcopy (dummy, dummy1, sizeof dummy);
    FIX_CALL_DUMMY (dummy1, start_sp, funaddr, nargs, value_type);
  }

#ifndef CANNOT_EXECUTE_STACK
  write_memory (start_sp, dummy1, sizeof dummy);

#else
  /* Convex Unix prohibits executing in the stack segment. */
  /* Hope there is empty room at the top of the text segment. */
  {
    extern CORE_ADDR text_end;
    static checked = 0;
    if (!checked)
      for (start_sp = text_end - sizeof dummy; start_sp < text_end; ++start_sp)
	if (read_memory_integer (start_sp, 1) != 0)
	  error ("text segment full -- no place to put call");
    checked = 1;
    sp = old_sp;
    start_sp = text_end - sizeof dummy;
    write_memory (start_sp, dummy1, sizeof dummy);
  }
#endif /* CANNOT_EXECUTE_STACK */
#ifdef STACK_ALIGN
  /* If stack grows down, we must leave a hole at the top. */
  {
    int len = 0;

    /* Reserve space for the return structure to be written on the
       stack, if necessary */

    if (struct_return)
      len += TYPE_LENGTH (value_type);
    
    for (i = nargs - 1; i >= 0; i--)
      len += TYPE_LENGTH (VALUE_TYPE (value_arg_coerce (args[i])));
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

    /* Reserve space for the return structure to be written on the
       stack, if necessary */

    if (struct_return)
      {
#if 1 INNER_THAN 2
	sp -= TYPE_LENGTH (value_type);
	struct_addr = sp;
#else
	struct_addr = sp;
	sp += TYPE_LENGTH (value_type);
#endif
      }
    
  for (i = nargs - 1; i >= 0; i--)
    sp = value_arg_push (sp, args[i]);

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

     Also note that on some machines (like the sparc) pcc uses this
     convention in a slightly twisted way also.  */

  if (struct_return)
    STORE_STRUCT_RETURN (struct_addr, sp);

  /* Write the stack pointer.  This is here because the statement above
     might fool with it */
  write_register (SP_REGNUM, sp);

  /* Figure out the value returned by the function.  */
  {
    char retbuf[REGISTER_BYTES];

    /* Execute the stack dummy routine, calling FUNCTION.
       When it is done, discard the empty frame
       after storing the contents of all regs into retbuf.  */
    run_stack_dummy (start_sp + CALL_DUMMY_START_OFFSET, retbuf);

    do_cleanups (old_chain);

    return value_being_returned (value_type, retbuf, struct_return);
  }
}
#endif

/* Create a value for a string constant:
   Call the function malloc in the inferior to get space for it,
   then copy the data into that space
   and then return the address with type char *.
   PTR points to the string constant data; LEN is number of characters.  */

value
value_string (ptr, len)
     char *ptr;
     int len;
{
  register value val;
  register struct symbol *sym;
  value blocklen;
  register char *copy = (char *) alloca (len + 1);
  char *i = ptr;
  register char *o = copy, *ibeg = ptr;
  register int c;
#ifdef KERNELDEBUG
  extern int kernel_debugging;

  if (kernel_debugging)
    error("Can't stuff string constants into kernel (yet).");
#endif

  /* Copy the string into COPY, processing escapes.
     We could not conveniently process them in expread
     because the string there wants to be a substring of the input.  */

  while (i - ibeg < len)
    {
      c = *i++;
      if (c == '\\')
	{
	  c = parse_escape (&i);
	  if (c == -1)
	    continue;
	}
      *o++ = c;
    }
  *o = 0;

  /* Get the length of the string after escapes are processed.  */

  len = o - copy;

  /* Find the address of malloc in the inferior.  */

  sym = lookup_symbol ("malloc", 0, VAR_NAMESPACE, 0);
  if (sym != 0)
    {
      if (SYMBOL_CLASS (sym) != LOC_BLOCK)
	error ("\"malloc\" exists in this program but is not a function.");
      val = value_of_variable (sym);
    }
  else
    {
      register int i;
      for (i = 0; i < misc_function_count; i++)
	if (!strcmp (misc_function_vector[i].name, "malloc"))
	  break;
      if (i < misc_function_count)
	val = value_from_long (builtin_type_long,
			     (LONGEST) misc_function_vector[i].address);
      else
	error ("String constants require the program to have a function \"malloc\".");
    }

  blocklen = value_from_long (builtin_type_int, (LONGEST) (len + 1));
  val = call_function (val, 1, &blocklen);
  if (value_zerop (val))
    error ("No memory available for string constant.");
  write_memory ((CORE_ADDR) value_as_long (val), copy, len + 1);
  VALUE_TYPE (val) = lookup_pointer_type (builtin_type_char);
  return val;
}

static int
type_field_index(t, name)
  register struct type *t;
  register char *name;
{
  register int i;

  for (i = TYPE_NFIELDS(t); --i >= 0;)
    {
      register char *t_field_name = TYPE_FIELD_NAME (t, i);

      if (t_field_name && !strcmp (t_field_name, name))
	break;
    }
  return (i);
}

/* Given ARG1, a value of type (pointer to a)* structure/union,
   extract the component named NAME from the ultimate target structure/union
   and return it as a value with its appropriate type.
   ERR is used in the error message if ARG1's type is wrong.

   C++: ARGS is a list of argument types to aid in the selection of
   an appropriate method. Also, handle derived types.

   STATIC_MEMFUNCP, if non-NULL, points to a caller-supplied location
   where the truthvalue of whether the function that was resolved was
   a static member function or not.

   ERR is an error message to be printed in case the field is not found.  */

value
value_struct_elt (arg1, args, name, static_memfuncp, err)
     register value arg1, *args;
     char *name;
     int *static_memfuncp;
     char *err;
{
  register struct type *t;
  register int i;
  int found = 0;

  struct type *baseclass;

  COERCE_ARRAY (arg1);

  t = VALUE_TYPE (arg1);

  /* Check for the usual case: we have pointer, target type is a struct
   * and `name' is a legal field of the struct.  In this case, we can
   * just snarf the value of the field & not waste time while value_ind
   * sucks over the entire struct. */
  if (! args)
    {
      if (TYPE_CODE(t) == TYPE_CODE_PTR
          && (TYPE_CODE((baseclass = TYPE_TARGET_TYPE(t))) == TYPE_CODE_STRUCT
	      || TYPE_CODE(baseclass) == TYPE_CODE_UNION)
          && (i = type_field_index(baseclass, name)) >= 0)
	{
	  register int offset;
	  register struct type *f = TYPE_FIELD_TYPE(baseclass, i);

	  offset = TYPE_FIELD_BITPOS(baseclass, i) >> 3;
	  if (TYPE_FIELD_BITSIZE(baseclass, i) == 0)
	    return value_at(f, (CORE_ADDR)(value_as_long(arg1) + offset));
	}
    }

  /* Follow pointers until we get to a non-pointer.  */

  while (TYPE_CODE (t) == TYPE_CODE_PTR || TYPE_CODE (t) == TYPE_CODE_REF)
    {
      arg1 = value_ind (arg1);
      COERCE_ARRAY (arg1);
      t = VALUE_TYPE (arg1);
    }

  if (TYPE_CODE (t) == TYPE_CODE_MEMBER)
    error ("not implemented: member type in value_struct_elt");

  if (TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Attempt to extract a component of a value that is not a %s.", err);

  baseclass = t;

  /* Assume it's not, unless we see that it is.  */
  if (static_memfuncp)
    *static_memfuncp =0;

  if (!args)
    {
      /* if there are no arguments ...do this...  */

      /* Try as a variable first, because if we succeed, there
	 is less work to be done.  */
      while (t)
	{
	  i = type_field_index(t, name);
	  if (i >= 0)
	    return TYPE_FIELD_STATIC (t, i)
	      ? value_static_field (t, name, i) : value_field (arg1, i);

	  if (TYPE_N_BASECLASSES (t) == 0)
	    break;

	  t = TYPE_BASECLASS (t, 1);
	  VALUE_TYPE (arg1) = t; /* side effect! */
	}

      /* C++: If it was not found as a data field, then try to
         return it as a pointer to a method.  */
      t = baseclass;
      VALUE_TYPE (arg1) = t;	/* side effect! */

      if (destructor_name_p (name, t))
	error ("use `info method' command to print out value of destructor");

      while (t)
	{
	  for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; --i)
	    {
	      if (! strcmp (TYPE_FN_FIELDLIST_NAME (t, i), name))
		{
		  error ("use `info method' command to print value of method \"%s\"", name);
		}
	    }

	  if (TYPE_N_BASECLASSES (t) == 0)
	    break;

	  t = TYPE_BASECLASS (t, 1);
	}

      error ("There is no field named %s.", name);
      return 0;
    }

  if (destructor_name_p (name, t))
    {
      if (!args[1])
	{
	  /* destructors are a special case.  */
	  return (value)value_fn_field (arg1, 0,
					TYPE_FN_FIELDLIST_LENGTH (t, 0));
	}
      else
	{
	  error ("destructor should not have any argument");
	}
    }

  /*   This following loop is for methods with arguments.  */
  while (t)
    {
      /* Look up as method first, because that is where we
	 expect to find it first.  */
      for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; i--)
	{
	  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, i);

	  if (!strcmp (TYPE_FN_FIELDLIST_NAME (t, i), name))
	    {
	      int j;
	      struct fn_field *f = TYPE_FN_FIELDLIST1 (t, i);

	      found = 1;
	      for (j = TYPE_FN_FIELDLIST_LENGTH (t, i) - 1; j >= 0; --j)
		if (!typecmp (TYPE_FN_FIELD_STATIC_P (f, j),
			      TYPE_FN_FIELD_ARGS (f, j), args))
		  {
		    if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
		      return (value)value_virtual_fn_field (arg1, f, j, t);
		    if (TYPE_FN_FIELD_STATIC_P (f, j) && static_memfuncp)
		      *static_memfuncp = 1;
		    return (value)value_fn_field (arg1, i, j);
		  }
	    }
	}

      if (TYPE_N_BASECLASSES (t) == 0)
	break;
      
      t = TYPE_BASECLASS (t, 1);
      VALUE_TYPE (arg1) = t;	/* side effect! */
    }

  if (found)
    {
      error ("Structure method %s not defined for arglist.", name);
      return 0;
    }
  else
    {
      /* See if user tried to invoke data as function */
      t = baseclass;
      while (t)
	{
	  i = type_field_index(t, name);
	  if (i >= 0)
	    return TYPE_FIELD_STATIC (t, i)
	      ? value_static_field (t, name, i) : value_field (arg1, i);

	  if (TYPE_N_BASECLASSES (t) == 0)
	    break;

	  t = TYPE_BASECLASS (t, 1);
	  VALUE_TYPE (arg1) = t; /* side effect! */
	}
      error ("Structure has no component named %s.", name);
    }
}

/* C++: return 1 is NAME is a legitimate name for the destructor
   of type TYPE.  If TYPE does not have a destructor, or
   if NAME is inappropriate for TYPE, an error is signaled.  */
int
destructor_name_p (name, type)
     char *name;
     struct type *type;
{
  /* destructors are a special case.  */
  char *dname = TYPE_NAME (type);

  if (name[0] == '~')
    {
      if (! TYPE_HAS_DESTRUCTOR (type))
	error ("type `%s' does not have destructor defined",
	       TYPE_NAME (type));
      /* Skip past the "struct " at the front.  */
      while (*dname++ != ' ') ;
      if (strcmp (dname, name+1))
	error ("destructor specification error");
      else
	return 1;
    }
  return 0;
}

/* C++: Given ARG1, a value of type (pointer to a)* structure/union,
   return 1 if the component named NAME from the ultimate
   target structure/union is defined, otherwise, return 0.  */

int
check_field (arg1, name)
     register value arg1;
     char *name;
{
  register struct type *t;
  register int i;
  int found = 0;

  struct type *baseclass;

  COERCE_ARRAY (arg1);

  t = VALUE_TYPE (arg1);

  /* Follow pointers until we get to a non-pointer.  */

  while (TYPE_CODE (t) == TYPE_CODE_PTR || TYPE_CODE (t) == TYPE_CODE_REF)
    t = TYPE_TARGET_TYPE (t);

  if (TYPE_CODE (t) == TYPE_CODE_MEMBER)
    error ("not implemented: member type in check_field");

  if (TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Internal error: `this' is not an aggregate");

  baseclass = t;

  while (t)
    {
      for (i = TYPE_NFIELDS (t) - 1; i >= 0; i--)
	{
	  char *t_field_name = TYPE_FIELD_NAME (t, i);
	  if (t_field_name && !strcmp (t_field_name, name))
		  goto success;
	}
      if (TYPE_N_BASECLASSES (t) == 0)
	break;

      t = TYPE_BASECLASS (t, 1);
      VALUE_TYPE (arg1) = t;	/* side effect! */
    }

  /* C++: If it was not found as a data field, then try to
     return it as a pointer to a method.  */
  t = baseclass;

  /* Destructors are a special case.  */
  if (destructor_name_p (name, t))
    goto success;

  while (t)
    {
      for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; --i)
	{
	  if (!strcmp (TYPE_FN_FIELDLIST_NAME (t, i), name))
	    return 1;
	}

      if (TYPE_N_BASECLASSES (t) == 0)
	break;

      t = TYPE_BASECLASS (t, 1);
    }
  return 0;

 success:
  t = VALUE_TYPE (arg1);
  while (TYPE_CODE (t) == TYPE_CODE_PTR || TYPE_CODE (t) == TYPE_CODE_REF)
    {
      arg1 = value_ind (arg1);
      COERCE_ARRAY (arg1);
      t = VALUE_TYPE (arg1);
    }
}

/* C++: Given an aggregate type DOMAIN, and a member name NAME,
   return the address of this member as a pointer to member
   type.  If INTYPE is non-null, then it will be the type
   of the member we are looking for.  This will help us resolve
   pointers to member functions.  */

value
value_struct_elt_for_address (domain, intype, name)
     struct type *domain, *intype;
     char *name;
{
  register struct type *t = domain;
  register int i;
  int found = 0;
  value v;

  struct type *baseclass;

  if (TYPE_CODE (t) != TYPE_CODE_STRUCT
      && TYPE_CODE (t) != TYPE_CODE_UNION)
    error ("Internal error: non-aggregate type to value_struct_elt_for_address");

  baseclass = t;

  while (t)
    {
      for (i = TYPE_NFIELDS (t) - 1; i >= 0; i--)
	{
	  char *t_field_name = TYPE_FIELD_NAME (t, i);
	  if (t_field_name && !strcmp (t_field_name, name))
	    {
	      if (TYPE_FIELD_PACKED (t, i))
		error ("pointers to bitfield members not allowed");

	      v = value_from_long (builtin_type_int,
				   (LONGEST) (TYPE_FIELD_BITPOS (t, i) >> 3));
	      VALUE_TYPE (v) = lookup_pointer_type (
		      lookup_member_type (TYPE_FIELD_TYPE (t, i), baseclass));
	      return v;
	    }
	}

      if (TYPE_N_BASECLASSES (t) == 0)
	break;

      t = TYPE_BASECLASS (t, 1);
    }

  /* C++: If it was not found as a data field, then try to
     return it as a pointer to a method.  */
  t = baseclass;

  /* Destructors are a special case.  */
  if (destructor_name_p (name, t))
    {
      error ("pointers to destructors not implemented yet");
    }

  /* Perform all necessary dereferencing.  */
  while (intype && TYPE_CODE (intype) == TYPE_CODE_PTR)
    intype = TYPE_TARGET_TYPE (intype);

  while (t)
    {
      for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; --i)
	{
	  if (!strcmp (TYPE_FN_FIELDLIST_NAME (t, i), name))
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

	      if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
		{
		  v = value_from_long (builtin_type_long,
				       (LONGEST) TYPE_FN_FIELD_VOFFSET (f, j));
		}
	      else
		{
		  struct symbol *s = lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, j),
						    0, VAR_NAMESPACE, 0);
		  v = locate_var_value (s, 0);
		}
	      VALUE_TYPE (v) = lookup_pointer_type (lookup_member_type (TYPE_FN_FIELD_TYPE (f, j), baseclass));
	      return v;
	    }
	}

      if (TYPE_N_BASECLASSES (t) == 0)
	break;

      t = TYPE_BASECLASS (t, 1);
    }
  return 0;
}

/* Compare two argument lists and return the position in which they differ,
   or zero if equal.

   STATICP is nonzero if the T1 argument list came from a
   static member function.

   For non-static member functions, we ignore the first argument,
   which is the type of the instance variable.  This is because we want
   to handle calls with objects from derived classes.  This is not
   entirely correct: we should actually check to make sure that a
   requested operation is type secure, shouldn't we?  */

int
typecmp (staticp, t1, t2)
     int staticp;
     struct type *t1[];
     value t2[];
{
  int i;

  if (staticp && t1 == 0)
    return t2[1] != 0;
  if (t1 == 0)
    return 1;
  if (t1[0]->code == TYPE_CODE_VOID) return 0;
  if (t1[!staticp] == 0) return 0;
  for (i = !staticp; t1[i] && t1[i]->code != TYPE_CODE_VOID; i++)
    {
      if (! t2[i]
	  || t1[i]->code != t2[i]->type->code
	  || t1[i]->target_type != t2[i]->type->target_type)
	return i+1;
    }
  if (!t1[i]) return 0;
  return t2[i] ? i+1 : 0;
}

/* C++: return the value of the class instance variable, if one exists.
   Flag COMPLAIN signals an error if the request is made in an
   inappropriate context.  */
value
value_of_this (complain)
     int complain;
{
  extern FRAME selected_frame;
  struct symbol *func, *sym;
  char *funname = 0;
  struct block *b;
  int i;

  if (selected_frame == 0)
    if (complain)
      error ("no frame selected");
    else return 0;

  func = get_frame_function (selected_frame);
  if (func)
    funname = SYMBOL_NAME (func);
  else
    if (complain)
      error ("no `this' in nameless context");
    else return 0;

  b = SYMBOL_BLOCK_VALUE (func);
  i = BLOCK_NSYMS (b);
  if (i <= 0)
    if (complain)
      error ("no args, no `this'");
    else return 0;

  sym = BLOCK_SYM (b, 0);
  if (strncmp ("$this", SYMBOL_NAME (sym), 5))
    if (complain)
      error ("current stack frame not in method");
    else return 0;

  return read_var_value (sym, selected_frame);
}
