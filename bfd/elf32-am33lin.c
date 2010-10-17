/* Matsushita AM33/2.0 support for 32-bit GNU/Linux ELF
   Copyright 2003
   Free Software Foundation, Inc.

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

#define elf_symbol_leading_char 0

#define TARGET_LITTLE_SYM	bfd_elf32_am33lin_vec
#define TARGET_LITTLE_NAME	"elf32-am33lin"
#define ELF_ARCH		bfd_arch_mn10300
#define ELF_MACHINE_CODE	EM_MN10300
#define ELF_MACHINE_ALT1	EM_CYGNUS_MN10300
#define ELF_MAXPAGESIZE		0x1000

/* Rename global functions.  */
#define _bfd_mn10300_elf_merge_private_bfd_data  _bfd_am33_elf_merge_private_bfd_data
#define _bfd_mn10300_elf_object_p                _bfd_am33_elf_object_p
#define _bfd_mn10300_elf_final_write_processing  _bfd_am33_elf_final_write_processing

#include "elf-m10300.c"
