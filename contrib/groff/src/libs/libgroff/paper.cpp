// -*- C++ -*-
/* Copyright (C) 2002, 2003
   Free Software Foundation, Inc.
     Written by Werner Lemberg (wl@gnu.org)

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
#include "paper.h"

paper papersizes[NUM_PAPERSIZES];

// length and width in mm
static void add_iso_paper(char series, int offset,
			  int start_length, int start_width)
{
  int length = start_length;
  int width = start_width;
  for (int i = 0; i < 8; i++)
  {
    char *p = new char[3];
    p[0] = series;
    p[1] = '0' + i;
    p[2] = '\0';
    papersizes[offset + i].name = p;
    // convert mm to inch
    papersizes[offset + i].length = (double)length / 25.4;
    papersizes[offset + i].width = (double)width / 25.4;
    // after division by two, values must be rounded down to the next
    // integer (as specified by ISO)
    int tmp = length;
    length = width;
    width = tmp / 2;
  }
}

// length and width in inch
static void add_american_paper(const char *name, int index,
			       double length, double width )
{
  char *p = new char[strlen(name) + 1];
  strcpy(p, name);
  papersizes[index].name = p;
  papersizes[index].length = length;
  papersizes[index].width = width;
}

int papersize_init::initialised = 0;

papersize_init::papersize_init()
{
  if (initialised)
    return;
  initialised = 1;
  add_iso_paper('a', 0, 1189, 841);
  add_iso_paper('b', 8, 1414, 1000);
  add_iso_paper('c', 16, 1297, 917);
  add_iso_paper('d', 24, 1090, 771);
  add_american_paper("letter", 32, 11, 8.5);
  add_american_paper("legal", 33, 14, 8.5);
  add_american_paper("tabloid", 34, 17, 11);
  add_american_paper("ledger", 35, 11, 17);
  add_american_paper("statement", 36, 8.5, 5.5);
  add_american_paper("executive", 37, 10, 7.5);
  // the next three entries are for grolj4
  add_american_paper("com10", 38, 9.5, 4.125);
  add_american_paper("monarch", 39, 7.5, 3.875);
  // this is an ISO format, but it easier to use add_american_paper
  add_american_paper("dl", 40, 220/25.4, 110/25.4);
}
