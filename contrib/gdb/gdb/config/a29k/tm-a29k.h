/* Parameters for target machine AMD 29000, for GDB, the GNU debugger.
   Copyright 1990, 1991, 1993, 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Jim Kingdon.

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

/* Parameters for an EB29K (a board which plugs into a PC and is
   accessed through EBMON software running on the PC, which we
   use as we'd use a remote stub (see remote-eb.c).

   If gdb is ported to other a29k machines/systems, the
   machine/system-specific parts should be removed from this file (a
   la tm-m68k.h).  */

/* Byte order is configurable, but this machine runs big-endian.  */
#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Floating point uses IEEE representations.  */
#define IEEE_FLOAT

/* Recognize our magic number.  */
#define BADMAG(x) ((x).f_magic != 0572)

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(pc) \
  { pc = skip_prologue (pc); }
CORE_ADDR skip_prologue ();

/* Immediately after a function call, return the saved pc.
   Can't go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) ((frame->flags & TRANSPARENT) \
				    ? read_register (TPC_REGNUM) \
				    : read_register (LR0_REGNUM))

/* Stack grows downward.  */

#define INNER_THAN <

/* Stack must be aligned on 32-bit word boundaries.  */
#define STACK_ALIGN(ADDR) (((ADDR) + 3) & ~3)

/* Sequence of bytes for breakpoint instruction.  */
/* ASNEQ 0x50, gr1, gr1
   The trap number 0x50 is chosen arbitrarily.
   We let the command line (or previously included files) override this
   setting.  */
#ifndef BREAKPOINT
#if TARGET_BYTE_ORDER == BIG_ENDIAN
#define BREAKPOINT {0x72, 0x50, 0x01, 0x01}
#else /* Target is little-endian.  */
#define BREAKPOINT {0x01, 0x01, 0x50, 0x72}
#endif /* Target is little-endian.  */
#endif /* BREAKPOINT */

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_BREAK 0

/* Nonzero if instruction at PC is a return instruction.
   On the a29k, this is a "jmpi l0" instruction.  */

#define ABOUT_TO_RETURN(pc) \
  ((read_memory_integer (pc, 4) & 0xff0000ff) == 0xc0000080)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* Allow the register declarations here to be overridden for remote
   kernel debugging.  */
#if !defined (REGISTER_NAMES)

/* Number of machine registers */

#define NUM_REGS 205

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.

   FIXME, add floating point registers and support here.

   Also note that this list does not attempt to deal with kernel
   debugging (in which the first 32 registers are gr64-gr95).  */

#define REGISTER_NAMES \
{"gr96", "gr97", "gr98", "gr99", "gr100", "gr101", "gr102", "gr103", "gr104", \
 "gr105", "gr106", "gr107", "gr108", "gr109", "gr110", "gr111", "gr112", \
 "gr113", "gr114", "gr115", "gr116", "gr117", "gr118", "gr119", "gr120", \
 "gr121", "gr122", "gr123", "gr124", "gr125", "gr126", "gr127",		 \
 "lr0", "lr1", "lr2", "lr3", "lr4", "lr5", "lr6", "lr7", "lr8", "lr9",   \
 "lr10", "lr11", "lr12", "lr13", "lr14", "lr15", "lr16", "lr17", "lr18", \
 "lr19", "lr20", "lr21", "lr22", "lr23", "lr24", "lr25", "lr26", "lr27", \
 "lr28", "lr29", "lr30", "lr31", "lr32", "lr33", "lr34", "lr35", "lr36", \
 "lr37", "lr38", "lr39", "lr40", "lr41", "lr42", "lr43", "lr44", "lr45", \
 "lr46", "lr47", "lr48", "lr49", "lr50", "lr51", "lr52", "lr53", "lr54", \
 "lr55", "lr56", "lr57", "lr58", "lr59", "lr60", "lr61", "lr62", "lr63", \
 "lr64", "lr65", "lr66", "lr67", "lr68", "lr69", "lr70", "lr71", "lr72", \
 "lr73", "lr74", "lr75", "lr76", "lr77", "lr78", "lr79", "lr80", "lr81", \
 "lr82", "lr83", "lr84", "lr85", "lr86", "lr87", "lr88", "lr89", "lr90", \
 "lr91", "lr92", "lr93", "lr94", "lr95", "lr96", "lr97", "lr98", "lr99", \
 "lr100", "lr101", "lr102", "lr103", "lr104", "lr105", "lr106", "lr107", \
 "lr108", "lr109", "lr110", "lr111", "lr112", "lr113", "lr114", "lr115", \
 "lr116", "lr117", "lr118", "lr119", "lr120", "lr121", "lr122", "lr123", \
 "lr124", "lr125", "lr126", "lr127",					 \
  "AI0", "AI1", "AI2", "AI3", "AI4", "AI5", "AI6", "AI7", "AI8", "AI9",  \
  "AI10", "AI11", "AI12", "AI13", "AI14", "AI15", "FP",			 \
  "bp", "fc", "cr", "q",						 \
  "vab", "ops", "cps", "cfg", "cha", "chd", "chc", "rbp", "tmc", "tmr",	 \
  "pc0", "pc1", "pc2", "mmu", "lru", "fpe", "inte", "fps", "exo", "gr1",  \
  "alu", "ipc", "ipa", "ipb" }

/*
 * Converts an sdb register number to an internal gdb register number.
 * Currently under epi, gr96->0...gr127->31...lr0->32...lr127->159, or...
 * 		  	gr64->0...gr95->31, lr0->32...lr127->159.
 */
#define SDB_REG_TO_REGNUM(value) \
  (((value) >= 96 && (value) <= 127) ? ((value) - 96) : \
   ((value) >= 128 && (value) <=  255) ? ((value) - 128 + LR0_REGNUM) : \
   (value))

/*
 * Provide the processor register numbers of some registers that are
 * expected/written in instructions that might change under different
 * register sets.  Namely, gcc can compile (-mkernel-registers) so that
 * it uses gr64-gr95 in stead of gr96-gr127.
 */
#define MSP_HW_REGNUM	125		/* gr125 */
#define RAB_HW_REGNUM	126		/* gr126 */

/* Convert Processor Special register #x to REGISTER_NAMES register # */
#define SR_REGNUM(x) \
  ((x) < 15  ? VAB_REGNUM + (x)					 \
   : (x) >= 128 && (x) < 131 ? IPC_REGNUM + (x) - 128		 \
   : (x) == 131 ? Q_REGNUM					 \
   : (x) == 132 ? ALU_REGNUM					 \
   : (x) >= 133 && (x) < 136 ? BP_REGNUM + (x) - 133		 \
   : (x) >= 160 && (x) < 163 ? FPE_REGNUM + (x) - 160		 \
   : (x) == 164 ? EXO_REGNUM                                     \
   : (error ("Internal error in SR_REGNUM"), 0))
#define GR96_REGNUM 0

/* Define the return register separately, so it can be overridden for
   kernel procedure calling conventions. */
#define	RETURN_REGNUM	GR96_REGNUM
#define GR1_REGNUM 200
/* This needs to be the memory stack pointer, not the register stack pointer,
   to make call_function work right.  */
#define SP_REGNUM MSP_REGNUM
#define FP_REGNUM 33 /* lr1 */

/* Return register for transparent calling convention (gr122).  */
#define TPC_REGNUM (122 - 96 + GR96_REGNUM)

/* Large Return Pointer (gr123).  */
#define LRP_REGNUM (123 - 96 + GR96_REGNUM)

/* Static link pointer (gr124).  */
#define SLP_REGNUM (124 - 96 + GR96_REGNUM)

/* Memory Stack Pointer (gr125).  */
#define MSP_REGNUM (125 - 96 + GR96_REGNUM)

/* Register allocate bound (gr126).  */
#define RAB_REGNUM (126 - 96 + GR96_REGNUM)

/* Register Free Bound (gr127).  */
#define RFB_REGNUM (127 - 96 + GR96_REGNUM)

/* Register Stack Pointer.  */
#define RSP_REGNUM GR1_REGNUM
#define LR0_REGNUM 32
#define BP_REGNUM 177
#define FC_REGNUM 178
#define CR_REGNUM 179
#define Q_REGNUM 180
#define VAB_REGNUM 181
#define OPS_REGNUM (VAB_REGNUM + 1)
#define CPS_REGNUM (VAB_REGNUM + 2)
#define CFG_REGNUM (VAB_REGNUM + 3)
#define CHA_REGNUM (VAB_REGNUM + 4)
#define CHD_REGNUM (VAB_REGNUM + 5)
#define CHC_REGNUM (VAB_REGNUM + 6)
#define RBP_REGNUM (VAB_REGNUM + 7)
#define TMC_REGNUM (VAB_REGNUM + 8)
#define TMR_REGNUM (VAB_REGNUM + 9)
#define NPC_REGNUM (VAB_REGNUM + 10)  /* pc0 */
#define PC_REGNUM  (VAB_REGNUM + 11)  /* pc1 */
#define PC2_REGNUM (VAB_REGNUM + 12)
#define MMU_REGNUM (VAB_REGNUM + 13)
#define LRU_REGNUM (VAB_REGNUM + 14)
#define FPE_REGNUM (VAB_REGNUM + 15)
#define INTE_REGNUM (VAB_REGNUM + 16)
#define FPS_REGNUM (VAB_REGNUM + 17)
#define EXO_REGNUM (VAB_REGNUM + 18)
/* gr1 is defined above as 200 = VAB_REGNUM + 19 */
#define ALU_REGNUM (VAB_REGNUM + 20)
#define PS_REGNUM  ALU_REGNUM
#define IPC_REGNUM (VAB_REGNUM + 21)
#define IPA_REGNUM (VAB_REGNUM + 22)
#define IPB_REGNUM (VAB_REGNUM + 23)

#endif	/* !defined(REGISTER_NAMES) */

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */
#define REGISTER_BYTES (NUM_REGS * 4)

/* Index within `registers' of the first byte of the space for
   register N.  */
#define REGISTER_BYTE(N)  ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

/* All regs are 4 bytes.  */

#define REGISTER_RAW_SIZE(N) (4)

/* Number of bytes of storage in the program's representation
   for register N.  */

/* All regs are 4 bytes.  */

#define REGISTER_VIRTUAL_SIZE(N) (4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE (4)

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE (4)

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
  (((N) == PC_REGNUM || (N) == LRP_REGNUM || (N) == SLP_REGNUM         \
    || (N) == MSP_REGNUM || (N) == RAB_REGNUM || (N) == RFB_REGNUM     \
    || (N) == GR1_REGNUM || (N) == FP_REGNUM || (N) == LR0_REGNUM       \
    || (N) == NPC_REGNUM || (N) == PC2_REGNUM)                           \
   ? lookup_pointer_type (builtin_type_void) : builtin_type_int)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */
/* On the a29k the LRP points to the part of the structure beyond the first
   16 words.  */
#define STORE_STRUCT_RETURN(ADDR, SP) \
  write_register (LRP_REGNUM, (ADDR) + 16 * 4);

/* Should call_function allocate stack space for a struct return?  */
/* On the a29k objects over 16 words require the caller to allocate space.  */
#define USE_STRUCT_CONVENTION(gcc_p, type) (TYPE_LENGTH (type) > 16 * 4)

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)	      \
  {    	       	       	       	       	       	       	       	       	   \
    int reg_length = TYPE_LENGTH (TYPE);				   \
    if (reg_length > 16 * 4)						   \
      {									   \
	reg_length = 16 * 4;						   \
	read_memory (*((int *)(REGBUF) + LRP_REGNUM), (VALBUF) + 16 * 4,   \
		     TYPE_LENGTH (TYPE) - 16 * 4);			   \
      }									   \
    memcpy ((VALBUF), ((int *)(REGBUF))+RETURN_REGNUM, reg_length);	   \
  }

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  {									  \
    int reg_length = TYPE_LENGTH (TYPE);				  \
    if (reg_length > 16 * 4)						  \
      {									  \
        reg_length = 16 * 4;						  \
        write_memory (read_register (LRP_REGNUM),			  \
		      (char *)(VALBUF) + 16 * 4,			  \
		      TYPE_LENGTH (TYPE) - 16 * 4);			  \
      }									  \
    write_register_bytes (REGISTER_BYTE (RETURN_REGNUM), (char *)(VALBUF),  \
			  TYPE_LENGTH (TYPE));				  \
  }

/* The a29k user's guide documents well what the stacks look like.
   But what isn't so clear there is how this interracts with the
   symbols, or with GDB.
   In the following saved_msp, saved memory stack pointer (which functions
   as a memory frame pointer), means either
   a register containing the memory frame pointer or, in the case of
   functions with fixed size memory frames (i.e. those who don't use
   alloca()), the result of the calculation msp + msize.

   LOC_ARG, LOC_LOCAL - For GCC, these are relative to saved_msp.
     For high C, these are relative to msp (making alloca impossible).
   LOC_REGISTER, LOC_REGPARM - The register number is the number at the
     time the function is running (after the prologue), or in the case
     of LOC_REGPARM, may be a register number in the range 160-175.

   The compilers do things like store an argument into memory, and then put out
   a LOC_ARG for it, or put it into global registers and put out a
   LOC_REGPARM.  Thus is it important to execute the first line of
   code (i.e. the line of the open brace, i.e. the prologue) of a function
   before trying to print arguments or anything.

   The following diagram attempts to depict what is going on in memory
   (see also the _a29k user's guide_) and also how that interacts with
   GDB frames.  We arbitrarily pick fci->frame to point the same place
   as the register stack pointer; since we set it ourself in
   INIT_EXTRA_FRAME_INFO, and access it only through the FRAME_*
   macros, it doesn't really matter exactly how we
   do it.  However, note that FRAME_FP is used in two ways in GDB:
   (1) as a "magic cookie" which uniquely identifies frames (even over
   calls to the inferior), (2) (in PC_IN_CALL_DUMMY [ON_STACK])
   as the value of SP_REGNUM before the dummy frame was pushed.  These
   two meanings would be incompatible for the a29k if we defined
   CALL_DUMMY_LOCATION == ON_STACK (but we don't, so don't worry about it).
   Also note that "lr1" below, while called a frame pointer
   in the user's guide, has only one function:  To determine whether
   registers need to be filled in the function epilogue.

   Consider the code:
              < call bar>
       	loc1: . . .
        bar:  sub gr1,gr1,rsize_b
	      . . .
	      add mfp,msp,0
	      sub msp,msp,msize_b
	      . . .
	      < call foo >
	loc2: . . .
        foo:  sub gr1,gr1,rsize_f
	      . . .
	      add mfp,msp,0
	      sub msp,msp,msize_f
	      . . .
        loc3: < suppose the inferior stops here >

                   memory stack      register stack
		   |           |     |____________|
		   |           |     |____loc1____|
	  +------->|___________|     |            |   ^
	  |        | ^         |     |  locals_b  |   |
	  |        | |         |     |____________|   |
	  |        | |         |     |            |   | rsize_b
	  |        | | msize_b |     | args_to_f  |   |
	  |        | |         |     |____________|   |
	  |        | |         |     |____lr1_____|   V
	  |        | V         |     |____loc2____|<----------------+
	  |   +--->|___________|<---------mfp     |   ^             |
	  |   |    | ^         |     |  locals_f  |   |             |
	  |   |    | | msize_f |     |____________|   |             |
	  |   |    | |         |     |            |   | rsize_f     |
	  |   |    | V         |     |   args     |   |             |
	  |   |    |___________|<msp |____________|   |             |
	  |   |                      |_____lr1____|   V             |
	  |   |                      |___garbage__| <- gr1 <----+   |
 	  |   |                 		                |   |
          |   |                 		                |   |
	  |   |	       	       	     pc=loc3	                |   |
	  |   |         		      	                |   |
	  |   |         		      	                |   |
	  |   |            frame cache	      	                |   |
          |   |       |_________________|     	                |   |
          |   |       |rsize=rsize_b    |     	                |   |
          |   |       |msize=msize_b    |     	                |   |
          +---|--------saved_msp        |     	                |   |
              |       |frame------------------------------------|---+
              |       |pc=loc2          |                       |
              |       |_________________|                       |
              |       |rsize=rsize_f    |                       |
              |       |msize=msize_f    |                       |
              +--------saved_msp        |                       |
                      |frame------------------------------------+
                      |pc=loc3          |
                      |_________________|

   So, is that sufficiently confusing?  Welcome to the 29000.
   Notes:
   * The frame for foo uses a memory frame pointer but the frame for
     bar does not.  In the latter case the saved_msp is
     computed by adding msize to the saved_msp of the
     next frame.
   * msize is in the frame cache only for high C's sake.  */

void read_register_stack ();
long read_register_stack_integer ();

#define EXTRA_FRAME_INFO  \
  CORE_ADDR saved_msp;    \
  unsigned int rsize;     \
  unsigned int msize;	  \
  unsigned char flags;

/* Bits for flags in EXTRA_FRAME_INFO */
#define TRANSPARENT	0x1		/* This is a transparent frame */
#define MFP_USED	0x2		/* A memory frame pointer is used */

/* Because INIT_FRAME_PC gets passed fromleaf, that's where we init
   not only ->pc and ->frame, but all the extra stuff, when called from
   get_prev_frame_info, that is.  */
#define INIT_EXTRA_FRAME_INFO(fromleaf, fci)  init_extra_frame_info(fci)
void init_extra_frame_info ();

#define INIT_FRAME_PC(fromleaf, fci) init_frame_pc(fromleaf, fci)
void init_frame_pc ();


/* FRAME_CHAIN takes a FRAME
   and produces the frame's chain-pointer.

   However, if FRAME_CHAIN_VALID returns zero,
   it means the given frame is the outermost one and has no caller.  */

/* On the a29k, the nominal address of a frame is the address on the
   register stack of the return address (the one next to the incoming
   arguments, not down at the bottom so nominal address == stack pointer).

   GDB expects "nominal address" to equal contents of FP_REGNUM,
   at least when it comes time to create the innermost frame.
   However, that doesn't work for us, so when creating the innermost
   frame we set ->frame ourselves in INIT_EXTRA_FRAME_INFO.  */

/* These are mostly dummies for the a29k because INIT_FRAME_PC
   sets prev->frame instead.  */
/* If rsize is zero, we must be at end of stack (or otherwise hosed).
   If we don't check rsize, we loop forever if we see rsize == 0.  */
#define FRAME_CHAIN(thisframe) \
  ((thisframe)->rsize == 0 \
   ? 0 \
   : (thisframe)->frame + (thisframe)->rsize)

/* Determine if the frame has a 'previous' and back-traceable frame. */
#define FRAME_IS_UNCHAINED(frame)	((frame)->flags & TRANSPARENT)

/* Find the previous frame of a transparent routine.
 * For now lets not try and trace through a transparent routine (we might 
 * have to assume that all transparent routines are traps).
 */
#define FIND_PREV_UNCHAINED_FRAME(frame)	0

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

/* Saved pc (i.e. return address).  */
#define FRAME_SAVED_PC(fraim) \
  (read_register_stack_integer ((fraim)->frame + (fraim)->rsize, 4))

/* Local variables (i.e. LOC_LOCAL) are on the memory stack, with their
   offsets being relative to the memory stack pointer (high C) or
   saved_msp (gcc).  */

#define FRAME_LOCALS_ADDRESS(fi) frame_locals_address (fi)
extern CORE_ADDR frame_locals_address ();

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */
/* We tried going to the effort of finding the tags word and getting
   the argcount field from it, to support debugging assembler code.
   Problem was, the "argcount" field never did hold the argument
   count.  */
#define	FRAME_NUM_ARGS(numargs, fi) ((numargs) = -1)

#define FRAME_ARGS_ADDRESS(fi) FRAME_LOCALS_ADDRESS (fi)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 0

/* Provide our own get_saved_register.  HAVE_REGISTER_WINDOWS is insufficient
   because registers get renumbered on the a29k without getting saved.  */

#define GET_SAVED_REGISTER

/* Call function stuff.  */

/* The dummy frame looks like this (see also the general frame picture
   above):

					register stack

	                	      |                |  frame for function
               	                      |   locals_sproc |  executing at time
                                      |________________|  of call_function.
                     		      |	               |  We must not disturb
                     		      |	args_out_sproc |  it.
        memory stack 		      |________________|
                     		      |____lr1_sproc___|<-+
       |            |		      |__retaddr_sproc_|  | <-- gr1 (at start)
       |____________|<-msp 0 <-----------mfp_dummy_____|  |
       |            |  (at start)     |  save regs     |  |
       | arg_slop   |		      |  pc0,pc1       |  |
       |            |		      |  pc2,lr0 sproc |  |
       | (16 words) |		      | gr96-gr124     |  |
       |____________|<-msp 1--after   | sr160-sr162    |  |
       |            | PUSH_DUMMY_FRAME| sr128-sr135    |  |
       | struct ret |                 |________________|  |
       | 17+        |                 |                |  | 
       |____________|<- lrp           | args_out_dummy |  |
       | struct ret |		      |  (16 words)    |  |
       | 16         |		      |________________|  |
       | (16 words) |                 |____lr1_dummy___|--+
       |____________|<- msp 2--after  |_retaddr_dummy__|<- gr1 after
       |            | struct ret      |                |   PUSH_DUMMY_FRAME
       | margs17+   | area allocated  |  locals_inf    |
       |            |                 |________________|    called
       |____________|<- msp 4--when   |                |    function's
       |            |   inf called    | args_out_inf   |    frame (set up
       | margs16    |                 |________________|    by called
       | (16 words) |                 |_____lr1_inf____|    function).
       |____________|<- msp 3--after  |       .        |
       |            |   args pushed   |       .        |
       |            |	              |       .        |
                                      |                |

   arg_slop: This area is so that when the call dummy adds 16 words to
      the msp, it won't end up larger than mfp_dummy (it is needed in the
      case where margs and struct_ret do not add up to at least 16 words).
   struct ret:  This area is allocated by GDB if the return value is more
      than 16 words.  struct ret_16 is not used on the a29k.
   margs:  Pushed by GDB.  The call dummy copies the first 16 words to
      args_out_dummy.
   retaddr_sproc:  Contains the PC at the time we call the function.
      set by PUSH_DUMMY_FRAME and read by POP_FRAME.
   retaddr_dummy:  This points to a breakpoint instruction in the dummy.  */

/* Rsize for dummy frame, in bytes.  */

/* Bytes for outgoing args, lr1, and retaddr.  */
#define DUMMY_ARG (2 * 4 + 16 * 4)

/* Number of special registers (sr128-) to save.  */
#define DUMMY_SAVE_SR128 8
/* Number of special registers (sr160-) to save.  */
#define DUMMY_SAVE_SR160 3
/* Number of general (gr96- or gr64-) registers to save.  */
#define DUMMY_SAVE_GREGS 29

#define DUMMY_FRAME_RSIZE \
(4 /* mfp_dummy */     	  \
 + 4 * 4  /* pc0, pc1, pc2, lr0 */  \
 + DUMMY_SAVE_GREGS * 4   \
 + DUMMY_SAVE_SR160 * 4	  \
 + DUMMY_SAVE_SR128 * 4	  \
 + DUMMY_ARG		  \
 + 4 /* pad to doubleword */ )

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME push_dummy_frame()
extern void push_dummy_frame ();

/* Discard from the stack the innermost frame,
   restoring all saved registers.  */

#define POP_FRAME pop_frame()
extern void pop_frame ();

/* This sequence of words is the instructions
   mtsrim cr, 15
   loadm 0, 0, lr2, msp     ; load first 16 words of arguments into registers
   add msp, msp, 16 * 4     ; point to the remaining arguments
 CONST_INSN:
   const lr0,inf		; (replaced by       half of target addr)
   consth lr0,inf		; (replaced by other half of target addr)
   calli lr0, lr0 
   aseq 0x40,gr1,gr1   ; nop
 BREAKPT_INSN:
   asneq 0x50,gr1,gr1  ; breakpoint	(replaced by local breakpoint insn)
   */

#if TARGET_BYTE_ORDER == HOST_BYTE_ORDER
#define BS(const)	const
#else
#define	BS(const)	(((const) & 0xff) << 24) |	\
			(((const) & 0xff00) << 8) |	\
			(((const) & 0xff0000) >> 8) |	\
			(((const) & 0xff000000) >> 24)
#endif

/* Position of the "const" and blkt instructions within CALL_DUMMY in bytes. */
#define CONST_INSN (3 * 4)
#define BREAKPT_INSN (7 * 4)
#define CALL_DUMMY {	\
		BS(0x0400870f),\
		BS(0x36008200|(MSP_HW_REGNUM)), \
		BS(0x15000040|(MSP_HW_REGNUM<<8)|(MSP_HW_REGNUM<<16)), \
		BS(0x03ff80ff),	\
		BS(0x02ff80ff),	\
		BS(0xc8008080),	\
		BS(0x70400101),	\
		BS(0x72500101)}
#define CALL_DUMMY_LENGTH (8 * 4)

#define CALL_DUMMY_START_OFFSET 0  /* Start execution at beginning of dummy */

/* Helper macro for FIX_CALL_DUMMY.  WORDP is a long * which points to a
   word in target byte order; bits 0-7 and 16-23 of *WORDP are replaced with
   bits 0-7 and 8-15 of DATA (which is in host byte order).  */

#if TARGET_BYTE_ORDER == BIG_ENDIAN
#define STUFF_I16(WORDP, DATA) \
  { \
    *((char *)(WORDP) + 3) = ((DATA) & 0xff);\
    *((char *)(WORDP) + 1) = (((DATA) >> 8) & 0xff);\
  }
#else /* Target is little endian.  */
#define STUFF_I16(WORDP, DATA) \
  {
    *(char *)(WORDP) = ((DATA) & 0xff);
    *((char *)(WORDP) + 2) = (((DATA) >> 8) & 0xff);
  }
#endif /* Target is little endian.  */

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.  */

/* Currently this stuffs in the address of the function that we are calling.
   Since different a29k systems use different breakpoint instructions, it
   also stuffs BREAKPOINT in the right place (to avoid having to
   duplicate CALL_DUMMY in each tm-*.h file).  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)   \
  {\
    STUFF_I16((char *)dummyname + CONST_INSN, fun);		\
    STUFF_I16((char *)dummyname + CONST_INSN + 4, fun >> 16);	\
  /* FIXME  memcpy ((char *)(dummyname) + BREAKPT_INSN, break_insn, 4); */ \
  }

/* a29k architecture has separate data & instruction memories -- wired to
   different pins on the chip -- and can't execute the data memory.
   Also, there should be space after text_end;
   we won't get a SIGSEGV or scribble on data space.  */

#define CALL_DUMMY_LOCATION AFTER_TEXT_END

/* Because of this, we need (as a kludge) to know the addresses of the
   text section.  */

#define	NEED_TEXT_START_END

/* How to translate register numbers in the .stab's into gdb's internal register
   numbers.  We don't translate them, but we warn if an invalid register
   number is seen.  Note that FIXME, we use the value "sym" as an implicit
   argument in printing the error message.  It happens to be available where
   this macro is used.  (This macro definition appeared in a late revision
   of gdb-3.91.6 and is not well tested.  Also, it should be a "complaint".) */

#define	STAB_REG_TO_REGNUM(num) \
	(((num) > LR0_REGNUM + 127) \
	   ? fprintf(stderr, 	\
		"Invalid register number %d in symbol table entry for %s\n", \
	         (num), SYMBOL_SOURCE_NAME (sym)), (num)	\
	   : (num))

extern enum a29k_processor_types {
  a29k_unknown,

  /* Bit 0x400 of the CPS does *not* identify freeze mode, i.e. 29000,
     29030, etc.  */
  a29k_no_freeze_mode,

  /* Bit 0x400 of the CPS does identify freeze mode, i.e. 29050.  */
  a29k_freeze_mode
} processor_type;

/* We need three arguments for a general frame specification for the
   "frame" or "info frame" command.  */

#define SETUP_ARBITRARY_FRAME(argc, argv) setup_arbitrary_frame (argc, argv)
extern struct frame_info *setup_arbitrary_frame PARAMS ((int, CORE_ADDR *));
