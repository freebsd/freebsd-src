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

#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif

enum cmap_builtin { CMAP_BUILTIN };

class cmap {
public:
  cmap();
  cmap(cmap_builtin);
  int operator()(unsigned char) const;
  unsigned char &operator[](unsigned char);

  friend class cmap_init;
private:
  unsigned char v[UCHAR_MAX+1];
};

inline int cmap::operator()(unsigned char c) const
{
  return v[c];
}

inline unsigned char &cmap::operator[](unsigned char c)
{
  return v[c];
}

extern cmap cmlower;
extern cmap cmupper;

static class cmap_init {
  static int initialised;
public:
  cmap_init();
} _cmap_init;
