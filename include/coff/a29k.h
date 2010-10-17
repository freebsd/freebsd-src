/* COFF spec for AMD 290*0 

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   
   Contributed by David Wood @ New York University. */
 
#ifndef AMD
# define AMD
#endif

#define L_LNNO_SIZE 2
#include "coff/external.h"

/*
** Magic numbers for Am29000 
**	(AT&T will assign the "real" magic number)  
*/

#define SIPFBOMAGIC     0572    /* Am29000 (Byte 0 is MSB) */
#define SIPRBOMAGIC     0573    /* Am29000 (Byte 0 is LSB) */

#define A29K_MAGIC_BIG 		SIPFBOMAGIC	
#define A29K_MAGIC_LITTLE	SIPRBOMAGIC	
#define A29KBADMAG(x) 	( ((x).f_magic != A29K_MAGIC_BIG) && \
			  ((x).f_magic != A29K_MAGIC_LITTLE))

#define OMAGIC A29K_MAGIC_BIG
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** File header flags currently known to us.
**
** Am29000 will use the F_AR32WR and F_AR32W flags to indicate
** the byte ordering in the file.
*/

/*--------------------------------------------------------------*/


/* aouthdr magic numbers */
#define NMAGIC		0410	/* separate i/d executable */
#define SHMAGIC	0406		/* NYU/Ultra3 shared data executable 
				   (writable text) */
#undef  _ETEXT
#define _ETEXT   	"_etext"

/*--------------------------------------------------------------*/


/* More names of "special" sections.  */
#define _LIT	".lit"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Section types - with additional section type for global 
** registers which will be relocatable for the Am29000.
**
** In instances where it is necessary for a linker to produce an
** output file which contains text or data not based at virtual
** address 0, e.g. for a ROM, then the linker should accept
** address base information as command input and use PAD sections
** to skip over unused addresses.
*/

#define	STYP_BSSREG	0x1200	/* Global register area (like STYP_INFO) */
#define STYP_ENVIR	0x2200	/* Environment (like STYP_INFO) */
#define STYP_ABS	0x4000	/* Absolute (allocated, not reloc, loaded) */

/*--------------------------------------------------------------*/

/*
** Relocation information declaration and related definitions
*/

struct external_reloc
{
  char r_vaddr[4];	/* (virtual) address of reference */
  char r_symndx[4];	/* index into symbol table */
  char r_type[2];	/* relocation type */
};

#define	RELOC		struct external_reloc
#define	RELSZ		10		/* sizeof (RELOC) */ 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Relocation types for the Am29000 
*/

#define	R_ABS		0	/* reference is absolute */
 
#define	R_IREL		030	/* instruction relative (jmp/call) */
#define	R_IABS		031	/* instruction absolute (jmp/call) */
#define	R_ILOHALF	032	/* instruction low half  (const)  */
#define	R_IHIHALF	033	/* instruction high half (consth) part 1 */
#define	R_IHCONST	034	/* instruction high half (consth) part 2 */
				/* constant offset of R_IHIHALF relocation */
#define	R_BYTE		035	/* relocatable byte value */
#define R_HWORD		036	/* relocatable halfword value */
#define R_WORD		037	/* relocatable word value */

#define	R_IGLBLRC	040	/* instruction global register RC */
#define	R_IGLBLRA	041	/* instruction global register RA */
#define	R_IGLBLRB	042	/* instruction global register RB */
 
/*
NOTE:
All the "I" forms refer to 29000 instruction formats.  The linker is 
expected to know how the numeric information is split and/or aligned
within the instruction word(s).  R_BYTE works for instructions, too.

If the parameter to a CONSTH instruction is a relocatable type, two 
relocation records are written.  The first has an r_type of R_IHIHALF 
(33 octal) and a normal r_vaddr and r_symndx.  The second relocation 
record has an r_type of R_IHCONST (34 octal), a normal r_vaddr (which 
is redundant), and an r_symndx containing the 32-bit constant offset 
to the relocation instead of the actual symbol table index.  This 
second record is always written, even if the constant offset is zero.
The constant fields of the instruction are set to zero.
*/

/*--------------------------------------------------------------*/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Storage class definitions - new classes for global registers.
*/

#define C_GLBLREG	19		/* global register */
#define C_EXTREG	20		/* external global register */
#define	C_DEFREG	21		/* ext. def. of global register */
