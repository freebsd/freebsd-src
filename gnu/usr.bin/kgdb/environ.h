/* Header for environment manipulation library.
   Copyright (C) 1989, Free Software Foundation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* We manipulate environments represented as these structures.  */

struct environ
{
  /* Number of usable slots allocated in VECTOR.
     VECTOR always has one slot not counted here,
     to hold the terminating zero.  */
  int allocated;
  /* A vector of slots, ALLOCATED + 1 of them.
     The first few slots contain strings "VAR=VALUE"
     and the next one contains zero.
     Then come some unused slots.  */
  char **vector;
};

struct environ *make_environ ();
void free_environ ();
void init_environ ();
char *get_in_environ ();
void set_in_environ ();
void unset_in_environ ();
char **environ_vector ();
