/* Low level packing and unpacking of values for GDB, the GNU Debugger.
   Copyright 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

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
#include <string.h>
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "gdbcore.h"
#include "frame.h"
#include "command.h"
#include "gdbcmd.h"
#include "target.h"
#include "demangle.h"

/* Local function prototypes. */

static value
value_headof PARAMS ((value, struct type *, struct type *));

static void
show_values PARAMS ((char *, int));

static void
show_convenience PARAMS ((char *, int));

/* The value-history records all the values printed
   by print commands during this session.  Each chunk
   records 60 consecutive values.  The first chunk on
   the chain records the most recent values.
   The total number of values is in value_history_count.  */

#define VALUE_HISTORY_CHUNK 60

struct value_history_chunk
{
  struct value_history_chunk *next;
  value values[VALUE_HISTORY_CHUNK];
};

/* Chain of chunks now in use.  */

static struct value_history_chunk *value_history_chain;

static int value_history_count;	/* Abs number of last entry stored */

/* List of all value objects currently allocated
   (except for those released by calls to release_value)
   This is so they can be freed after each command.  */

static value all_values;

/* Allocate a  value  that has the correct length for type TYPE.  */

value
allocate_value (type)
     struct type *type;
{
  register value val;

  check_stub_type (type);

  val = (value) xmalloc (sizeof (struct value) + TYPE_LENGTH (type));
  VALUE_NEXT (val) = all_values;
  all_values = val;
  VALUE_TYPE (val) = type;
  VALUE_LVAL (val) = not_lval;
  VALUE_ADDRESS (val) = 0;
  VALUE_FRAME (val) = 0;
  VALUE_OFFSET (val) = 0;
  VALUE_BITPOS (val) = 0;
  VALUE_BITSIZE (val) = 0;
  VALUE_REPEATED (val) = 0;
  VALUE_REPETITIONS (val) = 0;
  VALUE_REGNO (val) = -1;
  VALUE_LAZY (val) = 0;
  VALUE_OPTIMIZED_OUT (val) = 0;
  return val;
}

/* Allocate a  value  that has the correct length
   for COUNT repetitions type TYPE.  */

value
allocate_repeat_value (type, count)
     struct type *type;
     int count;
{
  register value val;

  val = (value) xmalloc (sizeof (struct value) + TYPE_LENGTH (type) * count);
  VALUE_NEXT (val) = all_values;
  all_values = val;
  VALUE_TYPE (val) = type;
  VALUE_LVAL (val) = not_lval;
  VALUE_ADDRESS (val) = 0;
  VALUE_FRAME (val) = 0;
  VALUE_OFFSET (val) = 0;
  VALUE_BITPOS (val) = 0;
  VALUE_BITSIZE (val) = 0;
  VALUE_REPEATED (val) = 1;
  VALUE_REPETITIONS (val) = count;
  VALUE_REGNO (val) = -1;
  VALUE_LAZY (val) = 0;
  VALUE_OPTIMIZED_OUT (val) = 0;
  return val;
}

/* Return a mark in the value chain.  All values allocated after the
   mark is obtained (except for those released) are subject to being freed
   if a subsequent value_free_to_mark is passed the mark.  */
value
value_mark ()
{
  return all_values;
}

/* Free all values allocated since MARK was obtained by value_mark
   (except for those released).  */
void
value_free_to_mark (mark)
     value mark;
{
  value val, next;

  for (val = all_values; val && val != mark; val = next)
    {
      next = VALUE_NEXT (val);
      value_free (val);
    }
  all_values = val;
}

/* Free all the values that have been allocated (except for those released).
   Called after each command, successful or not.  */

void
free_all_values ()
{
  register value val, next;

  for (val = all_values; val; val = next)
    {
      next = VALUE_NEXT (val);
      value_free (val);
    }

  all_values = 0;
}

/* Remove VAL from the chain all_values
   so it will not be freed automatically.  */

void
release_value (val)
     register value val;
{
  register value v;

  if (all_values == val)
    {
      all_values = val->next;
      return;
    }

  for (v = all_values; v; v = v->next)
    {
      if (v->next == val)
	{
	  v->next = val->next;
	  break;
	}
    }
}

/* Return a copy of the value ARG.
   It contains the same contents, for same memory address,
   but it's a different block of storage.  */

value
value_copy (arg)
     value arg;
{
  register value val;
  register struct type *type = VALUE_TYPE (arg);
  if (VALUE_REPEATED (arg))
    val = allocate_repeat_value (type, VALUE_REPETITIONS (arg));
  else
    val = allocate_value (type);
  VALUE_LVAL (val) = VALUE_LVAL (arg);
  VALUE_ADDRESS (val) = VALUE_ADDRESS (arg);
  VALUE_OFFSET (val) = VALUE_OFFSET (arg);
  VALUE_BITPOS (val) = VALUE_BITPOS (arg);
  VALUE_BITSIZE (val) = VALUE_BITSIZE (arg);
  VALUE_REGNO (val) = VALUE_REGNO (arg);
  VALUE_LAZY (val) = VALUE_LAZY (arg);
  if (!VALUE_LAZY (val))
    {
      memcpy (VALUE_CONTENTS_RAW (val), VALUE_CONTENTS_RAW (arg),
	      TYPE_LENGTH (VALUE_TYPE (arg))
	      * (VALUE_REPEATED (arg) ? VALUE_REPETITIONS (arg) : 1));
    }
  return val;
}

/* Access to the value history.  */

/* Record a new value in the value history.
   Returns the absolute history index of the entry.
   Result of -1 indicates the value was not saved; otherwise it is the
   value history index of this new item.  */

int
record_latest_value (val)
     value val;
{
  int i;

  /* Check error now if about to store an invalid float.  We return -1
     to the caller, but allow them to continue, e.g. to print it as "Nan". */
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FLT)
    {
      unpack_double (VALUE_TYPE (val), VALUE_CONTENTS (val), &i);
      if (i) return -1;		/* Indicate value not saved in history */
    }

  /* Here we treat value_history_count as origin-zero
     and applying to the value being stored now.  */

  i = value_history_count % VALUE_HISTORY_CHUNK;
  if (i == 0)
    {
      register struct value_history_chunk *new
	= (struct value_history_chunk *)
	  xmalloc (sizeof (struct value_history_chunk));
      memset (new->values, 0, sizeof new->values);
      new->next = value_history_chain;
      value_history_chain = new;
    }

  value_history_chain->values[i] = val;

  /* We don't want this value to have anything to do with the inferior anymore.
     In particular, "set $1 = 50" should not affect the variable from which
     the value was taken, and fast watchpoints should be able to assume that
     a value on the value history never changes.  */
  if (VALUE_LAZY (val))
    value_fetch_lazy (val);
  VALUE_LVAL (val) = not_lval;
  release_value (val);

  /* Now we regard value_history_count as origin-one
     and applying to the value just stored.  */

  return ++value_history_count;
}

/* Return a copy of the value in the history with sequence number NUM.  */

value
access_value_history (num)
     int num;
{
  register struct value_history_chunk *chunk;
  register int i;
  register int absnum = num;

  if (absnum <= 0)
    absnum += value_history_count;

  if (absnum <= 0)
    {
      if (num == 0)
	error ("The history is empty.");
      else if (num == 1)
	error ("There is only one value in the history.");
      else
	error ("History does not go back to $$%d.", -num);
    }
  if (absnum > value_history_count)
    error ("History has not yet reached $%d.", absnum);

  absnum--;

  /* Now absnum is always absolute and origin zero.  */

  chunk = value_history_chain;
  for (i = (value_history_count - 1) / VALUE_HISTORY_CHUNK - absnum / VALUE_HISTORY_CHUNK;
       i > 0; i--)
    chunk = chunk->next;

  return value_copy (chunk->values[absnum % VALUE_HISTORY_CHUNK]);
}

/* Clear the value history entirely.
   Must be done when new symbol tables are loaded,
   because the type pointers become invalid.  */

void
clear_value_history ()
{
  register struct value_history_chunk *next;
  register int i;
  register value val;

  while (value_history_chain)
    {
      for (i = 0; i < VALUE_HISTORY_CHUNK; i++)
	if ((val = value_history_chain->values[i]) != NULL)
	  free ((PTR)val);
      next = value_history_chain->next;
      free ((PTR)value_history_chain);
      value_history_chain = next;
    }
  value_history_count = 0;
}

static void
show_values (num_exp, from_tty)
     char *num_exp;
     int from_tty;
{
  register int i;
  register value val;
  static int num = 1;

  if (num_exp)
    {
	/* "info history +" should print from the stored position.
	   "info history <exp>" should print around value number <exp>.  */
      if (num_exp[0] != '+' || num_exp[1] != '\0')
	num = parse_and_eval_address (num_exp) - 5;
    }
  else
    {
      /* "info history" means print the last 10 values.  */
      num = value_history_count - 9;
    }

  if (num <= 0)
    num = 1;

  for (i = num; i < num + 10 && i <= value_history_count; i++)
    {
      val = access_value_history (i);
      printf_filtered ("$%d = ", i);
      value_print (val, stdout, 0, Val_pretty_default);
      printf_filtered ("\n");
    }

  /* The next "info history +" should start after what we just printed.  */
  num += 10;

  /* Hitting just return after this command should do the same thing as
     "info history +".  If num_exp is null, this is unnecessary, since
     "info history +" is not useful after "info history".  */
  if (from_tty && num_exp)
    {
      num_exp[0] = '+';
      num_exp[1] = '\0';
    }
}

/* Internal variables.  These are variables within the debugger
   that hold values assigned by debugger commands.
   The user refers to them with a '$' prefix
   that does not appear in the variable names stored internally.  */

static struct internalvar *internalvars;

/* Look up an internal variable with name NAME.  NAME should not
   normally include a dollar sign.

   If the specified internal variable does not exist,
   one is created, with a void value.  */

struct internalvar *
lookup_internalvar (name)
     char *name;
{
  register struct internalvar *var;

  for (var = internalvars; var; var = var->next)
    if (STREQ (var->name, name))
      return var;

  var = (struct internalvar *) xmalloc (sizeof (struct internalvar));
  var->name = concat (name, NULL);
  var->value = allocate_value (builtin_type_void);
  release_value (var->value);
  var->next = internalvars;
  internalvars = var;
  return var;
}

value
value_of_internalvar (var)
     struct internalvar *var;
{
  register value val;

#ifdef IS_TRAPPED_INTERNALVAR
  if (IS_TRAPPED_INTERNALVAR (var->name))
    return VALUE_OF_TRAPPED_INTERNALVAR (var);
#endif 

  val = value_copy (var->value);
  if (VALUE_LAZY (val))
    value_fetch_lazy (val);
  VALUE_LVAL (val) = lval_internalvar;
  VALUE_INTERNALVAR (val) = var;
  return val;
}

void
set_internalvar_component (var, offset, bitpos, bitsize, newval)
     struct internalvar *var;
     int offset, bitpos, bitsize;
     value newval;
{
  register char *addr = VALUE_CONTENTS (var->value) + offset;

#ifdef IS_TRAPPED_INTERNALVAR
  if (IS_TRAPPED_INTERNALVAR (var->name))
    SET_TRAPPED_INTERNALVAR (var, newval, bitpos, bitsize, offset);
#endif

  if (bitsize)
    modify_field (addr, value_as_long (newval),
		  bitpos, bitsize);
  else
    memcpy (addr, VALUE_CONTENTS (newval), TYPE_LENGTH (VALUE_TYPE (newval)));
}

void
set_internalvar (var, val)
     struct internalvar *var;
     value val;
{
#ifdef IS_TRAPPED_INTERNALVAR
  if (IS_TRAPPED_INTERNALVAR (var->name))
    SET_TRAPPED_INTERNALVAR (var, val, 0, 0, 0);
#endif

  free ((PTR)var->value);
  var->value = value_copy (val);
  /* Force the value to be fetched from the target now, to avoid problems
     later when this internalvar is referenced and the target is gone or
     has changed.  */
  if (VALUE_LAZY (var->value))
    value_fetch_lazy (var->value);
  release_value (var->value);
}

char *
internalvar_name (var)
     struct internalvar *var;
{
  return var->name;
}

/* Free all internalvars.  Done when new symtabs are loaded,
   because that makes the values invalid.  */

void
clear_internalvars ()
{
  register struct internalvar *var;

  while (internalvars)
    {
      var = internalvars;
      internalvars = var->next;
      free ((PTR)var->name);
      free ((PTR)var->value);
      free ((PTR)var);
    }
}

static void
show_convenience (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  register struct internalvar *var;
  int varseen = 0;

  for (var = internalvars; var; var = var->next)
    {
#ifdef IS_TRAPPED_INTERNALVAR
      if (IS_TRAPPED_INTERNALVAR (var->name))
	continue;
#endif
      if (!varseen)
	{
	  varseen = 1;
	}
      printf_filtered ("$%s = ", var->name);
      value_print (var->value, stdout, 0, Val_pretty_default);
      printf_filtered ("\n");
    }
  if (!varseen)
    printf ("No debugger convenience variables now defined.\n\
Convenience variables have names starting with \"$\";\n\
use \"set\" as in \"set $foo = 5\" to define them.\n");
}

/* Extract a value as a C number (either long or double).
   Knows how to convert fixed values to double, or
   floating values to long.
   Does not deallocate the value.  */

LONGEST
value_as_long (val)
     register value val;
{
  /* This coerces arrays and functions, which is necessary (e.g.
     in disassemble_command).  It also dereferences references, which
     I suspect is the most logical thing to do.  */
  if (TYPE_CODE (VALUE_TYPE (val)) != TYPE_CODE_ENUM)
    COERCE_ARRAY (val);
  return unpack_long (VALUE_TYPE (val), VALUE_CONTENTS (val));
}

double
value_as_double (val)
     register value val;
{
  double foo;
  int inv;
  
  foo = unpack_double (VALUE_TYPE (val), VALUE_CONTENTS (val), &inv);
  if (inv)
    error ("Invalid floating value found in program.");
  return foo;
}
/* Extract a value as a C pointer.
   Does not deallocate the value.  */
CORE_ADDR
value_as_pointer (val)
     value val;
{
  /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
     whether we want this to be true eventually.  */
#if 0
  /* ADDR_BITS_REMOVE is wrong if we are being called for a
     non-address (e.g. argument to "signal", "info break", etc.), or
     for pointers to char, in which the low bits *are* significant.  */
  return ADDR_BITS_REMOVE(value_as_long (val));
#else
  return value_as_long (val);
#endif
}

/* Unpack raw data (copied from debugee, target byte order) at VALADDR
   as a long, or as a double, assuming the raw data is described
   by type TYPE.  Knows how to convert different sizes of values
   and can convert between fixed and floating point.  We don't assume
   any alignment for the raw data.  Return value is in host byte order.

   If you want functions and arrays to be coerced to pointers, and
   references to be dereferenced, call value_as_long() instead.

   C++: It is assumed that the front-end has taken care of
   all matters concerning pointers to members.  A pointer
   to member which reaches here is considered to be equivalent
   to an INT (or some size).  After all, it is only an offset.  */

/* FIXME:  This should be rewritten as a switch statement for speed and
   ease of comprehension.  */

LONGEST
unpack_long (type, valaddr)
     struct type *type;
     char *valaddr;
{
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);
  register int nosign = TYPE_UNSIGNED (type);

  if (code == TYPE_CODE_ENUM || code == TYPE_CODE_BOOL)
    code = TYPE_CODE_INT;
  if (code == TYPE_CODE_FLT)
    {
      if (len == sizeof (float))
	{
	  float retval;
	  memcpy (&retval, valaddr, sizeof (retval));
	  SWAP_TARGET_AND_HOST (&retval, sizeof (retval));
	  return retval;
	}

      if (len == sizeof (double))
	{
	  double retval;
	  memcpy (&retval, valaddr, sizeof (retval));
	  SWAP_TARGET_AND_HOST (&retval, sizeof (retval));
	  return retval;
	}
      else
	{
	  error ("Unexpected type of floating point number.");
	}
    }
  else if ((code == TYPE_CODE_INT || code == TYPE_CODE_CHAR) && nosign)
    {
      return extract_unsigned_integer (valaddr, len);
    }
  else if (code == TYPE_CODE_INT || code == TYPE_CODE_CHAR)
    {
      return extract_signed_integer (valaddr, len);
    }
  /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
     whether we want this to be true eventually.  */
  else if (code == TYPE_CODE_PTR || code == TYPE_CODE_REF)
    {
      return extract_address (valaddr, len);
    }
  else if (code == TYPE_CODE_MEMBER)
    error ("not implemented: member types in unpack_long");

  error ("Value not integer or pointer.");
  return 0; 	/* For lint -- never reached */
}

/* Return a double value from the specified type and address.
   INVP points to an int which is set to 0 for valid value,
   1 for invalid value (bad float format).  In either case,
   the returned double is OK to use.  Argument is in target
   format, result is in host format.  */

double
unpack_double (type, valaddr, invp)
     struct type *type;
     char *valaddr;
     int *invp;
{
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);
  register int nosign = TYPE_UNSIGNED (type);

  *invp = 0;			/* Assume valid.   */
  if (code == TYPE_CODE_FLT)
    {
      if (INVALID_FLOAT (valaddr, len))
	{
	  *invp = 1;
	  return 1.234567891011121314;
	}

      if (len == sizeof (float))
	{
	  float retval;
	  memcpy (&retval, valaddr, sizeof (retval));
	  SWAP_TARGET_AND_HOST (&retval, sizeof (retval));
	  return retval;
	}

      if (len == sizeof (double))
	{
	  double retval;
	  memcpy (&retval, valaddr, sizeof (retval));
	  SWAP_TARGET_AND_HOST (&retval, sizeof (retval));
	  return retval;
	}
      else
	{
	  error ("Unexpected type of floating point number.");
	  return 0; /* Placate lint.  */
	}
    }
  else if (nosign) {
   /* Unsigned -- be sure we compensate for signed LONGEST.  */
   return (unsigned LONGEST) unpack_long (type, valaddr);
  } else {
    /* Signed -- we are OK with unpack_long.  */
    return unpack_long (type, valaddr);
  }
}

/* Unpack raw data (copied from debugee, target byte order) at VALADDR
   as a CORE_ADDR, assuming the raw data is described by type TYPE.
   We don't assume any alignment for the raw data.  Return value is in
   host byte order.

   If you want functions and arrays to be coerced to pointers, and
   references to be dereferenced, call value_as_pointer() instead.

   C++: It is assumed that the front-end has taken care of
   all matters concerning pointers to members.  A pointer
   to member which reaches here is considered to be equivalent
   to an INT (or some size).  After all, it is only an offset.  */

CORE_ADDR
unpack_pointer (type, valaddr)
     struct type *type;
     char *valaddr;
{
  /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
     whether we want this to be true eventually.  */
  return unpack_long (type, valaddr);
}

/* Given a value ARG1 (offset by OFFSET bytes)
   of a struct or union type ARG_TYPE,
   extract and return the value of one of its fields.
   FIELDNO says which field.

   For C++, must also be able to return values from static fields */

value
value_primitive_field (arg1, offset, fieldno, arg_type)
     register value arg1;
     int offset;
     register int fieldno;
     register struct type *arg_type;
{
  register value v;
  register struct type *type;

  check_stub_type (arg_type);
  type = TYPE_FIELD_TYPE (arg_type, fieldno);

  /* Handle packed fields */

  offset += TYPE_FIELD_BITPOS (arg_type, fieldno) / 8;
  if (TYPE_FIELD_BITSIZE (arg_type, fieldno))
    {
      v = value_from_longest (type,
			   unpack_field_as_long (arg_type,
						 VALUE_CONTENTS (arg1),
						 fieldno));
      VALUE_BITPOS (v) = TYPE_FIELD_BITPOS (arg_type, fieldno) % 8;
      VALUE_BITSIZE (v) = TYPE_FIELD_BITSIZE (arg_type, fieldno);
    }
  else
    {
      v = allocate_value (type);
      if (VALUE_LAZY (arg1))
	VALUE_LAZY (v) = 1;
      else
	memcpy (VALUE_CONTENTS_RAW (v), VALUE_CONTENTS_RAW (arg1) + offset,
		TYPE_LENGTH (type));
    }
  VALUE_LVAL (v) = VALUE_LVAL (arg1);
  if (VALUE_LVAL (arg1) == lval_internalvar)
    VALUE_LVAL (v) = lval_internalvar_component;
  VALUE_ADDRESS (v) = VALUE_ADDRESS (arg1);
  VALUE_OFFSET (v) = offset + VALUE_OFFSET (arg1);
  return v;
}

/* Given a value ARG1 of a struct or union type,
   extract and return the value of one of its fields.
   FIELDNO says which field.

   For C++, must also be able to return values from static fields */

value
value_field (arg1, fieldno)
     register value arg1;
     register int fieldno;
{
  return value_primitive_field (arg1, 0, fieldno, VALUE_TYPE (arg1));
}

/* Return a non-virtual function as a value.
   F is the list of member functions which contains the desired method.
   J is an index into F which provides the desired method. */

value
value_fn_field (arg1p, f, j, type, offset)
     value *arg1p;
     struct fn_field *f;
     int j;
     struct type *type;
     int offset;
{
  register value v;
  register struct type *ftype = TYPE_FN_FIELD_TYPE (f, j);
  struct symbol *sym;

  sym = lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, j),
		       0, VAR_NAMESPACE, 0, NULL);
  if (! sym) error ("Internal error: could not find physical method named %s",
		    TYPE_FN_FIELD_PHYSNAME (f, j));
  
  v = allocate_value (ftype);
  VALUE_ADDRESS (v) = BLOCK_START (SYMBOL_BLOCK_VALUE (sym));
  VALUE_TYPE (v) = ftype;

  if (arg1p)
   {
    if (type != VALUE_TYPE (*arg1p))
      *arg1p = value_ind (value_cast (lookup_pointer_type (type),
				      value_addr (*arg1p)));

    /* Move the `this' pointer according to the offset. 
    VALUE_OFFSET (*arg1p) += offset;
    */
    }

  return v;
}

/* Return a virtual function as a value.
   ARG1 is the object which provides the virtual function
   table pointer.  *ARG1P is side-effected in calling this function.
   F is the list of member functions which contains the desired virtual
   function.
   J is an index into F which provides the desired virtual function.

   TYPE is the type in which F is located.  */
value
value_virtual_fn_field (arg1p, f, j, type, offset)
     value *arg1p;
     struct fn_field *f;
     int j;
     struct type *type;
     int offset;
{
  value arg1 = *arg1p;
  /* First, get the virtual function table pointer.  That comes
     with a strange type, so cast it to type `pointer to long' (which
     should serve just fine as a function type).  Then, index into
     the table, and convert final value to appropriate function type.  */
  value entry, vfn, vtbl;
  value vi = value_from_longest (builtin_type_int, 
			      (LONGEST) TYPE_FN_FIELD_VOFFSET (f, j));
  struct type *fcontext = TYPE_FN_FIELD_FCONTEXT (f, j);
  struct type *context;
  if (fcontext == NULL)
   /* We don't have an fcontext (e.g. the program was compiled with
      g++ version 1).  Try to get the vtbl from the TYPE_VPTR_BASETYPE.
      This won't work right for multiple inheritance, but at least we
      should do as well as GDB 3.x did.  */
    fcontext = TYPE_VPTR_BASETYPE (type);
  context = lookup_pointer_type (fcontext);
  /* Now context is a pointer to the basetype containing the vtbl.  */
  if (TYPE_TARGET_TYPE (context) != VALUE_TYPE (arg1))
    arg1 = value_ind (value_cast (context, value_addr (arg1)));

  context = VALUE_TYPE (arg1);
  /* Now context is the basetype containing the vtbl.  */

  /* This type may have been defined before its virtual function table
     was.  If so, fill in the virtual function table entry for the
     type now.  */
  if (TYPE_VPTR_FIELDNO (context) < 0)
    fill_in_vptr_fieldno (context);

  /* The virtual function table is now an array of structures
     which have the form { int16 offset, delta; void *pfn; }.  */
  vtbl = value_ind (value_primitive_field (arg1, 0, 
					   TYPE_VPTR_FIELDNO (context),
					   TYPE_VPTR_BASETYPE (context)));

  /* Index into the virtual function table.  This is hard-coded because
     looking up a field is not cheap, and it may be important to save
     time, e.g. if the user has set a conditional breakpoint calling
     a virtual function.  */
  entry = value_subscript (vtbl, vi);

  /* Move the `this' pointer according to the virtual function table. */ 
  VALUE_OFFSET (arg1) += value_as_long (value_field (entry, 0))/* + offset*/;

  if (! VALUE_LAZY (arg1))
    {
      VALUE_LAZY (arg1) = 1;
      value_fetch_lazy (arg1);
    }

  vfn = value_field (entry, 2);
  /* Reinstantiate the function pointer with the correct type.  */
  VALUE_TYPE (vfn) = lookup_pointer_type (TYPE_FN_FIELD_TYPE (f, j));

  *arg1p = arg1;
  return vfn;
}

/* ARG is a pointer to an object we know to be at least
   a DTYPE.  BTYPE is the most derived basetype that has
   already been searched (and need not be searched again).
   After looking at the vtables between BTYPE and DTYPE,
   return the most derived type we find.  The caller must
   be satisfied when the return value == DTYPE.

   FIXME-tiemann: should work with dossier entries as well.  */

static value
value_headof (in_arg, btype, dtype)
     value in_arg;
     struct type *btype, *dtype;
{
  /* First collect the vtables we must look at for this object.  */
  /* FIXME-tiemann: right now, just look at top-most vtable.  */
  value arg, vtbl, entry, best_entry = 0;
  int i, nelems;
  int offset, best_offset = 0;
  struct symbol *sym;
  CORE_ADDR pc_for_sym;
  char *demangled_name;
  struct minimal_symbol *msymbol;

  btype = TYPE_VPTR_BASETYPE (dtype);
  check_stub_type (btype);
  arg = in_arg;
  if (btype != dtype)
    arg = value_cast (lookup_pointer_type (btype), arg);
  vtbl = value_ind (value_field (value_ind (arg), TYPE_VPTR_FIELDNO (btype)));

  /* Check that VTBL looks like it points to a virtual function table.  */
  msymbol = lookup_minimal_symbol_by_pc (VALUE_ADDRESS (vtbl));
  if (msymbol == NULL
      || !VTBL_PREFIX_P (demangled_name = SYMBOL_NAME (msymbol)))
    {
      /* If we expected to find a vtable, but did not, let the user
	 know that we aren't happy, but don't throw an error.
	 FIXME: there has to be a better way to do this.  */
      struct type *error_type = (struct type *)xmalloc (sizeof (struct type));
      memcpy (error_type, VALUE_TYPE (in_arg), sizeof (struct type));
      TYPE_NAME (error_type) = savestring ("suspicious *", sizeof ("suspicious *"));
      VALUE_TYPE (in_arg) = error_type;
      return in_arg;
    }

  /* Now search through the virtual function table.  */
  entry = value_ind (vtbl);
  nelems = longest_to_int (value_as_long (value_field (entry, 2)));
  for (i = 1; i <= nelems; i++)
    {
      entry = value_subscript (vtbl, value_from_longest (builtin_type_int, 
						      (LONGEST) i));
      offset = longest_to_int (value_as_long (value_field (entry, 0)));
      /* If we use '<=' we can handle single inheritance
       * where all offsets are zero - just use the first entry found. */
      if (offset <= best_offset)
	{
	  best_offset = offset;
	  best_entry = entry;
	}
    }
  /* Move the pointer according to BEST_ENTRY's offset, and figure
     out what type we should return as the new pointer.  */
  if (best_entry == 0)
    {
      /* An alternative method (which should no longer be necessary).
       * But we leave it in for future use, when we will hopefully
       * have optimizes the vtable to use thunks instead of offsets. */
      /* Use the name of vtable itself to extract a base type. */
      demangled_name += 4;  /* Skip _vt$ prefix. */
    }
  else
    {
      pc_for_sym = value_as_pointer (value_field (best_entry, 2));
      sym = find_pc_function (pc_for_sym);
      demangled_name = cplus_demangle (SYMBOL_NAME (sym), DMGL_ANSI);
      *(strchr (demangled_name, ':')) = '\0';
    }
  sym = lookup_symbol (demangled_name, 0, VAR_NAMESPACE, 0, 0);
  if (sym == NULL)
    error ("could not find type declaration for `%s'", demangled_name);
  if (best_entry)
    {
      free (demangled_name);
      arg = value_add (value_cast (builtin_type_int, arg),
		       value_field (best_entry, 0));
    }
  else arg = in_arg;
  VALUE_TYPE (arg) = lookup_pointer_type (SYMBOL_TYPE (sym));
  return arg;
}

/* ARG is a pointer object of type TYPE.  If TYPE has virtual
   function tables, probe ARG's tables (including the vtables
   of its baseclasses) to figure out the most derived type that ARG
   could actually be a pointer to.  */

value
value_from_vtable_info (arg, type)
     value arg;
     struct type *type;
{
  /* Take care of preliminaries.  */
  if (TYPE_VPTR_FIELDNO (type) < 0)
    fill_in_vptr_fieldno (type);
  if (TYPE_VPTR_FIELDNO (type) < 0 || VALUE_REPEATED (arg))
    return 0;

  return value_headof (arg, 0, type);
}

/* Return true if the INDEXth field of TYPE is a virtual baseclass
   pointer which is for the base class whose type is BASECLASS.  */

static int
vb_match (type, index, basetype)
     struct type *type;
     int index;
     struct type *basetype;
{
  struct type *fieldtype;
  char *name = TYPE_FIELD_NAME (type, index);
  char *field_class_name = NULL;

  if (*name != '_')
    return 0;
  /* gcc 2.4 uses _vb$.  */
  if (name[1] == 'v' && name[2] == 'b' && name[3] == CPLUS_MARKER)
    field_class_name = name + 4;
  /* gcc 2.5 will use __vb_.  */
  if (name[1] == '_' && name[2] == 'v' && name[3] == 'b' && name[4] == '_')
    field_class_name = name + 5;

  if (field_class_name == NULL)
    /* This field is not a virtual base class pointer.  */
    return 0;

  /* It's a virtual baseclass pointer, now we just need to find out whether
     it is for this baseclass.  */
  fieldtype = TYPE_FIELD_TYPE (type, index);
  if (fieldtype == NULL
      || TYPE_CODE (fieldtype) != TYPE_CODE_PTR)
    /* "Can't happen".  */
    return 0;

  /* What we check for is that either the types are equal (needed for
     nameless types) or have the same name.  This is ugly, and a more
     elegant solution should be devised (which would probably just push
     the ugliness into symbol reading unless we change the stabs format).  */
  if (TYPE_TARGET_TYPE (fieldtype) == basetype)
    return 1;

  if (TYPE_NAME (basetype) != NULL
      && TYPE_NAME (TYPE_TARGET_TYPE (fieldtype)) != NULL
      && STREQ (TYPE_NAME (basetype),
		TYPE_NAME (TYPE_TARGET_TYPE (fieldtype))))
    return 1;
  return 0;
}

/* Compute the offset of the baseclass which is
   the INDEXth baseclass of class TYPE, for a value ARG,
   wih extra offset of OFFSET.
   The result is the offste of the baseclass value relative
   to (the address of)(ARG) + OFFSET.

   -1 is returned on error. */

int
baseclass_offset (type, index, arg, offset)
     struct type *type;
     int index;
     value arg;
     int offset;
{
  struct type *basetype = TYPE_BASECLASS (type, index);

  if (BASETYPE_VIA_VIRTUAL (type, index))
    {
      /* Must hunt for the pointer to this virtual baseclass.  */
      register int i, len = TYPE_NFIELDS (type);
      register int n_baseclasses = TYPE_N_BASECLASSES (type);

      /* First look for the virtual baseclass pointer
	 in the fields.  */
      for (i = n_baseclasses; i < len; i++)
	{
	  if (vb_match (type, i, basetype))
	    {
	      CORE_ADDR addr
		= unpack_pointer (TYPE_FIELD_TYPE (type, i),
				  VALUE_CONTENTS (arg) + VALUE_OFFSET (arg)
				  + offset
				  + (TYPE_FIELD_BITPOS (type, i) / 8));

	      if (VALUE_LVAL (arg) != lval_memory)
		  return -1;

	      return addr -
		  (LONGEST) (VALUE_ADDRESS (arg) + VALUE_OFFSET (arg) + offset);
	    }
	}
      /* Not in the fields, so try looking through the baseclasses.  */
      for (i = index+1; i < n_baseclasses; i++)
	{
	  int boffset =
	      baseclass_offset (type, i, arg, offset);
	  if (boffset)
	    return boffset;
	}
      /* Not found.  */
      return -1;
    }

  /* Baseclass is easily computed.  */
  return TYPE_BASECLASS_BITPOS (type, index) / 8;
}

/* Compute the address of the baseclass which is
   the INDEXth baseclass of class TYPE.  The TYPE base
   of the object is at VALADDR.

   If ERRP is non-NULL, set *ERRP to be the errno code of any error,
   or 0 if no error.  In that case the return value is not the address
   of the baseclasss, but the address which could not be read
   successfully.  */

/* FIXME Fix remaining uses of baseclass_addr to use baseclass_offset */

char *
baseclass_addr (type, index, valaddr, valuep, errp)
     struct type *type;
     int index;
     char *valaddr;
     value *valuep;
     int *errp;
{
  struct type *basetype = TYPE_BASECLASS (type, index);

  if (errp)
    *errp = 0;

  if (BASETYPE_VIA_VIRTUAL (type, index))
    {
      /* Must hunt for the pointer to this virtual baseclass.  */
      register int i, len = TYPE_NFIELDS (type);
      register int n_baseclasses = TYPE_N_BASECLASSES (type);

      /* First look for the virtual baseclass pointer
	 in the fields.  */
      for (i = n_baseclasses; i < len; i++)
	{
	  if (vb_match (type, i, basetype))
	    {
	      value val = allocate_value (basetype);
	      CORE_ADDR addr;
	      int status;

	      addr
		= unpack_pointer (TYPE_FIELD_TYPE (type, i),
				  valaddr + (TYPE_FIELD_BITPOS (type, i) / 8));

	      status = target_read_memory (addr,
					   VALUE_CONTENTS_RAW (val),
					   TYPE_LENGTH (basetype));
	      VALUE_LVAL (val) = lval_memory;
	      VALUE_ADDRESS (val) = addr;

	      if (status != 0)
		{
		  if (valuep)
		    *valuep = NULL;
		  release_value (val);
		  value_free (val);
		  if (errp)
		    *errp = status;
		  return (char *)addr;
		}
	      else
		{
		  if (valuep)
		    *valuep = val;
		  return (char *) VALUE_CONTENTS (val);
		}
	    }
	}
      /* Not in the fields, so try looking through the baseclasses.  */
      for (i = index+1; i < n_baseclasses; i++)
	{
	  char *baddr;

	  baddr = baseclass_addr (type, i, valaddr, valuep, errp);
	  if (baddr)
	    return baddr;
	}
      /* Not found.  */
      if (valuep)
	*valuep = 0;
      return 0;
    }

  /* Baseclass is easily computed.  */
  if (valuep)
    *valuep = 0;
  return valaddr + TYPE_BASECLASS_BITPOS (type, index) / 8;
}

/* Unpack a field FIELDNO of the specified TYPE, from the anonymous object at
   VALADDR.

   Extracting bits depends on endianness of the machine.  Compute the
   number of least significant bits to discard.  For big endian machines,
   we compute the total number of bits in the anonymous object, subtract
   off the bit count from the MSB of the object to the MSB of the
   bitfield, then the size of the bitfield, which leaves the LSB discard
   count.  For little endian machines, the discard count is simply the
   number of bits from the LSB of the anonymous object to the LSB of the
   bitfield.

   If the field is signed, we also do sign extension. */

LONGEST
unpack_field_as_long (type, valaddr, fieldno)
     struct type *type;
     char *valaddr;
     int fieldno;
{
  unsigned LONGEST val;
  unsigned LONGEST valmask;
  int bitpos = TYPE_FIELD_BITPOS (type, fieldno);
  int bitsize = TYPE_FIELD_BITSIZE (type, fieldno);
  int lsbcount;

  val = extract_unsigned_integer (valaddr + bitpos / 8, sizeof (val));

  /* Extract bits.  See comment above. */

#if BITS_BIG_ENDIAN
  lsbcount = (sizeof val * 8 - bitpos % 8 - bitsize);
#else
  lsbcount = (bitpos % 8);
#endif
  val >>= lsbcount;

  /* If the field does not entirely fill a LONGEST, then zero the sign bits.
     If the field is signed, and is negative, then sign extend. */

  if ((bitsize > 0) && (bitsize < 8 * sizeof (val)))
    {
      valmask = (((unsigned LONGEST) 1) << bitsize) - 1;
      val &= valmask;
      if (!TYPE_UNSIGNED (TYPE_FIELD_TYPE (type, fieldno)))
	{
	  if (val & (valmask ^ (valmask >> 1)))
	    {
	      val |= ~valmask;
	    }
	}
    }
  return (val);
}

/* Modify the value of a bitfield.  ADDR points to a block of memory in
   target byte order; the bitfield starts in the byte pointed to.  FIELDVAL
   is the desired value of the field, in host byte order.  BITPOS and BITSIZE
   indicate which bits (in target bit order) comprise the bitfield.  */

void
modify_field (addr, fieldval, bitpos, bitsize)
     char *addr;
     LONGEST fieldval;
     int bitpos, bitsize;
{
  LONGEST oword;

  /* Reject values too big to fit in the field in question,
     otherwise adjoining fields may be corrupted.  */
  if (bitsize < (8 * sizeof (fieldval))
      && 0 != (fieldval & ~((1<<bitsize)-1)))
    {
      /* FIXME: would like to include fieldval in the message, but
	 we don't have a sprintf_longest.  */
      error ("Value does not fit in %d bits.", bitsize);
    }

  oword = extract_signed_integer (addr, sizeof oword);

  /* Shifting for bit field depends on endianness of the target machine.  */
#if BITS_BIG_ENDIAN
  bitpos = sizeof (oword) * 8 - bitpos - bitsize;
#endif

  /* Mask out old value, while avoiding shifts >= size of oword */
  if (bitsize < 8 * sizeof (oword))
    oword &= ~(((((unsigned LONGEST)1) << bitsize) - 1) << bitpos);
  else
    oword &= ~((~(unsigned LONGEST)0) << bitpos);
  oword |= fieldval << bitpos;

  store_signed_integer (addr, sizeof oword, oword);
}

/* Convert C numbers into newly allocated values */

value
value_from_longest (type, num)
     struct type *type;
     register LONGEST num;
{
  register value val = allocate_value (type);
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);

  switch (code)
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_BOOL:
      store_signed_integer (VALUE_CONTENTS_RAW (val), len, num);
      break;
      
    case TYPE_CODE_REF:
    case TYPE_CODE_PTR:
      /* This assumes that all pointers of a given length
	 have the same form.  */
      store_address (VALUE_CONTENTS_RAW (val), len, (CORE_ADDR) num);
      break;

    default:
      error ("Unexpected type encountered for integer constant.");
    }
  return val;
}

value
value_from_double (type, num)
     struct type *type;
     double num;
{
  register value val = allocate_value (type);
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);

  if (code == TYPE_CODE_FLT)
    {
      if (len == sizeof (float))
	* (float *) VALUE_CONTENTS_RAW (val) = num;
      else if (len == sizeof (double))
	* (double *) VALUE_CONTENTS_RAW (val) = num;
      else
	error ("Floating type encountered with unexpected data length.");
    }
  else
    error ("Unexpected type encountered for floating constant.");

  /* num was in host byte order.  So now put the value's contents
     into target byte order.  */
  SWAP_TARGET_AND_HOST (VALUE_CONTENTS_RAW (val), len);

  return val;
}

/* Deal with the value that is "about to be returned".  */

/* Return the value that a function returning now
   would be returning to its caller, assuming its type is VALTYPE.
   RETBUF is where we look for what ought to be the contents
   of the registers (in raw form).  This is because it is often
   desirable to restore old values to those registers
   after saving the contents of interest, and then call
   this function using the saved values.
   struct_return is non-zero when the function in question is
   using the structure return conventions on the machine in question;
   0 when it is using the value returning conventions (this often
   means returning pointer to where structure is vs. returning value). */

value
value_being_returned (valtype, retbuf, struct_return)
     register struct type *valtype;
     char retbuf[REGISTER_BYTES];
     int struct_return;
     /*ARGSUSED*/
{
  register value val;
  CORE_ADDR addr;

#if defined (EXTRACT_STRUCT_VALUE_ADDRESS)
  /* If this is not defined, just use EXTRACT_RETURN_VALUE instead.  */
  if (struct_return) {
    addr = EXTRACT_STRUCT_VALUE_ADDRESS (retbuf);
    if (!addr)
      error ("Function return value unknown");
    return value_at (valtype, addr);
  }
#endif

  val = allocate_value (valtype);
  EXTRACT_RETURN_VALUE (valtype, retbuf, VALUE_CONTENTS_RAW (val));

  return val;
}

/* Should we use EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc
   and TYPE is the type (which is known to be struct, union or array).

   On most machines, the struct convention is used unless we are
   using gcc and the type is of a special size.  */
/* As of about 31 Mar 93, GCC was changed to be compatible with the
   native compiler.  GCC 2.3.3 was the last release that did it the
   old way.  Since gcc2_compiled was not changed, we have no
   way to correctly win in all cases, so we just do the right thing
   for gcc1 and for gcc2 after this change.  Thus it loses for gcc
   2.0-2.3.3.  This is somewhat unfortunate, but changing gcc2_compiled
   would cause more chaos than dealing with some struct returns being
   handled wrong.  */
#if !defined (USE_STRUCT_CONVENTION)
#define USE_STRUCT_CONVENTION(gcc_p, type)\
  (!((gcc_p == 1) && (TYPE_LENGTH (value_type) == 1                \
		      || TYPE_LENGTH (value_type) == 2             \
		      || TYPE_LENGTH (value_type) == 4             \
		      || TYPE_LENGTH (value_type) == 8             \
		      )                                            \
     ))
#endif

/* Return true if the function specified is using the structure returning
   convention on this machine to return arguments, or 0 if it is using
   the value returning convention.  FUNCTION is the value representing
   the function, FUNCADDR is the address of the function, and VALUE_TYPE
   is the type returned by the function.  GCC_P is nonzero if compiled
   with GCC.  */

int
using_struct_return (function, funcaddr, value_type, gcc_p)
     value function;
     CORE_ADDR funcaddr;
     struct type *value_type;
     int gcc_p;
     /*ARGSUSED*/
{
  register enum type_code code = TYPE_CODE (value_type);

  if (code == TYPE_CODE_ERROR)
    error ("Function return type unknown.");

  if (code == TYPE_CODE_STRUCT ||
      code == TYPE_CODE_UNION ||
      code == TYPE_CODE_ARRAY)
    return USE_STRUCT_CONVENTION (gcc_p, value_type);

  return 0;
}

/* Store VAL so it will be returned if a function returns now.
   Does not verify that VAL's type matches what the current
   function wants to return.  */

void
set_return_value (val)
     value val;
{
  register enum type_code code = TYPE_CODE (VALUE_TYPE (val));
  double dbuf;
  LONGEST lbuf;

  if (code == TYPE_CODE_ERROR)
    error ("Function return type unknown.");

  if (   code == TYPE_CODE_STRUCT
      || code == TYPE_CODE_UNION)	/* FIXME, implement struct return.  */
    error ("GDB does not support specifying a struct or union return value.");

  /* FIXME, this is bogus.  We don't know what the return conventions
     are, or how values should be promoted.... */
  if (code == TYPE_CODE_FLT)
    {
      dbuf = value_as_double (val);

      STORE_RETURN_VALUE (VALUE_TYPE (val), (char *)&dbuf);
    }
  else
    {
      lbuf = value_as_long (val);
      STORE_RETURN_VALUE (VALUE_TYPE (val), (char *)&lbuf);
    }
}

void
_initialize_values ()
{
  add_cmd ("convenience", no_class, show_convenience,
	    "Debugger convenience (\"$foo\") variables.\n\
These variables are created when you assign them values;\n\
thus, \"print $foo=1\" gives \"$foo\" the value 1.  Values may be any type.\n\n\
A few convenience variables are given values automatically:\n\
\"$_\"holds the last address examined with \"x\" or \"info lines\",\n\
\"$__\" holds the contents of the last address examined with \"x\".",
	   &showlist);

  add_cmd ("values", no_class, show_values,
	   "Elements of value history around item number IDX (or last ten).",
	   &showlist);
}
