/* Header file for ns32k routines.
   Copyright 1996, 2001, 2002 Free Software Foundation, Inc.
   Written by Cygnus Support.

   This file is part of BFD, the Binary File Descriptor library.

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

extern bfd_reloc_status_type _bfd_ns32k_relocate_contents
  PARAMS ((reloc_howto_type *, bfd *, bfd_vma, bfd_byte *));

extern bfd_reloc_status_type _bfd_do_ns32k_reloc_contents
  PARAMS ((reloc_howto_type *, bfd *, bfd_vma, bfd_byte *,
	   bfd_vma (*) (bfd_byte *, int),
	   void (*) (bfd_vma, bfd_byte *, int)));

extern bfd_reloc_status_type _bfd_ns32k_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *, bfd_vma,
	   bfd_vma, bfd_vma));

extern bfd_vma _bfd_ns32k_get_displacement PARAMS ((bfd_byte *, int));
extern bfd_vma _bfd_ns32k_get_immediate PARAMS ((bfd_byte *, int));
extern void _bfd_ns32k_put_displacement PARAMS ((bfd_vma, bfd_byte *, int));
extern void _bfd_ns32k_put_immediate PARAMS ((bfd_vma, bfd_byte *, int));

extern bfd_reloc_status_type _bfd_ns32k_reloc_disp
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern bfd_reloc_status_type _bfd_ns32k_reloc_imm
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
