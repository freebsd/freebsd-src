/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)values.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* Low level packing and unpacking of values for GDB.
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

#include <stdio.h>
#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "value.h"

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

  /* If the type we want had no definition in the file it first
   * appeared in, it will be marked a `stub'.  The real definition
   * probably appeared later so try to find it. */
  if (TYPE_FLAGS(type) & TYPE_FLAG_STUB)
    {
      register char *cp;
      register struct symbol *sym;
      extern char *index();

      if (cp = index(TYPE_NAME(type), ' '))
	++cp;
      else
	cp = TYPE_NAME(type);

      sym = lookup_symbol(cp, 0, STRUCT_NAMESPACE, 0);

      if (sym && TYPE_CODE(SYMBOL_TYPE(sym)) == TYPE_CODE(type))
	bcopy (SYMBOL_TYPE (sym), type, sizeof (*type));
    }
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
  return val;
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
      free (val);
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

static value
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
  bcopy (VALUE_CONTENTS (arg), VALUE_CONTENTS (val),
	 TYPE_LENGTH (VALUE_TYPE (arg))
	 * (VALUE_REPEATED (arg) ? VALUE_REPETITIONS (arg) : 1));
  return val;
}

/* Access to the value history.  */

/* Record a new value in the value history.
   Returns the absolute history index of the entry.  */

int
record_latest_value (val)
     value val;
{
  int i;
  double foo;

  /* Check error now if about to store an invalid float.  We return -1
     to the caller, but allow them to continue, e.g. to print it as "Nan". */
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FLT) {
    foo = unpack_double (VALUE_TYPE (val), VALUE_CONTENTS (val), &i);
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
      bzero (new->values, sizeof new->values);
      new->next = value_history_chain;
      value_history_chain = new;
    }

  value_history_chain->values[i] = val;
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
	if (val = value_history_chain->values[i])
	  free (val);
      next = value_history_chain->next;
      free (value_history_chain);
      value_history_chain = next;
    }
  value_history_count = 0;
}

static void
value_history_info (num_exp, from_tty)
     char *num_exp;
     int from_tty;
{
  register int i;
  register value val;
  static int num = 1;

  if (num_exp)
    {
      if (num_exp[0] == '+' && num_exp[1] == '\0')
	/* "info history +" should print from the stored position.  */
	;
      else
	/* "info history <exp>" should print around value number <exp>.  */
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
    if (!strcmp (var->name, name))
      return var;

  var = (struct internalvar *) xmalloc (sizeof (struct internalvar));
  var->name = concat (name, "", "");
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
    modify_field (addr, (int) value_as_long (newval),
		  bitpos, bitsize);
  else
    bcopy (VALUE_CONTENTS (newval), addr,
	   TYPE_LENGTH (VALUE_TYPE (newval)));
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

  free (var->value);
  var->value = value_copy (val);
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
      free (var->name);
      free (var->value);
      free (var);
    }
}

static void
convenience_info ()
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
	  printf ("Debugger convenience variables:\n\n");
	  varseen = 1;
	}
      printf ("$%s: ", var->name);
      value_print (var->value, stdout, 0, Val_pretty_default);
      printf ("\n");
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

/* Unpack raw data (copied from debugee) at VALADDR
   as a long, or as a double, assuming the raw data is described
   by type TYPE.  Knows how to convert different sizes of values
   and can convert between fixed and floating point.

   C++: It is assumed that the front-end has taken care of
   all matters concerning pointers to members.  A pointer
   to member which reaches here is considered to be equivalent
   to an INT (or some size).  After all, it is only an offset.  */

LONGEST
unpack_long (type, valaddr)
     struct type *type;
     char *valaddr;
{
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);
  register int nosign = TYPE_UNSIGNED (type);

  if (code == TYPE_CODE_ENUM)
    code = TYPE_CODE_INT;
  if (code == TYPE_CODE_FLT)
    {
      if (len == sizeof (float))
	return * (float *) valaddr;

      if (len == sizeof (double))
	return * (double *) valaddr;
    }
  else if (code == TYPE_CODE_INT && nosign)
    {
      if (len == sizeof (char))
	return * (unsigned char *) valaddr;

      if (len == sizeof (short))
	return * (unsigned short *) valaddr;

      if (len == sizeof (int))
	return * (unsigned int *) valaddr;

      if (len == sizeof (long))
	return * (unsigned long *) valaddr;
#ifdef LONG_LONG
      if (len == sizeof (long long))
	return * (unsigned long long *) valaddr;
#endif
    }
  else if (code == TYPE_CODE_INT)
    {
      if (len == sizeof (char))
	return * (char *) valaddr;

      if (len == sizeof (short))
	return * (short *) valaddr;

      if (len == sizeof (int))
	return * (int *) valaddr;

      if (len == sizeof (long))
	return * (long *) valaddr;

#ifdef LONG_LONG
      if (len == sizeof (long long))
	return * (long long *) valaddr;
#endif
    }
  else if (code == TYPE_CODE_PTR
	   || code == TYPE_CODE_REF)
    {
      if (len == sizeof (char *))
	return (CORE_ADDR) * (char **) valaddr;
    }
  else if (code == TYPE_CODE_MEMBER)
    error ("not implemented: member types in unpack_long");

  error ("Value not integer or pointer.");
}

/* Return a double value from the specified type and address.
   INVP points to an int which is set to 0 for valid value,
   1 for invalid value (bad float format).  In either case,
   the returned double is OK to use.  */

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
	return * (float *) valaddr;

      if (len == sizeof (double))
	{
	  /* Some machines require doubleword alignment for doubles.
	     This code works on them, and on other machines.  */
	  double temp;
	  bcopy ((char *) valaddr, (char *) &temp, sizeof (double));
	  return temp;
	}
    }
  else if (code == TYPE_CODE_INT && nosign)
    {
      if (len == sizeof (char))
	return * (unsigned char *) valaddr;

      if (len == sizeof (short))
	return * (unsigned short *) valaddr;

      if (len == sizeof (int))
	return * (unsigned int *) valaddr;

      if (len == sizeof (long))
	return * (unsigned long *) valaddr;

#ifdef LONG_LONG
      if (len == sizeof (long long))
	return * (unsigned long long *) valaddr;
#endif
    }
  else if (code == TYPE_CODE_INT)
    {
      if (len == sizeof (char))
	return * (char *) valaddr;

      if (len == sizeof (short))
	return * (short *) valaddr;

      if (len == sizeof (int))
	return * (int *) valaddr;

      if (len == sizeof (long))
	return * (long *) valaddr;

#ifdef LONG_LONG
      if (len == sizeof (long long))
	return * (long long *) valaddr;
#endif
    }

  error ("Value not floating number.");
  /* NOTREACHED */
  return (double) 0;		/* To silence compiler warning.  */
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
  register value v;
  register struct type *type = TYPE_FIELD_TYPE (VALUE_TYPE (arg1), fieldno);
  register int offset;

  /* Handle packed fields */

  offset = TYPE_FIELD_BITPOS (VALUE_TYPE (arg1), fieldno) / 8;
  if (TYPE_FIELD_BITSIZE (VALUE_TYPE (arg1), fieldno))
    {
      v = value_from_long (type,
			   unpack_field_as_long (VALUE_TYPE (arg1),
						 VALUE_CONTENTS (arg1),
						 fieldno));
      VALUE_BITPOS (v) = TYPE_FIELD_BITPOS (VALUE_TYPE (arg1), fieldno) % 8;
      VALUE_BITSIZE (v) = TYPE_FIELD_BITSIZE (VALUE_TYPE (arg1), fieldno);
    }
  else
    {
      v = allocate_value (type);
      bcopy (VALUE_CONTENTS (arg1) + offset,
	     VALUE_CONTENTS (v),
	     TYPE_LENGTH (type));
    }
  VALUE_LVAL (v) = VALUE_LVAL (arg1);
  if (VALUE_LVAL (arg1) == lval_internalvar)
    VALUE_LVAL (v) = lval_internalvar_component;
  VALUE_ADDRESS (v) = VALUE_ADDRESS (arg1);
  VALUE_OFFSET (v) = offset + VALUE_OFFSET (arg1);
  return v;
}

value
value_fn_field (arg1, fieldno, subfieldno)
     register value arg1;
     register int fieldno;
{
  register value v;
  struct fn_field *f = TYPE_FN_FIELDLIST1 (VALUE_TYPE (arg1), fieldno);
  register struct type *type = TYPE_FN_FIELD_TYPE (f, subfieldno);
  struct symbol *sym;

  sym = lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, subfieldno),
		       0, VAR_NAMESPACE, 0);
  if (! sym) error ("Internal error: could not find physical method named %s",
		    TYPE_FN_FIELD_PHYSNAME (f, subfieldno));
  
  v = allocate_value (type);
  VALUE_ADDRESS (v) = BLOCK_START (SYMBOL_BLOCK_VALUE (sym));
  VALUE_TYPE (v) = type;
  return v;
}

/* Return a virtual function as a value.
   ARG1 is the object which provides the virtual function
   table pointer.
   F is the list of member functions which contains the desired virtual
   function.
   J is an index into F which provides the desired virtual function.
   TYPE is the basetype which first provides the virtual function table.  */
value
value_virtual_fn_field (arg1, f, j, type)
     value arg1;
     struct fn_field *f;
     int j;
     struct type *type;
{
  /* First, get the virtual function table pointer.  That comes
     with a strange type, so cast it to type `pointer to long' (which
     should serve just fine as a function type).  Then, index into
     the table, and convert final value to appropriate function type.  */
  value vfn, vtbl;
  value vi = value_from_long (builtin_type_int, 
			      (LONGEST) TYPE_FN_FIELD_VOFFSET (f, j));
  VALUE_TYPE (arg1) = TYPE_VPTR_BASETYPE (type);

  /* This type may have been defined before its virtual function table
     was.  If so, fill in the virtual function table entry for the
     type now.  */
  if (TYPE_VPTR_FIELDNO (type) < 0)
    TYPE_VPTR_FIELDNO (type)
      = fill_in_vptr_fieldno (type);

  /* The virtual function table is now an array of structures
     which have the form { int16 offset, delta; void *pfn; }.  */
  vtbl = value_ind (value_field (arg1, TYPE_VPTR_FIELDNO (type)));

  /* Index into the virtual function table.  This is hard-coded because
     looking up a field is not cheap, and it may be important to save
     time, e.g. if the user has set a conditional breakpoint calling
     a virtual function.  */
  vfn = value_field (value_subscript (vtbl, vi), 2);

  /* Reinstantiate the function pointer with the correct type.  */
  VALUE_TYPE (vfn) = lookup_pointer_type (TYPE_FN_FIELD_TYPE (f, j));
  return vfn;
}

/* The value of a static class member does not depend
   on its instance, only on its type.  If FIELDNO >= 0,
   then fieldno is a valid field number and is used directly.
   Otherwise, FIELDNAME is the name of the field we are
   searching for.  If it is not a static field name, an
   error is signaled.  TYPE is the type in which we look for the
   static field member.  */
value
value_static_field (type, fieldname, fieldno)
     register struct type *type;
     char *fieldname;
     register int fieldno;
{
  register value v;
  struct symbol *sym;

  if (fieldno < 0)
    {
      register struct type *t = type;
      /* Look for static field.  */
      while (t)
	{
	  int i;
	  for (i = TYPE_NFIELDS (t) - 1; i >= 0; i--)
	    if (! strcmp (TYPE_FIELD_NAME (t, i), fieldname))
	      {
		if (TYPE_FIELD_STATIC (t, i))
		  {
		    fieldno = i;
		    goto found;
		  }
		else
		  error ("field `%s' is not static");
	      }
	  t = TYPE_BASECLASSES (t) ? TYPE_BASECLASS (t, 1) : 0;
	}

      t = type;

      if (destructor_name_p (fieldname, t))
	error ("use `info method' command to print out value of destructor");

      while (t)
	{
	  int i, j;

	  for (i = TYPE_NFN_FIELDS (t) - 1; i >= 0; i--)
	    {
	      if (! strcmp (TYPE_FN_FIELDLIST_NAME (t, i), fieldname))
		{
		  error ("use `info method' command to print value of method \"%s\"", fieldname);
		}
	    }
	  t = TYPE_BASECLASSES (t) ? TYPE_BASECLASS (t, 1) : 0;
	}
      error("there is no field named %s", fieldname);
    }

 found:

  sym = lookup_symbol (TYPE_FIELD_STATIC_PHYSNAME (type, fieldno),
		       0, VAR_NAMESPACE, 0);
  if (! sym) error ("Internal error: could not find physical static variable named %s", TYPE_FIELD_BITSIZE (type, fieldno));

  type = TYPE_FIELD_TYPE (type, fieldno);
  v = value_at (type, (CORE_ADDR)SYMBOL_BLOCK_VALUE (sym));
  return v;
}

long
unpack_field_as_long (type, valaddr, fieldno)
     struct type *type;
     char *valaddr;
     int fieldno;
{
  long val;
  int bitpos = TYPE_FIELD_BITPOS (type, fieldno);
  int bitsize = TYPE_FIELD_BITSIZE (type, fieldno);

  bcopy (valaddr + bitpos / 8, &val, sizeof val);

  /* Extracting bits depends on endianness of the machine.  */
#ifdef BITS_BIG_ENDIAN
  val = val >> (sizeof val * 8 - bitpos % 8 - bitsize);
#else
  val = val >> (bitpos % 8);
#endif

  val &= (1 << bitsize) - 1;
  return val;
}

void
modify_field (addr, fieldval, bitpos, bitsize)
     char *addr;
     int fieldval;
     int bitpos, bitsize;
{
  long oword;

  /* Reject values too big to fit in the field in question.
     Otherwise adjoining fields may be corrupted.  */
  if (fieldval & ~((1<<bitsize)-1))
    error ("Value %d does not fit in %d bits.", fieldval, bitsize);
  
  bcopy (addr, &oword, sizeof oword);

  /* Shifting for bit field depends on endianness of the machine.  */
#ifdef BITS_BIG_ENDIAN
  bitpos = sizeof (oword) * 8 - bitpos - bitsize;
#endif

  oword &= ~(((1 << bitsize) - 1) << bitpos);
  oword |= fieldval << bitpos;
  bcopy (&oword, addr, sizeof oword);
}

/* Convert C numbers into newly allocated values */

value
value_from_long (type, num)
     struct type *type;
     register LONGEST num;
{
  register value val = allocate_value (type);
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);

  if (code == TYPE_CODE_INT || code == TYPE_CODE_ENUM)
    {
      if (len == sizeof (char))
	* (char *) VALUE_CONTENTS (val) = num;
      else if (len == sizeof (short))
	* (short *) VALUE_CONTENTS (val) = num;
      else if (len == sizeof (int))
	* (int *) VALUE_CONTENTS (val) = num;
      else if (len == sizeof (long))
	* (long *) VALUE_CONTENTS (val) = num;
#ifdef LONG_LONG
      else if (len == sizeof (long long))
	* (long long *) VALUE_CONTENTS (val) = num;
#endif
      else
	error ("Integer type encountered with unexpected data length.");
    }
  else
    error ("Unexpected type encountered for integer constant.");

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
	* (float *) VALUE_CONTENTS (val) = num;
      else if (len == sizeof (double))
	* (double *) VALUE_CONTENTS (val) = num;
      else
	error ("Floating type encountered with unexpected data length.");
    }
  else
    error ("Unexpected type encountered for floating constant.");

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
{
  register value val;

  if (struct_return)
    return value_at (valtype, EXTRACT_STRUCT_VALUE_ADDRESS (retbuf));

  val = allocate_value (valtype);
  EXTRACT_RETURN_VALUE (valtype, retbuf, VALUE_CONTENTS (val));

  return val;
}

/* Return true if the function specified is using the structure returning
   convention on this machine to return arguments, or 0 if it is using
   the value returning convention.  FUNCTION is the value representing
   the function, FUNCADDR is the address of the function, and VALUE_TYPE
   is the type returned by the function */

struct block *block_for_pc ();

int
using_struct_return (function, funcaddr, value_type)
     value function;
     CORE_ADDR funcaddr;
     struct type *value_type;
{
  register enum type_code code = TYPE_CODE (value_type);

  if (code == TYPE_CODE_STRUCT ||
      code == TYPE_CODE_UNION ||
      code == TYPE_CODE_ARRAY)
    {
      struct block *b = block_for_pc (funcaddr);

      if (!(BLOCK_GCC_COMPILED (b) && TYPE_LENGTH (value_type) < 8))
	return 1;
    }

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
  char regbuf[REGISTER_BYTES];
  double dbuf;
  LONGEST lbuf;

  if (code == TYPE_CODE_STRUCT
      || code == TYPE_CODE_UNION)
    error ("Specifying a struct or union return value is not supported.");

  if (code == TYPE_CODE_FLT)
    {
      dbuf = value_as_double (val);

      STORE_RETURN_VALUE (VALUE_TYPE (val), &dbuf);
    }
  else
    {
      lbuf = value_as_long (val);
      STORE_RETURN_VALUE (VALUE_TYPE (val), &lbuf);
    }
}

void
_initialize_values ()
{
  add_info ("convenience", convenience_info,
	    "Debugger convenience (\"$foo\") variables.\n\
These variables are created when you assign them values;\n\
thus, \"print $foo=1\" gives \"$foo\" the value 1.  Values may be any type.\n\n\
A few convenience variables are given values automatically GDB:\n\
\"$_\"holds the last address examined with \"x\" or \"info lines\",\n\
\"$__\" holds the contents of the last address examined with \"x\".");

  add_info ("values", value_history_info,
	    "Elements of value history (around item number IDX, or last ten).");
  add_info_alias ("history", value_history_info, 0);
}
