/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)vi.h	8.39 (Berkeley) 3/23/94
 */

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
typedef struct _cb		CB;
typedef struct _ch		CH;
typedef struct _chname		CHNAME;
typedef struct _excmdarg	EXCMDARG;
typedef struct _exf		EXF;
typedef struct _fref		FREF;
typedef struct _gs		GS;
typedef struct _ibuf		IBUF;
typedef struct _lmark		LMARK;
typedef struct _mark		MARK;
typedef struct _msg		MSG;
typedef struct _option		OPTION;
typedef struct _optlist		OPTLIST;
typedef struct _scr		SCR;
typedef struct _script		SCRIPT;
typedef struct _seq		SEQ;
typedef struct _tag		TAG;
typedef struct _tagf		TAGF;
typedef struct _text		TEXT;

/*
 * Local includes.
 */
#include "compat.h"		/* typedef u_int8_t et al. */
#include "term.h"		/* Required by args.h. */
#include "args.h"		/* Required by options.h. */
#include "options.h"		/* Required by screen.h. */
#include "search.h"		/* Required by screen.h. */

#include "msg.h"		/* Required by gs.h. */
#include "cut.h"		/* Required by gs.h. */
#include "gs.h"			/* Required by screen.h. */
#include "screen.h"		/* Required by exf.h. */
#include "mark.h"		/* Required by exf.h. */
#include "exf.h"
#include "log.h"
#include "mem.h"

#if FWOPEN_NOT_AVAILABLE	/* See PORT/clib/fwopen.c. */
#define	EXCOOKIE	sp
int	 ex_fflush __P((SCR *));
int	 ex_printf __P((SCR *, const char *, ...));
FILE	*fwopen __P((SCR *, void *));
#else
#define	EXCOOKIE	sp->stdfp
#define	ex_fflush	fflush
#define	ex_printf	fprintf
#endif

/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

#define	LF_INIT(f)	flags = (f)
#define	LF_SET(f)	flags |= (f)
#define	LF_CLR(f)	flags &= ~(f)
#define	LF_ISSET(f)	(flags & (f))

/*
 * XXX
 * MIN/MAX have traditionally been in <sys/param.h>.  Don't
 * try to get them from there, it's just not worth the effort.
 */
#ifndef	MAX
#define	MAX(_a,_b)	((_a)<(_b)?(_b):(_a))
#endif
#ifndef	MIN
#define	MIN(_a,_b)	((_a)<(_b)?(_a):(_b))
#endif

/* Function prototypes that don't seem to belong anywhere else. */
u_long	 baud_from_bval __P((SCR *));
char	*charname __P((SCR *, ARG_CHAR_T));
int	 nonblank __P((SCR *, EXF *, recno_t, size_t *));
void	 set_alt_name __P((SCR *, char *));
int	 set_window_size __P((SCR *, u_int, int));
int	 status __P((SCR *, EXF *, recno_t, int));
char	*tail __P((char *));
CHAR_T	*v_strdup __P((SCR *, CHAR_T *, size_t));

#ifdef DEBUG
void	TRACE __P((SCR *, const char *, ...));
#endif

/* Digraphs (not currently real). */
int	digraph __P((SCR *, int, int));
int	digraph_init __P((SCR *));
void	digraph_save __P((SCR *, int));
