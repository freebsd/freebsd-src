/* cre-stddefh.c -- Check the size of a pointer and output an
   appropriate size_t declaration.

Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>

main (argc, argv)
     int argc;
     char **argv;
{
  if (sizeof (int *) == sizeof (unsigned long int))
    puts ("typedef unsigned long int size_t;");
  else
  if (sizeof (int *) == sizeof (unsigned int))
    puts ("typedef unsigned int size_t;");
  else
    {
      fprintf (stderr,
	       "%s: Can't find a reasonable definition for \"size_t\".\n",
	       argv[0]);
      exit (1);
    }

  exit (0);
}
