/* List lines of source files for GDB, the GNU debugger.
   Copyright 1999 Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef SOURCE_H
#define SOURCE_H

/* Open a source file given a symtab S.  Returns a file descriptor or
   negative number for error.  */
extern int open_source_file (struct symtab *s);

/* Create and initialize the table S->line_charpos that records the
   positions of the lines in the source file, which is assumed to be
   open on descriptor DESC.  All set S->nlines to the number of such
   lines.  */
extern void find_source_lines (struct symtab *s, int desc);

#endif
