/* Macro definitions for GDB for a Fujitsu SPARClite.
   Copyright 1993, 1994, 1995, 1998, 1999, 2000
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

#include "regcache.h"

#define TARGET_SPARCLITE 1	/* Still needed for non-multi-arch case */

#include "sparc/tm-sparc.h"

/* Note: we are not defining GDB_MULTI_ARCH for the sparclet target
   at this time, because we have not figured out how to detect the
   sparclet target from the bfd structure.  */

/* Sparclite regs, for debugging purposes */

enum {
  DIA1_REGNUM = 72,		/* debug instr address register 1 */
  DIA2_REGNUM = 73,		/* debug instr address register 2 */
  DDA1_REGNUM = 74,		/* debug data address register 1 */
  DDA2_REGNUM = 75,		/* debug data address register 2 */
  DDV1_REGNUM = 76,		/* debug data value register 1 */
  DDV2_REGNUM = 77,		/* debug data value register 2 */
  DCR_REGNUM  = 78,		/* debug control register */
  DSR_REGNUM  = 79		/* debug status regsiter */
};

/* overrides of tm-sparc.h */

#undef TARGET_BYTE_ORDER

/* Select the sparclite disassembler.  Slightly different instruction set from
   the V8 sparc.  */

#undef TM_PRINT_INSN_MACH
#define TM_PRINT_INSN_MACH bfd_mach_sparc_sparclite

/* Amount PC must be decremented by after a hardware instruction breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_HW_BREAK 4

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH.
 */

#undef  FRAME_CHAIN_VALID
#define FRAME_CHAIN_VALID(FP,FI) func_frame_chain_valid (FP, FI)

#undef NUM_REGS
#define NUM_REGS 80

#undef REGISTER_BYTES
#define REGISTER_BYTES (32*4+32*4+8*4+8*4)

#undef REGISTER_NAMES
#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",       \
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",       \
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",       \
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",       \
                                                                \
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",       \
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15", \
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",       \
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",       \
                                                                \
  "y", "psr", "wim", "tbr", "pc", "npc", "fpsr", "cpsr",        \
  "dia1", "dia2", "dda1", "dda2", "ddv1", "ddv2", "dcr", "dsr" }

#define DIA1_REGNUM 72		/* debug instr address register 1 */
#define DIA2_REGNUM 73		/* debug instr address register 2 */
#define DDA1_REGNUM 74		/* debug data address register 1 */
#define DDA2_REGNUM 75		/* debug data address register 2 */
#define DDV1_REGNUM 76		/* debug data value register 1 */
#define DDV2_REGNUM 77		/* debug data value register 2 */
#define DCR_REGNUM 78		/* debug control register */
#define DSR_REGNUM 79		/* debug status regsiter */

#endif /* GDB_MULTI_ARCH */

#define TARGET_HW_BREAK_LIMIT 2
#define TARGET_HW_WATCH_LIMIT 2

/* Enable watchpoint macro's */

#define TARGET_HAS_HARDWARE_WATCHPOINTS

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
	sparclite_check_watch_resources (type, cnt, ot)

/* When a hardware watchpoint fires off the PC will be left at the
   instruction which caused the watchpoint.  It will be necessary for
   GDB to step over the watchpoint. ***

   #define STOPPED_BY_WATCHPOINT(W) \
   ((W).kind == TARGET_WAITKIND_STOPPED \
   && (W).value.sig == TARGET_SIGNAL_TRAP \
   && ((int) read_register (IPSW_REGNUM) & 0x00100000))
 */

/* Use these macros for watchpoint insertion/deletion.  */
#define target_insert_watchpoint(addr, len, type) sparclite_insert_watchpoint (addr, len, type)
#define target_remove_watchpoint(addr, len, type) sparclite_remove_watchpoint (addr, len, type)
#define target_insert_hw_breakpoint(addr, len) sparclite_insert_hw_breakpoint (addr, len)
#define target_remove_hw_breakpoint(addr, len) sparclite_remove_hw_breakpoint (addr, len)
#define target_stopped_data_address() sparclite_stopped_data_address()
