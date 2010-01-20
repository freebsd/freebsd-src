// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2003, 2004
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

extern void fatal_with_file_and_line(const char *filename, int lineno,
				     const char *format,
				     const errarg &arg1 = empty_errarg,
				     const errarg &arg2 = empty_errarg,
				     const errarg &arg3 = empty_errarg);

extern void error_with_file_and_line(const char *filename, int lineno,
				     const char *format,
				     const errarg &arg1 = empty_errarg,
				     const errarg &arg2 = empty_errarg,
				     const errarg &arg3 = empty_errarg);

extern void warning_with_file_and_line(const char *filename, int lineno,
				     const char *format,
				     const errarg &arg1 = empty_errarg,
				     const errarg &arg2 = empty_errarg,
				     const errarg &arg3 = empty_errarg);

extern void fatal(const char *,
		  const errarg &arg1 = empty_errarg,
		  const errarg &arg2 = empty_errarg,
		  const errarg &arg3 = empty_errarg);

extern void error(const char *,
		  const errarg &arg1 = empty_errarg,
		  const errarg &arg2 = empty_errarg,
		  const errarg &arg3 = empty_errarg);

extern void warning(const char *,
		    const errarg &arg1 = empty_errarg,
		    const errarg &arg2 = empty_errarg,
		    const errarg &arg3 = empty_errarg);


extern "C" const char *program_name;
extern int current_lineno;
extern const char *current_filename;
extern const char *current_source_filename;
