/* Native-dependent code for Alpha BSD's.
   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

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

#include "defs.h"
#include "inferior.h"
#include "regcache.h"

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#ifdef HAVE_SYS_PROCFS_H
#include <sys/procfs.h>
#endif

#ifndef HAVE_GREGSET_T
typedef struct reg gregset_t;
#endif

#ifndef HAVE_FPREGSET_T
typedef struct fpreg fpregset_t;
#endif

#include "gregset.h"

/* Number of general-purpose registers.  */
#define NUM_GREGS  32

/* Number of floating point registers.  */
#define NUM_FPREGS 31


/* Transfering the registers between GDB, inferiors and core files.  */

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (gregset_t *gregsetp)
{
  int i;

  for (i = 0; i < NUM_GREGS; i++)
    {
      if (CANNOT_FETCH_REGISTER (i))
	supply_register (i, NULL);
      else
	supply_register (i, (char *) &gregsetp->r_regs[i]);
    }

  /* The PC travels in the R_ZERO slot.  */
  supply_register (PC_REGNUM, (char *) &gregsetp->r_regs[R_ZERO]);
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int i;

  for (i = 0; i < NUM_GREGS; i++)
    if ((regno == -1 || regno == i) && ! CANNOT_STORE_REGISTER (i))
      regcache_collect (i, (char *) &gregsetp->r_regs[i]);

  /* The PC travels in the R_ZERO slot.  */
  if (regno == -1 || regno == PC_REGNUM)
    regcache_collect (PC_REGNUM, (char *) &gregsetp->r_regs[R_ZERO]);
}

/* Fill GDB's register array with the floating-point register values
   in *FPREGSETP.  */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  int i;

  for (i = FP0_REGNUM; i < FP0_REGNUM + NUM_FPREGS; i++)
    {
      if (CANNOT_FETCH_REGISTER (i))
	supply_register (i, NULL);
      else
	supply_register (i, (char *) &fpregsetp->fpr_regs[i - FP0_REGNUM]);
    }

  supply_register (FPCR_REGNUM, (char *) &fpregsetp->fpr_cr);
}

/* Fill register REGNO (if it is a floating-point register) in
   *FPREGSETP with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int i;

  for (i = FP0_REGNUM; i < FP0_REGNUM + NUM_FPREGS; i++)
    if ((regno == -1 || regno == i) && ! CANNOT_STORE_REGISTER (i))
      regcache_collect (i, (char *) &fpregsetp->fpr_regs[i - FP0_REGNUM]);

  if (regno == -1 || regno == FPCR_REGNUM)
    regcache_collect (FPCR_REGNUM, (char *) &fpregsetp->fpr_cr);
}


/* Determine if PT_GETREGS fetches this register.  */

static int
getregs_supplies (int regno)
{

  return ((regno >= V0_REGNUM && regno <= ZERO_REGNUM)
	  || regno >= PC_REGNUM);
}


/* Fetch register REGNO from the inferior.  If REGNO is -1, do this
   for all registers (including the floating point registers).  */

void
fetch_inferior_registers (int regno)
{

  if (regno == -1 || getregs_supplies (regno))
    {
      gregset_t gregs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_ARG3_TYPE) &gregs, 0) == -1)
	perror_with_name ("Couldn't get registers");

      supply_gregset (&gregs);
      if (regno != -1)
	return;
    }

  if (regno == -1 || regno >= FP0_REGNUM)
    {
      fpregset_t fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_ARG3_TYPE) &fpregs, 0) == -1)
	perror_with_name ("Couldn't get floating point status");

      supply_fpregset (&fpregs);
    }

  /* Reset virtual frame pointer.  */
  supply_register (FP_REGNUM, NULL);
}

/* Store register REGNO back into the inferior.  If REGNO is -1, do
   this for all registers (including the floating point registers).  */

void
store_inferior_registers (int regno)
{

  if (regno == -1 || getregs_supplies (regno))
    {
      gregset_t gregs;
      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
                  (PTRACE_ARG3_TYPE) &gregs, 0) == -1)
        perror_with_name ("Couldn't get registers");

      fill_gregset (&gregs, regno);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
                  (PTRACE_ARG3_TYPE) &gregs, 0) == -1)
        perror_with_name ("Couldn't write registers");

      if (regno != -1)
	return;
    }

  if (regno == -1 || regno >= FP0_REGNUM)
    {
      fpregset_t fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_ARG3_TYPE) &fpregs, 0) == -1)
	perror_with_name ("Couldn't get floating point status");

      fill_fpregset (&fpregs, regno);

      if (ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_ARG3_TYPE) &fpregs, 0) == -1)
	perror_with_name ("Couldn't write floating point status");
    }
}
