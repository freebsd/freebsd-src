/* Support routines for manipulating internal types for GDB.
   Copyright (C) 1992 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include <string.h>
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
    }
  memset ((char *) type, 0, sizeof (struct type));

  /* Initialize the fields that might not be zero. */

  TYPE_CODE (type) = TYPE_CODE_UNDEF;
  TYPE_OBJFILE (type) = objfile;
  TYPE_VPTR_FIELDNO (type) = -1;

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
    if (typeptr == 0)		
      return ntype;	/* Don't care about alloc, and have new type.  */
    else if (*typeptr == 0)
      {
	*typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	return ntype;
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
    if (typeptr == 0)		
      return ntype;	/* Don't care about alloc, and have new type.  */
    else if (*typeptr == 0)
      {
	*typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	return ntype;
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

  ntype = TYPE_FUNCTION_TYPE (type);

  if (ntype) 
    if (typeptr == 0)		
      return ntype;	/* Don't care about alloc, and have new type.  */
    else if (*typeptr == 0)
      {
	*typeptr = ntype;	/* Tracking alloc, and we have new type.  */
	return ntype;
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
  TYPE_FUNCTION_TYPE (type) = ntype;

  TYPE_LENGTH (ntype) = 1;
  TYPE_CODE (ntype) = TYPE_CODE_FUNC;
  
  if (!TYPE_FUNCTION_TYPE (type))	/* Remember it, if don't have one.  */
    TYPE_FUNCTION_TYPE (type) = ntype;

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
  TYPE_LENGTH (result_type) = TYPE_LENGTH (index_type);
  TYPE_NFIELDS (result_type) = 2;
  TYPE_FIELDS (result_type) = (struct field *)
    TYPE_ALLOC (result_type, 2 * sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, 2 * sizeof (struct field));
  TYPE_FIELD_BITPOS (result_type, 0) = low_bound;
  TYPE_FIELD_BITPOS (result_type, 1) = high_bound;
  TYPE_FIELD_TYPE (result_type, 0) = builtin_type_int;		/* FIXME */
  TYPE_FIELD_TYPE (result_type, 1) = builtin_type_int;		/* FIXME */

  return (result_type);
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
  int low_bound;
  int high_bound;

  if (TYPE_CODE (range_type) != TYPE_CODE_RANGE)
    {
      /* FIXME:  We only handle range types at the moment.  Complain and
	 create a dummy range type to use. */
      warning ("internal error:  array index type must be a range type");
      range_type = lookup_fundamental_type (TYPE_OBJFILE (range_type),
					    FT_INTEGER);
      range_type = create_range_type ((struct type *) NULL, range_type, 0, 0);
    }
  if (result_type == NULL)
    {
      result_type = alloc_type (TYPE_OBJFILE (range_type));
    }
  TYPE_CODE (result_type) = TYPE_CODE_ARRAY;
  TYPE_TARGET_TYPE (result_type) = element_type;
  low_bound = TYPE_FIELD_BITPOS (range_type, 0);
  high_bound = TYPE_FIELD_BITPOS (range_type, 1);
  TYPE_LENGTH (result_type) =
    TYPE_LENGTH (element_type) * (high_bound - low_bound + 1);
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) =
    (struct field *) TYPE_ALLOC (result_type, sizeof (struct field));
  memset (TYPE_FIELDS (result_type), 0, sizeof (struct field));
  TYPE_FIELD_TYPE (result_type, 0) = range_type;
  TYPE_VPTR_FIELDNO (result_type) = -1;

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
  result_type = create_array_type (result_type, builtin_type_char, range_type);
  TYPE_CODE (result_type) = TYPE_CODE_STRING;
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

  sym = lookup_symbol (name, block, STRUCT_NAMESPACE, 0,
		       (struct symtab **) NULL);

  if (sym == NULL)
    {
      error ("No union type named %s.", name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_UNION)
    {
      error ("This context has class, struct or enum %s, not a union.", name);
    }
  return (SYMBOL_TYPE (sym));
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

  if (TYPE_CODE (type) == TYPE_CODE_PTR ||
      TYPE_CODE (type) == TYPE_CODE_REF)
      type = TYPE_TARGET_TYPE (type);

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT &&
      TYPE_CODE (type) != TYPE_CODE_UNION)
    {
      target_terminal_ours ();
      fflush (stdout);
      fprintf (stderr, "Type ");
      type_print (type, "", stderr, -1);
      error (" is not a structure or union type.");
    }

  check_stub_type (type);

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
  fflush (stdout);
  fprintf (stderr, "Type ");
  type_print (type, "", stderr, -1);
  fprintf (stderr, " has no component named ");
  fputs_filtered (name, stderr);
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
  check_stub_type (type);

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

/* Added by Bryan Boreham, Kewill, Sun Sep 17 18:07:17 1989.

   If this is a stubbed struct (i.e. declared as struct foo *), see if
   we can find a full definition in some other file. If so, copy this
   definition, so we can use it in future.  If not, set a flag so we 
   don't waste too much time in future.  (FIXME, this doesn't seem
   to be happening...)

   This used to be coded as a macro, but I don't think it is called 
   often enough to merit such treatment.
*/

struct complaint stub_noname_complaint =
  {"stub type has NULL name", 0, 0};

void 
check_stub_type (type)
     struct type *type;
{
  if (TYPE_FLAGS(type) & TYPE_FLAG_STUB)
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
	  return;
	}
      sym = lookup_symbol (name, 0, STRUCT_NAMESPACE, 0, 
			   (struct symtab **) NULL);
      if (sym)
	{
	  memcpy ((char *)type, (char *)SYMBOL_TYPE(sym), sizeof (struct type));
	}
    }
}

/* Ugly hack to convert method stubs into method types.

   He ain't kiddin'.  This demangles the name of the method into a string
   including argument types, parses out each argument type, generates
   a string casting a zero to that type, evaluates the string, and stuffs
   the resulting type into an argtype vector!!!  Then it knows the type
   of the whole function (including argument types for overloading),
   which info used to be in the stab's but was removed to hack back
   the space required for them.  */

void
check_stub_method (type, i, j)
     struct type *type;
     int i;
     int j;
{
  struct fn_field *f;
  char *mangled_name = gdb_mangle_name (type, i, j);
  char *demangled_name = cplus_demangle (mangled_name,
					 DMGL_PARAMS | DMGL_ANSI);
  char *argtypetext, *p;
  int depth = 0, argcount = 1;
  struct type **argtypes;
  struct type *mtype;

  if (demangled_name == NULL)
    {
      error ("Internal: Cannot demangle mangled name `%s'.", mangled_name);
    }

  /* Now, read in the parameters that define this type.  */
  argtypetext = strchr (demangled_name, '(') + 1;
  p = argtypetext;
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
  argtypes[0] = lookup_pointer_type (type);
  argcount = 1;

  if (*p != ')')			/* () means no args, skip while */
    {
      depth = 0;
      while (*p)
	{
	  if (depth <= 0 && (*p == ',' || *p == ')'))
	    {
	      argtypes[argcount] =
		  parse_and_eval_type (argtypetext, p - argtypetext);
	      argcount += 1;
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

  f = TYPE_FN_FIELDLIST1 (type, i);
  TYPE_FN_FIELD_PHYSNAME (f, j) = mangled_name;

  /* Now update the old "stub" type into a real type.  */
  mtype = TYPE_FN_FIELD_TYPE (f, j);
  TYPE_DOMAIN_TYPE (mtype) = type;
  TYPE_ARG_TYPES (mtype) = argtypes;
  TYPE_FLAGS (mtype) &= ~TYPE_FLAG_STUB;
  TYPE_FN_FIELD_STUB (f, j) = 0;
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

  printfi_filtered (spaces, "fn_fieldlists 0x%lx\n",
		    (unsigned long) TYPE_FN_FIELDLISTS (type));
  for (method_idx = 0; method_idx < TYPE_NFN_FIELDS (type); method_idx++)
    {
      f = TYPE_FN_FIELDLIST1 (type, method_idx);
      printfi_filtered (spaces + 2, "[%d] name '%s' (0x%lx) length %d\n",
			method_idx,
			TYPE_FN_FIELDLIST_NAME (type, method_idx),
			(unsigned long) TYPE_FN_FIELDLIST_NAME (type, method_idx),
			TYPE_FN_FIELDLIST_LENGTH (type, method_idx));
      for (overload_idx = 0;
	   overload_idx < TYPE_FN_FIELDLIST_LENGTH (type, method_idx);
	   overload_idx++)
	{
	  printfi_filtered (spaces + 4, "[%d] physname '%s' (0x%lx)\n",
			    overload_idx,
			    TYPE_FN_FIELD_PHYSNAME (f, overload_idx),
			    (unsigned long) TYPE_FN_FIELD_PHYSNAME (f, overload_idx));
	  printfi_filtered (spaces + 8, "type 0x%lx\n",
			    (unsigned long) TYPE_FN_FIELD_TYPE (f, overload_idx));
	  recursive_dump_type (TYPE_FN_FIELD_TYPE (f, overload_idx),
			       spaces + 8 + 2);
	  printfi_filtered (spaces + 8, "args 0x%lx\n",
			    (unsigned long) TYPE_FN_FIELD_ARGS (f, overload_idx));
	  print_arg_types (TYPE_FN_FIELD_ARGS (f, overload_idx), spaces);
	  printfi_filtered (spaces + 8, "fcontext 0x%lx\n",
			    (unsigned long) TYPE_FN_FIELD_FCONTEXT (f, overload_idx));
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
      printfi_filtered (spaces, "virtual_field_bits (%d bits at *0x%lx)",
			TYPE_N_BASECLASSES (type),
			(unsigned long) TYPE_FIELD_VIRTUAL_BITS (type));
      print_bit_vector (TYPE_FIELD_VIRTUAL_BITS (type),
			TYPE_N_BASECLASSES (type));
      puts_filtered ("\n");
    }
  if (TYPE_NFIELDS (type) > 0)
    {
      if (TYPE_FIELD_PRIVATE_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "private_field_bits (%d bits at *0x%lx)",
			    TYPE_NFIELDS (type),
			    (unsigned long) TYPE_FIELD_PRIVATE_BITS (type));
	  print_bit_vector (TYPE_FIELD_PRIVATE_BITS (type),
			    TYPE_NFIELDS (type));
	  puts_filtered ("\n");
	}
      if (TYPE_FIELD_PROTECTED_BITS (type) != NULL)
	{
	  printfi_filtered (spaces, "protected_field_bits (%d bits at *0x%lx)",
			    TYPE_NFIELDS (type),
			    (unsigned long) TYPE_FIELD_PROTECTED_BITS (type));
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

void
recursive_dump_type (type, spaces)
     struct type *type;
     int spaces;
{
  int idx;

  printfi_filtered (spaces, "type node 0x%lx\n", (unsigned long)type);
  printfi_filtered (spaces, "name '%s' (0x%lx)\n",
		    TYPE_NAME (type) ? TYPE_NAME (type) : "<NULL>",
		    (unsigned long)TYPE_NAME (type));
  if (TYPE_TAG_NAME (type) != NULL)
    printfi_filtered (spaces, "tagname '%s' (0x%lx)\n",
		      TYPE_TAG_NAME (type),
		      (unsigned long)TYPE_TAG_NAME (type));
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
      default:
	printf_filtered ("(UNKNOWN TYPE CODE)");
	break;
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "length %d\n", TYPE_LENGTH (type));
  printfi_filtered (spaces, "objfile 0x%lx\n",
		    (unsigned long) TYPE_OBJFILE (type));
  printfi_filtered (spaces, "target_type 0x%lx\n",
		    (unsigned long) TYPE_TARGET_TYPE (type));
  if (TYPE_TARGET_TYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_TARGET_TYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "pointer_type 0x%lx\n",
		    (unsigned long) TYPE_POINTER_TYPE (type));
  printfi_filtered (spaces, "reference_type 0x%lx\n",
		    (unsigned long) TYPE_REFERENCE_TYPE (type));
  printfi_filtered (spaces, "function_type 0x%lx\n",
		    (unsigned long) TYPE_FUNCTION_TYPE (type));
  printfi_filtered (spaces, "flags 0x%x", TYPE_FLAGS (type));
  if (TYPE_FLAGS (type) & TYPE_FLAG_UNSIGNED)
    {
      puts_filtered (" TYPE_FLAG_UNSIGNED");
    }
  if (TYPE_FLAGS (type) & TYPE_FLAG_SIGNED)
    {
      puts_filtered (" TYPE_FLAG_SIGNED");
    }
  if (TYPE_FLAGS (type) & TYPE_FLAG_STUB)
    {
      puts_filtered (" TYPE_FLAG_STUB");
    }
  puts_filtered ("\n");
  printfi_filtered (spaces, "nfields %d 0x%lx\n", TYPE_NFIELDS (type),
		    (unsigned long) TYPE_FIELDS (type));
  for (idx = 0; idx < TYPE_NFIELDS (type); idx++)
    {
      printfi_filtered (spaces + 2,
			"[%d] bitpos %d bitsize %d type 0x%lx name '%s' (0x%lx)\n",
			idx, TYPE_FIELD_BITPOS (type, idx),
			TYPE_FIELD_BITSIZE (type, idx),
			(unsigned long) TYPE_FIELD_TYPE (type, idx),
			TYPE_FIELD_NAME (type, idx) != NULL
			  ? TYPE_FIELD_NAME (type, idx)
			  : "<NULL>",
			(unsigned long) TYPE_FIELD_NAME (type, idx));
      if (TYPE_FIELD_TYPE (type, idx) != NULL)
	{
	  recursive_dump_type (TYPE_FIELD_TYPE (type, idx), spaces + 4);
	}
    }
  printfi_filtered (spaces, "vptr_basetype 0x%lx\n",
		    (unsigned long) TYPE_VPTR_BASETYPE (type));
  if (TYPE_VPTR_BASETYPE (type) != NULL)
    {
      recursive_dump_type (TYPE_VPTR_BASETYPE (type), spaces + 2);
    }
  printfi_filtered (spaces, "vptr_fieldno %d\n", TYPE_VPTR_FIELDNO (type));
  switch (TYPE_CODE (type))
    {
      case TYPE_CODE_METHOD:
      case TYPE_CODE_FUNC:
	printfi_filtered (spaces, "arg_types 0x%lx\n",
			  (unsigned long) TYPE_ARG_TYPES (type));
	print_arg_types (TYPE_ARG_TYPES (type), spaces);
	break;

      case TYPE_CODE_STRUCT:
	printfi_filtered (spaces, "cplus_stuff 0x%lx\n",
			  (unsigned long) TYPE_CPLUS_SPECIFIC (type));
	print_cplus_stuff (type, spaces);
	break;

      default:
	/* We have to pick one of the union types to be able print and test
	   the value.  Pick cplus_struct_type, even though we know it isn't
	   any particular one. */
	printfi_filtered (spaces, "type_specific 0x%lx",
			  (unsigned long) TYPE_CPLUS_SPECIFIC (type));
	if (TYPE_CPLUS_SPECIFIC (type) != NULL)
	  {
	    printf_filtered (" (unknown data form)");
	  }
	printf_filtered ("\n");
	break;

    }
}

#endif	/* MAINTENANCE_CMDS */

void
_initialize_gdbtypes ()
{
  builtin_type_void =
    init_type (TYPE_CODE_VOID, 1,
	       0,
	       "void", (struct objfile *) NULL);
  builtin_type_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "char", (struct objfile *) NULL);
  builtin_type_signed_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       TYPE_FLAG_SIGNED,
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
    init_type (TYPE_CODE_FLT, TARGET_COMPLEX_BIT / TARGET_CHAR_BIT,
	       0,
	       "complex", (struct objfile *) NULL);
  builtin_type_double_complex =
    init_type (TYPE_CODE_FLT, TARGET_DOUBLE_COMPLEX_BIT / TARGET_CHAR_BIT,
	       0,
	       "double complex", (struct objfile *) NULL);
  builtin_type_string =
    init_type (TYPE_CODE_STRING, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0,
	       "string", (struct objfile *) NULL);
}
