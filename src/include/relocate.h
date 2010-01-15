// -*- C++ -*-
/* Provide relocation for macro and font files.
   Copyright (C) 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301,
   USA.  */

extern char *curr_prefix;
extern size_t curr_prefix_len;

void set_current_prefix ();
char *xdirname (char *s);
char *searchpath (const char *name, const char *pathp);
char *relocatep (const char *path);
char *relocate (const char *path);
