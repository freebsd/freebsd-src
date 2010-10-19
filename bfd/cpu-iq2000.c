/* BFD support for the Vitesse IQ2000 processor.
   Copyright (C) 2003 Free Software Foundation, Inc.

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

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,				/* bits per word */
    32,				/* bits per address */
    8,				/* bits per byte */
    bfd_arch_iq2000,		/* architecture */
    bfd_mach_iq10,		/* machine */
    "iq2000",			/* architecture name */
    "iq10",			/* printable name */
    3,				/* section align power */
    FALSE,			/* the default ? */
    bfd_default_compatible,	/* architecture comparison fn */
    bfd_default_scan,		/* string to architecture convert fn */
    NULL			/* next in list */
  }
};

const bfd_arch_info_type bfd_iq2000_arch =
{
  32,				/* bits per word */
  32,				/* bits per address */
  8,				/* bits per byte */
  bfd_arch_iq2000,		/* architecture */
  bfd_mach_iq2000,		/* machine */
  "iq2000",			/* architecture name */
  "iq2000",			/* printable name */
  3,				/* section align power */
  TRUE,				/* the default ? */
  bfd_default_compatible,	/* architecture comparison fn */
  bfd_default_scan,		/* string to architecture convert fn */
  &arch_info_struct[0],		/* next in list */
};
