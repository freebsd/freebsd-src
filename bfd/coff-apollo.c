/* BFD back-end for Apollo 68000 COFF binaries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   By Troy Rollo (troy@cbme.unsw.edu.au)
   Based on m68k standard COFF version Written by Cygnus Support.

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
#include "coff/apollo.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

#ifdef ONLY_DECLARE_RELOCS
extern reloc_howto_type apollocoff_howto_table[];
#else
reloc_howto_type apollocoff_howto_table[] =
  {
    HOWTO (R_RELBYTE,	       0,  0,  	8,  FALSE, 0, complain_overflow_bitfield, 0, "8",	TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (R_RELWORD,	       0,  1, 	16, FALSE, 0, complain_overflow_bitfield, 0, "16",	TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (R_RELLONG,	       0,  2, 	32, FALSE, 0, complain_overflow_bitfield, 0, "32",	TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (R_PCRBYTE,	       0,  0, 	8,  TRUE,  0, complain_overflow_signed,   0, "DISP8",   TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (R_PCRWORD,	       0,  1, 	16, TRUE,  0, complain_overflow_signed,   0, "DISP16",  TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (R_PCRLONG,	       0,  2, 	32, TRUE,  0, complain_overflow_signed,   0, "DISP32",  TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (R_RELLONG_NEG,      0,  -2, 	32, FALSE, 0, complain_overflow_bitfield, 0, "-32",	TRUE, 0xffffffff,0xffffffff, FALSE),
  };
#endif /* not ONLY_DECLARE_RELOCS */

#ifndef BADMAG
#define BADMAG(x) M68KBADMAG(x)
#endif
#define APOLLO_M68 1		/* Customize coffcode.h */

/* Turn a howto into a reloc number.  */

extern void apollo_rtype2howto PARAMS ((arelent *, int));
extern int  apollo_howto2rtype PARAMS ((reloc_howto_type *));
#ifndef ONLY_DECLARE_RELOCS

void
apollo_rtype2howto (internal, relocentry)
     arelent *internal;
     int relocentry;
{
  switch (relocentry)
    {
    case R_RELBYTE:	internal->howto = apollocoff_howto_table + 0; break;
    case R_RELWORD:	internal->howto = apollocoff_howto_table + 1; break;
    case R_RELLONG:	internal->howto = apollocoff_howto_table + 2; break;
    case R_PCRBYTE:	internal->howto = apollocoff_howto_table + 3; break;
    case R_PCRWORD:	internal->howto = apollocoff_howto_table + 4; break;
    case R_PCRLONG:	internal->howto = apollocoff_howto_table + 5; break;
    case R_RELLONG_NEG:	internal->howto = apollocoff_howto_table + 6; break;
    }
}

int
apollo_howto2rtype (internal)
     reloc_howto_type *internal;
{
  if (internal->pc_relative)
    {
      switch (internal->bitsize)
	{
	case 32: return R_PCRLONG;
	case 16: return R_PCRWORD;
	case 8: return R_PCRBYTE;
	}
    }
  else
    {
      switch (internal->bitsize)
	{
	case 32: return R_RELLONG;
	case 16: return R_RELWORD;
	case 8: return R_RELBYTE;
	}
    }
  return R_RELLONG;
}
#endif /* not ONLY_DECLARE_RELOCS */

#define RTYPE2HOWTO(internal, relocentry) \
  apollo_rtype2howto (internal, (relocentry)->r_type)

#define SELECT_RELOC(external, internal) \
  external.r_type = apollo_howto2rtype (internal);

#include "coffcode.h"

#ifndef TARGET_SYM
#define TARGET_SYM apollocoff_vec
#endif

#ifndef TARGET_NAME
#define TARGET_NAME "apollo-m68k"
#endif

#ifdef NAMES_HAVE_UNDERSCORE
CREATE_BIG_COFF_TARGET_VEC (TARGET_SYM, TARGET_NAME, 0, 0, '_', NULL, COFF_SWAP_TABLE)
#else
CREATE_BIG_COFF_TARGET_VEC (TARGET_SYM, TARGET_NAME, 0, 0, 0, NULL, COFF_SWAP_TABLE)
#endif
