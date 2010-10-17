/* basic_blocks.h
   Copyright 2000, 2002 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef basic_blocks_h
#define basic_blocks_h

/* Options:  */
extern bfd_boolean bb_annotate_all_lines; /* Force annotation of all lines?  */
extern int bb_table_length;		/* Length of most-used bb table.  */
extern unsigned long bb_min_calls;	/* Minimum execution count.  */

extern void bb_read_rec             PARAMS ((FILE *, const char *));
extern void bb_write_blocks         PARAMS ((FILE *, const char *));
extern void bb_create_syms          PARAMS ((void));
extern void print_annotated_source  PARAMS ((void));
extern void print_exec_counts       PARAMS ((void));
#endif /* basic_blocks_h */
