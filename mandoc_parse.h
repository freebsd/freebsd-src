/*	$Id: mandoc_parse.h,v 1.4 2018/12/30 00:49:55 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014,2015,2016,2017,2018 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Top level parser interface.  For use in the main program
 * and in the main parser, but not in formatters.
 */

/*
 * Parse options.
 */
#define	MPARSE_MDOC	(1 << 0)  /* assume -mdoc */
#define	MPARSE_MAN	(1 << 1)  /* assume -man */
#define	MPARSE_SO	(1 << 2)  /* honour .so requests */
#define	MPARSE_QUICK	(1 << 3)  /* abort the parse early */
#define	MPARSE_UTF8	(1 << 4)  /* accept UTF-8 input */
#define	MPARSE_LATIN1	(1 << 5)  /* accept ISO-LATIN-1 input */
#define	MPARSE_VALIDATE	(1 << 6)  /* call validation functions */


struct	roff_meta;
struct	mparse;

struct mparse	 *mparse_alloc(int, enum mandoc_os, const char *);
void		  mparse_copy(const struct mparse *);
void		  mparse_free(struct mparse *);
int		  mparse_open(struct mparse *, const char *);
void		  mparse_readfd(struct mparse *, int, const char *);
void		  mparse_reset(struct mparse *);
struct roff_meta *mparse_result(struct mparse *);
