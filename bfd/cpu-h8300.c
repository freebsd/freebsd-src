/* BFD library support routines for the Renesas H8/300 architecture.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
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

static bfd_boolean
h8300_scan (const struct bfd_arch_info *info, const char *string)
{
  if (*string != 'h' && *string != 'H')
    return FALSE;

  string++;
  if (*string != '8')
    return FALSE;

  string++;
  if (*string == '/')
    string++;

  if (*string != '3')
    return FALSE;
  string++;
  if (*string != '0')
    return FALSE;
  string++;
  if (*string != '0')
    return FALSE;
  string++;
  if (*string == '-')
    string++;

  /* In ELF linker scripts, we typically express the architecture/machine
     as architecture:machine.

     So if we've matched so far and encounter a colon, try to match the
     string following the colon.  */
  if (*string == ':')
    {
      string++;
      return h8300_scan (info, string);
    }

  if (*string == 'h' || *string == 'H')
    {
      string++;
      if (*string == 'n' || *string == 'N')
	return (info->mach == bfd_mach_h8300hn);

      return (info->mach == bfd_mach_h8300h);
    }
  else if (*string == 's' || *string == 'S')
    {
      string++;
      if (*string == 'n' || *string == 'N')
	return (info->mach == bfd_mach_h8300sn);

      if (*string == 'x' || *string == 'X')
	{
	  string++;
	  if (*string == 'n' || *string == 'N')
	    return (info->mach == bfd_mach_h8300sxn);

	  return (info->mach == bfd_mach_h8300sx);
	}
      
      return (info->mach == bfd_mach_h8300s);
    }
  else
    return info->mach == bfd_mach_h8300;
}

/* This routine is provided two arch_infos and works out the machine
   which would be compatible with both and returns a pointer to its
   info structure.  */

static const bfd_arch_info_type *
compatible (const bfd_arch_info_type *in, const bfd_arch_info_type *out)
{
  /* It's really not a good idea to mix and match modes.  */
  if (in->arch != out->arch || in->mach != out->mach)
    return 0;
  else
    return in;
}

static const bfd_arch_info_type h8300sxn_info_struct =
{
  32,				/* 32 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300sxn,
  "h8300sxn",			/* arch_name  */
  "h8300sxn",			/* printable name */
  1,
  FALSE,			/* the default machine */
  compatible,
  h8300_scan,
  0
};

static const bfd_arch_info_type h8300sx_info_struct =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300sx,
  "h8300sx",			/* arch_name  */
  "h8300sx",			/* printable name */
  1,
  FALSE,			/* the default machine */
  compatible,
  h8300_scan,
  &h8300sxn_info_struct
};

static const bfd_arch_info_type h8300sn_info_struct =
{
  32,				/* 32 bits in a word.  */
  16,				/* 16 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_h8300,
  bfd_mach_h8300sn,
  "h8300sn",			/* Architecture name.  */
  "h8300sn",			/* Printable name.  */
  1,
  FALSE,			/* The default machine.  */
  compatible,
  h8300_scan,
  &h8300sx_info_struct
};

static const bfd_arch_info_type h8300hn_info_struct =
{
  32,				/* 32 bits in a word.  */
  16,				/* 16 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_h8300,
  bfd_mach_h8300hn,
  "h8300hn",			/* Architecture name.  */
  "h8300hn",			/* Printable name.  */
  1,
  FALSE,			/* The default machine.  */
  compatible,
  h8300_scan,
  &h8300sn_info_struct
};

static const bfd_arch_info_type h8300s_info_struct =
{
  32,				/* 32 bits in a word.  */
  32,				/* 32 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_h8300,
  bfd_mach_h8300s,
  "h8300s",			/* Architecture name.  */
  "h8300s",			/* Printable name.  */
  1,
  FALSE,			/* The default machine.  */
  compatible,
  h8300_scan,
  & h8300hn_info_struct
};

static const bfd_arch_info_type h8300h_info_struct =
{
  32,				/* 32 bits in a word.  */
  32,				/* 32 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_h8300,
  bfd_mach_h8300h,
  "h8300h",			/* Architecture name.  */
  "h8300h",			/* Printable name.  */
  1,
  FALSE,			/* The default machine.  */
  compatible,
  h8300_scan,
  &h8300s_info_struct
};

const bfd_arch_info_type bfd_h8300_arch =
{
  16,				/* 16 bits in a word.  */
  16,				/* 16 bits in an address.  */
  8,				/* 8 bits in a byte.  */
  bfd_arch_h8300,
  bfd_mach_h8300,
  "h8300",			/* Architecture name.  */
  "h8300",			/* Printable name.  */
  1,
  TRUE,				/* The default machine.  */
  compatible,
  h8300_scan,
  &h8300h_info_struct
};

/* Pad the given address to 32 bits, converting 16-bit and 24-bit
   addresses into the values they would have had on a h8s target.  */

bfd_vma
bfd_h8300_pad_address (bfd *abfd, bfd_vma address)
{
  /* Cope with bfd_vma's larger than 32 bits.  */
  address &= 0xffffffffu;

  switch (bfd_get_mach (abfd))
    {
    case bfd_mach_h8300:
    case bfd_mach_h8300hn:
    case bfd_mach_h8300sn:
    case bfd_mach_h8300sxn:
      /* Sign extend a 16-bit address.  */
      if (address >= 0x8000)
	return address | 0xffff0000u;
      return address;

    case bfd_mach_h8300h:
      /* Sign extend a 24-bit address.  */
      if (address >= 0x800000)
	return address | 0xff000000u;
      return address;

    case bfd_mach_h8300s:
    case bfd_mach_h8300sx:
      return address;

    default:
      abort ();
    }
}
