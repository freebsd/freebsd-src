/* xmalloc.c - get memory or bust

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
  xmalloc() - get memory or bust
  INDEX
  xmalloc() uses malloc()

  SYNOPSIS
  char *	my_memory;

  my_memory = xmalloc(42); / * my_memory gets address of 42 chars * /

  DESCRIPTION

  Use xmalloc() as an "error-free" malloc(). It does almost the same job.
  When it cannot honour your request for memory it BOMBS your program
  with a "virtual memory exceeded" message. Malloc() returns NULL and
  does not bomb your program.

  SEE ALSO
  malloc()

  */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include <stdio.h>

#if __STDC__ == 1
#include <stdlib.h>
#else
#ifdef USG
#include <malloc.h>
#else
char *	malloc();
#endif /* USG */
#endif /* not __STDC__ */

#define error as_fatal

char * xmalloc(n)
long n;
{
	char *	retval;
	void	error();

	if ((retval = malloc ((unsigned)n)) == NULL)
	    {
		    error("virtual memory exceeded");
	    }
	return (retval);
}

/* end of xmalloc.c */
