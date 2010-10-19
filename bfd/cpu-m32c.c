/* BFD support for the M16C/M32C processors.
   Copyright (C) 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,				/* bits per word */
    32,				/* bits per address */
    8,				/* bits per byte */
    bfd_arch_m32c,		/* architecture */
    bfd_mach_m32c,		/* machine */
    "m32c",			/* architecture name */
    "m32c",			/* printable name */
    3,				/* section align power */
    FALSE,			/* the default ? */
    bfd_default_compatible,	/* architecture comparison fn */
    bfd_default_scan,		/* string to architecture convert fn */
    NULL			/* next in list */
  },
};

const bfd_arch_info_type bfd_m32c_arch =
{
  32,				/* Bits per word.  */
  32,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_m32c,		/* Architecture.  */
  bfd_mach_m16c,		/* Machine.  */
  "m32c",			/* Architecture name.  */
  "m16c",			/* Printable name.  */
  4,				/* Section align power.  */
  TRUE,				/* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  &arch_info_struct[0],		/* Next in list.  */
};
