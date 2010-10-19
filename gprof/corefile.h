/* corefile.h

   Copyright 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

This file is part of GNU Binutils.

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

#ifndef corefile_h
#define corefile_h

struct function_map
{
  char *function_name;
  char *file_name;
};

extern struct function_map *symbol_map;
extern unsigned int symbol_map_count;

extern bfd *core_bfd;		/* BFD for core-file.  */
extern asection *core_text_sect;/* Core text section.  */
extern PTR core_text_space;	/* Text space of a.out in core.  */
extern int offset_to_code;	/* Offset (in bytes) of code from entry
				   address of routine.  */

extern void core_init                  (const char *);
extern void core_get_text_space        (bfd *);
extern void core_create_function_syms  (void);
extern void core_create_line_syms      (void);

#endif /* corefile_h */
