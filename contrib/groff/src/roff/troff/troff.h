// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include "lib.h"
#include "assert.h"
#include "device.h"
#include "searchpath.h"

void cleanup_and_exit(int n);

typedef int units;

extern units scale(units n, units x, units y); // scale n by x/y

extern units units_per_inch;

extern int ascii_output_flag;
extern int suppress_output_flag;
extern int is_html;

extern int tcommand_flag;
extern int vresolution;
extern int hresolution;
extern int sizescale;

extern search_path *mac_path;

#include "cset.h"
#include "cmap.h"
#include "errarg.h"
#include "error.h"

enum warning_type {
  WARN_CHAR = 01,
  WARN_NUMBER = 02,
  WARN_BREAK = 04,
  WARN_DELIM = 010,
  WARN_EL = 020,
  WARN_SCALE = 040,
  WARN_RANGE = 0100,
  WARN_SYNTAX = 0200,
  WARN_DI = 0400,
  WARN_MAC = 01000,
  WARN_REG = 02000,
  WARN_TAB = 04000,
  WARN_RIGHT_BRACE = 010000,
  WARN_MISSING = 020000,
  WARN_INPUT = 040000,
  WARN_ESCAPE = 0100000,
  WARN_SPACE = 0200000,
  WARN_FONT = 0400000,
  WARN_IG =  01000000
  // change WARN_TOTAL if you add more warning types
};

const int WARN_TOTAL = 01777777;

int warning(warning_type, const char *,
	    const errarg & = empty_errarg,
	    const errarg & = empty_errarg,
	    const errarg & = empty_errarg);
