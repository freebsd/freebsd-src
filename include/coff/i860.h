/* COFF information for the Intel i860.
   
   Copyright 2001, 2003 Free Software Foundation, Inc.

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

/* This file was hacked from i386.h   [dolan@ssd.intel.com] */

#define L_LNNO_SIZE 2
#include "coff/external.h"

/* Bits for f_flags:
 	F_RELFLG	relocation info stripped from file
 	F_EXEC		file is executable (no unresolved external references)
 	F_LNNO		line numbers stripped from file
 	F_LSYMS		local symbols stripped from file
 	F_AR32WR	file has byte ordering of an AR32WR machine (e.g. vax).  */

#define F_RELFLG	(0x0001)
#define F_EXEC		(0x0002)
#define F_LNNO		(0x0004)
#define F_LSYMS		(0x0008)

#define	I860MAGIC	0x14d

#define I860BADMAG(x)   ((x).f_magic != I860MAGIC)

#undef AOUTSZ
#define AOUTSZ 36

/* FIXME: What are the a.out magic numbers?  */

#define _ETEXT	"etext"

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
};

#define RELOC struct external_reloc
#define RELSZ 10

/* The relocation directory entry types.
     PAIR   : The low half that follows relates to the preceeding HIGH[ADJ].
     HIGH   : The high half of a 32-bit constant.
     LOWn   : The low half, insn bits 15..(n-1), 2^n-byte aligned. 
     SPLITn : The low half, insn bits 20..16 and 10..(n-1), 2^n-byte aligned. 
     HIGHADJ: Similar to HIGH, but with adjustment.
     BRADDR : 26-bit branch displacement.

   Note: The Intel assembler manual lists LOW4 as one of the
   relocation types, but it appears to be useless for the i860.
   We will recognize it anyway, just in case it actually appears in
   any object files.  */

enum {
  COFF860_R_PAIR	= 0x1c,
  COFF860_R_HIGH	= 0x1e,
  COFF860_R_LOW0	= 0x1f,
  COFF860_R_LOW1	= 0x20,
  COFF860_R_LOW2	= 0x21,
  COFF860_R_LOW3	= 0x22,
  COFF860_R_LOW4	= 0x23,
  COFF860_R_SPLIT0	= 0x24,
  COFF860_R_SPLIT1	= 0x25,
  COFF860_R_SPLIT2	= 0x26,
  COFF860_R_HIGHADJ	= 0x27,
  COFF860_R_BRADDR	= 0x28
};

