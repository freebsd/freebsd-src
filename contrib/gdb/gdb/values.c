/* Low level packing and unpacking of values for GDB, the GNU Debugger.
   Copyright 1986, 87, 89, 91, 93, 94, 95, 96, 97, 1998
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
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "gdbcore.h"
#include "frame.h"
#include "command.h"
#include "gdbcmd.h"
#include "target.h"
#include "language.h"
#include "scm-lang.h"
#include "demangle.h"

/* Prototypes for exported functions. */

void _initialize_values PARAMS ((void));

/* Prototypes for local functions. */

static value_ptr value_headof PARAMS ((value_ptr, struct type *,
				       struct type *));

static void show_values PARAMS ((char *, int));

static void show_convenience PARAMS ((char *, int));

static int vb_match PARAMS ((struct type *, int, struct type *));

/* The value-history records all the values printed
   by print commands during this session.  Each chunk
   records 60 consecutive values.  The first chunk on
   the chain records the most recent values.
   The total number of values is in value_history_count.  */

#define VALUE_HISTORY_CHUNK 60

struct value_history_chunk
{
  struct value_history_chunk *next;
  value_ptr values[VALUE_HISTORY_CHUNK];
};

/* Chain of chunks now in use.  */

static struct value_history_chunk *value_history_chain;

static int value_history_count;	/* Abs number of last entry stored */

/* List of all value objects currently allocated
   (except for those released by calls to release_value)
   This is so they can be freed after each command.  */

static value_ptr all_values;

/* Allocate a  value  that has the correct length for type TYPE.  */

value_ptr
allocate_value (type)
     struct type *type;
{
  register value_ptr val;
  struct type *atype = check_typedef (type);

  val = (struct value *) xmalloc (sizeof (struct value) + TYPE_LENGTH (atype));
  VALUE_NEXT (val) = all_values;
  all_values = val;
  VALUE_TYPE (val) = type;
  VALUE_ENCLOSING_TYPE (val) = type;
  VALUE_LVAL (val) = not_lval;
  VALUE_ADDRESS (val) = 0;
  VALUE_FRAME (val) = 0;
  VALUE_OFFSET (val) = 0;
  VALUE_BITPOS (val) = 0;
  VALUE_BITSIZE (val) = 0;
  VALUE_REGNO (val) = -1;
  VALUE_LAZY (val) = 0;
  VALUE_OPTIMIZED_OUT (val) = 0;
  VALUE_BFD_SECTION (val) = NULL;
  VALUE_EMBEDDED_OFFSET (val) = 0;
  VALUE_POINTED_TO_OFFSET (val) = 0;
  val->modifiable = 1;
  return val;
}

/* Allocate a  value  that has the correct length
   for COUNT repetitions type TYPE.  */

value_ptr
allocate_repeat_value (type, count)
     struct type *type;
     int count;
{
  int low_bound = current_language->string_lower_bound; /* ??? */
  /* FIXME-type-allocation: need a way to free this type when we are
     done with it.  */
  struct type *range_type
    = create_range_type ((struct type *) NULL, builtin_type_int,
			 low_bound, count + low_bound - 1);
  /* FIXME-type-allocation: need a way to free this type when we are
     done with it.  */
  return allocate_value (create_array_type ((struct type *) NULL,
					    type, range_type));
}

/* Return a mark in the value chain.  All values allocated after the
   mark is obtained (except for those released) are subject to being freed
   if a subsequent value_free_to_mark is passed the mark.  */
value_ptr
value_mark ()
{
  return all_values;
}

/* Free all values allocated since MARK was obtained by value_mark
   (except for those released).  */
void
value_free_to_mark (mark)
     value_ptr mark;
{
  value_ptr val, next;

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
  register value_ptr val, next;

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
     register value_ptr val;
{
  register value_ptr v;

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

/* Release all values up to mark  */
value_ptr
value_release_to_mark (mark)
     value_ptr mark;
{
  value_ptr val, next;

  for (val = next = all_values; next; next = VALUE_NEXT (next))
    if (VALUE_NEXT (next) == mark)
      {
	all_values = VALUE_NEXT (next);
	VALUE_NEXT (next) = 0;
	return val;
      }
  all_values = 0;
  return val;
}

/* Return a copy of the value ARG.
   It contains the same contents, for same memory address,
   but it's a different block of storage.  */

value_ptr
value_copy (arg)
     value_ptr arg;
{
  register struct type *encl_type = VALUE_ENCLOSING_TYPE (arg);
  register value_ptr val = allocate_value (encl_type);
  VALUE_TYPE (val) = VALUE_TYPE (arg);
  VALUE_LVAL (val) = VALUE_LVAL (arg);
  VALUE_ADDRESS (val) = VALUE_ADDRESS (arg);
  VALUE_OFFSET (val) = VALUE_OFFSET (arg);
  VALUE_BITPOS (val) = VALUE_BITPOS (arg);
  VALUE_BITSIZE (val) = VALUE_BITSIZE (arg);
  VALUE_FRAME (val) = VALUE_FRAME (arg);
  VALUE_REGNO (val) = VALUE_REGNO (arg);
  VALUE_LAZY (val) = VALUE_LAZY (arg);
  VALUE_OPTIMIZED_OUT (val) = VALUE_OPTIMIZED_OUT (arg);
  VALUE_EMBEDDED_OFFSET (val) = VALUE_EMBEDDED_OFFSET (arg);
  VALUE_POINTED_TO_OFFSET (val) = VALUE_POINTED_TO_OFFSET (arg);
  VALUE_BFD_SECTION (val) = VALUE_BFD_SECTION (arg);
  val->modifiable = arg->modifiable;
  if (!VALUE_LAZY (val))
    {
      memcpy (VALUE_CONTENTS_ALL_RAW (val), VALUE_CONTENTS_ALL_RAW (arg),
	      TYPE_LENGTH (VALUE_ENCLOSING_TYPE (arg)));

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
     value_ptr val;
{
  int i;

  /* We don't want this value to have anything to do with the inferior anymore.
     In particular, "set $1 = 50" should not affect the variable from which
     the value was taken, and fast watchpoints should be able to assume that
     a value on the value history never changes.  */
  if (VALUE_LAZY (val))
    value_fetch_lazy (val);
  /* We preserve VALUE_LVAL so that the user can find out where it was fetched
     from.  This is a bit dubious, because then *&$1 does not just return $1
     but the current contents of that location.  c'est la vie...  */
  val->modifiable = 0;
  release_value (val);

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

  /* Now we regard value_history_count as origin-one
     and applying to the value just stored.  */

  return ++value_history_count;
}

/* Return a copy of the value in the history with sequence number NUM.  */

value_ptr
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
  register value_ptr val;

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
  register value_ptr val;
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
      value_print (val, gdb_stdout, 0, Val_pretty_default);
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

value_ptr
value_of_internalvar (var)
     struct internalvar *var;
{
  register value_ptr val;

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
     value_ptr newval;
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
     value_ptr val;
{
  value_ptr newval;

#ifdef IS_TRAPPED_INTERNALVAR
  if (IS_TRAPPED_INTERNALVAR (var->name))
    SET_TRAPPED_INTERNALVAR (var, val, 0, 0, 0);
#endif

  newval = value_copy (val);
  newval->modifiable = 1;

  /* Force the value to be fetched from the target now, to avoid problems
     later when this internalvar is referenced and the target is gone or
     has changed.  */
  if (VALUE_LAZY (newval))
    value_fetch_lazy (newval);

  /* Begin code which must not call error().  If var->value points to
     something free'd, an error() obviously leaves a dangling pointer.
     But we also get a danling pointer if var->value points to
     something in the value chain (i.e., before release_value is
     called), because after the error free_all_values will get called before
     long.  */
  free ((PTR)var->value);
  var->value = newval;
  release_value (newval);
  /* End code which must not call error().  */
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
      value_print (var->value, gdb_stdout, 0, Val_pretty_default);
      printf_filtered ("\n");
    }
  if (!varseen)
    printf_unfiltered ("No debugger convenience variables now defined.\n\
Convenience variables have names starting with \"$\";\n\
use \"set\" as in \"set $foo = 5\" to define them.\n");
}

/* Extract a value as a C number (either long or double).
   Knows how to convert fixed values to double, or
   floating values to long.
   Does not deallocate the value.  */

LONGEST
value_as_long (val)
     register value_ptr val;
{
  /* This coerces arrays and functions, which is necessary (e.g.
     in disassemble_command).  It also dereferences references, which
     I suspect is the most logical thing to do.  */
  COERCE_ARRAY (val);
  return unpack_long (VALUE_TYPE (val), VALUE_CONTENTS (val));
}

DOUBLEST
value_as_double (val)
     register value_ptr val;
{
  DOUBLEST foo;
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
     value_ptr val;
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

LONGEST
unpack_long (type, valaddr)
     struct type *type;
     char *valaddr;
{
  register enum type_code code = TYPE_CODE (type);
  register int len = TYPE_LENGTH (type);
  register int nosign = TYPE_UNSIGNED (type);

  if (current_language->la_language == language_scm
      && is_scmvalue_type (type))
    return scm_unpack (type, valaddr, TYPE_CODE_INT);

  switch (code)
    {
    case TYPE_CODE_TYPEDEF:
      return unpack_long (check_typedef (type), valaddr);
    case TYPE_CODE_ENUM:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_RANGE:
      if (nosign)
	return extract_unsigned_integer (valaddr, len);
      else
	return extract_signed_integer (valaddr, len);

    case TYPE_CODE_FLT:
      return extract_floating (valaddr, len);

    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
	 whether we want this to be true eventually.  */
#ifdef GDB_TARGET_IS_D10V
      if (len == 2)
	  return D10V_MAKE_DADDR(extract_address (valaddr, len));
#endif
      return extract_address (valaddr, len);

    case TYPE_CODE_MEMBER:
      error ("not implemented: member types in unpack_long");

    default:
      error ("Value can't be converted to integer.");
    }
  return 0; /* Placate lint.  */
}

/* Return a double value from the specified type and address.
   INVP points to an int which is set to 0 for valid value,
   1 for invalid value (bad float format).  In either case,
   the returned double is OK to use.  Argument is in target
   format, result is in host format.  */

DOUBLEST
unpack_double (type, valaddr, invp)
     struct type *type;
     char *valaddr;
     int *invp;
{
  enum type_code code;
  int len;
  int nosign;

  *invp = 0;			/* Assume valid.   */
  CHECK_TYPEDEF (type);
  code = TYPE_CODE (type);
  len = TYPE_LENGTH (type);
  nosign = TYPE_UNSIGNED (type);
  if (code == TYPE_CODE_FLT)
    {
#ifdef INVALID_FLOAT
      if (INVALID_FLOAT (valaddr, len))
	{
	  *invp = 1;
	  return 1.234567891011121314;
	}
#endif
      return extract_floating (valaddr, len);
    }
  else if (nosign)
    {
      /* Unsigned -- be sure we compensate for signed LONGEST.  */
#if !defined (_MSC_VER) || (_MSC_VER > 900)
      return (ULONGEST) unpack_long (type, valaddr);
#else
      /* FIXME!!! msvc22 doesn't support unsigned __int64 -> double */
      return (LONGEST) unpack_long (type, valaddr);
#endif /* _MSC_VER */
    }
  else
    {
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

/* Get the value of the FIELDN'th field (which must be static) of TYPE. */

value_ptr
value_static_field (type, fieldno)
     struct type *type;
     int fieldno;
{
  CORE_ADDR addr;
  asection *sect;
  if (TYPE_FIELD_STATIC_HAS_ADDR (type, fieldno))
    {
      addr = TYPE_FIELD_STATIC_PHYSADDR (type, fieldno);
      sect = NULL;
    }
  else
    {
      char *phys_name = TYPE_FIELD_STATIC_PHYSNAME (type, fieldno);
      struct symbol *sym = lookup_symbol (phys_name, 0, VAR_NAMESPACE, 0, NULL);
      if (sym == NULL)
	{
	  /* With some compilers, e.g. HP aCC, static data members are reported
	     as non-debuggable symbols */ 
	  struct minimal_symbol * msym = lookup_minimal_symbol (phys_name, NULL, NULL);
	  if (!msym)
	    return NULL;
	  else
	    {     
	      addr = SYMBOL_VALUE_ADDRESS (msym);
	      sect = SYMBOL_BFD_SECTION (msym);
	    }
	}
      else
	{
	  addr = SYMBOL_VALUE_ADDRESS (sym);
	  sect = SYMBOL_BFD_SECTION (sym);
	}
      SET_FIELD_PHYSADDR (TYPE_FIELD (type, fieldno), addr);
    }
  return value_at (TYPE_FIELD_TYPE (type, fieldno), addr, sect);
}

/* Given a value ARG1 (offset by OFFSET bytes)
   of a struct or union type ARG_TYPE,
   extract and return the value of one of its (non-static) fields.
   FIELDNO says which field. */

value_ptr
value_primitive_field (arg1, offset, fieldno, arg_type)
     register value_ptr arg1;
     int offset;
     register int fieldno;
     register struct type *arg_type;
{
  register value_ptr v;
  register struct type *type;

  CHECK_TYPEDEF (arg_type);
  type = TYPE_FIELD_TYPE (arg_type, fieldno);

  /* Handle packed fields */

  if (TYPE_FIELD_BITSIZE (arg_type, fieldno))
    {
      v = value_from_longest (type,
			      unpack_field_as_long (arg_type,
						    VALUE_CONTENTS (arg1)
						      + offset,
						    fieldno));
      VALUE_BITPOS (v) = TYPE_FIELD_BITPOS (arg_type, fieldno) % 8;
      VALUE_BITSIZE (v) = TYPE_FIELD_BITSIZE (arg_type, fieldno);
    }
  else if (fieldno < TYPE_N_BASECLASSES (arg_type))
    {
      /* This field is actually a base subobject, so preserve the
         entire object's contents for later references to virtual
         bases, etc.  */
      v = allocate_value (VALUE_ENCLOSING_TYPE (arg1));
      VALUE_TYPE (v) = arg_type;
      if (VALUE_LAZY (arg1))
	VALUE_LAZY (v) = 1;
      else
	memcpy (VALUE_CONTENTS_ALL_RAW (v), VALUE_CONTENTS_ALL_RAW (arg1),
		TYPE_LENGTH (VALUE_ENCLOSING_TYPE (arg1)));
      VALUE_OFFSET (v) = VALUE_OFFSET (arg1);
      VALUE_EMBEDDED_OFFSET (v)
        = offset + 
          VALUE_EMBEDDED_OFFSET (arg1) + 
          TYPE_FIELD_BITPOS (arg_type, fieldno) / 8;
    }
  else
    {
      /* Plain old data member */
      offset += TYPE_FIELD_BITPOS (arg_type, fieldno) / 8;
      v = allocate_value (type);
      if (VALUE_LAZY (arg1))
	VALUE_LAZY (v) = 1;
      else
	memcpy (VALUE_CONTENTS_RAW (v),
		VALUE_CONTENTS_RAW (arg1) + offset,
		TYPE_LENGTH (type));
      VALUE_OFFSET (v) = VALUE_OFFSET (arg1) + offset;
    }
  VALUE_LVAL (v) = VALUE_LVAL (arg1);
  if (VALUE_LVAL (arg1) == lval_internalvar)
    VALUE_LVAL (v) = lval_internalvar_component;
  VALUE_ADDRESS (v) = VALUE_ADDRESS (arg1);
/*  VALUE_OFFSET (v) = VALUE_OFFSET (arg1) + offset
		     + TYPE_FIELD_BITPOS (arg_type, fieldno) / 8; */
  return v;
}

/* Given a value ARG1 of a struct or union type,
   extract and return the value of one of its (non-static) fields.
   FIELDNO says which field. */

value_ptr
value_field (arg1, fieldno)
     register value_ptr arg1;
     register int fieldno;
{
  return value_primitive_field (arg1, 0, fieldno, VALUE_TYPE (arg1));
}

/* Return a non-virtual function as a value.
   F is the list of member functions which contains the desired method.
   J is an index into F which provides the desired method. */

value_ptr
value_fn_field (arg1p, f, j, type, offset)
     value_ptr *arg1p;
     struct fn_field *f;
     int j;
     struct type *type;
     int offset;
{
  register value_ptr v;
  register struct type *ftype = TYPE_FN_FIELD_TYPE (f, j);
  struct symbol *sym;

  sym = lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, j),
		       0, VAR_NAMESPACE, 0, NULL);
  if (! sym) 
	return NULL;
/*
	error ("Internal error: could not find physical method named %s",
		    TYPE_FN_FIELD_PHYSNAME (f, j));
*/
  
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
value_ptr
value_virtual_fn_field (arg1p, f, j, type, offset)
     value_ptr *arg1p;
     struct fn_field *f;
     int j;
     struct type *type;
     int offset;
{
  value_ptr arg1 = *arg1p;
  struct type *type1 = check_typedef (VALUE_TYPE (arg1));

  if (TYPE_HAS_VTABLE (type))
    {
      /* Deal with HP/Taligent runtime model for virtual functions */
      value_ptr vp;
      value_ptr argp;        /* arg1 cast to base */
      CORE_ADDR vfunc_addr;  /* address of virtual method */
      CORE_ADDR coreptr;     /* pointer to target address */ 
      int class_index;       /* which class segment pointer to use */
      struct type * ftype = TYPE_FN_FIELD_TYPE (f, j);   /* method type */

      argp = value_cast (type, *arg1p);

      if (VALUE_ADDRESS (argp) == 0)
        error ("Address of object is null; object may not have been created.");
      
      /* pai: FIXME -- 32x64 possible problem? */
      /* First word (4 bytes) in object layout is the vtable pointer */
      coreptr = * (CORE_ADDR *) (VALUE_CONTENTS (argp)); /* pai: (temp)  */
                                 /* + offset + VALUE_EMBEDDED_OFFSET (argp)); */ 

      if (!coreptr)
        error ("Virtual table pointer is null for object; object may not have been created.");
      
      /* pai/1997-05-09
       * FIXME: The code here currently handles only
       * the non-RRBC case of the Taligent/HP runtime spec; when RRBC
       * is introduced, the condition for the "if" below will have to
       * be changed to be a test for the RRBC case.  */
       
      if (1)
        {
          /* Non-RRBC case; the virtual function pointers are stored at fixed
           * offsets in the virtual table. */

          /* Retrieve the offset in the virtual table from the debug
           * info.  The offset of the vfunc's entry is in words from
           * the beginning of the vtable; but first we have to adjust
           * by HP_ACC_VFUNC_START to account for other entries */
          
          /* pai: FIXME: 32x64 problem here, a word may be 8 bytes in
           * which case the multiplier should be 8 and values should be long */
          vp = value_at (builtin_type_int,
                         coreptr + 4 * (TYPE_FN_FIELD_VOFFSET (f, j) + HP_ACC_VFUNC_START), NULL);
          
          coreptr = * (CORE_ADDR *) (VALUE_CONTENTS (vp));
          /* coreptr now contains the address of the virtual function */
          /* (Actually, it contains the pointer to the plabel for the function. */
        }
      else
        {
          /* RRBC case; the virtual function pointers are found by double
           * indirection through the class segment tables. */
          
          /* Choose class segment depending on type we were passed */ 
          class_index = class_index_in_primary_list (type);
      
          /* Find class segment pointer.  These are in the vtable slots after
           * some other entries, so adjust by HP_ACC_VFUNC_START for that. */
          /* pai: FIXME 32x64 problem here, if words are 8 bytes long
           * the multiplier below has to be 8 and value should be long. */
          vp = value_at (builtin_type_int,
                         coreptr + 4 * (HP_ACC_VFUNC_START + class_index), NULL);
          /* Indirect once more, offset by function index */
          /* pai: FIXME 32x64 problem here, again multiplier could be 8 and value long */
          coreptr = * (CORE_ADDR *) (VALUE_CONTENTS (vp) + 4 * TYPE_FN_FIELD_VOFFSET (f, j));
          vp = value_at (builtin_type_int, coreptr, NULL);
          coreptr = * (CORE_ADDR *) (VALUE_CONTENTS (vp));
          
          /* coreptr now contains the address of the virtual function */
          /* (Actually, it contains the pointer to the plabel for the function.) */
          
        }

      if (!coreptr)
        error ("Address of virtual function is null; error in virtual table?");

      /* Wrap this addr in a value and return pointer */ 
      vp = allocate_value (ftype);
      VALUE_TYPE (vp) = ftype;
      VALUE_ADDRESS (vp) = coreptr;
      
      /* pai: (temp) do we need the value_ind stuff in value_fn_field? */
      return vp;
    }
  else  
    { /* Not using HP/Taligent runtime conventions; so try to
       * use g++ conventions for virtual table */
      
      struct type *entry_type;
      /* First, get the virtual function table pointer.  That comes
         with a strange type, so cast it to type `pointer to long' (which
         should serve just fine as a function type).  Then, index into
         the table, and convert final value to appropriate function type.  */
      value_ptr entry, vfn, vtbl;
      value_ptr vi = value_from_longest (builtin_type_int, 
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
      if (TYPE_TARGET_TYPE (context) != type1)
        {
	  value_ptr tmp = value_cast (context, value_addr (arg1));
	  VALUE_POINTED_TO_OFFSET (tmp) = 0;
          arg1 = value_ind (tmp);
          type1 = check_typedef (VALUE_TYPE (arg1));
        }

      context = type1;
      /* Now context is the basetype containing the vtbl.  */

      /* This type may have been defined before its virtual function table
         was.  If so, fill in the virtual function table entry for the
         type now.  */
      if (TYPE_VPTR_FIELDNO (context) < 0)
        fill_in_vptr_fieldno (context);

      /* The virtual function table is now an array of structures
         which have the form { int16 offset, delta; void *pfn; }.  */
      vtbl = value_primitive_field (arg1, 0, TYPE_VPTR_FIELDNO (context),
				    TYPE_VPTR_BASETYPE (context));
      
      /* With older versions of g++, the vtbl field pointed to an array
	 of structures.  Nowadays it points directly to the structure. */
      if (TYPE_CODE (VALUE_TYPE (vtbl)) == TYPE_CODE_PTR
	  && TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (vtbl))) == TYPE_CODE_ARRAY)
	{
	  /* Handle the case where the vtbl field points to an
	     array of structures. */
	  vtbl = value_ind (vtbl);

	  /* Index into the virtual function table.  This is hard-coded because
	     looking up a field is not cheap, and it may be important to save
	     time, e.g. if the user has set a conditional breakpoint calling
	     a virtual function.  */
	  entry = value_subscript (vtbl, vi);
	}
      else
	{
	  /* Handle the case where the vtbl field points directly to a structure. */
	  vtbl = value_add (vtbl, vi);
	  entry = value_ind (vtbl);
	}

      entry_type = check_typedef (VALUE_TYPE (entry));

      if (TYPE_CODE (entry_type) == TYPE_CODE_STRUCT)
        {
          /* Move the `this' pointer according to the virtual function table. */
          VALUE_OFFSET (arg1) += value_as_long (value_field (entry, 0));

          if (! VALUE_LAZY (arg1))
            {
              VALUE_LAZY (arg1) = 1;
              value_fetch_lazy (arg1);
            }

          vfn = value_field (entry, 2);
        }
      else if (TYPE_CODE (entry_type) == TYPE_CODE_PTR)
        vfn = entry;
      else
        error ("I'm confused:  virtual function table has bad type");
      /* Reinstantiate the function pointer with the correct type.  */
      VALUE_TYPE (vfn) = lookup_pointer_type (TYPE_FN_FIELD_TYPE (f, j));

      *arg1p = arg1;
      return vfn;
    }
}

/* ARG is a pointer to an object we know to be at least
   a DTYPE.  BTYPE is the most derived basetype that has
   already been searched (and need not be searched again).
   After looking at the vtables between BTYPE and DTYPE,
   return the most derived type we find.  The caller must
   be satisfied when the return value == DTYPE.

   FIXME-tiemann: should work with dossier entries as well.  */

static value_ptr
value_headof (in_arg, btype, dtype)
     value_ptr in_arg;
     struct type *btype, *dtype;
{
  /* First collect the vtables we must look at for this object.  */
  /* FIXME-tiemann: right now, just look at top-most vtable.  */
  value_ptr arg, vtbl, entry, best_entry = 0;
  int i, nelems;
  int offset, best_offset = 0;
  struct symbol *sym;
  CORE_ADDR pc_for_sym;
  char *demangled_name;
  struct minimal_symbol *msymbol;

  btype = TYPE_VPTR_BASETYPE (dtype);
  CHECK_TYPEDEF (btype);
  arg = in_arg;
  if (btype != dtype)
    arg = value_cast (lookup_pointer_type (btype), arg);
  vtbl = value_ind (value_field (value_ind (arg), TYPE_VPTR_FIELDNO (btype)));

  /* Check that VTBL looks like it points to a virtual function table.  */
  msymbol = lookup_minimal_symbol_by_pc (VALUE_ADDRESS (vtbl));
  if (msymbol == NULL
      || (demangled_name = SYMBOL_NAME (msymbol)) == NULL
      || !VTBL_PREFIX_P (demangled_name))
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
      /* This won't work if we're using thunks. */
      if (TYPE_CODE (check_typedef (VALUE_TYPE (entry))) != TYPE_CODE_STRUCT)
	break;
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

value_ptr
value_from_vtable_info (arg, type)
     value_ptr arg;
     struct type *type;
{
  /* Take care of preliminaries.  */
  if (TYPE_VPTR_FIELDNO (type) < 0)
    fill_in_vptr_fieldno (type);
  if (TYPE_VPTR_FIELDNO (type) < 0)
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
  if (name[1] == 'v' && name[2] == 'b' && is_cplus_marker (name[3]))
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
   the INDEXth baseclass of class TYPE,
   for value at VALADDR (in host) at ADDRESS (in target).
   The result is the offset of the baseclass value relative
   to (the address of)(ARG) + OFFSET.

   -1 is returned on error. */

int
baseclass_offset (type, index, valaddr, address)
     struct type *type;
     int index;
     char *valaddr;
     CORE_ADDR address;
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
				  valaddr + (TYPE_FIELD_BITPOS (type, i) / 8));

	      return addr - (LONGEST) address;
	    }
	}
      /* Not in the fields, so try looking through the baseclasses.  */
      for (i = index+1; i < n_baseclasses; i++)
	{
	  int boffset =
	      baseclass_offset (type, i, valaddr, address);
	  if (boffset)
	    return boffset;
	}
      /* Not found.  */
      return -1;
    }

  /* Baseclass is easily computed.  */
  return TYPE_BASECLASS_BITPOS (type, index) / 8;
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
  ULONGEST val;
  ULONGEST valmask;
  int bitpos = TYPE_FIELD_BITPOS (type, fieldno);
  int bitsize = TYPE_FIELD_BITSIZE (type, fieldno);
  int lsbcount;
  struct type *field_type;

  val = extract_unsigned_integer (valaddr + bitpos / 8, sizeof (val));
  field_type = TYPE_FIELD_TYPE (type, fieldno);
  CHECK_TYPEDEF (field_type);

  /* Extract bits.  See comment above. */

  if (BITS_BIG_ENDIAN)
    lsbcount = (sizeof val * 8 - bitpos % 8 - bitsize);
  else
    lsbcount = (bitpos % 8);
  val >>= lsbcount;

  /* If the field does not entirely fill a LONGEST, then zero the sign bits.
     If the field is signed, and is negative, then sign extend. */

  if ((bitsize > 0) && (bitsize < 8 * (int) sizeof (val)))
    {
      valmask = (((ULONGEST) 1) << bitsize) - 1;
      val &= valmask;
      if (!TYPE_UNSIGNED (field_type))
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

  /* If a negative fieldval fits in the field in question, chop
     off the sign extension bits.  */
  if (bitsize < (8 * (int) sizeof (fieldval))
      && (~fieldval & ~((1 << (bitsize - 1)) - 1)) == 0)
    fieldval = fieldval & ((1 << bitsize) - 1);

  /* Warn if value is too big to fit in the field in question.  */
  if (bitsize < (8 * (int) sizeof (fieldval))
      && 0 != (fieldval & ~((1<<bitsize)-1)))
    {
      /* FIXME: would like to include fieldval in the message, but
	 we don't have a sprintf_longest.  */
      warning ("Value does not fit in %d bits.", bitsize);

      /* Truncate it, otherwise adjoining fields may be corrupted.  */
      fieldval = fieldval & ((1 << bitsize) - 1);
    }

  oword = extract_signed_integer (addr, sizeof oword);

  /* Shifting for bit field depends on endianness of the target machine.  */
  if (BITS_BIG_ENDIAN)
    bitpos = sizeof (oword) * 8 - bitpos - bitsize;

  /* Mask out old value, while avoiding shifts >= size of oword */
  if (bitsize < 8 * (int) sizeof (oword))
    oword &= ~(((((ULONGEST)1) << bitsize) - 1) << bitpos);
  else
    oword &= ~((~(ULONGEST)0) << bitpos);
  oword |= fieldval << bitpos;

  store_signed_integer (addr, sizeof oword, oword);
}

/* Convert C numbers into newly allocated values */

value_ptr
value_from_longest (type, num)
     struct type *type;
     register LONGEST num;
{
  register value_ptr val = allocate_value (type);
  register enum type_code code;
  register int len;
 retry:
  code = TYPE_CODE (type);
  len = TYPE_LENGTH (type);

  switch (code)
    {
    case TYPE_CODE_TYPEDEF:
      type = check_typedef (type);
      goto retry;
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_RANGE:
      store_signed_integer (VALUE_CONTENTS_RAW (val), len, num);
      break;
      
    case TYPE_CODE_REF:
    case TYPE_CODE_PTR:
      /* This assumes that all pointers of a given length
	 have the same form.  */
      store_address (VALUE_CONTENTS_RAW (val), len, (CORE_ADDR) num);
      break;
      
    default:
      error ("Unexpected type (%d) encountered for integer constant.", code);
    }
  return val;
}

value_ptr
value_from_double (type, num)
     struct type *type;
     DOUBLEST num;
{
  register value_ptr val = allocate_value (type);
  struct type *base_type = check_typedef (type);
  register enum type_code code = TYPE_CODE (base_type);
  register int len = TYPE_LENGTH (base_type);

  if (code == TYPE_CODE_FLT)
    {
      store_floating (VALUE_CONTENTS_RAW (val), len, num);
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

value_ptr
value_being_returned (valtype, retbuf, struct_return)
     register struct type *valtype;
     char retbuf[REGISTER_BYTES];
     int struct_return;
     /*ARGSUSED*/
{
  register value_ptr val;
  CORE_ADDR addr;

#if defined (EXTRACT_STRUCT_VALUE_ADDRESS)
  /* If this is not defined, just use EXTRACT_RETURN_VALUE instead.  */
  if (struct_return) {
    addr = EXTRACT_STRUCT_VALUE_ADDRESS (retbuf);
    if (!addr)
      error ("Function return value unknown");
    return value_at (valtype, addr, NULL);
  }
#endif

  val = allocate_value (valtype);
  CHECK_TYPEDEF (valtype);
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

int
generic_use_struct_convention (gcc_p, value_type)
     int gcc_p;
     struct type *value_type;
{     
  return !((gcc_p == 1)
	    && (TYPE_LENGTH (value_type) == 1
		|| TYPE_LENGTH (value_type) == 2
		|| TYPE_LENGTH (value_type) == 4
		|| TYPE_LENGTH (value_type) == 8));
}

#ifndef USE_STRUCT_CONVENTION
#define USE_STRUCT_CONVENTION(gcc_p,type) generic_use_struct_convention (gcc_p, type)
#endif

/* Some fundamental types (such as long double) are returned on the stack for
   certain architectures.  This macro should return true for any type besides
   struct, union or array that gets returned on the stack.  */

#ifndef RETURN_VALUE_ON_STACK
#define RETURN_VALUE_ON_STACK(TYPE) 0
#endif

/* Return true if the function specified is using the structure returning
   convention on this machine to return arguments, or 0 if it is using
   the value returning convention.  FUNCTION is the value representing
   the function, FUNCADDR is the address of the function, and VALUE_TYPE
   is the type returned by the function.  GCC_P is nonzero if compiled
   with GCC.  */

int
using_struct_return (function, funcaddr, value_type, gcc_p)
     value_ptr function;
     CORE_ADDR funcaddr;
     struct type *value_type;
     int gcc_p;
     /*ARGSUSED*/
{
  register enum type_code code = TYPE_CODE (value_type);

  if (code == TYPE_CODE_ERROR)
    error ("Function return type unknown.");

  if (code == TYPE_CODE_STRUCT
      || code == TYPE_CODE_UNION
      || code == TYPE_CODE_ARRAY
      || RETURN_VALUE_ON_STACK (value_type))
    return USE_STRUCT_CONVENTION (gcc_p, value_type);

  return 0;
}

/* Store VAL so it will be returned if a function returns now.
   Does not verify that VAL's type matches what the current
   function wants to return.  */

void
set_return_value (val)
     value_ptr val;
{
  struct type *type = check_typedef (VALUE_TYPE (val));
  register enum type_code code = TYPE_CODE (type);

  if (code == TYPE_CODE_ERROR)
    error ("Function return type unknown.");

  if (   code == TYPE_CODE_STRUCT
      || code == TYPE_CODE_UNION)	/* FIXME, implement struct return.  */
    error ("GDB does not support specifying a struct or union return value.");

  STORE_RETURN_VALUE (type, VALUE_CONTENTS (val));
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
