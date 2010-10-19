/* BFD back-end for ARM EPOC PE files.
   Copyright 1999, 2000 Free Software Foundation, Inc.

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

#define TARGET_UNDERSCORE    0
#define USER_LABEL_PREFIX    ""

#define TARGET_LITTLE_SYM    arm_epoc_pe_little_vec
#define TARGET_LITTLE_NAME   "epoc-pe-arm-little"
#define TARGET_BIG_SYM       arm_epoc_pe_big_vec
#define TARGET_BIG_NAME      "epoc-pe-arm-big"

#define bfd_arm_allocate_interworking_sections \
	bfd_arm_epoc_pe_allocate_interworking_sections
#define bfd_arm_get_bfd_for_interworking \
	bfd_arm_epoc_pe_get_bfd_for_interworking
#define bfd_arm_process_before_allocation \
	bfd_arm_epoc_pe_process_before_allocation

#define EXTRA_S_FLAGS (SEC_LINK_ONCE | SEC_LINK_DUPLICATES | SEC_CODE | SEC_READONLY | SEC_DATA)

#include "pe-arm.c"
