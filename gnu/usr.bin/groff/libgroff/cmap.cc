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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <ctype.h>
#ifdef __FreeBSD__
#include <locale.h>
#endif
#include "cmap.h"

cmap cmlower(CMAP_BUILTIN);
cmap cmupper(CMAP_BUILTIN);

#if defined(isascii) && !defined(__FreeBSD__)
#define ISASCII(c) isascii(c)
#else
#define ISASCII(c) (1)
#endif

cmap::cmap()
{
  unsigned char *p = v;
  for (int i = 0; i <= UCHAR_MAX; i++)
    p[i] = i;
}

cmap::cmap(cmap_builtin)
{
  // these are initialised by cmap_init::cmap_init()
}

int cmap_init::initialised = 0;

cmap_init::cmap_init()
{
  if (initialised)
    return;
  initialised = 1;
#ifdef __FreeBSD__
  (void) setlocale(LC_CTYPE, "");
#endif
  for (int i = 0; i <= UCHAR_MAX; i++) {
    cmupper.v[i] = ISASCII(i) && islower(i) ? toupper(i) : i;
    cmlower.v[i] = ISASCII(i) && isupper(i) ? tolower(i) : i;
  }
}
