/* cg_print.h

   Copyright 2000, 2001 Free Software Foundation, Inc.

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

#ifndef cg_print_h
#define cg_print_h

extern double print_time;	/* Total of time being printed.  */

extern void cg_print                    PARAMS ((Sym **));
extern void cg_print_index              PARAMS ((void));
extern void cg_print_file_ordering      PARAMS ((void));
extern void cg_print_function_ordering  PARAMS ((void));

#endif /* cg_print_h */
