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

#include "driver.h"
#include "device.h"
#include "cset.h"

const char *current_filename;
int current_lineno;
const char *device = 0;
FILE *current_file;

int get_integer();		// don't read the newline
int possibly_get_integer(int *);
char *get_string(int is_long = 0);
void skip_line();

struct environment_list {
  environment env;
  environment_list *next;

  environment_list(const environment &, environment_list *);
};

environment_list::environment_list(const environment &e, environment_list *p)
: env(e), next(p)
{
}

inline int get_char()
{
  return getc(current_file);
}

void do_file(const char *filename)
{
  int npages = 0;
  if (filename[0] == '-' && filename[1] == '\0') {
    current_filename = "<standard input>";
    current_file = stdin;
  }
  else {
    errno = 0;
    current_file = fopen(filename, "r");
    if (current_file == 0) {
      error("can't open `%1'", filename);
      return;
    }
    current_filename = filename;
  }
  environment env;
  env.vpos = -1;
  env.hpos = -1;
  env.fontno = -1;
  env.height = 0;
  env.slant = 0;
  environment_list *env_list = 0;
  current_lineno = 1;
  int command;
  char *s;
  command = get_char();
  if (command == EOF)
    return;
  if (command != 'x')
    fatal("the first command must be `x T'");
  s = get_string();
  if (s[0] != 'T')
    fatal("the first command must be `x T'");
  char *dev = get_string();
  if (pr == 0) {
    device = strsave(dev);
    if (!font::load_desc())
      fatal("sorry, I can't continue");
  }
  else {
    if (device == 0 || strcmp(device, dev) != 0)
      fatal("all files must use the same device");
  }
  skip_line();
  env.size = 10*font::sizescale;
  command = get_char();
  if (command != 'x')
    fatal("the second command must be `x res'");
  s = get_string();
  if (s[0] != 'r')
    fatal("the second command must be `x res'");
  int n = get_integer();
  if (n != font::res)
    fatal("resolution does not match");
  n = get_integer();
  if (n != font::hor)
    fatal("horizontal resolution does not match");
  n = get_integer();
  if (n != font::vert)
    fatal("vertical resolution does not match");
  skip_line();
  command = get_char();
  if (command != 'x')
    fatal("the third command must be `x init'");
  s = get_string();
  if (s[0] != 'i')
    fatal("the third command must be `x init'");
  skip_line();
  if (pr == 0)
    pr = make_printer();
  while ((command = get_char()) != EOF) {
    switch (command) {
    case 's':
      env.size = get_integer();
      if (env.height == env.size)
	env.height = 0;
      break;
    case 'f':
      env.fontno = get_integer();
      break;
    case 'C':
      {
	if (npages == 0)
	  fatal("`C' command illegal before first `p' command");
	char *s = get_string();
	pr->set_special_char(s, &env);
      }
      break;
    case 'N':
      {
	if (npages == 0)
	  fatal("`N' command illegal before first `p' command");
	pr->set_numbered_char(get_integer(), &env);
      }
      break;
    case 'H':
      env.hpos = get_integer();
      break;
    case 'h':
      env.hpos += get_integer();
      break;
    case 'V':
      env.vpos = get_integer();
      break;
    case 'v':
      env.vpos += get_integer();
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
	int c = get_char();
	if (!csdigit(c))
	  fatal("digit expected");
	env.hpos += (command - '0')*10 + (c - '0');
      }
      // fall through
    case 'c':
      {
	if (npages == 0)
	  fatal("`c' command illegal before first `p' command");
	int c = get_char();
	if (c == EOF)
	  fatal("missing argument to `c' command");
	pr->set_ascii_char(c, &env);
      }
      break;
    case 'n':
      if (npages == 0)
	fatal("`n' command illegal before first `p' command");
      pr->end_of_line();
      (void)get_integer();
      (void)get_integer();
      break;
    case 'w':
    case ' ':
      break;
    case '\n':
      current_lineno++;
      break;
    case 'p':
      if (npages)
	pr->end_page(env.vpos);
      npages++;
      pr->begin_page(get_integer());
      env.vpos = 0;
      break;
    case '{':
      env_list = new environment_list(env, env_list);
      break;
    case '}':
      if (!env_list) {
	fatal("can't pop");
      }
      else {
	env = env_list->env;
	environment_list *tem = env_list;
	env_list = env_list->next;
	delete tem;
      }
      break;
    case 'u':
      {
	if (npages == 0)
	  fatal("`u' command illegal before first `p' command");
	int kern = get_integer();
	int c = get_char();
	while (c == ' ')
	  c = get_char();
	while (c != EOF) {
	  if (c == '\n') {
	    current_lineno++;
	    break;
	  }
	  int w;
	  pr->set_ascii_char(c, &env, &w);
	  env.hpos += w + kern;
	  c = get_char();
	  if (c == ' ')
	    break;
	}
      }
      break;
    case 't':
      {
	if (npages == 0)
	  fatal("`t' command illegal before first `p' command");
	int c;
	while ((c = get_char()) != EOF && c != ' ') {
	  if (c == '\n') {
	    current_lineno++;
	    break;
	  }
	  int w;
	  pr->set_ascii_char(c, &env, &w);
	  env.hpos += w;
	}
      }
      break;
    case '#':
      skip_line();
      break;
    case 'D':
      {
	if (npages == 0)
	  fatal("`D' command illegal before first `p' command");
	int c;
	while ((c = get_char()) == ' ')
	  ;
	int n;
	int *p = 0;
	int szp = 0;
	int np;
	for (np = 0; possibly_get_integer(&n); np++) {
	  if (np >= szp) {
	    if (szp == 0) {
	      szp = 16;
	      p = new int[szp];
	    }
	    else {
	      int *oldp = p;
	      p = new int[szp*2];
	      memcpy(p, oldp, szp*sizeof(int));
	      szp *= 2;
	      a_delete oldp;
	    }
	  }
	  p[np] = n;
	}
	pr->draw(c, p, np, &env);
	if (c == 'e') {
	  if (np > 0)
	    env.hpos += p[0];
	}
	else if (c == 'f' || c == 't')
	  ;
	else { 
	  int i;
	  for (i = 0; i < np/2; i++) {
	    env.hpos += p[i*2];
	    env.vpos += p[i*2 + 1];
	  }
	  // there might be an odd number of characters
	  if (i*2 < np)
	    env.hpos += p[i*2];
	}
	a_delete p;
	skip_line();
      }
      break;
    case 'x':
      {
	char *s = get_string();
	int suppress_skip = 0;
	switch (s[0]) {
	case 'i':
	  error("duplicate `x init' command");
	  break;
	case 'T':
	  error("duplicate `x T' command");
	  break;
	case 'r':
	  error("duplicate `x res' command");
	  break;
	case 'p':
	  break;
	case 's':
	  break;
	case 't':
	  break;
	case 'f':
	  {
	    int n = get_integer();
	    char *name = get_string();
	    pr->load_font(n, name);
	  }
	  break;
	case 'H':
	  env.height = get_integer();
	  if (env.height == env.size)
	    env.height = 0;
	  break;
	case 'S':
	  env.slant = get_integer();
	  break;
	case 'X':
	  if (npages == 0)
	    fatal("`x X' command illegal before first `p' command");
	  pr->special(get_string(1), &env);
	  suppress_skip = 1;
	  break;
	default:
	  error("unrecognised x command `%1'", s);
	}
	if (!suppress_skip)
	  skip_line();
      }
      break;
    default:
      error("unrecognised command code %1", int(command));
      skip_line();
      break;
    }
  }
  if (npages)
    pr->end_page(env.vpos);
}

int get_integer()
{
  int c = get_char();
  while (c == ' ')
    c = get_char();
  int neg = 0;
  if (c == '-') {
    neg = 1;
    c = get_char();
  }
  if (!csdigit(c))
    fatal("integer expected");
  int total = 0;
  do {
    total = total*10;
    if (neg)
      total -= c - '0';
    else
      total += c - '0';
    c = get_char();
  }  while (csdigit(c));
  if (c != EOF)
    ungetc(c, current_file);
  return total;
}

int possibly_get_integer(int *res)
{
  int c = get_char();
  while (c == ' ')
    c = get_char();
  int neg = 0;
  if (c == '-') {
    neg = 1;
    c = get_char();
  }
  if (!csdigit(c)) {
    if (c != EOF)
      ungetc(c, current_file);
    return 0;
  }
  int total = 0;
  do {
    total = total*10;
    if (neg)
      total -= c - '0';
    else
      total += c - '0';
    c = get_char();
  }  while (csdigit(c));
  if (c != EOF)
    ungetc(c, current_file);
  *res = total;
  return 1;
}


char *get_string(int is_long)
{
  static char *buf;
  static int buf_size;
  int c = get_char();
  while (c == ' ')
    c = get_char();
  for (int i = 0;; i++) {
    if (i >= buf_size) {
      if (buf_size == 0) {
	buf_size = 16;
	buf = new char[buf_size];
      }
      else {
	char *old_buf = buf;
	int old_size = buf_size;
	buf_size *= 2;
	buf = new char[buf_size];
	memcpy(buf, old_buf, old_size);
	a_delete old_buf;
      }
    }
    if ((!is_long && (c == ' ' || c == '\n')) || c == EOF) {
      buf[i] = '\0';
      break;
    }
    if (is_long && c == '\n') {
      current_lineno++;
      c = get_char();
      if (c == '+')
	c = '\n';
      else {
	buf[i] = '\0';
	break;
      }
    }  
    buf[i] = c;
    c = get_char();
  }
  if (c != EOF)
    ungetc(c, current_file);
  return buf;
}

void skip_line()
{
  int c;
  while ((c = get_char()) != EOF)
    if (c == '\n') {
      current_lineno++;
      break;
    }
}

