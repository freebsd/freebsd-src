/* Macro definitions for i386, Mach 3.0, OSF 1/MK
   Copyright 1992, 1993, 2000 Free Software Foundation, Inc.

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

/* Until OSF switches to a newer Mach kernel that has
 * a different get_emul_vector() interface.
 */
#define MK67 1

#include "i386/tm-i386m3.h"

/* FIMXE: kettenis/2000-03-26: On OSF 1, `long double' is equivalent
   to `double'.  However, I'm not sure what is the consequence of:

   #define TARGET_LONG_DOUBLE_FORMAT TARGET_DOUBLE_FORMAT
   #define TARGET_LONG_DOUBLE_BIT TARGET_DOUBLE_BIT

   So I'll go with the current status quo instead.  It looks like this
   target won't compile anyway.  Perhaps it should be obsoleted?  */
   
#undef TARGET_LONG_DOUBLE_FORMAT
#undef TARGET_LONG_DOUBLE_BIT
