/* Target machine definitions for GDB for an embedded SPARC.
   Copyright 1996, 1997, 2000 Free Software Foundation, Inc.

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

#define TARGET_SPARCLET 1	/* Still needed for non-multi-arch case */

#include "sparc/tm-sparc.h"

/* Note: we are not defining GDB_MULTI_ARCH for the sparclet target
   at this time, because we have not figured out how to detect the
   sparclet target from the bfd structure.  */

/* Sparclet regs, for debugging purposes.  */

enum { 
  CCSR_REGNUM   = 72,
  CCPR_REGNUM   = 73, 
  CCCRCR_REGNUM = 74,
  CCOR_REGNUM   = 75, 
  CCOBR_REGNUM  = 76,
  CCIBR_REGNUM  = 77,
  CCIR_REGNUM   = 78
};

/* Select the sparclet disassembler.  Slightly different instruction set from
   the V8 sparc.  */

#undef TM_PRINT_INSN_MACH
#define TM_PRINT_INSN_MACH bfd_mach_sparc_sparclet

/* overrides of tm-sparc.h */

#undef TARGET_BYTE_ORDER

/* Sequence of bytes for breakpoint instruction (ta 1). */
#undef BREAKPOINT
#define BIG_BREAKPOINT {0x91, 0xd0, 0x20, 0x01}
#define LITTLE_BREAKPOINT {0x01, 0x20, 0xd0, 0x91}

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH.
 */

#undef  NUM_REGS		/* formerly "72" */
/*                WIN  FP   CPU  CCP  ASR  AWR  APSR */
#define NUM_REGS (32 + 32 + 8  + 8  + 8/*+ 32 + 1*/)

#undef  REGISTER_BYTES		/* formerly "(32*4 + 32*4 + 8*4)" */
#define REGISTER_BYTES (32*4 + 32*4 + 8*4 + 8*4 + 8*4/* + 32*4 + 1*4*/)

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */
/* Sparclet has no fp! */
/* Compiler maps types for floats by number, so can't 
   change the numbers here. */

#undef REGISTER_NAMES
#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",	\
  "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7",	\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",	\
  "i0", "i1", "i2", "i3", "i4", "i5", "i6", "i7",	\
							\
  "", "", "", "", "", "", "", "", /* no FPU regs */	\
  "", "", "", "", "", "", "", "", 			\
  "", "", "", "", "", "", "", "", 			\
  "", "", "", "", "", "", "", "", 			\
				  /* no CPSR, FPSR */	\
  "y", "psr", "wim", "tbr", "pc", "npc", "", "", 	\
							\
  "ccsr", "ccpr", "cccrcr", "ccor", "ccobr", "ccibr", "ccir", "", \
								  \
  /*       ASR15                 ASR19 (don't display them) */    \
  "asr1",  "", "asr17", "asr18", "", "asr20", "asr21", "asr22",   \
/*									  \
  "awr0",  "awr1",  "awr2",  "awr3",  "awr4",  "awr5",  "awr6",  "awr7",  \
  "awr8",  "awr9",  "awr10", "awr11", "awr12", "awr13", "awr14", "awr15", \
  "awr16", "awr17", "awr18", "awr19", "awr20", "awr21", "awr22", "awr23", \
  "awr24", "awr25", "awr26", "awr27", "awr28", "awr29", "awr30", "awr31", \
  "apsr",								  \
 */									  \
}

/* Remove FP dependant code which was defined in tm-sparc.h */
#undef	FP0_REGNUM		/* Floating point register 0 */
#undef  FPS_REGNUM		/* Floating point status register */
#undef 	CPS_REGNUM		/* Coprocessor status register */

/* sparclet register numbers */
#define CCSR_REGNUM 72

#undef EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)                       \
  {                                                                    \
    memcpy ((VALBUF),                                                  \
	    (char *)(REGBUF) + REGISTER_RAW_SIZE (O0_REGNUM) * 8 +     \
	    (TYPE_LENGTH(TYPE) >= REGISTER_RAW_SIZE (O0_REGNUM)        \
	     ? 0 : REGISTER_RAW_SIZE (O0_REGNUM) - TYPE_LENGTH(TYPE)), \
	    TYPE_LENGTH(TYPE));                                        \
  }
#undef STORE_RETURN_VALUE
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  {                                                                    \
    /* Other values are returned in register %o0.  */                  \
    write_register_bytes (REGISTER_BYTE (O0_REGNUM), (VALBUF),         \
			  TYPE_LENGTH (TYPE));                         \
  }

#endif /* GDB_MULTI_ARCH */

#undef PRINT_REGISTER_HOOK
#define PRINT_REGISTER_HOOK(regno)

/* Offsets into jmp_buf.  Not defined by Sun, but at least documented in a
   comment in <machine/setjmp.h>! */

#define JB_ELEMENT_SIZE 4	/* Size of each element in jmp_buf */

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_NPC 4
#define JB_PSR 5
#define JB_G1 6
#define JB_O0 7
#define JB_WBCNT 8

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int get_longjmp_target (CORE_ADDR *);

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
