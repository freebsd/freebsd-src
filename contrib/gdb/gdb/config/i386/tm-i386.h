/* Macro definitions for GDB on an Intel i[345]86.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

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

#ifndef TM_I386_H
#define TM_I386_H 1

#define GDB_MULTI_ARCH GDB_MULTI_ARCH_PARTIAL

#include "regcache.h"

/* Forward declarations for prototypes.  */
struct frame_info;
struct frame_saved_regs;
struct value;
struct type;

/* The format used for `long double' on almost all i386 targets is the
   i387 extended floating-point format.  In fact, of all targets in the
   GCC 2.95 tree, only OSF/1 does it different, and insists on having
   a `long double' that's not `long' at all.  */

#define TARGET_LONG_DOUBLE_FORMAT &floatformat_i387_ext

/* Although the i386 extended floating-point has only 80 significant
   bits, a `long double' actually takes up 96, probably to enforce
   alignment.  */

#define TARGET_LONG_DOUBLE_BIT 96

/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is 2
   on most implementations. */

#define START_INFERIOR_TRAPS_EXPECTED 2

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions to reach some
   "real" code.  */

#define SKIP_PROLOGUE(frompc)   (i386_skip_prologue (frompc))

extern int i386_skip_prologue (int);

/* Immediately after a function call, return the saved pc.  */

#define SAVED_PC_AFTER_CALL(frame) i386_saved_pc_after_call (frame)
extern CORE_ADDR i386_saved_pc_after_call (struct frame_info *frame);

/* Stack grows downward.  */

#define INNER_THAN(lhs,rhs) ((lhs) < (rhs))

/* Sequence of bytes for breakpoint instruction.  */

#define BREAKPOINT {0xcc}

/* Amount PC must be decremented by after a breakpoint.  This is often the
   number of bytes in BREAKPOINT but not always. */

#define DECR_PC_AFTER_BREAK 1

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* This register file is parameterized by two macros:
   HAVE_I387_REGS --- register file should include i387 registers
   HAVE_SSE_REGS  --- register file should include SSE registers
   If HAVE_SSE_REGS is #defined, then HAVE_I387_REGS must also be #defined.
   
   However, GDB code should not test those macros with #ifdef, since
   that makes code which is annoying to multi-arch.  Instead, GDB code
   should check the values of NUM_GREGS, NUM_FREGS, and NUM_SSE_REGS,
   which will eventually get mapped onto architecture vector entries.

   It's okay to use the macros in tm-*.h files, though, since those
   files will get completely replaced when we multi-arch anyway.  */

/* Number of general registers, present on every 32-bit x86 variant.  */
#define NUM_GREGS (16)

/* Number of floating-point unit registers.  */
#ifdef HAVE_I387_REGS
#define NUM_FREGS (16)
#else
#define NUM_FREGS (0)
#endif

/* Number of SSE registers.  */
#ifdef HAVE_SSE_REGS
#define NUM_SSE_REGS (9)
#else
#define NUM_SSE_REGS (0)
#endif

/* Largest number of registers we could have in any configuration.  */
#define MAX_NUM_REGS (16 + 16 + 9)

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define FP_REGNUM 5		/* (ebp) Contains address of executing stack
				   frame */
#define SP_REGNUM 4		/* (usp) Contains address of top of stack */
#define PC_REGNUM 8		/* (eip) Contains program counter */
#define PS_REGNUM 9		/* (ps)  Contains processor status */

/* First FPU data register.  */
#ifdef HAVE_I387_REGS
#define FP0_REGNUM 16
#else
#define FP0_REGNUM 0
#endif

/* Return the name of register REG.  */

#define REGISTER_NAME(reg) i386_register_name ((reg))
extern char *i386_register_name (int reg);

/* Use the "default" register numbering scheme for stabs and COFF.  */

#define STAB_REG_TO_REGNUM(reg) i386_stab_reg_to_regnum ((reg))
#define SDB_REG_TO_REGNUM(reg) i386_stab_reg_to_regnum ((reg))
extern int i386_stab_reg_to_regnum (int reg);

/* Use the DWARF register numbering scheme for DWARF and DWARF 2.  */

#define DWARF_REG_TO_REGNUM(reg) i386_dwarf_reg_to_regnum ((reg))
#define DWARF2_REG_TO_REGNUM(reg) i386_dwarf_reg_to_regnum ((reg))
extern int i386_dwarf_reg_to_regnum (int reg);

/* We don't define ECOFF_REG_TO_REGNUM, since ECOFF doesn't seem to be
   in use on any of the supported i386 targets.  */


/* Sizes of individual register sets.  These cover the entire register
   file, so summing up the sizes of those portions actually present
   yields REGISTER_BYTES.  */
#define SIZEOF_GREGS (NUM_GREGS * 4)
#define SIZEOF_FPU_REGS (8 * 10)
#define SIZEOF_FPU_CTRL_REGS (8 * 4)
#define SIZEOF_SSE_REGS (8 * 16 + 4)


/* Total amount of space needed to store our copies of the machine's register
   state, the array `registers'. */
#ifdef HAVE_SSE_REGS
#define REGISTER_BYTES \
  (SIZEOF_GREGS + SIZEOF_FPU_REGS + SIZEOF_FPU_CTRL_REGS + SIZEOF_SSE_REGS)
#else
#ifdef HAVE_I387_REGS
#define REGISTER_BYTES (SIZEOF_GREGS + SIZEOF_FPU_REGS + SIZEOF_FPU_CTRL_REGS)
#else
#define REGISTER_BYTES (SIZEOF_GREGS)
#endif
#endif

/* Return the offset into the register array of the start of register
   number REG.  */
#define REGISTER_BYTE(reg) i386_register_byte ((reg))
extern int i386_register_byte (int reg);

/* Return the number of bytes of storage in GDB's register array
   occupied by register REG.  */
#define REGISTER_RAW_SIZE(reg) i386_register_raw_size ((reg))
extern int i386_register_raw_size (int reg);

/* Largest value REGISTER_RAW_SIZE can have.  */
#define MAX_REGISTER_RAW_SIZE 16

/* Return the size in bytes of the virtual type of register REG.  */
#define REGISTER_VIRTUAL_SIZE(reg) i386_register_virtual_size ((reg))
extern int i386_register_virtual_size (int reg);

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */
#define MAX_REGISTER_VIRTUAL_SIZE 16

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM.  */

#define REGISTER_VIRTUAL_TYPE(regnum) i386_register_virtual_type (regnum)
extern struct type *i386_register_virtual_type (int regnum);

/* Return true iff register REGNUM's virtual format is different from
   its raw format.  */

#define REGISTER_CONVERTIBLE(regnum) i386_register_convertible (regnum)
extern int i386_register_convertible (int regnum);

/* Convert data from raw format for register REGNUM in buffer FROM to
   virtual format with type TYPE in buffer TO.  */

#define REGISTER_CONVERT_TO_VIRTUAL(regnum, type, from, to) \
  i386_register_convert_to_virtual ((regnum), (type), (from), (to))
extern void i386_register_convert_to_virtual (int regnum, struct type *type,
					      char *from, char *to);

/* Convert data from virtual format with type TYPE in buffer FROM to
   raw format for register REGNUM in buffer TO.  */

#define REGISTER_CONVERT_TO_RAW(type, regnum, from, to) \
  i386_register_convert_to_raw ((type), (regnum), (from), (to))
extern void i386_register_convert_to_raw (struct type *type, int regnum,
					  char *from, char *to);

/* Print out the i387 floating point state.  */
#ifdef HAVE_I387_REGS
extern void i387_float_info (void);
#define FLOAT_INFO { i387_float_info (); }
#endif


#define PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr) \
  i386_push_arguments ((nargs), (args), (sp), (struct_return), (struct_addr))
extern CORE_ADDR i386_push_arguments (int nargs, struct value **args,
				      CORE_ADDR sp, int struct_return,
				      CORE_ADDR struct_addr);

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function.  */

#define STORE_STRUCT_RETURN(addr, sp) \
  i386_store_struct_return ((addr), (sp))
extern void i386_store_struct_return (CORE_ADDR addr, CORE_ADDR sp);

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(type, regbuf, valbuf) \
  i386_extract_return_value ((type), (regbuf), (valbuf))
extern void i386_extract_return_value (struct type *type, char *regbuf,
				       char *valbuf);

/* Write into the appropriate registers a function return value stored
   in VALBUF of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(type, valbuf) \
  i386_store_return_value ((type), (valbuf))
extern void i386_store_return_value (struct type *type, char *valbuf);

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR.  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(regbuf) \
  i386_extract_struct_value_address ((regbuf))
extern CORE_ADDR i386_extract_struct_value_address (char *regbuf);

/* The following redefines make backtracing through sigtramp work.
   They manufacture a fake sigtramp frame and obtain the saved pc in sigtramp
   from the sigcontext structure which is pushed by the kernel on the
   user stack, along with a pointer to it.  */

/* Return the chain-pointer for FRAME.  In the case of the i386, the
   frame's nominal address is the address of a 4-byte word containing
   the calling frame's address.  */

#define FRAME_CHAIN(frame) i386_frame_chain ((frame))
extern CORE_ADDR i386_frame_chain (struct frame_info *frame);

/* Determine whether the function invocation represented by FRAME does
   not have a from on the stack associated with it.  If it does not,
   return non-zero, otherwise return zero.  */

#define FRAMELESS_FUNCTION_INVOCATION(frame) \
  i386_frameless_function_invocation (frame)
extern int i386_frameless_function_invocation (struct frame_info *frame);

/* Return the saved program counter for FRAME.  */

#define FRAME_SAVED_PC(frame) i386_frame_saved_pc (frame)
extern CORE_ADDR i386_frame_saved_pc (struct frame_info *frame);

#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Return number of args passed to a frame.  Can return -1, meaning no way
   to tell, which is typical now that the C compiler delays popping them.  */

#define FRAME_NUM_ARGS(fi) (i386_frame_num_args(fi))

extern int i386_frame_num_args (struct frame_info *);

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 8

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

extern void i386_frame_init_saved_regs (struct frame_info *);
#define FRAME_INIT_SAVED_REGS(FI) i386_frame_init_saved_regs (FI)



/* Things needed for making the inferior call functions.  */

/* "An argument's size is increased, if necessary, to make it a
   multiple of [32 bit] words.  This may require tail padding,
   depending on the size of the argument" - from the x86 ABI.  */
#define PARM_BOUNDARY 32

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME { i386_push_dummy_frame (); }

extern void i386_push_dummy_frame (void);

/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME  { i386_pop_frame (); }

extern void i386_pop_frame (void);


/* this is 
 *   call 11223344 (32 bit relative)
 *   int3
 */

#define CALL_DUMMY { 0x223344e8, 0xcc11 }

#define CALL_DUMMY_LENGTH 8

#define CALL_DUMMY_START_OFFSET 0	/* Start execution at beginning of dummy */

#define CALL_DUMMY_BREAKPOINT_OFFSET 5

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p) \
  i386_fix_call_dummy (dummyname, pc, fun, nargs, args, type, gcc_p)
extern void i386_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun,
				 int nargs, struct value **args,
				 struct type *type, int gcc_p);

/* FIXME: kettenis/2000-06-12: These do not belong here.  */
extern void print_387_control_word (unsigned int);
extern void print_387_status_word (unsigned int);

/* Offset from SP to first arg on stack at first instruction of a function */

#define SP_ARG0 (1 * 4)

#endif /* ifndef TM_I386_H */
