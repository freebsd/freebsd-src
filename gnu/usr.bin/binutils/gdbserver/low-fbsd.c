/* Low level interface to ptrace, for the remote server for GDB.
   Copyright (C) 1995 Free Software Foundation, Inc.

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
#include <sys/wait.h>
#include "frame.h"
#include "inferior.h"

#include <stdio.h>
#include <sys/param.h>
#include <dirent.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include <fcntl.h>
#include <string.h>

/***************Begin MY defs*********************/
int quit_flag = 0;
char registers[REGISTER_BYTES];

/* Index within `registers' of the first byte of the space for
   register N.  */


char buf2[MAX_REGISTER_RAW_SIZE];
/***************End MY defs*********************/

#include <sys/ptrace.h>
#include <machine/reg.h>

extern char **environ;
extern int inferior_pid;
void quit (), perror_with_name ();
int query ();

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args.
   ENV is the environment vector to pass.  */

int
create_inferior (program, allargs)
     char *program;
     char **allargs;
{
  int pid;

  pid = fork ();
  if (pid < 0)
    perror_with_name ("fork");

  if (pid == 0)
    {
      ptrace (PT_TRACE_ME, 0, 0, 0);

      execv (program, allargs);

      fprintf (stderr, "Cannot exec %s: %s.\n", program, strerror(errno));
      fflush (stderr);
      _exit (0177);
    }

  return pid;
}

/* Kill the inferior process.  Make us have no inferior.  */

void
kill_inferior ()
{
  if (inferior_pid == 0)
    return;
  ptrace (PT_KILL, inferior_pid, 0, 0);
  wait (0);
  /*************inferior_died ();****VK**************/
}

/* Return nonzero if the given thread is still alive.  */
int
mythread_alive (pid)
     int pid;
{
  return 1;
}

/* Wait for process, returns status */

unsigned char
mywait (status)
     char *status;
{
  int pid;
  int w;

  pid = wait (&w);
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
myresume (step, signal)
     int step;
     int signal;
{
  errno = 0;
  ptrace (step ? PT_STEP : PT_CONTINUE, inferior_pid,
	  (PTRACE_ARG3_TYPE) 1, signal);
  if (errno)
    perror_with_name ("ptrace");
}

#if defined(__i386__)

/* this table must line up with REGISTER_NAMES in tm-i386v.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] =
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tFS, tGS,
};

static struct save87 pcb_savefpu;

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;	/* ptrace order, not gcc/gdb order */
  int r;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  for (r = 0; r < NUM_REGS; r++)
    memcpy (&registers[REGISTER_BYTE (r)], ((int *)&inferior_registers) + tregmap[r], 4);
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;	/* ptrace order, not gcc/gdb order */
  int r;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  for (r = 0; r < NUM_REGS; r++)
    memcpy (((int *)&inferior_registers) + tregmap[r], &registers[REGISTER_BYTE (r)], 4);

  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
}

#elif defined(__alpha__)

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg regs;	/* ptrace order, not gcc/gdb order */
  struct fpreg fpregs;
  int r;

  ptrace (PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
  ptrace (PT_GETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE) &fpregs, 0);

  for (r = 0; r < 31; r++)
    memcpy (&registers[REGISTER_BYTE (r)],
	    &regs.r_regs[r], sizeof(u_int64_t));
  for (r = 0; r < 32; r++)
    memcpy (&registers[REGISTER_BYTE (r + FP0_REGNUM)],
	    &fpregs.fpr_regs[r], sizeof(u_int64_t));
  memcpy (&registers[REGISTER_BYTE (PC_REGNUM)],
	  &regs.r_regs[31], sizeof(u_int64_t));

  memset (&registers[REGISTER_BYTE (ZERO_REGNUM)], 0, sizeof(u_int64_t));
  memset (&registers[REGISTER_BYTE (FP_REGNUM)], 0, sizeof(u_int64_t));
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg regs;	/* ptrace order, not gcc/gdb order */
  struct fpreg fpregs;
  int r;

  for (r = 0; r < 31; r++)
    memcpy (&regs.r_regs[r],
	    &registers[REGISTER_BYTE (r)], sizeof(u_int64_t));
  for (r = 0; r < 32; r++)
    memcpy (&fpregs.fpr_regs[r],
	    &registers[REGISTER_BYTE (r + FP0_REGNUM)], sizeof(u_int64_t));
  memcpy (&regs.r_regs[31],
	  &registers[REGISTER_BYTE (PC_REGNUM)], sizeof(u_int64_t));

  ptrace (PT_SETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
  ptrace (PT_SETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE) &fpregs, 0);
}

#endif


/* NOTE! I tried using PTRACE_READDATA, etc., to read and write memory
   in the NEW_SUN_PTRACE case.
   It ought to be straightforward.  But it appears that writing did
   not write the data that I specified.  I cannot understand where
   it got the data that it actually did write.  */

/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -sizeof (int);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (int) - 1) / sizeof (int);
  /* Allocate buffer of that many longwords.  */
  register int *buffer = (int *) alloca (count * sizeof (int));

  /* Read all the longwords */
  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      buffer[i] = ptrace (PT_READ_I, inferior_pid, (PTRACE_ARG3_TYPE) addr, 0);
    }

  /* Copy appropriate bytes out of the buffer.  */
  memcpy (myaddr, (char *) buffer + (memaddr & (sizeof (int) - 1)), len);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.
   On failure (cannot write the inferior)
   returns the value of errno.  */

int
write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -sizeof (int);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (int) - 1) / sizeof (int);
  /* Allocate buffer of that many longwords.  */
  register int *buffer = (int *) alloca (count * sizeof (int));
  extern int errno;

  /* Fill start and end extra bytes of buffer with existing memory data.  */

  buffer[0] = ptrace (PT_READ_I, inferior_pid,
		      (PTRACE_ARG3_TYPE) addr, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (PT_READ_I, inferior_pid,
	      	  (PTRACE_ARG3_TYPE) addr + (count - 1) * sizeof (int), 0);
    }

  /* Copy data to be written over corresponding part of buffer */

  memcpy ((char *) buffer + (memaddr & (sizeof (int) - 1)), myaddr, len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      errno = 0;
      ptrace (PT_WRITE_I, inferior_pid, (PTRACE_ARG3_TYPE) addr, buffer[i]);
      if (errno)
	return errno;
    }

  return 0;
}

void
initialize ()
{
  inferior_pid = 0;
}

int
have_inferior_p ()
{
  return inferior_pid != 0;
}
