/* Macro defintions for IBM AIX PS/2 (i386).
   Copyright 1986, 1987, 1989, 1992, 1993, 1994, 1995, 2000
   Free Software Foundation, Inc.

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

/* Changes for IBM AIX PS/2 by Minh Tran-Le (tranle@intellicorp.com).  */

#ifndef TM_I386AIX_H
#define TM_I386AIX_H 1

#include "i386/tm-i386.h"
#include <sys/reg.h>

#ifndef I386
#define I386 1
#endif

/* AIX/i386 has FPU support.  However, the native configuration (which
   is the only supported configuration) doesn't make the FPU control
   registers available.  Override the appropriate symbols such that
   only the normal FPU registers are included in GDB's register array.  */

#undef NUM_FPREGS
#define NUM_FPREGS (8)

#undef NUM_REGS
#define NUM_REGS (NUM_GREGS + NUM_FPREGS)

#undef REGISTER_BYTES
#define REGISTER_BYTES (SIZEOF_GREGS + SIZEOF_FPU_REGS)

#endif /* TM_I386AIX_H */
