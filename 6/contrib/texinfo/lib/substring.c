/* substring.c -- extract substring.
   $Id: substring.c,v 1.2 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1999, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

char *
substring (const char *start, const char *end)
{
  char *result = xmalloc (end - start + 1);
  char *scan_result = result;
  const char *scan = start;

  while (scan < end)
    *scan_result++ = *scan++;

  *scan_result = 0;
  return result;
}

