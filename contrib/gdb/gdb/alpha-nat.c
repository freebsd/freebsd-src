/* Low level Alpha interface, for GDB when running native.
   Copyright 1993, 1995, 1996 Free Software Foundation, Inc.

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
#include "target.h"
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <sys/user.h>

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

/* Extract the register values out of the core file and store
   them where `read_register' will find them.

   CORE_REG_SECT points to the register values themselves, read into memory.
   CORE_REG_SIZE is the size of that area.
   WHICH says which set of registers we are handling (0 = int, 2 = float
         on machines where they are discontiguous).
   REG_ADDR is the offset from u.u_ar0 to the register values relative to
            core_reg_sect.  This is used with old-fashioned core files to
	    locate the registers in a large upage-plus-stack ".reg" section.
	    Original upage address X is at location core_reg_sect+x+reg_addr.
 */

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned reg_addr;
{
  register int regno;
  register int addr;
  int bad_reg = -1;

  /* Table to map a gdb regnum to an index in the core register section.
     The floating point register values are garbage in OSF/1.2 core files.  */
  static int core_reg_mapping[NUM_REGS] =
  {
#define EFL (EF_SIZE / 8)
	EF_V0,	EF_T0,	EF_T1,	EF_T2,	EF_T3,	EF_T4,	EF_T5,	EF_T6,
	EF_T7,	EF_S0,	EF_S1,	EF_S2,	EF_S3,	EF_S4,	EF_S5,	EF_S6,
	EF_A0,	EF_A1,	EF_A2,	EF_A3,	EF_A4,	EF_A5,	EF_T8,	EF_T9,
	EF_T10,	EF_T11,	EF_RA,	EF_T12,	EF_AT,	EF_GP,	EF_SP,	-1,
	EFL+0,	EFL+1,	EFL+2,	EFL+3,	EFL+4,	EFL+5,	EFL+6,	EFL+7,
	EFL+8,	EFL+9,	EFL+10,	EFL+11,	EFL+12,	EFL+13,	EFL+14,	EFL+15,
	EFL+16,	EFL+17,	EFL+18,	EFL+19,	EFL+20,	EFL+21,	EFL+22,	EFL+23,
	EFL+24,	EFL+25,	EFL+26,	EFL+27,	EFL+28,	EFL+29,	EFL+30,	EFL+31,
	EF_PC,	-1
  };
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      if (CANNOT_FETCH_REGISTER (regno))
	{
	  supply_register (regno, zerobuf);
	  continue;
	}
      addr = 8 * core_reg_mapping[regno];
      if (addr < 0 || addr >= core_reg_size)
	{
	  if (bad_reg < 0)
	    bad_reg = regno;
	}
      else
	{
	  supply_register (regno, core_reg_sect + addr);
	}
    }
  if (bad_reg >= 0)
    {
      error ("Register %s not found in core file.", reg_names[bad_reg]);
    }
}

/* Map gdb internal register number to a ptrace ``address''.
   These ``addresses'' are defined in <sys/ptrace.h> */

#define REGISTER_PTRACE_ADDR(regno) \
   (regno < FP0_REGNUM ? 	GPR_BASE + (regno) \
  : regno == PC_REGNUM ?	PC	\
  : regno >= FP0_REGNUM ?	FPR_BASE + ((regno) - FP0_REGNUM) \
  : 0)

/* Return the ptrace ``address'' of register REGNO. */

unsigned int
register_addr (regno, blockend)
     int regno;
     int blockend;
{
  return REGISTER_PTRACE_ADDR (regno);
}

int
kernel_u_size ()
{
  return (sizeof (struct user));
}

#ifdef USE_PROC_FS
#include <sys/procfs.h>

/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 */

void 
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
  register int regi;
  register long *regp = gregsetp->regs;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

  for (regi = 0; regi < 31; regi++)
    supply_register (regi, (char *)(regp + regi));

  supply_register (PC_REGNUM, (char *)(regp + 31));

  /* Fill inaccessible registers with zero.  */
  supply_register (ZERO_REGNUM, zerobuf);
  supply_register (FP_REGNUM, zerobuf);
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
  int regi;
  register long *regp = gregsetp->regs;

  for (regi = 0; regi < 31; regi++)
    if ((regno == -1) || (regno == regi))
      *(regp + regi) = *(long *) &registers[REGISTER_BYTE (regi)];

  if ((regno == -1) || (regno == PC_REGNUM))
    *(regp + 31) = *(long *) &registers[REGISTER_BYTE (PC_REGNUM)];
}

/*
 * Now we do the same thing for floating-point registers.
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (fpregsetp)
     fpregset_t *fpregsetp;
{
  register int regi;
  register long *regp = fpregsetp->regs;

  for (regi = 0; regi < 32; regi++)
    supply_register (regi + FP0_REGNUM, (char *)(regp + regi));
}

void
fill_fpregset (fpregsetp, regno)
     fpregset_t *fpregsetp;
     int regno;
{
  int regi;
  register long *regp = fpregsetp->regs;

  for (regi = FP0_REGNUM; regi < FP0_REGNUM + 32; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regi - FP0_REGNUM) =
	    *(long *) &registers[REGISTER_BYTE (regi)];
	}
    }
}
#endif


/* Register that we are able to handle alpha core file formats. */

static struct core_fns alpha_core_fns =
{
  bfd_target_aout_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_alpha ()
{
  add_core_fns (&alpha_core_fns);
}
