/* Target machine sub-parameters for SPARC64, for GDB, the GNU debugger.
   This is included by other tm-*.h files to define SPARC64 cpu-related info.
   Copyright 1994, 1995, 1996, 1998, 1999, 2000
   Free Software Foundation, Inc.
   This is (obviously) based on the SPARC Vn (n<9) port.
   Contributed by Doug Evans (dje@cygnus.com).
   Further modified by Bob Manson (manson@cygnus.com).

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

#define GDB_MULTI_ARCH GDB_MULTI_ARCH_PARTIAL

#ifndef GDB_TARGET_IS_SPARC64
#define GDB_TARGET_IS_SPARC64 1
#endif

#include "sparc/tm-sparc.h"

/* Eeeew. Ok, we have to assume (for now) that the processor really is
   in sparc64 mode. While this is the same instruction sequence as
   on the Sparc, the stack frames are offset by +2047 (and the arguments
   are 8 bytes instead of 4). */
/* Instructions are:
   std  %f10, [ %fp + 0x7a7 ]
   std  %f8, [ %fp + 0x79f ]
   std  %f6, [ %fp + 0x797 ]
   std  %f4, [ %fp + 0x78f ]
   std  %f2, [ %fp + 0x787 ]
   std  %f0, [ %fp + 0x77f ]
   std  %g6, [ %fp + 0x777 ]
   std  %g4, [ %fp + 0x76f ]
   std  %g2, [ %fp + 0x767 ]
   std  %g0, [ %fp + 0x75f ]
   std  %fp, [ %fp + 0x757 ]
   std  %i4, [ %fp + 0x74f ]
   std  %i2, [ %fp + 0x747 ]
   std  %i0, [ %fp + 0x73f ]
   nop
   nop
   nop
   nop
   rd  %tbr, %o0
   st  %o0, [ %fp + 0x72b ]
   rd  %tpc, %o0
   st  %o0, [ %fp + 0x727 ]
   rd  %psr, %o0
   st  %o0, [ %fp + 0x723 ]
   rd  %y, %o0
   st  %o0, [ %fp + 0x71f ]
   ldx  [ %sp + 0x8a7 ], %o5
   ldx  [ %sp + 0x89f ], %o4
   ldx  [ %sp + 0x897 ], %o3
   ldx  [ %sp + 0x88f ], %o2
   ldx  [ %sp + 0x887 ], %o1
   call  %g0
   ldx  [ %sp + 0x87f ], %o0
   nop
   ta  1
   nop
   nop
 */

#if !defined (GDB_MULTI_ARCH) || (GDB_MULTI_ARCH == 0)
/*
 * The following defines must go away for MULTI_ARCH.
 */

#ifndef DO_CALL_DUMMY_ON_STACK

/*
 * These defines will suffice for the AT_ENTRY_POINT call dummy method.
 */

#undef  CALL_DUMMY
#define CALL_DUMMY {0}
#undef  CALL_DUMMY_LENGTH
#define CALL_DUMMY_LENGTH 0
#undef  CALL_DUMMY_CALL_OFFSET
#define CALL_DUMMY_CALL_OFFSET 0
#undef  CALL_DUMMY_START_OFFSET
#define CALL_DUMMY_START_OFFSET 0
#undef  CALL_DUMMY_BREAKPOINT_OFFSET
#define CALL_DUMMY_BREAKPOINT_OFFSET 0
#undef  CALL_DUMMY_BREAKPOINT_OFFSET_P
#define CALL_DUMMY_BREAKPOINT_OFFSET_P 1
#undef  CALL_DUMMY_LOCATION 
#define CALL_DUMMY_LOCATION AT_ENTRY_POINT
#undef  CALL_DUMMY_STACK_ADJUST
#define CALL_DUMMY_STACK_ADJUST 128
#undef  SIZEOF_CALL_DUMMY_WORDS
#define SIZEOF_CALL_DUMMY_WORDS 0
#undef  CALL_DUMMY_ADDRESS
#define CALL_DUMMY_ADDRESS() entry_point_address()
#undef  FIX_CALL_DUMMY
#define FIX_CALL_DUMMY(DUMMYNAME, PC, FUN, NARGS, ARGS, TYPE, GCC_P) 
#undef  PUSH_RETURN_ADDRESS
#define PUSH_RETURN_ADDRESS(PC, SP) sparc_at_entry_push_return_address (PC, SP)
extern CORE_ADDR 
sparc_at_entry_push_return_address (CORE_ADDR pc, CORE_ADDR sp);

#undef  STORE_STRUCT_RETURN
#define STORE_STRUCT_RETURN(ADDR, SP) \
     sparc_at_entry_store_struct_return (ADDR, SP)
extern void 
sparc_at_entry_store_struct_return (CORE_ADDR addr, CORE_ADDR sp);


#else
/*
 * Old call dummy method, with CALL_DUMMY on the stack.
 */

#undef  CALL_DUMMY
#define CALL_DUMMY {		 0x9de3bec0fd3fa7f7LL, 0xf93fa7eff53fa7e7LL,\
				 0xf13fa7dfed3fa7d7LL, 0xe93fa7cfe53fa7c7LL,\
				 0xe13fa7bfdd3fa7b7LL, 0xd93fa7afd53fa7a7LL,\
				 0xd13fa79fcd3fa797LL, 0xc93fa78fc53fa787LL,\
				 0xc13fa77fcc3fa777LL, 0xc83fa76fc43fa767LL,\
				 0xc03fa75ffc3fa757LL, 0xf83fa74ff43fa747LL,\
				 0xf03fa73f01000000LL, 0x0100000001000000LL,\
				 0x0100000091580000LL, 0xd027a72b93500000LL,\
				 0xd027a72791480000LL, 0xd027a72391400000LL,\
				 0xd027a71fda5ba8a7LL, 0xd85ba89fd65ba897LL,\
				 0xd45ba88fd25ba887LL, 0x9fc02000d05ba87fLL,\
				 0x0100000091d02001LL, 0x0100000001000000LL }


/* 128 is to reserve space to write the %i/%l registers that will be restored
   when we resume. */
#undef  CALL_DUMMY_STACK_ADJUST
#define CALL_DUMMY_STACK_ADJUST 128

/* Size of the call dummy in bytes. */
#undef  CALL_DUMMY_LENGTH
#define CALL_DUMMY_LENGTH 192

/* Offset within CALL_DUMMY of the 'call' instruction. */
#undef  CALL_DUMMY_START_OFFSET
#define CALL_DUMMY_START_OFFSET 148

/* Offset within CALL_DUMMY of the 'call' instruction. */
#undef  CALL_DUMMY_CALL_OFFSET
#define CALL_DUMMY_CALL_OFFSET (CALL_DUMMY_START_OFFSET + (5 * 4))

/* Offset within CALL_DUMMY of the 'ta 1' instruction. */
#undef  CALL_DUMMY_BREAKPOINT_OFFSET
#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + (8 * 4))

/* Let's GDB know that it can make a call_dummy breakpoint.  */
#undef  CALL_DUMMY_BREAKPOINT_OFFSET_P
#define CALL_DUMMY_BREAKPOINT_OFFSET_P 1

/* Call dummy will be located on the stack.  */
#undef  CALL_DUMMY_LOCATION
#define CALL_DUMMY_LOCATION ON_STACK

/* Insert the function address into the call dummy.  */
#undef  FIX_CALL_DUMMY
#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p) \
 sparc_fix_call_dummy (dummyname, pc, fun, type, gcc_p)
void sparc_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun,
			   struct type *value_type, int using_gcc);


/* The remainder of these will accept the default definition.  */
#undef  SIZEOF_CALL_DUMMY_WORDS
#undef  PUSH_RETURN_ADDRESS
#undef  CALL_DUMMY_ADDRESS
#undef  STORE_STRUCT_RETURN

#endif

/* Does the specified function use the "struct returning" convention
   or the "value returning" convention?  The "value returning" convention
   almost invariably returns the entire value in registers.  The
   "struct returning" convention often returns the entire value in
   memory, and passes a pointer (out of or into the function) saying
   where the value (is or should go).

   Since this sometimes depends on whether it was compiled with GCC,
   this is also an argument.  This is used in call_function to build a
   stack, and in value_being_returned to print return values. 

   On Sparc64, we only pass pointers to structs if they're larger than
   32 bytes. Otherwise they're stored in %o0-%o3 (floating-point
   values go into %fp0-%fp3).  */

#undef  USE_STRUCT_CONVENTION
#define USE_STRUCT_CONVENTION(gcc_p, type) (TYPE_LENGTH (type) > 32)

CORE_ADDR sparc64_push_arguments (int,
				  struct value **, CORE_ADDR, int, CORE_ADDR);
#undef PUSH_ARGUMENTS
#define PUSH_ARGUMENTS(A,B,C,D,E) \
     (sparc64_push_arguments ((A), (B), (C), (D), (E)))

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */
/* FIXME: V9 uses %o0 for this.  */

#undef  STORE_STRUCT_RETURN
#define STORE_STRUCT_RETURN(ADDR, SP) \
  { target_write_memory ((SP)+(16*8), (char *)&(ADDR), 8); }

/* Stack must be aligned on 128-bit boundaries when synthesizing
   function calls. */

#undef  STACK_ALIGN
#define STACK_ALIGN(ADDR) (((ADDR) + 15 ) & -16)

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */
/* Some of these registers are only accessible from priviledged mode.
   They are here for kernel debuggers, etc.  */
/* FIXME: icc and xcc are currently considered separate registers.
   This may have to change and consider them as just one (ccr).
   Let's postpone this as long as we can.  It's nice to be able to set
   them individually.  */
/* FIXME: fcc0-3 are currently separate, even though they are also part of
   fsr.  May have to remove them but let's postpone this as long as
   possible.  It's nice to be able to set them individually.  */
/* FIXME: Whether to include f33, f35, etc. here is not clear.
   There are advantages and disadvantages.  */

#undef  REGISTER_NAMES
#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",	\
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",	\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",	\
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",	\
								\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",		\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",		\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
  "f32", "f34", "f36", "f38", "f40", "f42", "f44", "f46",	\
  "f48", "f50", "f52", "f54", "f56", "f58", "f60", "f62",	\
                                                                \
  "pc", "npc", "ccr", "fsr", "fprs", "y", "asi",		\
  "ver", "tick", "pil", "pstate",				\
  "tstate", "tba", "tl", "tt", "tpc", "tnpc", "wstate",		\
  "cwp", "cansave", "canrestore", "cleanwin", "otherwin",	\
  "asr16", "asr17", "asr18", "asr19", "asr20", "asr21",		\
  "asr22", "asr23", "asr24", "asr25", "asr26", "asr27",		\
  "asr28", "asr29", "asr30", "asr31",				\
  /* These are here at the end to simplify removing them if we have to.  */ \
  "icc", "xcc", "fcc0", "fcc1", "fcc2", "fcc3"			\
}

#undef REG_STRUCT_HAS_ADDR
#define REG_STRUCT_HAS_ADDR(gcc_p,type) (TYPE_LENGTH (type) > 32)

extern CORE_ADDR sparc64_read_sp ();
extern CORE_ADDR sparc64_read_fp ();
extern void sparc64_write_sp (CORE_ADDR);
extern void sparc64_write_fp (CORE_ADDR);

#define TARGET_READ_SP() (sparc64_read_sp ())
#define TARGET_READ_FP() (sparc64_read_fp ())
#define TARGET_WRITE_SP(X) (sparc64_write_sp (X))
#define TARGET_WRITE_FP(X) (sparc64_write_fp (X))

#undef EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
     sp64_extract_return_value(TYPE, REGBUF, VALBUF, 0)
extern void sp64_extract_return_value (struct type *, char[], char *, int);

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#if 0				/* defined in tm-sparc.h, replicated
				   for doc purposes */
#define	G0_REGNUM 0		/* %g0 */
#define	G1_REGNUM 1		/* %g1 */
#define O0_REGNUM 8		/* %o0 */
#define	SP_REGNUM 14		/* Contains address of top of stack, \
				   which is also the bottom of the frame.  */
#define	RP_REGNUM 15		/* Contains return address value, *before* \
				   any windows get switched.  */
#define	O7_REGNUM 15		/* Last local reg not saved on stack frame */
#define	L0_REGNUM 16		/* First local reg that's saved on stack frame
				   rather than in machine registers */
#define	I0_REGNUM 24		/* %i0 */
#define	FP_REGNUM 30		/* Contains address of executing stack frame */
#define	I7_REGNUM 31		/* Last local reg saved on stack frame */
#define	FP0_REGNUM 32		/* Floating point register 0 */
#endif

/*#define FP_MAX_REGNUM 80*/	/* 1 + last fp reg number */

/* #undef v8 misc. regs */

#undef Y_REGNUM
#undef PS_REGNUM
#undef WIM_REGNUM
#undef TBR_REGNUM
#undef PC_REGNUM
#undef NPC_REGNUM
#undef FPS_REGNUM
#undef CPS_REGNUM

/* v9 misc. and priv. regs */

#define C0_REGNUM 80			/* Start of control registers */

#define PC_REGNUM (C0_REGNUM + 0)	/* Current PC */
#define NPC_REGNUM (C0_REGNUM + 1)	/* Next PC */
#define CCR_REGNUM (C0_REGNUM + 2)	/* Condition Code Register (%xcc,%icc) */
#define FSR_REGNUM (C0_REGNUM + 3)	/* Floating Point State */
#define FPRS_REGNUM (C0_REGNUM + 4)	/* Floating Point Registers State */
#define	Y_REGNUM (C0_REGNUM + 5)	/* Temp register for multiplication, etc.  */
#define ASI_REGNUM (C0_REGNUM + 6)	/* Alternate Space Identifier */
#define VER_REGNUM (C0_REGNUM + 7)	/* Version register */
#define TICK_REGNUM (C0_REGNUM + 8)	/* Tick register */
#define PIL_REGNUM (C0_REGNUM + 9)	/* Processor Interrupt Level */
#define PSTATE_REGNUM (C0_REGNUM + 10)	/* Processor State */
#define TSTATE_REGNUM (C0_REGNUM + 11)	/* Trap State */
#define TBA_REGNUM (C0_REGNUM + 12)	/* Trap Base Address */
#define TL_REGNUM (C0_REGNUM + 13)	/* Trap Level */
#define TT_REGNUM (C0_REGNUM + 14)	/* Trap Type */
#define TPC_REGNUM (C0_REGNUM + 15)	/* Trap pc */
#define TNPC_REGNUM (C0_REGNUM + 16)	/* Trap npc */
#define WSTATE_REGNUM (C0_REGNUM + 17)	/* Window State */
#define CWP_REGNUM (C0_REGNUM + 18)	/* Current Window Pointer */
#define CANSAVE_REGNUM (C0_REGNUM + 19)		/* Savable Windows */
#define CANRESTORE_REGNUM (C0_REGNUM + 20)	/* Restorable Windows */
#define CLEANWIN_REGNUM (C0_REGNUM + 21)	/* Clean Windows */
#define OTHERWIN_REGNUM (C0_REGNUM + 22)	/* Other Windows */
#define ASR_REGNUM(n) (C0_REGNUM+(23-16)+(n))	/* Ancillary State Register
						   (n = 16...31) */
#define ICC_REGNUM (C0_REGNUM + 39)	/* 32 bit condition codes */
#define XCC_REGNUM (C0_REGNUM + 40)	/* 64 bit condition codes */
#define FCC0_REGNUM (C0_REGNUM + 41)	/* fp cc reg 0 */
#define FCC1_REGNUM (C0_REGNUM + 42)	/* fp cc reg 1 */
#define FCC2_REGNUM (C0_REGNUM + 43)	/* fp cc reg 2 */
#define FCC3_REGNUM (C0_REGNUM + 44)	/* fp cc reg 3 */

/* Number of machine registers.  */

#undef  NUM_REGS
#define NUM_REGS 125

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.
   Some of the registers aren't 64 bits, but it's a lot simpler just to assume
   they all are (since most of them are).  */
#undef  REGISTER_BYTES
#define REGISTER_BYTES (32*8+32*8+45*8)

/* Index within `registers' of the first byte of the space for
   register N.  */
#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) \
  ((N) < 32 ? (N)*8				\
   : (N) < 64 ? 32*8 + ((N)-32)*4		\
   : (N) < C0_REGNUM ? 32*8 + 32*4 + ((N)-64)*8	\
   : 64*8 + ((N)-C0_REGNUM)*8)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#undef  REGISTER_SIZE
#define REGISTER_SIZE 8

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) \
  ((N) < 32 ? 8 : (N) < 64 ? 4 : 8)

/* Number of bytes of storage in the program's representation
   for register N.  */

#undef  REGISTER_VIRTUAL_SIZE
#define REGISTER_VIRTUAL_SIZE(N) \
  ((N) < 32 ? 8 : (N) < 64 ? 4 : 8)

/* Largest value REGISTER_RAW_SIZE can have.  */
/* tm-sparc.h defines this as 8, but play it safe.  */

#undef  MAX_REGISTER_RAW_SIZE
#define MAX_REGISTER_RAW_SIZE 8

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */
/* tm-sparc.h defines this as 8, but play it safe.  */

#undef  MAX_REGISTER_VIRTUAL_SIZE
#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#undef  REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
 ((N) < 32 ? builtin_type_long_long \
  : (N) < 64 ? builtin_type_float \
  : (N) < 80 ? builtin_type_double \
  : builtin_type_long_long)

/* We use to support both 32 bit and 64 bit pointers.
   We can't anymore because TARGET_PTR_BIT must now be a constant.  */
#undef  TARGET_PTR_BIT
#define TARGET_PTR_BIT 64

/* Longs are 64 bits. */
#undef TARGET_LONG_BIT
#define TARGET_LONG_BIT 64

#undef TARGET_LONG_LONG_BIT
#define TARGET_LONG_LONG_BIT 64

/* Return number of bytes at start of arglist that are not really args.  */

#undef  FRAME_ARGS_SKIP
#define FRAME_ARGS_SKIP 136

#endif /* GDB_MULTI_ARCH */

/* Offsets into jmp_buf.
   FIXME: This was borrowed from the v8 stuff and will probably have to change
   for v9.  */

#define JB_ELEMENT_SIZE 8	/* Size of each element in jmp_buf */

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_NPC 4
#define JB_PSR 5
#define JB_G1 6
#define JB_O0 7
#define JB_WBCNT 8

/* Figure out where the longjmp will land.  We expect that we have
   just entered longjmp and haven't yet setup the stack frame, so the
   args are still in the output regs.  %o0 (O0_REGNUM) points at the
   jmp_buf structure from which we extract the pc (JB_PC) that we will
   land at.  The pc is copied into ADDR.  This routine returns true on
   success */

extern int get_longjmp_target (CORE_ADDR *);

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)

#undef TM_PRINT_INSN_MACH
#define TM_PRINT_INSN_MACH bfd_mach_sparc_v9a

