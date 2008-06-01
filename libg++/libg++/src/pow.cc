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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <builtin.h>
#include <math.h>

double pow(double x, long p)
{
  if (p == 0)
    return 1.0;
  else if (x == 0.0)
    return 0.0;
  else
  {
    if (p < 0)
    {
      p = -p;
      x = 1.0 / x;
    }

    double r = 1.0;
    for(;;)
    {
      if (p & 1)
        r *= x;
      if ((p >>= 1) == 0)
        return r;
      else
        x *= x;
    }
  }
}

long  pow(long  x, long p)
{
  if (p == 0)
    return 1;
  else if (p < 0 || x == 0)
    return 0;
  else
  {
    long r = 1;
    for(;;)
    {
      if (p & 1)
        r *= x;
      if ((p >>= 1) == 0)
        return r;
      else
        x *= x;
    }
  }
}
