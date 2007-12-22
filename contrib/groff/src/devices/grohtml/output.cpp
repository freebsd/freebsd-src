// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2003, 2004, 2005 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote output.cpp
 *  but it owes a huge amount of ideas and raw code from
 *  James Clark (jjc@jclark.com) grops/ps.cpp.
 *
 *  output.cpp
 *
 *  provide the simple low level output routines needed by html.cpp
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "driver.h"
#include "stringclass.h"
#include "cset.h"

#include <time.h>
#include "html.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#undef DEBUGGING
// #define DEBUGGING

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif


#if defined(DEBUGGING)
#  define FPUTC(X,Y)   do { fputc((X),(Y)); fputc((X), stderr); fflush(stderr); } while (0)
#  define FPUTS(X,Y)   do { fputs((X),(Y)); fputs((X), stderr); fflush(stderr); } while (0)
#  define PUTC(X,Y)    do { putc((X),(Y)); putc((X), stderr); fflush(stderr); } while (0)
#else
#  define FPUTC(X,Y)   do { fputc((X),(Y)); } while (0)
#  define FPUTS(X,Y)   do { fputs((X),(Y)); } while (0)
#  define PUTC(X,Y)    do { putc((X),(Y)); } while (0)
#endif


/*
 *  word - initialise a word and set next to NULL
 */

word::word (const char *w, int n)
  : next(0)
{
  s = new char[n+1];
  strncpy(s, w, n);
  s[n] = (char)0;
}

/*
 *  destroy word and the string copy.
 */

word::~word ()
{
  a_delete s;
}

/*
 *  word_list - create an empty word list.
 */

word_list::word_list ()
  : length(0), head(0), tail(0)
{
}

/*
 *  flush - flush a word list to a FILE, f, and return the
 *          length of the buffered string.
 */

int word_list::flush (FILE *f)
{
  word *t;
  int   len=length;

  while (head != 0) {
    t = head;
    head = head->next;
    FPUTS(t->s, f);
    delete t;
  }
  head   = 0;
  tail   = 0;
  length = 0;
#if defined(DEBUGGING)
  fflush(f);   // just for testing
#endif
  return( len );
}

/*
 *  add_word - adds a word to the outstanding word list.
 */

void word_list::add_word (const char *s, int n)
{
  if (head == 0) {
    head = new word(s, n);
    tail = head;
  } else {
    tail->next = new word(s, n);
    tail       = tail->next;
  }
  length += n;
}

/*
 *  get_length - returns the number of characters buffered
 */

int word_list::get_length (void)
{
  return( length );
}

/*
 *  the classes and methods for simple_output manipulation
 */

simple_output::simple_output(FILE *f, int n)
: fp(f), max_line_length(n), col(0), fixed_point(0), newlines(0)
{
}

simple_output &simple_output::set_file(FILE *f)
{
  if (fp)
    fflush(fp);
  fp = f;
  return *this;
}

simple_output &simple_output::copy_file(FILE *infp)
{
  int c;
  while ((c = getc(infp)) != EOF)
    PUTC(c, fp);
  return *this;
}

simple_output &simple_output::end_line()
{
  flush_last_word();
  if (col != 0) {
    PUTC('\n', fp);
    col = 0;
  }
  return *this;
}

simple_output &simple_output::special(const char *)
{
  return *this;
}

simple_output &simple_output::simple_comment(const char *s)
{
  flush_last_word();
  if (col != 0)
    PUTC('\n', fp);
  FPUTS("<!-- ", fp);
  FPUTS(s, fp);
  FPUTS(" -->\n", fp);
  col = 0;
  return *this;
}

simple_output &simple_output::begin_comment(const char *s)
{
  flush_last_word();
  if (col != 0)
    PUTC('\n', fp);
  col = 0;
  put_string("<!--");
  space_or_newline();
  last_word.add_word(s, strlen(s));
  return *this;
}

simple_output &simple_output::end_comment()
{
  flush_last_word();
  space_or_newline();
  put_string("-->").nl();
  return *this;
}

/*
 *  check_newline - checks to see whether we are able to issue
 *                  a newline and that one is needed.
 */

simple_output &simple_output::check_newline(int n)
{
  if ((col + n + last_word.get_length() + 1 > max_line_length) && (newlines)) {
    FPUTC('\n', fp);
    col = last_word.flush(fp);
  }
  return *this;
}

/*
 *  space_or_newline - will emit a newline or a space later on
 *                     depending upon the current column.
 */

simple_output &simple_output::space_or_newline (void)
{
  if ((col + last_word.get_length() + 1 > max_line_length) && (newlines)) {
    FPUTC('\n', fp);
    if (last_word.get_length() > 0) {
      col = last_word.flush(fp);
    } else {
      col = 0;
    }
  } else {
    if (last_word.get_length() != 0) {
      if (col > 0) {
	FPUTC(' ', fp);
	col++;
      }
      col += last_word.flush(fp);
    }
  }
  return *this;
}

/*
 *  force_nl - forces a newline.
 */

simple_output &simple_output::force_nl (void)
{
  space_or_newline();
  col += last_word.flush(fp);
  FPUTC('\n', fp);
  col = 0;
  return *this ;
}

/*
 *  nl - writes a newline providing that we
 *       are not in the first column.
 */

simple_output &simple_output::nl (void)
{
  space_or_newline();
  col += last_word.flush(fp);
  FPUTC('\n', fp);
  col = 0;
  return *this ;
}

simple_output &simple_output::set_fixed_point(int n)
{
  assert(n >= 0 && n <= 10);
  fixed_point = n;
  return *this;
}

simple_output &simple_output::put_raw_char(char c)
{
  col += last_word.flush(fp);
  PUTC(c, fp);
  col++;
  return *this;
}

simple_output &simple_output::put_string(const char *s, int n)
{
  last_word.add_word(s, n);
  return *this;
}

simple_output &simple_output::put_string(const char *s)
{
  last_word.add_word(s, strlen(s));
  return *this;
}

simple_output &simple_output::put_string(const string &s)
{
  last_word.add_word(s.contents(), s.length());
  return *this;
}

simple_output &simple_output::put_number(int n)
{
  char buf[1 + INT_DIGITS + 1];
  sprintf(buf, "%d", n);
  put_string(buf);
  return *this;
}

simple_output &simple_output::put_float(double d)
{
  char buf[128];

  sprintf(buf, "%.4f", d);
  put_string(buf);
  return *this;
}

simple_output &simple_output::enable_newlines (int auto_newlines)
{
  check_newline(0);
  newlines = auto_newlines;
  check_newline(0);
  return *this;
}

/*
 *  flush_last_word - flushes the last word and adjusts the
 *                    col position. It will insert a newline
 *                    before the last word if allowed and if
 *                    necessary.
 */

void simple_output::flush_last_word (void)
{
  int len=last_word.get_length();

  if (len > 0) {
    if (newlines) {
      if (col + len + 1 > max_line_length) {
	FPUTS("\n", fp);
	col = 0;
      } else {
	FPUTS(" ", fp);
	col++;
      }
      len += last_word.flush(fp);
    } else {
      FPUTS(" ", fp);
      col++;
      col += last_word.flush(fp);
    }
  }
}
