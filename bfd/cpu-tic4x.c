/* bfd back-end for TMS320C[34]x support
   Copyright 1996, 1997, 2002, 2003 Free Software Foundation, Inc.

   Contributed by Michael Hayes (m.hayes@elec.canterbury.ac.nz)

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

static bfd_boolean tic4x_scan
    PARAMS ((const struct bfd_arch_info *, const char * ));


static bfd_boolean
tic4x_scan (info, string)
     const struct bfd_arch_info *info;
     const char *string;
{
  /* Allow strings of form [ti][Cc][34][0-9], let's not be too picky
     about strange numbered machines in C3x or C4x series.  */
  if (string[0] == 't' && string[1] == 'i')
    string += 2;
  if (*string == 'C' || *string == 'c')
    string++;
  if (string[1] < '0' && string[1] > '9')
    return FALSE;

  if (*string == '3')
    return (info->mach == bfd_mach_tic3x);
  else if (*string == '4')
    return info->mach == bfd_mach_tic4x;

  return FALSE;
}


const bfd_arch_info_type bfd_tic3x_arch =
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    32,				/* 32 bits in a byte.  */
    bfd_arch_tic4x,
    bfd_mach_tic3x,		/* Machine number.  */
    "tic3x",			/* Architecture name.  */
    "tms320c3x",		/* Printable name.  */
    0,				/* Alignment power.  */
    FALSE,			/* Not the default architecture.  */
    bfd_default_compatible,
    tic4x_scan,
    0
  };

const bfd_arch_info_type bfd_tic4x_arch =
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    32,				/* 32 bits in a byte.  */
    bfd_arch_tic4x,
    bfd_mach_tic4x,		/* Machine number.  */
    "tic4x",			/* Architecture name.  */
    "tms320c4x",		/* Printable name.  */
    0,				/* Alignment power.  */
    TRUE,			/* The default architecture.  */
    bfd_default_compatible,
    tic4x_scan,
    &bfd_tic3x_arch,
  };


