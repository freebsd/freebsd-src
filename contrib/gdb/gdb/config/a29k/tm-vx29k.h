/* Target machine description for VxWorks on the 29k, for GDB, the GNU debugger.
   Copyright 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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

#include "a29k/tm-a29k.h"

#define	GDBINIT_FILENAME	".vxgdbinit"

#define	DEFAULT_PROMPT		"(vxgdb) "

/* Number of registers in a ptrace_getregs call. */

#define VX_NUM_REGS (NUM_REGS)

/* Number of registers in a ptrace_getfpregs call. */

/* #define VX_SIZE_FPREGS */

/* This is almost certainly the wrong place for this: */
#define LR2_REGNUM 34


/* Vxworks has its own CALL_DUMMY since it manages breakpoints in the kernel */

#undef CALL_DUMMY

/* Replace the breakpoint instruction in the CALL_DUMMY with a nop.
   For Vxworks, the breakpoint is set and deleted by calls to
   CALL_DUMMY_BREAK_SET and CALL_DUMMY_BREAK_DELETE.  */

#if TARGET_BYTE_ORDER == HOST_BYTE_ORDER
#define CALL_DUMMY {0x0400870f,\
		0x36008200|(MSP_HW_REGNUM), \
		0x15000040|(MSP_HW_REGNUM<<8)|(MSP_HW_REGNUM<<16), \
		0x03ff80ff, 0x02ff80ff, 0xc8008080, 0x70400101, 0x70400101}
#else /* Byte order differs.  */
#define CALL_DUMMY {0x0f870004,\
		0x00820036|(MSP_HW_REGNUM << 24), \
		0x40000015|(MSP_HW_REGNUM<<8)|(MSP_HW_REGNUM<<16), \
		0xff80ff03, 0xff80ff02, 0x808000c8, 0x01014070, 0x01014070}
#endif /* Byte order differs.  */


/* For the basic CALL_DUMMY definitions, see "tm-29k.h."  We use the
   same CALL_DUMMY code, but define FIX_CALL_DUMMY (and related macros)
   locally to handle remote debugging of VxWorks targets.  The difference
   is in the setting and clearing of the breakpoint at the end of the
   CALL_DUMMY code fragment; under VxWorks, we can't simply insert a
   breakpoint instruction into the code, since that would interfere with
   the breakpoint management mechanism on the target.
   Note that CALL_DUMMY is a piece of code that is used to call any C function
   thru VxGDB */

/* The offset of the instruction within the CALL_DUMMY code where we
   want the inferior to stop after the function call has completed.
   call_function_by_hand () sets a breakpoint here (via CALL_DUMMY_BREAK_SET),
   which POP_FRAME later deletes (via CALL_DUMMY_BREAK_DELETE).  */
 
#define CALL_DUMMY_STOP_OFFSET (7 * 4)
 
/* The offset of the first instruction of the CALL_DUMMY code fragment
   relative to the frame pointer for a dummy frame.  This is equal to
   the size of the CALL_DUMMY plus the arg_slop area size (see the diagram
   in "tm-29k.h").  */
/* PAD : the arg_slop area size doesn't appear to me to be useful since, the
   call dummy code no longer modify the msp. See below. This must be checked. */

#define CALL_DUMMY_OFFSET_IN_FRAME (CALL_DUMMY_LENGTH + 16 * 4)

/* Insert the specified number of args and function address
   into a CALL_DUMMY sequence stored at DUMMYNAME, replace the third
   instruction (add msp, msp, 16*4) with a nop, and leave the final nop.
   We can't keep using a CALL_DUMMY that modify the msp since, for VxWorks,
   CALL_DUMMY is stored in the Memory Stack. Adding 16 words to the msp
   would then make possible for the inferior to overwrite the CALL_DUMMY code,
   thus creating a lot of trouble when exiting the inferior to come back in
   a CALL_DUMMY code that no longer exists... Furthermore, ESF are also stored
   from the msp in the memory stack. If msp is set higher than the dummy code,
   an ESF may clobber this code. */

#if TARGET_BYTE_ORDER == BIG_ENDIAN
#define NOP_INSTR  0x70400101
#else /* Target is little endian */
#define NOP_INSTR  0x01014070
#endif

#undef FIX_CALL_DUMMY
#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)  \
  {                                                                   \
    *(int *)((char *)dummyname + 8) = NOP_INSTR;                      \
    STUFF_I16((char *)dummyname + CONST_INSN, fun);                   \
    STUFF_I16((char *)dummyname + CONST_INSN + 4, fun >> 16);         \
  }

/* For VxWorks, CALL_DUMMY must be stored in the stack of the task that is
   being debugged and executed "in the context of" this task */

#undef CALL_DUMMY_LOCATION
#define CALL_DUMMY_LOCATION     ON_STACK

/* Set or delete a breakpoint at the location within a CALL_DUMMY code
   fragment where we want the target program to stop after the function
   call is complete.  CALL_DUMMY_ADDR is the address of the first
   instruction in the CALL_DUMMY.  DUMMY_FRAME_ADDR is the value of the
   frame pointer in the dummy frame.

   NOTE: in the both of the following definitions, we take advantage of
	 knowledge of the implementation of the target breakpoint operation,
	 in that we pass a null pointer as the second argument.  It seems
	 reasonable to assume that any target requiring the use of 
	 CALL_DUMMY_BREAK_{SET,DELETE} will not store the breakpoint
	 shadow contents in GDB; in any case, this assumption is vaild
	 for all VxWorks-related targets.  */

#define CALL_DUMMY_BREAK_SET(call_dummy_addr) \
  target_insert_breakpoint ((call_dummy_addr) + CALL_DUMMY_STOP_OFFSET, \
			    (char *) 0)

#define CALL_DUMMY_BREAK_DELETE(dummy_frame_addr) \
  target_remove_breakpoint ((dummy_frame_addr) - (CALL_DUMMY_OFFSET_IN_FRAME \
				                  - CALL_DUMMY_STOP_OFFSET), \
			    (char *) 0)

/* Return nonzero if the pc is executing within a CALL_DUMMY frame.  */

#define PC_IN_CALL_DUMMY(pc, sp, frame_address) \
  ((pc) >= (sp) \
    && (pc) <= (sp) + CALL_DUMMY_OFFSET_IN_FRAME + CALL_DUMMY_LENGTH)

/* Defining this prevents us from trying to pass a structure-valued argument
   to a function called via the CALL_DUMMY mechanism.  This is not handled
   properly in call_function_by_hand (), and the fix might require re-writing
   the CALL_DUMMY handling for all targets (at least, a clean solution
   would probably require this).  Arguably, this should go in "tm-29k.h"
   rather than here.  */
   
#define STRUCT_VAL_ARGS_UNSUPPORTED

#define BKPT_OFFSET	(7 * 4)
#define BKPT_INSTR	0x72500101

#undef FIX_CALL_DUMMY
#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)   \
  {\
    STUFF_I16((char *)dummyname + CONST_INSN, fun);\
    STUFF_I16((char *)dummyname + CONST_INSN + 4, fun >> 16);\
    *(int *)((char *)dummyname + BKPT_OFFSET) = BKPT_INSTR;\
  }


/* Offsets into jmp_buf.  They are derived from VxWorks' REG_SET struct
   (see VxWorks' setjmp.h). Note that Sun2, Sun3 and SunOS4 and VxWorks have
   different REG_SET structs, hence different layouts for the jmp_buf struct.
   Only JB_PC is needed for getting the saved PC value.  */

#define JB_ELEMENT_SIZE 4       /* size of each element in jmp_buf */
#define JB_PC		3	/* offset of pc (pc1) in jmp_buf */
 
/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  lr2 (LR2_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int get_longjmp_target PARAMS ((CORE_ADDR *));

/* VxWorks adjusts the PC after a breakpoint has been hit.  */
 
#undef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 0

/* Do whatever promotions are appropriate on a value being returned
   from a function.  VAL is the user-supplied value, and FUNC_TYPE
   is the return type of the function if known, else 0.
 
   For the Am29k, as far as I understand, if the function return type is known,
   cast the value to that type; otherwise, ensure that integer return values
   fill all of gr96.

   This definition really belongs in "tm-29k.h", since it applies
   to most Am29K-based systems; but once moved into that file, it might
   need to be redefined for all Am29K-based targets that also redefine
   STORE_RETURN_VALUE.  For now, to be safe, we define it here.  */
 
#define PROMOTE_RETURN_VALUE(val, func_type) \
  do {                                                                  \
      if (func_type)                                                    \
        val = value_cast (func_type, val);                              \
      if ((TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_INT                \
           || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_ENUM)           \
          && TYPE_LENGTH (VALUE_TYPE (val)) < REGISTER_RAW_SIZE (0))    \
        val = value_cast (builtin_type_int, val);                       \
  } while (0)

#define SPECIAL_FRAME_CHAIN_FP get_fp_contents
#undef FRAME_CHAIN_VALID
#define FRAME_CHAIN_VALID(chain, thisframe) \
  (SPECIAL_FRAME_CHAIN_FP (chain, thisframe))

extern int SPECIAL_FRAME_CHAIN_FP ();

extern CORE_ADDR frame_saved_call_site ();

#undef PREPARE_TO_INIT_FRAME_INFO
#define PREPARE_TO_INIT_FRAME_INFO(fci)	do {				      \
  long current_msp = read_register (MSP_REGNUM);			      \
  if (PC_IN_CALL_DUMMY (fci->pc, current_msp, 0))			      \
    {									      \
      fci->rsize = DUMMY_FRAME_RSIZE;					      \
      fci->msize = 0;							      \
      fci->saved_msp =	 						      \
	read_register_stack_integer (fci->frame + DUMMY_FRAME_RSIZE - 4, 4);  \
      fci->flags |= (TRANSPARENT|MFP_USED);				      \
      return;								      \
    }									      \
  } while (0)
