/* BFD back-end for ARM PECOFF files.
   Copyright 1995, 1996, 1999, 2000, 2001 Free Software Foundation, Inc.

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

#include "bfd.h"
#include "sysdep.h"

#ifndef TARGET_LITTLE_SYM
#define TARGET_LITTLE_SYM    armpe_little_vec
#define TARGET_LITTLE_NAME   "pe-arm-little"
#define TARGET_BIG_SYM       armpe_big_vec
#define TARGET_BIG_NAME      "pe-arm-big"
#endif

#define COFF_WITH_PE
#define PCRELOFFSET          true
#define COFF_LONG_SECTION_NAMES

#ifndef bfd_arm_allocate_interworking_sections
#define bfd_arm_allocate_interworking_sections \
	bfd_arm_pe_allocate_interworking_sections
#define bfd_arm_get_bfd_for_interworking \
	bfd_arm_pe_get_bfd_for_interworking
#define bfd_arm_process_before_allocation \
	bfd_arm_pe_process_before_allocation
#endif

#ifdef ARM_WINCE
#define TARGET_UNDERSCORE 0
#endif

#include "coff-arm.c"
