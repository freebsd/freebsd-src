/* Support routines for manipulating internal types for GDB.
   Copyright (C) 1992, 93, 94, 95, 96, 1998 Free Software Foundation, Inc.
   Contributed by Cygnus Support, using pieces from other GDB modules.

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
#include "bfd.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "expression.h"
#include "language.h"
#include "target.h"
#include "value.h"
#include "demangle.h"
#include "complaints.h"
#include "gdbcmd.h"

/* These variables point to the objects
   representing the predefined C data types.  */

struct type *builtin_type_void;
struct type *builtin_type_char;
struct type *builtin_type_short;
struct type *builtin_type_int;
struct type *builtin_type_long;
struct type *builtin_type_long_long;
struct type *builtin_type_signed_char;
struct type *builtin_type_unsigned_char;
struct type *builtin_type_unsigned_short;
struct type *builtin_type_unsigned_int;
struct type *builtin_type_unsigned_long;
struct type *builtin_type_unsigned_long_long;
struct type *builtin_type_float;
struct type *builtin_type_double;
struct type *builtin_type_long_double;
struct type *builtin_type_complex;
struct type *builtin_type_double_complex;
struct type *builtin_type_string;
struct type *builtin_type_int8;
struct type *builtin_type_uint8;
struct type *builtin_type_int16;
struct type *builtin_type_uint16;
struct type *builtin_type_int32;
struct type *builtin_type_uint32;
struct type *builtin_type_int64;
struct type *builtin_type_uint64;
struct type *builtin_type_bool;

int opaque_type_resolution = 1;


struct extra { char str[128]; int len; }; /* maximum extention is 128! FIXME */

static void add_name PARAMS ((struct extra *, char *));
static void add_mangled_type PARAMS ((struct extra *, struct type *));
#if 0
static void cfront_mangle_name PARAMS ((struct type *, int, int));
#endif
static void print_bit_vector PARAMS ((B_TYPE *, int));
static void print_arg_types PARAMS ((struct type **, int));
static void dump_fn_fieldlists PARAMS ((struct type *, int));
static void print_cplus_stuff PARAMS ((struct type *, int));

/* Alloc a new type structure and fill it with some defaults.  If
   OBJFILE is non-NULL, then allocate the space for the type structure
   in that objfile's type_obstack. */

struct type *
alloc_type (objfile)
     struct objfile *objfile;
{
  register struct type *type;

  /* Alloc the structure and start off with all fields zeroed. */

  if (objfile == NULL)
    {
      type  = (struct type *) xmalloc (sizeof (struct type));
    }
  else
    {
      type  = (struct type *) obstack_alloc (&objfile -> type_obstack,
					     sizeof (struct type));
      OBJSTAT (objfile, n_types++);
    }
  memset ((char *) type, 0, sizeof (struct type));

  /* Initialize the fields that might not be zero. */

  TYPE_CODE (type) = TYPE_CODE_UNDEF;
  TYPE_OBJFILE (type) = objfile;
  TYPE_VPTR_FIELDNO (type) = -1;
  TYPE_CV_TYPE (type) = type;  /* chain back to itself */ 

  return (type);
}

/* Lookup a pointer to a type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the pointer type should be stored.
   If *TYPEPTR is zero, update it to point to the pointer type we return.
   We allocate new memory if needed.  */

struct type *
make_pointer_type (type, typeptr)
     struct type *type;
     struct type **typeptr;
{
  register struct type *ntype;		/* New type */
  struct objfile *objfile;

  ntype = TYPE_POINTER_TYPE (type);

  if (ntype) 
    {
      if (typeptr == 0)		
        return ntype;	/* Don't care about alloc, and have new type.  */
      else if (*typeptr == 0)
        {
	  *typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	  return ntype;
        }
    }

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else				/* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      memset ((char *) ntype, 0, sizeof (struct type));
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;
  TYPE_POINTER_TYPE (type) = ntype;

  /* FIXME!  Assume the machine has only one representation for pointers!  */

  TYPE_LENGTH (ntype) = TARGET_PTR_BIT / TARGET_CHAR_BIT;
  TYPE_CODE (ntype) = TYPE_CODE_PTR;

  /* pointers are unsigned */
  TYPE_FLAGS (ntype) |= TYPE_FLAG_UNSIGNED;
  
  if (!TYPE_POINTER_TYPE (type))	/* Remember it, if don't have one.  */
    TYPE_POINTER_TYPE (type) = ntype;

  return ntype;
}

/* Given a type TYPE, return a type of pointers to that type.
   May need to construct such a type if this is the first use.  */

struct type *
lookup_pointer_type (type)
     struct type *type;
{
  return make_pointer_type (type, (struct type **)0);
}

/* Lookup a C++ `reference' to a type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the reference type should be stored.
   If *TYPEPTR is zero, update it to point to the reference type we return.
   We allocate new memory if needed.  */

struct type *
make_reference_type (type, typeptr)
     struct type *type;
     struct type **typeptr;
{
  register struct type *ntype;		/* New type */
  struct objfile *objfile;

  ntype = TYPE_REFERENCE_TYPE (type);

  if (ntype) 
    {
      if (typeptr == 0)		
        return ntype;	/* Don't care about alloc, and have new type.  */
      else if (*typeptr == 0)
        {
	  *typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	  return ntype;
        }
    }

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else				/* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      memset ((char *) ntype, 0, sizeof (struct type));
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;
  TYPE_REFERENCE_TYPE (type) = ntype;

  /* FIXME!  Assume the machine has only one representation for references,
     and that it matches the (only) representation for pointers!  */

  TYPE_LENGTH (ntype) = TARGET_PTR_BIT / TARGET_CHAR_BIT;
  TYPE_CODE (ntype) = TYPE_CODE_REF;
  
  if (!TYPE_REFERENCE_TYPE (type))	/* Remember it, if don't have one.  */
    TYPE_REFERENCE_TYPE (type) = ntype;

  return ntype;
}

/* Same as above, but caller doesn't care about memory allocation details.  */

struct type *
lookup_reference_type (type)
     struct type *type;
{
  return make_reference_type (type, (struct type **)0);
}

/* Lookup a function type that returns type TYPE.  TYPEPTR, if nonzero, points
   to a pointer to memory where the function type should be stored.
   If *TYPEPTR is zero, update it to point to the function type we return.
   We allocate new memory if needed.  */

struct type *
make_function_type (type, typeptr)
     struct type *type;
     struct type **typeptr;
{
  register struct type *ntype;		/* New type */
  struct objfile *objfile;

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else				/* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      memset ((char *) ntype, 0, sizeof (struct type));
      TYPE_OBJFILE (ntype) = objfile;
    }

  TYPE_TARGET_TYPE (ntype) = type;

  TYPE_LENGTH (ntype) = 1;
  TYPE_CODE (ntype) = TYPE_CODE_FUNC;
  
  return ntype;
}


/* Given a type TYPE, return a type of functions that return that type.
   May need to construct such a type if this is the first use.  */

struct type *
lookup_function_type (type)
     struct type *type;
{
  return make_function_type (type, (struct type **)0);
}


/* Make a "c-v" variant of a type -- a type that is identical to the
   one supplied except that it may have const or volatile attributes
   CNST is a flag for setting the const attribute
   VOLTL is a flag for setting the volatile attribute
   TYPE is the base type whose variant we are creating.
   TYPEPTR, if nonzero, points
   to a pointer to memory where the reference type should be stored.
   If *TYPEPTR is zero, update it to point to the reference type we return.
   We allocate new memory if needed.  */

struct type *
make_cv_type (cnst, voltl, type, typeptr)
     int cnst;
     int voltl;
     struct type *type;
     struct type **typeptr;
{
  register struct type *ntype;		 /* New type */
  register struct type *tmp_type = type; /* tmp type */
  struct objfile *objfile;

  ntype = TYPE_CV_TYPE (type);

  while (ntype != type)
    {
      if ((TYPE_CONST (ntype) == cnst) &&
          (TYPE_VOLATILE (ntype) == voltl))
          {
            if (typeptr == 0)
              return ntype;
            else if (*typeptr == 0)
              {
                *typeptr = ntype;	/* Tracking alloc, and we have new type.  */
                return ntype;
              }
          }
      tmp_type = ntype;
      ntype = TYPE_CV_TYPE (ntype);
    }

  if (typeptr == 0 || *typeptr == 0)	/* We'll need to allocate one.  */
    {
      ntype = alloc_type (TYPE_OBJFILE (type));
      if (typeptr)
	*typeptr = ntype;
    }
  else				/* We have storage, but need to reset it.  */
    {
      ntype = *typeptr;
      objfile = TYPE_OBJFILE (ntype);
      /* memset ((char *) ntype, 0, sizeof (struct type)); */
      TYPE_OBJFILE (ntype) = objfile;
    }

  /* Copy original type */ 
  memcpy ((char *) ntype, (char *) type, sizeof (struct type));
  /* But zero out fields that shouldn't be copied */
  TYPE_POINTER_TYPE (ntype) = (struct type *) 0;    /* Need new pointer kind */
  TYPE_REFERENCE_TYPE (ntype) = (struct type *) 0;  /* Need new referene kind */
  /* Note: TYPE_TARGET_TYPE can be left as is */

  /* Set flags appropriately */
  if (cnst)
    TYPE_FLAGS (ntype) |=  TYPE_FLAG_CONST;
  else
    TYPE_FLAGS (ntype) &=  ~TYPE_FLAG_CONST;

  if (voltl)
    TYPE_FLAGS (ntype) |=  TYPE_FLAG_VOLATILE;
  else
    TYPE_FLAGS (ntype) &=  ~TYPE_FLAG_VOLATILE;

  /* Fix the chain of cv variants */
  TYPE_CV_TYPE (ntype) = type;
  TYPE_CV_TYPE (tmp_type) = ntype;

  return ntype;
}




/* Implement direct support for MEMBER_TYPE in GNU C++.
   May need to construct such a type if this is the first use.
   The TYPE is the type of the member.  The DOMAIN is the type
   of the aggregate that the member belongs to.  */

struct type *
lookup_member_type (type, domain)
     struct type *type;
     struct type *domain;
{
  register struct type *mtype;

  mtype = alloc_type (TYPE_OBJFILE (type));
  smash_to_member_type (mtype, domain, type);
  return (mtype);
}

/* Allocate a stub method whose return type is TYPE.  
   This apparently happens for speed of symbol reading, since parsing
   out the arguments to the method is cpu-intensive, the way we are doing
   it.  So, we will fill in arguments later.
   This always returns a fresh type.   */

struct type *
allocate_stub_method (type)
     struct type *type;
{
  struct type *mtype;

  mtype = alloc_type (TYPE_OBJFILE (type));
  TYPE_TARGET_TYPE (mtype) = type;
  /*  _DOMAIN_TYPE (mtype) = unknown yet */
  /*  _ARG_TYPES (mtype) = unknown yet */
  TYPE_FLAGS (mtype) = TYPE_FLAG_STUB;
  TYPE_CODE (mtype) = TYPE_CODE_METHOD;
  TYPE_LENGTH (mtype) = 1;
  return (mtype);
}

/* Create a range type using either a blank type supplied in RESULT_TYPE,
   or creating a new type, inheriting the objfile from INDEX_TYPE.

   Indices will be of type INDEX_TYPE, and will range from LOW_BOUND to
   HIGH_BOUND, inclusive.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into a range type? */

struct type *
create_range_type (result_type, index_type, low_bound, high_bound)
     struct type *result_type;
     struct type *index_type;
     int low_bound;
     int high_bound;
{
  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (index_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_RANGE;
  TYPE_TARGET_TYPE (result_type) = index_type;
  if (TYPE_FLAGS (index_type) & TYPE_FLAG_STUB)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_TARGET_STUB;
  else
    TYPE_LENGTH (result_type) = TYPE_LENGTH (check_typedef (index_type));
  TYPE_NFIELDS (result_type) = 2;
  TYPE_FIELDS (result_type) = (struct field *)
    TYPE_ALLOC (result_type, 2 * sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, 2 * sizeof (struct field));
  TYPE_FIELD_BITPOS (result_type, 0) = low_bound;
  TYPE_FIELD_BITPOS (result_type, 1) = high_bound;
  TYPE_FIELD_TYPE (result_type, 0) = builtin_type_int;		/* FIXME */
  TYPE_FIELD_TYPE (result_type, 1) = builtin_type_int;		/* FIXME */

  if(low_bound >= 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_UNSIGNED;

  return (result_type);
}

/* Set *LOWP and *HIGHP to the lower and upper bounds of discrete type TYPE.
   Return 1 of type is a range type, 0 if it is discrete (and bounds
   will fit in LONGEST), or -1 otherwise. */

int
get_discrete_bounds (type, lowp, highp)
     struct type *type;
     LONGEST *lowp, *highp;
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_RANGE:
      *lowp = TYPE_LOW_BOUND (type);
      *highp = TYPE_HIGH_BOUND (type);
      return 1;
    case TYPE_CODE_ENUM:
      if (TYPE_NFIELDS (type) > 0)
	{
	  /* The enums may not be sorted by value, so search all
	     entries */
	  int i;

	  *lowp = *highp = TYPE_FIELD_BITPOS (type, 0);
	  for (i = 0; i < TYPE_NFIELDS (type); i++)
	    {
	      if (TYPE_FIELD_BITPOS (type, i) < *lowp)
		*lowp = TYPE_FIELD_BITPOS (type, i);
	      if (TYPE_FIELD_BITPOS (type, i) > *highp)
		*highp = TYPE_FIELD_BITPOS (type, i);
	    }

	  /* Set unsigned indicator if warranted. */
	  if(*lowp >= 0)
	    {
	      TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;
	    }
	}
      else
	{
	  *lowp = 0;
	  *highp = -1;
	}
      return 0;
    case TYPE_CODE_BOOL:
      *lowp = 0;
      *highp = 1;
      return 0;
    case TYPE_CODE_INT:
      if (TYPE_LENGTH (type) > sizeof (LONGEST))  /* Too big */
	return -1;
      if (!TYPE_UNSIGNED (type))
	{
	  *lowp = - (1 << (TYPE_LENGTH (type) * TARGET_CHAR_BIT - 1));
	  *highp = -*lowp - 1;
	  return 0;
	}
      /* ... fall through for unsigned ints ... */
    case TYPE_CODE_CHAR:
      *lowp = 0;
      /* This round-about calculation is to avoid shifting by
	 TYPE_LENGTH (type) * TARGET_CHAR_BIT, which will not work
	 if TYPE_LENGTH (type) == sizeof (LONGEST). */
      *highp = 1 << (TYPE_LENGTH (type) * TARGET_CHAR_BIT - 1);
      *highp = (*highp - 1) | *highp;
      return 0;
    default:
      return -1;
    }
}

/* Create an array type using either a blank type supplied in RESULT_TYPE,
   or creating a new type, inheriting the objfile from RANGE_TYPE.

   Elements will be of type ELEMENT_TYPE, the indices will be of type
   RANGE_TYPE.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into an array type? */

struct type *
create_array_type (result_type, element_type, range_type)
     struct type *result_type;
     struct type *element_type;
     struct type *range_type;
{
  LONGEST low_bound, high_bound;

  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (range_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_ARRAY;
  TYPE_TARGET_TYPE (result_type) = element_type;
  if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
    low_bound = high_bound = 0;
  CHECK_TYPEDEF (element_type);
  TYPE_LENGTH (result_type) =
    TYPE_LENGTH (element_type) * (high_bound - low_bound + 1);
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) =
    (struct field *) TYPE_ALLOC (result_type, sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, sizeof (struct field));
  TYPE_FIELD_TYPE (result_type, 0) = range_type;
  TYPE_VPTR_FIELDNO (result_type) = -1;

  /* TYPE_FLAG_TARGET_STUB will take care of zero length arrays */
  if (TYPE_LENGTH (result_type) == 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_TARGET_STUB;

  return (result_type);
}

/* Create a string type using either a blank type supplied in RESULT_TYPE,
   or creating a new type.  String types are similar enough to array of
   char types that we can use create_array_type to build the basic type
   and then bash it into a string type.

   For fixed length strings, the range type contains 0 as the lower
   bound and the length of the string minus one as the upper bound.

   FIXME:  Maybe we should check the TYPE_CODE of RESULT_TYPE to make
   sure it is TYPE_CODE_UNDEF before we bash it into a string type? */

struct type *
create_string_type (result_type, range_type)
     struct type *result_type;
     struct type *range_type;
{
  result_type = create_array_type (result_type,
				   *current_language->string_char_type,
				   range_type);
  TYPE_CODE (result_type) = TYPE_CODE_STRING;
  return (result_type);
}

struct type *
create_set_type (result_type, domain_type)
     struct type *result_type;
     struct type *domain_type;
{
  LONGEST low_bound, high_bound, bit_length;
  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (domain_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_SET;
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) = (struct field *)
    TYPE_ALLOC (result_type, 1 * sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, sizeof (struct field));

  if (! (TYPE_FLAGS (domain_type) & TYPE_FLAG_STUB))
    {
      if (get_discrete_bounds (domain_type, &low_bound, &high_bound) < 0)
	low_bound = high_bound = 0;
      bit_length = high_bound - low_bound + 1;
      TYPE_LENGTH (result_type)
	= (bit_length + TARGET_CHAR_BIT - 1) / TARGET_CHAR_BIT;
    }
  TYPE_FIELD_TYPE (result_type, 0) = domain_type;

  if(low_bound >= 0)
    TYPE_FLAGS (result_type) |= TYPE_FLAG_UNSIGNED;

  return (result_type);
}

/* Smash TYPE to be a type of members of DOMAIN with type TO_TYPE. 
   A MEMBER is a wierd thing -- it amounts to a typed offset into
   a struct, e.g. "an int at offset 8".  A MEMBER TYPE doesn't
   include the offset (that's the value of the MEMBER itself), but does
   include the structure type into which it points (for some reason).

   When "smashing" the type, we preserve the objfile that the
   old type pointed to, since we aren't changing where the type is actually
   allocated.  */

void
smash_to_member_type (type, domain, to_type)
     struct type *type;
     struct type *domain;
     struct type *to_type;
{
  struct objfile *objfile;

  objfile = TYPE_OBJFILE (type);

  memset ((char *) type, 0, sizeof (struct type));
  TYPE_OBJFILE (type) = objfile;
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;
  TYPE_LENGTH (type) = 1;	/* In practice, this is never needed.  */
  TYPE_CODE (type) = TYPE_CODE_MEMBER;
}

/* Smash TYPE to be a type of method of DOMAIN with type TO_TYPE.
   METHOD just means `function that gets an extra "this" argument'.

   When "smashing" the type, we preserve the objfile that the
   old type pointed to, since we aren't changing where the type is actually
   allocated.  */

void
smash_to_method_type (type, domain, to_type, args)
     struct type *type;
     struct type *domain;
     struct type *to_type;
     struct type **args;
{
  struct objfile *objfile;

  objfile = TYPE_OBJFILE (type);

  memset ((char *) type, 0, sizeof (struct type));
  TYPE_OBJFILE (type) = objfile;
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;
  TYPE_ARG_TYPES (type) = args;
  TYPE_LENGTH (type) = 1;	/* In practice, this is never needed.  */
  TYPE_CODE (type) = TYPE_CODE_METHOD;
}

/* Return a typename for a struct/union/enum type without "struct ",
   "union ", or "enum ".  If the type has a NULL name, return NULL.  */

char *
type_name_no_tag (type)
     register const struct type *type;
{
  if (TYPE_TAG_NAME (type) != NULL)
    return TYPE_TAG_NAME (type);

  /* Is there code which expects this to return the name if there is no
     tag name?  My guess is that this is mainly used for C++ in cases where
     the two will always be the same.  */
  return TYPE_NAME (type);
}

/* Lookup a primitive type named NAME. 
   Return zero if NAME is not a primitive type.*/

struct type *
lookup_primitive_typename (name)
     char *name;
{
   struct type ** const *p;

   for (p = current_language -> la_builtin_type_vector; *p != NULL; p++)
     {
       if (STREQ ((**p) -> name, name))
	 {
	   return (**p);
	 }
     }
   return (NULL); 
}

/* Lookup a typedef or primitive type named NAME,
   visible in lexical block BLOCK.
   If NOERR is nonzero, return zero if NAME is not suitably defined.  */

struct type *
lookup_typename (name, block, noerr)
     char *name;
     struct block *block;
     int noerr;
{
  register struct symbol *sym;
  register struct type *tmp;

  sym = lookup_symbol (name, block, VAR_NAMESPACE, 0, (struct symtab **) NULL);
  if (sym == NULL || SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      tmp = lookup_primitive_typename (name);
      if (tmp)
	{
	  return (tmp);
	}
      else if (!tmp && noerr)
	{
	  return (NULL);
	}
      else
	{
	  error ("No type named %s.", name);
	}
    }
  return (SYMBOL_TYPE (sym));
}

struct type *
lookup_unsigned_typename (name)
     char *name;
{
  char *uns = alloca (strlen (name) + 10);

  strcpy (uns, "unsigned ");
  strcpy (uns + 9, name);
  return (lookup_typename (uns, (struct block *) NULL, 0));
}

struct type *
lookup_signed_typename (name)
     char *name;
{
  struct type *t;
  char *uns = alloca (strlen (name) + 8);

  strcpy (uns, "signed ");
  strcpy (uns + 7, name);
  t = lookup_typename (uns, (struct block *) NULL, 1);
  /* If we don't find "signed FOO" just try again with plain "FOO". */
  if (t != NULL)
    return t;
  return lookup_typename (name, (struct block *) NULL, 0);
}

/* Lookup a structure type named "struct NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_struct (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_NAMESPACE, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    {
      error ("No struct type named %s.", name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      error ("This context has class, union or enum %s, not a struct.", name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Lookup a union type named "union NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_union (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym;
  struct type * t;

  sym = lookup_symbol (name, block, STRUCT_NAMESPACE, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    error ("No union type named %s.", name);

  t = SYMBOL_TYPE(sym);

  if (TYPE_CODE (t) == TYPE_CODE_UNION)
    return (t);

  /* C++ unions may come out with TYPE_CODE_CLASS, but we look at
   * a further "declared_type" field to discover it is really a union.
   */
  if (HAVE_CPLUS_STRUCT (t)) 
    if (TYPE_DECLARED_TYPE(t) == DECLARED_TYPE_UNION) 
      return (t);

  /* If we get here, it's not a union */
  error ("This context has class, struct or enum %s, not a union.", name);
}


/* Lookup an enum type named "enum NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_enum (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_NAMESPACE, 0, 
		       (struct symtab **) NULL);
  if (sym == NULL)
    {
      error ("No enum type named %s.", name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_ENUM)
    {
      error ("This context has class, struct or union %s, not an enum.", name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Lookup a template type named "template NAME<TYPE>",
   visible in lexical block BLOCK.  */

struct type *
lookup_template_type (name, type, block)
     char *name;
     struct type *type;
     struct block *block;
{
  struct symbol *sym;
  char *nam = (char*) alloca(strlen(name) + strlen(type->name) + 4);
  strcpy (nam, name);
  strcat (nam, "<");
  strcat (nam, type->name);
  strcat (nam, " >");	/* FIXME, extra space still introduced in gcc? */

  sym = lookup_symbol (nam, block, VAR_NAMESPACE, 0, (struct symtab **)NULL);

  if (sym == NULL)
    {
      error ("No template type named %s.", name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      error ("This context has class, union or enum %s, not a struct.", name);
    }
  return (SYMBOL_TYPE (sym));
}

/* Given a type TYPE, lookup the type of the component of type named NAME.  

   TYPE can be either a struct or union, or a pointer or reference to a struct or
   union.  If it is a pointer or reference, its target type is automatically used.
   Thus '.' and '->' are interchangable, as specified for the definitions of the
   expression element types STRUCTOP_STRUCT and STRUCTOP_PTR.

   If NOERR is nonzero, return zero if NAME is not suitably defined.
   If NAME is the name of a baseclass type, return that type.  */

struct type *
lookup_struct_elt_type (type, name, noerr)
     struct type *type;
     char *name;
    int noerr;
{
  int i;

  for (;;)
    {
      CHECK_TYPEDEF (type);
      if (TYPE_CODE (type) != TYPE_CODE_PTR
	  && TYPE_CODE (type) != TYPE_CODE_REF)
	break;
      type = TYPE_TARGET_TYPE (type);
    }

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT &&
      TYPE_CODE (type) != TYPE_CODE_UNION)
    {
      target_terminal_ours ();
      gdb_flush (gdb_stdout);
      fprintf_unfiltered (gdb_stderr, "Type ");
      type_print (type, "", gdb_stderr, -1);
      error (" is not a structure or union type.");
    }

#if 0
  /* FIXME:  This change put in by Michael seems incorrect for the case where
     the structure tag name is the same as the member name.  I.E. when doing
     "ptype bell->bar" for "struct foo { int bar; int foo; } bell;"
     Disabled by fnf. */
  {
    char *typename;

    typename = type_name_no_tag (type);
    if (typename != NULL && STREQ (typename, name))
      return type;
  }
#endif

  for (i = TYPE_NFIELDS (type) - 1; i >= TYPE_N_BASECLASSES (type); i--)
    {
      char *t_field_name = TYPE_FIELD_NAME (type, i);

      if (t_field_name && STREQ (t_field_name, name))
	{
	  return TYPE_FIELD_TYPE (type, i);
	}
    }

  /* OK, it's not in this class.  Recursively check the baseclasses.  */
  for (i = TYPE_N_BASECLASSES (type) - 1; i >= 0; i--)
    {
      struct type *t;

      t = lookup_struct_elt_type (TYPE_BASECLASS (type, i), name, noerr);
      if (t != NULL)
	{
	  return t;
	}
    }

  if (noerr)
    {
      return NULL;
    }
  
  target_terminal_ours ();
  gdb_flush (gdb_stdout);
  fprintf_unfiltered (gdb_stderr, "Type ");
  type_print (type, "", gdb_stderr, -1);
  fprintf_unfiltered (gdb_stderr, " has no component named ");
  fputs_filtered (name, gdb_stderr);
  error (".");
  return (struct type *)-1;	/* For lint */
}

/* If possible, make the vptr_fieldno and vptr_basetype fields of TYPE
   valid.  Callers should be aware that in some cases (for example,
   the type or one of its baseclasses is a stub type and we are
   debugging a .o file), this function will not be able to find the virtual
   function table pointer, and vptr_fieldno will remain -1 and vptr_basetype
   will remain NULL.  */

void
fill_in_vptr_fieldno (type)
     struct type *type;
{
  CHECK_TYPEDEF (type);

  if (TYPE_VPTR_FIELDNO (type) < 0)
    {
      int i;

      /* We must start at zero in case the first (and only) baseclass is
	 virtual (and hence we cannot share the table pointer).  */
      for (i = 0; i < TYPE_N_BASECLASSES (type); i++)
	{
	  fill_in_vptr_fieldno (TYPE_BASECLASS (type, i));
	  if (TYPE_VPTR_FIELDNO (TYPE_BASECLASS (type, i)) >= 0)
	    {
	      TYPE_VPTR_FIELDNO (type)
		= TYPE_VPTR_FIELDNO (TYPE_BASECLASS (type, i));
	      TYPE_VPTR_BASETYPE (type)
		= TYPE_VPTR_BASETYPE (TYPE_BASECLASS (type, i));
	      break;
	    }
	}
    }
}

/* Find the method and field indices for the destructor in class type T.
   Return 1 if the destructor was found, otherwise, return 0.  */

int
get_destructor_fn_field (t, method_indexp, field_indexp)
     struct type *t;
     int *method_indexp;
     int *field_indexp;
{
  int i;

  for (i = 0; i < TYPE_NFN_FIELDS (t); i++)
    {
      int j;
      struct fn_field *f = TYPE_FN_FIELDLIST1 (t, i);

      for (j = 0; j < TYPE_FN_FIELDLIST_LENGTH (t, i); j++)
	{
	  if (DESTRUCTOR_PREFIX_P (TYPE_FN_FIELD_PHYSNAME (f, j)))
	    {
	      *method_indexp = i;
	      *field_indexp = j;
	      return 1;
	    }
	}
    }
  return 0;
}

/* Added by Bryan Boreham, Kewill, Sun Sep 17 18:07:17 1989.

   If this is a stubbed struct (i.e. declared as struct foo *), see if
   we can find a full definition in some other file. If so, copy this
   definition, so we can use it in future.  There used to be a comment (but
   not any code) that if we don't find a full definition, we'd set a flag
   so we don't spend time in the future checking the same type.  That would
   be a mistake, though--we might load in more symbols which contain a
   full definition for the type.

   This used to be coded as a macro, but I don't think it is called 
   often enough to merit such treatment.  */

struct complaint stub_noname_complaint =
  {"stub type has NULL name", 0, 0};

struct type *
check_typedef (type)
     register struct type *type;
{
  struct type *orig_type = type;
  while (TYPE_CODE (type) == TYPE_CODE_TYPEDEF)
    {
      if (!TYPE_TARGET_TYPE (type))
	{
	  char* name;
	  struct symbol *sym;

	  /* It is dangerous to call lookup_symbol if we are currently
	     reading a symtab.  Infinite recursion is one danger. */
	  if (currently_reading_symtab)
	    return type;

	  name = type_name_no_tag (type);
	  /* FIXME: shouldn't we separately check the TYPE_NAME and the
	     TYPE_TAG_NAME, and look in STRUCT_NAMESPACE and/or VAR_NAMESPACE
	     as appropriate?  (this code was written before TYPE_NAME and
	     TYPE_TAG_NAME were separate).  */
	  if (name == NULL)
	    {
	      complain (&stub_noname_complaint);
	      return type;
	    }
	  sym = lookup_symbol (name, 0, STRUCT_NAMESPACE, 0, 
			       (struct symtab **) NULL);
	  if (sym)
	    TYPE_TARGET_TYPE (type) = SYMBOL_TYPE (sym);
	  else
	    TYPE_TARGET_TYPE (type) = alloc_type (NULL);  /* TYPE_CODE_UNDEF */
	}
      type = TYPE_TARGET_TYPE (type);
    }

  /* If this is a struct/class/union with no fields, then check whether a
     full definition exists somewhere else.  This is for systems where a
     type definition with no fields is issued for such types, instead of
     identifying them as stub types in the first place */ 
     
  if (TYPE_IS_OPAQUE (type) && opaque_type_resolution && !currently_reading_symtab)
    {
      char * name = type_name_no_tag (type);
      struct type * newtype;
      if (name == NULL)
	{
	  complain (&stub_noname_complaint);
	  return type;
	}
      newtype = lookup_transparent_type (name);
      if (newtype)
	{
	  memcpy ((char *) type, (char *) newtype, sizeof (struct type));
	}
    }
  /* Otherwise, rely on the stub flag being set for opaque/stubbed types */
  else if ((TYPE_FLAGS(type) & TYPE_FLAG_STUB) && ! currently_reading_symtab)
    {
      char* name = type_name_no_tag (type);
      /* FIXME: shouldn't we separately check the TYPE_NAME and the
	 TYPE_TAG_NAME, and look in STRUCT_NAMESPACE and/or VAR_NAMESPACE
	 as appropriate?  (this code was written before TYPE_NAME and
	 TYPE_TAG_NAME were separate).  */
      struct symbol *sym;
      if (name == NULL)
	{
	  complain (&stub_noname_complaint);
	  return type;
	}
      sym = lookup_symbol (name, 0, STRUCT_NAMESPACE, 0, (struct symtab **) NULL);
      if (sym)
	{
	  memcpy ((char *)type, (char *)SYMBOL_TYPE(sym), sizeof (struct type));
	}
    }

  if (TYPE_FLAGS (type) & TYPE_FLAG_TARGET_STUB)
    {
      struct type *range_type;
      struct type *target_type = check_typedef (TYPE_TARGET_TYPE (type));

      if (TYPE_FLAGS (target_type) & (TYPE_FLAG_STUB | TYPE_FLAG_TARGET_STUB))
	{ }
      else if (TYPE_CODE (type) == TYPE_CODE_ARRAY
	       && TYPE_NFIELDS (type) == 1
	       && (TYPE_CODE (range_type = TYPE_FIELD_TYPE (type, 0))
		   == TYPE_CODE_RANGE))
	{
	  /* Now recompute the length of the array type, based on its
	     number of elements and the target type's length.  */
	  TYPE_LENGTH (type) =
	    ((TYPE_FIELD_BITPOS (range_type, 1)
	      - TYPE_FIELD_BITPOS (range_type, 0)
	      + 1)
	     * TYPE_LENGTH (target_type));
	  TYPE_FLAGS (type) &= ~TYPE_FLAG_TARGET_STUB;
	}
      else if (TYPE_CODE (type) == TYPE_CODE_RANGE)
	{
	  TYPE_LENGTH (type) = TYPE_LENGTH (target_type);
	  TYPE_FLAGS (type) &= ~TYPE_FLAG_TARGET_STUB;
	}
    }
  /* Cache TYPE_LENGTH for future use. */
  TYPE_LENGTH (orig_type) = TYPE_LENGTH (type);
  return type;
}

/* New code added to support parsing of Cfront stabs strings */
#include <ctype.h>
#define INIT_EXTRA { pextras->len=0; pextras->str[0]='\0'; }
#define ADD_EXTRA(c) { pextras->str[pextras->len++]=c; }

static void 
add_name(pextras,n) 
  struct extra * pextras;
  char * n; 
{
  int nlen;

  if ((nlen = (n ? strlen(n) : 0))==0) 
    return;
  sprintf(pextras->str+pextras->len,"%d%s",nlen,n);
  pextras->len=strlen(pextras->str);
}

static void 
add_mangled_type(pextras,t) 
  struct extra * pextras;
  struct type * t;
{
  enum type_code tcode;
  int tlen, tflags;
  char * tname;

  tcode = TYPE_CODE(t);
  tlen = TYPE_LENGTH(t);
  tflags = TYPE_FLAGS(t);
  tname = TYPE_NAME(t);
  /* args of "..." seem to get mangled as "e" */

  switch (tcode) 
    {
      case TYPE_CODE_INT: 
        if (tflags==1)
          ADD_EXTRA('U');
        switch (tlen) 
          {
            case 1:
              ADD_EXTRA('c');
              break;
            case 2:
              ADD_EXTRA('s');
              break;
            case 4: 
              {
              char* pname;
              if ((pname=strrchr(tname,'l'),pname) && !strcmp(pname,"long"))
                ADD_EXTRA('l')
              else
                ADD_EXTRA('i')
              }
              break;
            default: 
              {
          
                static struct complaint msg = {"Bad int type code length x%x\n",0,0};
          
                complain (&msg, tlen);
          
              }
          }
        break;
      case TYPE_CODE_FLT: 
          switch (tlen) 
            {
              case 4:
                ADD_EXTRA('f');
                break;
              case 8:
                ADD_EXTRA('d');
                break;
              case 16:
                ADD_EXTRA('r');
                break;
              default: 
	 	{
                  static struct complaint msg = {"Bad float type code length x%x\n",0,0};
          	  complain (&msg, tlen);
            	}
      	      }
            break;
      case TYPE_CODE_REF:
        ADD_EXTRA('R');
        /* followed by what it's a ref to */
        break;
      case TYPE_CODE_PTR:
        ADD_EXTRA('P');
        /* followed by what it's a ptr to */
        break;
      case TYPE_CODE_TYPEDEF: 
        {
          static struct complaint msg = {"Typedefs in overloaded functions not yet supported\n",0,0};
          complain (&msg);
        }
      /* followed by type bytes & name */
      break;
    case TYPE_CODE_FUNC:
      ADD_EXTRA('F');
      /* followed by func's arg '_' & ret types */
      break;
    case TYPE_CODE_VOID:
      ADD_EXTRA('v');
      break;
    case TYPE_CODE_METHOD:
      ADD_EXTRA('M');
      /* followed by name of class and func's arg '_' & ret types */
      add_name(pextras,tname);
      ADD_EXTRA('F');  /* then mangle function */
      break;
    case TYPE_CODE_STRUCT: /* C struct */
    case TYPE_CODE_UNION:  /* C union */
    case TYPE_CODE_ENUM:   /* Enumeration type */
      /* followed by name of type */
      add_name(pextras,tname);
      break;

    /* errors possible types/not supported */
    case TYPE_CODE_CHAR:              
    case TYPE_CODE_ARRAY:  /* Array type */
    case TYPE_CODE_MEMBER: /* Member type */
    case TYPE_CODE_BOOL:
    case TYPE_CODE_COMPLEX:            /* Complex float */
    case TYPE_CODE_UNDEF:
    case TYPE_CODE_SET:                /* Pascal sets */
    case TYPE_CODE_RANGE:  
    case TYPE_CODE_STRING:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_ERROR:
    default: 
      {
        static struct complaint msg = {"Unknown type code x%x\n",0,0};
        complain (&msg, tcode);
      }
    }
  if (t->target_type)
    add_mangled_type(pextras,t->target_type);
}

#if 0
void
cfront_mangle_name(type, i, j)
     struct type *type;
     int i;
     int j;
{
   struct fn_field *f;
   char *mangled_name = gdb_mangle_name (type, i, j);

   f = TYPE_FN_FIELDLIST1 (type, i);	/* moved from below */

   /* kludge to support cfront methods - gdb expects to find "F" for 
      ARM_mangled names, so when we mangle, we have to add it here */
   if (ARM_DEMANGLING) 
     {
	int k;
	char * arm_mangled_name;
	struct fn_field *method = &f[j];
	char *field_name = TYPE_FN_FIELDLIST_NAME (type, i);
        char *physname = TYPE_FN_FIELD_PHYSNAME (f, j);
        char *newname = type_name_no_tag (type);

        struct type *ftype = TYPE_FN_FIELD_TYPE (f, j);
	int nargs = TYPE_NFIELDS(ftype);	/* number of args */
	struct extra extras, * pextras = &extras;	
	INIT_EXTRA

	if (TYPE_FN_FIELD_STATIC_P (f, j))	/* j for sublist within this list */
	  ADD_EXTRA('S')
	ADD_EXTRA('F')
	/* add args here! */
	if (nargs <= 1)				/* no args besides this */
		ADD_EXTRA('v')
	else {
	  for (k=1; k<nargs; k++) 
	    {
	      struct type * t;
	      t = TYPE_FIELD_TYPE(ftype,k);
	      add_mangled_type(pextras,t);
	    }
	}
	ADD_EXTRA('\0')
	printf("add_mangled_type: %s\n",extras.str); /* FIXME */
	arm_mangled_name = malloc(strlen(mangled_name)+extras.len);
        sprintf(arm_mangled_name,"%s%s",mangled_name,extras.str);
	free(mangled_name);
	mangled_name = arm_mangled_name;
     }
}
#endif	/* 0 */

#undef ADD_EXTRA
/* End of new code added to support parsing of Cfront stabs strings */

/* Ugly hack to convert method stubs into method types.

   He ain't kiddin'.  This demangles the name of the method into a string
   including argument types, parses out each argument type, generates
   a string casting a zero to that type, evaluates the string, and stuffs
   the resulting type into an argtype vector!!!  Then it knows the type
   of the whole function (including argument types for overloading),
   which info used to be in the stab's but was removed to hack back
   the space required for them.  */

void
check_stub_method (type, method_id, signature_id)
     struct type *type;
     int method_id;
     int signature_id;
{
  struct fn_field *f;
  char *mangled_name = gdb_mangle_name (type, method_id, signature_id);
  char *demangled_name = cplus_demangle (mangled_name,
					 DMGL_PARAMS | DMGL_ANSI);
  char *argtypetext, *p;
  int depth = 0, argcount = 1;
  struct type **argtypes;
  struct type *mtype;

  /* Make sure we got back a function string that we can use.  */
  if (demangled_name)
    p = strchr (demangled_name, '(');

  if (demangled_name == NULL || p == NULL)
    error ("Internal: Cannot demangle mangled name `%s'.", mangled_name);

  /* Now, read in the parameters that define this type.  */
  p += 1;
  argtypetext = p;
  while (*p)
    {
      if (*p == '(')
	{
	  depth += 1;
	}
      else if (*p == ')')
	{
	  depth -= 1;
	}
      else if (*p == ',' && depth == 0)
	{
	  argcount += 1;
	}

      p += 1;
    }

  /* We need two more slots: one for the THIS pointer, and one for the
     NULL [...] or void [end of arglist].  */

  argtypes = (struct type **)
    TYPE_ALLOC (type, (argcount + 2) * sizeof (struct type *));
  p = argtypetext;
  /* FIXME: This is wrong for static member functions.  */
  argtypes[0] = lookup_pointer_type (type);
  argcount = 1;

  if (*p != ')')			/* () means no args, skip while */
    {
      depth = 0;
      while (*p)
	{
	  if (depth <= 0 && (*p == ',' || *p == ')'))
	    {
	      /* Avoid parsing of ellipsis, they will be handled below.  */
	      if (strncmp (argtypetext, "...", p - argtypetext) != 0)
		{
		  argtypes[argcount] =
		      parse_and_eval_type (argtypetext, p - argtypetext);
		  argcount += 1;
		}
	      argtypetext = p + 1;
	    }

	  if (*p == '(')
	    {
	      depth += 1;
	    }
	  else if (*p == ')')
	    {
	      depth -= 1;
	    }

	  p += 1;
	}
    }

  if (p[-2] != '.')			/* Not '...' */
    {
      argtypes[argcount] = builtin_type_void;	/* List terminator */
    }
  else
    {
      argtypes[argcount] = NULL;		/* Ellist terminator */
    }

  free (demangled_name);

  f = TYPE_FN_FIELDLIST1 (type, method_id);	

  TYPE_FN_FIELD_PHYSNAME (f, signature_id) = mangled_name;

  /* Now update the old "stub" type into a real type.  */
  mtype = TYPE_FN_FIELD_TYPE (f, signature_id);
  TYPE_DOMAIN_TYPE (mtype) = type;
  TYPE_ARG_TYPES (mtype) = argtypes;
  TYPE_FLAGS (mtype) &= ~TYPE_FLAG_STUB;
  TYPE_FN_FIELD_STUB (f, signature_id) = 0;
}

const struct cplus_struct_type cplus_struct_default;

void
allocate_cplus_struct_type (type)
     struct type *type;
{
  if (!HAVE_CPLUS_STRUCT (type))
    {
      TYPE_CPLUS_SPECIFIC (type) = (struct cplus_struct_type *)
	TYPE_ALLOC (type, sizeof (struct cplus_struct_type));
      *(TYPE_CPLUS_SPECIFIC(type)) = cplus_struct_default;
    }
}

/* Helper function to initialize the standard scalar types.

   If NAME is non-NULL and OBJFILE is non-NULL, then we make a copy
   of the string pointed to by name in the type_obstack for that objfile,
   and initialize the type name to that copy.  There are places (mipsread.c
   in particular, where init_type is called with a NULL value for NAME). */

struct type *
init_type (code, length, flags, name, objfile)
     enum type_code code;
     int length;
     int flags;
     char *name;
     struct objfile *objfile;
{
  register struct type *type;

  type = alloc_type (objfile);
  TYPE_CODE (type) = code;
  TYPE_LENGTH (type) = length;
  TYPE_FLAGS (type) |= flags;
  if ((name != NULL) && (objfile != NULL))
    {
      TYPE_NAME (type) =
	obsavestring (name, strlen (name), &objfile -> type_obstack);
    }
  else
    {
      TYPE_NAME (type) = name;
    }

  /* C++ fancies.  */

  if (code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
    {
      INIT_CPLUS_SPECIFIC (type);
    }
  return (type);
}

/* Look up a fundamental type for the specified objfile.
   May need to construct such a type if this is the first use.

   Some object file formats (ELF, COFF, etc) do not define fundamental
   types such as "int" or "double".  Others (stabs for example), do
   define fundamental types.

   For the formats which don't provide fundamental types, gdb can create
   such types, using defaults reasonable for the current language and
   the current target machine.

   NOTE:  This routine is obsolescent.  Each debugging format reader
   should manage it's own fundamental types, either creating them from
   suitable defaults or reading them from the debugging information,
   whichever is appropriate.  The DWARF reader has already been
   fixed to do this.  Once the other readers are fixed, this routine
   will go away.  Also note that fundamental types should be managed
   on a compilation unit basis in a multi-language environment, not
   on a linkage unit basis as is done here. */


struct type *
lookup_fundamental_type (objfile, typeid)
     struct objfile *objfile;
     int typeid;
{
  register struct type **typep;
  register int nbytes;

  if (typeid < 0 || typeid >= FT_NUM_MEMBERS)
    {
      error ("internal error - invalid fundamental type id %d", typeid);
    }

  /* If this is the first time we need a fundamental type for this objfile
     then we need to initialize the vector of type pointers. */
  
  if (objfile -> fundamental_types == NULL)
    {
      nbytes = FT_NUM_MEMBERS * sizeof (struct type *);
      objfile -> fundamental_types = (struct type **)
	obstack_alloc (&objfile -> type_obstack, nbytes);
      memset ((char *) objfile -> fundamental_types, 0, nbytes);
      OBJSTAT (objfile, n_types += FT_NUM_MEMBERS);
    }

  /* Look for this particular type in the fundamental type vector.  If one is
     not found, create and install one appropriate for the current language. */

  typep = objfile -> fundamental_types + typeid;
  if (*typep == NULL)
    {
      *typep = create_fundamental_type (objfile, typeid);
    }

  return (*typep);
}

int
can_dereference (t)
     struct type *t;
{
  /* FIXME: Should we return true for references as well as pointers?  */
  CHECK_TYPEDEF (t);
  return
    (t != NULL
     && TYPE_CODE (t) == TYPE_CODE_PTR
     && TYPE_CODE (TYPE_TARGET_TYPE (t)) != TYPE_CODE_VOID);
}

/* Chill varying string and arrays are represented as follows:

   struct { int __var_length; ELEMENT_TYPE[MAX_SIZE] __var_data};

   Return true if TYPE is such a Chill varying type. */

int
chill_varying_type (type)
     struct type *type;
{
  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      || TYPE_NFIELDS (type) != 2
      || strcmp (TYPE_FIELD_NAME (type, 0), "__var_length") != 0)
    return 0;
  return 1;
}

/* Check whether BASE is an ancestor or base class or DCLASS 
   Return 1 if so, and 0 if not.
   Note: callers may want to check for identity of the types before
   calling this function -- identical types are considered to satisfy
   the ancestor relationship even if they're identical */

int
is_ancestor (base, dclass)
  struct type * base;
  struct type * dclass;
{
  int i;
  
  CHECK_TYPEDEF (base);
  CHECK_TYPEDEF (dclass);

  if (base == dclass)
    return 1;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    if (is_ancestor (base, TYPE_BASECLASS (dclass, i)))
      return 1;

  return 0;
}



/* See whether DCLASS has a virtual table.  This routine is aimed at
   the HP/Taligent ANSI C++ runtime model, and may not work with other
   runtime models.  Return 1 => Yes, 0 => No.  */

int
has_vtable (dclass)
  struct type * dclass;
{
  /* In the HP ANSI C++ runtime model, a class has a vtable only if it
     has virtual functions or virtual bases.  */

  register int i;

  if (TYPE_CODE(dclass) != TYPE_CODE_CLASS)
    return 0;
  
  /* First check for the presence of virtual bases */
  if (TYPE_FIELD_VIRTUAL_BITS(dclass))
    for (i=0; i < TYPE_N_BASECLASSES(dclass); i++)
      if (B_TST(TYPE_FIELD_VIRTUAL_BITS(dclass), i))
        return 1;
  
  /* Next check for virtual functions */
  if (TYPE_FN_FIELDLISTS(dclass))
    for (i=0; i < TYPE_NFN_FIELDS(dclass); i++)
      if (TYPE_FN_FIELD_VIRTUAL_P(TYPE_FN_FIELDLIST1(dclass, i), 0))
        return 1;

  /* Recurse on non-virtual bases to see if any of them needs a vtable */ 
  if (TYPE_FIELD_VIRTUAL_BITS(dclass))
    for (i=0; i < TYPE_N_BASECLASSES(dclass); i++)
      if ((!B_TST (TYPE_FIELD_VIRTUAL_BITS(dclass), i)) &&
	  (has_vtable (TYPE_FIELD_TYPE(dclass, i))))
	return 1;
  
  /* Well, maybe we don't need a virtual table */ 
  return 0;
}

/* Return a pointer to the "primary base class" of DCLASS.
 
   A NULL return indicates that DCLASS has no primary base, or that it
   couldn't be found (insufficient information).
  
   This routine is aimed at the HP/Taligent ANSI C++ runtime model,
   and may not work with other runtime models.  */

struct type *
primary_base_class (dclass)
  struct type * dclass;
{
  /* In HP ANSI C++'s runtime model, a "primary base class" of a class
     is the first directly inherited, non-virtual base class that
     requires a virtual table */

  register int i;

  if (TYPE_CODE(dclass) != TYPE_CODE_CLASS)
    return NULL;

  for (i=0; i < TYPE_N_BASECLASSES(dclass); i++)
    if (!TYPE_FIELD_VIRTUAL(dclass, i) &&
        has_vtable(TYPE_FIELD_TYPE(dclass, i)))
      return TYPE_FIELD_TYPE(dclass, i);

  return NULL;
}

/* Global manipulated by virtual_base_list[_aux]() */

static struct vbase * current_vbase_list = NULL;

/* Return a pointer to a null-terminated list of struct vbase
   items. The vbasetype pointer of each item in the list points to the
   type information for a virtual base of the argument DCLASS.
  
   Helper function for virtual_base_list(). 
   Note: the list goes backward, right-to-left. virtual_base_list()
   copies the items out in reverse order.  */

struct vbase *
virtual_base_list_aux (dclass)
  struct type * dclass;
{
  struct vbase * tmp_vbase;
  register int i;

  if (TYPE_CODE(dclass) != TYPE_CODE_CLASS)
    return NULL;

  for (i = 0; i < TYPE_N_BASECLASSES (dclass); i++)
    {
      /* Recurse on this ancestor, first */
      virtual_base_list_aux(TYPE_FIELD_TYPE(dclass, i));

      /* If this current base is itself virtual, add it to the list */
      if (BASETYPE_VIA_VIRTUAL(dclass, i))
        {
          struct type * basetype = TYPE_FIELD_TYPE (dclass, i);
          
          /* Check if base already recorded */
          tmp_vbase = current_vbase_list;
          while (tmp_vbase)
            {
              if (tmp_vbase->vbasetype == basetype)
                break; /* found it */
              tmp_vbase = tmp_vbase->next;
            }

          if (!tmp_vbase) /* normal exit from loop */
            {
              /* Allocate new item for this virtual base */
              tmp_vbase = (struct vbase *) xmalloc (sizeof (struct vbase));

              /* Stick it on at the end of the list */
              tmp_vbase->vbasetype = basetype;
              tmp_vbase->next = current_vbase_list;
              current_vbase_list = tmp_vbase;
            }
        } /* if virtual */
    } /* for loop over bases */
}


/* Compute the list of virtual bases in the right order.  Virtual
   bases are laid out in the object's memory area in order of their
   occurrence in a depth-first, left-to-right search through the
   ancestors.
  
   Argument DCLASS is the type whose virtual bases are required.
   Return value is the address of a null-terminated array of pointers
   to struct type items.
   
   This routine is aimed at the HP/Taligent ANSI C++ runtime model,
   and may not work with other runtime models.
  
   This routine merely hands off the argument to virtual_base_list_aux()
   and then copies the result into an array to save space.  */

struct type **
virtual_base_list (dclass)
  struct type * dclass;
{
  register struct vbase * tmp_vbase;
  register struct vbase * tmp_vbase_2;
  register int i;
  int count;
  struct type ** vbase_array;

  current_vbase_list = NULL;
  virtual_base_list_aux(dclass);

  for (i=0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; i++, tmp_vbase = tmp_vbase->next)
    /* no body */ ;

  count = i;

  vbase_array = (struct type **) xmalloc((count + 1) * sizeof (struct type *));

  for (i=count -1, tmp_vbase = current_vbase_list; i >= 0; i--, tmp_vbase = tmp_vbase->next)
    vbase_array[i] = tmp_vbase->vbasetype;

  /* Get rid of constructed chain */
  tmp_vbase_2 = tmp_vbase = current_vbase_list;
  while (tmp_vbase)
    {
      tmp_vbase = tmp_vbase->next;
      free(tmp_vbase_2);
      tmp_vbase_2 = tmp_vbase;
    }
  
  vbase_array[count] = NULL;
  return vbase_array;
}

/* Return the length of the virtual base list of the type DCLASS.  */

int
virtual_base_list_length (dclass)
  struct type * dclass;
{
  register int i;
  register struct vbase * tmp_vbase;
  
  current_vbase_list = NULL;
  virtual_base_list_aux(dclass);

  for (i=0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; i++, tmp_vbase = tmp_vbase->next)
    /* no body */ ;
  return i;
}

/* Return the number of elements of the virtual base list of the type
   DCLASS, ignoring those appearing in the primary base (and its
   primary base, recursively).  */

int
virtual_base_list_length_skip_primaries (dclass)
  struct type * dclass;
{
  register int i;
  register struct vbase * tmp_vbase;
  struct type * primary;

  primary = TYPE_RUNTIME_PTR (dclass) ? TYPE_PRIMARY_BASE (dclass) : NULL;

  if (!primary)
    return virtual_base_list_length (dclass);

  current_vbase_list = NULL;
  virtual_base_list_aux(dclass);

  for (i=0, tmp_vbase = current_vbase_list; tmp_vbase != NULL; tmp_vbase = tmp_vbase->next)
    {
      if (virtual_base_index (tmp_vbase->vbasetype, primary) >= 0)
        continue;
      i++;
    }
  return i;
}


/* Return the index (position) of type BASE, which is a virtual base
   class of DCLASS, in the latter's virtual base list.  A return of -1
   indicates "not found" or a problem.  */

int
virtual_base_index(base, dclass)
  struct type * base;
  struct type * dclass;
{
  register struct type * vbase;
  register int i;

  if ((TYPE_CODE(dclass) != TYPE_CODE_CLASS) ||
      (TYPE_CODE(base) != TYPE_CODE_CLASS))
    return -1;

  i = 0;
  vbase = TYPE_VIRTUAL_BASE_LIST(dclass)[0];
  while (vbase)
    {
      if (vbase == base)
        break;
      vbase = TYPE_VIRTUAL_BASE_LIST(dclass)[++i];
    }

  return vbase ? i : -1;
}



/* Return the index (position) of type BASE, which is a virtual base
   class of DCLASS, in the latter's virtual base list. Skip over all
   bases that may appear in the virtual base list of the primary base
   class of DCLASS (recursively).  A return of -1 indicates "not
   found" or a problem.  */

int
virtual_base_index_skip_primaries(base, dclass)
  struct type * base;
  struct type * dclass;
{
  register struct type * vbase;
  register int i, j;
  struct type * primary;

  if ((TYPE_CODE(dclass) != TYPE_CODE_CLASS) ||
      (TYPE_CODE(base) != TYPE_CODE_CLASS))
    return -1;

  primary = TYPE_RUNTIME_PTR(dclass) ? TYPE_PRIMARY_BASE(dclass) : NULL;

  j = -1;
  i = 0;
  vbase = TYPE_VIRTUAL_BASE_LIST(dclass)[0];
  while (vbase)
    {
      if (!primary || (virtual_base_index_skip_primaries(vbase, primary) < 0))
        j++;
      if (vbase == base)
        break;
      vbase = TYPE_VIRTUAL_BASE_LIST(dclass)[++i];
    }

  return vbase ? j : -1;
}

/* Return position of a derived class DCLASS in the list of
 * primary bases starting with the remotest ancestor.
 * Position returned is 0-based. */

int
class_index_in_primary_list (dclass)
  struct type * dclass;
{
  struct type * pbc; /* primary base class */

  /* Simply recurse on primary base */ 
  pbc = TYPE_PRIMARY_BASE (dclass);
  if (pbc)
    return 1 + class_index_in_primary_list (pbc);
  else
    return 0;
}

/* Return a count of the number of virtual functions a type has.
 * This includes all the virtual functions it inherits from its
 * base classes too.
 */

/* pai: FIXME This doesn't do the right thing: count redefined virtual
 * functions only once (latest redefinition)
 */

int
count_virtual_fns (dclass)
  struct type * dclass;
{
  int base;     /* index for base classes */
  int fn, oi;   /* function and overloaded instance indices */
  
  int vfuncs;   /* count to return */ 

  /* recurse on bases that can share virtual table */ 
  struct type * pbc = primary_base_class (dclass);
  if (pbc)
    vfuncs = count_virtual_fns (pbc);
  
  for (fn = 0; fn < TYPE_NFN_FIELDS (dclass); fn++)
    for (oi = 0; oi < TYPE_FN_FIELDLIST_LENGTH (dclass, fn); oi++)
      if (TYPE_FN_FIELD_VIRTUAL_P (TYPE_FN_FIELDLIST1 (dclass, fn), oi))
        vfuncs++;

  return vfuncs;
}



/* Functions for overload resolution begin here */

/* Compare two badness vectors A and B and return the result.
 * 0 => A and B are identical
 * 1 => A and B are incomparable
 * 2 => A is better than B
 * 3 => A is worse than B */

int
compare_badness (a, b)
  struct badness_vector * a;
  struct badness_vector * b;
{
  int i;
  int tmp;
  short found_pos = 0;      /* any positives in c? */
  short found_neg = 0;      /* any negatives in c? */
  
  /* differing lengths => incomparable */ 
  if (a->length != b->length)
    return 1;

  /* Subtract b from a */ 
  for (i=0; i < a->length; i++)
    {
      tmp = a->rank[i] - b->rank[i];
      if (tmp > 0)
        found_pos = 1;
      else if (tmp < 0)
        found_neg = 1;
    }

  if (found_pos)
    {
      if (found_neg)
        return 1; /* incomparable */ 
      else
        return 3; /* A > B */ 
    }
  else /* no positives */ 
    {
      if (found_neg)
        return 2; /* A < B */
      else
        return 0; /* A == B */
    }
}

/* Rank a function by comparing its parameter types (PARMS, length NPARMS),
 * to the types of an argument list (ARGS, length NARGS).
 * Return a pointer to a badness vector. This has NARGS + 1 entries. */

struct badness_vector *
rank_function (parms, nparms, args, nargs)
  struct type ** parms;
  int nparms;
  struct type ** args;
  int nargs;
{
  int i;
  struct badness_vector * bv;
  int min_len = nparms < nargs ? nparms : nargs;

  bv = xmalloc (sizeof (struct badness_vector));
  bv->length = nargs + 1; /* add 1 for the length-match rank */ 
  bv->rank = xmalloc ((nargs + 1) * sizeof (int));

  /* First compare the lengths of the supplied lists.
   * If there is a mismatch, set it to a high value. */
   
  /* pai/1997-06-03 FIXME: when we have debug info about default
   * arguments and ellipsis parameter lists, we should consider those
   * and rank the length-match more finely. */

  LENGTH_MATCH (bv) = (nargs != nparms) ? LENGTH_MISMATCH_BADNESS : 0;

  /* Now rank all the parameters of the candidate function */
  for (i=1; i <= min_len; i++)
    bv->rank[i] = rank_one_type (parms[i-1], args[i-1]);

  /* If more arguments than parameters, add dummy entries */ 
  for (i = min_len +1; i <= nargs; i++)
    bv->rank[i] = TOO_FEW_PARAMS_BADNESS;

  return bv;
}

/* Compare one type (PARM) for compatibility with another (ARG).
 * PARM is intended to be the parameter type of a function; and
 * ARG is the supplied argument's type.  This function tests if
 * the latter can be converted to the former.
 *
 * Return 0 if they are identical types;
 * Otherwise, return an integer which corresponds to how compatible
 * PARM is to ARG. The higher the return value, the worse the match.
 * Generally the "bad" conversions are all uniformly assigned a 100 */

int
rank_one_type (parm, arg)
  struct type * parm;
  struct type * arg;
{
  /* Identical type pointers */
  /* However, this still doesn't catch all cases of same type for arg
   * and param. The reason is that builtin types are different from
   * the same ones constructed from the object. */
  if (parm == arg)
    return 0;

  /* Resolve typedefs */
  if (TYPE_CODE (parm) == TYPE_CODE_TYPEDEF)
    parm = check_typedef (parm);
  if (TYPE_CODE (arg) == TYPE_CODE_TYPEDEF)
    arg = check_typedef (arg);

  /* Check if identical after resolving typedefs */
  if (parm == arg)
    return 0;

#if 0
  /* Debugging only */ 
  printf("------ Arg is %s [%d], parm is %s [%d]\n",
         TYPE_NAME (arg), TYPE_CODE (arg), TYPE_NAME (parm), TYPE_CODE (parm));
#endif

  /* x -> y means arg of type x being supplied for parameter of type y */

  switch (TYPE_CODE (parm))
    {
      case TYPE_CODE_PTR:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_PTR: 
              if (TYPE_CODE (TYPE_TARGET_TYPE (parm)) == TYPE_CODE_VOID)
                return VOID_PTR_CONVERSION_BADNESS;
              else
                return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
            case TYPE_CODE_ARRAY:
              return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
            case TYPE_CODE_FUNC:
              return rank_one_type (TYPE_TARGET_TYPE (parm), arg);
            case TYPE_CODE_INT:
            case TYPE_CODE_ENUM:
            case TYPE_CODE_CHAR:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_BOOL:
              return POINTER_CONVERSION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
      case TYPE_CODE_ARRAY:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_PTR:
            case TYPE_CODE_ARRAY:
              return rank_one_type (TYPE_TARGET_TYPE (parm), TYPE_TARGET_TYPE (arg));
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
      case TYPE_CODE_FUNC:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_PTR: /* funcptr -> func */
              return rank_one_type (parm, TYPE_TARGET_TYPE (arg));
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
      case TYPE_CODE_INT:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_INT:
              if (TYPE_LENGTH (arg) == TYPE_LENGTH (parm))
                {
                  /* Deal with signed, unsigned, and plain chars and
                     signed and unsigned ints */
                  if (TYPE_NOSIGN (parm))
                    {
                      /* This case only for character types */
                      if (TYPE_NOSIGN (arg))  /* plain char -> plain char */
                        return 0;
                      else
                        return INTEGER_COERCION_BADNESS; /* signed/unsigned char -> plain char */
                    }
                  else if (TYPE_UNSIGNED (parm))
                    {
                      if (TYPE_UNSIGNED (arg))
                        {
                          if (!strcmp (TYPE_NAME (parm), TYPE_NAME (arg))) 
                            return 0;  /* unsigned int -> unsigned int, or unsigned long -> unsigned long */
                          else if (!strcmp (TYPE_NAME (arg), "int") && !strcmp (TYPE_NAME (parm), "long"))
                            return INTEGER_PROMOTION_BADNESS; /* unsigned int -> unsigned long */
                          else
                            return INTEGER_COERCION_BADNESS; /* unsigned long -> unsigned int */ 
                        }
                      else
                        {
                          if (!strcmp (TYPE_NAME (arg), "long") && !strcmp (TYPE_NAME (parm), "int"))
                            return INTEGER_COERCION_BADNESS; /* signed long -> unsigned int */
                          else 
                            return INTEGER_CONVERSION_BADNESS; /* signed int/long -> unsigned int/long */ 
                        }
                    }
                  else if (!TYPE_NOSIGN (arg) && !TYPE_UNSIGNED (arg))
                    {
                      if (!strcmp (TYPE_NAME (parm), TYPE_NAME (arg)))
                        return 0;
                      else if (!strcmp (TYPE_NAME (arg), "int") && !strcmp (TYPE_NAME (parm), "long"))
                        return INTEGER_PROMOTION_BADNESS;
                      else
                        return INTEGER_COERCION_BADNESS;
                    }
                  else
                    return INTEGER_COERCION_BADNESS;
                }
              else if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
                return INTEGER_PROMOTION_BADNESS;
              else
                return INTEGER_COERCION_BADNESS;
            case TYPE_CODE_ENUM:
            case TYPE_CODE_CHAR:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_BOOL:
              return INTEGER_PROMOTION_BADNESS;
            case TYPE_CODE_FLT:
              return INT_FLOAT_CONVERSION_BADNESS;
            case TYPE_CODE_PTR:
              return NS_POINTER_CONVERSION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_ENUM:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_INT:
            case TYPE_CODE_CHAR:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_BOOL:
            case TYPE_CODE_ENUM:
              return INTEGER_COERCION_BADNESS;
            case TYPE_CODE_FLT:
              return INT_FLOAT_CONVERSION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_CHAR:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_RANGE:
            case TYPE_CODE_BOOL:
            case TYPE_CODE_ENUM:
              return INTEGER_COERCION_BADNESS;
            case TYPE_CODE_FLT:
              return INT_FLOAT_CONVERSION_BADNESS;
            case TYPE_CODE_INT: 
              if (TYPE_LENGTH (arg) > TYPE_LENGTH (parm))
                return INTEGER_COERCION_BADNESS;
              else if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
                return INTEGER_PROMOTION_BADNESS;
              /* >>> !! else fall through !! <<< */ 
            case TYPE_CODE_CHAR:
              /* Deal with signed, unsigned, and plain chars for C++
                 and with int cases falling through from previous case */
              if (TYPE_NOSIGN (parm))
                {
                  if (TYPE_NOSIGN (arg))
                    return 0;
                  else
                    return INTEGER_COERCION_BADNESS;
                }
              else if (TYPE_UNSIGNED (parm))
                {
                  if (TYPE_UNSIGNED (arg))
                    return 0;
                  else
                    return INTEGER_PROMOTION_BADNESS;
                }
              else if (!TYPE_NOSIGN (arg) && !TYPE_UNSIGNED (arg))
                return 0;
              else
                return INTEGER_COERCION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_RANGE:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_INT:
            case TYPE_CODE_CHAR:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_BOOL:
            case TYPE_CODE_ENUM:
              return INTEGER_COERCION_BADNESS;
            case TYPE_CODE_FLT:
              return INT_FLOAT_CONVERSION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_BOOL:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_INT:
            case TYPE_CODE_CHAR:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_ENUM:
            case TYPE_CODE_FLT:
            case TYPE_CODE_PTR:
              return BOOLEAN_CONVERSION_BADNESS;
            case TYPE_CODE_BOOL:
              return 0;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_FLT:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_FLT:
              if (TYPE_LENGTH (arg) < TYPE_LENGTH (parm))
                return FLOAT_PROMOTION_BADNESS;
              else if (TYPE_LENGTH (arg) == TYPE_LENGTH (parm))
                return 0;
              else
                return FLOAT_CONVERSION_BADNESS;
            case TYPE_CODE_INT:
            case TYPE_CODE_BOOL:
            case TYPE_CODE_ENUM:
            case TYPE_CODE_RANGE:
            case TYPE_CODE_CHAR:
              return INT_FLOAT_CONVERSION_BADNESS;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_COMPLEX:
        switch (TYPE_CODE (arg))
          { /* Strictly not needed for C++, but... */
            case TYPE_CODE_FLT:
              return FLOAT_PROMOTION_BADNESS;
            case TYPE_CODE_COMPLEX:
              return 0;
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_STRUCT:
      /* currently same as TYPE_CODE_CLASS */
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_STRUCT:
              /* Check for derivation */
              if (is_ancestor (parm, arg))
                return BASE_CONVERSION_BADNESS;
              /* else fall through */
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_UNION:
        switch (TYPE_CODE (arg))
          {
            case TYPE_CODE_UNION:
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_MEMBER:
        switch (TYPE_CODE (arg))
          {
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_METHOD:
        switch (TYPE_CODE (arg))
          {
            
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_REF:
        switch (TYPE_CODE (arg))
          {
            
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }

        break;
      case TYPE_CODE_SET:
        switch (TYPE_CODE (arg))
          {
            /* Not in C++ */
            case TYPE_CODE_SET:
              return rank_one_type (TYPE_FIELD_TYPE (parm, 0), TYPE_FIELD_TYPE (arg, 0));
            default:
              return INCOMPATIBLE_TYPE_BADNESS;
          }
        break;
      case TYPE_CODE_VOID:
      default:  
        return INCOMPATIBLE_TYPE_BADNESS;
    } /* switch (TYPE_CODE (arg)) */
}

 
/* End of functions for overload resolution */ 



#if MAINTENANCE_CMDS

static void
print_bit_vector (bits, nbits)
     B_TYPE *bits;
     int nbits;
{
  int bitno;

  for (bitno = 0; bitno < nbits; bitno++)
    {
      if ((bitno % 8) == 0)
	{
	  puts_filtered (" ");
	}
      if (B_TST (bits, bitno))
	{
	  printf_filtered ("1");
	}
      else
	{
	  printf_filtered ("0");
	}
    }
}

/* The args list is a strange beast.  It is either terminated by a NULL
   pointer for varargs functions, or by a pointer to a TYPE_CODE_VOID
   type for normal fixed argcount functions.  (FIXME someday)
   Also note the first arg should be the "this" pointer, we may not want to
   include it since we may get into a infinitely recursive situation. */

static void
print_arg_types (args, spaces)
     struct type **args;
     int spaces;
{
  if (args != NULL)
    {
      while (*args != NULL)
	{
	  recursive_dump_type (*args, spaces + 2);
	  if ((*args++) -> code == TYPE_CODE_VOID)
	    {
	      break;
	    }
	}
    }
}

static void
dump_fn_fieldlists (type, spaces)
     struct type *type;
     int spaces;
{
  int method_idx;
  int overload_idx;
  struct fn_field *f;

  printfi_filtered (spaces, "fn_fieldlists ");
  gdb_print_address (TYPE_FN_FIELDLISTS (type), gdb_stdout);
  printf_filtered ("\n");
  for (method_idx = 0; method_idx < TYPE_NFN_FIELDS (type); method_idx++)
    {
      f = TYPE_FN_FIELDLIST1 (type, method_idx);
      printfi_filtered (spaces + 2, "[%d] name '%s' (",
			method_idx,
			TYPE_FN_FIELDLIST_NAME (type, method_idx));
      gdb_print_address (TYPE_FN_FIELDLIST_NAME (type, method_idx),
			 gdb_stdout);
      printf_filtered (") length %d\n",
		       TYPE_FN_FIELDLIST_LENGTH (type, method_idx));
      for (overload_idx = 0;
	   overload_idx < TYPE_FN_FIELDLIST_LENGTH (type, method_idx);
	   overload_idx++)
	{
	  printfi_filtered (spaces + 4, "[%d] physname '%s' (",
			    overload_idx,
			    TYPE_FN_FIELD_PHYSNAME (f, overload_idx));
	  gdb_print_address (TYPE_FN_FIELD_PHYSNAME (f, overload_idx),
			     gdb_stdout);
	  printf_filtered (")\n");
	  printfi_filtered (spaces + 8, "type ");
	  gdb_print_address (TYPE_FN_FIELD_TYPE (f, overload_idx), gdb_stdout);
	  printf_filtered ("\n");

	  recursive_dump_type (TYPE_FN_FIELD_TYPE (f, overload_idx),
			       spaces + 8 + 2);

	  printfi_filtered (spaces + 8, "args ");
	  gdb_print_address (TYPE_FN_FIELD_ARGS (f, overload_idx), gdb_stdout);
	  printf_filtered ("\n");

	  print_arg_types (TYPE_FN_FIELD_ARGS (f, overload_idx), spaces);
	  printfi_filtered (spaces + 8, "fcontext ");
	  gdb_print_address (TYPE_FN_FIELD_FCONTEXT (f, overload_idx),
			     gdb_stdout);
	  printf_filtered ("\n");

	  printfi_filtered (spaces + 8, "is_const %d\n",
			    TYPE_FN_FIELD_CONST (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_volatile %d\n",
			    TYPE_FN_FIELD_VOLATILE (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_private %d\n",
			    TYPE_FN_FIELD_PRIVATE (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_protected %d\n",
			    TYPE_FN_FIELD_PROTECTED (f, overload_idx));
	  printfi_filtered (spaces + 8, "is_stub %d\n",
			    TYPE_FN_FIELD_STUB (f, overload_idx));
	  printfi_filtered (spaces + 8, "voffset %u\n",
			    TYPE_FN_FIELD_VOFFSET (f, overload_idx));
	}
    }
}

static void
print_cplus_stuff (type, spaces)
     struct type *type;
     int spaces;
{
  printfi_filtered (spaces, "n_baseclasses %d\n",
		    TYPE_N_BASECLASSES (type));
  printfi_filtered (spaces, "nfn_fields %d\n",
		    TYPE_NFN_FIELDS (type));
  printfi_filtered (spaces, "nfn_fields_total %d\n",
		    TYPE_NFN_FIELDS_TOTAL (type));
  if (TYPE_N_BASECLASSES (type) > 0)
    {
      printfi_filtered (spaces, "virtual_field_bits (%d bits at *",
			TYPE_N_BASECLASSES (type));
      gdb_print_address (TYPE_FIELD_VIRTUAL_BITS (type), gdb_stdout);
      printf_filtered (")");

      print_bit_vector (TYPE_FIELD_VIRTUAL_BITS (type),
			TYPE_N_BASECLASSES (type));
      puts_filtered ("\n");
    }
  if (TYPE_NFIELDS (type) > 0)
    {
      if (TYPE_FIELD_PRIVATE_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "private_field_bits (%d bits at *",
			    TYPE_NFIELDS (type));
	  gdb_print_address (TYPE_FIELD_PRIVATE_BITS (type), gdb_stdout);
	  printf_filtered (")");
	  print_bit_vector (TYPE_FIELD_PRIVATE_BITS (type),
			    TYPE_NFIELDS (type));
	  puts_filtered ("\n");
	}
      if (TYPE_FIELD_PROTECTED_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "protected_field_bits (%d bits at *",
			    TYPE_NFIELDS (type));
	  gdb_print_address (TYPE_FIELD_PROTECTED_BITS (type), gdb_stdout);
	  printf_filtered (")");
	  print_bit_vector (TYPE_FIELD_PROTECTED_BITS (type),
			    TYPE_NFIELDS (type));
	  puts_filtered ("\n");
	}
    }
  if (TYPE_NFN_FIELDS (type) > 0)
    {
      dump_fn_fieldlists (type, spaces);
    }
}

static struct obstack dont_print_type_obstack;

void
recursive_dump_type (type, spaces)
     struct type *type;
     int spaces;
{
  int idx;

  if (spaces == 0)
    obstack_begin (&dont_print_type_obstack, 0);

  if (TYPE_NFIELDS (type) > 0
      || (TYPE_CPLUS_SPECIFIC (type) && TYPE_NFN_FIELDS (type) > 0))
    {
      struct type **first_dont_print
	= (struct type **)obstack_base (&dont_print_type_obstack);

      int i = (struct type **)obstack_next_free (&dont_print_type_obstack)
	- first_dont_print;

      while (--i >= 0)
	{
	  if (type == first_dont_print[i])
	    {
	      printfi_filtered (spaces, "type node ");
	      gdb_print_address (type, gdb_stdout);
	      printf_filtered (" <same as already seen type>\n");
	      return;
	    }
	}

      obstack_ptr_grow (&dont_print_type_obstack, type);
    }

  printfi_filtered (spaces, "type node ");
  gdb_print_address (type, gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "name '%s' (",
		    TYPE_NAME (type) ? TYPE_NAME (type) : "<NULL>");
  gdb_print_address (TYPE_NAME (type), gdb_stdout);
  printf_filtered (")\n");
  if (TYPE_TAG_NAME (type) != NULL)
    {
      printfi_filtered (spaces, "tagname '%s' (",
			TYPE_TAG_NAME (type));
      gdb_print_address (TYPE_TAG_NAME (type), gdb_stdout);
      printf_filtered (")\n");
    }
  printfi_filtered (spaces, "code 0x%x ", TYPE_CODE (type));
  switch (TYPE_CODE (type))
    {
      case TYPE_CODE_UNDEF:
        printf_filtered ("(TYPE_CODE_UNDEF)");
	break;
      case TYPE_CODE_PTR:
	printf_filtered ("(TYPE_CODE_PTR)");
	break;
      case TYPE_CODE_ARRAY:
	printf_filtered ("(TYPE_CODE_ARRAY)");
	break;
      case TYPE_CODE_STRUCT:
	printf_filtered ("(TYPE_CODE_STRUCT)");
	break;
      case TYPE_CODE_UNION:
	printf_filtered ("(TYPE_CODE_UNION)");
	break;
      case TYPE_CODE_ENUM:
	printf_filtered ("(TYPE_CODE_ENUM)");
	break;
      case TYPE_CODE_FUNC:
	printf_filtered ("(TYPE_CODE_FUNC)");
	break;
      case TYPE_CODE_INT:
	printf_filtered ("(TYPE_CODE_INT)");
	break;
      case TYPE_CODE_FLT:
	printf_filtered ("(TYPE_CODE_FLT)");
	break;
      case TYPE_CODE_VOID:
	printf_filtered ("(TYPE_CODE_VOID)");
	break;
      case TYPE_CODE_SET:
	printf_filtered ("(TYPE_CODE_SET)");
	break;
      case TYPE_CODE_RANGE:
	printf_filtered ("(TYPE_CODE_RANGE)");
	break;
      case TYPE_CODE_STRING:
	printf_filtered ("(TYPE_CODE_STRING)");
	break;
      case TYPE_CODE_ERROR:
	printf_filtered ("(TYPE_CODE_ERROR)");
	break;
      case TYPE_CODE_MEMBER:
	printf_filtered ("(TYPE_CODE_MEMBER)");
	break;
      case TYPE_CODE_METHOD:
	printf_filtered ("(TYPE_CODE_METHOD)");
	break;
      case TYPE_CODE_REF:
	printf_filtered ("(TYPE_CODE_REF)");
	break;
      case TYPE_CODE_CHAR:
	printf_filtered ("(TYPE_CODE_CHAR)");
	break;
      case TYPE_CODE_BOOL:
	printf_filtered ("(TYPE_CODE_BOOL)");
	break;
      case TYPE_CODE_TYPEDEF:
	printf_filtered ("(TYPE_CODE_TYPEDEF)");
	break;
      default:
	printf_filtered ("(UNKNOWN TYPE CODE)");
	break;
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "length %d\n", TYPE_LENGTH (type));
  printfi_filtered (spaces, "objfile ");
  gdb_print_address (TYPE_OBJFILE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "target_type ");
  gdb_print_address (TYPE_TARGET_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  if (TYPE_TARGET_TYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_TARGET_TYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "pointer_type ");
  gdb_print_address (TYPE_POINTER_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "reference_type ");
  gdb_print_address (TYPE_REFERENCE_TYPE (type), gdb_stdout);
  printf_filtered ("\n");
  printfi_filtered (spaces, "flags 0x%x", TYPE_FLAGS (type));
  if (TYPE_FLAGS (type) & TYPE_FLAG_UNSIGNED)
    {
      puts_filtered (" TYPE_FLAG_UNSIGNED");
    }
  if (TYPE_FLAGS (type) & TYPE_FLAG_STUB)
    {
      puts_filtered (" TYPE_FLAG_STUB");
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "nfields %d ", TYPE_NFIELDS (type));
  gdb_print_address (TYPE_FIELDS (type), gdb_stdout);
  puts_filtered ("\n");
  for (idx = 0; idx < TYPE_NFIELDS (type); idx++)
    {
      printfi_filtered (spaces + 2,
			"[%d] bitpos %d bitsize %d type ",
			idx, TYPE_FIELD_BITPOS (type, idx),
			TYPE_FIELD_BITSIZE (type, idx));
      gdb_print_address (TYPE_FIELD_TYPE (type, idx), gdb_stdout);
      printf_filtered (" name '%s' (",
		       TYPE_FIELD_NAME (type, idx) != NULL
		       ? TYPE_FIELD_NAME (type, idx)
		       : "<NULL>");
      gdb_print_address (TYPE_FIELD_NAME (type, idx), gdb_stdout);
      printf_filtered (")\n");
      if (TYPE_FIELD_TYPE (type, idx) != NULL)
	{
	  recursive_dump_type (TYPE_FIELD_TYPE (type, idx), spaces + 4);
	}
    }
  printfi_filtered (spaces, "vptr_basetype ");
  gdb_print_address (TYPE_VPTR_BASETYPE (type), gdb_stdout);
  puts_filtered ("\n");
  if (TYPE_VPTR_BASETYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_VPTR_BASETYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "vptr_fieldno %d\n", TYPE_VPTR_FIELDNO (type));
  switch (TYPE_CODE (type))
    {
      case TYPE_CODE_METHOD:
      case TYPE_CODE_FUNC:
	printfi_filtered (spaces, "arg_types ");
	gdb_print_address (TYPE_ARG_TYPES (type), gdb_stdout);
	puts_filtered ("\n");
	print_arg_types (TYPE_ARG_TYPES (type), spaces);
	break;

      case TYPE_CODE_STRUCT:
	printfi_filtered (spaces, "cplus_stuff ");
	gdb_print_address (TYPE_CPLUS_SPECIFIC (type), gdb_stdout);
	puts_filtered ("\n");
	print_cplus_stuff (type, spaces);
	break;

      default:
	/* We have to pick one of the union types to be able print and test
	   the value.  Pick cplus_struct_type, even though we know it isn't
	   any particular one. */
	printfi_filtered (spaces, "type_specific ");
	gdb_print_address (TYPE_CPLUS_SPECIFIC (type), gdb_stdout);
	if (TYPE_CPLUS_SPECIFIC (type) != NULL)
	  {
	    printf_filtered (" (unknown data form)");
	  }
	printf_filtered ("\n");
	break;

    }
  if (spaces == 0)
    obstack_free (&dont_print_type_obstack, NULL);
}

#endif	/* MAINTENANCE_CMDS */


static void build_gdbtypes PARAMS ((void));
static void
build_gdbtypes ()
{
  builtin_type_void =
    init_type (TYPE_CODE_VOID, 1,
	       0,
	       "void", (struct objfile *) NULL);
  builtin_type_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "char", (struct objfile *) NULL);
  TYPE_FLAGS (builtin_type_char) |= TYPE_FLAG_NOSIGN;
  
  builtin_type_signed_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "signed char", (struct objfile *) NULL);
  builtin_type_unsigned_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned char", (struct objfile *) NULL);
  builtin_type_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       0,
	       "short", (struct objfile *) NULL);
  builtin_type_unsigned_short =
    init_type (TYPE_CODE_INT, TARGET_SHORT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned short", (struct objfile *) NULL);
  builtin_type_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       0,
	       "int", (struct objfile *) NULL);
  builtin_type_unsigned_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned int", (struct objfile *) NULL);
  builtin_type_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long", (struct objfile *) NULL);
  builtin_type_unsigned_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long", (struct objfile *) NULL);
  builtin_type_long_long =
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       0,
	       "long long", (struct objfile *) NULL);
  builtin_type_unsigned_long_long = 
    init_type (TYPE_CODE_INT, TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_UNSIGNED,
	       "unsigned long long", (struct objfile *) NULL);
  builtin_type_float =
    init_type (TYPE_CODE_FLT, TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
	       0,
	       "float", (struct objfile *) NULL);
  builtin_type_double =
    init_type (TYPE_CODE_FLT, TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "double", (struct objfile *) NULL);
  builtin_type_long_double =
    init_type (TYPE_CODE_FLT, TARGET_LONG_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "long double", (struct objfile *) NULL);
  builtin_type_complex =
    init_type (TYPE_CODE_COMPLEX, 2 * TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
	       0,
	       "complex", (struct objfile *) NULL);
  TYPE_TARGET_TYPE (builtin_type_complex) = builtin_type_float;
  builtin_type_double_complex =
    init_type (TYPE_CODE_COMPLEX, 2 * TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0,
	       "double complex", (struct objfile *) NULL);
  TYPE_TARGET_TYPE (builtin_type_double_complex) = builtin_type_double;
  builtin_type_string =
    init_type (TYPE_CODE_STRING, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "string", (struct objfile *) NULL);
  builtin_type_int8 =
    init_type (TYPE_CODE_INT, 8 / 8,
	       0,
	       "int8_t", (struct objfile *) NULL);
  builtin_type_uint8 =
    init_type (TYPE_CODE_INT, 8 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint8_t", (struct objfile *) NULL);
  builtin_type_int16 =
    init_type (TYPE_CODE_INT, 16 / 8,
	       0,
	       "int16_t", (struct objfile *) NULL);
  builtin_type_uint16 =
    init_type (TYPE_CODE_INT, 16 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint16_t", (struct objfile *) NULL);
  builtin_type_int32 =
    init_type (TYPE_CODE_INT, 32 / 8,
	       0,
	       "int32_t", (struct objfile *) NULL);
  builtin_type_uint32 =
    init_type (TYPE_CODE_INT, 32 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint32_t", (struct objfile *) NULL);
  builtin_type_int64 =
    init_type (TYPE_CODE_INT, 64 / 8,
	       0,
	       "int64_t", (struct objfile *) NULL);
  builtin_type_uint64 =
    init_type (TYPE_CODE_INT, 64 / 8,
	       TYPE_FLAG_UNSIGNED,
	       "uint64_t", (struct objfile *) NULL);
  builtin_type_bool =
    init_type (TYPE_CODE_BOOL, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "bool", (struct objfile *) NULL);

  /* Add user knob for controlling resolution of opaque types */ 
  add_show_from_set
    (add_set_cmd ("opaque-type-resolution", class_support, var_boolean, (char *)&opaque_type_resolution,
		  "Set resolution of opaque struct/class/union types (if set before loading symbols).",
		  &setlist),
     &showlist);
  opaque_type_resolution = 1;

}


extern void _initialize_gdbtypes PARAMS ((void));
void
_initialize_gdbtypes ()
{
  build_gdbtypes ();
}
