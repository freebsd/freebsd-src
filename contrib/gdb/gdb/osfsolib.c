/* Handle OSF/1 shared libraries for GDB, the GNU Debugger.
   Copyright 1993, 94, 95, 96, 98, 1999 Free Software Foundation, Inc.

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

/* FIXME: Most of this code could be merged with solib.c by using
   next_link_map_member and xfer_link_map_member in solib.c.  */

#include "defs.h"

#include <sys/types.h>
#include <signal.h>
#include "gdb_string.h"
#include <fcntl.h>

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "command.h"
#include "target.h"
#include "frame.h"
#include "gdb_regex.h"
#include "inferior.h"
#include "language.h"
#include "gdbcmd.h"

#define MAX_PATH_SIZE 1024	/* FIXME: Should be dynamic */

/* When handling shared libraries, GDB has to find out the pathnames
   of all shared libraries that are currently loaded (to read in their
   symbols) and where the shared libraries are loaded in memory
   (to relocate them properly from their prelinked addresses to the
   current load address).

   Under OSF/1 there are two possibilities to get at this information:
   1) Peek around in the runtime loader structures.
   These are not documented, and they are not defined in the system
   header files. The definitions below were obtained by experimentation,
   but they seem stable enough.
   2) Use the undocumented libxproc.a library, which contains the
   equivalent ldr_* routines.
   This approach is somewhat cleaner, but it requires that the GDB
   executable is dynamically linked. In addition it requires a
   NAT_CLIBS= -lxproc -Wl,-expect_unresolved,ldr_process_context
   linker specification for GDB and all applications that are using
   libgdb.
   We will use the peeking approach until it becomes unwieldy.  */

#ifndef USE_LDR_ROUTINES

/* Definition of runtime loader structures, found by experimentation.  */
#define RLD_CONTEXT_ADDRESS	0x3ffc0000000

typedef struct
  {
    CORE_ADDR next;
    CORE_ADDR previous;
    CORE_ADDR unknown1;
    char *module_name;
    CORE_ADDR modinfo_addr;
    long module_id;
    CORE_ADDR unknown2;
    CORE_ADDR unknown3;
    long region_count;
    CORE_ADDR regioninfo_addr;
  }
ldr_module_info_t;

typedef struct
  {
    long unknown1;
    CORE_ADDR regionname_addr;
    long protection;
    CORE_ADDR vaddr;
    CORE_ADDR mapaddr;
    long size;
    long unknown2[5];
  }
ldr_region_info_t;

typedef struct
  {
    CORE_ADDR unknown1;
    CORE_ADDR unknown2;
    CORE_ADDR head;
    CORE_ADDR tail;
  }
ldr_context_t;

static ldr_context_t ldr_context;

#else

#include <loader.h>
static ldr_process_t fake_ldr_process;

/* Called by ldr_* routines to read memory from the current target.  */

static int ldr_read_memory PARAMS ((CORE_ADDR, char *, int, int));

static int
ldr_read_memory (memaddr, myaddr, len, readstring)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int readstring;
{
  int result;
  char *buffer;

  if (readstring)
    {
      target_read_string (memaddr, &buffer, len, &result);
      if (result == 0)
	strcpy (myaddr, buffer);
      free (buffer);
    }
  else
    result = target_read_memory (memaddr, myaddr, len);

  if (result != 0)
    result = -result;
  return result;
}

#endif

/* Define our own link_map structure.
   This will help to share code with solib.c.  */

struct link_map
{
  CORE_ADDR l_offset;		/* prelink to load address offset */
  char *l_name;			/* full name of loaded object */
  ldr_module_info_t module_info;	/* corresponding module info */
};

#define LM_OFFSET(so) ((so) -> lm.l_offset)
#define LM_NAME(so) ((so) -> lm.l_name)

struct so_list
  {
    struct so_list *next;	/* next structure in linked list */
    struct link_map lm;		/* copy of link map from inferior */
    struct link_map *lmaddr;	/* addr in inferior lm was read from */
    CORE_ADDR lmend;		/* upper addr bound of mapped object */
    char so_name[MAX_PATH_SIZE];	/* shared object lib name (FIXME) */
    char symbols_loaded;	/* flag: symbols read in yet? */
    char from_tty;		/* flag: print msgs? */
    struct objfile *objfile;	/* objfile for loaded lib */
    struct section_table *sections;
    struct section_table *sections_end;
    struct section_table *textsection;
    bfd *abfd;
  };

static struct so_list *so_list_head;	/* List of known shared objects */

extern int
fdmatch PARAMS ((int, int));	/* In libiberty */

/* Local function prototypes */

static void
sharedlibrary_command PARAMS ((char *, int));

static void
info_sharedlibrary_command PARAMS ((char *, int));

static int
symbol_add_stub PARAMS ((char *));

static struct so_list *
  find_solib PARAMS ((struct so_list *));

static struct link_map *
  first_link_map_member PARAMS ((void));

static struct link_map *
  next_link_map_member PARAMS ((struct so_list *));

static void
xfer_link_map_member PARAMS ((struct so_list *, struct link_map *));

static int
solib_map_sections PARAMS ((char *));

/*

   LOCAL FUNCTION

   solib_map_sections -- open bfd and build sections for shared lib

   SYNOPSIS

   static int solib_map_sections (struct so_list *so)

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

static int
solib_map_sections (arg)
     char *arg;
{
  struct so_list *so = (struct so_list *) arg;	/* catch_errors bogon */
  char *filename;
  char *scratch_pathname;
  int scratch_chan;
  struct section_table *p;
  struct cleanup *old_chain;
  bfd *abfd;

  filename = tilde_expand (so->so_name);
  old_chain = make_cleanup (free, filename);

  scratch_chan = openp (getenv ("PATH"), 1, filename, O_RDONLY, 0,
			&scratch_pathname);
  if (scratch_chan < 0)
    {
      scratch_chan = openp (getenv ("LD_LIBRARY_PATH"), 1, filename,
			    O_RDONLY, 0, &scratch_pathname);
    }
  if (scratch_chan < 0)
    {
      perror_with_name (filename);
    }
  /* Leave scratch_pathname allocated.  bfd->name will point to it.  */

  abfd = bfd_fdopenr (scratch_pathname, gnutarget, scratch_chan);
  if (!abfd)
    {
      close (scratch_chan);
      error ("Could not open `%s' as an executable file: %s",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  /* Leave bfd open, core_xfer_memory and "info files" need it.  */
  so->abfd = abfd;
  abfd->cacheable = true;

  if (!bfd_check_format (abfd, bfd_object))
    {
      error ("\"%s\": not in executable format: %s.",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  if (build_section_table (abfd, &so->sections, &so->sections_end))
    {
      error ("Can't find the file sections in `%s': %s",
	     bfd_get_filename (exec_bfd), bfd_errmsg (bfd_get_error ()));
    }

  for (p = so->sections; p < so->sections_end; p++)
    {
      /* Relocate the section binding addresses as recorded in the shared
         object's file by the offset to get the address to which the
         object was actually mapped.  */
      p->addr += LM_OFFSET (so);
      p->endaddr += LM_OFFSET (so);
      so->lmend = (CORE_ADDR) max (p->endaddr, so->lmend);
      if (STREQ (p->the_bfd_section->name, ".text"))
	{
	  so->textsection = p;
	}
    }

  /* Free the file names, close the file now.  */
  do_cleanups (old_chain);

  return (1);
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
  static struct link_map first_lm;

#ifdef USE_LDR_ROUTINES
  ldr_module_t mod_id = LDR_NULL_MODULE;
  size_t retsize;

  fake_ldr_process = ldr_core_process ();
  ldr_set_core_reader (ldr_read_memory);
  ldr_xdetach (fake_ldr_process);
  if (ldr_xattach (fake_ldr_process) != 0
      || ldr_next_module (fake_ldr_process, &mod_id) != 0
      || mod_id == LDR_NULL_MODULE
      || ldr_inq_module (fake_ldr_process, mod_id,
			 &first_lm.module_info, sizeof (ldr_module_info_t),
			 &retsize) != 0)
    return lm;
#else
  CORE_ADDR ldr_context_addr;

  if (target_read_memory ((CORE_ADDR) RLD_CONTEXT_ADDRESS,
			  (char *) &ldr_context_addr,
			  sizeof (CORE_ADDR)) != 0
      || target_read_memory (ldr_context_addr,
			     (char *) &ldr_context,
			     sizeof (ldr_context_t)) != 0
      || target_read_memory ((CORE_ADDR) ldr_context.head,
			     (char *) &first_lm.module_info,
			     sizeof (ldr_module_info_t)) != 0)
    return lm;
#endif

  lm = &first_lm;

  /* The first entry is for the main program and should be skipped.  */
  lm->l_name = NULL;

  return lm;
}

static struct link_map *
next_link_map_member (so_list_ptr)
     struct so_list *so_list_ptr;
{
  struct link_map *lm = NULL;
  static struct link_map next_lm;
#ifdef USE_LDR_ROUTINES
  ldr_module_t mod_id = so_list_ptr->lm.module_info.lmi_modid;
  size_t retsize;

  if (ldr_next_module (fake_ldr_process, &mod_id) != 0
      || mod_id == LDR_NULL_MODULE
      || ldr_inq_module (fake_ldr_process, mod_id,
			 &next_lm.module_info, sizeof (ldr_module_info_t),
			 &retsize) != 0)
    return lm;

  lm = &next_lm;
  lm->l_name = lm->module_info.lmi_name;
#else
  CORE_ADDR ldr_context_addr;

  /* Reread context in case ldr_context.tail was updated.  */

  if (target_read_memory ((CORE_ADDR) RLD_CONTEXT_ADDRESS,
			  (char *) &ldr_context_addr,
			  sizeof (CORE_ADDR)) != 0
      || target_read_memory (ldr_context_addr,
			     (char *) &ldr_context,
			     sizeof (ldr_context_t)) != 0
      || so_list_ptr->lm.module_info.modinfo_addr == ldr_context.tail
      || target_read_memory (so_list_ptr->lm.module_info.next,
			     (char *) &next_lm.module_info,
			     sizeof (ldr_module_info_t)) != 0)
    return lm;

  lm = &next_lm;
  lm->l_name = lm->module_info.module_name;
#endif
  return lm;
}

static void
xfer_link_map_member (so_list_ptr, lm)
     struct so_list *so_list_ptr;
     struct link_map *lm;
{
  int i;
  so_list_ptr->lm = *lm;

  /* OSF/1 shared libraries are pre-linked to particular addresses,
     but the runtime loader may have to relocate them if the
     address ranges of the libraries used by the target executable clash,
     or if the target executable is linked with the -taso option.
     The offset is the difference between the address where the shared
     library is mapped and the pre-linked address of the shared library.

     FIXME:  GDB is currently unable to relocate the shared library
     sections by different offsets. If sections are relocated by
     different offsets, put out a warning and use the offset of the
     first section for all remaining sections.  */
  LM_OFFSET (so_list_ptr) = 0;

  /* There is one entry that has no name (for the inferior executable)
     since it is not a shared object. */
  if (LM_NAME (so_list_ptr) != 0)
    {

#ifdef USE_LDR_ROUTINES
      int len = strlen (LM_NAME (so_list_ptr) + 1);

      if (len > MAX_PATH_SIZE)
	len = MAX_PATH_SIZE;
      strncpy (so_list_ptr->so_name, LM_NAME (so_list_ptr), MAX_PATH_SIZE);
      so_list_ptr->so_name[MAX_PATH_SIZE - 1] = '\0';

      for (i = 0; i < lm->module_info.lmi_nregion; i++)
	{
	  ldr_region_info_t region_info;
	  size_t retsize;
	  CORE_ADDR region_offset;

	  if (ldr_inq_region (fake_ldr_process, lm->module_info.lmi_modid,
			      i, &region_info, sizeof (region_info),
			      &retsize) != 0)
	    break;
	  region_offset = (CORE_ADDR) region_info.lri_mapaddr
	    - (CORE_ADDR) region_info.lri_vaddr;
	  if (i == 0)
	    LM_OFFSET (so_list_ptr) = region_offset;
	  else if (LM_OFFSET (so_list_ptr) != region_offset)
	    warning ("cannot handle shared library relocation for %s (%s)",
		     so_list_ptr->so_name, region_info.lri_name);
	}
#else
      int errcode;
      char *buffer;
      target_read_string ((CORE_ADDR) LM_NAME (so_list_ptr), &buffer,
			  MAX_PATH_SIZE - 1, &errcode);
      if (errcode != 0)
	error ("xfer_link_map_member: Can't read pathname for load map: %s\n",
	       safe_strerror (errcode));
      strncpy (so_list_ptr->so_name, buffer, MAX_PATH_SIZE - 1);
      free (buffer);
      so_list_ptr->so_name[MAX_PATH_SIZE - 1] = '\0';

      for (i = 0; i < lm->module_info.region_count; i++)
	{
	  ldr_region_info_t region_info;
	  CORE_ADDR region_offset;

	  if (target_read_memory (lm->module_info.regioninfo_addr
				  + i * sizeof (region_info),
				  (char *) &region_info,
				  sizeof (region_info)) != 0)
	    break;
	  region_offset = region_info.mapaddr - region_info.vaddr;
	  if (i == 0)
	    LM_OFFSET (so_list_ptr) = region_offset;
	  else if (LM_OFFSET (so_list_ptr) != region_offset)
	    {
	      char *region_name;
	      target_read_string (region_info.regionname_addr, &buffer,
				  MAX_PATH_SIZE - 1, &errcode);
	      if (errcode == 0)
		region_name = buffer;
	      else
		region_name = "??";
	      warning ("cannot handle shared library relocation for %s (%s)",
		       so_list_ptr->so_name, region_name);
	      free (buffer);
	    }
	}
#endif

      catch_errors (solib_map_sections, (char *) so_list_ptr,
		    "Error while mapping shared library sections:\n",
		    RETURN_MASK_ALL);
    }
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
	  /* Find the first link map list member. */
	  lm = first_link_map_member ();
	}
    }
  else
    {
      /* We have been called before, and are in the process of walking
         the shared library list.  Advance to the next shared object. */
      lm = next_link_map_member (so_list_ptr);
      so_list_next = so_list_ptr->next;
    }
  if ((so_list_next == NULL) && (lm != NULL))
    {
      /* Get next link map structure from inferior image and build a local
         abbreviated load_map structure */
      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      memset ((char *) new, 0, sizeof (struct so_list));
      new->lmaddr = lm;
      /* Add the new node as the next node in the list, or as the root
         node if this is the first one. */
      if (so_list_ptr != NULL)
	{
	  so_list_ptr->next = new;
	}
      else
	{
	  so_list_head = new;
	}
      so_list_next = new;
      xfer_link_map_member (new, lm);
    }
  return (so_list_next);
}

/* A small stub to get us past the arg-passing pinhole of catch_errors.  */

static int
symbol_add_stub (arg)
     char *arg;
{
  register struct so_list *so = (struct so_list *) arg;		/* catch_errs bogon */
  CORE_ADDR text_addr = 0;
  struct section_addr_info section_addrs;

  memset (&section_addrs, 0, sizeof (section_addrs));
  if (so->textsection)
    text_addr = so->textsection->addr;
  else if (so->abfd != NULL)
    {
      asection *lowest_sect;

      /* If we didn't find a mapped non zero sized .text section, set up
         text_addr so that the relocation in symbol_file_add does no harm.  */

      lowest_sect = bfd_get_section_by_name (so->abfd, ".text");
      if (lowest_sect == NULL)
	bfd_map_over_sections (so->abfd, find_lowest_section,
			       (PTR) &lowest_sect);
      if (lowest_sect)
	text_addr = bfd_section_vma (so->abfd, lowest_sect) + LM_OFFSET (so);
    }

  section_addrs.text_addr = text_addr;
  so->objfile = symbol_file_add (so->so_name, so->from_tty,
				 &section_addrs, 0, OBJF_SHARED);
  return (1);
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
  register struct so_list *so = NULL;	/* link map state variable */

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
	  if (so->so_name[0])
	    {
	      count += so->sections_end - so->sections;
	    }
	}

      if (count)
	{
	  /* Add these section table entries to the target's table.  */

	  old = target_resize_to_sections (target, count);
	  
	  while ((so = find_solib (so)) != NULL)
	    {
	      if (so->so_name[0])
		{
		  count = so->sections_end - so->sections;
		  memcpy ((char *) (target->to_sections + old),
			  so->sections,
			  (sizeof (struct section_table)) * count);
		  old += count;
		}
	    }
	}
    }

  /* Now add the symbol files.  */
  so = NULL;
  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0] && re_exec (so->so_name))
	{
	  so->from_tty = from_tty;
	  if (so->symbols_loaded)
	    {
	      if (from_tty)
		{
		  printf_unfiltered ("Symbols already loaded for %s\n", so->so_name);
		}
	    }
	  else if (catch_errors
		   (symbol_add_stub, (char *) so,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
	    {
	      so_last = so;
	      so->symbols_loaded = 1;
	    }
	}
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  if (so_last)
    reinit_frame_cache ();
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
  register struct so_list *so = NULL;	/* link map state variable */
  int header_done = 0;

  if (exec_bfd == NULL)
    {
      printf_unfiltered ("No executable file.\n");
      return;
    }
  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0])
	{
	  unsigned long txt_start = 0;
	  unsigned long txt_end = 0;

	  if (!header_done)
	    {
	      printf_unfiltered ("%-20s%-20s%-12s%s\n", "From", "To", "Syms Read",
				 "Shared Object Library");
	      header_done++;
	    }
	  if (so->textsection)
	    {
	      txt_start = (unsigned long) so->textsection->addr;
	      txt_end = (unsigned long) so->textsection->endaddr;
	    }
	  printf_unfiltered ("%-20s", local_hex_string_custom (txt_start, "08l"));
	  printf_unfiltered ("%-20s", local_hex_string_custom (txt_end, "08l"));
	  printf_unfiltered ("%-12s", so->symbols_loaded ? "Yes" : "No");
	  printf_unfiltered ("%s\n", so->so_name);
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

   char *solib_address (CORE_ADDR address)

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
  register struct so_list *so = 0;	/* link map state variable */

  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0] && so->textsection)
	{
	  if ((address >= (CORE_ADDR) so->textsection->addr) &&
	      (address < (CORE_ADDR) so->textsection->endaddr))
	    return (so->so_name);
	}
    }
  return (0);
}

/* Called by free_all_symtabs */

void
clear_solib ()
{
  struct so_list *next;
  char *bfd_filename;

  disable_breakpoints_in_shlibs (1);

  while (so_list_head)
    {
      if (so_list_head->sections)
	{
	  free ((PTR) so_list_head->sections);
	}
      if (so_list_head->abfd)
	{
	  bfd_filename = bfd_get_filename (so_list_head->abfd);
	  if (!bfd_close (so_list_head->abfd))
	    warning ("cannot close \"%s\": %s",
		     bfd_filename, bfd_errmsg (bfd_get_error ()));
	}
      else
	/* This happens for the executable on SVR4.  */
	bfd_filename = NULL;

      next = so_list_head->next;
      if (bfd_filename)
	free ((PTR) bfd_filename);
      free ((PTR) so_list_head);
      so_list_head = next;
    }
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
   For a statically bound executable, this first instruction is the
   one at "_start", or a similar text label. No further processing is
   needed in that case.
   For a dynamically bound executable, this first instruction is somewhere
   in the rld, and the actual user executable is not yet mapped in.
   We continue the inferior again, rld then maps in the actual user
   executable and any needed shared libraries and then sends
   itself a SIGTRAP.
   At that point we discover the names of all shared libraries and
   read their symbols in.

   FIXME

   This code does not properly handle hitting breakpoints which the
   user might have set in the rld itself.  Proper handling would have
   to check if the SIGTRAP happened due to a kill call.

   Also, what if child has exit()ed?  Must exit loop somehow.
 */

void
solib_create_inferior_hook ()
{

  /* Nothing to do for statically bound executables.  */

  if (symfile_objfile == NULL
      || symfile_objfile->obfd == NULL
      || ((bfd_get_file_flags (symfile_objfile->obfd) & DYNAMIC) == 0))
    return;

  /* Now run the target.  It will eventually get a SIGTRAP, at
     which point all of the libraries will have been mapped in and we
     can go groveling around in the rld structures to find
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

  /*  solib_add will call reinit_frame_cache.
     But we are stopped in the runtime loader and we do not have symbols
     for the runtime loader. So heuristic_proc_start will be called
     and will put out an annoying warning.
     Delaying the resetting of stop_soon_quietly until after symbol loading
     suppresses the warning.  */
  if (auto_solib_add)
    solib_add ((char *) 0, 0, (struct target_ops *) 0);
  stop_soon_quietly = 0;
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

void
_initialize_solib ()
{
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
}
