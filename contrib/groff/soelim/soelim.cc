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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "stringclass.h"

static int include_list_length;
static char **include_list;

int compatible_flag = 0;

extern int interpret_lf_args(const char *);

int do_file(const char *filename);


static void
include_path_append(char *path)
{
  ++include_list_length;
  size_t nbytes = include_list_length * sizeof(include_list[0]);
  include_list = (char **)realloc((void *)include_list, nbytes);
  include_list[include_list_length - 1] = path;
}


void usage()
{
  fprintf(stderr, "usage: %s [ -vC ] [ -I file ] [ files ]\n", program_name);
  exit(1);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  include_path_append(".");
  int opt;
  while ((opt = getopt(argc, argv, "CI:v")) != EOF)
    switch (opt) {
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "GNU soelim version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'C':
      compatible_flag = 1;
      break;
    case 'I':
      include_path_append(optarg);
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  int nbad = 0;
  if (optind >= argc)
    nbad += !do_file("-");
  else
    for (int i = optind; i < argc; i++)
      nbad += !do_file(argv[i]);
  if (ferror(stdout) || fflush(stdout) < 0)
    fatal("output error");
  return nbad != 0;
}

void set_location()
{
  printf(".lf %d %s\n", current_lineno, current_filename);
}

void do_so(const char *line)
{
  const char *p = line;
  while (*p == ' ')
    p++;
  string filename;
  int success = 1;
  for (const char *q = p;
       success && *q != '\0' && *q != '\n' && *q != ' ';
       q++)
    if (*q == '\\') {
      switch (*++q) {
      case 'e':
      case '\\':
	filename += '\\';
	break;
      case ' ':
	filename += ' ';
	break;
      default:
	success = 0;
	break;
      }
    }
    else
      filename += char(*q);
  if (success && filename.length() > 0) {
    filename += '\0';
    const char *fn = current_filename;
    int ln = current_lineno;
    current_lineno--;
    if (do_file(filename.contents())) {
      current_filename = fn;
      current_lineno = ln;
      set_location();
      return;
    }
    current_lineno++;
  }
  fputs(".so", stdout);
  fputs(line, stdout);
}

int do_file(const char *filename)
{
  FILE *fp;
  string whole_filename;
  if (strcmp(filename, "-") == 0) {
    fp = stdin;
    whole_filename = filename;
    whole_filename += '\0';
  }
  else if (filename[0] == '/') {
    whole_filename = filename;
    whole_filename += '\0';
    errno = 0;
    fp = fopen(filename, "r");
    if (fp == 0) {
      error("can't open `%1': %2", filename, strerror(errno));
      return 0;
    }
  }
  else {
    size_t j;
    for (j = 0; j < include_list_length; ++j)
    {
      char *path = include_list[j];
      if (0 == strcmp(path, "."))
      	whole_filename = filename;
      else
        whole_filename = string(path) + "/" + filename;
      whole_filename += '\0';
      errno = 0;
      fp = fopen(whole_filename.contents(), "r");
      if (fp != 0)
      	break;
      if (errno != ENOENT) {
        error("can't open `%1': %2",
	      whole_filename.contents(), strerror(errno));
        return 0;
      }
    }
    if (j >= include_list_length)
    {
      errno = ENOENT;
      error("can't open `%1': %2", filename, strerror(errno));
      return 0;
    }
  }
  current_filename = whole_filename.contents();
  current_lineno = 1;
  set_location();
  enum { START, MIDDLE, HAD_DOT, HAD_s, HAD_so, HAD_l, HAD_lf } state = START;
  for (;;) {
    int c = getc(fp);
    if (c == EOF)
      break;
    switch (state) {
    case START:
      if (c == '.')
	state = HAD_DOT;
      else {
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case MIDDLE:
      putchar(c);
      if (c == '\n') {
	current_lineno++;
	state = START;
      }
      break;
    case HAD_DOT:
      if (c == 's')
	state = HAD_s;
      else if (c == 'l')
	state = HAD_l;
      else {
	putchar('.');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_s:
      if (c == 'o')
	state = HAD_so;
      else  {
	putchar('.');
	putchar('s');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_so:
      if (c == ' ' || c == '\n' || compatible_flag) {
	string line;
	for (; c != EOF && c != '\n'; c = getc(fp))
	  line += c;
	current_lineno++;
	line += '\n';
	line += '\0';
	do_so(line.contents());
	state = START;
      }
      else {
	fputs(".so", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    case HAD_l:
      if (c == 'f')
	state = HAD_lf;
      else {
	putchar('.');
	putchar('l');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_lf:
      if (c == ' ' || c == '\n' || compatible_flag) {
	string line;
	for (; c != EOF && c != '\n'; c = getc(fp))
	  line += c;
	current_lineno++;
	line += '\n';
	line += '\0';
	interpret_lf_args(line.contents());
	printf(".lf%s", line.contents());
	state = START;
      }
      else {
	fputs(".lf", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    default:
      assert(0);
    }
  }
  switch (state) {
  case HAD_DOT:
    fputs(".\n", stdout);
    break;
  case HAD_l:
    fputs(".l\n", stdout);
    break;
  case HAD_s:
    fputs(".s\n", stdout);
    break;
  case HAD_lf:
    fputs(".lf\n", stdout);
    break;
  case HAD_so:
    fputs(".so\n", stdout);
    break;
  case MIDDLE:
    putc('\n', stdout);
    break;
  case START:
    break;
  }
  if (fp != stdin)
    fclose(fp);
  current_filename = 0;
  return 1;
}
