/* BFD support for the SPARC architecture.
   Copyright (C) 1992, 1995, 1996 Free Software Foundation, Inc.

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

/* Don't mix 32 bit and 64 bit files.  */

static const bfd_arch_info_type *
sparc_compatible (a, b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  if (a->bits_per_word != b->bits_per_word)
    return NULL;

  return bfd_default_compatible (a, b);
}

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,	/* bits in a word */
    32,	/* bits in an address */
    8,	/* bits in a byte */
    bfd_arch_sparc,
    bfd_mach_sparc_v8plus,
    "sparc",
    "sparc:v8plus",
    3,
    false,
    sparc_compatible, 
    bfd_default_scan,
    &arch_info_struct[1],
  },
  {
    32,	/* bits in a word */
    32,	/* bits in an address */
    8,	/* bits in a byte */
    bfd_arch_sparc,
    bfd_mach_sparc_v8plusa,
    "sparc",
    "sparc:v8plusa",
    3,
    false,
    sparc_compatible, 
    bfd_default_scan,
    &arch_info_struct[2],
  },
  {
    64,	/* bits in a word */
    64,	/* bits in an address */
    8,	/* bits in a byte */
    bfd_arch_sparc,
    bfd_mach_sparc_v9,
    "sparc",
    "sparc:v9",
    3,
    false,
    sparc_compatible, 
    bfd_default_scan,
    &arch_info_struct[3],
  },
  {
    64,	/* bits in a word */
    64,	/* bits in an address */
    8,	/* bits in a byte */
    bfd_arch_sparc,
    bfd_mach_sparc_v9a,
    "sparc",
    "sparc:v9a",
    3,
    false,
    sparc_compatible, 
    bfd_default_scan,
    0,
  }
};

const bfd_arch_info_type bfd_sparc_arch =
  {
    32,	/* bits in a word */
    32,	/* bits in an address */
    8,	/* bits in a byte */
    bfd_arch_sparc,
    bfd_mach_sparc,
    "sparc",
    "sparc",
    3,
    true, /* the default */
    sparc_compatible, 
    bfd_default_scan,
    &arch_info_struct[0],
  };
