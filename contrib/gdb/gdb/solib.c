/* Handle SunOS and SVR4 shared libraries for GDB, the GNU Debugger.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996
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

/* This file is only compilable if link.h is available. */

#ifdef HAVE_LINK_H

#include <sys/types.h>
#include <signal.h>
#include "gdb_string.h"
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef SVR4_SHARED_LIBS
 /* SunOS shared libs need the nlist structure.  */
#include <a.out.h> 
#else
#include "elf/external.h"
#include "elf/common.h"
#endif

#include <link.h>

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "command.h"
#include "target.h"
#include "frame.h"
#include "gnu-regex.h"
#include "inferior.h"
#include "environ.h"
#include "language.h"
#include "gdbcmd.h"

#define MAX_PATH_SIZE 512		/* FIXME: Should be dynamic */

/* On SVR4 systems, a list of symbols in the dynamic linker where
   GDB can try to place a breakpoint to monitor shared library
   events.

   If none of these symbols are found, or other errors occur, then
   SVR4 systems will fall back to using a symbol as the "startup
   mapping complete" breakpoint address.  */

#ifdef SVR4_SHARED_LIBS
static char *solib_break_names[] = {
  "r_debug_state",
  "_r_debug_state",
  "_dl_debug_state",
  NULL
};
#endif

#define BKPT_AT_SYMBOL 1

#if defined (BKPT_AT_SYMBOL) && defined (SVR4_SHARED_LIBS)
static char *bkpt_names[] = {
#ifdef SOLIB_BKPT_NAME
  SOLIB_BKPT_NAME,		/* Prefer configured name if it exists. */
#endif
  "_start",
  "main",
  NULL
};
#endif

/* Symbols which are used to locate the base of the link map structures. */

#ifndef SVR4_SHARED_LIBS
static char *debug_base_symbols[] = {
  "_DYNAMIC",
  "_DYNAMIC__MGC",
  NULL
};
#endif

static char *main_name_list[] = {
  "main_$main",
  NULL
};

/* local data declarations */

#ifndef SVR4_SHARED_LIBS

#define LM_ADDR(so) ((so) -> lm.lm_addr)
#define LM_NEXT(so) ((so) -> lm.lm_next)
#define LM_NAME(so) ((so) -> lm.lm_name)
/* Test for first link map entry; first entry is a shared library. */
#define IGNORE_FIRST_LINK_MAP_ENTRY(x) (0)
static struct link_dynamic dynamic_copy;
static struct link_dynamic_2 ld_2_copy;
static struct ld_debug debug_copy;
static CORE_ADDR debug_addr;
static CORE_ADDR flag_addr;

#else	/* SVR4_SHARED_LIBS */

#define LM_ADDR(so) ((so) -> lm.l_addr)
#define LM_NEXT(so) ((so) -> lm.l_next)
#define LM_NAME(so) ((so) -> lm.l_name)
/* Test for first link map entry; first entry is the exec-file. */
#define IGNORE_FIRST_LINK_MAP_ENTRY(x) ((x).l_prev == NULL)
static struct r_debug debug_copy;
char shadow_contents[BREAKPOINT_MAX];	/* Stash old bkpt addr contents */

#endif	/* !SVR4_SHARED_LIBS */

struct so_list {
  struct so_list *next;			/* next structure in linked list */
  struct link_map lm;			/* copy of link map from inferior */
  struct link_map *lmaddr;		/* addr in inferior lm was read from */
  CORE_ADDR lmend;			/* upper addr bound of mapped object */
  char so_name[MAX_PATH_SIZE];		/* shared object lib name (FIXME) */
  char symbols_loaded;			/* flag: symbols read in yet? */
  char from_tty;			/* flag: print msgs? */
  struct objfile *objfile;		/* objfile for loaded lib */
  struct section_table *sections;
  struct section_table *sections_end;
  struct section_table *textsection;
  bfd *abfd;
};

static struct so_list *so_list_head;	/* List of known shared objects */
static CORE_ADDR debug_base;		/* Base of dynamic linker structures */
static CORE_ADDR breakpoint_addr;	/* Address where end bkpt is set */

extern int
fdmatch PARAMS ((int, int));		/* In libiberty */

/* Local function prototypes */

static void
special_symbol_handling PARAMS ((struct so_list *));

static void
sharedlibrary_command PARAMS ((char *, int));

static int
enable_break PARAMS ((void));

static void
info_sharedlibrary_command PARAMS ((char *, int));

static int
symbol_add_stub PARAMS ((char *));

static struct so_list *
find_solib PARAMS ((struct so_list *));

static struct link_map *
first_link_map_member PARAMS ((void));

static CORE_ADDR
locate_base PARAMS ((void));

static void
solib_map_sections PARAMS ((struct so_list *));

#ifdef SVR4_SHARED_LIBS

static CORE_ADDR
elf_locate_base PARAMS ((void));

#else

static int
disable_break PARAMS ((void));

static void
allocate_rt_common_objfile PARAMS ((void));

static void
solib_add_common_symbols PARAMS ((struct rtc_symb *));

#endif

/*

LOCAL FUNCTION

	solib_map_sections -- open bfd and build sections for shared lib

SYNOPSIS

	static void solib_map_sections (struct so_list *so)

DESCRIPTION

	Given a pointer to one of the shared objects in our list
	of mapped objects, use the recorded name to open a bfd
	descriptor for the object, build a section table, and then
	relocate all the section addresses by the base address at
	which the shared object was mapped.

FIXMES

	In most (all?) cases the shared object file name recorded in the
	dynamic linkage tables will be a fully qualified pathname.  For
	cases where it isn't, do we really mimic the systems search
	mechanism correctly in the below code (particularly the tilde
	expansion stuff?).
 */

static void
solib_map_sections (so)
     struct so_list *so;
{
  char *filename;
  char *scratch_pathname;
  int scratch_chan;
  struct section_table *p;
  struct cleanup *old_chain;
  bfd *abfd;
  
  filename = tilde_expand (so -> so_name);
  old_chain = make_cleanup (free, filename);
  
  scratch_chan = openp (get_in_environ (inferior_environ, "PATH"), 
		        1, filename, O_RDONLY, 0, &scratch_pathname);
  if (scratch_chan < 0)
    {
      scratch_chan = openp (get_in_environ 
			    (inferior_environ, "LD_LIBRARY_PATH"), 
			    1, filename, O_RDONLY, 0, &scratch_pathname);
    }
  if (scratch_chan < 0)
    {
      perror_with_name (filename);
    }
  /* Leave scratch_pathname allocated.  abfd->name will point to it.  */

  abfd = bfd_fdopenr (scratch_pathname, gnutarget, scratch_chan);
  if (!abfd)
    {
      close (scratch_chan);
      error ("Could not open `%s' as an executable file: %s",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  /* Leave bfd open, core_xfer_memory and "info files" need it.  */
  so -> abfd = abfd;
  abfd -> cacheable = true;

  /* copy full path name into so_name, so that later symbol_file_add can find
     it */
  if (strlen (scratch_pathname) >= MAX_PATH_SIZE)
    error ("Full path name length of shared library exceeds MAX_PATH_SIZE in so_list structure.");
  strcpy (so->so_name, scratch_pathname);

  if (!bfd_check_format (abfd, bfd_object))
    {
      error ("\"%s\": not in executable format: %s.",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  if (build_section_table (abfd, &so -> sections, &so -> sections_end))
    {
      error ("Can't find the file sections in `%s': %s", 
	     bfd_get_filename (abfd), bfd_errmsg (bfd_get_error ()));
    }

  for (p = so -> sections; p < so -> sections_end; p++)
    {
      /* Relocate the section binding addresses as recorded in the shared
	 object's file by the base address to which the object was actually
	 mapped. */
      p -> addr += (CORE_ADDR) LM_ADDR (so);
      p -> endaddr += (CORE_ADDR) LM_ADDR (so);
      so -> lmend = (CORE_ADDR) max (p -> endaddr, so -> lmend);
      if (STREQ (p -> the_bfd_section -> name, ".text"))
	{
	  so -> textsection = p;
	}
    }

  /* Free the file names, close the file now.  */
  do_cleanups (old_chain);
}

#ifndef SVR4_SHARED_LIBS

/* Allocate the runtime common object file.  */

static void
allocate_rt_common_objfile ()
{
  struct objfile *objfile;
  struct objfile *last_one;

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
  objfile -> name = mstrsave (objfile -> md, "rt_common");

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

  rt_common_objfile = objfile;
}

/* Read all dynamically loaded common symbol definitions from the inferior
   and put them into the minimal symbol table for the runtime common
   objfile.  */

static void
solib_add_common_symbols (rtc_symp)
    struct rtc_symb *rtc_symp;
{
  struct rtc_symb inferior_rtc_symb;
  struct nlist inferior_rtc_nlist;
  int len;
  char *name;
  char *origname;

  /* Remove any runtime common symbols from previous runs.  */

  if (rt_common_objfile != NULL && rt_common_objfile -> minimal_symbol_count)
    {
      obstack_free (&rt_common_objfile -> symbol_obstack, 0);
      obstack_specify_allocation (&rt_common_objfile -> symbol_obstack, 0, 0,
				  xmalloc, free);
      rt_common_objfile -> minimal_symbol_count = 0;
      rt_common_objfile -> msymbols = NULL;
    }

  init_minimal_symbol_collection ();
  make_cleanup (discard_minimal_symbols, 0);

  while (rtc_symp)
    {
      read_memory ((CORE_ADDR) rtc_symp,
		   (char *) &inferior_rtc_symb,
		   sizeof (inferior_rtc_symb));
      read_memory ((CORE_ADDR) inferior_rtc_symb.rtc_sp,
		   (char *) &inferior_rtc_nlist,
		   sizeof(inferior_rtc_nlist));
      if (inferior_rtc_nlist.n_type == N_COMM)
	{
	  /* FIXME: The length of the symbol name is not available, but in the
	     current implementation the common symbol is allocated immediately
	     behind the name of the symbol. */
	  len = inferior_rtc_nlist.n_value - inferior_rtc_nlist.n_un.n_strx;

	  origname = name = xmalloc (len);
	  read_memory ((CORE_ADDR) inferior_rtc_nlist.n_un.n_name, name, len);

	  /* Allocate the runtime common objfile if necessary. */
	  if (rt_common_objfile == NULL)
	    allocate_rt_common_objfile ();

	  name = obsavestring (name, strlen (name),
			       &rt_common_objfile -> symbol_obstack);
	  prim_record_minimal_symbol (name, inferior_rtc_nlist.n_value,
				      mst_bss, rt_common_objfile);
	  free (origname);
	}
      rtc_symp = inferior_rtc_symb.rtc_next;
    }

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for the runtime common objfile.  */

  install_minimal_symbols (rt_common_objfile);
}

#endif	/* SVR4_SHARED_LIBS */


#ifdef SVR4_SHARED_LIBS

static CORE_ADDR
bfd_lookup_symbol PARAMS ((bfd *, char *));

/*

LOCAL FUNCTION

	bfd_lookup_symbol -- lookup the value for a specific symbol

SYNOPSIS

	CORE_ADDR bfd_lookup_symbol (bfd *abfd, char *symname)

DESCRIPTION

	An expensive way to lookup the value of a single symbol for
	bfd's that are only temporary anyway.  This is used by the
	shared library support to find the address of the debugger
	interface structures in the shared library.

	Note that 0 is specifically allowed as an error return (no
	such symbol).
*/

static CORE_ADDR
bfd_lookup_symbol (abfd, symname)
     bfd *abfd;
     char *symname;
{
  unsigned int storage_needed;
  asymbol *sym;
  asymbol **symbol_table;
  unsigned int number_of_symbols;
  unsigned int i;
  struct cleanup *back_to;
  CORE_ADDR symaddr = 0;
  
  storage_needed = bfd_get_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (free, (PTR)symbol_table);
      number_of_symbols = bfd_canonicalize_symtab (abfd, symbol_table); 
  
      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = *symbol_table++;
	  if (STREQ (sym -> name, symname))
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym -> value + sym -> section -> vma;
	      break;
	    }
	}
      do_cleanups (back_to);
    }

  if (symaddr) return (symaddr);

  storage_needed = bfd_get_dynamic_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (free, (PTR)symbol_table);
      number_of_symbols = bfd_canonicalize_dynamic_symtab (abfd, symbol_table); 
  
      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = *symbol_table++;
	  if (STREQ (sym -> name, symname))
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym -> value + sym -> section -> vma;
	      break;
	    }
	}
      do_cleanups (back_to);
    }
  return (symaddr);
}

#ifdef HANDLE_SVR4_EXEC_EMULATORS

/*
	Solaris BCP (the part of Solaris which allows it to run SunOS4
	a.out files) throws in another wrinkle. Solaris does not fill
	in the usual a.out link map structures when running BCP programs,
	the only way to get at them is via groping around in the dynamic
	linker.
	The dynamic linker and it's structures are located in the shared
	C library, which gets run as the executable's "interpreter" by
	the kernel.

	Note that we can assume nothing about the process state at the time
	we need to find these structures.  We may be stopped on the first
	instruction of the interpreter (C shared library), the first
	instruction of the executable itself, or somewhere else entirely
	(if we attached to the process for example).
*/

static char *debug_base_symbols[] = {
  "r_debug",	/* Solaris 2.3 */
  "_r_debug",	/* Solaris 2.1, 2.2 */
  NULL
};

static int
look_for_base PARAMS ((int, CORE_ADDR));

/*

LOCAL FUNCTION

	look_for_base -- examine file for each mapped address segment

SYNOPSYS

	static int look_for_base (int fd, CORE_ADDR baseaddr)

DESCRIPTION

	This function is passed to proc_iterate_over_mappings, which
	causes it to get called once for each mapped address space, with
	an open file descriptor for the file mapped to that space, and the
	base address of that mapped space.

	Our job is to find the debug base symbol in the file that this
	fd is open on, if it exists, and if so, initialize the dynamic
	linker structure base address debug_base.

	Note that this is a computationally expensive proposition, since
	we basically have to open a bfd on every call, so we specifically
	avoid opening the exec file.
 */

static int
look_for_base (fd, baseaddr)
     int fd;
     CORE_ADDR baseaddr;
{
  bfd *interp_bfd;
  CORE_ADDR address = 0;
  char **symbolp;

  /* If the fd is -1, then there is no file that corresponds to this
     mapped memory segment, so skip it.  Also, if the fd corresponds
     to the exec file, skip it as well. */

  if (fd == -1
      || (exec_bfd != NULL
	  && fdmatch (fileno ((GDB_FILE *)(exec_bfd -> iostream)), fd)))
    {
      return (0);
    }

  /* Try to open whatever random file this fd corresponds to.  Note that
     we have no way currently to find the filename.  Don't gripe about
     any problems we might have, just fail. */

  if ((interp_bfd = bfd_fdopenr ("unnamed", gnutarget, fd)) == NULL)
    {
      return (0);
    }
  if (!bfd_check_format (interp_bfd, bfd_object))
    {
      /* FIXME-leak: on failure, might not free all memory associated with
	 interp_bfd.  */
      bfd_close (interp_bfd);
      return (0);
    }

  /* Now try to find our debug base symbol in this file, which we at
     least know to be a valid ELF executable or shared library. */

  for (symbolp = debug_base_symbols; *symbolp != NULL; symbolp++)
    {
      address = bfd_lookup_symbol (interp_bfd, *symbolp);
      if (address != 0)
	{
	  break;
	}
    }
  if (address == 0)
    {
      /* FIXME-leak: on failure, might not free all memory associated with
	 interp_bfd.  */
      bfd_close (interp_bfd);
      return (0);
    }

  /* Eureka!  We found the symbol.  But now we may need to relocate it
     by the base address.  If the symbol's value is less than the base
     address of the shared library, then it hasn't yet been relocated
     by the dynamic linker, and we have to do it ourself.  FIXME: Note
     that we make the assumption that the first segment that corresponds
     to the shared library has the base address to which the library
     was relocated. */

  if (address < baseaddr)
    {
      address += baseaddr;
    }
  debug_base = address;
  /* FIXME-leak: on failure, might not free all memory associated with
     interp_bfd.  */
  bfd_close (interp_bfd);
  return (1);
}
#endif /* HANDLE_SVR4_EXEC_EMULATORS */

/*

LOCAL FUNCTION

	elf_locate_base -- locate the base address of dynamic linker structs
	for SVR4 elf targets.

SYNOPSIS

	CORE_ADDR elf_locate_base (void)

DESCRIPTION

	For SVR4 elf targets the address of the dynamic linker's runtime
	structure is contained within the dynamic info section in the
	executable file.  The dynamic section is also mapped into the
	inferior address space.  Because the runtime loader fills in the
	real address before starting the inferior, we have to read in the
	dynamic info section from the inferior address space.
	If there are any errors while trying to find the address, we
	silently return 0, otherwise the found address is returned.

 */

static CORE_ADDR
elf_locate_base ()
{
  sec_ptr dyninfo_sect;
  int dyninfo_sect_size;
  CORE_ADDR dyninfo_addr;
  char *buf;
  char *bufend;

  /* Find the start address of the .dynamic section.  */
  dyninfo_sect = bfd_get_section_by_name (exec_bfd, ".dynamic");
  if (dyninfo_sect == NULL)
    return 0;
  dyninfo_addr = bfd_section_vma (exec_bfd, dyninfo_sect);

  /* Read in .dynamic section, silently ignore errors.  */
  dyninfo_sect_size = bfd_section_size (exec_bfd, dyninfo_sect);
  buf = alloca (dyninfo_sect_size);
  if (target_read_memory (dyninfo_addr, buf, dyninfo_sect_size))
    return 0;

  /* Find the DT_DEBUG entry in the the .dynamic section.
     For mips elf we look for DT_MIPS_RLD_MAP, mips elf apparently has
     no DT_DEBUG entries.  */
  /* FIXME: In lack of a 64 bit ELF ABI the following code assumes
     a 32 bit ELF ABI target.  */
  for (bufend = buf + dyninfo_sect_size;
       buf < bufend;
       buf += sizeof (Elf32_External_Dyn))
    {
      Elf32_External_Dyn *x_dynp = (Elf32_External_Dyn *)buf;
      long dyn_tag;
      CORE_ADDR dyn_ptr;

      dyn_tag = bfd_h_get_32 (exec_bfd, (bfd_byte *) x_dynp->d_tag);
      if (dyn_tag == DT_NULL)
	break;
      else if (dyn_tag == DT_DEBUG)
	{
	  dyn_ptr = bfd_h_get_32 (exec_bfd, (bfd_byte *) x_dynp->d_un.d_ptr);
	  return dyn_ptr;
	}
#ifdef DT_MIPS_RLD_MAP
      else if (dyn_tag == DT_MIPS_RLD_MAP)
	{
	  char pbuf[TARGET_PTR_BIT / HOST_CHAR_BIT];

	  /* DT_MIPS_RLD_MAP contains a pointer to the address
	     of the dynamic link structure.  */
	  dyn_ptr = bfd_h_get_32 (exec_bfd, (bfd_byte *) x_dynp->d_un.d_ptr);
	  if (target_read_memory (dyn_ptr, pbuf, sizeof (pbuf)))
	    return 0;
	  return extract_unsigned_integer (pbuf, sizeof (pbuf));
	}
#endif
    }

  /* DT_DEBUG entry not found.  */
  return 0;
}

#endif	/* SVR4_SHARED_LIBS */

/*

LOCAL FUNCTION

	locate_base -- locate the base address of dynamic linker structs

SYNOPSIS

	CORE_ADDR locate_base (void)

DESCRIPTION

	For both the SunOS and SVR4 shared library implementations, if the
	inferior executable has been linked dynamically, there is a single
	address somewhere in the inferior's data space which is the key to
	locating all of the dynamic linker's runtime structures.  This
	address is the value of the debug base symbol.  The job of this
	function is to find and return that address, or to return 0 if there
	is no such address (the executable is statically linked for example).

	For SunOS, the job is almost trivial, since the dynamic linker and
	all of it's structures are statically linked to the executable at
	link time.  Thus the symbol for the address we are looking for has
	already been added to the minimal symbol table for the executable's
	objfile at the time the symbol file's symbols were read, and all we
	have to do is look it up there.  Note that we explicitly do NOT want
	to find the copies in the shared library.

	The SVR4 version is a bit more complicated because the address
	is contained somewhere in the dynamic info section.  We have to go
	to a lot more work to discover the address of the debug base symbol.
	Because of this complexity, we cache the value we find and return that
	value on subsequent invocations.  Note there is no copy in the
	executable symbol tables.

 */

static CORE_ADDR
locate_base ()
{

#ifndef SVR4_SHARED_LIBS

  struct minimal_symbol *msymbol;
  CORE_ADDR address = 0;
  char **symbolp;

  /* For SunOS, we want to limit the search for the debug base symbol to the
     executable being debugged, since there is a duplicate named symbol in the
     shared library.  We don't want the shared library versions. */

  for (symbolp = debug_base_symbols; *symbolp != NULL; symbolp++)
    {
      msymbol = lookup_minimal_symbol (*symbolp, NULL, symfile_objfile);
      if ((msymbol != NULL) && (SYMBOL_VALUE_ADDRESS (msymbol) != 0))
	{
	  address = SYMBOL_VALUE_ADDRESS (msymbol);
	  return (address);
	}
    }
  return (0);

#else	/* SVR4_SHARED_LIBS */

  /* Check to see if we have a currently valid address, and if so, avoid
     doing all this work again and just return the cached address.  If
     we have no cached address, try to locate it in the dynamic info
     section for ELF executables.  */

  if (debug_base == 0)
    {
      if (exec_bfd != NULL
	  && bfd_get_flavour (exec_bfd) == bfd_target_elf_flavour)
	debug_base = elf_locate_base ();
#ifdef HANDLE_SVR4_EXEC_EMULATORS
      /* Try it the hard way for emulated executables.  */
      else if (inferior_pid != 0)
	proc_iterate_over_mappings (look_for_base);
#endif
    }
  return (debug_base);

#endif	/* !SVR4_SHARED_LIBS */

}

/*

LOCAL FUNCTION

	first_link_map_member -- locate first member in dynamic linker's map

SYNOPSIS

	static struct link_map *first_link_map_member (void)

DESCRIPTION

	Read in a copy of the first member in the inferior's dynamic
	link map from the inferior's dynamic linker structures, and return
	a pointer to the copy in our address space.
*/

static struct link_map *
first_link_map_member ()
{
  struct link_map *lm = NULL;

#ifndef SVR4_SHARED_LIBS

  read_memory (debug_base, (char *) &dynamic_copy, sizeof (dynamic_copy));
  if (dynamic_copy.ld_version >= 2)
    {
      /* It is a version that we can deal with, so read in the secondary
	 structure and find the address of the link map list from it. */
      read_memory ((CORE_ADDR) dynamic_copy.ld_un.ld_2, (char *) &ld_2_copy,
		   sizeof (struct link_dynamic_2));
      lm = ld_2_copy.ld_loaded;
    }

#else	/* SVR4_SHARED_LIBS */

  read_memory (debug_base, (char *) &debug_copy, sizeof (struct r_debug));
  /* FIXME:  Perhaps we should validate the info somehow, perhaps by
     checking r_version for a known version number, or r_state for
     RT_CONSISTENT. */
  lm = debug_copy.r_map;

#endif	/* !SVR4_SHARED_LIBS */

  return (lm);
}

/*

LOCAL FUNCTION

	find_solib -- step through list of shared objects

SYNOPSIS

	struct so_list *find_solib (struct so_list *so_list_ptr)

DESCRIPTION

	This module contains the routine which finds the names of any
	loaded "images" in the current process. The argument in must be
	NULL on the first call, and then the returned value must be passed
	in on subsequent calls. This provides the capability to "step" down
	the list of loaded objects. On the last object, a NULL value is
	returned.

	The arg and return value are "struct link_map" pointers, as defined
	in <link.h>.
 */

static struct so_list *
find_solib (so_list_ptr)
     struct so_list *so_list_ptr;	/* Last lm or NULL for first one */
{
  struct so_list *so_list_next = NULL;
  struct link_map *lm = NULL;
  struct so_list *new;
  
  if (so_list_ptr == NULL)
    {
      /* We are setting up for a new scan through the loaded images. */
      if ((so_list_next = so_list_head) == NULL)
	{
	  /* We have not already read in the dynamic linking structures
	     from the inferior, lookup the address of the base structure. */
	  debug_base = locate_base ();
	  if (debug_base != 0)
	    {
	      /* Read the base structure in and find the address of the first
		 link map list member. */
	      lm = first_link_map_member ();
	    }
	}
    }
  else
    {
      /* We have been called before, and are in the process of walking
	 the shared library list.  Advance to the next shared object. */
      if ((lm = LM_NEXT (so_list_ptr)) == NULL)
	{
	  /* We have hit the end of the list, so check to see if any were
	     added, but be quiet if we can't read from the target any more. */
	  int status = target_read_memory ((CORE_ADDR) so_list_ptr -> lmaddr,
					   (char *) &(so_list_ptr -> lm),
					   sizeof (struct link_map));
	  if (status == 0)
	    {
	      lm = LM_NEXT (so_list_ptr);
	    }
	  else
	    {
	      lm = NULL;
	    }
	}
      so_list_next = so_list_ptr -> next;
    }
  if ((so_list_next == NULL) && (lm != NULL))
    {
      /* Get next link map structure from inferior image and build a local
	 abbreviated load_map structure */
      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      memset ((char *) new, 0, sizeof (struct so_list));
      new -> lmaddr = lm;
      /* Add the new node as the next node in the list, or as the root
	 node if this is the first one. */
      if (so_list_ptr != NULL)
	{
	  so_list_ptr -> next = new;
	}
      else
	{
	  so_list_head = new;
	}      
      so_list_next = new;
      read_memory ((CORE_ADDR) lm, (char *) &(new -> lm),
		   sizeof (struct link_map));
      /* For SVR4 versions, the first entry in the link map is for the
	 inferior executable, so we must ignore it.  For some versions of
	 SVR4, it has no name.  For others (Solaris 2.3 for example), it
	 does have a name, so we can no longer use a missing name to
	 decide when to ignore it. */
      if (!IGNORE_FIRST_LINK_MAP_ENTRY (new -> lm))
	{
	  int errcode;
	  char *buffer;
	  target_read_string ((CORE_ADDR) LM_NAME (new), &buffer,
			      MAX_PATH_SIZE - 1, &errcode);
	  if (errcode != 0)
	    error ("find_solib: Can't read pathname for load map: %s\n",
		   safe_strerror (errcode));
	  strncpy (new -> so_name, buffer, MAX_PATH_SIZE - 1);
	  new -> so_name[MAX_PATH_SIZE - 1] = '\0';
	  free (buffer);
	  solib_map_sections (new);
	}      
    }
  return (so_list_next);
}

/* A small stub to get us past the arg-passing pinhole of catch_errors.  */

static int
symbol_add_stub (arg)
     char *arg;
{
  register struct so_list *so = (struct so_list *) arg;	/* catch_errs bogon */
  
  so -> objfile =
    symbol_file_add (so -> so_name, so -> from_tty,
		     (so->textsection == NULL
		      ? 0
		      : (unsigned int) so -> textsection -> addr),
		     0, 0, 0);
  return (1);
}

/* This function will check the so name to see if matches the main list.
   In some system the main object is in the list, which we want to exclude */

static int match_main (soname)
    char *soname;
{
  char **mainp;

  for (mainp = main_name_list; *mainp != NULL; mainp++)
    {
      if (strcmp (soname, *mainp) == 0)
	return (1);
    }

  return (0);
}

/*

GLOBAL FUNCTION

	solib_add -- add a shared library file to the symtab and section list

SYNOPSIS

	void solib_add (char *arg_string, int from_tty,
			struct target_ops *target)

DESCRIPTION

*/

void
solib_add (arg_string, from_tty, target)
     char *arg_string;
     int from_tty;
     struct target_ops *target;
{	
  register struct so_list *so = NULL;   	/* link map state variable */

  /* Last shared library that we read.  */
  struct so_list *so_last = NULL;

  char *re_err;
  int count;
  int old;
  
  if ((re_err = re_comp (arg_string ? arg_string : ".")) != NULL)
    {
      error ("Invalid regexp: %s", re_err);
    }
  
  /* Add the shared library sections to the section table of the
     specified target, if any.  */
  if (target)
    {
      /* Count how many new section_table entries there are.  */
      so = NULL;
      count = 0;
      while ((so = find_solib (so)) != NULL)
	{
	  if (so -> so_name[0] && !match_main (so -> so_name))
	    {
	      count += so -> sections_end - so -> sections;
	    }
	}
      
      if (count)
	{
	  int update_coreops;

	  /* We must update the to_sections field in the core_ops structure
	     here, otherwise we dereference a potential dangling pointer
	     for each call to target_read/write_memory within this routine.  */
	  update_coreops = core_ops.to_sections == target->to_sections;
	     
	  /* Reallocate the target's section table including the new size.  */
	  if (target -> to_sections)
	    {
	      old = target -> to_sections_end - target -> to_sections;
	      target -> to_sections = (struct section_table *)
		xrealloc ((char *)target -> to_sections,
			 (sizeof (struct section_table)) * (count + old));
	    }
	  else
	    {
	      old = 0;
	      target -> to_sections = (struct section_table *)
		xmalloc ((sizeof (struct section_table)) * count);
	    }
	  target -> to_sections_end = target -> to_sections + (count + old);
	  
	  /* Update the to_sections field in the core_ops structure
	     if needed.  */
	  if (update_coreops)
	    {
	      core_ops.to_sections = target->to_sections;
	      core_ops.to_sections_end = target->to_sections_end;
	    }

	  /* Add these section table entries to the target's table.  */
	  while ((so = find_solib (so)) != NULL)
	    {
	      if (so -> so_name[0])
		{
		  count = so -> sections_end - so -> sections;
		  memcpy ((char *) (target -> to_sections + old),
			  so -> sections, 
			  (sizeof (struct section_table)) * count);
		  old += count;
		}
	    }
	}
    }
  
  /* Now add the symbol files.  */
  while ((so = find_solib (so)) != NULL)
    {
      if (so -> so_name[0] && re_exec (so -> so_name) && 
      !match_main (so -> so_name))
	{
	  so -> from_tty = from_tty;
	  if (so -> symbols_loaded)
	    {
	      if (from_tty)
		{
		  printf_unfiltered ("Symbols already loaded for %s\n", so -> so_name);
		}
	    }
	  else if (catch_errors
		   (symbol_add_stub, (char *) so,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
	    {
	      so_last = so;
	      so -> symbols_loaded = 1;
	    }
	}
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  if (so_last)
    reinit_frame_cache ();

  if (so_last)
    special_symbol_handling (so_last);
}

/*

LOCAL FUNCTION

	info_sharedlibrary_command -- code for "info sharedlibrary"

SYNOPSIS

	static void info_sharedlibrary_command ()

DESCRIPTION

	Walk through the shared library list and print information
	about each attached library.
*/

static void
info_sharedlibrary_command (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  register struct so_list *so = NULL;  	/* link map state variable */
  int header_done = 0;
  
  if (exec_bfd == NULL)
    {
      printf_unfiltered ("No exec file.\n");
      return;
    }
  while ((so = find_solib (so)) != NULL)
    {
      if (so -> so_name[0])
	{
	  if (!header_done)
	    {
	      printf_unfiltered("%-12s%-12s%-12s%s\n", "From", "To", "Syms Read",
		     "Shared Object Library");
	      header_done++;
	    }
	  /* FIXME-32x64: need print_address_numeric with field width or
	     some such.  */
	  printf_unfiltered ("%-12s",
		  local_hex_string_custom ((unsigned long) LM_ADDR (so),
					   "08l"));
	  printf_unfiltered ("%-12s",
		  local_hex_string_custom ((unsigned long) so -> lmend,
					   "08l"));
	  printf_unfiltered ("%-12s", so -> symbols_loaded ? "Yes" : "No");
	  printf_unfiltered ("%s\n",  so -> so_name);
	}
    }
  if (so_list_head == NULL)
    {
      printf_unfiltered ("No shared libraries loaded at this time.\n");	
    }
}

/*

GLOBAL FUNCTION

	solib_address -- check to see if an address is in a shared lib

SYNOPSIS

	char * solib_address (CORE_ADDR address)

DESCRIPTION

	Provides a hook for other gdb routines to discover whether or
	not a particular address is within the mapped address space of
	a shared library.  Any address between the base mapping address
	and the first address beyond the end of the last mapping, is
	considered to be within the shared library address space, for
	our purposes.

	For example, this routine is called at one point to disable
	breakpoints which are in shared libraries that are not currently
	mapped in.
 */

char *
solib_address (address)
     CORE_ADDR address;
{
  register struct so_list *so = 0;   	/* link map state variable */
  
  while ((so = find_solib (so)) != NULL)
    {
      if (so -> so_name[0])
	{
	  if ((address >= (CORE_ADDR) LM_ADDR (so)) &&
	      (address < (CORE_ADDR) so -> lmend))
	    return (so->so_name);
	}
    }
  return (0);
}

/* Called by free_all_symtabs */

void 
clear_solib()
{
  struct so_list *next;
  char *bfd_filename;
  
  while (so_list_head)
    {
      if (so_list_head -> sections)
	{
	  free ((PTR)so_list_head -> sections);
	}
      if (so_list_head -> abfd)
	{
	  bfd_filename = bfd_get_filename (so_list_head -> abfd);
	  if (!bfd_close (so_list_head -> abfd))
	    warning ("cannot close \"%s\": %s",
		     bfd_filename, bfd_errmsg (bfd_get_error ()));
	}
      else
	/* This happens for the executable on SVR4.  */
	bfd_filename = NULL;
      
      next = so_list_head -> next;
      if (bfd_filename)
	free ((PTR)bfd_filename);
      free ((PTR)so_list_head);
      so_list_head = next;
    }
  debug_base = 0;
}

/*

LOCAL FUNCTION

	disable_break -- remove the "mapping changed" breakpoint

SYNOPSIS

	static int disable_break ()

DESCRIPTION

	Removes the breakpoint that gets hit when the dynamic linker
	completes a mapping change.

*/

#ifndef SVR4_SHARED_LIBS

static int
disable_break ()
{
  int status = 1;

#ifndef SVR4_SHARED_LIBS

  int in_debugger = 0;
  
  /* Read the debugger structure from the inferior to retrieve the
     address of the breakpoint and the original contents of the
     breakpoint address.  Remove the breakpoint by writing the original
     contents back. */

  read_memory (debug_addr, (char *) &debug_copy, sizeof (debug_copy));

  /* Set `in_debugger' to zero now. */

  write_memory (flag_addr, (char *) &in_debugger, sizeof (in_debugger));

  breakpoint_addr = (CORE_ADDR) debug_copy.ldd_bp_addr;
  write_memory (breakpoint_addr, (char *) &debug_copy.ldd_bp_inst,
		sizeof (debug_copy.ldd_bp_inst));

#else	/* SVR4_SHARED_LIBS */

  /* Note that breakpoint address and original contents are in our address
     space, so we just need to write the original contents back. */

  if (memory_remove_breakpoint (breakpoint_addr, shadow_contents) != 0)
    {
      status = 0;
    }

#endif	/* !SVR4_SHARED_LIBS */

  /* For the SVR4 version, we always know the breakpoint address.  For the
     SunOS version we don't know it until the above code is executed.
     Grumble if we are stopped anywhere besides the breakpoint address. */

  if (stop_pc != breakpoint_addr)
    {
      warning ("stopped at unknown breakpoint while handling shared libraries");
    }

  return (status);
}

#endif	/* #ifdef SVR4_SHARED_LIBS */

/*

LOCAL FUNCTION

	enable_break -- arrange for dynamic linker to hit breakpoint

SYNOPSIS

	int enable_break (void)

DESCRIPTION

	Both the SunOS and the SVR4 dynamic linkers have, as part of their
	debugger interface, support for arranging for the inferior to hit
	a breakpoint after mapping in the shared libraries.  This function
	enables that breakpoint.

	For SunOS, there is a special flag location (in_debugger) which we
	set to 1.  When the dynamic linker sees this flag set, it will set
	a breakpoint at a location known only to itself, after saving the
	original contents of that place and the breakpoint address itself,
	in it's own internal structures.  When we resume the inferior, it
	will eventually take a SIGTRAP when it runs into the breakpoint.
	We handle this (in a different place) by restoring the contents of
	the breakpointed location (which is only known after it stops),
	chasing around to locate the shared libraries that have been
	loaded, then resuming.

	For SVR4, the debugger interface structure contains a member (r_brk)
	which is statically initialized at the time the shared library is
	built, to the offset of a function (_r_debug_state) which is guaran-
	teed to be called once before mapping in a library, and again when
	the mapping is complete.  At the time we are examining this member,
	it contains only the unrelocated offset of the function, so we have
	to do our own relocation.  Later, when the dynamic linker actually
	runs, it relocates r_brk to be the actual address of _r_debug_state().

	The debugger interface structure also contains an enumeration which
	is set to either RT_ADD or RT_DELETE prior to changing the mapping,
	depending upon whether or not the library is being mapped or unmapped,
	and then set to RT_CONSISTENT after the library is mapped/unmapped.
*/

static int
enable_break ()
{
  int success = 0;

#ifndef SVR4_SHARED_LIBS

  int j;
  int in_debugger;

  /* Get link_dynamic structure */

  j = target_read_memory (debug_base, (char *) &dynamic_copy,
			  sizeof (dynamic_copy));
  if (j)
    {
      /* unreadable */
      return (0);
    }

  /* Calc address of debugger interface structure */

  debug_addr = (CORE_ADDR) dynamic_copy.ldd;

  /* Calc address of `in_debugger' member of debugger interface structure */

  flag_addr = debug_addr + (CORE_ADDR) ((char *) &debug_copy.ldd_in_debugger -
					(char *) &debug_copy);

  /* Write a value of 1 to this member.  */

  in_debugger = 1;
  write_memory (flag_addr, (char *) &in_debugger, sizeof (in_debugger));
  success = 1;

#else	/* SVR4_SHARED_LIBS */

#ifdef BKPT_AT_SYMBOL

  struct minimal_symbol *msymbol;
  char **bkpt_namep;
  asection *interp_sect;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

#ifdef SVR4_SHARED_LIBS
  /* Find the .interp section; if not found, warn the user and drop
     into the old breakpoint at symbol code.  */
  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");
  if (interp_sect)
    {
      unsigned int interp_sect_size;
      char *buf;
      CORE_ADDR load_addr;
      bfd *tmp_bfd;
      CORE_ADDR sym_addr = 0;

      /* Read the contents of the .interp section into a local buffer;
	 the contents specify the dynamic linker this program uses.  */
      interp_sect_size = bfd_section_size (exec_bfd, interp_sect);
      buf = alloca (interp_sect_size);
      bfd_get_section_contents (exec_bfd, interp_sect,
				buf, 0, interp_sect_size);

      /* Now we need to figure out where the dynamic linker was
	 loaded so that we can load its symbols and place a breakpoint
	 in the dynamic linker itself.

	 This address is stored on the stack.  However, I've been unable
	 to find any magic formula to find it for Solaris (appears to
	 be trivial on Linux).  Therefore, we have to try an alternate
	 mechanism to find the dynamic linker's base address.  */
      tmp_bfd = bfd_openr (buf, gnutarget);
      if (tmp_bfd == NULL)
	goto bkpt_at_symbol;

      /* Make sure the dynamic linker's really a useful object.  */
      if (!bfd_check_format (tmp_bfd, bfd_object))
	{
	  warning ("Unable to grok dynamic linker %s as an object file", buf);
	  bfd_close (tmp_bfd);
	  goto bkpt_at_symbol;
	}

      /* We find the dynamic linker's base address by examining the
	 current pc (which point at the entry point for the dynamic
	 linker) and subtracting the offset of the entry point.  */
      load_addr = read_pc () - tmp_bfd->start_address;

      /* Now try to set a breakpoint in the dynamic linker.  */
      for (bkpt_namep = solib_break_names; *bkpt_namep != NULL; bkpt_namep++)
	{
	  sym_addr = bfd_lookup_symbol (tmp_bfd, *bkpt_namep);
	  if (sym_addr != 0)
	    break;
	}

      /* We're done with the temporary bfd.  */
      bfd_close (tmp_bfd);

      if (sym_addr != 0)
	{
	  create_solib_event_breakpoint (load_addr + sym_addr);
	  return 1;
	}

      /* For whatever reason we couldn't set a breakpoint in the dynamic
	 linker.  Warn and drop into the old code.  */
bkpt_at_symbol:
      warning ("Unable to find dynamic linker breakpoint function.");
      warning ("GDB will be unable to debug shared library initializers");
      warning ("and track explicitly loaded dynamic code.");
    }
#endif

  /* Scan through the list of symbols, trying to look up the symbol and
     set a breakpoint there.  Terminate loop when we/if we succeed. */

  breakpoint_addr = 0;
  for (bkpt_namep = bkpt_names; *bkpt_namep != NULL; bkpt_namep++)
    {
      msymbol = lookup_minimal_symbol (*bkpt_namep, NULL, symfile_objfile);
      if ((msymbol != NULL) && (SYMBOL_VALUE_ADDRESS (msymbol) != 0))
	{
	  create_solib_event_breakpoint (SYMBOL_VALUE_ADDRESS (msymbol));
	  return 1;
	}
    }

  /* Nothing good happened.  */
  return 0;

#endif	/* BKPT_AT_SYMBOL */

#endif	/* !SVR4_SHARED_LIBS */

  return (success);
}
  
/*
  
GLOBAL FUNCTION
  
	solib_create_inferior_hook -- shared library startup support
  
SYNOPSIS
  
	void solib_create_inferior_hook()
  
DESCRIPTION
  
	When gdb starts up the inferior, it nurses it along (through the
	shell) until it is ready to execute it's first instruction.  At this
	point, this function gets called via expansion of the macro
	SOLIB_CREATE_INFERIOR_HOOK.

	For SunOS executables, this first instruction is typically the
	one at "_start", or a similar text label, regardless of whether
	the executable is statically or dynamically linked.  The runtime
	startup code takes care of dynamically linking in any shared
	libraries, once gdb allows the inferior to continue.

	For SVR4 executables, this first instruction is either the first
	instruction in the dynamic linker (for dynamically linked
	executables) or the instruction at "start" for statically linked
	executables.  For dynamically linked executables, the system
	first exec's /lib/libc.so.N, which contains the dynamic linker,
	and starts it running.  The dynamic linker maps in any needed
	shared libraries, maps in the actual user executable, and then
	jumps to "start" in the user executable.

	For both SunOS shared libraries, and SVR4 shared libraries, we
	can arrange to cooperate with the dynamic linker to discover the
	names of shared libraries that are dynamically linked, and the
	base addresses to which they are linked.

	This function is responsible for discovering those names and
	addresses, and saving sufficient information about them to allow
	their symbols to be read at a later time.

FIXME

	Between enable_break() and disable_break(), this code does not
	properly handle hitting breakpoints which the user might have
	set in the startup code or in the dynamic linker itself.  Proper
	handling will probably have to wait until the implementation is
	changed to use the "breakpoint handler function" method.

	Also, what if child has exit()ed?  Must exit loop somehow.
  */

void 
solib_create_inferior_hook()
{
  /* If we are using the BKPT_AT_SYMBOL code, then we don't need the base
     yet.  In fact, in the case of a SunOS4 executable being run on
     Solaris, we can't get it yet.  find_solib will get it when it needs
     it.  */
#if !(defined (SVR4_SHARED_LIBS) && defined (BKPT_AT_SYMBOL))
  if ((debug_base = locate_base ()) == 0)
    {
      /* Can't find the symbol or the executable is statically linked. */
      return;
    }
#endif

  if (!enable_break ())
    {
      warning ("shared library handler failed to enable breakpoint");
      return;
    }

#ifndef SVR4_SHARED_LIBS
  /* Only SunOS needs the loop below, other systems should be using the
     special shared library breakpoints and the shared library breakpoint
     service routine.

     Now run the target.  It will eventually hit the breakpoint, at
     which point all of the libraries will have been mapped in and we
     can go groveling around in the dynamic linker structures to find
     out what we need to know about them. */

  clear_proceed_status ();
  stop_soon_quietly = 1;
  stop_signal = TARGET_SIGNAL_0;
  do
    {
      target_resume (-1, 0, stop_signal);
      wait_for_inferior ();
    }
  while (stop_signal != TARGET_SIGNAL_TRAP);
  stop_soon_quietly = 0;
  
  /* We are now either at the "mapping complete" breakpoint (or somewhere
     else, a condition we aren't prepared to deal with anyway), so adjust
     the PC as necessary after a breakpoint, disable the breakpoint, and
     add any shared libraries that were mapped in. */

  if (DECR_PC_AFTER_BREAK)
    {
      stop_pc -= DECR_PC_AFTER_BREAK;
      write_register (PC_REGNUM, stop_pc);
    }

  if (!disable_break ())
    {
      warning ("shared library handler failed to disable breakpoint");
    }

  if (auto_solib_add)
    solib_add ((char *) 0, 0, (struct target_ops *) 0);
#endif
}

/*

LOCAL FUNCTION

	special_symbol_handling -- additional shared library symbol handling

SYNOPSIS

	void special_symbol_handling (struct so_list *so)

DESCRIPTION

	Once the symbols from a shared object have been loaded in the usual
	way, we are called to do any system specific symbol handling that 
	is needed.

	For SunOS4, this consists of grunging around in the dynamic
	linkers structures to find symbol definitions for "common" symbols
	and adding them to the minimal symbol table for the runtime common
	objfile.

*/

static void
special_symbol_handling (so)
struct so_list *so;
{
#ifndef SVR4_SHARED_LIBS
  int j;

  if (debug_addr == 0)
    {
      /* Get link_dynamic structure */

      j = target_read_memory (debug_base, (char *) &dynamic_copy,
			      sizeof (dynamic_copy));
      if (j)
	{
	  /* unreadable */
	  return;
	}

      /* Calc address of debugger interface structure */
      /* FIXME, this needs work for cross-debugging of core files
	 (byteorder, size, alignment, etc).  */

      debug_addr = (CORE_ADDR) dynamic_copy.ldd;
    }

  /* Read the debugger structure from the inferior, just to make sure
     we have a current copy. */

  j = target_read_memory (debug_addr, (char *) &debug_copy,
			  sizeof (debug_copy));
  if (j)
    return;		/* unreadable */

  /* Get common symbol definitions for the loaded object. */

  if (debug_copy.ldd_cp)
    {
      solib_add_common_symbols (debug_copy.ldd_cp);
    }

#endif	/* !SVR4_SHARED_LIBS */
}


/*

LOCAL FUNCTION

	sharedlibrary_command -- handle command to explicitly add library

SYNOPSIS

	static void sharedlibrary_command (char *args, int from_tty)

DESCRIPTION

*/

static void
sharedlibrary_command (args, from_tty)
char *args;
int from_tty;
{
  dont_repeat ();
  solib_add (args, from_tty, (struct target_ops *) 0);
}

#endif /* HAVE_LINK_H */

void
_initialize_solib()
{
#ifdef HAVE_LINK_H

  add_com ("sharedlibrary", class_files, sharedlibrary_command,
	   "Load shared object library symbols for files matching REGEXP.");
  add_info ("sharedlibrary", info_sharedlibrary_command, 
	    "Status of loaded shared object libraries.");

  add_show_from_set
    (add_set_cmd ("auto-solib-add", class_support, var_zinteger,
		  (char *) &auto_solib_add,
		  "Set autoloading of shared library symbols.\n\
If nonzero, symbols from all shared object libraries will be loaded\n\
automatically when the inferior begins execution or when the dynamic linker\n\
informs gdb that a new library has been loaded.  Otherwise, symbols\n\
must be loaded manually, using `sharedlibrary'.",
		  &setlist),
     &showlist);

#endif /* HAVE_LINK_H */
}
