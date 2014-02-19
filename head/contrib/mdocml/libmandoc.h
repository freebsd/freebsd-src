/*	$Id: libmandoc.h,v 1.35 2013/12/15 21:23:52 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef LIBMANDOC_H
#define LIBMANDOC_H

enum	rofferr {
	ROFF_CONT, /* continue processing line */
	ROFF_RERUN, /* re-run roff interpreter with offset */
	ROFF_APPEND, /* re-run main parser, appending next line */
	ROFF_REPARSE, /* re-run main parser on the result */
	ROFF_SO, /* include another file */
	ROFF_IGN, /* ignore current line */
	ROFF_TBL, /* a table row was successfully parsed */
	ROFF_EQN, /* an equation was successfully parsed */
	ROFF_ERR /* badness: puke and stop */
};

__BEGIN_DECLS

struct	roff;
struct	mdoc;
struct	man;

void		 mandoc_msg(enum mandocerr, struct mparse *, 
			int, int, const char *);
void		 mandoc_vmsg(enum mandocerr, struct mparse *, 
			int, int, const char *, ...);
char		*mandoc_getarg(struct mparse *, char **, int, int *);
char		*mandoc_normdate(struct mparse *, char *, int, int);
int		 mandoc_eos(const char *, size_t, int);
int		 mandoc_strntoi(const char *, size_t, int);
const char	*mandoc_a2msec(const char*);

void	 	 mdoc_free(struct mdoc *);
struct	mdoc	*mdoc_alloc(struct roff *, struct mparse *, char *);
void		 mdoc_reset(struct mdoc *);
int	 	 mdoc_parseln(struct mdoc *, int, char *, int);
int		 mdoc_endparse(struct mdoc *);
int		 mdoc_addspan(struct mdoc *, const struct tbl_span *);
int		 mdoc_addeqn(struct mdoc *, const struct eqn *);

void	 	 man_free(struct man *);
struct	man	*man_alloc(struct roff *, struct mparse *);
void		 man_reset(struct man *);
int	 	 man_parseln(struct man *, int, char *, int);
int		 man_endparse(struct man *);
int		 man_addspan(struct man *, const struct tbl_span *);
int		 man_addeqn(struct man *, const struct eqn *);

void	 	 roff_free(struct roff *);
struct roff	*roff_alloc(enum mparset, struct mparse *);
void		 roff_reset(struct roff *);
enum rofferr	 roff_parseln(struct roff *, int, 
			char **, size_t *, int, int *);
void		 roff_endparse(struct roff *);
void		 roff_setreg(struct roff *, const char *, int, char sign);
int		 roff_getreg(const struct roff *, const char *);
char		*roff_strdup(const struct roff *, const char *);
int		 roff_getcontrol(const struct roff *, 
			const char *, int *);
#if 0
char		 roff_eqndelim(const struct roff *);
void		 roff_openeqn(struct roff *, const char *, 
			int, int, const char *);
int		 roff_closeeqn(struct roff *);
#endif

const struct tbl_span	*roff_span(const struct roff *);
const struct eqn	*roff_eqn(const struct roff *);

__END_DECLS

#endif /*!LIBMANDOC_H*/
