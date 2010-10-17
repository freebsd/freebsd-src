/* BFD back-end for PowerPC PE IMAGE COFF files.
   Copyright 1995, 1996, 1999 Free Software Foundation, Inc.

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
Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"

/* setting up for a PE environment stolen directly from the i386 structure */
#define E_FILNMLEN	18	/* # characters in a file name		*/

#define PPC_PE

#define TARGET_LITTLE_SYM   bfd_powerpcle_pei_vec
#define TARGET_LITTLE_NAME "pei-powerpcle"

#define TARGET_BIG_SYM      bfd_powerpc_pei_vec
#define TARGET_BIG_NAME    "pei-powerpc"

#define COFF_IMAGE_WITH_PE
#define COFF_WITH_PE

#define COFF_LONG_SECTION_NAMES

/* FIXME: Verify PCRELOFFSET is always false */

/* FIXME: This target no longer works.  Search for POWERPC_LE_PE in
   coff-ppc.c and peigen.c.  */

#include "coff-ppc.c"
