/* Low level interface to HP800 running mach 4.0.
   Copyright (C) 1995 Free Software Foundation, Inc.

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

#include <target.h>

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
  unsigned int stateCnt = TRACE_FLAVOR_SIZE;
  int index;
  
  if (! MACH_PORT_VALID (current_thread))
    error ("fetch inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  ret = thread_get_state (current_thread,
			  TRACE_FLAVOR,
			  state,
			  &stateCnt);

  if (ret != KERN_SUCCESS)
    warning ("fetch_inferior_registers: %s ",
	     mach_error_string (ret));
  else
    {
      for (index = 0; index < NUM_REGS; index++) 
	supply_register (index,(void*)&state[index]);
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
  unsigned int stateCnt = TRACE_FLAVOR_SIZE;
  register int index;

  if (! MACH_PORT_VALID (current_thread))
    error ("store inferior registers: Invalid thread");

  if (must_suspend_thread)
    setup_thread (current_thread, 1);

  /* Fetch the state of the current thread */
  ret = thread_get_state (current_thread,
			  TRACE_FLAVOR,
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
  if (regno > 0 && regno < NUM_REGS ) 
    {
      memcpy(&state[regno], &registers[REGISTER_BYTE (regno)], 
	     REGISTER_RAW_SIZE(regno));
    }
  else
    {
      for (index = 0; index < NUM_REGS; index++) 
	memcpy(&state[index], &registers[REGISTER_BYTE (index)], 
	       REGISTER_RAW_SIZE(index));
/* 	state[index] = registers[REGISTER_BYTE (index)];*/

    }
  
  /* Write gdb's current view of register to the thread
   */
  ret = thread_set_state (current_thread,
			  TRACE_FLAVOR,
			  state,
			  TRACE_FLAVOR_SIZE);
  
  if (ret != KERN_SUCCESS)
    warning ("store_inferior_registers (set): %s",
	     mach_error_string (ret));

  if (must_suspend_thread)
    setup_thread (current_thread, 0);
}
