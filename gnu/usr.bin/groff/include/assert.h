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

#ifndef ASSERT_H
#define ASSERT_H
#ifdef __GNUG__
volatile
#endif
void assertion_failed(int, const char *);

inline void do_assert(int expr, int line, const char *file)
{
  if (!expr)
    assertion_failed(line, file);
}
#endif /* ASSERT_H */

#undef assert

#ifdef NDEBUG
#define assert(ignore) /* as nothing */
#else
#define assert(expr) do_assert(expr, __LINE__, __FILE__)
#endif
