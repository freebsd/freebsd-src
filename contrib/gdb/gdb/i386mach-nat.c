/* Native dependent code for Mach 386's for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "gdbcore.h"

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



void
fetch_inferior_registers (regno)
     int regno;		/* Original value discarded */
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;
  extern char registers[];

  registers_fetched ();

  ptrace (PTRACE_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
  ptrace (PTRACE_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);

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
store_inferior_registers (regno)
     int regno;
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;
  extern char registers[];

  memcpy (&inferior_registers, registers, 20 * 4);

  memcpy (inferior_fp_registers.f_st,&registers[REGISTER_BYTE (FP0_REGNUM)],
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
      int stuff = ptrace (PTRACE_PEEKDATA, inferior_pid,
			  (PTRACE_ARG3_TYPE) stack);
      int reg = inferior_registers.r_reg[EAX];
      inferior_registers.r_reg[EAX] =
	inferior_registers.r_reg[FP_REGNUM];
      ptrace (PTRACE_SETREGS, inferior_pid,
	      (PTRACE_ARG3_TYPE) &inferior_registers);
      ptrace (PTRACE_POKEDATA, inferior_pid, (PTRACE_ARG3_TYPE) stack, 0xc589);
      ptrace (PTRACE_SINGLESTEP, inferior_pid, (PTRACE_ARG3_TYPE) stack, 0);
      wait (0);
      ptrace (PTRACE_POKEDATA, inferior_pid, (PTRACE_ARG3_TYPE) stack, stuff);
      inferior_registers.r_reg[EAX] = reg;
    }
#endif
  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
  ptrace (PTRACE_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);
}



/* Work with core files, for GDB. */

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  int val;
  extern char registers[];

  switch (which) {
  case 0:
  case 1:
    memcpy (registers, core_reg_sect, core_reg_size);
    break;

  case 2:
#ifdef FP0_REGNUM
    memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	   core_reg_sect,
	   core_reg_size);		/* FIXME, probably bogus */
#endif
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
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_i386mach ()
{
  add_core_fns (&i386mach_core_fns);
}
