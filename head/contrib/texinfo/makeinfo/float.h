/* float.h -- declarations for the float environment.
   $Id: float.h,v 1.5 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

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

   Written by Alper Ersoy <dirt@gtk.org>.  */

#ifndef FLOAT_H
#define FLOAT_H

typedef struct float_elt
{
  struct float_elt *next;
  char *id;
  char *type;
  char *title;
  char *shorttitle;
  char *position;
  char *number;
  char *section;
  char *section_name;
  short title_used;
  int defining_line;
} FLOAT_ELT;

extern void add_new_float (char *id, char *title, char *shorttitle,
    char *type, char *position);
extern void current_float_set_title_used (void);

/* Information retrieval about the current float env.  */
extern char *current_float_id (void);
extern char *current_float_title (void);
extern char *current_float_shorttitle (void);
extern char *current_float_type (void);
extern char *current_float_position (void);
extern char *current_float_number (void);
extern char *get_float_ref (char *id);

extern int count_floats_of_type_in_chapter (char *type, char *chapter);
extern int current_float_used_title (void);

#endif /* not FLOAT_H */
