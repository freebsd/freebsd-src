/* Generic symbol file reading for the GNU debugger, GDB.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998
   Free Software Foundation, Inc.
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
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "frame.h"
#include "target.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "breakpoint.h"
#include "language.h"
#include "complaints.h"
#include "demangle.h"
#include "inferior.h" /* for write_pc */
#include "gdb-stabs.h"
#include "obstack.h"

#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef HPUXHPPA

/* Some HP-UX related globals to clear when a new "main"
   symbol file is loaded. HP-specific.  */

extern int hp_som_som_object_present;
extern int hp_cxx_exception_support_initialized;
#define RESET_HP_UX_GLOBALS() do {\
                                    hp_som_som_object_present = 0;             /* indicates HP-compiled code */        \
                                    hp_cxx_exception_support_initialized = 0;  /* must reinitialize exception stuff */ \
                              } while (0)
#endif

int (*ui_load_progress_hook) PARAMS ((char *, unsigned long));
void (*pre_add_symbol_hook) PARAMS ((char *));
void (*post_add_symbol_hook) PARAMS ((void));

/* Global variables owned by this file */
int readnow_symbol_files;		/* Read full symbols immediately */

struct complaint oldsyms_complaint = {
  "Replacing old symbols for `%s'", 0, 0
};

struct complaint empty_symtab_complaint = {
  "Empty symbol table found for `%s'", 0, 0
};

/* External variables and functions referenced. */

extern int info_verbose;

extern void report_transfer_performance PARAMS ((unsigned long,
						 time_t, time_t));

/* Functions this file defines */

#if 0
static int simple_read_overlay_region_table PARAMS ((void));
static void simple_free_overlay_region_table PARAMS ((void));
#endif

static void set_initial_language PARAMS ((void));

static void load_command PARAMS ((char *, int));

static void add_symbol_file_command PARAMS ((char *, int));

static void add_shared_symbol_files_command PARAMS ((char *, int));

static void cashier_psymtab PARAMS ((struct partial_symtab *));

static int compare_psymbols PARAMS ((const void *, const void *));

static int compare_symbols PARAMS ((const void *, const void *));

bfd *symfile_bfd_open PARAMS ((char *));

static void find_sym_fns PARAMS ((struct objfile *));

static void decrement_reading_symtab PARAMS ((void *));

static void overlay_invalidate_all PARAMS ((void));

static int overlay_is_mapped PARAMS ((struct obj_section *));

void list_overlays_command PARAMS ((char *, int));

void map_overlay_command PARAMS ((char *, int));

void unmap_overlay_command PARAMS ((char *, int));

static void overlay_auto_command PARAMS ((char *, int));

static void overlay_manual_command PARAMS ((char *, int));

static void overlay_off_command PARAMS ((char *, int));

static void overlay_load_command PARAMS ((char *, int));

static void overlay_command PARAMS ((char *, int));

static void simple_free_overlay_table PARAMS ((void));

static void read_target_long_array PARAMS ((CORE_ADDR, unsigned int *, int));

static int simple_read_overlay_table PARAMS ((void));

static int simple_overlay_update_1 PARAMS ((struct obj_section *));

void _initialize_symfile PARAMS ((void));

/* List of all available sym_fns.  On gdb startup, each object file reader
   calls add_symtab_fns() to register information on each format it is
   prepared to read. */

static struct sym_fns *symtab_fns = NULL;

/* Flag for whether user will be reloading symbols multiple times.
   Defaults to ON for VxWorks, otherwise OFF.  */

#ifdef SYMBOL_RELOADING_DEFAULT
int symbol_reloading = SYMBOL_RELOADING_DEFAULT;
#else
int symbol_reloading = 0;
#endif

/* If non-zero, then on HP-UX (i.e., platforms that use somsolib.c),
   this variable is interpreted as a threshhold.  If adding a new
   library's symbol table to those already known to the debugger would
   exceed this threshhold, then the shlib's symbols are not added.

   If non-zero on other platforms, shared library symbols will be added
   automatically when the inferior is created, new libraries are loaded,
   or when attaching to the inferior.  This is almost always what users
   will want to have happen; but for very large programs, the startup
   time will be excessive, and so if this is a problem, the user can
   clear this flag and then add the shared library symbols as needed.
   Note that there is a potential for confusion, since if the shared
   library symbols are not loaded, commands like "info fun" will *not*
   report all the functions that are actually present. 

   Note that HP-UX interprets this variable to mean, "threshhold size
   in megabytes, where zero means never add".  Other platforms interpret
   this variable to mean, "always add if non-zero, never add if zero."
   */

int auto_solib_add = 1;


/* Since this function is called from within qsort, in an ANSI environment
   it must conform to the prototype for qsort, which specifies that the
   comparison function takes two "void *" pointers. */

static int
compare_symbols (s1p, s2p)
     const PTR s1p;
     const PTR s2p;
{
  register struct symbol **s1, **s2;

  s1 = (struct symbol **) s1p;
  s2 = (struct symbol **) s2p;

  return (STRCMP (SYMBOL_NAME (*s1), SYMBOL_NAME (*s2)));
}

/*

LOCAL FUNCTION

	compare_psymbols -- compare two partial symbols by name

DESCRIPTION

	Given pointers to pointers to two partial symbol table entries,
	compare them by name and return -N, 0, or +N (ala strcmp).
	Typically used by sorting routines like qsort().

NOTES

	Does direct compare of first two characters before punting
	and passing to strcmp for longer compares.  Note that the
	original version had a bug whereby two null strings or two
	identically named one character strings would return the
	comparison of memory following the null byte.

 */

static int
compare_psymbols (s1p, s2p)
     const PTR s1p;
     const PTR s2p;
{
  register char *st1 = SYMBOL_NAME (*(struct partial_symbol **) s1p);
  register char *st2 = SYMBOL_NAME (*(struct partial_symbol **) s2p);

  if ((st1[0] - st2[0]) || !st1[0])
    {
      return (st1[0] - st2[0]);
    }
  else if ((st1[1] - st2[1]) || !st1[1])
    {
      return (st1[1] - st2[1]);
    }
  else
    {
      /* Note: I replaced the STRCMP line (commented out below)
       * with a simpler "strcmp()" which compares the 2 strings
       * from the beginning. (STRCMP is a macro which first compares
       * the initial characters, then falls back on strcmp).
       * The reason is that the STRCMP line was tickling a C compiler
       * bug on HP-UX 10.30, which is avoided with the simpler
       * code. The performance gain from the more complicated code
       * is negligible, given that we have already checked the
       * initial 2 characters above. I reported the compiler bug,
       * and once it is fixed the original line can be put back. RT
       */
      /* return ( STRCMP (st1 + 2, st2 + 2)); */
      return ( strcmp (st1, st2));
    }
}

void
sort_pst_symbols (pst)
     struct partial_symtab *pst;
{
  /* Sort the global list; don't sort the static list */

  qsort (pst -> objfile -> global_psymbols.list + pst -> globals_offset,
	 pst -> n_global_syms, sizeof (struct partial_symbol *),
	 compare_psymbols);
}

/* Call sort_block_syms to sort alphabetically the symbols of one block.  */

void
sort_block_syms (b)
     register struct block *b;
{
  qsort (&BLOCK_SYM (b, 0), BLOCK_NSYMS (b),
	 sizeof (struct symbol *), compare_symbols);
}

/* Call sort_symtab_syms to sort alphabetically
   the symbols of each block of one symtab.  */

void
sort_symtab_syms (s)
     register struct symtab *s;
{
  register struct blockvector *bv;
  int nbl;
  int i;
  register struct block *b;

  if (s == 0)
    return;
  bv = BLOCKVECTOR (s);
  nbl = BLOCKVECTOR_NBLOCKS (bv);
  for (i = 0; i < nbl; i++)
    {
      b = BLOCKVECTOR_BLOCK (bv, i);
      if (BLOCK_SHOULD_SORT (b))
	sort_block_syms (b);
    }
}

/* Make a null terminated copy of the string at PTR with SIZE characters in
   the obstack pointed to by OBSTACKP .  Returns the address of the copy.
   Note that the string at PTR does not have to be null terminated, I.E. it
   may be part of a larger string and we are only saving a substring. */

char *
obsavestring (ptr, size, obstackp)
     char *ptr;
     int size;
     struct obstack *obstackp;
{
  register char *p = (char *) obstack_alloc (obstackp, size + 1);
  /* Open-coded memcpy--saves function call time.  These strings are usually
     short.  FIXME: Is this really still true with a compiler that can
     inline memcpy? */
  {
    register char *p1 = ptr;
    register char *p2 = p;
    char *end = ptr + size;
    while (p1 != end)
      *p2++ = *p1++;
  }
  p[size] = 0;
  return p;
}

/* Concatenate strings S1, S2 and S3; return the new string.  Space is found
   in the obstack pointed to by OBSTACKP.  */

char *
obconcat (obstackp, s1, s2, s3)
     struct obstack *obstackp;
     const char *s1, *s2, *s3;
{
  register int len = strlen (s1) + strlen (s2) + strlen (s3) + 1;
  register char *val = (char *) obstack_alloc (obstackp, len);
  strcpy (val, s1);
  strcat (val, s2);
  strcat (val, s3);
  return val;
}

/* True if we are nested inside psymtab_to_symtab. */

int currently_reading_symtab = 0;

static void
decrement_reading_symtab (dummy)
     void *dummy;
{
  currently_reading_symtab--;
}

/* Get the symbol table that corresponds to a partial_symtab.
   This is fast after the first time you do it.  In fact, there
   is an even faster macro PSYMTAB_TO_SYMTAB that does the fast
   case inline.  */

struct symtab *
psymtab_to_symtab (pst)
     register struct partial_symtab *pst;
{
  /* If it's been looked up before, return it. */
  if (pst->symtab)
    return pst->symtab;

  /* If it has not yet been read in, read it.  */
  if (!pst->readin)
    { 
      struct cleanup *back_to = make_cleanup (decrement_reading_symtab, NULL);
      currently_reading_symtab++;
      (*pst->read_symtab) (pst);
      do_cleanups (back_to);
    }

  return pst->symtab;
}

/* Initialize entry point information for this objfile. */

void
init_entry_point_info (objfile)
     struct objfile *objfile;
{
  /* Save startup file's range of PC addresses to help blockframe.c
     decide where the bottom of the stack is.  */

  if (bfd_get_file_flags (objfile -> obfd) & EXEC_P)
    {
      /* Executable file -- record its entry point so we'll recognize
	 the startup file because it contains the entry point.  */
      objfile -> ei.entry_point = bfd_get_start_address (objfile -> obfd);
    }
  else
    {
      /* Examination of non-executable.o files.  Short-circuit this stuff.  */
      objfile -> ei.entry_point = INVALID_ENTRY_POINT;
    }
  objfile -> ei.entry_file_lowpc = INVALID_ENTRY_LOWPC;
  objfile -> ei.entry_file_highpc = INVALID_ENTRY_HIGHPC;
  objfile -> ei.entry_func_lowpc = INVALID_ENTRY_LOWPC;
  objfile -> ei.entry_func_highpc = INVALID_ENTRY_HIGHPC;
  objfile -> ei.main_func_lowpc = INVALID_ENTRY_LOWPC;
  objfile -> ei.main_func_highpc = INVALID_ENTRY_HIGHPC;
}

/* Get current entry point address.  */

CORE_ADDR
entry_point_address()
{
  return symfile_objfile ? symfile_objfile->ei.entry_point : 0;
}

/* Remember the lowest-addressed loadable section we've seen.  
   This function is called via bfd_map_over_sections. 

   In case of equal vmas, the section with the largest size becomes the
   lowest-addressed loadable section.

   If the vmas and sizes are equal, the last section is considered the
   lowest-addressed loadable section.  */

void
find_lowest_section (abfd, sect, obj)
     bfd *abfd;
     asection *sect;
     PTR obj;
{
  asection **lowest = (asection **)obj;

  if (0 == (bfd_get_section_flags (abfd, sect) & SEC_LOAD))
    return;
  if (!*lowest)
    *lowest = sect;		/* First loadable section */
  else if (bfd_section_vma (abfd, *lowest) > bfd_section_vma (abfd, sect))
    *lowest = sect;		/* A lower loadable section */
  else if (bfd_section_vma (abfd, *lowest) == bfd_section_vma (abfd, sect)
	   && (bfd_section_size (abfd, (*lowest))
	       <= bfd_section_size (abfd, sect)))
    *lowest = sect;
}

/* Parse the user's idea of an offset for dynamic linking, into our idea
   of how to represent it for fast symbol reading.  This is the default 
   version of the sym_fns.sym_offsets function for symbol readers that
   don't need to do anything special.  It allocates a section_offsets table
   for the objectfile OBJFILE and stuffs ADDR into all of the offsets.  */

struct section_offsets *
default_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;

  objfile->num_sections = SECT_OFF_MAX;
  section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack, SIZEOF_SECTION_OFFSETS);

  for (i = 0; i < SECT_OFF_MAX; i++)
    ANOFFSET (section_offsets, i) = addr;
  
  return section_offsets;
}


/* Process a symbol file, as either the main file or as a dynamically
   loaded file.

   NAME is the file name (which will be tilde-expanded and made
   absolute herein) (but we don't free or modify NAME itself).
   FROM_TTY says how verbose to be.  MAINLINE specifies whether this
   is the main symbol file, or whether it's an extra symbol file such
   as dynamically loaded code.  If !mainline, ADDR is the address
   where the text segment was loaded.  If VERBO, the caller has printed
   a verbose message about the symbol reading (and complaints can be
   more terse about it).  */

void
syms_from_objfile (objfile, addr, mainline, verbo)
     struct objfile *objfile;
     CORE_ADDR addr;
     int mainline;
     int verbo;
{
  struct section_offsets *section_offsets;
  asection *lowest_sect;
  struct cleanup *old_chain;

  init_entry_point_info (objfile);
  find_sym_fns (objfile);

  /* Make sure that partially constructed symbol tables will be cleaned up
     if an error occurs during symbol reading.  */
  old_chain = make_cleanup ((make_cleanup_func) free_objfile, objfile);

  if (mainline) 
    {
      /* We will modify the main symbol table, make sure that all its users
	 will be cleaned up if an error occurs during symbol reading.  */
      make_cleanup ((make_cleanup_func) clear_symtab_users, 0);

      /* Since no error yet, throw away the old symbol table.  */

      if (symfile_objfile != NULL)
	{
	  free_objfile (symfile_objfile);
	  symfile_objfile = NULL;
	}

      /* Currently we keep symbols from the add-symbol-file command.
	 If the user wants to get rid of them, they should do "symbol-file"
	 without arguments first.  Not sure this is the best behavior
	 (PR 2207).  */

      (*objfile -> sf -> sym_new_init) (objfile);
    }

  /* Convert addr into an offset rather than an absolute address.
     We find the lowest address of a loaded segment in the objfile,
     and assume that <addr> is where that got loaded.  Due to historical
     precedent, we warn if that doesn't happen to be a text segment.  */

  if (mainline)
    {
      addr = 0;		/* No offset from objfile addresses.  */
    }
  else
    {
      lowest_sect = bfd_get_section_by_name (objfile->obfd, ".text");
      if (lowest_sect == NULL)
	bfd_map_over_sections (objfile->obfd, find_lowest_section,
			       (PTR) &lowest_sect);

      if (lowest_sect == NULL)
	warning ("no loadable sections found in added symbol-file %s",
		 objfile->name);
      else if ((bfd_get_section_flags (objfile->obfd, lowest_sect) & SEC_CODE)
	       == 0)
	/* FIXME-32x64--assumes bfd_vma fits in long.  */
	warning ("Lowest section in %s is %s at 0x%lx",
		 objfile->name,
		 bfd_section_name (objfile->obfd, lowest_sect),
		 (unsigned long) bfd_section_vma (objfile->obfd, lowest_sect));

      if (lowest_sect)
	addr -= bfd_section_vma (objfile->obfd, lowest_sect);
    }

  /* Initialize symbol reading routines for this objfile, allow complaints to
     appear for this new file, and record how verbose to be, then do the
     initial symbol reading for this file. */

  (*objfile -> sf -> sym_init) (objfile);
  clear_complaints (1, verbo);

  section_offsets = (*objfile -> sf -> sym_offsets) (objfile, addr);
  objfile->section_offsets = section_offsets;

#ifndef IBM6000_TARGET
  /* This is a SVR4/SunOS specific hack, I think.  In any event, it
     screws RS/6000.  sym_offsets should be doing this sort of thing,
     because it knows the mapping between bfd sections and
     section_offsets.  */
  /* This is a hack.  As far as I can tell, section offsets are not
     target dependent.  They are all set to addr with a couple of
     exceptions.  The exceptions are sysvr4 shared libraries, whose
     offsets are kept in solib structures anyway and rs6000 xcoff
     which handles shared libraries in a completely unique way.

     Section offsets are built similarly, except that they are built
     by adding addr in all cases because there is no clear mapping
     from section_offsets into actual sections.  Note that solib.c
     has a different algorythm for finding section offsets.

     These should probably all be collapsed into some target
     independent form of shared library support.  FIXME.  */

  if (addr)
    {
      struct obj_section *s;

      for (s = objfile->sections; s < objfile->sections_end; ++s)
	{
	  s->addr -= s->offset;
	  s->addr += addr;
	  s->endaddr -= s->offset;
	  s->endaddr += addr;
	  s->offset += addr;
	}
    }
#endif /* not IBM6000_TARGET */

  (*objfile -> sf -> sym_read) (objfile, section_offsets, mainline);

  if (!have_partial_symbols () && !have_full_symbols ())
    {
      wrap_here ("");
      printf_filtered ("(no debugging symbols found)...");
      wrap_here ("");
    }

  /* Don't allow char * to have a typename (else would get caddr_t).
     Ditto void *.  FIXME: Check whether this is now done by all the
     symbol readers themselves (many of them now do), and if so remove
     it from here.  */

  TYPE_NAME (lookup_pointer_type (builtin_type_char)) = 0;
  TYPE_NAME (lookup_pointer_type (builtin_type_void)) = 0;

  /* Mark the objfile has having had initial symbol read attempted.  Note
     that this does not mean we found any symbols... */

  objfile -> flags |= OBJF_SYMS;

  /* Discard cleanups as symbol reading was successful.  */

  discard_cleanups (old_chain);

/* Call this after reading in a new symbol table to give target dependant code
   a crack at the new symbols.  For instance, this could be used to update the
   values of target-specific symbols GDB needs to keep track of (such as
   _sigtramp, or whatever).  */

  TARGET_SYMFILE_POSTREAD (objfile);
}

/* Perform required actions after either reading in the initial
   symbols for a new objfile, or mapping in the symbols from a reusable
   objfile. */
   
void
new_symfile_objfile (objfile, mainline, verbo)
     struct objfile *objfile;
     int mainline;
     int verbo;
{

  /* If this is the main symbol file we have to clean up all users of the
     old main symbol file. Otherwise it is sufficient to fixup all the
     breakpoints that may have been redefined by this symbol file.  */
  if (mainline)
    {
      /* OK, make it the "real" symbol file.  */
      symfile_objfile = objfile;

      clear_symtab_users ();
    }
  else
    {
      breakpoint_re_set ();
    }

  /* We're done reading the symbol file; finish off complaints.  */
  clear_complaints (0, verbo);
}

/* Process a symbol file, as either the main file or as a dynamically
   loaded file.

   NAME is the file name (which will be tilde-expanded and made
   absolute herein) (but we don't free or modify NAME itself).
   FROM_TTY says how verbose to be.  MAINLINE specifies whether this
   is the main symbol file, or whether it's an extra symbol file such
   as dynamically loaded code.  If !mainline, ADDR is the address
   where the text segment was loaded.

   USER_LOADED is TRUE if the add-symbol-file command was how this
   symbol file came to be processed.

   IS_SOLIB is TRUE if this symbol file represents a solib, as discovered
   by the target's implementation of the solib package.

   Upon success, returns a pointer to the objfile that was added.
   Upon failure, jumps back to command level (never returns). */

struct objfile *
symbol_file_add (name, from_tty, addr, mainline, mapped, readnow, user_loaded, is_solib)
     char *name;
     int from_tty;
     CORE_ADDR addr;
     int mainline;
     int mapped;
     int readnow;
     int  user_loaded;
     int  is_solib;
{
  struct objfile *objfile;
  struct partial_symtab *psymtab;
  bfd *abfd;

  /* Open a bfd for the file, and give user a chance to burp if we'd be
     interactively wiping out any existing symbols.  */

  abfd = symfile_bfd_open (name);

  if ((have_full_symbols () || have_partial_symbols ())
      && mainline
      && from_tty
      && !query ("Load new symbol table from \"%s\"? ", name))
      error ("Not confirmed.");

  objfile = allocate_objfile (abfd, mapped, user_loaded, is_solib);

  /* If the objfile uses a mapped symbol file, and we have a psymtab for
     it, then skip reading any symbols at this time. */

  if ((objfile -> flags & OBJF_MAPPED) && (objfile -> flags & OBJF_SYMS))
    {
      /* We mapped in an existing symbol table file that already has had
	 initial symbol reading performed, so we can skip that part.  Notify
	 the user that instead of reading the symbols, they have been mapped.
	 */
      if (from_tty || info_verbose)
	{
	  printf_filtered ("Mapped symbols for %s...", name);
	  wrap_here ("");
	  gdb_flush (gdb_stdout);
	}
      init_entry_point_info (objfile);
      find_sym_fns (objfile);
    }
  else
    {
      /* We either created a new mapped symbol table, mapped an existing
	 symbol table file which has not had initial symbol reading
	 performed, or need to read an unmapped symbol table. */
      if (from_tty || info_verbose)
	{
	  if (pre_add_symbol_hook)
	    pre_add_symbol_hook (name);
	  else
	    {
	      printf_filtered ("Reading symbols from %s...", name);
	      wrap_here ("");
	      gdb_flush (gdb_stdout);
	    }
	}
      syms_from_objfile (objfile, addr, mainline, from_tty);
    }

  /* We now have at least a partial symbol table.  Check to see if the
     user requested that all symbols be read on initial access via either
     the gdb startup command line or on a per symbol file basis.  Expand
     all partial symbol tables for this objfile if so. */

  if (readnow || readnow_symbol_files)
    {
      if (from_tty || info_verbose)
	{
	  printf_filtered ("expanding to full symbols...");
	  wrap_here ("");
	  gdb_flush (gdb_stdout);
	}

      for (psymtab = objfile -> psymtabs;
	   psymtab != NULL;
	   psymtab = psymtab -> next)
	{
	  psymtab_to_symtab (psymtab);
	}
    }

  if (from_tty || info_verbose)
    {
      if (post_add_symbol_hook)
        post_add_symbol_hook ();
      else
        {
          printf_filtered ("done.\n");
          gdb_flush (gdb_stdout);
        }
    }

  new_symfile_objfile (objfile, mainline, from_tty);

  target_new_objfile (objfile);

  return (objfile);
}

/* This is the symbol-file command.  Read the file, analyze its
   symbols, and add a struct symtab to a symtab list.  The syntax of
   the command is rather bizarre--(1) buildargv implements various
   quoting conventions which are undocumented and have little or
   nothing in common with the way things are quoted (or not quoted)
   elsewhere in GDB, (2) options are used, which are not generally
   used in GDB (perhaps "set mapped on", "set readnow on" would be
   better), (3) the order of options matters, which is contrary to GNU
   conventions (because it is confusing and inconvenient).  */

void
symbol_file_command (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  char *name = NULL;
  CORE_ADDR text_relocation = 0;		/* text_relocation */
  struct cleanup *cleanups;
  int mapped = 0;
  int readnow = 0;

  dont_repeat ();

  if (args == NULL)
    {
      if ((have_full_symbols () || have_partial_symbols ())
	  && from_tty
	  && !query ("Discard symbol table from `%s'? ",
		     symfile_objfile -> name))
	error ("Not confirmed.");
      free_all_objfiles ();

      /* solib descriptors may have handles to objfiles.  Since their
         storage has just been released, we'd better wipe the solib
         descriptors as well.
         */
#if defined(SOLIB_RESTART)
      SOLIB_RESTART ();
#endif

      symfile_objfile = NULL;
      if (from_tty)
	{
	  printf_unfiltered ("No symbol file now.\n");
	}
#ifdef HPUXHPPA
      RESET_HP_UX_GLOBALS ();
#endif
    }
  else
    {
      if ((argv = buildargv (args)) == NULL)
	{
	  nomem (0);
	}
      cleanups = make_cleanup ((make_cleanup_func) freeargv, (char *) argv);
      while (*argv != NULL)
	{
	  if (STREQ (*argv, "-mapped"))
	    {
	      mapped = 1;
	    }
	  else if (STREQ (*argv, "-readnow"))
	    {
	      readnow = 1;
	    }
	  else if (**argv == '-')
	    {
	      error ("unknown option `%s'", *argv);
	    }
	  else
	    {
            char *p;

              name = *argv;

              /* this is for rombug remote only, to get the text relocation by
              using link command */
              p = strrchr(name, '/');
              if (p != NULL) p++;
              else p = name;

              target_link(p, &text_relocation);

              if (text_relocation == (CORE_ADDR)0)
                return;
              else if (text_relocation == (CORE_ADDR)-1)
                {
                  symbol_file_add (name, from_tty, (CORE_ADDR)0, 
                                   1, mapped, readnow, 1, 0);
#ifdef HPUXHPPA
                  RESET_HP_UX_GLOBALS ();
#endif
                }
              else
                symbol_file_add (name, from_tty, (CORE_ADDR)text_relocation,
				 0, mapped, readnow, 1, 0);

	      /* Getting new symbols may change our opinion about what is
		 frameless.  */
	      reinit_frame_cache ();

              set_initial_language ();
	    }
	  argv++;
	}

      if (name == NULL)
	{
	  error ("no symbol file name was specified");
	}
      TUIDO(((TuiOpaqueFuncPtr)tuiDisplayMainFunction));
      do_cleanups (cleanups);
    }
}

/* Set the initial language.

   A better solution would be to record the language in the psymtab when reading
   partial symbols, and then use it (if known) to set the language.  This would
   be a win for formats that encode the language in an easily discoverable place,
   such as DWARF.  For stabs, we can jump through hoops looking for specially
   named symbols or try to intuit the language from the specific type of stabs
   we find, but we can't do that until later when we read in full symbols.
   FIXME.  */

static void
set_initial_language ()
{
  struct partial_symtab *pst;
  enum language lang = language_unknown;  	

  pst = find_main_psymtab ();
  if (pst != NULL)
    {
      if (pst -> filename != NULL)
	{
	  lang = deduce_language_from_filename (pst -> filename);
        }
      if (lang == language_unknown)
	{
	    /* Make C the default language */
	    lang = language_c;
	}
      set_language (lang);
      expected_language = current_language;	/* Don't warn the user */
    }
}

/* Open file specified by NAME and hand it off to BFD for preliminary
   analysis.  Result is a newly initialized bfd *, which includes a newly
   malloc'd` copy of NAME (tilde-expanded and made absolute).
   In case of trouble, error() is called.  */

bfd *
symfile_bfd_open (name)
     char *name;
{
  bfd *sym_bfd;
  int desc;
  char *absolute_name;



  name = tilde_expand (name);	/* Returns 1st new malloc'd copy */

  /* Look down path for it, allocate 2nd new malloc'd copy.  */
  desc = openp (getenv ("PATH"), 1, name, O_RDONLY | O_BINARY, 0, &absolute_name);
#if defined(__GO32__) || defined(_WIN32)
  if (desc < 0)
    {
      char *exename = alloca (strlen (name) + 5);
      strcat (strcpy (exename, name), ".exe");
      desc = openp (getenv ("PATH"), 1, exename, O_RDONLY | O_BINARY,
                    0, &absolute_name);
    }
#endif
  if (desc < 0)
    {
      make_cleanup (free, name);
      perror_with_name (name);
    }
  fcntl (desc, F_SETFD, 1);
  free (name);			/* Free 1st new malloc'd copy */
  name = absolute_name;		/* Keep 2nd malloc'd copy in bfd */
				/* It'll be freed in free_objfile(). */

  sym_bfd = bfd_fdopenr (name, gnutarget, desc);
  if (!sym_bfd)
    {
      close (desc);
      make_cleanup (free, name);
      error ("\"%s\": can't open to read symbols: %s.", name,
	     bfd_errmsg (bfd_get_error ()));
    }
  sym_bfd->cacheable = true;

  if (!bfd_check_format (sym_bfd, bfd_object))
    {
      /* FIXME: should be checking for errors from bfd_close (for one thing,
	 on error it does not free all the storage associated with the
	 bfd).  */
      bfd_close (sym_bfd);	/* This also closes desc */
      make_cleanup (free, name);
      error ("\"%s\": can't read symbols: %s.", name,
	     bfd_errmsg (bfd_get_error ()));
    }
  return (sym_bfd);
}

/* Link a new symtab_fns into the global symtab_fns list.  Called on gdb
   startup by the _initialize routine in each object file format reader,
   to register information about each format the the reader is prepared
   to handle. */

void
add_symtab_fns (sf)
     struct sym_fns *sf;
{
  sf->next = symtab_fns;
  symtab_fns = sf;
}


/* Initialize to read symbols from the symbol file sym_bfd.  It either
   returns or calls error().  The result is an initialized struct sym_fns
   in the objfile structure, that contains cached information about the
   symbol file.  */

static void
find_sym_fns (objfile)
     struct objfile *objfile;
{
  struct sym_fns *sf;
  enum bfd_flavour our_flavour = bfd_get_flavour (objfile -> obfd);
  char *our_target = bfd_get_target (objfile -> obfd);

  /* Special kludge for RS/6000 and PowerMac.  See xcoffread.c.  */
  if (STREQ (our_target, "aixcoff-rs6000") ||
      STREQ (our_target, "xcoff-powermac"))
    our_flavour = (enum bfd_flavour)-1;

  /* Special kludge for apollo.  See dstread.c.  */
  if (STREQN (our_target, "apollo", 6))
    our_flavour = (enum bfd_flavour)-2;

  for (sf = symtab_fns; sf != NULL; sf = sf -> next)
    {
      if (our_flavour == sf -> sym_flavour)
	{
	  objfile -> sf = sf;
	  return;
	}
    }
  error ("I'm sorry, Dave, I can't do that.  Symbol format `%s' unknown.",
	 bfd_get_target (objfile -> obfd));
}

/* This function runs the load command of our current target.  */

static void
load_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg == NULL)
    arg = get_exec_file (1);
  target_load (arg, from_tty);
}

/* This version of "load" should be usable for any target.  Currently
   it is just used for remote targets, not inftarg.c or core files,
   on the theory that only in that case is it useful.

   Avoiding xmodem and the like seems like a win (a) because we don't have
   to worry about finding it, and (b) On VMS, fork() is very slow and so
   we don't want to run a subprocess.  On the other hand, I'm not sure how
   performance compares.  */
#define GENERIC_LOAD_CHUNK 256
#define VALIDATE_DOWNLOAD 0
void
generic_load (filename, from_tty)
    char *filename;
    int from_tty;
{
  struct cleanup *old_cleanups;
  asection *s;
  bfd *loadfile_bfd;
  time_t start_time, end_time;	/* Start and end times of download */
  unsigned long data_count = 0;	/* Number of bytes transferred to memory */
  int n; 
  unsigned long load_offset = 0; 	/* offset to add to vma for each section */
  char buf[GENERIC_LOAD_CHUNK+8];
#if VALIDATE_DOWNLOAD  
  char verify_buffer[GENERIC_LOAD_CHUNK+8] ;
#endif  

  /* enable user to specify address for downloading as 2nd arg to load */
  n = sscanf(filename, "%s 0x%lx", buf, &load_offset);
  if (n > 1 ) 
    filename = buf;
  else
    load_offset = 0;

  loadfile_bfd = bfd_openr (filename, gnutarget);
  if (loadfile_bfd == NULL)
    {
      perror_with_name (filename);
      return;
    }
  /* FIXME: should be checking for errors from bfd_close (for one thing,
     on error it does not free all the storage associated with the
     bfd).  */
  old_cleanups = make_cleanup ((make_cleanup_func) bfd_close, loadfile_bfd);

  if (!bfd_check_format (loadfile_bfd, bfd_object)) 
    {
      error ("\"%s\" is not an object file: %s", filename,
	     bfd_errmsg (bfd_get_error ()));
    }
  
  start_time = time (NULL);

  for (s = loadfile_bfd->sections; s; s = s->next) 
    {
      if (s->flags & SEC_LOAD) 
        {
          bfd_size_type size;

          size = bfd_get_section_size_before_reloc (s);
          if (size > 0)
            {
              char *buffer;
              struct cleanup *old_chain;
              bfd_vma lma;
              unsigned long l = size ;
              int err;
              char *sect;
              unsigned long sent;
              unsigned long len;
	      
	      l = l > GENERIC_LOAD_CHUNK ? GENERIC_LOAD_CHUNK : l ;

              buffer = xmalloc (size);
              old_chain = make_cleanup (free, buffer);

              lma = s->lma;
              lma += load_offset;

              /* Is this really necessary?  I guess it gives the user something
                 to look at during a long download.  */
              printf_filtered ("Loading section %s, size 0x%lx lma ",
                               bfd_get_section_name (loadfile_bfd, s),
                               (unsigned long) size);
              print_address_numeric (lma, 1, gdb_stdout);
              printf_filtered ("\n");

              bfd_get_section_contents (loadfile_bfd, s, buffer, 0, size);

              sect = (char *) bfd_get_section_name (loadfile_bfd, s);
              sent = 0;
              do
                {            
                  len = (size - sent) < l ? (size - sent) : l;
                  sent += len;
                  err = target_write_memory (lma, buffer, len);
                  if (ui_load_progress_hook)
                    if (ui_load_progress_hook (sect, sent))
		      error ("Canceled the download");
#if VALIDATE_DOWNLOAD
		  /* Broken memories and broken monitors manifest themselves
		     here when bring new computers to life.
		     This doubles already slow downloads.
		  */
		  if (err) break ;
		  {
		    target_read_memory(lma,verify_buffer,len) ;
		    if (0 != bcmp(buffer,verify_buffer,len))
		      error("Download verify failed at %08x",
			    (unsigned long)lma) ;
		  }

#endif
		  data_count += len ;
                  lma  += len;
                  buffer += len;
                } /* od */
              while (err == 0 && sent < size);

              if (err != 0)
                error ("Memory access error while loading section %s.", 
                       bfd_get_section_name (loadfile_bfd, s));
                
              do_cleanups (old_chain);
            }
        }
    }

  end_time = time (NULL);
  {
    unsigned long entry ;
    entry = bfd_get_start_address(loadfile_bfd) ;
    printf_filtered ("Start address 0x%lx , load size %d\n", entry,data_count);
    /* We were doing this in remote-mips.c, I suspect it is right
       for other targets too.  */
    write_pc (entry);
  }

  /* FIXME: are we supposed to call symbol_file_add or not?  According to
     a comment from remote-mips.c (where a call to symbol_file_add was
     commented out), making the call confuses GDB if more than one file is
     loaded in.  remote-nindy.c had no call to symbol_file_add, but remote-vx.c
     does.  */

  report_transfer_performance (data_count, start_time, end_time);

  do_cleanups (old_cleanups);
}

/* Report how fast the transfer went. */

void
report_transfer_performance (data_count, start_time, end_time)
unsigned long data_count;
time_t start_time, end_time;
{
  printf_filtered ("Transfer rate: ");
  if (end_time != start_time)
    printf_filtered ("%d bits/sec",
		     (data_count * 8) / (end_time - start_time));
  else
    printf_filtered ("%d bits in <1 sec", (data_count * 8));
  printf_filtered (".\n");
}

/* This function allows the addition of incrementally linked object files.
   It does not modify any state in the target, only in the debugger.  */

/* ARGSUSED */
static void
add_symbol_file_command (args, from_tty)
     char *args;
     int from_tty;
{
  char *name = NULL;
  CORE_ADDR text_addr;
  char *arg;
  int readnow = 0;
  int mapped = 0;
  
  dont_repeat ();

  if (args == NULL)
    {
      error ("add-symbol-file takes a file name and an address");
    }

  /* Make a copy of the string that we can safely write into. */

  args = strdup (args);
  make_cleanup (free, args);

  /* Pick off any -option args and the file name. */

  while ((*args != '\000') && (name == NULL))
    {
      while (isspace (*args)) {args++;}
      arg = args;
      while ((*args != '\000') && !isspace (*args)) {args++;}
      if (*args != '\000')
	{
	  *args++ = '\000';
	}
      if (*arg != '-')
	{
	  name = arg;
	}
      else if (STREQ (arg, "-mapped"))
	{
	  mapped = 1;
	}
      else if (STREQ (arg, "-readnow"))
	{
	  readnow = 1;
	}
      else
	{
	  error ("unknown option `%s'", arg);
	}
    }

  /* After picking off any options and the file name, args should be
     left pointing at the remainder of the command line, which should
     be the address expression to evaluate. */

  if (name == NULL)
    {
      error ("add-symbol-file takes a file name");
    }
  name = tilde_expand (name);
  make_cleanup (free, name);

  if (*args != '\000')
    {
      text_addr = parse_and_eval_address (args);
    }
  else
    {
      target_link(name, &text_addr);
      if (text_addr == (CORE_ADDR)-1)
	error("Don't know how to get text start location for this file");
    }

  /* FIXME-32x64: Assumes text_addr fits in a long.  */
  if ((from_tty)
      && (!query ("add symbol table from file \"%s\" at text_addr = %s?\n",
	          name, local_hex_string ((unsigned long)text_addr))))
    error ("Not confirmed.");

  symbol_file_add (name, from_tty, text_addr, 0, mapped, readnow,
                   1,  /* user_loaded */
                   0); /* We'll guess it's ! is_solib */

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  reinit_frame_cache ();
}

static void
add_shared_symbol_files_command  (args, from_tty)
     char *args;
     int from_tty;
{
#ifdef ADD_SHARED_SYMBOL_FILES
  ADD_SHARED_SYMBOL_FILES (args, from_tty);
#else
  error ("This command is not available in this configuration of GDB.");
#endif  
}

/* Re-read symbols if a symbol-file has changed.  */
void
reread_symbols ()
{
  struct objfile *objfile;
  long new_modtime;
  int reread_one = 0;
  struct stat new_statbuf;
  int res;

  /* With the addition of shared libraries, this should be modified,
     the load time should be saved in the partial symbol tables, since
     different tables may come from different source files.  FIXME.
     This routine should then walk down each partial symbol table
     and see if the symbol table that it originates from has been changed */

  for (objfile = object_files; objfile; objfile = objfile->next) {
    if (objfile->obfd) {
#ifdef IBM6000_TARGET
     /* If this object is from a shared library, then you should
        stat on the library name, not member name. */

     if (objfile->obfd->my_archive)
       res = stat (objfile->obfd->my_archive->filename, &new_statbuf);
     else
#endif
      res = stat (objfile->name, &new_statbuf);
      if (res != 0) {
	/* FIXME, should use print_sys_errmsg but it's not filtered. */
	printf_filtered ("`%s' has disappeared; keeping its symbols.\n",
			 objfile->name);
	continue;
      }
      new_modtime = new_statbuf.st_mtime;
      if (new_modtime != objfile->mtime)
	{
	  struct cleanup *old_cleanups;
	  struct section_offsets *offsets;
	  int num_offsets;
	  int section_offsets_size;
	  char *obfd_filename;

	  printf_filtered ("`%s' has changed; re-reading symbols.\n",
			   objfile->name);

	  /* There are various functions like symbol_file_add,
	     symfile_bfd_open, syms_from_objfile, etc., which might
	     appear to do what we want.  But they have various other
	     effects which we *don't* want.  So we just do stuff
	     ourselves.  We don't worry about mapped files (for one thing,
	     any mapped file will be out of date).  */

	  /* If we get an error, blow away this objfile (not sure if
	     that is the correct response for things like shared
	     libraries).  */
	  old_cleanups = make_cleanup ((make_cleanup_func) free_objfile, 
                                       objfile);
	  /* We need to do this whenever any symbols go away.  */
	  make_cleanup ((make_cleanup_func) clear_symtab_users, 0);

	  /* Clean up any state BFD has sitting around.  We don't need
	     to close the descriptor but BFD lacks a way of closing the
	     BFD without closing the descriptor.  */
	  obfd_filename = bfd_get_filename (objfile->obfd);
	  if (!bfd_close (objfile->obfd))
	    error ("Can't close BFD for %s: %s", objfile->name,
		   bfd_errmsg (bfd_get_error ()));
	  objfile->obfd = bfd_openr (obfd_filename, gnutarget);
	  if (objfile->obfd == NULL)
	    error ("Can't open %s to read symbols.", objfile->name);
	  /* bfd_openr sets cacheable to true, which is what we want.  */
	  if (!bfd_check_format (objfile->obfd, bfd_object))
	    error ("Can't read symbols from %s: %s.", objfile->name,
		   bfd_errmsg (bfd_get_error ()));

	  /* Save the offsets, we will nuke them with the rest of the
	     psymbol_obstack.  */
	  num_offsets = objfile->num_sections;
	  section_offsets_size =
	    sizeof (struct section_offsets)
	      + sizeof (objfile->section_offsets->offsets) * num_offsets;
	  offsets = (struct section_offsets *) alloca (section_offsets_size);
	  memcpy (offsets, objfile->section_offsets, section_offsets_size);

	  /* Nuke all the state that we will re-read.  Much of the following
	     code which sets things to NULL really is necessary to tell
	     other parts of GDB that there is nothing currently there.  */

	  /* FIXME: Do we have to free a whole linked list, or is this
	     enough?  */
	  if (objfile->global_psymbols.list)
	    mfree (objfile->md, objfile->global_psymbols.list);
	  memset (&objfile -> global_psymbols, 0,
		  sizeof (objfile -> global_psymbols));
	  if (objfile->static_psymbols.list)
	    mfree (objfile->md, objfile->static_psymbols.list);
	  memset (&objfile -> static_psymbols, 0,
		  sizeof (objfile -> static_psymbols));

	  /* Free the obstacks for non-reusable objfiles */
	  obstack_free (&objfile -> psymbol_cache.cache, 0);
	  memset (&objfile -> psymbol_cache, 0,
		  sizeof (objfile -> psymbol_cache));
	  obstack_free (&objfile -> psymbol_obstack, 0);
	  obstack_free (&objfile -> symbol_obstack, 0);
	  obstack_free (&objfile -> type_obstack, 0);
	  objfile->sections = NULL;
	  objfile->symtabs = NULL;
	  objfile->psymtabs = NULL;
	  objfile->free_psymtabs = NULL;
	  objfile->msymbols = NULL;
	  objfile->minimal_symbol_count= 0;
	  objfile->fundamental_types = NULL;
	  if (objfile -> sf != NULL)
	    {
	      (*objfile -> sf -> sym_finish) (objfile);
	    }

	  /* We never make this a mapped file.  */
	  objfile -> md = NULL;
	  /* obstack_specify_allocation also initializes the obstack so
	     it is empty.  */
	  obstack_specify_allocation (&objfile -> psymbol_cache.cache, 0, 0,
				      xmalloc, free);
	  obstack_specify_allocation (&objfile -> psymbol_obstack, 0, 0,
				      xmalloc, free);
	  obstack_specify_allocation (&objfile -> symbol_obstack, 0, 0,
				      xmalloc, free);
	  obstack_specify_allocation (&objfile -> type_obstack, 0, 0,
				      xmalloc, free);
	  if (build_objfile_section_table (objfile))
	    {
	      error ("Can't find the file sections in `%s': %s", 
		     objfile -> name, bfd_errmsg (bfd_get_error ()));
	    }

	  /* We use the same section offsets as from last time.  I'm not
	     sure whether that is always correct for shared libraries.  */
	  objfile->section_offsets = (struct section_offsets *)
	    obstack_alloc (&objfile -> psymbol_obstack, section_offsets_size);
	  memcpy (objfile->section_offsets, offsets, section_offsets_size);
	  objfile->num_sections = num_offsets;

	  /* What the hell is sym_new_init for, anyway?  The concept of
	     distinguishing between the main file and additional files
	     in this way seems rather dubious.  */
	  if (objfile == symfile_objfile)
            {
              (*objfile->sf->sym_new_init) (objfile);
#ifdef HPUXHPPA
              RESET_HP_UX_GLOBALS ();
#endif
            }

	  (*objfile->sf->sym_init) (objfile);
	  clear_complaints (1, 1);
	  /* The "mainline" parameter is a hideous hack; I think leaving it
	     zero is OK since dbxread.c also does what it needs to do if
	     objfile->global_psymbols.size is 0.  */
	  (*objfile->sf->sym_read) (objfile, objfile->section_offsets, 0);
	  if (!have_partial_symbols () && !have_full_symbols ())
	    {
	      wrap_here ("");
	      printf_filtered ("(no debugging symbols found)\n");
	      wrap_here ("");
	    }
	  objfile -> flags |= OBJF_SYMS;

	  /* We're done reading the symbol file; finish off complaints.  */
	  clear_complaints (0, 1);

	  /* Getting new symbols may change our opinion about what is
	     frameless.  */

	  reinit_frame_cache ();

	  /* Discard cleanups as symbol reading was successful.  */
	  discard_cleanups (old_cleanups);

	  /* If the mtime has changed between the time we set new_modtime
	     and now, we *want* this to be out of date, so don't call stat
	     again now.  */
	  objfile->mtime = new_modtime;
	  reread_one = 1;

	  /* Call this after reading in a new symbol table to give target
	     dependant code a crack at the new symbols.  For instance, this
	     could be used to update the values of target-specific symbols GDB
	     needs to keep track of (such as _sigtramp, or whatever).  */

	  TARGET_SYMFILE_POSTREAD (objfile);
	}
    }
  }

  if (reread_one)
    clear_symtab_users ();
}



typedef struct {
  char         *ext;
  enum language lang;
} filename_language;

static filename_language * filename_language_table;
static int fl_table_size, fl_table_next;

static void
add_filename_language (ext, lang)
     char         *ext;
     enum language lang;
{
  if (fl_table_next >= fl_table_size)
    {
      fl_table_size += 10;
      filename_language_table = realloc (filename_language_table, 
					 fl_table_size);
    }

  filename_language_table[fl_table_next].ext  = strsave (ext);
  filename_language_table[fl_table_next].lang = lang;
  fl_table_next++;
}

static char *ext_args;

static void
set_ext_lang_command (args, from_tty)
     char *args;
     int from_tty;
{
  int i;
  char *cp = ext_args;
  enum language lang;

  /* First arg is filename extension, starting with '.' */
  if (*cp != '.')
    error ("'%s': Filename extension must begin with '.'", ext_args);

  /* Find end of first arg.  */
  while (*cp && !isspace (*cp))	
    cp++;

  if (*cp == '\0')
    error ("'%s': two arguments required -- filename extension and language",
	   ext_args);

  /* Null-terminate first arg */
  *cp++ = '\0'; 

  /* Find beginning of second arg, which should be a source language.  */
  while (*cp && isspace (*cp))
    cp++;

  if (*cp == '\0')
    error ("'%s': two arguments required -- filename extension and language",
	   ext_args);

  /* Lookup the language from among those we know.  */
  lang = language_enum (cp);

  /* Now lookup the filename extension: do we already know it?  */
  for (i = 0; i < fl_table_next; i++)
    if (0 == strcmp (ext_args, filename_language_table[i].ext))
      break;

  if (i >= fl_table_next)
    {
      /* new file extension */
      add_filename_language (ext_args, lang);
    }
  else
    {
      /* redefining a previously known filename extension */

      /* if (from_tty) */
      /*   query ("Really make files of type %s '%s'?", */
      /*          ext_args, language_str (lang));           */

      free (filename_language_table[i].ext);
      filename_language_table[i].ext  = strsave (ext_args);
      filename_language_table[i].lang = lang;
    }
}

static void
info_ext_lang_command (args, from_tty)
     char *args;
     int   from_tty;
{
  int i;

  printf_filtered ("Filename extensions and the languages they represent:");
  printf_filtered ("\n\n");
  for (i = 0; i < fl_table_next; i++)
    printf_filtered ("\t%s\t- %s\n", 
		     filename_language_table[i].ext, 
		     language_str (filename_language_table[i].lang));
}

static void
init_filename_language_table ()
{
  if (fl_table_size == 0)	/* protect against repetition */
    {
      fl_table_size = 20;
      fl_table_next = 0;
      filename_language_table = 
	xmalloc (fl_table_size * sizeof (*filename_language_table));
      add_filename_language (".c",     language_c);
      add_filename_language (".C",     language_cplus);
      add_filename_language (".cc",    language_cplus);
      add_filename_language (".cp",    language_cplus);
      add_filename_language (".cpp",   language_cplus);
      add_filename_language (".cxx",   language_cplus);
      add_filename_language (".c++",   language_cplus);
      add_filename_language (".java",  language_java);
      add_filename_language (".class", language_java);
      add_filename_language (".ch",    language_chill);
      add_filename_language (".c186",  language_chill);
      add_filename_language (".c286",  language_chill);
      add_filename_language (".f",     language_fortran);
      add_filename_language (".F",     language_fortran);
      add_filename_language (".s",     language_asm);
      add_filename_language (".S",     language_asm);
    }
}

enum language
deduce_language_from_filename (filename)
     char *filename;
{
  int i;
  char *cp;

  if (filename != NULL)
    if ((cp = strrchr (filename, '.')) != NULL)
      for (i = 0; i < fl_table_next; i++)
	if (strcmp (cp, filename_language_table[i].ext) == 0)
	  return filename_language_table[i].lang;

  return language_unknown;
}

/* allocate_symtab:

   Allocate and partly initialize a new symbol table.  Return a pointer
   to it.  error() if no space.

   Caller must set these fields:
	LINETABLE(symtab)
	symtab->blockvector
	symtab->dirname
	symtab->free_code
	symtab->free_ptr
	possibly free_named_symtabs (symtab->filename);
 */

struct symtab *
allocate_symtab (filename, objfile)
     char *filename;
     struct objfile *objfile;
{
  register struct symtab *symtab;

  symtab = (struct symtab *)
    obstack_alloc (&objfile -> symbol_obstack, sizeof (struct symtab));
  memset (symtab, 0, sizeof (*symtab));
  symtab -> filename = obsavestring (filename, strlen (filename),
				     &objfile -> symbol_obstack);
  symtab -> fullname = NULL;
  symtab -> language = deduce_language_from_filename (filename);
  symtab -> debugformat = obsavestring ("unknown", 7,
					&objfile -> symbol_obstack);

  /* Hook it to the objfile it comes from */

  symtab -> objfile = objfile;
  symtab -> next = objfile -> symtabs;
  objfile -> symtabs = symtab;

  /* FIXME: This should go away.  It is only defined for the Z8000,
     and the Z8000 definition of this macro doesn't have anything to
     do with the now-nonexistent EXTRA_SYMTAB_INFO macro, it's just
     here for convenience.  */
#ifdef INIT_EXTRA_SYMTAB_INFO
  INIT_EXTRA_SYMTAB_INFO (symtab);
#endif

  return (symtab);
}

struct partial_symtab *
allocate_psymtab (filename, objfile)
     char *filename;
     struct objfile *objfile;
{
  struct partial_symtab *psymtab;

  if (objfile -> free_psymtabs)
    {
      psymtab = objfile -> free_psymtabs;
      objfile -> free_psymtabs = psymtab -> next;
    }
  else
    psymtab = (struct partial_symtab *)
      obstack_alloc (&objfile -> psymbol_obstack,
		     sizeof (struct partial_symtab));

  memset (psymtab, 0, sizeof (struct partial_symtab));
  psymtab -> filename = obsavestring (filename, strlen (filename),
				      &objfile -> psymbol_obstack);
  psymtab -> symtab = NULL;

  /* Prepend it to the psymtab list for the objfile it belongs to.
     Psymtabs are searched in most recent inserted -> least recent
     inserted order. */

  psymtab -> objfile = objfile;
  psymtab -> next = objfile -> psymtabs;
  objfile -> psymtabs = psymtab;
#if 0
  {
    struct partial_symtab **prev_pst;
    psymtab -> objfile = objfile;
    psymtab -> next = NULL;
    prev_pst = &(objfile -> psymtabs);
    while ((*prev_pst) != NULL)
      prev_pst = &((*prev_pst) -> next);
    (*prev_pst) = psymtab;
  }  
#endif
  
  return (psymtab);
}

void
discard_psymtab (pst)
     struct partial_symtab *pst;
{
  struct partial_symtab **prev_pst;

  /* From dbxread.c:
     Empty psymtabs happen as a result of header files which don't
     have any symbols in them.  There can be a lot of them.  But this
     check is wrong, in that a psymtab with N_SLINE entries but
     nothing else is not empty, but we don't realize that.  Fixing
     that without slowing things down might be tricky.  */

  /* First, snip it out of the psymtab chain */

  prev_pst = &(pst->objfile->psymtabs);
  while ((*prev_pst) != pst)
    prev_pst = &((*prev_pst)->next);
  (*prev_pst) = pst->next;

  /* Next, put it on a free list for recycling */

  pst->next = pst->objfile->free_psymtabs;
  pst->objfile->free_psymtabs = pst;
}


/* Reset all data structures in gdb which may contain references to symbol
   table data.  */

void
clear_symtab_users ()
{
  /* Someday, we should do better than this, by only blowing away
     the things that really need to be blown.  */
  clear_value_history ();
  clear_displays ();
  clear_internalvars ();
  breakpoint_re_set ();
  set_default_breakpoint (0, 0, 0, 0);
  current_source_symtab = 0;
  current_source_line = 0;
  clear_pc_function_cache ();
  target_new_objfile (NULL);
}

/* clear_symtab_users_once:

   This function is run after symbol reading, or from a cleanup.
   If an old symbol table was obsoleted, the old symbol table
   has been blown away, but the other GDB data structures that may 
   reference it have not yet been cleared or re-directed.  (The old
   symtab was zapped, and the cleanup queued, in free_named_symtab()
   below.)

   This function can be queued N times as a cleanup, or called
   directly; it will do all the work the first time, and then will be a
   no-op until the next time it is queued.  This works by bumping a
   counter at queueing time.  Much later when the cleanup is run, or at
   the end of symbol processing (in case the cleanup is discarded), if
   the queued count is greater than the "done-count", we do the work
   and set the done-count to the queued count.  If the queued count is
   less than or equal to the done-count, we just ignore the call.  This
   is needed because reading a single .o file will often replace many
   symtabs (one per .h file, for example), and we don't want to reset
   the breakpoints N times in the user's face.

   The reason we both queue a cleanup, and call it directly after symbol
   reading, is because the cleanup protects us in case of errors, but is
   discarded if symbol reading is successful.  */

#if 0
/* FIXME:  As free_named_symtabs is currently a big noop this function
   is no longer needed.  */
static void
clear_symtab_users_once PARAMS ((void));

static int clear_symtab_users_queued;
static int clear_symtab_users_done;

static void
clear_symtab_users_once ()
{
  /* Enforce once-per-`do_cleanups'-semantics */
  if (clear_symtab_users_queued <= clear_symtab_users_done)
    return;
  clear_symtab_users_done = clear_symtab_users_queued;

  clear_symtab_users ();
}
#endif

/* Delete the specified psymtab, and any others that reference it.  */

static void
cashier_psymtab (pst)
     struct partial_symtab *pst;
{
  struct partial_symtab *ps, *pprev = NULL;
  int i;

  /* Find its previous psymtab in the chain */
  for (ps = pst->objfile->psymtabs; ps; ps = ps->next) {
    if (ps == pst)
      break;
    pprev = ps;
  }

  if (ps) {
    /* Unhook it from the chain.  */
    if (ps == pst->objfile->psymtabs)
      pst->objfile->psymtabs = ps->next;
    else
      pprev->next = ps->next;

    /* FIXME, we can't conveniently deallocate the entries in the
       partial_symbol lists (global_psymbols/static_psymbols) that
       this psymtab points to.  These just take up space until all
       the psymtabs are reclaimed.  Ditto the dependencies list and
       filename, which are all in the psymbol_obstack.  */

    /* We need to cashier any psymtab that has this one as a dependency... */
again:
    for (ps = pst->objfile->psymtabs; ps; ps = ps->next) {
      for (i = 0; i < ps->number_of_dependencies; i++) {
	if (ps->dependencies[i] == pst) {
	  cashier_psymtab (ps);
	  goto again;		/* Must restart, chain has been munged. */
	}
      }
    }
  }
}

/* If a symtab or psymtab for filename NAME is found, free it along
   with any dependent breakpoints, displays, etc.
   Used when loading new versions of object modules with the "add-file"
   command.  This is only called on the top-level symtab or psymtab's name;
   it is not called for subsidiary files such as .h files.

   Return value is 1 if we blew away the environment, 0 if not.
   FIXME.  The return valu appears to never be used.

   FIXME.  I think this is not the best way to do this.  We should
   work on being gentler to the environment while still cleaning up
   all stray pointers into the freed symtab.  */

int
free_named_symtabs (name)
     char *name;
{
#if 0
  /* FIXME:  With the new method of each objfile having it's own
     psymtab list, this function needs serious rethinking.  In particular,
     why was it ever necessary to toss psymtabs with specific compilation
     unit filenames, as opposed to all psymtabs from a particular symbol
     file?  -- fnf
     Well, the answer is that some systems permit reloading of particular
     compilation units.  We want to blow away any old info about these
     compilation units, regardless of which objfiles they arrived in. --gnu.  */

  register struct symtab *s;
  register struct symtab *prev;
  register struct partial_symtab *ps;
  struct blockvector *bv;
  int blewit = 0;

  /* We only wack things if the symbol-reload switch is set.  */
  if (!symbol_reloading)
    return 0;

  /* Some symbol formats have trouble providing file names... */
  if (name == 0 || *name == '\0')
    return 0;

  /* Look for a psymtab with the specified name.  */

again2:
  for (ps = partial_symtab_list; ps; ps = ps->next) {
    if (STREQ (name, ps->filename)) {
      cashier_psymtab (ps);	/* Blow it away...and its little dog, too.  */
      goto again2;		/* Must restart, chain has been munged */
    }
  }

  /* Look for a symtab with the specified name.  */

  for (s = symtab_list; s; s = s->next)
    {
      if (STREQ (name, s->filename))
	break;
      prev = s;
    }

  if (s)
    {
      if (s == symtab_list)
	symtab_list = s->next;
      else
	prev->next = s->next;

      /* For now, queue a delete for all breakpoints, displays, etc., whether
	 or not they depend on the symtab being freed.  This should be
	 changed so that only those data structures affected are deleted.  */

      /* But don't delete anything if the symtab is empty.
	 This test is necessary due to a bug in "dbxread.c" that
	 causes empty symtabs to be created for N_SO symbols that
	 contain the pathname of the object file.  (This problem
	 has been fixed in GDB 3.9x).  */

      bv = BLOCKVECTOR (s);
      if (BLOCKVECTOR_NBLOCKS (bv) > 2
	  || BLOCK_NSYMS (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK))
	  || BLOCK_NSYMS (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)))
	{
	  complain (&oldsyms_complaint, name);

	  clear_symtab_users_queued++;
	  make_cleanup (clear_symtab_users_once, 0);
	  blewit = 1;
	} else {
	  complain (&empty_symtab_complaint, name);
	}

      free_symtab (s);
    }
  else
    {
      /* It is still possible that some breakpoints will be affected
	 even though no symtab was found, since the file might have
	 been compiled without debugging, and hence not be associated
	 with a symtab.  In order to handle this correctly, we would need
	 to keep a list of text address ranges for undebuggable files.
	 For now, we do nothing, since this is a fairly obscure case.  */
      ;
    }

  /* FIXME, what about the minimal symbol table? */
  return blewit;
#else
  return (0);
#endif
}

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   SYMFILE_NAME is the name of the symbol-file we are reading from, and ADDR
   is the address relative to which its symbols are (incremental) or 0
   (normal). */


struct partial_symtab *
start_psymtab_common (objfile, section_offsets,
		      filename, textlow, global_syms, static_syms)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     char *filename;
     CORE_ADDR textlow;
     struct partial_symbol **global_syms;
     struct partial_symbol **static_syms;
{
  struct partial_symtab *psymtab;

  psymtab = allocate_psymtab (filename, objfile);
  psymtab -> section_offsets = section_offsets;
  psymtab -> textlow = textlow;
  psymtab -> texthigh = psymtab -> textlow;  /* default */
  psymtab -> globals_offset = global_syms - objfile -> global_psymbols.list;
  psymtab -> statics_offset = static_syms - objfile -> static_psymbols.list;
  return (psymtab);
}

/* Add a symbol with a long value to a psymtab.
   Since one arg is a struct, we pass in a ptr and deref it (sigh).  */

void
add_psymbol_to_list (name, namelength, namespace, class, list, val, coreaddr,
		     language, objfile)
     char *name;
     int namelength;
     namespace_enum namespace;
     enum address_class class;
     struct psymbol_allocation_list *list;
     long val;					/* Value as a long */
     CORE_ADDR coreaddr;			/* Value as a CORE_ADDR */
     enum language language;
     struct objfile *objfile;
{
  register struct partial_symbol *psym;
  char *buf = alloca (namelength + 1);
  /* psymbol is static so that there will be no uninitialized gaps in the
     structure which might contain random data, causing cache misses in
     bcache. */
  static struct partial_symbol psymbol;

  /* Create local copy of the partial symbol */
  memcpy (buf, name, namelength);
  buf[namelength] = '\0';
  SYMBOL_NAME (&psymbol) = bcache (buf, namelength + 1, &objfile->psymbol_cache);
  /* val and coreaddr are mutually exclusive, one of them *will* be zero */
  if (val != 0)
    {
      SYMBOL_VALUE (&psymbol) = val;
    }
  else
    {
      SYMBOL_VALUE_ADDRESS (&psymbol) = coreaddr;
    }
  SYMBOL_SECTION (&psymbol) = 0;
  SYMBOL_LANGUAGE (&psymbol) = language;
  PSYMBOL_NAMESPACE (&psymbol) = namespace;
  PSYMBOL_CLASS (&psymbol) = class;
  SYMBOL_INIT_LANGUAGE_SPECIFIC (&psymbol, language);

  /* Stash the partial symbol away in the cache */
  psym = bcache (&psymbol, sizeof (struct partial_symbol), &objfile->psymbol_cache);

  /* Save pointer to partial symbol in psymtab, growing symtab if needed. */
  if (list->next >= list->list + list->size)
    {
      extend_psymbol_list (list, objfile);
    }
  *list->next++ = psym;
  OBJSTAT (objfile, n_psyms++);
}

/* Add a symbol with a long value to a psymtab. This differs from
 * add_psymbol_to_list above in taking both a mangled and a demangled
 * name. */

void
add_psymbol_with_dem_name_to_list (name, namelength, dem_name, dem_namelength,
                                   namespace, class, list, val, coreaddr, language, objfile)
     char *name;
     int namelength;
     char *dem_name;
     int dem_namelength;
     namespace_enum namespace;
     enum address_class class;
     struct psymbol_allocation_list *list;
     long val;					/* Value as a long */
     CORE_ADDR coreaddr;			/* Value as a CORE_ADDR */
     enum language language;
     struct objfile *objfile;
{
  register struct partial_symbol *psym;
  char *buf = alloca (namelength + 1);
  /* psymbol is static so that there will be no uninitialized gaps in the
     structure which might contain random data, causing cache misses in
     bcache. */
  static struct partial_symbol psymbol;

  /* Create local copy of the partial symbol */

  memcpy (buf, name, namelength);
  buf[namelength] = '\0';
  SYMBOL_NAME (&psymbol) = bcache (buf, namelength + 1, &objfile->psymbol_cache);

  buf = alloca (dem_namelength + 1);
  memcpy (buf, dem_name, dem_namelength);
  buf[dem_namelength] = '\0';
  
  switch (language)
    {
      case language_c:
      case language_cplus:
        SYMBOL_CPLUS_DEMANGLED_NAME (&psymbol) =
          bcache (buf, dem_namelength + 1, &objfile->psymbol_cache);
        break;
      case language_chill:
        SYMBOL_CHILL_DEMANGLED_NAME (&psymbol) = 
          bcache (buf, dem_namelength + 1, &objfile->psymbol_cache);
        
      /* FIXME What should be done for the default case? Ignoring for now. */
    }

  /* val and coreaddr are mutually exclusive, one of them *will* be zero */
  if (val != 0)
    {
      SYMBOL_VALUE (&psymbol) = val;
    }
  else
    {
      SYMBOL_VALUE_ADDRESS (&psymbol) = coreaddr;
    }
  SYMBOL_SECTION (&psymbol) = 0;
  SYMBOL_LANGUAGE (&psymbol) = language;
  PSYMBOL_NAMESPACE (&psymbol) = namespace;
  PSYMBOL_CLASS (&psymbol) = class;
  SYMBOL_INIT_LANGUAGE_SPECIFIC (&psymbol, language);

  /* Stash the partial symbol away in the cache */
  psym = bcache (&psymbol, sizeof (struct partial_symbol), &objfile->psymbol_cache);

  /* Save pointer to partial symbol in psymtab, growing symtab if needed. */
  if (list->next >= list->list + list->size)
    {
      extend_psymbol_list (list, objfile);
    }
  *list->next++ = psym;
  OBJSTAT (objfile, n_psyms++);
}

/* Initialize storage for partial symbols.  */

void
init_psymbol_list (objfile, total_symbols)
     struct objfile *objfile;
     int total_symbols;
{
  /* Free any previously allocated psymbol lists.  */
  
  if (objfile -> global_psymbols.list)
    {
      mfree (objfile -> md, (PTR)objfile -> global_psymbols.list);
    }
  if (objfile -> static_psymbols.list)
    {
      mfree (objfile -> md, (PTR)objfile -> static_psymbols.list);
    }
  
  /* Current best guess is that approximately a twentieth
     of the total symbols (in a debugging file) are global or static
     oriented symbols */
  
  objfile -> global_psymbols.size = total_symbols / 10;
  objfile -> static_psymbols.size = total_symbols / 10;

  if (objfile -> global_psymbols.size > 0)
    {
      objfile -> global_psymbols.next =
	objfile -> global_psymbols.list = (struct partial_symbol **)
	xmmalloc (objfile -> md, (objfile -> global_psymbols.size
				  * sizeof (struct partial_symbol *)));
    }
  if (objfile -> static_psymbols.size > 0)
    {
      objfile -> static_psymbols.next =
	objfile -> static_psymbols.list = (struct partial_symbol **)
	xmmalloc (objfile -> md, (objfile -> static_psymbols.size
				  * sizeof (struct partial_symbol *)));
    }
}

/* OVERLAYS:
   The following code implements an abstraction for debugging overlay sections.

   The target model is as follows:
   1) The gnu linker will permit multiple sections to be mapped into the
      same VMA, each with its own unique LMA (or load address).
   2) It is assumed that some runtime mechanism exists for mapping the
      sections, one by one, from the load address into the VMA address.
   3) This code provides a mechanism for gdb to keep track of which 
      sections should be considered to be mapped from the VMA to the LMA.
      This information is used for symbol lookup, and memory read/write.
      For instance, if a section has been mapped then its contents 
      should be read from the VMA, otherwise from the LMA.

   Two levels of debugger support for overlays are available.  One is
   "manual", in which the debugger relies on the user to tell it which
   overlays are currently mapped.  This level of support is
   implemented entirely in the core debugger, and the information about
   whether a section is mapped is kept in the objfile->obj_section table.

   The second level of support is "automatic", and is only available if
   the target-specific code provides functionality to read the target's
   overlay mapping table, and translate its contents for the debugger
   (by updating the mapped state information in the obj_section tables).

   The interface is as follows:
     User commands:
       overlay map <name>	-- tell gdb to consider this section mapped
       overlay unmap <name>	-- tell gdb to consider this section unmapped
       overlay list		-- list the sections that GDB thinks are mapped
       overlay read-target	-- get the target's state of what's mapped
       overlay off/manual/auto -- set overlay debugging state
     Functional interface:
       find_pc_mapped_section(pc):    if the pc is in the range of a mapped
				      section, return that section.
       find_pc_overlay(pc):	      find any overlay section that contains 
				      the pc, either in its VMA or its LMA
       overlay_is_mapped(sect):       true if overlay is marked as mapped
       section_is_overlay(sect):      true if section's VMA != LMA
       pc_in_mapped_range(pc,sec):    true if pc belongs to section's VMA
       pc_in_unmapped_range(...):     true if pc belongs to section's LMA
       overlay_mapped_address(...):   map an address from section's LMA to VMA
       overlay_unmapped_address(...): map an address from section's VMA to LMA
       symbol_overlayed_address(...): Return a "current" address for symbol:
				      either in VMA or LMA depending on whether
				      the symbol's section is currently mapped
 */

/* Overlay debugging state: */

int overlay_debugging = 0;	/* 0 == off, 1 == manual, -1 == auto */
int overlay_cache_invalid = 0;	/* True if need to refresh mapped state */

/* Target vector for refreshing overlay mapped state */
static void simple_overlay_update PARAMS ((struct obj_section *));
void (*target_overlay_update) PARAMS ((struct obj_section *)) 
     = simple_overlay_update;

/* Function: section_is_overlay (SECTION)
   Returns true if SECTION has VMA not equal to LMA, ie. 
   SECTION is loaded at an address different from where it will "run".  */

int
section_is_overlay (section)
     asection *section;
{
  if (overlay_debugging)
    if (section && section->lma != 0 &&
	section->vma != section->lma)
      return 1;

  return 0;
}

/* Function: overlay_invalidate_all (void)
   Invalidate the mapped state of all overlay sections (mark it as stale).  */

static void
overlay_invalidate_all ()
{
  struct objfile     *objfile;
  struct obj_section *sect;

  ALL_OBJSECTIONS (objfile, sect)
    if (section_is_overlay (sect->the_bfd_section))
      sect->ovly_mapped = -1;
}

/* Function: overlay_is_mapped (SECTION)
   Returns true if section is an overlay, and is currently mapped. 
   Private: public access is thru function section_is_mapped.

   Access to the ovly_mapped flag is restricted to this function, so
   that we can do automatic update.  If the global flag
   OVERLAY_CACHE_INVALID is set (by wait_for_inferior), then call
   overlay_invalidate_all.  If the mapped state of the particular
   section is stale, then call TARGET_OVERLAY_UPDATE to refresh it.  */

static int 
overlay_is_mapped (osect)
     struct obj_section *osect;
{
  if (osect == 0 || !section_is_overlay (osect->the_bfd_section))
    return 0;

  switch (overlay_debugging) 
    {
    default:
    case 0:	return 0;	/* overlay debugging off */
    case -1:			/* overlay debugging automatic */
      /* Unles there is a target_overlay_update function, 
	 there's really nothing useful to do here (can't really go auto)  */
      if (target_overlay_update)
	{
	  if (overlay_cache_invalid)
	    {
	      overlay_invalidate_all ();
	      overlay_cache_invalid = 0;
	    }
	  if (osect->ovly_mapped == -1)
	    (*target_overlay_update) (osect);
	}
      /* fall thru to manual case */
    case 1:			/* overlay debugging manual */
      return osect->ovly_mapped == 1;
    }
}

/* Function: section_is_mapped
   Returns true if section is an overlay, and is currently mapped.  */

int
section_is_mapped (section)
     asection *section;
{
  struct objfile     *objfile;
  struct obj_section *osect;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      ALL_OBJSECTIONS (objfile, osect)
	if (osect->the_bfd_section == section)
	  return overlay_is_mapped (osect);

  return 0;
}

/* Function: pc_in_unmapped_range
   If PC falls into the lma range of SECTION, return true, else false.  */

CORE_ADDR
pc_in_unmapped_range (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  int size;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      {
	size = bfd_get_section_size_before_reloc (section);
	if (section->lma <= pc && pc < section->lma + size)
	  return 1;
      }
  return 0;
}

/* Function: pc_in_mapped_range
   If PC falls into the vma range of SECTION, return true, else false.  */

CORE_ADDR
pc_in_mapped_range (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  int size;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      {
	size = bfd_get_section_size_before_reloc (section);
	if (section->vma <= pc && pc < section->vma + size)
	  return 1;
      }
  return 0;
}

/* Function: overlay_unmapped_address (PC, SECTION)
   Returns the address corresponding to PC in the unmapped (load) range.
   May be the same as PC.  */

CORE_ADDR
overlay_unmapped_address (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  if (overlay_debugging)
    if (section && section_is_overlay (section) &&
	pc_in_mapped_range (pc, section))
      return pc + section->lma - section->vma;

  return pc;
}

/* Function: overlay_mapped_address (PC, SECTION)
   Returns the address corresponding to PC in the mapped (runtime) range.
   May be the same as PC.  */

CORE_ADDR
overlay_mapped_address (pc, section)
     CORE_ADDR pc;
     asection *section;
{
  if (overlay_debugging)
    if (section && section_is_overlay (section) &&
	pc_in_unmapped_range (pc, section))
      return pc + section->vma - section->lma;

  return pc;
}


/* Function: symbol_overlayed_address 
   Return one of two addresses (relative to the VMA or to the LMA),
   depending on whether the section is mapped or not.  */

CORE_ADDR 
symbol_overlayed_address (address, section)
     CORE_ADDR address;
     asection *section;
{
  if (overlay_debugging)
    {
      /* If the symbol has no section, just return its regular address. */
      if (section == 0)
	return address;
      /* If the symbol's section is not an overlay, just return its address */
      if (!section_is_overlay (section))
	return address;
      /* If the symbol's section is mapped, just return its address */
      if (section_is_mapped (section))
	return address;
      /*
       * HOWEVER: if the symbol is in an overlay section which is NOT mapped,
       * then return its LOADED address rather than its vma address!!
       */
      return overlay_unmapped_address (address, section);
    }
  return address;
}

/* Function: find_pc_overlay (PC) 
   Return the best-match overlay section for PC:
   If PC matches a mapped overlay section's VMA, return that section.
   Else if PC matches an unmapped section's VMA, return that section.
   Else if PC matches an unmapped section's LMA, return that section.  */

asection *
find_pc_overlay (pc)
     CORE_ADDR pc;
{
  struct objfile     *objfile;
  struct obj_section *osect, *best_match = NULL;

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (section_is_overlay (osect->the_bfd_section))
	{
	  if (pc_in_mapped_range (pc, osect->the_bfd_section))
	    {
	      if (overlay_is_mapped (osect))
		return osect->the_bfd_section;
	      else
		best_match = osect;
	    }
	  else if (pc_in_unmapped_range (pc, osect->the_bfd_section))
	    best_match = osect;
	}
  return best_match ? best_match->the_bfd_section : NULL;
}

/* Function: find_pc_mapped_section (PC)
   If PC falls into the VMA address range of an overlay section that is 
   currently marked as MAPPED, return that section.  Else return NULL.  */

asection *
find_pc_mapped_section (pc)
     CORE_ADDR pc;
{
  struct objfile     *objfile;
  struct obj_section *osect;

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (pc_in_mapped_range (pc, osect->the_bfd_section) &&
	  overlay_is_mapped (osect))
	return osect->the_bfd_section;

  return NULL;
}

/* Function: list_overlays_command
   Print a list of mapped sections and their PC ranges */

void
list_overlays_command (args, from_tty)
     char *args;
     int from_tty;
{
  int                nmapped = 0;
  struct objfile     *objfile;
  struct obj_section *osect;

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (overlay_is_mapped (osect))
	{
	  const char *name;
	  bfd_vma     lma, vma;
	  int         size;

	  vma  = bfd_section_vma (objfile->obfd, osect->the_bfd_section);
	  lma  = bfd_section_lma (objfile->obfd, osect->the_bfd_section);
	  size = bfd_get_section_size_before_reloc (osect->the_bfd_section);
	  name = bfd_section_name (objfile->obfd, osect->the_bfd_section);

	  printf_filtered ("Section %s, loaded at ", name);
	  print_address_numeric (lma, 1, gdb_stdout);
	  puts_filtered (" - ");
	  print_address_numeric (lma + size, 1, gdb_stdout);
	  printf_filtered (", mapped at ");
	  print_address_numeric (vma, 1, gdb_stdout);
	  puts_filtered (" - ");
	  print_address_numeric (vma + size, 1, gdb_stdout);
	  puts_filtered ("\n");

	  nmapped ++;
	}
  if (nmapped == 0)
    printf_filtered ("No sections are mapped.\n");
}

/* Function: map_overlay_command
   Mark the named section as mapped (ie. residing at its VMA address).  */

void
map_overlay_command (args, from_tty)
     char *args;
     int   from_tty;
{
  struct objfile     *objfile, *objfile2;
  struct obj_section *sec,     *sec2;
  asection           *bfdsec;

  if (!overlay_debugging)
    error ("Overlay debugging not enabled.  Use the 'OVERLAY ON' command.");

  if (args == 0 || *args == 0)
    error ("Argument required: name of an overlay section");

  /* First, find a section matching the user supplied argument */
  ALL_OBJSECTIONS (objfile, sec)
    if (!strcmp (bfd_section_name (objfile->obfd, sec->the_bfd_section), args))
      { 
	/* Now, check to see if the section is an overlay. */
	bfdsec = sec->the_bfd_section;
	if (!section_is_overlay (bfdsec))
	  continue;		/* not an overlay section */

	/* Mark the overlay as "mapped" */
	sec->ovly_mapped = 1;

	/* Next, make a pass and unmap any sections that are
	   overlapped by this new section: */
	ALL_OBJSECTIONS (objfile2, sec2)
	  if (sec2->ovly_mapped &&
	      sec != sec2 &&
	      sec->the_bfd_section != sec2->the_bfd_section &&
	      (pc_in_mapped_range (sec2->addr,    sec->the_bfd_section) ||
	       pc_in_mapped_range (sec2->endaddr, sec->the_bfd_section)))
	    {
	      if (info_verbose)
		printf_filtered ("Note: section %s unmapped by overlap\n",
				 bfd_section_name (objfile->obfd, 
						   sec2->the_bfd_section));
	      sec2->ovly_mapped = 0;	/* sec2 overlaps sec: unmap sec2 */
	    }
	return;
      }
  error ("No overlay section called %s", args);
}

/* Function: unmap_overlay_command
   Mark the overlay section as unmapped 
   (ie. resident in its LMA address range, rather than the VMA range).  */

void
unmap_overlay_command (args, from_tty)
     char *args;
     int   from_tty;
{
  struct objfile     *objfile;
  struct obj_section *sec;

  if (!overlay_debugging)
    error ("Overlay debugging not enabled.  Use the 'OVERLAY ON' command.");

  if (args == 0 || *args == 0)
    error ("Argument required: name of an overlay section");

  /* First, find a section matching the user supplied argument */
  ALL_OBJSECTIONS (objfile, sec)
    if (!strcmp (bfd_section_name (objfile->obfd, sec->the_bfd_section), args))
      {
	if (!sec->ovly_mapped)
	  error ("Section %s is not mapped", args);
	sec->ovly_mapped = 0;
	return;
      }
  error ("No overlay section called %s", args);
}

/* Function: overlay_auto_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_auto_command (args, from_tty)
     char *args;
     int   from_tty;
{
  overlay_debugging = -1;
  if (info_verbose)
    printf_filtered ("Automatic overlay debugging enabled.");
}

/* Function: overlay_manual_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_manual_command (args, from_tty)
     char *args;
     int   from_tty;
{
  overlay_debugging = 1;
  if (info_verbose)
    printf_filtered ("Overlay debugging enabled.");
}

/* Function: overlay_off_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_off_command (args, from_tty)
     char *args;
     int   from_tty;
{
  overlay_debugging = 0; 
  if (info_verbose)
    printf_filtered ("Overlay debugging disabled.");
}

static void
overlay_load_command (args, from_tty)
     char *args;
     int   from_tty;
{
  if (target_overlay_update)
    (*target_overlay_update) (NULL);
  else
    error ("This target does not know how to read its overlay state.");
}

/* Function: overlay_command
   A place-holder for a mis-typed command */

/* Command list chain containing all defined "overlay" subcommands. */
struct cmd_list_element *overlaylist;

static void
overlay_command (args, from_tty)
     char *args;
     int from_tty;
{
  printf_unfiltered 
    ("\"overlay\" must be followed by the name of an overlay command.\n");
  help_list (overlaylist, "overlay ", -1, gdb_stdout);
}


/* Target Overlays for the "Simplest" overlay manager:

   This is GDB's default target overlay layer.  It works with the 
   minimal overlay manager supplied as an example by Cygnus.  The 
   entry point is via a function pointer "target_overlay_update", 
   so targets that use a different runtime overlay manager can 
   substitute their own overlay_update function and take over the
   function pointer.

   The overlay_update function pokes around in the target's data structures
   to see what overlays are mapped, and updates GDB's overlay mapping with
   this information.

   In this simple implementation, the target data structures are as follows:
   	unsigned _novlys;		/# number of overlay sections #/
	unsigned _ovly_table[_novlys][4] = {
	  {VMA, SIZE, LMA, MAPPED},	/# one entry per overlay section #/
	  {..., ...,  ..., ...},
	}
	unsigned _novly_regions;	/# number of overlay regions #/
	unsigned _ovly_region_table[_novly_regions][3] = {
	  {VMA, SIZE, MAPPED_TO_LMA},	/# one entry per overlay region #/
	  {..., ...,  ...},
	}
   These functions will attempt to update GDB's mappedness state in the
   symbol section table, based on the target's mappedness state.

   To do this, we keep a cached copy of the target's _ovly_table, and
   attempt to detect when the cached copy is invalidated.  The main
   entry point is "simple_overlay_update(SECT), which looks up SECT in
   the cached table and re-reads only the entry for that section from
   the target (whenever possible).
 */

/* Cached, dynamically allocated copies of the target data structures: */
static unsigned  (*cache_ovly_table)[4] = 0;
#if 0
static unsigned  (*cache_ovly_region_table)[3] = 0;
#endif
static unsigned  cache_novlys = 0;
#if 0
static unsigned  cache_novly_regions = 0;
#endif
static CORE_ADDR cache_ovly_table_base = 0;
#if 0
static CORE_ADDR cache_ovly_region_table_base = 0;
#endif
enum   ovly_index { VMA, SIZE, LMA, MAPPED};
#define TARGET_LONG_BYTES (TARGET_LONG_BIT / TARGET_CHAR_BIT)

/* Throw away the cached copy of _ovly_table */
static void
simple_free_overlay_table ()
{
  if (cache_ovly_table)
    free(cache_ovly_table);
  cache_novlys     = 0;
  cache_ovly_table = NULL;
  cache_ovly_table_base = 0;
}

#if 0
/* Throw away the cached copy of _ovly_region_table */
static void
simple_free_overlay_region_table ()
{
  if (cache_ovly_region_table)
    free(cache_ovly_region_table);
  cache_novly_regions     = 0;
  cache_ovly_region_table = NULL;
  cache_ovly_region_table_base = 0;
}
#endif

/* Read an array of ints from the target into a local buffer.
   Convert to host order.  int LEN is number of ints  */
static void
read_target_long_array (memaddr, myaddr, len)
     CORE_ADDR     memaddr;
     unsigned int *myaddr;
     int           len;
{
  char *buf = alloca (len * TARGET_LONG_BYTES);
  int           i;

  read_memory (memaddr, buf, len * TARGET_LONG_BYTES);
  for (i = 0; i < len; i++)
    myaddr[i] = extract_unsigned_integer (TARGET_LONG_BYTES * i + buf, 
					  TARGET_LONG_BYTES);
}

/* Find and grab a copy of the target _ovly_table
   (and _novlys, which is needed for the table's size) */
static int 
simple_read_overlay_table ()
{
  struct minimal_symbol *msym;

  simple_free_overlay_table ();
  msym = lookup_minimal_symbol ("_novlys", 0, 0);
  if (msym != NULL)
    cache_novlys = read_memory_integer (SYMBOL_VALUE_ADDRESS (msym), 4);
  else 
    return 0;	/* failure */
  cache_ovly_table = (void *) xmalloc (cache_novlys * sizeof(*cache_ovly_table));
  if (cache_ovly_table != NULL)
    {
      msym = lookup_minimal_symbol ("_ovly_table", 0, 0);
      if (msym != NULL)
	{
	  cache_ovly_table_base = SYMBOL_VALUE_ADDRESS (msym);
	  read_target_long_array (cache_ovly_table_base, 
				  (int *) cache_ovly_table, 
				  cache_novlys * 4);
	}
      else 
	return 0;	/* failure */
    }
  else 
    return 0;	/* failure */
  return 1;	/* SUCCESS */
}

#if 0
/* Find and grab a copy of the target _ovly_region_table
   (and _novly_regions, which is needed for the table's size) */
static int 
simple_read_overlay_region_table ()
{
  struct minimal_symbol *msym;

  simple_free_overlay_region_table ();
  msym = lookup_minimal_symbol ("_novly_regions", 0, 0);
  if (msym != NULL)
    cache_novly_regions = read_memory_integer (SYMBOL_VALUE_ADDRESS (msym), 4);
  else 
    return 0;	/* failure */
  cache_ovly_region_table = (void *) xmalloc (cache_novly_regions * 12);
  if (cache_ovly_region_table != NULL)
    {
      msym = lookup_minimal_symbol ("_ovly_region_table", 0, 0);
      if (msym != NULL)
	{
	  cache_ovly_region_table_base = SYMBOL_VALUE_ADDRESS (msym);
	  read_target_long_array (cache_ovly_region_table_base, 
				  (int *) cache_ovly_region_table, 
				  cache_novly_regions * 3);
	}
      else 
	return 0;	/* failure */
    }
  else 
    return 0;	/* failure */
  return 1;	/* SUCCESS */
}
#endif

/* Function: simple_overlay_update_1 
   A helper function for simple_overlay_update.  Assuming a cached copy
   of _ovly_table exists, look through it to find an entry whose vma,
   lma and size match those of OSECT.  Re-read the entry and make sure
   it still matches OSECT (else the table may no longer be valid).
   Set OSECT's mapped state to match the entry.  Return: 1 for
   success, 0 for failure.  */

static int
simple_overlay_update_1 (osect)
     struct obj_section *osect;
{
  int i, size;

  size = bfd_get_section_size_before_reloc (osect->the_bfd_section);
  for (i = 0; i < cache_novlys; i++)
    if (cache_ovly_table[i][VMA]  == osect->the_bfd_section->vma &&
	cache_ovly_table[i][LMA]  == osect->the_bfd_section->lma /* &&
	cache_ovly_table[i][SIZE] == size */)
      {
	read_target_long_array (cache_ovly_table_base + i * TARGET_LONG_BYTES,
				(int *) cache_ovly_table[i], 4);
	if (cache_ovly_table[i][VMA]  == osect->the_bfd_section->vma &&
	    cache_ovly_table[i][LMA]  == osect->the_bfd_section->lma /* &&
	    cache_ovly_table[i][SIZE] == size */)
	  {
	    osect->ovly_mapped = cache_ovly_table[i][MAPPED];
	    return 1;
	  }
	else	/* Warning!  Warning!  Target's ovly table has changed! */
	  return 0;
      }
  return 0;
}

/* Function: simple_overlay_update
   If OSECT is NULL, then update all sections' mapped state 
   (after re-reading the entire target _ovly_table). 
   If OSECT is non-NULL, then try to find a matching entry in the 
   cached ovly_table and update only OSECT's mapped state.
   If a cached entry can't be found or the cache isn't valid, then 
   re-read the entire cache, and go ahead and update all sections.  */

static void
simple_overlay_update (osect)
     struct obj_section *osect;
{
  struct objfile        *objfile;

  /* Were we given an osect to look up?  NULL means do all of them. */
  if (osect)
    /* Have we got a cached copy of the target's overlay table? */
    if (cache_ovly_table != NULL)
      /* Does its cached location match what's currently in the symtab? */
      if (cache_ovly_table_base == 
	  SYMBOL_VALUE_ADDRESS (lookup_minimal_symbol ("_ovly_table", 0, 0)))
	/* Then go ahead and try to look up this single section in the cache */
	if (simple_overlay_update_1 (osect))
	  /* Found it!  We're done. */
	  return;

  /* Cached table no good: need to read the entire table anew.
     Or else we want all the sections, in which case it's actually
     more efficient to read the whole table in one block anyway.  */

  if (simple_read_overlay_table () == 0)	/* read failed?  No table? */
    {
      warning ("Failed to read the target overlay mapping table.");
      return;
    }
  /* Now may as well update all sections, even if only one was requested. */
  ALL_OBJSECTIONS (objfile, osect)
    if (section_is_overlay (osect->the_bfd_section))
      {
	int i, size;

	size = bfd_get_section_size_before_reloc (osect->the_bfd_section);
	for (i = 0; i < cache_novlys; i++)
	  if (cache_ovly_table[i][VMA]  == osect->the_bfd_section->vma &&
	      cache_ovly_table[i][LMA]  == osect->the_bfd_section->lma /* &&
	      cache_ovly_table[i][SIZE] == size */)
	    { /* obj_section matches i'th entry in ovly_table */
	      osect->ovly_mapped = cache_ovly_table[i][MAPPED];
	      break;	/* finished with inner for loop: break out */
	    }
      }
}


void
_initialize_symfile ()
{
  struct cmd_list_element *c;
  
  c = add_cmd ("symbol-file", class_files, symbol_file_command,
   "Load symbol table from executable file FILE.\n\
The `file' command can also load symbol tables, as well as setting the file\n\
to execute.", &cmdlist);
  c->completer = filename_completer;

  c = add_cmd ("add-symbol-file", class_files, add_symbol_file_command,
   "Usage: add-symbol-file FILE ADDR\n\
Load the symbols from FILE, assuming FILE has been dynamically loaded.\n\
ADDR is the starting address of the file's text.",
	       &cmdlist);
  c->completer = filename_completer;

  c = add_cmd ("add-shared-symbol-files", class_files,
	       add_shared_symbol_files_command,
   "Load the symbols from shared objects in the dynamic linker's link map.",
   	       &cmdlist);
  c = add_alias_cmd ("assf", "add-shared-symbol-files", class_files, 1,
		     &cmdlist);

  c = add_cmd ("load", class_files, load_command,
   "Dynamically load FILE into the running program, and record its symbols\n\
for access from GDB.", &cmdlist);
  c->completer = filename_completer;

  add_show_from_set
    (add_set_cmd ("symbol-reloading", class_support, var_boolean,
		  (char *)&symbol_reloading,
	  "Set dynamic symbol table reloading multiple times in one run.",
		  &setlist),
     &showlist);

  add_prefix_cmd ("overlay", class_support, overlay_command, 
		  "Commands for debugging overlays.", &overlaylist, 
		  "overlay ", 0, &cmdlist);

  add_com_alias ("ovly", "overlay", class_alias, 1);
  add_com_alias ("ov", "overlay", class_alias, 1);

  add_cmd ("map-overlay", class_support, map_overlay_command, 
	   "Assert that an overlay section is mapped.", &overlaylist);

  add_cmd ("unmap-overlay", class_support, unmap_overlay_command, 
	   "Assert that an overlay section is unmapped.", &overlaylist);

  add_cmd ("list-overlays", class_support, list_overlays_command, 
	   "List mappings of overlay sections.", &overlaylist);

  add_cmd ("manual", class_support, overlay_manual_command, 
	   "Enable overlay debugging.", &overlaylist);
  add_cmd ("off", class_support, overlay_off_command, 
	   "Disable overlay debugging.", &overlaylist);
  add_cmd ("auto", class_support, overlay_auto_command, 
	   "Enable automatic overlay debugging.", &overlaylist);
  add_cmd ("load-target", class_support, overlay_load_command, 
	   "Read the overlay mapping state from the target.", &overlaylist);

  /* Filename extension to source language lookup table: */
  init_filename_language_table ();
  c = add_set_cmd ("extension-language", class_files, var_string_noescape,
		   (char *) &ext_args, 
		   "Set mapping between filename extension and source language.\n\
Usage: set extension-language .foo bar",
		     &setlist);
  c->function.cfunc = set_ext_lang_command;

  add_info ("extensions", info_ext_lang_command, 
	    "All filename extensions associated with a source language.");
}
