/* Native-dependent code for BSD Unix running on i386's, for GDB.
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

/* Extract the register values out of the core file and store
   them where `read_register' will find them.
   Extract the floating point state out of the core file and store
   it where `float_info' will find it.

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
     CORE_ADDR reg_addr;
{
#if 0				/* XXX laters */
  register int regno;
  register int cregno;
  register int addr;
  int bad_reg = -1;
  int offset;
  struct user *tmp_uaddr;

  /* 
   * First get virtual address of user structure. Then calculate offset.
   */
  memcpy(&tmp_uaddr,
	 &((struct user *) core_reg_sect)->u_kproc.kp_proc.p_addr,
	 sizeof(tmp_uaddr));
  offset = -reg_addr - (int) tmp_uaddr;
  
  for (regno = 0; regno < NUM_REGS; regno++)
    {
      cregno = tregmap[regno];
      if (cregno == tFS)
        addr = offsetof (struct user, u_pcb) + offsetof (struct pcb, pcb_fs);
      else if (cregno == tGS)
        addr = offsetof (struct user, u_pcb) + offsetof (struct pcb, pcb_gs);
      else
        addr = offset + 4 * cregno;
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
      error ("Register %s not found in core file.", gdb_register_names[bad_reg]);
    }

  addr = offsetof (struct user, u_pcb) + offsetof (struct pcb, pcb_savefpu);
  memcpy (&pcb_savefpu, core_reg_sect + addr, sizeof pcb_savefpu);
#endif
}

int
kernel_u_size ()
{
  return (sizeof (struct user));
}


/* Register that we are able to handle aout (trad-core) file formats.  */

static struct core_fns aout_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_aout ()
{
  add_core_fns (&aout_core_fns);
}
