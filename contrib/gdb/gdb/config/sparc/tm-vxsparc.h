/* Target machine description for VxWorks sparc's, for GDB, the GNU debugger.
   Copyright 1993, 1999 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "sparc/tm-spc-em.h"
#include "tm-vxworks.h"

/* FIXME: These are almost certainly wrong. */

/* Number of registers in a ptrace_getregs call. */

#define VX_NUM_REGS (NUM_REGS)

/* Number of registers in a ptrace_getfpregs call. */

/* #define VX_SIZE_FPREGS (don't know how many) */
