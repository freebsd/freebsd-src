/* Target-machine dependent code for Motorola 88000 series, for GDB.
   Copyright 1988, 1990, 1991, 1994, 1995 Free Software Foundation, Inc.

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
#include "value.h"
#include "gdbcore.h"
#include "symtab.h"
#include "setjmp.h"
#include "value.h"

/* Size of an instruction */
#define	BYTES_PER_88K_INSN	4

void frame_find_saved_regs ();

/* Is this target an m88110?  Otherwise assume m88100.  This has
   relevance for the ways in which we screw with instruction pointers.  */

int target_is_m88110 = 0;

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.

   For us, the frame address is its stack pointer value, so we look up
   the function prologue to determine the caller's sp value, and return it.  */

CORE_ADDR
frame_chain (thisframe)
     struct frame_info *thisframe;
{

  frame_find_saved_regs (thisframe, (struct frame_saved_regs *) 0);
  /* NOTE:  this depends on frame_find_saved_regs returning the VALUE, not
 	    the ADDRESS, of SP_REGNUM.  It also depends on the cache of
	    frame_find_saved_regs results.  */
  if (thisframe->fsr->regs[SP_REGNUM])
    return thisframe->fsr->regs[SP_REGNUM];
  else
    return thisframe->frame;	/* Leaf fn -- next frame up has same SP. */
}

int
frameless_function_invocation (frame)
     struct frame_info *frame;
{

  frame_find_saved_regs (frame, (struct frame_saved_regs *) 0);
  /* NOTE:  this depends on frame_find_saved_regs returning the VALUE, not
 	    the ADDRESS, of SP_REGNUM.  It also depends on the cache of
	    frame_find_saved_regs results.  */
  if (frame->fsr->regs[SP_REGNUM])
    return 0;			/* Frameful -- return addr saved somewhere */
  else
    return 1;			/* Frameless -- no saved return address */
}

void
init_extra_frame_info (fromleaf, frame)
     int fromleaf;
     struct frame_info *frame;
{
  frame->fsr = 0;			/* Not yet allocated */
  frame->args_pointer = 0;		/* Unknown */
  frame->locals_pointer = 0;	/* Unknown */
}

/* Examine an m88k function prologue, recording the addresses at which
   registers are saved explicitly by the prologue code, and returning
   the address of the first instruction after the prologue (but not
   after the instruction at address LIMIT, as explained below).

   LIMIT places an upper bound on addresses of the instructions to be
   examined.  If the prologue code scan reaches LIMIT, the scan is
   aborted and LIMIT is returned.  This is used, when examining the
   prologue for the current frame, to keep examine_prologue () from
   claiming that a given register has been saved when in fact the
   instruction that saves it has not yet been executed.  LIMIT is used
   at other times to stop the scan when we hit code after the true
   function prologue (e.g. for the first source line) which might
   otherwise be mistaken for function prologue.

   The format of the function prologue matched by this routine is
   derived from examination of the source to gcc 1.95, particularly
   the routine output_prologue () in config/out-m88k.c.

   subu r31,r31,n			# stack pointer update

   (st rn,r31,offset)?			# save incoming regs
   (st.d rn,r31,offset)?

   (addu r30,r31,n)?			# frame pointer update

   (pic sequence)?			# PIC code prologue

   (or   rn,rm,0)?			# Move parameters to other regs
*/

/* Macros for extracting fields from instructions.  */

#define BITMASK(pos, width) (((0x1 << (width)) - 1) << (pos))
#define EXTRACT_FIELD(val, pos, width) ((val) >> (pos) & BITMASK (0, width))
#define	SUBU_OFFSET(x)	((unsigned)(x & 0xFFFF))
#define	ST_OFFSET(x)	((unsigned)((x) & 0xFFFF))
#define	ST_SRC(x)	EXTRACT_FIELD ((x), 21, 5)
#define	ADDU_OFFSET(x)	((unsigned)(x & 0xFFFF))

/*
 * prologue_insn_tbl is a table of instructions which may comprise a
 * function prologue.  Associated with each table entry (corresponding
 * to a single instruction or group of instructions), is an action.
 * This action is used by examine_prologue (below) to determine
 * the state of certain machine registers and where the stack frame lives.
 */

enum prologue_insn_action {
  PIA_SKIP,			/* don't care what the instruction does */
  PIA_NOTE_ST,			/* note register stored and where */
  PIA_NOTE_STD,			/* note pair of registers stored and where */
  PIA_NOTE_SP_ADJUSTMENT,	/* note stack pointer adjustment */
  PIA_NOTE_FP_ASSIGNMENT,	/* note frame pointer assignment */
  PIA_NOTE_PROLOGUE_END,	/* no more prologue */
};

struct prologue_insns {
    unsigned long insn;
    unsigned long mask;
    enum prologue_insn_action action;
};

struct prologue_insns prologue_insn_tbl[] = {
  /* Various register move instructions */
  { 0x58000000, 0xf800ffff, PIA_SKIP },		/* or/or.u with immed of 0 */
  { 0xf4005800, 0xfc1fffe0, PIA_SKIP },		/* or rd, r0, rs */
  { 0xf4005800, 0xfc00ffff, PIA_SKIP },		/* or rd, rs, r0 */

  /* Stack pointer setup: "subu sp, sp, n" where n is a multiple of 8 */
  { 0x67ff0000, 0xffff0007, PIA_NOTE_SP_ADJUSTMENT },

  /* Frame pointer assignment: "addu r30, r31, n" */
  { 0x63df0000, 0xffff0000, PIA_NOTE_FP_ASSIGNMENT },

  /* Store to stack instructions; either "st rx, sp, n" or "st.d rx, sp, n" */
  { 0x241f0000, 0xfc1f0000, PIA_NOTE_ST },	/* st rx, sp, n */
  { 0x201f0000, 0xfc1f0000, PIA_NOTE_STD },	/* st.d rs, sp, n */

  /* Instructions needed for setting up r25 for pic code. */
  { 0x5f200000, 0xffff0000, PIA_SKIP },		/* or.u r25, r0, offset_high */
  { 0xcc000002, 0xffffffff, PIA_SKIP },		/* bsr.n Lab */
  { 0x5b390000, 0xffff0000, PIA_SKIP },		/* or r25, r25, offset_low */
  { 0xf7396001, 0xffffffff, PIA_SKIP },		/* Lab: addu r25, r25, r1 */

  /* Various branch or jump instructions which have a delay slot -- these
     do not form part of the prologue, but the instruction in the delay
     slot might be a store instruction which should be noted. */
  { 0xc4000000, 0xe4000000, PIA_NOTE_PROLOGUE_END }, 
  					/* br.n, bsr.n, bb0.n, or bb1.n */
  { 0xec000000, 0xfc000000, PIA_NOTE_PROLOGUE_END }, /* bcnd.n */
  { 0xf400c400, 0xfffff7e0, PIA_NOTE_PROLOGUE_END } /* jmp.n or jsr.n */

};


/* Fetch the instruction at ADDR, returning 0 if ADDR is beyond LIM or
   is not the address of a valid instruction, the address of the next
   instruction beyond ADDR otherwise.  *PWORD1 receives the first word
   of the instruction. */

#define NEXT_PROLOGUE_INSN(addr, lim, pword1) \
  (((addr) < (lim)) ? next_insn (addr, pword1) : 0)

/* Read the m88k instruction at 'memaddr' and return the address of 
   the next instruction after that, or 0 if 'memaddr' is not the
   address of a valid instruction.  The instruction
   is stored at 'pword1'.  */

CORE_ADDR
next_insn (memaddr, pword1)
     unsigned long *pword1;
     CORE_ADDR memaddr;
{
  *pword1 = read_memory_integer (memaddr, BYTES_PER_88K_INSN);
  return memaddr + BYTES_PER_88K_INSN;
}

/* Read a register from frames called by us (or from the hardware regs).  */

static int
read_next_frame_reg(frame, regno)
     struct frame_info *frame;
     int regno;
{
  for (; frame; frame = frame->next) {
      if (regno == SP_REGNUM)
	return FRAME_FP (frame);
      else if (frame->fsr->regs[regno])
	return read_memory_integer(frame->fsr->regs[regno], 4);
  }
  return read_register(regno);
}

/* Examine the prologue of a function.  `ip' points to the first instruction.
   `limit' is the limit of the prologue (e.g. the addr of the first 
   linenumber, or perhaps the program counter if we're stepping through).
   `frame_sp' is the stack pointer value in use in this frame.  
   `fsr' is a pointer to a frame_saved_regs structure into which we put
   info about the registers saved by this frame.  
   `fi' is a struct frame_info pointer; we fill in various fields in it
   to reflect the offsets of the arg pointer and the locals pointer.  */

static CORE_ADDR
examine_prologue (ip, limit, frame_sp, fsr, fi)
     register CORE_ADDR ip;
     register CORE_ADDR limit;
     CORE_ADDR frame_sp;
     struct frame_saved_regs *fsr;
     struct frame_info *fi;
{
  register CORE_ADDR next_ip;
  register int src;
  unsigned int insn;
  int size, offset;
  char must_adjust[32];		/* If set, must adjust offsets in fsr */
  int sp_offset = -1;		/* -1 means not set (valid must be mult of 8) */
  int fp_offset = -1;		/* -1 means not set */
  CORE_ADDR frame_fp;
  CORE_ADDR prologue_end = 0;

  memset (must_adjust, '\0', sizeof (must_adjust));
  next_ip = NEXT_PROLOGUE_INSN (ip, limit, &insn);

  while (next_ip)
    {
      struct prologue_insns *pip; 

      for (pip=prologue_insn_tbl; (insn & pip->mask) != pip->insn; )
	  if (++pip >= prologue_insn_tbl + sizeof prologue_insn_tbl)
	      goto end_of_prologue_found;	/* not a prologue insn */

      switch (pip->action)
	{
	  case PIA_NOTE_ST:
	  case PIA_NOTE_STD:
	    if (sp_offset != -1) {
		src = ST_SRC (insn);
		offset = ST_OFFSET (insn);
		must_adjust[src] = 1;
		fsr->regs[src++] = offset;	/* Will be adjusted later */
		if (pip->action == PIA_NOTE_STD && src < 32)
		  {
		    offset += 4;
		    must_adjust[src] = 1;
		    fsr->regs[src++] = offset;
		  }
	    }
	    else
		goto end_of_prologue_found;
	    break;
	  case PIA_NOTE_SP_ADJUSTMENT:
	    if (sp_offset == -1)
		sp_offset = -SUBU_OFFSET (insn);
	    else
		goto end_of_prologue_found;
	    break;
	  case PIA_NOTE_FP_ASSIGNMENT:
	    if (fp_offset == -1)
		fp_offset = ADDU_OFFSET (insn);
	    else
		goto end_of_prologue_found;
	    break;
	  case PIA_NOTE_PROLOGUE_END:
	    if (!prologue_end)
		prologue_end = ip;
	    break;
	  case PIA_SKIP:
	  default :
	    /* Do nothing */
	    break;
	}

      ip = next_ip;
      next_ip = NEXT_PROLOGUE_INSN (ip, limit, &insn);
    }

end_of_prologue_found:

    if (prologue_end)
	ip = prologue_end;

  /* We're done with the prologue.  If we don't care about the stack
     frame itself, just return.  (Note that fsr->regs has been trashed,
     but the one caller who calls with fi==0 passes a dummy there.)  */

  if (fi == 0)
    return ip;

  /*
     OK, now we have:

     	sp_offset	original (before any alloca calls) displacement of SP
			(will be negative).

	fp_offset	displacement from original SP to the FP for this frame
			or -1.

	fsr->regs[0..31]	displacement from original SP to the stack
				location where reg[0..31] is stored.

	must_adjust[0..31]	set if corresponding offset was set.

     If alloca has been called between the function prologue and the current
     IP, then the current SP (frame_sp) will not be the original SP as set by
     the function prologue.  If the current SP is not the original SP, then the
     compiler will have allocated an FP for this frame, fp_offset will be set,
     and we can use it to calculate the original SP.

     Then, we figure out where the arguments and locals are, and relocate the
     offsets in fsr->regs to absolute addresses.  */

  if (fp_offset != -1) {
    /* We have a frame pointer, so get it, and base our calc's on it.  */
    frame_fp = (CORE_ADDR) read_next_frame_reg (fi->next, ACTUAL_FP_REGNUM);
    frame_sp = frame_fp - fp_offset;
  } else {
    /* We have no frame pointer, therefore frame_sp is still the same value
       as set by prologue.  But where is the frame itself?  */
    if (must_adjust[SRP_REGNUM]) {
      /* Function header saved SRP (r1), the return address.  Frame starts
	 4 bytes down from where it was saved.  */
      frame_fp = frame_sp + fsr->regs[SRP_REGNUM] - 4;
      fi->locals_pointer = frame_fp;
    } else {
      /* Function header didn't save SRP (r1), so we are in a leaf fn or
	 are otherwise confused.  */
      frame_fp = -1;
    }
  }

  /* The locals are relative to the FP (whether it exists as an allocated
     register, or just as an assumed offset from the SP) */
  fi->locals_pointer = frame_fp;

  /* The arguments are just above the SP as it was before we adjusted it
     on entry.  */
  fi->args_pointer = frame_sp - sp_offset;

  /* Now that we know the SP value used by the prologue, we know where
     it saved all the registers.  */
  for (src = 0; src < 32; src++)
    if (must_adjust[src])
      fsr->regs[src] += frame_sp;
 
  /* The saved value of the SP is always known.  */
  /* (we hope...) */
  if (fsr->regs[SP_REGNUM] != 0 
   && fsr->regs[SP_REGNUM] != frame_sp - sp_offset)
    fprintf_unfiltered(gdb_stderr, "Bad saved SP value %x != %x, offset %x!\n",
        fsr->regs[SP_REGNUM],
	frame_sp - sp_offset, sp_offset);

  fsr->regs[SP_REGNUM] = frame_sp - sp_offset;

  return (ip);
}

/* Given an ip value corresponding to the start of a function,
   return the ip of the first instruction after the function 
   prologue.  */

CORE_ADDR
skip_prologue (ip)
     CORE_ADDR (ip);
{
  struct frame_saved_regs saved_regs_dummy;
  struct symtab_and_line sal;
  CORE_ADDR limit;

  sal = find_pc_line (ip, 0);
  limit = (sal.end) ? sal.end : 0xffffffff;

  return (examine_prologue (ip, limit, (CORE_ADDR) 0, &saved_regs_dummy,
			    (struct frame_info *)0 ));
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.

   We cache the result of doing this in the frame_cache_obstack, since
   it is fairly expensive.  */

void
frame_find_saved_regs (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  register struct frame_saved_regs *cache_fsr;
  extern struct obstack frame_cache_obstack;
  CORE_ADDR ip;
  struct symtab_and_line sal;
  CORE_ADDR limit;

  if (!fi->fsr)
    {
      cache_fsr = (struct frame_saved_regs *)
		  obstack_alloc (&frame_cache_obstack,
				 sizeof (struct frame_saved_regs));
      memset (cache_fsr, '\0', sizeof (struct frame_saved_regs));
      fi->fsr = cache_fsr;

      /* Find the start and end of the function prologue.  If the PC
	 is in the function prologue, we only consider the part that
	 has executed already.  In the case where the PC is not in
	 the function prologue, we set limit to two instructions beyond
	 where the prologue ends in case if any of the prologue instructions
	 were moved into a delay slot of a branch instruction. */
         
      ip = get_pc_function_start (fi->pc);
      sal = find_pc_line (ip, 0);
      limit = (sal.end && sal.end < fi->pc) ? sal.end + 2 * BYTES_PER_88K_INSN 
					    : fi->pc;

      /* This will fill in fields in *fi as well as in cache_fsr.  */
#ifdef SIGTRAMP_FRAME_FIXUP
      if (fi->signal_handler_caller)
	SIGTRAMP_FRAME_FIXUP(fi->frame);
#endif
      examine_prologue (ip, limit, fi->frame, cache_fsr, fi);
#ifdef SIGTRAMP_SP_FIXUP
      if (fi->signal_handler_caller && fi->fsr->regs[SP_REGNUM])
	SIGTRAMP_SP_FIXUP(fi->fsr->regs[SP_REGNUM]);
#endif
    }

  if (fsr)
    *fsr = *fi->fsr;
}

/* Return the address of the locals block for the frame
   described by FI.  Returns 0 if the address is unknown.
   NOTE!  Frame locals are referred to by negative offsets from the
   argument pointer, so this is the same as frame_args_address().  */

CORE_ADDR
frame_locals_address (fi)
     struct frame_info *fi;
{
  struct frame_saved_regs fsr;

  if (fi->args_pointer)	/* Cached value is likely there.  */
    return fi->args_pointer;

  /* Nope, generate it.  */

  get_frame_saved_regs (fi, &fsr);

  return fi->args_pointer;
}

/* Return the address of the argument block for the frame
   described by FI.  Returns 0 if the address is unknown.  */

CORE_ADDR
frame_args_address (fi)
     struct frame_info *fi;
{
  struct frame_saved_regs fsr;

  if (fi->args_pointer)		/* Cached value is likely there.  */
    return fi->args_pointer;

  /* Nope, generate it.  */

  get_frame_saved_regs (fi, &fsr);

  return fi->args_pointer;
}

/* Return the saved PC from this frame.

   If the frame has a memory copy of SRP_REGNUM, use that.  If not,
   just use the register SRP_REGNUM itself.  */

CORE_ADDR
frame_saved_pc (frame)
     struct frame_info *frame;
{
  return read_next_frame_reg(frame, SRP_REGNUM);
}


#define DUMMY_FRAME_SIZE 192

static void
write_word (sp, word)
     CORE_ADDR sp;
     unsigned LONGEST word;
{
  register int len = REGISTER_SIZE;
  char buffer[MAX_REGISTER_RAW_SIZE];

  store_unsigned_integer (buffer, len, word);
  write_memory (sp, buffer, len);
}

void
m88k_push_dummy_frame()
{
  register CORE_ADDR sp = read_register (SP_REGNUM);
  register int rn;
  int offset;

  sp -= DUMMY_FRAME_SIZE;	/* allocate a bunch of space */

  for (rn = 0, offset = 0; rn <= SP_REGNUM; rn++, offset+=4)
    write_word (sp+offset, read_register(rn));
  
  write_word (sp+offset, read_register (SXIP_REGNUM));
  offset += 4;

  write_word (sp+offset, read_register (SNIP_REGNUM));
  offset += 4;

  write_word (sp+offset, read_register (SFIP_REGNUM));
  offset += 4;

  write_word (sp+offset, read_register (PSR_REGNUM));
  offset += 4;

  write_word (sp+offset, read_register (FPSR_REGNUM));
  offset += 4;

  write_word (sp+offset, read_register (FPCR_REGNUM));
  offset += 4;

  write_register (SP_REGNUM, sp);
  write_register (ACTUAL_FP_REGNUM, sp);
}

void
pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR fp;
  register int regnum;
  struct frame_saved_regs fsr;

  fp = FRAME_FP (frame);
  get_frame_saved_regs (frame, &fsr);

  if (PC_IN_CALL_DUMMY (read_pc (), read_register (SP_REGNUM), FRAME_FP (fi)))
    {
      /* FIXME: I think get_frame_saved_regs should be handling this so
	 that we can deal with the saved registers properly (e.g. frame
	 1 is a call dummy, the user types "frame 2" and then "print $ps").  */
      register CORE_ADDR sp = read_register (ACTUAL_FP_REGNUM);
      int offset;

      for (regnum = 0, offset = 0; regnum <= SP_REGNUM; regnum++, offset+=4)
	(void) write_register (regnum, read_memory_integer (sp+offset, 4));
  
      write_register (SXIP_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

      write_register (SNIP_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

      write_register (SFIP_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

      write_register (PSR_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

      write_register (FPSR_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

      write_register (FPCR_REGNUM, read_memory_integer (sp+offset, 4));
      offset += 4;

    }
  else 
    {
      for (regnum = FP_REGNUM ; regnum > 0 ; regnum--)
	  if (fsr.regs[regnum])
	      write_register (regnum,
			      read_memory_integer (fsr.regs[regnum], 4));
      write_pc (frame_saved_pc (frame));
    }
  reinit_frame_cache ();
}

void
_initialize_m88k_tdep ()
{
  tm_print_insn = print_insn_m88k;
}
