/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 *
 * $Header: /a/cvs/386BSD/src/gnu/gdb/symtab.c,v 1.1 1993/06/29 09:47:40 nate Exp $;
 */

#ifndef lint
static char sccsid[] = "@(#)symtab.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* Symbol table lookup for the GNU debugger, GDB.
   Copyright (C) 1986, 1987, 1988, 1989 Free Software Foundation, Inc.

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

#include <obstack.h>
#include <assert.h>

char *index ();
extern char *cplus_demangle ();
extern struct value * value_of_this ();

/* Allocate an obstack to hold objects that should be freed
   when we load a new symbol table.
   This includes the symbols made by dbxread
   and the types that are not permanent.  */

struct obstack obstack1;

struct obstack *symbol_obstack = &obstack1;

/* This obstack will be used for partial_symbol objects.  It can
   probably actually be the same as the symbol_obstack above, but I'd
   like to keep them seperate for now.  If I want to later, I'll
   replace one with the other.  */

struct obstack obstack2;

struct obstack *psymbol_obstack = &obstack2;

/* These variables point to the objects
   representing the predefined C data types.  */

struct type *builtin_type_void;
struct type *builtin_type_char;
struct type *builtin_type_short;
struct type *builtin_type_int;
struct type *builtin_type_long;
#ifdef LONG_LONG
struct type *builtin_type_long_long;
#endif
struct type *builtin_type_unsigned_char;
struct type *builtin_type_unsigned_short;
struct type *builtin_type_unsigned_int;
struct type *builtin_type_unsigned_long;
#ifdef LONG_LONG
struct type *builtin_type_unsigned_long_long;
#endif
struct type *builtin_type_float;
struct type *builtin_type_double;

/* Block in which the most recently searched-for symbol was found.
   Might be better to make this a parameter to lookup_symbol and 
   value_of_this. */
struct block *block_found;

/* Functions */
static int find_line_common ();
int lookup_misc_func ();
struct partial_symtab *lookup_partial_symtab ();
struct symtab *psymtab_to_symtab ();
static struct partial_symbol *lookup_partial_symbol ();

/* Check for a symtab of a specific name; first in symtabs, then in
   psymtabs.  *If* there is no '/' in the name, a match after a '/'
   in the symtab filename will also work.  */

static struct symtab *
lookup_symtab_1 (name)
     char *name;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register char *slash = index (name, '/');
  register int len = strlen (name);

  for (s = symtab_list; s; s = s->next)
    if (!strcmp (name, s->filename))
      return s;

  for (ps = partial_symtab_list; ps; ps = ps->next)
    if (!strcmp (name, ps->filename))
      {
	if (ps->readin)
	  fatal ("Internal: readin pst found when no symtab found.");
	s = psymtab_to_symtab (ps);
	return s;
      }

  if (!slash)
    {
      for (s = symtab_list; s; s = s->next)
	{
	  int l = strlen (s->filename);

	  if (s->filename[l - len -1] == '/'
	      && !strcmp (s->filename + l - len, name))
	    return s;
	}

      for (ps = partial_symtab_list; ps; ps = ps->next)
	{
	  int l = strlen (ps->filename);

	  if (ps->filename[l - len - 1] == '/'
	      && !strcmp (ps->filename + l - len, name))
	    {
	      if (ps->readin)
		fatal ("Internal: readin pst found when no symtab found.");
	      s = psymtab_to_symtab (ps);
	      return s;
	    }
	}
    }
  return 0;
}

/* Lookup the symbol table of a source file named NAME.  Try a couple
   of variations if the first lookup doesn't work.  */

struct symtab *
lookup_symtab (name)
     char *name;
{
  register struct symtab *s;
  register char *copy;

  s = lookup_symtab_1 (name);
  if (s) return s;

  /* If name not found as specified, see if adding ".c" helps.  */

  copy = (char *) alloca (strlen (name) + 3);
  strcpy (copy, name);
  strcat (copy, ".c");
  s = lookup_symtab_1 (copy);
  if (s) return s;

  /* We didn't find anything; die.  */
  return 0;
}

/* Lookup the partial symbol table of a source file named NAME.  This
   only returns true on an exact match (ie. this semantics are
   different from lookup_symtab.  */

struct partial_symtab *
lookup_partial_symtab (name)
char *name;
{
  register struct partial_symtab *s;
  register char *copy;
  
  for (s = partial_symtab_list; s; s = s->next)
    if (!strcmp (name, s->filename))
      return s;
  
  return 0;
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
  register struct symbol *sym = lookup_symbol (name, block, VAR_NAMESPACE, 0);
  if (sym == 0 || SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      if (!strcmp (name, "int"))
	return builtin_type_int;
      if (!strcmp (name, "long"))
	return builtin_type_long;
      if (!strcmp (name, "short"))
	return builtin_type_short;
      if (!strcmp (name, "char"))
	return builtin_type_char;
      if (!strcmp (name, "float"))
	return builtin_type_float;
      if (!strcmp (name, "double"))
	return builtin_type_double;
      if (!strcmp (name, "void"))
	return builtin_type_void;

      if (noerr)
	return 0;
      error ("No type named %s.", name);
    }
  return SYMBOL_TYPE (sym);
}

struct type *
lookup_unsigned_typename (name)
     char *name;
{
  if (!strcmp (name, "int"))
    return builtin_type_unsigned_int;
  if (!strcmp (name, "long"))
    return builtin_type_unsigned_long;
  if (!strcmp (name, "short"))
    return builtin_type_unsigned_short;
  if (!strcmp (name, "char"))
    return builtin_type_unsigned_char;
  error ("No type named unsigned %s.", name);
}

/* Lookup a structure type named "struct NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_struct (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym 
    = lookup_symbol (name, block, STRUCT_NAMESPACE, 0);

  if (sym == 0)
    error ("No struct type named %s.", name);
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    error ("This context has class, union or enum %s, not a struct.", name);
  return SYMBOL_TYPE (sym);
}

/* Lookup a union type named "union NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_union (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym 
    = lookup_symbol (name, block, STRUCT_NAMESPACE, 0);

  if (sym == 0)
    error ("No union type named %s.", name);
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_UNION)
    error ("This context has class, struct or enum %s, not a union.", name);
  return SYMBOL_TYPE (sym);
}

/* Lookup an enum type named "enum NAME",
   visible in lexical block BLOCK.  */

struct type *
lookup_enum (name, block)
     char *name;
     struct block *block;
{
  register struct symbol *sym 
    = lookup_symbol (name, block, STRUCT_NAMESPACE, 0);
  if (sym == 0)
    error ("No enum type named %s.", name);
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_ENUM)
    error ("This context has class, struct or union %s, not an enum.", name);
  return SYMBOL_TYPE (sym);
}

/* Given a type TYPE, lookup the type of the component of type named
   NAME.  */

struct type *
lookup_struct_elt_type (type, name)
     struct type *type;
     char *name;
{
  struct type *t;
  int i;
  char *errmsg;

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      && TYPE_CODE (type) != TYPE_CODE_UNION)
    {
      terminal_ours ();
      fflush (stdout);
      fprintf (stderr, "Type ");
      type_print (type, "", stderr, -1);
      fprintf (stderr, " is not a structure or union type.\n");
      return_to_top_level ();
    }

  for (i = TYPE_NFIELDS (type) - 1; i >= 0; i--)
    if (!strcmp (TYPE_FIELD_NAME (type, i), name))
      return TYPE_FIELD_TYPE (type, i);

  terminal_ours ();
  fflush (stdout);
  fprintf (stderr, "Type ");
  type_print (type, "", stderr, -1);
  fprintf (stderr, " has no component named %s\n", name);
  return_to_top_level ();
}

/* Given a type TYPE, return a type of pointers to that type.
   May need to construct such a type if this is the first use.

   C++: use TYPE_MAIN_VARIANT and TYPE_CHAIN to keep pointer
   to member types under control.  */

struct type *
lookup_pointer_type (type)
     struct type *type;
{
  register struct type *ptype = TYPE_POINTER_TYPE (type);
  if (ptype) return TYPE_MAIN_VARIANT (ptype);

  /* This is the first time anyone wanted a pointer to a TYPE.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    ptype  = (struct type *) xmalloc (sizeof (struct type));
  else
    ptype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  bzero (ptype, sizeof (struct type));
  TYPE_MAIN_VARIANT (ptype) = ptype;
  TYPE_TARGET_TYPE (ptype) = type;
  TYPE_POINTER_TYPE (type) = ptype;
  /* New type is permanent if type pointed to is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (ptype) |= TYPE_FLAG_PERM;
  /* We assume the machine has only one representation for pointers!  */
  TYPE_LENGTH (ptype) = sizeof (char *);
  TYPE_CODE (ptype) = TYPE_CODE_PTR;
  return ptype;
}

struct type *
lookup_reference_type (type)
     struct type *type;
{
  register struct type *rtype = TYPE_REFERENCE_TYPE (type);
  if (rtype) return TYPE_MAIN_VARIANT (rtype);

  /* This is the first time anyone wanted a pointer to a TYPE.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    rtype  = (struct type *) xmalloc (sizeof (struct type));
  else
    rtype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  bzero (rtype, sizeof (struct type));
  TYPE_MAIN_VARIANT (rtype) = rtype;
  TYPE_TARGET_TYPE (rtype) = type;
  TYPE_REFERENCE_TYPE (type) = rtype;
  /* New type is permanent if type pointed to is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (rtype) |= TYPE_FLAG_PERM;
  /* We assume the machine has only one representation for pointers!  */
  TYPE_LENGTH (rtype) = sizeof (char *);
  TYPE_CODE (rtype) = TYPE_CODE_REF;
  return rtype;
}


/* Implement direct support for MEMBER_TYPE in GNU C++.
   May need to construct such a type if this is the first use.
   The TYPE is the type of the member.  The DOMAIN is the type
   of the aggregate that the member belongs to.  */

struct type *
lookup_member_type (type, domain)
     struct type *type, *domain;
{
  register struct type *mtype = TYPE_MAIN_VARIANT (type);
  struct type *main_type;

  main_type = mtype;
  while (mtype)
    {
      if (TYPE_DOMAIN_TYPE (mtype) == domain)
	return mtype;
      mtype = TYPE_NEXT_VARIANT (mtype);
    }

  /* This is the first time anyone wanted this member type.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    mtype  = (struct type *) xmalloc (sizeof (struct type));
  else
    mtype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  bzero (mtype, sizeof (struct type));
  if (main_type == 0)
    main_type = mtype;
  else
    {
      TYPE_NEXT_VARIANT (mtype) = TYPE_NEXT_VARIANT (main_type);
      TYPE_NEXT_VARIANT (main_type) = mtype;
    }
  TYPE_MAIN_VARIANT (mtype) = main_type;
  TYPE_TARGET_TYPE (mtype) = type;
  TYPE_DOMAIN_TYPE (mtype) = domain;
  /* New type is permanent if type pointed to is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (mtype) |= TYPE_FLAG_PERM;

  /* In practice, this is never used.  */
  TYPE_LENGTH (mtype) = 1;
  TYPE_CODE (mtype) = TYPE_CODE_MEMBER;

#if 0
  /* Now splice in the new member pointer type.  */
  if (main_type)
    {
      /* This type was not "smashed".  */
      TYPE_CHAIN (mtype) = TYPE_CHAIN (main_type);
      TYPE_CHAIN (main_type) = mtype;
    }
#endif

  return mtype;
}

struct type *
lookup_method_type (type, domain, args)
     struct type *type, *domain, **args;
{
  register struct type *mtype = TYPE_MAIN_VARIANT (type);
  struct type *main_type;

  main_type = mtype;
  while (mtype)
    {
      if (TYPE_DOMAIN_TYPE (mtype) == domain)
	{
	  struct type **t1 = args;
	  struct type **t2 = TYPE_ARG_TYPES (mtype);
	  if (t2)
	    {
	      int i;
	      for (i = 0; t1[i] != 0 && t1[i]->code != TYPE_CODE_VOID; i++)
		if (t1[i] != t2[i])
		  break;
	      if (t1[i] == t2[i])
		return mtype;
	    }
	}
      mtype = TYPE_NEXT_VARIANT (mtype);
    }

  /* This is the first time anyone wanted this member type.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    mtype  = (struct type *) xmalloc (sizeof (struct type));
  else
    mtype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  bzero (mtype, sizeof (struct type));
  if (main_type == 0)
    main_type = mtype;
  else
    {
      TYPE_NEXT_VARIANT (mtype) = TYPE_NEXT_VARIANT (main_type);
      TYPE_NEXT_VARIANT (main_type) = mtype;
    }
  TYPE_MAIN_VARIANT (mtype) = main_type;
  TYPE_TARGET_TYPE (mtype) = type;
  TYPE_DOMAIN_TYPE (mtype) = domain;
  TYPE_ARG_TYPES (mtype) = args;
  /* New type is permanent if type pointed to is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (mtype) |= TYPE_FLAG_PERM;

  /* In practice, this is never used.  */
  TYPE_LENGTH (mtype) = 1;
  TYPE_CODE (mtype) = TYPE_CODE_METHOD;

#if 0
  /* Now splice in the new member pointer type.  */
  if (main_type)
    {
      /* This type was not "smashed".  */
      TYPE_CHAIN (mtype) = TYPE_CHAIN (main_type);
      TYPE_CHAIN (main_type) = mtype;
    }
#endif

  return mtype;
}

/* Given a type TYPE, return a type which has offset OFFSET,
   via_virtual VIA_VIRTUAL, and via_public VIA_PUBLIC.
   May need to construct such a type if none exists.  */
struct type *
lookup_basetype_type (type, offset, via_virtual, via_public)
     struct type *type;
     int offset;
     int via_virtual, via_public;
{
  register struct type *btype = TYPE_MAIN_VARIANT (type);
  struct type *main_type;

  if (offset != 0)
    {
      printf ("Internal error: type offset non-zero in lookup_basetype_type");
      offset = 0;
    }

  main_type = btype;
  while (btype)
    {
      if (/* TYPE_OFFSET (btype) == offset
	  && */ TYPE_VIA_PUBLIC (btype) == via_public
	  && TYPE_VIA_VIRTUAL (btype) == via_virtual)
	return btype;
      btype = TYPE_NEXT_VARIANT (btype);
    }

  /* This is the first time anyone wanted this member type.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    btype  = (struct type *) xmalloc (sizeof (struct type));
  else
    btype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  if (main_type == 0)
    {
      main_type = btype;
      bzero (btype, sizeof (struct type));
      TYPE_MAIN_VARIANT (btype) = main_type;
    }
  else
    {
      bcopy (main_type, btype, sizeof (struct type));
      TYPE_NEXT_VARIANT (main_type) = btype;
    }
/* TYPE_OFFSET (btype) = offset; */
  if (via_public)
    TYPE_FLAGS (btype) |= TYPE_FLAG_VIA_PUBLIC;
  if (via_virtual)
    TYPE_FLAGS (btype) |= TYPE_FLAG_VIA_VIRTUAL;
  /* New type is permanent if type pointed to is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (btype) |= TYPE_FLAG_PERM;

  /* In practice, this is never used.  */
  TYPE_LENGTH (btype) = 1;
  TYPE_CODE (btype) = TYPE_CODE_STRUCT;

  return btype;
}

/* Given a type TYPE, return a type of functions that return that type.
   May need to construct such a type if this is the first use.  */

struct type *
lookup_function_type (type)
     struct type *type;
{
  register struct type *ptype = TYPE_FUNCTION_TYPE (type);
  if (ptype) return ptype;

  /* This is the first time anyone wanted a function returning a TYPE.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    ptype  = (struct type *) xmalloc (sizeof (struct type));
  else
    ptype  = (struct type *) obstack_alloc (symbol_obstack,
					    sizeof (struct type));

  bzero (ptype, sizeof (struct type));
  TYPE_TARGET_TYPE (ptype) = type;
  TYPE_FUNCTION_TYPE (type) = ptype;
  /* New type is permanent if type returned is permanent.  */
  if (TYPE_FLAGS (type) & TYPE_FLAG_PERM)
    TYPE_FLAGS (ptype) |= TYPE_FLAG_PERM;
  TYPE_LENGTH (ptype) = 1;
  TYPE_CODE (ptype) = TYPE_CODE_FUNC;
  TYPE_NFIELDS (ptype) = 0;
  return ptype;
}

/* Create an array type.  Elements will be of type TYPE, and there will
   be NUM of them.

   Eventually this should be extended to take two more arguments which
   specify the bounds of the array and the type of the index.
   It should also be changed to be a "lookup" function, with the
   appropriate data structures added to the type field.
   Then read array type should call here.  */

struct type *
create_array_type (element_type, number)
     struct type *element_type;
     int number;
{
  struct type *result_type = (struct type *)
    obstack_alloc (symbol_obstack, sizeof (struct type));

  bzero (result_type, sizeof (struct type));

  TYPE_CODE (result_type) = TYPE_CODE_ARRAY;
  TYPE_TARGET_TYPE (result_type) = element_type;
  TYPE_LENGTH (result_type) = number * TYPE_LENGTH (element_type);
  TYPE_NFIELDS (result_type) = 1;
  TYPE_FIELDS (result_type) =
    (struct field *) obstack_alloc (symbol_obstack, sizeof (struct field));
  TYPE_FIELD_TYPE (result_type, 0) = builtin_type_int;
  TYPE_VPTR_FIELDNO (result_type) = -1;

  return result_type;
}


/* Smash TYPE to be a type of pointers to TO_TYPE.
   If TO_TYPE is not permanent and has no pointer-type yet,
   record TYPE as its pointer-type.  */

void
smash_to_pointer_type (type, to_type)
     struct type *type, *to_type;
{
  int type_permanent = (TYPE_FLAGS (type) & TYPE_FLAG_PERM);
  
  bzero (type, sizeof (struct type));
  TYPE_TARGET_TYPE (type) = to_type;
  /* We assume the machine has only one representation for pointers!  */
  TYPE_LENGTH (type) = sizeof (char *);
  TYPE_CODE (type) = TYPE_CODE_PTR;

  TYPE_MAIN_VARIANT (type) = type;

  if (type_permanent)
    TYPE_FLAGS (type) |= TYPE_FLAG_PERM;

  if (TYPE_POINTER_TYPE (to_type) == 0
      && (!(TYPE_FLAGS (to_type) & TYPE_FLAG_PERM)
	  || type_permanent))
    {
      TYPE_POINTER_TYPE (to_type) = type;
    }
}

/* Smash TYPE to be a type of members of DOMAIN with type TO_TYPE.  */

void
smash_to_member_type (type, domain, to_type)
     struct type *type, *domain, *to_type;
{
  bzero (type, sizeof (struct type));
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;

  /* In practice, this is never needed.  */
  TYPE_LENGTH (type) = 1;
  TYPE_CODE (type) = TYPE_CODE_MEMBER;

  TYPE_MAIN_VARIANT (type) = lookup_member_type (domain, to_type);
}

/* Smash TYPE to be a type of method of DOMAIN with type TO_TYPE.  */

void
smash_to_method_type (type, domain, to_type, args)
     struct type *type, *domain, *to_type, **args;
{
  bzero (type, sizeof (struct type));
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_DOMAIN_TYPE (type) = domain;
  TYPE_ARG_TYPES (type) = args;

  /* In practice, this is never needed.  */
  TYPE_LENGTH (type) = 1;
  TYPE_CODE (type) = TYPE_CODE_METHOD;

  TYPE_MAIN_VARIANT (type) = lookup_method_type (domain, to_type, args);
}

/* Smash TYPE to be a type of reference to TO_TYPE.
   If TO_TYPE is not permanent and has no pointer-type yet,
   record TYPE as its pointer-type.  */

void
smash_to_reference_type (type, to_type)
     struct type *type, *to_type;
{
  int type_permanent = (TYPE_FLAGS (type) & TYPE_FLAG_PERM);

  bzero (type, sizeof (struct type));
  TYPE_TARGET_TYPE (type) = to_type;
  /* We assume the machine has only one representation for pointers!  */
  TYPE_LENGTH (type) = sizeof (char *);
  TYPE_CODE (type) = TYPE_CODE_REF;

  TYPE_MAIN_VARIANT (type) = type;

  if (type_permanent)
    TYPE_FLAGS (type) |= TYPE_FLAG_PERM;

  if (TYPE_REFERENCE_TYPE (to_type) == 0
      && (!(TYPE_FLAGS (to_type) & TYPE_FLAG_PERM)
	  || type_permanent))
    {
      TYPE_REFERENCE_TYPE (to_type) = type;
    }
}

/* Smash TYPE to be a type of functions returning TO_TYPE.
   If TO_TYPE is not permanent and has no function-type yet,
   record TYPE as its function-type.  */

void
smash_to_function_type (type, to_type)
     struct type *type, *to_type;
{
  int type_permanent = (TYPE_FLAGS (type) & TYPE_FLAG_PERM);

  bzero (type, sizeof (struct type));
  TYPE_TARGET_TYPE (type) = to_type;
  TYPE_LENGTH (type) = 1;
  TYPE_CODE (type) = TYPE_CODE_FUNC;
  TYPE_NFIELDS (type) = 0;

  if (type_permanent)
    TYPE_FLAGS (type) |= TYPE_FLAG_PERM;

  if (TYPE_FUNCTION_TYPE (to_type) == 0
      && (!(TYPE_FLAGS (to_type) & TYPE_FLAG_PERM)
	  || type_permanent))
    {
      TYPE_FUNCTION_TYPE (to_type) = type;
    }
}

/* Find which partial symtab on the partial_symtab_list contains
   PC.  Return 0 if none.  */

struct partial_symtab *
find_pc_psymtab (pc)
     register CORE_ADDR pc;
{
  register struct partial_symtab *ps;

  for (ps = partial_symtab_list; ps; ps = ps->next)
    if (pc >= ps->textlow && pc < ps->texthigh)
      return ps;

  return 0;
}

/* Find which partial symbol within a psymtab contains PC.  Return 0
   if none.  Check all psymtabs if PSYMTAB is 0.  */
struct partial_symbol *
find_pc_psymbol (psymtab, pc)
     struct partial_symtab *psymtab;
     CORE_ADDR pc;
{
  struct partial_symbol *best, *p;
  int best_pc;
  
  if (!psymtab)
    psymtab = find_pc_psymtab (pc);
  if (!psymtab)
    return 0;

  best_pc = psymtab->textlow - 1;

  for (p = static_psymbols.list + psymtab->statics_offset;
       (p - (static_psymbols.list + psymtab->statics_offset)
	< psymtab->n_static_syms);
       p++)
    if (SYMBOL_NAMESPACE (p) == VAR_NAMESPACE
	&& SYMBOL_CLASS (p) == LOC_BLOCK
	&& pc >= SYMBOL_VALUE (p)
	&& SYMBOL_VALUE (p) > best_pc)
      {
	best_pc = SYMBOL_VALUE (p);
	best = p;
      }
  if (best_pc == psymtab->textlow - 1)
    return 0;
  return best;
}


static struct symbol *lookup_block_symbol ();

/* Find the definition for a specified symbol name NAME
   in namespace NAMESPACE, visible from lexical block BLOCK.
   Returns the struct symbol pointer, or zero if no symbol is found. 
   C++: if IS_A_FIELD_OF_THIS is nonzero on entry, check to see if
   NAME is a field of the current implied argument `this'.  If so set
   *IS_A_FIELD_OF_THIS to 1, otherwise set it to zero. 
   BLOCK_FOUND is set to the block in which NAME is found (in the case of
   a field of `this', value_of_this sets BLOCK_FOUND to the proper value.) */

struct symbol *
lookup_symbol (name, block, namespace, is_a_field_of_this)
     char *name;
     register struct block *block;
     enum namespace namespace;
     int *is_a_field_of_this;
{
  register int i, n;
  register struct symbol *sym;
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct partial_symbol *psym;
  struct blockvector *bv;

  /* Search specified block and its superiors.  */

  while (block != 0)
    {
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  return sym;
	}
      block = BLOCK_SUPERBLOCK (block);
    }

  /* C++: If requested to do so by the caller, 
     check to see if NAME is a field of `this'. */
  if (is_a_field_of_this)
    {
      struct value *v = value_of_this (0);
      
      *is_a_field_of_this = 0;
      if (v && check_field (v, name))
	{
	  *is_a_field_of_this = 1;	    
	  return 0;
	}
    }

  /* Now search all global blocks.  Do the symtab's first, then
     check the psymtab's */

  for (s = symtab_list; s; s = s->next)
    {
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, 0);
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  return sym;
	}
    }

  /* Check for the possibility of the symbol being a global function
     that is stored on the misc function vector.  Eventually, all
     global symbols might be resolved in this way.  */
  
  if (namespace == VAR_NAMESPACE)
    {
      int index = lookup_misc_func (name);

      if (index == -1)
	{ /* Look for a mangled C++ name for NAME. */
	  int name_len = strlen (name);
	  for (index = misc_function_count; --index >= 0; )
	      /* Assume orginal name is prefix of mangled name. */
	      if (!strncmp (misc_function_vector[index].name, name, name_len))
		{
		  char *demangled =
		      cplus_demangle(misc_function_vector[index].name, -1);
		  if (demangled != NULL)
		    {
		      int cond = strcmp (demangled, name);
		      free (demangled);
		      if (!cond)
			  break;
		    }
	        }
	  /* Loop terminates on no match with index == -1. */
        }

      if (index != -1)
	{
	  ps = find_pc_psymtab (misc_function_vector[index].address);
	  if (ps && !ps->readin)
	    {
	      s = psymtab_to_symtab (ps);
	      bv = BLOCKVECTOR (s);
	      block = BLOCKVECTOR_BLOCK (bv, 0);
	      sym = lookup_block_symbol (block, name, namespace);
	      /* sym == 0 if symbol was found in the psymtab but not
		 in the symtab.
		 Return 0 to use the misc_function definition of "foo_".

		 This happens for Fortran  "foo_" symbols,
		 which are "foo" in the symtab.

		 This can also happen if "asm" is used to make a
		 regular symbol but not a debugging symbol, e.g.
		 asm(".globl _main");
		 asm("_main:");
		 */
	      
	      return sym;
	    }
	}
    }
      
  if (psym = lookup_partial_symbol (name, 1, namespace))
    {
      ps = psym->pst;
      s = psymtab_to_symtab(ps);
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, 0);
      sym = lookup_block_symbol (block, name, namespace);
      if (!sym)
	fatal ("Internal: global symbol found in psymtab but not in symtab");
      return sym;
    }

  /* Now search all per-file blocks.
     Not strictly correct, but more useful than an error.
     Do the symtabs first, then check the psymtabs */

  for (s = symtab_list; s; s = s->next)
    {
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, 1);
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  return sym;
	}
    }

  if (psym = lookup_partial_symbol(name, 0, namespace))
    {
      ps = psym->pst;
      s = psymtab_to_symtab(ps);
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, 1);
      sym = lookup_block_symbol (block, name, namespace);
      if (!sym)
	fatal ("Internal: static symbol found in psymtab but not in symtab");
      return sym;
    }

  return 0;
}

/* Look, in partial_symtab PST, for symbol NAME.  Check the global
   symbols if GLOBAL, the static symbols if not */

static struct partial_symbol *
lookup_partial_symbol (name, global, namespace)
     register char *name;
     register int global;
     register enum namespace namespace;
{
  register struct partial_symbol *start, *psym;
  register struct partial_symbol *top, *bottom, *center;
  register struct partial_symtab *pst;
  register int length;

  if (global)
    {
      start = global_psymbols.list;
      length = global_psymbols.next - start;
    }
  else
    {
      start = static_psymbols.list;
      length = static_psymbols.next - start;
    }

  if (!length)
    return (struct partial_symbol *) 0;
  
  /* Binary search.  This search is guarranteed to end with center
     pointing at the earliest partial symbol with the correct
     name.  At that point *all* partial symbols with that name
     will be checked against the correct namespace. */
  bottom = start;
  top = start + length - 1;
  while (top > bottom)
    {
      center = bottom + (top - bottom) / 2;

      assert (center < top);
      
      if (strcmp (SYMBOL_NAME (center), name) >= 0)
	top = center;
      else
	bottom = center + 1;
    }
  assert (top == bottom);
  
  while (strcmp (SYMBOL_NAME (top), name) == 0)
    {
      if (!top->pst->readin && SYMBOL_NAMESPACE (top) == namespace)
	return top;
      top ++;
    }

  return (struct partial_symbol *) 0;
}

/* Look for a symbol in block BLOCK.  */

static struct symbol *
lookup_block_symbol (block, name, namespace)
     register struct block *block;
     char *name;
     enum namespace namespace;
{
  register int bot, top, inc;
  register struct symbol *sym, *parameter_sym;

  top = BLOCK_NSYMS (block);
  bot = 0;

  /* If the blocks's symbols were sorted, start with a binary search.  */

  if (BLOCK_SHOULD_SORT (block))
    {
      /* First, advance BOT to not far before
	 the first symbol whose name is NAME.  */

      while (1)
	{
	  inc = (top - bot + 1);
	  /* No need to keep binary searching for the last few bits worth.  */
	  if (inc < 4)
	    break;
	  inc = (inc >> 1) + bot;
	  sym = BLOCK_SYM (block, inc);
	  if (SYMBOL_NAME (sym)[0] < name[0])
	    bot = inc;
	  else if (SYMBOL_NAME (sym)[0] > name[0])
	    top = inc;
	  else if (strcmp (SYMBOL_NAME (sym), name) < 0)
	    bot = inc;
	  else
	    top = inc;
	}

      /* Now scan forward until we run out of symbols,
	 find one whose name is greater than NAME,
	 or find one we want.
	 If there is more than one symbol with the right name and namespace,
	 we return the first one.  dbxread.c is careful to make sure
	 that if one is a register then it comes first.  */

      top = BLOCK_NSYMS (block);
      while (bot < top)
	{
	  sym = BLOCK_SYM (block, bot);
	  inc = SYMBOL_NAME (sym)[0] - name[0];
	  if (inc == 0)
	    inc = strcmp (SYMBOL_NAME (sym), name);
	  if (inc == 0 && SYMBOL_NAMESPACE (sym) == namespace)
	    return sym;
	  if (inc > 0)
	    return 0;
	  bot++;
	}
      return 0;
    }

  /* Here if block isn't sorted.
     This loop is equivalent to the loop above,
     but hacked greatly for speed.

     Note that parameter symbols do not always show up last in the
     list; this loop makes sure to take anything else other than
     parameter symbols first; it only uses parameter symbols as a
     last resort.  Note that this only takes up extra computation
     time on a match.  */

  parameter_sym = (struct symbol *) 0;
  top = BLOCK_NSYMS (block);
  inc = name[0];
  while (bot < top)
    {
      sym = BLOCK_SYM (block, bot);
      if (SYMBOL_NAME (sym)[0] == inc
	  && !strcmp (SYMBOL_NAME (sym), name)
	  && SYMBOL_NAMESPACE (sym) == namespace)
	{
	  if (SYMBOL_CLASS (sym) == LOC_ARG
	      || SYMBOL_CLASS (sym) == LOC_REF_ARG
	      || SYMBOL_CLASS (sym) == LOC_REGPARM)
	    parameter_sym = sym;
	  else
	    return sym;
	}
      bot++;
    }
  return parameter_sym;		/* Will be 0 if not found. */
}

/* Return the symbol for the function which contains a specified
   lexical block, described by a struct block BL.  */

struct symbol *
block_function (bl)
     struct block *bl;
{
  while (BLOCK_FUNCTION (bl) == 0 && BLOCK_SUPERBLOCK (bl) != 0)
    bl = BLOCK_SUPERBLOCK (bl);

  return BLOCK_FUNCTION (bl);
}

/* Subroutine of find_pc_line */

struct symtab *
find_pc_symtab (pc)
     register CORE_ADDR pc;
{
  register struct block *b;
  struct blockvector *bv;
  register struct symtab *s;
  register struct partial_symtab *ps;

  /* Search all symtabs for one whose file contains our pc */

  for (s = symtab_list; s; s = s->next)
    {
      bv = BLOCKVECTOR (s);
      b = BLOCKVECTOR_BLOCK (bv, 0);
      if (BLOCK_START (b) <= pc
	  && BLOCK_END (b) > pc)
	break;
    }

  if (!s)
    {
      ps = find_pc_psymtab (pc);
      if (ps && ps->readin)
	fatal ("Internal error: pc in read in psymtab, but not in symtab.");

      if (ps)
	s = psymtab_to_symtab (ps);
    }

  return s;
}

/* Find the source file and line number for a given PC value.
   Return a structure containing a symtab pointer, a line number,
   and a pc range for the entire source line.
   The value's .pc field is NOT the specified pc.
   NOTCURRENT nonzero means, if specified pc is on a line boundary,
   use the line that ends there.  Otherwise, in that case, the line
   that begins there is used.  */

struct symtab_and_line
find_pc_line (pc, notcurrent)
     CORE_ADDR pc;
     int notcurrent;
{
  struct symtab *s;
  register struct linetable *l;
  register int len;
  register int i;
  register struct linetable_entry *item;
  struct symtab_and_line value;
  struct blockvector *bv;

  /* Info on best line seen so far, and where it starts, and its file.  */

  int best_line = 0;
  CORE_ADDR best_pc = 0;
  CORE_ADDR best_end = 0;
  struct symtab *best_symtab = 0;

  /* Store here the first line number
     of a file which contains the line at the smallest pc after PC.
     If we don't find a line whose range contains PC,
     we will use a line one less than this,
     with a range from the start of that file to the first line's pc.  */
  int alt_line = 0;
  CORE_ADDR alt_pc = 0;
  struct symtab *alt_symtab = 0;

  /* Info on best line seen in this file.  */

  int prev_line;
  CORE_ADDR prev_pc;

  /* Info on first line of this file.  */

  int first_line;
  CORE_ADDR first_pc;

  /* If this pc is not from the current frame,
     it is the address of the end of a call instruction.
     Quite likely that is the start of the following statement.
     But what we want is the statement containing the instruction.
     Fudge the pc to make sure we get that.  */

  if (notcurrent) pc -= 1;

  s = find_pc_symtab (pc);
  if (s == 0)
    {
      value.symtab = 0;
      value.line = 0;
      value.pc = pc;
      value.end = 0;
      return value;
    }

  bv = BLOCKVECTOR (s);

  /* Look at all the symtabs that share this blockvector.
     They all have the same apriori range, that we found was right;
     but they have different line tables.  */

  for (; s && BLOCKVECTOR (s) == bv; s = s->next)
    {
      /* Find the best line in this symtab.  */
      l = LINETABLE (s);
      len = l->nitems;
      prev_line = -1;
      first_line = -1;
      for (i = 0; i < len; i++)
	{
	  item = &(l->item[i]);
	  
	  if (first_line < 0)
	    {
	      first_line = item->line;
	      first_pc = item->pc;
	    }
	  /* Return the last line that did not start after PC.  */
	  if (pc >= item->pc)
	    {
	      prev_line = item->line;
	      prev_pc = item->pc;
	    }
	  else
	    break;
	}

      /* Is this file's best line closer than the best in the other files?
	 If so, record this file, and its best line, as best so far.  */
      if (prev_line >= 0 && prev_pc > best_pc)
	{
	  best_pc = prev_pc;
	  best_line = prev_line;
	  best_symtab = s;
	  if (i < len)
	    best_end = item->pc;
	  else
	    best_end = 0;
	}
      /* Is this file's first line closer than the first lines of other files?
	 If so, record this file, and its first line, as best alternate.  */
      if (first_line >= 0 && first_pc > pc
	  && (alt_pc == 0 || first_pc < alt_pc))
	{
	  alt_pc = first_pc;
	  alt_line = first_line;
	  alt_symtab = s;
	}
    }
  if (best_symtab == 0)
    {
      value.symtab = alt_symtab;
      value.line = alt_line - 1;
      value.pc = BLOCK_END (BLOCKVECTOR_BLOCK (bv, 0));
      value.end = alt_pc;
    }
  else
    {
      value.symtab = best_symtab;
      value.line = best_line;
      value.pc = best_pc;
      value.end = (best_end ? best_end
		   : (alt_pc ? alt_pc
		      : BLOCK_END (BLOCKVECTOR_BLOCK (bv, 0))));
    }
  return value;
}

/* Find the PC value for a given source file and line number.
   Returns zero for invalid line number.
   The source file is specified with a struct symtab.  */

CORE_ADDR
find_line_pc (symtab, line)
     struct symtab *symtab;
     int line;
{
  register struct linetable *l;
  register int index;
  int dummy;

  if (symtab == 0)
    return 0;
  l = LINETABLE (symtab);
  index = find_line_common(l, line, &dummy);
  return index ? l->item[index].pc : 0;
}

/* Find the range of pc values in a line.
   Store the starting pc of the line into *STARTPTR
   and the ending pc (start of next line) into *ENDPTR.
   Returns 1 to indicate success.
   Returns 0 if could not find the specified line.  */

int
find_line_pc_range (symtab, thisline, startptr, endptr)
     struct symtab *symtab;
     int thisline;
     CORE_ADDR *startptr, *endptr;
{
  register struct linetable *l;
  register int index;
  int exact_match;		/* did we get an exact linenumber match */
  register CORE_ADDR prev_pc;
  CORE_ADDR last_pc;

  if (symtab == 0)
    return 0;

  l = LINETABLE (symtab);
  index = find_line_common (l, thisline, &exact_match);
  if (index)
    {
      *startptr = l->item[index].pc;
      /* If we have not seen an entry for the specified line,
	 assume that means the specified line has zero bytes.  */
      if (!exact_match || index == l->nitems-1)
	*endptr = *startptr;
      else
	/* Perhaps the following entry is for the following line.
	   It's worth a try.  */
	if (l->item[index+1].line == thisline + 1)
	  *endptr = l->item[index+1].pc;
	else
	  *endptr = find_line_pc (symtab, thisline+1);
      return 1;
    }

  return 0;
}

/* Given a line table and a line number, return the index into the line
   table for the pc of the nearest line whose number is >= the specified one.
   Return 0 if none is found.  The value is never zero is it is an index.

   Set *EXACT_MATCH nonzero if the value returned is an exact match.  */

static int
find_line_common (l, lineno, exact_match)
     register struct linetable *l;
     register int lineno;
     int *exact_match;
{
  register int i;
  register int len;

  /* BEST is the smallest linenumber > LINENO so far seen,
     or 0 if none has been seen so far.
     BEST_INDEX identifies the item for it.  */

  int best_index = 0;
  int best = 0;

  int nextline = -1;

  if (lineno <= 0)
    return 0;

  len = l->nitems;
  for (i = 0; i < len; i++)
    {
      register struct linetable_entry *item = &(l->item[i]);

      if (item->line == lineno)
	{
	  *exact_match = 1;
	  return i;
	}

      if (item->line > lineno && (best == 0 || item->line < best))
	{
	  best = item->line;
	  best_index = i;
	}
    }

  /* If we got here, we didn't get an exact match.  */

  *exact_match = 0;
  return best_index;
}

int
find_pc_line_pc_range (pc, startptr, endptr)
     CORE_ADDR pc;
     CORE_ADDR *startptr, *endptr;
{
  struct symtab_and_line sal;
  sal = find_pc_line (pc, 0);
  *startptr = sal.pc;
  *endptr = sal.end;
  return sal.symtab != 0;
}

/* Parse a string that specifies a line number.
   Pass the address of a char * variable; that variable will be
   advanced over the characters actually parsed.

   The string can be:

   LINENUM -- that line number in current file.  PC returned is 0.
   FILE:LINENUM -- that line in that file.  PC returned is 0.
   FUNCTION -- line number of openbrace of that function.
      PC returned is the start of the function.
   FILE:FUNCTION -- likewise, but prefer functions in that file.
   *EXPR -- line in which address EXPR appears.

   FUNCTION may be an undebuggable function found in misc_function_vector.

   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside a function when a function is specified.

   DEFAULT_SYMTAB specifies the file to use if none is specified.
   It defaults to current_source_symtab.
   DEFAULT_LINE specifies the line number to use for relative
   line numbers (that start with signs).  Defaults to current_source_line.

   Note that it is possible to return zero for the symtab
   if no file is validly specified.  Callers must check that.
   Also, the line number returned may be invalid.  */

struct symtabs_and_lines
decode_line_1 (argptr, funfirstline, default_symtab, default_line)
     char **argptr;
     int funfirstline;
     struct symtab *default_symtab;
     int default_line;
{
  struct symtabs_and_lines decode_line_2 ();
  struct symtabs_and_lines values;
  struct symtab_and_line value;
  register char *p, *p1;
  register struct symtab *s;
  register struct symbol *sym;
  register CORE_ADDR pc;
  register int i;
  char *copy;
  struct symbol *sym_class;
  char *class_name, *method_name, *phys_name;
  int method_counter;
  int i1;
  struct symbol **sym_arr;
  struct type *t, *field;
  char **physnames;
  
  /* Defaults have defaults.  */

  if (default_symtab == 0)
    {
      default_symtab = current_source_symtab;
      default_line = current_source_line;
    }

  /* See if arg is *PC */

  if (**argptr == '*')
    {
      (*argptr)++;
      pc = parse_and_eval_address_1 (argptr);
      values.sals = (struct symtab_and_line *)
	malloc (sizeof (struct symtab_and_line));
      values.nelts = 1;
      values.sals[0] = find_pc_line (pc, 0);
      values.sals[0].pc = pc;
      return values;
    }

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */

  s = 0;

  for (p = *argptr; *p; p++)
    {
      if (p[0] == ':' || p[0] == ' ' || p[0] == '\t')
	break;
    }
  while (p[0] == ' ' || p[0] == '\t') p++;

  if (p[0] == ':')
    {

      /*  C++  */
      if (p[1] ==':')
	{
	  /* Extract the class name.  */
	  p1 = p;
	  while (p != *argptr && p[-1] == ' ') --p;
	  copy = (char *) alloca (p - *argptr + 1);
	  bcopy (*argptr, copy, p - *argptr);
	  copy[p - *argptr] = 0;

	  /* Discard the class name from the arg.  */
	  p = p1 + 2;
	  while (*p == ' ' || *p == '\t') p++;
	  *argptr = p;

	  sym_class = lookup_symbol (copy, 0, STRUCT_NAMESPACE, 0);
       
	  if (sym_class &&
	      (TYPE_CODE (SYMBOL_TYPE (sym_class)) == TYPE_CODE_STRUCT
	       || TYPE_CODE (SYMBOL_TYPE (sym_class)) == TYPE_CODE_UNION))
	    {
	      /* Arg token is not digits => try it as a function name
		 Find the next token (everything up to end or next whitespace). */
	      p = *argptr;
	      while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p !=':') p++;
	      copy = (char *) alloca (p - *argptr + 1);
	      bcopy (*argptr, copy, p - *argptr);
	      copy[p - *argptr] = '\0';

	      /* no line number may be specified */
	      while (*p == ' ' || *p == '\t') p++;
	      *argptr = p;

	      sym = 0;
	      i1 = 0;		/*  counter for the symbol array */
	      t = SYMBOL_TYPE (sym_class);
	      sym_arr = (struct symbol **) alloca(TYPE_NFN_FIELDS_TOTAL (t) * sizeof(struct symbol*));
	      physnames = (char **) alloca (TYPE_NFN_FIELDS_TOTAL (t) * sizeof(char*));

	      if (destructor_name_p (copy, t))
		{
		  /* destructors are a special case.  */
		  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, 0);
		  int len = TYPE_FN_FIELDLIST_LENGTH (t, 0) - 1;
		  phys_name = TYPE_FN_FIELD_PHYSNAME (f, len);
		  physnames[i1] = (char *)alloca (strlen (phys_name) + 1);
		  strcpy (physnames[i1], phys_name);
		  sym_arr[i1] = lookup_symbol (phys_name, SYMBOL_BLOCK_VALUE (sym_class), VAR_NAMESPACE, 0);
		  if (sym_arr[i1]) i1++;
		}
	      else while (t)
		{
		  class_name = TYPE_NAME (t);
		  /* Ignore this class if it doesn't have a name.
		     This prevents core dumps, but is just a workaround
		     because we might not find the function in
		     certain cases, such as
		     struct D {virtual int f();}
		     struct C : D {virtual int g();}
		     (in this case g++ 1.35.1- does not put out a name
		     for D as such, it defines type 19 (for example) in
		     the same stab as C, and then does a
		     .stabs "D:T19" and a .stabs "D:t19".
		     Thus
		     "break C::f" should not be looking for field f in
		     the class named D, 
		     but just for the field f in the baseclasses of C
		     (no matter what their names).

		     However, I don't know how to replace the code below
		     that depends on knowing the name of D.  */
		  if (class_name)
		    {
		      /* We just want the class name.  In the context
			 of C++, stripping off "struct " is always
			 sensible.  */
		      if (strncmp("struct ", class_name, 7) == 0)
			class_name += 7;
		      if (strncmp("union ", class_name, 6) == 0)
			class_name += 6;

		      sym_class = lookup_symbol (class_name, 0, STRUCT_NAMESPACE, 0);
		      for (method_counter = TYPE_NFN_FIELDS (SYMBOL_TYPE (sym_class)) - 1;
			   method_counter >= 0;
			   --method_counter)
			{
			  int field_counter;
			  struct fn_field *f =
			    TYPE_FN_FIELDLIST1 (SYMBOL_TYPE (sym_class), method_counter);

			  method_name = TYPE_FN_FIELDLIST_NAME (SYMBOL_TYPE (sym_class), method_counter);
			  if (!strcmp (copy, method_name))
			    /* Find all the fields with that name.  */
			    for (field_counter = TYPE_FN_FIELDLIST_LENGTH (SYMBOL_TYPE (sym_class), method_counter) - 1;
				 field_counter >= 0;
				 --field_counter)
			      {
				phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
				physnames[i1] = (char*) alloca (strlen (phys_name) + 1);
				strcpy (physnames[i1], phys_name);
				sym_arr[i1] = lookup_symbol (phys_name, SYMBOL_BLOCK_VALUE (sym_class), VAR_NAMESPACE, 0);
				if (sym_arr[i1]) i1++;
			      }
			}
		    }
		  if (TYPE_N_BASECLASSES (t))
		    t = TYPE_BASECLASS(t, 1);
		  else
		    break;
		}

	      if (i1 == 1)
		{
		  /* There is exactly one field with that name.  */
		  sym = sym_arr[0];

		  if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
		    {
		      /* Arg is the name of a function */
		      pc = BLOCK_START (SYMBOL_BLOCK_VALUE (sym)) + FUNCTION_START_OFFSET;
		      if (funfirstline)
			SKIP_PROLOGUE (pc);
		      values.sals = (struct symtab_and_line *)malloc (sizeof (struct symtab_and_line));
		      values.nelts = 1;
		      values.sals[0] = find_pc_line (pc, 0);
		      values.sals[0].pc = (values.sals[0].end && values.sals[0].pc != pc) ? values.sals[0].end : pc;
		    }
		  else
		    {
		      values.nelts = 0;
		    }
		  return values;
		}
	      if (i1 > 0)
		{
		  /* There is more than one field with that name
		     (overloaded).  Ask the user which one to use.  */
		  return decode_line_2 (argptr, sym_arr, physnames,
					i1, funfirstline);
		}
	      else
		error ("that class does not have any method named %s",copy);
	    }
	  else
	    error("no class, struct, or union named %s", copy );
	}
      /*  end of C++  */


      /* Extract the file name.  */
      p1 = p;
      while (p != *argptr && p[-1] == ' ') --p;
      copy = (char *) alloca (p - *argptr + 1);
      bcopy (*argptr, copy, p - *argptr);
      copy[p - *argptr] = 0;

      /* Find that file's data.  */
      s = lookup_symtab (copy);
      if (s == 0)
	{
	  if (symtab_list == 0 && partial_symtab_list == 0)
	    error ("No symbol table is loaded.  Use the \"symbol-file\" command.");
	  error ("No source file named %s.", copy);
	}

      /* Discard the file name from the arg.  */
      p = p1 + 1;
      while (*p == ' ' || *p == '\t') p++;
      *argptr = p;
    }

  /* S is specified file's symtab, or 0 if no file specified.
     arg no longer contains the file name.  */

  /* Check whether arg is all digits (and sign) */

  p = *argptr;
  if (*p == '-' || *p == '+') p++;
  while (*p >= '0' && *p <= '9')
    p++;

  if (p != *argptr && (*p == 0 || *p == ' ' || *p == '\t' || *p == ','))
    {
      /* We found a token consisting of all digits -- at least one digit.  */
      enum sign {none, plus, minus} sign = none;

      /* This is where we need to make sure that we have good defaults.
	 We must guarrantee that this section of code is never executed
	 when we are called with just a function name, since
	 select_source_symtab calls us with such an argument  */

      if (s == 0 && default_symtab == 0)
	{
	  if (symtab_list == 0 && partial_symtab_list == 0)
	    error ("No symbol table is loaded.  Use the \"symbol-file\" command.");
	  select_source_symtab (0);
	  default_symtab = current_source_symtab;
	  default_line = current_source_line;
	}

      if (**argptr == '+')
	sign = plus, (*argptr)++;
      else if (**argptr == '-')
	sign = minus, (*argptr)++;
      value.line = atoi (*argptr);
      switch (sign)
	{
	case plus:
	  if (p == *argptr)
	    value.line = 5;
	  if (s == 0)
	    value.line = default_line + value.line;
	  break;
	case minus:
	  if (p == *argptr)
	    value.line = 15;
	  if (s == 0)
	    value.line = default_line - value.line;
	  else
	    value.line = 1;
	  break;
	}

      while (*p == ' ' || *p == '\t') p++;
      *argptr = p;
      if (s == 0)
	s = default_symtab;
      value.symtab = s;
      value.pc = 0;
      values.sals = (struct symtab_and_line *)malloc (sizeof (struct symtab_and_line));
      values.sals[0] = value;
      values.nelts = 1;
      return values;
    }

  /* Arg token is not digits => try it as a function name
     Find the next token (everything up to end or next whitespace).  */
  p = *argptr;
  while (*p && *p != ' ' && *p != '\t' && *p != ',') p++;
  copy = (char *) alloca (p - *argptr + 1);
  bcopy (*argptr, copy, p - *argptr);
  copy[p - *argptr] = 0;
  while (*p == ' ' || *p == '\t') p++;
  *argptr = p;

  /* Look up that token as a function.
     If file specified, use that file's per-file block to start with.  */

  if (s == 0)
    /* use current file as default if none is specified. */
    s = default_symtab;

  sym = lookup_symbol (copy, s ? BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), 1) : 0,
		       VAR_NAMESPACE, 0);

  if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
    {
      /* Arg is the name of a function */
      pc = BLOCK_START (SYMBOL_BLOCK_VALUE (sym)) + FUNCTION_START_OFFSET;
      if (funfirstline)
	SKIP_PROLOGUE (pc);
      value = find_pc_line (pc, 0);
#ifdef PROLOGUE_FIRSTLINE_OVERLAP
      /* Convex: no need to suppress code on first line, if any */
      value.pc = pc;
#else
      value.pc = (value.end && value.pc != pc) ? value.end : pc;
#endif
      values.sals = (struct symtab_and_line *)malloc (sizeof (struct symtab_and_line));
      values.sals[0] = value;
      values.nelts = 1;
      return values;
    }

  if (sym)
    error ("%s is not a function.", copy);

  if (symtab_list == 0 && partial_symtab_list == 0)
    error ("No symbol table is loaded.  Use the \"symbol-file\" command.");

  if ((i = lookup_misc_func (copy)) >= 0)
    {
      value.symtab = 0;
      value.line = 0;
      value.pc = misc_function_vector[i].address + FUNCTION_START_OFFSET;
      if (funfirstline)
	SKIP_PROLOGUE (value.pc);
      values.sals = (struct symtab_and_line *)malloc (sizeof (struct symtab_and_line));
      values.sals[0] = value;
      values.nelts = 1;
      return values;
    }

  error ("Function %s not defined.", copy);
}

struct symtabs_and_lines
decode_line_spec (string, funfirstline)
     char *string;
     int funfirstline;
{
  struct symtabs_and_lines sals;
  if (string == 0)
    error ("Empty line specification.");
  sals = decode_line_1 (&string, funfirstline,
			current_source_symtab, current_source_line);
  if (*string)
    error ("Junk at end of line specification: %s", string);
  return sals;
}

/* Given a list of NELTS symbols in sym_arr (with corresponding
   mangled names in physnames), return a list of lines to operate on
   (ask user if necessary).  */
struct symtabs_and_lines
decode_line_2 (argptr, sym_arr, physnames, nelts, funfirstline)
     char **argptr;
     struct symbol *sym_arr[];
     char *physnames[];
     int nelts;
     int funfirstline;
{
  char *getenv();
  struct symtabs_and_lines values, return_values;
  register CORE_ADDR pc;
  char *args, *arg1, *command_line_input ();
  int i;
  char *prompt;

  values.sals = (struct symtab_and_line *) alloca (nelts * sizeof(struct symtab_and_line));
  return_values.sals = (struct symtab_and_line *) malloc (nelts * sizeof(struct symtab_and_line));

  i = 0;
  printf("[0] cancel\n[1] all\n");
  while (i < nelts)
    {
      if (sym_arr[i] && SYMBOL_CLASS (sym_arr[i]) == LOC_BLOCK)
	{
	  /* Arg is the name of a function */
	  pc = BLOCK_START (SYMBOL_BLOCK_VALUE (sym_arr[i])) 
	       + FUNCTION_START_OFFSET;
	  if (funfirstline)
	    SKIP_PROLOGUE (pc);
	  values.sals[i] = find_pc_line (pc, 0);
	  values.sals[i].pc = (values.sals[i].end && values.sals[i].pc != pc) ? values.sals[i].end : pc;
	  printf("[%d] file:%s; line number:%d\n",
		 (i+2), values.sals[i].symtab->filename, values.sals[i].line);
	}
      else printf ("?HERE\n");
      i++;
    }
  
  if ((prompt = getenv ("PS2")) == NULL)
    {
      prompt = ">";
    }
  printf("%s ",prompt);
  fflush(stdout);

  args = command_line_input (0, 0);
  
  if (args == 0)
    error_no_arg ("one or more choice numbers");

  i = 0;
  while (*args)
    {
      int num;

      arg1 = args;
      while (*arg1 >= '0' && *arg1 <= '9') arg1++;
      if (*arg1 && *arg1 != ' ' && *arg1 != '\t')
	error ("Arguments must be choice numbers.");

      num = atoi (args);

      if (num == 0)
	error ("cancelled");
      else if (num == 1)
	{
	  bcopy (values.sals, return_values.sals, (nelts * sizeof(struct symtab_and_line)));
	  return_values.nelts = nelts;
	  return return_values;
	}

      if (num > nelts + 2)
	{
	  printf ("No choice number %d.\n", num);
	}
      else
	{
	  num -= 2;
	  if (values.sals[num].pc)
	    {
	      return_values.sals[i++] = values.sals[num];
	      values.sals[num].pc = 0;
	    }
	  else
	    {
	      printf ("duplicate request for %d ignored.\n", num);
	    }
	}

      args = arg1;
      while (*args == ' ' || *args == '\t') args++;
    }
  return_values.nelts = i;
  return return_values;
}

/* hash a symbol ("hashpjw" from Aho, Sethi & Ullman, p.436) */

int
hash_symbol(str)
	register char *str;
{
	register unsigned int h = 0, g;
	register unsigned char c;

	while (c = *(unsigned char *)str++) {
		h = (h << 4) + c;
		if (g = h & 0xf0000000) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	return ((int)h);
}

/* Return the index of misc function named NAME.  */

int
lookup_misc_func (name)
     register char *name;
{
  register int i = hash_symbol(name) & (MISC_FUNC_HASH_SIZE - 1);

  if (misc_function_vector == 0)
	  error("No symbol file");

  i = misc_function_hash_tab[i];
  while (i >= 0)
    {
      if (strcmp(misc_function_vector[i].name, name) == 0)
	break;
      i = misc_function_vector[i].next;
    }
  return (i);
}

/*
 * Slave routine for sources_info.  Force line breaks at ,'s.
 */
static void
output_source_filename (name, next)
char *name;
int next;
{
  static int column = 0;
  
  if (column != 0 && column + strlen (name) >= 70)
    {
      printf_filtered ("\n");
      column = 0;
    }
  else if (column != 0)
    {
      printf_filtered (" ");
      column++;
    }
  printf_filtered ("%s", name);
  column += strlen (name);
  if (next)
    {
      printf_filtered (",");
      column++;
    }
  
  if (!next) column = 0;
}  

static void
sources_info ()
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register int column = 0;

  if (symtab_list == 0 && partial_symtab_list == 0)
    {
      printf ("No symbol table is loaded.\n");
      return;
    }
  
  printf_filtered ("Source files for which symbols have been read in:\n\n");

  for (s = symtab_list; s; s = s->next)
    output_source_filename (s->filename, s->next);
  printf_filtered ("\n\n");
  
  printf_filtered ("Source files for which symbols will be read in on demand:\n\n");

  for (ps = partial_symtab_list; ps; ps = ps->next)
    if (!ps->readin)
      output_source_filename (ps->filename, ps->next);
  printf_filtered ("\n");
}

/* List all symbols (if REGEXP is 0) or all symbols matching REGEXP.
   If CLASS is zero, list all symbols except functions and type names.
   If CLASS is 1, list only functions.
   If CLASS is 2, list only type names.  */

static void sort_block_syms ();

static void
list_symbols (regexp, class)
     char *regexp;
     int class;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  register struct block *b;
  register int i, j;
  register struct symbol *sym;
  struct partial_symbol *psym, *bound;
  char *val;
  static char *classnames[]
    = {"variable", "function", "type", "method"};
  int print_count = 0;
  int found_in_file = 0;

  if (regexp)
    if (val = (char *) re_comp (regexp))
      error ("Invalid regexp: %s", val);

  /* Search through the partial_symtab_list *first* for all symbols
     matching the regexp.  That way we don't have to reproduce all of
     the machinery below. */
  for (psym = global_psymbols.list, bound = global_psymbols.next; ;
       psym = static_psymbols.list, bound = static_psymbols.next)
    {
      for (; psym < bound; ++psym)
	{
	  if (psym->pst->readin)
	    continue;

	  QUIT;
	  /* If it would match (logic taken from loop below)
	     load the file and go on to the next one */
	  if ((regexp == 0 || re_exec (SYMBOL_NAME (psym)))
	      && ((class == 0 && SYMBOL_CLASS (psym) != LOC_TYPEDEF
		   && SYMBOL_CLASS (psym) != LOC_BLOCK)
		  || (class == 1 && SYMBOL_CLASS (psym) == LOC_BLOCK)
		  || (class == 2 && SYMBOL_CLASS (psym) == LOC_TYPEDEF)
		  || (class == 3 && SYMBOL_CLASS (psym) == LOC_BLOCK)))
	    psymtab_to_symtab(psym->pst);
	}
      if (psym == static_psymbols.next)
	break;
    }

  /* Printout here so as to get after the "Reading in symbols"
     messages which will be generated above.  */
  printf_filtered (regexp
	  ? "All %ss matching regular expression \"%s\":\n"
	  : "All defined %ss:\n",
	  classnames[class],
	  regexp);

  /* Here, *if* the class is correct (function only, right now), we
     should search through the misc function vector for symbols that
     match and call find_pc_psymtab on them.  If find_pc_psymtab returns
     0, don't worry about it (already read in or no debugging info).  */

  if (class == 1)
    {
      for (i = 0; i < misc_function_count; i++)
	if (regexp == 0 || re_exec (misc_function_vector[i].name))
	  {
	    ps = find_pc_psymtab (misc_function_vector[i].address);
	    if (ps && !ps->readin)
	      psymtab_to_symtab (ps);
	  }
    }

  for (s = symtab_list; s; s = s->next)
    {
      found_in_file = 0;
      bv = BLOCKVECTOR (s);
      /* Often many files share a blockvector.
	 Scan each blockvector only once so that
	 we don't get every symbol many times.
	 It happens that the first symtab in the list
	 for any given blockvector is the main file.  */
      if (bv != prev_bv)
	for (i = 0; i < 2; i++)
	  {
	    b = BLOCKVECTOR_BLOCK (bv, i);
	    /* Skip the sort if this block is always sorted.  */
	    if (!BLOCK_SHOULD_SORT (b))
	      sort_block_syms (b);
	    for (j = 0; j < BLOCK_NSYMS (b); j++)
	      {
		QUIT;
		sym = BLOCK_SYM (b, j);
		if ((regexp == 0 || re_exec (SYMBOL_NAME (sym)))
		    && ((class == 0 && SYMBOL_CLASS (sym) != LOC_TYPEDEF
			 && SYMBOL_CLASS (sym) != LOC_BLOCK)
			|| (class == 1 && SYMBOL_CLASS (sym) == LOC_BLOCK)
			|| (class == 2 && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
			|| (class == 3 && SYMBOL_CLASS (sym) == LOC_BLOCK)))
		  {
		    if (!found_in_file)
		      {
			printf_filtered ("\nFile %s:\n", s->filename);
			print_count += 2;
		      }
		    found_in_file = 1;
		    if (class != 2 && i == 1)
		      printf_filtered ("static ");
		    if (class == 2
			&& SYMBOL_NAMESPACE (sym) != STRUCT_NAMESPACE)
		      printf_filtered ("typedef ");

		    if (class < 3)
		      {
			type_print (SYMBOL_TYPE (sym),
				    (SYMBOL_CLASS (sym) == LOC_TYPEDEF
				     ? "" : SYMBOL_NAME (sym)),
				    stdout, 0);

			if (class == 2
			    && SYMBOL_NAMESPACE (sym) != STRUCT_NAMESPACE
			    && (TYPE_NAME ((SYMBOL_TYPE (sym))) == 0
				|| 0 != strcmp (TYPE_NAME ((SYMBOL_TYPE (sym))),
						SYMBOL_NAME (sym))))
			  printf_filtered (" %s", SYMBOL_NAME (sym));

			printf_filtered (";\n");
		      }
		    else
		      {
# if 0
			char buf[1024];
			type_print_base (TYPE_FN_FIELD_TYPE(t, i), stdout, 0, 0); 
			type_print_varspec_prefix (TYPE_FN_FIELD_TYPE(t, i), stdout, 0); 
			sprintf (buf, " %s::", TYPE_NAME (t));
			type_print_method_args (TYPE_FN_FIELD_ARGS (t, i), buf, name, stdout);
# endif
		      }
		  }
	      }
	  }
      prev_bv = bv;
    }
}

static void
variables_info (regexp)
     char *regexp;
{
  list_symbols (regexp, 0);
}

static void
functions_info (regexp)
     char *regexp;
{
  list_symbols (regexp, 1);
}

static void
types_info (regexp)
     char *regexp;
{
  list_symbols (regexp, 2);
}

#if 0
/* Tiemann says: "info methods was never implemented."  */
static void
methods_info (regexp)
     char *regexp;
{
  list_symbols (regexp, 3);
}
#endif /* 0 */

/* Call sort_block_syms to sort alphabetically the symbols of one block.  */

static int
compare_symbols (s1, s2)
     struct symbol **s1, **s2;
{
  /* Names that are less should come first.  */
  register int namediff = strcmp (SYMBOL_NAME (*s1), SYMBOL_NAME (*s2));
  if (namediff != 0) return namediff;
  /* For symbols of the same name, registers should come first.  */
  return ((SYMBOL_CLASS (*s2) == LOC_REGISTER)
	  - (SYMBOL_CLASS (*s1) == LOC_REGISTER));
}

static void
sort_block_syms (b)
     register struct block *b;
{
  qsort (&BLOCK_SYM (b, 0), BLOCK_NSYMS (b),
	 sizeof (struct symbol *), compare_symbols);
}

/* Initialize the standard C scalar types.  */

static
struct type *
init_type (code, length, uns, name)
     enum type_code code;
     int length, uns;
     char *name;
{
  register struct type *type;

  type = (struct type *) xmalloc (sizeof (struct type));
  bzero (type, sizeof *type);
  TYPE_MAIN_VARIANT (type) = type;
  TYPE_CODE (type) = code;
  TYPE_LENGTH (type) = length;
  TYPE_FLAGS (type) = uns ? TYPE_FLAG_UNSIGNED : 0;
  TYPE_FLAGS (type) |= TYPE_FLAG_PERM;
  TYPE_NFIELDS (type) = 0;
  TYPE_NAME (type) = name;

  /* C++ fancies.  */
  TYPE_NFN_FIELDS (type) = 0;
  TYPE_N_BASECLASSES (type) = 0;
  TYPE_BASECLASSES (type) = 0;
  return type;
}

/* Return Nonzero if block a is lexically nested within block b,
   or if a and b have the same pc range.
   Return zero otherwise. */
int
contained_in (a, b)
     struct block *a, *b;
{
  if (!a || !b)
    return 0;
  return a->startaddr >= b->startaddr && a->endaddr <= b->endaddr;
}


/* Helper routine for make_symbol_completion_list.  */

int return_val_size, return_val_index;
char **return_val;

void
completion_list_add_symbol (symname)
     char *symname;
{
  if (return_val_index + 3 > return_val_size)
    return_val =
      (char **)xrealloc (return_val,
			 (return_val_size *= 2) * sizeof (char *));
  
  return_val[return_val_index] =
    (char *)xmalloc (1 + strlen (symname));
  
  strcpy (return_val[return_val_index], symname);
  
  return_val[++return_val_index] = (char *)NULL;
}

/* Return a NULL terminated array of all symbols (regardless of class) which
   begin by matching TEXT.  If the answer is no symbols, then the return value
   is an array which contains only a NULL pointer.

   Problem: All of the symbols have to be copied because readline
   frees them.  I'm not going to worry about this; hopefully there
   won't be that many.  */

char **
make_symbol_completion_list (text)
  char *text;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  register struct block *b, *surrounding_static_block;
  extern struct block *get_selected_block ();
  register int i, j;
  register struct symbol *sym;
  struct partial_symbol *psym;

  int text_len = strlen (text);
  return_val_size = 100;
  return_val_index = 0;
  return_val =
    (char **)xmalloc ((1 + return_val_size) *sizeof (char *));
  return_val[0] = (char *)NULL;

  /* Look through the partial symtabs for all symbols which begin
     by matching TEXT.  Add each one that you find to the list.  */

  for (ps = partial_symtab_list; ps; ps = ps->next)
    {
      /* If the psymtab's been read in we'll get it when we search
	 through the blockvector.  */
      if (ps->readin) continue;

      for (psym = global_psymbols.list + ps->globals_offset;
	   psym < (global_psymbols.list + ps->globals_offset
		   + ps->n_global_syms);
	   psym++)
	{
	  QUIT;			/* If interrupted, then quit. */
	  if ((strncmp (SYMBOL_NAME (psym), text, text_len) == 0))
	    completion_list_add_symbol (SYMBOL_NAME (psym));
	}
      
      for (psym = static_psymbols.list + ps->statics_offset;
	   psym < (static_psymbols.list + ps->statics_offset
		   + ps->n_static_syms);
	   psym++)
	{
	  QUIT;
	  if ((strncmp (SYMBOL_NAME (psym), text, text_len) == 0))
	    completion_list_add_symbol (SYMBOL_NAME (psym));
	}
    }

  /* At this point scan through the misc function vector and add each
     symbol you find to the list.  Eventually we want to ignore
      anything that isn't a text symbol (everything else will be
      handled by the psymtab code above).  */

  for (i = 0; i < misc_function_count; i++)
    if (!strncmp (text, misc_function_vector[i].name, text_len))
      completion_list_add_symbol (misc_function_vector[i].name);

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */
  for (b = get_selected_block (); b; b = BLOCK_SUPERBLOCK (b))
    {
      if (!BLOCK_SUPERBLOCK (b))
	surrounding_static_block = b; /* For elmin of dups */
      
      /* Also catch fields of types defined in this places which
	 match our text string.  Only complete on types visible
	 from current context.  */
      for (i = 0; i < BLOCK_NSYMS (b); i++)
	{
	  register struct symbol *sym = BLOCK_SYM (b, i);
	  
	  if (!strncmp (SYMBOL_NAME (sym), text, text_len))
	    completion_list_add_symbol (SYMBOL_NAME (sym));

	  if (SYMBOL_CLASS (sym) == LOC_TYPEDEF)
	    {
	      struct type *t = SYMBOL_TYPE (sym);
	      enum type_code c = TYPE_CODE (t);

	      if (c == TYPE_CODE_UNION || c == TYPE_CODE_STRUCT)
		for (j = 0; j < TYPE_NFIELDS (t); j++)
		  if (TYPE_FIELD_NAME (t, j) &&
		      !strncmp (TYPE_FIELD_NAME (t, j), text, text_len))
		    completion_list_add_symbol (TYPE_FIELD_NAME (t, j));
	    }
	}
    }

  /* Go through the symtabs and check the externs and statics for
     symbols which match.  */

  for (s = symtab_list; s; s = s->next)
    {
      struct block *b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), 0);
      
      for (i = 0; i < BLOCK_NSYMS (b); i++)
	if (!strncmp (SYMBOL_NAME (BLOCK_SYM (b, i)), text, text_len))
	  completion_list_add_symbol (SYMBOL_NAME (BLOCK_SYM (b, i)));
    }

  for (s = symtab_list; s; s = s->next)
    {
      struct block *b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), 1);

      /* Don't do this block twice.  */
      if (b == surrounding_static_block) continue;
      
      for (i = 0; i < BLOCK_NSYMS (b); i++)
	if (!strncmp (SYMBOL_NAME (BLOCK_SYM (b, i)), text, text_len))
	  completion_list_add_symbol (SYMBOL_NAME (BLOCK_SYM (b, i)));
    }

  return (return_val);
}

void
_initialize_symtab ()
{
  add_info ("variables", variables_info,
	    "All global and static variable names, or those matching REGEXP.");
  add_info ("functions", functions_info,
	    "All function names, or those matching REGEXP.");
  add_info ("types", types_info,
	    "All types names, or those matching REGEXP.");
#if 0
  add_info ("methods", methods_info,
	    "All method names, or those matching REGEXP::REGEXP.\n\
If the class qualifier is ommited, it is assumed to be the current scope.\n\
If the first REGEXP is ommited, then all methods matching the second REGEXP\n\
are listed.");
#endif
  add_info ("sources", sources_info,
	    "Source files in the program.");

  obstack_init (symbol_obstack);
  obstack_init (psymbol_obstack);

  builtin_type_void = init_type (TYPE_CODE_VOID, 1, 0, "void");

  builtin_type_float = init_type (TYPE_CODE_FLT, sizeof (float), 0, "float");
  builtin_type_double = init_type (TYPE_CODE_FLT, sizeof (double), 0, "double");

  builtin_type_char = init_type (TYPE_CODE_INT, sizeof (char), 0, "char");
  builtin_type_short = init_type (TYPE_CODE_INT, sizeof (short), 0, "short");
  builtin_type_long = init_type (TYPE_CODE_INT, sizeof (long), 0, "long");
  builtin_type_int = init_type (TYPE_CODE_INT, sizeof (int), 0, "int");

  builtin_type_unsigned_char = init_type (TYPE_CODE_INT, sizeof (char), 1, "unsigned char");
  builtin_type_unsigned_short = init_type (TYPE_CODE_INT, sizeof (short), 1, "unsigned short");
  builtin_type_unsigned_long = init_type (TYPE_CODE_INT, sizeof (long), 1, "unsigned long");
  builtin_type_unsigned_int = init_type (TYPE_CODE_INT, sizeof (int), 1, "unsigned int");
#ifdef LONG_LONG
  builtin_type_long_long =
    init_type (TYPE_CODE_INT, sizeof (long long), 0, "long long");
  builtin_type_unsigned_long_long = 
    init_type (TYPE_CODE_INT, sizeof (long long), 1, "unsigned long long");
#endif
}

