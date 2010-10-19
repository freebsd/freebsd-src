/* coff information for Renesas H8/300 and H8/300-H

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define L_LNNO_SIZE 4
#include "coff/external.h"

#define	H8300MAGIC	0x8300
#define	H8300HMAGIC	0x8301
#define	H8300SMAGIC	0x8302
#define	H8300HNMAGIC	0x8303
#define	H8300SNMAGIC	0x8304

#define H8300BADMAG(x)   (((x).f_magic != H8300MAGIC))
#define H8300HBADMAG(x)  (((x).f_magic != H8300HMAGIC))
#define H8300SBADMAG(x)  (((x).f_magic != H8300SMAGIC))
#define H8300HNBADMAG(x) (((x).f_magic != H8300HNMAGIC))
#define H8300SNBADMAG(x) (((x).f_magic != H8300SNMAGIC))

/* Relocation directives.  */

/* The external reloc has an offset field, because some of the reloc
   types on the h8 don't have room in the instruction for the entire
   offset - eg the strange jump and high page addressing modes.  */

struct external_reloc
{
  char r_vaddr[4];
  char r_symndx[4];
  char r_offset[4];
  char r_type[2];
  char r_stuff[2];
};

#define RELOC struct external_reloc
#define RELSZ 16




