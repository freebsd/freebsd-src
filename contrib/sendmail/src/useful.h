/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	@(#)useful.h	8.12 (Berkeley) 5/19/1998
 */

# include <sys/types.h>

/* support for bool type */
typedef int	bool;
#ifndef TRUE
# define TRUE	1
# define FALSE	0
#endif

# ifndef NULL
# define NULL	0
# endif /* NULL */

/* bit hacking */
# define bitset(bit, word)	(((word) & (bit)) != 0)

/* some simple functions */
# ifndef max
# define max(a, b)	((a) > (b) ? (a) : (b))
# define min(a, b)	((a) < (b) ? (a) : (b))
# endif

/* assertions */
# ifndef NASSERT
# define ASSERT(expr, msg, parm)\
	if (!(expr))\
	{\
		fprintf(stderr, "assertion botch: %s:%d: ", __FILE__, __LINE__);\
		fprintf(stderr, msg, parm);\
	}
# else /* NASSERT */
# define ASSERT(expr, msg, parm)
# endif /* NASSERT */

/* sccs id's */
# ifndef lint
#  ifdef __STDC__
#   define SCCSID(arg)	static char SccsId[] = #arg;
#  else
#   define SCCSID(arg)	static char SccsId[] = "arg";
#  endif
# else
#  define SCCSID(arg)
# endif
