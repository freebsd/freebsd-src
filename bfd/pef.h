/* PEF support for BFD.
   Copyright 1999, 2000, 2001, 2002
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"

#include <stdio.h>

struct bfd_pef_header
{
  unsigned long tag1;
  unsigned long tag2;
  unsigned long architecture;
  unsigned long format_version;
  unsigned long timestamp;
  unsigned long old_definition_version;
  unsigned long old_implementation_version;
  unsigned long current_version;
  unsigned short section_count;
  unsigned short instantiated_section_count;
  unsigned long reserved;
};
typedef struct bfd_pef_header bfd_pef_header;

struct bfd_pef_loader_header
{
  long main_section;
  unsigned long main_offset;
  long init_section;
  unsigned long init_offset;
  long term_section;
  unsigned long term_offset;
  unsigned long imported_library_count;
  unsigned long total_imported_symbol_count;
  unsigned long reloc_section_count;
  unsigned long reloc_instr_offset;
  unsigned long loader_strings_offset;
  unsigned long export_hash_offset;
  unsigned long export_hash_table_power;
  unsigned long exported_symbol_count;
};
typedef struct bfd_pef_loader_header bfd_pef_loader_header;

struct bfd_pef_imported_library
{
  unsigned long name_offset;
  unsigned long old_implementation_version;
  unsigned long current_version;
  unsigned long imported_symbol_count;
  unsigned long first_imported_symbol;
  unsigned char options;
  unsigned char reserved_a;
  unsigned short reserved_b;
};
typedef struct bfd_pef_imported_library bfd_pef_imported_library;

enum bfd_pef_imported_library_options
  {
    BFD_PEF_WEAK_IMPORT_LIB = 0x40,
    BFD_PEF_INIT_LIB_BEFORE = 0x80
  };

struct bfd_pef_imported_symbol
{
  unsigned char class;
  unsigned long name;
};
typedef struct bfd_pef_imported_symbol bfd_pef_imported_symbol;

enum bfd_pef_imported_symbol_class
  {
    BFD_PEF_CODE_SYMBOL = 0x00,
    BFD_PEF_DATA_SYMBOL = 0x01,
    BFD_PEF_TVECTOR_SYMBOL = 0x02,
    BFD_PEF_TOC_SYMBOL = 0x03,
    BFD_PEF_GLUE_SYMBOL = 0x04,
    BFD_PEF_UNDEFINED_SYMBOL = 0x0F,
    BFD_PEF_WEAK_IMPORT_SYMBOL_MASK = 0x80
  };

#define BFD_PEF_TAG1 0x4A6F7921 /* 'Joy!' */
#define BFD_PEF_TAG2 0x70656666 /* 'peff' */

#define BFD_PEF_VERSION 0x00000001

struct bfd_pef_section
{
  long name_offset;
  unsigned long header_offset;
  unsigned long default_address;
  unsigned long total_length;
  unsigned long unpacked_length;
  unsigned long container_length;
  unsigned long container_offset;
  unsigned char section_kind;
  unsigned char share_kind;
  unsigned char alignment;
  unsigned char reserved;
  asection *bfd_section;
};
typedef struct bfd_pef_section bfd_pef_section;

#define BFD_PEF_SECTION_CODE 0
#define BFD_PEF_SECTION_UNPACKED_DATA 1
#define BFD_PEF_SECTION_PACKED_DATA 2
#define BFD_PEF_SECTION_CONSTANT 3
#define BFD_PEF_SECTION_LOADER 4
#define BFD_PEF_SECTION_DEBUG 5
#define BFD_PEF_SECTION_EXEC_DATA 6
#define BFD_PEF_SECTION_EXCEPTION 7
#define BFD_PEF_SECTION_TRACEBACK 8

#define BFD_PEF_SHARE_PROCESS 1
#define BFD_PEF_SHARE_GLOBAL 4
#define BFD_PEF_SHARE_PROTECTED 5

struct bfd_pef_data_struct
{
  bfd_pef_header header;
  bfd_pef_section *sections;
  bfd *ibfd;
};
typedef struct bfd_pef_data_struct bfd_pef_data_struct;

#define BFD_PEF_XLIB_TAG1 0xF04D6163 /* '?Mac' */
#define BFD_PEF_VLIB_TAG2 0x564C6962 /* 'VLib' */
#define BFD_PEF_BLIB_TAG2 0x424C6962 /* 'BLib' */

#define BFD_PEF_XLIB_VERSION 0x00000001

struct bfd_pef_xlib_header
{
  unsigned long tag1;
  unsigned long tag2;
  unsigned long current_format;
  unsigned long container_strings_offset;
  unsigned long export_hash_offset;
  unsigned long export_key_offset;
  unsigned long export_symbol_offset;
  unsigned long export_names_offset;
  unsigned long export_hash_table_power;
  unsigned long exported_symbol_count;

  unsigned long frag_name_offset;
  unsigned long frag_name_length;
  unsigned long dylib_path_offset;
  unsigned long dylib_path_length;
  unsigned long cpu_family;
  unsigned long cpu_model;
  unsigned long date_time_stamp;
  unsigned long current_version;
  unsigned long old_definition_version;
  unsigned long old_implementation_version;
};
typedef struct bfd_pef_xlib_header bfd_pef_xlib_header;

struct bfd_pef_xlib_data_struct
{
  bfd_pef_xlib_header header;
};
typedef struct bfd_pef_xlib_data_struct bfd_pef_xlib_data_struct;

int  bfd_pef_parse_loader_header    (bfd *, unsigned char *, size_t, bfd_pef_loader_header *);
int  bfd_pef_print_loader_section   (bfd *, FILE *);
void bfd_pef_print_loader_header    (bfd *, bfd_pef_loader_header *, FILE *);
int  bfd_pef_parse_imported_library (bfd *, unsigned char *, size_t, bfd_pef_imported_library *);
int  bfd_pef_parse_imported_symbol  (bfd *, unsigned char *, size_t, bfd_pef_imported_symbol *);
int  bfd_pef_scan_section           (bfd *, bfd_pef_section *);
int  bfd_pef_scan_start_address     (bfd *);
int  bfd_pef_scan                   (bfd *, bfd_pef_header *, bfd_pef_data_struct *);
