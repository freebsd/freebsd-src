/* Copyright (C) 1998, 1999, 2001 Free Software Foundation, Inc.
   This file is derived from the one in the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>

/* Disable the definition of mkstemp to rpl_mkstemp (from config.h) in this
   file.  Otherwise, we'd get conflicting prototypes for rpl_mkstemp on
   most systems.  */
#undef mkstemp

#include <stdio.h>
#include <stdlib.h>

#ifndef __GT_FILE
# define __GT_FILE 0
#endif

int __gen_tempname ();

/* Generate a unique temporary file name from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the filename unique.
   Then open the file and return a fd. */
int
rpl_mkstemp (char *template)
{
  return __gen_tempname (template, __GT_FILE);
}
