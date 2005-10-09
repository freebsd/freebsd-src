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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

struct font_params {
  int italic;
  int em;
  int x_height;
  int fig_height;
  int cap_height;
  int asc_height;
  int body_height;
  int comma_depth;
  int desc_depth;
  int body_depth;
};

struct char_metric {
  int width;
  int type;
  int height;
  int depth;
  int ic;
  int left_ic;
  int sk;
};
  
void guess(const char *s, const font_params &param, char_metric *metric);
