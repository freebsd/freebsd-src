/* BFD support for the XSTORMY16 processor.
   Copyright 2001, 2002 Free Software Foundation, Inc.

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

const bfd_arch_info_type bfd_xstormy16_arch =
{
  16,				/* bits per word */
  32,				/* bits per address */
  8,				/* bits per byte */
  bfd_arch_xstormy16,		/* architecture */
  bfd_mach_xstormy16,		/* machine */
  "xstormy16",			/* architecture name */
  "xstormy16",			/* printable name */
  2,				/* section align power */
  TRUE,				/* the default ? */
  bfd_default_compatible,	/* architecture comparison fn */
  bfd_default_scan,		/* string to architecture convert fn */
  NULL				/* next in list */
};
