/* 
Copyright (C) 1988, 1993 Free Software Foundation
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

// Routines for converting between Integers and doubles.
// Split up into a separate file to avoid Integer.o's need
// for libm.a on some systems (including SunOS 4).

#include <Integer.h>
#include "Integer.hP"
#include <float.h>
#include <math.h>
#include <limits.h>

#ifndef HUGE_VAL
#ifdef HUGE
#define HUGE_VAL HUGE
#else
#define HUGE_VAL DBL_MAX
#endif
#endif

// convert to a double 

double Itodouble(const IntRep* rep)
{ 
  double d = 0.0;
  double bound = DBL_MAX / 2.0;
  for (int i = rep->len - 1; i >= 0; --i)
  {
    unsigned short a = I_RADIX >> 1;
    while (a != 0)
    {
      if (d >= bound)
        return (rep->sgn == I_NEGATIVE) ? -HUGE_VAL : HUGE_VAL;
      d *= 2.0;
      if (rep->s[i] & a)
        d += 1.0;
      a >>= 1;
    }
  }
  if (rep->sgn == I_NEGATIVE)
    return -d;
  else
    return d;
}

// see whether op double() will work-
// have to actually try it in order to find out
// since otherwise might trigger fp exception

int Iisdouble(const IntRep* rep)
{
  double d = 0.0;
  double bound = DBL_MAX / 2.0;
  for (int i = rep->len - 1; i >= 0; --i)
  {
    unsigned short a = I_RADIX >> 1;
    while (a != 0)
    {
      if (d > bound || (d == bound && (i > 0 || (rep->s[i] & a))))
        return 0;
      d *= 2.0;
      if (rep->s[i] & a)
        d += 1.0;
      a >>= 1;
    }
  }
  return 1;
}

// real division of num / den

double ratio(const Integer& num, const Integer& den)
{
  Integer q, r;
  divide(num, den, q, r);
  double d1 = q.as_double();
 
  if (d1 >= DBL_MAX || d1 <= -DBL_MAX || sign(r) == 0)
    return d1;
  else      // use as much precision as available for fractional part
  {
    double  d2 = 0.0;
    double  d3 = 0.0; 
    int cont = 1;
    for (int i = den.rep->len - 1; i >= 0 && cont; --i)
    {
      unsigned short a = I_RADIX >> 1;
      while (a != 0)
      {
        if (d2 + 1.0 == d2) // out of precision when we get here
        {
          cont = 0;
          break;
        }

        d2 *= 2.0;
        if (den.rep->s[i] & a)
          d2 += 1.0;

        if (i < r.rep->len)
        {
          d3 *= 2.0;
          if (r.rep->s[i] & a)
            d3 += 1.0;
        }

        a >>= 1;
      }
    }

    if (sign(r) < 0)
      d3 = -d3;
    return d1 + d3 / d2;
  }
}

double
Integer::as_double () const
{
  return Itodouble (rep);
}

int
Integer::fits_in_double () const
{
  return Iisdouble(rep);
}
