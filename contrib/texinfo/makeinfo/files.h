/* files.h -- declarations for files.c.
   $Id: files.h,v 1.1 1998/10/24 21:37:25 karl Exp $

   Copyright (C) 1998 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   */

#ifndef FILES_H
#define FILES_H

/* A stack of file information records.  If a new file is read in with
   "@input", we remember the old input file state on this stack. */
typedef struct fstack
{
  struct fstack *next;
  char *filename;
  char *text;
  int size;
  int offset;
  int line_number;
} FSTACK;
extern FSTACK *filestack;

extern void pushfile (), popfile ();
extern void flush_file_stack ();
extern char *find_and_load ();
extern char *output_name_from_input_name ();
extern char *expand_filename ();
extern char *filename_part ();
extern char *pathname_part ();

#endif /* !FILES_H */
