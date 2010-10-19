/* BFD support for the MAXQ20/10 architecture.
   Copyright  2004, 2005  Free Software Foundation, Inc.

   Written by Vineet Sharma(vineets@noida.hcltech.com)
	      Inderpreet Singh(inderpreetb@noida.hcltech.com)		

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

/* MAXQ Archtecture info.  */
static const bfd_arch_info_type bfd_maxq10_arch =
{
  16,				/* 16 bits in a word.  */
  16,				/* 16 bits in an address.  */
  8,				/* 16 bits in a byte.  */
  bfd_arch_maxq,		/* Architecture number.  */
  bfd_mach_maxq10,		/* Machine number.  */
  "maxq",			/* Architecture name.  */
  "maxq10",			/* Machine name.  */
  0,				/* Section align power.  */
  FALSE,			/* Not the default machine.  */
  bfd_default_compatible,
  bfd_default_scan,
  NULL
};


const bfd_arch_info_type bfd_maxq_arch =
{
  16,				/* 16 bits in a word.  */
  16,				/* 16 bits in an address.  */
  8,				/* 16 bits in a byte.  */
  bfd_arch_maxq,		/* Architecture number.  */
  bfd_mach_maxq20,		/* Machine number.  */
  "maxq",			/* Architecture name.  */
  "maxq20",			/* Machine name.  */
  0,				/* Section align power.  */
  TRUE,				/* This is the default machine.  */
  bfd_default_compatible,
  bfd_default_scan,
  & bfd_maxq10_arch
};
