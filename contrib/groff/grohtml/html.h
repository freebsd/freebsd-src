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

class simple_output {
public:
  simple_output(FILE *, int max_line_length);
  simple_output &put_string(const char *, int);
  simple_output &put_string(const char *s);
  simple_output &put_troffps_char (const char *s);
  simple_output &put_translated_string(const char *s);
  simple_output &put_number(int);
  simple_output &put_float(double);
  simple_output &put_symbol(const char *);
  simple_output &put_literal_symbol(const char *);
  simple_output &set_fixed_point(int);
  simple_output &simple_comment(const char *);
  simple_output &begin_comment(const char *);
  simple_output &comment_arg(const char *);
  simple_output &end_comment();
  simple_output &set_file(FILE *);
  simple_output &include_file(FILE *);
  simple_output &copy_file(FILE *);
  simple_output &end_line();
  simple_output &put_raw_char(char);
  simple_output &special(const char *);
  simple_output &put_html_char (char);
  FILE *get_file();
private:
  FILE *fp;
  int max_line_length;		// not including newline
  int col;
  int need_space;
  int fixed_point;
};

inline FILE *simple_output::get_file()
{
  return fp;
}

