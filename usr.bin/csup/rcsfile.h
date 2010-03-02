/*-
 * Copyright (c) 2007-2009, Ulf Lilleengen <lulf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 */

#ifndef _RCSFILE_H_
#define _RCSFILE_H_

/* RCSFILE fields. */
#define RCSFILE_HEAD	0
#define RCSFILE_BRANCH	1
#define RCSFILE_STRICT	2
#define RCSFILE_COMMENT	3
#define RCSFILE_EXPAND	4
#define RCSFILE_DESC	5

struct rcsfile;
struct delta;
struct stream;

/* Fetching, sending and writing an RCS file. */
struct rcsfile	*rcsfile_frompath(char *, char *, char *, char *, int);
int		 rcsfile_send_details(struct rcsfile *, struct stream *);
int		 rcsfile_write(struct rcsfile *, struct stream *);
void		 rcsfile_print(struct rcsfile *);
void		 rcsfile_free(struct rcsfile *);

/* Used for adding and setting rcsfile values. */
void		 rcsfile_addaccess(struct rcsfile *, char *);
void		 rcsfile_addtag(struct rcsfile *, char *, char *);
void		 rcsfile_importtag(struct rcsfile *, char *, char *);
void		 rcsfile_deleterev(struct rcsfile *, char *);
void		 rcsfile_deletetag(struct rcsfile *, char *, char *);
struct delta	*rcsfile_getdelta(struct rcsfile *, char *);
void		 rcsfile_setval(struct rcsfile *, int, char *);

/* Functions used for operating on RCS deltas. */
struct delta	*rcsfile_addelta(struct rcsfile *, char *, char *, char *,
		    char *);
void		 rcsfile_importdelta(struct rcsfile *, char *, char *, char *,
		    char *, char *);

int		 rcsdelta_addlog(struct delta *, char *, int);
int		 rcsdelta_addtext(struct delta *, char *, int);
int		 rcsdelta_appendlog(struct delta *, char *, size_t);
int		 rcsdelta_appendtext(struct delta *, char *, size_t);
void		 rcsdelta_setstate(struct delta *, char *);
void		 rcsdelta_truncatetext(struct delta *, off_t);
void		 rcsdelta_truncatelog(struct delta *, off_t);
#endif /* !_RCSFILE_H_ */
