/* coff information for M68K
   
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

#ifndef GNU_COFF_M68K_H
#define GNU_COFF_M68K_H 1

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

/* This is the magic of the Bull dpx/2 */
#define MC68KBCSMAGIC	0526

/* This is Lynx's all-platform magic number for executables. */

#define LYNXCOFFMAGIC	0415

#define OMAGIC M68MAGIC

/* This intentionally does not include MC68KBCSMAGIC; it only includes
   magic numbers which imply that names do not have underscores.  */
#define M68KBADMAG(x) (((x).f_magic != MC68MAGIC) \
                    && ((x).f_magic != MC68KWRMAGIC) \
                    && ((x).f_magic != MC68TVMAGIC) \
                    && ((x).f_magic != MC68KROMAGIC) \
                    && ((x).f_magic != MC68KPGMAGIC) \
                    && ((x).f_magic != M68MAGIC) \
                    && ((x).f_magic != M68TVMAGIC) \
                    && ((x).f_magic != LYNXCOFFMAGIC))

/* Magic numbers for the a.out header.  */

#define PAGEMAGICEXECSWAPPED  0407 /* executable (swapped) */
#define PAGEMAGICPEXECSWAPPED 0410 /* pure executable (swapped) */
#define PAGEMAGICPEXECTSHLIB  0443 /* pure executable (target shared library) */
#define PAGEMAGICPEXECPAGED   0413 /* pure executable (paged) */

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

#endif /* GNU_COFF_M68K_H */
