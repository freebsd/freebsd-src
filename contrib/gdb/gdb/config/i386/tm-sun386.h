/* Parameters for a Sun 386i target machine, for GDB, the GNU debugger.
   Copyright 1986, 1987, 1991, 1992, 1993 Free Software Foundation, Inc.

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

#if !defined (TM_SUN386_H)
#define TM_SUN386_H 1

#include "i386/tm-i386.h"

#ifndef sun386
#define sun386
#endif
#define GDB_TARGET_IS_SUN386 1
#define SUNOS4
#define USE_MACHINE_REG_H

/* Perhaps some day this will work even without the following #define */
#define COFF_ENCAPSULATE

#ifdef COFF_ENCAPSULATE
/* Avoid conflicts between our include files and <sys/exec.h>
   (maybe not needed anymore).  */
#define _EXEC_
#endif

/* sun386 ptrace seems unable to change the frame pointer */
#define PTRACE_FP_BUG

/* Address of end of stack space.  */

#define STACK_END_ADDR 0xfc000000

/* Number of machine registers */

#undef  NUM_REGS
#define NUM_REGS 35

/* Initializer for an array of names of registers.  There should be NUM_REGS
   strings in this initializer.  The order of the first 8 registers must match
   the compiler's numbering scheme (which is the same as the 386 scheme) also,
   this table must match regmap in i386-pinsn.c. */

#undef  REGISTER_NAMES
#define REGISTER_NAMES { "gs", "fs", "es", "ds",		\
			 "edi", "esi", "ebp", "esp",		\
			 "ebx", "edx", "ecx", "eax",		\
			 "retaddr", "trapnum", "errcode", "ip",	\
			 "cs", "ps", "sp", "ss",		\
			 "fst0", "fst1", "fst2", "fst3",	\
			 "fst4", "fst5", "fst6", "fst7",	\
			 "fctrl", "fstat", "ftag", "fip",	\
			 "fcs", "fopoff", "fopsel"		\
			 }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#undef  FP_REGNUM
#define FP_REGNUM   6	/* (ebp) Contains address of executing stack frame */
#undef  SP_REGNUM
#define SP_REGNUM  18	/* (usp) Contains address of top of stack */
#undef  PS_REGNUM
#define PS_REGNUM  17	/* (ps)  Contains processor status */
#undef  PC_REGNUM
#define PC_REGNUM  15	/* (eip) Contains program counter */
#undef  FP0_REGNUM
#define FP0_REGNUM 20	/* Floating point register 0 */
#undef  FPC_REGNUM
#define FPC_REGNUM 28	/* 80387 control register */

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */

#undef  REGISTER_BYTES
#define REGISTER_BYTES (20*4+8*10+7*4)

/* Index within `registers' of the first byte of the space for
   register N.  */

#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) \
 ((N) >= FPC_REGNUM ? (((N) - FPC_REGNUM) * 4) + 160	\
  : (N) >= FP0_REGNUM ? (((N) - FP0_REGNUM) * 10) + 80	\
  : (N) * 4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) (((unsigned)((N) - FP0_REGNUM)) < 8 ? 10 : 4)

/* Number of bytes of storage in the program's representation
   for register N. */

#undef  REGISTER_VIRTUAL_SIZE
#define REGISTER_VIRTUAL_SIZE(N) (((unsigned)((N) - FP0_REGNUM)) < 8 ? 8 : 4)

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#undef  REGISTER_CONVERTIBLE
#define REGISTER_CONVERTIBLE(N) (((unsigned)((N) - FP0_REGNUM)) < 8)

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#undef  REGISTER_CONVERT_TO_VIRTUAL
#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double val; \
  i387_to_double ((FROM), (char *)&val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), val); \
}
extern void
i387_to_double PARAMS ((char *, char *));

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#undef  REGISTER_CONVERT_TO_RAW
#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO) \
{ \
  double val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  double_to_i387((char *)&val, (TO)); \
}
extern void
double_to_i387 PARAMS ((char *, char *));

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#undef  REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
 (((unsigned)((N) - FP0_REGNUM)) < 8 ? builtin_type_double : builtin_type_int)

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#undef  EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy (VALBUF, REGBUF + REGISTER_BYTE (TYPE_CODE (TYPE) == TYPE_CODE_FLT ? FP0_REGNUM : 11), TYPE_LENGTH (TYPE))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#undef  STORE_RETURN_VALUE
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (REGISTER_BYTE (TYPE_CODE (TYPE) == TYPE_CODE_FLT ? FP0_REGNUM : 11), VALBUF, TYPE_LENGTH (TYPE))

/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer. */

#undef  FRAME_CHAIN
#define FRAME_CHAIN(thisframe) \
  (!inside_entry_file ((thisframe)->pc) ? \
   read_memory_integer ((thisframe)->frame, 4) :\
   0)

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */

#undef  FRAMELESS_FUNCTION_INVOCATION
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
{ (FRAMELESS) = frameless_look_for_prologue (FI); }

#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) (read_memory_integer ((FRAME)->frame + 4, 4))

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

#undef  FIX_CALL_DUMMY
#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)   \
{ \
	*(int *)((char *)(dummyname) + 1) = (int)(fun) - (pc) - 5; \
}

#endif /* !defined (TM_SUN386_H) */

