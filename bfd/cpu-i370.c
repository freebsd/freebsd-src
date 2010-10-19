/* BFD i370 CPU definition
   Copyright 1994, 1995, 1996, 1998, 1999, 2000, 2002, 2005
   Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor, Cygnus Support.
   Hacked by Linas Vepstas <linas@linas.org> in 1998, 1999

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

static const bfd_arch_info_type arch_info_struct[] =
{
  /* Hack alert: old old machines are really 16 and 24 bit arch ...  */
  {
    32, 	/* 32 bits in a word.  */
    32, 	/* 32 bits in an address.  */
    8,  	/* 8 bits in a byte.  */
    bfd_arch_i370,
    360, 	/* For the 360.  */
    "i370",
    "i370:360",
    3,
    FALSE, 	/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    &arch_info_struct[1]
  },
  {
    32, 	/* 32 bits in a word.  */
    32, 	/* 32 bits in an address.  */
    8,  	/* 8 bits in a byte.  */
    bfd_arch_i370,
    370, 	/* For the 370.  */
    "i370",
    "i370:370",
    3,
    FALSE, 	/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    0
  },
};

const bfd_arch_info_type bfd_i370_arch =
{
  32, 		/* 32 bits in a word.  */
  32, 		/* 32 bits in an address.  */
  8,  		/* 8 bits in a byte.  */
  bfd_arch_i370,
  0,		/* For the 360/370 common architecture.  */
  "i370",
  "i370:common",
  3,
  TRUE, 	/* The default.  */
  bfd_default_compatible,
  bfd_default_scan,
  & arch_info_struct[0]
};
