/* COFF spec for MAXQ

   Copyright 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free 
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

   Contributed by Vineet Sharma(vineets@noida.hcltech.com) Inderpreet
   S.(inderpreetb@noida.hcltech.com) HCL Technologies Ltd.  */

#define L_LNNO_SIZE 2

#include "coff/external.h"

/* Bits for f_flags: F_RELFLG relocation info stripped from file F_EXEC file
   is executable (no unresolved external references) F_LNNO line numbers
   stripped from file F_LSYMS local symbols stripped from file.  */

#define F_RELFLG        (0x0001)
#define F_EXEC          (0x0002)
#define F_LNNO          (0x0004)
#define F_LSYMS         (0x0008)

/* Variant Specific Flags for MAXQ10 and MAXQ20.  */
#define F_MAXQ10	(0x0030)
#define F_MAXQ20	(0x0040)

#define F_MACHMASK	(0x00F0)

/* Magic numbers for maxq.  */
#define MAXQ20MAGIC      0xa0
#define MAXQ20BADMAG(x) (((x).f_magic != MAXQ20MAGIC))
#define BADMAG(x)        MAXQ20BADMAG (x)

/* Relocation information declaration and related definitions.  */
struct external_reloc
{
  char r_vaddr[4];		/* (Virtual) address of reference.  */
  char r_symndx[4];		/* Index into symbol table.  */
  char r_type[2];		/* Relocation type.  */
  char r_offset[2];		/* Addend.  */
};

#define	RELOC		struct external_reloc
#define	RELSZ		(10 + 2)	/* sizeof (RELOC) */
