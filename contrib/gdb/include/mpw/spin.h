/* Progress macros that use SpinCursor in MPW.
   Copyright (C) 1994 Free Software Foundation, Inc.

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

#ifndef _SPIN_H
#define _SPIN_H

/* For MPW, progress macros just need to "spin the cursor" frequently,
   preferably several times per second on a 68K Mac.  */

/* In order to determine if we're meeting the goal, define this macro
   and information about frequency of spinning will be collected and
   displayed.  */

#define SPIN_MEASUREMENT

#include <CursorCtl.h>

/* Programs use this macro to indicate the start of a lengthy
   activity.  STR identifies the particular activity, while N
   indicates the expected duration, in unspecified units.  If N is
   zero, then the expected time to completion is unknown.  */

#undef START_PROGRESS
#define START_PROGRESS(STR,N) mpw_start_progress (STR, N, __FILE__, __LINE__);

/* Programs use this macro to indicate that progress has been made on a
   lengthy activity.  */

#undef PROGRESS
#ifdef SPIN_MEASUREMENT
#define PROGRESS(X) mpw_progress_measured (X, __FILE__, __LINE__);
#else
#define PROGRESS(X) mpw_progress (X);
#endif 

/* Programs use this macro to indicate the end of a lengthy activity.
   STR must match a STR passed to START_PROGRESS previously.  */

#undef END_PROGRESS
#define END_PROGRESS(STR) mpw_end_progress (STR, __FILE__, __LINE__);

extern void mpw_start_progress (char *, int, char *, int);

extern void mpw_progress (int);

extern void mpw_progress_measured (int, char *, int);

extern void mpw_end_progress (char *, char *, int);

#endif /* _SPIN_H */
