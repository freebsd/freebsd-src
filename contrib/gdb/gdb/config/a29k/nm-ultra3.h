/* Host definitions for GDB running on an a29k NYU Ultracomputer
   Copyright (C) 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.
   Contributed by David Wood (wood@lab.ultra.nyu.edu).

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

/* If we ever *do* end up using the standard fetch_inferior_registers,
   this is the right value for U_REGS_OFFSET.  */
#define	U_REGS_OFFSET	0

/* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */
#define FETCH_INFERIOR_REGISTERS
