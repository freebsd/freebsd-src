/* Target machine definitions for GDB on a Sequent Symmetry under dynix 3.0,
   with Weitek 1167 and i387 support.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994
   Free Software Foundation, Inc.
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

#ifndef TM_SYMMETRY_H
#define TM_SYMMETRY_H 1

/* I don't know if this will work for cross-debugging, even if you do get
   a copy of the right include file.  */
#include <machine/reg.h>

#include "i386/tm-i386v.h"

#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 2

/* Amount PC must be decremented by after a breakpoint.  This is often the
   number of bytes in BREAKPOINT but not always (such as now). */

#undef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 0

#if 0
/* --- this code can't be used unless we know we are running native,
       since it uses host specific ptrace calls. */
/* code for 80387 fpu.  Functions are from i386-dep.c, copied into
 * symm-dep.c.
 */
#define FLOAT_INFO { i386_float_info(); }
#endif

/* Number of machine registers */

#undef NUM_REGS
#define NUM_REGS 49

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

/* Initializer for an array of names of registers.  There should be at least
   NUM_REGS strings in this initializer.  Any excess ones are simply ignored.
   Symmetry registers are in this weird order to match the register numbers
   in the symbol table entries.  If you change the order, things will probably
   break mysteriously for no apparent reason.  Also note that the st(0)...
   st(7) 387 registers are represented as st0...st7.  */

#undef  REGISTER_NAMES
#define REGISTER_NAMES {     "eax",  "edx",  "ecx",   "st0",  "st1", \
			     "ebx",  "esi",  "edi",   "st2",  "st3", \
			     "st4",  "st5",  "st6",   "st7",  "esp", \
			     "ebp",  "eip",  "eflags","fp1",  "fp2", \
			     "fp3",  "fp4",  "fp5",   "fp6",  "fp7", \
			     "fp8",  "fp9",  "fp10",  "fp11", "fp12", \
			     "fp13", "fp14", "fp15",  "fp16", "fp17", \
			     "fp18", "fp19", "fp20",  "fp21", "fp22", \
			     "fp23", "fp24", "fp25",  "fp26", "fp27", \
			     "fp28", "fp29", "fp30",  "fp31" }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define EAX_REGNUM	0
#define EDX_REGNUM	1
#define ECX_REGNUM	2
#define ST0_REGNUM	3
#define ST1_REGNUM	4
#define EBX_REGNUM	5
#define ESI_REGNUM	6
#define EDI_REGNUM	7
#define ST2_REGNUM	8
#define ST3_REGNUM	9

#define ST4_REGNUM	10
#define ST5_REGNUM	11
#define ST6_REGNUM	12
#define ST7_REGNUM	13

#define FP1_REGNUM 18		/* first 1167 register */
/* Get %fp2 - %fp31 by addition, since they are contiguous */

#undef  SP_REGNUM
#define SP_REGNUM 14	/* (usp) Contains address of top of stack */
#define ESP_REGNUM 14
#undef  FP_REGNUM
#define FP_REGNUM 15	/* (ebp) Contains address of executing stack frame */
#define EBP_REGNUM 15
#undef  PC_REGNUM
#define PC_REGNUM 16	/* (eip) Contains program counter */
#define EIP_REGNUM 16
#undef  PS_REGNUM
#define PS_REGNUM 17	/* (ps)  Contains processor status */
#define EFLAGS_REGNUM 17

/*
 * Following macro translates i386 opcode register numbers to Symmetry
 * register numbers.  This is used by i386_frame_find_saved_regs.
 *
 *           %eax  %ecx  %edx  %ebx  %esp  %ebp  %esi  %edi
 * i386        0     1     2     3     4     5     6     7
 * Symmetry    0     2     1     5    14    15     6     7
 *
 */
#define I386_REGNO_TO_SYMMETRY(n) \
((n)==0?0 :(n)==1?2 :(n)==2?1 :(n)==3?5 :(n)==4?14 :(n)==5?15 :(n))

/* The magic numbers below are offsets into u_ar0 in the user struct.
 * They live in <machine/reg.h>.  Gdb calls this macro with blockend
 * holding u.u_ar0 - KERNEL_U_ADDR.  Only the registers listed are
 * saved in the u area (along with a few others that aren't useful
 * here.  See <machine/reg.h>).
 */

#define REGISTER_U_ADDR(addr, blockend, regno) \
{ struct user foo;	/* needed for finding fpu regs */ \
switch (regno) { \
    case 0: \
      addr = blockend + EAX * sizeof(int); break; \
  case 1: \
      addr = blockend + EDX * sizeof(int); break; \
  case 2: \
      addr = blockend + ECX * sizeof(int); break; \
  case 3:			/* st(0) */ \
      addr = ((int)&foo.u_fpusave.fpu_stack[0][0] - (int)&foo); \
      break; \
  case 4:			/* st(1) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[1][0] - (int)&foo); \
      break; \
  case 5: \
      addr = blockend + EBX * sizeof(int); break; \
  case 6: \
      addr = blockend + ESI * sizeof(int); break; \
  case 7: \
      addr = blockend + EDI * sizeof(int); break; \
  case 8:			/* st(2) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[2][0] - (int)&foo); \
      break; \
  case 9:			/* st(3) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[3][0] - (int)&foo); \
      break; \
  case 10:			/* st(4) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[4][0] - (int)&foo); \
      break; \
  case 11:			/* st(5) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[5][0] - (int)&foo); \
      break; \
  case 12:			/* st(6) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[6][0] - (int)&foo); \
      break; \
  case 13:			/* st(7) */ \
      addr = ((int) &foo.u_fpusave.fpu_stack[7][0] - (int)&foo); \
      break; \
  case 14: \
      addr = blockend + ESP * sizeof(int); break; \
  case 15: \
      addr = blockend + EBP * sizeof(int); break; \
  case 16: \
      addr = blockend + EIP * sizeof(int); break; \
  case 17: \
      addr = blockend + FLAGS * sizeof(int); break; \
  case 18:			/* fp1 */ \
  case 19:			/* fp2 */ \
  case 20:			/* fp3 */ \
  case 21:			/* fp4 */ \
  case 22:			/* fp5 */ \
  case 23:			/* fp6 */ \
  case 24:			/* fp7 */ \
  case 25:			/* fp8 */ \
  case 26:			/* fp9 */ \
  case 27:			/* fp10 */ \
  case 28:			/* fp11 */ \
  case 29:			/* fp12 */ \
  case 30:			/* fp13 */ \
  case 31:			/* fp14 */ \
  case 32:			/* fp15 */ \
  case 33:			/* fp16 */ \
  case 34:			/* fp17 */ \
  case 35:			/* fp18 */ \
  case 36:			/* fp19 */ \
  case 37:			/* fp20 */ \
  case 38:			/* fp21 */ \
  case 39:			/* fp22 */ \
  case 40:			/* fp23 */ \
  case 41:			/* fp24 */ \
  case 42:			/* fp25 */ \
  case 43:			/* fp26 */ \
  case 44:			/* fp27 */ \
  case 45:			/* fp28 */ \
  case 46:			/* fp29 */ \
  case 47:			/* fp30 */ \
  case 48:			/* fp31 */ \
     addr = ((int) &foo.u_fpasave.fpa_regs[(regno)-18] - (int)&foo); \
  } \
}

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  10 i*86 registers, 8 i387
   registers, and 31 Weitek 1167 registers */

#undef  REGISTER_BYTES
#define REGISTER_BYTES ((10 * 4) + (8 * 10) + (31 * 4))

/* Index within `registers' of the first byte of the space for
   register N.  */

#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) 		\
(((N) < 3) ? ((N) * 4) :		\
((N) < 5) ? ((((N) - 2) * 10) + 2) :	\
((N) < 8) ? ((((N) - 5) * 4) + 32) :	\
((N) < 14) ? ((((N) - 8) * 10) + 44) :	\
    ((((N) - 14) * 4) + 104))

/* Number of bytes of storage in the actual machine representation
 * for register N.  All registers are 4 bytes, except 387 st(0) - st(7),
 * which are 80 bits each. 
 */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) \
(((N) < 3) ? 4 :	\
((N) < 5) ? 10 :	\
((N) < 8) ? 4 :		\
((N) < 14) ? 10 :	\
    4)

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#undef  REGISTER_CONVERTIBLE
#define REGISTER_CONVERTIBLE(N) \
(((N) < 3) ? 0 : \
((N) < 5) ? 1  : \
((N) < 8) ? 0  : \
((N) < 14) ? 1 : \
    0)

#include "floatformat.h"

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#undef REGISTER_CONVERT_TO_VIRTUAL
#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double val; \
  floatformat_to_double (&floatformat_i387_ext, (FROM), &val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), val); \
}

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#undef REGISTER_CONVERT_TO_RAW
#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO) \
{ \
  double val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  floatformat_from_double (&floatformat_i387_ext, &val, (TO)); \
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#undef REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
((N < 3) ? builtin_type_int : \
(N < 5) ? builtin_type_double : \
(N < 8) ? builtin_type_int : \
(N < 14) ? builtin_type_double : \
    builtin_type_int)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function.
   Native cc passes the address in eax, gcc (up to version 2.5.8)
   passes it on the stack.  gcc should be fixed in future versions to
   adopt native cc conventions.  */

#undef  STORE_STRUCT_RETURN
#define STORE_STRUCT_RETURN(ADDR, SP) write_register(0, (ADDR))

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#undef  EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  symmetry_extract_return_value(TYPE, REGBUF, VALBUF)

/* The following redefines make backtracing through sigtramp work.
   They manufacture a fake sigtramp frame and obtain the saved pc in sigtramp
   from the sigcontext structure which is pushed by the kernel on the
   user stack, along with a pointer to it.  */

#define IN_SIGTRAMP(pc, name) ((name) && STREQ ("_sigcode", name))

/* Offset to saved PC in sigcontext, from <signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 16

#endif	/* ifndef TM_SYMMETRY_H */

