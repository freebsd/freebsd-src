/* BFD support for the Scenix IP2xxx processor.
   Copyright 2000, 2002 Free Software Foundation, Inc.

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

const bfd_arch_info_type bfd_ip2k_nonext_arch =
{
  32,				/* Bits per word - not really true.  */
  16,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_ip2k,		/* Architecture.  */
  bfd_mach_ip2022,		/* Machine.  */
  "ip2k",			/* Architecture name.  */
  "ip2022",			/* Machine name.  */
  1,				/* Section align power.  */
  FALSE,		        /* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  NULL				/* Next in list.  */
};

const bfd_arch_info_type bfd_ip2k_arch =
{
  32,				/* Bits per word - not really true.  */
  16,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_ip2k,		/* Architecture.  */
  bfd_mach_ip2022ext,		/* Machine.  */
  "ip2k",			/* Architecture name.  */
  "ip2022ext",			/* Machine name.  */
  1,				/* Section align power.  */
  TRUE,				/* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  & bfd_ip2k_nonext_arch	/* Next in list.  */
};
