/* BFD library support routines for architectures.
   Copyright 1990, 1991, 1992, 1993, 1994, 1997, 1998, 2000, 2001, 2002,
   2003, 2004, 2006 Free Software Foundation, Inc.
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
#include "opcode/m68k.h"

static const bfd_arch_info_type *
bfd_m68k_compatible (const bfd_arch_info_type *a,
		     const bfd_arch_info_type *b);

#define N(name, print,d,next)  \
{  32, 32, 8, bfd_arch_m68k, name, "m68k",print,2,d,bfd_m68k_compatible,bfd_default_scan, next, }

static const bfd_arch_info_type arch_info_struct[] =
  {
    N(bfd_mach_m68000,  "m68k:68000", FALSE, &arch_info_struct[1]),
    N(bfd_mach_m68008,  "m68k:68008", FALSE, &arch_info_struct[2]),
    N(bfd_mach_m68010,  "m68k:68010", FALSE, &arch_info_struct[3]),
    N(bfd_mach_m68020,  "m68k:68020", FALSE, &arch_info_struct[4]),
    N(bfd_mach_m68030,  "m68k:68030", FALSE, &arch_info_struct[5]),
    N(bfd_mach_m68040,  "m68k:68040", FALSE, &arch_info_struct[6]),
    N(bfd_mach_m68060,  "m68k:68060", FALSE, &arch_info_struct[7]),
    N(bfd_mach_cpu32,   "m68k:cpu32", FALSE, &arch_info_struct[8]),

    /* Various combinations of CF architecture features */
    N(bfd_mach_mcf_isa_a_nodiv, "m68k:isa-a:nodiv",
      FALSE, &arch_info_struct[9]),
    N(bfd_mach_mcf_isa_a, "m68k:isa-a",
      FALSE, &arch_info_struct[10]),
    N(bfd_mach_mcf_isa_a_mac, "m68k:isa-a:mac",
      FALSE, &arch_info_struct[11]),
    N(bfd_mach_mcf_isa_a_emac, "m68k:isa-a:emac",
      FALSE, &arch_info_struct[12]),
    N(bfd_mach_mcf_isa_aplus, "m68k:isa-aplus",
      FALSE, &arch_info_struct[13]),
    N(bfd_mach_mcf_isa_aplus_mac, "m68k:isa-aplus:mac",
      FALSE, &arch_info_struct[14]),
    N(bfd_mach_mcf_isa_aplus_emac, "m68k:isa-aplus:emac",
      FALSE, &arch_info_struct[15]),
    N(bfd_mach_mcf_isa_b_nousp, "m68k:isa-b:nousp",
      FALSE, &arch_info_struct[16]),
    N(bfd_mach_mcf_isa_b_nousp_mac, "m68k:isa-b:nousp:mac",
      FALSE, &arch_info_struct[17]),
    N(bfd_mach_mcf_isa_b_nousp_emac, "m68k:isa-b:nousp:emac",
      FALSE, &arch_info_struct[18]),
    N(bfd_mach_mcf_isa_b, "m68k:isa-b",
      FALSE, &arch_info_struct[19]),
    N(bfd_mach_mcf_isa_b_mac, "m68k:isa-b:mac",
      FALSE, &arch_info_struct[20]),
    N(bfd_mach_mcf_isa_b_emac, "m68k:isa-b:emac",
      FALSE, &arch_info_struct[21]),
    N(bfd_mach_mcf_isa_b_float, "m68k:isa-b:float",
      FALSE, &arch_info_struct[22]),
    N(bfd_mach_mcf_isa_b_float_mac, "m68k:isa-b:float:mac",
      FALSE, &arch_info_struct[23]),
    N(bfd_mach_mcf_isa_b_float_emac, "m68k:isa-b:float:emac",
      FALSE, &arch_info_struct[24]),

    /* Legacy names for CF architectures */
    N(bfd_mach_mcf_isa_a_nodiv, "m68k:5200", FALSE, &arch_info_struct[25]),
    N(bfd_mach_mcf_isa_a_mac,"m68k:5206e", FALSE, &arch_info_struct[26]),
    N(bfd_mach_mcf_isa_a_mac, "m68k:5307", FALSE, &arch_info_struct[27]),
    N(bfd_mach_mcf_isa_b_nousp_mac, "m68k:5407", FALSE, &arch_info_struct[28]),
    N(bfd_mach_mcf_isa_aplus_emac, "m68k:528x", FALSE, &arch_info_struct[29]),
    N(bfd_mach_mcf_isa_aplus_emac, "m68k:521x", FALSE, &arch_info_struct[30]),
    N(bfd_mach_mcf_isa_a_emac, "m68k:5249", FALSE, &arch_info_struct[31]),
    N(bfd_mach_mcf_isa_b_float_emac, "m68k:547x",
      FALSE, &arch_info_struct[32]),
    N(bfd_mach_mcf_isa_b_float_emac, "m68k:548x",
      FALSE, &arch_info_struct[33]),
    N(bfd_mach_mcf_isa_b_float_emac, "m68k:cfv4e", FALSE, 0),
  };

const bfd_arch_info_type bfd_m68k_arch =
  N(0, "m68k", TRUE, &arch_info_struct[0]);

/* Table indexed by bfd_mach_arch number indicating which
   architectural features are supported.  */
static const unsigned m68k_arch_features[] = 
{
  0,
  m68000|m68881|m68851,
  m68000|m68881|m68851,
  m68010|m68881|m68851,
  m68020|m68881|m68851,
  m68030|m68881|m68851,
  m68040|m68881|m68851,
  m68060|m68881|m68851,
  cpu32|m68881,
  mcfisa_a,
  mcfisa_a|mcfhwdiv,
  mcfisa_a|mcfhwdiv|mcfmac,
  mcfisa_a|mcfhwdiv|mcfemac,
  mcfisa_a|mcfisa_aa|mcfhwdiv|mcfusp,
  mcfisa_a|mcfisa_aa|mcfhwdiv|mcfusp|mcfmac,
  mcfisa_a|mcfisa_aa|mcfhwdiv|mcfusp|mcfemac,
  mcfisa_a|mcfhwdiv|mcfisa_b,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfmac,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfemac,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp|mcfmac,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp|mcfemac,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp|cfloat,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp|cfloat|mcfmac,
  mcfisa_a|mcfhwdiv|mcfisa_b|mcfusp|cfloat|mcfemac,
};

/* Return the count of bits set in MASK  */
static unsigned
bit_count (unsigned mask)
{
  unsigned ix;

  for (ix = 0; mask; ix++)
    /* Clear the LSB set */
    mask ^= mask & -mask;
  return ix;
}

/* Return the architectural features supported by MACH */

unsigned
bfd_m68k_mach_to_features (int mach)
{
  if ((unsigned)mach
      >= sizeof (m68k_arch_features) / sizeof (m68k_arch_features[0]))
    mach = 0;
  return m68k_arch_features[mach];
}

/* Return the bfd machine that most closely represents the
   architectural features.  We find the machine with the smallest
   number of additional features.  If there is no such machine, we
   find the one with the smallest number of missing features.  */

int bfd_m68k_features_to_mach (unsigned features)
{
  int superset = 0, subset = 0;
  unsigned extra = 99, missing = 99;
  unsigned ix;

  for (ix = 0;
       ix != sizeof (m68k_arch_features) / sizeof (m68k_arch_features[0]);
       ix++)
    {
      unsigned this_extra, this_missing;
      
      if (m68k_arch_features[ix] == features)
	return ix;
      this_extra = bit_count (m68k_arch_features[ix] & ~features);
      if (this_extra < extra)
	{
	  extra = this_extra;
	  superset = ix;
	}
      
      this_missing = bit_count (features & ~m68k_arch_features[ix]);
      if (this_missing < missing)
	{
	  missing = this_missing;
	  superset = ix;
	}
    }
  return superset ? superset : subset;
}

static const bfd_arch_info_type *
bfd_m68k_compatible (const bfd_arch_info_type *a,
		     const bfd_arch_info_type *b)
{
  if (a->arch != b->arch)
    return NULL;

  if (a->bits_per_word != b->bits_per_word)
    return NULL;

  if (!a->mach)
    return b;
  if (!b->mach)
    return a;
  
  if (a->mach <= bfd_mach_m68060 && b->mach <= bfd_mach_m68060)
    /* Merge m68k machine. */
    return a->mach > b->mach ? a : b;
  else if (a->mach == bfd_mach_cpu32 && b->mach == bfd_mach_cpu32)
    /* CPU32 is compatible with itself. */
    return a;
  else if (a->mach >= bfd_mach_mcf_isa_a_nodiv
	   && b->mach >= bfd_mach_mcf_isa_a_nodiv)
    {
      /* Merge cf machine.  */
      unsigned features = (bfd_m68k_mach_to_features (a->mach)
			   | bfd_m68k_mach_to_features (b->mach));

      /* ISA A+ and ISA B are incompatible.  */
      if ((~features & (mcfisa_aa | mcfisa_b)) == 0)
	return NULL;

      /* MAC and EMAC code cannot be merged.  */
      if ((~features & (mcfmac | mcfemac)) == 0)
	return NULL;

      return bfd_lookup_arch (a->arch, bfd_m68k_features_to_mach (features));
    }
  else
    /* They are incompatible.  */
    return NULL;
}
