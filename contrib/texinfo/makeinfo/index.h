/* index.h -- declarations for index.c.
   $Id: index.h,v 1.4 1999/04/19 18:12:17 karl Exp $

   Copyright (C) 1998, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef INDEX_H
#define INDEX_H

#include "makeinfo.h"
#include "cmds.h"

/* User commands are only new indices.  (Macros are handled separately.)  */
extern COMMAND **user_command_array;
extern int user_command_array_len;

/* Initialize all indices.  */
extern void init_indices ();

/* Function to compare index entries for sorting.  */
extern int (*index_compare_fn) ();

#endif /* !INDEX_H */
