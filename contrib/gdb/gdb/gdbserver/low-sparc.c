/* Low level interface to ptrace, for the remote server for GDB.
   Copyright (C) 1986, 1987, 1993 Free Software Foundation, Inc.

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
/***************************
#include "initialize.h"
****************************/

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include <fcntl.h>

/***************Begin MY defs*********************/
int quit_flag = 0;
char registers[REGISTER_BYTES];

/* Index within `registers' of the first byte of the space for
   register N.  */


char buf2[MAX_REGISTER_RAW_SIZE];
/***************End MY defs*********************/

#include <sys/ptrace.h>
#include <sys/reg.h>

extern int sys_nerr;
extern char **sys_errlist;
extern char **environ;
extern int errno;
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
      ptrace (PTRACE_TRACEME);

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
kill_inferior ()
{
  if (inferior_pid == 0)
    return;
  ptrace (8, inferior_pid, 0, 0);
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
  union wait w;

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
  ptrace (step ? PTRACE_SINGLESTEP : PTRACE_CONT, inferior_pid, 1, signal);
  if (errno)
    perror_with_name ("ptrace");
}

/* Fetch one or more registers from the inferior.  REGNO == -1 to get
   them all.  We actually fetch more than requested, when convenient,
   marking them as valid so we won't fetch them again.  */

void
fetch_inferior_registers (ignored)
     int ignored;
{
  struct regs inferior_registers;
  struct fp_status inferior_fp_registers;
  int i;

  /* Global and Out regs are fetched directly, as well as the control
     registers.  If we're getting one of the in or local regs,
     and the stack pointer has not yet been fetched,
     we have to do that first, since they're found in memory relative
     to the stack pointer.  */

  if (ptrace (PTRACE_GETREGS, inferior_pid,
	      (PTRACE_ARG3_TYPE) &inferior_registers, 0))
    perror("ptrace_getregs");
      
  registers[REGISTER_BYTE (0)] = 0;
  memcpy (&registers[REGISTER_BYTE (1)], &inferior_registers.r_g1,
	  15 * REGISTER_RAW_SIZE (G0_REGNUM));
  *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = inferior_registers.r_ps; 
  *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = inferior_registers.r_pc;
  *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] = inferior_registers.r_npc;
  *(int *)&registers[REGISTER_BYTE (Y_REGNUM)] = inferior_registers.r_y;

  /* Floating point registers */

  if (ptrace (PTRACE_GETFPREGS, inferior_pid,
	      (PTRACE_ARG3_TYPE) &inferior_fp_registers,
	      0))
    perror("ptrace_getfpregs");
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], &inferior_fp_registers,
	  sizeof inferior_fp_registers.fpu_fr);

  /* These regs are saved on the stack by the kernel.  Only read them
     all (16 ptrace calls!) if we really need them.  */

  read_inferior_memory (*(CORE_ADDR*)&registers[REGISTER_BYTE (SP_REGNUM)],
			&registers[REGISTER_BYTE (L0_REGNUM)],
			16*REGISTER_RAW_SIZE (L0_REGNUM));
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (ignored)
     int ignored;
{
  struct regs inferior_registers;
  struct fp_status inferior_fp_registers;
  CORE_ADDR sp = *(CORE_ADDR *)&registers[REGISTER_BYTE (SP_REGNUM)];

  write_inferior_memory (sp, &registers[REGISTER_BYTE (L0_REGNUM)],
			 16*REGISTER_RAW_SIZE (L0_REGNUM));

  memcpy (&inferior_registers.r_g1, &registers[REGISTER_BYTE (G1_REGNUM)],
	  15 * REGISTER_RAW_SIZE (G1_REGNUM));

  inferior_registers.r_ps =
    *(int *)&registers[REGISTER_BYTE (PS_REGNUM)];
  inferior_registers.r_pc =
    *(int *)&registers[REGISTER_BYTE (PC_REGNUM)];
  inferior_registers.r_npc =
    *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)];
  inferior_registers.r_y =
    *(int *)&registers[REGISTER_BYTE (Y_REGNUM)];

  if (ptrace (PTRACE_SETREGS, inferior_pid,
	      (PTRACE_ARG3_TYPE) &inferior_registers, 0))
    perror("ptrace_setregs");

  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	  sizeof inferior_fp_registers.fpu_fr);

  if (ptrace (PTRACE_SETFPREGS, inferior_pid,
	      (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0))
    perror("ptrace_setfpregs");
}

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
      buffer[i] = ptrace (1, inferior_pid, addr, 0);
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

  buffer[0] = ptrace (1, inferior_pid, addr, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (1, inferior_pid,
		  addr + (count - 1) * sizeof (int), 0);
    }

  /* Copy data to be written over corresponding part of buffer */

  bcopy (myaddr, (char *) buffer + (memaddr & (sizeof (int) - 1)), len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      errno = 0;
      ptrace (4, inferior_pid, addr, buffer[i]);
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
