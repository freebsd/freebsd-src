/* Target machine parameters for MIPS r4000
   Copyright 1994 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor (ian@cygnus.com)

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

#define GDB_TARGET_IS_MIPS64 1

/* force LONGEST to be long long in gdb */
#define FORCE_LONG_LONG

/* Use eight byte registers.  */
#define MIPS_REGSIZE 8

/* define 8 byte register type */
#define REGISTER_VIRTUAL_TYPE(N) \
        (((N) >= FP0_REGNUM && (N) < FP0_REGNUM+32)  \
         ? builtin_type_double : builtin_type_long_long) \

/* Load double words in CALL_DUMMY.  */
#define OP_LDFPR 065	/* ldc1 */
#define OP_LDGPR 067	/* ld */

/* Get the basic MIPS definitions.  */
#include "tm-mips.h"
