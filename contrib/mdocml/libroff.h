/*	$Id: libroff.h,v 1.28 2013/05/31 21:37:17 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef LIBROFF_H
#define LIBROFF_H

__BEGIN_DECLS

enum	tbl_part {
	TBL_PART_OPTS, /* in options (first line) */
	TBL_PART_LAYOUT, /* describing layout */
	TBL_PART_DATA, /* creating data rows */
	TBL_PART_CDATA /* continue previous row */
};

struct	tbl_node {
	struct mparse	 *parse; /* parse point */
	int		  pos; /* invocation column */
	int		  line; /* invocation line */
	enum tbl_part	  part;
	struct tbl_opts	  opts;
	struct tbl_row	 *first_row;
	struct tbl_row	 *last_row;
	struct tbl_span	 *first_span;
	struct tbl_span	 *current_span;
	struct tbl_span	 *last_span;
	struct tbl_head	 *first_head;
	struct tbl_head	 *last_head;
	struct tbl_node	 *next;
};

struct	eqn_node {
	struct eqn_def	 *defs;
	size_t		  defsz;
	char		 *data;
	size_t		  rew;
	size_t		  cur;
	size_t		  sz;
	int		  gsize;
	struct eqn	  eqn;
	struct mparse	 *parse;
	struct eqn_node  *next;
};

struct	eqn_def {
	char		 *key;
	size_t		  keysz;
	char		 *val;
	size_t		  valsz;
};

struct tbl_node	*tbl_alloc(int, int, struct mparse *);
void		 tbl_restart(int, int, struct tbl_node *);
void		 tbl_free(struct tbl_node *);
void		 tbl_reset(struct tbl_node *);
enum rofferr 	 tbl_read(struct tbl_node *, int, const char *, int);
int		 tbl_option(struct tbl_node *, int, const char *);
int		 tbl_layout(struct tbl_node *, int, const char *);
int		 tbl_data(struct tbl_node *, int, const char *);
int		 tbl_cdata(struct tbl_node *, int, const char *);
const struct tbl_span	*tbl_span(struct tbl_node *);
void		 tbl_end(struct tbl_node **);
struct eqn_node	*eqn_alloc(const char *, int, int, struct mparse *);
enum rofferr	 eqn_end(struct eqn_node **);
void		 eqn_free(struct eqn_node *);
enum rofferr 	 eqn_read(struct eqn_node **, int, 
			const char *, int, int *);

__END_DECLS

#endif /*LIBROFF_H*/
