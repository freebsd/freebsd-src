/* BFD back-end for Motorola m88k a.out (Mach 3) binaries.
   Copyright (C) 1990, 1991, 1993, 1994 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

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

#define	TARGET_PAGE_SIZE	(4096*2)
#define SEGMENT_SIZE	0x20000
#define TEXT_START_ADDR	0
#define BYTES_IN_WORD	4
#define N_HEADER_IN_TEXT(x)	1 		/* (N_MAGIG(x) == ZMAGIC) */
#define N_SHARED_LIB(x) 0

#define N_TXTSIZE(x)	((x).a_text)

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

#define DEFAULT_ARCH bfd_arch_m88k
#define MY(OP) CAT(m88kmach3_,OP)
#define TARGETNAME "a.out-m88k-mach3"

#include "aout-target.h"
