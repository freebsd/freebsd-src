#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include <nwtypes.h>
#include <nwdfs.h>
#include <nwconio.h>
#include <nwadv.h>
#include <nwdbgapi.h>
#include <nwthread.h>
#include "ppc.h"

extern char *mem2hex (void *mem, char *buf, int count, int may_fault);
extern char *hex2mem (char *buf, void *mem, int count, int may_fault);
extern int computeSignal (int exceptionVector);

void
flush_i_cache (void)
{
}

/* Get the registers out of the frame information.  */

void
frame_to_registers (struct StackFrame *frame, char *regs)
{
  mem2hex (&frame->ExceptionState.CsavedRegs, &regs[GP0_REGNUM * 4 * 2], 4 * 32, 0);

  mem2hex (&frame->ExceptionState.CSavedFPRegs, &regs[FP0_REGNUM * 4 * 2], 4 * 32, 0);

  mem2hex (&frame->ExceptionPC, &regs[PC_REGNUM * 4 * 2], 4 * 1, 0);

  mem2hex (&frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedSRR1, &regs[CR_REGNUM * 4 * 2], 4 * 1, 0);
  mem2hex (&frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedLR, &regs[LR_REGNUM * 4 * 2], 4 * 1, 0);
  mem2hex (&frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedCTR, &regs[CTR_REGNUM * 4 * 2], 4 * 1, 0);
  mem2hex (&frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedXER, &regs[XER_REGNUM * 4 * 2], 4 * 1, 0);
  mem2hex (&frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedMQ, &regs[MQ_REGNUM * 4 * 2], 4 * 1, 0);
}

/* Put the registers back into the frame information.  */

void
registers_to_frame (char *regs, struct StackFrame *frame)
{
  hex2mem (&regs[GP0_REGNUM * 4 * 2], &frame->ExceptionState.CsavedRegs, 4 * 32, 0);

  hex2mem (&regs[FP0_REGNUM * 4 * 2], &frame->ExceptionState.CSavedFPRegs, 4 * 32, 0);

  hex2mem (&regs[PC_REGNUM * 4 * 2], &frame->ExceptionPC, 4 * 1, 0);

  hex2mem (&regs[CR_REGNUM * 4 * 2], &frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedSRR1, 4 * 1, 0);
  hex2mem (&regs[LR_REGNUM * 4 * 2], &frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedLR, 4 * 1, 0);
  hex2mem (&regs[CTR_REGNUM * 4 * 2], &frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedCTR, 4 * 1, 0);
  hex2mem (&regs[XER_REGNUM * 4 * 2], &frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedXER, 4 * 1, 0);
  hex2mem (&regs[MQ_REGNUM * 4 * 2], &frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedMQ, 4 * 1, 0);
}


extern volatile int mem_err;

#ifdef ALTERNATE_MEM_FUNCS
extern int ReadByteAltDebugger (char* addr, char *theByte);
extern int WriteByteAltDebugger (char* addr, char theByte);
int
get_char (char *addr)
{
  char c;

  if (!ReadByteAltDebugger (addr, &c))
    mem_err = 1;

  return c;
}

void
set_char (char *addr, int val)
{
  if (!WriteByteAltDebugger (addr, val))
    mem_err = 1;
}
#endif

int
mem_write (char *dst, char *src, int len)
{
  while (len-- && !mem_err)
    set_char (dst++, *src++);

  return mem_err;
}

union inst
{
  LONG l;

  struct
    {
      union
	{
	  struct		/* Unconditional branch */
	    {
	      unsigned opcode : 6; /* 18 */
	      signed li : 24;
	      unsigned aa : 1;
	      unsigned lk : 1;
	    } b;
	  struct		/* Conditional branch */
	    {
	      unsigned opcode : 6; /* 16 */
	      unsigned bo : 5;
	      unsigned bi : 5;
	      signed bd : 14;
	      unsigned aa : 1;
	      unsigned lk : 1;
	    } bc;
	  struct		/* Conditional branch to ctr or lr reg */
	    {
	      unsigned opcode : 6; /* 19 */
	      unsigned bo : 5;
	      unsigned bi : 5;
	      unsigned type : 15; /* 528 = ctr, 16 = lr */
	      unsigned lk : 1;
	    } bclr;
	} variant;
    } inst;
};

static LONG saved_inst;
static LONG *saved_inst_pc = 0;
static LONG saved_target_inst;
static LONG *saved_target_inst_pc = 0;

void
set_step_traps (struct StackFrame *frame)
{
  union inst inst;
  LONG *target;
  int opcode;
  int ra, rb;
  LONG *pc = (LONG *)frame->ExceptionPC;

  inst.l = *pc++;

  opcode = inst.inst.variant.b.opcode;

  target = pc;

  switch (opcode)
    {
    case 18:			/* Unconditional branch */

      if (inst.inst.variant.b.aa) /* Absolute? */
	target = 0;
      target += inst.inst.variant.b.li;

      break;
    case 16:			/* Conditional branch */

      if (!inst.inst.variant.bc.aa) /* Absolute? */
	target = 0;
      target += inst.inst.variant.bc.bd;

      break;
    case 19:			/* Cond. branch via ctr or lr reg */
      switch (inst.inst.variant.bclr.type)
	{
	case 528:		/* ctr */
	  target = (LONG *)frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedCTR;
	  break;
	case 16:		/* lr */
	  target = (LONG *)frame->ExceptionState.u.SpecialRegistersEnumerated.CsavedLR;
	  break;
	}
      break;
    }

  saved_inst = *pc;
  mem_write (pc, breakpoint_insn, BREAKPOINT_SIZE);
  saved_inst_pc = pc;

  if (target != pc)
    {
      saved_target_inst = *target;
      mem_write (target, breakpoint_insn, BREAKPOINT_SIZE);
      saved_target_inst_pc = target;
    }
}

/* Remove step breakpoints.  Returns non-zero if pc was at a step breakpoint,
   zero otherwise.  This routine works even if there were no step breakpoints
   set.  */

int
clear_step_traps (struct StackFrame *frame)
{
  int retcode;
  LONG *pc = (LONG *)frame->ExceptionPC;

  if (saved_inst_pc == pc || saved_target_inst_pc == pc)
    retcode = 1;
  else
    retcode = 0;

  if (saved_inst_pc)
    {
      mem_write (saved_inst_pc, saved_inst, BREAKPOINT_SIZE);
      saved_inst_pc = 0;
    }

  if (saved_target_inst_pc)
    {
      mem_write (saved_target_inst_pc, saved_target_inst, BREAKPOINT_SIZE);
      saved_target_inst_pc = 0;
    }

  return retcode;
}

void
do_status (char *ptr, struct StackFrame *frame)
{
  int sigval;

  sigval = computeSignal (frame->ExceptionNumber);

  sprintf (ptr, "T%02x", sigval);
  ptr += 3;

  sprintf (ptr, "%02x:", PC_REGNUM);
  ptr = mem2hex (&frame->ExceptionPC, ptr + 3, 4, 0);
  *ptr++ = ';';

  sprintf (ptr, "%02x:", SP_REGNUM);
  ptr = mem2hex (&frame->ExceptionState.CsavedRegs[SP_REGNUM], ptr + 3, 4, 0);
  *ptr++ = ';';

  sprintf (ptr, "%02x:", LR_REGNUM);
  ptr = mem2hex (&frame->ExceptionState.CsavedRegs[LR_REGNUM], ptr + 3, 4, 0);
  *ptr++ = ';';

  *ptr = '\000';
}
