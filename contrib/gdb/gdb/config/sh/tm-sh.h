/* Target-specific definition for a Hitachi Super-H.
   Copyright (C) 1993 Free Software Foundation, Inc.

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

/* Contributed by Steve Chamberlain sac@cygnus.com */

#define GDB_TARGET_IS_SH

#define IEEE_FLOAT 1

/* Define the bit, byte, and word ordering of the machine.  */

#define TARGET_BYTE_ORDER_SELECTABLE


/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

extern CORE_ADDR sh_skip_prologue ();
#define SKIP_PROLOGUE(ip) \
    {(ip) = sh_skip_prologue(ip);}


/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions. 

   The return address is the value saved in the PR register + 4  */

#define SAVED_PC_AFTER_CALL(frame) \
  (ADDR_BITS_REMOVE(read_register(PR_REGNUM)))

/* Stack grows downward.  */

#define INNER_THAN <

/* Illegal instruction - used by the simulator for breakpoint
   detection */

#define BREAKPOINT {0xc3, 0xc3}  /* 0xc3c3 is trapa #c3, and it works in big 
				    and little endian modes  */
#define REMOTE_BREAKPOINT { 0xc3, 0x20}

/* If your kernel resets the pc after the trap happens you may need to
   define this before including this file.  */
#define DECR_PC_AFTER_BREAK 0

/* Nonzero if instruction at PC is a return instruction.  */
#define ABOUT_TO_RETURN(pc) (read_memory_integer(pc,2) == 0x000b)

/* Say how long registers are.  */
#define REGISTER_TYPE  long

/* Say how much memory is needed to store a copy of the register set */
#define REGISTER_BYTES    (NUM_REGS*4) 

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N)  ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#define REGISTER_RAW_SIZE(N) 4

#define REGISTER_VIRTUAL_SIZE(N) 4

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 4

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 4

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
	((((N) >= FP0_REGNUM && (N) < FP15_REGNUM)	\
	  || (N) == FPUL_REGNUM)			\
         ? builtin_type_float : builtin_type_int)

/* Initializer for an array of names of registers.
   Entries beyond the first NUM_REGS are ignored.  */

#define REGISTER_NAMES \
  {"r0", "r1", "r2", "r3", "r4",  "r5",  "r6", "r7", \
   "r8", "r9", "r10","r11","r12", "r13", "r14","r15",\
   "pc", "pr", "gbr","vbr","mach","macl","sr", \
   "fpul","fpscr", \
   "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", \
   "fr8", "fr9", "fr10","fr11","fr12","fr13","fr14","fr15",\
   "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0", \
   "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1" \
  }

#define NUM_REGS 57

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define R0_REGNUM	0
#define FP_REGNUM 	14
#define SP_REGNUM 	15
#define PC_REGNUM 	16
#define PR_REGNUM 	17
#define GBR_REGNUM 	18
#define VBR_REGNUM 	19
#define MACH_REGNUM 	20
#define MACL_REGNUM 	21
#define SR_REGNUM 	22
#define NUM_REALREGS    23
#define FPUL_REGNUM	23
#define FP0_REGNUM	25
#define FP15_REGNUM	41
#undef  NUM_REALREGS
#define NUM_REALREGS    57

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. 

   We store structs through a pointer passed in R4 */

#define STORE_STRUCT_RETURN(ADDR, SP) \
    { write_register (4, (ADDR));  }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy (VALBUF, (char *)(REGBUF), TYPE_LENGTH(TYPE))


/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  

   Things always get returned in R4/R5 */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (REGISTER_BYTE(4), VALBUF, TYPE_LENGTH (TYPE))


/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(CORE_ADDR *)(REGBUF))


/* Define other aspects of the stack frame. 
   we keep a copy of the worked out return pc lying around, since it
   is a useful bit of info */

#define EXTRA_FRAME_INFO \
    CORE_ADDR return_pc; \
    int leaf_function;   \
    int f_offset;    

#define INIT_EXTRA_FRAME_INFO(fromleaf, fi) \
    init_extra_frame_info(fromleaf, fi) 

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */

#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

#define FRAME_CHAIN(FRAME)       sh_frame_chain(FRAME)
#define FRAME_SAVED_PC(FRAME)    ((FRAME)->return_pc)
#define FRAME_ARGS_ADDRESS(fi)   (fi)->frame
#define FRAME_LOCALS_ADDRESS(fi) (fi)->frame

/* Set VAL to the number of args passed to frame described by FI.
   Can set VAL to -1, meaning no way to tell.  */

/* We can't tell how many args there are */

#define FRAME_NUM_ARGS(val,fi) (val = -1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 0

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs)	    \
   frame_find_saved_regs(frame_info, &(frame_saved_regs))

#define NAMES_HAVE_UNDERSCORE

typedef unsigned short INSN_WORD;

#define ADDR_BITS_REMOVE(addr) ((addr))

#define CALL_DUMMY_LENGTH 10

/* Discard from the stack the innermost frame,
   restoring all saved registers.  */

#define POP_FRAME pop_frame();


#define NOP   {0x20, 0x0b}

#define REGISTER_SIZE 4

