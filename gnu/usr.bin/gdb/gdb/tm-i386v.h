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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if !defined (TM_I386V_H)
#define TM_I386V_H 1

/*
 * Changes for 80386 by Pace Willisson (pace@prep.ai.mit.edu)
 * July 1988
 */

#define TARGET_BYTE_ORDER LITTLE_ENDIAN

/* turn this on when rest of gdb is ready */
#define IEEE_FLOAT

/* number of traps that happen between exec'ing the shell 
 * to run an inferior, and when we finally get to 
 * the inferior code.  This is 2 on most implementations.
 */
#ifndef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 4
#endif

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(frompc)   {(frompc) = i386_skip_prologue((frompc));}

extern int
i386_skip_prologue PARAMS ((int));

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) \
  (read_memory_integer (read_register (SP_REGNUM), 4))

/* Stack grows downward.  */

#define INNER_THAN <

/* Sequence of bytes for breakpoint instruction.  */

#define BREAKPOINT {0xcc}

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#ifndef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 1
#endif

/* Nonzero if instruction at PC is a return instruction.  */

#define ABOUT_TO_RETURN(pc) (read_memory_integer (pc, 1) == 0xc3)

/* Return 1 if P points to an invalid floating point value.
   LEN is the length in bytes -- not relevant on the 386.  */

#define INVALID_FLOAT(p, len) (0)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* Number of machine registers */

#define NUM_REGS 16

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

/* the order of the first 8 registers must match the compiler's 
 * numbering scheme (which is the same as the 386 scheme)
 * also, this table must match regmap in i386-pinsn.c.
 */
#define REGISTER_NAMES { "eax", "ecx", "edx", "ebx", \
			 "esp", "ebp", "esi", "edi", \
			 "eip", "ps", "cs", "ss", \
			 "ds", "es", "fs", "gs", \
			 }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define FP_REGNUM 5		/* Contains address of executing stack frame */
#define SP_REGNUM 4		/* Contains address of top of stack */

#define PC_REGNUM 8
#define PS_REGNUM 9

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */
#define REGISTER_BYTES (NUM_REGS * 4)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N) ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#define REGISTER_RAW_SIZE(N) (4)

/* Number of bytes of storage in the program's representation
   for register N. */

#define REGISTER_VIRTUAL_SIZE(N) (4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 4

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 4

/* Return the GDB type object for the "standard" data type
   of data in register N.  */
/* Perhaps si and di should go here, but potentially they could be
   used for things other than address.  */
#define REGISTER_VIRTUAL_TYPE(N) \
  ((N) == PC_REGNUM || (N) == FP_REGNUM || (N) == SP_REGNUM ?         \
   lookup_pointer_type (builtin_type_void) : builtin_type_int)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#define STORE_STRUCT_RETURN(ADDR, SP) \
  { (SP) -= sizeof (ADDR);		\
    write_memory ((SP), (char *) &(ADDR), sizeof (ADDR)); }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy ((VALBUF), (REGBUF), TYPE_LENGTH (TYPE))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (0, VALBUF, TYPE_LENGTH (TYPE))

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(int *)(REGBUF))


/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer. */

#define FRAME_CHAIN(thisframe) \
  (!inside_entry_file ((thisframe)->pc) ? \
   read_memory_integer ((thisframe)->frame, 4) :\
   0)

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

#define FRAME_SAVED_PC(FRAME) (read_memory_integer ((FRAME)->frame + 4, 4))

#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#define FRAME_NUM_ARGS(numargs, fi) (numargs) = -1

#ifdef __STDC__		/* Forward decl's for prototypes */
struct frame_info;
struct frame_saved_regs;
#endif

extern int
i386_frame_num_args PARAMS ((struct frame_info *));

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 8

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs) \
{ i386_frame_find_saved_regs ((frame_info), &(frame_saved_regs)); }

extern void
i386_frame_find_saved_regs PARAMS ((struct frame_info *,
				    struct frame_saved_regs *));


/* Things needed for making the inferior call functions.  */

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME { i386_push_dummy_frame (); }

extern void
i386_push_dummy_frame PARAMS ((void));

/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME  { i386_pop_frame (); }

extern void
i386_pop_frame PARAMS ((void));

/* this is 
 *   call 11223344 (32 bit relative)
 *   int3
 */

#define CALL_DUMMY { 0x223344e8, 0xcc11 }

#define CALL_DUMMY_LENGTH 8

#define CALL_DUMMY_START_OFFSET 0  /* Start execution at beginning of dummy */

#define CALL_DUMMY_BREAKPOINT_OFFSET 5

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)   \
{ \
	int from, to, delta, loc; \
	loc = (int)(read_register (SP_REGNUM) - CALL_DUMMY_LENGTH); \
	from = loc + 5; \
	to = (int)(fun); \
	delta = to - from; \
	*((char *)(dummyname) + 1) = (delta & 0xff); \
	*((char *)(dummyname) + 2) = ((delta >> 8) & 0xff); \
	*((char *)(dummyname) + 3) = ((delta >> 16) & 0xff); \
	*((char *)(dummyname) + 4) = ((delta >> 24) & 0xff); \
}

extern void
print_387_control_word PARAMS ((unsigned int));

extern void
print_387_status_word PARAMS ((unsigned int));

/* Offset from SP to first arg on stack at first instruction of a function */

#define SP_ARG0 (1 * 4)

#endif	/* !defined (TM_I386V_H) */
