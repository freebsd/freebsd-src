/* Target-dependent code for GDB, the GNU debugger.
   Copyright 2000
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

#ifndef PPC_TDEP_H
#define PPC_TDEP_H

struct frame_info;
struct value;

/* From ppc-linux-tdep.c... */
CORE_ADDR ppc_linux_frame_saved_pc (struct frame_info *fi);
void ppc_linux_init_extra_frame_info (int fromleaf, struct frame_info *);
int ppc_linux_frameless_function_invocation (struct frame_info *);
void ppc_linux_frame_init_saved_regs (struct frame_info *);
CORE_ADDR ppc_linux_frame_chain (struct frame_info *);
CORE_ADDR ppc_sysv_abi_push_arguments (int, struct value **, CORE_ADDR, int,
				       CORE_ADDR);
int ppc_linux_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache);
struct link_map_offsets *ppc_linux_svr4_fetch_link_map_offsets (void);


/* From rs6000-tdep.c... */
CORE_ADDR rs6000_frame_saved_pc (struct frame_info *fi);
void rs6000_init_extra_frame_info (int fromleaf, struct frame_info *);
int rs6000_frameless_function_invocation (struct frame_info *);
void rs6000_frame_init_saved_regs (struct frame_info *);
CORE_ADDR rs6000_frame_chain (struct frame_info *);
int altivec_register_p (int regno);

/* Private data that this module attaches to struct gdbarch. */

struct gdbarch_tdep
  {
    int wordsize;              /* size in bytes of fixed-point word */
    int osabi;                 /* OS / ABI from ELF header */
    int *regoff;               /* byte offsets in register arrays */
    const struct reg *regs;    /* from current variant */
    int ppc_gp0_regnum;		/* GPR register 0 */
    int ppc_gplast_regnum;	/* GPR register 31 */
    int ppc_toc_regnum;		/* TOC register */
    int ppc_ps_regnum;	        /* Processor (or machine) status (%msr) */
    int ppc_cr_regnum;		/* Condition register */
    int ppc_lr_regnum;		/* Link register */
    int ppc_ctr_regnum;		/* Count register */
    int ppc_xer_regnum;		/* Integer exception register */
    int ppc_mq_regnum;		/* Multiply/Divide extension register */
    int ppc_vr0_regnum;		/* First AltiVec register */
    int ppc_vrsave_regnum;	/* Last AltiVec register */
};

#endif
