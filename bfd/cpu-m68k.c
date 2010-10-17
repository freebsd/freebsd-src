/* BFD library support routines for architectures.
   Copyright 1990, 1991, 1992, 1993, 1994, 1997, 1998, 2000, 2001, 2002,
   2003 Free Software Foundation, Inc.
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

#define N(name, print,d,next)  \
{  32, 32, 8, bfd_arch_m68k, name, "m68k",print,2,d,bfd_default_compatible,bfd_default_scan, next, }

static const bfd_arch_info_type arch_info_struct[] =
  {
    N(bfd_mach_m68000,  "m68k:68000", FALSE, &arch_info_struct[1]),
    N(bfd_mach_m68008,  "m68k:68008", FALSE, &arch_info_struct[2]),
    N(bfd_mach_m68010,  "m68k:68010", FALSE, &arch_info_struct[3]),
    N(bfd_mach_m68020,  "m68k:68020", FALSE, &arch_info_struct[4]),
    N(bfd_mach_m68030,  "m68k:68030", FALSE, &arch_info_struct[5]),
    N(bfd_mach_m68040,  "m68k:68040", FALSE, &arch_info_struct[6]),
    N(bfd_mach_cpu32,   "m68k:cpu32", FALSE, &arch_info_struct[7]),
    N(bfd_mach_mcf5200, "m68k:5200",  FALSE, &arch_info_struct[8]),
    N(bfd_mach_mcf5206e,"m68k:5206e", FALSE, &arch_info_struct[9]),
    N(bfd_mach_mcf5307, "m68k:5307",  FALSE, &arch_info_struct[10]),
    N(bfd_mach_mcf5407, "m68k:5407",  FALSE, &arch_info_struct[11]),
    N(bfd_mach_m68060,  "m68k:68060", FALSE, &arch_info_struct[12]),
    N(bfd_mach_mcf528x, "m68k:528x",  FALSE, 0),
  };

const bfd_arch_info_type bfd_m68k_arch =
  N(0, "m68k", TRUE, &arch_info_struct[0]);
