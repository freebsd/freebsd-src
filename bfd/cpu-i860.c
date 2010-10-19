/* BFD support for the Intel 860 architecture.
   Copyright 1992, 1995, 2000, 2002 Free Software Foundation, Inc.
   Created mostly by substituting "860" for "386" in cpu-i386.c
   Harry Dolan <dolan@ssd.intel.com>, October 1995

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

const bfd_arch_info_type bfd_i860_arch =
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_i860,		/* Architecture */
    0,				/* Only one machine */
    "i860",			/* Architecture name */
    "i860",			/* Printable name */
    3,				/* Section alignment exponent */
    TRUE,			/* Is this the default architecture? */
    bfd_default_compatible,
    bfd_default_scan,
    0,				/* Next in list */
  };
