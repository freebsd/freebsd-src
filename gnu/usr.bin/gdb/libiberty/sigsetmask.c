/* Version of sigsetmask.c
   Copyright 1991, 1992 Free Software Foundation, Inc.
   Written by Steve Chamberlain (sac@cygnus.com).
   Contributed by Cygnus Support.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* Set the current signal mask to the set provided, and return the 
   previous value */

#define _POSIX_SOURCE
#include <ansidecl.h>
#include <signal.h>

#ifdef SIG_SETMASK
int
DEFUN(sigsetmask,(set),
      int set)
{
    sigset_t new;
    sigset_t old;
    
    sigemptyset (&new);
    if (set != 0) {
      abort();	/* FIXME, we don't know how to translate old mask to new */
    }
    sigprocmask(SIG_SETMASK, &new, &old);
    return 1;	/* FIXME, we always return 1 as old value.  */
}
#endif
