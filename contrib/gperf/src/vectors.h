/* This may look like C code, but it is really -*- C++ -*- */

/* Static class data members that are shared between several classes via
   inheritance.

   Copyright (C) 1989-1998 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

#ifndef vectors_h
#define vectors_h 1

static const int MAX_ALPHA_SIZE = 256;

struct Vectors
{
  static int   ALPHA_SIZE;                  /* Size of alphabet. */
  static int   occurrences[MAX_ALPHA_SIZE]; /* Counts occurrences of each key set character. */
  static int   asso_values[MAX_ALPHA_SIZE]; /* Value associated with each character. */
};

#endif
