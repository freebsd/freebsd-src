// -*- C++ -*-
/* Copyright (C) 2000 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote output.cc
 *  but it owes a huge amount of ideas and raw code from
 *  James Clark (jjc@jclark.com) grops/ps.cc.
 *
 *  output.cc
 *
 *  provide the simple low level output routines needed by html.cc
 */

/*
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

#include "driver.h"
#include "stringclass.h"
#include "cset.h"

#include <time.h>
#include "html.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif


/*
 *  the classes and methods for simple_output manipulation
 */

simple_output::simple_output(FILE *f, int n)
: fp(f), max_line_length(n), col(0), need_space(0), fixed_point(0)
{
}

simple_output &simple_output::set_file(FILE *f)
{
  fp = f;
  col = 0;
  return *this;
}

simple_output &simple_output::copy_file(FILE *infp)
{
  int c;
  while ((c = getc(infp)) != EOF)
    putc(c, fp);
  return *this;
}

simple_output &simple_output::end_line()
{
  if (col != 0) {
    putc('\n', fp);
    col = 0;
    need_space = 0;
  }
  return *this;
}

simple_output &simple_output::special(const char *s)
{
  return *this;
}

simple_output &simple_output::simple_comment(const char *s)
{
  if (col != 0)
    putc('\n', fp);
  fputs("<!-- ", fp);
  fputs(s, fp);
  fputs(" -->\n", fp);
  col = 0;
  need_space = 0;
  return *this;
}

simple_output &simple_output::begin_comment(const char *s)
{
  if (col != 0)
    putc('\n', fp);
  fputs("<!-- ", fp);
  fputs(s, fp);
  col = 5 + strlen(s);
  return *this;
}

simple_output &simple_output::end_comment()
{
  if (need_space) {
    putc(' ', fp);
  }
  fputs(" -->\n", fp);
  col = 0;
  need_space = 0;
  return *this;
}

simple_output &simple_output::comment_arg(const char *s)
{
  int len = strlen(s);

  if (col + len + 1 > max_line_length) {
    fputs("\n ", fp);
    col = 1;
  }
  fputs(s, fp);
  col += len + 1;
  return *this;
}

simple_output &simple_output::set_fixed_point(int n)
{
  assert(n >= 0 && n <= 10);
  fixed_point = n;
  return *this;
}

simple_output &simple_output::put_raw_char(char c)
{
  putc(c, fp);
  col++;
  need_space = 0;
  return *this;
}

simple_output &simple_output::put_string(const char *s, int n)
{
  int i=0;

  while (i<n) {
    fputc(s[i], fp);
    i++;
  }
  col += n;
  return *this;
}

simple_output &simple_output::put_translated_string(const char *s)
{
  int i=0;

  while (s[i] != (char)0) {
    if ((s[i] & 0x7f) == s[i]) {
      fputc(s[i], fp);
    }
    i++;
  }
  col += i;
  return *this;
}

simple_output &simple_output::put_string(const char *s)
{
  int i=0;

  while (s[i] != '\0') {
    fputc(s[i], fp);
    i++;
  }
  col += i;
  return *this;
}

struct html_2_postscript {
  char *html_char;
  char *postscript_char;
};

static struct html_2_postscript ps_char_conversions[] = {
  { "+-", "char177", },
  { "eq", "="      , },
  { "mu", "char215", },
  { NULL, NULL     , },
};


/*
 * this is an aweful hack which attempts to translate html characters onto
 * postscript characters. Can this be done inside the devhtml files?
 *
 * or should we read the devps files and find out the translations?
 */

simple_output &simple_output::put_troffps_char (const char *s)
{
  int i=0;

  while (ps_char_conversions[i].html_char != NULL) {
    if (strcmp(s, ps_char_conversions[i].html_char) == 0) {
      put_string(ps_char_conversions[i].postscript_char);
      return *this;
    } else {
      i++;
    }
  }
  put_string(s);
  return *this;
}

simple_output &simple_output::put_number(int n)
{
  char buf[1 + INT_DIGITS + 1];
  sprintf(buf, "%d", n);
  int len = strlen(buf);
  put_string(buf, len);
  need_space = 1;
  return *this;
}

simple_output &simple_output::put_float(double d)
{
  char buf[128];

  sprintf(buf, "%.4f", d);
  int len = strlen(buf);
  put_string(buf, len);
  need_space = 1;
  return *this;
}


simple_output &simple_output::put_symbol(const char *s)
{
  int len = strlen(s);

  if (need_space) {
    putc(' ', fp);
    col++;
  }
  fputs(s, fp);
  col += len;
  need_space = 1;
  return *this;
}
