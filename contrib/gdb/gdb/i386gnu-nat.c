/* Low level interface to I386 running the GNU Hurd
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

#include "defs.h"
#include "inferior.h"
#include "floatformat.h"

#include <stdio.h>

#include <mach.h>
#include <mach/message.h>
#include <mach/exception.h>
#include <mach_error.h>

#include "gnu-nat.h"

/* Hmmm... Should this not be here?
 * Now for i386_float_info() target_has_execution
 */
#include <target.h>

/* @@@ Should move print_387_status() to i387-tdep.c */
extern void print_387_control_word ();		/* i387-tdep.h */
extern void print_387_status_word ();

/* Find offsets to thread states at compile time.
 * If your compiler does not grok this, calculate offsets
 * offsets yourself and use them (or get a compatible compiler :-)
 */

#define  REG_OFFSET(reg) (int)(&((struct i386_thread_state *)0)->reg)

/* at reg_offset[i] is the offset to the i386_thread_state
 * location where the gdb registers[i] is stored.
 */

static int reg_offset[] = 
{
  REG_OFFSET(eax),  REG_OFFSET(ecx), REG_OFFSET(edx), REG_OFFSET(ebx),
  REG_OFFSET(uesp), REG_OFFSET(ebp), REG_OFFSET(esi), REG_OFFSET(edi),
  REG_OFFSET(eip),  REG_OFFSET(efl), REG_OFFSET(cs),  REG_OFFSET(ss),
  REG_OFFSET(ds),   REG_OFFSET(es),  REG_OFFSET(fs),  REG_OFFSET(gs)
};

#define REG_ADDR(state,regnum) ((char *)(state)+reg_offset[regnum])

/* Fetch COUNT contiguous registers from thread STATE starting from REGNUM
 * Caller knows that the regs handled in one transaction are of same size.
 */
#define FETCH_REGS(state, regnum, count) \
  memcpy (&registers[REGISTER_BYTE (regnum)], \
	  REG_ADDR (state, regnum), \
	  count * REGISTER_RAW_SIZE (regnum))

/* Store COUNT contiguous registers to thread STATE starting from REGNUM */
#define STORE_REGS(state, regnum, count) \
  memcpy (REG_ADDR (state, regnum), \
	  &registers[REGISTER_BYTE (regnum)], \
	  count * REGISTER_RAW_SIZE (regnum))

/*
 * Fetch inferiors registers for gdb.
 * REG specifies which (as gdb views it) register, -1 for all.
 */
void
gnu_fetch_registers (int reg)
{
  thread_state_t state;
  struct proc *thread = inf_tid_to_thread (current_inferior, inferior_pid);
  
  if (!thread)
    error ("fetch inferior registers: %d: Invalid thread", inferior_pid);

  state = proc_get_state (thread, 0);

  if (! state)
    warning ("Couldn't fetch register %s.", reg_names[reg]);
  else if (reg >= 0)
    {
      proc_debug (thread, "fetching register: %s", reg_names[reg]);
      supply_register (reg, REG_ADDR(state, reg));
      thread->fetched_regs |= (1 << reg);
    }
  else
    {
      proc_debug (thread, "fetching all registers");
      for (reg = 0; reg < NUM_REGS; reg++) 
	supply_register (reg, REG_ADDR(state, reg));
      thread->fetched_regs = ~0;
    }
}

/* Store our register values back into the inferior.
 * If REG is -1, do this for all registers.
 * Otherwise, REG specifies which register
 *
 * On mach3 all registers are always saved in one call.
 */
void
gnu_store_registers (reg)
     int reg;
{
  int was_aborted, was_valid;
  thread_state_t state;
  thread_state_data_t old_state;
  struct proc *thread = inf_tid_to_thread (current_inferior, inferior_pid);

  if (! thread)
    error ("store inferior registers: %d: Invalid thread", inferior_pid);

  proc_debug (thread, "storing register %s.", reg_names[reg]);

  was_aborted = thread->aborted;
  was_valid = thread->state_valid;
  if (! was_aborted && was_valid)
    bcopy (&thread->state, &old_state, sizeof (old_state));

  state = proc_get_state (thread, 1);

  if (! state)
    warning ("Couldn't store register %s.", reg_names[reg]);
  else
    {
      if (! was_aborted && was_valid)
	/* See which registers have changed after aborting the thread.  */
	{
	  int check_reg;
	  for (check_reg = 0; check_reg < NUM_REGS; check_reg++)
	    if ((thread->fetched_regs & (1 << check_reg))
		&& bcmp (REG_ADDR (&old_state, check_reg),
			 REG_ADDR (state, check_reg),
			 REGISTER_RAW_SIZE (check_reg)))
	      /* Register CHECK_REG has changed!  Ack!  */
	      {
		warning ("Register %s changed after thread was aborted.",
			 reg_names [check_reg]);
		if (reg >= 0 && reg != check_reg)
		  /* Update gdb's copy of the register.  */
		  supply_register (check_reg, REG_ADDR (state, check_reg));
		else
		  warning ("... also writing this register!  Suspicious...");
	      }
	}

      if (reg >= 0)
	{
	  proc_debug (thread, "storing register: %s", reg_names[reg]);
	  STORE_REGS (state, reg, 1);
	}
      else
	{
	  proc_debug (thread, "storing all registers");
	  for (reg = 0; reg < NUM_REGS; reg++) 
	    STORE_REGS (state, reg, 1);
	}
    }
}

/* jtv@hut.fi: I copied and modified this 387 code from
 * gdb/i386-xdep.c. Modifications for Mach 3.0.
 *
 * i387 status dumper. See also i387-tdep.c
 */
struct env387 
{
  unsigned short control;
  unsigned short r0;
  unsigned short status;
  unsigned short r1;
  unsigned short tag;
  unsigned short r2;
  unsigned long eip;
  unsigned short code_seg;
  unsigned short opcode;
  unsigned long operand;
  unsigned short operand_seg;
  unsigned short r3;
  unsigned char regs[8][10];
};
/* This routine is machine independent?
 * Should move it to i387-tdep.c but you need to export struct env387
 */
static
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  unsigned char *p;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("u: ");
      print_387_status_word (status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("e: ");
      print_387_status_word (ep->status);
    }
  
  print_387_control_word (ep->control);
  printf_unfiltered ("last exception: ");
  printf_unfiltered ("opcode %s; ", local_hex_string(ep->opcode));
  printf_unfiltered ("pc %s:", local_hex_string(ep->code_seg));
  printf_unfiltered ("%s; ", local_hex_string(ep->eip));
  printf_unfiltered ("operand %s", local_hex_string(ep->operand_seg));
  printf_unfiltered (":%s\n", local_hex_string(ep->operand));
  
  top = (ep->status >> 11) & 7;
  
  printf_unfiltered ("regno  tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      double val;
      
      printf_unfiltered ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg);
      
      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf_unfiltered ("valid "); break;
	case 1: printf_unfiltered ("zero  "); break;
	case 2: printf_unfiltered ("trap  "); break;
	case 3: printf_unfiltered ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf_unfiltered ("%02x", ep->regs[fpreg][i]);
      
      floatformat_to_double (&floatformat_i387_ext, (char *)ep->regs[fpreg],
			       &val);
      printf_unfiltered ("  %g\n", val);
    }
  if (ep->r0)
    printf_unfiltered ("warning: reserved0 is %s\n", local_hex_string(ep->r0));
  if (ep->r1)
    printf_unfiltered ("warning: reserved1 is %s\n", local_hex_string(ep->r1));
  if (ep->r2)
    printf_unfiltered ("warning: reserved2 is %s\n", local_hex_string(ep->r2));
  if (ep->r3)
    printf_unfiltered ("warning: reserved3 is %s\n", local_hex_string(ep->r3));
}
	
/*
 * values that go into fp_kind (from <i386/fpreg.h>)
 */
#define FP_NO   0       /* no fp chip, no emulator (no fp support)      */
#define FP_SW   1       /* no fp chip, using software emulator          */
#define FP_HW   2       /* chip present bit                             */
#define FP_287  2       /* 80287 chip present                           */
#define FP_387  3       /* 80387 chip present                           */

typedef struct fpstate {
#if 1
  unsigned char	state[FP_STATE_BYTES]; /* "hardware" state */
#else
  struct env387	state;	/* Actually this */
#endif
  int status;		/* Duplicate status */
} *fpstate_t;

/* Mach 3 specific routines.
 */
static int
get_i387_state (fstate)
     struct fpstate *fstate;
{
  error_t err;
  thread_state_data_t state;
  unsigned int fsCnt = i386_FLOAT_STATE_COUNT;
  struct i386_float_state *fsp;
  struct proc *thread = inf_tid_to_thread (current_inferior, inferior_pid);
  
  if (!thread)
    error ("get_i387_state: Invalid thread");

  proc_abort (thread, 0);	/* Make sure THREAD's in a reasonable state. */

  err = thread_get_state (thread->port, i386_FLOAT_STATE, state, &fsCnt);
  if (err)
    {
      warning ("Can not get live floating point state: %s",
	       mach_error_string (err));
      return 0;
    }

  fsp = (struct i386_float_state *)state;
  /* The 387 chip (also 486 counts) or a software emulator? */
  if (!fsp->initialized || (fsp->fpkind != FP_387 && fsp->fpkind != FP_SW))
    return 0;

  /* Clear the target then copy thread's float state there.
     Make a copy of the status word, for some reason?
   */
  memset (fstate, 0, sizeof (struct fpstate));

  fstate->status = fsp->exc_status;

  memcpy (fstate->state, (char *)&fsp->hw_state, FP_STATE_BYTES);

  return 1;
}

/*
 * This is called by "info float" command
 */
void
i386_mach3_float_info()
{
  char buf [sizeof (struct fpstate) + 2 * sizeof (int)];
  int valid = 0;
  fpstate_t fps;
  
  if (target_has_execution)
    valid = get_i387_state (buf);

  if (!valid) 
    {
      warning ("no floating point status saved");
      return;
    }
  
  fps = (fpstate_t) buf;

  print_387_status (fps->status, (struct env387 *)fps->state);
}
