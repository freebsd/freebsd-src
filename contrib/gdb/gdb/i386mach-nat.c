/* Native dependent code for Mach 386's for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1995, 1996, 1999, 2000,
   2001 Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/ptrace.h>
#include <machine/reg.h>

#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/core.h>

static void fetch_core_registers (char *, unsigned, int, CORE_ADDR);

void
fetch_inferior_registers (int regno)
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;

  registers_fetched ();

  ptrace (PTRACE_GETREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_registers);
  ptrace (PTRACE_GETFPREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_fp_registers);

  memcpy (registers, &inferior_registers, sizeof inferior_registers);

  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	  inferior_fp_registers.f_st,
	  sizeof inferior_fp_registers.f_st);
  memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	  &inferior_fp_registers.f_ctrl,
	  sizeof inferior_fp_registers - sizeof inferior_fp_registers.f_st);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (int regno)
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;

  memcpy (&inferior_registers, registers, 20 * 4);

  memcpy (inferior_fp_registers.f_st, &registers[REGISTER_BYTE (FP0_REGNUM)],
	  sizeof inferior_fp_registers.f_st);
  memcpy (&inferior_fp_registers.f_ctrl,
	  &registers[REGISTER_BYTE (FPC_REGNUM)],
	  sizeof inferior_fp_registers - sizeof inferior_fp_registers.f_st);

#ifdef PTRACE_FP_BUG
  if (regno == FP_REGNUM || regno == -1)
    /* Storing the frame pointer requires a gross hack, in which an
       instruction that moves eax into ebp gets single-stepped.  */
    {
      int stack = inferior_registers.r_reg[SP_REGNUM];
      int stuff = ptrace (PTRACE_PEEKDATA, PIDGET (inferior_ptid),
			  (PTRACE_ARG3_TYPE) stack);
      int reg = inferior_registers.r_reg[EAX];
      inferior_registers.r_reg[EAX] =
	inferior_registers.r_reg[FP_REGNUM];
      ptrace (PTRACE_SETREGS, PIDGET (inferior_ptid),
	      (PTRACE_ARG3_TYPE) & inferior_registers);
      ptrace (PTRACE_POKEDATA, PIDGET (inferior_ptid),
              (PTRACE_ARG3_TYPE) stack, 0xc589);
      ptrace (PTRACE_SINGLESTEP, PIDGET (inferior_ptid),
              (PTRACE_ARG3_TYPE) stack, 0);
      wait (0);
      ptrace (PTRACE_POKEDATA, PIDGET (inferior_ptid),
              (PTRACE_ARG3_TYPE) stack, stuff);
      inferior_registers.r_reg[EAX] = reg;
    }
#endif
  ptrace (PTRACE_SETREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_registers);
  ptrace (PTRACE_SETFPREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_fp_registers);
}



/* Provide registers to GDB from a core file.

   CORE_REG_SECT points to an array of bytes, which were obtained from
   a core file which BFD thinks might contain register contents. 
   CORE_REG_SIZE is its size.

   WHICH says which register set corelow suspects this is:
     0 --- the general-purpose register set
     2 --- the floating-point register set

   REG_ADDR isn't used.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  int val;

  switch (which)
    {
    case 0:
    case 1:
      memcpy (registers, core_reg_sect, core_reg_size);
      break;

    case 2:
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	      core_reg_sect,
	      core_reg_size);	/* FIXME, probably bogus */
#ifdef FPC_REGNUM
      memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	      &corestr.c_fpu.f_fpstatus.f_ctrl,
	      sizeof corestr.c_fpu.f_fpstatus -
	      sizeof corestr.c_fpu.f_fpstatus.f_st);
#endif
      break;
    }
}


/* Register that we are able to handle i386mach core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns i386mach_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

void
_initialize_core_i386mach (void)
{
  add_core_fns (&i386mach_core_fns);
}
