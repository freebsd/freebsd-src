/* Work with executable files, for GDB. 
   Copyright 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <sys/param.h>
#include <fcntl.h>
#include <string.h>

#include "gdbcore.h"

#include <ctype.h>
#include <sys/stat.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Prototypes for local functions */

static void
add_to_section_table PARAMS ((bfd *, sec_ptr, PTR));

static void
exec_close PARAMS ((int));

static void
file_command PARAMS ((char *, int));

static void
set_section_command PARAMS ((char *, int));

static void
exec_files_info PARAMS ((struct target_ops *));

extern int info_verbose;

/* The Binary File Descriptor handle for the executable file.  */

bfd *exec_bfd = NULL;

/* Whether to open exec and core files read-only or read-write.  */

int write_files = 0;

/* Text start and end addresses (KLUDGE) if needed */

#ifdef NEED_TEXT_START_END
CORE_ADDR text_start = 0;
CORE_ADDR text_end   = 0;
#endif

/* Forward decl */

extern struct target_ops exec_ops;

/* ARGSUSED */
static void
exec_close (quitting)
     int quitting;
{
  if (exec_bfd) {
    char *name = bfd_get_filename (exec_bfd);
    bfd_close (exec_bfd);
    free (name);
    exec_bfd = NULL;
  }
  if (exec_ops.to_sections) {
    free ((PTR)exec_ops.to_sections);
    exec_ops.to_sections = NULL;
    exec_ops.to_sections_end = NULL;
  }
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

  /* Remove any previous exec file.  */
  unpush_target (&exec_ops);

  /* Now open and digest the file the user requested, if any.  */

  if (args)
    {
      char *scratch_pathname;
      int scratch_chan;
      
      /* Scan through the args and pick up the first non option arg
	 as the filename. */

      if ((argv = buildargv (args)) == NULL)
	{
	  nomem (0);
	}
      make_cleanup (freeargv, (char *) argv);

      for (; (*argv != NULL) && (**argv == '-'); argv++) {;}
      if (*argv == NULL)
	{
	  error ("no exec file name was specified");
	}

      filename = tilde_expand (*argv);
      make_cleanup (free, filename);
      
      scratch_chan = openp (getenv ("PATH"), 1, filename, 
			    write_files? O_RDWR|O_BINARY: O_RDONLY|O_BINARY, 0,
			    &scratch_pathname);
      if (scratch_chan < 0)
	perror_with_name (filename);

      exec_bfd = bfd_fdopenr (scratch_pathname, gnutarget, scratch_chan);
      if (!exec_bfd)
	error ("Could not open `%s' as an executable file: %s",
	       scratch_pathname, bfd_errmsg (bfd_error));
      if (!bfd_check_format (exec_bfd, bfd_object))
	error ("\"%s\": not in executable format: %s.",
	       scratch_pathname, bfd_errmsg (bfd_error));

      if (build_section_table (exec_bfd, &exec_ops.to_sections,
				&exec_ops.to_sections_end))
	error ("Can't find the file sections in `%s': %s", 
		exec_bfd->filename, bfd_errmsg (bfd_error));

#ifdef NEED_TEXT_START_END

      /* text_end is sometimes used for where to put call dummies.  A
	 few ports use these for other purposes too.  */

      {
	struct section_table *p;

	/* Set text_start to the lowest address of the start of any
	   readonly code section and set text_end to the highest
	   address of the end of any readonly code section.  */

	text_start = ~(CORE_ADDR)0;
	text_end = (CORE_ADDR)0;
	for (p = exec_ops.to_sections; p < exec_ops.to_sections_end; p++)
	  if (bfd_get_section_flags (p->bfd, p->sec_ptr)
	      & (SEC_CODE | SEC_READONLY))
	    {
	      if (text_start > p->addr) 
		text_start = p->addr;
	      if (text_end < p->endaddr)
		text_end = p->endaddr;
	    }
      }
#endif

      validate_files ();

      push_target (&exec_ops);

      /* Tell display code (if any) about the changed file name.  */
      if (exec_file_display_hook)
	(*exec_file_display_hook) (filename);
    }
  else if (from_tty)
    printf ("No exec file now.\n");
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
  /* FIXME, we need to handle BSS segment here...it alloc's but doesn't load */
  if (!(aflag & SEC_LOAD))
    return;
  if (0 == bfd_section_size (abfd, asect))
    return;
  (*table_pp)->bfd = abfd;
  (*table_pp)->sec_ptr = asect;
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

  if (len <= 0)
    abort();

  memend = memaddr + len;
  xfer_fn = write? bfd_set_section_contents: bfd_get_section_contents;
  nextsectaddr = memend;

  for (p = target->to_sections; p < target->to_sections_end; p++)
    {
      if (p->addr <= memaddr)
	if (p->endaddr >= memend)
	  {
	    /* Entire transfer is within this section.  */
	    res = xfer_fn (p->bfd, p->sec_ptr, myaddr, memaddr - p->addr, len);
	    return (res != false)? len: 0;
	  }
	else if (p->endaddr <= memaddr)
	  {
	    /* This section ends before the transfer starts.  */
	    continue;
	  }
	else 
	  {
	    /* This section overlaps the transfer.  Just do half.  */
	    len = p->endaddr - memaddr;
	    res = xfer_fn (p->bfd, p->sec_ptr, myaddr, memaddr - p->addr, len);
	    return (res != false)? len: 0;
	  }
      else if (p->addr < nextsectaddr)
	nextsectaddr = p->addr;
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
  printf_filtered ("\tEntry point: %s\n",
		   local_hex_string ((unsigned long) bfd_get_start_address (exec_bfd)));
  for (p = t->to_sections; p < t->to_sections_end; p++) {
    printf_filtered ("\t%s", local_hex_string_custom ((unsigned long) p->addr, "08l"));
    printf_filtered (" - %s", local_hex_string_custom ((unsigned long) p->endaddr, "08l"));
    if (info_verbose)
      printf_filtered (" @ %s",
		       local_hex_string_custom ((unsigned long) p->sec_ptr->filepos, "08l"));
    printf_filtered (" is %s", bfd_section_name (p->bfd, p->sec_ptr));
    if (p->bfd != abfd) {
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
    if (!strncmp (secname, bfd_section_name (exec_bfd, p->sec_ptr), seclen)
	&& bfd_section_name (exec_bfd, p->sec_ptr)[seclen] == '\0') {
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
   `gdb internal error' (since generic_mourn calls breakpoint_init_inferior).  */

static int
ignore (addr, contents)
     CORE_ADDR addr;
     char *contents;
{
  return 0;
}

struct target_ops exec_ops = {
	"exec", "Local exec file",
	"Use an executable file as a target.\n\
Specify the filename of the executable file.",
	exec_file_command, exec_close, /* open, close */
	find_default_attach, 0, 0, 0, /* attach, detach, resume, wait, */
	0, 0, /* fetch_registers, store_registers, */
	0, /* prepare_to_store, */
	xfer_memory, exec_files_info,
	ignore, ignore, /* insert_breakpoint, remove_breakpoint, */
	0, 0, 0, 0, 0, /* terminal stuff */
	0, 0, /* kill, load */
	0, /* lookup sym */
	find_default_create_inferior,
	0, /* mourn_inferior */
	0, /* can_run */
	0, /* notice_signals */
	file_stratum, 0, /* next */
	0, 1, 0, 0, 0,	/* all mem, mem, stack, regs, exec */
	0, 0,			/* section pointers */
	OPS_MAGIC,		/* Always the last thing */
};

void
_initialize_exec()
{
  struct cmd_list_element *c;

  c = add_cmd ("file", class_files, file_command,
	       "Use FILE as program to be debugged.\n\
It is read for its symbols, for getting the contents of pure memory,\n\
and it is the program executed when you use the `run' command.\n\
If FILE cannot be found as specified, your execution directory path\n\
($PATH) is searched for a command of that name.\n\
No arg means to have no executable file and no symbols.", &cmdlist);
  c->completer = filename_completer;

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
