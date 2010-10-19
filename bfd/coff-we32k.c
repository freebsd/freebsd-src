/* BFD back-end for we32k COFF files.
   Copyright 1992, 1993, 1994, 1999, 2000, 2002, 2003
   Free Software Foundation, Inc.
   Contributed by Brendan Kehoe (brendan@cs.widener.edu).

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
#include "coff/we32k.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

static reloc_howto_type howto_table[] =
{
    EMPTY_HOWTO (0),
    EMPTY_HOWTO (1),
    EMPTY_HOWTO (2),
    EMPTY_HOWTO (3),
    EMPTY_HOWTO (4),
    EMPTY_HOWTO (5),
  HOWTO(R_DIR32,	       0,  2, 	32, FALSE, 0,complain_overflow_bitfield, 0, "dir32",	TRUE, 0xffffffff,0xffffffff, FALSE),
    EMPTY_HOWTO (7),
    EMPTY_HOWTO (010),
    EMPTY_HOWTO (011),
    EMPTY_HOWTO (012),
    EMPTY_HOWTO (013),
    EMPTY_HOWTO (014),
    EMPTY_HOWTO (015),
    EMPTY_HOWTO (016),
  HOWTO(R_RELBYTE,	       0,  0,  	8,  FALSE, 0, complain_overflow_bitfield, 0, "8",	TRUE, 0x000000ff,0x000000ff, FALSE),
  HOWTO(R_RELWORD,	       0,  1, 	16, FALSE, 0, complain_overflow_bitfield, 0, "16",	TRUE, 0x0000ffff,0x0000ffff, FALSE),
  HOWTO(R_RELLONG,	       0,  2, 	32, FALSE, 0, complain_overflow_bitfield, 0, "32",	TRUE, 0xffffffff,0xffffffff, FALSE),
  HOWTO(R_PCRBYTE,	       0,  0, 	8,  TRUE,  0, complain_overflow_signed, 0, "DISP8",    TRUE, 0x000000ff,0x000000ff, FALSE),
  HOWTO(R_PCRWORD,	       0,  1, 	16, TRUE,  0, complain_overflow_signed, 0, "DISP16",   TRUE, 0x0000ffff,0x0000ffff, FALSE),
  HOWTO(R_PCRLONG,	       0,  2, 	32, TRUE,  0, complain_overflow_signed, 0, "DISP32",   TRUE, 0xffffffff,0xffffffff, FALSE),
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) WE32KBADMAG(x)
#define WE32K	1

#define RTYPE2HOWTO(cache_ptr, dst) \
	    (cache_ptr)->howto = howto_table + (dst)->r_type;

#include "coffcode.h"

#define coff_write_armap bsd_write_armap

CREATE_BIG_COFF_TARGET_VEC (we32kcoff_vec, "coff-we32k", 0, 0, 0, NULL, COFF_SWAP_TABLE)
