// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "lib.h"

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include "errarg.h"
#include "error.h"
#include "font.h"
#include "ptable.h"

declare_ptable(int)
implement_ptable(int)

class character_indexer {
public:
  character_indexer();
  ~character_indexer();
  int ascii_char_index(unsigned char);
  int named_char_index(const char *);
  int numbered_char_index(int);
private:
  enum { NSMALL = 256 };
  int next_index;
  int ascii_index[256];
  int small_number_index[NSMALL];
  PTABLE(int) table;
};

character_indexer::character_indexer()
: next_index(0)
{
  int i;
  for (i = 0; i < 256; i++)
    ascii_index[i] = -1;
  for (i = 0; i < NSMALL; i++)
    small_number_index[i] = -1;
}

character_indexer::~character_indexer()
{
}

int character_indexer::ascii_char_index(unsigned char c)
{
  if (ascii_index[c] < 0)
    ascii_index[c] = next_index++;
  return ascii_index[c];
}

int character_indexer::numbered_char_index(int n)
{
  if (n >= 0 && n < NSMALL) {
    if (small_number_index[n] < 0)
      small_number_index[n] = next_index++;
    return small_number_index[n];
  }
  // Not the most efficient possible implementation.
  char buf[INT_DIGITS + 3];
  buf[0] = ' ';
  strcpy(buf + 1, i_to_a(n));
  return named_char_index(buf);
}

int character_indexer::named_char_index(const char *s)
{
  int *np = table.lookup(s);
  if (!np) {
    np = new int;
    *np = next_index++;
    table.define(s, np);
  }
  return *np;
}

static character_indexer indexer;

int font::number_to_index(int n)
{
  return indexer.numbered_char_index(n);
}

int font::name_to_index(const char *s)
{
  assert(s != 0 && s[0] != '\0' && s[0] != ' ');
  if (s[1] == '\0')
    return indexer.ascii_char_index(s[0]);
  /* char128 and \200 are synonyms */
  if (s[0] == 'c' && s[1] == 'h' && s[2] == 'a' && s[3] == 'r') {
    char *res;
    long n = strtol(s + 4, &res, 10);
    if (res != s + 4 && *res == '\0' && n >= 0 && n < 256)
      return indexer.ascii_char_index((unsigned char)n);
  }
  return indexer.named_char_index(s);
}

