/* SuperH SH64-specific support for 32-bit ELF
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#define SH64_ELF

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/sh.h"
#include "elf32-sh64.h"
#include "../opcodes/sh64-opc.h"

static bfd_boolean sh64_address_in_cranges
  (asection *cranges, bfd_vma, sh64_elf_crange *);

/* Ordering functions of a crange, for the qsort and bsearch calls and for
   different endianness.  */

int
_bfd_sh64_crange_qsort_cmpb (const void *p1, const void *p2)
{
  bfd_vma a1 = bfd_getb32 (p1);
  bfd_vma a2 = bfd_getb32 (p2);

  /* Preserve order if there's ambiguous contents.  */
  if (a1 == a2)
    return (char *) p1 - (char *) p2;

  return a1 - a2;
}

int
_bfd_sh64_crange_qsort_cmpl (const void *p1, const void *p2)
{
  bfd_vma a1 = (bfd_vma) bfd_getl32 (p1);
  bfd_vma a2 = (bfd_vma) bfd_getl32 (p2);

  /* Preserve order if there's ambiguous contents.  */
  if (a1 == a2)
    return (char *) p1 - (char *) p2;

  return a1 - a2;
}

int
_bfd_sh64_crange_bsearch_cmpb (const void *p1, const void *p2)
{
  bfd_vma a1 = *(bfd_vma *) p1;
  bfd_vma a2 = (bfd_vma) bfd_getb32 (p2);
  bfd_size_type size
    = (bfd_size_type) bfd_getb32 (SH64_CRANGE_CR_SIZE_OFFSET + (char *) p2);

  if (a1 >= a2 + size)
    return 1;
  if (a1 < a2)
    return -1;
  return 0;
}

int
_bfd_sh64_crange_bsearch_cmpl (const void *p1, const void *p2)
{
  bfd_vma a1 = *(bfd_vma *) p1;
  bfd_vma a2 = (bfd_vma) bfd_getl32 (p2);
  bfd_size_type size
    = (bfd_size_type) bfd_getl32 (SH64_CRANGE_CR_SIZE_OFFSET + (char *) p2);

  if (a1 >= a2 + size)
    return 1;
  if (a1 < a2)
    return -1;
  return 0;
}

/* Check whether a specific address is specified within a .cranges
   section.  Return FALSE if not found, and TRUE if found, and the region
   filled into RANGEP if non-NULL.  */

static bfd_boolean
sh64_address_in_cranges (asection *cranges, bfd_vma addr,
			 sh64_elf_crange *rangep)
{
  bfd_byte *cranges_contents;
  bfd_byte *found_rangep;
  bfd_size_type cranges_size = bfd_section_size (cranges->owner, cranges);

  /* If the size is not a multiple of the cranges entry size, then
     something is badly wrong.  */
  if ((cranges_size % SH64_CRANGE_SIZE) != 0)
    return FALSE;

  /* If this section has relocations, then we can't do anything sane.  */
  if (bfd_get_section_flags (cranges->owner, cranges) & SEC_RELOC)
    return FALSE;

  /* Has some kind soul (or previous call) left processed, sorted contents
     for us?  */
  if ((bfd_get_section_flags (cranges->owner, cranges) & SEC_IN_MEMORY)
      && elf_section_data (cranges)->this_hdr.sh_type == SHT_SH5_CR_SORTED)
    cranges_contents = cranges->contents;
  else
    {
      cranges_contents
	= bfd_malloc (cranges->_cooked_size != 0
		      ? cranges->_cooked_size : cranges->_raw_size);
      if (cranges_contents == NULL)
	return FALSE;

      if (! bfd_get_section_contents (cranges->owner, cranges,
				      cranges_contents, (file_ptr) 0,
				      cranges_size))
	goto error_return;

      /* Is it sorted?  */
      if (elf_section_data (cranges)->this_hdr.sh_type
	  != SHT_SH5_CR_SORTED)
	/* Nope.  Lets sort it.  */
	qsort (cranges_contents, cranges_size / SH64_CRANGE_SIZE,
	       SH64_CRANGE_SIZE,
	       bfd_big_endian (cranges->owner)
	       ? _bfd_sh64_crange_qsort_cmpb : _bfd_sh64_crange_qsort_cmpl);

      /* Let's keep it around.  */
      cranges->contents = cranges_contents;
      bfd_set_section_flags (cranges->owner, cranges,
			     bfd_get_section_flags (cranges->owner, cranges)
			     | SEC_IN_MEMORY);

      /* It's sorted now.  */
      elf_section_data (cranges)->this_hdr.sh_type = SHT_SH5_CR_SORTED;
    }

  /* Try and find a matching range.  */
  found_rangep
    = bsearch (&addr, cranges_contents, cranges_size / SH64_CRANGE_SIZE,
	       SH64_CRANGE_SIZE,
	       bfd_big_endian (cranges->owner)
	       ? _bfd_sh64_crange_bsearch_cmpb
	       : _bfd_sh64_crange_bsearch_cmpl);

  /* Fill in a few return values if we found a matching range.  */
  if (found_rangep)
    {
      enum sh64_elf_cr_type cr_type
	= bfd_get_16 (cranges->owner,
		      SH64_CRANGE_CR_TYPE_OFFSET + found_rangep);
      bfd_vma cr_addr
	= bfd_get_32 (cranges->owner,
		      SH64_CRANGE_CR_ADDR_OFFSET
		      + (char *) found_rangep);
      bfd_size_type cr_size
	= bfd_get_32 (cranges->owner,
		      SH64_CRANGE_CR_SIZE_OFFSET
		      + (char *) found_rangep);

      rangep->cr_addr = cr_addr;
      rangep->cr_size = cr_size;
      rangep->cr_type = cr_type;

      return TRUE;
    }

  /* There is a .cranges section, but it does not have a descriptor
     matching this address.  */
  return FALSE;

error_return:
  free (cranges_contents);
  return FALSE;
}

/* Determine what ADDR points to in SEC, and fill in a range descriptor in
   *RANGEP if it's non-NULL.  */

enum sh64_elf_cr_type
sh64_get_contents_type (asection *sec, bfd_vma addr, sh64_elf_crange *rangep)
{
  asection *cranges;

  /* Fill in the range with the boundaries of the section as a default.  */
  if (bfd_get_flavour (sec->owner) == bfd_target_elf_flavour
      && elf_elfheader (sec->owner)->e_type == ET_EXEC)
    {
      rangep->cr_addr = bfd_get_section_vma (sec->owner, sec);
      rangep->cr_size = bfd_section_size (sec->owner, sec);
      rangep->cr_type = CRT_NONE;
    }
  else
    return FALSE;

  /* If none of the pertinent bits are set, then it's a SHcompact (or at
     least not SHmedia).  */
  if ((elf_section_data (sec)->this_hdr.sh_flags
       & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED)) == 0)
    {
      enum sh64_elf_cr_type cr_type
	= ((bfd_get_section_flags (sec->owner, sec) & SEC_CODE) != 0
	   ? CRT_SH5_ISA16 : CRT_DATA);
      rangep->cr_type = cr_type;
      return cr_type;
    }

  /* If only the SHF_SH5_ISA32 bit is set, then we have SHmedia.  */
  if ((elf_section_data (sec)->this_hdr.sh_flags
       & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED)) == SHF_SH5_ISA32)
    {
      rangep->cr_type = CRT_SH5_ISA32;
      return CRT_SH5_ISA32;
    }

  /* Otherwise, we have to look up the .cranges section.  */
  cranges = bfd_get_section_by_name (sec->owner, SH64_CRANGES_SECTION_NAME);

  if (cranges == NULL)
    /* A mixed section but there's no .cranges section.  This is probably
       bad input; it does not comply to specs.  */
    return CRT_NONE;

  /* If this call fails, we will still have CRT_NONE in rangep->cr_type
     and that will be suitable to return.  */
  sh64_address_in_cranges (cranges, addr, rangep);

  return rangep->cr_type;
}

/* This is a simpler exported interface for the benefit of gdb et al.  */

bfd_boolean
sh64_address_is_shmedia (asection *sec, bfd_vma addr)
{
  sh64_elf_crange dummy;
  return sh64_get_contents_type (sec, addr, &dummy) == CRT_SH5_ISA32;
}
