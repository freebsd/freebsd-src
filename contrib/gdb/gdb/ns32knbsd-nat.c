/* Functions specific to running gdb native on an ns32k running NetBSD
   Copyright 1989, 1992, 1993, 1994, 1996 Free Software Foundation, Inc.

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
#include <machine/pcb.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

#define RF(dst, src) \
	memcpy(&registers[REGISTER_BYTE(dst)], &src, sizeof(src))

#define RS(src, dst) \
	memcpy(&dst, &registers[REGISTER_BYTE(src)], sizeof(dst))

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fpregisters;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  ptrace (PT_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fpregisters, 0);

  RF(R0_REGNUM + 0, inferior_registers.r_r0);
  RF(R0_REGNUM + 1, inferior_registers.r_r1);
  RF(R0_REGNUM + 2, inferior_registers.r_r2);
  RF(R0_REGNUM + 3, inferior_registers.r_r3);
  RF(R0_REGNUM + 4, inferior_registers.r_r4);
  RF(R0_REGNUM + 5, inferior_registers.r_r5);
  RF(R0_REGNUM + 6, inferior_registers.r_r6);
  RF(R0_REGNUM + 7, inferior_registers.r_r7);

  RF(SP_REGNUM	  , inferior_registers.r_sp);
  RF(FP_REGNUM	  , inferior_registers.r_fp);
  RF(PC_REGNUM	  , inferior_registers.r_pc);
  RF(PS_REGNUM	  , inferior_registers.r_psr);

  RF(FPS_REGNUM   , inferior_fpregisters.r_fsr);
  RF(FP0_REGNUM +0, inferior_fpregisters.r_freg[0]);
  RF(FP0_REGNUM +2, inferior_fpregisters.r_freg[2]);
  RF(FP0_REGNUM +4, inferior_fpregisters.r_freg[4]);
  RF(FP0_REGNUM +6, inferior_fpregisters.r_freg[6]);
  RF(LP0_REGNUM + 1, inferior_fpregisters.r_freg[1]);
  RF(LP0_REGNUM + 3, inferior_fpregisters.r_freg[3]);
  RF(LP0_REGNUM + 5, inferior_fpregisters.r_freg[5]);
  RF(LP0_REGNUM + 7, inferior_fpregisters.r_freg[7]);
   registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fpregisters;

  RS(R0_REGNUM + 0, inferior_registers.r_r0);
  RS(R0_REGNUM + 1, inferior_registers.r_r1);
  RS(R0_REGNUM + 2, inferior_registers.r_r2);
  RS(R0_REGNUM + 3, inferior_registers.r_r3);
  RS(R0_REGNUM + 4, inferior_registers.r_r4);
  RS(R0_REGNUM + 5, inferior_registers.r_r5);
  RS(R0_REGNUM + 6, inferior_registers.r_r6);
  RS(R0_REGNUM + 7, inferior_registers.r_r7);

  RS(SP_REGNUM	  , inferior_registers.r_sp);
  RS(FP_REGNUM	  , inferior_registers.r_fp);
  RS(PC_REGNUM	  , inferior_registers.r_pc);
  RS(PS_REGNUM	  , inferior_registers.r_psr);

  RS(FPS_REGNUM   , inferior_fpregisters.r_fsr);
  RS(FP0_REGNUM +0, inferior_fpregisters.r_freg[0]);
  RS(FP0_REGNUM +2, inferior_fpregisters.r_freg[2]);
  RS(FP0_REGNUM +4, inferior_fpregisters.r_freg[4]);
  RS(FP0_REGNUM +6, inferior_fpregisters.r_freg[6]);
  RS(LP0_REGNUM + 1, inferior_fpregisters.r_freg[1]);
  RS(LP0_REGNUM + 3, inferior_fpregisters.r_freg[3]);
  RS(LP0_REGNUM + 5, inferior_fpregisters.r_freg[5]);
  RS(LP0_REGNUM + 7, inferior_fpregisters.r_freg[7]);

  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  ptrace (PT_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fpregisters, 0);
}


/* XXX - Add this to machine/regs.h instead? */
struct coreregs {
  struct reg intreg;
  struct fpreg freg;
};

/* Get registers from a core file. */
static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  struct coreregs *core_reg;

  core_reg = (struct coreregs *)core_reg_sect;

  /*
   * We have *all* registers
   * in the first core section.
   * Ignore which.
   */

  if (core_reg_size < sizeof(*core_reg)) {
    fprintf_unfiltered (gdb_stderr, "Couldn't read regs from core file\n");
    return;
  }

  /* Integer registers */
  RF(R0_REGNUM + 0, core_reg->intreg.r_r0);
  RF(R0_REGNUM + 1, core_reg->intreg.r_r1);
  RF(R0_REGNUM + 2, core_reg->intreg.r_r2);
  RF(R0_REGNUM + 3, core_reg->intreg.r_r3);
  RF(R0_REGNUM + 4, core_reg->intreg.r_r4);
  RF(R0_REGNUM + 5, core_reg->intreg.r_r5);
  RF(R0_REGNUM + 6, core_reg->intreg.r_r6);
  RF(R0_REGNUM + 7, core_reg->intreg.r_r7);

  RF(SP_REGNUM	  , core_reg->intreg.r_sp);
  RF(FP_REGNUM	  , core_reg->intreg.r_fp);
  RF(PC_REGNUM	  , core_reg->intreg.r_pc);
  RF(PS_REGNUM	  , core_reg->intreg.r_psr);

  /* Floating point registers */
  RF(FPS_REGNUM   , core_reg->freg.r_fsr);
  RF(FP0_REGNUM +0, core_reg->freg.r_freg[0]);
  RF(FP0_REGNUM +2, core_reg->freg.r_freg[2]);
  RF(FP0_REGNUM +4, core_reg->freg.r_freg[4]);
  RF(FP0_REGNUM +6, core_reg->freg.r_freg[6]);
  RF(LP0_REGNUM + 1, core_reg->freg.r_freg[1]);
  RF(LP0_REGNUM + 3, core_reg->freg.r_freg[3]);
  RF(LP0_REGNUM + 5, core_reg->freg.r_freg[5]);
  RF(LP0_REGNUM + 7, core_reg->freg.r_freg[7]);
  registers_fetched ();
}

/* Register that we are able to handle ns32knbsd core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns nat_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_ns32knbsd_nat ()
{
  add_core_fns (&nat_core_fns);
}


/*
 * kernel_u_size() is not helpful on NetBSD because
 * the "u" struct is NOT in the core dump file.
 */

#ifdef	FETCH_KCORE_REGISTERS
/*
 * Get registers from a kernel crash dump or live kernel.
 * Called by kcore-nbsd.c:get_kcore_registers().
 */
void
fetch_kcore_registers (pcb)
     struct pcb *pcb;
{
  struct switchframe sf;
  struct reg intreg;
  int dummy;

  /* Integer registers */
  if (target_read_memory((CORE_ADDR)pcb->pcb_ksp, (char *)&sf, sizeof sf))
     error("Cannot read integer registers.");

  /* We use the psr at kernel entry */
  if (target_read_memory((CORE_ADDR)pcb->pcb_onstack, (char *)&intreg, sizeof intreg))
     error("Cannot read processor status register.");

  dummy = 0;
  RF(R0_REGNUM + 0, dummy);
  RF(R0_REGNUM + 1, dummy);
  RF(R0_REGNUM + 2, dummy);
  RF(R0_REGNUM + 3, sf.sf_r3);
  RF(R0_REGNUM + 4, sf.sf_r4);
  RF(R0_REGNUM + 5, sf.sf_r5);
  RF(R0_REGNUM + 6, sf.sf_r6);
  RF(R0_REGNUM + 7, sf.sf_r7);

  dummy = pcb->pcb_kfp + 8;
  RF(SP_REGNUM	  , dummy);
  RF(FP_REGNUM	  , sf.sf_fp);
  RF(PC_REGNUM	  , sf.sf_pc);
  RF(PS_REGNUM	  , intreg.r_psr);

  /* Floating point registers */
  RF(FPS_REGNUM   , pcb->pcb_fsr);
  RF(FP0_REGNUM +0, pcb->pcb_freg[0]);
  RF(FP0_REGNUM +2, pcb->pcb_freg[2]);
  RF(FP0_REGNUM +4, pcb->pcb_freg[4]);
  RF(FP0_REGNUM +6, pcb->pcb_freg[6]);
  RF(LP0_REGNUM + 1, pcb->pcb_freg[1]);
  RF(LP0_REGNUM + 3, pcb->pcb_freg[3]);
  RF(LP0_REGNUM + 5, pcb->pcb_freg[5]);
  RF(LP0_REGNUM + 7, pcb->pcb_freg[7]);
  registers_fetched ();
}
#endif	/* FETCH_KCORE_REGISTERS */

void
clear_regs()
{
  double zero = 0.0;
  int null = 0;
  
  /* Integer registers */
  RF(R0_REGNUM + 0, null);
  RF(R0_REGNUM + 1, null);
  RF(R0_REGNUM + 2, null);
  RF(R0_REGNUM + 3, null);
  RF(R0_REGNUM + 4, null);
  RF(R0_REGNUM + 5, null);
  RF(R0_REGNUM + 6, null);
  RF(R0_REGNUM + 7, null);

  RF(SP_REGNUM	  , null);
  RF(FP_REGNUM	  , null);
  RF(PC_REGNUM	  , null);
  RF(PS_REGNUM	  , null);

  /* Floating point registers */
  RF(FPS_REGNUM   , zero);
  RF(FP0_REGNUM +0, zero);
  RF(FP0_REGNUM +2, zero);
  RF(FP0_REGNUM +4, zero);
  RF(FP0_REGNUM +6, zero);
  RF(LP0_REGNUM + 0, zero);
  RF(LP0_REGNUM + 1, zero);
  RF(LP0_REGNUM + 2, zero);
  RF(LP0_REGNUM + 3, zero);
  return;
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell. */

int
frame_num_args(fi)
struct frame_info *fi;
{
	CORE_ADDR	enter_addr;
	CORE_ADDR	argp;
	int		inst;
	int		args;
	int		i;

	if (read_memory_integer (fi->frame, 4) == 0 && fi->pc < 0x10000) {
		/* main is always called with three args */
		return(3);
	}
	enter_addr = ns32k_get_enter_addr(fi->pc);
	if (enter_addr = 0)
		return(-1);
	argp = enter_addr == 1 ? SAVED_PC_AFTER_CALL(fi) : FRAME_SAVED_PC(fi);
	for (i = 0; i < 16; i++) {
		/*
		 * After a bsr gcc may emit the following instructions
		 * to remove the arguments from the stack:
		 *   cmpqd 0,tos 	- to remove 4 bytes from the stack
		 *   cmpd tos,tos	- to remove 8 bytes from the stack
		 *   adjsp[bwd] -n	- to remove n bytes from the stack
		 * Gcc sometimes delays emitting these instructions and
		 * may even throw a branch between our feet.
		 */		 
		inst = read_memory_integer(argp    , 4);
		args = read_memory_integer(argp + 2, 4);
		if ((inst & 0xff) == 0xea) {		/* br */
			args = ((inst >> 8) & 0xffffff) | (args << 24);
			if (args & 0x80) {
				if (args & 0x40) {
					args = ntohl(args);
				} else {
					args = ntohs(args & 0xffff);
					if (args & 0x2000)
						args |= 0xc000;
				}
			} else {
				args = args & 0xff;
				if (args & 0x40)
					args |= 0x80;
			}
			argp += args;
			continue;
		}
		if ((inst & 0xffff) == 0xb81f)		/* cmpqd 0,tos */
			return(1);
		else if ((inst & 0xffff) == 0xbdc7)	/* cmpd tos,tos */
			return(2);
		else if ((inst & 0xfffc) == 0xa57c) {	/* adjsp[bwd] */
			switch (inst & 3) {
			case 0:
				args = ((args & 0xff) + 0x80);
				break;
			case 1:
				args = ((ntohs(args) & 0xffff) + 0x8000);
				break;
			case 3:
				args = -ntohl(args);
				break;
			default:
				return(-1);
			}
			if (args / 4 > 10 || (args & 3) != 0)
				continue;
			return(args / 4);
		}
		argp += 1;
	}
	return(-1);
}
