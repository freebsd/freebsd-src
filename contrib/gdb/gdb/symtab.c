/* Symbol table lookup for the GNU debugger, GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995
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
#include "gdbcore.h"
#include "frame.h"
#include "target.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "call-cmds.h"
#include "gnu-regex.h"
#include "expression.h"
#include "language.h"
#include "demangle.h"

#include "obstack.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>

/* Prototypes for local functions */

extern int
find_methods PARAMS ((struct type *, char *, struct symbol **));

static void
completion_list_add_name PARAMS ((char *, char *, int, char *, char *));

static void
build_canonical_line_spec PARAMS ((struct symtab_and_line *, char *, char ***));

static struct symtabs_and_lines
decode_line_2 PARAMS ((struct symbol *[], int, int, char ***));

static void
rbreak_command PARAMS ((char *, int));

static void
types_info PARAMS ((char *, int));

static void
functions_info PARAMS ((char *, int));

static void
variables_info PARAMS ((char *, int));

static void
sources_info PARAMS ((char *, int));

static void
list_symbols PARAMS ((char *, int, int, int));

static void
output_source_filename PARAMS ((char *, int *));

static char *
operator_chars PARAMS ((char *, char **));

static int find_line_common PARAMS ((struct linetable *, int, int *));

static struct partial_symbol *
lookup_partial_symbol PARAMS ((struct partial_symtab *, const char *,
			       int, namespace_enum));

static struct symtab *
lookup_symtab_1 PARAMS ((char *));

/* */

/* The single non-language-specific builtin type */
struct type *builtin_type_error;

/* Block in which the most recently searched-for symbol was found.
   Might be better to make this a parameter to lookup_symbol and 
   value_of_this. */

const struct block *block_found;

char no_symtab_msg[] = "No symbol table is loaded.  Use the \"file\" command.";

/* While the C++ support is still in flux, issue a possibly helpful hint on
   using the new command completion feature on single quoted demangled C++
   symbols.  Remove when loose ends are cleaned up.   FIXME -fnf */

void
cplusplus_hint (name)
     char *name;
{
  while (*name == '\'')
    name++;
  printf_filtered ("Hint: try '%s<TAB> or '%s<ESC-?>\n", name, name);
  printf_filtered ("(Note leading single quote.)\n");
}

/* Check for a symtab of a specific name; first in symtabs, then in
   psymtabs.  *If* there is no '/' in the name, a match after a '/'
   in the symtab filename will also work.  */

static struct symtab *
lookup_symtab_1 (name)
     char *name;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register char *slash;
  register struct objfile *objfile;

 got_symtab:

  /* First, search for an exact match */

  ALL_SYMTABS (objfile, s)
    if (STREQ (name, s->filename))
      return s;

  slash = strchr (name, '/');

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (!slash)
    ALL_SYMTABS (objfile, s)
      {
	char *p = s -> filename;
	char *tail = strrchr (p, '/');

	if (tail)
	  p = tail + 1;

	if (STREQ (p, name))
	  return s;
      }

  /* Same search rules as above apply here, but now we look thru the
     psymtabs.  */

  ps = lookup_partial_symtab (name);
  if (!ps)
    return (NULL);

  if (ps -> readin)
    error ("Internal: readin %s pst for `%s' found when no symtab found.",
	   ps -> filename, name);

  s = PSYMTAB_TO_SYMTAB (ps);

  if (s)
    return s;

  /* At this point, we have located the psymtab for this file, but
     the conversion to a symtab has failed.  This usually happens
     when we are looking up an include file.  In this case,
     PSYMTAB_TO_SYMTAB doesn't return a symtab, even though one has
     been created.  So, we need to run through the symtabs again in
     order to find the file.
     XXX - This is a crock, and should be fixed inside of the the
     symbol parsing routines. */
  goto got_symtab;
}

/* Lookup the symbol table of a source file named NAME.  Try a couple
   of variations if the first lookup doesn't work.  */

struct symtab *
lookup_symtab (name)
     char *name;
{
  register struct symtab *s;
#if 0
  register char *copy;
#endif

  s = lookup_symtab_1 (name);
  if (s) return s;

#if 0
  /* This screws c-exp.y:yylex if there is both a type "tree" and a symtab
     "tree.c".  */

  /* If name not found as specified, see if adding ".c" helps.  */
  /* Why is this?  Is it just a user convenience?  (If so, it's pretty
     questionable in the presence of C++, FORTRAN, etc.).  It's not in
     the GDB manual.  */

  copy = (char *) alloca (strlen (name) + 3);
  strcpy (copy, name);
  strcat (copy, ".c");
  s = lookup_symtab_1 (copy);
  if (s) return s;
#endif /* 0 */

  /* We didn't find anything; die.  */
  return 0;
}

/* Lookup the partial symbol table of a source file named NAME.
   *If* there is no '/' in the name, a match after a '/'
   in the psymtab filename will also work.  */

struct partial_symtab *
lookup_partial_symtab (name)
char *name;
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;
  
  ALL_PSYMTABS (objfile, pst)
    {
      if (STREQ (name, pst -> filename))
	{
	  return (pst);
	}
    }

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (!strchr (name, '/'))
    ALL_PSYMTABS (objfile, pst)
      {
	char *p = pst -> filename;
	char *tail = strrchr (p, '/');

	if (tail)
	  p = tail + 1;

	if (STREQ (p, name))
	  return (pst);
      }

  return (NULL);
}

/* Demangle a GDB method stub type.
   Note that this function is g++ specific. */

char *
gdb_mangle_name (type, i, j)
     struct type *type;
     int i, j;
{
  int mangled_name_len;
  char *mangled_name;
  struct fn_field *f = TYPE_FN_FIELDLIST1 (type, i);
  struct fn_field *method = &f[j];
  char *field_name = TYPE_FN_FIELDLIST_NAME (type, i);
  char *physname = TYPE_FN_FIELD_PHYSNAME (f, j);
  char *newname = type_name_no_tag (type);

  /* Does the form of physname indicate that it is the full mangled name
     of a constructor (not just the args)?  */
  int is_full_physname_constructor;

  int is_constructor;
  int is_destructor = DESTRUCTOR_PREFIX_P (physname);
  /* Need a new type prefix.  */
  char *const_prefix = method->is_const ? "C" : "";
  char *volatile_prefix = method->is_volatile ? "V" : "";
  char buf[20];
  int len = (newname == NULL ? 0 : strlen (newname));

  is_full_physname_constructor = 
    ((physname[0]=='_' && physname[1]=='_' && 
      (isdigit(physname[2]) || physname[2]=='Q' || physname[2]=='t'))
     || (strncmp(physname, "__ct", 4) == 0));

  is_constructor =
    is_full_physname_constructor || (newname && STREQ(field_name, newname));

  if (!is_destructor)
    is_destructor = (strncmp(physname, "__dt", 4) == 0); 

  if (is_destructor || is_full_physname_constructor)
    {
      mangled_name = (char*) xmalloc(strlen(physname)+1);
      strcpy(mangled_name, physname);
      return mangled_name;
    }

  if (len == 0)
    {
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
      if (strcmp(buf, "__") == 0)
	buf[0] = '\0';
    }
  else if (newname != NULL && strchr (newname, '<') != NULL)
    {
      /* Template methods are fully mangled.  */
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
      newname = NULL;
      len = 0;
    }
  else
    {
      sprintf (buf, "__%s%s%d", const_prefix, volatile_prefix, len);
    }
  mangled_name_len = ((is_constructor ? 0 : strlen (field_name))
			  + strlen (buf) + len
			  + strlen (physname)
			  + 1);

  /* Only needed for GNU-mangled names.  ANSI-mangled names
     work with the normal mechanisms.  */
  if (OPNAME_PREFIX_P (field_name))
    {
      const char *opname = cplus_mangle_opname (field_name + 3, 0);
      if (opname == NULL)
	error ("No mangling for \"%s\"", field_name);
      mangled_name_len += strlen (opname);
      mangled_name = (char *)xmalloc (mangled_name_len);

      strncpy (mangled_name, field_name, 3);
      mangled_name[3] = '\0';
      strcat (mangled_name, opname);
    }
  else
    {
      mangled_name = (char *)xmalloc (mangled_name_len);
      if (is_constructor)
	mangled_name[0] = '\0';
      else
	strcpy (mangled_name, field_name);
    }
  strcat (mangled_name, buf);
  /* If the class doesn't have a name, i.e. newname NULL, then we just
     mangle it using 0 for the length of the class.  Thus it gets mangled
     as something starting with `::' rather than `classname::'. */ 
  if (newname != NULL)
    strcat (mangled_name, newname);

  strcat (mangled_name, physname);
  return (mangled_name);
}


/* Find which partial symtab on contains PC.  Return 0 if none.  */

struct partial_symtab *
find_pc_psymtab (pc)
     register CORE_ADDR pc;
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;

  ALL_PSYMTABS (objfile, pst)
    {
      if (pc >= pst->textlow && pc < pst->texthigh)
	{
	  struct minimal_symbol *msymbol;
	  struct partial_symtab *tpst;

	  /* An objfile that has its functions reordered might have
	     many partial symbol tables containing the PC, but
	     we want the partial symbol table that contains the
	     function containing the PC.  */
	  if (!(objfile->flags & OBJF_REORDERED))
	    return (pst);

	  msymbol = lookup_minimal_symbol_by_pc (pc);
	  if (msymbol == NULL)
	    return (pst);

	  for (tpst = pst; tpst != NULL; tpst = tpst->next)
	    {
	      if (pc >= tpst->textlow && pc < tpst->texthigh)
		{
		  struct partial_symbol *p;

		  p = find_pc_psymbol (tpst, pc);
		  if (p != NULL
		      && SYMBOL_VALUE_ADDRESS(p)
			 == SYMBOL_VALUE_ADDRESS (msymbol))
		    return (tpst);
		}
	    }
	  return (pst);
	}
    }
  return (NULL);
}

/* Find which partial symbol within a psymtab contains PC.  Return 0
   if none.  Check all psymtabs if PSYMTAB is 0.  */
struct partial_symbol *
find_pc_psymbol (psymtab, pc)
     struct partial_symtab *psymtab;
     CORE_ADDR pc;
{
  struct partial_symbol *best = NULL, *p, **pp;
  CORE_ADDR best_pc;
  
  if (!psymtab)
    psymtab = find_pc_psymtab (pc);
  if (!psymtab)
    return 0;

  best_pc = psymtab->textlow - 1;

  /* Search the global symbols as well as the static symbols, so that
     find_pc_partial_function doesn't use a minimal symbol and thus
     cache a bad endaddr.  */
  for (pp = psymtab->objfile->global_psymbols.list + psymtab->globals_offset;
       (pp - (psymtab->objfile->global_psymbols.list + psymtab->globals_offset)
	< psymtab->n_global_syms);
       pp++)
    {
      p = *pp;
      if (SYMBOL_NAMESPACE (p) == VAR_NAMESPACE
	  && SYMBOL_CLASS (p) == LOC_BLOCK
	  && pc >= SYMBOL_VALUE_ADDRESS (p)
	  && SYMBOL_VALUE_ADDRESS (p) > best_pc)
	{
	  best_pc = SYMBOL_VALUE_ADDRESS (p);
	  best = p;
	}
    }
  for (pp = psymtab->objfile->static_psymbols.list + psymtab->statics_offset;
       (pp - (psymtab->objfile->static_psymbols.list + psymtab->statics_offset)
	< psymtab->n_static_syms);
       pp++)
    {
      p = *pp;
      if (SYMBOL_NAMESPACE (p) == VAR_NAMESPACE
	  && SYMBOL_CLASS (p) == LOC_BLOCK
	  && pc >= SYMBOL_VALUE_ADDRESS (p)
	  && SYMBOL_VALUE_ADDRESS (p) > best_pc)
	{
	  best_pc = SYMBOL_VALUE_ADDRESS (p);
	  best = p;
	}
    }
  if (best_pc == psymtab->textlow - 1)
    return 0;
  return best;
}


/* Find the definition for a specified symbol name NAME
   in namespace NAMESPACE, visible from lexical block BLOCK.
   Returns the struct symbol pointer, or zero if no symbol is found.
   If SYMTAB is non-NULL, store the symbol table in which the
   symbol was found there, or NULL if not found.
   C++: if IS_A_FIELD_OF_THIS is nonzero on entry, check to see if
   NAME is a field of the current implied argument `this'.  If so set
   *IS_A_FIELD_OF_THIS to 1, otherwise set it to zero. 
   BLOCK_FOUND is set to the block in which NAME is found (in the case of
   a field of `this', value_of_this sets BLOCK_FOUND to the proper value.) */

/* This function has a bunch of loops in it and it would seem to be
   attractive to put in some QUIT's (though I'm not really sure
   whether it can run long enough to be really important).  But there
   are a few calls for which it would appear to be bad news to quit
   out of here: find_proc_desc in alpha-tdep.c and mips-tdep.c, and
   nindy_frame_chain_valid in nindy-tdep.c.  (Note that there is C++
   code below which can error(), but that probably doesn't affect
   these calls since they are looking for a known variable and thus
   can probably assume it will never hit the C++ code).  */

struct symbol *
lookup_symbol (name, block, namespace, is_a_field_of_this, symtab)
     const char *name;
     register const struct block *block;
     const namespace_enum namespace;
     int *is_a_field_of_this;
     struct symtab **symtab;
{
  register struct symbol *sym;
  register struct symtab *s = NULL;
  register struct partial_symtab *ps;
  struct blockvector *bv;
  register struct objfile *objfile;
  register struct block *b;
  register struct minimal_symbol *msymbol;

  /* Search specified block and its superiors.  */

  while (block != 0)
    {
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  if (symtab != NULL)
	    {
	      /* Search the list of symtabs for one which contains the
		 address of the start of this block.  */
	      ALL_SYMTABS (objfile, s)
		{
		  bv = BLOCKVECTOR (s);
		  b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
		  if (BLOCK_START (b) <= BLOCK_START (block)
		      && BLOCK_END (b) > BLOCK_START (block))
		    goto found;
		}
found:
	      *symtab = s;
	    }

	  return (sym);
	}
      block = BLOCK_SUPERBLOCK (block);
    }

  /* FIXME: this code is never executed--block is always NULL at this
     point.  What is it trying to do, anyway?  We already should have
     checked the STATIC_BLOCK above (it is the superblock of top-level
     blocks).  Why is VAR_NAMESPACE special-cased?  */
  /* Don't need to mess with the psymtabs; if we have a block,
     that file is read in.  If we don't, then we deal later with
     all the psymtab stuff that needs checking.  */
  if (namespace == VAR_NAMESPACE && block != NULL)
    {
      struct block *b;
      /* Find the right symtab.  */
      ALL_SYMTABS (objfile, s)
	{
	  bv = BLOCKVECTOR (s);
	  b = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	  if (BLOCK_START (b) <= BLOCK_START (block)
	      && BLOCK_END (b) > BLOCK_START (block))
	    {
	      sym = lookup_block_symbol (b, name, VAR_NAMESPACE);
	      if (sym)
		{
		  block_found = b;
		  if (symtab != NULL)
		    *symtab = s;
		  return sym;
		}
	    }
	}
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
	  if (symtab != NULL)
	    *symtab = NULL;
	  return 0;
	}
    }

  /* Now search all global blocks.  Do the symtab's first, then
     check the psymtab's */
  
  ALL_SYMTABS (objfile, s)
    {
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  if (symtab != NULL)
	    *symtab = s;
	  return sym;
	}
    }

  /* Check for the possibility of the symbol being a function or
     a mangled variable that is stored in one of the minimal symbol tables.
     Eventually, all global symbols might be resolved in this way.  */
  
  if (namespace == VAR_NAMESPACE)
    {
      msymbol = lookup_minimal_symbol (name, NULL, NULL);
      if (msymbol != NULL)
	{
	  s = find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol));
	  if (s != NULL)
	    {
	      /* This is a function which has a symtab for its address.  */
	      bv = BLOCKVECTOR (s);
	      block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	      sym = lookup_block_symbol (block, SYMBOL_NAME (msymbol),
					 namespace);
              /* We kept static functions in minimal symbol table as well as
		 in static scope. We want to find them in the symbol table. */
		if (!sym) {
		  block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
		  sym = lookup_block_symbol (block, SYMBOL_NAME (msymbol),
					     namespace);
		}

	      /* sym == 0 if symbol was found in the minimal symbol table
		 but not in the symtab.
		 Return 0 to use the msymbol definition of "foo_".

		 This happens for Fortran  "foo_" symbols,
		 which are "foo" in the symtab.

		 This can also happen if "asm" is used to make a
		 regular symbol but not a debugging symbol, e.g.
		 asm(".globl _main");
		 asm("_main:");
		 */

	      if (symtab != NULL)
		*symtab = s;
	      return sym;
	    }
	  else if (MSYMBOL_TYPE (msymbol) != mst_text
		   && MSYMBOL_TYPE (msymbol) != mst_file_text
		   && !STREQ (name, SYMBOL_NAME (msymbol)))
	    {
	      /* This is a mangled variable, look it up by its
		 mangled name.  */
	      return lookup_symbol (SYMBOL_NAME (msymbol), block,
				    namespace, is_a_field_of_this, symtab);
	    }
	  /* There are no debug symbols for this file, or we are looking
	     for an unmangled variable.
	     Try to find a matching static symbol below. */
	}
    }
      
  ALL_PSYMTABS (objfile, ps)
    {
      if (!ps->readin && lookup_partial_symbol (ps, name, 1, namespace))
	{
	  s = PSYMTAB_TO_SYMTAB(ps);
	  bv = BLOCKVECTOR (s);
	  block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	  sym = lookup_block_symbol (block, name, namespace);
	  if (!sym)
	    error ("Internal: global symbol `%s' found in %s psymtab but not in symtab", name, ps->filename);
	  if (symtab != NULL)
	    *symtab = s;
	  return sym;
	}
    }

  /* Now search all per-file blocks.
     Not strictly correct, but more useful than an error.
     Do the symtabs first, then check the psymtabs */

  ALL_SYMTABS (objfile, s)
    {
      bv = BLOCKVECTOR (s);
      block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
      sym = lookup_block_symbol (block, name, namespace);
      if (sym) 
	{
	  block_found = block;
	  if (symtab != NULL)
	    *symtab = s;
	  return sym;
	}
    }

  ALL_PSYMTABS (objfile, ps)
    {
      if (!ps->readin && lookup_partial_symbol (ps, name, 0, namespace))
	{
	  s = PSYMTAB_TO_SYMTAB(ps);
	  bv = BLOCKVECTOR (s);
	  block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	  sym = lookup_block_symbol (block, name, namespace);
	  if (!sym)
	    error ("Internal: static symbol `%s' found in %s psymtab but not in symtab", name, ps->filename);
	  if (symtab != NULL)
	    *symtab = s;
	  return sym;
	}
    }

  if (symtab != NULL)
    *symtab = NULL;
  return 0;
}

/* Look, in partial_symtab PST, for symbol NAME.  Check the global
   symbols if GLOBAL, the static symbols if not */

static struct partial_symbol *
lookup_partial_symbol (pst, name, global, namespace)
     struct partial_symtab *pst;
     const char *name;
     int global;
     namespace_enum namespace;
{
  struct partial_symbol **start, **psym;
  struct partial_symbol **top, **bottom, **center;
  int length = (global ? pst->n_global_syms : pst->n_static_syms);
  int do_linear_search = 1;

  if (length == 0)
    {
      return (NULL);
    }
  
  start = (global ?
	   pst->objfile->global_psymbols.list + pst->globals_offset :
	   pst->objfile->static_psymbols.list + pst->statics_offset  );

  if (global)		/* This means we can use a binary search. */
    {
      do_linear_search = 0;

      /* Binary search.  This search is guaranteed to end with center
         pointing at the earliest partial symbol with the correct
	 name.  At that point *all* partial symbols with that name
	 will be checked against the correct namespace. */

      bottom = start;
      top = start + length - 1;
      while (top > bottom)
	{
	  center = bottom + (top - bottom) / 2;
	  if (!(center < top))
	    abort ();
	  if (!do_linear_search && SYMBOL_LANGUAGE (*center) == language_cplus)
	    {
	      do_linear_search = 1;
	    }
	  if (STRCMP (SYMBOL_NAME (*center), name) >= 0)
	    {
	      top = center;
	    }
	  else
	    {
	      bottom = center + 1;
	    }
	}
      if (!(top == bottom))
	abort ();
      while (STREQ (SYMBOL_NAME (*top), name))
	{
	  if (SYMBOL_NAMESPACE (*top) == namespace)
	    {
	      return (*top);
	    }
	  top ++;
	}
    }

  /* Can't use a binary search or else we found during the binary search that
     we should also do a linear search. */

  if (do_linear_search)
    {
      for (psym = start; psym < start + length; psym++)
	{
	  if (namespace == SYMBOL_NAMESPACE (*psym))
	    {
	      if (SYMBOL_MATCHES_NAME (*psym, name))
		{
		  return (*psym);
		}
	    }
	}
    }

  return (NULL);
}

/* Find the psymtab containing main(). */
/* FIXME:  What about languages without main() or specially linked
   executables that have no main() ? */

struct partial_symtab *
find_main_psymtab ()
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;

  ALL_PSYMTABS (objfile, pst)
    {
      if (lookup_partial_symbol (pst, "main", 1, VAR_NAMESPACE))
	{
	  return (pst);
	}
    }
  return (NULL);
}

/* Search BLOCK for symbol NAME in NAMESPACE.

   Note that if NAME is the demangled form of a C++ symbol, we will fail
   to find a match during the binary search of the non-encoded names, but
   for now we don't worry about the slight inefficiency of looking for
   a match we'll never find, since it will go pretty quick.  Once the
   binary search terminates, we drop through and do a straight linear
   search on the symbols.  Each symbol which is marked as being a C++
   symbol (language_cplus set) has both the encoded and non-encoded names
   tested for a match. */

struct symbol *
lookup_block_symbol (block, name, namespace)
     register const struct block *block;
     const char *name;
     const namespace_enum namespace;
{
  register int bot, top, inc;
  register struct symbol *sym;
  register struct symbol *sym_found = NULL;
  register int do_linear_search = 1;

  /* If the blocks's symbols were sorted, start with a binary search.  */

  if (BLOCK_SHOULD_SORT (block))
    {
      /* Reset the linear search flag so if the binary search fails, we
	 won't do the linear search once unless we find some reason to
	 do so, such as finding a C++ symbol during the binary search.
	 Note that for C++ modules, ALL the symbols in a block should
	 end up marked as C++ symbols. */

      do_linear_search = 0;
      top = BLOCK_NSYMS (block);
      bot = 0;

      /* Advance BOT to not far before the first symbol whose name is NAME. */

      while (1)
	{
	  inc = (top - bot + 1);
	  /* No need to keep binary searching for the last few bits worth.  */
	  if (inc < 4)
	    {
	      break;
	    }
	  inc = (inc >> 1) + bot;
	  sym = BLOCK_SYM (block, inc);
	  if (!do_linear_search && SYMBOL_LANGUAGE (sym) == language_cplus)
	    {
	      do_linear_search = 1;
	    }
	  if (SYMBOL_NAME (sym)[0] < name[0])
	    {
	      bot = inc;
	    }
	  else if (SYMBOL_NAME (sym)[0] > name[0])
	    {
	      top = inc;
	    }
	  else if (STRCMP (SYMBOL_NAME (sym), name) < 0)
	    {
	      bot = inc;
	    }
	  else
	    {
	      top = inc;
	    }
	}

      /* Now scan forward until we run out of symbols, find one whose
	 name is greater than NAME, or find one we want.  If there is
	 more than one symbol with the right name and namespace, we
	 return the first one; I believe it is now impossible for us
	 to encounter two symbols with the same name and namespace
	 here, because blocks containing argument symbols are no
	 longer sorted.  */

      top = BLOCK_NSYMS (block);
      while (bot < top)
	{
	  sym = BLOCK_SYM (block, bot);
	  inc = SYMBOL_NAME (sym)[0] - name[0];
	  if (inc == 0)
	    {
	      inc = STRCMP (SYMBOL_NAME (sym), name);
	    }
	  if (inc == 0 && SYMBOL_NAMESPACE (sym) == namespace)
	    {
	      return (sym);
	    }
	  if (inc > 0)
	    {
	      break;
	    }
	  bot++;
	}
    }

  /* Here if block isn't sorted, or we fail to find a match during the
     binary search above.  If during the binary search above, we find a
     symbol which is a C++ symbol, then we have re-enabled the linear
     search flag which was reset when starting the binary search.

     This loop is equivalent to the loop above, but hacked greatly for speed.

     Note that parameter symbols do not always show up last in the
     list; this loop makes sure to take anything else other than
     parameter symbols first; it only uses parameter symbols as a
     last resort.  Note that this only takes up extra computation
     time on a match.  */

  if (do_linear_search)
    {
      top = BLOCK_NSYMS (block);
      bot = 0;
      while (bot < top)
	{
	  sym = BLOCK_SYM (block, bot);
	  if (SYMBOL_NAMESPACE (sym) == namespace &&
	      SYMBOL_MATCHES_NAME (sym, name))
	    {
	      sym_found = sym;
	      if (SYMBOL_CLASS (sym) != LOC_ARG &&
		  SYMBOL_CLASS (sym) != LOC_LOCAL_ARG &&
		  SYMBOL_CLASS (sym) != LOC_REF_ARG &&
		  SYMBOL_CLASS (sym) != LOC_REGPARM &&
		  SYMBOL_CLASS (sym) != LOC_REGPARM_ADDR &&
		  SYMBOL_CLASS (sym) != LOC_BASEREG_ARG)
		{
		  break;
		}
	    }
	  bot++;
	}
    }
  return (sym_found);		/* Will be NULL if not found. */
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

/* Find the symtab associated with PC.  Look through the psymtabs and read in
   another symtab if necessary. */

struct symtab *
find_pc_symtab (pc)
     register CORE_ADDR pc;
{
  register struct block *b;
  struct blockvector *bv;
  register struct symtab *s = NULL;
  register struct symtab *best_s = NULL;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  int distance = 0;

  /* Search all symtabs for the one whose file contains our address, and which
     is the smallest of all the ones containing the address.  This is designed
     to deal with a case like symtab a is at 0x1000-0x2000 and 0x3000-0x4000
     and symtab b is at 0x2000-0x3000.  So the GLOBAL_BLOCK for a is from
     0x1000-0x4000, but for address 0x2345 we want to return symtab b.

     This happens for native ecoff format, where code from included files
     gets its own symtab. The symtab for the included file should have
     been read in already via the dependency mechanism.
     It might be swifter to create several symtabs with the same name
     like xcoff does (I'm not sure).

     It also happens for objfiles that have their functions reordered.
     For these, the symtab we are looking for is not necessarily read in.  */

  ALL_SYMTABS (objfile, s)
    {
      bv = BLOCKVECTOR (s);
      b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
      if (BLOCK_START (b) <= pc
	  && BLOCK_END (b) > pc
	  && (distance == 0
	      || BLOCK_END (b) - BLOCK_START (b) < distance))
	{
	  /* For an objfile that has its functions reordered,
	     find_pc_psymtab will find the proper partial symbol table
	     and we simply return its corresponding symtab.  */
	  if ((objfile->flags & OBJF_REORDERED) && objfile->psymtabs)
	    {
	      ps = find_pc_psymtab (pc);
	      if (ps)
		s = PSYMTAB_TO_SYMTAB (ps);
	      else
	        s = NULL;
	      return (s);
	    }
	  distance = BLOCK_END (b) - BLOCK_START (b);
	  best_s = s;
	}
    }

  if (best_s != NULL)
    return(best_s);

  s = NULL;
  ps = find_pc_psymtab (pc);
  if (ps)
    {
      if (ps->readin)
	/* Might want to error() here (in case symtab is corrupt and
	   will cause a core dump), but maybe we can successfully
	   continue, so let's not.  */
	/* FIXME-32x64: assumes pc fits in a long */
	warning ("\
(Internal error: pc 0x%lx in read in psymtab, but not in symtab.)\n",
	         (unsigned long) pc);
      s = PSYMTAB_TO_SYMTAB (ps);
    }
  return (s);
}

#if 0

/* Find the closest symbol value (of any sort -- function or variable)
   for a given address value.  Slow but complete.  (currently unused,
   mainly because it is too slow.  We could fix it if each symtab and
   psymtab had contained in it the addresses ranges of each of its
   sections, which also would be required to make things like "info
   line *0x2345" cause psymtabs to be converted to symtabs).  */

struct symbol *
find_addr_symbol (addr, symtabp, symaddrp)
     CORE_ADDR addr;
     struct symtab **symtabp;
     CORE_ADDR *symaddrp;
{
  struct symtab *symtab, *best_symtab;
  struct objfile *objfile;
  register int bot, top;
  register struct symbol *sym;
  register CORE_ADDR sym_addr;
  struct block *block;
  int blocknum;

  /* Info on best symbol seen so far */

  register CORE_ADDR best_sym_addr = 0;
  struct symbol *best_sym = 0;

  /* FIXME -- we should pull in all the psymtabs, too!  */
  ALL_SYMTABS (objfile, symtab)
    {
      /* Search the global and static blocks in this symtab for
	 the closest symbol-address to the desired address.  */

      for (blocknum = GLOBAL_BLOCK; blocknum <= STATIC_BLOCK; blocknum++)
	{
	  QUIT;
	  block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (symtab), blocknum);
	  top = BLOCK_NSYMS (block);
	  for (bot = 0; bot < top; bot++)
	    {
	      sym = BLOCK_SYM (block, bot);
	      switch (SYMBOL_CLASS (sym))
		{
		case LOC_STATIC:	
		case LOC_LABEL:	
		  sym_addr = SYMBOL_VALUE_ADDRESS (sym);
		  break;

		case LOC_BLOCK:
		  sym_addr = BLOCK_START (SYMBOL_BLOCK_VALUE (sym));
		  break;

		default:
		  continue;
		}

		if (sym_addr <= addr)
		  if (sym_addr > best_sym_addr)
		    {
		      /* Quit if we found an exact match.  */
		      best_sym = sym;
		      best_sym_addr = sym_addr;
		      best_symtab = symtab;
		      if (sym_addr == addr)
			goto done;
		    }
	    }
	}
    }

 done:
  if (symtabp)
    *symtabp = best_symtab;
  if (symaddrp)
    *symaddrp = best_sym_addr;
  return best_sym;
}
#endif /* 0 */

/* Find the source file and line number for a given PC value.
   Return a structure containing a symtab pointer, a line number,
   and a pc range for the entire source line.
   The value's .pc field is NOT the specified pc.
   NOTCURRENT nonzero means, if specified pc is on a line boundary,
   use the line that ends there.  Otherwise, in that case, the line
   that begins there is used.  */

/* The big complication here is that a line may start in one file, and end just
   before the start of another file.  This usually occurs when you #include
   code in the middle of a subroutine.  To properly find the end of a line's PC
   range, we must search all symtabs associated with this compilation unit, and
   find the one whose first PC is closer than that of the next line in this
   symtab.  */

/* If it's worth the effort, we could be using a binary search.  */

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
  struct symtab_and_line val;
  struct blockvector *bv;

  /* Info on best line seen so far, and where it starts, and its file.  */

  struct linetable_entry *best = NULL;
  CORE_ADDR best_end = 0;
  struct symtab *best_symtab = 0;

  /* Store here the first line number
     of a file which contains the line at the smallest pc after PC.
     If we don't find a line whose range contains PC,
     we will use a line one less than this,
     with a range from the start of that file to the first line's pc.  */
  struct linetable_entry *alt = NULL;
  struct symtab *alt_symtab = 0;

  /* Info on best line seen in this file.  */

  struct linetable_entry *prev;

  /* If this pc is not from the current frame,
     it is the address of the end of a call instruction.
     Quite likely that is the start of the following statement.
     But what we want is the statement containing the instruction.
     Fudge the pc to make sure we get that.  */

  if (notcurrent) pc -= 1;

  s = find_pc_symtab (pc);
  if (!s)
    {
      val.symtab = 0;
      val.line = 0;
      val.pc = pc;
      val.end = 0;
      return val;
    }

  bv = BLOCKVECTOR (s);

  /* Look at all the symtabs that share this blockvector.
     They all have the same apriori range, that we found was right;
     but they have different line tables.  */

  for (; s && BLOCKVECTOR (s) == bv; s = s->next)
    {
      /* Find the best line in this symtab.  */
      l = LINETABLE (s);
      if (!l)
        continue;
      len = l->nitems;
      if (len <= 0)
	{
	  /* I think len can be zero if the symtab lacks line numbers
	     (e.g. gcc -g1).  (Either that or the LINETABLE is NULL;
	     I'm not sure which, and maybe it depends on the symbol
	     reader).  */
	  continue;
	}

      prev = NULL;
      item = l->item;		/* Get first line info */

      /* Is this file's first line closer than the first lines of other files?
	 If so, record this file, and its first line, as best alternate.  */
      if (item->pc > pc && (!alt || item->pc < alt->pc))
	{
	  alt = item;
	  alt_symtab = s;
	}

      for (i = 0; i < len; i++, item++)
	{
	  /* Return the last line that did not start after PC.  */
	  if (item->pc > pc)
	    break;

	  prev = item;
	}

      /* At this point, prev points at the line whose start addr is <= pc, and
	 item points at the next line.  If we ran off the end of the linetable
	 (pc >= start of the last line), then prev == item.  If pc < start of
	 the first line, prev will not be set.  */

      /* Is this file's best line closer than the best in the other files?
	 If so, record this file, and its best line, as best so far.  */

      if (prev && (!best || prev->pc > best->pc))
	{
	  best = prev;
	  best_symtab = s;
	  /* If another line is in the linetable, and its PC is closer
	     than the best_end we currently have, take it as best_end.  */
	  if (i < len && (best_end == 0 || best_end > item->pc))
	    best_end = item->pc;
	}
    }

  if (!best_symtab)
    {
      if (!alt_symtab)
	{			/* If we didn't find any line # info, just
				 return zeros.  */
	  val.symtab = 0;
	  val.line = 0;
	  val.pc = pc;
	  val.end = 0;
	}
      else
	{
	  val.symtab = alt_symtab;
	  val.line = alt->line - 1;

	  /* Don't return line 0, that means that we didn't find the line.  */
	  if (val.line == 0) ++val.line;

	  val.pc = BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
	  val.end = alt->pc;
	}
    }
  else
    {
      val.symtab = best_symtab;
      val.line = best->line;
      val.pc = best->pc;
      if (best_end && (!alt || best_end < alt->pc))
	val.end = best_end;
      else if (alt)
	val.end = alt->pc;
      else
	val.end = BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
    }
  return val;
}

static int find_line_symtab PARAMS ((struct symtab *, int, struct linetable **,
				     int *, int *));

/* Find line number LINE in any symtab whose name is the same as
   SYMTAB.

   If found, return 1, set *LINETABLE to the linetable in which it was
   found, set *INDEX to the index in the linetable of the best entry
   found, and set *EXACT_MATCH nonzero if the value returned is an
   exact match.

   If not found, return 0.  */

static int
find_line_symtab (symtab, line, linetable, index, exact_match)
     struct symtab *symtab;
     int line;
     struct linetable **linetable;
     int *index;
     int *exact_match;
{
  int exact;

  /* BEST_INDEX and BEST_LINETABLE identify the smallest linenumber > LINE
     so far seen.  */

  int best_index;
  struct linetable *best_linetable;

  /* First try looking it up in the given symtab.  */
  best_linetable = LINETABLE (symtab);
  best_index = find_line_common (best_linetable, line, &exact);
  if (best_index < 0 || !exact)
    {
      /* Didn't find an exact match.  So we better keep looking for
	 another symtab with the same name.  In the case of xcoff,
	 multiple csects for one source file (produced by IBM's FORTRAN
	 compiler) produce multiple symtabs (this is unavoidable
	 assuming csects can be at arbitrary places in memory and that
	 the GLOBAL_BLOCK of a symtab has a begin and end address).  */

      /* BEST is the smallest linenumber > LINE so far seen,
	 or 0 if none has been seen so far.
	 BEST_INDEX and BEST_LINETABLE identify the item for it.  */
      int best;

      struct objfile *objfile;
      struct symtab *s;

      if (best_index >= 0)
	best = best_linetable->item[best_index].line;
      else
	best = 0;

      ALL_SYMTABS (objfile, s)
	{
	  struct linetable *l;
	  int ind;

	  if (!STREQ (symtab->filename, s->filename))
	    continue;
	  l = LINETABLE (s);
	  ind = find_line_common (l, line, &exact);
	  if (ind >= 0)
	    {
	      if (exact)
		{
		  best_index = ind;
		  best_linetable = l;
		  goto done;
		}
	      if (best == 0 || l->item[ind].line < best)
		{
		  best = l->item[ind].line;
		  best_index = ind;
		  best_linetable = l;
		}
	    }
	}
    }
 done:
  if (best_index < 0)
    return 0;

  if (index)
    *index = best_index;
  if (linetable)
    *linetable = best_linetable;
  if (exact_match)
    *exact_match = exact;
  return 1;
}

/* Find the PC value for a given source file and line number.
   Returns zero for invalid line number.
   The source file is specified with a struct symtab.  */

CORE_ADDR
find_line_pc (symtab, line)
     struct symtab *symtab;
     int line;
{
  struct linetable *l;
  int ind;

  if (symtab == 0)
    return 0;
  if (find_line_symtab (symtab, line, &l, &ind, NULL))
    return l->item[ind].pc;
  else
    return 0;
}

/* Find the range of pc values in a line.
   Store the starting pc of the line into *STARTPTR
   and the ending pc (start of next line) into *ENDPTR.
   Returns 1 to indicate success.
   Returns 0 if could not find the specified line.  */

int
find_line_pc_range (sal, startptr, endptr)
     struct symtab_and_line sal;
     CORE_ADDR *startptr, *endptr;
{
  CORE_ADDR startaddr;
  struct symtab_and_line found_sal;

  startaddr = sal.pc;
  if (startaddr == 0)
    {
      startaddr = find_line_pc (sal.symtab, sal.line);
    }
  if (startaddr == 0)
    return 0;

  /* This whole function is based on address.  For example, if line 10 has
     two parts, one from 0x100 to 0x200 and one from 0x300 to 0x400, then
     "info line *0x123" should say the line goes from 0x100 to 0x200
     and "info line *0x355" should say the line goes from 0x300 to 0x400.
     This also insures that we never give a range like "starts at 0x134
     and ends at 0x12c".  */

  found_sal = find_pc_line (startaddr, 0);
  if (found_sal.line != sal.line)
    {
      /* The specified line (sal) has zero bytes.  */
      *startptr = found_sal.pc;
      *endptr = found_sal.pc;
    }
  else
    {
      *startptr = found_sal.pc;
      *endptr = found_sal.end;
    }
  return 1;
}

/* Given a line table and a line number, return the index into the line
   table for the pc of the nearest line whose number is >= the specified one.
   Return -1 if none is found.  The value is >= 0 if it is an index.

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

  int best_index = -1;
  int best = 0;

  if (lineno <= 0)
    return -1;
  if (l == 0)
    return -1;

  len = l->nitems;
  for (i = 0; i < len; i++)
    {
      register struct linetable_entry *item = &(l->item[i]);

      if (item->line == lineno)
	{
	  /* Return the first (lowest address) entry which matches.  */
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

/* Given a function symbol SYM, find the symtab and line for the start
   of the function.
   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside the function.  */

static struct symtab_and_line
find_function_start_sal PARAMS ((struct symbol *sym, int));

static struct symtab_and_line
find_function_start_sal (sym, funfirstline)
     struct symbol *sym;
     int funfirstline;
{
  CORE_ADDR pc;
  struct symtab_and_line sal;

  pc = BLOCK_START (SYMBOL_BLOCK_VALUE (sym));
  if (funfirstline)
    {
      pc += FUNCTION_START_OFFSET;
      SKIP_PROLOGUE (pc);
    }
  sal = find_pc_line (pc, 0);

#ifdef PROLOGUE_FIRSTLINE_OVERLAP
  /* Convex: no need to suppress code on first line, if any */
  sal.pc = pc;
#else
  /* Check if SKIP_PROLOGUE left us in mid-line, and the next
     line is still part of the same function.  */
  if (sal.pc != pc
      && BLOCK_START (SYMBOL_BLOCK_VALUE (sym)) <= sal.end
      && sal.end < BLOCK_END (SYMBOL_BLOCK_VALUE (sym)))
    {
      /* First pc of next line */
      pc = sal.end;
      /* Recalculate the line number (might not be N+1).  */
      sal = find_pc_line (pc, 0);
    }
  sal.pc = pc;
#endif

  return sal;
}

/* If P is of the form "operator[ \t]+..." where `...' is
   some legitimate operator text, return a pointer to the
   beginning of the substring of the operator text.
   Otherwise, return "".  */
static char *
operator_chars (p, end)
     char *p;
     char **end;
{
  *end = "";
  if (strncmp (p, "operator", 8))
    return *end;
  p += 8;

  /* Don't get faked out by `operator' being part of a longer
     identifier.  */
  if (isalpha(*p) || *p == '_' || *p == '$' || *p == '\0')
    return *end;

  /* Allow some whitespace between `operator' and the operator symbol.  */
  while (*p == ' ' || *p == '\t')
    p++;

  /* Recognize 'operator TYPENAME'. */

  if (isalpha(*p) || *p == '_' || *p == '$')
    {
      register char *q = p+1;
      while (isalnum(*q) || *q == '_' || *q == '$')
	q++;
      *end = q;
      return p;
    }

  switch (*p)
    {
    case '!':
    case '=':
    case '*':
    case '/':
    case '%':
    case '^':
      if (p[1] == '=')
	*end = p+2;
      else
	*end = p+1;
      return p;
    case '<':
    case '>':
    case '+':
    case '-':
    case '&':
    case '|':
      if (p[1] == '=' || p[1] == p[0])
	*end = p+2;
      else
	*end = p+1;
      return p;
    case '~':
    case ',':
      *end = p+1;
      return p;
    case '(':
      if (p[1] != ')')
	error ("`operator ()' must be specified without whitespace in `()'");
      *end = p+2;
      return p;
    case '?':
      if (p[1] != ':')
	error ("`operator ?:' must be specified without whitespace in `?:'");
      *end = p+2;
      return p;
    case '[':
      if (p[1] != ']')
	error ("`operator []' must be specified without whitespace in `[]'");
      *end = p+2;
      return p;
    default:
      error ("`operator %s' not supported", p);
      break;
    }
  *end = "";
  return *end;
}

/* Return the number of methods described for TYPE, including the
   methods from types it derives from. This can't be done in the symbol
   reader because the type of the baseclass might still be stubbed
   when the definition of the derived class is parsed.  */

static int total_number_of_methods PARAMS ((struct type *type));

static int
total_number_of_methods (type)
     struct type *type;
{
  int n;
  int count;

  CHECK_TYPEDEF (type);
  count = TYPE_NFN_FIELDS_TOTAL (type);

  for (n = 0; n < TYPE_N_BASECLASSES (type); n++)
    count += total_number_of_methods (TYPE_BASECLASS (type, n));

  return count;
}

/* Recursive helper function for decode_line_1.
   Look for methods named NAME in type T.
   Return number of matches.
   Put matches in SYM_ARR, which should have been allocated with
   a size of total_number_of_methods (T) * sizeof (struct symbol *).
   Note that this function is g++ specific.  */

int
find_methods (t, name, sym_arr)
     struct type *t;
     char *name;
     struct symbol **sym_arr;
{
  int i1 = 0;
  int ibase;
  struct symbol *sym_class;
  char *class_name = type_name_no_tag (t);
  /* Ignore this class if it doesn't have a name.  This is ugly, but
     unless we figure out how to get the physname without the name of
     the class, then the loop can't do any good.  */
  if (class_name
      && (sym_class = lookup_symbol (class_name,
				     (struct block *)NULL,
				     STRUCT_NAMESPACE,
				     (int *)NULL,
				     (struct symtab **)NULL)))
    {
      int method_counter;
      /* FIXME: Shouldn't this just be CHECK_TYPEDEF (t)?  */
      t = SYMBOL_TYPE (sym_class);
      for (method_counter = TYPE_NFN_FIELDS (t) - 1;
	   method_counter >= 0;
	   --method_counter)
	{
	  int field_counter;
	  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, method_counter);
	  char *method_name = TYPE_FN_FIELDLIST_NAME (t, method_counter);
	  char dem_opname[64];

          if (strncmp(method_name, "__", 2)==0 ||
	    strncmp(method_name, "op", 2)==0 ||
	    strncmp(method_name, "type", 4)==0 )
	    {
	      if (cplus_demangle_opname(method_name, dem_opname, DMGL_ANSI))
	        method_name = dem_opname;
	      else if (cplus_demangle_opname(method_name, dem_opname, 0))
	        method_name = dem_opname; 
	    }
	  if (STREQ (name, method_name))
	    /* Find all the fields with that name.  */
	    for (field_counter = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
		 field_counter >= 0;
		 --field_counter)
	      {
		char *phys_name;
		if (TYPE_FN_FIELD_STUB (f, field_counter))
		  check_stub_method (t, method_counter, field_counter);
		phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
		/* Destructor is handled by caller, dont add it to the list */
		if (DESTRUCTOR_PREFIX_P (phys_name))
		  continue;

		/* FIXME: Why are we looking this up in the
		   SYMBOL_BLOCK_VALUE (sym_class)?  It is intended as a hook
		   for nested types?  If so, it should probably hook to the
		   type, not the symbol.  mipsread.c is the only symbol
		   reader which sets the SYMBOL_BLOCK_VALUE for types, and
		   this is not documented in symtab.h.  -26Aug93.  */

		sym_arr[i1] = lookup_symbol (phys_name,
					     SYMBOL_BLOCK_VALUE (sym_class),
					     VAR_NAMESPACE,
					     (int *) NULL,
					     (struct symtab **) NULL);
		if (sym_arr[i1]) i1++;
		else
		  {
		    fputs_filtered("(Cannot find method ", gdb_stdout);
		    fprintf_symbol_filtered (gdb_stdout, phys_name,
					     language_cplus,
					     DMGL_PARAMS | DMGL_ANSI);
		    fputs_filtered(" - possibly inlined.)\n", gdb_stdout);
		  }
	      }
	}
    }

  /* Only search baseclasses if there is no match yet, since names in
     derived classes override those in baseclasses.

     FIXME: The above is not true; it is only true of member functions
     if they have the same number of arguments (??? - section 13.1 of the
     ARM says the function members are not in the same scope but doesn't
     really spell out the rules in a way I understand.  In any case, if
     the number of arguments differ this is a case in which we can overload
     rather than hiding without any problem, and gcc 2.4.5 does overload
     rather than hiding in this case).  */

  if (i1)
    return i1;
  for (ibase = 0; ibase < TYPE_N_BASECLASSES (t); ibase++)
    i1 += find_methods(TYPE_BASECLASS(t, ibase), name,
		       sym_arr + i1);
  return i1;
}

/* Helper function for decode_line_1.
   Build a canonical line spec in CANONICAL if it is non-NULL and if
   the SAL has a symtab.
   If SYMNAME is non-NULL the canonical line spec is `filename:symname'.
   If SYMNAME is NULL the line number from SAL is used and the canonical
   line spec is `filename:linenum'.  */

static void
build_canonical_line_spec (sal, symname, canonical)
     struct symtab_and_line *sal;
     char *symname;
     char ***canonical;
{
  char **canonical_arr;
  char *canonical_name;
  char *filename;
  struct symtab *s = sal->symtab;

  if (s == (struct symtab *)NULL
      || s->filename == (char *)NULL
      || canonical == (char ***)NULL)
    return;
 
  canonical_arr = (char **) xmalloc (sizeof (char *));
  *canonical = canonical_arr;

  filename = s->filename;
  if (symname != NULL)
    {
      canonical_name = xmalloc (strlen (filename) + strlen (symname) + 2);
      sprintf (canonical_name, "%s:%s", filename, symname);
    }
  else
    {
      canonical_name = xmalloc (strlen (filename) + 30);
      sprintf (canonical_name, "%s:%d", filename, sal->line);
    }
  canonical_arr[0] = canonical_name;
}

/* Parse a string that specifies a line number.
   Pass the address of a char * variable; that variable will be
   advanced over the characters actually parsed.

   The string can be:

   LINENUM -- that line number in current file.  PC returned is 0.
   FILE:LINENUM -- that line in that file.  PC returned is 0.
   FUNCTION -- line number of openbrace of that function.
      PC returned is the start of the function.
   VARIABLE -- line number of definition of that variable.
      PC returned is 0.
   FILE:FUNCTION -- likewise, but prefer functions in that file.
   *EXPR -- line in which address EXPR appears.

   FUNCTION may be an undebuggable function found in minimal symbol table.

   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside a function when a function is specified, and it is
   not OK to specify a variable or type to get its line number.

   DEFAULT_SYMTAB specifies the file to use if none is specified.
   It defaults to current_source_symtab.
   DEFAULT_LINE specifies the line number to use for relative
   line numbers (that start with signs).  Defaults to current_source_line.
   If CANONICAL is non-NULL, store an array of strings containing the canonical
   line specs there if necessary. Currently overloaded member functions and
   line numbers or static functions without a filename yield a canonical
   line spec. The array and the line spec strings are allocated on the heap,
   it is the callers responsibility to free them.

   Note that it is possible to return zero for the symtab
   if no file is validly specified.  Callers must check that.
   Also, the line number returned may be invalid.  */

/* We allow single quotes in various places.  This is a hideous
   kludge, which exists because the completer can't yet deal with the
   lack of single quotes.  FIXME: write a linespec_completer which we
   can use as appropriate instead of make_symbol_completion_list.  */

struct symtabs_and_lines
decode_line_1 (argptr, funfirstline, default_symtab, default_line, canonical)
     char **argptr;
     int funfirstline;
     struct symtab *default_symtab;
     int default_line;
     char ***canonical;
{
  struct symtabs_and_lines values;
#ifdef HPPA_COMPILER_BUG
  /* FIXME: The native HP 9000/700 compiler has a bug which appears
     when optimizing this file with target i960-vxworks.  I haven't
     been able to construct a simple test case.  The problem is that
     in the second call to SKIP_PROLOGUE below, the compiler somehow
     does not realize that the statement val = find_pc_line (...) will
     change the values of the fields of val.  It extracts the elements
     into registers at the top of the block, and does not update the
     registers after the call to find_pc_line.  You can check this by
     inserting a printf at the end of find_pc_line to show what values
     it is returning for val.pc and val.end and another printf after
     the call to see what values the function actually got (remember,
     this is compiling with cc -O, with this patch removed).  You can
     also examine the assembly listing: search for the second call to
     skip_prologue; the LDO statement before the next call to
     find_pc_line loads the address of the structure which
     find_pc_line will return; if there is a LDW just before the LDO,
     which fetches an element of the structure, then the compiler
     still has the bug.

     Setting val to volatile avoids the problem.  We must undef
     volatile, because the HPPA native compiler does not define
     __STDC__, although it does understand volatile, and so volatile
     will have been defined away in defs.h.  */
#undef volatile
  volatile struct symtab_and_line val;
#define volatile /*nothing*/
#else
  struct symtab_and_line val;
#endif
  register char *p, *p1;
  char *q, *pp;
#if 0
  char *q1;
#endif
  register struct symtab *s;

  register struct symbol *sym;
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab;

  register CORE_ADDR pc;
  register struct minimal_symbol *msymbol;
  char *copy;
  struct symbol *sym_class;
  int i1;
  int is_quoted, has_parens;
  struct symbol **sym_arr;
  struct type *t;
  char *saved_arg = *argptr;
  extern char *gdb_completer_quote_characters;
  
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
	xmalloc (sizeof (struct symtab_and_line));
      values.nelts = 1;
      values.sals[0] = find_pc_line (pc, 0);
      values.sals[0].pc = pc;
      return values;
    }

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */

  s = NULL;
  is_quoted = (**argptr
	       && strchr (gdb_completer_quote_characters, **argptr) != NULL);
  has_parens = ((pp = strchr (*argptr, '(')) != NULL
		 && (pp = strchr (pp, ')')) != NULL);

  for (p = *argptr; *p; p++)
    {
      if (p[0] == '<') 
	{
	  while(++p && *p != '>');
	  if (!p)
	    {
	      error ("non-matching '<' and '>' in command");
	    }
	}
      if (p[0] == ':' || p[0] == ' ' || p[0] == '\t')
	break;
    }
  while (p[0] == ' ' || p[0] == '\t') p++;

  if ((p[0] == ':') && !has_parens)
    {

      /*  C++  */
      if (is_quoted) *argptr = *argptr+1;
      if (p[1] ==':')
	{
	  /* Extract the class name.  */
	  p1 = p;
	  while (p != *argptr && p[-1] == ' ') --p;
	  copy = (char *) alloca (p - *argptr + 1);
	  memcpy (copy, *argptr, p - *argptr);
	  copy[p - *argptr] = 0;

	  /* Discard the class name from the arg.  */
	  p = p1 + 2;
	  while (*p == ' ' || *p == '\t') p++;
	  *argptr = p;

	  sym_class = lookup_symbol (copy, 0, STRUCT_NAMESPACE, 0, 
				     (struct symtab **)NULL);
       
	  if (sym_class &&
	      (t = check_typedef (SYMBOL_TYPE (sym_class)),
	       (TYPE_CODE (t) == TYPE_CODE_STRUCT
		|| TYPE_CODE (t) == TYPE_CODE_UNION)))
	    {
	      /* Arg token is not digits => try it as a function name
		 Find the next token(everything up to end or next blank). */
	      if (**argptr
		  && strchr (gdb_completer_quote_characters, **argptr) != NULL)
		{
		  p = skip_quoted(*argptr);
		  *argptr = *argptr + 1;
		}
	      else
		{
	          p = *argptr;
	          while (*p && *p!=' ' && *p!='\t' && *p!=',' && *p!=':') p++;
		}
/*
	      q = operator_chars (*argptr, &q1);
	      if (q1 - q)
		{
		  char *opname;
		  char *tmp = alloca (q1 - q + 1);
		  memcpy (tmp, q, q1 - q);
		  tmp[q1 - q] = '\0';
		  opname = cplus_mangle_opname (tmp, DMGL_ANSI);
		  if (opname == NULL)
		    {
		      error_begin ();
		      printf_filtered ("no mangling for \"%s\"\n", tmp);
		      cplusplus_hint (saved_arg);
		      return_to_top_level (RETURN_ERROR);
		    }
		  copy = (char*) alloca (3 + strlen(opname));
		  sprintf (copy, "__%s", opname);
		  p = q1;
		}
	      else
*/
		{
		  copy = (char *) alloca (p - *argptr + 1 );
		  memcpy (copy, *argptr, p - *argptr);
		  copy[p - *argptr] = '\0';
		  if (p != *argptr
		      && copy[p - *argptr - 1]
		      && strchr (gdb_completer_quote_characters,
				 copy[p - *argptr - 1]) != NULL)
		    copy[p - *argptr - 1] = '\0';
		}

	      /* no line number may be specified */
	      while (*p == ' ' || *p == '\t') p++;
	      *argptr = p;

	      sym = 0;
	      i1 = 0;		/*  counter for the symbol array */
	      sym_arr = (struct symbol **) alloca(total_number_of_methods (t)
						  * sizeof(struct symbol *));

	      /* Cfront objects don't have fieldlists.  */
	      if (destructor_name_p (copy, t) && TYPE_FN_FIELDLISTS (t) != NULL)
		{
		  /* destructors are a special case.  */
		  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, 0);
		  int len = TYPE_FN_FIELDLIST_LENGTH (t, 0) - 1;
		  /* gcc 1.x puts destructor in last field,
		     gcc 2.x puts destructor in first field.  */
		  char *phys_name = TYPE_FN_FIELD_PHYSNAME (f, len);
		  if (!DESTRUCTOR_PREFIX_P (phys_name))
		    {
		      phys_name = TYPE_FN_FIELD_PHYSNAME (f, 0);
		      if (!DESTRUCTOR_PREFIX_P (phys_name))
			phys_name = "";
		    }
		  sym_arr[i1] =
		    lookup_symbol (phys_name, SYMBOL_BLOCK_VALUE (sym_class),
				   VAR_NAMESPACE, 0, (struct symtab **)NULL);
		  if (sym_arr[i1]) i1++;
		}
	      else
		i1 = find_methods (t, copy, sym_arr);
	      if (i1 == 1)
		{
		  /* There is exactly one field with that name.  */
		  sym = sym_arr[0];

		  if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
		    {
		      values.sals = (struct symtab_and_line *)xmalloc (sizeof (struct symtab_and_line));
		      values.nelts = 1;
		      values.sals[0] = find_function_start_sal (sym,
								funfirstline);
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
		  return decode_line_2 (sym_arr, i1, funfirstline, canonical);
		}
	      else
		{
		  char *tmp;

		  if (OPNAME_PREFIX_P (copy))
		    {
		      tmp = (char *)alloca (strlen (copy+3) + 9);
		      strcpy (tmp, "operator ");
		      strcat (tmp, copy+3);
		    }
		  else
		    tmp = copy;
		  error_begin ();
		  if (tmp[0] == '~')
		    printf_filtered
		      ("the class `%s' does not have destructor defined\n",
		       SYMBOL_SOURCE_NAME(sym_class));
		  else
		    printf_filtered
		      ("the class %s does not have any method named %s\n",
		       SYMBOL_SOURCE_NAME(sym_class), tmp);
		  cplusplus_hint (saved_arg);
		  return_to_top_level (RETURN_ERROR);
		}
	    }
	  else
	    {
	      error_begin ();
	      /* The quotes are important if copy is empty.  */
	      printf_filtered
		("can't find class, struct, or union named \"%s\"\n", copy);
	      cplusplus_hint (saved_arg);
	      return_to_top_level (RETURN_ERROR);
	    }
	}
      /*  end of C++  */


      /* Extract the file name.  */
      p1 = p;
      while (p != *argptr && p[-1] == ' ') --p;
      copy = (char *) alloca (p - *argptr + 1);
      memcpy (copy, *argptr, p - *argptr);
      copy[p - *argptr] = 0;

      /* Find that file's data.  */
      s = lookup_symtab (copy);
      if (s == 0)
	{
	  if (!have_full_symbols () && !have_partial_symbols ())
	    error (no_symtab_msg);
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

  q = *argptr;
  if (*q == '-' || *q == '+') q++;
  while (*q >= '0' && *q <= '9')
    q++;

  if (q != *argptr && (*q == 0 || *q == ' ' || *q == '\t' || *q == ','))
    {
      /* We found a token consisting of all digits -- at least one digit.  */
      enum sign {none, plus, minus} sign = none;

      /* We might need a canonical line spec if no file was specified.  */
      int need_canonical = (s == 0) ? 1 : 0;

      /* This is where we need to make sure that we have good defaults.
	 We must guarantee that this section of code is never executed
	 when we are called with just a function name, since
	 select_source_symtab calls us with such an argument  */

      if (s == 0 && default_symtab == 0)
	{
	  select_source_symtab (0);
	  default_symtab = current_source_symtab;
	  default_line = current_source_line;
	}

      if (**argptr == '+')
	sign = plus, (*argptr)++;
      else if (**argptr == '-')
	sign = minus, (*argptr)++;
      val.line = atoi (*argptr);
      switch (sign)
	{
	case plus:
	  if (q == *argptr)
	    val.line = 5;
	  if (s == 0)
	    val.line = default_line + val.line;
	  break;
	case minus:
	  if (q == *argptr)
	    val.line = 15;
	  if (s == 0)
	    val.line = default_line - val.line;
	  else
	    val.line = 1;
	  break;
	case none:
	  break;	/* No need to adjust val.line.  */
	}

      while (*q == ' ' || *q == '\t') q++;
      *argptr = q;
      if (s == 0)
	s = default_symtab;
      val.symtab = s;
      val.pc = 0;
      values.sals = (struct symtab_and_line *)xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = val;
      values.nelts = 1;
      if (need_canonical)
	build_canonical_line_spec (values.sals, NULL, canonical);
      return values;
    }

  /* Arg token is not digits => try it as a variable name
     Find the next token (everything up to end or next whitespace).  */

  if (**argptr == '$')		/* Convenience variable */
    p = skip_quoted (*argptr + 1);
  else if (is_quoted)
    {
      p = skip_quoted (*argptr);
      if (p[-1] != '\'')
        error ("Unmatched single quote.");
    }
  else if (has_parens)
    {
      p = pp+1;
    }
  else 
    {
      p = skip_quoted(*argptr);
    }

  copy = (char *) alloca (p - *argptr + 1);
  memcpy (copy, *argptr, p - *argptr);
  copy[p - *argptr] = '\0';
  if (p != *argptr
      && copy[0]
      && copy[0] == copy [p - *argptr - 1]
      && strchr (gdb_completer_quote_characters, copy[0]) != NULL)
    {
      copy [p - *argptr - 1] = '\0';
      copy++;
    }
  while (*p == ' ' || *p == '\t') p++;
  *argptr = p;

  /* See if it's a convenience variable */

  if (*copy == '$')
    {
      value_ptr valx;
      int need_canonical = (s == 0) ? 1 : 0;

      valx = value_of_internalvar (lookup_internalvar (copy + 1));
      if (TYPE_CODE (VALUE_TYPE (valx)) != TYPE_CODE_INT)
	error ("Convenience variables used in line specs must have integer values.");

      val.symtab = s ? s : default_symtab;
      val.line = value_as_long (valx);
      val.pc = 0;

      values.sals = (struct symtab_and_line *)xmalloc (sizeof val);
      values.sals[0] = val;
      values.nelts = 1;

      if (need_canonical)
	build_canonical_line_spec (values.sals, NULL, canonical);

      return values;
    }


  /* Look up that token as a variable.
     If file specified, use that file's per-file block to start with.  */

  sym = lookup_symbol (copy,
		       (s ? BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK)
			: get_selected_block ()),
		       VAR_NAMESPACE, 0, &sym_symtab);

  if (sym != NULL)
    {
      if (SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  /* Arg is the name of a function */
	  values.sals = (struct symtab_and_line *)xmalloc (sizeof (struct symtab_and_line));
	  values.sals[0] = find_function_start_sal (sym, funfirstline);
	  values.nelts = 1;

	  /* Don't use the SYMBOL_LINE; if used at all it points to
	     the line containing the parameters or thereabouts, not
	     the first line of code.  */

	  /* We might need a canonical line spec if it is a static
	     function.  */
	  if (s == 0)
	    {
	      struct blockvector *bv = BLOCKVECTOR (sym_symtab);
	      struct block *b = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	      if (lookup_block_symbol (b, copy, VAR_NAMESPACE) != NULL)
		build_canonical_line_spec (values.sals, copy, canonical);
	    }
	  return values;
	}
      else
	{
	  if (funfirstline)
	    error ("\"%s\" is not a function", copy);
	  else if (SYMBOL_LINE (sym) != 0)
	    {
	      /* We know its line number.  */
	      values.sals = (struct symtab_and_line *)
		xmalloc (sizeof (struct symtab_and_line));
	      values.nelts = 1;
	      memset (&values.sals[0], 0, sizeof (values.sals[0]));
	      values.sals[0].symtab = sym_symtab;
	      values.sals[0].line = SYMBOL_LINE (sym);
	      return values;
	    }
	  else
	    /* This can happen if it is compiled with a compiler which doesn't
	       put out line numbers for variables.  */
	    /* FIXME: Shouldn't we just set .line and .symtab to zero
	       and return?  For example, "info line foo" could print
	       the address.  */
	    error ("Line number not known for symbol \"%s\"", copy);
	}
    }

  msymbol = lookup_minimal_symbol (copy, NULL, NULL);
  if (msymbol != NULL)
    {
      val.symtab = 0;
      val.line = 0;
      val.pc = SYMBOL_VALUE_ADDRESS (msymbol);
      if (funfirstline)
	{
	  val.pc += FUNCTION_START_OFFSET;
	  SKIP_PROLOGUE (val.pc);
	}
      values.sals = (struct symtab_and_line *)xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = val;
      values.nelts = 1;
      return values;
    }

  if (!have_full_symbols () &&
      !have_partial_symbols () && !have_minimal_symbols ())
    error (no_symtab_msg);

  error ("Function \"%s\" not defined.", copy);
  return values;	/* for lint */
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
			current_source_symtab, current_source_line,
			(char ***)NULL);
  if (*string)
    error ("Junk at end of line specification: %s", string);
  return sals;
}

/* Given a list of NELTS symbols in SYM_ARR, return a list of lines to
   operate on (ask user if necessary).
   If CANONICAL is non-NULL return a corresponding array of mangled names
   as canonical line specs there.  */

static struct symtabs_and_lines
decode_line_2 (sym_arr, nelts, funfirstline, canonical)
     struct symbol *sym_arr[];
     int nelts;
     int funfirstline;
     char ***canonical;
{
  struct symtabs_and_lines values, return_values;
  char *args, *arg1;
  int i;
  char *prompt;
  char *symname;
  struct cleanup *old_chain;
  char **canonical_arr = (char **)NULL;

  values.sals = (struct symtab_and_line *) alloca (nelts * sizeof(struct symtab_and_line));
  return_values.sals = (struct symtab_and_line *) xmalloc (nelts * sizeof(struct symtab_and_line));
  old_chain = make_cleanup (free, return_values.sals);

  if (canonical)
    {
      canonical_arr = (char **) xmalloc (nelts * sizeof (char *));
      make_cleanup (free, canonical_arr);
      memset (canonical_arr, 0, nelts * sizeof (char *));
      *canonical = canonical_arr;
    }

  i = 0;
  printf_unfiltered("[0] cancel\n[1] all\n");
  while (i < nelts)
    {
      if (sym_arr[i] && SYMBOL_CLASS (sym_arr[i]) == LOC_BLOCK)
	{
	  values.sals[i] = find_function_start_sal (sym_arr[i], funfirstline);
	  printf_unfiltered ("[%d] %s at %s:%d\n",
			     (i+2),
			     SYMBOL_SOURCE_NAME (sym_arr[i]),
			     values.sals[i].symtab->filename,
			     values.sals[i].line);
	}
      else
	printf_unfiltered ("?HERE\n");
      i++;
    }
  
  if ((prompt = getenv ("PS2")) == NULL)
    {
      prompt = ">";
    }
  printf_unfiltered("%s ",prompt);
  gdb_flush(gdb_stdout);

  args = command_line_input ((char *) NULL, 0, "overload-choice");
  
  if (args == 0 || *args == 0)
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
	  if (canonical_arr)
	    {
	      for (i = 0; i < nelts; i++)
		{
	          if (canonical_arr[i] == NULL)
		    {
		      symname = SYMBOL_NAME (sym_arr[i]);
	              canonical_arr[i] = savestring (symname, strlen (symname));
		    }
		}
	    }
	  memcpy (return_values.sals, values.sals,
		  (nelts * sizeof(struct symtab_and_line)));
	  return_values.nelts = nelts;
	  discard_cleanups (old_chain);
	  return return_values;
	}

      if (num > nelts + 2)
	{
	  printf_unfiltered ("No choice number %d.\n", num);
	}
      else
	{
	  num -= 2;
	  if (values.sals[num].pc)
	    {
	      if (canonical_arr)
		{
		  symname = SYMBOL_NAME (sym_arr[num]);
		  make_cleanup (free, symname);
		  canonical_arr[i] = savestring (symname, strlen (symname));
		}
	      return_values.sals[i++] = values.sals[num];
	      values.sals[num].pc = 0;
	    }
	  else
	    {
	      printf_unfiltered ("duplicate request for %d ignored.\n", num);
	    }
	}

      args = arg1;
      while (*args == ' ' || *args == '\t') args++;
    }
  return_values.nelts = i;
  discard_cleanups (old_chain);
  return return_values;
}


/* Slave routine for sources_info.  Force line breaks at ,'s.
   NAME is the name to print and *FIRST is nonzero if this is the first
   name printed.  Set *FIRST to zero.  */
static void
output_source_filename (name, first)
     char *name;
     int *first;
{
  /* Table of files printed so far.  Since a single source file can
     result in several partial symbol tables, we need to avoid printing
     it more than once.  Note: if some of the psymtabs are read in and
     some are not, it gets printed both under "Source files for which
     symbols have been read" and "Source files for which symbols will
     be read in on demand".  I consider this a reasonable way to deal
     with the situation.  I'm not sure whether this can also happen for
     symtabs; it doesn't hurt to check.  */
  static char **tab = NULL;
  /* Allocated size of tab in elements.
     Start with one 256-byte block (when using GNU malloc.c).
     24 is the malloc overhead when range checking is in effect.  */
  static int tab_alloc_size = (256 - 24) / sizeof (char *);
  /* Current size of tab in elements.  */
  static int tab_cur_size;

  char **p;

  if (*first)
    {
      if (tab == NULL)
	tab = (char **) xmalloc (tab_alloc_size * sizeof (*tab));
      tab_cur_size = 0;
    }

  /* Is NAME in tab?  */
  for (p = tab; p < tab + tab_cur_size; p++)
    if (STREQ (*p, name))
      /* Yes; don't print it again.  */
      return;
  /* No; add it to tab.  */
  if (tab_cur_size == tab_alloc_size)
    {
      tab_alloc_size *= 2;
      tab = (char **) xrealloc ((char *) tab, tab_alloc_size * sizeof (*tab));
    }
  tab[tab_cur_size++] = name;

  if (*first)
    {
      *first = 0;
    }
  else
    {
      printf_filtered (", ");
    }

  wrap_here ("");
  fputs_filtered (name, gdb_stdout);
}  

static void
sources_info (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  int first;
  
  if (!have_full_symbols () && !have_partial_symbols ())
    {
      error (no_symtab_msg);
    }
  
  printf_filtered ("Source files for which symbols have been read in:\n\n");

  first = 1;
  ALL_SYMTABS (objfile, s)
    {
      output_source_filename (s -> filename, &first);
    }
  printf_filtered ("\n\n");
  
  printf_filtered ("Source files for which symbols will be read in on demand:\n\n");

  first = 1;
  ALL_PSYMTABS (objfile, ps)
    {
      if (!ps->readin)
	{
	  output_source_filename (ps -> filename, &first);
	}
    }
  printf_filtered ("\n");
}

/* List all symbols (if REGEXP is NULL) or all symbols matching REGEXP.
   If CLASS is zero, list all symbols except functions, type names, and
		     constants (enums).
   If CLASS is 1, list only functions.
   If CLASS is 2, list only type names.
   If CLASS is 3, list only method names.

   BPT is non-zero if we should set a breakpoint at the functions
   we find.  */

static void
list_symbols (regexp, class, bpt, from_tty)
     char *regexp;
     int class;
     int bpt;
     int from_tty;
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  register struct block *b;
  register int i, j;
  register struct symbol *sym;
  struct partial_symbol **psym;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  char *val;
  static char *classnames[]
    = {"variable", "function", "type", "method"};
  int found_in_file = 0;
  int found_misc = 0;
  static enum minimal_symbol_type types[]
    = {mst_data, mst_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types2[]
    = {mst_bss,  mst_file_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types3[]
    = {mst_file_data,  mst_solib_trampoline, mst_abs, mst_unknown};
  static enum minimal_symbol_type types4[]
    = {mst_file_bss,   mst_text, mst_abs, mst_unknown};
  enum minimal_symbol_type ourtype = types[class];
  enum minimal_symbol_type ourtype2 = types2[class];
  enum minimal_symbol_type ourtype3 = types3[class];
  enum minimal_symbol_type ourtype4 = types4[class];

  if (regexp != NULL)
    {
      /* Make sure spacing is right for C++ operators.
	 This is just a courtesy to make the matching less sensitive
	 to how many spaces the user leaves between 'operator'
	 and <TYPENAME> or <OPERATOR>. */
      char *opend;
      char *opname = operator_chars (regexp, &opend);
      if (*opname)
	{
          int fix = -1; /* -1 means ok; otherwise number of spaces needed. */
	  if (isalpha(*opname) || *opname == '_' || *opname == '$')
	    {
	      /* There should 1 space between 'operator' and 'TYPENAME'. */
	      if (opname[-1] != ' ' || opname[-2] == ' ')
	        fix = 1;
	    }
	  else
	    {
	      /* There should 0 spaces between 'operator' and 'OPERATOR'. */
	      if (opname[-1] == ' ')
	        fix = 0;
	    }
	  /* If wrong number of spaces, fix it. */
	  if (fix >= 0)
	    {
	      char *tmp = (char*) alloca(opend-opname+10);
	      sprintf(tmp, "operator%.*s%s", fix, " ", opname);
	      regexp = tmp;
	    }
        }
      
      if (0 != (val = re_comp (regexp)))
	error ("Invalid regexp (%s): %s", val, regexp);
    }

  /* Search through the partial symtabs *first* for all symbols
     matching the regexp.  That way we don't have to reproduce all of
     the machinery below. */

  ALL_PSYMTABS (objfile, ps)
    {
      struct partial_symbol **bound, **gbound, **sbound;
      int keep_going = 1;
      
      if (ps->readin) continue;
      
      gbound = objfile->global_psymbols.list + ps->globals_offset + ps->n_global_syms;
      sbound = objfile->static_psymbols.list + ps->statics_offset + ps->n_static_syms;
      bound = gbound;
      
      /* Go through all of the symbols stored in a partial
	 symtab in one loop. */
      psym = objfile->global_psymbols.list + ps->globals_offset;
      while (keep_going)
	{
	  if (psym >= bound)
	    {
	      if (bound == gbound && ps->n_static_syms != 0)
		{
		  psym = objfile->static_psymbols.list + ps->statics_offset;
		  bound = sbound;
		}
	      else
		keep_going = 0;
	      continue;
	    }
	  else
	    {
	      QUIT;

	      /* If it would match (logic taken from loop below)
		 load the file and go on to the next one */
	      if ((regexp == NULL || SYMBOL_MATCHES_REGEXP (*psym))
		  && ((class == 0 && SYMBOL_CLASS (*psym) != LOC_TYPEDEF
		       && SYMBOL_CLASS (*psym) != LOC_BLOCK)
		      || (class == 1 && SYMBOL_CLASS (*psym) == LOC_BLOCK)
		      || (class == 2 && SYMBOL_CLASS (*psym) == LOC_TYPEDEF)
		      || (class == 3 && SYMBOL_CLASS (*psym) == LOC_BLOCK)))
		{
		  PSYMTAB_TO_SYMTAB(ps);
		  keep_going = 0;
		}
	    }
	  psym++;
	}
    }

  /* Here, we search through the minimal symbol tables for functions
     and variables that match, and force their symbols to be read.
     This is in particular necessary for demangled variable names,
     which are no longer put into the partial symbol tables.
     The symbol will then be found during the scan of symtabs below.

     For functions, find_pc_symtab should succeed if we have debug info
     for the function, for variables we have to call lookup_symbol
     to determine if the variable has debug info.
     If the lookup fails, set found_misc so that we will rescan to print
     any matching symbols without debug info.
  */

  if (class == 0 || class == 1)
    {
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  if (MSYMBOL_TYPE (msymbol) == ourtype ||
	      MSYMBOL_TYPE (msymbol) == ourtype2 ||
	      MSYMBOL_TYPE (msymbol) == ourtype3 ||
	      MSYMBOL_TYPE (msymbol) == ourtype4)
	    {
	      if (regexp == NULL || SYMBOL_MATCHES_REGEXP (msymbol))
		{
		  if (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol)))
		    {
		      if (class == 1
			  || lookup_symbol (SYMBOL_NAME (msymbol), 
					    (struct block *) NULL,
					    VAR_NAMESPACE,
					    0, (struct symtab **) NULL) == NULL)
		        found_misc = 1;
		    }
		}
	    }
	}
    }

  /* Printout here so as to get after the "Reading in symbols"
     messages which will be generated above.  */
  if (!bpt)
    printf_filtered (regexp
	  ? "All %ss matching regular expression \"%s\":\n"
	  : "All defined %ss:\n",
	  classnames[class],
	  regexp);

  ALL_SYMTABS (objfile, s)
    {
      found_in_file = 0;
      bv = BLOCKVECTOR (s);
      /* Often many files share a blockvector.
	 Scan each blockvector only once so that
	 we don't get every symbol many times.
	 It happens that the first symtab in the list
	 for any given blockvector is the main file.  */
      if (bv != prev_bv)
	for (i = GLOBAL_BLOCK; i <= STATIC_BLOCK; i++)
	  {
	    b = BLOCKVECTOR_BLOCK (bv, i);
	    /* Skip the sort if this block is always sorted.  */
	    if (!BLOCK_SHOULD_SORT (b))
	      sort_block_syms (b);
	    for (j = 0; j < BLOCK_NSYMS (b); j++)
	      {
		QUIT;
		sym = BLOCK_SYM (b, j);
		if ((regexp == NULL || SYMBOL_MATCHES_REGEXP (sym))
		    && ((class == 0 && SYMBOL_CLASS (sym) != LOC_TYPEDEF
			 && SYMBOL_CLASS (sym) != LOC_BLOCK
			 && SYMBOL_CLASS (sym) != LOC_CONST)
			|| (class == 1 && SYMBOL_CLASS (sym) == LOC_BLOCK)
			|| (class == 2 && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
			|| (class == 3 && SYMBOL_CLASS (sym) == LOC_BLOCK)))
		  {
		    if (bpt)
		      {
			/* Set a breakpoint here, if it's a function */
			if (class == 1)
			  {
			    /* There may be more than one function with the
			       same name but in different files.  In order to
			       set breakpoints on all of them, we must give
			       both the file name and the function name to
			       break_command.
			       Quoting the symbol name gets rid of problems
			       with mangled symbol names that contain
			       CPLUS_MARKER characters.  */
			    char *string =
			      (char *) alloca (strlen (s->filename)
					       + strlen (SYMBOL_NAME(sym))
					       + 4);
			    strcpy (string, s->filename);
			    strcat (string, ":'");
			    strcat (string, SYMBOL_NAME(sym));
			    strcat (string, "'");
			    break_command (string, from_tty);
			  }
		      }
		    else if (!found_in_file)
		      {
			fputs_filtered ("\nFile ", gdb_stdout);
			fputs_filtered (s->filename, gdb_stdout);
			fputs_filtered (":\n", gdb_stdout);
		      }
		    found_in_file = 1;
		    
		    if (class != 2 && i == STATIC_BLOCK)
		      printf_filtered ("static ");
		    
		    /* Typedef that is not a C++ class */
		    if (class == 2
			&& SYMBOL_NAMESPACE (sym) != STRUCT_NAMESPACE)
		      c_typedef_print (SYMBOL_TYPE(sym), sym, gdb_stdout);
		    /* variable, func, or typedef-that-is-c++-class */
		    else if (class < 2 || 
			     (class == 2 && 
			      SYMBOL_NAMESPACE(sym) == STRUCT_NAMESPACE))
		      {
			type_print (SYMBOL_TYPE (sym),
				    (SYMBOL_CLASS (sym) == LOC_TYPEDEF
				     ? "" : SYMBOL_SOURCE_NAME (sym)),
				    gdb_stdout, 0);
			
			printf_filtered (";\n");
		      }
		    else
		      {
# if 0  /* FIXME, why is this zapped out? */
			char buf[1024];
			c_type_print_base (TYPE_FN_FIELD_TYPE(t, i),
					   gdb_stdout, 0, 0); 
			c_type_print_varspec_prefix (TYPE_FN_FIELD_TYPE(t, i),
						     gdb_stdout, 0); 
			sprintf (buf, " %s::", type_name_no_tag (t));
			cp_type_print_method_args (TYPE_FN_FIELD_ARGS (t, i),
						   buf, name, gdb_stdout);
# endif
		      }
		  }
	      }
	  }
      prev_bv = bv;
    }

  /* If there are no eyes, avoid all contact.  I mean, if there are
     no debug symbols, then print directly from the msymbol_vector.  */

  if (found_misc || class != 1)
    {
      found_in_file = 0;
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  if (MSYMBOL_TYPE (msymbol) == ourtype ||
	      MSYMBOL_TYPE (msymbol) == ourtype2 ||
	      MSYMBOL_TYPE (msymbol) == ourtype3 ||
	      MSYMBOL_TYPE (msymbol) == ourtype4)
	    {
	      if (regexp == NULL || SYMBOL_MATCHES_REGEXP (msymbol))
		{
		  /* Functions:  Look up by address. */
		  if (class != 1 ||
		      (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol))))
		    {
		      /* Variables/Absolutes:  Look up by name */
		      if (lookup_symbol (SYMBOL_NAME (msymbol), 
					 (struct block *) NULL, VAR_NAMESPACE,
					 0, (struct symtab **) NULL) == NULL)
			{
                          if (bpt)
                            {
                              break_command (SYMBOL_NAME (msymbol), from_tty);
                              printf_filtered ("<function, no debug info> %s;\n",
                                               SYMBOL_SOURCE_NAME (msymbol));
                              continue;
                            }
			  if (!found_in_file)
			    {
			      printf_filtered ("\nNon-debugging symbols:\n");
			      found_in_file = 1;
			    }
			  printf_filtered ("	%08lx  %s\n",
					   (unsigned long) SYMBOL_VALUE_ADDRESS (msymbol),
					   SYMBOL_SOURCE_NAME (msymbol));
			}
		    }
		}
	    }
	}
    }
}

static void
variables_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  list_symbols (regexp, 0, 0, from_tty);
}

static void
functions_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  list_symbols (regexp, 1, 0, from_tty);
}

static void
types_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  list_symbols (regexp, 2, 0, from_tty);
}

#if 0
/* Tiemann says: "info methods was never implemented."  */
static void
methods_info (regexp)
     char *regexp;
{
  list_symbols (regexp, 3, 0, from_tty);
}
#endif /* 0 */

/* Breakpoint all functions matching regular expression. */
static void
rbreak_command (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  list_symbols (regexp, 1, 1, from_tty);
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
  return BLOCK_START (a) >= BLOCK_START (b)
      && BLOCK_END (a)   <= BLOCK_END (b);
}


/* Helper routine for make_symbol_completion_list.  */

static int return_val_size;
static int return_val_index;
static char **return_val;

#define COMPLETION_LIST_ADD_SYMBOL(symbol, sym_text, len, text, word) \
  do { \
    if (SYMBOL_DEMANGLED_NAME (symbol) != NULL) \
      /* Put only the mangled name on the list.  */ \
      /* Advantage:  "b foo<TAB>" completes to "b foo(int, int)" */ \
      /* Disadvantage:  "b foo__i<TAB>" doesn't complete.  */ \
      completion_list_add_name \
	(SYMBOL_DEMANGLED_NAME (symbol), (sym_text), (len), (text), (word)); \
    else \
      completion_list_add_name \
	(SYMBOL_NAME (symbol), (sym_text), (len), (text), (word)); \
  } while (0)

/*  Test to see if the symbol specified by SYMNAME (which is already
    demangled for C++ symbols) matches SYM_TEXT in the first SYM_TEXT_LEN
    characters.  If so, add it to the current completion list. */

static void
completion_list_add_name (symname, sym_text, sym_text_len, text, word)
     char *symname;
     char *sym_text;
     int sym_text_len;
     char *text;
     char *word;
{
  int newsize;
  int i;

  /* clip symbols that cannot match */

  if (strncmp (symname, sym_text, sym_text_len) != 0)
    {
      return;
    }

  /* Clip any symbol names that we've already considered.  (This is a
     time optimization)  */

  for (i = 0; i < return_val_index; ++i)
    {
      if (STREQ (symname, return_val[i]))
	{
	  return;
	}
    }
  
  /* We have a match for a completion, so add SYMNAME to the current list
     of matches. Note that the name is moved to freshly malloc'd space. */

  {
    char *new;
    if (word == sym_text)
      {
	new = xmalloc (strlen (symname) + 5);
	strcpy (new, symname);
      }
    else if (word > sym_text)
      {
	/* Return some portion of symname.  */
	new = xmalloc (strlen (symname) + 5);
	strcpy (new, symname + (word - sym_text));
      }
    else
      {
	/* Return some of SYM_TEXT plus symname.  */
	new = xmalloc (strlen (symname) + (sym_text - word) + 5);
	strncpy (new, word, sym_text - word);
	new[sym_text - word] = '\0';
	strcat (new, symname);
      }

    /* Recheck for duplicates if we intend to add a modified symbol.  */
    if (word != sym_text)
      {
	for (i = 0; i < return_val_index; ++i)
	  {
	    if (STREQ (new, return_val[i]))
	      {
		free (new);
		return;
	      }
	  }
      }

    if (return_val_index + 3 > return_val_size)
      {
	newsize = (return_val_size *= 2) * sizeof (char *);
	return_val = (char **) xrealloc ((char *) return_val, newsize);
      }
    return_val[return_val_index++] = new;
    return_val[return_val_index] = NULL;
  }
}

/* Return a NULL terminated array of all symbols (regardless of class) which
   begin by matching TEXT.  If the answer is no symbols, then the return value
   is an array which contains only a NULL pointer.

   Problem: All of the symbols have to be copied because readline frees them.
   I'm not going to worry about this; hopefully there won't be that many.  */

char **
make_symbol_completion_list (text, word)
     char *text;
     char *word;
{
  register struct symbol *sym;
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct minimal_symbol *msymbol;
  register struct objfile *objfile;
  register struct block *b, *surrounding_static_block = 0;
  register int i, j;
  struct partial_symbol **psym;
  /* The symbol we are completing on.  Points in same buffer as text.  */
  char *sym_text;
  /* Length of sym_text.  */
  int sym_text_len;

  /* Now look for the symbol we are supposed to complete on.
     FIXME: This should be language-specific.  */
  {
    char *p;
    char quote_found;
    char *quote_pos = NULL;

    /* First see if this is a quoted string.  */
    quote_found = '\0';
    for (p = text; *p != '\0'; ++p)
      {
	if (quote_found != '\0')
	  {
	    if (*p == quote_found)
	      /* Found close quote.  */
	      quote_found = '\0';
	    else if (*p == '\\' && p[1] == quote_found)
	      /* A backslash followed by the quote character
		 doesn't end the string.  */
	      ++p;
	  }
	else if (*p == '\'' || *p == '"')
	  {
	    quote_found = *p;
	    quote_pos = p;
	  }
      }
    if (quote_found == '\'')
      /* A string within single quotes can be a symbol, so complete on it.  */
      sym_text = quote_pos + 1;
    else if (quote_found == '"')
      /* A double-quoted string is never a symbol, nor does it make sense
	 to complete it any other way.  */
      return NULL;
    else
      {
	/* It is not a quoted string.  Break it based on the characters
	   which are in symbols.  */
	while (p > text)
	  {
	    if (isalnum (p[-1]) || p[-1] == '_' || p[-1] == '\0')
	      --p;
	    else
	      break;
	  }
	sym_text = p;
      }
  }

  sym_text_len = strlen (sym_text);

  return_val_size = 100;
  return_val_index = 0;
  return_val = (char **) xmalloc ((return_val_size + 1) * sizeof (char *));
  return_val[0] = NULL;

  /* Look through the partial symtabs for all symbols which begin
     by matching SYM_TEXT.  Add each one that you find to the list.  */

  ALL_PSYMTABS (objfile, ps)
    {
      /* If the psymtab's been read in we'll get it when we search
	 through the blockvector.  */
      if (ps->readin) continue;
      
      for (psym = objfile->global_psymbols.list + ps->globals_offset;
	   psym < (objfile->global_psymbols.list + ps->globals_offset
		   + ps->n_global_syms);
	   psym++)
	{
	  /* If interrupted, then quit. */
	  QUIT;
	  COMPLETION_LIST_ADD_SYMBOL (*psym, sym_text, sym_text_len, text, word);
	}
      
      for (psym = objfile->static_psymbols.list + ps->statics_offset;
	   psym < (objfile->static_psymbols.list + ps->statics_offset
		   + ps->n_static_syms);
	   psym++)
	{
	  QUIT;
	  COMPLETION_LIST_ADD_SYMBOL (*psym, sym_text, sym_text_len, text, word);
	}
    }

  /* At this point scan through the misc symbol vectors and add each
     symbol you find to the list.  Eventually we want to ignore
     anything that isn't a text symbol (everything else will be
     handled by the psymtab code above).  */

  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      COMPLETION_LIST_ADD_SYMBOL (msymbol, sym_text, sym_text_len, text, word);
    }

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */

  for (b = get_selected_block (); b != NULL; b = BLOCK_SUPERBLOCK (b))
    {
      if (!BLOCK_SUPERBLOCK (b))
	{
	  surrounding_static_block = b; 	/* For elmin of dups */
	}
      
      /* Also catch fields of types defined in this places which match our
	 text string.  Only complete on types visible from current context. */

      for (i = 0; i < BLOCK_NSYMS (b); i++)
	{
	  sym = BLOCK_SYM (b, i);
	  COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
	  if (SYMBOL_CLASS (sym) == LOC_TYPEDEF)
	    {
	      struct type *t = SYMBOL_TYPE (sym);
	      enum type_code c = TYPE_CODE (t);

	      if (c == TYPE_CODE_UNION || c == TYPE_CODE_STRUCT)
		{
		  for (j = TYPE_N_BASECLASSES (t); j < TYPE_NFIELDS (t); j++)
		    {
		      if (TYPE_FIELD_NAME (t, j))
			{
			  completion_list_add_name (TYPE_FIELD_NAME (t, j),
						      sym_text, sym_text_len, text, word);
			}
		    }
		}
	    }
	}
    }

  /* Go through the symtabs and check the externs and statics for
     symbols which match.  */

  ALL_SYMTABS (objfile, s)
    {
      QUIT;
      b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
      for (i = 0; i < BLOCK_NSYMS (b); i++)
	{
	  sym = BLOCK_SYM (b, i);
	  COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
	}
    }

  ALL_SYMTABS (objfile, s)
    {
      QUIT;
      b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
      /* Don't do this block twice.  */
      if (b == surrounding_static_block) continue;
      for (i = 0; i < BLOCK_NSYMS (b); i++)
	{
	  sym = BLOCK_SYM (b, i);
	  COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
	}
    }

  return (return_val);
}

/* Determine if PC is in the prologue of a function.  The prologue is the area
   between the first instruction of a function, and the first executable line.
   Returns 1 if PC *might* be in prologue, 0 if definately *not* in prologue.

   If non-zero, func_start is where we thing the prologue starts, possibly
   by previous examination of symbol table information.
 */

int
in_prologue (pc, func_start)
     CORE_ADDR pc;
     CORE_ADDR func_start;
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  if (!find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    goto nosyms;		/* Might be in prologue */

  sal = find_pc_line (func_addr, 0);

  if (sal.line == 0)
    goto nosyms;

  if (sal.end > func_addr
      && sal.end <= func_end)	/* Is prologue in function? */
    return pc < sal.end;	/* Yes, is pc in prologue? */

  /* The line after the prologue seems to be outside the function.  In this
     case, tell the caller to find the prologue the hard way.  */

  return 1;

/* Come here when symtabs don't contain line # info.  In this case, it is
   likely that the user has stepped into a library function w/o symbols, or
   is doing a stepi/nexti through code without symbols.  */

 nosyms:

/* If func_start is zero (meaning unknown) then we don't know whether pc is
   in the prologue or not.  I.E. it might be. */

  if (!func_start) return 1;

/* We need to call the target-specific prologue skipping functions with the
   function's start address because PC may be pointing at an instruction that
   could be mistakenly considered part of the prologue.  */

  SKIP_PROLOGUE (func_start);

  return pc < func_start;
}


void
_initialize_symtab ()
{
  add_info ("variables", variables_info,
	    "All global and static variable names, or those matching REGEXP.");
  add_info ("functions", functions_info,
	    "All function names, or those matching REGEXP.");

  /* FIXME:  This command has at least the following problems:
     1.  It prints builtin types (in a very strange and confusing fashion).
     2.  It doesn't print right, e.g. with
         typedef struct foo *FOO
	 type_print prints "FOO" when we want to make it (in this situation)
	 print "struct foo *".
     I also think "ptype" or "whatis" is more likely to be useful (but if
     there is much disagreement "info types" can be fixed).  */
  add_info ("types", types_info,
	    "All type names, or those matching REGEXP.");

#if 0
  add_info ("methods", methods_info,
	    "All method names, or those matching REGEXP::REGEXP.\n\
If the class qualifier is omitted, it is assumed to be the current scope.\n\
If the first REGEXP is omitted, then all methods matching the second REGEXP\n\
are listed.");
#endif
  add_info ("sources", sources_info,
	    "Source files in the program.");

  add_com ("rbreak", no_class, rbreak_command,
	    "Set a breakpoint for all functions matching REGEXP.");

  /* Initialize the one built-in type that isn't language dependent... */
  builtin_type_error = init_type (TYPE_CODE_ERROR, 0, 0,
				  "<unknown type>", (struct objfile *) NULL);
}
