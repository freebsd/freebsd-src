/* This file is obj-hp300.h
   Copyright 1993, 2000 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "config/obj-aout.c"

/* Aout file generation & utilities */
void
hp300_header_append (where, headers)
     char **where;
     object_headers *headers;
{
  tc_headers_hook (headers);

#define DO(FIELD)	\
  { \
    md_number_to_chars (*where, headers->header.FIELD, sizeof (headers->header.FIELD)); \
    *where += sizeof (headers->header.FIELD); \
  }

  DO (a_info);
  DO (a_spare1);
  DO (a_spare2);
  DO (a_text);
  DO (a_data);
  DO (a_bss);
  DO (a_trsize);
  DO (a_drsize);
  DO (a_spare3);
  DO (a_spare4);
  DO (a_spare5);
  DO (a_entry);
  DO (a_spare6);
  DO (a_spare7);
  DO (a_syms);
  DO (a_spare8);
}
