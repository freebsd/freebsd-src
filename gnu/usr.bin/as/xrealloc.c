/* xrealloc.c - new memory or bust

   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/*

  NAME
  xrealloc () - get more memory or bust
  INDEX
  xrealloc () uses realloc ()
  SYNOPSIS
  char   *my_memory;

  my_memory = xrealloc (my_memory, 42);
  / * my_memory gets (perhaps new) address of 42 chars * /

  DESCRIPTION

  Use xrealloc () as an "error-free" realloc ().It does almost the same
  job.  When it cannot honour your request for memory it BOMBS your
  program with a "virtual memory exceeded" message.  Realloc() returns
  NULL and does not bomb your program.

  SEE ALSO
  realloc ()
  */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif


#if __STDC__ == 1
#include <stdlib.h>
#else
#ifdef USG
#include <malloc.h>
#else
char   *realloc ();
#endif /* USG */
#endif /* not __STDC__ */

#define error as_fatal

char   *
    xrealloc (ptr, n)
register char  *ptr;
long    n;
{
	void	error();

	if ((ptr = realloc (ptr, (unsigned)n)) == 0)
	    error ("virtual memory exceeded");
	return (ptr);
}

/* end of xrealloc.c */
