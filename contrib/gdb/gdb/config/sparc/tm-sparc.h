/* Target machine sub-parameters for SPARC, for GDB, the GNU debugger.
   This is included by other tm-*.h files to define SPARC cpu-related info.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000
   Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@mcc.com)

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

struct type;
struct value;
struct frame_info;

/*
 * The following enums are purely for the convenience of the GDB 
 * developer, when debugging GDB.
 */

enum {	/* Sparc general registers, for all sparc versions.  */
  G0_REGNUM, G1_REGNUM, G2_REGNUM, G3_REGNUM, 
  G4_REGNUM, G5_REGNUM, G6_REGNUM, G7_REGNUM, 
  O0_REGNUM, O1_REGNUM, O2_REGNUM, O3_REGNUM, 
  O4_REGNUM, O5_REGNUM, O6_REGNUM, O7_REGNUM,
  L0_REGNUM, L1_REGNUM, L2_REGNUM, L3_REGNUM, 
  L4_REGNUM, L5_REGNUM, L6_REGNUM, L7_REGNUM, 
  I0_REGNUM, I1_REGNUM, I2_REGNUM, I3_REGNUM, 
  I4_REGNUM, I5_REGNUM, I6_REGNUM, I7_REGNUM,
  FP0_REGNUM 		/* Floating point register 0 */
};

enum {	/* Sparc general registers, alternate names.  */
  R0_REGNUM,  R1_REGNUM,  R2_REGNUM,  R3_REGNUM, 
  R4_REGNUM,  R5_REGNUM,  R6_REGNUM,  R7_REGNUM, 
  R8_REGNUM,  R9_REGNUM,  R10_REGNUM, R11_REGNUM, 
  R12_REGNUM, R13_REGNUM, R14_REGNUM, R15_REGNUM, 
  R16_REGNUM, R17_REGNUM, R18_REGNUM, R19_REGNUM, 
  R20_REGNUM, R21_REGNUM, R22_REGNUM, R23_REGNUM, 
  R24_REGNUM, R25_REGNUM, R26_REGNUM, R27_REGNUM, 
  R28_REGNUM, R29_REGNUM, R30_REGNUM, R31_REGNUM
};

enum {			/* Sparc32 control registers.  */
  PS_REGNUM  = 65, 	/* PC, NPC, and Y are omitted because */
  WIM_REGNUM = 66,	/* they have different values depending on */
  TBR_REGNUM = 67,	/* 32-bit / 64-bit mode.  */
  FPS_REGNUM = 70,
  CPS_REGNUM = 71
};

/* v9 misc. and priv. regs */

/* Note: specifying values explicitly for documentation purposes.  */
enum {	/* Sparc64 control registers, excluding Y, PC, and NPC.  */
  CCR_REGNUM = 82,		/* Condition Code Register (%xcc,%icc) */
  FSR_REGNUM = 83,		/* Floating Point State */
  FPRS_REGNUM = 84,		/* Floating Point Registers State */
  ASI_REGNUM = 86,		/* Alternate Space Identifier */
  VER_REGNUM = 87,		/* Version register */
  TICK_REGNUM = 88,		/* Tick register */
  PIL_REGNUM = 89, 		/* Processor Interrupt Level */
  PSTATE_REGNUM = 90,		/* Processor State */
  TSTATE_REGNUM = 91,		/* Trap State */
  TBA_REGNUM = 92,		/* Trap Base Address */
  TL_REGNUM = 93,		/* Trap Level */
  TT_REGNUM = 94,		/* Trap Type */
  TPC_REGNUM = 95,		/* Trap pc */
  TNPC_REGNUM = 96,		/* Trap npc */
  WSTATE_REGNUM = 97,		/* Window State */
  CWP_REGNUM = 98,		/* Current Window Pointer */
  CANSAVE_REGNUM = 99,		/* Savable Windows */
  CANRESTORE_REGNUM = 100,	/* Restorable Windows */
  CLEANWIN_REGNUM = 101,	/* Clean Windows */
  OTHERWIN_REGNUM = 102,	/* Other Windows */
  ASR16_REGNUM = 103,		/* Ancillary State Registers */
  ASR17_REGNUM = 104,
  ASR18_REGNUM = 105,
  ASR19_REGNUM = 106,
  ASR20_REGNUM = 107,
  ASR21_REGNUM = 108,
  ASR22_REGNUM = 109,
  ASR23_REGNUM = 110,
  ASR24_REGNUM = 111,
  ASR25_REGNUM = 112,
  ASR26_REGNUM = 113,
  ASR27_REGNUM = 114,
  ASR28_REGNUM = 115,
  ASR29_REGNUM = 116,
  ASR30_REGNUM = 117,
  ASR31_REGNUM = 118,
  ICC_REGNUM   = 119,		/* 32 bit condition codes */
  XCC_REGNUM   = 120,		/* 64 bit condition codes */
  FCC0_REGNUM  = 121,		/* fp cc reg 0 */
  FCC1_REGNUM  = 122,		/* fp cc reg 1 */
  FCC2_REGNUM  = 123,		/* fp cc reg 2 */
  FCC3_REGNUM  = 124		/* fp cc reg 3 */
};

/*
 * Make sparc target multi-archable: April 2000
 */

#if defined (GDB_MULTI_ARCH) && (GDB_MULTI_ARCH > 0)

/* Multi-arch definition of TARGET_IS_SPARC64, TARGET_ELF64 */
#undef  GDB_TARGET_IS_SPARC64
#define GDB_TARGET_IS_SPARC64 \
     (sparc_intreg_size () == 8)
#undef  TARGET_ELF64
#define TARGET_ELF64 \
     (sparc_intreg_size () == 8)
extern int sparc_intreg_size (void);
#else

/* Non-multi-arch: if it isn't defined, define it to zero.  */
#ifndef GDB_TARGET_IS_SPARC64
#define GDB_TARGET_IS_SPARC64 0
#endif
#ifndef TARGET_ELF64
#define TARGET_ELF64 0
#endif
#endif

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH
 */

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",		\
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",		\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",		\
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",		\
								\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",		\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",		\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
                                                                \
  "y", "psr", "wim", "tbr", "pc", "npc", "fpsr", "cpsr" 	\
}

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_BREAK 0

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* Number of machine registers */

#define NUM_REGS 72

#define	SP_REGNUM 14		/* Contains address of top of stack, \
				   which is also the bottom of the frame.  */
#define	FP_REGNUM 30		/* Contains address of executing stack frame */

#define	FP0_REGNUM 32		/* Floating point register 0 */

#define	Y_REGNUM 64		/* Temp register for multiplication, etc.  */

#define	PC_REGNUM 68		/* Contains program counter */

#define	NPC_REGNUM 69		/* Contains next PC */


/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  On the sparc, `registers'
   contains the ins and locals, even though they are saved on the
   stack rather than with the other registers, and this causes hair
   and confusion in places like pop_frame.  It might be better to
   remove the ins and locals from `registers', make sure that
   get_saved_register can get them from the stack (even in the
   innermost frame), and make this the way to access them.  For the
   frame pointer we would do that via TARGET_READ_FP.  On the other
   hand, that is likely to be confusing or worse for flat frames.  */

#define REGISTER_BYTES (32*4+32*4+8*4)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N)  ((N)*4)

/* Number of bytes of storage in the actual machine representation for
   register N.  */

/* On the SPARC, all regs are 4 bytes (except Sparc64, where they're 8).  */

#define REGISTER_RAW_SIZE(N) (4)

/* Number of bytes of storage in the program's representation
   for register N.  */

/* On the SPARC, all regs are 4 bytes (except Sparc64, where they're 8).  */

#define REGISTER_VIRTUAL_SIZE(N) (4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 8

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
     ((N) < 32 ? builtin_type_int : (N) < 64 ? builtin_type_float : \
      builtin_type_int)

/* Sun /bin/cc gets this right as of SunOS 4.1.x.  We need to define
   BELIEVE_PCC_PROMOTION to get this right now that the code which
   detects gcc2_compiled. is broken.  This loses for SunOS 4.0.x and
   earlier.  */

#define BELIEVE_PCC_PROMOTION 1

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  SKIP_PROLOGUE_FRAMELESS_P advances
   the PC past some of the prologue, but stops as soon as it
   knows that the function has a frame.  Its result is equal
   to its input PC if the function is frameless, unequal otherwise.  */

#define SKIP_PROLOGUE(PC) sparc_skip_prologue (PC, 0)

/* Immediately after a function call, return the saved pc.
   Can't go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(FRAME) PC_ADJUST (read_register (RP_REGNUM))

/* Stack grows downward.  */

#define INNER_THAN(LHS,RHS) ((LHS) < (RHS))

/* Write into appropriate registers a function return value of type
   TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE, VALBUF) \
     sparc_store_return_value (TYPE, VALBUF)
extern void sparc_store_return_value (struct type *, char *);

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) \
     sparc_extract_struct_value_address (REGBUF)

extern CORE_ADDR sparc_extract_struct_value_address (char *);

/* If the current gcc for for this target does not produce correct
   debugging information for float parameters, both prototyped and
   unprototyped, then define this macro.  This forces gdb to always
   assume that floats are passed as doubles and then converted in the
   callee. */

#define COERCE_FLOAT_TO_DOUBLE(FORMAL, ACTUAL) (1)

/* Stack must be aligned on 64-bit boundaries when synthesizing
   function calls (128-bit for sparc64).  */

#define STACK_ALIGN(ADDR) sparc32_stack_align (ADDR)
extern CORE_ADDR sparc32_stack_align (CORE_ADDR addr);

/* The Sparc returns long doubles on the stack.  */

#define RETURN_VALUE_ON_STACK(TYPE) \
  (TYPE_CODE(TYPE) == TYPE_CODE_FLT \
   && TYPE_LENGTH(TYPE) > 8)

/* When passing a structure to a function, Sun cc passes the address
   not the structure itself.  It (under SunOS4) creates two symbols,
   which we need to combine to a LOC_REGPARM.  Gcc version two (as of
   1.92) behaves like sun cc.  REG_STRUCT_HAS_ADDR is smart enough to
   distinguish between Sun cc, gcc version 1 and gcc version 2.  */

#define REG_STRUCT_HAS_ADDR(GCC_P, TYPE) \
     sparc_reg_struct_has_addr (GCC_P, TYPE)
extern int sparc_reg_struct_has_addr (int, struct type *);

#endif /* GDB_MULTI_ARCH */

#if defined (GDB_MULTI_ARCH) && (GDB_MULTI_ARCH > 0)
/*
 * The following defines should ONLY appear for MULTI_ARCH.
 */

/* Multi-arch the nPC and Y registers.  */
#define Y_REGNUM              (sparc_y_regnum ())
extern int sparc_npc_regnum (void);
extern int sparc_y_regnum (void);

#endif /* GDB_MULTI_ARCH */

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

#define PC_ADJUST(PC) sparc_pc_adjust (PC)
extern CORE_ADDR sparc_pc_adjust (CORE_ADDR);

/* Advance PC across any function entry prologue instructions to reach
   some "real" code.  SKIP_PROLOGUE_FRAMELESS_P advances the PC past
   some of the prologue, but stops as soon as it knows that the
   function has a frame.  Its result is equal to its input PC if the
   function is frameless, unequal otherwise.  */

#define SKIP_PROLOGUE_FRAMELESS_P(PC) sparc_skip_prologue (PC, 1)
extern CORE_ADDR sparc_skip_prologue (CORE_ADDR, int);

/* If an argument is declared "register", Sun cc will keep it in a register,
   never saving it onto the stack.  So we better not believe the "p" symbol
   descriptor stab.  */

#define USE_REGISTER_NOT_ARG

/* For acc, there's no need to correct LBRAC entries by guessing how
   they should work.  In fact, this is harmful because the LBRAC
   entries now all appear at the end of the function, not intermixed
   with the SLINE entries.  n_opt_found detects acc for Solaris binaries;
   function_stab_type detects acc for SunOS4 binaries.

   For binary from SunOS4 /bin/cc, need to correct LBRAC's.

   For gcc, like acc, don't correct.  */

#define	SUN_FIXED_LBRAC_BUG \
  (n_opt_found \
   || function_stab_type == N_STSYM \
   || function_stab_type == N_GSYM \
   || processing_gcc_compilation)

/* Do variables in the debug stabs occur after the N_LBRAC or before it?
   acc: after, gcc: before, SunOS4 /bin/cc: before.  */

#define VARIABLES_INSIDE_BLOCK(desc, gcc_p) \
  (!(gcc_p) \
   && (n_opt_found \
       || function_stab_type == N_STSYM \
       || function_stab_type == N_GSYM))

/* Sequence of bytes for breakpoint instruction (ta 1). */

#define BREAKPOINT {0x91, 0xd0, 0x20, 0x01}

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define	G0_REGNUM 0		/* %g0 */
#define	G1_REGNUM 1		/* %g1 */
#define O0_REGNUM 8		/* %o0 */
#define	RP_REGNUM 15		/* Contains return address value, *before* \
				   any windows get switched.  */
#define	O7_REGNUM 15		/* Last local reg not saved on stack frame */
#define	L0_REGNUM 16		/* First local reg that's saved on stack frame
				   rather than in machine registers */
#define	I0_REGNUM 24		/* %i0 */
#define	I7_REGNUM 31		/* Last local reg saved on stack frame */
#define	PS_REGNUM 65		/* Contains processor status */
#define PS_FLAG_CARRY 0x100000	/* Carry bit in PS */
#define	WIM_REGNUM 66		/* Window Invalid Mask (not really supported) */
#define	TBR_REGNUM 67		/* Trap Base Register (not really supported) */
#define	FPS_REGNUM 70		/* Floating point status register */
#define	CPS_REGNUM 71		/* Coprocessor status register */

/* Writing to %g0 is a noop (not an error or exception or anything like
   that, however).  */

#define CANNOT_STORE_REGISTER(regno) ((regno) == G0_REGNUM)

/*
 * FRAME_CHAIN and FRAME_INFO definitions, collected here for convenience.
 */

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH.
 */

/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer. */

/* In the case of the Sun 4, the frame-chain's nominal address
   is held in the frame pointer register.

   On the Sun4, the frame (in %fp) is %sp for the previous frame.
   From the previous frame's %sp, we can find the previous frame's
   %fp: it is in the save area just above the previous frame's %sp.

   If we are setting up an arbitrary frame, we'll need to know where
   it ends.  Hence the following.  This part of the frame cache
   structure should be checked before it is assumed that this frame's
   bottom is in the stack pointer.

   If there isn't a frame below this one, the bottom of this frame is
   in the stack pointer.

   If there is a frame below this one, and the frame pointers are
   identical, it's a leaf frame and the bottoms are the same also.

   Otherwise the bottom of this frame is the top of the next frame.

   The bottom field is misnamed, since it might imply that memory from
   bottom to frame contains this frame.  That need not be true if
   stack frames are allocated in different segments (e.g. some on a
   stack, some on a heap in the data segment).

   GCC 2.6 and later can generate ``flat register window'' code that
   makes frames by explicitly saving those registers that need to be
   saved.  %i7 is used as the frame pointer, and the frame is laid out
   so that flat and non-flat calls can be intermixed freely within a
   program.  Unfortunately for GDB, this means it must detect and
   record the flatness of frames.

   Since the prologue in a flat frame also tells us where fp and pc
   have been stashed (the frame is of variable size, so their location
   is not fixed), it's convenient to record them in the frame info.  */

#define EXTRA_FRAME_INFO \
  CORE_ADDR bottom;      \
  int in_prologue;       \
  int flat;              \
  /* Following fields only relevant for flat frames.  */ \
  CORE_ADDR pc_addr;     \
  CORE_ADDR fp_addr;     \
  /* Add this to ->frame to get the value of the stack pointer at the */ \
  /* time of the register saves.  */ \
  int sp_offset;

/* We need to override GET_SAVED_REGISTER so that we can deal with the way
   outs change into ins in different frames.  HAVE_REGISTER_WINDOWS can't
   deal with this case and also handle flat frames at the same time.  */

void sparc_get_saved_register (char *raw_buffer,
			       int *optimized,
			       CORE_ADDR * addrp,
			       struct frame_info *frame,
			       int regnum, enum lval_type *lvalp);

#define GET_SAVED_REGISTER(RAW_BUFFER, OPTIMIZED, ADDRP, FRAME, REGNUM, LVAL) \
     sparc_get_saved_register (RAW_BUFFER, OPTIMIZED, ADDRP, \
			       FRAME, REGNUM, LVAL)

#define FRAME_INIT_SAVED_REGS(FP)	/*no-op */

#define INIT_EXTRA_FRAME_INFO(FROMLEAF, FCI) \
     sparc_init_extra_frame_info (FROMLEAF, FCI)
extern void sparc_init_extra_frame_info (int, struct frame_info *);

#define FRAME_CHAIN(THISFRAME) (sparc_frame_chain (THISFRAME))
extern CORE_ADDR sparc_frame_chain (struct frame_info *);

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */

#define FRAMELESS_FUNCTION_INVOCATION(FI) \
     frameless_look_for_prologue (FI)

/* Where is the PC for a specific frame */

#define FRAME_SAVED_PC(FRAME) sparc_frame_saved_pc (FRAME)
extern CORE_ADDR sparc_frame_saved_pc (struct frame_info *);

/* If the argument is on the stack, it will be here.  */
#define FRAME_ARGS_ADDRESS(FI) ((FI)->frame)

#define FRAME_LOCALS_ADDRESS(FI) ((FI)->frame)

/* Set VAL to the number of args passed to frame described by FI.
   Can set VAL to -1, meaning no way to tell.  */

/* We can't tell how many args there are
   now that the C compiler delays popping them.  */
#define FRAME_NUM_ARGS(FI) (-1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 68

#endif /* GDB_MULTI_ARCH */

#define	PRINT_EXTRA_FRAME_INFO(FI) \
     sparc_print_extra_frame_info (FI)
extern void sparc_print_extra_frame_info (struct frame_info *);

/* INIT_EXTRA_FRAME_INFO needs the PC to detect flat frames.  */

#define	INIT_FRAME_PC(FROMLEAF, PREV)	/* nothing */
#define INIT_FRAME_PC_FIRST(FROMLEAF, PREV) \
  (PREV)->pc = ((FROMLEAF) ? SAVED_PC_AFTER_CALL ((PREV)->next) : \
	      (PREV)->next ? FRAME_SAVED_PC ((PREV)->next) : read_pc ());

/* Define other aspects of the stack frame.  */

/* The location of I0 w.r.t SP.  This is actually dependent on how the
   system's window overflow/underflow routines are written.  Most
   vendors save the L regs followed by the I regs (at the higher
   address).  Some vendors get it wrong.  */

#define	FRAME_SAVED_L0	0
#define	FRAME_SAVED_I0	(8 * REGISTER_RAW_SIZE (L0_REGNUM))

#define FRAME_STRUCT_ARGS_ADDRESS(FI) ((FI)->frame)

/* Things needed for making the inferior call functions.  */
/*
 * First of all, let me give my opinion of what the DUMMY_FRAME
 * actually looks like.
 *
 *               |                                 |
 *               |                                 |
 *               + - - - - - - - - - - - - - - - - +<-- fp (level 0)
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |  Frame of innermost program     |
 *               |           function              |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |---------------------------------|<-- sp (level 0), fp (c)
 *               |                                 |
 *     DUMMY     |             fp0-31              |
 *               |                                 |
 *               |             ------              |<-- fp - 0x80
 *     FRAME     |              g0-7               |<-- fp - 0xa0
 *               |              i0-7               |<-- fp - 0xc0
 *               |             other               |<-- fp - 0xe0
 *               |               ?                 |
 *               |               ?                 |
 *               |---------------------------------|<-- sp' = fp - 0x140
 *               |                                 |
 * xcution start |                                 |
 * sp' + 0x94 -->|        CALL_DUMMY (x code)      |
 *               |                                 |
 *               |                                 |
 *               |---------------------------------|<-- sp'' = fp - 0x200
 *               |  align sp to 8 byte boundary    |
 *               |     ==> args to fn <==          |
 *  Room for     |                                 |
 * i & l's + agg | CALL_DUMMY_STACK_ADJUST = 0x0x44|
 *               |---------------------------------|<-- final sp (variable)
 *               |                                 |
 *               |   Where function called will    |
 *               |           build frame.          |
 *               |                                 |
 *               |                                 |
 *
 *   I understand everything in this picture except what the space
 * between fp - 0xe0 and fp - 0x140 is used for.  Oh, and I don't
 * understand why there's a large chunk of CALL_DUMMY that never gets
 * executed (its function is superceeded by PUSH_DUMMY_FRAME; they
 * are designed to do the same thing).
 *
 *   PUSH_DUMMY_FRAME saves the registers above sp' and pushes the
 * register file stack down one.
 *
 *   call_function then writes CALL_DUMMY, pushes the args onto the
 * stack, and adjusts the stack pointer.
 *
 *   run_stack_dummy then starts execution (in the middle of
 * CALL_DUMMY, as directed by call_function).
 */

#ifndef CALL_DUMMY
/* This sequence of words is the instructions

   00:   bc 10 00 01     mov  %g1, %fp
   04:   9d e3 80 00     save  %sp, %g0, %sp
   08:   bc 10 00 02     mov  %g2, %fp
   0c:   be 10 00 03     mov  %g3, %i7
   10:   da 03 a0 58     ld  [ %sp + 0x58 ], %o5
   14:   d8 03 a0 54     ld  [ %sp + 0x54 ], %o4
   18:   d6 03 a0 50     ld  [ %sp + 0x50 ], %o3
   1c:   d4 03 a0 4c     ld  [ %sp + 0x4c ], %o2
   20:   d2 03 a0 48     ld  [ %sp + 0x48 ], %o1
   24:   40 00 00 00     call  <fun>
   28:   d0 03 a0 44     ld  [ %sp + 0x44 ], %o0
   2c:   01 00 00 00     nop 
   30:   91 d0 20 01     ta  1
   34:   01 00 00 00     nop

   NOTES:
   * the first four instructions are necessary only on the simulator.
   * this is a multiple of 8 (not only 4) bytes.
   * the `call' insn is a relative, not an absolute call.
   * the `nop' at the end is needed to keep the trap from
     clobbering things (if NPC pointed to garbage instead).
 */

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH.
 */

#define CALL_DUMMY { 0xbc100001, 0x9de38000, 0xbc100002, 0xbe100003,	\
		     0xda03a058, 0xd803a054, 0xd603a050, 0xd403a04c,	\
		     0xd203a048, 0x40000000, 0xd003a044, 0x01000000,	\
		     0x91d02001, 0x01000000 }


/* Size of the call dummy in bytes. */

#define CALL_DUMMY_LENGTH 0x38

/* Offset within call dummy of first instruction to execute. */

#define CALL_DUMMY_START_OFFSET 0

/* Offset within CALL_DUMMY of the 'call' instruction. */

#define CALL_DUMMY_CALL_OFFSET (CALL_DUMMY_START_OFFSET + 0x24)

/* Offset within CALL_DUMMY of the 'ta 1' trap instruction. */

#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + 0x30)

#define CALL_DUMMY_STACK_ADJUST 68

/* Call dummy method (eg. on stack, at entry point, etc.) */

#define CALL_DUMMY_LOCATION ON_STACK

/* Method for detecting dummy frames.  */

#define PC_IN_CALL_DUMMY(PC, SP, FRAME_ADDRESS) \
     pc_in_call_dummy_on_stack (PC, SP, FRAME_ADDRESS)

#endif /* GDB_MULTI_ARCH */

#endif /* CALL_DUMMY */

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH. 
 */

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

#define FIX_CALL_DUMMY(DUMMYNAME, PC, FUN, NARGS, ARGS, TYPE, GCC_P) \
     sparc_fix_call_dummy (DUMMYNAME, PC, FUN, TYPE, GCC_P)
void sparc_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun,
			   struct type *value_type, int using_gcc);

/* Arguments smaller than an int must be promoted to ints when
   synthesizing function calls.  */

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME	sparc_push_dummy_frame ()
#define POP_FRAME		sparc_pop_frame ()

void sparc_push_dummy_frame (void);
void sparc_pop_frame (void);

#define PUSH_ARGUMENTS(NARGS, ARGS, SP, STRUCT_RETURN, STRUCT_ADDR) \
     sparc32_push_arguments (NARGS, ARGS, SP, STRUCT_RETURN, STRUCT_ADDR)

extern CORE_ADDR
sparc32_push_arguments (int, struct value **, CORE_ADDR, int, CORE_ADDR);

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function_by_hand. 
   The ultimate mystery is, tho, what is the value "16"?  */

#define STORE_STRUCT_RETURN(ADDR, SP) \
  { char val[4]; \
    store_unsigned_integer (val, 4, (ADDR)); \
    write_memory ((SP)+(16*4), val, 4); }

/* Default definition of USE_STRUCT_CONVENTION.  */

#ifndef USE_STRUCT_CONVENTION
#define USE_STRUCT_CONVENTION(GCC_P, TYPE) \
     generic_use_struct_convention (GCC_P, TYPE)
#endif

/* Extract from an array REGBUF containing the (raw) register state a
   function return value of type TYPE, and copy that, in virtual
   format, into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE, REGBUF, VALBUF) \
     sparc32_extract_return_value (TYPE, REGBUF, VALBUF)
extern void sparc32_extract_return_value (struct type *, char[], char *);

#endif /* GDB_MULTI_ARCH */


/* Sparc has no reliable single step ptrace call */

#define SOFTWARE_SINGLE_STEP_P() 1
extern void sparc_software_single_step (enum target_signal, int);
#define SOFTWARE_SINGLE_STEP(sig,bp_p) sparc_software_single_step (sig,bp_p)

/* We need more arguments in a frame specification for the
   "frame" or "info frame" command.  */

#define SETUP_ARBITRARY_FRAME(argc, argv) setup_arbitrary_frame (argc, argv)
extern struct frame_info *setup_arbitrary_frame (int, CORE_ADDR *);

/* To print every pair of float registers as a double, we use this hook.
   We also print the condition code registers in a readable format
   (FIXME: can expand this to all control regs).  */

#undef 	PRINT_REGISTER_HOOK
#define	PRINT_REGISTER_HOOK(regno)	\
  sparc_print_register_hook (regno)
extern void sparc_print_register_hook (int regno);

/* Optimization for storing registers to the inferior.  The hook
   DO_DEFERRED_STORES
   actually executes any deferred stores.  It is called any time
   we are going to proceed the child, or read its registers.
   The hook CLEAR_DEFERRED_STORES is called when we want to throw
   away the inferior process, e.g. when it dies or we kill it.
   FIXME, this does not handle remote debugging cleanly.  */

extern int deferred_stores;
#define	DO_DEFERRED_STORES	\
  if (deferred_stores)		\
    target_store_registers (-2);
#define	CLEAR_DEFERRED_STORES	\
  deferred_stores = 0;

/* Select the sparc disassembler */

#define TM_PRINT_INSN_MACH bfd_mach_sparc

