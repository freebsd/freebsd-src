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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}
#include "errarg.h"
#include "error.h"

extern void fatal_error_exit();

enum error_type { WARNING, ERROR, FATAL };

static void do_error_with_file_and_line(const char *filename, int lineno,
					error_type type, 
					const char *format, 
					const errarg &arg1,
					const errarg &arg2,
					const errarg &arg3)
{
  int need_space = 0;
  if (program_name) {
    fprintf(stderr, "%s:", program_name);
    need_space = 1;
  }
  if (lineno >= 0 && filename != 0) {
    if (strcmp(filename, "-") == 0)
      filename = "<standard input>";
    fprintf(stderr, "%s:%d:", filename, lineno);
    need_space = 1;
  }
  switch (type) {
  case FATAL:
    fputs("fatal error:", stderr);
    need_space = 1;
    break;
  case ERROR:
    break;
  case WARNING:
    fputs("warning:", stderr);
    need_space = 1;
    break;
  }
  if (need_space)
    fputc(' ', stderr);
  errprint(format, arg1, arg2, arg3);
  fputc('\n', stderr);
  fflush(stderr);
  if (type == FATAL)
    fatal_error_exit();
}
      

static void do_error(error_type type, 
		     const char *format, 
		     const errarg &arg1,
		     const errarg &arg2,
		     const errarg &arg3)
{
  do_error_with_file_and_line(current_filename, current_lineno,
			      type, format, arg1, arg2, arg3);
}


void error(const char *format, 
	   const errarg &arg1,
	   const errarg &arg2,
	   const errarg &arg3)
{
  do_error(ERROR, format, arg1, arg2, arg3);
}

void warning(const char *format, 
	     const errarg &arg1,
	     const errarg &arg2,
	     const errarg &arg3)
{
  do_error(WARNING, format, arg1, arg2, arg3);
}

void fatal(const char *format, 
	   const errarg &arg1,
	   const errarg &arg2,
	   const errarg &arg3)
{
  do_error(FATAL, format, arg1, arg2, arg3);
}

void error_with_file_and_line(const char *filename,
			      int lineno,
			      const char *format, 
			      const errarg &arg1,
			      const errarg &arg2,
			      const errarg &arg3)
{
  do_error_with_file_and_line(filename, lineno, 
			      ERROR, format, arg1, arg2, arg3);
}

void warning_with_file_and_line(const char *filename,
			      int lineno,
			      const char *format, 
			      const errarg &arg1,
			      const errarg &arg2,
			      const errarg &arg3)
{
  do_error_with_file_and_line(filename, lineno, 
			      WARNING, format, arg1, arg2, arg3);
}

void fatal_with_file_and_line(const char *filename,
			      int lineno,
			      const char *format, 
			      const errarg &arg1,
			      const errarg &arg2,
			      const errarg &arg3)
{
  do_error_with_file_and_line(filename, lineno, 
			      FATAL, format, arg1, arg2, arg3);
}
