/* Definitions for complaint handling during symbol reading in GDB.
   Copyright (C) 1990, 1991, 1992  Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#if !defined (COMPLAINTS_H)
#define COMPLAINTS_H


/* Support for complaining about things in the symbol file that aren't
   catastrophic.

   Each such thing gets a counter.  The first time we have the problem,
   during a symbol read, we report it.  At the end of symbol reading,
   if verbose, we report how many of each problem we had.  */

struct complaint
{
  char *message;
  unsigned counter;
  struct complaint *next;
};

/* Root of the chain of complaints that have at some point been issued. 
   This is used to reset the counters, and/or report the total counts.  */

extern struct complaint complaint_root[1];

/* Functions that handle complaints.  (in complaints.c)  */

extern void
complain PARAMS ((struct complaint *, ...));

extern void
clear_complaints PARAMS ((int, int));


#endif	/* !defined (COMPLAINTS_H) */
