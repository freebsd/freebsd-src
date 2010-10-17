/* TI COFF information for Texas Instruments TMS320C54X.
   This file customizes the settings in coff/ti.h. 
   
   Copyright 2001 Free Software Foundation, Inc.

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

#ifndef COFF_TIC54X_H
#define COFF_TIC54X_H

#define TIC54X_TARGET_ID 0x98
#define TIC54XALGMAGIC  0x009B  /* c54x algebraic assembler output */
#define TIC5X_TARGET_ID  0x92
#define TI_TARGET_ID TIC54X_TARGET_ID
#define OCTETS_PER_BYTE_POWER 1 /* octets per byte, as a power of two */
#define HOWTO_BANK 6 /* add to howto to get absolute/sect-relative version */
#define TICOFF_TARGET_ARCH bfd_arch_tic54x
#define TICOFF_DEFAULT_MAGIC TICOFF1MAGIC /* we use COFF1 for compatibility */

/* Page macros

   The first GDB port requires flags in its remote memory access commands to
   distinguish between data/prog space.  Hopefully we can make this go away
   eventually.  Stuff the page in the upper bits of a 32-bit address, since
   the c5x family only uses 16 or 23 bits.

   c2x, c5x and most c54x devices have 16-bit addresses, but the c548 has
   23-bit program addresses.  Make sure the page flags don't interfere.
   These flags are used by GDB to identify the destination page for
   addresses. 
*/

/* Recognized load pages (by common convention).  */
#define PG_PROG         0x0         /* PROG page */
#define PG_DATA         0x1         /* DATA page */
#define PG_IO           0x2         /* I/O page */

/** Indicate whether the given storage class requires a page flag.  */
#define NEEDS_PAGE(X) ((X)==C_EXT)
#define PAGE_MASK       0xFF000000
#define ADDR_MASK       0x00FFFFFF
#define PG_TO_FLAG(p)   (((unsigned long)(p) & 0xFF) << 24)
#define FLAG_TO_PG(f)   (((f) >> 24) & 0xFF)

#include "coff/ti.h"

#endif /* COFF_TIC54X_H */
