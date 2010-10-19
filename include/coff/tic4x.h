/* TI COFF information for Texas Instruments TMS320C4X/C3X.
   This file customizes the settings in coff/ti.h. 
   
   Copyright 2002, 2003 Free Software Foundation, Inc.

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

#ifndef COFF_TIC4X_H
#define COFF_TIC4X_H

#define TIC4X_TARGET_ID 	0x0093
/* Octets per byte, as a power of two.  */
#define TI_TARGET_ID 		TIC4X_TARGET_ID
#define OCTETS_PER_BYTE_POWER 	2 
/* Add to howto to get absolute/sect-relative version.  */
#define HOWTO_BANK 		6		
#define TICOFF_TARGET_ARCH 	bfd_arch_tic4x
/* We use COFF2.  */
#define TICOFF_DEFAULT_MAGIC 	TICOFF2MAGIC 

#define TICOFF_TARGET_MACHINE_GET(FLAGS) \
  (((FLAGS) & F_VERS) ? bfd_mach_tic4x : bfd_mach_tic3x)

#define TICOFF_TARGET_MACHINE_SET(FLAGSP, MACHINE)	\
  do							\
    {							\
      if ((MACHINE) == bfd_mach_tic4x)			\
        *(FLAGSP) |= F_VERS;				\
    }							\
  while (0)

#include "coff/ti.h"

#endif /* COFF_TIC4X_H */
