/* BFD library support routines for the H8/500 architecture.
   Copyright 1993, 1995, 1999, 2000, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support.

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

static bfd_boolean scan_mach
  PARAMS ((const struct bfd_arch_info *, const char *));

static bfd_boolean
scan_mach (info, string)
     const struct bfd_arch_info *info ATTRIBUTE_UNUSED;
     const char *string;
{
  if (strcmp (string,"h8/500") == 0)
    return TRUE;
  if (strcmp (string,"H8/500") == 0)
    return TRUE;
  if (strcmp (string,"h8500") == 0)
    return TRUE;
  if (strcmp (string,"H8500") == 0)
    return TRUE;
  return FALSE;
}

const bfd_arch_info_type bfd_h8500_arch =
{
  16,				/* 16 bits in a word */
  24,				/* 24 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8500,
  0,				/* only 1 machine */
  "h8500",			/* arch_name  */
  "h8500",			/* printable name */
  1,
  TRUE,				/* the default machine */
  bfd_default_compatible,
  scan_mach,
  0,
};
