/* Native-dependent code for Motorola m68k's running NetBSD, for GDB.
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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>

#include "defs.h"
#include "inferior.h"

void
fetch_inferior_registers(regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  ptrace (PT_GETREGS, inferior_pid, 
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers, 
	  sizeof(inferior_registers));

  ptrace (PT_GETFPREGS, inferior_pid, 
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], &inferior_fp_registers, 
	  sizeof(inferior_fp_registers));

  registers_fetched ();
}

void
store_inferior_registers(regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)],
	  sizeof(inferior_registers));
  ptrace (PT_SETREGS, inferior_pid, 
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	  sizeof(inferior_fp_registers));
  ptrace (PT_SETFPREGS, inferior_pid, 
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);
}

struct md_core {
  struct reg intreg; 
  struct fpreg freg;
};

void
fetch_core_registers (core_reg_sect, core_reg_size, which, ignore)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int ignore;
{
  struct md_core *core_reg = (struct md_core *)core_reg_sect;
  
  /* Integer registers */
  memcpy(&registers[REGISTER_BYTE (0)],
	 &core_reg->intreg, sizeof(struct reg));
  /* Floating point registers */
  memcpy(&registers[REGISTER_BYTE (FP0_REGNUM)],
	 &core_reg->freg, sizeof(struct fpreg));
}
