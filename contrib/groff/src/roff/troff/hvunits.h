// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */


class vunits {
  int n;
public:
  vunits();
  vunits(units);
  units to_units();
  int is_zero();
  vunits& operator+=(const vunits&);
  vunits& operator-=(const vunits&);
  friend inline vunits scale(vunits n, units x, units y); // scale n by x/y
  friend inline vunits scale(vunits n, vunits x, vunits y);
  friend inline vunits operator +(const vunits&, const vunits&);
  friend inline vunits operator -(const vunits&, const vunits&);
  friend inline vunits operator -(const vunits&);
  friend inline int operator /(const vunits&, const vunits&);
  friend inline vunits operator /(const vunits&, int);
  friend inline vunits operator *(const vunits&, int);
  friend inline vunits operator *(int, const vunits&);
  friend inline int operator <(const vunits&, const vunits&);
  friend inline int operator >(const vunits&, const vunits&);
  friend inline int operator <=(const vunits&, const vunits&);
  friend inline int operator >=(const vunits&, const vunits&);
  friend inline int operator ==(const vunits&, const vunits&);
  friend inline int operator !=(const vunits&, const vunits&);
};

extern vunits V0;


class hunits {
  int n;
public:
  hunits();
  hunits(units);
  units to_units();
  int is_zero();
  hunits& operator+=(const hunits&);
  hunits& operator-=(const hunits&);
  friend inline hunits scale(hunits n, units x, units y); // scale n by x/y
  friend inline hunits scale(hunits n, double x);
  friend inline hunits operator +(const hunits&, const hunits&);
  friend inline hunits operator -(const hunits&, const hunits&);
  friend inline hunits operator -(const hunits&);
  friend inline int operator /(const hunits&, const hunits&);
  friend inline hunits operator /(const hunits&, int);
  friend inline hunits operator *(const hunits&, int);
  friend inline hunits operator *(int, const hunits&);
  friend inline int operator <(const hunits&, const hunits&);
  friend inline int operator >(const hunits&, const hunits&);
  friend inline int operator <=(const hunits&, const hunits&);
  friend inline int operator >=(const hunits&, const hunits&);
  friend inline int operator ==(const hunits&, const hunits&);
  friend inline int operator !=(const hunits&, const hunits&);
};

extern hunits H0;

extern int get_vunits(vunits *, unsigned char si);
extern int get_hunits(hunits *, unsigned char si);
extern int get_vunits(vunits *, unsigned char si, vunits prev_value);
extern int get_hunits(hunits *, unsigned char si, hunits prev_value);

inline vunits:: vunits() : n(0)
{
}

inline units vunits::to_units()
{
  return n*vresolution;
}

inline int vunits::is_zero()
{
  return n == 0;
}

inline vunits operator +(const vunits & x, const vunits & y)
{
  vunits r;
  r = x;
  r.n += y.n;
  return r;
}

inline vunits operator -(const vunits & x, const vunits & y)
{
  vunits r;
  r = x;
  r.n -= y.n;
  return r;
}

inline vunits operator -(const vunits & x)
{
  vunits r;
  r.n = -x.n;
  return r;
}

inline int operator /(const vunits & x, const vunits & y)
{
  return x.n/y.n;
}

inline vunits operator /(const vunits & x, int n)
{
  vunits r;
  r = x;
  r.n /= n;
  return r;
}

inline vunits operator *(const vunits & x, int n)
{
  vunits r;
  r = x;
  r.n *= n;
  return r;
}

inline vunits operator *(int n, const vunits & x)
{
  vunits r;
  r = x;
  r.n *= n;
  return r;
}

inline int operator <(const vunits & x, const vunits & y)
{
  return x.n < y.n;
}

inline int operator >(const vunits & x, const vunits & y)
{
  return x.n > y.n;
}

inline int operator <=(const vunits & x, const vunits & y)
{
  return x.n <= y.n;
}

inline int operator >=(const vunits & x, const vunits & y)
{
  return x.n >= y.n;
}

inline int operator ==(const vunits & x, const vunits & y)
{
  return x.n == y.n;
}

inline int operator !=(const vunits & x, const vunits & y)
{
  return x.n != y.n;
}


inline vunits& vunits::operator+=(const vunits & x)
{
  n += x.n;
  return *this;
}

inline vunits& vunits::operator-=(const vunits & x)
{
  n -= x.n;
  return *this;
}

inline hunits:: hunits() : n(0)
{
}

inline units hunits::to_units()
{
  return n*hresolution;
}

inline int hunits::is_zero()
{
  return n == 0;
}

inline hunits operator +(const hunits & x, const hunits & y)
{
  hunits r;
  r = x;
  r.n += y.n;
  return r;
}

inline hunits operator -(const hunits & x, const hunits & y)
{
  hunits r;
  r = x;
  r.n -= y.n;
  return r;
}

inline hunits operator -(const hunits & x)
{
  hunits r;
  r = x;
  r.n = -x.n;
  return r;
}

inline int operator /(const hunits & x, const hunits & y)
{
  return x.n/y.n;
}

inline hunits operator /(const hunits & x, int n)
{
  hunits r;
  r = x;
  r.n /= n;
  return r;
}

inline hunits operator *(const hunits & x, int n)
{
  hunits r;
  r = x;
  r.n *= n;
  return r;
}

inline hunits operator *(int n, const hunits & x)
{
  hunits r;
  r = x;
  r.n *= n;
  return r;
}

inline int operator <(const hunits & x, const hunits & y)
{
  return x.n < y.n;
}

inline int operator >(const hunits & x, const hunits & y)
{
  return x.n > y.n;
}

inline int operator <=(const hunits & x, const hunits & y)
{
  return x.n <= y.n;
}

inline int operator >=(const hunits & x, const hunits & y)
{
  return x.n >= y.n;
}

inline int operator ==(const hunits & x, const hunits & y)
{
  return x.n == y.n;
}

inline int operator !=(const hunits & x, const hunits & y)
{
  return x.n != y.n;
}


inline hunits& hunits::operator+=(const hunits & x)
{
  n += x.n;
  return *this;
}

inline hunits& hunits::operator-=(const hunits & x)
{
  n -= x.n;
  return *this;
}

inline hunits scale(hunits n, units x, units y)
{
  hunits r;
  r.n = scale(n.n, x, y);
  return r;
}

inline vunits scale(vunits n, units x, units y)
{
  vunits r;
  r.n = scale(n.n, x, y);
  return r;
}

inline vunits scale(vunits n, vunits x, vunits y)
{
  vunits r;
  r.n = scale(n.n, x.n, y.n);
  return r;
}

inline hunits scale(hunits n, double x)
{
  hunits r;
  r.n = int(n.n*x);
  return r;
}

inline units scale(units n, double x)
{
  return int(n*x);
}

inline units points_to_units(units n)
{
  return scale(n, units_per_inch, 72);
}

