/* Low level interface to ns532 running mach 3.0.
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

#include <stdio.h>

#include <mach.h>
#include <mach/message.h>
#include <mach/exception.h>
#include <mach_error.h>

#define private static


/* Find offsets to thread states at compile time.
 * If your compiler does not grok this, calculate offsets
 * offsets yourself and use them (or get a compatible compiler :-)
 */

#define  REG_N_OFFSET(reg) (int)(&((struct ns532_combined_state *)0)->ts.reg)
#define  REG_F_OFFSET(reg) (int)(&((struct ns532_combined_state *)0)->fs.reg)

/* at reg_offset[i] is the offset to the ns532_combined_state
 * location where the gdb registers[i] is stored.
 */

static int reg_offset[] = 
{
  REG_N_OFFSET(r0),  REG_N_OFFSET(r1), REG_N_OFFSET(r2), REG_N_OFFSET(r3),
  REG_N_OFFSET(r4),  REG_N_OFFSET(r5), REG_N_OFFSET(r6), REG_N_OFFSET(r7),
  REG_F_OFFSET(l0a), REG_F_OFFSET(l1a),REG_F_OFFSET(l2a),REG_F_OFFSET(l3a),
  REG_F_OFFSET(l4a), REG_F_OFFSET(l5a),REG_F_OFFSET(l6a),REG_F_OFFSET(l7a),
  REG_N_OFFSET(sp),  REG_N_OFFSET(fp), REG_N_OFFSET(pc), REG_N_OFFSET(psr),
  REG_F_OFFSET(fsr),
  REG_F_OFFSET(l0a), REG_F_OFFSET(l2a),REG_F_OFFSET(l4a),REG_F_OFFSET(l6a)
  /* @@@ 532 has more double length floating point regs, not accessed currently */
};

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
  unsigned int stateCnt = NS532_COMBINED_STATE_COUNT;
  int index;
  
  if (! MACH_PORT_VALID (current_thread))
    error ("fetch inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  ret = thread_get_state (current_thread,
			  NS532_COMBINED_STATE,
			  state,
			  &stateCnt);

  if (ret != KERN_SUCCESS)
    message ("fetch_inferior_registers: %s ",
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
  unsigned int stateCnt = NS532_COMBINED_STATE_COUNT;
  register int index;

  if (! MACH_PORT_VALID (current_thread))
    error ("store inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  /* Fetch the state of the current thread */
  ret = thread_get_state (current_thread,
			  NS532_COMBINED_STATE,
			  state,
			  &stateCnt);

   if (ret != KERN_SUCCESS) 
    {
      message ("store_inferior_registers (get): %s",
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
			  NS532_COMBINED_STATE,
			  state,
			  NS532_COMBINED_STATE_COUNT);
  
  if (ret != KERN_SUCCESS)
    message ("store_inferior_registers (set): %s",
	     mach_error_string (ret));

  if (must_suspend_thread)
    setup_thread (current_thread, 0);
}
