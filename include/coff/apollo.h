/* coff information for Apollo M68K

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define DO_NOT_DEFINE_AOUTHDR
#define L_LNNO_SIZE 2
#include "coff/external.h"

/* Motorola 68000/68008/68010/68020 */
#define	MC68MAGIC	0520
#define MC68KWRMAGIC	0520	/* writeable text segments */
#define	MC68TVMAGIC	0521
#define MC68KROMAGIC	0521	/* readonly shareable text segments */
#define MC68KPGMAGIC	0522	/* demand paged text segments */
#define	M68MAGIC	0210
#define	M68TVMAGIC	0211

/* Apollo 68000-based machines have a different magic number. This comes
 * from /usr/include/apollo/filehdr.h
 */
#define APOLLOM68KMAGIC 0627

#define OMAGIC M68MAGIC
#define M68KBADMAG(x) (((x).f_magic!=MC68MAGIC) && ((x).f_magic!=MC68KWRMAGIC) && ((x).f_magic!=MC68TVMAGIC) && \
  ((x).f_magic!=MC68KROMAGIC) && ((x).f_magic!=MC68KPGMAGIC) && ((x).f_magic!=M68MAGIC) && ((x).f_magic!=M68TVMAGIC)  && \
  ((x).f_magic!=APOLLOM68KMAGIC) )

/********************** AOUT "OPTIONAL HEADER" **********************/

typedef struct 
{
  char 	magic[2];		/* type of file				*/
  char	vstamp[2];		/* version stamp			*/
  char	tsize[4];		/* text size in bytes, padded to FW bdry*/
  char	dsize[4];		/* initialized data "  "		*/
  char	bsize[4];		/* uninitialized data "   "		*/
  char	entry[4];		/* entry pt.				*/
  char 	text_start[4];		/* base of text used for this file */
  char 	data_start[4];		/* base of data used for this file */
  char	o_sri[4];		/* Apollo specific - .sri data pointer */
  char  o_inlib[4];		/* Apollo specific - .inlib data pointer */
  char	vid[8];			/* Apollo specific - 64 bit version ID */
}
AOUTHDR;

#define	APOLLO_COFF_VERSION_NUMBER 1 /* the value of the aouthdr magic */
#define	AOUTHDRSZ 44
#define AOUTSZ 44

/* Apollo allowa for larger section names by allowing
   them to be in the string table.  */

/* If s_zeores is all zeroes, s_offset gives the real
   location of the name in the string table.  */

#define	s_zeroes section_name.s_name
#define	s_offset (section_name.s_name+4)

/* More names of "special" sections.  */
#define _TV	".tv"
#define _INIT	".init"
#define _FINI	".fini"
#define	_LINES	".lines"
#define	_BLOCKS	".blocks"
#define _SRI    ".sri"                  /* Static Resource Information (systype,
 et al.) */
#define _MIR    ".mir"                  /* Module Information Records  */
#define _APTV   ".aptv"                 /* Apollo-style transfer vectors. */
#define _INLIB  ".inlib"                /* Shared Library information */
#define _RWDI   ".rwdi"         /* Read/write data initialization directives for
 compressed sections */
#define _UNWIND ".unwind"               /* Stack unwind information */

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
#ifdef M68K_COFF_OFFSET
  char r_offset[4];
#endif

};

#define RELOC struct external_reloc

#ifdef M68K_COFF_OFFSET
#define RELSZ 14
#else
#define RELSZ 10
#endif

/* Apollo specific STYP flags */

#define STYP_RELOCATED_NOT_LOADED 0x00010000	/* Section is relocated normally during linking, but need
                                            	   not be loaded during program execution */
#define STYP_DEBUG              0x00020000	/* debug section */
#define STYP_OVERLAY		0x00040000	/* Section is overlayed */
#define STYP_INSTRUCTION    	0x00200000	/* Section contains executable code */

#define STYP_ZERO		0x00800000	/* Section is initialized to zero */
#define STYP_INSTALLED		0x02000000	/* Section should be installable in KGT */
#define STYP_LOOK_INSTALLED	0x04000000	/* Look for section in KGT */
#define STYP_SECALIGN1		0x08000000	/* Specially aligned section */
#define STYP_SECALIGN2		0x10000000	/*      "       "       "    */
#define STYP_COMPRESSED		0x20000000	/* No section data per se (s_scnptr = 0), but there are
						   initialization directives for it in .rwdi section
						   (used in conjunction with STYP_BSS) */
