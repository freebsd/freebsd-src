/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)less.h	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 */

#define MAXVARLENGTH	(20)

#define	NULL_POSITION	((off_t)(-1))

#define	EOI		(0)
#define	READ_INTR	(-2)

/* Special chars used to tell put_line() to do something special */
#define	UL_CHAR		'\201'		/* Enter underline mode */
#define	UE_CHAR		'\202'		/* Exit underline mode */
#define	BO_CHAR		'\203'		/* Enter boldface mode */
#define	BE_CHAR		'\204'		/* Exit boldface mode */

#define CONTROL_CHAR(c)         (!isprint(c))
#define	CARAT_CHAR(c)		((c == '\177') ? '?' : (c | 0100))

#define	TOP		(0)
#define	TOP_PLUS_ONE	(1)
#define	BOTTOM		(-1)
#define	BOTTOM_PLUS_ONE	(-2)
#define	MIDDLE		(-3)

/* The return type of runmacro() */
enum runmacro { OK=0, TOOMACRO, BADMACRO, NOMACRO, BADCOMMAND };

#define NOFLAGS 0
#define FORCE_OPEN 0         /* edit() flag */
#define NO_FORCE_OPEN 1      /* edit() flag */

#ifdef DEFINEGLOBALS
#define GLOBAL(var, val) var = val
#else
#define GLOBAL(var, val) extern var
#endif

/*
 * This style of error-reporting (see also command.c) is only used by some
 * code.  Eventually most of the code should use it, since it is becoming
 * inconvenient to have John Q. random function() calling error().
 *
 * This style of error-reporting still leaves somewhat to be desired....
 *
 * Note that more(1) needs to potentially work under low-memory conditions
 * (such as may occur when all available memory has been sucked-up by
 * the file buffer in ch.c).
 */

/* Be careful about ordering correctly!!  (must match deferrinit_) */
enum error { E_OK=0, E_AMBIG, E_BADMATH, E_BADVAR, E_BOGCOM, E_CANTPARSE,
             E_CANTXPND, E_COMPLIM, E_EXTERN, E_NOMAC, E_MALLOC, E_NONUM,
             E_NOSTR, E_NOTOG, E_NULL };

/* Listed here for reference only.  Be careful about ordering correctly!! */
#define deferrinit_ { \
	"",						/* E_OK */ \
	"ambigious macro",				/* E_AMBIG */ \
	"invalid arithmetic expression",                /* E_BADMATH */ \
	"bad variable",                                 /* E_BADVAR */ \
	"bogus command",				/* E_BOGCOM */ \
	"could not parse command string",		/* E_CANTPARSE */ \
	"could not expand macro",			/* E_CANTXPND */ \
	"compile time limit",                           /* E_COMPLIM */ \
	"external dependency error",                    /* E_EXTERN */ \
	"could not find match for macro",               /* E_NOMAC */ \
	"malloc() failed",				/* E_MALLOC */ \
	"missing numeric argument to command",          /* E_NONUM */ \
	"missing string argument to command",           /* E_NOSTR */ \
	"bad n-toggle argument to command",             /* E_NOTOG */ \
	"to the unknown error",                         /* E_NULL */ \
}
GLOBAL(const char *deferr[], deferrinit_ );

/*
 * It is possible for erreur to become unsynchronized from errstr if
 * its users aren't careful.  Access through the macros is considered
 * careful.
 */
GLOBAL(enum error erreur, NULL);
GLOBAL(char *errstr, NULL);  /* must point be null or free()'ble */

#define SETERR(e) do { \
		erreur = (e); \
		if (errstr) free(errstr); \
		errstr = NULL; \
	} while (0)
/* SETERRSTR() also exists.  It is in command.c */

/*
 * An emalloc() traditionally never fails, but fmalloc() may fail, hence
 * the non-standard name.  The fmalloc() is just syntactic sugar that sets
 * erreur for the user.
 *
 * fmalloc(size, pointer-to-new-memory);
 *
 * Don't compile this puppy with -Wall or she'll squeel loud!
 */

#define FMALLOC(s,v) ((((v) = malloc(s)) ? 0 : \
	((errstr ? free(errstr), errstr=NULL : 0), erreur = E_MALLOC)), (v))
