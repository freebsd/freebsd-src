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

class errarg {
  enum { EMPTY, STRING, CHAR, INTEGER, DOUBLE } type;
  union {
    const char *s;
    int n;
    char c;
    double d;
  };
 public:
  errarg();
  errarg(const char *);
  errarg(char);
  errarg(unsigned char);
  errarg(int);
  errarg(double);
  int empty() const;
  void print() const;
};

extern errarg empty_errarg;

extern void errprint(const char *,
		     const errarg &arg1 = empty_errarg,
		     const errarg &arg2 = empty_errarg,
		     const errarg &arg3 = empty_errarg);

