/* BFD internal declarations for Oasys file format handling.
   Copyright 1990, 1991, 1992, 1993, 1994, 1997, 2002
   Free Software Foundation, Inc.
   Scrawled by Steve Chamberlain of Cygnus Support.

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

typedef struct _oasys_symbol
{
  asymbol symbol;
} oasys_symbol_type;

typedef struct _oasys_reloc {
  arelent relent;
  struct _oasys_reloc *next;
  unsigned int symbol;
} oasys_reloc_type;


#define oasys_symbol(x) ((oasys_symbol_type *)(x))
#define oasys_per_section(x) ((oasys_per_section_type *)(x->used_by_bfd))

typedef struct _oasys_per_section
{
  asection *section;
  bfd_byte *data;
  bfd_vma offset;
  bfd_boolean had_vma;
  oasys_reloc_type **reloc_tail_ptr;
  bfd_vma pc;


  file_ptr current_pos;
  unsigned int current_byte;
  bfd_boolean initialized;
} oasys_per_section_type;

#define NSECTIONS 10

typedef struct _oasys_ar_obstack {
  file_ptr file_offset;
  bfd *abfd;
} oasys_ar_obstack_type;


typedef struct _oasys_module_info {
  file_ptr pos;
  unsigned int size;
  bfd *abfd;
  char *name;
} oasys_module_info_type;

typedef struct _oasys_ar_data {
  oasys_module_info_type *module;
  unsigned int module_count;
  unsigned int module_index;
} oasys_ar_data_type;

typedef struct _oasys_data {
  char *strings;
  asymbol *symbols;
  unsigned int symbol_string_length;
  asection *sections[OASYS_MAX_SEC_COUNT];
  file_ptr first_data_record;
} oasys_data_type;

#define OASYS_DATA(abfd)	((abfd)->tdata.oasys_obj_data)
#define OASYS_AR_DATA(abfd)	((abfd)->tdata.oasys_ar_data)

