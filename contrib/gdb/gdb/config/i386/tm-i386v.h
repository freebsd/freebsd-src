/* Macro definitions for i386, Unix System V.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

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

#ifndef TM_I386V_H
#define TM_I386V_H 1

/* First pick up the generic *86 target file. */

#include "i386/tm-i386.h"

/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is
   2 on most implementations.  Override here to 4. */

#undef  START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 4

/* Number of machine registers */

#undef  NUM_REGS
#define NUM_REGS 16

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

/* the order of the first 8 registers must match the compiler's 
 * numbering scheme (which is the same as the 386 scheme)
 * also, this table must match regmap in i386-pinsn.c.
 */

#undef  REGISTER_NAMES
#define REGISTER_NAMES { "eax", "ecx", "edx", "ebx", \
			 "esp", "ebp", "esi", "edi", \
			 "eip", "ps", "cs", "ss", \
			 "ds", "es", "fs", "gs", \
			 }

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */

#undef  REGISTER_BYTES
#define REGISTER_BYTES (NUM_REGS * 4)

/* Index within `registers' of the first byte of the space for
   register N.  */

#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) (4)

/* Number of bytes of storage in the program's representation
   for register N. */

#undef  REGISTER_VIRTUAL_SIZE
#define REGISTER_VIRTUAL_SIZE(N) (4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#undef  MAX_REGISTER_RAW_SIZE
#define MAX_REGISTER_RAW_SIZE 4

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#undef  MAX_REGISTER_VIRTUAL_SIZE
#define MAX_REGISTER_VIRTUAL_SIZE 4

/* Return the GDB type object for the "standard" data type
   of data in register N.  */
/* Perhaps si and di should go here, but potentially they could be
   used for things other than address.  */

#undef  REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
  ((N) == PC_REGNUM || (N) == FP_REGNUM || (N) == SP_REGNUM ?         \
   lookup_pointer_type (builtin_type_void) : builtin_type_int)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#undef  STORE_STRUCT_RETURN
#define STORE_STRUCT_RETURN(ADDR, SP) \
  { (SP) -= sizeof (ADDR);		\
    write_memory ((SP), (char *) &(ADDR), sizeof (ADDR)); }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#undef  EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy ((VALBUF), (REGBUF), TYPE_LENGTH (TYPE))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#undef  STORE_RETURN_VALUE
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (0, VALBUF, TYPE_LENGTH (TYPE))


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
  (FRAMELESS) = frameless_look_for_prologue(FI)

#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) (read_memory_integer ((FRAME)->frame + 4, 4))

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#undef  FRAME_NUM_ARGS
#define FRAME_NUM_ARGS(numargs, fi) (numargs) = -1

#ifdef __STDC__		/* Forward decl's for prototypes */
struct frame_info;
struct frame_saved_regs;
#endif

extern int
i386_frame_num_args PARAMS ((struct frame_info *));

#endif	/* ifndef TM_I386V_H */
