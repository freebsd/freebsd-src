/* Access for application keys in mmap'd malloc managed region.
   Copyright 1992 Free Software Foundation, Inc.

   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* This module provides access to some keys that the application can use to
   provide persistent access to locations in the mapped memory section.
   The intent is that these keys are to be used sparingly as sort of
   persistent global variables which the application can use to reinitialize
   access to data in the mapped region.

   For the moment, these keys are simply stored in the malloc descriptor
   itself, in an array of fixed length.  This should be fixed so that there
   can be an unlimited number of keys, possibly using a multilevel access
   scheme of some sort. */

#include "mmalloc.h"

int
mmalloc_setkey (md, keynum, key)
  PTR md;     
  int keynum;
  PTR key;
{
  struct mdesc *mdp = (struct mdesc *) md;
  int result = 0;

  if ((mdp != NULL) && (keynum >= 0) && (keynum < MMALLOC_KEYS))
    {
      mdp -> keys [keynum] = key;
      result++;
    }
  return (result);
}

PTR
mmalloc_getkey (md, keynum)
  PTR md;     
  int keynum;
{
  struct mdesc *mdp = (struct mdesc *) md;
  PTR keyval = NULL;

  if ((mdp != NULL) && (keynum >= 0) && (keynum < MMALLOC_KEYS))
    {
      keyval = mdp -> keys [keynum];
    }
  return (keyval);
}
