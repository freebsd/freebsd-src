/* Definitions for values of C expressions, for GDB.
   Copyright 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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

#if !defined (VALUE_H)
#define VALUE_H 1

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
  lval_reg_frame_relative
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
	/* Pointer to internal variable.  */
	struct internalvar *internalvar;
	/* Number of register.  Only used with
	   lval_reg_frame_relative.  */
	int regnum;
      } location;
    /* Describes offset of a value within lval a structure in bytes.  */
    int offset;	
    /* Only used for bitfields; number of bits contained in them.  */
    int bitsize;
    /* Only used for bitfields; position of start of field.
       For BITS_BIG_ENDIAN=0 targets, it is the position of the LSB.
       For BITS_BIG_ENDIAN=1 targets, it is the position of the MSB. */
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
    /* If zero, contents of this value are in the contents field.
       If nonzero, contents are in inferior memory at address
       in the location.address field plus the offset field
       (and the lval field should be lval_memory).  */
    char lazy;
    /* If nonzero, this is the value of a variable which does not
       actually exist in the program.  */
    char optimized_out;
    /* Actual contents of the value.  For use of this value; setting
       it uses the stuff above.  Not valid if lazy is nonzero.
       Target byte-order.  We force it to be aligned properly for any
       possible value.  */
    union {
      long contents[1];
      double force_double_align;
#ifdef CC_HAS_LONG_LONG
      long long force_longlong_align;
#endif
    } aligner;

  };

typedef struct value *value;

#define VALUE_TYPE(val) (val)->type
#define VALUE_LAZY(val) (val)->lazy
/* VALUE_CONTENTS and VALUE_CONTENTS_RAW both return the address of
   the gdb buffer used to hold a copy of the contents of the lval.  
   VALUE_CONTENTS is used when the contents of the buffer are needed --
   it uses value_fetch_lazy() to load the buffer from the process being 
   debugged if it hasn't already been loaded.  VALUE_CONTENTS_RAW is 
   used when data is being stored into the buffer, or when it is 
   certain that the contents of the buffer are valid.  */
#define VALUE_CONTENTS_RAW(val) ((char *) (val)->aligner.contents)
#define VALUE_CONTENTS(val) ((void)(VALUE_LAZY(val) && value_fetch_lazy(val)),\
			     VALUE_CONTENTS_RAW(val))
extern int
value_fetch_lazy PARAMS ((value val));

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
#define VALUE_OPTIMIZED_OUT(val) ((val)->optimized_out)

/* Convert a REF to the object referenced. */

#define COERCE_REF(arg)    \
{ if (TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_REF)			\
    arg = value_at_lazy (TYPE_TARGET_TYPE (VALUE_TYPE (arg)),		\
			 unpack_long (VALUE_TYPE (arg),			\
				      VALUE_CONTENTS (arg)));}

/* If ARG is an array, convert it to a pointer.
   If ARG is an enum, convert it to an integer.
   If ARG is a function, convert it to a function pointer.

   References are dereferenced.  */

#define COERCE_ARRAY(arg)    \
{ COERCE_REF(arg);							\
  if (VALUE_REPEATED (arg)						\
      || TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_ARRAY)		\
    arg = value_coerce_array (arg);					\
  if (TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_FUNC)                   \
    arg = value_coerce_function (arg);                                  \
  if (TYPE_CODE (VALUE_TYPE (arg)) == TYPE_CODE_ENUM)			\
    arg = value_cast (builtin_type_unsigned_int, arg);			\
}

/* If ARG is an enum, convert it to an integer.  */

#define COERCE_ENUM(arg)    \
{ COERCE_REF (arg); \
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

/* Pointer to member function.  Depends on compiler implementation. */

#define METHOD_PTR_IS_VIRTUAL(ADDR)  ((ADDR) & 0x80000000)
#define METHOD_PTR_FROM_VOFFSET(OFFSET) (0x80000000 + (OFFSET))
#define METHOD_PTR_TO_VOFFSET(ADDR) (~0x80000000 & (ADDR))


#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"

#ifdef __STDC__
struct frame_info;
struct fn_field;
#endif

extern void
print_address_demangle PARAMS ((CORE_ADDR, FILE *, int));

extern LONGEST
value_as_long PARAMS ((value val));

extern double
value_as_double PARAMS ((value val));

extern CORE_ADDR
value_as_pointer PARAMS ((value val));

extern LONGEST
unpack_long PARAMS ((struct type *type, char *valaddr));

extern double
unpack_double PARAMS ((struct type *type, char *valaddr, int *invp));

extern CORE_ADDR unpack_pointer PARAMS ((struct type *type, char *valaddr));

extern LONGEST unpack_field_as_long PARAMS ((struct type *type, char *valaddr,
					     int fieldno));

extern value value_from_longest PARAMS ((struct type *type, LONGEST num));

extern value value_from_double PARAMS ((struct type *type, double num));

extern value value_at PARAMS ((struct type *type, CORE_ADDR addr));

extern value value_at_lazy PARAMS ((struct type *type, CORE_ADDR addr));

/* FIXME:  Assumes equivalence of "struct frame_info *" and "FRAME" */
extern value value_from_register PARAMS ((struct type *type, int regnum,
					  struct frame_info * frame));

extern value value_of_variable PARAMS ((struct symbol *var, struct block *b));

extern value value_of_register PARAMS ((int regnum));

extern int symbol_read_needs_frame PARAMS ((struct symbol *));

/* FIXME:  Assumes equivalence of "struct frame_info *" and "FRAME" */
extern value read_var_value PARAMS ((struct symbol *var,
				     struct frame_info *frame));

/* FIXME:  Assumes equivalence of "struct frame_info *" and "FRAME" */
extern value locate_var_value PARAMS ((struct symbol *var,
				       struct frame_info *frame));

extern value allocate_value PARAMS ((struct type *type));

extern value allocate_repeat_value PARAMS ((struct type *type, int count));

extern value value_mark PARAMS ((void));

extern void value_free_to_mark PARAMS ((value mark));

extern value value_string PARAMS ((char *ptr, int len));

extern value value_array PARAMS ((int lowbound, int highbound,
				  value *elemvec));

extern value value_concat PARAMS ((value arg1, value arg2));

extern value value_binop PARAMS ((value arg1, value arg2, enum exp_opcode op));

extern value value_add PARAMS ((value arg1, value arg2));

extern value value_sub PARAMS ((value arg1, value arg2));

extern value value_coerce_array PARAMS ((value arg1));

extern value value_coerce_function PARAMS ((value arg1));

extern value value_ind PARAMS ((value arg1));

extern value value_addr PARAMS ((value arg1));

extern value value_assign PARAMS ((value toval, value fromval));

extern value value_neg PARAMS ((value arg1));

extern value value_complement PARAMS ((value arg1));

extern value value_struct_elt PARAMS ((value *argp, value *args, char *name,
				       int *static_memfuncp, char *err));

extern value value_struct_elt_for_reference PARAMS ((struct type *domain,
						     int offset,
						     struct type *curtype,
						     char *name,
						     struct type *intype));

extern value value_field PARAMS ((value arg1, int fieldno));

extern value value_primitive_field PARAMS ((value arg1, int offset,
					    int fieldno,
					    struct type *arg_type));

extern value value_cast PARAMS ((struct type *type, value arg2));

extern value value_zero PARAMS ((struct type *type, enum lval_type lv));

extern value value_repeat PARAMS ((value arg1, int count));

extern value value_subscript PARAMS ((value array, value idx));

extern value value_from_vtable_info PARAMS ((value arg, struct type *type));

extern value value_being_returned PARAMS ((struct type *valtype, 
					   char retbuf[REGISTER_BYTES],
					   int struct_return));

extern int
using_struct_return PARAMS ((value function, CORE_ADDR funcaddr,
			     struct type *value_type, int gcc_p));

extern void
set_return_value PARAMS ((value val));

extern value
evaluate_expression PARAMS ((struct expression *exp));

extern value
evaluate_type PARAMS ((struct expression *exp));

extern value
parse_and_eval PARAMS ((char *exp));

extern value
parse_to_comma_and_eval PARAMS ((char **expp));

extern struct type *
parse_and_eval_type PARAMS ((char *p, int length));

extern CORE_ADDR
parse_and_eval_address PARAMS ((char *exp));

extern CORE_ADDR
parse_and_eval_address_1 PARAMS ((char **expptr));

extern value
access_value_history PARAMS ((int num));

extern value
value_of_internalvar PARAMS ((struct internalvar *var));

extern void
set_internalvar PARAMS ((struct internalvar *var, value val));

extern void
set_internalvar_component PARAMS ((struct internalvar *var, int offset,
				   int bitpos, int bitsize,
				   value newvalue));

extern struct internalvar *
lookup_internalvar PARAMS ((char *name));

extern int
value_equal PARAMS ((value arg1, value arg2));

extern int
value_less PARAMS ((value arg1, value arg2));

extern int
value_logical_not PARAMS ((value arg1));

/* C++ */

extern value
value_of_this PARAMS ((int complain));

extern value
value_x_binop PARAMS ((value arg1, value arg2, enum exp_opcode op,
		       enum exp_opcode otherop));

extern value
value_x_unop PARAMS ((value arg1, enum exp_opcode op));

extern value
value_fn_field PARAMS ((value *arg1p, struct fn_field *f, int j,
			struct type* type, int offset));

extern value
value_virtual_fn_field PARAMS ((value *arg1p, struct fn_field *f, int j,
				struct type *type, int offset));

extern int
binop_user_defined_p PARAMS ((enum exp_opcode op, value arg1, value arg2));

extern int
unop_user_defined_p PARAMS ((enum exp_opcode op, value arg1));

extern int
destructor_name_p PARAMS ((const char *name, const struct type *type));

#define value_free(val) free ((PTR)val)

extern void
free_all_values PARAMS ((void));

extern void
release_value PARAMS ((value val));

extern int
record_latest_value PARAMS ((value val));

extern void
registers_changed PARAMS ((void));

extern void
read_register_bytes PARAMS ((int regbyte, char *myaddr, int len));

extern void
write_register_bytes PARAMS ((int regbyte, char *myaddr, int len));

extern void
read_register_gen PARAMS ((int regno, char *myaddr));

extern CORE_ADDR
read_register PARAMS ((int regno));

extern void
write_register PARAMS ((int regno, LONGEST val));

extern void
supply_register PARAMS ((int regno, char *val));

/* FIXME:  Assumes equivalence of "struct frame_info *" and "FRAME" */
extern void
get_saved_register PARAMS ((char *raw_buffer, int *optimized,
			    CORE_ADDR *addrp, struct frame_info *frame,
			    int regnum, enum lval_type *lval));

extern void
modify_field PARAMS ((char *addr, LONGEST fieldval, int bitpos, int bitsize));

extern void
type_print PARAMS ((struct type *type, char *varstring, FILE *stream,
		    int show));

extern char *
baseclass_addr PARAMS ((struct type *type, int index, char *valaddr,
			value *valuep, int *errp));

extern void
print_longest PARAMS ((FILE *stream, int format, int use_local,
		       LONGEST value));

extern void
print_floating PARAMS ((char *valaddr, struct type *type, FILE *stream));

extern int
value_print PARAMS ((value val, FILE *stream, int format,
		     enum val_prettyprint pretty));

extern int
val_print PARAMS ((struct type *type, char *valaddr, CORE_ADDR address,
		   FILE *stream, int format, int deref_ref,
		   int recurse, enum val_prettyprint pretty));

extern int
val_print_string PARAMS ((CORE_ADDR addr, unsigned int len, FILE *stream));

/* FIXME:  Assumes equivalence of "struct frame_info *" and "FRAME" */
extern void
print_variable_value PARAMS ((struct symbol *var, struct frame_info *frame,
			      FILE *stream));

extern value
value_arg_coerce PARAMS ((value));

extern int
check_field PARAMS ((value, const char *));

extern void
c_typedef_print PARAMS ((struct type *type, struct symbol *new, FILE *stream));

extern char *
internalvar_name PARAMS ((struct internalvar *var));

extern void
clear_value_history PARAMS ((void));

extern void
clear_internalvars PARAMS ((void));

/* From values.c */

extern value
value_copy PARAMS ((value));

extern int
baseclass_offset PARAMS ((struct type *, int, value, int));

/* From valops.c */

extern value
call_function_by_hand PARAMS ((value, int, value *));

#endif	/* !defined (VALUE_H) */
