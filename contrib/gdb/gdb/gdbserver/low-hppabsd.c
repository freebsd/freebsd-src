/* Low level interface to ptrace, for the remote server for GDB.
   Copyright 1995, 1996, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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

#include "server.h"
#include <sys/wait.h>
#include "frame.h"
#include "inferior.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include <fcntl.h>

/***************Begin MY defs*********************/
static char my_registers[REGISTER_BYTES];
char *registers = my_registers;
/***************End MY defs*********************/

#include <sys/ptrace.h>
#include <machine/reg.h>

extern int errno;

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args. */

int
create_inferior (char *program, char **allargs)
{
  int pid;

  pid = fork ();
  if (pid < 0)
    perror_with_name ("fork");

  if (pid == 0)
    {
      ptrace (PT_TRACE_ME, 0, 0, 0, 0);

      execv (program, allargs);

      fprintf (stderr, "Cannot exec %s: %s.\n", program,
	       errno < sys_nerr ? sys_errlist[errno] : "unknown error");
      fflush (stderr);
      _exit (0177);
    }

  return pid;
}

/* Kill the inferior process.  Make us have no inferior.  */

void
kill_inferior (void)
{
  if (inferior_pid == 0)
    return;
  ptrace (8, inferior_pid, 0, 0, 0);
  wait (0);
/*************inferior_died ();****VK**************/
}

/* Attaching is not supported.  */
int
myattach (int pid)
{
  return -1;
}

/* Return nonzero if the given thread is still alive.  */
int
mythread_alive (int pid)
{
  return 1;
}

/* Wait for process, returns status */

unsigned char
mywait (char *status)
{
  int pid;
  union wait w;

  enable_async_io ();
  pid = waitpid (inferior_pid, &w, 0);
  disable_async_io ();
  if (pid != inferior_pid)
    perror_with_name ("wait");

  if (WIFEXITED (w))
    {
      fprintf (stderr, "\nChild exited with retcode = %x \n", WEXITSTATUS (w));
      *status = 'W';
      return ((unsigned char) WEXITSTATUS (w));
    }
  else if (!WIFSTOPPED (w))
    {
      fprintf (stderr, "\nChild terminated with signal = %x \n", WTERMSIG (w));
      *status = 'X';
      return ((unsigned char) WTERMSIG (w));
    }

  fetch_inferior_registers (0);

  *status = 'T';
  return ((unsigned char) WSTOPSIG (w));
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
myresume (int step, int signal)
{
  errno = 0;
  ptrace (step ? PT_STEP : PT_CONTINUE, inferior_pid, 1, signal, 0);
  if (errno)
    perror_with_name ("ptrace");
}


#if !defined (offsetof)
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

/* U_REGS_OFFSET is the offset of the registers within the u area.  */
#if !defined (U_REGS_OFFSET)
#define U_REGS_OFFSET \
  ptrace (PT_READ_U, inferior_pid, \
          (PTRACE_ARG3_TYPE) (offsetof (struct user, u_ar0)), 0) \
    - KERNEL_U_ADDR
#endif

CORE_ADDR
register_addr (int regno, CORE_ADDR blockend)
{
  CORE_ADDR addr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Invalid register number %d.", regno);

  REGISTER_U_ADDR (addr, blockend, regno);

  return addr;
}

/* Fetch one register.  */

static void
fetch_register (int regno)
{
  register unsigned int regaddr;
  char buf[MAX_REGISTER_RAW_SIZE];
  register int i;

  /* Offset of registers within the u area.  */
  unsigned int offset;

  offset = U_REGS_OFFSET;

  regaddr = register_addr (regno, offset);
  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
    {
      errno = 0;
      *(int *) &registers[regno * 4 + i] = ptrace (PT_RUREGS, inferior_pid,
					  (PTRACE_ARG3_TYPE) regaddr, 0, 0);
      regaddr += sizeof (int);
      if (errno != 0)
	{
	  /* Warning, not error, in case we are attached; sometimes the
	     kernel doesn't let us at the registers.  */
	  char *err = strerror (errno);
	  char *msg = alloca (strlen (err) + 128);
	  sprintf (msg, "reading register %d: %s", regno, err);
	  error (msg);
	  goto error_exit;
	}
    }
error_exit:;
}

/* Fetch all registers, or just one, from the child process.  */

void
fetch_inferior_registers (int regno)
{
  if (regno == -1 || regno == 0)
    for (regno = 0; regno < NUM_REGS; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (int regno)
{
  register unsigned int regaddr;
  char buf[80];
  extern char registers[];
  register int i;
  unsigned int offset = U_REGS_OFFSET;
  int scratch;

  if (regno >= 0)
    {
      if (CANNOT_STORE_REGISTER (regno))
	return;
      regaddr = register_addr (regno, offset);
      errno = 0;
      if (regno == PCOQ_HEAD_REGNUM || regno == PCOQ_TAIL_REGNUM)
	{
	  scratch = *(int *) &registers[REGISTER_BYTE (regno)] | 0x3;
	  ptrace (PT_WUREGS, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		  scratch, 0);
	  if (errno != 0)
	    {
	      /* Error, even if attached.  Failing to write these two
	         registers is pretty serious.  */
	      sprintf (buf, "writing register number %d", regno);
	      perror_with_name (buf);
	    }
	}
      else
	for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
	  {
	    errno = 0;
	    ptrace (PT_WUREGS, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		    *(int *) &registers[REGISTER_BYTE (regno) + i], 0);
	    if (errno != 0)
	      {
		/* Warning, not error, in case we are attached; sometimes the
		   kernel doesn't let us at the registers.  */
		char *err = strerror (errno);
		char *msg = alloca (strlen (err) + 128);
		sprintf (msg, "writing register %d: %s",
			 regno, err);
		error (msg);
		return;
	      }
	    regaddr += sizeof (int);
	  }
    }
  else
    for (regno = 0; regno < NUM_REGS; regno++)
      store_inferior_registers (regno);
}

/* NOTE! I tried using PTRACE_READDATA, etc., to read and write memory
   in the NEW_SUN_PTRACE case.
   It ought to be straightforward.  But it appears that writing did
   not write the data that I specified.  I cannot understand where
   it got the data that it actually did write.  */

/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

void
read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (int);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (int) - 1) / sizeof (int);
  /* Allocate buffer of that many longwords.  */
  register int *buffer = (int *) alloca (count * sizeof (int));

  /* Read all the longwords */
  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      buffer[i] = ptrace (1, inferior_pid, addr, 0, 0);
    }

  /* Copy appropriate bytes out of the buffer.  */
  memcpy (myaddr, (char *) buffer + (memaddr & (sizeof (int) - 1)), len);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.
   On failure (cannot write the inferior)
   returns the value of errno.  */

int
write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (int);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (int) - 1) / sizeof (int);
  /* Allocate buffer of that many longwords.  */
  register int *buffer = (int *) alloca (count * sizeof (int));
  extern int errno;

  /* Fill start and end extra bytes of buffer with existing memory data.  */

  buffer[0] = ptrace (1, inferior_pid, addr, 0, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (1, inferior_pid,
		  addr + (count - 1) * sizeof (int), 0, 0);
    }

  /* Copy data to be written over corresponding part of buffer */

  memcpy ((char *) buffer + (memaddr & (sizeof (int) - 1)), myaddr, len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      errno = 0;
      ptrace (4, inferior_pid, addr, buffer[i], 0);
      if (errno)
	return errno;
    }

  return 0;
}

void
initialize_low (void)
{
}
