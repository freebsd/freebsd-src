// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2002
   Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include <stdio.h>
#include "assert.h"
#include "errarg.h"

errarg::errarg(const char *p) : type(STRING)
{
  s = p ? p : "(null)";
}

errarg::errarg() : type(EMPTY)
{
}

errarg::errarg(int nn) : type(INTEGER)
{
  n = nn;
}

errarg::errarg(unsigned int uu) : type(UNSIGNED_INTEGER)
{
  u = uu;
}

errarg::errarg(char cc) : type(CHAR)
{
  c = cc;
}

errarg::errarg(unsigned char cc) : type(CHAR)
{
  c = cc;
}

errarg::errarg(double dd) : type(DOUBLE)
{
  d = dd;
}

int errarg::empty() const
{
  return type == EMPTY;
}

extern "C" {
  const char *i_to_a(int);
  const char *ui_to_a(unsigned int);
}
	    
void errarg::print() const
{
  switch (type) {
  case INTEGER:
    fputs(i_to_a(n), stderr);
    break;
  case UNSIGNED_INTEGER:
    fputs(ui_to_a(u), stderr);
    break;
  case CHAR:
    putc(c, stderr);
    break;
  case STRING:
    fputs(s, stderr);
    break;
  case DOUBLE:
    fprintf(stderr, "%g", d);
    break;
  case EMPTY:
    break;
  }
}

errarg empty_errarg;

void errprint(const char *format, 
	      const errarg &arg1,
	      const errarg &arg2,
	      const errarg &arg3)
{
  assert(format != 0);
  char c;
  while ((c = *format++) != '\0') {
    if (c == '%') {
      c = *format++;
      switch(c) {
      case '%':
	fputc('%', stderr);
	break;
      case '1':
	assert(!arg1.empty());
	arg1.print();
	break;
      case '2':
	assert(!arg2.empty());
	arg2.print();
	break;
      case '3':
	assert(!arg3.empty());
	arg3.print();
	break;
      default:
	assert(0);
      }
    }
    else
      putc(c, stderr);
  }
}
