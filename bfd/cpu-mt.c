/* BFD support for the Morpho Technologies MT processor.
   Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.

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

const bfd_arch_info_type arch_info_struct[] =
{
{
  32,				/* Bits per word - not really true.  */
  32,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_mt,			/* Architecture.  */
  bfd_mach_mrisc2,		/* Machine.  */
  "mt",				/* Architecture name.  */
  "ms1-003",			/* Printable name.  */
  1,				/* Section align power.  */
  FALSE,		        /* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  &arch_info_struct[1]          /* Next in list.  */
},
{
  32,				/* Bits per word - not really true.  */
  32,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_mt,			/* Architecture.  */
  bfd_mach_ms2,		        /* Machine.  */
  "mt",				/* Architecture name.  */
  "ms2",			/* Printable name.  */
  1,				/* Section align power.  */
  FALSE,		        /* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  NULL				/* Next in list.  */
},
};

const bfd_arch_info_type bfd_mt_arch =
{
  32,				/* Bits per word - not really true.  */
  32,				/* Bits per address.  */
  8,				/* Bits per byte.  */
  bfd_arch_mt,			/* Architecture.  */
  bfd_mach_ms1,			/* Machine.  */
  "mt",				/* Architecture name.  */
  "ms1",			/* Printable name.  */
  1,				/* Section align power.  */
  TRUE,		        	/* The default ?  */
  bfd_default_compatible,	/* Architecture comparison fn.  */
  bfd_default_scan,		/* String to architecture convert fn.  */
  &arch_info_struct[0]		/* Next in list.  */
};

