/* Core dump and executable file functions below target vector, for GDB.
   Copyright 1986, 87, 89, 91, 92, 93, 94, 95, 96, 97, 1998
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
#include "gdb_string.h"
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symtab.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbthread.h"

/* List of all available core_fns.  On gdb startup, each core file register
   reader calls add_core_fns() to register information on each core format it
   is prepared to read. */

static struct core_fns *core_file_fns = NULL;

static void core_files_info PARAMS ((struct target_ops *));

#ifdef SOLIB_ADD
static int solib_add_stub PARAMS ((PTR));
#endif

static void core_open PARAMS ((char *, int));

static void core_detach PARAMS ((char *, int));

static void core_close PARAMS ((int));

static void get_core_registers PARAMS ((int));

static void add_to_thread_list PARAMS ((bfd *, asection *, PTR));

static int ignore PARAMS ((CORE_ADDR, char *));

static char *core_file_to_sym_file PARAMS ((char *));

static int core_file_thread_alive PARAMS ((int tid));

static void init_core_ops PARAMS ((void));

void _initialize_corelow PARAMS ((void));

struct target_ops core_ops;

/* Link a new core_fns into the global core_file_fns list.  Called on gdb
   startup by the _initialize routine in each core file register reader, to
   register information about each format the the reader is prepared to
   handle. */

void
add_core_fns (cf)
     struct core_fns *cf;
{
  cf -> next = core_file_fns;
  core_file_fns = cf;
}


/* Discard all vestiges of any previous core file and mark data and stack
   spaces as empty.  */

/* ARGSUSED */
static void
core_close (quitting)
     int quitting;
{
  char *name;

  if (core_bfd)
    {
      inferior_pid = 0;		/* Avoid confusion from thread stuff */

      name = bfd_get_filename (core_bfd);
      if (!bfd_close (core_bfd))
	warning ("cannot close \"%s\": %s",
		 name, bfd_errmsg (bfd_get_error ()));
      free (name);
      core_bfd = NULL;
#ifdef CLEAR_SOLIB
      CLEAR_SOLIB ();
#endif
      if (core_ops.to_sections)
	{
	  free ((PTR)core_ops.to_sections);
	  core_ops.to_sections = NULL;
	  core_ops.to_sections_end = NULL;
	}
    }
}

#ifdef SOLIB_ADD
/* Stub function for catch_errors around shared library hacking.  FROM_TTYP
   is really an int * which points to from_tty.  */

static int 
solib_add_stub (from_ttyp)
     PTR from_ttyp;
{
  SOLIB_ADD (NULL, *(int *)from_ttyp, &current_target);
  re_enable_breakpoints_in_shlibs ();
  return 0;
}
#endif /* SOLIB_ADD */

/* Look for sections whose names start with `.reg/' so that we can extract the
   list of threads in a core file.  */

static void
add_to_thread_list (abfd, asect, reg_sect_arg)
     bfd *abfd;
     asection *asect;
     PTR reg_sect_arg;
{
  int thread_id;
  asection *reg_sect = (asection *) reg_sect_arg;

  if (strncmp (bfd_section_name (abfd, asect), ".reg/", 5) != 0)
    return;

  thread_id = atoi (bfd_section_name (abfd, asect) + 5);

  add_thread (thread_id);

/* Warning, Will Robinson, looking at BFD private data! */

  if (reg_sect != NULL
      && asect->filepos == reg_sect->filepos) /* Did we find .reg? */
    inferior_pid = thread_id;	/* Yes, make it current */
}

/* This routine opens and sets up the core file bfd.  */

static void
core_open (filename, from_tty)
     char *filename;
     int from_tty;
{
  const char *p;
  int siggy;
  struct cleanup *old_chain;
  char *temp;
  bfd *temp_bfd;
  int ontop;
  int scratch_chan;

  target_preopen (from_tty);
  if (!filename)
    {
      error (core_bfd ? 
       "No core file specified.  (Use `detach' to stop debugging a core file.)"
     : "No core file specified.");
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
      temp = concat (current_directory, "/", filename, NULL);
      free (filename);
      filename = temp;
    }

  old_chain = make_cleanup (free, filename);

  scratch_chan = open (filename, write_files ? O_RDWR : O_RDONLY, 0);
  if (scratch_chan < 0)
    perror_with_name (filename);

  temp_bfd = bfd_fdopenr (filename, gnutarget, scratch_chan);
  if (temp_bfd == NULL)
    perror_with_name (filename);

  if (!bfd_check_format (temp_bfd, bfd_core))
    {
      /* Do it after the err msg */
      /* FIXME: should be checking for errors from bfd_close (for one thing,
	 on error it does not free all the storage associated with the
	 bfd).  */
      make_cleanup ((make_cleanup_func) bfd_close, temp_bfd);
      error ("\"%s\" is not a core dump: %s",
	     filename, bfd_errmsg (bfd_get_error ()));
    }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain);		/* Don't free filename any more */
  unpush_target (&core_ops);
  core_bfd = temp_bfd;
  old_chain = make_cleanup ((make_cleanup_func) core_close, core_bfd);

  validate_files ();

  /* Find the data section */
  if (build_section_table (core_bfd, &core_ops.to_sections,
			   &core_ops.to_sections_end))
    error ("\"%s\": Can't find sections: %s",
	   bfd_get_filename (core_bfd), bfd_errmsg (bfd_get_error ()));

  ontop = !push_target (&core_ops);
  discard_cleanups (old_chain);

  p = bfd_core_file_failing_command (core_bfd);
  if (p)
    printf_filtered ("Core was generated by `%s'.\n", p);

  siggy = bfd_core_file_failing_signal (core_bfd);
  if (siggy > 0)
    printf_filtered ("Program terminated with signal %d, %s.\n", siggy,
		     safe_strsignal (siggy));

  /* Build up thread list from BFD sections. */

  init_thread_list ();
  bfd_map_over_sections (core_bfd, add_to_thread_list,
			 bfd_get_section_by_name (core_bfd, ".reg"));

  if (ontop)
    {
      /* Fetch all registers from core file.  */
      target_fetch_registers (-1);

      /* Add symbols and section mappings for any shared libraries.  */
#ifdef SOLIB_ADD
      catch_errors (solib_add_stub, &from_tty, (char *)0,
		    RETURN_MASK_ALL);
#endif

      /* Now, set up the frame cache, and print the top of stack.  */
      flush_cached_frames ();
      select_frame (get_current_frame (), 0);
      print_stack_frame (selected_frame, selected_frame_level, 1);
    }
  else
    {
      warning (
"you won't be able to access this core file until you terminate\n\
your %s; do ``info files''", target_longname);
    }
}

static void
core_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    error ("Too many arguments");
  unpush_target (&core_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No core file now.\n");
}

/* Get the registers out of a core file.  This is the machine-
   independent part.  Fetch_core_registers is the machine-dependent
   part, typically implemented in the xm-file for each architecture.  */

/* We just get all the registers, so we don't use regno.  */

/* ARGSUSED */
static void
get_core_registers (regno)
     int regno;
{
  sec_ptr reg_sec;
  unsigned size;
  char *the_regs;
  char secname[30];
  enum bfd_flavour our_flavour = bfd_get_flavour (core_bfd);
  struct core_fns *cf = NULL;

  if (core_file_fns == NULL)
    {
      fprintf_filtered (gdb_stderr,
			"Can't fetch registers from this type of core file\n");
      return;
    }

  /* Thread support.  If inferior_pid is non-zero, then we have found a core
     file with threads (or multiple processes).  In that case, we need to
     use the appropriate register section, else we just use `.reg'. */

  /* XXX - same thing needs to be done for floating-point (.reg2) sections. */

  if (inferior_pid)
    sprintf (secname, ".reg/%d", inferior_pid);
  else
    strcpy (secname, ".reg");

  reg_sec = bfd_get_section_by_name (core_bfd, secname);
  if (!reg_sec)
    goto cant;
  size = bfd_section_size (core_bfd, reg_sec);
  the_regs = alloca (size);
  /* Look for the core functions that match this flavor.  Default to the
     first one if nothing matches. */
  for (cf = core_file_fns; cf != NULL; cf = cf -> next)
    {
      if (our_flavour == cf -> core_flavour)
	{
	  break;
	}
    }
  if (cf == NULL)
    {
      cf = core_file_fns;
    }
  if (cf != NULL &&
      bfd_get_section_contents (core_bfd, reg_sec, the_regs, (file_ptr)0, size) &&
      cf -> core_read_registers != NULL)
    {
      (cf -> core_read_registers (the_regs, size, 0,
				  (unsigned) bfd_section_vma (abfd,reg_sec)));
    }
  else
    {
cant:
      fprintf_filtered (gdb_stderr,
			"Couldn't fetch registers from core file: %s\n",
			bfd_errmsg (bfd_get_error ()));
    }

  /* Now do it again for the float registers, if they exist.  */
  reg_sec = bfd_get_section_by_name (core_bfd, ".reg2");
  if (reg_sec)
    {
      size = bfd_section_size (core_bfd, reg_sec);
      the_regs = alloca (size);
      if (cf != NULL &&
	  bfd_get_section_contents (core_bfd, reg_sec, the_regs, (file_ptr)0, size) &&
	  cf -> core_read_registers != NULL)
	{
	  (cf -> core_read_registers (the_regs, size, 2,
				      (unsigned) bfd_section_vma (abfd,reg_sec)));
	}
      else
	{
	  fprintf_filtered (gdb_stderr, 
			    "Couldn't fetch register set 2 from core file: %s\n",
			    bfd_errmsg (bfd_get_error ()));
	}
    }
  registers_fetched ();
}

static char *
core_file_to_sym_file (core)
  char *  core;
{
  CONST char *  failing_command;
  char *  p;
  char *  temp;
  bfd *  temp_bfd;
  int  scratch_chan;

  if (! core)
    error ("No core file specified.");

  core = tilde_expand (core);
  if (core[0] != '/')
    {
      temp = concat (current_directory, "/", core, NULL);
      core = temp;
    }

  scratch_chan = open (core, write_files ? O_RDWR : O_RDONLY, 0);
  if (scratch_chan < 0)
    perror_with_name (core);

  temp_bfd = bfd_fdopenr (core, gnutarget, scratch_chan);
  if (temp_bfd == NULL)
    perror_with_name (core);

  if (!bfd_check_format (temp_bfd, bfd_core))
    {
      /* Do it after the err msg */
      /* FIXME: should be checking for errors from bfd_close (for one thing,
	 on error it does not free all the storage associated with the
	 bfd).  */
      make_cleanup ((make_cleanup_func) bfd_close, temp_bfd);
      error ("\"%s\" is not a core dump: %s",
	     core, bfd_errmsg (bfd_get_error ()));
    }

  /* Find the data section */
  if (build_section_table (temp_bfd, &core_ops.to_sections,
			   &core_ops.to_sections_end))
    error ("\"%s\": Can't find sections: %s",
	   bfd_get_filename (temp_bfd), bfd_errmsg (bfd_get_error ()));

  failing_command = bfd_core_file_failing_command (temp_bfd);

  bfd_close (temp_bfd);

  /* If we found a filename, remember that it is probably saved
     relative to the executable that created it.  If working directory
     isn't there now, we may not be able to find the executable.  Rather
     than trying to be sauve about finding it, just check if the file
     exists where we are now.  If not, then punt and tell our client
     we couldn't find the sym file.
     */
  p = (char *) failing_command;
  if ((p != NULL) && (access (p, F_OK) != 0))
    p = NULL;

  return p;
}

static void
core_files_info (t)
  struct target_ops *t;
{
  print_section_info (t, core_bfd);
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls breakpoint_init_inferior).  */

static int
ignore (addr, contents)
     CORE_ADDR addr;
     char *contents;
{
  return 0;
}


/* Okay, let's be honest: threads gleaned from a core file aren't
   exactly lively, are they?  On the other hand, if we don't claim
   that each & every one is alive, then we don't get any of them
   to appear in an "info thread" command, which is quite a useful
   behaviour.
   */
static int
core_file_thread_alive (tid)
  int  tid;
{
  return 1;
}

/* Fill in core_ops with its defined operations and properties.  */

static void
init_core_ops ()
{
  core_ops.to_shortname = "core";
  core_ops.to_longname = "Local core dump file";
  core_ops.to_doc =
    "Use a core file as a target.  Specify the filename of the core file.";
  core_ops.to_open = core_open;
  core_ops.to_close = core_close;
  core_ops.to_attach = find_default_attach;
  core_ops.to_require_attach = find_default_require_attach;
  core_ops.to_detach = core_detach;
  core_ops.to_require_detach = find_default_require_detach;
  core_ops.to_fetch_registers = get_core_registers;
  core_ops.to_xfer_memory = xfer_memory;
  core_ops.to_files_info = core_files_info;
  core_ops.to_insert_breakpoint = ignore;
  core_ops.to_remove_breakpoint = ignore;
  core_ops.to_create_inferior = find_default_create_inferior;
  core_ops.to_clone_and_follow_inferior = find_default_clone_and_follow_inferior;
  core_ops.to_thread_alive = core_file_thread_alive;
  core_ops.to_core_file_to_sym_file = core_file_to_sym_file;
  core_ops.to_stratum = core_stratum;
  core_ops.to_has_memory = 1;
  core_ops.to_has_stack = 1;
  core_ops.to_has_registers = 1;
  core_ops.to_magic = OPS_MAGIC;	
}

/* non-zero if we should not do the add_target call in
   _initialize_corelow; not initialized (i.e., bss) so that
   the target can initialize it (i.e., data) if appropriate.
   This needs to be set at compile time because we don't know
   for sure whether the target's initialize routine is called
   before us or after us. */
int coreops_suppress_target;

void
_initialize_corelow ()
{
  init_core_ops ();

  if (!coreops_suppress_target)
    add_target (&core_ops);
}
