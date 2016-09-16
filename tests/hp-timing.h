/* High precision, low overhead timing functions.  Generic version.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1998.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HP_TIMING_H
#define _HP_TIMING_H	1


/* There are no generic definitions for the times.  We could write something
   using the `gettimeofday' system call where available but the overhead of
   the system call might be too high.

   In case a platform supports timers in the hardware the following macros
   and types must be defined:

   - HP_TIMING_AVAIL: test for availability.

   - HP_TIMING_INLINE: this macro is non-zero if the functionality is not
     implemented using function calls but instead uses some inlined code
     which might simply consist of a few assembler instructions.  We have to
     know this since we might want to use the macros here in places where we
     cannot make function calls.

   - hp_timing_t: This is the type for variables used to store the time
     values.

   - HP_TIMING_ZERO: clear `hp_timing_t' object.

   - HP_TIMING_NOW: place timestamp for current time in variable given as
     parameter.

   - HP_TIMING_DIFF_INIT: do whatever is necessary to be able to use the
     HP_TIMING_DIFF macro.

   - HP_TIMING_DIFF: compute difference between two times and store it
     in a third.  Source and destination might overlap.

   - HP_TIMING_ACCUM: add time difference to another variable.  This might
     be a bit more complicated to implement for some platforms as the
     operation should be thread-safe and 64bit arithmetic on 32bit platforms
     is not.

   - HP_TIMING_ACCUM_NT: this is the variant for situations where we know
     there are no threads involved.

   - HP_TIMING_PRINT: write decimal representation of the timing value into
     the given string.  This operation need not be inline even though
     HP_TIMING_INLINE is specified.

*/

/* Provide dummy definitions.  */
#define HP_TIMING_AVAIL		(0)
#define HP_TIMING_INLINE	(0)
typedef int hp_timing_t;
#define HP_TIMING_ZERO(Var)
#define HP_TIMING_NOW(var)
#define HP_TIMING_DIFF_INIT()
#define HP_TIMING_DIFF(Diff, Start, End)
#define HP_TIMING_ACCUM(Sum, Diff)
#define HP_TIMING_ACCUM_NT(Sum, Diff)
#define HP_TIMING_PRINT(Buf, Len, Val)

/* Since this implementation is not available we tell the user about it.  */
#define HP_TIMING_NONAVAIL	1

#endif	/* hp-timing.h */
