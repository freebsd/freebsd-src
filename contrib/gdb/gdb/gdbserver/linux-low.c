/* Low level interface to ptrace, for the remote server for GDB.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "server.h"
#include "linux-low.h"

#include <sys/wait.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define PTRACE_ARG3_TYPE long
#define PTRACE_XFER_TYPE long

#ifdef HAVE_LINUX_REGSETS
static int use_regsets_p = 1;
#endif

extern int errno;

#ifdef HAVE_LINUX_USRREGS
extern int num_regs;
extern int regmap[];
#endif

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
      ptrace (PTRACE_TRACEME, 0, 0, 0);

      execv (program, allargs);

      fprintf (stderr, "Cannot exec %s: %s.\n", program,
	       strerror (errno));
      fflush (stderr);
      _exit (0177);
    }

  return pid;
}

/* Attach to an inferior process.  */

int
myattach (int pid)
{
  if (ptrace (PTRACE_ATTACH, pid, 0, 0) != 0)
    {
      fprintf (stderr, "Cannot attach to process %d: %s (%d)\n", pid,
	       errno < sys_nerr ? sys_errlist[errno] : "unknown error",
	       errno);
      fflush (stderr);
      _exit (0177);
    }

  return 0;
}

/* Kill the inferior process.  Make us have no inferior.  */

void
kill_inferior (void)
{
  if (inferior_pid == 0)
    return;
  ptrace (PTRACE_KILL, inferior_pid, 0, 0);
  wait (0);
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
  int w;

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
  ptrace (step ? PTRACE_SINGLESTEP : PTRACE_CONT, inferior_pid, 1, signal);
  if (errno)
    perror_with_name ("ptrace");
}


#ifdef HAVE_LINUX_USRREGS

#define REGISTER_RAW_SIZE(regno) register_size((regno))

int
register_addr (int regnum)
{
  int addr;

  if (regnum < 0 || regnum >= num_regs)
    error ("Invalid register number %d.", regnum);

  addr = regmap[regnum];
  if (addr == -1)
    addr = 0;

  return addr;
}

/* Fetch one register.  */
static void
fetch_register (int regno)
{
  CORE_ADDR regaddr;
  register int i;

  if (regno >= num_regs)
    return;
  if (cannot_fetch_register (regno))
    return;

  regaddr = register_addr (regno);
  if (regaddr == -1)
    return;
  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) (register_data (regno) + i) =
	ptrace (PTRACE_PEEKUSER, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
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
static void
usr_fetch_inferior_registers (int regno)
{
  if (regno == -1 || regno == 0)
    for (regno = 0; regno < num_regs; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */
static void
usr_store_inferior_registers (int regno)
{
  CORE_ADDR regaddr;
  int i;

  if (regno >= 0)
    {
      if (regno >= num_regs)
	return;

      if (cannot_store_register (regno))
	return;

      regaddr = register_addr (regno);
      if (regaddr == -1)
	return;
      errno = 0;
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  ptrace (PTRACE_POKEUSER, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		  *(int *) (register_data (regno) + i));
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
    for (regno = 0; regno < num_regs; regno++)
      store_inferior_registers (regno);
}
#endif /* HAVE_LINUX_USRREGS */



#ifdef HAVE_LINUX_REGSETS

static int
regsets_fetch_inferior_registers (void)
{
  struct regset_info *regset;

  regset = target_regsets;

  while (regset->size >= 0)
    {
      void *buf;
      int res;

      if (regset->size == 0)
	{
	  regset ++;
	  continue;
	}

      buf = malloc (regset->size);
      res = ptrace (regset->get_request, inferior_pid, 0, (int) buf);
      if (res < 0)
	{
	  if (errno == EIO)
	    {
	      /* If we get EIO on the first regset, do not try regsets again.
		 If we get EIO on a later regset, disable that regset.  */
	      if (regset == target_regsets)
		{
		  use_regsets_p = 0;
		  return -1;
		}
	      else
		{
		  regset->size = 0;
		  continue;
		}
	    }
	  else
	    {
	      perror ("Warning: ptrace(regsets_fetch_inferior_registers)");
	    }
	}
      regset->store_function (buf);
      regset ++;
    }
}

static int
regsets_store_inferior_registers (void)
{
  struct regset_info *regset;

  regset = target_regsets;

  while (regset->size >= 0)
    {
      void *buf;
      int res;

      if (regset->size == 0)
	{
	  regset ++;
	  continue;
	}

      buf = malloc (regset->size);
      regset->fill_function (buf);
      res = ptrace (regset->set_request, inferior_pid, 0, (int) buf);
      if (res < 0)
	{
	  if (errno == EIO)
	    {
	      /* If we get EIO on the first regset, do not try regsets again.
		 If we get EIO on a later regset, disable that regset.  */
	      if (regset == target_regsets)
		{
		  use_regsets_p = 0;
		  return -1;
		}
	      else
		{
		  regset->size = 0;
		  continue;
		}
	    }
	  else
	    {
	      perror ("Warning: ptrace(regsets_fetch_inferior_registers)");
	    }
	}
      regset ++;
    }
}

#endif /* HAVE_LINUX_REGSETS */


void
fetch_inferior_registers (int regno)
{
#ifdef HAVE_LINUX_REGSETS
  if (use_regsets_p)
    {
      if (regsets_fetch_inferior_registers () == 0)
	return;
    }
#endif
#ifdef HAVE_LINUX_USRREGS
  usr_fetch_inferior_registers (regno);
#endif
}

void
store_inferior_registers (int regno)
{
#ifdef HAVE_LINUX_REGSETS
  if (use_regsets_p)
    {
      if (regsets_store_inferior_registers () == 0)
	return;
    }
#endif
#ifdef HAVE_LINUX_USRREGS
  usr_store_inferior_registers (regno);
#endif
}


/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

void
read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count 
    = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1) 
      / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer 
    = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));

  /* Read all the longwords */
  for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
    {
      buffer[i] = ptrace (PTRACE_PEEKTEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr, 0);
    }

  /* Copy appropriate bytes out of the buffer.  */
  memcpy (myaddr, (char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)), len);
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
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1) / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));
  extern int errno;

  /* Fill start and end extra bytes of buffer with existing memory data.  */

  buffer[0] = ptrace (PTRACE_PEEKTEXT, inferior_pid,
		      (PTRACE_ARG3_TYPE) addr, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (PTRACE_PEEKTEXT, inferior_pid,
		  (PTRACE_ARG3_TYPE) (addr + (count - 1)
				      * sizeof (PTRACE_XFER_TYPE)),
		  0);
    }

  /* Copy data to be written over corresponding part of buffer */

  memcpy ((char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)), myaddr, len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      ptrace (PTRACE_POKETEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr, buffer[i]);
      if (errno)
	return errno;
    }

  return 0;
}

void
initialize_low (void)
{
  init_registers ();
}
