/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by Gaius Mulley (gaius@glam.ac.uk)

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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "posix.h"
#include "stringclass.h"

/*
 *  this file contains a very simple set of routines shared by
 *  tbl, pic, eqn which help the html device driver to make
 *  sensible formatting choices. Currently it simply indicates
 *  when a region of gyphs should be rendered as an image rather
 *  than html. In the future it should be expanded so to enable:
 *
 *    tbl    to inform grohtml about table widths.
 *    troff  to inform grohtml about tab positions and whether
 *           we are entering two/three column mode.
 */


static int is_in_graphic_start = 0;

/*
 *  graphic_start - emit a html graphic start indicator, but only
 *                  if one has not already been issued.
 */

void graphic_start (void)
{
  if (! is_in_graphic_start) {
    put_string(".if '\\*(.T'html' \\X(graphic-start(\\c\n", stdout);
    is_in_graphic_start = 1;
  }
}

/*
 *  graphic_end - emit a html graphic end indicator, but only
 *                if a corresponding matching graphic-start has
 *                been issued.
 */

void graphic_end (void)
{
  if (is_in_graphic_start) {
    put_string(".if '\\*(.T'html' \\X(graphic-end(\\c\n", stdout);
    is_in_graphic_start = 0;
  }
}
