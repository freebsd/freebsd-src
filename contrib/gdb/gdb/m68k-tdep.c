/* Target dependent code for the Motorola 68000 series.
   Copyright (C) 1990, 1992 Free Software Foundation, Inc.

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
#include "symtab.h"


/* Push an empty stack frame, to record the current PC, etc.  */

void
m68k_push_dummy_frame ()
{
  register CORE_ADDR sp = read_register (SP_REGNUM);
  register int regnum;
  char raw_buffer[12];

  sp = push_word (sp, read_register (PC_REGNUM));
  sp = push_word (sp, read_register (FP_REGNUM));
  write_register (FP_REGNUM, sp);

  /* Always save the floating-point registers, whether they exist on
     this target or not.  */
  for (regnum = FP0_REGNUM + 7; regnum >= FP0_REGNUM; regnum--)
    {
      read_register_bytes (REGISTER_BYTE (regnum), raw_buffer, 12);
      sp = push_bytes (sp, raw_buffer, 12);
    }

  for (regnum = FP_REGNUM - 1; regnum >= 0; regnum--)
    {
      sp = push_word (sp, read_register (regnum));
    }
  sp = push_word (sp, read_register (PS_REGNUM));
  write_register (SP_REGNUM, sp);
}

/* Discard from the stack the innermost frame,
   restoring all saved registers.  */

void
m68k_pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR fp;
  register int regnum;
  struct frame_saved_regs fsr;
  struct frame_info *fi;
  char raw_buffer[12];

  fp = FRAME_FP (frame);
  get_frame_saved_regs (frame, &fsr);
  for (regnum = FP0_REGNUM + 7 ; regnum >= FP0_REGNUM ; regnum--)
    {
      if (fsr.regs[regnum])
	{
	  read_memory (fsr.regs[regnum], raw_buffer, 12);
	  write_register_bytes (REGISTER_BYTE (regnum), raw_buffer, 12);
	}
    }
  for (regnum = FP_REGNUM - 1 ; regnum >= 0 ; regnum--)
    {
      if (fsr.regs[regnum])
	{
	  write_register (regnum, read_memory_integer (fsr.regs[regnum], 4));
	}
    }
  if (fsr.regs[PS_REGNUM])
    {
      write_register (PS_REGNUM, read_memory_integer (fsr.regs[PS_REGNUM], 4));
    }
  write_register (FP_REGNUM, read_memory_integer (fp, 4));
  write_register (PC_REGNUM, read_memory_integer (fp + 4, 4));
  write_register (SP_REGNUM, fp + 8);
  flush_cached_frames ();
}


/* Given an ip value corresponding to the start of a function,
   return the ip of the first instruction after the function 
   prologue.  This is the generic m68k support.  Machines which
   require something different can override the SKIP_PROLOGUE
   macro to point elsewhere.

   Some instructions which typically may appear in a function
   prologue include:

   A link instruction, word form:

	link.w	%a6,&0			4e56  XXXX

   A link instruction, long form:

	link.l  %fp,&F%1		480e  XXXX  XXXX

   A movm instruction to preserve integer regs:

	movm.l  &M%1,(4,%sp)		48ef  XXXX  XXXX

   A fmovm instruction to preserve float regs:

	fmovm   &FPM%1,(FPO%1,%sp)	f237  XXXX  XXXX  XXXX  XXXX

   Some profiling setup code (FIXME, not recognized yet):

	lea.l   (.L3,%pc),%a1		43fb  XXXX  XXXX  XXXX
	bsr     _mcount			61ff  XXXX  XXXX

  */

#define P_LINK_L	0x480e
#define P_LINK_W	0x4e56
#define P_MOV_L		0x207c
#define P_JSR		0x4eb9
#define P_BSR		0x61ff
#define P_LEA_L		0x43fb
#define P_MOVM_L	0x48ef
#define P_FMOVM		0xf237
#define P_TRAP		0x4e40

CORE_ADDR
m68k_skip_prologue (ip)
CORE_ADDR ip;
{
  register CORE_ADDR limit;
  struct symtab_and_line sal;
  register int op;

  /* Find out if there is a known limit for the extent of the prologue.
     If so, ensure we don't go past it.  If not, assume "infinity". */

  sal = find_pc_line (ip, 0);
  limit = (sal.end) ? sal.end : (CORE_ADDR) ~0;

  while (ip < limit)
    {
      op = read_memory_integer (ip, 2);
      op &= 0xFFFF;
      
      if (op == P_LINK_W)
	{
	  ip += 4;	/* Skip link.w */
	}
      else if (op == 0x4856)
	ip += 2; /* Skip pea %fp */
      else if (op == 0x2c4f)
	ip += 2; /* Skip move.l %sp, %fp */
      else if (op == P_LINK_L)
	{
	  ip += 6;	/* Skip link.l */
	}
      else if (op == P_MOVM_L)
	{
	  ip += 6;	/* Skip movm.l */
	}
      else if (op == P_FMOVM)
	{
	  ip += 10;	/* Skip fmovm */
	}
      else
	{
	  break;	/* Found unknown code, bail out. */
	}
    }
  return (ip);
}

void
m68k_find_saved_regs (frame_info, saved_regs)
     struct frame_info *frame_info;
     struct frame_saved_regs *saved_regs;
{
  register int regnum;							
  register int regmask;							
  register CORE_ADDR next_addr;						
  register CORE_ADDR pc;

  /* First possible address for a pc in a call dummy for this frame.  */
  CORE_ADDR possible_call_dummy_start =
    (frame_info)->frame - CALL_DUMMY_LENGTH - FP_REGNUM*4 - 4 - 8*12;

  int nextinsn;
  memset (saved_regs, 0, sizeof (*saved_regs));
  if ((frame_info)->pc >= possible_call_dummy_start
      && (frame_info)->pc <= (frame_info)->frame)
    {

      /* It is a call dummy.  We could just stop now, since we know
	 what the call dummy saves and where.  But this code proceeds
	 to parse the "prologue" which is part of the call dummy.
	 This is needlessly complex and confusing.  FIXME.  */

      next_addr = (frame_info)->frame;
      pc = possible_call_dummy_start;
    }
  else   								
    {
      pc = get_pc_function_start ((frame_info)->pc); 			

      if (0x4856 == read_memory_integer (pc, 2)
	  && 0x2c4f == read_memory_integer (pc + 2, 2))
	{
	  /*
	    pea %fp
            move.l %sp, %fp */

	  pc += 4;
	  next_addr = frame_info->frame;
	}
      else if (044016 == read_memory_integer (pc, 2))
	/* link.l %fp */
	/* Find the address above the saved   
	   regs using the amount of storage from the link instruction.  */
	next_addr = (frame_info)->frame + read_memory_integer (pc += 2, 4), pc+=4; 
      else if (047126 == read_memory_integer (pc, 2))			
	/* link.w %fp */
	/* Find the address above the saved   
	   regs using the amount of storage from the link instruction.  */
	next_addr = (frame_info)->frame + read_memory_integer (pc += 2, 2), pc+=2; 
      else goto lose;

      /* If have an addal #-n, sp next, adjust next_addr.  */		
      if ((0177777 & read_memory_integer (pc, 2)) == 0157774)		
	next_addr += read_memory_integer (pc += 2, 4), pc += 4;		
    }									
  regmask = read_memory_integer (pc + 2, 2);				

  /* Here can come an fmovem.  Check for it.  */		
  nextinsn = 0xffff & read_memory_integer (pc, 2);			
  if (0xf227 == nextinsn						
      && (regmask & 0xff00) == 0xe000)					
    { pc += 4; /* Regmask's low bit is for register fp7, the first pushed */ 
      for (regnum = FP0_REGNUM + 7; regnum >= FP0_REGNUM; regnum--, regmask >>= 1)		
	if (regmask & 1)						
          saved_regs->regs[regnum] = (next_addr -= 12);		
      regmask = read_memory_integer (pc + 2, 2); }

  /* next should be a moveml to (sp) or -(sp) or a movl r,-(sp) */	
  if (0044327 == read_memory_integer (pc, 2))				
    { pc += 4; /* Regmask's low bit is for register 0, the first written */ 
      for (regnum = 0; regnum < 16; regnum++, regmask >>= 1)		
	if (regmask & 1)						
          saved_regs->regs[regnum] = (next_addr += 4) - 4; }	
  else if (0044347 == read_memory_integer (pc, 2))			
    {
      pc += 4; /* Regmask's low bit is for register 15, the first pushed */ 
      for (regnum = 15; regnum >= 0; regnum--, regmask >>= 1)		
	if (regmask & 1)						
          saved_regs->regs[regnum] = (next_addr -= 4);
    }
  else if (0x2f00 == (0xfff0 & read_memory_integer (pc, 2)))		
    {
      regnum = 0xf & read_memory_integer (pc, 2); pc += 2;		
      saved_regs->regs[regnum] = (next_addr -= 4);
      /* gcc, at least, may use a pair of movel instructions when saving
	 exactly 2 registers.  */
      if (0x2f00 == (0xfff0 & read_memory_integer (pc, 2)))
	{
	  regnum = 0xf & read_memory_integer (pc, 2);
	  pc += 2;
	  saved_regs->regs[regnum] = (next_addr -= 4);
	}
    }

  /* fmovemx to index of sp may follow.  */				
  regmask = read_memory_integer (pc + 2, 2);				
  nextinsn = 0xffff & read_memory_integer (pc, 2);			
  if (0xf236 == nextinsn						
      && (regmask & 0xff00) == 0xf000)					
    { pc += 10; /* Regmask's low bit is for register fp0, the first written */ 
      for (regnum = FP0_REGNUM + 7; regnum >= FP0_REGNUM; regnum--, regmask >>= 1)		
	if (regmask & 1)						
          saved_regs->regs[regnum] = (next_addr += 12) - 12;	
      regmask = read_memory_integer (pc + 2, 2); }			

  /* clrw -(sp); movw ccr,-(sp) may follow.  */				
  if (0x426742e7 == read_memory_integer (pc, 4))			
    saved_regs->regs[PS_REGNUM] = (next_addr -= 4);		
  lose: ;								
  saved_regs->regs[SP_REGNUM] = (frame_info)->frame + 8;		
  saved_regs->regs[FP_REGNUM] = (frame_info)->frame;		
  saved_regs->regs[PC_REGNUM] = (frame_info)->frame + 4;		
#ifdef SIG_SP_FP_OFFSET
  /* Adjust saved SP_REGNUM for fake _sigtramp frames.  */
  if (frame_info->signal_handler_caller && frame_info->next)
    saved_regs->regs[SP_REGNUM] = frame_info->next->frame + SIG_SP_FP_OFFSET;
#endif
}


#ifdef USE_PROC_FS	/* Target dependent support for /proc */

#include <sys/procfs.h>

/*  The /proc interface divides the target machine's register set up into
    two different sets, the general register set (gregset) and the floating
    point register set (fpregset).  For each set, there is an ioctl to get
    the current register set and another ioctl to set the current values.

    The actual structure passed through the ioctl interface is, of course,
    naturally machine dependent, and is different for each set of registers.
    For the m68k for example, the general register set is typically defined
    by:

	typedef int gregset_t[18];

	#define	R_D0	0
	...
	#define	R_PS	17

    and the floating point set by:

    	typedef	struct fpregset {
	  int	f_pcr;
	  int	f_psr;
	  int	f_fpiaddr;
	  int	f_fpregs[8][3];		(8 regs, 96 bits each)
	} fpregset_t;

    These routines provide the packing and unpacking of gregset_t and
    fpregset_t formatted data.

 */

/* Atari SVR4 has R_SR but not R_PS */

#if !defined (R_PS) && defined (R_SR)
#define R_PS R_SR
#endif

/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values. */

void
supply_gregset (gregsetp)
gregset_t *gregsetp;
{
  register int regi;
  register greg_t *regp = (greg_t *) gregsetp;

  for (regi = 0 ; regi < R_PC ; regi++)
    {
      supply_register (regi, (char *) (regp + regi));
    }
  supply_register (PS_REGNUM, (char *) (regp + R_PS));
  supply_register (PC_REGNUM, (char *) (regp + R_PC));
}

void
fill_gregset (gregsetp, regno)
gregset_t *gregsetp;
int regno;
{
  register int regi;
  register greg_t *regp = (greg_t *) gregsetp;
  extern char registers[];

  for (regi = 0 ; regi < R_PC ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regi) = *(int *) &registers[REGISTER_BYTE (regi)];
	}
    }
  if ((regno == -1) || (regno == PS_REGNUM))
    {
      *(regp + R_PS) = *(int *) &registers[REGISTER_BYTE (PS_REGNUM)];
    }
  if ((regno == -1) || (regno == PC_REGNUM))
    {
      *(regp + R_PC) = *(int *) &registers[REGISTER_BYTE (PC_REGNUM)];
    }
}

#if defined (FP0_REGNUM)

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), unpack the register contents and supply them as gdb's
    idea of the current floating point register values. */

void 
supply_fpregset (fpregsetp)
fpregset_t *fpregsetp;
{
  register int regi;
  char *from;
  
  for (regi = FP0_REGNUM ; regi < FPC_REGNUM ; regi++)
    {
      from = (char *) &(fpregsetp -> f_fpregs[regi-FP0_REGNUM][0]);
      supply_register (regi, from);
    }
  supply_register (FPC_REGNUM, (char *) &(fpregsetp -> f_pcr));
  supply_register (FPS_REGNUM, (char *) &(fpregsetp -> f_psr));
  supply_register (FPI_REGNUM, (char *) &(fpregsetp -> f_fpiaddr));
}

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), update the register specified by REGNO from gdb's idea
    of the current floating point register set.  If REGNO is -1, update
    them all. */

void
fill_fpregset (fpregsetp, regno)
fpregset_t *fpregsetp;
int regno;
{
  int regi;
  char *to;
  char *from;
  extern char registers[];

  for (regi = FP0_REGNUM ; regi < FPC_REGNUM ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp -> f_fpregs[regi-FP0_REGNUM][0]);
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }
  if ((regno == -1) || (regno == FPC_REGNUM))
    {
      fpregsetp -> f_pcr = *(int *) &registers[REGISTER_BYTE (FPC_REGNUM)];
    }
  if ((regno == -1) || (regno == FPS_REGNUM))
    {
      fpregsetp -> f_psr = *(int *) &registers[REGISTER_BYTE (FPS_REGNUM)];
    }
  if ((regno == -1) || (regno == FPI_REGNUM))
    {
      fpregsetp -> f_fpiaddr = *(int *) &registers[REGISTER_BYTE (FPI_REGNUM)];
    }
}

#endif	/* defined (FP0_REGNUM) */

#endif  /* USE_PROC_FS */

#ifdef GET_LONGJMP_TARGET
/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target(pc)
     CORE_ADDR *pc;
{
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];
  CORE_ADDR sp, jb_addr;

  sp = read_register(SP_REGNUM);

  if (target_read_memory (sp + SP_ARG0, /* Offset of first arg on stack */
			  buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  jb_addr = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}
#endif /* GET_LONGJMP_TARGET */

/* Immediately after a function call, return the saved pc before the frame
   is setup.  For sun3's, we check for the common case of being inside of a
   system call, and if so, we know that Sun pushes the call # on the stack
   prior to doing the trap. */

CORE_ADDR
m68k_saved_pc_after_call(frame)
     struct frame_info *frame;
{
#ifdef SYSCALL_TRAP
  int op;

  op = read_memory_integer (frame->pc - SYSCALL_TRAP_OFFSET, 2);

  if (op == SYSCALL_TRAP)
    return read_memory_integer (read_register (SP_REGNUM) + 4, 4);
  else
#endif /* SYSCALL_TRAP */
    return read_memory_integer (read_register (SP_REGNUM), 4);
}

void
_initialize_m68k_tdep ()
{
  tm_print_insn = print_insn_m68k;
}
