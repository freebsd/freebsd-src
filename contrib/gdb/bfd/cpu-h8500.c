/* BFD library support routines for the H8/500 architecture.
   Copyright (C) 1993 Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#if 0

/* 
Relocations for the Z8K

*/
static bfd_reloc_status_type 
howto16_callback (abfd, reloc_entry, symbol_in, data,
		  ignore_input_section, ignore_bfd)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection *ignore_input_section;
     bfd *ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_16(abfd, (bfd_byte *)data + addr);

  HOWTO_PREPARE(relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_16(abfd, x, (bfd_byte *)data + addr);
  return bfd_reloc_ok;
}


static bfd_reloc_status_type 
howto8_callback (abfd, reloc_entry, symbol_in, data,
		 ignore_input_section, ignore_bfd)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection *ignore_input_section;
     bfd *ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_8(abfd, (bfd_byte *)data + addr);

  HOWTO_PREPARE(relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8(abfd, x, (bfd_byte *)data + addr);
  return bfd_reloc_ok;
}


static bfd_reloc_status_type 
howto8_FFnn_callback (abfd, reloc_entry, symbol_in, data,
		      ignore_input_section, ignore_bfd)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection *ignore_input_section;
     bfd *ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;

  long x = bfd_get_8(abfd, (bfd_byte *)data + addr);
  abort();
  HOWTO_PREPARE(relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8(abfd, x, (bfd_byte *)data + addr);
  return bfd_reloc_ok;
}

static bfd_reloc_status_type 
howto8_pcrel_callback (abfd, reloc_entry, symbol_in, data,
		       ignore_input_section, ignore_bfd)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection *ignore_input_section;
     bfd *ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_8(abfd, (bfd_byte *)data + addr);
  abort();
  HOWTO_PREPARE(relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8(abfd, x, (bfd_byte *)data + addr);
  return bfd_reloc_ok;
}



static reloc_howto_type howto_16
  = NEWHOWTO(howto16_callback,"abs16",1,false,false);
static reloc_howto_type howto_8
  = NEWHOWTO(howto8_callback,"abs8",0,false,false);

static reloc_howto_type howto_8_FFnn
  = NEWHOWTO(howto8_FFnn_callback,"ff00+abs8",0,false,false);

static reloc_howto_type howto_8_pcrel
  = NEWHOWTO(howto8_pcrel_callback,"pcrel8",0,false,true);


static reloc_howto_type *
local_bfd_reloc_type_lookup (arch, code)
     const struct bfd_arch_info *arch;
     bfd_reloc_code_real_type code;
{
  switch (code) {
  case BFD_RELOC_16:
    return &howto_16;
  case BFD_RELOC_8_FFnn:
    return &howto_8_FFnn;
  case BFD_RELOC_8:
    return &howto_8;
  case BFD_RELOC_8_PCREL:
    return &howto_8_pcrel;
  }
  return (reloc_howto_type *)NULL;
}
#endif

int bfd_default_scan_num_mach();

static boolean 
scan_mach (info, string)
     const struct bfd_arch_info *info;
     const char *string;
{
  if (strcmp(string,"h8/500") == 0) return true;
  if (strcmp(string,"H8/500") == 0) return true;
  if (strcmp(string,"h8500") == 0) return true;
  if (strcmp(string,"H8500") == 0) return true;
  return false;
}


#if 0 /* not used currently */
/* This routine is provided two arch_infos and returns whether
   they'd be compatible */

static const bfd_arch_info_type *
compatible (a,b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  if (a->arch != b->arch || a->mach != b->mach)
   return NULL;
  return a;
}
#endif

const bfd_arch_info_type bfd_h8500_arch =
{
  16,				/* 16 bits in a word */
  24,				/* 24 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8500,
  0,				/* only 1 machine */
  "h8500",			/* arch_name  */
  "h8500",			/* printable name */
  1,
  true,				/* the default machine */
  bfd_default_compatible,
  scan_mach,
  0,
};
