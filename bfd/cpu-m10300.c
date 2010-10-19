/* BFD support for the Matsushita 10300 processor
   Copyright 1996, 1997, 1999, 2000, 2002, 2003
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

const bfd_arch_info_type bfd_am33_2_arch =
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_mn10300,
    332,
    "am33_2",
    "am33-2",
    2,
    FALSE,
    bfd_default_compatible,
    bfd_default_scan,
    0,
  };

const bfd_arch_info_type bfd_am33_arch =
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_mn10300,
    330,
    "am33",
    "am33",
    2,
    FALSE,
    bfd_default_compatible,
    bfd_default_scan,
    &bfd_am33_2_arch,
  };

const bfd_arch_info_type bfd_mn10300_arch =
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_mn10300,
    300,
    "mn10300",
    "mn10300",
    2,
    TRUE, /* the one and only */
    bfd_default_compatible,
    bfd_default_scan,
    &bfd_am33_arch,
  };
