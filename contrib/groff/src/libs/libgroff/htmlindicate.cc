/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
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

#include "nonposix.h"
#include "stringclass.h"
#include "html-strings.h"

/*
 *  This file contains a very simple set of routines shared by
 *  tbl, pic, eqn which help the html device driver to make
 *  sensible formatting choices.  Currently it simply indicates
 *  to pre-html when an image is about to be created this is then
 *  passes to pre-html.
 *  Pre-html runs troff twice, once with -Thtml and once with -Tps.
 *  troff -Thtml device driver emits a <src='image'.png> tag
 *  and the postscript device driver works out the min/max limits
 *  of the graphic region.  These region limits are read by pre-html
 *  and an image is generated via troff -Tps -> gs -> png
 */

static int is_in_graphic_start = 0;
static int is_inline_image = 0;

/*
 *  html_begin_suppress - emit a start of image tag which will be seen
 *                        by pre-html.
 */
void html_begin_suppress(int is_inline)
{
  if (is_inline)
    put_string(HTML_IMAGE_INLINE_BEGIN, stdout);
  else {
    put_string(HTML_IMAGE_CENTERED, stdout);
    put_string("\n", stdout);
  }
}

/*
 *  html_end_suppress - emit an end of image tag which will be seen
 *                      by pre-html.
 */
void html_end_suppress(int is_inline)
{
  if (is_inline)
    put_string(HTML_IMAGE_INLINE_END, stdout);
  else {
    put_string(HTML_IMAGE_END, stdout);
    put_string("\n", stdout);
  }
}

/*
 *  graphic_start - The boolean, is_inline, should be:
 *
 *                  FALSE if this is called via EQ, TS, PS, and
 *                  TRUE if issued via delim $$  $ x over y $ etc.
 */
void graphic_start(int is_inline)
{
  if (!is_in_graphic_start) {
    html_begin_suppress(is_inline);
    is_inline_image = is_inline;
    is_in_graphic_start = 1;
  }
}

/*
 *  graphic_end - tell troff that the image region is ending.
 */

void graphic_end()
{
  if (is_in_graphic_start) {
    html_end_suppress(is_inline_image);
    is_in_graphic_start = 0;
  }
}
