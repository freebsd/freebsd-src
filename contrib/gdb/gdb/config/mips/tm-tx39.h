/* Copyright (C) 1993 Free Software Foundation, Inc.

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

#define TARGET_BYTE_ORDER_SELECTABLE_P 1
#define MIPS_EABI 1
#define MIPS_DEFAULT_FPU_TYPE MIPS_FPU_NONE

#include "mips/tm-bigmips.h"

#undef  REGISTER_NAMES
#define REGISTER_NAMES 	\
    {	"zero",	"at",	"v0",	"v1",	"a0",	"a1",	"a2",	"a3", \
	"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6",	"t7", \
	"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7", \
	"t8",	"t9",	"k0",	"k1",	"gp",	"sp",	"s8",	"ra", \
	"sr",	"lo",	"hi",	"bad",	"cause","pc", \
	"",   	"",   	"",   	"",   	"",   	"",   	"",   	"", \
	"",   	"",   	"",  	"",  	"",  	"",  	"",  	"", \
	"",  	"",  	"",  	"",  	"",  	"",  	"",  	"", \
	"",  	"",  	"",  	"",  	"",  	"",  	"",  	"", \
	"",  	"",  	"",	"", \
	"",	"",	"",	"",	"",	"",	"",	"", \
	"",	"", "config", "cache", "debug", "depc", "epc",	"" \
    }
