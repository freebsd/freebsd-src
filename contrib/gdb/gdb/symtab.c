/* Symbol table lookup for the GNU debugger, GDB.

   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
   1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

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
#include "gdb_regex.h"
#include "expression.h"
#include "language.h"
#include "demangle.h"
#include "inferior.h"
#include "linespec.h"
#include "filenames.h"		/* for FILENAME_CMP */

#include "obstack.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>
#include "cp-abi.h"

/* Prototype for one function in parser-defs.h,
   instead of including that entire file. */

extern char *find_template_name_end (char *);

/* Prototypes for local functions */

static void completion_list_add_name (char *, char *, int, char *, char *);

static void rbreak_command (char *, int);

static void types_info (char *, int);

static void functions_info (char *, int);

static void variables_info (char *, int);

static void sources_info (char *, int);

static void output_source_filename (char *, int *);

static int find_line_common (struct linetable *, int, int *);

/* This one is used by linespec.c */

char *operator_chars (char *p, char **end);

static struct partial_symbol *lookup_partial_symbol (struct partial_symtab *,
						     const char *, int,
						     namespace_enum);

static struct symbol *lookup_symbol_aux (const char *name, const
					 struct block *block, const
					 namespace_enum namespace, int
					 *is_a_field_of_this, struct
					 symtab **symtab);


static struct symbol *find_active_alias (struct symbol *sym, CORE_ADDR addr);

/* This flag is used in hppa-tdep.c, and set in hp-symtab-read.c */
/* Signals the presence of objects compiled by HP compilers */
int hp_som_som_object_present = 0;

static void fixup_section (struct general_symbol_info *, struct objfile *);

static int file_matches (char *, char **, int);

static void print_symbol_info (namespace_enum,
			       struct symtab *, struct symbol *, int, char *);

static void print_msymbol_info (struct minimal_symbol *);

static void symtab_symbol_info (char *, namespace_enum, int);

static void overload_list_add_symbol (struct symbol *sym, char *oload_name);

void _initialize_symtab (void);

/* */

/* The single non-language-specific builtin type */
struct type *builtin_type_error;

/* Block in which the most recently searched-for symbol was found.
   Might be better to make this a parameter to lookup_symbol and 
   value_of_this. */

const struct block *block_found;

/* While the C++ support is still in flux, issue a possibly helpful hint on
   using the new command completion feature on single quoted demangled C++
   symbols.  Remove when loose ends are cleaned up.   FIXME -fnf */

static void
cplusplus_hint (char *name)
{
  while (*name == '\'')
    name++;
  printf_filtered ("Hint: try '%s<TAB> or '%s<ESC-?>\n", name, name);
  printf_filtered ("(Note leading single quote.)\n");
}

/* Check for a symtab of a specific name; first in symtabs, then in
   psymtabs.  *If* there is no '/' in the name, a match after a '/'
   in the symtab filename will also work.  */

struct symtab *
lookup_symtab (const char *name)
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  char *real_path = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    real_path = gdb_realpath (name);

got_symtab:

  /* First, search for an exact match */

  ALL_SYMTABS (objfile, s)
  {
    if (FILENAME_CMP (name, s->filename) == 0)
      {
	xfree (real_path);
	return s;
      }
    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    if (real_path != NULL)
      {
	char *rp = symtab_to_filename (s);
	if (FILENAME_CMP (real_path, rp) == 0)
	  {
	    xfree (real_path);
	    return s;
	  }
      }
  }

  xfree (real_path);

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (lbasename (name) == name)
    ALL_SYMTABS (objfile, s)
    {
      if (FILENAME_CMP (lbasename (s->filename), name) == 0)
	return s;
    }

  /* Same search rules as above apply here, but now we look thru the
     psymtabs.  */

  ps = lookup_partial_symtab (name);
  if (!ps)
    return (NULL);

  if (ps->readin)
    error ("Internal: readin %s pst for `%s' found when no symtab found.",
	   ps->filename, name);

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

/* Lookup the partial symbol table of a source file named NAME.
   *If* there is no '/' in the name, a match after a '/'
   in the psymtab filename will also work.  */

struct partial_symtab *
lookup_partial_symtab (const char *name)
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;
  char *real_path = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    real_path = gdb_realpath (name);

  ALL_PSYMTABS (objfile, pst)
  {
    if (FILENAME_CMP (name, pst->filename) == 0)
      {
	xfree (real_path);
	return (pst);
      }
    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    if (real_path != NULL)
      {
	if (pst->fullname == NULL)
	  source_full_path_of (pst->filename, &pst->fullname);
	if (pst->fullname != NULL
	    && FILENAME_CMP (real_path, pst->fullname) == 0)
	  {
	    xfree (real_path);
	    return pst;
	  }
      }
  }

  xfree (real_path);

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (lbasename (name) == name)
    ALL_PSYMTABS (objfile, pst)
    {
      if (FILENAME_CMP (lbasename (pst->filename), name) == 0)
	return (pst);
    }

  return (NULL);
}

/* Mangle a GDB method stub type.  This actually reassembles the pieces of the
   full method name, which consist of the class name (from T), the unadorned
   method name from METHOD_ID, and the signature for the specific overload,
   specified by SIGNATURE_ID.  Note that this function is g++ specific. */

char *
gdb_mangle_name (struct type *type, int method_id, int signature_id)
{
  int mangled_name_len;
  char *mangled_name;
  struct fn_field *f = TYPE_FN_FIELDLIST1 (type, method_id);
  struct fn_field *method = &f[signature_id];
  char *field_name = TYPE_FN_FIELDLIST_NAME (type, method_id);
  char *physname = TYPE_FN_FIELD_PHYSNAME (f, signature_id);
  char *newname = type_name_no_tag (type);

  /* Does the form of physname indicate that it is the full mangled name
     of a constructor (not just the args)?  */
  int is_full_physname_constructor;

  int is_constructor;
  int is_destructor = is_destructor_name (physname);
  /* Need a new type prefix.  */
  char *const_prefix = method->is_const ? "C" : "";
  char *volatile_prefix = method->is_volatile ? "V" : "";
  char buf[20];
  int len = (newname == NULL ? 0 : strlen (newname));

  /* Nothing to do if physname already contains a fully mangled v3 abi name
     or an operator name.  */
  if ((physname[0] == '_' && physname[1] == 'Z')
      || is_operator_name (field_name))
    return xstrdup (physname);

  is_full_physname_constructor = is_constructor_name (physname);

  is_constructor =
    is_full_physname_constructor || (newname && STREQ (field_name, newname));

  if (!is_destructor)
    is_destructor = (strncmp (physname, "__dt", 4) == 0);

  if (is_destructor || is_full_physname_constructor)
    {
      mangled_name = (char *) xmalloc (strlen (physname) + 1);
      strcpy (mangled_name, physname);
      return mangled_name;
    }

  if (len == 0)
    {
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
    }
  else if (physname[0] == 't' || physname[0] == 'Q')
    {
      /* The physname for template and qualified methods already includes
         the class name.  */
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
      newname = NULL;
      len = 0;
    }
  else
    {
      sprintf (buf, "__%s%s%d", const_prefix, volatile_prefix, len);
    }
  mangled_name_len = ((is_constructor ? 0 : strlen (field_name))
		      + strlen (buf) + len + strlen (physname) + 1);

    {
      mangled_name = (char *) xmalloc (mangled_name_len);
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



/* Find which partial symtab on contains PC and SECTION.  Return 0 if none.  */

struct partial_symtab *
find_pc_sect_psymtab (CORE_ADDR pc, asection *section)
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;
  struct minimal_symbol *msymbol;

  /* If we know that this is not a text address, return failure.  This is
     necessary because we loop based on texthigh and textlow, which do
     not include the data ranges.  */
  msymbol = lookup_minimal_symbol_by_pc_section (pc, section);
  if (msymbol
      && (msymbol->type == mst_data
	  || msymbol->type == mst_bss
	  || msymbol->type == mst_abs
	  || msymbol->type == mst_file_data
	  || msymbol->type == mst_file_bss))
    return NULL;

  ALL_PSYMTABS (objfile, pst)
  {
    if (pc >= pst->textlow && pc < pst->texthigh)
      {
	struct partial_symtab *tpst;

	/* An objfile that has its functions reordered might have
	   many partial symbol tables containing the PC, but
	   we want the partial symbol table that contains the
	   function containing the PC.  */
	if (!(objfile->flags & OBJF_REORDERED) &&
	    section == 0)	/* can't validate section this way */
	  return (pst);

	if (msymbol == NULL)
	  return (pst);

	for (tpst = pst; tpst != NULL; tpst = tpst->next)
	  {
	    if (pc >= tpst->textlow && pc < tpst->texthigh)
	      {
		struct partial_symbol *p;

		p = find_pc_sect_psymbol (tpst, pc, section);
		if (p != NULL
		    && SYMBOL_VALUE_ADDRESS (p)
		    == SYMBOL_VALUE_ADDRESS (msymbol))
		  return (tpst);
	      }
	  }
	return (pst);
      }
  }
  return (NULL);
}

/* Find which partial symtab contains PC.  Return 0 if none. 
   Backward compatibility, no section */

struct partial_symtab *
find_pc_psymtab (CORE_ADDR pc)
{
  return find_pc_sect_psymtab (pc, find_pc_mapped_section (pc));
}

/* Find which partial symbol within a psymtab matches PC and SECTION.  
   Return 0 if none.  Check all psymtabs if PSYMTAB is 0.  */

struct partial_symbol *
find_pc_sect_psymbol (struct partial_symtab *psymtab, CORE_ADDR pc,
		      asection *section)
{
  struct partial_symbol *best = NULL, *p, **pp;
  CORE_ADDR best_pc;

  if (!psymtab)
    psymtab = find_pc_sect_psymtab (pc, section);
  if (!psymtab)
    return 0;

  /* Cope with programs that start at address 0 */
  best_pc = (psymtab->textlow != 0) ? psymtab->textlow - 1 : 0;

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
	  && (SYMBOL_VALUE_ADDRESS (p) > best_pc
	      || (psymtab->textlow == 0
		  && best_pc == 0 && SYMBOL_VALUE_ADDRESS (p) == 0)))
	{
	  if (section)		/* match on a specific section */
	    {
	      fixup_psymbol_section (p, psymtab->objfile);
	      if (SYMBOL_BFD_SECTION (p) != section)
		continue;
	    }
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
	  && (SYMBOL_VALUE_ADDRESS (p) > best_pc
	      || (psymtab->textlow == 0
		  && best_pc == 0 && SYMBOL_VALUE_ADDRESS (p) == 0)))
	{
	  if (section)		/* match on a specific section */
	    {
	      fixup_psymbol_section (p, psymtab->objfile);
	      if (SYMBOL_BFD_SECTION (p) != section)
		continue;
	    }
	  best_pc = SYMBOL_VALUE_ADDRESS (p);
	  best = p;
	}
    }

  return best;
}

/* Find which partial symbol within a psymtab matches PC.  Return 0 if none.  
   Check all psymtabs if PSYMTAB is 0.  Backwards compatibility, no section. */

struct partial_symbol *
find_pc_psymbol (struct partial_symtab *psymtab, CORE_ADDR pc)
{
  return find_pc_sect_psymbol (psymtab, pc, find_pc_mapped_section (pc));
}

/* Debug symbols usually don't have section information.  We need to dig that
   out of the minimal symbols and stash that in the debug symbol.  */

static void
fixup_section (struct general_symbol_info *ginfo, struct objfile *objfile)
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (ginfo->name, NULL, objfile);

  if (msym)
    {
      ginfo->bfd_section = SYMBOL_BFD_SECTION (msym);
      ginfo->section = SYMBOL_SECTION (msym);
    }
}

struct symbol *
fixup_symbol_section (struct symbol *sym, struct objfile *objfile)
{
  if (!sym)
    return NULL;

  if (SYMBOL_BFD_SECTION (sym))
    return sym;

  fixup_section (&sym->ginfo, objfile);

  return sym;
}

struct partial_symbol *
fixup_psymbol_section (struct partial_symbol *psym, struct objfile *objfile)
{
  if (!psym)
    return NULL;

  if (SYMBOL_BFD_SECTION (psym))
    return psym;

  fixup_section (&psym->ginfo, objfile);

  return psym;
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
lookup_symbol (const char *name, const struct block *block,
	       const namespace_enum namespace, int *is_a_field_of_this,
	       struct symtab **symtab)
{
  char *modified_name = NULL;
  char *modified_name2 = NULL;
  int needtofreename = 0;
  struct symbol *returnval;

  if (case_sensitivity == case_sensitive_off)
    {
      char *copy;
      int len, i;

      len = strlen (name);
      copy = (char *) alloca (len + 1);
      for (i= 0; i < len; i++)
        copy[i] = tolower (name[i]);
      copy[len] = 0;
      modified_name = copy;
    }
  else 
      modified_name = (char *) name;

  /* If we are using C++ language, demangle the name before doing a lookup, so
     we can always binary search. */
  if (current_language->la_language == language_cplus)
    {
      modified_name2 = cplus_demangle (modified_name, DMGL_ANSI | DMGL_PARAMS);
      if (modified_name2)
	{
	  modified_name = modified_name2;
	  needtofreename = 1;
	}
    }

  returnval = lookup_symbol_aux (modified_name, block, namespace,
				 is_a_field_of_this, symtab);
  if (needtofreename)
    xfree (modified_name2);

  return returnval;	 
}

static struct symbol *
lookup_symbol_aux (const char *name, const struct block *block,
	       const namespace_enum namespace, int *is_a_field_of_this,
	       struct symtab **symtab)
{
  register struct symbol *sym;
  register struct symtab *s = NULL;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  register struct objfile *objfile = NULL;
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

	  return fixup_symbol_section (sym, objfile);
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
  /* Note (RT): The following never-executed code looks unnecessary to me also.
   * If we change the code to use the original (passed-in)
   * value of 'block', we could cause it to execute, but then what
   * would it do? The STATIC_BLOCK of the symtab containing the passed-in
   * 'block' was already searched by the above code. And the STATIC_BLOCK's
   * of *other* symtabs (those files not containing 'block' lexically)
   * should not contain 'block' address-wise. So we wouldn't expect this
   * code to find any 'sym''s that were not found above. I vote for 
   * deleting the following paragraph of code.
   */
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
		return fixup_symbol_section (sym, objfile);
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
	  return NULL;
	}
    }

  /* Now search all global blocks.  Do the symtab's first, then
     check the psymtab's. If a psymtab indicates the existence
     of the desired name as a global, then do psymtab-to-symtab
     conversion on the fly and return the found symbol. */

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
	return fixup_symbol_section (sym, objfile);
      }
  }

#ifndef HPUXHPPA

  /* Check for the possibility of the symbol being a function or
     a mangled variable that is stored in one of the minimal symbol tables.
     Eventually, all global symbols might be resolved in this way.  */

  if (namespace == VAR_NAMESPACE)
    {
      msymbol = lookup_minimal_symbol (name, NULL, NULL);
      if (msymbol != NULL)
	{
	  s = find_pc_sect_symtab (SYMBOL_VALUE_ADDRESS (msymbol),
				   SYMBOL_BFD_SECTION (msymbol));
	  if (s != NULL)
	    {
	      /* This is a function which has a symtab for its address.  */
	      bv = BLOCKVECTOR (s);
	      block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	      sym = lookup_block_symbol (block, SYMBOL_NAME (msymbol),
					 namespace);
	      /* We kept static functions in minimal symbol table as well as
	         in static scope. We want to find them in the symbol table. */
	      if (!sym)
		{
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
	      return fixup_symbol_section (sym, objfile);
	    }
	  else if (MSYMBOL_TYPE (msymbol) != mst_text
		   && MSYMBOL_TYPE (msymbol) != mst_file_text
		   && !STREQ (name, SYMBOL_NAME (msymbol)))
	    {
	      /* This is a mangled variable, look it up by its
	         mangled name.  */
	      return lookup_symbol_aux (SYMBOL_NAME (msymbol), block,
					namespace, is_a_field_of_this, symtab);
	    }
	  /* There are no debug symbols for this file, or we are looking
	     for an unmangled variable.
	     Try to find a matching static symbol below. */
	}
    }

#endif

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, 1, namespace))
      {
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	sym = lookup_block_symbol (block, name, namespace);
	if (!sym)
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the statics even though the psymtab
	     * claimed the symbol was global. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	    sym = lookup_block_symbol (block, name, namespace);
	    if (!sym)
	      error ("Internal: global symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		     name, ps->filename, name, name);
	  }
	if (symtab != NULL)
	  *symtab = s;
	return fixup_symbol_section (sym, objfile);
      }
  }

  /* Now search all static file-level symbols.
     Not strictly correct, but more useful than an error.
     Do the symtabs first, then check the psymtabs.
     If a psymtab indicates the existence
     of the desired name as a file-level static, then do psymtab-to-symtab
     conversion on the fly and return the found symbol. */

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
	return fixup_symbol_section (sym, objfile);
      }
  }

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, 0, namespace))
      {
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	sym = lookup_block_symbol (block, name, namespace);
	if (!sym)
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the globals even though the psymtab
	     * claimed the symbol was static. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	    sym = lookup_block_symbol (block, name, namespace);
	    if (!sym)
	      error ("Internal: static symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		     name, ps->filename, name, name);
	  }
	if (symtab != NULL)
	  *symtab = s;
	return fixup_symbol_section (sym, objfile);
      }
  }

#ifdef HPUXHPPA

  /* Check for the possibility of the symbol being a function or
     a global variable that is stored in one of the minimal symbol tables.
     The "minimal symbol table" is built from linker-supplied info.

     RT: I moved this check to last, after the complete search of
     the global (p)symtab's and static (p)symtab's. For HP-generated
     symbol tables, this check was causing a premature exit from
     lookup_symbol with NULL return, and thus messing up symbol lookups
     of things like "c::f". It seems to me a check of the minimal
     symbol table ought to be a last resort in any case. I'm vaguely
     worried about the comment below which talks about FORTRAN routines "foo_"
     though... is it saying we need to do the "minsym" check before
     the static check in this case? 
   */

  if (namespace == VAR_NAMESPACE)
    {
      msymbol = lookup_minimal_symbol (name, NULL, NULL);
      if (msymbol != NULL)
	{
	  /* OK, we found a minimal symbol in spite of not
	   * finding any symbol. There are various possible
	   * explanations for this. One possibility is the symbol
	   * exists in code not compiled -g. Another possibility
	   * is that the 'psymtab' isn't doing its job.
	   * A third possibility, related to #2, is that we were confused 
	   * by name-mangling. For instance, maybe the psymtab isn't
	   * doing its job because it only know about demangled
	   * names, but we were given a mangled name...
	   */

	  /* We first use the address in the msymbol to try to
	   * locate the appropriate symtab. Note that find_pc_symtab()
	   * has a side-effect of doing psymtab-to-symtab expansion,
	   * for the found symtab.
	   */
	  s = find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol));
	  if (s != NULL)
	    {
	      bv = BLOCKVECTOR (s);
	      block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	      sym = lookup_block_symbol (block, SYMBOL_NAME (msymbol),
					 namespace);
	      /* We kept static functions in minimal symbol table as well as
	         in static scope. We want to find them in the symbol table. */
	      if (!sym)
		{
		  block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
		  sym = lookup_block_symbol (block, SYMBOL_NAME (msymbol),
					     namespace);
		}
	      /* If we found one, return it */
	      if (sym)
		{
		  if (symtab != NULL)
		    *symtab = s;
		  return sym;
		}

	      /* If we get here with sym == 0, the symbol was 
	         found in the minimal symbol table
	         but not in the symtab.
	         Fall through and return 0 to use the msymbol 
	         definition of "foo_".
	         (Note that outer code generally follows up a call
	         to this routine with a call to lookup_minimal_symbol(),
	         so a 0 return means we'll just flow into that other routine).

	         This happens for Fortran  "foo_" symbols,
	         which are "foo" in the symtab.

	         This can also happen if "asm" is used to make a
	         regular symbol but not a debugging symbol, e.g.
	         asm(".globl _main");
	         asm("_main:");
	       */
	    }

	  /* If the lookup-by-address fails, try repeating the
	   * entire lookup process with the symbol name from
	   * the msymbol (if different from the original symbol name).
	   */
	  else if (MSYMBOL_TYPE (msymbol) != mst_text
		   && MSYMBOL_TYPE (msymbol) != mst_file_text
		   && !STREQ (name, SYMBOL_NAME (msymbol)))
	    {
	      return lookup_symbol_aux (SYMBOL_NAME (msymbol), block,
					namespace, is_a_field_of_this, symtab);
	    }
	}
    }

#endif

  if (symtab != NULL)
    *symtab = NULL;
  return 0;
}
								
/* Look, in partial_symtab PST, for symbol NAME.  Check the global
   symbols if GLOBAL, the static symbols if not */

static struct partial_symbol *
lookup_partial_symbol (struct partial_symtab *pst, const char *name, int global,
		       namespace_enum namespace)
{
  struct partial_symbol *temp;
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
	   pst->objfile->static_psymbols.list + pst->statics_offset);
  
  if (global)			/* This means we can use a binary search. */
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
	    internal_error (__FILE__, __LINE__, "failed internal consistency check");
	  if (!do_linear_search
	      && (SYMBOL_LANGUAGE (*center) == language_java))
	    {
	      do_linear_search = 1;
	    }
	  if (strcmp (SYMBOL_SOURCE_NAME (*center), name) >= 0)
	    {
	      top = center;
	    }
	  else
	    {
	      bottom = center + 1;
	    }
	}
      if (!(top == bottom))
	internal_error (__FILE__, __LINE__, "failed internal consistency check");

      /* djb - 2000-06-03 - Use SYMBOL_MATCHES_NAME, not a strcmp, so
	 we don't have to force a linear search on C++. Probably holds true
	 for JAVA as well, no way to check.*/
      while (SYMBOL_MATCHES_NAME (*top,name))
	{
	  if (SYMBOL_NAMESPACE (*top) == namespace)
	    {
		  return (*top);
	    }
	  top++;
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

/* Look up a type named NAME in the struct_namespace.  The type returned
   must not be opaque -- i.e., must have at least one field defined

   This code was modelled on lookup_symbol -- the parts not relevant to looking
   up types were just left out.  In particular it's assumed here that types
   are available in struct_namespace and only at file-static or global blocks. */


struct type *
lookup_transparent_type (const char *name)
{
  register struct symbol *sym;
  register struct symtab *s = NULL;
  register struct partial_symtab *ps;
  struct blockvector *bv;
  register struct objfile *objfile;
  register struct block *block;

  /* Now search all the global symbols.  Do the symtab's first, then
     check the psymtab's. If a psymtab indicates the existence
     of the desired name as a global, then do psymtab-to-symtab
     conversion on the fly and return the found symbol.  */

  ALL_SYMTABS (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
    sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
    if (sym && !TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
      {
	return SYMBOL_TYPE (sym);
      }
  }

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, 1, STRUCT_NAMESPACE))
      {
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
	if (!sym)
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the statics even though the psymtab
	     * claimed the symbol was global. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	    sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
	    if (!sym)
	      error ("Internal: global symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		     name, ps->filename, name, name);
	  }
	if (!TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
	  return SYMBOL_TYPE (sym);
      }
  }

  /* Now search the static file-level symbols.
     Not strictly correct, but more useful than an error.
     Do the symtab's first, then
     check the psymtab's. If a psymtab indicates the existence
     of the desired name as a file-level static, then do psymtab-to-symtab
     conversion on the fly and return the found symbol.
   */

  ALL_SYMTABS (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
    sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
    if (sym && !TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
      {
	return SYMBOL_TYPE (sym);
      }
  }

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, 0, STRUCT_NAMESPACE))
      {
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
	if (!sym)
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the globals even though the psymtab
	     * claimed the symbol was static. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	    sym = lookup_block_symbol (block, name, STRUCT_NAMESPACE);
	    if (!sym)
	      error ("Internal: static symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		     name, ps->filename, name, name);
	  }
	if (!TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
	  return SYMBOL_TYPE (sym);
      }
  }
  return (struct type *) 0;
}


/* Find the psymtab containing main(). */
/* FIXME:  What about languages without main() or specially linked
   executables that have no main() ? */

struct partial_symtab *
find_main_psymtab (void)
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;

  ALL_PSYMTABS (objfile, pst)
  {
    if (lookup_partial_symbol (pst, main_name (), 1, VAR_NAMESPACE))
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
lookup_block_symbol (register const struct block *block, const char *name,
		     const namespace_enum namespace)
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
         do so */

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
	  if (!do_linear_search && (SYMBOL_LANGUAGE (sym) == language_java))
	    {
	      do_linear_search = 1;
	    }
	  if (SYMBOL_SOURCE_NAME (sym)[0] < name[0])
	    {
	      bot = inc;
	    }
	  else if (SYMBOL_SOURCE_NAME (sym)[0] > name[0])
	    {
	      top = inc;
	    }
	  else if (strcmp (SYMBOL_SOURCE_NAME (sym), name) < 0)
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
	  if (SYMBOL_NAMESPACE (sym) == namespace &&
	      SYMBOL_MATCHES_NAME (sym, name))
	    {
	      return sym;
	    }
          if (SYMBOL_SOURCE_NAME (sym)[0] > name[0])
            {
              break;
            }
	  bot++;
	}
    }

  /* Here if block isn't sorted, or we fail to find a match during the
     binary search above.  If during the binary search above, we find a
     symbol which is a Java symbol, then we have re-enabled the linear
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
	      /* If SYM has aliases, then use any alias that is active
	         at the current PC.  If no alias is active at the current
	         PC, then use the main symbol.

	         ?!? Is checking the current pc correct?  Is this routine
	         ever called to look up a symbol from another context?

		 FIXME: No, it's not correct.  If someone sets a
		 conditional breakpoint at an address, then the
		 breakpoint's `struct expression' should refer to the
		 `struct symbol' appropriate for the breakpoint's
		 address, which may not be the PC.

		 Even if it were never called from another context,
		 it's totally bizarre for lookup_symbol's behavior to
		 depend on the value of the inferior's current PC.  We
		 should pass in the appropriate PC as well as the
		 block.  The interface to lookup_symbol should change
		 to require the caller to provide a PC.  */

	      if (SYMBOL_ALIASES (sym))
		sym = find_active_alias (sym, read_pc ());

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

/* Given a main symbol SYM and ADDR, search through the alias
   list to determine if an alias is active at ADDR and return
   the active alias.

   If no alias is active, then return SYM.  */

static struct symbol *
find_active_alias (struct symbol *sym, CORE_ADDR addr)
{
  struct range_list *r;
  struct alias_list *aliases;

  /* If we have aliases, check them first.  */
  aliases = SYMBOL_ALIASES (sym);

  while (aliases)
    {
      if (!SYMBOL_RANGES (aliases->sym))
	return aliases->sym;
      for (r = SYMBOL_RANGES (aliases->sym); r; r = r->next)
	{
	  if (r->start <= addr && r->end > addr)
	    return aliases->sym;
	}
      aliases = aliases->next;
    }

  /* Nothing found, return the main symbol.  */
  return sym;
}


/* Return the symbol for the function which contains a specified
   lexical block, described by a struct block BL.  */

struct symbol *
block_function (struct block *bl)
{
  while (BLOCK_FUNCTION (bl) == 0 && BLOCK_SUPERBLOCK (bl) != 0)
    bl = BLOCK_SUPERBLOCK (bl);

  return BLOCK_FUNCTION (bl);
}

/* Find the symtab associated with PC and SECTION.  Look through the
   psymtabs and read in another symtab if necessary. */

struct symtab *
find_pc_sect_symtab (CORE_ADDR pc, asection *section)
{
  register struct block *b;
  struct blockvector *bv;
  register struct symtab *s = NULL;
  register struct symtab *best_s = NULL;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  CORE_ADDR distance = 0;
  struct minimal_symbol *msymbol;

  /* If we know that this is not a text address, return failure.  This is
     necessary because we loop based on the block's high and low code
     addresses, which do not include the data ranges, and because
     we call find_pc_sect_psymtab which has a similar restriction based
     on the partial_symtab's texthigh and textlow.  */
  msymbol = lookup_minimal_symbol_by_pc_section (pc, section);
  if (msymbol
      && (msymbol->type == mst_data
	  || msymbol->type == mst_bss
	  || msymbol->type == mst_abs
	  || msymbol->type == mst_file_data
	  || msymbol->type == mst_file_bss))
    return NULL;

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
	/* In order to better support objfiles that contain both
	   stabs and coff debugging info, we continue on if a psymtab
	   can't be found. */
	if ((objfile->flags & OBJF_REORDERED) && objfile->psymtabs)
	  {
	    ps = find_pc_sect_psymtab (pc, section);
	    if (ps)
	      return PSYMTAB_TO_SYMTAB (ps);
	  }
	if (section != 0)
	  {
	    int i;

	    for (i = 0; i < b->nsyms; i++)
	      {
		fixup_symbol_section (b->sym[i], objfile);
		if (section == SYMBOL_BFD_SECTION (b->sym[i]))
		  break;
	      }
	    if (i >= b->nsyms)
	      continue;		/* no symbol in this symtab matches section */
	  }
	distance = BLOCK_END (b) - BLOCK_START (b);
	best_s = s;
      }
  }

  if (best_s != NULL)
    return (best_s);

  s = NULL;
  ps = find_pc_sect_psymtab (pc, section);
  if (ps)
    {
      if (ps->readin)
	/* Might want to error() here (in case symtab is corrupt and
	   will cause a core dump), but maybe we can successfully
	   continue, so let's not.  */
	warning ("\
(Internal error: pc 0x%s in read in psymtab, but not in symtab.)\n",
		 paddr_nz (pc));
      s = PSYMTAB_TO_SYMTAB (ps);
    }
  return (s);
}

/* Find the symtab associated with PC.  Look through the psymtabs and
   read in another symtab if necessary.  Backward compatibility, no section */

struct symtab *
find_pc_symtab (CORE_ADDR pc)
{
  return find_pc_sect_symtab (pc, find_pc_mapped_section (pc));
}


#if 0

/* Find the closest symbol value (of any sort -- function or variable)
   for a given address value.  Slow but complete.  (currently unused,
   mainly because it is too slow.  We could fix it if each symtab and
   psymtab had contained in it the addresses ranges of each of its
   sections, which also would be required to make things like "info
   line *0x2345" cause psymtabs to be converted to symtabs).  */

struct symbol *
find_addr_symbol (CORE_ADDR addr, struct symtab **symtabp, CORE_ADDR *symaddrp)
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

	      case LOC_INDIRECT:
		sym_addr = SYMBOL_VALUE_ADDRESS (sym);
		/* An indirect symbol really lives at *sym_addr,
		 * so an indirection needs to be done.
		 * However, I am leaving this commented out because it's
		 * expensive, and it's possible that symbolization
		 * could be done without an active process (in
		 * case this read_memory will fail). RT
		 sym_addr = read_memory_unsigned_integer
		 (sym_addr, TARGET_PTR_BIT / TARGET_CHAR_BIT);
		 */
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

/* Find the source file and line number for a given PC value and SECTION.
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
find_pc_sect_line (CORE_ADDR pc, struct sec *section, int notcurrent)
{
  struct symtab *s;
  register struct linetable *l;
  register int len;
  register int i;
  register struct linetable_entry *item;
  struct symtab_and_line val;
  struct blockvector *bv;
  struct minimal_symbol *msymbol;
  struct minimal_symbol *mfunsym;

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

  INIT_SAL (&val);		/* initialize to zeroes */

  /* It's tempting to assume that, if we can't find debugging info for
     any function enclosing PC, that we shouldn't search for line
     number info, either.  However, GAS can emit line number info for
     assembly files --- very helpful when debugging hand-written
     assembly code.  In such a case, we'd have no debug info for the
     function, but we would have line info.  */

  if (notcurrent)
    pc -= 1;

  /* elz: added this because this function returned the wrong
     information if the pc belongs to a stub (import/export)
     to call a shlib function. This stub would be anywhere between
     two functions in the target, and the line info was erroneously 
     taken to be the one of the line before the pc. 
   */
  /* RT: Further explanation:

   * We have stubs (trampolines) inserted between procedures.
   *
   * Example: "shr1" exists in a shared library, and a "shr1" stub also
   * exists in the main image.
   *
   * In the minimal symbol table, we have a bunch of symbols
   * sorted by start address. The stubs are marked as "trampoline",
   * the others appear as text. E.g.:
   *
   *  Minimal symbol table for main image 
   *     main:  code for main (text symbol)
   *     shr1: stub  (trampoline symbol)
   *     foo:   code for foo (text symbol)
   *     ...
   *  Minimal symbol table for "shr1" image:
   *     ...
   *     shr1: code for shr1 (text symbol)
   *     ...
   *
   * So the code below is trying to detect if we are in the stub
   * ("shr1" stub), and if so, find the real code ("shr1" trampoline),
   * and if found,  do the symbolization from the real-code address
   * rather than the stub address.
   *
   * Assumptions being made about the minimal symbol table:
   *   1. lookup_minimal_symbol_by_pc() will return a trampoline only
   *      if we're really in the trampoline. If we're beyond it (say
   *      we're in "foo" in the above example), it'll have a closer 
   *      symbol (the "foo" text symbol for example) and will not
   *      return the trampoline.
   *   2. lookup_minimal_symbol_text() will find a real text symbol
   *      corresponding to the trampoline, and whose address will
   *      be different than the trampoline address. I put in a sanity
   *      check for the address being the same, to avoid an
   *      infinite recursion.
   */
  msymbol = lookup_minimal_symbol_by_pc (pc);
  if (msymbol != NULL)
    if (MSYMBOL_TYPE (msymbol) == mst_solib_trampoline)
      {
	mfunsym = lookup_minimal_symbol_text (SYMBOL_NAME (msymbol), NULL, NULL);
	if (mfunsym == NULL)
	  /* I eliminated this warning since it is coming out
	   * in the following situation:
	   * gdb shmain // test program with shared libraries
	   * (gdb) break shr1  // function in shared lib
	   * Warning: In stub for ...
	   * In the above situation, the shared lib is not loaded yet, 
	   * so of course we can't find the real func/line info,
	   * but the "break" still works, and the warning is annoying.
	   * So I commented out the warning. RT */
	  /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_NAME(msymbol)) */ ;
	/* fall through */
	else if (SYMBOL_VALUE (mfunsym) == SYMBOL_VALUE (msymbol))
	  /* Avoid infinite recursion */
	  /* See above comment about why warning is commented out */
	  /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_NAME(msymbol)) */ ;
	/* fall through */
	else
	  return find_pc_line (SYMBOL_VALUE (mfunsym), 0);
      }


  s = find_pc_sect_symtab (pc, section);
  if (!s)
    {
      /* if no symbol information, return previous pc */
      if (notcurrent)
	pc++;
      val.pc = pc;
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
	  /* Leave prev pointing to the linetable entry for the last line
	     that started at or before PC.  */
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

	  /* Discard BEST_END if it's before the PC of the current BEST.  */
	  if (best_end <= best->pc)
	    best_end = 0;
	}

      /* If another line (denoted by ITEM) is in the linetable and its
         PC is after BEST's PC, but before the current BEST_END, then
	 use ITEM's PC as the new best_end.  */
      if (best && i < len && item->pc > best->pc
          && (best_end == 0 || best_end > item->pc))
	best_end = item->pc;
    }

  if (!best_symtab)
    {
      if (!alt_symtab)
	{			/* If we didn't find any line # info, just
				   return zeros.  */
	  val.pc = pc;
	}
      else
	{
	  val.symtab = alt_symtab;
	  val.line = alt->line - 1;

	  /* Don't return line 0, that means that we didn't find the line.  */
	  if (val.line == 0)
	    ++val.line;

	  val.pc = BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
	  val.end = alt->pc;
	}
    }
  else if (best->line == 0)
    {
      /* If our best fit is in a range of PC's for which no line
	 number info is available (line number is zero) then we didn't
	 find any valid line information. */
      val.pc = pc;
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
  val.section = section;
  return val;
}

/* Backward compatibility (no section) */

struct symtab_and_line
find_pc_line (CORE_ADDR pc, int notcurrent)
{
  asection *section;

  section = find_pc_overlay (pc);
  if (pc_in_unmapped_range (pc, section))
    pc = overlay_mapped_address (pc, section);
  return find_pc_sect_line (pc, section, notcurrent);
}

/* Find line number LINE in any symtab whose name is the same as
   SYMTAB.

   If found, return the symtab that contains the linetable in which it was
   found, set *INDEX to the index in the linetable of the best entry
   found, and set *EXACT_MATCH nonzero if the value returned is an
   exact match.

   If not found, return NULL.  */

struct symtab *
find_line_symtab (struct symtab *symtab, int line, int *index, int *exact_match)
{
  int exact;

  /* BEST_INDEX and BEST_LINETABLE identify the smallest linenumber > LINE
     so far seen.  */

  int best_index;
  struct linetable *best_linetable;
  struct symtab *best_symtab;

  /* First try looking it up in the given symtab.  */
  best_linetable = LINETABLE (symtab);
  best_symtab = symtab;
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
		best_symtab = s;
		goto done;
	      }
	    if (best == 0 || l->item[ind].line < best)
	      {
		best = l->item[ind].line;
		best_index = ind;
		best_linetable = l;
		best_symtab = s;
	      }
	  }
      }
    }
done:
  if (best_index < 0)
    return NULL;

  if (index)
    *index = best_index;
  if (exact_match)
    *exact_match = exact;

  return best_symtab;
}

/* Set the PC value for a given source file and line number and return true.
   Returns zero for invalid line number (and sets the PC to 0).
   The source file is specified with a struct symtab.  */

int
find_line_pc (struct symtab *symtab, int line, CORE_ADDR *pc)
{
  struct linetable *l;
  int ind;

  *pc = 0;
  if (symtab == 0)
    return 0;

  symtab = find_line_symtab (symtab, line, &ind, NULL);
  if (symtab != NULL)
    {
      l = LINETABLE (symtab);
      *pc = l->item[ind].pc;
      return 1;
    }
  else
    return 0;
}

/* Find the range of pc values in a line.
   Store the starting pc of the line into *STARTPTR
   and the ending pc (start of next line) into *ENDPTR.
   Returns 1 to indicate success.
   Returns 0 if could not find the specified line.  */

int
find_line_pc_range (struct symtab_and_line sal, CORE_ADDR *startptr,
		    CORE_ADDR *endptr)
{
  CORE_ADDR startaddr;
  struct symtab_and_line found_sal;

  startaddr = sal.pc;
  if (startaddr == 0 && !find_line_pc (sal.symtab, sal.line, &startaddr))
    return 0;

  /* This whole function is based on address.  For example, if line 10 has
     two parts, one from 0x100 to 0x200 and one from 0x300 to 0x400, then
     "info line *0x123" should say the line goes from 0x100 to 0x200
     and "info line *0x355" should say the line goes from 0x300 to 0x400.
     This also insures that we never give a range like "starts at 0x134
     and ends at 0x12c".  */

  found_sal = find_pc_sect_line (startaddr, sal.section, 0);
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
find_line_common (register struct linetable *l, register int lineno,
		  int *exact_match)
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
find_pc_line_pc_range (CORE_ADDR pc, CORE_ADDR *startptr, CORE_ADDR *endptr)
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

struct symtab_and_line
find_function_start_sal (struct symbol *sym, int funfirstline)
{
  CORE_ADDR pc;
  struct symtab_and_line sal;

  pc = BLOCK_START (SYMBOL_BLOCK_VALUE (sym));
  fixup_symbol_section (sym, NULL);
  if (funfirstline)
    {				/* skip "first line" of function (which is actually its prologue) */
      asection *section = SYMBOL_BFD_SECTION (sym);
      /* If function is in an unmapped overlay, use its unmapped LMA
         address, so that SKIP_PROLOGUE has something unique to work on */
      if (section_is_overlay (section) &&
	  !section_is_mapped (section))
	pc = overlay_unmapped_address (pc, section);

      pc += FUNCTION_START_OFFSET;
      pc = SKIP_PROLOGUE (pc);

      /* For overlays, map pc back into its mapped VMA range */
      pc = overlay_mapped_address (pc, section);
    }
  sal = find_pc_sect_line (pc, SYMBOL_BFD_SECTION (sym), 0);

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
      sal = find_pc_sect_line (pc, SYMBOL_BFD_SECTION (sym), 0);
    }
  sal.pc = pc;
#endif

  return sal;
}

/* If P is of the form "operator[ \t]+..." where `...' is
   some legitimate operator text, return a pointer to the
   beginning of the substring of the operator text.
   Otherwise, return "".  */
char *
operator_chars (char *p, char **end)
{
  *end = "";
  if (strncmp (p, "operator", 8))
    return *end;
  p += 8;

  /* Don't get faked out by `operator' being part of a longer
     identifier.  */
  if (isalpha (*p) || *p == '_' || *p == '$' || *p == '\0')
    return *end;

  /* Allow some whitespace between `operator' and the operator symbol.  */
  while (*p == ' ' || *p == '\t')
    p++;

  /* Recognize 'operator TYPENAME'. */

  if (isalpha (*p) || *p == '_' || *p == '$')
    {
      register char *q = p + 1;
      while (isalnum (*q) || *q == '_' || *q == '$')
	q++;
      *end = q;
      return p;
    }

  while (*p)
    switch (*p)
      {
      case '\\':			/* regexp quoting */
	if (p[1] == '*')
	  {
	    if (p[2] == '=')	/* 'operator\*=' */
	      *end = p + 3;
	    else			/* 'operator\*'  */
	      *end = p + 2;
	    return p;
	  }
	else if (p[1] == '[')
	  {
	    if (p[2] == ']')
	      error ("mismatched quoting on brackets, try 'operator\\[\\]'");
	    else if (p[2] == '\\' && p[3] == ']')
	      {
		*end = p + 4;	/* 'operator\[\]' */
		return p;
	      }
	    else
	      error ("nothing is allowed between '[' and ']'");
	  }
	else 
	  {
	    /* Gratuitous qoute: skip it and move on. */
	    p++;
	    continue;
	  }
	break;
      case '!':
      case '=':
      case '*':
      case '/':
      case '%':
      case '^':
	if (p[1] == '=')
	  *end = p + 2;
	else
	  *end = p + 1;
	return p;
      case '<':
      case '>':
      case '+':
      case '-':
      case '&':
      case '|':
	if (p[0] == '-' && p[1] == '>')
	  {
	    /* Struct pointer member operator 'operator->'. */
	    if (p[2] == '*')
	      {
		*end = p + 3;	/* 'operator->*' */
		return p;
	      }
	    else if (p[2] == '\\')
	      {
		*end = p + 4;	/* Hopefully 'operator->\*' */
		return p;
	      }
	    else
	      {
		*end = p + 2;	/* 'operator->' */
		return p;
	      }
	  }
	if (p[1] == '=' || p[1] == p[0])
	  *end = p + 2;
	else
	  *end = p + 1;
	return p;
      case '~':
      case ',':
	*end = p + 1;
	return p;
      case '(':
	if (p[1] != ')')
	  error ("`operator ()' must be specified without whitespace in `()'");
	*end = p + 2;
	return p;
      case '?':
	if (p[1] != ':')
	  error ("`operator ?:' must be specified without whitespace in `?:'");
	*end = p + 2;
	return p;
      case '[':
	if (p[1] != ']')
	  error ("`operator []' must be specified without whitespace in `[]'");
	*end = p + 2;
	return p;
      default:
	error ("`operator %s' not supported", p);
	break;
      }

  *end = "";
  return *end;
}


/* If FILE is not already in the table of files, return zero;
   otherwise return non-zero.  Optionally add FILE to the table if ADD
   is non-zero.  If *FIRST is non-zero, forget the old table
   contents.  */
static int
filename_seen (const char *file, int add, int *first)
{
  /* Table of files seen so far.  */
  static const char **tab = NULL;
  /* Allocated size of tab in elements.
     Start with one 256-byte block (when using GNU malloc.c).
     24 is the malloc overhead when range checking is in effect.  */
  static int tab_alloc_size = (256 - 24) / sizeof (char *);
  /* Current size of tab in elements.  */
  static int tab_cur_size;
  const char **p;

  if (*first)
    {
      if (tab == NULL)
	tab = (const char **) xmalloc (tab_alloc_size * sizeof (*tab));
      tab_cur_size = 0;
    }

  /* Is FILE in tab?  */
  for (p = tab; p < tab + tab_cur_size; p++)
    if (strcmp (*p, file) == 0)
      return 1;

  /* No; maybe add it to tab.  */
  if (add)
    {
      if (tab_cur_size == tab_alloc_size)
	{
	  tab_alloc_size *= 2;
	  tab = (const char **) xrealloc ((char *) tab,
					  tab_alloc_size * sizeof (*tab));
	}
      tab[tab_cur_size++] = file;
    }

  return 0;
}

/* Slave routine for sources_info.  Force line breaks at ,'s.
   NAME is the name to print and *FIRST is nonzero if this is the first
   name printed.  Set *FIRST to zero.  */
static void
output_source_filename (char *name, int *first)
{
  /* Since a single source file can result in several partial symbol
     tables, we need to avoid printing it more than once.  Note: if
     some of the psymtabs are read in and some are not, it gets
     printed both under "Source files for which symbols have been
     read" and "Source files for which symbols will be read in on
     demand".  I consider this a reasonable way to deal with the
     situation.  I'm not sure whether this can also happen for
     symtabs; it doesn't hurt to check.  */

  /* Was NAME already seen?  */
  if (filename_seen (name, 1, first))
    {
      /* Yes; don't print it again.  */
      return;
    }
  /* No; print it and reset *FIRST.  */
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
sources_info (char *ignore, int from_tty)
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  int first;

  if (!have_full_symbols () && !have_partial_symbols ())
    {
      error ("No symbol table is loaded.  Use the \"file\" command.");
    }

  printf_filtered ("Source files for which symbols have been read in:\n\n");

  first = 1;
  ALL_SYMTABS (objfile, s)
  {
    output_source_filename (s->filename, &first);
  }
  printf_filtered ("\n\n");

  printf_filtered ("Source files for which symbols will be read in on demand:\n\n");

  first = 1;
  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin)
      {
	output_source_filename (ps->filename, &first);
      }
  }
  printf_filtered ("\n");
}

static int
file_matches (char *file, char *files[], int nfiles)
{
  int i;

  if (file != NULL && nfiles != 0)
    {
      for (i = 0; i < nfiles; i++)
	{
	  if (strcmp (files[i], lbasename (file)) == 0)
	    return 1;
	}
    }
  else if (nfiles == 0)
    return 1;
  return 0;
}

/* Free any memory associated with a search. */
void
free_search_symbols (struct symbol_search *symbols)
{
  struct symbol_search *p;
  struct symbol_search *next;

  for (p = symbols; p != NULL; p = next)
    {
      next = p->next;
      xfree (p);
    }
}

static void
do_free_search_symbols_cleanup (void *symbols)
{
  free_search_symbols (symbols);
}

struct cleanup *
make_cleanup_free_search_symbols (struct symbol_search *symbols)
{
  return make_cleanup (do_free_search_symbols_cleanup, symbols);
}

/* Helper function for sort_search_symbols and qsort.  Can only
   sort symbols, not minimal symbols.  */
static int
compare_search_syms (const void *sa, const void *sb)
{
  struct symbol_search **sym_a = (struct symbol_search **) sa;
  struct symbol_search **sym_b = (struct symbol_search **) sb;

  return strcmp (SYMBOL_SOURCE_NAME ((*sym_a)->symbol),
		 SYMBOL_SOURCE_NAME ((*sym_b)->symbol));
}

/* Sort the ``nfound'' symbols in the list after prevtail.  Leave
   prevtail where it is, but update its next pointer to point to
   the first of the sorted symbols.  */
static struct symbol_search *
sort_search_symbols (struct symbol_search *prevtail, int nfound)
{
  struct symbol_search **symbols, *symp, *old_next;
  int i;

  symbols = (struct symbol_search **) xmalloc (sizeof (struct symbol_search *)
					       * nfound);
  symp = prevtail->next;
  for (i = 0; i < nfound; i++)
    {
      symbols[i] = symp;
      symp = symp->next;
    }
  /* Generally NULL.  */
  old_next = symp;

  qsort (symbols, nfound, sizeof (struct symbol_search *),
	 compare_search_syms);

  symp = prevtail;
  for (i = 0; i < nfound; i++)
    {
      symp->next = symbols[i];
      symp = symp->next;
    }
  symp->next = old_next;

  xfree (symbols);
  return symp;
}

/* Search the symbol table for matches to the regular expression REGEXP,
   returning the results in *MATCHES.

   Only symbols of KIND are searched:
   FUNCTIONS_NAMESPACE - search all functions
   TYPES_NAMESPACE     - search all type names
   METHODS_NAMESPACE   - search all methods NOT IMPLEMENTED
   VARIABLES_NAMESPACE - search all symbols, excluding functions, type names,
   and constants (enums)

   free_search_symbols should be called when *MATCHES is no longer needed.

   The results are sorted locally; each symtab's global and static blocks are
   separately alphabetized.
 */
void
search_symbols (char *regexp, namespace_enum kind, int nfiles, char *files[],
		struct symbol_search **matches)
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  register struct block *b;
  register int i = 0;
  register int j;
  register struct symbol *sym;
  struct partial_symbol **psym;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  char *val;
  int found_misc = 0;
  static enum minimal_symbol_type types[]
  =
  {mst_data, mst_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types2[]
  =
  {mst_bss, mst_file_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types3[]
  =
  {mst_file_data, mst_solib_trampoline, mst_abs, mst_unknown};
  static enum minimal_symbol_type types4[]
  =
  {mst_file_bss, mst_text, mst_abs, mst_unknown};
  enum minimal_symbol_type ourtype;
  enum minimal_symbol_type ourtype2;
  enum minimal_symbol_type ourtype3;
  enum minimal_symbol_type ourtype4;
  struct symbol_search *sr;
  struct symbol_search *psr;
  struct symbol_search *tail;
  struct cleanup *old_chain = NULL;

  if (kind < VARIABLES_NAMESPACE)
    error ("must search on specific namespace");

  ourtype = types[(int) (kind - VARIABLES_NAMESPACE)];
  ourtype2 = types2[(int) (kind - VARIABLES_NAMESPACE)];
  ourtype3 = types3[(int) (kind - VARIABLES_NAMESPACE)];
  ourtype4 = types4[(int) (kind - VARIABLES_NAMESPACE)];

  sr = *matches = NULL;
  tail = NULL;

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
	  int fix = -1;		/* -1 means ok; otherwise number of spaces needed. */
	  if (isalpha (*opname) || *opname == '_' || *opname == '$')
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
	      char *tmp = (char *) alloca (8 + fix + strlen (opname) + 1);
	      sprintf (tmp, "operator%.*s%s", fix, " ", opname);
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

    if (ps->readin)
      continue;

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
	    if (file_matches (ps->filename, files, nfiles)
		&& ((regexp == NULL || SYMBOL_MATCHES_REGEXP (*psym))
		    && ((kind == VARIABLES_NAMESPACE && SYMBOL_CLASS (*psym) != LOC_TYPEDEF
			 && SYMBOL_CLASS (*psym) != LOC_BLOCK)
			|| (kind == FUNCTIONS_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_BLOCK)
			|| (kind == TYPES_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_TYPEDEF)
			|| (kind == METHODS_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_BLOCK))))
	      {
		PSYMTAB_TO_SYMTAB (ps);
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

  if (nfiles == 0 && (kind == VARIABLES_NAMESPACE || kind == FUNCTIONS_NAMESPACE))
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
		    if (kind == FUNCTIONS_NAMESPACE
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

  ALL_SYMTABS (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    /* Often many files share a blockvector.
       Scan each blockvector only once so that
       we don't get every symbol many times.
       It happens that the first symtab in the list
       for any given blockvector is the main file.  */
    if (bv != prev_bv)
      for (i = GLOBAL_BLOCK; i <= STATIC_BLOCK; i++)
	{
	  struct symbol_search *prevtail = tail;
	  int nfound = 0;
	  b = BLOCKVECTOR_BLOCK (bv, i);
	  for (j = 0; j < BLOCK_NSYMS (b); j++)
	    {
	      QUIT;
	      sym = BLOCK_SYM (b, j);
	      if (file_matches (s->filename, files, nfiles)
		  && ((regexp == NULL || SYMBOL_MATCHES_REGEXP (sym))
		      && ((kind == VARIABLES_NAMESPACE && SYMBOL_CLASS (sym) != LOC_TYPEDEF
			   && SYMBOL_CLASS (sym) != LOC_BLOCK
			   && SYMBOL_CLASS (sym) != LOC_CONST)
			  || (kind == FUNCTIONS_NAMESPACE && SYMBOL_CLASS (sym) == LOC_BLOCK)
			  || (kind == TYPES_NAMESPACE && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
			  || (kind == METHODS_NAMESPACE && SYMBOL_CLASS (sym) == LOC_BLOCK))))
		{
		  /* match */
		  psr = (struct symbol_search *) xmalloc (sizeof (struct symbol_search));
		  psr->block = i;
		  psr->symtab = s;
		  psr->symbol = sym;
		  psr->msymbol = NULL;
		  psr->next = NULL;
		  if (tail == NULL)
		    sr = psr;
		  else
		    tail->next = psr;
		  tail = psr;
		  nfound ++;
		}
	    }
	  if (nfound > 0)
	    {
	      if (prevtail == NULL)
		{
		  struct symbol_search dummy;

		  dummy.next = sr;
		  tail = sort_search_symbols (&dummy, nfound);
		  sr = dummy.next;

		  old_chain = make_cleanup_free_search_symbols (sr);
		}
	      else
		tail = sort_search_symbols (prevtail, nfound);
	    }
	}
    prev_bv = bv;
  }

  /* If there are no eyes, avoid all contact.  I mean, if there are
     no debug symbols, then print directly from the msymbol_vector.  */

  if (found_misc || kind != FUNCTIONS_NAMESPACE)
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
		/* Functions:  Look up by address. */
		if (kind != FUNCTIONS_NAMESPACE ||
		    (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol))))
		  {
		    /* Variables/Absolutes:  Look up by name */
		    if (lookup_symbol (SYMBOL_NAME (msymbol),
				       (struct block *) NULL, VAR_NAMESPACE,
				       0, (struct symtab **) NULL) == NULL)
		      {
			/* match */
			psr = (struct symbol_search *) xmalloc (sizeof (struct symbol_search));
			psr->block = i;
			psr->msymbol = msymbol;
			psr->symtab = NULL;
			psr->symbol = NULL;
			psr->next = NULL;
			if (tail == NULL)
			  {
			    sr = psr;
			    old_chain = make_cleanup_free_search_symbols (sr);
			  }
			else
			  tail->next = psr;
			tail = psr;
		      }
		  }
	      }
	  }
      }
    }

  *matches = sr;
  if (sr != NULL)
    discard_cleanups (old_chain);
}

/* Helper function for symtab_symbol_info, this function uses
   the data returned from search_symbols() to print information
   regarding the match to gdb_stdout.
 */
static void
print_symbol_info (namespace_enum kind, struct symtab *s, struct symbol *sym,
		   int block, char *last)
{
  if (last == NULL || strcmp (last, s->filename) != 0)
    {
      fputs_filtered ("\nFile ", gdb_stdout);
      fputs_filtered (s->filename, gdb_stdout);
      fputs_filtered (":\n", gdb_stdout);
    }

  if (kind != TYPES_NAMESPACE && block == STATIC_BLOCK)
    printf_filtered ("static ");

  /* Typedef that is not a C++ class */
  if (kind == TYPES_NAMESPACE
      && SYMBOL_NAMESPACE (sym) != STRUCT_NAMESPACE)
    typedef_print (SYMBOL_TYPE (sym), sym, gdb_stdout);
  /* variable, func, or typedef-that-is-c++-class */
  else if (kind < TYPES_NAMESPACE ||
	   (kind == TYPES_NAMESPACE &&
	    SYMBOL_NAMESPACE (sym) == STRUCT_NAMESPACE))
    {
      type_print (SYMBOL_TYPE (sym),
		  (SYMBOL_CLASS (sym) == LOC_TYPEDEF
		   ? "" : SYMBOL_SOURCE_NAME (sym)),
		  gdb_stdout, 0);

      printf_filtered (";\n");
    }
  else
    {
#if 0
      /* Tiemann says: "info methods was never implemented."  */
      char *demangled_name;
      c_type_print_base (TYPE_FN_FIELD_TYPE (t, block),
			 gdb_stdout, 0, 0);
      c_type_print_varspec_prefix (TYPE_FN_FIELD_TYPE (t, block),
				   gdb_stdout, 0);
      if (TYPE_FN_FIELD_STUB (t, block))
	check_stub_method (TYPE_DOMAIN_TYPE (type), j, block);
      demangled_name =
	cplus_demangle (TYPE_FN_FIELD_PHYSNAME (t, block),
			DMGL_ANSI | DMGL_PARAMS);
      if (demangled_name == NULL)
	fprintf_filtered (stream, "<badly mangled name %s>",
			  TYPE_FN_FIELD_PHYSNAME (t, block));
      else
	{
	  fputs_filtered (demangled_name, stream);
	  xfree (demangled_name);
	}
#endif
    }
}

/* This help function for symtab_symbol_info() prints information
   for non-debugging symbols to gdb_stdout.
 */
static void
print_msymbol_info (struct minimal_symbol *msymbol)
{
  char *tmp;

  if (TARGET_ADDR_BIT <= 32)
    tmp = longest_local_hex_string_custom (SYMBOL_VALUE_ADDRESS (msymbol)
					   & (CORE_ADDR) 0xffffffff,
					   "08l");
  else
    tmp = longest_local_hex_string_custom (SYMBOL_VALUE_ADDRESS (msymbol),
					   "016l");
  printf_filtered ("%s  %s\n",
		   tmp, SYMBOL_SOURCE_NAME (msymbol));
}

/* This is the guts of the commands "info functions", "info types", and
   "info variables". It calls search_symbols to find all matches and then
   print_[m]symbol_info to print out some useful information about the
   matches.
 */
static void
symtab_symbol_info (char *regexp, namespace_enum kind, int from_tty)
{
  static char *classnames[]
  =
  {"variable", "function", "type", "method"};
  struct symbol_search *symbols;
  struct symbol_search *p;
  struct cleanup *old_chain;
  char *last_filename = NULL;
  int first = 1;

  /* must make sure that if we're interrupted, symbols gets freed */
  search_symbols (regexp, kind, 0, (char **) NULL, &symbols);
  old_chain = make_cleanup_free_search_symbols (symbols);

  printf_filtered (regexp
		   ? "All %ss matching regular expression \"%s\":\n"
		   : "All defined %ss:\n",
		   classnames[(int) (kind - VARIABLES_NAMESPACE)], regexp);

  for (p = symbols; p != NULL; p = p->next)
    {
      QUIT;

      if (p->msymbol != NULL)
	{
	  if (first)
	    {
	      printf_filtered ("\nNon-debugging symbols:\n");
	      first = 0;
	    }
	  print_msymbol_info (p->msymbol);
	}
      else
	{
	  print_symbol_info (kind,
			     p->symtab,
			     p->symbol,
			     p->block,
			     last_filename);
	  last_filename = p->symtab->filename;
	}
    }

  do_cleanups (old_chain);
}

static void
variables_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, VARIABLES_NAMESPACE, from_tty);
}

static void
functions_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, FUNCTIONS_NAMESPACE, from_tty);
}


static void
types_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, TYPES_NAMESPACE, from_tty);
}

#if 0
/* Tiemann says: "info methods was never implemented."  */
static void
methods_info (char *regexp)
{
  symtab_symbol_info (regexp, METHODS_NAMESPACE, 0, from_tty);
}
#endif /* 0 */

/* Breakpoint all functions matching regular expression. */

void
rbreak_command_wrapper (char *regexp, int from_tty)
{
  rbreak_command (regexp, from_tty);
}

static void
rbreak_command (char *regexp, int from_tty)
{
  struct symbol_search *ss;
  struct symbol_search *p;
  struct cleanup *old_chain;

  search_symbols (regexp, FUNCTIONS_NAMESPACE, 0, (char **) NULL, &ss);
  old_chain = make_cleanup_free_search_symbols (ss);

  for (p = ss; p != NULL; p = p->next)
    {
      if (p->msymbol == NULL)
	{
	  char *string = (char *) alloca (strlen (p->symtab->filename)
					  + strlen (SYMBOL_NAME (p->symbol))
					  + 4);
	  strcpy (string, p->symtab->filename);
	  strcat (string, ":'");
	  strcat (string, SYMBOL_NAME (p->symbol));
	  strcat (string, "'");
	  break_command (string, from_tty);
	  print_symbol_info (FUNCTIONS_NAMESPACE,
			     p->symtab,
			     p->symbol,
			     p->block,
			     p->symtab->filename);
	}
      else
	{
	  break_command (SYMBOL_NAME (p->msymbol), from_tty);
	  printf_filtered ("<function, no debug info> %s;\n",
			   SYMBOL_SOURCE_NAME (p->msymbol));
	}
    }

  do_cleanups (old_chain);
}


/* Return Nonzero if block a is lexically nested within block b,
   or if a and b have the same pc range.
   Return zero otherwise. */
int
contained_in (struct block *a, struct block *b)
{
  if (!a || !b)
    return 0;
  return BLOCK_START (a) >= BLOCK_START (b)
    && BLOCK_END (a) <= BLOCK_END (b);
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
completion_list_add_name (char *symname, char *sym_text, int sym_text_len,
			  char *text, char *word)
{
  int newsize;
  int i;

  /* clip symbols that cannot match */

  if (strncmp (symname, sym_text, sym_text_len) != 0)
    {
      return;
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

    if (return_val_index + 3 > return_val_size)
      {
	newsize = (return_val_size *= 2) * sizeof (char *);
	return_val = (char **) xrealloc ((char *) return_val, newsize);
      }
    return_val[return_val_index++] = new;
    return_val[return_val_index] = NULL;
  }
}

/* Return a NULL terminated array of all symbols (regardless of class)
   which begin by matching TEXT.  If the answer is no symbols, then
   the return value is an array which contains only a NULL pointer.

   Problem: All of the symbols have to be copied because readline frees them.
   I'm not going to worry about this; hopefully there won't be that many.  */

char **
make_symbol_completion_list (char *text, char *word)
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
      {
	return_val = (char **) xmalloc (sizeof (char *));
	return_val[0] = NULL;
	return return_val;
      }
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
    if (ps->readin)
      continue;

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
	  surrounding_static_block = b;		/* For elmin of dups */
	}

      /* Also catch fields of types defined in this places which match our
         text string.  Only complete on types visible from current context. */

      ALL_BLOCK_SYMBOLS (b, i, sym)
	{
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
    ALL_BLOCK_SYMBOLS (b, i, sym)
      {
	COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
      }
  }

  ALL_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
    /* Don't do this block twice.  */
    if (b == surrounding_static_block)
      continue;
    ALL_BLOCK_SYMBOLS (b, i, sym)
      {
	COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
      }
  }

  return (return_val);
}

/* Like make_symbol_completion_list, but returns a list of symbols
   defined in a source file FILE.  */

char **
make_file_symbol_completion_list (char *text, char *word, char *srcfile)
{
  register struct symbol *sym;
  register struct symtab *s;
  register struct block *b;
  register int i;
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
      {
	return_val = (char **) xmalloc (sizeof (char *));
	return_val[0] = NULL;
	return return_val;
      }
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

  return_val_size = 10;
  return_val_index = 0;
  return_val = (char **) xmalloc ((return_val_size + 1) * sizeof (char *));
  return_val[0] = NULL;

  /* Find the symtab for SRCFILE (this loads it if it was not yet read
     in).  */
  s = lookup_symtab (srcfile);
  if (s == NULL)
    {
      /* Maybe they typed the file with leading directories, while the
	 symbol tables record only its basename.  */
      const char *tail = lbasename (srcfile);

      if (tail > srcfile)
	s = lookup_symtab (tail);
    }

  /* If we have no symtab for that file, return an empty list.  */
  if (s == NULL)
    return (return_val);

  /* Go through this symtab and check the externs and statics for
     symbols which match.  */

  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
  ALL_BLOCK_SYMBOLS (b, i, sym)
    {
      COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
    }

  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
  ALL_BLOCK_SYMBOLS (b, i, sym)
    {
      COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
    }

  return (return_val);
}

/* A helper function for make_source_files_completion_list.  It adds
   another file name to a list of possible completions, growing the
   list as necessary.  */

static void
add_filename_to_list (const char *fname, char *text, char *word,
		      char ***list, int *list_used, int *list_alloced)
{
  char *new;
  size_t fnlen = strlen (fname);

  if (*list_used + 1 >= *list_alloced)
    {
      *list_alloced *= 2;
      *list = (char **) xrealloc ((char *) *list,
				  *list_alloced * sizeof (char *));
    }

  if (word == text)
    {
      /* Return exactly fname.  */
      new = xmalloc (fnlen + 5);
      strcpy (new, fname);
    }
  else if (word > text)
    {
      /* Return some portion of fname.  */
      new = xmalloc (fnlen + 5);
      strcpy (new, fname + (word - text));
    }
  else
    {
      /* Return some of TEXT plus fname.  */
      new = xmalloc (fnlen + (text - word) + 5);
      strncpy (new, word, text - word);
      new[text - word] = '\0';
      strcat (new, fname);
    }
  (*list)[*list_used] = new;
  (*list)[++*list_used] = NULL;
}

static int
not_interesting_fname (const char *fname)
{
  static const char *illegal_aliens[] = {
    "_globals_",	/* inserted by coff_symtab_read */
    NULL
  };
  int i;

  for (i = 0; illegal_aliens[i]; i++)
    {
      if (strcmp (fname, illegal_aliens[i]) == 0)
	return 1;
    }
  return 0;
}

/* Return a NULL terminated array of all source files whose names
   begin with matching TEXT.  The file names are looked up in the
   symbol tables of this program.  If the answer is no matchess, then
   the return value is an array which contains only a NULL pointer.  */

char **
make_source_files_completion_list (char *text, char *word)
{
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  int first = 1;
  int list_alloced = 1;
  int list_used = 0;
  size_t text_len = strlen (text);
  char **list = (char **) xmalloc (list_alloced * sizeof (char *));
  const char *base_name;

  list[0] = NULL;

  if (!have_full_symbols () && !have_partial_symbols ())
    return list;

  ALL_SYMTABS (objfile, s)
    {
      if (not_interesting_fname (s->filename))
	continue;
      if (!filename_seen (s->filename, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	  && strncasecmp (s->filename, text, text_len) == 0
#else
	  && strncmp (s->filename, text, text_len) == 0
#endif
	  )
	{
	  /* This file matches for a completion; add it to the current
	     list of matches.  */
	  add_filename_to_list (s->filename, text, word,
				&list, &list_used, &list_alloced);
	}
      else
	{
	  /* NOTE: We allow the user to type a base name when the
	     debug info records leading directories, but not the other
	     way around.  This is what subroutines of breakpoint
	     command do when they parse file names.  */
	  base_name = lbasename (s->filename);
	  if (base_name != s->filename
	      && !filename_seen (base_name, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	      && strncasecmp (base_name, text, text_len) == 0
#else
	      && strncmp (base_name, text, text_len) == 0
#endif
	      )
	    add_filename_to_list (base_name, text, word,
				  &list, &list_used, &list_alloced);
	}
    }

  ALL_PSYMTABS (objfile, ps)
    {
      if (not_interesting_fname (ps->filename))
	continue;
      if (!ps->readin)
	{
	  if (!filename_seen (ps->filename, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	      && strncasecmp (ps->filename, text, text_len) == 0
#else
	      && strncmp (ps->filename, text, text_len) == 0
#endif
	      )
	    {
	      /* This file matches for a completion; add it to the
		 current list of matches.  */
	      add_filename_to_list (ps->filename, text, word,
				    &list, &list_used, &list_alloced);

	    }
	  else
	    {
	      base_name = lbasename (ps->filename);
	      if (base_name != ps->filename
		  && !filename_seen (base_name, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
		  && strncasecmp (base_name, text, text_len) == 0
#else
		  && strncmp (base_name, text, text_len) == 0
#endif
		  )
		add_filename_to_list (base_name, text, word,
				      &list, &list_used, &list_alloced);
	    }
	}
    }

  return list;
}

/* Determine if PC is in the prologue of a function.  The prologue is the area
   between the first instruction of a function, and the first executable line.
   Returns 1 if PC *might* be in prologue, 0 if definately *not* in prologue.

   If non-zero, func_start is where we think the prologue starts, possibly
   by previous examination of symbol table information.
 */

int
in_prologue (CORE_ADDR pc, CORE_ADDR func_start)
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  /* We have several sources of information we can consult to figure
     this out.
     - Compilers usually emit line number info that marks the prologue
       as its own "source line".  So the ending address of that "line"
       is the end of the prologue.  If available, this is the most
       reliable method.
     - The minimal symbols and partial symbols, which can usually tell
       us the starting and ending addresses of a function.
     - If we know the function's start address, we can call the
       architecture-defined SKIP_PROLOGUE function to analyze the
       instruction stream and guess where the prologue ends.
     - Our `func_start' argument; if non-zero, this is the caller's
       best guess as to the function's entry point.  At the time of
       this writing, handle_inferior_event doesn't get this right, so
       it should be our last resort.  */

  /* Consult the partial symbol table, to find which function
     the PC is in.  */
  if (! find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      CORE_ADDR prologue_end;

      /* We don't even have minsym information, so fall back to using
         func_start, if given.  */
      if (! func_start)
	return 1;		/* We *might* be in a prologue.  */

      prologue_end = SKIP_PROLOGUE (func_start);

      return func_start <= pc && pc < prologue_end;
    }

  /* If we have line number information for the function, that's
     usually pretty reliable.  */
  sal = find_pc_line (func_addr, 0);

  /* Now sal describes the source line at the function's entry point,
     which (by convention) is the prologue.  The end of that "line",
     sal.end, is the end of the prologue.

     Note that, for functions whose source code is all on a single
     line, the line number information doesn't always end up this way.
     So we must verify that our purported end-of-prologue address is
     *within* the function, not at its start or end.  */
  if (sal.line == 0
      || sal.end <= func_addr
      || func_end <= sal.end)
    {
      /* We don't have any good line number info, so use the minsym
	 information, together with the architecture-specific prologue
	 scanning code.  */
      CORE_ADDR prologue_end = SKIP_PROLOGUE (func_addr);

      return func_addr <= pc && pc < prologue_end;
    }

  /* We have line number info, and it looks good.  */
  return func_addr <= pc && pc < sal.end;
}


/* Begin overload resolution functions */
/* Helper routine for make_symbol_completion_list.  */

static int sym_return_val_size;
static int sym_return_val_index;
static struct symbol **sym_return_val;

/*  Test to see if the symbol specified by SYMNAME (which is already
   demangled for C++ symbols) matches SYM_TEXT in the first SYM_TEXT_LEN
   characters.  If so, add it to the current completion list. */

static void
overload_list_add_symbol (struct symbol *sym, char *oload_name)
{
  int newsize;
  int i;

  /* Get the demangled name without parameters */
  char *sym_name = cplus_demangle (SYMBOL_NAME (sym), DMGL_ARM | DMGL_ANSI);
  if (!sym_name)
    {
      sym_name = (char *) xmalloc (strlen (SYMBOL_NAME (sym)) + 1);
      strcpy (sym_name, SYMBOL_NAME (sym));
    }

  /* skip symbols that cannot match */
  if (strcmp (sym_name, oload_name) != 0)
    {
      xfree (sym_name);
      return;
    }

  /* If there is no type information, we can't do anything, so skip */
  if (SYMBOL_TYPE (sym) == NULL)
    return;

  /* skip any symbols that we've already considered. */
  for (i = 0; i < sym_return_val_index; ++i)
    if (!strcmp (SYMBOL_NAME (sym), SYMBOL_NAME (sym_return_val[i])))
      return;

  /* We have a match for an overload instance, so add SYM to the current list
   * of overload instances */
  if (sym_return_val_index + 3 > sym_return_val_size)
    {
      newsize = (sym_return_val_size *= 2) * sizeof (struct symbol *);
      sym_return_val = (struct symbol **) xrealloc ((char *) sym_return_val, newsize);
    }
  sym_return_val[sym_return_val_index++] = sym;
  sym_return_val[sym_return_val_index] = NULL;

  xfree (sym_name);
}

/* Return a null-terminated list of pointers to function symbols that
 * match name of the supplied symbol FSYM.
 * This is used in finding all overloaded instances of a function name.
 * This has been modified from make_symbol_completion_list.  */


struct symbol **
make_symbol_overload_list (struct symbol *fsym)
{
  register struct symbol *sym;
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  register struct block *b, *surrounding_static_block = 0;
  register int i;
  /* The name we are completing on. */
  char *oload_name = NULL;
  /* Length of name.  */
  int oload_name_len = 0;

  /* Look for the symbol we are supposed to complete on.
   * FIXME: This should be language-specific.  */

  oload_name = cplus_demangle (SYMBOL_NAME (fsym), DMGL_ARM | DMGL_ANSI);
  if (!oload_name)
    {
      oload_name = (char *) xmalloc (strlen (SYMBOL_NAME (fsym)) + 1);
      strcpy (oload_name, SYMBOL_NAME (fsym));
    }
  oload_name_len = strlen (oload_name);

  sym_return_val_size = 100;
  sym_return_val_index = 0;
  sym_return_val = (struct symbol **) xmalloc ((sym_return_val_size + 1) * sizeof (struct symbol *));
  sym_return_val[0] = NULL;

  /* Look through the partial symtabs for all symbols which begin
     by matching OLOAD_NAME.  Make sure we read that symbol table in. */

  ALL_PSYMTABS (objfile, ps)
  {
    struct partial_symbol **psym;

    /* If the psymtab's been read in we'll get it when we search
       through the blockvector.  */
    if (ps->readin)
      continue;

    for (psym = objfile->global_psymbols.list + ps->globals_offset;
	 psym < (objfile->global_psymbols.list + ps->globals_offset
		 + ps->n_global_syms);
	 psym++)
      {
	/* If interrupted, then quit. */
	QUIT;
        /* This will cause the symbol table to be read if it has not yet been */
        s = PSYMTAB_TO_SYMTAB (ps);
      }

    for (psym = objfile->static_psymbols.list + ps->statics_offset;
	 psym < (objfile->static_psymbols.list + ps->statics_offset
		 + ps->n_static_syms);
	 psym++)
      {
	QUIT;
        /* This will cause the symbol table to be read if it has not yet been */
        s = PSYMTAB_TO_SYMTAB (ps);
      }
  }

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */

  for (b = get_selected_block (); b != NULL; b = BLOCK_SUPERBLOCK (b))
    {
      if (!BLOCK_SUPERBLOCK (b))
	{
	  surrounding_static_block = b;		/* For elimination of dups */
	}

      /* Also catch fields of types defined in this places which match our
         text string.  Only complete on types visible from current context. */

      ALL_BLOCK_SYMBOLS (b, i, sym)
	{
	  overload_list_add_symbol (sym, oload_name);
	}
    }

  /* Go through the symtabs and check the externs and statics for
     symbols which match.  */

  ALL_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
    ALL_BLOCK_SYMBOLS (b, i, sym)
      {
	overload_list_add_symbol (sym, oload_name);
      }
  }

  ALL_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
    /* Don't do this block twice.  */
    if (b == surrounding_static_block)
      continue;
    ALL_BLOCK_SYMBOLS (b, i, sym)
      {
	overload_list_add_symbol (sym, oload_name);
      }
  }

  xfree (oload_name);

  return (sym_return_val);
}

/* End of overload resolution functions */

struct symtabs_and_lines
decode_line_spec (char *string, int funfirstline)
{
  struct symtabs_and_lines sals;
  if (string == 0)
    error ("Empty line specification.");
  sals = decode_line_1 (&string, funfirstline,
			current_source_symtab, current_source_line,
			(char ***) NULL);
  if (*string)
    error ("Junk at end of line specification: %s", string);
  return sals;
}

/* Track MAIN */
static char *name_of_main;

void
set_main_name (const char *name)
{
  if (name_of_main != NULL)
    {
      xfree (name_of_main);
      name_of_main = NULL;
    }
  if (name != NULL)
    {
      name_of_main = xstrdup (name);
    }
}

char *
main_name (void)
{
  if (name_of_main != NULL)
    return name_of_main;
  else
    return "main";
}


void
_initialize_symtab (void)
{
  add_info ("variables", variables_info,
	 "All global and static variable names, or those matching REGEXP.");
  if (dbx_commands)
    add_com ("whereis", class_info, variables_info,
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

  add_com ("rbreak", class_breakpoint, rbreak_command,
	   "Set a breakpoint for all functions matching REGEXP.");

  if (xdb_commands)
    {
      add_com ("lf", class_info, sources_info, "Source files in the program");
      add_com ("lg", class_info, variables_info,
	 "All global and static variable names, or those matching REGEXP.");
    }

  /* Initialize the one built-in type that isn't language dependent... */
  builtin_type_error = init_type (TYPE_CODE_ERROR, 0, 0,
				  "<unknown type>", (struct objfile *) NULL);
}
