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

#include "eqn.h"
#include "stringclass.h"
#include "device.h"
#include "searchpath.h"
#include "macropath.h"

#define STARTUP_FILE "eqnrc"

extern int yyparse();

static char *delim_search(char *, int);
static int inline_equation(FILE *, string &, string &);

char start_delim = '\0';
char end_delim = '\0';
int non_empty_flag;
int inline_flag;
int draw_flag = 0;
int one_size_reduction_flag = 0;
int compatible_flag = 0;
int no_newline_in_delim_flag = 0;

int read_line(FILE *fp, string *p)
{
  p->clear();
  int c = -1;
  while ((c = getc(fp)) != EOF) {
    if (!illegal_input_char(c))
      *p += char(c);
    else
      error("illegal input character code `%1'", c);
    if (c == '\n')
      break;
  }
  current_lineno++;
  return p->length() > 0;
}

void do_file(FILE *fp, const char *filename)
{
  string linebuf;
  string str;
  printf(".lf 1 %s\n", filename);
  current_filename = filename;
  current_lineno = 0;
  while (read_line(fp, &linebuf)) {
    if (linebuf.length() >= 4
	&& linebuf[0] == '.' && linebuf[1] == 'l' && linebuf[2] == 'f'
	&& (linebuf[3] == ' ' || linebuf[3] == '\n' || compatible_flag)) {
      put_string(linebuf, stdout);
      linebuf += '\0';
      if (interpret_lf_args(linebuf.contents() + 3))
	current_lineno--;
    }
    else if (linebuf.length() >= 4
	     && linebuf[0] == '.'
	     && linebuf[1] == 'E'
	     && linebuf[2] == 'Q'
	     && (linebuf[3] == ' ' || linebuf[3] == '\n' || compatible_flag)) {
      put_string(linebuf, stdout);
      int start_lineno = current_lineno + 1;
      str.clear();
      for (;;) {
	if (!read_line(fp, &linebuf))
	  fatal("end of file before .EN");
	if (linebuf.length() >= 3 && linebuf[0] == '.' && linebuf[1] == 'E') {
	  if (linebuf[2] == 'N'
	      && (linebuf.length() == 3 || linebuf[3] == ' '
		  || linebuf[3] == '\n' || compatible_flag))
	    break;
	  else if (linebuf[2] == 'Q' && linebuf.length() > 3
		   && (linebuf[3] == ' ' || linebuf[3] == '\n'
		       || compatible_flag))
	    fatal("nested .EQ");
	}
	str += linebuf;
      }
      str += '\0';
      start_string();
      init_lex(str.contents(), current_filename, start_lineno);
      non_empty_flag = 0;
      inline_flag = 0;
      yyparse();
      if (non_empty_flag) {
	printf(".lf %d\n", current_lineno - 1);
	output_string();
      }
      restore_compatibility();
      printf(".lf %d\n", current_lineno);
      put_string(linebuf, stdout);
    }
    else if (start_delim != '\0' && linebuf.search(start_delim) >= 0
	     && inline_equation(fp, linebuf, str))
      ;
    else
      put_string(linebuf, stdout);
  }
  current_filename = 0;
  current_lineno = 0;
}

/* Handle an inline equation.  Return 1 if it was an inline equation,
0 otherwise. */

static int inline_equation(FILE *fp, string &linebuf, string &str)
{
  linebuf += '\0';
  char *ptr = &linebuf[0];
  char *start = delim_search(ptr, start_delim);
  if (!start) {
    // It wasn't a delimiter after all.
    linebuf.set_length(linebuf.length() - 1); // strip the '\0'
    return 0;
  }
  start_string();
  inline_flag = 1;
  for (;;) {
    if (no_newline_in_delim_flag && strchr(start + 1, end_delim) == 0) {
      error("missing `%1'", end_delim);
      char *nl = strchr(start + 1, '\n');
      if (nl != 0)
	*nl = '\0';
      do_text(ptr);
      break;
    }
    int start_lineno = current_lineno;
    *start = '\0';
    do_text(ptr);
    ptr = start + 1;
    str.clear();
    for (;;) {
      char *end = strchr(ptr, end_delim);
      if (end != 0) {
	*end = '\0';
	str += ptr;
	ptr = end + 1;
	break;
      }
      str += ptr;
      if (!read_line(fp, &linebuf))
	fatal("end of file before `%1'", end_delim);
      linebuf += '\0';
      ptr = &linebuf[0];
    }
    str += '\0';
    init_lex(str.contents(), current_filename, start_lineno);
    yyparse();
    start = delim_search(ptr, start_delim);
    if (start == 0) {
      char *nl = strchr(ptr, '\n');
      if (nl != 0)
	*nl = '\0';
      do_text(ptr);
      break;
    }
  }
  printf(".lf %d\n", current_lineno);
  output_string();
  restore_compatibility();
  printf(".lf %d\n", current_lineno + 1);
  return 1;
}

/* Search for delim.  Skip over number register and string names etc. */

static char *delim_search(char *ptr, int delim)
{
  while (*ptr) {
    if (*ptr == delim)
      return ptr;
    if (*ptr++ == '\\') {
      switch (*ptr) {
      case 'n':
      case '*':
      case 'f':
      case 'g':
      case 'k':
	switch (*++ptr) {
	case '\0':
	case '\\':
	  break;
	case '(':
	  if (*++ptr != '\\' && *ptr != '\0' && *++ptr != '\\' && *ptr != '\0')
	      ptr++;
	  break;
	case '[':
	  while (*++ptr != '\0')
	    if (*ptr == ']') {
	      ptr++;
	      break;
	    }
	  break;
	default:
	  ptr++;
	  break;
	}
	break;
      case '\\':
      case '\0':
	break;
      default:
	ptr++;
	break;
      }
    }
  }
  return 0;
}

void usage()
{
  fprintf(stderr,
        "usage: %s [ -rvDCNR ] -dxx -fn -sn -pn -mn -Mdir -Ts [ files ... ]\n",
	program_name);
  exit(1);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int opt;
  int load_startup_file = 1;
  while ((opt = getopt(argc, argv, "DCRvd:f:p:s:m:T:M:rN")) != EOF)
    switch (opt) {
    case 'C':
      compatible_flag = 1;
      break;
    case 'R':			// don't load eqnchar
      load_startup_file = 0;
      break;
    case 'M':
      macro_path.command_line_dir(optarg);
      break;
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "GNU eqn version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'd':
      if (optarg[0] == '\0' || optarg[1] == '\0')
	error("-d requires two character argument");
      else if (illegal_input_char(optarg[0]))
	error("bad delimiter `%1'", optarg[0]);
      else if (illegal_input_char(optarg[1]))
	error("bad delimiter `%1'", optarg[1]);
      else {
	start_delim = optarg[0];
	end_delim = optarg[1];
      }
      break;
    case 'f':
      set_gfont(optarg);
      break;
    case 'T':
      device = optarg;
      break;
    case 's':
      if (!set_gsize(optarg))
	error("invalid size `%1'", optarg);
      break;
    case 'p':
      {
	int n;
	if (sscanf(optarg, "%d", &n) == 1)
	  set_script_reduction(n);
	else
	  error("bad size `%1'", optarg);
      }
      break;
    case 'm':
      {
	int n;
	if (sscanf(optarg, "%d", &n) == 1)
	  set_minimum_size(n);
	else
	  error("bad size `%1'", optarg);
      }
      break;
    case 'r':
      one_size_reduction_flag = 1;
      break;
    case 'D':
      warning("-D option is obsolete: use `set draw_lines 1' instead");
      draw_flag = 1;
      break;
    case 'N':
      no_newline_in_delim_flag = 1;
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  init_table(device);
  init_char_table();
  printf(".if !'\\*(.T'%s' "
	 ".tm warning: %s should have been given a `-T\\*(.T' option\n",
	 device, program_name);
  if (load_startup_file) {
    char *path;
    FILE *fp = macro_path.open_file(STARTUP_FILE, &path);
    if (fp) {
      do_file(fp, path);
      fclose(fp);
      a_delete path;
    }
  }
  if (optind >= argc)
    do_file(stdin, "-");
  else
    for (int i = optind; i < argc; i++)
      if (strcmp(argv[i], "-") == 0)
	do_file(stdin, "-");
      else {
	errno = 0;
	FILE *fp = fopen(argv[i], "r");
	if (!fp)
	  fatal("can't open `%1': %2", argv[i], strerror(errno));
	else {
	  do_file(fp, argv[i]);
	  fclose(fp);
	}
      }
  if (ferror(stdout) || fflush(stdout) < 0)
    fatal("output error");
  return 0;
}
