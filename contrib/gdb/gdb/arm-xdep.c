/* Acorn Risc Machine host machine support.
   Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc.

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
#include "arm-opcode.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#define N_TXTADDR(hdr) 0x8000
#define N_DATADDR(hdr) (hdr.a_text + 0x8000)

#include "gdbcore.h"

#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>
#include "gdb_stat.h"

#include <errno.h>

void
fetch_inferior_registers (regno)
     int regno;		/* Original value discarded */
{
  register unsigned int regaddr;
  char buf[MAX_REGISTER_RAW_SIZE];
  register int i;

  struct user u;
  unsigned int offset = (char *) &u.u_ar0 - (char *) &u;
  offset = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0)
      - KERNEL_U_ADDR;

  registers_fetched ();
  
  for (regno = 0; regno < 16; regno++)
    {
      regaddr = offset + regno * 4;
      *(int *)&buf[0] = ptrace (PT_READ_U, inferior_pid,
				(PTRACE_ARG3_TYPE) regaddr, 0);
      if (regno == PC_REGNUM)
	  *(int *)&buf[0] = GET_PC_PART(*(int *)&buf[0]);
      supply_register (regno, buf);
    }
  *(int *)&buf[0] = ptrace (PT_READ_U, inferior_pid,
			    (PTRACE_ARG3_TYPE) (offset + PC*4), 0);
  supply_register (PS_REGNUM, buf); /* set virtual register ps same as pc */

  /* read the floating point registers */
  offset = (char *) &u.u_fp_regs - (char *)&u;
  *(int *)buf = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0);
  supply_register (FPS_REGNUM, buf);
  for (regno = 16; regno < 24; regno++) {
      regaddr = offset + 4 + 12 * (regno - 16);
      for (i = 0; i < 12; i += sizeof(int))
	  *(int *) &buf[i] = ptrace (PT_READ_U, inferior_pid,
				     (PTRACE_ARG3_TYPE) (regaddr + i), 0);
      supply_register (regno, buf);
  }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  char buf[80];

  struct user u;
  unsigned long value;
  unsigned int offset = (char *) &u.u_ar0 - (char *) &u;
  offset = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0)
      - KERNEL_U_ADDR;

  if (regno >= 0) {
      if (regno >= 16) return;
      regaddr = offset + 4 * regno;
      errno = 0;
      value = read_register(regno);
      if (regno == PC_REGNUM)
	  value = SET_PC_PART(read_register (PS_REGNUM), value);
      ptrace (PT_WRITE_U, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, value);
      if (errno != 0)
	{
	  sprintf (buf, "writing register number %d", regno);
	  perror_with_name (buf);
	}
    }
  else for (regno = 0; regno < 15; regno++)
    {
      regaddr = offset + regno * 4;
      errno = 0;
      value = read_register(regno);
      if (regno == PC_REGNUM)
	  value = SET_PC_PART(read_register (PS_REGNUM), value);
      ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, value);
      if (errno != 0)
	{
	  sprintf (buf, "writing all regs, number %d", regno);
	  perror_with_name (buf);
	}
    }
}

/* Work with core dump and executable files, for GDB. 
   This code would be in corefile.c if it weren't machine-dependent. */

/* Structure to describe the chain of shared libraries used
   by the execfile.
   e.g. prog shares Xt which shares X11 which shares c. */

struct shared_library {
    struct exec_header header;
    char name[SHLIBLEN];
    CORE_ADDR text_start;	/* CORE_ADDR of 1st byte of text, this file */
    long data_offset;		/* offset of data section in file */
    int chan;			/* file descriptor for the file */
    struct shared_library *shares; /* library this one shares */
};
static struct shared_library *shlib = 0;

/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) ();
   
static CORE_ADDR unshared_text_start;

/* extended header from exec file (for shared library info) */

static struct exec_header exec_header;

void
core_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int val;
  extern char registers[];

  /* Discard all vestiges of any previous core file
     and mark data and stack spaces as empty.  */

  if (corefile)
    free (corefile);
  corefile = 0;

  if (corechan >= 0)
    close (corechan);
  corechan = -1;

  data_start = 0;
  data_end = 0;
  stack_start = STACK_END_ADDR;
  stack_end = STACK_END_ADDR;

  /* Now, if a new core file was specified, open it and digest it.  */

  if (filename)
    {
      filename = tilde_expand (filename);
      make_cleanup (free, filename);
      
      if (have_inferior_p ())
	error ("To look at a core file, you must kill the program with \"kill\".");
      corechan = open (filename, O_RDONLY, 0);
      if (corechan < 0)
	perror_with_name (filename);
      /* 4.2-style (and perhaps also sysV-style) core dump file.  */
      {
	struct user u;

	unsigned int reg_offset, fp_reg_offset;

	val = myread (corechan, &u, sizeof u);
	if (val < 0)
	  perror_with_name ("Not a core file: reading upage");
	if (val != sizeof u)
	  error ("Not a core file: could only read %d bytes", val);

	/* We are depending on exec_file_command having been called
	   previously to set exec_data_start.  Since the executable
	   and the core file share the same text segment, the address
	   of the data segment will be the same in both.  */
	data_start = exec_data_start;

	data_end = data_start + NBPG * u.u_dsize;
	stack_start = stack_end - NBPG * u.u_ssize;
	data_offset = NBPG * UPAGES;
	stack_offset = NBPG * (UPAGES + u.u_dsize);

	/* Some machines put an absolute address in here and some put
	   the offset in the upage of the regs.  */
	reg_offset = (int) u.u_ar0;
	if (reg_offset > NBPG * UPAGES)
	  reg_offset -= KERNEL_U_ADDR;
	fp_reg_offset = (char *) &u.u_fp_regs - (char *)&u;

	/* I don't know where to find this info.
	   So, for now, mark it as not available.  */
	N_SET_MAGIC (core_aouthdr, 0);

	/* Read the register values out of the core file and store
	   them where `read_register' will find them.  */

	{
	  register int regno;

	  for (regno = 0; regno < NUM_REGS; regno++)
	    {
	      char buf[MAX_REGISTER_RAW_SIZE];

	      if (regno < 16)
		  val = lseek (corechan, reg_offset + 4 * regno, 0);
	      else if (regno < 24)
		  val = lseek (corechan, fp_reg_offset + 4 + 12*(regno - 24), 0);
	      else if (regno == 24)
		  val = lseek (corechan, fp_reg_offset, 0);
	      else if (regno == 25)
		  val = lseek (corechan, reg_offset + 4 * PC, 0);
	      if (val < 0
		  || (val = myread (corechan, buf, sizeof buf)) < 0)
		{
		  char * buffer = (char *) alloca (strlen (REGISTER_NAME (regno))
						   + 30);
		  strcpy (buffer, "Reading register ");
		  strcat (buffer, REGISTER_NAME (regno));
						   
		  perror_with_name (buffer);
		}

	      if (regno == PC_REGNUM)
		  *(int *)buf = GET_PC_PART(*(int *)buf);
	      supply_register (regno, buf);
	    }
	}
      }
      if (filename[0] == '/')
	corefile = savestring (filename, strlen (filename));
      else
	{
	  corefile = concat (current_directory, "/", filename, NULL);
	}

      flush_cached_frames ();
      select_frame (get_current_frame (), 0);
      validate_files ();
    }
  else if (from_tty)
    printf ("No core file now.\n");
}

#if 0
/* Work with core dump and executable files, for GDB. 
   This code would be in corefile.c if it weren't machine-dependent. */

/* Structure to describe the chain of shared libraries used
   by the execfile.
   e.g. prog shares Xt which shares X11 which shares c. */

struct shared_library {
    struct exec_header header;
    char name[SHLIBLEN];
    CORE_ADDR text_start;	/* CORE_ADDR of 1st byte of text, this file */
    long data_offset;		/* offset of data section in file */
    int chan;			/* file descriptor for the file */
    struct shared_library *shares; /* library this one shares */
};
static struct shared_library *shlib = 0;

/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) ();
   
static CORE_ADDR unshared_text_start;

/* extended header from exec file (for shared library info) */

static struct exec_header exec_header;

void
exec_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int val;

  /* Eliminate all traces of old exec file.
     Mark text segment as empty.  */

  if (execfile)
    free (execfile);
  execfile = 0;
  data_start = 0;
  data_end -= exec_data_start;
  text_start = 0;
  unshared_text_start = 0;
  text_end = 0;
  exec_data_start = 0;
  exec_data_end = 0;
  if (execchan >= 0)
    close (execchan);
  execchan = -1;
  if (shlib) {
      close_shared_library(shlib);
      shlib = 0;
  }

  /* Now open and digest the file the user requested, if any.  */

  if (filename)
    {
      filename = tilde_expand (filename);
      make_cleanup (free, filename);

      execchan = openp (getenv ("PATH"), 1, filename, O_RDONLY, 0,
			&execfile);
      if (execchan < 0)
	perror_with_name (filename);

      {
	struct stat st_exec;

#ifdef HEADER_SEEK_FD
	HEADER_SEEK_FD (execchan);
#endif
	
	val = myread (execchan, &exec_header, sizeof exec_header);
	exec_aouthdr = exec_header.a_exec;

	if (val < 0)
	  perror_with_name (filename);

	text_start = 0x8000;

	/* Look for shared library if needed */
	if (exec_header.a_exec.a_magic & MF_USES_SL)
	    shlib = open_shared_library(exec_header.a_shlibname, text_start);

	text_offset = N_TXTOFF (exec_aouthdr);
	exec_data_offset = N_TXTOFF (exec_aouthdr) + exec_aouthdr.a_text;

	if (shlib) {
	    unshared_text_start = shared_text_end(shlib) & ~0x7fff;
	    stack_start = shlib->header.a_exec.a_sldatabase;
	    stack_end = STACK_END_ADDR;
	} else
	    unshared_text_start = 0x8000;
	text_end = unshared_text_start + exec_aouthdr.a_text;

	exec_data_start = unshared_text_start + exec_aouthdr.a_text;
        exec_data_end = exec_data_start + exec_aouthdr.a_data;

	data_start = exec_data_start;
	data_end += exec_data_start;

	fstat (execchan, &st_exec);
	exec_mtime = st_exec.st_mtime;
      }

      validate_files ();
    }
  else if (from_tty)
    printf ("No executable file now.\n");

  /* Tell display code (if any) about the changed file name.  */
  if (exec_file_display_hook)
    (*exec_file_display_hook) (filename);
}
#endif

#if 0
/* Read from the program's memory (except for inferior processes).
   This function is misnamed, since it only reads, never writes; and
   since it will use the core file and/or executable file as necessary.

   It should be extended to write as well as read, FIXME, for patching files.

   Return 0 if address could be read, EIO if addresss out of bounds.  */

int
xfer_core_file (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  register int i;
  register int val;
  int xferchan;
  char **xferfile;
  int fileptr;
  int returnval = 0;

  while (len > 0)
    {
      xferfile = 0;
      xferchan = 0;

      /* Determine which file the next bunch of addresses reside in,
	 and where in the file.  Set the file's read/write pointer
	 to point at the proper place for the desired address
	 and set xferfile and xferchan for the correct file.

	 If desired address is nonexistent, leave them zero.

	 i is set to the number of bytes that can be handled
	 along with the next address.

	 We put the most likely tests first for efficiency.  */

      /* Note that if there is no core file
	 data_start and data_end are equal.  */
      if (memaddr >= data_start && memaddr < data_end)
	{
	  i = min (len, data_end - memaddr);
	  fileptr = memaddr - data_start + data_offset;
	  xferfile = &corefile;
	  xferchan = corechan;
	}
      /* Note that if there is no core file
	 stack_start and stack_end define the shared library data.  */
      else if (memaddr >= stack_start && memaddr < stack_end)
	{
	    if (corechan < 0) {
		struct shared_library *lib;
		for (lib = shlib; lib; lib = lib->shares)
		    if (memaddr >= lib->header.a_exec.a_sldatabase &&
			memaddr < lib->header.a_exec.a_sldatabase +
			  lib->header.a_exec.a_data)
			break;
		if (lib) {
		    i = min (len, lib->header.a_exec.a_sldatabase +
			     lib->header.a_exec.a_data - memaddr);
		    fileptr = lib->data_offset + memaddr -
			lib->header.a_exec.a_sldatabase;
		    xferfile = execfile;
		    xferchan = lib->chan;
		}
	    } else {
		i = min (len, stack_end - memaddr);
		fileptr = memaddr - stack_start + stack_offset;
		xferfile = &corefile;
		xferchan = corechan;
	    }
	}
      else if (corechan < 0
	       && memaddr >= exec_data_start && memaddr < exec_data_end)
	{
	  i = min (len, exec_data_end - memaddr);
	  fileptr = memaddr - exec_data_start + exec_data_offset;
	  xferfile = &execfile;
	  xferchan = execchan;
	}
      else if (memaddr >= text_start && memaddr < text_end)
	{
	    struct shared_library *lib;
	    for (lib = shlib; lib; lib = lib->shares)
		if (memaddr >= lib->text_start &&
		    memaddr < lib->text_start + lib->header.a_exec.a_text)
		    break;
	    if (lib) {
		i = min (len, lib->header.a_exec.a_text +
			 lib->text_start - memaddr);
		fileptr = memaddr - lib->text_start + text_offset;
		xferfile = &execfile;
		xferchan = lib->chan;
	    } else {
		i = min (len, text_end - memaddr);
		fileptr = memaddr - unshared_text_start + text_offset;
		xferfile = &execfile;
		xferchan = execchan;
	    }
	}
      else if (memaddr < text_start)
	{
	  i = min (len, text_start - memaddr);
	}
      else if (memaddr >= text_end
	       && memaddr < (corechan >= 0? data_start : exec_data_start))
	{
	  i = min (len, data_start - memaddr);
	}
      else if (corechan >= 0
	       && memaddr >= data_end && memaddr < stack_start)
	{
	  i = min (len, stack_start - memaddr);
	}
      else if (corechan < 0 && memaddr >= exec_data_end)
	{
	  i = min (len, - memaddr);
	}
      else if (memaddr >= stack_end && stack_end != 0)
	{
	  i = min (len, - memaddr);
	}
      else
	{
	  /* Address did not classify into one of the known ranges.
	     This shouldn't happen; we catch the endpoints.  */
	  fatal ("Internal: Bad case logic in xfer_core_file.");
	}

      /* Now we know which file to use.
	 Set up its pointer and transfer the data.  */
      if (xferfile)
	{
	  if (*xferfile == 0)
	    if (xferfile == &execfile)
	      error ("No program file to examine.");
	    else
	      error ("No core dump file or running program to examine.");
	  val = lseek (xferchan, fileptr, 0);
	  if (val < 0)
	    perror_with_name (*xferfile);
	  val = myread (xferchan, myaddr, i);
	  if (val < 0)
	    perror_with_name (*xferfile);
	}
      /* If this address is for nonexistent memory,
	 read zeros if reading, or do nothing if writing.
	 Actually, we never right.  */
      else
	{
	  memset (myaddr, '\0', i);
	  returnval = EIO;
	}

      memaddr += i;
      myaddr += i;
      len -= i;
    }
  return returnval;
}
#endif
