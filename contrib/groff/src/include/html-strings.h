// -*- C++ -*-
/* Copyright (C) 2001 Free Software Foundation, Inc.
     Written by Gaius Mulley (gaius@glam.ac.uk).

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

/*
 *  defines the image tags issued by the pre-processors (tbl, pic, eqn)
 *  and later detected by pre-html.cc
 */

#define HTML_IMAGE_INLINE_BEGIN	"\\O[HTML-IMAGE-INLINE-BEGIN]"
#define HTML_IMAGE_INLINE_END	"\\O[HTML-IMAGE-INLINE-END]"
#define HTML_IMAGE_CENTERED	".HTML-IMAGE"
#define HTML_IMAGE_RIGHT	".HTML-IMAGE-RIGHT"
#define HTML_IMAGE_LEFT		".HTML-IMAGE-LEFT"
#define HTML_IMAGE_END		".HTML-IMAGE-END"
