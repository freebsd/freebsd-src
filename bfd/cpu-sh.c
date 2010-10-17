/* BFD library support routines for the Renesas / SuperH SH architecture.
   Copyright 1993, 1994, 1997, 1998, 2000, 2001, 2002, 2003
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

#define SH_NEXT      &arch_info_struct[0]
#define SH2_NEXT     &arch_info_struct[1]
#define SH2E_NEXT    &arch_info_struct[2]
#define SH_DSP_NEXT  &arch_info_struct[3]
#define SH3_NEXT     &arch_info_struct[4]
#define SH3_DSP_NEXT &arch_info_struct[5]
#define SH3E_NEXT    &arch_info_struct[6]
#define SH4_NEXT     &arch_info_struct[7]
#define SH4A_NEXT    &arch_info_struct[8]
#define SH4AL_DSP_NEXT &arch_info_struct[9]
#define SH4_NOFPU_NEXT &arch_info_struct[10]
#define SH4A_NOFPU_NEXT &arch_info_struct[11]
#define SH64_NEXT    NULL

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh2,
    "sh",			/* arch_name  */
    "sh2",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH2_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh2e,
    "sh",			/* arch_name  */
    "sh2e",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH2E_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh_dsp,
    "sh",			/* arch_name  */
    "sh-dsp",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh3,
    "sh",			/* arch_name  */
    "sh3",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH3_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh3_dsp,
    "sh",			/* arch_name  */
    "sh3-dsp",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH3_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh3e,
    "sh",			/* arch_name  */
    "sh3e",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH3E_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh4,
    "sh",			/* arch_name  */
    "sh4",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH4_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh4a,
    "sh",			/* arch_name  */
    "sh4a",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH4A_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh4al_dsp,
    "sh",			/* arch_name  */
    "sh4al-dsp",		/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH4AL_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh4_nofpu,
    "sh",			/* arch_name  */
    "sh4-nofpu",		/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH4_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word */
    32,				/* 32 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh4a_nofpu,
    "sh",			/* arch_name  */
    "sh4a-nofpu",		/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH4A_NOFPU_NEXT
  },
  {
    64,				/* 64 bits in a word */
    64,				/* 64 bits in an address */
    8,				/* 8 bits in a byte */
    bfd_arch_sh,
    bfd_mach_sh5,
    "sh",			/* arch_name  */
    "sh5",			/* printable name */
    1,
    FALSE,			/* not the default */
    bfd_default_compatible,
    bfd_default_scan,
    SH64_NEXT
  },
};

const bfd_arch_info_type bfd_sh_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_sh,
  bfd_mach_sh,
  "sh",				/* arch_name  */
  "sh",				/* printable name */
  1,
  TRUE,				/* the default machine */
  bfd_default_compatible,
  bfd_default_scan,
  SH_NEXT
};
