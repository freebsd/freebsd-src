// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <builtin.h>
#include "<T>.<C>.Map.h"


Pix <T><C>Map::seek(<T&> item)
{
  for (Pix i = first(); i != 0 && !(<T>EQ(key(i), item)); next(i));
  return i;
}

int <T><C>Map::owns(Pix idx)
{
  if (idx == 0) return 0;
  for (Pix i = first(); i; next(i)) if (i == idx) return 1;
  return 0;
}

void <T><C>Map::clear()
{
  Pix i = first(); 
  while (i != 0)
  {
    del(key(i));
    i = first();
  }
}

int <T><C>Map::contains (<T&> item)
{
  return seek(item) != 0;
}


void <T><C>Map::error(const char* msg)
{
  (*lib_error_handler)("Map", msg);
}
