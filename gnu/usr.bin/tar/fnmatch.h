/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifndef	_FNMATCH_H

#define	_FNMATCH_H	1

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __FreeBSD__
#include <sys/cdefs.h>
#else
#if defined (__cplusplus) || (defined (__STDC__) && __STDC__)
#undef	__P
#define	__P(args)	args
#else /* Not C++ or ANSI C.  */
#undef	__P
#define	__P(args)	()
#undef	const
#define	const
#endif /* C++ or ANSI C.  */
#endif

/* Bits set in the FLAGS argument to `fnmatch'.  */
#ifdef FNM_PATHNAME    /* Because it is already defined in <unistd.h> */
#undef FNM_PATHNAME
#endif
#define	FNM_PATHNAME	(1 << 0)/* No wildcard can ever match `/'.  */
#define	FNM_NOESCAPE	(1 << 1)/* Backslashes don't quote special chars.  */
#define	FNM_PERIOD	(1 << 2)/* Leading `.' is matched only explicitly.  */
#define	__FNM_FLAGS	(FNM_PATHNAME|FNM_NOESCAPE|FNM_PERIOD|FNM_LEADING_DIR)

#if !defined (_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 2 || defined (_BSD_SOURCE)
#define	FNM_LEADING_DIR	(1 << 3)/* Ignore `/...' after a match.  */
#define	FNM_FILE_NAME	FNM_PATHNAME
#endif

/* Value returned by `fnmatch' if STRING does not match PATTERN.  */
#define	FNM_NOMATCH	1

/* Match STRING against the filename pattern PATTERN,
   returning zero if it matches, FNM_NOMATCH if not.  */
extern int fnmatch __P ((const char *__pattern, const char *__string,
			 int __flags));

#ifdef	__cplusplus
}
#endif

#endif /* fnmatch.h */
