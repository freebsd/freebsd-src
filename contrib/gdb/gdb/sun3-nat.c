/* Host-dependent code for Sun-3 for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "inferior.h"
#include "gdbcore.h"

#include <sys/ptrace.h>
#define KERNEL		/* To get floating point reg definitions */
#include <machine/reg.h>

void
fetch_inferior_registers (regno)
     int regno;
{
  struct regs inferior_registers;
#ifdef FP0_REGNUM
  struct fp_status inferior_fp_registers;
#endif
  extern char registers[];

  registers_fetched ();
  
  ptrace (PTRACE_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
#ifdef FP0_REGNUM
  ptrace (PTRACE_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);
#endif 
  
  memcpy (registers, &inferior_registers, 16 * 4);
#ifdef FP0_REGNUM
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], &inferior_fp_registers,
	 sizeof inferior_fp_registers.fps_regs);
#endif 
  *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = inferior_registers.r_ps;
  *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = inferior_registers.r_pc;
#ifdef FP0_REGNUM
  memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	 &inferior_fp_registers.fps_control,
	 sizeof inferior_fp_registers - sizeof inferior_fp_registers.fps_regs);
#endif 
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  struct regs inferior_registers;
#ifdef FP0_REGNUM
  struct fp_status inferior_fp_registers;
#endif
  extern char registers[];

  memcpy (&inferior_registers, registers, 16 * 4);
#ifdef FP0_REGNUM
  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	 sizeof inferior_fp_registers.fps_regs);
#endif
  inferior_registers.r_ps = *(int *)&registers[REGISTER_BYTE (PS_REGNUM)];
  inferior_registers.r_pc = *(int *)&registers[REGISTER_BYTE (PC_REGNUM)];

#ifdef FP0_REGNUM
  memcpy (&inferior_fp_registers.fps_control,
	 &registers[REGISTER_BYTE (FPC_REGNUM)],
	 sizeof inferior_fp_registers - sizeof inferior_fp_registers.fps_regs);
#endif

  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
#if FP0_REGNUM
  ptrace (PTRACE_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);
#endif
}


/* All of this stuff is only relevant if both host and target are sun3.  */
/* Machine-dependent code for pulling registers out of a Sun-3 core file. */

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  extern char registers[];
  struct regs *regs = (struct regs *) core_reg_sect;

  if (which == 0) {
    if (core_reg_size < sizeof (struct regs))
      error ("Can't find registers in core file");

    memcpy (registers, (char *)regs, 16 * 4);
    supply_register (PS_REGNUM, (char *)&regs->r_ps);
    supply_register (PC_REGNUM, (char *)&regs->r_pc);

  } else if (which == 2) {

#define fpustruct  ((struct fpu *) core_reg_sect)

    if (core_reg_size >= sizeof (struct fpu))
      {
#ifdef FP0_REGNUM
	memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	      fpustruct->f_fpstatus.fps_regs,
	      sizeof fpustruct->f_fpstatus.fps_regs);
	memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	      &fpustruct->f_fpstatus.fps_control,
	      sizeof fpustruct->f_fpstatus - 
		sizeof fpustruct->f_fpstatus.fps_regs);
#endif
      }
    else
      fprintf_unfiltered (gdb_stderr, "Couldn't read float regs from core file\n");
  }
}


/* Register that we are able to handle sun3 core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns sun3_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_sun3 ()
{
  add_core_fns (&sun3_core_fns);
}
