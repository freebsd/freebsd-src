/* BFD PowerPC CPU definition
   Copyright (C) 1994, 1995, 1996 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor, Cygnus Support.

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

/* The common PowerPC architecture is compatible with the RS/6000.  */

static const bfd_arch_info_type *powerpc_compatible
  PARAMS ((const bfd_arch_info_type *, const bfd_arch_info_type *));

static const bfd_arch_info_type *
powerpc_compatible (a,b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  BFD_ASSERT (a->arch == bfd_arch_powerpc);
  switch (b->arch)
    {
    default:
      return NULL;
    case bfd_arch_powerpc:
      return bfd_default_compatible (a, b);
    case bfd_arch_rs6000:
      if (a->mach == 0)
	return a;
      return NULL;
    }
  /*NOTREACHED*/
}

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_powerpc,
    603, /* for the mpc603 */
    "powerpc",
    "powerpc:603",
    3,
    false, /* not the default */
    powerpc_compatible, 
    bfd_default_scan,
    &arch_info_struct[1]
  },
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_powerpc,
    604, /* for the mpc604 */
    "powerpc",
    "powerpc:604",
    3,
    false, /* not the default */
    powerpc_compatible, 
    bfd_default_scan,
    &arch_info_struct[2]
  },
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_powerpc,
    403, /* for the 403 */
    "powerpc",
    "powerpc:403",
    3,
    false, /* not the default */
    powerpc_compatible, 
    bfd_default_scan,
    &arch_info_struct[3]
  },
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_powerpc,
    601, /* for the mpc601 */
    "powerpc",
    "powerpc:601",
    3,
    false, /* not the default */
    powerpc_compatible, 
    bfd_default_scan,
    0
  }
};

const bfd_arch_info_type bfd_powerpc_arch =
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_powerpc,
    0, /* for the POWER/PowerPC common architecture */
    "powerpc",
    "powerpc:common",
    3,
    true, /* the default */
    powerpc_compatible, 
    bfd_default_scan,
    &arch_info_struct[0]
  };
