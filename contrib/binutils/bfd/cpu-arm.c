/* BFD support for the ARM processor
   Copyright 1994, 95, 97, 1999 Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)

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

static const bfd_arch_info_type *compatible
  PARAMS ((const bfd_arch_info_type *, const bfd_arch_info_type *));
static boolean scan PARAMS ((const struct bfd_arch_info *, const char *));

/* This routine is provided two arch_infos and works out which ARM
   machine which would be compatible with both and returns a pointer
   to its info structure */

static const bfd_arch_info_type *
compatible (a,b)
     const bfd_arch_info_type * a;
     const bfd_arch_info_type * b;
{
  /* If a & b are for different architecture we can do nothing */
  if (a->arch != b->arch)
      return NULL;

  /* If a & b are for the same machine then all is well */
  if (a->mach == b->mach)
    return a;

  /* Otherwise if either a or b is the 'default' machine then
     it can be polymorphed into the other */
  if (a->the_default)
    return b;
  
  if (b->the_default)
    return a;

  /* So far all newer ARM architecture cores are supersets of previous cores */
  if (a->mach < b->mach)
    return b;
  else if (a->mach > b->mach)
    return a;

  /* Never reached! */
  return NULL;
}

static struct
{
  enum bfd_architecture arch;
  char *                name;
}
processors[] =
{
  { bfd_mach_arm_2,  "arm2"     },
  { bfd_mach_arm_2a, "arm250"   },
  { bfd_mach_arm_2a, "arm3"     },
  { bfd_mach_arm_3,  "arm6"     },
  { bfd_mach_arm_3,  "arm60"    },
  { bfd_mach_arm_3,  "arm600"   },
  { bfd_mach_arm_3,  "arm610"   },
  { bfd_mach_arm_3,  "arm7"     },
  { bfd_mach_arm_3,  "arm710"   },
  { bfd_mach_arm_3,  "arm7500"  },
  { bfd_mach_arm_3,  "arm7d"    },
  { bfd_mach_arm_3,  "arm7di"   },
  { bfd_mach_arm_3M, "arm7dm"   },
  { bfd_mach_arm_3M, "arm7dmi"  },
  { bfd_mach_arm_4T, "arm7tdmi" },
  { bfd_mach_arm_4,  "arm8"     },
  { bfd_mach_arm_4,  "arm810"   },
  { bfd_mach_arm_4,  "arm9"     },
  { bfd_mach_arm_4,  "arm920"   },
  { bfd_mach_arm_4T, "arm920t"  },
  { bfd_mach_arm_4T, "arm9tdmi" },
  { bfd_mach_arm_4,  "sa1"      },
  { bfd_mach_arm_4,  "strongarm"},
  { bfd_mach_arm_4,  "strongarm110" },
  { bfd_mach_arm_4,  "strongarm1100" },
};

static boolean 
scan (info, string)
     const struct bfd_arch_info * info;
     const char * string;
{
  int  i;

  /* First test for an exact match */
  if (strcasecmp (string, info->printable_name) == 0)
    return true;

  /* Next check for a processor name instead of an Architecture name */
  for (i = sizeof (processors) / sizeof (processors[0]); i--;)
    {
      if (strcasecmp (string, processors[ i ].name) == 0)
	break;
    }

  if (i != -1 && info->arch == processors[ i ].arch)
    return true;

  /* Finally check for the default architecture */
  if (strcasecmp (string, "arm") == 0)
    return info->the_default;
  
  return false;
}


#define N(number, print, default, next)  \
{  32, 32, 8, bfd_arch_arm, number, "arm", print, 4, default, compatible, scan, next }

static const bfd_arch_info_type arch_info_struct[] =
{ 
  N( bfd_mach_arm_2,  "armv2",  false, & arch_info_struct[1] ),
  N( bfd_mach_arm_2a, "armv2a", false, & arch_info_struct[2] ),
  N( bfd_mach_arm_3,  "armv3",  false, & arch_info_struct[3] ),
  N( bfd_mach_arm_3M, "armv3m", false, & arch_info_struct[4] ),
  N( bfd_mach_arm_4,  "armv4",  false, & arch_info_struct[5] ),
  N( bfd_mach_arm_4T, "armv4t", false, & arch_info_struct[6] ),
  N( bfd_mach_arm_5,  "armv5",  false, & arch_info_struct[7] ),
  N( bfd_mach_arm_5T, "armv5t", false, NULL )
};

const bfd_arch_info_type bfd_arm_arch =
  N( 0, "arm", true, & arch_info_struct[0] );
