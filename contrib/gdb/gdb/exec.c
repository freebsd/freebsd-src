/* Work with executable files, for GDB. 
   Copyright 1988, 1989, 1991, 1992, 1993, 1994, 1997, 1998
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
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "language.h"
#include "symfile.h"
#include "objfiles.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <fcntl.h>
#include "gdb_string.h"

#include "gdbcore.h"

#include <ctype.h>
#include "gdb_stat.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "xcoffsolib.h"

struct vmap *map_vmap PARAMS ((bfd *, bfd *));

void (*file_changed_hook) PARAMS ((char *));

/* Prototypes for local functions */

static void add_to_section_table PARAMS ((bfd *, sec_ptr, PTR));

static void exec_close PARAMS ((int));

static void file_command PARAMS ((char *, int));

static void set_section_command PARAMS ((char *, int));

static void exec_files_info PARAMS ((struct target_ops *));

static void bfdsec_to_vmap PARAMS ((bfd *, sec_ptr, PTR));

static int ignore PARAMS ((CORE_ADDR, char *));

static void init_exec_ops PARAMS ((void));

void _initialize_exec PARAMS ((void));

extern int info_verbose;

/* The target vector for executable files.  */

struct target_ops exec_ops;

/* The Binary File Descriptor handle for the executable file.  */

bfd *exec_bfd = NULL;

/* Whether to open exec and core files read-only or read-write.  */

int write_files = 0;

/* Text start and end addresses (KLUDGE) if needed */

#ifndef NEED_TEXT_START_END
#define NEED_TEXT_START_END (0)
#endif
CORE_ADDR text_start = 0;
CORE_ADDR text_end   = 0;

struct vmap *vmap;

/* ARGSUSED */
static void
exec_close (quitting)
     int quitting;
{
  int need_symtab_cleanup = 0;
  struct vmap *vp, *nxt;
  
  for (nxt = vmap; nxt != NULL; )
    {
      vp = nxt;
      nxt = vp->nxt;

      /* if there is an objfile associated with this bfd,
	 free_objfile() will do proper cleanup of objfile *and* bfd. */
		   
      if (vp->objfile)
	{
	  free_objfile (vp->objfile);
	  need_symtab_cleanup = 1;
	}
      else if (vp->bfd != exec_bfd)
	/* FIXME-leak: We should be freeing vp->name too, I think.  */
	if (!bfd_close (vp->bfd))
	  warning ("cannot close \"%s\": %s",
		   vp->name, bfd_errmsg (bfd_get_error ()));

      /* FIXME: This routine is #if 0'd in symfile.c.  What should we
	 be doing here?  Should we just free everything in
	 vp->objfile->symtabs?  Should free_objfile do that?
	 FIXME-as-well: free_objfile already free'd vp->name, so it isn't
	 valid here.  */
      free_named_symtabs (vp->name);
      free (vp);
    }

  vmap = NULL;

  if (exec_bfd)
    {
      char *name = bfd_get_filename (exec_bfd);

      if (!bfd_close (exec_bfd))
	warning ("cannot close \"%s\": %s",
		 name, bfd_errmsg (bfd_get_error ()));
      free (name);
      exec_bfd = NULL;
    }

  if (exec_ops.to_sections)
    {
      free ((PTR)exec_ops.to_sections);
      exec_ops.to_sections = NULL;
      exec_ops.to_sections_end = NULL;
    }
}

/*  Process the first arg in ARGS as the new exec file.

    This function is intended to be behave essentially the same
    as exec_file_command, except that the latter will detect when
    a target is being debugged, and will ask the user whether it
    should be shut down first.  (If the answer is "no", then the
    new file is ignored.)

    This file is used by exec_file_command, to do the work of opening
    and processing the exec file after any prompting has happened.

    And, it is used by child_attach, when the attach command was
    given a pid but not a exec pathname, and the attach command could
    figure out the pathname from the pid.  (In this case, we shouldn't
    ask the user whether the current target should be shut down --
    we're supplying the exec pathname late for good reason.) */

void
exec_file_attach (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  char *filename;

  /* Remove any previous exec file.  */
  unpush_target (&exec_ops);

  /* Now open and digest the file the user requested, if any.  */

  if (args)
    {
      char *scratch_pathname;
      int scratch_chan;
      
      /* Scan through the args and pick up the first non option arg
	 as the filename.  */

      argv = buildargv (args);
      if (argv == NULL)
	nomem (0);

      make_cleanup ((make_cleanup_func) freeargv, (char *) argv);

      for (; (*argv != NULL) && (**argv == '-'); argv++) {;}
      if (*argv == NULL)
	error ("no exec file name was specified");

      filename = tilde_expand (*argv);
      make_cleanup (free, filename);
      
      scratch_chan = openp (getenv ("PATH"), 1, filename, 
			    write_files? O_RDWR|O_BINARY: O_RDONLY|O_BINARY, 0,
			    &scratch_pathname);
#if defined(__GO32__) || defined(_WIN32)
      if (scratch_chan < 0)
      {
	char *exename = alloca (strlen (filename) + 5);
	strcat (strcpy (exename, filename), ".exe");
	scratch_chan = openp (getenv ("PATH"), 1, exename, write_files ?
		O_RDWR|O_BINARY : O_RDONLY|O_BINARY, 0, &scratch_pathname);
      }
#endif
      if (scratch_chan < 0)
	perror_with_name (filename);
      fcntl (scratch_chan, F_SETFD, 1);
      exec_bfd = bfd_fdopenr (scratch_pathname, gnutarget, scratch_chan);

      if (!exec_bfd)
	error ("\"%s\": could not open as an executable file: %s",
	       scratch_pathname, bfd_errmsg (bfd_get_error ()));

      /* At this point, scratch_pathname and exec_bfd->name both point to the
	 same malloc'd string.  However exec_close() will attempt to free it
	 via the exec_bfd->name pointer, so we need to make another copy and
	 leave exec_bfd as the new owner of the original copy. */
      scratch_pathname = strdup (scratch_pathname);
      make_cleanup (free, scratch_pathname);
      
      if (!bfd_check_format (exec_bfd, bfd_object))
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error ("\"%s\": not in executable format: %s",
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}

      /* FIXME - This should only be run for RS6000, but the ifdef is a poor
	 way to accomplish.  */
#ifdef IBM6000_TARGET
      /* Setup initial vmap. */

      map_vmap (exec_bfd, 0);
      if (vmap == NULL)
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error ("\"%s\": can't find the file sections: %s",
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}
#endif /* IBM6000_TARGET */

      if (build_section_table (exec_bfd, &exec_ops.to_sections,
				&exec_ops.to_sections_end))
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error ("\"%s\": can't find the file sections: %s", 
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}

      /* text_end is sometimes used for where to put call dummies.  A
	 few ports use these for other purposes too.  */
      if (NEED_TEXT_START_END)
	{
	  struct section_table *p;
	  
	  /* Set text_start to the lowest address of the start of any
	     readonly code section and set text_end to the highest
	     address of the end of any readonly code section.  */
	  /* FIXME: The comment above does not match the code.  The
	     code checks for sections with are either code *or*
	     readonly.  */
	  text_start = ~(CORE_ADDR)0;
	  text_end = (CORE_ADDR)0;
	  for (p = exec_ops.to_sections; p < exec_ops.to_sections_end; p++)
	    if (bfd_get_section_flags (p->bfd, p->the_bfd_section)
		& (SEC_CODE | SEC_READONLY))
	      {
		if (text_start > p->addr) 
		  text_start = p->addr;
		if (text_end < p->endaddr)
		  text_end = p->endaddr;
	      }
	}

      validate_files ();

      set_gdbarch_from_file (exec_bfd);

      push_target (&exec_ops);

      /* Tell display code (if any) about the changed file name.  */
      if (exec_file_display_hook)
	(*exec_file_display_hook) (filename);
    }
  else if (from_tty)
    printf_unfiltered ("No executable file now.\n");
}

/*  Process the first arg in ARGS as the new exec file.

    Note that we have to explicitly ignore additional args, since we can
    be called from file_command(), which also calls symbol_file_command()
    which can take multiple args. */

void
exec_file_command (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  char *filename;

  target_preopen (from_tty);

  exec_file_attach (args, from_tty);
}

/* Set both the exec file and the symbol file, in one command.  
   What a novelty.  Why did GDB go through four major releases before this
   command was added?  */

static void
file_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  /* FIXME, if we lose on reading the symbol file, we should revert
     the exec file, but that's rough.  */
  exec_file_command (arg, from_tty);
  symbol_file_command (arg, from_tty);
  if (file_changed_hook)
    file_changed_hook (arg);
}


/* Locate all mappable sections of a BFD file. 
   table_pp_char is a char * to get it through bfd_map_over_sections;
   we cast it back to its proper type.  */

static void
add_to_section_table (abfd, asect, table_pp_char)
     bfd *abfd;
     sec_ptr asect;
     PTR table_pp_char;
{
  struct section_table **table_pp = (struct section_table **)table_pp_char;
  flagword aflag;

  aflag = bfd_get_section_flags (abfd, asect);
  if (!(aflag & SEC_ALLOC))
    return;
  if (0 == bfd_section_size (abfd, asect))
    return;
  (*table_pp)->bfd = abfd;
  (*table_pp)->the_bfd_section = asect;
  (*table_pp)->addr = bfd_section_vma (abfd, asect);
  (*table_pp)->endaddr = (*table_pp)->addr + bfd_section_size (abfd, asect);
  (*table_pp)++;
}

/* Builds a section table, given args BFD, SECTABLE_PTR, SECEND_PTR.
   Returns 0 if OK, 1 on error.  */

int
build_section_table (some_bfd, start, end)
     bfd *some_bfd;
     struct section_table **start, **end;
{
  unsigned count;

  count = bfd_count_sections (some_bfd);
  if (*start)
    free ((PTR)*start);
  *start = (struct section_table *) xmalloc (count * sizeof (**start));
  *end = *start;
  bfd_map_over_sections (some_bfd, add_to_section_table, (char *)end);
  if (*end > *start + count)
    abort();
  /* We could realloc the table, but it probably loses for most files.  */
  return 0;
}

static void
bfdsec_to_vmap(abfd, sect, arg3) 
     bfd *abfd;
     sec_ptr sect;
     PTR arg3;
{
  struct vmap_and_bfd *vmap_bfd = (struct vmap_and_bfd *) arg3;
  struct vmap *vp;

  vp = vmap_bfd->pvmap;

  if ((bfd_get_section_flags (abfd, sect) & SEC_LOAD) == 0)
    return;

  if (STREQ (bfd_section_name (abfd, sect), ".text"))
    {
      vp->tstart = bfd_section_vma (abfd, sect);
      vp->tend = vp->tstart + bfd_section_size (abfd, sect);
      vp->tvma = bfd_section_vma (abfd, sect);
      vp->toffs = sect->filepos;
    }
  else if (STREQ (bfd_section_name (abfd, sect), ".data"))
    {
      vp->dstart = bfd_section_vma (abfd, sect);
      vp->dend = vp->dstart + bfd_section_size (abfd, sect);
      vp->dvma = bfd_section_vma (abfd, sect);
    }
  /* Silently ignore other types of sections. (FIXME?)  */
}

/* Make a vmap for ABFD which might be a member of the archive ARCH.
   Return the new vmap.  */

struct vmap *
map_vmap (abfd, arch)
     bfd *abfd;
     bfd *arch;
{
  struct vmap_and_bfd vmap_bfd;
  struct vmap *vp, **vpp;

  vp = (struct vmap *) xmalloc (sizeof (*vp));
  memset ((char *) vp, '\0', sizeof (*vp));
  vp->nxt = 0;
  vp->bfd = abfd;
  vp->name = bfd_get_filename (arch ? arch : abfd);
  vp->member = arch ? bfd_get_filename (abfd) : "";
  
  vmap_bfd.pbfd = arch;
  vmap_bfd.pvmap = vp;
  bfd_map_over_sections (abfd, bfdsec_to_vmap, &vmap_bfd);

  /* Find the end of the list and append. */
  for (vpp = &vmap; *vpp; vpp = &(*vpp)->nxt)
    ;
  *vpp = vp;

  return vp;
}

/* Read or write the exec file.

   Args are address within a BFD file, address within gdb address-space,
   length, and a flag indicating whether to read or write.

   Result is a length:

	0:    We cannot handle this address and length.
	> 0:  We have handled N bytes starting at this address.
	      (If N == length, we did it all.)  We might be able
	      to handle more bytes beyond this length, but no
	      promises.
	< 0:  We cannot handle this address, but if somebody
	      else handles (-N) bytes, we can start from there.

    The same routine is used to handle both core and exec files;
    we just tail-call it with more arguments to select between them.  */

int
xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;
{
  boolean res;
  struct section_table *p;
  CORE_ADDR nextsectaddr, memend;
  boolean (*xfer_fn) PARAMS ((bfd *, sec_ptr, PTR, file_ptr, bfd_size_type));
  asection *section;

  if (len <= 0)
    abort();

  if (overlay_debugging)
    {
      section = find_pc_overlay (memaddr);
      if (pc_in_unmapped_range (memaddr, section))
	memaddr = overlay_mapped_address (memaddr, section);
    }

  memend = memaddr + len;
  xfer_fn = write ? bfd_set_section_contents : bfd_get_section_contents;
  nextsectaddr = memend;

#if 0 /* Stu's implementation */
/* If a section has been specified, try to use it.  Note that we cannot use the
   specified section directly.  This is because it usually comes from the
   symbol file, which may be different from the exec or core file.  Instead, we
   have to lookup the specified section by name in the bfd associated with
   to_sections.  */

  if (target_memory_bfd_section)
    {
      asection *s;
      bfd *abfd;
      asection *target_section;
      bfd *target_bfd;

      s = target_memory_bfd_section;
      abfd = s->owner;

      target_bfd = target->to_sections->bfd;
      target_section = bfd_get_section_by_name (target_bfd, bfd_section_name (abfd, s));

      if (target_section)
	{
	  bfd_vma sec_addr;
	  bfd_size_type sec_size;

	  sec_addr = bfd_section_vma (target_bfd, target_section);
	  sec_size = target_section->_raw_size;

	  /* Make sure the requested memory starts inside the section.  */

	  if (memaddr >= sec_addr
	      && memaddr < sec_addr + sec_size)
	    {
	      /* Cut back length in case request overflows the end of the section. */
	      len = min (len, sec_addr + sec_size - memaddr);

	      res = xfer_fn (target_bfd, target_section, myaddr, memaddr - sec_addr, len);

	      return res ? len : 0;
	    }
	}
    }
#endif /* 0, Stu's implementation */
  for (p = target->to_sections; p < target->to_sections_end; p++)
    {
      if (overlay_debugging && section && p->the_bfd_section &&
	  strcmp (section->name, p->the_bfd_section->name) != 0)
	continue;	/* not the section we need */
      if (memaddr >= p->addr)
 	if (memend <= p->endaddr)
	  {
	    /* Entire transfer is within this section.  */
	    res = xfer_fn (p->bfd, p->the_bfd_section, myaddr,
			   memaddr - p->addr, len);
	    return (res != 0) ? len : 0;
	  }
	else if (memaddr >= p->endaddr)
	  {
	    /* This section ends before the transfer starts.  */
	    continue;
	  }
	else 
	  {
	    /* This section overlaps the transfer.  Just do half.  */
	    len = p->endaddr - memaddr;
	    res = xfer_fn (p->bfd, p->the_bfd_section, myaddr,
			   memaddr - p->addr, len);
	    return (res != 0) ? len : 0;
	  }
      else
	nextsectaddr = min (nextsectaddr, p->addr);
    }

  if (nextsectaddr >= memend)
    return 0;				/* We can't help */
  else
    return - (nextsectaddr - memaddr);	/* Next boundary where we can help */
}

#ifdef FIXME
#ifdef REG_STACK_SEGMENT
/* MOVE TO BFD... */
    /* Pyramids and AM29000s have an extra segment in the virtual address space
       for the (control) stack of register-window frames.  The AM29000 folk
       call it the "register stack" rather than the "memory stack".  */
    else if (memaddr >= reg_stack_start && memaddr < reg_stack_end)
      {
	i = min (len, reg_stack_end - memaddr);
	fileptr = memaddr - reg_stack_start + reg_stack_offset;
	wanna_xfer = coredata;
      }
#endif				/* REG_STACK_SEGMENT */
#endif /* FIXME */

void
print_section_info (t, abfd)
  struct target_ops *t;
  bfd *abfd;
{
  struct section_table *p;

  printf_filtered ("\t`%s', ", bfd_get_filename(abfd));
  wrap_here ("        ");
  printf_filtered ("file type %s.\n", bfd_get_target(abfd));
  if (abfd == exec_bfd)
    {
      printf_filtered ("\tEntry point: ");
      print_address_numeric (bfd_get_start_address (abfd), 1, gdb_stdout);
      printf_filtered ("\n");
    }
  for (p = t->to_sections; p < t->to_sections_end; p++)
    {
      /* FIXME-32x64 need a print_address_numeric with field width */
      printf_filtered ("\t%s", local_hex_string_custom ((unsigned long) p->addr, "08l"));
      printf_filtered (" - %s", local_hex_string_custom ((unsigned long) p->endaddr, "08l"));
      if (info_verbose)
	printf_filtered (" @ %s",
			 local_hex_string_custom ((unsigned long) p->the_bfd_section->filepos, "08l"));
      printf_filtered (" is %s", bfd_section_name (p->bfd, p->the_bfd_section));
      if (p->bfd != abfd)
	{
	  printf_filtered (" in %s", bfd_get_filename (p->bfd));
	}
      printf_filtered ("\n");
    }
}

static void
exec_files_info (t)
     struct target_ops *t;
{
  print_section_info (t, exec_bfd);

  if (vmap)
    {
      struct vmap *vp;

      printf_unfiltered ("\tMapping info for file `%s'.\n", vmap->name);
      printf_unfiltered ("\t  %8.8s   %8.8s   %8.8s   %8.8s %8.8s %s\n",
		       "tstart", "tend", "dstart", "dend", "section",
		       "file(member)");

    for (vp = vmap; vp; vp = vp->nxt)
      printf_unfiltered ("\t0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x %s%s%s%s\n",
			 vp->tstart, vp->tend, vp->dstart, vp->dend, vp->name,
			 *vp->member ? "(" : "", vp->member,
			 *vp->member ? ")" : "");
    }
}

static void
set_section_command (args, from_tty)
     char *args;
     int from_tty;
{
  struct section_table *p;
  char *secname;
  unsigned seclen;
  unsigned long secaddr;
  char secprint[100];
  long offset;

  if (args == 0)
    error ("Must specify section name and its virtual address");

  /* Parse out section name */
  for (secname = args; !isspace(*args); args++) ;
  seclen = args - secname;

  /* Parse out new virtual address */
  secaddr = parse_and_eval_address (args);

  for (p = exec_ops.to_sections; p < exec_ops.to_sections_end; p++) {
    if (!strncmp (secname, bfd_section_name (exec_bfd, p->the_bfd_section), seclen)
	&& bfd_section_name (exec_bfd, p->the_bfd_section)[seclen] == '\0') {
      offset = secaddr - p->addr;
      p->addr += offset;
      p->endaddr += offset;
      if (from_tty)
	exec_files_info(&exec_ops);
      return;
    }
  } 
  if (seclen >= sizeof (secprint))
    seclen = sizeof (secprint) - 1;
  strncpy (secprint, secname, seclen);
  secprint[seclen] = '\0';
  error ("Section %s not found", secprint);
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls
   breakpoint_init_inferior).  */

static int
ignore (addr, contents)
     CORE_ADDR addr;
     char *contents;
{
  return 0;
}

/* Fill in the exec file target vector.  Very few entries need to be
   defined.  */

void
init_exec_ops ()
{
  exec_ops.to_shortname = "exec";
  exec_ops.to_longname = "Local exec file";
  exec_ops.to_doc = "Use an executable file as a target.\n\
Specify the filename of the executable file.";
  exec_ops.to_open = exec_file_command;
  exec_ops.to_close = exec_close;
  exec_ops.to_attach = find_default_attach;
  exec_ops.to_require_attach = find_default_require_attach;
  exec_ops.to_require_detach = find_default_require_detach;
  exec_ops.to_xfer_memory = xfer_memory;
  exec_ops.to_files_info = exec_files_info;
  exec_ops.to_insert_breakpoint = ignore;
  exec_ops.to_remove_breakpoint = ignore;
  exec_ops.to_create_inferior = find_default_create_inferior;
  exec_ops.to_clone_and_follow_inferior = find_default_clone_and_follow_inferior;
  exec_ops.to_stratum = file_stratum;
  exec_ops.to_has_memory = 1;
  exec_ops.to_magic = OPS_MAGIC;	
}

void
_initialize_exec ()
{
  struct cmd_list_element *c;

  init_exec_ops ();

  if (!dbx_commands)
    {
      c = add_cmd ("file", class_files, file_command,
		   "Use FILE as program to be debugged.\n\
It is read for its symbols, for getting the contents of pure memory,\n\
and it is the program executed when you use the `run' command.\n\
If FILE cannot be found as specified, your execution directory path\n\
($PATH) is searched for a command of that name.\n\
No arg means to have no executable file and no symbols.", &cmdlist);
      c->completer = filename_completer;
    }

  c = add_cmd ("exec-file", class_files, exec_file_command,
	   "Use FILE as program for getting contents of pure memory.\n\
If FILE cannot be found as specified, your execution directory path\n\
is searched for a command of that name.\n\
No arg means have no executable file.", &cmdlist);
  c->completer = filename_completer;

  add_com ("section", class_files, set_section_command,
   "Change the base address of section SECTION of the exec file to ADDR.\n\
This can be used if the exec file does not contain section addresses,\n\
(such as in the a.out format), or when the addresses specified in the\n\
file itself are wrong.  Each section must be changed separately.  The\n\
``info files'' command lists all the sections and their addresses.");

  add_show_from_set
    (add_set_cmd ("write", class_support, var_boolean, (char *)&write_files,
		  "Set writing into executable and core files.",
		  &setlist),
     &showlist);
  
  add_target (&exec_ops);
}
