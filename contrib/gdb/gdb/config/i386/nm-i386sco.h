/* Native support for i386.
   Copyright 1986, 1987, 1989, 1992 Free Software Foundation, Inc.
   Changes for 80386 by Pace Willisson (pace@prep.ai.mit.edu), July 1988.

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

#if 0
/* code to execute to print interesting information about the
   floating point processor (if any)
   No need to define if there is nothing to do.
   On the 386, unfortunately this code is host-dependent (and lives
   in the i386-xdep.c file), so we can't
   do this unless we *know* we aren't cross-debugging.  FIXME.
 */
#define FLOAT_INFO { i386_float_info (); }
#endif /*0*/

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = i386_register_u_addr ((blockend),(regno));

extern int
i386_register_u_addr PARAMS ((int, int));

/* When calling functions on SCO, sometimes we get an error writing some
   of the segment registers.  This would appear to be a kernel
   bug/non-feature.  */
#define CANNOT_STORE_REGISTER(regno) ((regno) == 14 || (regno) == 15)
