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

#include "server.h"
#include "frame.h"
#include "inferior.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#define LYNXOS
#include <sys/mem.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/kernel.h>
#ifndef __LYNXOS
#define __LYNXOS
#endif
#include <sys/itimer.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/fpp.h>

char registers[REGISTER_BYTES];

#include <sys/ptrace.h>

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args. */

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
      int pgrp;

      /* Switch child to it's own process group so that signals won't
	 directly affect gdbserver. */

      pgrp = getpid();
      setpgrp(0, pgrp);
      ioctl (0, TIOCSPGRP, &pgrp);

      ptrace (PTRACE_TRACEME, 0, (PTRACE_ARG3_TYPE)0, 0);

      execv (program, allargs);

      fprintf (stderr, "GDBserver (process %d):  Cannot exec %s: %s.\n",
	       getpid(), program,
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
  ptrace (PTRACE_KILL, inferior_pid, 0, 0);
  wait (0);

  inferior_pid = 0;
}

/* Return nonzero if the given thread is still alive.  */
int
mythread_alive (pid)
     int pid;
{
  /* Arggh.  Apparently pthread_kill only works for threads within
     the process that calls pthread_kill.

     We want to avoid the lynx signal extensions as they simply don't
     map well to the generic gdb interface we want to keep.

     All we want to do is determine if a particular thread is alive;
     it appears as if we can just make a harmless thread specific
     ptrace call to do that.  */
  return (ptrace (PTRACE_THREADUSER,
		  BUILDPID (PIDGET (inferior_pid), pid), 0, 0) != -1);
}

/* Wait for process, returns status */

unsigned char
mywait (status)
     char *status;
{
  int pid;
  union wait w;

  while (1)
    {
      enable_async_io();

      pid = wait (&w);

      disable_async_io();

      if (pid != PIDGET(inferior_pid))
	perror_with_name ("wait");

      thread_from_wait = w.w_tid;
      inferior_pid = BUILDPID (inferior_pid, w.w_tid);

      if (WIFSTOPPED(w)
	  && WSTOPSIG(w) == SIGTRAP)
	{
	  int realsig;

	  realsig = ptrace (PTRACE_GETTRACESIG, inferior_pid,
			    (PTRACE_ARG3_TYPE)0, 0);

	  if (realsig == SIGNEWTHREAD)
	    {
	      /* It's a new thread notification.  Nothing to do here since
		 the machine independent code in wait_for_inferior will
		 add the thread to the thread list and restart the thread
		 when pid != inferior_pid and pid is not in the thread list.
		 We don't even want to muck with realsig -- the code in
		 wait_for_inferior expects SIGTRAP.  */
	      ;
	    }
	}
      break;
    }

  if (WIFEXITED (w))
    {
      *status = 'W';
      return ((unsigned char) WEXITSTATUS (w));
    }
  else if (!WIFSTOPPED (w))
    {
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
  ptrace (step ? PTRACE_SINGLESTEP_ONE : PTRACE_CONT,
	  BUILDPID (inferior_pid, cont_thread == -1 ? 0 : cont_thread),
	  1, signal);
  if (errno)
    perror_with_name ("ptrace");
}

#undef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)

/* Mapping between GDB register #s and offsets into econtext.  Must be
   consistent with REGISTER_NAMES macro in various tmXXX.h files. */

#define X(ENTRY)(offsetof(struct econtext, ENTRY))

#ifdef I386
/* Mappings from tm-i386v.h */

static int regmap[] =
{
  X(eax),
  X(ecx),
  X(edx),
  X(ebx),
  X(esp),			/* sp */
  X(ebp),			/* fp */
  X(esi),
  X(edi),
  X(eip),			/* pc */
  X(flags),			/* ps */
  X(cs),
  X(ss),
  X(ds),
  X(es),
  X(ecode),			/* Lynx doesn't give us either fs or gs, so */
  X(fault),			/* we just substitute these two in the hopes
				   that they are useful. */
};
#endif

#ifdef M68K
/* Mappings from tm-m68k.h */

static int regmap[] =
{
  X(regs[0]),			/* d0 */
  X(regs[1]),			/* d1 */
  X(regs[2]),			/* d2 */
  X(regs[3]),			/* d3 */
  X(regs[4]),			/* d4 */
  X(regs[5]),			/* d5 */
  X(regs[6]),			/* d6 */
  X(regs[7]),			/* d7 */
  X(regs[8]),			/* a0 */
  X(regs[9]),			/* a1 */
  X(regs[10]),			/* a2 */
  X(regs[11]),			/* a3 */
  X(regs[12]),			/* a4 */
  X(regs[13]),			/* a5 */
  X(regs[14]),			/* fp */
  0,				/* sp */
  X(status),			/* ps */
  X(pc),

  X(fregs[0*3]),		/* fp0 */
  X(fregs[1*3]),		/* fp1 */
  X(fregs[2*3]),		/* fp2 */
  X(fregs[3*3]),		/* fp3 */
  X(fregs[4*3]),		/* fp4 */
  X(fregs[5*3]),		/* fp5 */
  X(fregs[6*3]),		/* fp6 */
  X(fregs[7*3]),		/* fp7 */

  X(fcregs[0]),			/* fpcontrol */
  X(fcregs[1]),			/* fpstatus */
  X(fcregs[2]),			/* fpiaddr */
  X(ssw),			/* fpcode */
  X(fault),			/* fpflags */
};
#endif

#ifdef SPARC
/* Mappings from tm-sparc.h */

#define FX(ENTRY)(offsetof(struct fcontext, ENTRY))

static int regmap[] =
{
  -1,				/* g0 */
  X(g1),
  X(g2),
  X(g3),
  X(g4),
  -1,				/* g5->g7 aren't saved by Lynx */
  -1,
  -1,

  X(o[0]),
  X(o[1]),
  X(o[2]),
  X(o[3]),
  X(o[4]),
  X(o[5]),
  X(o[6]),			/* sp */
  X(o[7]),			/* ra */

  -1,-1,-1,-1,-1,-1,-1,-1,	/* l0 -> l7 */

  -1,-1,-1,-1,-1,-1,-1,-1,	/* i0 -> i7 */

  FX(f.fregs[0]),		/* f0 */
  FX(f.fregs[1]),
  FX(f.fregs[2]),
  FX(f.fregs[3]),
  FX(f.fregs[4]),
  FX(f.fregs[5]),
  FX(f.fregs[6]),
  FX(f.fregs[7]),
  FX(f.fregs[8]),
  FX(f.fregs[9]),
  FX(f.fregs[10]),
  FX(f.fregs[11]),
  FX(f.fregs[12]),
  FX(f.fregs[13]),
  FX(f.fregs[14]),
  FX(f.fregs[15]),
  FX(f.fregs[16]),
  FX(f.fregs[17]),
  FX(f.fregs[18]),
  FX(f.fregs[19]),
  FX(f.fregs[20]),
  FX(f.fregs[21]),
  FX(f.fregs[22]),
  FX(f.fregs[23]),
  FX(f.fregs[24]),
  FX(f.fregs[25]),
  FX(f.fregs[26]),
  FX(f.fregs[27]),
  FX(f.fregs[28]),
  FX(f.fregs[29]),
  FX(f.fregs[30]),
  FX(f.fregs[31]),

  X(y),
  X(psr),
  X(wim),
  X(tbr),
  X(pc),
  X(npc),
  FX(fsr),			/* fpsr */
  -1,				/* cpsr */
};
#endif

#ifdef SPARC

/* This routine handles some oddball cases for Sparc registers and LynxOS.
   In partucular, it causes refs to G0, g5->7, and all fp regs to return zero.
   It also handles knows where to find the I & L regs on the stack.  */

void
fetch_inferior_registers (regno)
     int regno;
{
#if 0
  int whatregs = 0;

#define WHATREGS_FLOAT 1
#define WHATREGS_GEN 2
#define WHATREGS_STACK 4

  if (regno == -1)
    whatregs = WHATREGS_FLOAT | WHATREGS_GEN | WHATREGS_STACK;
  else if (regno >= L0_REGNUM && regno <= I7_REGNUM)
    whatregs = WHATREGS_STACK;
  else if (regno >= FP0_REGNUM && regno < FP0_REGNUM + 32)
    whatregs = WHATREGS_FLOAT;
  else
    whatregs = WHATREGS_GEN;

  if (whatregs & WHATREGS_GEN)
    {
      struct econtext ec;		/* general regs */
      char buf[MAX_REGISTER_RAW_SIZE];
      int retval;
      int i;

      errno = 0;
      retval = ptrace (PTRACE_GETREGS,
		       BUILDPID (inferior_pid, general_thread),
		       (PTRACE_ARG3_TYPE) &ec,
		       0);
      if (errno)
	perror_with_name ("Sparc fetch_inferior_registers(ptrace)");
  
      memset (buf, 0, REGISTER_RAW_SIZE (G0_REGNUM));
      supply_register (G0_REGNUM, buf);
      supply_register (TBR_REGNUM, (char *)&ec.tbr);

      memcpy (&registers[REGISTER_BYTE (G1_REGNUM)], &ec.g1,
	      4 * REGISTER_RAW_SIZE (G1_REGNUM));
      for (i = G1_REGNUM; i <= G1_REGNUM + 3; i++)
	register_valid[i] = 1;

      supply_register (PS_REGNUM, (char *)&ec.psr);
      supply_register (Y_REGNUM, (char *)&ec.y);
      supply_register (PC_REGNUM, (char *)&ec.pc);
      supply_register (NPC_REGNUM, (char *)&ec.npc);
      supply_register (WIM_REGNUM, (char *)&ec.wim);

      memcpy (&registers[REGISTER_BYTE (O0_REGNUM)], ec.o,
	      8 * REGISTER_RAW_SIZE (O0_REGNUM));
      for (i = O0_REGNUM; i <= O0_REGNUM + 7; i++)
	register_valid[i] = 1;
    }

  if (whatregs & WHATREGS_STACK)
    {
      CORE_ADDR sp;
      int i;

      sp = read_register (SP_REGNUM);

      target_xfer_memory (sp + FRAME_SAVED_I0,
			  &registers[REGISTER_BYTE(I0_REGNUM)],
			  8 * REGISTER_RAW_SIZE (I0_REGNUM), 0);
      for (i = I0_REGNUM; i <= I7_REGNUM; i++)
	register_valid[i] = 1;

      target_xfer_memory (sp + FRAME_SAVED_L0,
			  &registers[REGISTER_BYTE(L0_REGNUM)],
			  8 * REGISTER_RAW_SIZE (L0_REGNUM), 0);
      for (i = L0_REGNUM; i <= L0_REGNUM + 7; i++)
	register_valid[i] = 1;
    }

  if (whatregs & WHATREGS_FLOAT)
    {
      struct fcontext fc;		/* fp regs */
      int retval;
      int i;

      errno = 0;
      retval = ptrace (PTRACE_GETFPREGS, BUILDPID (inferior_pid, general_thread), (PTRACE_ARG3_TYPE) &fc,
		       0);
      if (errno)
	perror_with_name ("Sparc fetch_inferior_registers(ptrace)");
  
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], fc.f.fregs,
	      32 * REGISTER_RAW_SIZE (FP0_REGNUM));
      for (i = FP0_REGNUM; i <= FP0_REGNUM + 31; i++)
	register_valid[i] = 1;

      supply_register (FPS_REGNUM, (char *)&fc.fsr);
    }
#endif
}

/* This routine handles storing of the I & L regs for the Sparc.  The trick
   here is that they actually live on the stack.  The really tricky part is
   that when changing the stack pointer, the I & L regs must be written to
   where the new SP points, otherwise the regs will be incorrect when the
   process is started up again.   We assume that the I & L regs are valid at
   this point.  */

void
store_inferior_registers (regno)
     int regno;
{
#if 0
  int whatregs = 0;

  if (regno == -1)
    whatregs = WHATREGS_FLOAT | WHATREGS_GEN | WHATREGS_STACK;
  else if (regno >= L0_REGNUM && regno <= I7_REGNUM)
    whatregs = WHATREGS_STACK;
  else if (regno >= FP0_REGNUM && regno < FP0_REGNUM + 32)
    whatregs = WHATREGS_FLOAT;
  else if (regno == SP_REGNUM)
    whatregs = WHATREGS_STACK | WHATREGS_GEN;
  else
    whatregs = WHATREGS_GEN;

  if (whatregs & WHATREGS_GEN)
    {
      struct econtext ec;		/* general regs */
      int retval;

      ec.tbr = read_register (TBR_REGNUM);
      memcpy (&ec.g1, &registers[REGISTER_BYTE (G1_REGNUM)],
	      4 * REGISTER_RAW_SIZE (G1_REGNUM));

      ec.psr = read_register (PS_REGNUM);
      ec.y = read_register (Y_REGNUM);
      ec.pc = read_register (PC_REGNUM);
      ec.npc = read_register (NPC_REGNUM);
      ec.wim = read_register (WIM_REGNUM);

      memcpy (ec.o, &registers[REGISTER_BYTE (O0_REGNUM)],
	      8 * REGISTER_RAW_SIZE (O0_REGNUM));

      errno = 0;
      retval = ptrace (PTRACE_SETREGS, BUILDPID (inferior_pid, general_thread), (PTRACE_ARG3_TYPE) &ec,
		       0);
      if (errno)
	perror_with_name ("Sparc fetch_inferior_registers(ptrace)");
    }

  if (whatregs & WHATREGS_STACK)
    {
      int regoffset;
      CORE_ADDR sp;

      sp = read_register (SP_REGNUM);

      if (regno == -1 || regno == SP_REGNUM)
	{
	  if (!register_valid[L0_REGNUM+5])
	    abort();
	  target_xfer_memory (sp + FRAME_SAVED_I0,
			      &registers[REGISTER_BYTE (I0_REGNUM)],
			      8 * REGISTER_RAW_SIZE (I0_REGNUM), 1);

	  target_xfer_memory (sp + FRAME_SAVED_L0,
			      &registers[REGISTER_BYTE (L0_REGNUM)],
			      8 * REGISTER_RAW_SIZE (L0_REGNUM), 1);
	}
      else if (regno >= L0_REGNUM && regno <= I7_REGNUM)
	{
	  if (!register_valid[regno])
	    abort();
	  if (regno >= L0_REGNUM && regno <= L0_REGNUM + 7)
	    regoffset = REGISTER_BYTE (regno) - REGISTER_BYTE (L0_REGNUM)
	      + FRAME_SAVED_L0;
	  else
	    regoffset = REGISTER_BYTE (regno) - REGISTER_BYTE (I0_REGNUM)
	      + FRAME_SAVED_I0;
	  target_xfer_memory (sp + regoffset, &registers[REGISTER_BYTE (regno)],
			      REGISTER_RAW_SIZE (regno), 1);
	}
    }

  if (whatregs & WHATREGS_FLOAT)
    {
      struct fcontext fc;		/* fp regs */
      int retval;

/* We read fcontext first so that we can get good values for fq_t... */
      errno = 0;
      retval = ptrace (PTRACE_GETFPREGS, BUILDPID (inferior_pid, general_thread), (PTRACE_ARG3_TYPE) &fc,
		       0);
      if (errno)
	perror_with_name ("Sparc fetch_inferior_registers(ptrace)");
  
      memcpy (fc.f.fregs, &registers[REGISTER_BYTE (FP0_REGNUM)],
	      32 * REGISTER_RAW_SIZE (FP0_REGNUM));

      fc.fsr = read_register (FPS_REGNUM);

      errno = 0;
      retval = ptrace (PTRACE_SETFPREGS, BUILDPID (inferior_pid, general_thread), (PTRACE_ARG3_TYPE) &fc,
		       0);
      if (errno)
	perror_with_name ("Sparc fetch_inferior_registers(ptrace)");
      }
#endif
}
#endif /* SPARC */

#ifndef SPARC

/* Return the offset relative to the start of the per-thread data to the
   saved context block.  */

static unsigned long
lynx_registers_addr()
{
  CORE_ADDR stblock;
  int ecpoff = offsetof(st_t, ecp);
  CORE_ADDR ecp;

  errno = 0;
  stblock = (CORE_ADDR) ptrace (PTRACE_THREADUSER, BUILDPID (inferior_pid, general_thread),
				(PTRACE_ARG3_TYPE)0, 0);
  if (errno)
    perror_with_name ("PTRACE_THREADUSER");

  ecp = (CORE_ADDR) ptrace (PTRACE_PEEKTHREAD, BUILDPID (inferior_pid, general_thread),
			    (PTRACE_ARG3_TYPE)ecpoff, 0);
  if (errno)
    perror_with_name ("lynx_registers_addr(PTRACE_PEEKTHREAD)");

  return ecp - stblock;
}

/* Fetch one or more registers from the inferior.  REGNO == -1 to get
   them all.  We actually fetch more than requested, when convenient,
   marking them as valid so we won't fetch them again.  */

void
fetch_inferior_registers (ignored)
     int ignored;
{
  int regno;
  unsigned long reg;
  unsigned long ecp;

  ecp = lynx_registers_addr();

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      int ptrace_fun = PTRACE_PEEKTHREAD;

#ifdef PTRACE_PEEKUSP
      ptrace_fun = regno == SP_REGNUM ? PTRACE_PEEKUSP : PTRACE_PEEKTHREAD;
#endif

      errno = 0;
      reg = ptrace (ptrace_fun, BUILDPID (inferior_pid, general_thread),
		    (PTRACE_ARG3_TYPE) (ecp + regmap[regno]), 0);
      if (errno)
	perror_with_name ("fetch_inferior_registers(PTRACE_PEEKTHREAD)");
  
      *(unsigned long *)&registers[REGISTER_BYTE (regno)] = reg;
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (ignored)
     int ignored;
{
  int regno;
  unsigned long reg;
  unsigned long ecp;

  ecp = lynx_registers_addr();

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      int ptrace_fun = PTRACE_POKEUSER;

#ifdef PTRACE_POKEUSP
      ptrace_fun = regno == SP_REGNUM ? PTRACE_POKEUSP : PTRACE_POKEUSER;
#endif

      reg = *(unsigned long *)&registers[REGISTER_BYTE (regno)];

      errno = 0;
      ptrace (ptrace_fun, BUILDPID (inferior_pid, general_thread),
	      (PTRACE_ARG3_TYPE) (ecp + regmap[regno]), reg);
      if (errno)
	perror_with_name ("PTRACE_POKEUSER");
    }
}

#endif /* ! SPARC */

/* NOTE! I tried using PTRACE_READDATA, etc., to read and write memory
   in the NEW_SUN_PTRACE case.
   It ought to be straightforward.  But it appears that writing did
   not write the data that I specified.  I cannot understand where
   it got the data that it actually did write.  */

/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

void
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
      buffer[i] = ptrace (PTRACE_PEEKTEXT, BUILDPID (inferior_pid, general_thread), addr, 0);
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

  buffer[0] = ptrace (PTRACE_PEEKTEXT, BUILDPID (inferior_pid, general_thread), addr, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (PTRACE_PEEKTEXT, BUILDPID (inferior_pid, general_thread),
		  addr + (count - 1) * sizeof (int), 0);
    }

  /* Copy data to be written over corresponding part of buffer */

  memcpy ((char *) buffer + (memaddr & (sizeof (int) - 1)), myaddr, len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (int))
    {
      while (1)
	{
	  errno = 0;
	  ptrace (PTRACE_POKETEXT, BUILDPID (inferior_pid, general_thread), addr, buffer[i]);
	  if (errno)
	    {
	      fprintf(stderr, "\
ptrace (PTRACE_POKETEXT): errno=%d, pid=0x%x, addr=0x%x, buffer[i] = 0x%x\n",
		      errno, BUILDPID (inferior_pid, general_thread),
		      addr, buffer[i]);
	      fprintf(stderr, "Sleeping for 1 second\n");
	      sleep(1);
	    }
	  else
	    break;
	}
    }

  return 0;
}
