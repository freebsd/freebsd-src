/* stdc.h -- macros to make source compile on both ANSI C and K&R C
   compilers. */

/* Copyright (C) 1993 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#if !defined (_RL_STDC_H_)
#define _RL_STDC_H_

/* Adapted from BSD /usr/include/sys/cdefs.h. */

/* A function can be defined using prototypes and compile on both ANSI C
   and traditional C compilers with something like this:
	extern char *func __P((char *, char *, int)); */

#if !defined (__P)
#  if defined (__STDC__) || defined (__GNUC__) || defined (__cplusplus)
#    define __P(protos) protos
#  else
#    define __P(protos) ()
#  endif
#endif

#endif /* !_RL_STDC_H_ */
