/* Native-dependent code for Motorola m68k's running NetBSD, for GDB.
   Copyright 1988, 1989, 1991, 1992, 1994, 1996, 2000, 2001
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"

void
fetch_inferior_registers (int regno)
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_registers, 0);
  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers,
	  sizeof (inferior_registers));

  ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_fp_registers, 0);
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], &inferior_fp_registers,
	  sizeof (inferior_fp_registers));

  registers_fetched ();
}

void
store_inferior_registers (int regno)
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)],
	  sizeof (inferior_registers));
  ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_registers, 0);

  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	  sizeof (inferior_fp_registers));
  ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
	  (PTRACE_ARG3_TYPE) & inferior_fp_registers, 0);
}

struct md_core
{
  struct reg intreg;
  struct fpreg freg;
};

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size, int which,
		      CORE_ADDR ignore)
{
  struct md_core *core_reg = (struct md_core *) core_reg_sect;

  /* Integer registers */
  memcpy (&registers[REGISTER_BYTE (0)],
	  &core_reg->intreg, sizeof (struct reg));
  /* Floating point registers */
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	  &core_reg->freg, sizeof (struct fpreg));
}

/* Register that we are able to handle m68knbsd core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */
   
static struct core_fns m68knbsd_core_fns =
{  
  bfd_target_unknown_flavour,           /* core_flavour */
  default_check_format,                 /* check_format */
  default_core_sniffer,                 /* core_sniffer */
  fetch_core_registers,                 /* core_read_registers */
  NULL                                  /* next */
}; 
   
void
_initialize_m68knbsd_nat (void)
{
  add_core_fns (&m68knbsd_core_fns);
}
