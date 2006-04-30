// -*- C++ -*-
/* Copyright (C) 2002, 2003
   Free Software Foundation, Inc.
     Written by Werner Lemberg <wl@gnu.org>

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

const char *glyph_name_to_unicode(const char *);
const char *unicode_to_glyph_name(const char *);
const char *decompose_unicode(const char *);

const char *check_unicode_name(const char *);
