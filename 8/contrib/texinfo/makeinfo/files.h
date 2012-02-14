/* files.h -- declarations for files.c.
   $Id: files.h,v 1.4 2004/07/27 00:06:31 karl Exp $

   Copyright (C) 1998, 2002, 2004 Free Software Foundation, Inc.

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

extern void pushfile (void);
extern void popfile (void);
extern void flush_file_stack (void);
extern char *get_file_info_in_path (char *filename, char *path,
    struct stat *finfo);
extern char *find_and_load (char *filename, int use_path);
extern char *output_name_from_input_name (char *name);
extern char *expand_filename (char *filename, char *input_name);
extern char *filename_part (char *filename);
extern char *pathname_part (char *filename);
extern char *normalize_filename (char *fname);
extern void append_to_include_path (char *path);
extern void prepend_to_include_path (char *path);
extern void pop_path_from_include_path (void);
extern void register_delayed_write (char *delayed_command);
extern void handle_delayed_writes (void);

typedef struct delayed_write
{
  struct delayed_write *next;
  char *command;
  char *filename;
  char *input_filename;
  char *node;
  int position;
  int calling_line;

  int node_order;
  int index_order;
} DELAYED_WRITE;

extern int handling_delayed_writes;

#endif /* !FILES_H */
