/* Read NLM (NetWare Loadable Module) format executable files for GDB.
   Copyright 1993 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support (fnf@cygnus.com).

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
#include "bfd.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"

static void
nlm_new_init PARAMS ((struct objfile *));

static void
nlm_symfile_init PARAMS ((struct objfile *));

static void
nlm_symfile_read PARAMS ((struct objfile *, struct section_offsets *, int));

static void
nlm_symfile_finish PARAMS ((struct objfile *));

static void
nlm_symtab_read PARAMS ((bfd *,  CORE_ADDR, struct objfile *));

static struct section_offsets *
nlm_symfile_offsets PARAMS ((struct objfile *, CORE_ADDR));

static void
record_minimal_symbol PARAMS ((char *, CORE_ADDR, enum minimal_symbol_type,
			       struct objfile *));


/* Initialize anything that needs initializing when a completely new symbol
   file is specified (not just adding some symbols from another file, e.g. a
   shared library).

   We reinitialize buildsym, since gdb will be able to read stabs from an NLM
   file at some point in the near future.  */

static void
nlm_new_init (ignore)
     struct objfile *ignore;
{
  stabsread_new_init ();
  buildsym_new_init ();
}


/* NLM specific initialization routine for reading symbols.

   It is passed a pointer to a struct sym_fns which contains, among other
   things, the BFD for the file whose symbols are being read, and a slot for
   a pointer to "private data" which we can fill with goodies.

   For now at least, we have nothing in particular to do, so this function is
   just a stub. */

static void
nlm_symfile_init (ignore)
     struct objfile *ignore;
{
}

static void
record_minimal_symbol (name, address, ms_type, objfile)
     char *name;
     CORE_ADDR address;
     enum minimal_symbol_type ms_type;
     struct objfile *objfile;
{
  name = obsavestring (name, strlen (name), &objfile -> symbol_obstack);
  prim_record_minimal_symbol (name, address, ms_type);
}


/*

LOCAL FUNCTION

	nlm_symtab_read -- read the symbol table of an NLM file

SYNOPSIS

	void nlm_symtab_read (bfd *abfd, CORE_ADDR addr,
			      struct objfile *objfile)

DESCRIPTION

	Given an open bfd, a base address to relocate symbols to, and a
	flag that specifies whether or not this bfd is for an executable
	or not (may be shared library for example), add all the global
	function and data symbols to the minimal symbol table.
*/

static void
nlm_symtab_read (abfd, addr, objfile)
     bfd *abfd;
     CORE_ADDR addr;
     struct objfile *objfile;
{
  unsigned int storage_needed;
  asymbol *sym;
  asymbol **symbol_table;
  unsigned int number_of_symbols;
  unsigned int i;
  struct cleanup *back_to;
  CORE_ADDR symaddr;
  enum minimal_symbol_type ms_type;
  
  storage_needed = get_symtab_upper_bound (abfd);
  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (free, symbol_table);
      number_of_symbols = bfd_canonicalize_symtab (abfd, symbol_table); 
  
      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = symbol_table[i];
	  if (sym -> flags & BSF_GLOBAL)
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym -> value + sym -> section -> vma;
	      /* Relocate all non-absolute symbols by base address.  */
	      if (sym -> section != &bfd_abs_section)
		{
		  symaddr += addr;
		}

	      /* For non-absolute symbols, use the type of the section
		 they are relative to, to intuit text/data.  Bfd provides
		 no way of figuring this out for absolute symbols. */
	      if (sym -> section -> flags & SEC_CODE)
		{
		  ms_type = mst_text;
		}
	      else if (sym -> section -> flags & SEC_DATA)
		{
		  ms_type = mst_data;
		}
	      else
		{
		  ms_type = mst_unknown;
		}
	      record_minimal_symbol ((char *) sym -> name, symaddr, ms_type,
				     objfile);
	    }
	}
      do_cleanups (back_to);
    }
}


/* Scan and build partial symbols for a symbol file.
   We have been initialized by a call to nlm_symfile_init, which 
   currently does nothing.

   SECTION_OFFSETS is a set of offsets to apply to relocate the symbols
   in each section.  We simplify it down to a single offset for all
   symbols.  FIXME.

   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).

   This function only does the minimum work necessary for letting the
   user "name" things symbolically; it does not read the entire symtab.
   Instead, it reads the external and static symbols and puts them in partial
   symbol tables.  When more extensive information is requested of a
   file, the corresponding partial symbol table is mutated into a full
   fledged symbol table by going back and reading the symbols
   for real.

   Note that NLM files have two sets of information that is potentially
   useful for building gdb's minimal symbol table.  The first is a list
   of the publically exported symbols, and is currently used to build
   bfd's canonical symbol table.  The second is an optional native debugging
   format which contains additional symbols (and possibly duplicates of
   the publically exported symbols).  The optional native debugging format
   is not currently used. */

static void
nlm_symfile_read (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  bfd *abfd = objfile -> obfd;
  struct cleanup *back_to;
  CORE_ADDR offset;

  init_minimal_symbol_collection ();
  back_to = make_cleanup (discard_minimal_symbols, 0);

  /* FIXME, should take a section_offsets param, not just an offset.  */

  offset = ANOFFSET (section_offsets, 0);

  /* Process the NLM export records, which become the bfd's canonical symbol
     table. */

  nlm_symtab_read (abfd, offset, objfile);

  /* FIXME:  We could locate and read the optional native debugging format
     here and add the symbols to the minimal symbol table. */

  if (!have_partial_symbols ())
    {
      wrap_here ("");
      printf_filtered ("(no debugging symbols found)...");
      wrap_here ("");
    }

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

  install_minimal_symbols (objfile);

  do_cleanups (back_to);
}


/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
nlm_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile -> sym_private != NULL)
    {
      mfree (objfile -> md, objfile -> sym_private);
    }
}

/* NLM specific parsing routine for section offsets.
   FIXME:  This may or may not be necessary.  All the symbol readers seem
   to have similar code.  See if it can be generalized and moved elsewhere. */

static
struct section_offsets *
nlm_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;
 
  section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack,
		   sizeof (struct section_offsets) +
		   sizeof (section_offsets->offsets) * (SECT_OFF_MAX-1));

  for (i = 0; i < SECT_OFF_MAX; i++)
    {
      ANOFFSET (section_offsets, i) = addr;
    }
  
  return (section_offsets);
}


/*  Register that we are able to handle NLM file format. */

static struct sym_fns nlm_sym_fns =
{
  "nlm",		/* sym_name: name or name prefix of BFD target type */
  3,			/* sym_namelen: number of significant sym_name chars */
  nlm_new_init,		/* sym_new_init: init anything gbl to entire symtab */
  nlm_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  nlm_symfile_read,	/* sym_read: read a symbol file into symtab */
  nlm_symfile_finish,	/* sym_finish: finished with file, cleanup */
  nlm_symfile_offsets,	/* sym_offsets:  Translate ext. to int. relocation */
  NULL			/* next: pointer to next struct sym_fns */
};

void
_initialize_nlmread ()
{
  add_symtab_fns (&nlm_sym_fns);
}
