/* coff information for TI TMS320C80 (MVP)
   
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

#define DO_NOT_DEFINE_FILHDR
#define DO_NOT_DEFINE_SCNHDR
#define L_LNNO_SIZE 2
#include "coff/external.h"

/********************** FILE HEADER **********************/

struct external_filehdr
  {
    char f_magic[2];	/* magic number			*/
    char f_nscns[2];	/* number of sections		*/
    char f_timdat[4];	/* time & date stamp		*/
    char f_symptr[4];	/* file pointer to symtab	*/
    char f_nsyms[4];	/* number of symtab entries	*/
    char f_opthdr[2];	/* sizeof(optional hdr)		*/
    char f_flags[2];	/* flags			*/
    char f_target_id[2];/* target id (TIc80 specific)	*/
};

#define	TIC80_ARCH_MAGIC	0x0C1	/* Goes in the file header magic number field */
#define TIC80_TARGET_ID		0x95	/* Goes in the target id field */

#define TIC80BADMAG(x) ((x).f_magic != TIC80_ARCH_MAGIC)

#define	FILHDR	struct external_filehdr
#define	FILHSZ	22

#define TIC80_AOUTHDR_MAGIC	0x108	/* Goes in the optional file header magic number field */

/********************** SECTION HEADER **********************/

struct external_scnhdr
{
	char		s_name[8];	/* section name			*/
	char		s_paddr[4];	/* physical address, aliased s_nlib */
	char		s_vaddr[4];	/* virtual address		*/
	char		s_size[4];	/* section size			*/
	char		s_scnptr[4];	/* file ptr to raw data for section */
	char		s_relptr[4];	/* file ptr to relocation	*/
	char		s_lnnoptr[4];	/* file ptr to line numbers	*/
	char		s_nreloc[2];	/* number of relocation entries	*/
	char		s_nlnno[2];	/* number of line number entries*/
	char		s_flags[2];	/* flags			*/
	char		s_reserved[1];	/* reserved (TIc80 specific)	*/
	char		s_mempage[1];	/* memory page number (TIc80)	*/
};

/* Names of "special" sections.  */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _CINIT	".cinit"
#define _CONST	".const"
#define _SWITCH	".switch"
#define _STACK	".stack"
#define _SYSMEM	".sysmem"

#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	40
  
/* FIXME - need to correlate external_auxent with
   TIc80 Code Generation Tools User's Guide, CG:A-25 */

/********************** RELOCATION DIRECTIVES **********************/

/* The external reloc has an offset field, because some of the reloc
   types on the h8 don't have room in the instruction for the entire
   offset - eg the strange jump and high page addressing modes.  */

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_reserved[2];
  char r_type[2];
};

#define RELOC struct external_reloc
#define RELSZ 12

/* TIc80 relocation types. */

#define R_ABS		0x00		/* Absolute address - no relocation */
#define R_RELLONGX	0x11		/* PP: 32 bits, direct */
#define R_PPBASE	0x34		/* PP: Global base address type */
#define R_PPLBASE	0x35		/* PP: Local base address type */
#define R_PP15		0x38		/* PP: Global 15 bit offset */
#define R_PP15W		0x39		/* PP: Global 15 bit offset divided by 4 */
#define R_PP15H		0x3A		/* PP: Global 15 bit offset divided by 2 */
#define R_PP16B		0x3B		/* PP: Global 16 bit offset for bytes */
#define R_PPL15		0x3C		/* PP: Local 15 bit offset */
#define R_PPL15W	0x3D		/* PP: Local 15 bit offset divided by 4 */
#define R_PPL15H	0x3E		/* PP: Local 15 bit offset divided by 2 */
#define R_PPL16B	0x3F		/* PP: Local 16 bit offset for bytes */
#define R_PPN15		0x40		/* PP: Global 15 bit negative offset */
#define R_PPN15W	0x41		/* PP: Global 15 bit negative offset divided by 4 */
#define R_PPN15H	0x42		/* PP: Global 15 bit negative offset divided by 2 */
#define R_PPN16B	0x43		/* PP: Global 16 bit negative byte offset */
#define R_PPLN15	0x44		/* PP: Local 15 bit negative offset */
#define R_PPLN15W	0x45		/* PP: Local 15 bit negative offset divided by 4 */
#define R_PPLN15H	0x46		/* PP: Local 15 bit negative offset divided by 2 */
#define R_PPLN16B	0x47		/* PP: Local 16 bit negative byte offset */
#define R_MPPCR15W	0x4E		/* MP: 15 bit PC-relative divided by 4 */
#define R_MPPCR		0x4F		/* MP: 32 bit PC-relative divided by 4 */
