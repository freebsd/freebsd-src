/* Native support for i386 aix ps/2.
   Copyright 1986, 1987, 1989, 1992, 1993 Free Software Foundation, Inc.

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

/*
 * Changes for IBM AIX PS/2 by Minh Tran-Le (tranle@intellicorp.com)
 * Revision:	 5-May-93 00:11:35
 */

#ifndef NM_I386AIX_H
#define NM_I386AIX_H 1

/* code to execute to print interesting information about the
 * floating point processor (if any)
 * No need to define if there is nothing to do.
 */
#define FLOAT_INFO { i386_float_info (); }

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */
#undef  KERNEL_U_ADDR
#define KERNEL_U_ADDR 0xf03fd000

/* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */
#define FETCH_INFERIOR_REGISTERS

#endif /* NM_I386AIX_H */
