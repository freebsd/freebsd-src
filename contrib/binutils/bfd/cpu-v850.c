/* BFD support for the NEC V850 processor
   Copyright 1996, 1997, 1998 Free Software Foundation, Inc.

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

#include <ctype.h>

static boolean
scan (info, string)
     const struct bfd_arch_info * info;
     const char * string;
{
  const char *ptr_src;
  const char *ptr_tst;
  unsigned long number;
  enum bfd_architecture arch;

  /* First test for an exact match */
  if (strcasecmp (string, info->printable_name) == 0)
    return true;

  /* See how much of the supplied string matches with the
     architecture, eg the string m68k:68020 would match the m68k entry
     up to the :, then we get left with the machine number */

  for (ptr_src = string, ptr_tst = info->arch_name;
       *ptr_src && *ptr_tst;
       ptr_src++, ptr_tst++)
    {
      if (*ptr_src != *ptr_tst) break;
    }

  /* Chewed up as much of the architecture as will match, skip any
     colons */
  if (*ptr_src == ':')
    ptr_src++;

  if (*ptr_src == 0)
    {
      /* nothing more, then only keep this one if it is the default
	 machine for this architecture */
      return info->the_default;
    }

  number = 0;
  while (isdigit ((unsigned char) *ptr_src))
    {
      number = number * 10 + * ptr_src  - '0';
      ptr_src++;
    }

  switch (number)
    {
    case bfd_mach_v850e:  arch = bfd_arch_v850; break;
    case bfd_mach_v850ea: arch = bfd_arch_v850; break;
    default:
      return false;
    }

  if (arch != info->arch)
    return false;

  if (number != info->mach)
    return false;

  return true;
}

#define N(number, print, default, next)  \
{  32, 32, 8, bfd_arch_v850, number, "v850", print, 2, default, \
     bfd_default_compatible, scan, next }

#define NEXT NULL

static const bfd_arch_info_type arch_info_struct[] =
{
  N (bfd_mach_v850e,  "v850e",  false, &arch_info_struct[1]),
  N (bfd_mach_v850ea, "v850ea", false, NULL)
};

#undef NEXT
#define NEXT &arch_info_struct[0]

const bfd_arch_info_type bfd_v850_arch =
  N (bfd_mach_v850, "v850", true, NEXT);
