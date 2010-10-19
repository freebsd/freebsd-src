/* COFF information for PC running go32.

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

#define STUBSIZE 2048

struct external_filehdr_go32_exe
  {
    char stub[STUBSIZE];/* the stub to load the image	*/
			/* the standard COFF header     */
    char f_magic[2];	/* magic number			*/
    char f_nscns[2];	/* number of sections		*/
    char f_timdat[4];	/* time & date stamp		*/
    char f_symptr[4];	/* file pointer to symtab	*/
    char f_nsyms[4];	/* number of symtab entries	*/
    char f_opthdr[2];	/* sizeof(optional hdr)		*/
    char f_flags[2];	/* flags			*/
  };

#undef FILHDR
#define	FILHDR	struct external_filehdr_go32_exe
#undef FILHSZ
#define	FILHSZ	STUBSIZE+20
