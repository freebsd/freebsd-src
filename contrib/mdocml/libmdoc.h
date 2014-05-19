/*	$Id: libmdoc.h,v 1.82 2013/10/21 23:47:58 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef LIBMDOC_H
#define LIBMDOC_H

enum	mdoc_next {
	MDOC_NEXT_SIBLING = 0,
	MDOC_NEXT_CHILD
};

struct	mdoc {
	struct mparse	 *parse; /* parse pointer */
	char		 *defos; /* default argument for .Os */
	int		  flags; /* parse flags */
#define	MDOC_HALT	 (1 << 0) /* error in parse: halt */
#define	MDOC_LITERAL	 (1 << 1) /* in a literal scope */
#define	MDOC_PBODY	 (1 << 2) /* in the document body */
#define	MDOC_NEWLINE	 (1 << 3) /* first macro/text in a line */
#define	MDOC_PHRASELIT	 (1 << 4) /* literal within a partila phrase */
#define	MDOC_PPHRASE	 (1 << 5) /* within a partial phrase */
#define	MDOC_FREECOL	 (1 << 6) /* `It' invocation should close */
#define	MDOC_SYNOPSIS	 (1 << 7) /* SYNOPSIS-style formatting */
#define	MDOC_KEEP	 (1 << 8) /* in a word keep */
#define	MDOC_SMOFF	 (1 << 9) /* spacing is off */
	enum mdoc_next	  next; /* where to put the next node */
	struct mdoc_node *last; /* the last node parsed */
	struct mdoc_node *first; /* the first node parsed */
	struct mdoc_meta  meta; /* document meta-data */
	enum mdoc_sec	  lastnamed;
	enum mdoc_sec	  lastsec;
	struct roff	 *roff;
};

#define	MACRO_PROT_ARGS	struct mdoc *mdoc, \
			enum mdoct tok, \
			int line, \
			int ppos, \
			int *pos, \
			char *buf

struct	mdoc_macro {
	int		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MDOC_CALLABLE	 (1 << 0)
#define	MDOC_PARSED	 (1 << 1)
#define	MDOC_EXPLICIT	 (1 << 2)
#define	MDOC_PROLOGUE	 (1 << 3)
#define	MDOC_IGNDELIM	 (1 << 4)
#define	MDOC_JOIN	 (1 << 5)
};

enum	margserr {
	ARGS_ERROR,
	ARGS_EOLN, /* end-of-line */
	ARGS_WORD, /* normal word */
	ARGS_PUNCT, /* series of punctuation */
	ARGS_QWORD, /* quoted word */
	ARGS_PHRASE, /* Ta'd phrase (-column) */
	ARGS_PPHRASE, /* tabbed phrase (-column) */
	ARGS_PEND /* last phrase (-column) */
};

enum	margverr {
	ARGV_ERROR,
	ARGV_EOLN, /* end of line */
	ARGV_ARG, /* valid argument */
	ARGV_WORD /* normal word (or bad argument---same thing) */
};

/*
 * A punctuation delimiter is opening, closing, or "middle mark"
 * punctuation.  These govern spacing.
 * Opening punctuation (e.g., the opening parenthesis) suppresses the
 * following space; closing punctuation (e.g., the closing parenthesis)
 * suppresses the leading space; middle punctuation (e.g., the vertical
 * bar) can do either.  The middle punctuation delimiter bends the rules
 * depending on usage.
 */
enum	mdelim {
	DELIM_NONE = 0,
	DELIM_OPEN,
	DELIM_MIDDLE,
	DELIM_CLOSE,
	DELIM_MAX
};

extern	const struct mdoc_macro *const mdoc_macros;

__BEGIN_DECLS

#define		  mdoc_pmsg(mdoc, l, p, t) \
		  mandoc_msg((t), (mdoc)->parse, (l), (p), NULL)
#define		  mdoc_nmsg(mdoc, n, t) \
		  mandoc_msg((t), (mdoc)->parse, (n)->line, (n)->pos, NULL)
int		  mdoc_macro(MACRO_PROT_ARGS);
int		  mdoc_word_alloc(struct mdoc *, 
			int, int, const char *);
void		  mdoc_word_append(struct mdoc *, const char *);
int		  mdoc_elem_alloc(struct mdoc *, int, int, 
			enum mdoct, struct mdoc_arg *);
int		  mdoc_block_alloc(struct mdoc *, int, int, 
			enum mdoct, struct mdoc_arg *);
int		  mdoc_head_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_tail_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_body_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_endbody_alloc(struct mdoc *, int, int, enum mdoct,
			struct mdoc_node *, enum mdoc_endbody);
void		  mdoc_node_delete(struct mdoc *, struct mdoc_node *);
int		  mdoc_node_relink(struct mdoc *, struct mdoc_node *);
void		  mdoc_hash_init(void);
enum mdoct	  mdoc_hash_find(const char *);
const char	 *mdoc_a2att(const char *);
const char	 *mdoc_a2lib(const char *);
const char	 *mdoc_a2st(const char *);
const char	 *mdoc_a2arch(const char *);
const char	 *mdoc_a2vol(const char *);
int		  mdoc_valid_pre(struct mdoc *, struct mdoc_node *);
int		  mdoc_valid_post(struct mdoc *);
enum margverr	  mdoc_argv(struct mdoc *, int, enum mdoct,
			struct mdoc_arg **, int *, char *);
void		  mdoc_argv_free(struct mdoc_arg *);
enum margserr	  mdoc_args(struct mdoc *, int,
			int *, char *, enum mdoct, char **);
enum margserr	  mdoc_zargs(struct mdoc *, int, 
			int *, char *, char **);
int		  mdoc_macroend(struct mdoc *);
enum mdelim	  mdoc_isdelim(const char *);

__END_DECLS

#endif /*!LIBMDOC_H*/
