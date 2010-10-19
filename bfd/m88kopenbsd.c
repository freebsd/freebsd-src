/* BFD back-end for OpenBSD/m88k a.out binaries.
   Copyright 2004 Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define	TARGET_IS_BIG_ENDIAN_P

#define	TARGET_PAGE_SIZE	4096

#define	DEFAULT_ARCH		bfd_arch_m88k
#define	DEFAULT_MID		M_88K_OPENBSD

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (m88kopenbsd_,OP)
#define TARGETNAME "a.out-m88k-openbsd"

#include "netbsd.h"
