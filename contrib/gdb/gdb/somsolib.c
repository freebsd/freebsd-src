/* Handle HP SOM shared libraries for GDB, the GNU Debugger.
   Copyright 1993, 1996 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Written by the Center for Software Science at the Univerity of Utah
and by Cygnus Support.  */


#include "defs.h"

#include "frame.h"
#include "bfd.h"
#include "som.h"
#include "libhppa.h"
#include "gdbcore.h"
#include "symtab.h"
#include "breakpoint.h"
#include "symfile.h"
#include "objfiles.h"
#include "inferior.h"
#include "gdb-stabs.h"
#include "gdbcmd.h"

/* TODO:

   * Most of this code should work for hp300 shared libraries.  Does
   anyone care enough to weed out any SOM-isms.

   * Support for hpux8 dynamic linker.  */

/* The basic structure which describes a dynamically loaded object.  This
   data structure is private to the dynamic linker and isn't found in
   any HPUX include file.  */

struct som_solib_mapped_entry
{
  /* The name of the library.  */
  char *name;

  /* Version of this structure (it is expected to change again in hpux10).  */
  unsigned char struct_version;

  /* Binding mode for this library.  */
  unsigned char bind_mode;

  /* Version of this library.  */
  short library_version;

  /* Start of text address, link-time text location, end of text address.  */
  CORE_ADDR text_addr;
  CORE_ADDR text_link_addr;
  CORE_ADDR text_end;

  /* Start of data, start of bss and end of data.  */
  CORE_ADDR data_start;
  CORE_ADDR bss_start;
  CORE_ADDR data_end;

  /* Value of linkage pointer (%r19).  */
  CORE_ADDR got_value;

  /* Next entry.  */
  struct som_solib_mapped_entry *next;

  /* There are other fields, but I don't have information as to what is
     contained in them.  */
};

/* A structure to keep track of all the known shared objects.  */
struct so_list
{
  struct som_solib_mapped_entry som_solib;
  struct objfile *objfile;
  bfd *abfd;
  struct section_table *sections;
  struct section_table *sections_end;
  struct so_list *next;
};

static struct so_list *so_list_head;

static void som_sharedlibrary_info_command PARAMS ((char *, int));

/* Add symbols from shared libraries into the symtab list.  */

void
som_solib_add (arg_string, from_tty, target)
     char *arg_string;
     int from_tty;
     struct target_ops *target;
{
  struct minimal_symbol *msymbol;
  struct so_list *so_list_tail;
  CORE_ADDR addr;
  asection *shlib_info;
  int status;
  unsigned int dld_flags;
  char buf[4], *re_err;

  /* First validate our arguments.  */
  if ((re_err = re_comp (arg_string ? arg_string : ".")) != NULL)
    {
      error ("Invalid regexp: %s", re_err);
    }

  /* If we're debugging a core file, or have attached to a running
     process, then som_solib_create_inferior_hook will not have been
     called.

     We need to first determine if we're dealing with a dynamically
     linked executable.  If not, then return without an error or warning.

     We also need to examine __dld_flags to determine if the shared library
     list is valid and to determine if the libraries have been privately
     mapped.  */
  if (symfile_objfile == NULL)
    return;

  /* First see if the objfile was dynamically linked.  */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, "$SHLIB_INFO$");
  if (!shlib_info)
    return;

  /* It's got a $SHLIB_INFO$ section, make sure it's not empty.  */
  if (bfd_section_size (symfile_objfile->obfd, shlib_info) == 0)
    return;

  msymbol = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
  if (msymbol == NULL)
    {
      error ("Unable to find __dld_flags symbol in object file.\n");
      return;
    }

  addr = SYMBOL_VALUE_ADDRESS (msymbol);
  /* Read the current contents.  */
  status = target_read_memory (addr, buf, 4);
  if (status != 0)
    {
      error ("Unable to read __dld_flags\n");
      return;
    }
  dld_flags = extract_unsigned_integer (buf, 4);

  /* __dld_list may not be valid.  If it's not valid tell the user.  */
  if ((dld_flags & 4) == 0)
    {
      error ("__dld_list is not valid according to __dld_flags.\n");
      return;
    }

  /* If the libraries were not mapped private, warn the user.  */
  if ((dld_flags & 1) == 0)
    warning ("The shared libraries were not privately mapped; setting a\nbreakpoint in a shared library will not work until you rerun the program.\n");

  msymbol = lookup_minimal_symbol ("__dld_list", NULL, NULL);
  if (!msymbol)
    {
      /* Older crt0.o files (hpux8) don't have __dld_list as a symbol,
	 but the data is still available if you know where to look.  */
      msymbol = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
      if (!msymbol)
	{
	  error ("Unable to find dynamic library list.\n");
	  return;
	}
      addr = SYMBOL_VALUE_ADDRESS (msymbol) - 8;
    }
  else
    addr = SYMBOL_VALUE_ADDRESS (msymbol);

  status = target_read_memory (addr, buf, 4);
  if (status != 0)
    {
      error ("Unable to find dynamic library list.\n");
      return;
    }

  addr = extract_unsigned_integer (buf, 4);

  /* If addr is zero, then we're using an old dynamic loader which
     doesn't maintain __dld_list.  We'll have to use a completely
     different approach to get shared library information.  */
  if (addr == 0)
    goto old_dld;

  /* Using the information in __dld_list is the preferred method
     to get at shared library information.  It doesn't depend on
     any functions in /usr/lib/end.o and has a chance of working
     with hpux10 when it is released.  */
  status = target_read_memory (addr, buf, 4);
  if (status != 0)
    {
      error ("Unable to find dynamic library list.\n");
      return;
    }

  /* addr now holds the address of the first entry in the dynamic
     library list.  */
  addr = extract_unsigned_integer (buf, 4);

  /* Now that we have a pointer to the dynamic library list, walk
     through it and add the symbols for each library.  */

  so_list_tail = so_list_head;
  /* Find the end of the list of shared objects.  */
  while (so_list_tail && so_list_tail->next)
    so_list_tail = so_list_tail->next;

  while (1)
    {
      CORE_ADDR name_addr, text_addr;
      unsigned int name_len;
      char *name;
      struct so_list *new_so;
      struct so_list *so_list = so_list_head;
      struct section_table *p;
      struct stat statbuf;

      if (addr == 0)
	break;

      /* Get a pointer to the name of this library.  */
      status = target_read_memory (addr, buf, 4);
      if (status != 0)
	goto err;

      name_addr = extract_unsigned_integer (buf, 4);
      name_len = 0;
      while (1)
	{
	  target_read_memory (name_addr + name_len, buf, 1);
	  if (status != 0)
	    goto err;

	  name_len++;
	  if (*buf == '\0')
	    break;
	}
      name = alloca (name_len);
      status = target_read_memory (name_addr, name, name_len);
      if (status != 0)
	goto err;

      /* See if we've already loaded something with this name.  */
      while (so_list)
	{
	  if (!strcmp (so_list->som_solib.name, name))
	    break;
	  so_list = so_list->next;
	}

      /* See if the file exists.  If not, give a warning, but don't
	 die.  */
      status = stat (name, &statbuf);
      if (status == -1)
	{
	  warning ("Can't find file %s referenced in dld_list.", name);

	  status = target_read_memory (addr + 36, buf, 4);
	  if (status != 0)
	    goto err;

	  addr = (CORE_ADDR) extract_unsigned_integer (buf, 4);
	  continue;
	}

      /* If we've already loaded this one or it's the main program, skip it.  */
      if (so_list || !strcmp (name, symfile_objfile->name))
	{
	  status = target_read_memory (addr + 36, buf, 4);
	  if (status != 0)
	    goto err;

	  addr = (CORE_ADDR) extract_unsigned_integer (buf, 4);
	  continue;
	}

      name = obsavestring (name, name_len - 1,
			   &symfile_objfile->symbol_obstack);

      status = target_read_memory (addr + 8, buf, 4);
      if (status != 0)
	goto err;

      text_addr = extract_unsigned_integer (buf, 4);


      new_so = (struct so_list *) xmalloc (sizeof (struct so_list));
      memset ((char *)new_so, 0, sizeof (struct so_list));
      if (so_list_head == NULL)
	{
	  so_list_head = new_so;
	  so_list_tail = new_so;
	}
      else
	{
	  so_list_tail->next = new_so;
	  so_list_tail = new_so;
	}

      /* Fill in all the entries in GDB's shared library list.  */
      new_so->som_solib.name = name;
      status = target_read_memory (addr + 4, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.struct_version = extract_unsigned_integer (buf + 3, 1);
      new_so->som_solib.bind_mode = extract_unsigned_integer (buf + 2, 1);
      new_so->som_solib.library_version = extract_unsigned_integer (buf, 2);
      new_so->som_solib.text_addr = text_addr;

      status = target_read_memory (addr + 12, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.text_link_addr = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 16, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.text_end = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 20, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.data_start = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 24, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.bss_start = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 28, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.data_end = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 32, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.got_value = extract_unsigned_integer (buf, 4);

      status = target_read_memory (addr + 36, buf, 4);
      if (status != 0)
	goto err;

      new_so->som_solib.next = (void *)extract_unsigned_integer (buf, 4);
      addr = (CORE_ADDR)new_so->som_solib.next;

      new_so->objfile = symbol_file_add (name, from_tty, text_addr, 0, 0, 0);
      new_so->abfd = new_so->objfile->obfd;

      if (!bfd_check_format (new_so->abfd, bfd_object))
	{
	  error ("\"%s\": not in executable format: %s.",
		 name, bfd_errmsg (bfd_get_error ()));
	}

      /* Now we need to build a section table for this library since
	 we might be debugging a core file from a dynamically linked
	 executable in which the libraries were not privately mapped.  */
      if (build_section_table (new_so->abfd,
			       &new_so->sections,
			       &new_so->sections_end))
	{
	  error ("Unable to build section table for shared library\n.");
	  return;
	}

      /* Relocate all the sections based on where they got loaded.  */
      for (p = new_so->sections; p < new_so->sections_end; p++)
	{
	  if (p->the_bfd_section->flags & SEC_CODE)
	    {
	      p->addr += text_addr - new_so->som_solib.text_link_addr;
	      p->endaddr += text_addr - new_so->som_solib.text_link_addr;
	    }
	  else if (p->the_bfd_section->flags & SEC_DATA)
	    {
	      p->addr += new_so->som_solib.data_start;
	      p->endaddr += new_so->som_solib.data_start;
	    }
	}

      /* Now see if we need to map in the text and data for this shared
	 library (for example debugging a core file which does not use
	 private shared libraries.). 

	 Carefully peek at the first text address in the library.  If the
	 read succeeds, then the libraries were privately mapped and were
	 included in the core dump file.

	 If the peek failed, then the libraries were not privately mapped
	 and are not in the core file, we'll have to read them in ourselves.  */
      status = target_read_memory (text_addr, buf, 4);
      if (status != 0)
	{
	  int old, new;
	  int update_coreops;

	  /* We must update the to_sections field in the core_ops structure
	     here, otherwise we dereference a potential dangling pointer
	     for each call to target_read/write_memory within this routine.  */
	  update_coreops = core_ops.to_sections == target->to_sections;

	  new = new_so->sections_end - new_so->sections;
	  /* Add sections from the shared library to the core target.  */
	  if (target->to_sections)
	    {
	      old = target->to_sections_end - target->to_sections;
	      target->to_sections = (struct section_table *)
		xrealloc ((char *)target->to_sections,
			  ((sizeof (struct section_table)) * (old + new)));
	    }
	  else
	    {
	      old = 0;
	      target->to_sections = (struct section_table *)
		xmalloc ((sizeof (struct section_table)) * new);
	    }
	  target->to_sections_end = (target->to_sections + old + new);

	  /* Update the to_sections field in the core_ops structure
	     if needed.  */
	  if (update_coreops)
	    {
	      core_ops.to_sections = target->to_sections;
	      core_ops.to_sections_end = target->to_sections_end;
	    }

	  /* Copy over the old data before it gets clobbered.  */
	  memcpy ((char *)(target->to_sections + old),
		  new_so->sections,
		  ((sizeof (struct section_table)) * new));
	}
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  reinit_frame_cache ();
  return;

old_dld:
  error ("Debugging dynamic executables loaded via the hpux8 dld.sl is not supported.\n");
  return;

err:
  error ("Error while reading dynamic library list.\n");
  return;
}


/* This hook gets called just before the first instruction in the
   inferior process is executed.

   This is our opportunity to set magic flags in the inferior so
   that GDB can be notified when a shared library is mapped in and
   to tell the dynamic linker that a private copy of the library is
   needed (so GDB can set breakpoints in the library).

   __dld_flags is the location of the magic flags; as of this implementation
   there are 3 flags of interest:

   bit 0 when set indicates that private copies of the libraries are needed
   bit 1 when set indicates that the callback hook routine is valid
   bit 2 when set indicates that the dynamic linker should maintain the
         __dld_list structure when loading/unloading libraries.

   Note that shared libraries are not mapped in at this time, so we have
   run the inferior until the libraries are mapped in.  Typically this
   means running until the "_start" is called.  */

void
som_solib_create_inferior_hook()
{
  struct minimal_symbol *msymbol;
  unsigned int dld_flags, status, have_endo;
  asection *shlib_info;
  char shadow_contents[BREAKPOINT_MAX], buf[4];
  struct objfile *objfile;
  CORE_ADDR anaddr;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  if (symfile_objfile == NULL)
    return; 

  /* First see if the objfile was dynamically linked.  */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, "$SHLIB_INFO$");
  if (!shlib_info)
    return;

  /* It's got a $SHLIB_INFO$ section, make sure it's not empty.  */
  if (bfd_section_size (symfile_objfile->obfd, shlib_info) == 0)
    return;

  have_endo = 0;
  /* Slam the pid of the process into __d_pid; failing is only a warning!  */
  msymbol = lookup_minimal_symbol ("__d_pid", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find __d_pid symbol in object file.");
      warning ("Suggest linking with /usr/lib/end.o.");
      warning ("GDB will be unable to track shl_load/shl_unload calls");
      goto keep_going;
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  store_unsigned_integer (buf, 4, inferior_pid);
  status = target_write_memory (anaddr, buf, 4);
  if (status != 0)
    {
      warning ("Unable to write __d_pid");
      warning ("Suggest linking with /usr/lib/end.o.");
      warning ("GDB will be unable to track shl_load/shl_unload calls");
      goto keep_going;
    }

  /* Get the value of _DLD_HOOK (an export stub) and put it in __dld_hook;
     This will force the dynamic linker to call __d_trap when significant
     events occur.  */
  msymbol = lookup_minimal_symbol ("_DLD_HOOK", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find _DLD_HOOK symbol in object file.");
      warning ("Suggest linking with /usr/lib/end.o.");
      warning ("GDB will be unable to track shl_load/shl_unload calls");
      goto keep_going;
    }
  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* Grrr, this might not be an export symbol!  We have to find the
     export stub.  */
  ALL_OBJFILES (objfile)
    {
      struct unwind_table_entry *u;
      extern struct unwind_table_entry *find_unwind_entry PARAMS ((CORE_ADDR pc));

      /* What a crock.  */
      msymbol = lookup_minimal_symbol_solib_trampoline (SYMBOL_NAME (msymbol),
							NULL, objfile);
      /* Found a symbol with the right name.  */
      if (msymbol)
	{
	  struct unwind_table_entry *u;
	  /* It must be a shared library trampoline.  */
	  if (SYMBOL_TYPE (msymbol) != mst_solib_trampoline)
	    continue;

	  /* It must also be an export stub.  */
	  u = find_unwind_entry (SYMBOL_VALUE (msymbol));
	  if (!u || u->stub_type != EXPORT)
	    continue;

	  /* OK.  Looks like the correct import stub.  */
	  anaddr = SYMBOL_VALUE (msymbol);
	  break;
	}
     }
  store_unsigned_integer (buf, 4, anaddr);

  msymbol = lookup_minimal_symbol ("__dld_hook", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find __dld_hook symbol in object file.");
      warning ("Suggest linking with /usr/lib/end.o.");
      warning ("GDB will be unable to track shl_load/shl_unload calls");
      goto keep_going;
    }
  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  status = target_write_memory (anaddr, buf, 4);
  
  /* Now set a shlib_event breakpoint at __d_trap so we can track
     significant shared library events.  */
  msymbol = lookup_minimal_symbol ("__d_trap", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find __dld_d_trap symbol in object file.");
      warning ("Suggest linking with /usr/lib/end.o.");
      warning ("GDB will be unable to track shl_load/shl_unload calls");
      goto keep_going;
    }
  create_solib_event_breakpoint (SYMBOL_VALUE_ADDRESS (msymbol));

  /* We have all the support usually found in end.o, so we can track
     shl_load and shl_unload calls.  */
  have_endo = 1;

keep_going:

  /* Get the address of __dld_flags, if no such symbol exists, then we can
     not debug the shared code.  */
  msymbol = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
  if (msymbol == NULL)
    {
      error ("Unable to find __dld_flags symbol in object file.\n");
      goto keep_going;
      return;
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  /* Read the current contents.  */
  status = target_read_memory (anaddr, buf, 4);
  if (status != 0)
    {
      error ("Unable to read __dld_flags\n");
      return;
    }
  dld_flags = extract_unsigned_integer (buf, 4);

  /* Turn on the flags we care about.  */
  dld_flags |= (0x5 | (have_endo << 1));
  store_unsigned_integer (buf, 4, dld_flags);
  status = target_write_memory (anaddr, buf, 4);
  if (status != 0)
    {
      error ("Unable to write __dld_flags\n");
      return;
    }

  /* Now find the address of _start and set a breakpoint there. 
     We still need this code for two reasons:

	* Not all sites have /usr/lib/end.o, so it's not always
	possible to track the dynamic linker's events.

	* At this time no events are triggered for shared libraries
	loaded at startup time (what a crock).  */
	
  msymbol = lookup_minimal_symbol ("_start", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      error ("Unable to find _start symbol in object file.\n");
      return;
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* Make the breakpoint at "_start" a shared library event breakpoint.  */
  create_solib_event_breakpoint (anaddr);

  /* Wipe out all knowledge of old shared libraries since their
     mapping can change from one exec to another!  */
  while (so_list_head)
    {
      struct so_list *temp;

      free_objfile (so_list_head->objfile);
      temp = so_list_head;
      free (so_list_head);
      so_list_head = temp->next;
    }
  clear_symtab_users ();
}

/* Return the GOT value for the shared library in which ADDR belongs.  If
   ADDR isn't in any known shared library, return zero.  */

CORE_ADDR
som_solib_get_got_by_pc (addr)
     CORE_ADDR addr;
{
  struct so_list *so_list = so_list_head;
  CORE_ADDR got_value = 0;

  while (so_list)
    {
      if (so_list->som_solib.text_addr <= addr
	  && so_list->som_solib.text_end > addr)
	{
	  got_value = so_list->som_solib.got_value;
	  break;
	}
      so_list = so_list->next;
    }
  return got_value;
}

int
som_solib_section_offsets (objfile, offsets)
     struct objfile *objfile;
     struct section_offsets *offsets;
{
  struct so_list *so_list = so_list_head;

  while (so_list)
    {
      /* Oh what a pain!  We need the offsets before so_list->objfile
	 is valid.  The BFDs will never match.  Make a best guess.  */
      if (strstr (objfile->name, so_list->som_solib.name))
	{
	  asection *private_section;

	  /* The text offset is easy.  */
	  ANOFFSET (offsets, SECT_OFF_TEXT)
	    = (so_list->som_solib.text_addr
	       - so_list->som_solib.text_link_addr);
	  ANOFFSET (offsets, SECT_OFF_RODATA)
	    = ANOFFSET (offsets, SECT_OFF_TEXT);

	  /* We should look at presumed_dp in the SOM header, but
	     that's not easily available.  This should be OK though.  */
	  private_section = bfd_get_section_by_name (objfile->obfd,
						     "$PRIVATE$");
	  if (!private_section)
	    {
	      warning ("Unable to find $PRIVATE$ in shared library!");
	      ANOFFSET (offsets, SECT_OFF_DATA) = 0;
	      ANOFFSET (offsets, SECT_OFF_BSS) = 0;
	      return 1;
	    }
	  ANOFFSET (offsets, SECT_OFF_DATA)
	    = (so_list->som_solib.data_start - private_section->vma);
	  ANOFFSET (offsets, SECT_OFF_BSS)
	    = ANOFFSET (offsets, SECT_OFF_DATA);
	  return 1;
	}
      so_list = so_list->next;
    }
  return 0;
}

/* Dump information about all the currently loaded shared libraries.  */

static void
som_sharedlibrary_info_command (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  struct so_list *so_list = so_list_head;

  if (exec_bfd == NULL)
    {
      printf_unfiltered ("no exec file.\n");
      return;
    }

  if (so_list == NULL)
    {
      printf_unfiltered ("No shared libraries loaded at this time.\n");
      return;
    }

  printf_unfiltered ("Shared Object Libraries\n");
  printf_unfiltered ("    %-12s%-12s%-12s%-12s%-12s%-12s\n",
		     "  flags", "  tstart", "   tend", "  dstart", "   dend", "   dlt");
  while (so_list)
    {
      unsigned int flags;

      flags = so_list->som_solib.struct_version << 24;
      flags |= so_list->som_solib.bind_mode << 16;
      flags |= so_list->som_solib.library_version;
      printf_unfiltered ("%s\n", so_list->som_solib.name);
      printf_unfiltered ("    %-12s", local_hex_string_custom (flags, "08l"));
      printf_unfiltered ("%-12s",
	      local_hex_string_custom (so_list->som_solib.text_addr, "08l"));
      printf_unfiltered ("%-12s",
	      local_hex_string_custom (so_list->som_solib.text_end, "08l"));
      printf_unfiltered ("%-12s",
	      local_hex_string_custom (so_list->som_solib.data_start, "08l"));
      printf_unfiltered ("%-12s",
	      local_hex_string_custom (so_list->som_solib.data_end, "08l"));
      printf_unfiltered ("%-12s\n",
	      local_hex_string_custom (so_list->som_solib.got_value, "08l"));
      so_list = so_list->next;
    }
}

static void
som_solib_sharedlibrary_command (args, from_tty)
     char *args;
     int from_tty;
{
  dont_repeat ();
  som_solib_add (args, from_tty, (struct target_ops *) 0);
}

void
_initialize_som_solib ()
{
  add_com ("sharedlibrary", class_files, som_solib_sharedlibrary_command,
           "Load shared object library symbols for files matching REGEXP.");
  add_info ("sharedlibrary", som_sharedlibrary_info_command,
	    "Status of loaded shared object libraries.");
  add_show_from_set
    (add_set_cmd ("auto-solib-add", class_support, var_zinteger,
		  (char *) &auto_solib_add,
		  "Set autoloading of shared library symbols at startup.\n\
If nonzero, symbols from all shared object libraries will be loaded\n\
automatically when the inferior begins execution or when the dynamic linker\n\
informs gdb that a new library has been loaded.  Otherwise, symbols\n\
must be loaded manually, using `sharedlibrary'.",
		  &setlist),
     &showlist);

}
