/* BFD support for the HP Precision Architecture architecture.
   Copyright 1992, 1995, 1998, 1999, 2000, 2002, 2003
   Free Software Foundation, Inc.

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

static const bfd_arch_info_type bfd_hppa10_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_hppa,
  bfd_mach_hppa10,		/* By convention PA1.0 = 10 */
  "hppa",
  "hppa1.0",
  3,
  TRUE,				/* Unless we use 1.1 specific features */
  bfd_default_compatible,
  bfd_default_scan ,
  0,
};

/* PA2.0 in narrow mode */
static const bfd_arch_info_type bfd_hppa20_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_hppa,
  bfd_mach_hppa20,		/* By convention PA2.0 = 20 */
  "hppa",
  "hppa2.0",
  3,
  FALSE,			/* Unless we use 1.1 specific features */
  bfd_default_compatible,
  bfd_default_scan ,
  &bfd_hppa10_arch,
};

/* PA2.0 in wide mode */
static const bfd_arch_info_type bfd_hppa20w_arch =
{
  64,				/* 64 bits in a word */
  64,				/* 64 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_hppa,
  bfd_mach_hppa20w,		/* ??? How best to describe wide mode here?  */
  "hppa",
  "hppa2.0w",
  3,
  FALSE,			/* Unless we use 1.1 specific features */
  bfd_default_compatible,
  bfd_default_scan ,
  &bfd_hppa20_arch,
};

const bfd_arch_info_type bfd_hppa_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_hppa,
  bfd_mach_hppa11,		/* By convention PA1.1 = 11 */
  "hppa",
  "hppa1.1",
  3,
  FALSE,			/* 1.1 specific features used */
  bfd_default_compatible,
  bfd_default_scan ,
  &bfd_hppa20w_arch,
};
