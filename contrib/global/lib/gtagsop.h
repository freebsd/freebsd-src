/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	gtagsop.h				23-Dec-97
 *
 */

#ifndef _GTOP_H_
#define _GTOP_H_
#include <stdio.h>
#include "dbop.h"
#include "gparam.h"
#include "strbuf.h"

#define VERSIONKEY	" __.VERSION"
#define COMPACTKEY	" __.COMPACT"
#define PATHINDEXKEY	" __.PATHINDEX"

#define GTAGS		0
#define GRTAGS		1
#define GSYMS		2
#define GTAGLIM		3

#define	GTAGS_READ	0
#define GTAGS_CREATE	1
#define GTAGS_MODIFY	2

/* gtagsopen() */
#define GTAGS_STANDARD		0	/* standard format */
#define GTAGS_COMPACT		1	/* compact format */
#define GTAGS_PATHINDEX		2	/* use path index */
/* gtagsadd() */
#define GTAGS_UNIQUE		1	/* compress duplicate lines */
#define GTAGS_EXTRACTMETHOD	2	/* extract method from class definition */
/* gtagsfirst() */
#define GTOP_KEY		1	/* read key part */
#define GTOP_PREFIX		2	/* prefixed read */

typedef struct {
	DBOP	*dbop;			/* descripter of DBOP */
	int	format_version;		/* format version */
	int	format;			/* GTAGS_STANDARD, GTAGS_COMPACT */
	int	mode;			/* mode */
	int	db;			/* 0:GTAGS, 1:GRTAGS, 2:GSYMS */
	int	flags;			/* flags */
	char	root[MAXPATHLEN+1];	/* root directory of source tree */
	/*
	 * Stuff for compact format
	 */
	int	opened;			/* wether or not file opened */
	char	*line;			/* current record */
	char	tag[IDENTLEN+1];	/* current tag */
	char	prev_tag[IDENTLEN+1];	/* previous tag */
	char	path[MAXPATHLEN+1];	/* current path */
	char	prev_path[MAXPATHLEN+1];/* previous path */
	STRBUF	*sb;			/* string buffer */
	FILE	*fp;			/* descriptor of 'path' */
	char	*lnop;			/* current line number */
	int	lno;			/* integer value of 'lnop' */
} GTOP;

#ifndef __P
#if defined(__STDC__)
#define __P(protos)     protos
#else
#define __P(protos)     ()
#endif
#endif

const char *dbname __P((int));
void	makecommand __P((char *, char *, STRBUF *));
int	formatcheck __P((char *, int));
GTOP	*gtagsopen __P((char *, char *, int, int, int));
void	gtagsput __P((GTOP *, char *, char *));
char	*gtagsget __P((GTOP *, char *));
void    gtagsadd __P((GTOP *, char *, char *, int));
void	gtagsdelete __P((GTOP *, char *));
char	*gtagsfirst __P((GTOP *, char *, int));
char	*gtagsnext __P((GTOP *));
void	gtagsclose __P((GTOP *));

#endif /* ! _GTOP_H_ */
