/* BFD back-end for NetBSD/sparc a.out-ish binaries.
   Copyright (C) 1990, 91, 92, 94, 95, 97, 1998 Free Software Foundation, Inc.

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

#define BYTES_IN_WORD	4
#define TARGET_IS_BIG_ENDIAN_P

/* SPARC chips use either 4K or 8K pages, but object files always
   assume 8K page alignment so they will work on either one.  */
#define TARGET_PAGE_SIZE 0x2000

#define DEFAULT_ARCH	bfd_arch_sparc
#define DEFAULT_MID 	M_SPARC_NETBSD

#define MY(OP) CAT(sparcnetbsd_,OP)
/* This needs to start with a.out so GDB knows it is an a.out variant.  */
#define TARGETNAME "a.out-sparc-netbsd"

#include "netbsd.h"
