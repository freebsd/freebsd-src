/* coff information for we32k
   
   Copyright 2001 Free Software Foundation, Inc.

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
#define	F_BM32B		(0020000)
#define	F_BM32MAU	(0040000)

#define	WE32KMAGIC	0x170	/* we32k sans transfer vector */
#define FBOMAGIC	0x170	/* we32k sans transfer vector */
#define MTVMAGIC	0x171	/* we32k with transfer vector */
#define RBOMAGIC	0x172	/* reserved */
#define WE32KBADMAG(x) (   ((x).f_magic != WE32KMAGIC) \
			&& ((x).f_magic != FBOMAGIC) \
			&& ((x).f_magic != RBOMAGIC) \
			&& ((x).f_magic != MTVMAGIC))

/* More names of "special" sections.  */
#define _TV	".tv"
#define _INIT	".init"
#define _FINI	".fini"

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
};

#define RELOC struct external_reloc
#define RELSZ 10

