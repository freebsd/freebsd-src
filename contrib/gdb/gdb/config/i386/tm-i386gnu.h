/* Macro definitions for i386, GNU Hurd
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

/* Include common definitions for gnu systems */
#include "nm-gnu.h"

/* Thread flavors used in re-setting the T bit.
 * @@ this is also bad for cross debugging.
 */
#define THREAD_STATE_FLAVOR		i386_THREAD_STATE
#define THREAD_STATE_SIZE		i386_THREAD_STATE_COUNT
#define THREAD_STATE_SET_TRACED(state) \
  	((struct i386_thread_state *)state)->efl |= 0x100
#define THREAD_STATE_CLEAR_TRACED(state) \
  	((((struct i386_thread_state *)state)->efl &= ~0x100), 1)

/* we can do it */
#define ATTACH_DETACH 1

/* Sigh. There should be a file for i386 but no sysv stuff in it */
#include "i386/tm-i386v.h"

/* I want to test this float info code. See comment in tm-i386v.h */
#undef FLOAT_INFO
#define FLOAT_INFO { i386_mach3_float_info (); }

/* Address of end of stack space.
 * for MACH, see <machine/vmparam.h>
 * @@@ I don't know what is in the 5 ints...
 */
#undef  STACK_END_ADDR
#define STACK_END_ADDR (0xc0000000-sizeof(int [5]))
