/* version.c -- binutils version information
   Copyright 1991, 96, 97, 98, 99, 2000, 2001 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "bfd.h"
#include "bucomm.h"

/* This is the version numbers for the binutils.  They all change in
   lockstep -- it's easier that way. */

const char *program_version = VERSION;

/* Print the version number and copyright information, and exit.  This
   implements the --version option for the various programs.  */

void
print_version (name)
     const char *name;
{
  /* This output is intended to follow the GNU standards document.  */
  /* xgettext:c-format */
  printf ("GNU %s %s\n", name, program_version);
  printf (_("Copyright 1997, 98, 99, 2000, 2001 Free Software Foundation, Inc.\n"));
  printf (_("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n"));
  exit (0);
}
