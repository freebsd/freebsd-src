/* Definitions to make GDB run on a mips box under Mach 3.0
   Copyright (C) 1992 Free Software Foundation, Inc.

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

/* Mach specific routines for little endian mips (e.g. pmax)
 * running Mach 3.0
 *
 * Author: Jukka Virtanen <jtv@hut.fi>
 */

#include "defs.h"
#include "inferior.h"

#include <stdio.h>

#include <mach.h>
#include <mach/message.h>
#include <mach/exception.h>
#include <mach_error.h>

/* Find offsets to thread states at compile time.
 * If your compiler does not grok this, check the hand coded
 * offsets and use them.
 */

#if 1

#define  REG_OFFSET(reg) (int)(&((struct mips_thread_state *)0)->reg)
#define CREG_OFFSET(reg) (int)(&((struct mips_float_state *)0)->reg)
#define EREG_OFFSET(reg) (int)(&((struct mips_exc_state *)0)->reg)

/* at reg_offset[i] is the offset to the mips_thread_state
 * location where the gdb registers[i] is stored.
 *
 * -1 means mach does not save it anywhere.
 */
static int reg_offset[] = 
{
  /*  zero		at		  v0		    v1       */
      -1,           REG_OFFSET(r1),   REG_OFFSET(r2),   REG_OFFSET(r3),

  /*  a0		a1		  a2		    a3       */
  REG_OFFSET(r4),   REG_OFFSET(r5),   REG_OFFSET(r6),   REG_OFFSET(r7),

  /*  t0		t1		  t2		    t3       */
  REG_OFFSET(r8),   REG_OFFSET(r9),   REG_OFFSET(r10),  REG_OFFSET(r11),

  /*  t4		t5		  t6		    t7       */
  REG_OFFSET(r12),  REG_OFFSET(r13),  REG_OFFSET(r14),  REG_OFFSET(r15),

  /*  s0		s1		  s2		    s3       */
  REG_OFFSET(r16),  REG_OFFSET(r17),  REG_OFFSET(r18),  REG_OFFSET(r19),

  /*  s4		s5		  s6		    s7       */
  REG_OFFSET(r20),  REG_OFFSET(r21),  REG_OFFSET(r22),  REG_OFFSET(r23),

  /*  t8		t9		  k0		    k1       */
  REG_OFFSET(r24),  REG_OFFSET(r25),  REG_OFFSET(r26),  REG_OFFSET(r27),

  /*  gp		sp	      s8(30) == fp(72)	    ra       */
  REG_OFFSET(r28),  REG_OFFSET(r29),  REG_OFFSET(r30),  REG_OFFSET(r31),

  /*  sr(32) PS_REGNUM   */
  EREG_OFFSET(coproc_state),

  /*  lo(33)		hi(34)    */
  REG_OFFSET(mdlo), REG_OFFSET(mdhi),

  /*  bad(35)          	      cause(36)	         pc(37)  */
  EREG_OFFSET(address),  EREG_OFFSET(cause), REG_OFFSET(pc),

  /*  f0(38)		 f1(39)		    f2(40)	       f3(41)   */
  CREG_OFFSET(r0),   CREG_OFFSET(r1),   CREG_OFFSET(r2),   CREG_OFFSET(r3),
  CREG_OFFSET(r4),   CREG_OFFSET(r5),   CREG_OFFSET(r6),   CREG_OFFSET(r7),
  CREG_OFFSET(r8),   CREG_OFFSET(r9),   CREG_OFFSET(r10),  CREG_OFFSET(r11),
  CREG_OFFSET(r12),  CREG_OFFSET(r13),  CREG_OFFSET(r14),  CREG_OFFSET(r15),
  CREG_OFFSET(r16),  CREG_OFFSET(r17),  CREG_OFFSET(r18),  CREG_OFFSET(r19),
  CREG_OFFSET(r20),  CREG_OFFSET(r21),  CREG_OFFSET(r22),  CREG_OFFSET(r23),
  CREG_OFFSET(r24),  CREG_OFFSET(r25),  CREG_OFFSET(r26),  CREG_OFFSET(r27),
  CREG_OFFSET(r28),  CREG_OFFSET(r29),  CREG_OFFSET(r30),  CREG_OFFSET(r31),

  /*  fsr(70)		fir(71)		fp(72) == s8(30) */
  CREG_OFFSET(csr),  CREG_OFFSET(esr),  REG_OFFSET(r30)
};
#else
/* If the compiler does not grok the above defines */
static int reg_offset[] = 
{
/* mach_thread_state offsets: */
  -1,  0,  4,  8,  12, 16, 20, 24,  28, 32, 36, 40,  44, 48, 52, 56,
  60, 64, 68, 72,  76, 80, 84, 88,  92, 96,100,104, 108,112,116,120,
/*sr, lo, hi,addr,cause,pc   */
   8,124,128,  4,   0,132,
/* mach_float_state offsets: */
   0,  4,  8, 12,  16, 20, 24, 28,  32, 36, 40, 44,  48, 52, 56, 60,
  64, 68, 72, 76,  80, 84, 88, 92,  96,100,104,108, 112,116,120,124,
/*fsr,fir*/
 128,132,
/* FP_REGNUM pseudo maps to s8==r30 in mach_thread_state */
 116
};
#endif

/* Fetch COUNT contiguous registers from thread STATE starting from REGNUM
 * Caller knows that the regs handled in one transaction are of same size.
 */
#define FETCH_REGS(state, regnum, count) \
  memcpy (&registers[REGISTER_BYTE (regnum)], \
	  (char *)state+reg_offset[ regnum ], \
	  count*REGISTER_SIZE)

/* Store COUNT contiguous registers to thread STATE starting from REGNUM */
#define STORE_REGS(state, regnum, count) \
  memcpy ((char *)state+reg_offset[ regnum ], \
	  &registers[REGISTER_BYTE (regnum)], \
	  count*REGISTER_SIZE)

#define REGS_ALL    -1
#define REGS_NORMAL  1
#define REGS_EXC     2
#define REGS_COP1    4

/* Hardware regs that matches FP_REGNUM */
#define MACH_FP_REGNUM 30

/* Fech thread's registers. if regno == -1, fetch all regs */
void
fetch_inferior_registers (regno)
     int regno;
{
  kern_return_t ret;  

  thread_state_data_t      state;
  struct mips_exc_state    exc_state;

  int stateCnt   = MIPS_THREAD_STATE_COUNT;

  int which_regs = 0; /* A bit mask */

  if (! MACH_PORT_VALID (current_thread))
    error ("fetch inferior registers: Invalid thread");
  
  if (regno < -1 || regno >= NUM_REGS)
    error ("invalid register %d supplied to fetch_inferior_registers", regno);

  if (regno == -1)
    which_regs = REGS_ALL;
  else if (regno == ZERO_REGNUM)
    {
      int zero = 0;
      supply_register (ZERO_REGNUM, &zero);
      return;
    }
  else if ((ZERO_REGNUM < regno && regno < PS_REGNUM)
	   || regno == FP_REGNUM
	   || regno == LO_REGNUM
	   || regno == HI_REGNUM
	   || regno == PC_REGNUM)
    which_regs = REGS_NORMAL;
  else if (FP0_REGNUM <= regno && regno <= FCRIR_REGNUM)
    which_regs = REGS_COP1 | REGS_EXC;
  else
    which_regs = REGS_EXC;

  /* fetch regs saved to mips_thread_state */
  if (which_regs & REGS_NORMAL)
    {
      ret = thread_get_state (current_thread,
			      MIPS_THREAD_STATE,
			      state,
			      &stateCnt);
      CHK ("fetch inferior registers: thread_get_state", ret);

      if (which_regs == REGS_NORMAL)
	{
	  /* Fetch also FP_REGNUM if fetching MACH_FP_REGNUM and vice versa */
	  if (regno == MACH_FP_REGNUM || regno == FP_REGNUM)
	    {
	      supply_register (FP_REGNUM,
			       (char *)state+reg_offset[ MACH_FP_REGNUM ]);
	      supply_register (MACH_FP_REGNUM,
			       (char *)state+reg_offset[ MACH_FP_REGNUM ]);
	    }
	  else
	    supply_register (regno,
			     (char *)state+reg_offset[ regno ]);
	  return;
	}
			   
      /* ZERO_REGNUM is always zero */
      *(int *) registers = 0;
      
      /* Copy thread saved regs 1..31 to gdb's reg value array
       * Luckily, they are contiquous
       */
      FETCH_REGS (state, 1, 31);
      
      /* Copy mdlo and mdhi */
      FETCH_REGS (state, LO_REGNUM, 2);

      /* Copy PC */
      FETCH_REGS (state, PC_REGNUM, 1);

      /* Mach 3.0 saves FP to MACH_FP_REGNUM.
       * For some reason gdb wants to assign a pseudo register for it.
       */
      FETCH_REGS (state, FP_REGNUM, 1);
    }

  /* Read exc state. Also read if need to fetch floats */
  if (which_regs & REGS_EXC)
    {
      stateCnt = MIPS_EXC_STATE_COUNT;
      ret = thread_get_state (current_thread,
			      MIPS_EXC_STATE,
			      (thread_state_t) &exc_state,
			      &stateCnt);
      CHK ("fetch inferior regs (exc): thread_get_state", ret);

      /* We need to fetch exc_state to see if the floating
       * state is valid for the thread.
       */

      /* cproc_state: Which coprocessors the thread uses */
      supply_register (PS_REGNUM,
		       (char *)&exc_state+reg_offset[ PS_REGNUM ]);
      
      if (which_regs == REGS_EXC || which_regs == REGS_ALL)
	{
	  supply_register (BADVADDR_REGNUM,
			   (char *)&exc_state+reg_offset[ BADVADDR_REGNUM ]);
	  
	  supply_register (CAUSE_REGNUM,
			   (char *)&exc_state+reg_offset[ CAUSE_REGNUM ]);
	  if (which_regs == REGS_EXC)
	    return;
	}
    }


  if (which_regs & REGS_COP1)
    {
      /* If the thread does not have saved COPROC1, set regs to zero */
      
      if (! (exc_state.coproc_state & MIPS_STATUS_USE_COP1))
	bzero (&registers[ REGISTER_BYTE (FP0_REGNUM) ],
	       sizeof (struct mips_float_state));
      else
	{
	  stateCnt = MIPS_FLOAT_STATE_COUNT;
	  ret = thread_get_state (current_thread,
				  MIPS_FLOAT_STATE,
				  state,
				  &stateCnt);
	  CHK ("fetch inferior regs (floats): thread_get_state", ret);
	  
	  if (regno != -1)
	    {
	      supply_register (regno,
			       (char *)state+reg_offset[ regno ]);
	      return;
	    }
	  
	  FETCH_REGS (state, FP0_REGNUM, 34);
	}
    }
  
  /* All registers are valid, if not returned yet */
  registers_fetched ();
}

/* Store gdb's view of registers to the thread.
 * All registers are always valid when entering here.
 * @@ ahem, maybe that is too strict, we could validate the necessary ones
 *    here.
 *
 * Hmm. It seems that gdb set $reg=value command first reads everything,
 * then sets the reg and then stores everything. -> we must make sure
 * that the immutable registers are not changed by reading them first.
 */

void
store_inferior_registers (regno)
     register int regno;
{
  thread_state_data_t state;
  kern_return_t ret;
  
  if (! MACH_PORT_VALID (current_thread))
    error ("store inferior registers: Invalid thread");
  
  /* Check for read only regs.
   * @@ If some of these is can be changed, fix this
   */
  if (regno == ZERO_REGNUM	||
      regno == PS_REGNUM	||
      regno == BADVADDR_REGNUM	||
      regno == CAUSE_REGNUM	||
      regno == FCRIR_REGNUM)
    {
      message ("You can not alter read-only register `%s'",
	       reg_names[ regno ]);
      fetch_inferior_registers (regno);
      return;
    }

  if (regno == -1)
    {
      /* Don't allow these to change */

      /* ZERO_REGNUM */
      *(int *)registers = 0;
      
      fetch_inferior_registers (PS_REGNUM);
      fetch_inferior_registers (BADVADDR_REGNUM);
      fetch_inferior_registers (CAUSE_REGNUM);
      fetch_inferior_registers (FCRIR_REGNUM);
    }

  if (regno == -1 || (ZERO_REGNUM < regno && regno <= PC_REGNUM))
    {
#if 1
      /* Mach 3.0 saves thread's FP to MACH_FP_REGNUM.
       * GDB wants assigns a pseudo register FP_REGNUM for frame pointer.
       *
       * @@@ Here I assume (!) that gdb's FP has the value that
       *     should go to threads frame pointer. If not true, this
       *     fails badly!!!!!
       */
      memcpy (&registers[REGISTER_BYTE (MACH_FP_REGNUM)],
	      &registers[REGISTER_BYTE (FP_REGNUM)],
	      REGISTER_RAW_SIZE (FP_REGNUM));
#endif
      
      /* Save gdb's regs 1..31 to thread saved regs 1..31
       * Luckily, they are contiquous
       */
      STORE_REGS (state, 1, 31);

      /* Save mdlo, mdhi */
      STORE_REGS (state, LO_REGNUM, 2);

     /* Save PC */
      STORE_REGS (state, PC_REGNUM, 1);

      ret = thread_set_state (current_thread,
			      MIPS_THREAD_STATE,
			      state,
			      MIPS_FLOAT_STATE_COUNT);
      CHK ("store inferior regs : thread_set_state", ret);
    }
  
  if (regno == -1 || regno >= FP0_REGNUM)
    {
      /* If thread has floating state, save it */
      if (read_register (PS_REGNUM) & MIPS_STATUS_USE_COP1)
	{
	  /* Do NOT save FCRIR_REGNUM */
	  STORE_REGS (state, FP0_REGNUM, 33);
	
	  ret = thread_set_state (current_thread,
				  MIPS_FLOAT_STATE,
				  state,
				  MIPS_FLOAT_STATE_COUNT);
	  CHK ("store inferior registers (floats): thread_set_state", ret);
	}
      else if (regno != -1)
	message
	  ("Thread does not use floating point unit, floating regs not saved");
    }
}
