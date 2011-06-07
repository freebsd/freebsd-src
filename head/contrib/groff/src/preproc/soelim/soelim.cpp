// -*- C++ -*-
/* Copyright (C) 1989-1992, 2000, 2001, 2003, 2004, 2005
   Free Software Foundation, Inc.
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

#include "lib.h"

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "nonposix.h"
#include "searchpath.h"

// The include search path initially contains only the current directory.
static search_path include_search_path(0, 0, 0, 1);

int compatible_flag = 0;
int raw_flag = 0;
int tex_flag = 0;

extern "C" const char *Version_string;

int do_file(const char *filename);


void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [ -Crtv ] [ -I file ] [ files ]\n", program_name);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  int opt;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((opt = getopt_long(argc, argv, "CI:rtv", long_options, NULL)) != EOF)
    switch (opt) {
    case 'v':
      {
	printf("GNU soelim (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case 'C':
      compatible_flag = 1;
      break;
    case 'I':
      include_search_path.command_line_dir(optarg);
      break;
    case 'r':
      raw_flag = 1;
      break;
    case 't':
      tex_flag = 1;
      break;
    case CHAR_MAX + 1: // --help
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
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
  if(!raw_flag) {
    if(!tex_flag)
      printf(".lf %d %s\n", current_lineno, current_filename);
    else
      printf("%% file %s, line %d\n", current_filename, current_lineno);
  }
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
  char *file_name_in_path = 0;
  FILE *fp = include_search_path.open_file_cautious(filename,
						    &file_name_in_path);
  int err = errno;
  string whole_filename(file_name_in_path ? file_name_in_path : filename);
  whole_filename += '\0';
  a_delete file_name_in_path;
  if (fp == 0) {
    error("can't open `%1': %2", whole_filename.contents(), strerror(err));
    return 0;
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
