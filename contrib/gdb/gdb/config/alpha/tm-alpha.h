/* Definitions to make GDB run on an Alpha box under OSF1.  This is
   also used by the Alpha/Netware and Alpha/Linux targets.
   Copyright 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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

#ifndef TM_ALPHA_H
#define TM_ALPHA_H

#include "bfd.h"
#include "coff/sym.h"		/* Needed for PDR below.  */
#include "coff/symconst.h"

#ifdef __STDC__
struct frame_info;
struct type;
struct value;
struct symbol;
#endif

#if !defined (TARGET_BYTE_ORDER)
#define TARGET_BYTE_ORDER LITTLE_ENDIAN
#endif

/* Redefine some target bit sizes from the default.  */

#define TARGET_LONG_BIT 64
#define TARGET_LONG_LONG_BIT 64
#define TARGET_PTR_BIT 64

/* Floating point is IEEE compliant */
#define IEEE_FLOAT

/* Number of traps that happen between exec'ing the shell 
 * to run an inferior, and when we finally get to 
 * the inferior code.  This is 2 on most implementations.
 */
#define START_INFERIOR_TRAPS_EXPECTED 3

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(pc)	pc = alpha_skip_prologue(pc, 0)
extern CORE_ADDR alpha_skip_prologue PARAMS ((CORE_ADDR addr, int lenient));

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame)	alpha_saved_pc_after_call(frame)
extern CORE_ADDR
alpha_saved_pc_after_call PARAMS ((struct frame_info *));

/* Are we currently handling a signal ?  */

#define IN_SIGTRAMP(pc, name)	((name) && STREQ ("__sigtramp", (name)))

/* Stack grows downward.  */

#define INNER_THAN(lhs,rhs) ((lhs) < (rhs))

#define BREAKPOINT {0x80, 0, 0, 0} /* call_pal bpt */

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#ifndef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 4
#endif

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 8

/* Number of machine registers */

#define NUM_REGS 66

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES 	\
    {	"v0",	"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6", \
	"t7",	"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"fp", \
	"a0",	"a1",	"a2",	"a3",	"a4",	"a5",	"t8",	"t9", \
	"t10",	"t11",	"ra",	"t12",	"at",	"gp",	"sp",	"zero", \
	"f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7", \
	"f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15", \
	"f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",\
	"f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",\
	"pc",	"vfp",						\
    }

/* Register numbers of various important registers.
   Note that most of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and FP_REGNUM is a "phony" register number which is too large
   to be an actual register number as far as the user is concerned
   but serves to get the desired value when passed to read_register.  */

#define V0_REGNUM 0		/* Function integer return value */
#define T7_REGNUM 8		/* Return address register for OSF/1 __add* */
#define GCC_FP_REGNUM 15	/* Used by gcc as frame register */
#define A0_REGNUM 16		/* Loc of first arg during a subr call */
#define T9_REGNUM 23		/* Return address register for OSF/1 __div* */
#define T12_REGNUM 27		/* Contains start addr of current proc */
#define SP_REGNUM 30		/* Contains address of top of stack */
#define RA_REGNUM 26		/* Contains return address value */
#define ZERO_REGNUM 31		/* Read-only register, always 0 */
#define FP0_REGNUM 32           /* Floating point register 0 */
#define FPA0_REGNUM 48          /* First float arg during a subr call */
#define PC_REGNUM 64		/* Contains program counter */
#define FP_REGNUM 65		/* Virtual frame pointer */

#define CANNOT_FETCH_REGISTER(regno) \
  ((regno) == FP_REGNUM || (regno) == ZERO_REGNUM)
#define CANNOT_STORE_REGISTER(regno) \
  ((regno) == FP_REGNUM || (regno) == ZERO_REGNUM)

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */
#define REGISTER_BYTES (NUM_REGS * 8)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N) ((N) * 8)

/* Number of bytes of storage in the actual machine representation
   for register N.  On Alphas, all regs are 8 bytes.  */

#define REGISTER_RAW_SIZE(N) 8

/* Number of bytes of storage in the program's representation
   for register N.  On Alphas, all regs are 8 bytes.  */

#define REGISTER_VIRTUAL_SIZE(N) 8

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 8

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Nonzero if register N requires conversion
   from raw format to virtual format.
   The alpha needs a conversion between register and memory format if
   the register is a floating point register and
      memory format is float, as the register format must be double
   or
      memory format is an integer with 4 bytes or less, as the representation
      of integers in floating point registers is different. */

#define REGISTER_CONVERTIBLE(N) ((N) >= FP0_REGNUM && (N) < FP0_REGNUM + 32)

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM, TYPE, FROM, TO) \
  alpha_register_convert_to_virtual (REGNUM, TYPE, FROM, TO)
extern void
alpha_register_convert_to_virtual PARAMS ((int, struct type *, char *, char *));

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#define REGISTER_CONVERT_TO_RAW(TYPE, REGNUM, FROM, TO)	\
  alpha_register_convert_to_raw (TYPE, REGNUM, FROM, TO)
extern void
alpha_register_convert_to_raw PARAMS ((struct type *, int, char *, char *));

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
	(((N) >= FP0_REGNUM && (N) < FP0_REGNUM+32)  \
	 ? builtin_type_double : builtin_type_long) \

/* Store the address of the place in which to copy the structure the
   subroutine will return.  Handled by alpha_push_arguments.  */

#define STORE_STRUCT_RETURN(addr, sp)	/**/

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  alpha_extract_return_value(TYPE, REGBUF, VALBUF)
extern void
alpha_extract_return_value PARAMS ((struct type *, char *, char *));

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  alpha_store_return_value(TYPE, VALBUF)
extern void
alpha_store_return_value PARAMS ((struct type *, char *));

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */
/* The address is passed in a0 upon entry to the function, but when
   the function exits, the compiler has copied the value to v0.  This
   convention is specified by the System V ABI, so I think we can rely
   on it.  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) \
  (extract_address (REGBUF + REGISTER_BYTE (V0_REGNUM), \
		    REGISTER_RAW_SIZE (V0_REGNUM)))

/* Structures are returned by ref in extra arg0 */
#define USE_STRUCT_CONVENTION(gcc_p, type)	1


/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer. */

#define FRAME_CHAIN(thisframe) (CORE_ADDR) alpha_frame_chain (thisframe)
extern CORE_ADDR alpha_frame_chain PARAMS ((struct frame_info *));

/* Define other aspects of the stack frame.  */


/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
/* We handle this differently for alpha, and maybe we should not */

#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS)  {(FRAMELESS) = 0;}

/* Saved Pc.  */

#define FRAME_SAVED_PC(FRAME)	(alpha_frame_saved_pc(FRAME))
extern CORE_ADDR
alpha_frame_saved_pc PARAMS ((struct frame_info *));

/* The alpha has two different virtual pointers for arguments and locals.

   The virtual argument pointer is pointing to the bottom of the argument
   transfer area, which is located immediately below the virtual frame
   pointer. Its size is fixed for the native compiler, it is either zero
   (for the no arguments case) or large enough to hold all argument registers.
   gcc uses a variable sized argument transfer area. As it has
   to stay compatible with the native debugging tools it has to use the same
   virtual argument pointer and adjust the argument offsets accordingly.

   The virtual local pointer is localoff bytes below the virtual frame
   pointer, the value of localoff is obtained from the PDR.  */

#define ALPHA_NUM_ARG_REGS	6

#define FRAME_ARGS_ADDRESS(fi)	((fi)->frame - (ALPHA_NUM_ARG_REGS * 8))

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame - (fi)->localoff)

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#define FRAME_NUM_ARGS(num, fi)	((num) = -1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 0

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

extern void alpha_find_saved_regs PARAMS ((struct frame_info *));

#define FRAME_INIT_SAVED_REGS(frame_info) \
  do { \
    if ((frame_info)->saved_regs == NULL) \
      alpha_find_saved_regs (frame_info); \
    (frame_info)->saved_regs[SP_REGNUM] = (frame_info)->frame; \
  } while (0)


/* Things needed for making the inferior call functions.  */

#define PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr) \
    sp = alpha_push_arguments((nargs), (args), (sp), (struct_return), (struct_addr))
extern CORE_ADDR
alpha_push_arguments PARAMS ((int, struct value **, CORE_ADDR, int, CORE_ADDR));

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME 	alpha_push_dummy_frame()
extern void
alpha_push_dummy_frame PARAMS ((void));

/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME		alpha_pop_frame()
extern void
alpha_pop_frame PARAMS ((void));

/* Alpha OSF/1 inhibits execution of code on the stack.
   But there is no need for a dummy on the alpha. PUSH_ARGUMENTS
   takes care of all argument handling and bp_call_dummy takes care
   of stopping the dummy.  */

#define CALL_DUMMY_LOCATION AT_ENTRY_POINT

/* On the Alpha the call dummy code is never copied to user space,
   stopping the user call is achieved via a bp_call_dummy breakpoint.
   But we need a fake CALL_DUMMY definition to enable the proper
   call_function_by_hand and to avoid zero length array warnings
   in valops.c  */

#define CALL_DUMMY { 0 }	/* Content doesn't matter. */

#define CALL_DUMMY_START_OFFSET (0)

#define CALL_DUMMY_BREAKPOINT_OFFSET (0)

extern CORE_ADDR alpha_call_dummy_address PARAMS ((void));
#define CALL_DUMMY_ADDRESS() alpha_call_dummy_address()

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.
   We only have to set RA_REGNUM to the dummy breakpoint address
   and T12_REGNUM (the `procedure value register') to the function address.  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)    \
{									\
  CORE_ADDR bp_address = CALL_DUMMY_ADDRESS ();			\
  if (bp_address == 0)							\
    error ("no place to put call");					\
  write_register (RA_REGNUM, bp_address);				\
  write_register (T12_REGNUM, fun);					\
}

/* There's a mess in stack frame creation.  See comments in blockframe.c
   near reference to INIT_FRAME_PC_FIRST.  */

#define	INIT_FRAME_PC(fromleaf, prev) /* nada */

#define INIT_FRAME_PC_FIRST(fromleaf, prev) \
  (prev)->pc = ((fromleaf) ? SAVED_PC_AFTER_CALL ((prev)->next) : \
	      (prev)->next ? FRAME_SAVED_PC ((prev)->next) : read_pc ());

/* Special symbol found in blocks associated with routines.  We can hang
   alpha_extra_func_info_t's off of this.  */

#define MIPS_EFI_SYMBOL_NAME "__GDB_EFI_INFO__"
extern void ecoff_relocate_efi PARAMS ((struct symbol *, CORE_ADDR));

/* Specific information about a procedure.
   This overlays the ALPHA's PDR records, 
   alpharead.c (ab)uses this to save memory */

typedef struct alpha_extra_func_info {
	long	numargs;	/* number of args to procedure (was iopt) */
	PDR	pdr;		/* Procedure descriptor record */
} *alpha_extra_func_info_t;

/* Define the extra_func_info that mipsread.c needs.
   FIXME: We should define our own PDR interface, perhaps in a separate
   header file. This would get rid of the <bfd.h> inclusion in all sources
   and would abstract the mips/alpha interface from ecoff.  */
#define mips_extra_func_info alpha_extra_func_info
#define mips_extra_func_info_t alpha_extra_func_info_t

#define EXTRA_FRAME_INFO \
  int localoff; \
  int pc_reg; \
  alpha_extra_func_info_t proc_desc;

#define INIT_EXTRA_FRAME_INFO(fromleaf, fci) init_extra_frame_info(fci)
extern void
init_extra_frame_info PARAMS ((struct frame_info *));

#define	PRINT_EXTRA_FRAME_INFO(fi) \
  { \
    if (fi && fi->proc_desc && fi->proc_desc->pdr.framereg < NUM_REGS) \
      printf_filtered (" frame pointer is at %s+%d\n", \
                       REGISTER_NAME (fi->proc_desc->pdr.framereg), \
                                 fi->proc_desc->pdr.frameoffset); \
  }

/* It takes two values to specify a frame on the ALPHA.  Sigh.

   In fact, at the moment, the *PC* is the primary value that sets up
   a frame.  The PC is looked up to see what function it's in; symbol
   information from that function tells us which register is the frame
   pointer base, and what offset from there is the "virtual frame pointer".
   (This is usually an offset from SP.)  FIXME -- this should be cleaned
   up so that the primary value is the SP, and the PC is used to disambiguate
   multiple functions with the same SP that are at different stack levels. */

#define SETUP_ARBITRARY_FRAME(argc, argv) setup_arbitrary_frame (argc, argv)
extern struct frame_info *setup_arbitrary_frame PARAMS ((int, CORE_ADDR *));

/* This is used by heuristic_proc_start.  It should be shot it the head.  */
#ifndef __FreeBSD__
#ifndef VM_MIN_ADDRESS
#define VM_MIN_ADDRESS (CORE_ADDR)0x120000000
#endif
#endif

/* If PC is in a shared library trampoline code, return the PC
   where the function itself actually starts.  If not, return 0.  */
#define SKIP_TRAMPOLINE_CODE(pc)  find_solib_trampoline_target (pc)

/* If the current gcc for for this target does not produce correct debugging
   information for float parameters, both prototyped and unprototyped, then
   define this macro.  This forces gdb to  always assume that floats are
   passed as doubles and then converted in the callee.

   For the alpha, it appears that the debug info marks the parameters as
   floats regardless of whether the function is prototyped, but the actual
   values are always passed in as doubles.  Thus by setting this to 1, both
   types of calls will work. */

#define COERCE_FLOAT_TO_DOUBLE 1

/* Return TRUE if procedure descriptor PROC is a procedure descriptor
   that refers to a dynamically generated sigtramp function.

   OSF/1 doesn't use dynamic sigtramp functions, so this is always
   FALSE.  */

#define PROC_DESC_IS_DYN_SIGTRAMP(proc)	(0)
#define SET_PROC_DESC_IS_DYN_SIGTRAMP(proc)

/* If PC is inside a dynamically generated sigtramp function, return
   how many bytes the program counter is beyond the start of that
   function.  Otherwise, return a negative value.

   OSF/1 doesn't use dynamic sigtramp functions, so this always
   returns -1.  */

#define DYNAMIC_SIGTRAMP_OFFSET(pc)	(-1)

/* Translate a signal handler frame into the address of the sigcontext
   structure.  */

#define SIGCONTEXT_ADDR(frame) \
  (read_memory_integer ((frame)->next ? frame->next->frame : frame->frame, 8))

/* If FRAME refers to a sigtramp frame, return the address of the next
   frame.  */

#define FRAME_PAST_SIGTRAMP_FRAME(frame, pc) \
  (alpha_osf_skip_sigtramp_frame (frame, pc))
extern CORE_ADDR alpha_osf_skip_sigtramp_frame PARAMS ((struct frame_info *, CORE_ADDR));

#endif /* TM_ALPHA_H */
