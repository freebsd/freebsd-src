/* Definitions for values of C expressions, for GDB.
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

/*
 * The structure which defines the type of a value.  It should never
 * be possible for a program lval value to survive over a call to the inferior
 * (ie to be put into the history list or an internal variable).
 */
enum lval_type {
  /* Not an lval.  */
  not_lval,
  /* In memory.  Could be a saved register.  */
  lval_memory,
  /* In a register.  */
  lval_register,
  /* In a gdb internal variable.  */
  lval_internalvar,
  /* Part of a gdb internal variable (structure field).  */
  lval_internalvar_component,
  /* In a register series in a frame not the current one, which may have been
     partially saved or saved in different places (otherwise would be
     lval_register or lval_memory).  */
  lval_reg_frame_relative,
};

struct value
  {
    /* Type of value; either not an lval, or one of the various
       different possible kinds of lval.  */
    enum lval_type lval;
    /* Location of value (if lval).  */
    union
      {
	/* Address in inferior or byte of registers structure.  */
	CORE_ADDR address;
	/* Pointer to interrnal variable.  */
	struct internalvar *internalvar;
	/* Number of register.  Only used with
	   lval_reg_frame_relative.  */
	int regnum;
      } location;
    /* Describes offset of a value within lval a structure in bytes.  */
    int offset;	
    /* Only used for bitfields; number of bits contained in them.  */
    int bitsize;
    /* Only used for bitfields; position of start of field.  */
    int bitpos;
    /* Frame value is relative to.  In practice, this address is only
       used if the value is stored in several registers in other than
       the current frame, and these registers have not all been saved
       at the same place in memory.  This will be described in the
       lval enum above as "lval_reg_frame_relative".  */
    CORE_ADDR frame_addr;
    /* Type of the value.  */
    struct type *type;
    /* Values are stored in a chain, so that they can be deleted
       easily over calls to the inferior.  Values assigned to internal
       variables or put into the value history are taken off this
       list.  */
    struct value *next;
    /* If an lval is forced to repeat, a new value is created with
       these fields set.  The new value is not an lval.  */
    short repeated;
    short repetitions;
    /* Register number if the value is from a register.  Is not kept
       if you take a field of a structure that is stored in a
       register.  Shouldn't it be?  */
    short regno;
    /* Actual contents of the value.  For use of this value; setting
       it uses the stuff above.  */
    long contents[1];
  };

typedef struct value *value;

#define VALUE_TYPE(val) (val)->type
#define VALUE_CONTENTS(val) ((char *) (val)->contents)
#define VALUE_LVAL(val) (val)->lval
#define VALUE_ADDRESS(val) (val)->location.address
#define VALUE_INTERNALVAR(val) (val)->location.internalvar
#define VALUE_FRAME_REGNUM(val) ((val)->location.regnum)
#define VALUE_FRAME(val) ((val)->frame_addr)
#define VALUE_OFFSET(val) (val)->offset
#define VALUE_BITSIZE(val) (val)->bitsize
#define VALUE_BITPOS(val) (val)->bitpos
#define VALUE_NEXT(val) (val)->next
#define VALUE_REPEATED(val) (val)->repeated
#define VALUE_REPETITIONS(val) (val)->repetitions
#define VALUE_REGNO(val) (val)->regno

/* If ARG is an array, convert it to a pointer.
   If ARG is an enum, convert it to an integer.

   References are dereferenced.  */

#define COERCE_ARRAY(arg)    \
{ if (TYPE_CODE ( VALUE_TYPE (arg)) == TYPE_CODE_REF)			\
    arg = value_ind (arg);						\
  if (VALUE_REPEATED (arg)						\
      || TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_ARRAY)		\
    arg = value_coerce_array (arg);					\
  if (TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_ENUM)			\
    arg = value_cast (builtin_type_unsigned_int, arg);			\
}

/* If ARG is an enum, convert it to an integer.  */

#define COERCE_ENUM(arg)    \
{ if (TYPE_CODE ( VALUE_TYPE (arg)) == TYPE_CODE_REF)			\
    arg = value_ind (arg);						\
  if (TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_ENUM)			\
    arg = value_cast (builtin_type_unsigned_int, arg);			\
}

/* Internal variables (variables for convenience of use of debugger)
   are recorded as a chain of these structures.  */

struct internalvar
{
  struct internalvar *next;
  char *name;
  value value;
};

LONGEST value_as_long ();
double value_as_double ();
LONGEST unpack_long ();
double unpack_double ();
long unpack_field_as_long ();
value value_from_long ();
value value_from_double ();
value value_at ();
value value_from_register ();
value value_of_variable ();
value value_of_register ();
value read_var_value ();
value locate_var_value ();
value allocate_value ();
value allocate_repeat_value ();
value value_string ();

value value_binop ();
value value_add ();
value value_sub ();
value value_coerce_array ();
value value_ind ();
value value_addr ();
value value_assign ();
value value_neg ();
value value_lognot ();
value value_struct_elt (), value_struct_elt_for_address ();
value value_field ();
value value_cast ();
value value_zero ();
value value_repeat ();
value value_subscript ();

value call_function ();
value value_being_returned ();
int using_struct_return ();

value evaluate_expression ();
value evaluate_type ();
value parse_and_eval ();
value parse_to_comma_and_eval ();

value access_value_history ();
value value_of_internalvar ();
struct internalvar *lookup_internalvar ();

int value_equal ();
int value_less ();
int value_zerop ();

/* C++ */
value value_of_this ();
value value_static_field ();
value value_x_binop ();
value value_x_unop ();
int binop_user_defined_p ();
int unop_user_defined_p ();

void read_register_bytes ();
void modify_field ();
void type_print ();
void type_print_1 ();

/* Possibilities for prettyprint parameters to routines which print
   things.  */
enum val_prettyprint {
  Val_no_prettyprint = 0,
  Val_prettyprint,
  /* Use the default setting which the user has specified.  */
  Val_pretty_default
  };

