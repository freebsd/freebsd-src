/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1991 by William Jolitz at UUNET Technologies, Inc.
 *
 *	@(#)m-i386bsd.h	6.7 (Berkeley) 5/8/91
 */

/* Macro definitions for i386.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Define the bit, byte, and word ordering of the machine.  */
/* #define BITS_BIG_ENDIAN  */
/* #define BYTES_BIG_ENDIAN */
/* #define WORDS_BIG_ENDIAN */

/*
 * Changes for 80386 by Pace Willisson (pace@prep.ai.mit.edu)
 * July 1988
 * [ MODIFIED FOR 386BSD W. Jolitz ]
 */

#ifndef i386
#define i386	1
#define i386b	1
#endif

#define IEEE_FLOAT

/* Library stuff: POSIX tty (not supported yet), V7 tty (sigh), vprintf.  */

#define	HAVE_TERMIOS	1
#define	USE_OLD_TTY	1
#define	HAVE_VPRINTF	1

/* We support local and remote kernel debugging.  */

#define	KERNELDEBUG	1

/* Get rid of any system-imposed stack limit if possible.  */

#define SET_STACK_LIMIT_HUGE

/* Define this if the C compiler puts an underscore at the front
   of external names before giving them to the linker.  */

#define NAMES_HAVE_UNDERSCORE

/* Specify debugger information format.  */

#define READ_DBX_FORMAT

/* number of traps that happen between exec'ing the shell 
 * to run an inferior, and when we finally get to 
 * the inferior code.  This is 2 on most implementations.
 */
#define START_INFERIOR_TRAPS_EXPECTED 2

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(frompc)   {(frompc) = i386_skip_prologue((frompc));}

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) \
  (read_memory_integer (read_register (SP_REGNUM), 4))

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */

#ifdef NEWVM
#include <machine/vmparam.h>
#define KERNEL_U_ADDR USRSTACK
#else
#define KERNEL_U_ADDR 0xfdffd000
#endif

/* Address of end of stack space.  */

#define STACK_END_ADDR KERNEL_U_ADDR

/* Stack grows downward.  */

#define INNER_THAN <

/* Sequence of bytes for breakpoint instruction.  */

#define BREAKPOINT {0xcc}

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_BREAK 1

/* Nonzero if instruction at PC is a return instruction.  */

#define ABOUT_TO_RETURN(pc) \
	strchr("\302\303\312\313\317", read_memory_integer(pc, 1))

/* Return 1 if P points to an invalid floating point value.
   LEN is the length in bytes -- not relevant on the 386.  */

#define INVALID_FLOAT(p, len) (0)

/* code to execute to print interesting information about the
 * floating point processor (if any)
 * No need to define if there is nothing to do.
 */
#define FLOAT_INFO { i386_float_info (); }


/* Largest integer type */
#define LONGEST long

/* Name of the builtin type for the LONGEST type above. */
#define BUILTIN_TYPE_LONGEST builtin_type_long

/* Say how long (ordinary) registers are.  */

#define REGISTER_TYPE long

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

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = i386_register_u_addr ((blockend),(regno));

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

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#define REGISTER_CONVERTIBLE(N) (0)

/* Convert data from raw format for register REGNUM
   to virtual format for register REGNUM.  */

#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,FROM,TO) {bcopy ((FROM), (TO), 4);}

/* Convert data from virtual format for register REGNUM
   to raw format for register REGNUM.  */

#define REGISTER_CONVERT_TO_RAW(REGNUM,FROM,TO) {bcopy ((FROM), (TO), 4);}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) (builtin_type_int)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#define STORE_STRUCT_RETURN(ADDR, SP) \
  { (SP) -= sizeof (ADDR);		\
    write_memory ((SP), &(ADDR), sizeof (ADDR)); }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  bcopy (REGBUF, VALBUF, TYPE_LENGTH (TYPE))

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
   and produces the frame's chain-pointer.

   FRAME_CHAIN_COMBINE takes the chain pointer and the frame's nominal address
   and produces the nominal address of the caller frame.

   However, if FRAME_CHAIN_VALID returns zero,
   it means the given frame is the outermost one and has no caller.
   In that case, FRAME_CHAIN_COMBINE is not used.  */

#define FRAME_CHAIN(thisframe) \
  (outside_startup_file ((thisframe)->pc) ? \
   read_memory_integer ((thisframe)->frame, 4) :\
   0)

#ifdef KERNELDEBUG
#define	KERNTEXT_BASE	0xfe000000
#ifdef NEWVM
#define KERNSTACK_TOP (read_register(SP_REGNUM) + 0x2000) /* approximate */
#else
/* #define KERNSTACK_TOP (P1PAGES << PGSHIFT) */
#define KERNSTACK_TOP 0xfe000000
#endif
extern int kernel_debugging;
#define FRAME_CHAIN_VALID(chain, thisframe) \
	(chain != 0 && \
	 !kernel_debugging ? outside_startup_file(FRAME_SAVED_PC(thisframe)) :\
	 (chain >= read_register(SP_REGNUM) && chain < KERNSTACK_TOP))
#else
#define FRAME_CHAIN_VALID(chain, thisframe) \
  (chain != 0 && (outside_startup_file (FRAME_SAVED_PC (thisframe))))
#endif

#define FRAME_CHAIN_COMBINE(chain, thisframe) (chain)

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  FRAMELESS_LOOK_FOR_PROLOGUE(FI, FRAMELESS)

#define FRAME_SAVED_PC(FRAME) (read_memory_integer ((FRAME)->frame + 4, 4))

#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#define FRAME_NUM_ARGS(numargs, fi) (numargs) = i386_frame_num_args(fi)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 8

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs) \
{ i386_frame_find_saved_regs ((frame_info), &(frame_saved_regs)); }


/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME  { i386_pop_frame (); }

#define	NEW_CALL_FUNCTION

#if 0
/* Interface definitions for kernel debugger KDB.  */

/* Map machine fault codes into signal numbers.
   First subtract 0, divide by 4, then index in a table.
   Faults for which the entry in this table is 0
   are not handled by KDB; the program's own trap handler
   gets to handle then.  */

#define FAULT_CODE_ORIGIN 0
#define FAULT_CODE_UNITS 4
#define FAULT_TABLE    \
{ 0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0}

/* Start running with a stack stretching from BEG to END.
   BEG and END should be symbols meaningful to the assembler.
   This is used only for kdb.  */

#define INIT_STACK(beg, end)  {}

/* Push the frame pointer register on the stack.  */
#define PUSH_FRAME_PTR        {}

/* Copy the top-of-stack to the frame pointer register.  */
#define POP_FRAME_PTR  {}

/* After KDB is entered by a fault, push all registers
   that GDB thinks about (all NUM_REGS of them),
   so that they appear in order of ascending GDB register number.
   The fault code will be on the stack beyond the last register.  */

#define PUSH_REGISTERS        {}

/* Assuming the registers (including processor status) have been
   pushed on the stack in order of ascending GDB register number,
   restore them and return to the address in the saved PC register.  */

#define POP_REGISTERS      {}
#endif
