/* $Id: libmandoc.h,v 1.81 2025/01/05 16:58:22 schwarze Exp $ */
/*
 * Copyright (c) 2013-2015,2017,2018,2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2009, 2010, 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Internal interfaces for parser utilities needed by multiple parsers
 * and the top-level functions to call the mdoc, man, and roff parsers.
 */

/*
 * Return codes passed from the roff parser to the main parser.
 */

/* Main instruction: what to do with the returned line. */
#define	ROFF_IGN	0x000	/* Don't do anything with it. */
#define	ROFF_CONT	0x001	/* Give it to the high-level parser. */
#define	ROFF_RERUN	0x002	/* Re-run the roff parser with an offset. */
#define	ROFF_REPARSE	0x004	/* Recursively run the main parser on it. */
#define	ROFF_SO		0x008	/* Include the named file. */
#define	ROFF_MASK	0x00f	/* Only one of these bits should be set. */

/* Options for further parsing, to be OR'ed with the above. */
#define	ROFF_APPEND	0x010	/* Append the next line to this one. */
#define	ROFF_USERCALL	0x020	/* Start execution of a new macro. */
#define	ROFF_USERRET	0x040	/* Abort execution of the current macro. */
#define	ROFF_WHILE	0x100	/* Start a new .while loop. */
#define	ROFF_LOOPCONT	0x200	/* Iterate the current .while loop. */
#define	ROFF_LOOPEXIT	0x400	/* Exit the current .while loop. */
#define	ROFF_LOOPMASK	0xf00


struct	buf {
	char		*buf;
	size_t		 sz;
	struct buf	*next;
};


struct	roff;
struct	roff_man;
struct	roff_node;

char		*mandoc_normdate(struct roff_node *, struct roff_node *);
int		 mandoc_eos(const char *, size_t);
int		 mandoc_strntoi(const char *, size_t, int);
const char	*mandoc_a2msec(const char*);

int		 mdoc_parseln(struct roff_man *, int, char *, int);
void		 mdoc_endparse(struct roff_man *);

int		 man_parseln(struct roff_man *, int, char *, int);
void		 man_endparse(struct roff_man *);

int		 preconv_cue(const struct buf *, size_t);
int		 preconv_encode(const struct buf *, size_t *,
			struct buf *, size_t *, int *);

void		 roff_free(struct roff *);
struct roff	*roff_alloc(int);
void		 roff_reset(struct roff *);
void		 roff_man_free(struct roff_man *);
struct roff_man	*roff_man_alloc(struct roff *, const char *, int);
void		 roff_man_reset(struct roff_man *);
int		 roff_parseln(struct roff *, int, struct buf *, int *, size_t);
void		 roff_userret(struct roff *);
void		 roff_endparse(struct roff *);
void		 roff_setreg(struct roff *, const char *, int, char);
int		 roff_getreg(struct roff *, const char *);
int		 roff_evalnum(int, const char *, int *, int *, char, int);
char		*roff_strdup(const struct roff *, const char *);
char		*roff_getarg(struct roff *, char **, int, int *);
int		 roff_getcontrol(const struct roff *,
			const char *, int *);
int		 roff_getformat(const struct roff *);
