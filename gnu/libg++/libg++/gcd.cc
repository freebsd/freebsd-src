/* 
Copyright (C) 1990 Free Software Foundation
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


/*
 common functions on built-in types
*/

long gcd(long x, long y)        // euclid's algorithm
{
  long a = abs(x);
  long b = abs(y);

  long tmp;
  
  if (b > a)
  {
    tmp = a; a = b; b = tmp;
  }
  for(;;)
  {
    if (b == 0)
      return a;
    else if (b == 1)
      return b;
    else
    {
      tmp = b;
      b = a % b;
      a = tmp;
    }
  }
}
