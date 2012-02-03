/* Copyright (C) 2005 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#ifdef NEED_DECLARATION_HYPOT
  double hypot(double, double);
#endif /* NEED_DECLARATION_HYPOT */
  
double groff_hypot(double x, double y)
{
  double result = hypot(x, y);

#ifdef __INTERIX
  /* hypot() on Interix is broken */
  if (isnan(result) && !isnan(x) && !isnan(y))
    return 0.0;
#endif

  return result;
}
