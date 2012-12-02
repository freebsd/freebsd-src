/*	$Id: libman.h,v 1.55 2011/11/07 01:24:40 schwarze Exp $ */
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
#ifndef LIBMAN_H
#define LIBMAN_H

enum	man_next {
	MAN_NEXT_SIBLING = 0,
	MAN_NEXT_CHILD
};

struct	man {
	struct mparse	*parse; /* parse pointer */
	int		 flags; /* parse flags */
#define	MAN_HALT	(1 << 0) /* badness happened: die */
#define	MAN_ELINE	(1 << 1) /* Next-line element scope. */
#define	MAN_BLINE	(1 << 2) /* Next-line block scope. */
#define	MAN_ILINE	(1 << 3) /* Ignored in next-line scope. */
#define	MAN_LITERAL	(1 << 4) /* Literal input. */
#define	MAN_BPLINE	(1 << 5)
#define	MAN_NEWLINE	(1 << 6) /* first macro/text in a line */
	enum man_next	 next; /* where to put the next node */
	struct man_node	*last; /* the last parsed node */
	struct man_node	*first; /* the first parsed node */
	struct man_meta	 meta; /* document meta-data */
	struct roff	*roff;
};

#define	MACRO_PROT_ARGS	  struct man *m, \
			  enum mant tok, \
			  int line, \
			  int ppos, \
			  int *pos, \
			  char *buf

struct	man_macro {
	int		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MAN_SCOPED	 (1 << 0)
#define	MAN_EXPLICIT	 (1 << 1)	/* See blk_imp(). */
#define	MAN_FSCOPED	 (1 << 2)	/* See blk_imp(). */
#define	MAN_NSCOPED	 (1 << 3)	/* See in_line_eoln(). */
#define	MAN_NOCLOSE	 (1 << 4)	/* See blk_exp(). */
#define	MAN_BSCOPE	 (1 << 5)	/* Break BLINE scope. */
};

extern	const struct man_macro *const man_macros;

__BEGIN_DECLS

#define		  man_pmsg(m, l, p, t) \
		  mandoc_msg((t), (m)->parse, (l), (p), NULL)
#define		  man_nmsg(m, n, t) \
		  mandoc_msg((t), (m)->parse, (n)->line, (n)->pos, NULL)
int		  man_word_alloc(struct man *, int, int, const char *);
int		  man_block_alloc(struct man *, int, int, enum mant);
int		  man_head_alloc(struct man *, int, int, enum mant);
int		  man_tail_alloc(struct man *, int, int, enum mant);
int		  man_body_alloc(struct man *, int, int, enum mant);
int		  man_elem_alloc(struct man *, int, int, enum mant);
void		  man_node_delete(struct man *, struct man_node *);
void		  man_hash_init(void);
enum mant	  man_hash_find(const char *);
int		  man_macroend(struct man *);
int		  man_valid_post(struct man *);
int		  man_valid_pre(struct man *, struct man_node *);
int		  man_unscope(struct man *, 
			const struct man_node *, enum mandocerr);

__END_DECLS

#endif /*!LIBMAN_H*/
