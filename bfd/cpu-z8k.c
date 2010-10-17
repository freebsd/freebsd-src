/* BFD library support routines for the Z800n architecture.
   Copyright 1992, 1993, 1994, 2000, 2001, 2002, 2003
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/* This routine is provided two arch_infos and returns whether
   they'd be compatible */

static const bfd_arch_info_type *
compatible (const bfd_arch_info_type *a, const bfd_arch_info_type *b)
{
  if (a->arch != b->arch || a->mach != b->mach)
    return NULL;
  return a;
}

static const bfd_arch_info_type arch_info_struct[] =
{
  { 32, 16, 8, bfd_arch_z8k, bfd_mach_z8002, "z8k", "z8002", 1, FALSE,
    compatible, bfd_default_scan, 0 }
};

const bfd_arch_info_type bfd_z8k_arch =
{
  32, 32, 8, bfd_arch_z8k, bfd_mach_z8001, "z8k", "z8001", 1, TRUE,
  compatible, bfd_default_scan, &arch_info_struct[0]
};
