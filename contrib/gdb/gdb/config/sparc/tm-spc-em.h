/* Target machine definitions for GDB for an embedded SPARC.
   Copyright 1989, 1992, 1993, 2000 Free Software Foundation, Inc.

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

#include "sparc/tm-sparc.h"

/* Offsets into jmp_buf.  Not defined by Sun, but at least documented in a
   comment in <machine/setjmp.h>! */

#define JB_ELEMENT_SIZE 4	/* Size of each element in jmp_buf */

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_NPC 4
#define JB_PSR 5
#define JB_G1 6
#define JB_O0 7
#define JB_WBCNT 8

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int get_longjmp_target (CORE_ADDR *);

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
