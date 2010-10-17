/* coff information for 88k bcs
   
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

#define DO_NOT_DEFINE_SCNHDR
#define L_LNNO_SIZE 4
#define DO_NOT_DEFINE_SYMENT
#define DO_NOT_DEFINE_AUXENT
#include "coff/external.h"

#define MC88MAGIC  0540           /* 88k BCS executable */
#define MC88DMAGIC 0541           /* DG/UX executable   */
#define MC88OMAGIC 0555	          /* Object file        */

#define MC88BADMAG(x) (((x).f_magic != MC88MAGIC) \
                    && ((x).f_magic != MC88DMAGIC) \
                    && ((x).f_magic != MC88OMAGIC))

#define PAGEMAGIC3   0414 /* Split i&d, zero mapped */
#define PAGEMAGICBCS 0413

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
  char		s_nreloc[4];	/* number of relocation entries	*/
  char		s_nlnno[4];	/* number of line number entries*/
  char		s_flags[4];	/* flags			*/
};

#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	44

/* Names of "special" sections.  */
#define _TEXT   ".text"
#define _DATA   ".data"
#define _BSS    ".bss"
#define _COMMENT ".comment"


/********************** SYMBOLS **********************/

#define E_SYMNMLEN	8	/* # characters in a symbol name	*/
#define E_FILNMLEN	14	/* # characters in a file name		*/
#define E_DIMNUM	4	/* # array dimensions in auxiliary entry */

struct external_syment 
{
  union
  {
    char e_name[E_SYMNMLEN];

    struct
    {
      char e_zeroes[4];
      char e_offset[4];
    } e;

  } e;

  char e_value[4];
  char e_scnum[2];
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
  char pad2[2];
};

#define N_BTMASK	017
#define N_TMASK		060
#define N_BTSHFT	4
#define N_TSHIFT	2

/* Note that this isn't the same shape as other coffs */
union external_auxent
{
  struct
  {
    char x_tagndx[4];		/* str, un, or enum tag indx */
    /* 4 */
    
    union
    {
      char x_fsize[4];		/* size of function */

      struct
      {
	char  x_lnno[4];	/* declaration line number */
	char  x_size[4];	/* str/union/array size */
      } x_lnsz;

    } x_misc;
    
    /* 12 */
    union
    {
      struct 			/* if ISFCN, tag, or .bb */
      {
	char x_lnnoptr[4];	/* ptr to fcn line # */
	char x_endndx[4];		/* entry ndx past block end */
      } x_fcn;

      struct 			/* if ISARY, up to 4 dimen. */
      {
	char x_dimen[E_DIMNUM][2];
      } x_ary;

    } x_fcnary;
    /* 20 */
    
  } x_sym;
  
  union
  {
    char x_fname[E_FILNMLEN];

    struct
    {
      char x_zeroes[4];
      char x_offset[4];
    } x_n;

  } x_file;
  
  struct
  {
    char x_scnlen[4];		/* section length */
    char x_nreloc[4];		/* # relocation entries */
    char x_nlinno[4];		/* # line numbers */
  } x_scn;
  
  struct
  {
    char x_tvfill[4];		/* tv fill value */
    char x_tvlen[2];		/* length of .tv */
    char x_tvran[2][2];		/* tv range */
  } x_tv;			/* info about .tv section (in auxent of symbol .tv)) */
};

#define GET_LNSZ_SIZE(abfd, ext) \
  H_GET_32 (abfd, ext->x_sym.x_misc.x_lnsz.x_size)
#define GET_LNSZ_LNNO(abfd, ext) \
  H_GET_32 (abfd, ext->x_sym.x_misc.x_lnsz.x_lnno)
#define PUT_LNSZ_LNNO(abfd, in, ext) \
  H_PUT_32 (abfd, in, ext->x_sym.x_misc.x_lnsz.x_lnno)
#define PUT_LNSZ_SIZE(abfd, in, ext) \
  H_PUT_32 (abfd, in, ext->x_sym.x_misc.x_lnsz.x_size)
#define GET_SCN_NRELOC(abfd, ext) \
  H_GET_32 (abfd, ext->x_scn.x_nreloc)
#define GET_SCN_NLINNO(abfd, ext) \
  H_GET_32 (abfd, ext->x_scn.x_nlinno)
#define PUT_SCN_NRELOC(abfd, in, ext) \
  H_PUT_32 (abfd, in, ext->x_scn.x_nreloc)
#define PUT_SCN_NLINNO(abfd, in, ext) \
  H_PUT_32 (abfd,in, ext->x_scn.x_nlinno)

#define	SYMENT	struct external_syment
#define	SYMESZ	20
#define	AUXENT	union external_auxent
#define	AUXESZ	20

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
  char r_offset[2];
};

#define RELOC struct external_reloc
#define RELSZ  12

#define NO_TVNDX
