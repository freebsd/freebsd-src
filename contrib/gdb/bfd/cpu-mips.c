/* bfd back-end for mips support
   Copyright (C) 1990, 91, 92, 93, 94 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support.

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
#include "libbfd.h"

static const bfd_arch_info_type arch_info_struct[] = 
{
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_mips,
    6000,
    "mips",
    "mips:6000",
    3,
    false,
    bfd_default_compatible, 
    bfd_default_scan,
    &arch_info_struct[1],
  },
  {
    64, /* 64 bits in a word */
    64, /* 64 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_mips,
    4000,
    "mips",
    "mips:4000",
    3,
    false,
    bfd_default_compatible, 
    bfd_default_scan ,
    &arch_info_struct[2],
  },
  {
    64, /* 64 bits in a word */
    64, /* 64 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_mips,
    8000,
    "mips",
    "mips:8000",
    3,
    false,
    bfd_default_compatible, 
    bfd_default_scan ,
    0,
  }
};

const bfd_arch_info_type bfd_mips_arch =
{
  32,	/* 32 bits in a word */
  32,	/* 32 bits in an address */
  8,	/* 8 bits in a byte */
  bfd_arch_mips,
  3000,
  "mips",
  "mips:3000",
  3,
  true,
  bfd_default_compatible, 
  bfd_default_scan,
  &arch_info_struct[0],
};
