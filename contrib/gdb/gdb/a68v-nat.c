/* Host-dependent code for Apollo-68ks for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

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

#ifndef _ISP__M68K
#define _ISP__M68K 1
#endif

#include <ptrace.h>

extern int errno;

void
fetch_inferior_registers (ignored)
    int ignored;
{
  struct ptrace_$data_regs_m68k inferior_registers;
  struct ptrace_$floating_regs_m68k inferior_fp_registers;
  struct ptrace_$control_regs_m68k inferior_control_registers;
  extern char registers[];

  ptrace_$init_control(&inferior_control_registers);
  inferior_fp_registers.size = sizeof(inferior_fp_registers);

  registers_fetched ();
  
  ptrace (PTRACE_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers,
	  ptrace_$data_set,
	  (PTRACE_ARG3_TYPE) &inferior_registers,
	  ptrace_$data_set);

  ptrace (PTRACE_GETREGS, inferior_pid,
	(PTRACE_ARG3_TYPE) &inferior_fp_registers,
	ptrace_$floating_set_m68k,
	(PTRACE_ARG3_TYPE) &inferior_fp_registers,
	ptrace_$floating_set_m68k);

  ptrace (PTRACE_GETREGS, inferior_pid,
	(PTRACE_ARG3_TYPE) &inferior_control_registers,
	ptrace_$control_set_m68k,
	(PTRACE_ARG3_TYPE) &inferior_control_registers,
	ptrace_$control_set_m68k);

  bcopy (&inferior_registers, registers, 16 * 4);
  bcopy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	 sizeof inferior_fp_registers.regs);
  *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = inferior_control_registers.sr;
  *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = inferior_control_registers.pc;
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  struct ptrace_$data_regs_m68k inferior_registers;
  struct ptrace_$floating_regs_m68k inferior_fp_registers;
  struct ptrace_$control_regs_m68k inferior_control_registers;
  extern char registers[];

  ptrace_$init_control(&inferior_control_registers);
  inferior_fp_registers.size = sizeof(inferior_fp_registers);

  ptrace (PTRACE_GETREGS, inferior_pid,
	(PTRACE_ARG3_TYPE) &inferior_fp_registers,
	ptrace_$floating_set_m68k,
	(PTRACE_ARG3_TYPE) &inferior_fp_registers,
	ptrace_$floating_set_m68k);

  ptrace (PTRACE_GETREGS, inferior_pid,
	(PTRACE_ARG3_TYPE) &inferior_control_registers,
	ptrace_$control_set_m68k,
	(PTRACE_ARG3_TYPE) &inferior_control_registers,
	ptrace_$control_set_m68k);

  bcopy (registers, &inferior_registers, sizeof(inferior_registers));

  bcopy (&registers[REGISTER_BYTE (FP0_REGNUM)], inferior_fp_registers.regs,
	 sizeof inferior_fp_registers.regs);

  inferior_control_registers.sr = *(int *)&registers[REGISTER_BYTE (PS_REGNUM)];
  inferior_control_registers.pc = *(int *)&registers[REGISTER_BYTE (PC_REGNUM)];

  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers,
	  ptrace_$data_set_m68k,
	  (PTRACE_ARG3_TYPE) &inferior_registers,
	  ptrace_$data_set_m68k);

  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers,
	  ptrace_$floating_set_m68k,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers,
	  ptrace_$floating_set_m68k);

  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_control_registers,
	  ptrace_$control_set_m68k,
	  (PTRACE_ARG3_TYPE) &inferior_control_registers,
	  ptrace_$control_set_m68k);
}
