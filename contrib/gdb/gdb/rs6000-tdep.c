/* Target-dependent code for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "xcoffsolib.h"

extern struct obstack frame_cache_obstack;

extern int errno;

/* Nonzero if we just simulated a single step break. */
int one_stepped;

/* Breakpoint shadows for the single step instructions will be kept here. */

static struct sstep_breaks {
  /* Address, or 0 if this is not in use.  */
  CORE_ADDR address;
  /* Shadow contents.  */
  char data[4];
} stepBreaks[2];

/* Static function prototypes */

static CORE_ADDR find_toc_address PARAMS ((CORE_ADDR pc));

static CORE_ADDR branch_dest PARAMS ((int opcode, int instr, CORE_ADDR pc,
				      CORE_ADDR safety));

static void frame_get_cache_fsr PARAMS ((struct frame_info *fi,
					 struct rs6000_framedata *fdatap));

/* Calculate the destination of a branch/jump.  Return -1 if not a branch.  */

static CORE_ADDR
branch_dest (opcode, instr, pc, safety)
     int opcode;
     int instr;
     CORE_ADDR pc;
     CORE_ADDR safety;
{
  register long offset;
  CORE_ADDR dest;
  int immediate;
  int absolute;
  int ext_op;

  absolute = (int) ((instr >> 1) & 1);

  switch (opcode) {
     case 18	:
	immediate = ((instr & ~3) << 6) >> 6;	/* br unconditional */
	if (absolute)
	  dest = immediate;	
	else
	  dest = pc + immediate;
	break;

     case 16	:  
        immediate = ((instr & ~3) << 16) >> 16;	/* br conditional */
	if (absolute)
	  dest = immediate;	
	else
	  dest = pc + immediate;
	break;

      case 19	:
	ext_op = (instr>>1) & 0x3ff;

	if (ext_op == 16)			/* br conditional register */
	  dest = read_register (LR_REGNUM) & ~3;

	else if (ext_op == 528)			/* br cond to count reg */
	  {
	    dest = read_register (CTR_REGNUM) & ~3;

	    /* If we are about to execute a system call, dest is something
	       like 0x22fc or 0x3b00.  Upon completion the system call
	       will return to the address in the link register.  */
	    if (dest < TEXT_SEGMENT_BASE)
	      dest = read_register (LR_REGNUM) & ~3;
	  }
	else return -1; 
	break;
	
       default: return -1;
  }
  return (dest < TEXT_SEGMENT_BASE) ? safety : dest;
}



/* AIX does not support PT_STEP. Simulate it. */

void
single_step (signal)
     int signal;
{
#define	INSNLEN(OPCODE)	 4

  static char le_breakp[] = LITTLE_BREAKPOINT;
  static char be_breakp[] = BIG_BREAKPOINT;
  char *breakp = TARGET_BYTE_ORDER == BIG_ENDIAN ? be_breakp : le_breakp;
  int ii, insn;
  CORE_ADDR loc;
  CORE_ADDR breaks[2];
  int opcode;

  if (!one_stepped) {
    loc = read_pc ();

    insn = read_memory_integer (loc, 4);

    breaks[0] = loc + INSNLEN(insn);
    opcode = insn >> 26;
    breaks[1] = branch_dest (opcode, insn, loc, breaks[0]);

    /* Don't put two breakpoints on the same address. */
    if (breaks[1] == breaks[0])
      breaks[1] = -1;

    stepBreaks[1].address = 0;

    for (ii=0; ii < 2; ++ii) {

      /* ignore invalid breakpoint. */
      if ( breaks[ii] == -1)
        continue;

      read_memory (breaks[ii], stepBreaks[ii].data, 4);

      write_memory (breaks[ii], breakp, 4);
      stepBreaks[ii].address = breaks[ii];
    }  

    one_stepped = 1;
  } else {

    /* remove step breakpoints. */
    for (ii=0; ii < 2; ++ii)
      if (stepBreaks[ii].address != 0)
        write_memory 
           (stepBreaks[ii].address, stepBreaks[ii].data, 4);

    one_stepped = 0;
  }
  errno = 0;			/* FIXME, don't ignore errors! */
			/* What errors?  {read,write}_memory call error().  */
}


/* return pc value after skipping a function prologue and also return
   information about a function frame.

   in struct rs6000_frameinfo fdata:
    - frameless is TRUE, if function does not have a frame.
    - nosavedpc is TRUE, if function does not save %pc value in its frame.
    - offset is the number of bytes used in the frame to save registers.
    - saved_gpr is the number of the first saved gpr.
    - saved_fpr is the number of the first saved fpr.
    - alloca_reg is the number of the register used for alloca() handling.
      Otherwise -1.
    - gpr_offset is the offset of the saved gprs
    - fpr_offset is the offset of the saved fprs
    - lr_offset is the offset of the saved lr
    - cr_offset is the offset of the saved cr
 */

#define SIGNED_SHORT(x) 						\
  ((sizeof (short) == 2)						\
   ? ((int)(short)(x))							\
   : ((int)((((x) & 0xffff) ^ 0x8000) - 0x8000)))

#define GET_SRC_REG(x) (((x) >> 21) & 0x1f)

CORE_ADDR
skip_prologue (pc, fdata)
     CORE_ADDR pc;
     struct rs6000_framedata *fdata; 
{
  CORE_ADDR orig_pc = pc;
  char buf[4];
  unsigned long op;
  long offset = 0;
  int lr_reg = 0;
  int cr_reg = 0;
  int reg;
  int framep = 0;
  int minimal_toc_loaded = 0;
  static struct rs6000_framedata zero_frame;

  *fdata = zero_frame;
  fdata->saved_gpr = -1;
  fdata->saved_fpr = -1;
  fdata->alloca_reg = -1;
  fdata->frameless = 1;
  fdata->nosavedpc = 1;

  if (target_read_memory (pc, buf, 4))
    return pc;			/* Can't access it -- assume no prologue. */

  /* Assume that subsequent fetches can fail with low probability.  */
  pc -= 4;
  for (;;)
    {
      pc += 4;
      op = read_memory_integer (pc, 4);

      if ((op & 0xfc1fffff) == 0x7c0802a6) {		/* mflr Rx */
	lr_reg = (op & 0x03e00000) | 0x90010000;
	continue;

      } else if ((op & 0xfc1fffff) == 0x7c000026) {	/* mfcr Rx */
	cr_reg = (op & 0x03e00000) | 0x90010000;
	continue;

      } else if ((op & 0xfc1f0000) == 0xd8010000) {	/* stfd Rx,NUM(r1) */
	reg = GET_SRC_REG (op);
	if (fdata->saved_fpr == -1 || fdata->saved_fpr > reg) {
	  fdata->saved_fpr = reg;
	  fdata->fpr_offset = SIGNED_SHORT (op) + offset;
	}
	continue;

      } else if (((op & 0xfc1f0000) == 0xbc010000) || 	/* stm Rx, NUM(r1) */
		 ((op & 0xfc1f0000) == 0x90010000 &&	/* st rx,NUM(r1), rx >= r13 */
		  (op & 0x03e00000) >= 0x01a00000)) {

	reg = GET_SRC_REG (op);
	if (fdata->saved_gpr == -1 || fdata->saved_gpr > reg) {
	  fdata->saved_gpr = reg;
	  fdata->gpr_offset = SIGNED_SHORT (op) + offset;
	}
	continue;

      } else if ((op & 0xffff0000) == 0x3c000000) {	/* addis 0,0,NUM, used for >= 32k frames */
	fdata->offset = (op & 0x0000ffff) << 16;
	fdata->frameless = 0;
	continue;

      } else if ((op & 0xffff0000) == 0x60000000) {	/* ori 0,0,NUM, 2nd half of >= 32k frames */
	fdata->offset |= (op & 0x0000ffff);
	fdata->frameless = 0;
	continue;

      } else if ((op & 0xffff0000) == lr_reg) {		/* st Rx,NUM(r1) where Rx == lr */
	fdata->lr_offset = SIGNED_SHORT (op) + offset;
	fdata->nosavedpc = 0;
	lr_reg = 0;
	continue;

      } else if ((op & 0xffff0000) == cr_reg) {		/* st Rx,NUM(r1) where Rx == cr */
	fdata->cr_offset = SIGNED_SHORT (op) + offset;
	cr_reg = 0;
	continue;

      } else if (op == 0x48000005) {			/* bl .+4 used in -mrelocatable */
	continue;

      } else if (op == 0x48000004) {			/* b .+4 (xlc) */
	break;

      } else if (((op & 0xffff0000) == 0x801e0000 ||	/* lwz 0,NUM(r30), used in V.4 -mrelocatable */
		  op == 0x7fc0f214) &&			/* add r30,r0,r30, used in V.4 -mrelocatable */
		 lr_reg == 0x901e0000) {
	continue;

      } else if ((op & 0xffff0000) == 0x3fc00000 ||	/* addis 30,0,foo@ha, used in V.4 -mminimal-toc */
		 (op & 0xffff0000) == 0x3bde0000) {	/* addi 30,30,foo@l */
	continue;

      } else if ((op & 0xfc000000) == 0x48000000) {	/* bl foo, to save fprs??? */

	fdata->frameless = 0;
	/* Don't skip over the subroutine call if it is not within the first
	   three instructions of the prologue.  */
	if ((pc - orig_pc) > 8)
	  break;

	op = read_memory_integer (pc+4, 4);

	/* At this point, make sure this is not a trampoline function
	   (a function that simply calls another functions, and nothing else).
	   If the next is not a nop, this branch was part of the function
	   prologue. */

	if (op == 0x4def7b82 || op == 0)		/* crorc 15, 15, 15 */
	  break;					/* don't skip over this branch */

	continue;

      /* update stack pointer */
      } else if ((op & 0xffff0000) == 0x94210000) {	/* stu r1,NUM(r1) */
	fdata->frameless = 0;
	fdata->offset = SIGNED_SHORT (op);
	offset = fdata->offset;
	continue;

      } else if (op == 0x7c21016e) {			/* stwux 1,1,0 */
	fdata->frameless = 0;
	offset = fdata->offset;
	continue;

      /* Load up minimal toc pointer */
      } else if ((op >> 22) == 0x20f
	         && ! minimal_toc_loaded) {	/* l r31,... or l r30,... */
	minimal_toc_loaded = 1;
	continue;

      /* store parameters in stack */
      } else if ((op & 0xfc1f0000) == 0x90010000 ||	/* st rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xd8010000 ||	/* stfd Rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xfc010000) {	/* frsp, fp?,NUM(r1) */
	continue;

      /* store parameters in stack via frame pointer */
      } else if (framep &&
		 (op & 0xfc1f0000) == 0x901f0000 ||	/* st rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xd81f0000 ||	/* stfd Rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xfc1f0000) {	/* frsp, fp?,NUM(r1) */
	continue;

      /* Set up frame pointer */
      } else if (op == 0x603f0000			/* oril r31, r1, 0x0 */
		 || op == 0x7c3f0b78) {			/* mr r31, r1 */
	fdata->frameless = 0;
	framep = 1;
	fdata->alloca_reg = 31;
	continue;

      /* Another way to set up the frame pointer.  */
      } else if ((op & 0xfc1fffff) == 0x38010000) {	/* addi rX, r1, 0x0 */
	fdata->frameless = 0;
	framep = 1;
	fdata->alloca_reg = (op & ~0x38010000) >> 21;
	continue;

      } else {
	break;
      }
    }

#if 0
/* I have problems with skipping over __main() that I need to address
 * sometime. Previously, I used to use misc_function_vector which
 * didn't work as well as I wanted to be.  -MGO */

  /* If the first thing after skipping a prolog is a branch to a function,
     this might be a call to an initializer in main(), introduced by gcc2.
     We'd like to skip over it as well. Fortunately, xlc does some extra
     work before calling a function right after a prologue, thus we can
     single out such gcc2 behaviour. */
     

  if ((op & 0xfc000001) == 0x48000001) { /* bl foo, an initializer function? */
    op = read_memory_integer (pc+4, 4);

    if (op == 0x4def7b82) {		/* cror 0xf, 0xf, 0xf (nop) */

      /* check and see if we are in main. If so, skip over this initializer
         function as well. */

      tmp = find_pc_misc_function (pc);
      if (tmp >= 0 && STREQ (misc_function_vector [tmp].name, "main"))
        return pc + 8;
    }
  }
#endif /* 0 */
 
  fdata->offset = - fdata->offset;
  return pc;
}


/*************************************************************************
  Support for creating pushind a dummy frame into the stack, and popping
  frames, etc. 
*************************************************************************/

/* The total size of dummy frame is 436, which is;

	32 gpr's	- 128 bytes
	32 fpr's	- 256   "
	7  the rest	- 28    "
	and 24 extra bytes for the callee's link area. The last 24 bytes
	for the link area might not be necessary, since it will be taken
	care of by push_arguments(). */

#define DUMMY_FRAME_SIZE 436

#define	DUMMY_FRAME_ADDR_SIZE 10

/* Make sure you initialize these in somewhere, in case gdb gives up what it
   was debugging and starts debugging something else. FIXMEibm */

static int dummy_frame_count = 0;
static int dummy_frame_size = 0;
static CORE_ADDR *dummy_frame_addr = 0;

extern int stop_stack_dummy;

/* push a dummy frame into stack, save all register. Currently we are saving
   only gpr's and fpr's, which is not good enough! FIXMEmgo */
   
void
push_dummy_frame ()
{
  /* stack pointer.  */
  CORE_ADDR sp;
  /* Same thing, target byte order.  */
  char sp_targ[4];

  /* link register.  */
  CORE_ADDR pc;
  /* Same thing, target byte order.  */
  char pc_targ[4];
  
  /* Needed to figure out where to save the dummy link area.
     FIXME: There should be an easier way to do this, no?  tiemann 9/9/95.  */
  struct rs6000_framedata fdata;

  int ii;

  target_fetch_registers (-1);

  if (dummy_frame_count >= dummy_frame_size) {
    dummy_frame_size += DUMMY_FRAME_ADDR_SIZE;
    if (dummy_frame_addr)
      dummy_frame_addr = (CORE_ADDR*) xrealloc 
        (dummy_frame_addr, sizeof(CORE_ADDR) * (dummy_frame_size));
    else
      dummy_frame_addr = (CORE_ADDR*) 
	xmalloc (sizeof(CORE_ADDR) * (dummy_frame_size));
  }
  
  sp = read_register(SP_REGNUM);
  pc = read_register(PC_REGNUM);
  store_address (pc_targ, 4, pc);

  (void) skip_prologue (get_pc_function_start (pc) + FUNCTION_START_OFFSET, &fdata);

  dummy_frame_addr [dummy_frame_count++] = sp;

  /* Be careful! If the stack pointer is not decremented first, then kernel 
     thinks he is free to use the space underneath it. And kernel actually 
     uses that area for IPC purposes when executing ptrace(2) calls. So 
     before writing register values into the new frame, decrement and update
     %sp first in order to secure your frame. */

  /* FIXME: We don't check if the stack really has this much space.
     This is a problem on the ppc simulator (which only grants one page
     (4096 bytes) by default.  */

  write_register (SP_REGNUM, sp-DUMMY_FRAME_SIZE);

  /* gdb relies on the state of current_frame. We'd better update it,
     otherwise things like do_registers_info() wouldn't work properly! */

  flush_cached_frames ();

  /* save program counter in link register's space. */
  write_memory (sp + (fdata.lr_offset ? fdata.lr_offset : DEFAULT_LR_SAVE),
	        pc_targ, 4);

  /* save all floating point and general purpose registers here. */

  /* fpr's, f0..f31 */
  for (ii = 0; ii < 32; ++ii)
    write_memory (sp-8-(ii*8), &registers[REGISTER_BYTE (31-ii+FP0_REGNUM)], 8);

  /* gpr's r0..r31 */
  for (ii=1; ii <=32; ++ii)
    write_memory (sp-256-(ii*4), &registers[REGISTER_BYTE (32-ii)], 4);

  /* so far, 32*2 + 32 words = 384 bytes have been written. 
     7 extra registers in our register set: pc, ps, cnd, lr, cnt, xer, mq */

  for (ii=1; ii <= (LAST_SP_REGNUM-FIRST_SP_REGNUM+1); ++ii) {
    write_memory (sp-384-(ii*4), 
	       &registers[REGISTER_BYTE (FPLAST_REGNUM + ii)], 4);
  }

  /* Save sp or so called back chain right here. */
  store_address (sp_targ, 4, sp);
  write_memory (sp-DUMMY_FRAME_SIZE, sp_targ, 4);
  sp -= DUMMY_FRAME_SIZE;

  /* And finally, this is the back chain. */
  write_memory (sp+8, pc_targ, 4);
}


/* Pop a dummy frame.

   In rs6000 when we push a dummy frame, we save all of the registers. This
   is usually done before user calls a function explicitly.

   After a dummy frame is pushed, some instructions are copied into stack,
   and stack pointer is decremented even more.  Since we don't have a frame
   pointer to get back to the parent frame of the dummy, we start having
   trouble poping it.  Therefore, we keep a dummy frame stack, keeping
   addresses of dummy frames as such.  When poping happens and when we
   detect that was a dummy frame, we pop it back to its parent by using
   dummy frame stack (`dummy_frame_addr' array). 

FIXME:  This whole concept is broken.  You should be able to detect
a dummy stack frame *on the user's stack itself*.  When you do,
then you know the format of that stack frame -- including its
saved SP register!  There should *not* be a separate stack in the
GDB process that keeps track of these dummy frames!  -- gnu@cygnus.com Aug92
 */
   
pop_dummy_frame ()
{
  CORE_ADDR sp, pc;
  int ii;
  sp = dummy_frame_addr [--dummy_frame_count];

  /* restore all fpr's. */
  for (ii = 1; ii <= 32; ++ii)
    read_memory (sp-(ii*8), &registers[REGISTER_BYTE (32-ii+FP0_REGNUM)], 8);

  /* restore all gpr's */
  for (ii=1; ii <= 32; ++ii) {
    read_memory (sp-256-(ii*4), &registers[REGISTER_BYTE (32-ii)], 4);
  }

  /* restore the rest of the registers. */
  for (ii=1; ii <=(LAST_SP_REGNUM-FIRST_SP_REGNUM+1); ++ii)
    read_memory (sp-384-(ii*4),
    		&registers[REGISTER_BYTE (FPLAST_REGNUM + ii)], 4);

  read_memory (sp-(DUMMY_FRAME_SIZE-8), 
	       &registers [REGISTER_BYTE(PC_REGNUM)], 4);

  /* when a dummy frame was being pushed, we had to decrement %sp first, in 
     order to secure astack space. Thus, saved %sp (or %r1) value, is not the
     one we should restore. Change it with the one we need. */

  *(int*)&registers [REGISTER_BYTE(FP_REGNUM)] = sp;

  /* Now we can restore all registers. */

  target_store_registers (-1);
  pc = read_pc ();
  flush_cached_frames ();
}


/* pop the innermost frame, go back to the caller. */

void
pop_frame ()
{
  CORE_ADDR pc, lr, sp, prev_sp;		/* %pc, %lr, %sp */
  struct rs6000_framedata fdata;
  struct frame_info *frame = get_current_frame ();
  int addr, ii;

  pc = read_pc ();
  sp = FRAME_FP (frame);

  if (stop_stack_dummy && dummy_frame_count) {
    pop_dummy_frame ();
    return;
  }

  /* Make sure that all registers are valid.  */
  read_register_bytes (0, NULL, REGISTER_BYTES);

  /* figure out previous %pc value. If the function is frameless, it is 
     still in the link register, otherwise walk the frames and retrieve the
     saved %pc value in the previous frame. */

  addr = get_pc_function_start (frame->pc) + FUNCTION_START_OFFSET;
  (void) skip_prologue (addr, &fdata);

  if (fdata.frameless)
    prev_sp = sp;
  else
    prev_sp = read_memory_integer (sp, 4);
  if (fdata.lr_offset == 0)
    lr = read_register (LR_REGNUM);
  else
    lr = read_memory_integer (prev_sp + fdata.lr_offset, 4);

  /* reset %pc value. */
  write_register (PC_REGNUM, lr);

  /* reset register values if any was saved earlier. */
  addr = prev_sp - fdata.offset;

  if (fdata.saved_gpr != -1)
    for (ii = fdata.saved_gpr; ii <= 31; ++ii) {
      read_memory (addr, &registers [REGISTER_BYTE (ii)], 4);
      addr += 4;
    }

  if (fdata.saved_fpr != -1)
    for (ii = fdata.saved_fpr; ii <= 31; ++ii) {
      read_memory (addr, &registers [REGISTER_BYTE (ii+FP0_REGNUM)], 8);
      addr += 8;
  }

  write_register (SP_REGNUM, prev_sp);
  target_store_registers (-1);
  flush_cached_frames ();
}

/* fixup the call sequence of a dummy function, with the real function address.
   its argumets will be passed by gdb. */

void
fix_call_dummy (dummyname, pc, fun, nargs, type)
     char *dummyname;
     CORE_ADDR pc;
     CORE_ADDR fun;
     int nargs;					/* not used */
     int type;					/* not used */
{
#define	TOC_ADDR_OFFSET		20
#define	TARGET_ADDR_OFFSET	28

  int ii;
  CORE_ADDR target_addr;
  CORE_ADDR tocvalue;

  target_addr = fun;
  tocvalue = find_toc_address (target_addr);

  ii  = *(int*)((char*)dummyname + TOC_ADDR_OFFSET);
  ii = (ii & 0xffff0000) | (tocvalue >> 16);
  *(int*)((char*)dummyname + TOC_ADDR_OFFSET) = ii;

  ii  = *(int*)((char*)dummyname + TOC_ADDR_OFFSET+4);
  ii = (ii & 0xffff0000) | (tocvalue & 0x0000ffff);
  *(int*)((char*)dummyname + TOC_ADDR_OFFSET+4) = ii;

  ii  = *(int*)((char*)dummyname + TARGET_ADDR_OFFSET);
  ii = (ii & 0xffff0000) | (target_addr >> 16);
  *(int*)((char*)dummyname + TARGET_ADDR_OFFSET) = ii;

  ii  = *(int*)((char*)dummyname + TARGET_ADDR_OFFSET+4);
  ii = (ii & 0xffff0000) | (target_addr & 0x0000ffff);
  *(int*)((char*)dummyname + TARGET_ADDR_OFFSET+4) = ii;
}

/* Pass the arguments in either registers, or in the stack. In RS6000,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r11 registers.  float and double parameters are
   passed in fpr's, in addition to that. Rest of the parameters if any
   are passed in user stack. There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parametes can be passed in registers,
   starting from r4. */

CORE_ADDR
push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int ii, len;
  int argno;					/* current argument number */
  int argbytes;					/* current argument byte */
  char tmp_buffer [50];
  int f_argno = 0;				/* current floating point argno */
  value_ptr arg;
  struct type *type;

  CORE_ADDR saved_sp, pc;

  if ( dummy_frame_count <= 0)
    printf_unfiltered ("FATAL ERROR -push_arguments()! frame not found!!\n");

  /* The first eight words of ther arguments are passed in registers. Copy
     them appropriately.

     If the function is returning a `struct', then the first word (which 
     will be passed in r3) is used for struct return address. In that
     case we should advance one word and start from r4 register to copy 
     parameters. */

  ii =  struct_return ? 1 : 0;

  for (argno=0, argbytes=0; argno < nargs && ii<8; ++ii) {

    arg = args[argno];
    type = check_typedef (VALUE_TYPE (arg));
    len = TYPE_LENGTH (type);

    if (TYPE_CODE (type) == TYPE_CODE_FLT) {

      /* floating point arguments are passed in fpr's, as well as gpr's.
         There are 13 fpr's reserved for passing parameters. At this point
         there is no way we would run out of them. */

      if (len > 8)
        printf_unfiltered (
"Fatal Error: a floating point parameter #%d with a size > 8 is found!\n", argno);

      memcpy (&registers[REGISTER_BYTE(FP0_REGNUM + 1 + f_argno)], VALUE_CONTENTS (arg), 
         len);
      ++f_argno;
    }

    if (len > 4) {

      /* Argument takes more than one register. */
      while (argbytes < len) {

	*(int*)&registers[REGISTER_BYTE(ii+3)] = 0;
	memcpy (&registers[REGISTER_BYTE(ii+3)], 
			 ((char*)VALUE_CONTENTS (arg))+argbytes, 
			(len - argbytes) > 4 ? 4 : len - argbytes);
	++ii, argbytes += 4;

	if (ii >= 8)
	  goto ran_out_of_registers_for_arguments;
      }
      argbytes = 0;
      --ii;
    }
    else {        /* Argument can fit in one register. No problem. */
      *(int*)&registers[REGISTER_BYTE(ii+3)] = 0;
      memcpy (&registers[REGISTER_BYTE(ii+3)], VALUE_CONTENTS (arg), len);
    }
    ++argno;
  }

ran_out_of_registers_for_arguments:

  /* location for 8 parameters are always reserved. */
  sp -= 4 * 8;

  /* another six words for back chain, TOC register, link register, etc. */
  sp -= 24;

  /* if there are more arguments, allocate space for them in 
     the stack, then push them starting from the ninth one. */

  if ((argno < nargs) || argbytes) {
    int space = 0, jj;

    if (argbytes) {
      space += ((len - argbytes + 3) & -4);
      jj = argno + 1;
    }
    else
      jj = argno;

    for (; jj < nargs; ++jj) {
      value_ptr val = args[jj];
      space += ((TYPE_LENGTH (VALUE_TYPE (val))) + 3) & -4;
    }

    /* add location required for the rest of the parameters */
    space = (space + 7) & -8;
    sp -= space;

    /* This is another instance we need to be concerned about securing our
	stack space. If we write anything underneath %sp (r1), we might conflict
	with the kernel who thinks he is free to use this area. So, update %sp
	first before doing anything else. */

    write_register (SP_REGNUM, sp);

    /* if the last argument copied into the registers didn't fit there 
       completely, push the rest of it into stack. */

    if (argbytes) {
      write_memory (
        sp+24+(ii*4), ((char*)VALUE_CONTENTS (arg))+argbytes, len - argbytes);
      ++argno;
      ii += ((len - argbytes + 3) & -4) / 4;
    }

    /* push the rest of the arguments into stack. */
    for (; argno < nargs; ++argno) {

      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);


      /* float types should be passed in fpr's, as well as in the stack. */
      if (TYPE_CODE (type) == TYPE_CODE_FLT && f_argno < 13) {

        if (len > 8)
          printf_unfiltered (
"Fatal Error: a floating point parameter #%d with a size > 8 is found!\n", argno);

        memcpy (&registers[REGISTER_BYTE(FP0_REGNUM + 1 + f_argno)], VALUE_CONTENTS (arg), 
           len);
        ++f_argno;
      }

      write_memory (sp+24+(ii*4), (char *) VALUE_CONTENTS (arg), len);
      ii += ((len + 3) & -4) / 4;
    }
  }
  else
    /* Secure stack areas first, before doing anything else. */
    write_register (SP_REGNUM, sp);

  saved_sp = dummy_frame_addr [dummy_frame_count - 1];
  read_memory (saved_sp, tmp_buffer, 24);
  write_memory (sp, tmp_buffer, 24);

  /* set back chain properly */
  store_address (tmp_buffer, 4, saved_sp);
  write_memory (sp, tmp_buffer, 4);

  target_store_registers (-1);
  return sp;
}

/* a given return value in `regbuf' with a type `valtype', extract and copy its
   value into `valbuf' */

void
extract_return_value (valtype, regbuf, valbuf)
     struct type *valtype;
     char regbuf[REGISTER_BYTES];
     char *valbuf;
{
  int offset = 0;

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT) {

    double dd; float ff;
    /* floats and doubles are returned in fpr1. fpr's have a size of 8 bytes.
       We need to truncate the return value into float size (4 byte) if
       necessary. */

    if (TYPE_LENGTH (valtype) > 4) 		/* this is a double */
      memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM + 1)],
						TYPE_LENGTH (valtype));
    else {		/* float */
      memcpy (&dd, &regbuf[REGISTER_BYTE (FP0_REGNUM + 1)], 8);
      ff = (float)dd;
      memcpy (valbuf, &ff, sizeof(float));
    }
  }
  else {
    /* return value is copied starting from r3. */
    if (TARGET_BYTE_ORDER == BIG_ENDIAN
	&& TYPE_LENGTH (valtype) < REGISTER_RAW_SIZE (3))
      offset = REGISTER_RAW_SIZE (3) - TYPE_LENGTH (valtype);

    memcpy (valbuf, regbuf + REGISTER_BYTE (3) + offset,
	    TYPE_LENGTH (valtype));
  }
}


/* keep structure return address in this variable.
   FIXME:  This is a horrid kludge which should not be allowed to continue
   living.  This only allows a single nested call to a structure-returning
   function.  Come on, guys!  -- gnu@cygnus.com, Aug 92  */

CORE_ADDR rs6000_struct_return_address;


/* Indirect function calls use a piece of trampoline code to do context
   switching, i.e. to set the new TOC table. Skip such code if we are on
   its first instruction (as when we have single-stepped to here). 
   Also skip shared library trampoline code (which is different from
   indirect function call trampolines).
   Result is desired PC to step until, or NULL if we are not in
   trampoline code.  */

CORE_ADDR
skip_trampoline_code (pc)
     CORE_ADDR pc;
{
  register unsigned int ii, op;
  CORE_ADDR solib_target_pc;

  static unsigned trampoline_code[] = {
	0x800b0000,			/*     l   r0,0x0(r11)	*/
	0x90410014,			/*    st   r2,0x14(r1)	*/
	0x7c0903a6,			/* mtctr   r0		*/
	0x804b0004,			/*     l   r2,0x4(r11)	*/
	0x816b0008,			/*     l  r11,0x8(r11)	*/
	0x4e800420,			/*  bctr		*/
	0x4e800020,			/*    br		*/
	0
  };

  /* If pc is in a shared library trampoline, return its target.  */
  solib_target_pc = find_solib_trampoline_target (pc);
  if (solib_target_pc)
    return solib_target_pc;

  for (ii=0; trampoline_code[ii]; ++ii) {
    op  = read_memory_integer (pc + (ii*4), 4);
    if (op != trampoline_code [ii])
      return 0;
  }
  ii = read_register (11);		/* r11 holds destination addr	*/
  pc = read_memory_integer (ii, 4);	/* (r11) value			*/
  return pc;
}

/* Determines whether the function FI has a frame on the stack or not.  */

int
frameless_function_invocation (fi)
     struct frame_info *fi;
{
  CORE_ADDR func_start;
  struct rs6000_framedata fdata;

  if (fi->next != NULL)
    /* Don't even think about framelessness except on the innermost frame.  */
    /* FIXME: Can also be frameless if fi->next->signal_handler_caller (if
       a signal happens while executing in a frameless function).  */
    return 0;
  
  func_start = get_pc_function_start (fi->pc) + FUNCTION_START_OFFSET;

  /* If we failed to find the start of the function, it is a mistake
     to inspect the instructions. */

  if (!func_start)
    return 0;

  (void) skip_prologue (func_start, &fdata);
  return fdata.frameless;
}

/* Return the PC saved in a frame */

unsigned long
frame_saved_pc (fi)
     struct frame_info *fi;
{
  CORE_ADDR func_start;
  struct rs6000_framedata fdata;
  int frameless;

  if (fi->signal_handler_caller)
    return read_memory_integer (fi->frame + SIG_FRAME_PC_OFFSET, 4);

  func_start = get_pc_function_start (fi->pc) + FUNCTION_START_OFFSET;

  /* If we failed to find the start of the function, it is a mistake
     to inspect the instructions. */
  if (!func_start)
    return 0;

  (void) skip_prologue (func_start, &fdata);

  if (fdata.lr_offset == 0 && fi->next != NULL)
    return read_memory_integer (rs6000_frame_chain (fi) + DEFAULT_LR_SAVE, 4);

  if (fdata.lr_offset == 0)
    return read_register (LR_REGNUM);

  return read_memory_integer (rs6000_frame_chain (fi) + fdata.lr_offset, 4);
}

/* If saved registers of frame FI are not known yet, read and cache them.
   &FDATAP contains rs6000_framedata; TDATAP can be NULL,
   in which case the framedata are read.  */

static void
frame_get_cache_fsr (fi, fdatap)
     struct frame_info *fi;
     struct rs6000_framedata *fdatap;
{
  int ii;
  CORE_ADDR frame_addr; 
  struct rs6000_framedata work_fdata;

  if (fi->cache_fsr)
    return;
  
  if (fdatap == NULL) {
    fdatap = &work_fdata;
    (void) skip_prologue (get_pc_function_start (fi->pc), fdatap);
  }

  fi->cache_fsr = (struct frame_saved_regs *)
      obstack_alloc (&frame_cache_obstack, sizeof (struct frame_saved_regs));
  memset (fi->cache_fsr, '\0', sizeof (struct frame_saved_regs));

  if (fi->prev && fi->prev->frame)
    frame_addr = fi->prev->frame;
  else
    frame_addr = read_memory_integer (fi->frame, 4);
  
  /* if != -1, fdatap->saved_fpr is the smallest number of saved_fpr.
     All fpr's from saved_fpr to fp31 are saved.  */

  if (fdatap->saved_fpr >= 0) {
    int fpr_offset = frame_addr + fdatap->fpr_offset;
    for (ii = fdatap->saved_fpr; ii < 32; ii++) {
      fi->cache_fsr->regs [FP0_REGNUM + ii] = fpr_offset;
      fpr_offset += 8;
    }
  }

  /* if != -1, fdatap->saved_gpr is the smallest number of saved_gpr.
     All gpr's from saved_gpr to gpr31 are saved.  */
  
  if (fdatap->saved_gpr >= 0) {
    int gpr_offset = frame_addr + fdatap->gpr_offset;
    for (ii = fdatap->saved_gpr; ii < 32; ii++) {
      fi->cache_fsr->regs [ii] = gpr_offset;
      gpr_offset += 4;
    }
  }

  /* If != 0, fdatap->cr_offset is the offset from the frame that holds
     the CR.  */
  if (fdatap->cr_offset != 0)
    fi->cache_fsr->regs [CR_REGNUM] = frame_addr + fdatap->cr_offset;

  /* If != 0, fdatap->lr_offset is the offset from the frame that holds
     the LR.  */
  if (fdatap->lr_offset != 0)
    fi->cache_fsr->regs [LR_REGNUM] = frame_addr + fdatap->lr_offset;
}

/* Return the address of a frame. This is the inital %sp value when the frame
   was first allocated. For functions calling alloca(), it might be saved in
   an alloca register. */

CORE_ADDR
frame_initial_stack_address (fi)
     struct frame_info *fi;
{
  CORE_ADDR tmpaddr;
  struct rs6000_framedata fdata;
  struct frame_info *callee_fi;

  /* if the initial stack pointer (frame address) of this frame is known,
     just return it. */

  if (fi->initial_sp)
    return fi->initial_sp;

  /* find out if this function is using an alloca register.. */

  (void) skip_prologue (get_pc_function_start (fi->pc), &fdata);

  /* if saved registers of this frame are not known yet, read and cache them. */

  if (!fi->cache_fsr)
    frame_get_cache_fsr (fi, &fdata);

  /* If no alloca register used, then fi->frame is the value of the %sp for
     this frame, and it is good enough. */

  if (fdata.alloca_reg < 0) {
    fi->initial_sp = fi->frame;
    return fi->initial_sp;
  }

  /* This function has an alloca register. If this is the top-most frame
     (with the lowest address), the value in alloca register is good. */

  if (!fi->next)
    return fi->initial_sp = read_register (fdata.alloca_reg);     

  /* Otherwise, this is a caller frame. Callee has usually already saved
     registers, but there are exceptions (such as when the callee
     has no parameters). Find the address in which caller's alloca
     register is saved. */

  for (callee_fi = fi->next; callee_fi; callee_fi = callee_fi->next) {

    if (!callee_fi->cache_fsr)
      frame_get_cache_fsr (callee_fi, NULL);

    /* this is the address in which alloca register is saved. */

    tmpaddr = callee_fi->cache_fsr->regs [fdata.alloca_reg];
    if (tmpaddr) {
      fi->initial_sp = read_memory_integer (tmpaddr, 4); 
      return fi->initial_sp;
    }

    /* Go look into deeper levels of the frame chain to see if any one of
       the callees has saved alloca register. */
  }

  /* If alloca register was not saved, by the callee (or any of its callees)
     then the value in the register is still good. */

  return fi->initial_sp = read_register (fdata.alloca_reg);     
}

CORE_ADDR
rs6000_frame_chain (thisframe)
     struct frame_info *thisframe;
{
  CORE_ADDR fp;
  if (inside_entry_file ((thisframe)->pc))
    return 0;
  if (thisframe->signal_handler_caller)
    fp = read_memory_integer (thisframe->frame + SIG_FRAME_FP_OFFSET, 4);
  else
    fp = read_memory_integer ((thisframe)->frame, 4);

  return fp;
}

/* Keep an array of load segment information and their TOC table addresses.
   This info will be useful when calling a shared library function by hand. */
   
struct loadinfo {
  CORE_ADDR textorg, dataorg;
  unsigned long toc_offset;
};

#define	LOADINFOLEN	10

static	struct loadinfo *loadinfo = NULL;
static	int	loadinfolen = 0;
static	int	loadinfotocindex = 0;
static	int	loadinfotextindex = 0;


void
xcoff_init_loadinfo ()
{
  loadinfotocindex = 0;
  loadinfotextindex = 0;

  if (loadinfolen == 0) {
    loadinfo = (struct loadinfo *)
               xmalloc (sizeof (struct loadinfo) * LOADINFOLEN);
    loadinfolen = LOADINFOLEN;
  }
}


/* FIXME -- this is never called!  */
void
free_loadinfo ()
{
  if (loadinfo)
    free (loadinfo);
  loadinfo = NULL;
  loadinfolen = 0;
  loadinfotocindex = 0;
  loadinfotextindex = 0;
}

/* this is called from xcoffread.c */

void
xcoff_add_toc_to_loadinfo (tocoff)
     unsigned long tocoff;
{
  while (loadinfotocindex >= loadinfolen) {
    loadinfolen += LOADINFOLEN;
    loadinfo = (struct loadinfo *)
               xrealloc (loadinfo, sizeof(struct loadinfo) * loadinfolen);
  }
  loadinfo [loadinfotocindex++].toc_offset = tocoff;
}

void
add_text_to_loadinfo (textaddr, dataaddr)
     CORE_ADDR textaddr;
     CORE_ADDR dataaddr;
{
  while (loadinfotextindex >= loadinfolen) {
    loadinfolen += LOADINFOLEN;
    loadinfo = (struct loadinfo *)
               xrealloc (loadinfo, sizeof(struct loadinfo) * loadinfolen);
  }
  loadinfo [loadinfotextindex].textorg = textaddr;
  loadinfo [loadinfotextindex].dataorg = dataaddr;
  ++loadinfotextindex;
}


/* Note that this assumes that the "textorg" and "dataorg" elements of
   a member of this array are correlated with the "toc_offset" element
   of the same member.  This is taken care of because the loops which
   assign the former (in xcoff_relocate_symtab or xcoff_relocate_core)
   and the latter (in scan_xcoff_symtab, via vmap_symtab, in
   vmap_ldinfo or xcoff_relocate_core) traverse the same objfiles in
   the same order.  */

static CORE_ADDR
find_toc_address (pc)
     CORE_ADDR pc;
{
  int ii, toc_entry, tocbase = 0;

  toc_entry = -1;
  for (ii=0; ii < loadinfotextindex; ++ii)
    if (pc > loadinfo[ii].textorg && loadinfo[ii].textorg > tocbase) {
      toc_entry = ii;
      tocbase = loadinfo[ii].textorg;
    }

  if (toc_entry == -1)
    error ("Unable to find TOC entry for pc 0x%x\n", pc);
  return loadinfo[toc_entry].dataorg + loadinfo[toc_entry].toc_offset;
}

/* Return nonzero if ADDR (a function pointer) is in the data space and
   is therefore a special function pointer.  */

int
is_magic_function_pointer (addr)
     CORE_ADDR addr;
{
  struct obj_section *s;

  s = find_pc_section (addr);
  if (s && s->the_bfd_section->flags & SEC_CODE)
    return 0;
  else
    return 1;
}

#ifdef GDB_TARGET_POWERPC
int
gdb_print_insn_powerpc (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    return print_insn_big_powerpc (memaddr, info);
  else
    return print_insn_little_powerpc (memaddr, info);
}
#endif

void
_initialize_rs6000_tdep ()
{
  /* FIXME, this should not be decided via ifdef. */
#ifdef GDB_TARGET_POWERPC
  tm_print_insn = gdb_print_insn_powerpc;
#else
  tm_print_insn = print_insn_rs6000;
#endif
}
