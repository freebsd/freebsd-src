/* $OpenBSD: roff_int.h,v 1.16 2019/01/05 00:36:46 schwarze Exp $	*/
/*
 * Copyright (c) 2013-2015, 2017-2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Parser internals shared by multiple parsers.
 */

struct	ohash;
struct	roff_node;
struct	roff_meta;
struct	roff;
struct	mdoc_arg;

enum	roff_next {
	ROFF_NEXT_SIBLING = 0,
	ROFF_NEXT_CHILD
};

struct	roff_man {
	struct roff_meta  meta;    /* Public parse results. */
	struct roff	 *roff;    /* Roff parser state data. */
	struct ohash	 *mdocmac; /* Mdoc macro lookup table. */
	struct ohash	 *manmac;  /* Man macro lookup table. */
	const char	 *os_s;    /* Default operating system. */
	struct roff_node *last;    /* The last node parsed. */
	struct roff_node *last_es; /* The most recent Es node. */
	int		  quick;   /* Abort parse early. */
	int		  flags;   /* Parse flags. */
#define	ROFF_NOFILL	 (1 << 1)  /* Fill mode switched off. */
#define	MDOC_PBODY	 (1 << 2)  /* In the document body. */
#define	MDOC_NEWLINE	 (1 << 3)  /* First macro/text in a line. */
#define	MDOC_PHRASE	 (1 << 4)  /* In a Bl -column phrase. */
#define	MDOC_PHRASELIT	 (1 << 5)  /* Literal within a phrase. */
#define	MDOC_FREECOL	 (1 << 6)  /* `It' invocation should close. */
#define	MDOC_SYNOPSIS	 (1 << 7)  /* SYNOPSIS-style formatting. */
#define	MDOC_KEEP	 (1 << 8)  /* In a word keep. */
#define	MDOC_SMOFF	 (1 << 9)  /* Spacing is off. */
#define	MDOC_NODELIMC	 (1 << 10) /* Disable closing delimiter handling. */
#define	MAN_ELINE	 (1 << 11) /* Next-line element scope. */
#define	MAN_BLINE	 (1 << 12) /* Next-line block scope. */
#define	MDOC_PHRASEQF	 (1 << 13) /* Quote first word encountered. */
#define	MDOC_PHRASEQL	 (1 << 14) /* Quote last word of this phrase. */
#define	MDOC_PHRASEQN	 (1 << 15) /* Quote first word of the next phrase. */
#define	ROFF_NONOFILL	 (1 << 16) /* Temporarily suspend no-fill mode. */
#define	MAN_NEWLINE	  MDOC_NEWLINE
	enum roff_sec	  lastsec; /* Last section seen. */
	enum roff_sec	  lastnamed; /* Last standard section seen. */
	enum roff_next	  next;    /* Where to put the next node. */
	char		  filesec; /* Section digit in the file name. */
};


struct roff_node *roff_node_alloc(struct roff_man *, int, int,
			enum roff_type, int);
void		  roff_node_append(struct roff_man *, struct roff_node *);
void		  roff_word_alloc(struct roff_man *, int, int, const char *);
void		  roff_word_append(struct roff_man *, const char *);
void		  roff_elem_alloc(struct roff_man *, int, int, int);
struct roff_node *roff_block_alloc(struct roff_man *, int, int, int);
struct roff_node *roff_head_alloc(struct roff_man *, int, int, int);
struct roff_node *roff_body_alloc(struct roff_man *, int, int, int);
void		  roff_node_unlink(struct roff_man *, struct roff_node *);
void		  roff_node_relink(struct roff_man *, struct roff_node *);
void		  roff_node_free(struct roff_node *);
void		  roff_node_delete(struct roff_man *, struct roff_node *);

struct ohash	 *roffhash_alloc(enum roff_tok, enum roff_tok);
enum roff_tok	  roffhash_find(struct ohash *, const char *, size_t);
void		  roffhash_free(struct ohash *);

void		  roff_state_reset(struct roff_man *);
void		  roff_validate(struct roff_man *);

/*
 * Functions called from roff.c need to be declared here,
 * not in libmdoc.h or libman.h, even if they are specific
 * to either the mdoc(7) or the man(7) parser.
 */

void		  man_breakscope(struct roff_man *, int);
void		  mdoc_argv_free(struct mdoc_arg *);
