/* Target machine description for SGI Iris under Irix, for GDB.
   Copyright 1990, 1991, 1992, 1993, 1995 Free Software Foundation, Inc.

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

#include "mips/tm-bigmips.h"

/* SGI's assembler doesn't grok dollar signs in identifiers.
   So we use dots instead.  This item must be coordinated with G++. */
#undef CPLUS_MARKER
#define CPLUS_MARKER '.'

/* Redefine register numbers for SGI. */

#undef NUM_REGS
#undef REGISTER_NAMES
#undef FP0_REGNUM
#undef PC_REGNUM
#undef PS_REGNUM
#undef HI_REGNUM
#undef LO_REGNUM
#undef CAUSE_REGNUM
#undef BADVADDR_REGNUM
#undef FCRCS_REGNUM
#undef FCRIR_REGNUM

/* Number of machine registers */

#define NUM_REGS 71

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES 	\
    {	"zero",	"at",	"v0",	"v1",	"a0",	"a1",	"a2",	"a3", \
	"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6",	"t7", \
	"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7", \
	"t8",	"t9",	"k0",	"k1",	"gp",	"sp",	"fp",	"ra", \
	"f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7", \
	"f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15", \
	"f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",\
	"f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",\
	"pc",	"cause", "bad",	"hi",	"lo",	"fsr",  "fir" \
    }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define FP0_REGNUM 32		/* Floating point register 0 (single float) */
#define PC_REGNUM 64		/* Contains program counter */
#define CAUSE_REGNUM 65		/* describes last exception */
#define BADVADDR_REGNUM 66	/* bad vaddr for addressing exception */
#define HI_REGNUM 67		/* Multiple/divide temp */
#define LO_REGNUM 68		/* ... */
#define FCRCS_REGNUM 69		/* FP control/status */
#define FCRIR_REGNUM 70		/* FP implementation/revision */

/* Offsets for register values in _sigtramp frame.
   sigcontext is immediately above the _sigtramp frame on Irix.  */
#define SIGFRAME_BASE		0x0 
#define SIGFRAME_PC_OFF		(SIGFRAME_BASE + 2 * 4)
#define SIGFRAME_REGSAVE_OFF	(SIGFRAME_BASE + 3 * 4)
#define SIGFRAME_FPREGSAVE_OFF	(SIGFRAME_BASE + 3 * 4 + 32 * 4 + 4)
