/* Low level interface to I386 running mach 3.0.
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

/* Hmmm... Should this not be here?
 * Now for i386_float_info() target_has_execution
 */
#include <target.h>

/* This mess is duplicated in bfd/i386mach3.h
 *
 * This is an ugly way to hack around the incorrect
 * definition of UPAGES in i386/machparam.h.
 *
 * The definition should specify the size reserved
 * for "struct user" in core files in PAGES,
 * but instead it gives it in 512-byte core-clicks
 * for i386 and i860.
 */
#include <sys/param.h>
#if UPAGES == 16
#define UAREA_SIZE ctob(UPAGES)
#elif UPAGES == 2
#define UAREA_SIZE (NBPG*UPAGES)
#else
FIXME!! UPAGES is neither 2 nor 16
#endif

/* @@@ Should move print_387_status() to i387-tdep.c */
extern void print_387_control_word ();		/* i387-tdep.h */
extern void print_387_status_word ();

#define private static


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

#define REG_ADDRESS(state,regnum) ((char *)(state)+reg_offset[regnum])

/* Fetch COUNT contiguous registers from thread STATE starting from REGNUM
 * Caller knows that the regs handled in one transaction are of same size.
 */
#define FETCH_REGS(state, regnum, count) \
  memcpy (&registers[REGISTER_BYTE (regnum)], \
	  REG_ADDRESS (state, regnum), \
	  count*REGISTER_SIZE)

/* Store COUNT contiguous registers to thread STATE starting from REGNUM */
#define STORE_REGS(state, regnum, count) \
  memcpy (REG_ADDRESS (state, regnum), \
	  &registers[REGISTER_BYTE (regnum)], \
	  count*REGISTER_SIZE)

/*
 * Fetch inferiors registers for gdb.
 * REGNO specifies which (as gdb views it) register, -1 for all.
 */

void
fetch_inferior_registers (regno)
     int regno;
{
  kern_return_t ret;
  thread_state_data_t state;
  unsigned int stateCnt = i386_THREAD_STATE_COUNT;
  int index;
  
  if (! MACH_PORT_VALID (current_thread))
    error ("fetch inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  ret = thread_get_state (current_thread,
			  i386_THREAD_STATE,
			  state,
			  &stateCnt);

  if (ret != KERN_SUCCESS)
    warning ("fetch_inferior_registers: %s ",
	     mach_error_string (ret));
#if 0
  /* It may be more effective to store validate all of them,
   * since we fetched them all anyway
   */
  else if (regno != -1)
    supply_register (regno, (char *)state+reg_offset[regno]);
#endif
  else
    {
      for (index = 0; index < NUM_REGS; index++) 
	supply_register (index, (char *)state+reg_offset[index]);
    }

  if (must_suspend_thread)
    setup_thread (current_thread, 0);
}

/* Store our register values back into the inferior.
 * If REGNO is -1, do this for all registers.
 * Otherwise, REGNO specifies which register
 *
 * On mach3 all registers are always saved in one call.
 */
void
store_inferior_registers (regno)
     int regno;
{
  kern_return_t ret;
  thread_state_data_t state;
  unsigned int stateCnt = i386_THREAD_STATE_COUNT;
  register int index;

  if (! MACH_PORT_VALID (current_thread))
    error ("store inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  /* Fetch the state of the current thread */
  ret = thread_get_state (current_thread,
			  i386_THREAD_STATE,
			  state,
			  &stateCnt);

   if (ret != KERN_SUCCESS) 
    {
      warning ("store_inferior_registers (get): %s",
	       mach_error_string (ret));
      if (must_suspend_thread)
	setup_thread (current_thread, 0);
      return;
    }

  /* move gdb's registers to thread's state
   *
   * Since we save all registers anyway, save the ones
   * that gdb thinks are valid (e.g. ignore the regno
   * parameter)
   */
#if 0
  if (regno != -1)
    STORE_REGS (state, regno, 1);
  else
#endif
    {
      for (index = 0; index < NUM_REGS; index++) 
	STORE_REGS (state, index, 1);
    }
  
  /* Write gdb's current view of register to the thread
   */
  ret = thread_set_state (current_thread,
			  i386_THREAD_STATE,
			  state,
			  i386_THREAD_STATE_COUNT);
  
  if (ret != KERN_SUCCESS)
    warning ("store_inferior_registers (set): %s",
	     mach_error_string (ret));

  if (must_suspend_thread)
    setup_thread (current_thread, 0);
}



/* Return the address in the core dump or inferior of register REGNO.
 * BLOCKEND should be the address of the end of the UPAGES area read
 * in memory, but it's not?
 *
 * Currently our UX server dumps the whole thread state to the
 * core file. If your UX does something else, adapt the routine
 * below to return the offset to the given register.
 * 
 * Called by core-aout.c(fetch_core_registers)
 */

unsigned int
register_addr (regno, blockend)
     int regno;
     int blockend;
{
  unsigned int addr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Invalid register number %d.", regno);

  /* UAREA_SIZE == 8 kB in i386 */
  addr = (unsigned int)REG_ADDRESS (UAREA_SIZE - sizeof(struct i386_thread_state), regno);

  return addr;
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
private
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
private boolean_t
get_i387_state (fstate)
     struct fpstate *fstate;
{
  kern_return_t ret;
  thread_state_data_t state;
  unsigned int fsCnt = i386_FLOAT_STATE_COUNT;
  struct i386_float_state *fsp;
  
  ret = thread_get_state (current_thread,
			  i386_FLOAT_STATE,
			  state,
			  &fsCnt);

  if (ret != KERN_SUCCESS)
    {
      warning ("Can not get live floating point state: %s",
	       mach_error_string (ret));
      return FALSE;
    }

  fsp = (struct i386_float_state *)state;
  /* The 387 chip (also 486 counts) or a software emulator? */
  if (!fsp->initialized || (fsp->fpkind != FP_387 && fsp->fpkind != FP_SW))
    return FALSE;

  /* Clear the target then copy thread's float state there.
     Make a copy of the status word, for some reason?
   */
  memset (fstate, 0, sizeof (struct fpstate));

  fstate->status = fsp->exc_status;

  memcpy (fstate->state, (char *)&fsp->hw_state, FP_STATE_BYTES);

  return TRUE;
}

private boolean_t
get_i387_core_state (fstate)
     struct fpstate *fstate;
{
  /* Not implemented yet. Core files do not contain float state. */
  return FALSE;
}

/*
 * This is called by "info float" command
 */
void
i386_mach3_float_info()
{
  char buf [sizeof (struct fpstate) + 2 * sizeof (int)];
  boolean_t valid = FALSE;
  fpstate_t fps;
  
  if (target_has_execution)
    valid = get_i387_state (buf);
#if 0  
  else if (WE HAVE CORE FILE)  /* @@@@ Core files not supported */
    valid = get_i387_core_state (buf);
#endif    

  if (!valid) 
    {
      warning ("no floating point status saved");
      return;
    }
  
  fps = (fpstate_t) buf;

  print_387_status (fps->status, (struct env387 *)fps->state);
}
