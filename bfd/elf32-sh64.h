/* SH ELF support for BFD.
   Copyright 2003 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef ELF32_SH64_H
#define ELF32_SH64_H

#define SH64_CRANGES_SECTION_NAME ".cranges"
enum sh64_elf_cr_type {
  CRT_NONE = 0,
  CRT_DATA = 1,
  CRT_SH5_ISA16 = 2,
  CRT_SH5_ISA32 = 3
};

/* The official definition is this:

    typedef struct {
      Elf32_Addr cr_addr;
      Elf32_Word cr_size;
      Elf32_Half cr_type;
    } Elf32_CRange;

   but we have no use for that exact type.  Instead we use this struct for
   the internal representation.  */
typedef struct {
  bfd_vma cr_addr;
  bfd_size_type cr_size;
  enum sh64_elf_cr_type cr_type;
} sh64_elf_crange;

#define SH64_CRANGE_SIZE (4 + 4 + 2)
#define SH64_CRANGE_CR_ADDR_OFFSET 0
#define SH64_CRANGE_CR_SIZE_OFFSET 4
#define SH64_CRANGE_CR_TYPE_OFFSET (4 + 4)

/* Get the contents type of an arbitrary address, or return CRT_NONE.  */
extern enum sh64_elf_cr_type sh64_get_contents_type
  (asection *, bfd_vma, sh64_elf_crange *);

/* Simpler interface.
   FIXME: This seems redundant now that we export the interface above.  */
extern bfd_boolean sh64_address_is_shmedia
  (asection *, bfd_vma);

extern int _bfd_sh64_crange_qsort_cmpb
  (const void *, const void *);
extern int _bfd_sh64_crange_qsort_cmpl
  (const void *, const void *);
extern int _bfd_sh64_crange_bsearch_cmpb
  (const void *, const void *);
extern int _bfd_sh64_crange_bsearch_cmpl
  (const void *, const void *);

struct sh64_section_data
{
  flagword contents_flags;

  /* Only used in the cranges section, but we don't have an official
     backend-specific bfd field.  */
  bfd_size_type cranges_growth;
};

struct _sh64_elf_section_data
{
  struct bfd_elf_section_data elf;
  struct sh64_section_data *sh64_info;
};

#define sh64_elf_section_data(sec) \
  ((struct _sh64_elf_section_data *) elf_section_data (sec))

#endif /* ELF32_SH64_H */
