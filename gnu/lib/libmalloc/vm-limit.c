/* Functions for memory limit warnings.
   Copyright (C) 1990, 1992 Free Software Foundation, Inc.


This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifdef emacs
#include "config.h"
#include "lisp.h"
#endif

#ifndef emacs
#include <stddef.h>
typedef size_t SIZE;
typedef void *POINTER;
#define EXCEEDS_LISP_PTR(x) 0
#endif

#include "mem-limits.h"

/*
  Level number of warnings already issued.
  0 -- no warnings issued.
  1 -- 75% warning already issued.
  2 -- 85% warning already issued.
  3 -- 95% warning issued; keep warning frequently.
*/
static int warnlevel;

/* Function to call to issue a warning;
   0 means don't issue them.  */
static void (*warn_function) ();

/* Get more memory space, complaining if we're near the end. */

static void
check_memory_limits ()
{
  extern POINTER (*__morecore) ();

  register POINTER cp;
  int five_percent;
  int data_size;

  if (lim_data == 0)
    get_lim_data ();
  five_percent = lim_data / 20;

  /* Find current end of memory and issue warning if getting near max */
  cp = (char *) (*__morecore) (0);
  data_size = (char *) cp - (char *) data_space_start;

  if (warn_function)
    switch (warnlevel)
      {
      case 0: 
	if (data_size > five_percent * 15)
	  {
	    warnlevel++;
	    (*warn_function) ("Warning: past 75% of memory limit");
	  }
	break;

      case 1: 
	if (data_size > five_percent * 17)
	  {
	    warnlevel++;
	    (*warn_function) ("Warning: past 85% of memory limit");
	  }
	break;

      case 2: 
	if (data_size > five_percent * 19)
	  {
	    warnlevel++;
	    (*warn_function) ("Warning: past 95% of memory limit");
	  }
	break;

      default:
	(*warn_function) ("Warning: past acceptable memory limits");
	break;
      }

  /* If we go down below 70% full, issue another 75% warning
     when we go up again.  */
  if (data_size < five_percent * 14)
    warnlevel = 0;
  /* If we go down below 80% full, issue another 85% warning
     when we go up again.  */
  else if (warnlevel > 1 && data_size < five_percent * 16)
    warnlevel = 1;
  /* If we go down below 90% full, issue another 95% warning
     when we go up again.  */
  else if (warnlevel > 2 && data_size < five_percent * 18)
    warnlevel = 2;

  if (EXCEEDS_LISP_PTR (cp))
    (*warn_function) ("Warning: memory in use exceeds lisp pointer size");
}

/* Cause reinitialization based on job parameters;
   also declare where the end of pure storage is. */

void
memory_warnings (start, warnfun)
     POINTER start;
     void (*warnfun) ();
{
  extern void (* __after_morecore_hook) ();     /* From gmalloc.c */

  if (start)
    data_space_start = start;
  else
    data_space_start = start_of_data ();

  warn_function = warnfun;
  __after_morecore_hook = check_memory_limits;
}
