/* Symbol table lookup for the GNU debugger, GDB.
   Copyright 1986, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 1998
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
#include "inferior.h"

#include "obstack.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>

/* Prototype for one function in parser-defs.h,
   instead of including that entire file. */

extern char * find_template_name_end PARAMS ((char *));

/* Prototypes for local functions */

static int find_methods PARAMS ((struct type *, char *, struct symbol **));

static void completion_list_add_name PARAMS ((char *, char *, int, char *, 
                                              char *));

static void build_canonical_line_spec PARAMS ((struct symtab_and_line *, 
                                               char *, char ***));

static struct symtabs_and_lines decode_line_2 PARAMS ((struct symbol *[], 
                                                       int, int, char ***));

static void rbreak_command PARAMS ((char *, int));

static void types_info PARAMS ((char *, int));

static void functions_info PARAMS ((char *, int));

static void variables_info PARAMS ((char *, int));

static void sources_info PARAMS ((char *, int));

static void output_source_filename PARAMS ((char *, int *));

char *operator_chars PARAMS ((char *, char **));

static int find_line_common PARAMS ((struct linetable *, int, int *));

static struct partial_symbol *lookup_partial_symbol PARAMS 
                                  ((struct partial_symtab *, const char *,
			            int, namespace_enum));

static struct partial_symbol *fixup_psymbol_section PARAMS ((struct 
                                        partial_symbol *, struct objfile *));

static struct symtab *lookup_symtab_1 PARAMS ((char *));

static void cplusplus_hint PARAMS ((char *));

static struct symbol *find_active_alias PARAMS ((struct symbol *sym, 
                                                 CORE_ADDR addr));

/* This flag is used in hppa-tdep.c, and set in hp-symtab-read.c */
/* Signals the presence of objects compiled by HP compilers */
int hp_som_som_object_present = 0;

static void fixup_section PARAMS ((struct general_symbol_info *, 
                                   struct objfile *));

static int file_matches PARAMS ((char *, char **, int));

static void print_symbol_info PARAMS ((namespace_enum, 
                                       struct symtab *, struct symbol *, 
                                       int, char *));

static void print_msymbol_info PARAMS ((struct minimal_symbol *));

static void symtab_symbol_info PARAMS ((char *, namespace_enum, int));

void _initialize_symtab PARAMS ((void));

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

static void
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

/* Mangle a GDB method stub type.  This actually reassembles the pieces of the
   full method name, which consist of the class name (from T), the unadorned
   method name from METHOD_ID, and the signature for the specific overload,
   specified by SIGNATURE_ID.  Note that this function is g++ specific. */

char *
gdb_mangle_name (type, method_id, signature_id)
     struct type *type;
     int method_id, signature_id;
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



/* Find which partial symtab on contains PC and SECTION.  Return 0 if none.  */

struct partial_symtab *
find_pc_sect_psymtab (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  register struct partial_symtab *pst;
  register struct objfile *objfile;

  ALL_PSYMTABS (objfile, pst)
    {
#if defined(HPUXHPPA)
      if (pc >= pst->textlow && pc <= pst->texthigh)
#else
      if (pc >= pst->textlow && pc < pst->texthigh)
#endif
	{
	  struct minimal_symbol *msymbol;
	  struct partial_symtab *tpst;

	  /* An objfile that has its functions reordered might have
	     many partial symbol tables containing the PC, but
	     we want the partial symbol table that contains the
	     function containing the PC.  */
	  if (!(objfile->flags & OBJF_REORDERED) &&
	      section == 0)	/* can't validate section this way */
	    return (pst);

	  msymbol = lookup_minimal_symbol_by_pc_section (pc, section);
	  if (msymbol == NULL)
	    return (pst);

	  for (tpst = pst; tpst != NULL; tpst = tpst->next)
	    {
#if defined(HPUXHPPA)
	      if (pc >= tpst->textlow && pc <= tpst->texthigh)
#else
	      if (pc >= tpst->textlow && pc < tpst->texthigh)
#endif
		{
		  struct partial_symbol *p;

		  p = find_pc_sect_psymbol (tpst, pc, section);
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

/* Find which partial symtab contains PC.  Return 0 if none. 
   Backward compatibility, no section */

struct partial_symtab *
find_pc_psymtab (pc)
     CORE_ADDR pc;
{
  return find_pc_sect_psymtab (pc, find_pc_mapped_section (pc));
}

/* Find which partial symbol within a psymtab matches PC and SECTION.  
   Return 0 if none.  Check all psymtabs if PSYMTAB is 0.  */

struct partial_symbol *
find_pc_sect_psymbol (psymtab, pc, section)
     struct partial_symtab *psymtab;
     CORE_ADDR pc;
     asection *section;
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
	  if (section)	/* match on a specific section */
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
	  if (section)	/* match on a specific section */
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
find_pc_psymbol (psymtab, pc)
     struct partial_symtab *psymtab;
     CORE_ADDR pc;
{
  return find_pc_sect_psymbol (psymtab, pc, find_pc_mapped_section (pc));
}

/* Debug symbols usually don't have section information.  We need to dig that
   out of the minimal symbols and stash that in the debug symbol.  */

static void
fixup_section (ginfo, objfile)
     struct general_symbol_info *ginfo;
     struct objfile *objfile;
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (ginfo->name, NULL, objfile);

  if (msym)
    ginfo->bfd_section = SYMBOL_BFD_SECTION (msym);
}

struct symbol *
fixup_symbol_section (sym, objfile)
     struct symbol *sym;
     struct objfile *objfile;
{
  if (!sym)
    return NULL;

  if (SYMBOL_BFD_SECTION (sym))
    return sym;

  fixup_section (&sym->ginfo, objfile);

  return sym;
}

static struct partial_symbol *
fixup_psymbol_section (psym, objfile)
     struct partial_symbol *psym;
     struct objfile *objfile;
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
	      return fixup_symbol_section (sym, objfile);
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
      
#endif

  ALL_PSYMTABS (objfile, ps)
    {
      if (!ps->readin && lookup_partial_symbol (ps, name, 1, namespace))
	{
	  s = PSYMTAB_TO_SYMTAB(ps);
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
	  s = PSYMTAB_TO_SYMTAB(ps);
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
              if (sym) {
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
	      return lookup_symbol (SYMBOL_NAME (msymbol), block,
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
	  if (!do_linear_search
	      && (SYMBOL_LANGUAGE (*center) == language_cplus
		  || SYMBOL_LANGUAGE (*center) == language_java
		  ))
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

/* Look up a type named NAME in the struct_namespace.  The type returned
   must not be opaque -- i.e., must have at least one field defined

   This code was modelled on lookup_symbol -- the parts not relevant to looking
   up types were just left out.  In particular it's assumed here that types
   are available in struct_namespace and only at file-static or global blocks. */


struct type *
lookup_transparent_type (name)
     const char *name;
{
  register struct symbol *sym;
  register struct symtab *s = NULL;
  register struct partial_symtab *ps;
  struct blockvector *bv;
  register struct objfile *objfile;
  register struct block *block;
  register struct minimal_symbol *msymbol;

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
	  s = PSYMTAB_TO_SYMTAB(ps);
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
	  s = PSYMTAB_TO_SYMTAB(ps);
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
	  if (!do_linear_search
	      && (SYMBOL_LANGUAGE (sym) == language_cplus
		  || SYMBOL_LANGUAGE (sym) == language_java
		  ))
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
	      /* If SYM has aliases, then use any alias that is active
		 at the current PC.  If no alias is active at the current
		 PC, then use the main symbol.

		 ?!? Is checking the current pc correct?  Is this routine
		 ever called to look up a symbol from another context?  */
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
find_active_alias (sym, addr)
  struct symbol *sym;
  CORE_ADDR addr;
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
block_function (bl)
     struct block *bl;
{
  while (BLOCK_FUNCTION (bl) == 0 && BLOCK_SUPERBLOCK (bl) != 0)
    bl = BLOCK_SUPERBLOCK (bl);

  return BLOCK_FUNCTION (bl);
}

/* Find the symtab associated with PC and SECTION.  Look through the
   psymtabs and read in another symtab if necessary. */

struct symtab *
find_pc_sect_symtab (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  register struct block *b;
  struct blockvector *bv;
  register struct symtab *s = NULL;
  register struct symtab *best_s = NULL;
  register struct partial_symtab *ps;
  register struct objfile *objfile;
  CORE_ADDR distance = 0;

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
#if defined(HPUXHPPA)
	  && BLOCK_END (b) >= pc
#else
	  && BLOCK_END (b) > pc
#endif
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
		continue;	/* no symbol in this symtab matches section */
	    }
	  distance = BLOCK_END (b) - BLOCK_START (b);
	  best_s = s;
	}
    }

  if (best_s != NULL)
    return(best_s);

  s = NULL;
  ps = find_pc_sect_psymtab (pc, section);
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

/* Find the symtab associated with PC.  Look through the psymtabs and
   read in another symtab if necessary.  Backward compatibility, no section */

struct symtab *
find_pc_symtab (pc)
     CORE_ADDR pc;
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

/* Find the source file and line number for a given PC value and section.
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
find_pc_sect_line (pc, section, notcurrent)
     CORE_ADDR pc;
     struct sec *section;
     int notcurrent;
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

  INIT_SAL (&val);	/* initialize to zeroes */

  if (notcurrent)
    pc -= 1;

 /* elz: added this because this function returned the wrong
     information if the pc belongs to a stub (import/export)
     to call a shlib function. This stub would be anywhere between
     two functions in the target, and the line info was erroneously 
     taken to be the one of the line before the pc. 
  */
  /* RT: Further explanation:
   *
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
  msymbol = lookup_minimal_symbol_by_pc(pc);
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
        /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_NAME(msymbol)) */;
        /* fall through */
     else if (SYMBOL_VALUE(mfunsym) == SYMBOL_VALUE(msymbol))
        /* Avoid infinite recursion */
        /* See above comment about why warning is commented out */
        /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_NAME(msymbol)) */;
        /* fall through */
     else
       return find_pc_line( SYMBOL_VALUE (mfunsym), 0);
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
	  val.pc = pc;
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
  val.section = section;
  return val;
}

/* Backward compatibility (no section) */

struct symtab_and_line
find_pc_line (pc, notcurrent)
     CORE_ADDR pc;
     int notcurrent;
{
  asection     *section;

  section = find_pc_overlay (pc);
  if (pc_in_unmapped_range (pc, section))
    pc = overlay_mapped_address (pc, section);
  return find_pc_sect_line (pc, section, notcurrent);
}


static struct symtab* find_line_symtab PARAMS ((struct symtab *, int,
						int *, int *));

/* Find line number LINE in any symtab whose name is the same as
   SYMTAB.

   If found, return the symtab that contains the linetable in which it was
   found, set *INDEX to the index in the linetable of the best entry
   found, and set *EXACT_MATCH nonzero if the value returned is an
   exact match.

   If not found, return NULL.  */

static struct symtab*
find_line_symtab (symtab, line, index, exact_match)
     struct symtab *symtab;
     int line;
     int *index;
     int *exact_match;
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
find_line_pc (symtab, line, pc)
     struct symtab *symtab;
     int line;
     CORE_ADDR *pc;
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
find_line_pc_range (sal, startptr, endptr)
     struct symtab_and_line sal;
     CORE_ADDR *startptr, *endptr;
{
  CORE_ADDR startaddr;
  struct symtab_and_line found_sal;

  startaddr = sal.pc;
  if (startaddr==0 && !find_line_pc (sal.symtab, sal.line, &startaddr))
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
  fixup_symbol_section (sym, NULL);
  if (funfirstline)
    { /* skip "first line" of function (which is actually its prologue) */
      asection *section = SYMBOL_BFD_SECTION (sym);
      /* If function is in an unmapped overlay, use its unmapped LMA
	 address, so that SKIP_PROLOGUE has something unique to work on */
      if (section_is_overlay (section) &&
	  !section_is_mapped (section))
	pc = overlay_unmapped_address (pc, section);

      pc += FUNCTION_START_OFFSET;
      SKIP_PROLOGUE (pc);

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
  if (TYPE_CPLUS_SPECIFIC (type) == NULL)
    return 0;
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

static int
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

      /* Loop over each method name.  At this level, all overloads of a name
	 are counted as a single name.  There is an inner loop which loops over
	 each overload.  */

      for (method_counter = TYPE_NFN_FIELDS (t) - 1;
	   method_counter >= 0;
	   --method_counter)
	{
	  int field_counter;
	  char *method_name = TYPE_FN_FIELDLIST_NAME (t, method_counter);
	  char dem_opname[64];

          if (strncmp (method_name, "__", 2) == 0 ||
	      strncmp (method_name, "op", 2) == 0 ||
	      strncmp (method_name, "type", 4) == 0)
            {
	      if (cplus_demangle_opname (method_name, dem_opname, DMGL_ANSI))
	        method_name = dem_opname;
	      else if (cplus_demangle_opname (method_name, dem_opname, 0))
	        method_name = dem_opname; 
            }

	  if (STREQ (name, method_name))
	    /* Find all the overloaded methods with that name.  */
	    for (field_counter = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
		 field_counter >= 0;
		 --field_counter)
	      {
		struct fn_field *f;
		char *phys_name;

		f = TYPE_FN_FIELDLIST1 (t, method_counter);

		if (TYPE_FN_FIELD_STUB (f, field_counter))
		  {
		    char *tmp_name;

		    tmp_name = gdb_mangle_name (t,
						 method_counter,
						 field_counter);
		    phys_name = alloca (strlen (tmp_name) + 1);
		    strcpy (phys_name, tmp_name);
		    free (tmp_name);
		  }
		else
		  phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);

		/* Destructor is handled by caller, dont add it to the list */
		if (DESTRUCTOR_PREFIX_P (phys_name))
		  continue;

		sym_arr[i1] = lookup_symbol (phys_name,
					     NULL, VAR_NAMESPACE,
					     (int *) NULL,
					     (struct symtab **) NULL);
		if (sym_arr[i1])
		  i1++;
		else
		  {
		    /* This error message gets printed, but the method
		       still seems to be found
		       fputs_filtered("(Cannot find method ", gdb_stdout);
		       fprintf_symbol_filtered (gdb_stdout, phys_name,
		       language_cplus,
		       DMGL_PARAMS | DMGL_ANSI);
		       fputs_filtered(" - possibly inlined.)\n", gdb_stdout);
		       */
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

  if (i1 == 0)
    for (ibase = 0; ibase < TYPE_N_BASECLASSES (t); ibase++)
      i1 += find_methods (TYPE_BASECLASS (t, ibase), name, sym_arr + i1);

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
  char *q, *pp, *ii, *p2;
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
  int is_quoted;
  int has_parens;  
  int has_if = 0;
  struct symbol **sym_arr;
  struct type *t;
  char *saved_arg = *argptr;
  extern char *gdb_completer_quote_characters;
  
  INIT_SAL (&val);	/* initialize to zeroes */

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
      values.sals[0].section = find_pc_overlay (pc);

      return values;
    }

  /* 'has_if' is for the syntax:
   *     (gdb) break foo if (a==b)
   */
  if ((ii = strstr(*argptr, " if ")) != NULL ||
      (ii = strstr(*argptr, "\tif ")) != NULL ||
      (ii = strstr(*argptr, " if\t")) != NULL ||
      (ii = strstr(*argptr, "\tif\t")) != NULL ||
      (ii = strstr(*argptr, " if(")) != NULL ||
      (ii = strstr(*argptr, "\tif( ")) != NULL) 
    has_if = 1;
  /* Temporarily zap out "if (condition)" to not
   * confuse the parenthesis-checking code below.
   * This is undone below. Do not change ii!!
   */
  if (has_if) {
    *ii = '\0';
  }

  /* Set various flags.
   * 'has_parens' is important for overload checking, where
   * we allow things like: 
   *     (gdb) break c::f(int)
   */

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */

  is_quoted = (**argptr
	       && strchr (gdb_completer_quote_characters, **argptr) != NULL);

  has_parens = ((pp = strchr (*argptr, '(')) != NULL
		 && (pp = strchr (pp, ')')) != NULL);

  /* Now that we're safely past the has_parens check,
   * put back " if (condition)" so outer layers can see it 
   */
  if (has_if)
    *ii = ' ';

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */
  /* May also be CLASS::MEMBER, or NAMESPACE::NAME */
  /* Look for ':', but ignore inside of <> */

  s = NULL;
  for (p = *argptr; *p; p++)
    {
      if (p[0] == '<') 
	{
          char * temp_end = find_template_name_end (p);
          if (!temp_end)
            error ("malformed template specification in command");
          p = temp_end;
	}
      if (p[0] == ':' || p[0] == ' ' || p[0] == '\t' || !*p)
	break;
      if (p[0] == '.' && strchr (p, ':') == NULL) /* Java qualified method. */
	{
	  /* Find the *last* '.', since the others are package qualifiers. */
	  for (p1 = p;  *p1;  p1++)
	    {
	      if (*p1 == '.')
		p = p1;
	    }
	  break;
	}
    }
  while (p[0] == ' ' || p[0] == '\t') p++;

  if ((p[0] == ':' || p[0] == '.') && !has_parens)
    {
      /*  C++ */
      /*  ... or Java */
      if (is_quoted) *argptr = *argptr+1;
      if (p[0] == '.' || p[1] ==':')
	{
          int ix;
          char * saved_arg2 = *argptr;
          char * temp_end;
          /* First check for "global" namespace specification,
             of the form "::foo". If found, skip over the colons
             and jump to normal symbol processing */
          if ((*argptr == p) || (p[-1] == ' ') || (p[-1] == '\t'))
            saved_arg2 += 2;

          /* We have what looks like a class or namespace
             scope specification (A::B), possibly with many
             levels of namespaces or classes (A::B::C::D).

             Some versions of the HP ANSI C++ compiler (as also possibly
             other compilers) generate class/function/member names with
             embedded double-colons if they are inside namespaces. To
             handle this, we loop a few times, considering larger and
             larger prefixes of the string as though they were single
             symbols.  So, if the initially supplied string is
             A::B::C::D::foo, we have to look up "A", then "A::B",
             then "A::B::C", then "A::B::C::D", and finally
             "A::B::C::D::foo" as single, monolithic symbols, because
             A, B, C or D may be namespaces.

             Note that namespaces can nest only inside other
             namespaces, and not inside classes.  So we need only
             consider *prefixes* of the string; there is no need to look up
             "B::C" separately as a symbol in the previous example. */
          
          p2 = p; /* save for restart */
          while (1)
            {
  	     /* Extract the class name.  */
	     p1 = p;
	     while (p != *argptr && p[-1] == ' ') --p;
	     copy = (char *) alloca (p - *argptr + 1);
	     memcpy (copy, *argptr, p - *argptr);
	     copy[p - *argptr] = 0;

	     /* Discard the class name from the arg.  */
	     p = p1 + (p1[0] == ':' ? 2 : 1);
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

	         if (destructor_name_p (copy, t))
		   {
		     /* Destructors are a special case.  */
		     int m_index, f_index;

		     if (get_destructor_fn_field (t, &m_index, &f_index))
		       {
		         struct fn_field *f = TYPE_FN_FIELDLIST1 (t, m_index);

		         sym_arr[i1] =
			   lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, f_index),
				          NULL, VAR_NAMESPACE, (int *) NULL,
				          (struct symtab **)NULL);
		         if (sym_arr[i1])
			   i1++;
		       }
		   }
	         else
		   i1 = find_methods (t, copy, sym_arr);
	         if (i1 == 1)
		   {
		     /* There is exactly one field with that name.  */
		     sym = sym_arr[0];

		     if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
		       {
		         values.sals = (struct symtab_and_line *)
			   xmalloc (sizeof (struct symtab_and_line));
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

             /* Move pointer up to next possible class/namespace token */
              p = p2 + 1; /* restart with old value +1 */
              /* Move pointer ahead to next double-colon */
              while (*p && (p[0] != ' ') && (p[0] != '\t') && (p[0] != '\'')) {
                if (p[0] == '<') {
                  temp_end = find_template_name_end (p);
                  if (!temp_end)
                    error ("malformed template specification in command");
                  p = temp_end;
                }
                else if ((p[0] == ':') && (p[1] == ':'))
                  break; /* found double-colon */
                else
                  p++;
              }
              
              if (*p != ':')
                break; /* out of the while (1) */

              p2 = p; /* save restart for next time around */
              *argptr = saved_arg2; /* restore argptr */
            } /* while (1) */

          /* Last chance attempt -- check entire name as a symbol */
          /* Use "copy" in preparation for jumping out of this block,
             to be consistent with usage following the jump target */
          copy = (char *) alloca (p - saved_arg2 + 1);
          memcpy (copy, saved_arg2, p - saved_arg2);
          /* Note: if is_quoted should be true, we snuff out quote here anyway */
          copy[p-saved_arg2] = '\000'; 
          /* Set argptr to skip over the name */
          *argptr = (*p == '\'') ? p + 1 : p;
          /* Look up entire name */
          sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab);
          s = (struct symtab *) 0;
          /* Prepare to jump: restore the " if (condition)" so outer layers see it */
          if (has_if)
            *ii = ' ';
          /* Symbol was found --> jump to normal symbol processing.
             Code following "symbol_found" expects "copy" to have the
             symbol name, "sym" to have the symbol pointer, "s" to be
             a specified file's symtab, and sym_symtab to be the symbol's
             symtab. */
          /* By jumping there we avoid falling through the FILE:LINE and
             FILE:FUNC processing stuff below */
          if (sym)
            goto symbol_found;

          /* Couldn't find any interpretation as classes/namespaces, so give up */
          error_begin ();
          /* The quotes are important if copy is empty.  */
          printf_filtered
            ("Can't find member of namespace, class, struct, or union named \"%s\"\n", copy);
          cplusplus_hint (saved_arg);
          return_to_top_level (RETURN_ERROR);
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
  else {
    /* Check if what we have till now is a symbol name */

    /* We may be looking at a template instantiation such
       as "foo<int>".  Check here whether we know about it,
       instead of falling through to the code below which
       handles ordinary function names, because that code
       doesn't like seeing '<' and '>' in a name -- the
       skip_quoted call doesn't go past them.  So see if we
       can figure it out right now. */ 

    copy = (char *) alloca (p - *argptr + 1);
    memcpy (copy, *argptr, p - *argptr);
    copy[p - *argptr] = '\000';
    sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab);
    if (sym) {
      /* Yes, we have a symbol; jump to symbol processing */
      /* Code after symbol_found expects S, SYM_SYMTAB, SYM, 
         and COPY to be set correctly */ 
      if (has_if)
        *ii = ' ';
      *argptr = (*p == '\'') ? p + 1 : p;
      s = (struct symtab *) 0;
      goto symbol_found;
    }
    /* Otherwise fall out from here and go to file/line spec
       processing, etc. */ 
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

      /* It is possible that this source file has more than one symtab, 
	 and that the new line number specification has moved us from the
	 default (in s) to a new one.  */
      val.symtab = find_line_symtab (s, val.line, NULL, NULL);
      if (val.symtab == 0)
	val.symtab = s;
     
      val.pc = 0;
      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = val;
      values.nelts = 1;
      if (need_canonical)
	build_canonical_line_spec (values.sals, NULL, canonical);
      return values;
    }

  /* Arg token is not digits => try it as a variable name
     Find the next token (everything up to end or next whitespace).  */

  if (**argptr == '$')		/* May be a convenience variable */
    p = skip_quoted (*argptr + (((*argptr)[1] == '$') ? 2 : 1));  /* One or two $ chars possible */
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

  /* If it starts with $: may be a legitimate variable or routine name
     (e.g. HP-UX millicode routines such as $$dyncall), or it may
     be history value, or it may be a convenience variable */ 

  if (*copy == '$')
    {
      value_ptr valx;
      int index = 0;
      int need_canonical = 0;

      p = (copy[1] == '$') ? copy + 2 : copy + 1;
      while (*p >= '0' && *p <= '9')
        p++;
      if (!*p) /* reached end of token without hitting non-digit */
        {
          /* We have a value history reference */
          sscanf ((copy[1] == '$') ? copy + 2 : copy + 1, "%d", &index);
          valx = access_value_history ((copy[1] == '$') ? -index : index);
          if (TYPE_CODE (VALUE_TYPE (valx)) != TYPE_CODE_INT)
            error ("History values used in line specs must have integer values.");
        }
      else 
        {
          /* Not all digits -- may be user variable/function or a
              convenience variable */
          
          /* Look up entire name as a symbol first */
          sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab); 
          s = (struct symtab *) 0;
          need_canonical = 1;
          /* Symbol was found --> jump to normal symbol processing.
             Code following "symbol_found" expects "copy" to have the
             symbol name, "sym" to have the symbol pointer, "s" to be
             a specified file's symtab, and sym_symtab to be the symbol's
             symtab. */
          if (sym)
            goto symbol_found;

          /* If symbol was not found, look in minimal symbol tables */ 
          msymbol = lookup_minimal_symbol (copy, 0, 0);
          /* Min symbol was found --> jump to minsym processing. */ 
          if (msymbol)
            goto minimal_symbol_found;
          
          /* Not a user variable or function -- must be convenience variable */
          need_canonical = (s == 0) ? 1 : 0;
          valx = value_of_internalvar (lookup_internalvar (copy + 1));
          if (TYPE_CODE (VALUE_TYPE (valx)) != TYPE_CODE_INT)
            error ("Convenience variables used in line specs must have integer values.");
        }

      /* Either history value or convenience value from above, in valx */ 
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
  
symbol_found:   /* We also jump here from inside the C++ class/namespace 
                   code on finding a symbol of the form "A::B::C" */

  if (sym != NULL)
    {
      if (SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  /* Arg is the name of a function */
	  values.sals = (struct symtab_and_line *)
	    xmalloc (sizeof (struct symtab_and_line));
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

minimal_symbol_found: /* We also jump here from the case for variables
                         that begin with '$' */
  
  if (msymbol != NULL)
    {
      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = find_pc_sect_line ( SYMBOL_VALUE_ADDRESS (msymbol), 
					   (struct sec *)0,0 );
      values.sals[0].section = SYMBOL_BFD_SECTION (msymbol);
      if (funfirstline)
	{
	  values.sals[0].pc += FUNCTION_START_OFFSET;
	  SKIP_PROLOGUE (values.sals[0].pc);
	}
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

  values.sals = (struct symtab_and_line *) 
    alloca (nelts * sizeof(struct symtab_and_line));
  return_values.sals = (struct symtab_and_line *) 
    xmalloc (nelts * sizeof(struct symtab_and_line));
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
      INIT_SAL (&return_values.sals[i]);	/* initialize to zeroes */
      INIT_SAL (&values.sals[i]);
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
      prompt = "> ";
    }
  args = command_line_input (prompt, 0, "overload-choice");
  
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

      if (num >= nelts + 2)
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

static int
file_matches (file, files, nfiles)
     char *file;
     char *files[];
     int nfiles;
{
  int i;

  if (file != NULL && nfiles != 0)
    {
      for (i = 0; i < nfiles; i++)
        {
          if (strcmp (files[i], basename (file)) == 0)
            return 1;
        }
    }
  else if (nfiles == 0)
    return 1;
  return 0;
}

/* Free any memory associated with a search. */
void
free_search_symbols (symbols)
     struct symbol_search *symbols;
{
  struct symbol_search *p;
  struct symbol_search *next;

  for (p = symbols; p != NULL; p = next)
    {
      next = p->next;
      free (p);
    }
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
*/
void
search_symbols (regexp, kind, nfiles, files, matches)
     char *regexp;
     namespace_enum kind;
     int nfiles;
     char *files[];
     struct symbol_search **matches;
     
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
    = {mst_data, mst_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types2[]
    = {mst_bss,  mst_file_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types3[]
    = {mst_file_data,  mst_solib_trampoline, mst_abs, mst_unknown};
  static enum minimal_symbol_type types4[]
    = {mst_file_bss,   mst_text, mst_abs, mst_unknown};
  enum minimal_symbol_type ourtype;
  enum minimal_symbol_type ourtype2;
  enum minimal_symbol_type ourtype3;
  enum minimal_symbol_type ourtype4;
  struct symbol_search *sr;
  struct symbol_search *psr;
  struct symbol_search *tail;
  struct cleanup *old_chain = NULL;

  if (kind < LABEL_NAMESPACE)
    error ("must search on specific namespace");

  ourtype = types[(int) (kind - LABEL_NAMESPACE)];
  ourtype2 = types2[(int) (kind - LABEL_NAMESPACE)];
  ourtype3 = types3[(int) (kind - LABEL_NAMESPACE)];
  ourtype4 = types4[(int) (kind - LABEL_NAMESPACE)];

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
              if (file_matches (ps->filename, files, nfiles)
                  && ((regexp == NULL || SYMBOL_MATCHES_REGEXP (*psym))
                      && ((kind == VARIABLES_NAMESPACE && SYMBOL_CLASS (*psym) != LOC_TYPEDEF
                           && SYMBOL_CLASS (*psym) != LOC_BLOCK)
                          || (kind == FUNCTIONS_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_BLOCK)
                          || (kind == TYPES_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_TYPEDEF)
                          || (kind == METHODS_NAMESPACE && SYMBOL_CLASS (*psym) == LOC_BLOCK))))
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
            b = BLOCKVECTOR_BLOCK (bv, i);
            /* Skip the sort if this block is always sorted.  */
            if (!BLOCK_SHOULD_SORT (b))
              sort_block_syms (b);
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
                      {
                        sr = psr;
                        old_chain = make_cleanup ((make_cleanup_func) 
                                                  free_search_symbols, sr);
                      }
                    else
                      tail->next = psr;
                    tail = psr;
                  }
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
                          psr->symtab  = NULL;
                          psr->symbol  = NULL;
                          psr->next = NULL;
                          if (tail == NULL)
                            {
                              sr = psr;
                              old_chain = make_cleanup ((make_cleanup_func) 
                                                      free_search_symbols, &sr);
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
print_symbol_info (kind, s, sym, block, last)
     namespace_enum kind;
     struct symtab *s;
     struct symbol *sym;
     int block;
     char *last;
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
    c_typedef_print (SYMBOL_TYPE(sym), sym, gdb_stdout);
  /* variable, func, or typedef-that-is-c++-class */
  else if (kind < TYPES_NAMESPACE || 
           (kind == TYPES_NAMESPACE && 
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
# if 0
      /* Tiemann says: "info methods was never implemented."  */
      char *demangled_name;
      c_type_print_base (TYPE_FN_FIELD_TYPE(t, block),
                         gdb_stdout, 0, 0); 
      c_type_print_varspec_prefix (TYPE_FN_FIELD_TYPE(t, block),
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
          free (demangled_name);
        }
# endif
    }
}

/* This help function for symtab_symbol_info() prints information
   for non-debugging symbols to gdb_stdout.
*/
static void
print_msymbol_info (msymbol)
     struct minimal_symbol *msymbol;
{
  printf_filtered ("	%08lx  %s\n",
                   (unsigned long) SYMBOL_VALUE_ADDRESS (msymbol),
                   SYMBOL_SOURCE_NAME (msymbol));
}

/* This is the guts of the commands "info functions", "info types", and
   "info variables". It calls search_symbols to find all matches and then
   print_[m]symbol_info to print out some useful information about the
   matches.
*/
static void
symtab_symbol_info (regexp, kind, from_tty)
     char *regexp;
     namespace_enum kind;
     int   from_tty;
{
  static char *classnames[]
    = {"variable", "function", "type", "method"};
  struct symbol_search *symbols;
  struct symbol_search *p;
  struct cleanup *old_chain;
  char *last_filename = NULL;
  int first = 1;

  /* must make sure that if we're interrupted, symbols gets freed */
  search_symbols (regexp, kind, 0, (char **) NULL, &symbols);
  old_chain = make_cleanup ((make_cleanup_func) free_search_symbols, symbols);

  printf_filtered (regexp
                   ? "All %ss matching regular expression \"%s\":\n"
                   : "All defined %ss:\n",
                   classnames[(int) (kind - LABEL_NAMESPACE - 1)], regexp);

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
variables_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  symtab_symbol_info (regexp, VARIABLES_NAMESPACE, from_tty);
}

static void
functions_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  symtab_symbol_info (regexp, FUNCTIONS_NAMESPACE, from_tty);
}

static void
types_info (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  symtab_symbol_info (regexp, TYPES_NAMESPACE, from_tty);
}

#if 0
/* Tiemann says: "info methods was never implemented."  */
static void
methods_info (regexp)
     char *regexp;
{
  symtab_symbol_info (regexp, METHODS_NAMESPACE, 0, from_tty);
}
#endif /* 0 */

/* Breakpoint all functions matching regular expression. */
static void
rbreak_command (regexp, from_tty)
     char *regexp;
     int from_tty;
{
  struct symbol_search *ss;
  struct symbol_search *p;
  struct cleanup *old_chain;

  search_symbols (regexp, FUNCTIONS_NAMESPACE, 0, (char **) NULL, &ss);
  old_chain = make_cleanup ((make_cleanup_func) free_search_symbols, ss);

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

   If non-zero, func_start is where we think the prologue starts, possibly
   by previous examination of symbol table information.
 */

int
in_prologue (pc, func_start)
     CORE_ADDR pc;
     CORE_ADDR func_start;
{
#if 0
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  if (!find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    goto nosyms;		/* Might be in prologue */

  sal = find_pc_line (func_addr, 0);

  if (sal.line == 0)
    goto nosyms;

  /* sal.end is the address of the first instruction past sal.line. */
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
#endif

/* If func_start is zero (meaning unknown) then we don't know whether pc is
   in the prologue or not.  I.E. it might be. */

  if (!func_start) return 1;

/* We need to call the target-specific prologue skipping functions with the
   function's start address because PC may be pointing at an instruction that
   could be mistakenly considered part of the prologue.  */

  SKIP_PROLOGUE (func_start);

  return pc < func_start;
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
overload_list_add_symbol (sym, oload_name)
  struct symbol * sym;
  char * oload_name;
{
  int newsize;
  int i;

  /* Get the demangled name without parameters */
  char * sym_name = cplus_demangle (SYMBOL_NAME (sym), DMGL_ARM | DMGL_ANSI);
  if (!sym_name)
    {
      sym_name = (char *) xmalloc (strlen (SYMBOL_NAME (sym)) + 1);
      strcpy (sym_name, SYMBOL_NAME (sym));
    }

  /* skip symbols that cannot match */
  if (strcmp (sym_name, oload_name) != 0)
    return;

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
  
  free (sym_name);
}

/* Return a null-terminated list of pointers to function symbols that
 * match name of the supplied symbol FSYM.
 * This is used in finding all overloaded instances of a function name.
 * This has been modified from make_symbol_completion_list.  */


struct symbol **
make_symbol_overload_list (fsym)
  struct symbol * fsym;
{
  register struct symbol *sym;
  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct minimal_symbol *msymbol;
  register struct objfile *objfile;
  register struct block *b, *surrounding_static_block = 0;
  register int i, j;
  struct partial_symbol **psym;
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
     by matching OLOAD_NAME.  Add each one that you find to the list.  */

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
	  overload_list_add_symbol (*psym, oload_name);
	}
      
      for (psym = objfile->static_psymbols.list + ps->statics_offset;
	   psym < (objfile->static_psymbols.list + ps->statics_offset
		   + ps->n_static_syms);
	   psym++)
	{
	  QUIT;
	  overload_list_add_symbol (*psym, oload_name);
	}
    }

  /* At this point scan through the misc symbol vectors and add each
     symbol you find to the list.  Eventually we want to ignore
     anything that isn't a text symbol (everything else will be
     handled by the psymtab code above).  */

  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      overload_list_add_symbol (msymbol, oload_name);
    }

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */

  for (b = get_selected_block (); b != NULL; b = BLOCK_SUPERBLOCK (b))
    {
      if (!BLOCK_SUPERBLOCK (b))
	{
	  surrounding_static_block = b; 	/* For elimination of dups */
	}
      
      /* Also catch fields of types defined in this places which match our
	 text string.  Only complete on types visible from current context. */

      for (i = 0; i < BLOCK_NSYMS (b); i++)
	{
	  sym = BLOCK_SYM (b, i);
	  overload_list_add_symbol (sym, oload_name);
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
	  overload_list_add_symbol (sym, oload_name);
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
	  overload_list_add_symbol (sym, oload_name);
	}
    }

  free (oload_name);

  return (sym_return_val);
}

/* End of overload resolution functions */


void
_initialize_symtab ()
{
  add_info ("variables", variables_info,
	    "All global and static variable names, or those matching REGEXP.");
  if (dbx_commands)
    add_com("whereis", class_info, variables_info, 
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
