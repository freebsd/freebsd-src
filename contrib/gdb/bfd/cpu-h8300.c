/* BFD library support routines for the Hitachi H8/300 architecture.
   Copyright (C) 1990, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.
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

#if 0 /* not used currently */
/*
Relocations for the H8

*/
static bfd_reloc_status_type
howto16_callback (abfd, reloc_entry, symbol_in, data,
		  ignore_input_section, ignore_bfd)
     bfd * abfd;
     arelent * reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection * ignore_input_section;
     bfd * ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_16 (abfd, (bfd_byte *) data + addr);

  HOWTO_PREPARE (relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_16 (abfd, x, (bfd_byte *) data + addr);
  return bfd_reloc_ok;
}


static bfd_reloc_status_type
howto8_callback (abfd, reloc_entry, symbol_in, data,
		 ignore_input_section, ignore_bfd)
     bfd * abfd;
     arelent * reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection * ignore_input_section;
     bfd * ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_8 (abfd, (bfd_byte *) data + addr);

  HOWTO_PREPARE (relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8 (abfd, x, (bfd_byte *) data + addr);
  return bfd_reloc_ok;
}


static bfd_reloc_status_type
howto8_FFnn_callback (abfd, reloc_entry, symbol_in, data,
		      ignore_input_section, ignore_bfd)
     bfd * abfd;
     arelent * reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection * ignore_input_section;
     bfd * ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;

  long x = bfd_get_8 (abfd, (bfd_byte *) data + addr);
  abort ();
  HOWTO_PREPARE (relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8 (abfd, x, (bfd_byte *) data + addr);
  return bfd_reloc_ok;
}

static bfd_reloc_status_type
howto8_pcrel_callback (abfd, reloc_entry, symbol_in, data,
		       ignore_input_section, ignore_bfd)
     bfd * abfd;
     arelent * reloc_entry;
     struct symbol_cache_entry *symbol_in;
     PTR data;
     asection * ignore_input_section;
     bfd * ignore_bfd;
{
  long relocation = 0;
  bfd_vma addr = reloc_entry->address;
  long x = bfd_get_8 (abfd, (bfd_byte *) data + addr);
  abort ();
  HOWTO_PREPARE (relocation, symbol_in);

  x = (x + relocation + reloc_entry->addend);

  bfd_put_8 (abfd, x, (bfd_byte *) data + addr);
  return bfd_reloc_ok;
}

static reloc_howto_type howto_16
= NEWHOWTO (howto16_callback, "abs16", 1, false, false);
static reloc_howto_type howto_8
= NEWHOWTO (howto8_callback, "abs8", 0, false, false);

static reloc_howto_type howto_8_FFnn
= NEWHOWTO (howto8_FFnn_callback, "ff00+abs8", 0, false, false);

static reloc_howto_type howto_8_pcrel
= NEWHOWTO (howto8_pcrel_callback, "pcrel8", 0, false, true);

static reloc_howto_type *
local_bfd_reloc_type_lookup (arch, code)
     const struct bfd_arch_info *arch;
     bfd_reloc_code_real_type code;
{
    switch (code)
      {
      case BFD_RELOC_16:
	return &howto_16;
      case BFD_RELOC_8_FFnn:
	return &howto_8_FFnn;
      case BFD_RELOC_8:
	return &howto_8;
      case BFD_RELOC_8_PCREL:
	return &howto_8_pcrel;
      default:
	return (reloc_howto_type *) NULL;
      }
  }
#endif

int bfd_default_scan_num_mach ();

static boolean
h8300_scan (info, string)
     const struct bfd_arch_info *info;
     const char *string;
{
  if (*string != 'h' && *string != 'H')
    return false;

  string++;
  if (*string != '8')
    return false;

  string++;
  if (*string == '/')
    string++;

  if (*string != '3')
    return false;
  string++;
  if (*string != '0')
    return false;
  string++;
  if (*string != '0')
    return false;
  string++;
  if (*string == '-')
    string++;
  if (*string == 'h' || *string == 'H')
    {
      return (info->mach == bfd_mach_h8300h);
    }
  else
    {
      return info->mach == bfd_mach_h8300;
    }
}


/* This routine is provided two arch_infos and works out the 
   machine which would be compatible with both and returns a pointer
   to its info structure */

static const bfd_arch_info_type *
compatible (in, out)
     const bfd_arch_info_type * in;
     const bfd_arch_info_type * out;
{
  /* If the output is non-H and the input is -H, that's bad */
  if (in->mach == bfd_mach_h8300h &&
      out->mach == bfd_mach_h8300)
    return 0;

  /* If either is an -H, the answer is -H */
  if (in->mach == bfd_mach_h8300h)
    return in;
  return out;
}

static const bfd_arch_info_type h8300_info_struct =
{
  16,				/* 16 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300,
  "h8300",			/* arch_name  */
  "h8300",			/* printable name */
  1,
  true,				/* the default machine */
  compatible,
  h8300_scan,
/*    local_bfd_reloc_type_lookup, */
  0,
};


const bfd_arch_info_type bfd_h8300_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300h,
  "h8300h",			/* arch_name  */
  "h8300h",			/* printable name */
  1,
  false,			/* the default machine */
  compatible,
  h8300_scan,
/*    local_bfd_reloc_type_lookup, */
  &h8300_info_struct,
};
