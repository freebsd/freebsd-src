/* Target macro definitions for i386 running FreeBSD
   Copyright (C) 1997 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "i386/tm-i386bsd.h"


#undef NUM_REGS
#define NUM_REGS 14


#undef IN_SOLIB_CALL_TRAMPOLINE
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) STREQ (name, "_DYNAMIC")


extern i386_float_info ();
#define FLOAT_INFO  i386_float_info ()
