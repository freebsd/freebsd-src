/* GDB routines for manipulating objfiles.
   Copyright 1992, 1993, 1994, 1995 Free Software Foundation, Inc.
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

/* This file contains support routines for creating, manipulating, and
   destroying objfile structures. */

#include "defs.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "target.h"

#include <sys/types.h>
#include "gdb_stat.h"
#include <fcntl.h>
#include "obstack.h"
#include "gdb_string.h"

/* Prototypes for local functions */

#if !defined(NO_MMALLOC) && defined(HAVE_MMAP)

static int
open_existing_mapped_file PARAMS ((char *, long, int));

static int
open_mapped_file PARAMS ((char *filename, long mtime, int mapped));

static CORE_ADDR
map_to_address PARAMS ((void));

#endif  /* !defined(NO_MMALLOC) && defined(HAVE_MMAP) */

/* Externally visible variables that are owned by this module.
   See declarations in objfile.h for more info. */

struct objfile *object_files;		/* Linked list of all objfiles */
struct objfile *current_objfile;	/* For symbol file being read in */
struct objfile *symfile_objfile;	/* Main symbol table loaded from */
struct objfile *rt_common_objfile;	/* For runtime common symbols */

int mapped_symbol_files;		/* Try to use mapped symbol files */

/* Locate all mappable sections of a BFD file. 
   objfile_p_char is a char * to get it through
   bfd_map_over_sections; we cast it back to its proper type.  */

static void
add_to_objfile_sections (abfd, asect, objfile_p_char)
     bfd *abfd;
     sec_ptr asect;
     PTR objfile_p_char;
{
  struct objfile *objfile = (struct objfile *) objfile_p_char;
  struct obj_section section;
  flagword aflag;

  aflag = bfd_get_section_flags (abfd, asect);
  if (!(aflag & SEC_ALLOC))
    return;
  if (0 == bfd_section_size (abfd, asect))
    return;
  section.offset = 0;
  section.objfile = objfile;
  section.the_bfd_section = asect;
  section.addr = bfd_section_vma (abfd, asect);
  section.endaddr = section.addr + bfd_section_size (abfd, asect);
  obstack_grow (&objfile->psymbol_obstack, (char *) &section, sizeof(section));
  objfile->sections_end = (struct obj_section *) (((unsigned long) objfile->sections_end) + 1);
}

/* Builds a section table for OBJFILE.
   Returns 0 if OK, 1 on error (in which case bfd_error contains the
   error).  */

int
build_objfile_section_table (objfile)
     struct objfile *objfile;
{
  /* objfile->sections can be already set when reading a mapped symbol
     file.  I believe that we do need to rebuild the section table in
     this case (we rebuild other things derived from the bfd), but we
     can't free the old one (it's in the psymbol_obstack).  So we just
     waste some memory.  */

  objfile->sections_end = 0;
  bfd_map_over_sections (objfile->obfd, add_to_objfile_sections, (char *)objfile);
  objfile->sections = (struct obj_section *)
    obstack_finish (&objfile->psymbol_obstack);
  objfile->sections_end = objfile->sections + (unsigned long) objfile->sections_end;
  return(0);
}

/* Given a pointer to an initialized bfd (ABFD) and a flag that indicates
   whether or not an objfile is to be mapped (MAPPED), allocate a new objfile
   struct, fill it in as best we can, link it into the list of all known
   objfiles, and return a pointer to the new objfile struct. */

struct objfile *
allocate_objfile (abfd, mapped)
     bfd *abfd;
     int mapped;
{
  struct objfile *objfile = NULL;
  struct objfile *last_one = NULL;

  mapped |= mapped_symbol_files;

#if !defined(NO_MMALLOC) && defined(HAVE_MMAP)
  {

    /* If we can support mapped symbol files, try to open/reopen the
       mapped file that corresponds to the file from which we wish to
       read symbols.  If the objfile is to be mapped, we must malloc
       the structure itself using the mmap version, and arrange that
       all memory allocation for the objfile uses the mmap routines.
       If we are reusing an existing mapped file, from which we get
       our objfile pointer, we have to make sure that we update the
       pointers to the alloc/free functions in the obstack, in case
       these functions have moved within the current gdb.  */

    int fd;

    fd = open_mapped_file (bfd_get_filename (abfd), bfd_get_mtime (abfd),
			   mapped);
    if (fd >= 0)
      {
	CORE_ADDR mapto;
	PTR md;

	if (((mapto = map_to_address ()) == 0) ||
	    ((md = mmalloc_attach (fd, (PTR) mapto)) == NULL))
	  {
	    close (fd);
	  }
	else if ((objfile = (struct objfile *) mmalloc_getkey (md, 0)) != NULL)
	  {
	    /* Update memory corruption handler function addresses. */
	    init_malloc (md);
	    objfile -> md = md;
	    objfile -> mmfd = fd;
	    /* Update pointers to functions to *our* copies */
	    obstack_chunkfun (&objfile -> psymbol_cache.cache, xmmalloc);
	    obstack_freefun (&objfile -> psymbol_cache.cache, mfree);
	    obstack_chunkfun (&objfile -> psymbol_obstack, xmmalloc);
	    obstack_freefun (&objfile -> psymbol_obstack, mfree);
	    obstack_chunkfun (&objfile -> symbol_obstack, xmmalloc);
	    obstack_freefun (&objfile -> symbol_obstack, mfree);
	    obstack_chunkfun (&objfile -> type_obstack, xmmalloc);
	    obstack_freefun (&objfile -> type_obstack, mfree);
	    /* If already in objfile list, unlink it. */
	    unlink_objfile (objfile);
	    /* Forget things specific to a particular gdb, may have changed. */
	    objfile -> sf = NULL;
	  }
	else
	  {

	    /* Set up to detect internal memory corruption.  MUST be
	       done before the first malloc.  See comments in
	       init_malloc() and mmcheck().  */

	    init_malloc (md);

	    objfile = (struct objfile *)
	      xmmalloc (md, sizeof (struct objfile));
	    memset (objfile, 0, sizeof (struct objfile));
	    objfile -> md = md;
	    objfile -> mmfd = fd;
	    objfile -> flags |= OBJF_MAPPED;
	    mmalloc_setkey (objfile -> md, 0, objfile);
	    obstack_specify_allocation_with_arg (&objfile -> psymbol_cache.cache,
						 0, 0, xmmalloc, mfree,
						 objfile -> md);
	    obstack_specify_allocation_with_arg (&objfile -> psymbol_obstack,
						 0, 0, xmmalloc, mfree,
						 objfile -> md);
	    obstack_specify_allocation_with_arg (&objfile -> symbol_obstack,
						 0, 0, xmmalloc, mfree,
						 objfile -> md);
	    obstack_specify_allocation_with_arg (&objfile -> type_obstack,
						 0, 0, xmmalloc, mfree,
						 objfile -> md);
	  }
      }

    if (mapped && (objfile == NULL))
      {
	warning ("symbol table for '%s' will not be mapped",
		 bfd_get_filename (abfd));
      }
  }
#else	/* defined(NO_MMALLOC) || !defined(HAVE_MMAP) */

  if (mapped)
    {
      warning ("mapped symbol tables are not supported on this machine; missing or broken mmap().");

      /* Turn off the global flag so we don't try to do mapped symbol tables
	 any more, which shuts up gdb unless the user specifically gives the
	 "mapped" keyword again. */

      mapped_symbol_files = 0;
    }

#endif	/* !defined(NO_MMALLOC) && defined(HAVE_MMAP) */

  /* If we don't support mapped symbol files, didn't ask for the file to be
     mapped, or failed to open the mapped file for some reason, then revert
     back to an unmapped objfile. */

  if (objfile == NULL)
    {
      objfile = (struct objfile *) xmalloc (sizeof (struct objfile));
      memset (objfile, 0, sizeof (struct objfile));
      objfile -> md = NULL;
      obstack_specify_allocation (&objfile -> psymbol_cache.cache, 0, 0,
				  xmalloc, free);
      obstack_specify_allocation (&objfile -> psymbol_obstack, 0, 0, xmalloc,
				  free);
      obstack_specify_allocation (&objfile -> symbol_obstack, 0, 0, xmalloc,
				  free);
      obstack_specify_allocation (&objfile -> type_obstack, 0, 0, xmalloc,
				  free);
    }

  /* Update the per-objfile information that comes from the bfd, ensuring
     that any data that is reference is saved in the per-objfile data
     region. */

  objfile -> obfd = abfd;
  if (objfile -> name != NULL)
    {
      mfree (objfile -> md, objfile -> name);
    }
  objfile -> name = mstrsave (objfile -> md, bfd_get_filename (abfd));
  objfile -> mtime = bfd_get_mtime (abfd);

  /* Build section table.  */

  if (build_objfile_section_table (objfile))
    {
      error ("Can't find the file sections in `%s': %s", 
	     objfile -> name, bfd_errmsg (bfd_get_error ()));
    }

  /* Add this file onto the tail of the linked list of other such files. */

  objfile -> next = NULL;
  if (object_files == NULL)
    object_files = objfile;
  else
    {
      for (last_one = object_files;
	   last_one -> next;
	   last_one = last_one -> next);
      last_one -> next = objfile;
    }
  return (objfile);
}

/* Put OBJFILE at the front of the list.  */

void
objfile_to_front (objfile)
     struct objfile *objfile;
{
  struct objfile **objp;
  for (objp = &object_files; *objp != NULL; objp = &((*objp)->next))
    {
      if (*objp == objfile)
	{
	  /* Unhook it from where it is.  */
	  *objp = objfile->next;
	  /* Put it in the front.  */
	  objfile->next = object_files;
	  object_files = objfile;
	  break;
	}
    }
}

/* Unlink OBJFILE from the list of known objfiles, if it is found in the
   list.

   It is not a bug, or error, to call this function if OBJFILE is not known
   to be in the current list.  This is done in the case of mapped objfiles,
   for example, just to ensure that the mapped objfile doesn't appear twice
   in the list.  Since the list is threaded, linking in a mapped objfile
   twice would create a circular list.

   If OBJFILE turns out to be in the list, we zap it's NEXT pointer after
   unlinking it, just to ensure that we have completely severed any linkages
   between the OBJFILE and the list. */

void
unlink_objfile (objfile)
     struct objfile *objfile;
{
  struct objfile** objpp;

  for (objpp = &object_files; *objpp != NULL; objpp = &((*objpp) -> next))
    {
      if (*objpp == objfile) 
	{
	  *objpp = (*objpp) -> next;
	  objfile -> next = NULL;
	  break;
	}
    }
}


/* Destroy an objfile and all the symtabs and psymtabs under it.  Note
   that as much as possible is allocated on the symbol_obstack and
   psymbol_obstack, so that the memory can be efficiently freed.

   Things which we do NOT free because they are not in malloc'd memory
   or not in memory specific to the objfile include:

   	objfile -> sf

   FIXME:  If the objfile is using reusable symbol information (via mmalloc),
   then we need to take into account the fact that more than one process
   may be using the symbol information at the same time (when mmalloc is
   extended to support cooperative locking).  When more than one process
   is using the mapped symbol info, we need to be more careful about when
   we free objects in the reusable area. */

void
free_objfile (objfile)
     struct objfile *objfile;
{
  /* First do any symbol file specific actions required when we are
     finished with a particular symbol file.  Note that if the objfile
     is using reusable symbol information (via mmalloc) then each of
     these routines is responsible for doing the correct thing, either
     freeing things which are valid only during this particular gdb
     execution, or leaving them to be reused during the next one. */

  if (objfile -> sf != NULL)
    {
      (*objfile -> sf -> sym_finish) (objfile);
    }

  /* We always close the bfd. */

  if (objfile -> obfd != NULL)
    {
      char *name = bfd_get_filename (objfile->obfd);
      if (!bfd_close (objfile -> obfd))
	warning ("cannot close \"%s\": %s",
		 name, bfd_errmsg (bfd_get_error ()));
      free (name);
    }

  /* Remove it from the chain of all objfiles. */

  unlink_objfile (objfile);

  /* If we are going to free the runtime common objfile, mark it
     as unallocated.  */

  if (objfile == rt_common_objfile)
    rt_common_objfile = NULL;

  /* Before the symbol table code was redone to make it easier to
     selectively load and remove information particular to a specific
     linkage unit, gdb used to do these things whenever the monolithic
     symbol table was blown away.  How much still needs to be done
     is unknown, but we play it safe for now and keep each action until
     it is shown to be no longer needed. */
     
#if defined (CLEAR_SOLIB)
  CLEAR_SOLIB ();
  /* CLEAR_SOLIB closes the bfd's for any shared libraries.  But
     the to_sections for a core file might refer to those bfd's.  So
     detach any core file.  */
  {
    struct target_ops *t = find_core_target ();
    if (t != NULL)
      (t->to_detach) (NULL, 0);
  }
#endif
  /* I *think* all our callers call clear_symtab_users.  If so, no need
     to call this here.  */
  clear_pc_function_cache ();

  /* The last thing we do is free the objfile struct itself for the
     non-reusable case, or detach from the mapped file for the reusable
     case.  Note that the mmalloc_detach or the mfree is the last thing
     we can do with this objfile. */

#if !defined(NO_MMALLOC) && defined(HAVE_MMAP)

  if (objfile -> flags & OBJF_MAPPED)
    {
      /* Remember the fd so we can close it.  We can't close it before
	 doing the detach, and after the detach the objfile is gone. */
      int mmfd;

      mmfd = objfile -> mmfd;
      mmalloc_detach (objfile -> md);
      objfile = NULL;
      close (mmfd);
    }

#endif	/* !defined(NO_MMALLOC) && defined(HAVE_MMAP) */

  /* If we still have an objfile, then either we don't support reusable
     objfiles or this one was not reusable.  So free it normally. */

  if (objfile != NULL)
    {
      if (objfile -> name != NULL)
	{
	  mfree (objfile -> md, objfile -> name);
	}
      if (objfile->global_psymbols.list)
	mfree (objfile->md, objfile->global_psymbols.list);
      if (objfile->static_psymbols.list)
	mfree (objfile->md, objfile->static_psymbols.list);
      /* Free the obstacks for non-reusable objfiles */
      obstack_free (&objfile -> psymbol_cache.cache, 0);
      obstack_free (&objfile -> psymbol_obstack, 0);
      obstack_free (&objfile -> symbol_obstack, 0);
      obstack_free (&objfile -> type_obstack, 0);
      mfree (objfile -> md, objfile);
      objfile = NULL;
    }
}


/* Free all the object files at once and clean up their users.  */

void
free_all_objfiles ()
{
  struct objfile *objfile, *temp;

  ALL_OBJFILES_SAFE (objfile, temp)
    {
      free_objfile (objfile);
    }
  clear_symtab_users ();
}

/* Relocate OBJFILE to NEW_OFFSETS.  There should be OBJFILE->NUM_SECTIONS
   entries in new_offsets.  */
void
objfile_relocate (objfile, new_offsets)
     struct objfile *objfile;
     struct section_offsets *new_offsets;
{
  struct section_offsets *delta = (struct section_offsets *) alloca
    (sizeof (struct section_offsets)
     + objfile->num_sections * sizeof (delta->offsets));

  {
    int i;
    int something_changed = 0;
    for (i = 0; i < objfile->num_sections; ++i)
      {
	ANOFFSET (delta, i) =
	  ANOFFSET (new_offsets, i) - ANOFFSET (objfile->section_offsets, i);
	if (ANOFFSET (delta, i) != 0)
	  something_changed = 1;
      }
    if (!something_changed)
      return;
  }

  /* OK, get all the symtabs.  */
  {
    struct symtab *s;

    ALL_OBJFILE_SYMTABS (objfile, s)
      {
	struct linetable *l;
	struct blockvector *bv;
	int i;
	
	/* First the line table.  */
	l = LINETABLE (s);
	if (l)
	  {
	    for (i = 0; i < l->nitems; ++i)
	      l->item[i].pc += ANOFFSET (delta, s->block_line_section);
	  }

	/* Don't relocate a shared blockvector more than once.  */
	if (!s->primary)
	  continue;

	bv = BLOCKVECTOR (s);
	for (i = 0; i < BLOCKVECTOR_NBLOCKS (bv); ++i)
	  {
	    struct block *b;
	    int j;
	    
	    b = BLOCKVECTOR_BLOCK (bv, i);
	    BLOCK_START (b) += ANOFFSET (delta, s->block_line_section);
	    BLOCK_END (b) += ANOFFSET (delta, s->block_line_section);

	    for (j = 0; j < BLOCK_NSYMS (b); ++j)
	      {
		struct symbol *sym = BLOCK_SYM (b, j);
		/* The RS6000 code from which this was taken skipped
		   any symbols in STRUCT_NAMESPACE or UNDEF_NAMESPACE.
		   But I'm leaving out that test, on the theory that
		   they can't possibly pass the tests below.  */
		if ((SYMBOL_CLASS (sym) == LOC_LABEL
		     || SYMBOL_CLASS (sym) == LOC_STATIC)
		    && SYMBOL_SECTION (sym) >= 0)
		  {
		    SYMBOL_VALUE_ADDRESS (sym) +=
		      ANOFFSET (delta, SYMBOL_SECTION (sym));
		  }
#ifdef MIPS_EFI_SYMBOL_NAME
		/* Relocate Extra Function Info for ecoff.  */

		else
		  if (SYMBOL_CLASS (sym) == LOC_CONST
		      && SYMBOL_NAMESPACE (sym) == LABEL_NAMESPACE
		      && STRCMP (SYMBOL_NAME (sym), MIPS_EFI_SYMBOL_NAME) == 0)
		    ecoff_relocate_efi (sym, ANOFFSET (delta, s->block_line_section));
#endif
	      }
	  }
      }
  }

  {
    struct partial_symtab *p;

    ALL_OBJFILE_PSYMTABS (objfile, p)
      {
	p->textlow += ANOFFSET (delta, SECT_OFF_TEXT);
	p->texthigh += ANOFFSET (delta, SECT_OFF_TEXT);
      }
  }

  {
    struct partial_symbol **psym;

    for (psym = objfile->global_psymbols.list;
	 psym < objfile->global_psymbols.next;
	 psym++)
      if (SYMBOL_SECTION (*psym) >= 0)
	SYMBOL_VALUE_ADDRESS (*psym) += ANOFFSET (delta, SYMBOL_SECTION (*psym));
    for (psym = objfile->static_psymbols.list;
	 psym < objfile->static_psymbols.next;
	 psym++)
      if (SYMBOL_SECTION (*psym) >= 0)
	SYMBOL_VALUE_ADDRESS (*psym) += ANOFFSET (delta, SYMBOL_SECTION (*psym));
  }

  {
    struct minimal_symbol *msym;
    ALL_OBJFILE_MSYMBOLS (objfile, msym)
      if (SYMBOL_SECTION (msym) >= 0)
	SYMBOL_VALUE_ADDRESS (msym) += ANOFFSET (delta, SYMBOL_SECTION (msym));
  }
  /* Relocating different sections by different amounts may cause the symbols
     to be out of order.  */
  msymbols_sort (objfile);

  {
    int i;
    for (i = 0; i < objfile->num_sections; ++i)
      ANOFFSET (objfile->section_offsets, i) = ANOFFSET (new_offsets, i);
  }

  {
    struct obj_section *s;
    bfd *abfd;

    abfd = objfile->obfd;

    for (s = objfile->sections;
	 s < objfile->sections_end; ++s)
      {
	flagword flags;

	flags = bfd_get_section_flags (abfd, s->the_bfd_section);

	if (flags & SEC_CODE)
	  {
	    s->addr += ANOFFSET (delta, SECT_OFF_TEXT);
	    s->endaddr += ANOFFSET (delta, SECT_OFF_TEXT);
	  }
	else if (flags & (SEC_DATA | SEC_LOAD))
	  {
	    s->addr += ANOFFSET (delta, SECT_OFF_DATA);
	    s->endaddr += ANOFFSET (delta, SECT_OFF_DATA);
	  }
	else if (flags & SEC_ALLOC)
	  {
	    s->addr += ANOFFSET (delta, SECT_OFF_BSS);
	    s->endaddr += ANOFFSET (delta, SECT_OFF_BSS);
	  }
      }
  }

  if (objfile->ei.entry_point != ~0)
    objfile->ei.entry_point += ANOFFSET (delta, SECT_OFF_TEXT);

  if (objfile->ei.entry_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.entry_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT);
      objfile->ei.entry_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT);
    }

  if (objfile->ei.entry_file_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.entry_file_lowpc += ANOFFSET (delta, SECT_OFF_TEXT);
      objfile->ei.entry_file_highpc += ANOFFSET (delta, SECT_OFF_TEXT);
    }

  if (objfile->ei.main_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.main_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT);
      objfile->ei.main_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT);
    }
}

/* Many places in gdb want to test just to see if we have any partial
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_partial_symbols ()
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
    {
      if (ofp -> psymtabs != NULL)
	{
	  return 1;
	}
    }
  return 0;
}

/* Many places in gdb want to test just to see if we have any full
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_full_symbols ()
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
    {
      if (ofp -> symtabs != NULL)
	{
	  return 1;
	}
    }
  return 0;
}

/* Many places in gdb want to test just to see if we have any minimal
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_minimal_symbols ()
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
    {
      if (ofp -> msymbols != NULL)
	{
	  return 1;
	}
    }
  return 0;
}

#if !defined(NO_MMALLOC) && defined(HAVE_MMAP)

/* Given the name of a mapped symbol file in SYMSFILENAME, and the timestamp
   of the corresponding symbol file in MTIME, try to open an existing file
   with the name SYMSFILENAME and verify it is more recent than the base
   file by checking it's timestamp against MTIME.

   If SYMSFILENAME does not exist (or can't be stat'd), simply returns -1.

   If SYMSFILENAME does exist, but is out of date, we check to see if the
   user has specified creation of a mapped file.  If so, we don't issue
   any warning message because we will be creating a new mapped file anyway,
   overwriting the old one.  If not, then we issue a warning message so that
   the user will know why we aren't using this existing mapped symbol file.
   In either case, we return -1.

   If SYMSFILENAME does exist and is not out of date, but can't be opened for
   some reason, then prints an appropriate system error message and returns -1.

   Otherwise, returns the open file descriptor.  */

static int
open_existing_mapped_file (symsfilename, mtime, mapped)
     char *symsfilename;
     long mtime;
     int mapped;
{
  int fd = -1;
  struct stat sbuf;

  if (stat (symsfilename, &sbuf) == 0)
    {
      if (sbuf.st_mtime < mtime)
	{
	  if (!mapped)
	    {
	      warning ("mapped symbol file `%s' is out of date, ignored it",
		       symsfilename);
	    }
	}
      else if ((fd = open (symsfilename, O_RDWR)) < 0)
	{
	  if (error_pre_print)
	    {
	      printf_unfiltered (error_pre_print);
	    }
	  print_sys_errmsg (symsfilename, errno);
	}
    }
  return (fd);
}

/* Look for a mapped symbol file that corresponds to FILENAME and is more
   recent than MTIME.  If MAPPED is nonzero, the user has asked that gdb
   use a mapped symbol file for this file, so create a new one if one does
   not currently exist.

   If found, then return an open file descriptor for the file, otherwise
   return -1.

   This routine is responsible for implementing the policy that generates
   the name of the mapped symbol file from the name of a file containing
   symbols that gdb would like to read.  Currently this policy is to append
   ".syms" to the name of the file.

   This routine is also responsible for implementing the policy that
   determines where the mapped symbol file is found (the search path).
   This policy is that when reading an existing mapped file, a file of
   the correct name in the current directory takes precedence over a
   file of the correct name in the same directory as the symbol file.
   When creating a new mapped file, it is always created in the current
   directory.  This helps to minimize the chances of a user unknowingly
   creating big mapped files in places like /bin and /usr/local/bin, and
   allows a local copy to override a manually installed global copy (in
   /bin for example).  */

static int
open_mapped_file (filename, mtime, mapped)
     char *filename;
     long mtime;
     int mapped;
{
  int fd;
  char *symsfilename;

  /* First try to open an existing file in the current directory, and
     then try the directory where the symbol file is located. */

  symsfilename = concat ("./", basename (filename), ".syms", (char *) NULL);
  if ((fd = open_existing_mapped_file (symsfilename, mtime, mapped)) < 0)
    {
      free (symsfilename);
      symsfilename = concat (filename, ".syms", (char *) NULL);
      fd = open_existing_mapped_file (symsfilename, mtime, mapped);
    }

  /* If we don't have an open file by now, then either the file does not
     already exist, or the base file has changed since it was created.  In
     either case, if the user has specified use of a mapped file, then
     create a new mapped file, truncating any existing one.  If we can't
     create one, print a system error message saying why we can't.

     By default the file is rw for everyone, with the user's umask taking
     care of turning off the permissions the user wants off. */

  if ((fd < 0) && mapped)
    {
      free (symsfilename);
      symsfilename = concat ("./", basename (filename), ".syms",
			     (char *) NULL);
      if ((fd = open (symsfilename, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
	{
	  if (error_pre_print)
	    {
	      printf_unfiltered (error_pre_print);
	    }
	  print_sys_errmsg (symsfilename, errno);
	}
    }

  free (symsfilename);
  return (fd);
}

/* Return the base address at which we would like the next objfile's
   mapped data to start.

   For now, we use the kludge that the configuration specifies a base
   address to which it is safe to map the first mmalloc heap, and an
   increment to add to this address for each successive heap.  There are
   a lot of issues to deal with here to make this work reasonably, including:

     Avoid memory collisions with existing mapped address spaces

     Reclaim address spaces when their mmalloc heaps are unmapped

     When mmalloc heaps are shared between processes they have to be
     mapped at the same addresses in each

     Once created, a mmalloc heap that is to be mapped back in must be
     mapped at the original address.  I.E. each objfile will expect to
     be remapped at it's original address.  This becomes a problem if
     the desired address is already in use.

     etc, etc, etc.

 */


static CORE_ADDR
map_to_address ()
{

#if defined(MMAP_BASE_ADDRESS) && defined (MMAP_INCREMENT)

  static CORE_ADDR next = MMAP_BASE_ADDRESS;
  CORE_ADDR mapto = next;

  next += MMAP_INCREMENT;
  return (mapto);

#else

  warning ("need to recompile gdb with MMAP_BASE_ADDRESS and MMAP_INCREMENT defined");
  return (0);

#endif

}

#endif	/* !defined(NO_MMALLOC) && defined(HAVE_MMAP) */

/* Returns a section whose range includes PC or NULL if none found. */

struct obj_section *
find_pc_section(pc)
     CORE_ADDR pc;
{
  struct obj_section *s;
  struct objfile *objfile;
  
  ALL_OBJFILES (objfile)
    for (s = objfile->sections; s < objfile->sections_end; ++s)
      if (s->addr <= pc
	  && pc < s->endaddr)
	return(s);

  return(NULL);
}

/* In SVR4, we recognize a trampoline by it's section name. 
   That is, if the pc is in a section named ".plt" then we are in
   a trampoline.  */

int
in_plt_section(pc, name)
     CORE_ADDR pc;
     char *name;
{
  struct obj_section *s;
  int retval = 0;
  
  s = find_pc_section(pc);
  
  retval = (s != NULL
	    && s->the_bfd_section->name != NULL
	    && STREQ (s->the_bfd_section->name, ".plt"));
  return(retval);
}
