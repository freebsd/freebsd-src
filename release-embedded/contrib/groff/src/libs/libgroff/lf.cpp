// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2004 Free Software Foundation, Inc.
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

#include <ctype.h>

#include "lib.h"
#include "cset.h"
#include "stringclass.h"

extern void change_filename(const char *);
extern void change_lineno(int);

int interpret_lf_args(const char *p)
{
  while (*p == ' ')
    p++;
  if (!csdigit(*p))
    return 0;
  int ln = 0;
  do {
    ln *= 10;
    ln += *p++ - '0';
  } while (csdigit(*p));
  if (*p != ' ' && *p != '\n' && *p != '\0')
    return 0;
  while (*p == ' ')
    p++;
  if (*p == '\0' || *p == '\n')  {
    change_lineno(ln);
    return 1;
  }
  const char *q;
  for (q = p;
       *q != '\0' && *q != ' ' && *q != '\n' && *q != '\\';
       q++)
    ;
  string tem(p, q - p);
  while (*q == ' ')
    q++;
  if (*q != '\n' && *q != '\0')
    return 0;
  tem += '\0';
  change_filename(tem.contents());
  change_lineno(ln);
  return 1;
}
