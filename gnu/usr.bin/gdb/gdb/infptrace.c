/* Low level Unix child interface to ptrace, for GDB when running under Unix.
   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <sys/param.h>
#include <signal.h>
#include <sys/ioctl.h>

#ifndef NO_PTRACE_H
#ifdef PTRACE_IN_WRONG_PLACE
#include <ptrace.h>
#else
#include <sys/ptrace.h>
#endif
#endif /* NO_PTRACE_H */

#if !defined (PT_KILL)
#define PT_KILL 8
#endif

#if !defined (PT_STEP)
#define PT_STEP 9
#define PT_CONTINUE 7
#define PT_READ_U 3
#define PT_WRITE_U 6
#define PT_READ_I 1
#define	PT_READ_D 2
#define PT_WRITE_I 4
#define PT_WRITE_D 5
#endif /* No PT_STEP.  */

#ifndef PT_ATTACH
#define PT_ATTACH PTRACE_ATTACH
#endif
#ifndef PT_DETACH
#define PT_DETACH PTRACE_DETACH
#endif

#include "gdbcore.h"
#ifndef	NO_SYS_FILE
#include <sys/file.h>
#endif
#if 0
/* Don't think this is used anymore.  On the sequent (not sure whether it's
   dynix or ptx or both), it is included unconditionally by sys/user.h and
   not protected against multiple inclusion.  */
#include <sys/stat.h>
#endif

#if !defined (FETCH_INFERIOR_REGISTERS)
#include <sys/user.h>		/* Probably need to poke the user structure */
#if defined (KERNEL_U_ADDR_BSD)
#include <a.out.h>		/* For struct nlist */
#endif /* KERNEL_U_ADDR_BSD.  */
#endif /* !FETCH_INFERIOR_REGISTERS */


/* This function simply calls ptrace with the given arguments.
   It exists so that all calls to ptrace are isolated in this
   machine-dependent file. */
int
call_ptrace (request, pid, addr, data)
     int request, pid;
     PTRACE_ARG3_TYPE addr;
     int data;
{
  return ptrace (request, pid, addr, data
#if defined (FIVE_ARG_PTRACE)
		 /* Deal with HPUX 8.0 braindamage.  We never use the
		    calls which require the fifth argument.  */
		 , 0
#endif
		 );
}

#if defined (DEBUG_PTRACE) || defined (FIVE_ARG_PTRACE)
/* For the rest of the file, use an extra level of indirection */
/* This lets us breakpoint usefully on call_ptrace. */
#define ptrace call_ptrace
#endif

void
kill_inferior ()
{
  if (inferior_pid == 0)
    return;
  /* ptrace PT_KILL only works if process is stopped!!!  So stop it with
     a real signal first, if we can.  FIXME: This is bogus.  When the inferior
     is not stopped, GDB should just be waiting for it.  Either the following
     line is unecessary, or there is some problem elsewhere in GDB which
     causes us to get here when the inferior is not stopped.  */
  kill (inferior_pid, SIGKILL);
  ptrace (PT_KILL, inferior_pid, (PTRACE_ARG3_TYPE) 0, 0);
  wait ((int *)0);
  target_mourn_inferior ();
}

#ifndef CHILD_RESUME

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
child_resume (pid, step, signal)
     int pid;
     int step;
     enum target_signal signal;
{
  errno = 0;

  if (pid == -1)
    /* Resume all threads.  */
    /* I think this only gets used in the non-threaded case, where "resume
       all threads" and "resume inferior_pid" are the same.  */
    pid = inferior_pid;

  /* An address of (PTRACE_ARG3_TYPE)1 tells ptrace to continue from where
     it was.  (If GDB wanted it to start some other way, we have already
     written a new PC value to the child.)

     If this system does not support PT_STEP, a higher level function will
     have called single_step() to transmute the step request into a
     continue request (by setting breakpoints on all possible successor
     instructions), so we don't have to worry about that here.  */

  if (step)
    ptrace (PT_STEP,     pid, (PTRACE_ARG3_TYPE) 1,
	    target_signal_to_host (signal));
  else
    ptrace (PT_CONTINUE, pid, (PTRACE_ARG3_TYPE) 1,
	    target_signal_to_host (signal));

  if (errno)
    perror_with_name ("ptrace");
}
#endif /* CHILD_RESUME */


#ifdef ATTACH_DETACH
#include <sys/fcntl.h>
/* Start debugging the process whose number is PID.  */
int
attach (pid)
     int pid;
{
#if defined(BSD4_4) && BSD >= 199306
  char procfile[MAXPATHLEN];
  int fd;

  sprintf(procfile, "/proc/%d/ctl", pid);
  fd = open(procfile, O_WRONLY, 0);

  if (fd < 0) {
    perror_with_name ("open");
  }

  /* send attach message to the process */
  if (write (fd, "attach", 7) < 0) {
    close(fd);
    perror_with_name ("write:attach");
  }
  /* wait for the process to stop */
#if 0
  if (write (fd, "wait", 5) < 0) {
    close(fd);
    perror_with_name ("write:wait");
  }
#endif
  close (fd);
#else
  errno = 0;
  ptrace (PT_ATTACH, pid, (PTRACE_ARG3_TYPE) 0, 0);
  if (errno)
    perror_with_name ("ptrace");
#endif
  attach_flag = 1;
  return pid;
}

/* Stop debugging the process whose number is PID
   and continue it with signal number SIGNAL.
   SIGNAL = 0 means just continue it.  */

void
detach (signal)
     int signal;
{
#if defined(BSD4_4) && BSD >= 199306
  char procfile[MAXPATHLEN];
  int fd;

  sprintf(procfile, "/proc/%d/ctl", inferior_pid);
  fd = open(procfile, O_WRONLY, 0);

  if (fd < 0) {
    perror_with_name ("open");
  }
  /* send detach message to the process */
  if (write (fd, "detach", 7) < 0) {
    close(fd);
    perror_with_name ("write:detach");
  }
  /* TODO signals */
  close (fd);
#else
  errno = 0;
  ptrace (PT_DETACH, inferior_pid, (PTRACE_ARG3_TYPE) 1, signal);
  if (errno)
    perror_with_name ("ptrace");
#endif
  attach_flag = 0;
}
#endif /* ATTACH_DETACH */

/* Default the type of the ptrace transfer to int.  */
#ifndef PTRACE_XFER_TYPE
#define PTRACE_XFER_TYPE int
#endif

/* KERNEL_U_ADDR is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */
#if defined (KERNEL_U_ADDR_BSD) && !defined (FETCH_INFERIOR_REGISTERS)
/* Get kernel_u_addr using BSD-style nlist().  */
CORE_ADDR kernel_u_addr;
#endif /* KERNEL_U_ADDR_BSD.  */

void
_initialize_kernel_u_addr ()
{
#if defined (KERNEL_U_ADDR_BSD) && !defined (FETCH_INFERIOR_REGISTERS)
  struct nlist names[2];

  names[0].n_un.n_name = "_u";
  names[1].n_un.n_name = NULL;
  if (nlist ("/vmunix", names) == 0)
    kernel_u_addr = names[0].n_value;
  else
    fatal ("Unable to get kernel u area address.");
#endif /* KERNEL_U_ADDR_BSD.  */
}

#if !defined (FETCH_INFERIOR_REGISTERS)

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

/* Registers we shouldn't try to fetch.  */
#if !defined (CANNOT_FETCH_REGISTER)
#define CANNOT_FETCH_REGISTER(regno) 0
#endif

/* Fetch one register.  */

static void
fetch_register (regno)
     int regno;
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr;
  char buf[MAX_REGISTER_RAW_SIZE];
  char mess[128];				/* For messages */
  register int i;

  /* Offset of registers within the u area.  */
  unsigned int offset;

  if (CANNOT_FETCH_REGISTER (regno))
    {
      memset (buf, '\0', REGISTER_RAW_SIZE (regno));	/* Supply zeroes */
      supply_register (regno, buf);
      return;
    }

  offset = U_REGS_OFFSET;

  regaddr = register_addr (regno, offset);
  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) &buf[i] = ptrace (PT_READ_U, inferior_pid,
					      (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  sprintf (mess, "reading register %s (#%d)", reg_names[regno], regno);
	  perror_with_name (mess);
	}
    }
  supply_register (regno, buf);
}


/* Fetch all registers, or just one, from the child process.  */

void
fetch_inferior_registers (regno)
     int regno;
{
  int numregs;

  if (regno == -1)
    {
      numregs = ARCH_NUM_REGS;
      for (regno = 0; regno < numregs; regno++)
        fetch_register (regno);
    }
  else
    fetch_register (regno);
}

/* Registers we shouldn't try to store.  */
#if !defined (CANNOT_STORE_REGISTER)
#define CANNOT_STORE_REGISTER(regno) 0
#endif

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr;
  char buf[80];
  register int i, numregs;

  unsigned int offset = U_REGS_OFFSET;

  if (regno >= 0)
    {
      regaddr = register_addr (regno, offset);
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof(PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  ptrace (PT_WRITE_U, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		  *(PTRACE_XFER_TYPE *) &registers[REGISTER_BYTE (regno) + i]);
	  if (errno != 0)
	    {
	      sprintf (buf, "writing register number %d(%d)", regno, i);
	      perror_with_name (buf);
	    }
	  regaddr += sizeof(PTRACE_XFER_TYPE);
	}
    }
  else
    {
      numregs = ARCH_NUM_REGS;
      for (regno = 0; regno < numregs; regno++)
	{
	  if (CANNOT_STORE_REGISTER (regno))
	    continue;
	  regaddr = register_addr (regno, offset);
	  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof(PTRACE_XFER_TYPE))
	    {
	      errno = 0;
	      ptrace (PT_WRITE_U, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		      *(PTRACE_XFER_TYPE *) &registers[REGISTER_BYTE (regno) + i]);
	      if (errno != 0)
		{
		  sprintf (buf, "writing register number %d(%d)", regno, i);
		  perror_with_name (buf);
		}
	      regaddr += sizeof(PTRACE_XFER_TYPE);
	    }
	}
    }
}
#endif /* !defined (FETCH_INFERIOR_REGISTERS).  */


#if !defined (CHILD_XFER_MEMORY)
/* NOTE! I tried using PTRACE_READDATA, etc., to read and write memory
   in the NEW_SUN_PTRACE case.
   It ought to be straightforward.  But it appears that writing did
   not write the data that I specified.  I cannot understand where
   it got the data that it actually did write.  */

/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   WRITE is nonzero.

   Returns the length copied, which is either the LEN argument or zero.
   This xfer function does not do partial moves, since child_ops
   doesn't allow memory operations to cross below us in the target stack
   anyway.  */

int
child_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & - sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
    = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1)
      / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer
    = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));

  if (write)
    {
      /* Fill start and end extra bytes of buffer with existing memory data.  */

      if (addr != memaddr || len < (int) sizeof (PTRACE_XFER_TYPE)) {
	/* Need part of initial word -- fetch it.  */
        buffer[0] = ptrace (PT_READ_I, inferior_pid, (PTRACE_ARG3_TYPE) addr,
			    0);
      }

      if (count > 1)		/* FIXME, avoid if even boundary */
	{
	  buffer[count - 1]
	    = ptrace (PT_READ_I, inferior_pid,
		      ((PTRACE_ARG3_TYPE)
		       (addr + (count - 1) * sizeof (PTRACE_XFER_TYPE))),
		      0);
	}

      /* Copy data to be written over corresponding part of buffer */

      memcpy ((char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)),
	      myaddr,
	      len);

      /* Write the entire buffer.  */

      for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  ptrace (PT_WRITE_D, inferior_pid, (PTRACE_ARG3_TYPE) addr,
		  buffer[i]);
	  if (errno)
	    {
	      /* Using the appropriate one (I or D) is necessary for
		 Gould NP1, at least.  */
	      errno = 0;
	      ptrace (PT_WRITE_I, inferior_pid, (PTRACE_ARG3_TYPE) addr,
		      buffer[i]);
	    }
	  if (errno)
	    return 0;
	}
    }
  else
    {
      /* Read all the longwords */
      for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  buffer[i] = ptrace (PT_READ_I, inferior_pid,
			      (PTRACE_ARG3_TYPE) addr, 0);
	  if (errno)
	    return 0;
	  QUIT;
	}

      /* Copy appropriate bytes out of the buffer.  */
      memcpy (myaddr,
	      (char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)),
	      len);
    }
  return len;
}
#endif /* !defined (CHILD_XFER_MEMORY).  */
