/* BFD library support routines for the Renesas / SuperH SH architecture.
   Copyright 1993, 1994, 1997, 1998, 2000, 2001, 2002, 2003, 2004, 2005
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "../opcodes/sh-opc.h"

#define SH_NEXT                            arch_info_struct + 0
#define SH2_NEXT                           arch_info_struct + 1
#define SH2E_NEXT                          arch_info_struct + 2
#define SH_DSP_NEXT                        arch_info_struct + 3
#define SH3_NEXT                           arch_info_struct + 4
#define SH3_NOMMU_NEXT                     arch_info_struct + 5
#define SH3_DSP_NEXT                       arch_info_struct + 6
#define SH3E_NEXT                          arch_info_struct + 7
#define SH4_NEXT                           arch_info_struct + 8
#define SH4A_NEXT                          arch_info_struct + 9
#define SH4AL_DSP_NEXT                     arch_info_struct + 10
#define SH4_NOFPU_NEXT                     arch_info_struct + 11
#define SH4_NOMMU_NOFPU_NEXT               arch_info_struct + 12
#define SH4A_NOFPU_NEXT                    arch_info_struct + 13
#define SH2A_NEXT                          arch_info_struct + 14
#define SH2A_NOFPU_NEXT                    arch_info_struct + 15
#define SH2A_NOFPU_OR_SH4_NOMMU_NOFPU_NEXT arch_info_struct + 16
#define SH2A_NOFPU_OR_SH3_NOMMU_NEXT       arch_info_struct + 17
#define SH2A_OR_SH4_NEXT                   arch_info_struct + 18
#define SH2A_OR_SH3E_NEXT                  arch_info_struct + 19
#define SH64_NEXT                          NULL

static const bfd_arch_info_type arch_info_struct[] =
{
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2,
    "sh",			/* Architecture name.  */
    "sh2",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2e,
    "sh",			/* Architecture name.  */
    "sh2e",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2E_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh_dsp,
    "sh",			/* Architecture name.   */
    "sh-dsp",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh3,
    "sh",			/* Architecture name.   */
    "sh3",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH3_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh3_nommu,
    "sh",			/* Architecture name.   */
    "sh3-nommu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH3_NOMMU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh3_dsp,
    "sh",			/* Architecture name.   */
    "sh3-dsp",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH3_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh3e,
    "sh",			/* Architecture name.   */
    "sh3e",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH3E_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4,
    "sh",			/* Architecture name.   */
    "sh4",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4a,
    "sh",			/* Architecture name.   */
    "sh4a",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4A_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4al_dsp,
    "sh",			/* Architecture name.   */
    "sh4al-dsp",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4AL_DSP_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4_nofpu,
    "sh",			/* Architecture name.   */
    "sh4-nofpu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4_nommu_nofpu,
    "sh",			/* Architecture name.   */
    "sh4-nommu-nofpu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4_NOMMU_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh4a_nofpu,
    "sh",			/* Architecture name.   */
    "sh4a-nofpu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH4A_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a,
    "sh",			/* Architecture name.  */
    "sh2a",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a_nofpu,
    "sh",			/* Architecture name.  */
    "sh2a-nofpu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a_nofpu_or_sh4_nommu_nofpu,
    "sh",			/* Architecture name.  */
    "sh2a-nofpu-or-sh4-nommu-nofpu",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_NOFPU_OR_SH4_NOMMU_NOFPU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a_nofpu_or_sh3_nommu,
    "sh",			/* Architecture name. .  */
    "sh2a-nofpu-or-sh3-nommu",	/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_NOFPU_OR_SH3_NOMMU_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a_or_sh4,
    "sh",			/* Architecture name.  */
    "sh2a-or-sh4",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_OR_SH4_NEXT
  },
  {
    32,				/* 32 bits in a word.  */
    32,				/* 32 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh2a_or_sh3e,
    "sh",			/* Architecture name.  */
    "sh2a-or-sh3e",		/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH2A_OR_SH3E_NEXT
  },
  {
    64,				/* 64 bits in a word.  */
    64,				/* 64 bits in an address.  */
    8,				/* 8 bits in a byte.  */
    bfd_arch_sh,
    bfd_mach_sh5,
    "sh",			/* Architecture name.   */
    "sh5",			/* Machine name.  */
    1,
    FALSE,			/* Not the default.  */
    bfd_default_compatible,
    bfd_default_scan,
    SH64_NEXT
  },
};

const bfd_arch_info_type bfd_sh_arch =
{
  32,				/* 32 bits in a word.  */
  32,				/* 32 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_sh,
  bfd_mach_sh,
  "sh",				/* Architecture name.   */
  "sh",				/* Machine name.  */
  1,
  TRUE,				/* The default machine.  */
  bfd_default_compatible,
  bfd_default_scan,
  SH_NEXT
};


/* This table defines the mappings from the BFD internal numbering
   system to the opcodes internal flags system.
   It is used by the functions defined below.
   The prototypes for these SH specific functions are found in
   sh-opc.h .  */

static struct { unsigned long bfd_mach, arch, arch_up; } bfd_to_arch_table[] =
{
  { bfd_mach_sh,              arch_sh1,             arch_sh_up },
  { bfd_mach_sh2,             arch_sh2,             arch_sh2_up },
  { bfd_mach_sh2e,            arch_sh2e,            arch_sh2e_up },
  { bfd_mach_sh_dsp,          arch_sh_dsp,          arch_sh_dsp_up },
  { bfd_mach_sh2a,            arch_sh2a,            arch_sh2a_up },
  { bfd_mach_sh2a_nofpu,      arch_sh2a_nofpu,      arch_sh2a_nofpu_up },

  { bfd_mach_sh2a_nofpu_or_sh4_nommu_nofpu,         arch_sh2a_nofpu_or_sh4_nommu_nofpu,   arch_sh2a_nofpu_or_sh4_nommu_nofpu_up },
  { bfd_mach_sh2a_nofpu_or_sh3_nommu,               arch_sh2a_nofpu_or_sh3_nommu,         arch_sh2a_nofpu_or_sh3_nommu_up },
  { bfd_mach_sh2a_or_sh4,     arch_sh2a_or_sh4,     arch_sh2a_or_sh4_up },
  { bfd_mach_sh2a_or_sh3e,    arch_sh2a_or_sh3e,    arch_sh2a_or_sh3e_up },
  
  { bfd_mach_sh3,             arch_sh3,             arch_sh3_up },
  { bfd_mach_sh3_nommu,       arch_sh3_nommu,       arch_sh3_nommu_up },
  { bfd_mach_sh3_dsp,         arch_sh3_dsp,         arch_sh3_dsp_up },
  { bfd_mach_sh3e,            arch_sh3e,            arch_sh3e_up },
  { bfd_mach_sh4,             arch_sh4,             arch_sh4_up },
  { bfd_mach_sh4a,            arch_sh4a,            arch_sh4a_up },
  { bfd_mach_sh4al_dsp,       arch_sh4al_dsp,       arch_sh4al_dsp_up },
  { bfd_mach_sh4_nofpu,       arch_sh4_nofpu,       arch_sh4_nofpu_up },
  { bfd_mach_sh4_nommu_nofpu, arch_sh4_nommu_nofpu, arch_sh4_nommu_nofpu_up },
  { bfd_mach_sh4a_nofpu,      arch_sh4a_nofpu,      arch_sh4a_nofpu_up },
  { 0, 0, 0 }   /* Terminator.  */
};


/* Convert a BFD mach number into the right opcodes arch flags
   using the table above.  */

unsigned int
sh_get_arch_from_bfd_mach (unsigned long mach)
{
  int i = 0;

  while (bfd_to_arch_table[i].bfd_mach != 0)
    if (bfd_to_arch_table[i].bfd_mach == mach)
      return bfd_to_arch_table[i].arch;
    else
      i++;

  /* Machine not found.   */
  BFD_FAIL();

  return SH_ARCH_UNKNOWN_ARCH;
}


/* Convert a BFD mach number into a set of opcodes arch flags
   describing all the compatible architectures (i.e. arch_up)
   using the table above.  */

unsigned int
sh_get_arch_up_from_bfd_mach (unsigned long mach)
{
  int i = 0;

  while (bfd_to_arch_table[i].bfd_mach != 0)
    if (bfd_to_arch_table[i].bfd_mach == mach)
      return bfd_to_arch_table[i].arch_up;
    else
      i++;

  /* Machine not found.  */
  BFD_FAIL();

  return SH_ARCH_UNKNOWN_ARCH;
}


/* Convert an arbitary arch_set - not necessarily corresponding
   directly to anything in the table above - to the most generic
   architecture which supports all the required features, and
   return the corresponding BFD mach.  */

unsigned long
sh_get_bfd_mach_from_arch_set (unsigned int arch_set)
{
  unsigned long result = 0;
  unsigned int best = ~arch_set;
  unsigned int co_mask = ~0;
  int i = 0;

  /* If arch_set permits variants with no coprocessor then do not allow
     the other irrelevant co-processor bits to influence the choice:
       e.g. if dsp is disallowed by arch_set, then the algorithm would
       prefer fpu variants over nofpu variants because they also disallow
       dsp - even though the nofpu would be the most correct choice.
     This assumes that EVERY fpu/dsp variant has a no-coprocessor
     counter-part, or their non-fpu/dsp instructions do not have the
     no co-processor bit set.  */
  if (arch_set & arch_sh_no_co)
    co_mask = ~(arch_sh_sp_fpu | arch_sh_dp_fpu | arch_sh_has_dsp);

  while (bfd_to_arch_table[i].bfd_mach != 0)
    {
      unsigned int try = bfd_to_arch_table[i].arch_up & co_mask;

      /* Conceptually: Find the architecture with the least number
	 of extra features or, if they have the same number, then
	 the greatest number of required features.  Disregard
         architectures where the required features alone do
	 not describe a valid architecture.  */
      if (((try & ~arch_set) < (best & ~arch_set)
	   || ((try & ~arch_set) == (best & ~arch_set)
	       && (~try & arch_set) < (~best & arch_set)))
	  && SH_MERGE_ARCH_SET_VALID (try, arch_set))
	{
	  result = bfd_to_arch_table[i].bfd_mach;
	  best = try;
	}

      i++;
    }

  /* This might happen if a new variant is added to sh-opc.h
     but no corresponding entry is added to the table above.  */
  BFD_ASSERT (result != 0);

  return result;
}


/* Merge the architecture type of two BFD files, such that the
   resultant architecture supports all the features required
   by the two input BFDs.
   If the input BFDs are multually incompatible - i.e. one uses
   DSP while the other uses FPU - or there is no known architecture
   that fits the requirements then an error is emitted.  */

bfd_boolean
sh_merge_bfd_arch (bfd *ibfd, bfd *obfd)
{
  unsigned int old_arch, new_arch, merged_arch;

  if (! _bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  old_arch = sh_get_arch_up_from_bfd_mach (bfd_get_mach (obfd));
  new_arch = sh_get_arch_up_from_bfd_mach (bfd_get_mach (ibfd));

  merged_arch = SH_MERGE_ARCH_SET (old_arch, new_arch);

  if (!SH_VALID_CO_ARCH_SET (merged_arch))
    {
      (*_bfd_error_handler)
	("%B: uses %s instructions while previous modules use %s instructions",
	 ibfd,
	 SH_ARCH_SET_HAS_DSP (new_arch) ? "dsp" : "floating point",
	 SH_ARCH_SET_HAS_DSP (new_arch) ? "floating point" : "dsp");
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }
  else if (!SH_VALID_ARCH_SET (merged_arch))
    {
      (*_bfd_error_handler)
	("internal error: merge of architecture '%s' with architecture '%s' produced unknown architecture\n",
	 bfd_printable_name (obfd),
	 bfd_printable_name (ibfd));
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  bfd_default_set_arch_mach (obfd, bfd_arch_sh,
			     sh_get_bfd_mach_from_arch_set (merged_arch));
  
  return TRUE;
}
