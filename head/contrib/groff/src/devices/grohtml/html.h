// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2002, 2004 Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#if !defined(HTML_H)
#  define HTML_H

/*
 *  class and structure needed to buffer words
 */

struct word {
  char *s;
  word  *next;

  word  (const char *w, int n);
  ~word ();
};

class word_list {
public:
            word_list     ();
  int       flush         (FILE *f);
  void      add_word      (const char *s, int n);
  int       get_length    (void);
  
private:
  int       length;
  word     *head;
  word     *tail;
};

class simple_output {
public:
  simple_output(FILE *, int max_line_length);
  simple_output &put_string(const char *, int);
  simple_output &put_string(const char *s);
  simple_output &put_string(const string &s);
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
  simple_output &enable_newlines(int);
  simple_output &check_newline(int n);
  simple_output &nl(void);
  simple_output &force_nl(void);
  simple_output &space_or_newline (void);
  simple_output &begin_tag (void);
  FILE *get_file();
private:
  FILE         *fp;
  int           max_line_length;          // not including newline
  int           col;
  int           fixed_point;
  int           newlines;                 // can we issue newlines automatically?
  word_list     last_word;

  void          flush_last_word (void);
  int           check_space (const char *s, int n);
};

inline FILE *simple_output::get_file()
{
  return fp;
}

#endif
