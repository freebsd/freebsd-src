/* Target machine definitions for GDB on a Sequent Symmetry under ptx
   with Weitek 1167 and i387 support.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.
   Symmetry version by Jay Vosburgh (fubar@sequent.com).

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

#ifndef TM_PTX_H
#define TM_PTX_H 1

/* I don't know if this will work for cross-debugging, even if you do get
   a copy of the right include file.  */

#include <sys/reg.h>

#ifdef SEQUENT_PTX4
#include "i386/tm-i386v4.h"
#else /* !SEQUENT_PTX4 */
#include "i386/tm-i386v.h"
#endif

/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is 2
   on most implementations. Here we have to undo what tm-i386v.h gave
   us and restore the default. */

#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 2

/* Amount PC must be decremented by after a breakpoint.  This is often the
   number of bytes in BREAKPOINT but not always (such as now). */

#undef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 0

#if 0
 --- this code can't be used unless we know we are running native,
     since it uses host specific ptrace calls.
/* code for 80387 fpu.  Functions are from i386-dep.c, copied into
 * symm-dep.c.
 */
#define FLOAT_INFO { i386_float_info(); }
#endif

/* Number of machine registers */

#undef  NUM_REGS
#define NUM_REGS 49

/* Initializer for an array of names of registers.  There should be at least
   NUM_REGS strings in this initializer.  Any excess ones are simply ignored.
   The order of the first 8 registers must match the compiler's numbering
   scheme (which is the same as the 386 scheme) and also regmap in the various
   *-nat.c files. */

#undef  REGISTER_NAMES
#define REGISTER_NAMES { "eax",  "ecx",    "edx",  "ebx",  \
			 "esp",  "ebp",    "esi",  "edi",  \
			 "eip",  "eflags", "st0",  "st1",  \
			 "st2",  "st3",    "st4",  "st5",  \
			 "st6",  "st7",    "fp1",  "fp2",  \
			 "fp3",  "fp4",    "fp5",  "fp6",  \
			 "fp7",  "fp8",    "fp9",  "fp10", \
			 "fp11", "fp12",   "fp13", "fp14", \
			 "fp15", "fp16",   "fp17", "fp18", \
			 "fp19", "fp20",   "fp21", "fp22", \
			 "fp23", "fp24",   "fp25", "fp26", \
			 "fp27", "fp28",   "fp29", "fp30", \
			 "fp31" }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define EAX_REGNUM	0
#define ECX_REGNUM	1
#define EDX_REGNUM	2
#define EBX_REGNUM	3

#define ESP_REGNUM	4
#define EBP_REGNUM	5

#define ESI_REGNUM	6
#define EDI_REGNUM	7

#define EIP_REGNUM	8
#define EFLAGS_REGNUM	9

#define ST0_REGNUM	10
#define ST1_REGNUM	11
#define ST2_REGNUM	12
#define ST3_REGNUM	13

#define ST4_REGNUM	14
#define ST5_REGNUM	15
#define ST6_REGNUM	16
#define ST7_REGNUM	17

#define FP1_REGNUM 18		/* first 1167 register */
/* Get %fp2 - %fp31 by addition, since they are contiguous */

#undef  SP_REGNUM
#define SP_REGNUM ESP_REGNUM	/* Contains address of top of stack */
#undef  FP_REGNUM
#define FP_REGNUM EBP_REGNUM	/* Contains address of executing stack frame */
#undef  PC_REGNUM
#define PC_REGNUM EIP_REGNUM	/* Contains program counter */
#undef  PS_REGNUM
#define PS_REGNUM EFLAGS_REGNUM	/* Contains processor status */

/*
 * For ptx, this is a little bit bizarre, since the register block
 * is below the u area in memory.  This means that blockend here ends
 * up being negative (for the call from coredep.c) since the value in
 * u.u_ar0 will be less than KERNEL_U_ADDR (and coredep.c passes us
 * u.u_ar0 - KERNEL_U_ADDR in blockend).  Since we also define
 * FETCH_INFERIOR_REGISTERS (and supply our own functions for that),
 * the core file case will be the only use of this function.
 */

#define REGISTER_U_ADDR(addr, blockend, regno) \
{ (addr) = ptx_register_u_addr((blockend), (regno)); }

extern int
ptx_register_u_addr PARAMS ((int, int));

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  10 i*86 registers, 8 i387
   registers, and 31 Weitek 1167 registers */

#undef  REGISTER_BYTES
#define REGISTER_BYTES ((10 * 4) + (8 * 10) + (31 * 4))

/* Index within `registers' of the first byte of the space for register N. */

#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) 		\
(((N) < ST0_REGNUM) ? ((N) * 4) : \
 ((N) < FP1_REGNUM) ? (40 + (((N) - ST0_REGNUM) * 10)) : \
 (40 + 80 + (((N) - FP1_REGNUM) * 4)))

/* Number of bytes of storage in the actual machine representation for
   register N.  All registers are 4 bytes, except 387 st(0) - st(7),
   which are 80 bits each. */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) \
(((N) < ST0_REGNUM) ? 4 : \
 ((N) < FP1_REGNUM) ? 10 : \
 4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#undef  MAX_REGISTER_RAW_SIZE
#define MAX_REGISTER_RAW_SIZE 10

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#undef REGISTER_CONVERTIBLE
#define REGISTER_CONVERTIBLE(N) \
((N < ST0_REGNUM) ? 0 : \
 (N < FP1_REGNUM) ? 1 : \
 0)
  
/* Convert data from raw format for register REGNUM
   to virtual format for register REGNUM.  */
extern const struct floatformat floatformat_i387_ext; /* from floatformat.h */

#undef REGISTER_CONVERT_TO_VIRTUAL
#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO)	\
((REGNUM < ST0_REGNUM) ?  (void)memcpy ((TO), (FROM), 4) : \
 (REGNUM < FP1_REGNUM) ? (void)floatformat_to_double(&floatformat_i387_ext, \
						       (FROM),(TO)) : \
 (void)memcpy ((TO), (FROM), 4))
 
/* Convert data from virtual format for register REGNUM
   to raw format for register REGNUM.  */

#undef REGISTER_CONVERT_TO_RAW
#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO)	\
((REGNUM < ST0_REGNUM) ?  (void)memcpy ((TO), (FROM), 4) : \
 (REGNUM < FP1_REGNUM) ? (void)floatformat_from_double(&floatformat_i387_ext, \
						       (FROM),(TO)) : \
 (void)memcpy ((TO), (FROM), 4))

/* Return the GDB type object for the "standard" data type
   of data in register N.  */
/*
 * Note: the 1167 registers (the last line, builtin_type_float) are
 * generally used in pairs, with each pair being treated as a double.
 * It it also possible to use them singly as floats.  I'm not sure how
 * in gdb to treat the register pair pseudo-doubles. -fubar
 */
#undef REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
((N < ST0_REGNUM) ? builtin_type_int : \
 (N < FP1_REGNUM) ? builtin_type_double : \
 builtin_type_float)

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#undef  EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  symmetry_extract_return_value(TYPE, REGBUF, VALBUF)

/*
#undef  FRAME_FIND_SAVED_REGS
#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs) \
{ ptx_frame_find_saved_regs((frame_info), &(frame_saved_regs)); }
*/

#endif  /* ifndef TM_PTX_H */
