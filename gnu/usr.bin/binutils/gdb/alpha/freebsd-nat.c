/* $FreeBSD$ */
/* Native-dependent code for BSD Unix running on alphas's, for GDB.
   Copyright 1988, 1989, 1991, 1992, 1994, 1996 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <sys/param.h>
#include <sys/user.h>
#include <string.h>
#include "gdbcore.h"
#include "value.h"
#include "inferior.h"

#if defined(HAVE_GREGSET_T)
#include <sys/procfs.h>
#endif

int kernel_debugging = 0;

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 8

/* The definition for JB_PC in machine/reg.h is wrong.
   And we can't get at the correct definition in setjmp.h as it is
   not always available (eg. if _POSIX_SOURCE is defined which is the
   default). As the defintion is unlikely to change (see comment
   in <setjmp.h>, define the correct value here.  */

#undef JB_PC
#define JB_PC 2

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  jb_addr = read_register(A0_REGNUM);

  if (target_read_memory(jb_addr + JB_PC * JB_ELEMENT_SIZE, raw_buffer,
			 sizeof(CORE_ADDR)))
    return 0;

  *pc = extract_address (raw_buffer, sizeof(CORE_ADDR));
  return 1;
}

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

  registers_fetched ();
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

#ifdef HAVE_GREGSET_T
void
supply_gregset (gp)
  gregset_t *gp;
{
  int regno = 0;

  /* These must be ordered the same as REGISTER_NAMES in
     config/alpha/tm-alpha.h. */
  for (regno = 0; regno < 31; regno++)
    supply_register (regno, (char *)&gp->r_regs[regno]);
  supply_register (PC_REGNUM, (char *)&gp->r_regs[regno]);
}
#endif	/* HAVE_GREGSET_T */

#ifdef HAVE_FPREGSET_T
void
supply_fpregset (fp)
  fpregset_t *fp;
{
  int regno = 0;

  for (regno = 0; regno < 32; regno++)
    supply_register (regno + 32, (char *)&fp->fpr_regs[regno]);
}
#endif	/* HAVE_FPREGSET_T */

/*
 * Get registers from a kernel crash dump or live kernel.
 * Called by kvm-fbsd.c:get_kcore_registers().
 */
fetch_kcore_registers (pcbp)
  struct pcb *pcbp;
{

  /* First clear out any garbage. */
  memset(registers, '\0', REGISTER_BYTES);

  /* SP */
  *(long *) &registers[REGISTER_BYTE (SP_REGNUM)] =
    pcbp->pcb_hw.apcb_ksp;

  /* S0 through S6 */
  memcpy (&registers[REGISTER_BYTE (S0_REGNUM)],
          &pcbp->pcb_context[0], 7 * sizeof(long));

  /* PC */
  *(long *) &registers[REGISTER_BYTE (PC_REGNUM)] =
    pcbp->pcb_context[7];

  registers_fetched ();
}

