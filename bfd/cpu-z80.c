/* BFD library support routines for the Z80 architecture.
   Copyright 2005 Free Software Foundation, Inc.
   Contributed by Arnold Metselaar <arnold_m@operamail.com>

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

const bfd_arch_info_type bfd_z80_arch;

/* This routine is provided two arch_infos and
   returns whether they'd be compatible.  */

static const bfd_arch_info_type *
compatible (const bfd_arch_info_type *a, const bfd_arch_info_type *b)
{
  if (a->arch != b->arch)
    return NULL;

  if (a->mach == b->mach)
    return a;

  return (a->arch == bfd_arch_z80) ? & bfd_z80_arch : NULL;
}

#define N(name,print,default,next)  \
{ 16, 16, 8, bfd_arch_z80, name, "z80", print, 0, default, \
    compatible, bfd_default_scan, next }

#define M(n) &arch_info_struct[n]

static const bfd_arch_info_type arch_info_struct[] =
{
  N (bfd_mach_z80strict, "z80-strict", FALSE, M(1)),
  N (bfd_mach_z80,       "z80",        FALSE, M(2)),
  N (bfd_mach_z80full,   "z80-full",   FALSE, M(3)),
  N (bfd_mach_r800,      "r800",       FALSE, NULL)
};

const bfd_arch_info_type bfd_z80_arch = N (0, "z80-any", TRUE, M(0));
