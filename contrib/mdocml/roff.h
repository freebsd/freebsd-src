/*	$Id: roff.h,v 1.40 2017/02/16 03:00:23 schwarze Exp $	*/
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015, 2017 Ingo Schwarze <schwarze@openbsd.org>
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
 */

struct	mdoc_arg;
union	mdoc_data;

enum	roff_macroset {
	MACROSET_NONE = 0,
	MACROSET_MDOC,
	MACROSET_MAN
};

enum	roff_sec {
	SEC_NONE = 0,
	SEC_NAME,
	SEC_LIBRARY,
	SEC_SYNOPSIS,
	SEC_DESCRIPTION,
	SEC_CONTEXT,
	SEC_IMPLEMENTATION,	/* IMPLEMENTATION NOTES */
	SEC_RETURN_VALUES,
	SEC_ENVIRONMENT,
	SEC_FILES,
	SEC_EXIT_STATUS,
	SEC_EXAMPLES,
	SEC_DIAGNOSTICS,
	SEC_COMPATIBILITY,
	SEC_ERRORS,
	SEC_SEE_ALSO,
	SEC_STANDARDS,
	SEC_HISTORY,
	SEC_AUTHORS,
	SEC_CAVEATS,
	SEC_BUGS,
	SEC_SECURITY,
	SEC_CUSTOM,
	SEC__MAX
};

enum	roff_type {
	ROFFT_ROOT,
	ROFFT_BLOCK,
	ROFFT_HEAD,
	ROFFT_BODY,
	ROFFT_TAIL,
	ROFFT_ELEM,
	ROFFT_TEXT,
	ROFFT_TBL,
	ROFFT_EQN
};

enum	roff_next {
	ROFF_NEXT_SIBLING = 0,
	ROFF_NEXT_CHILD
};

/*
 * Indicates that a BODY's formatting has ended, but
 * the scope is still open.  Used for badly nested blocks.
 */
enum	mdoc_endbody {
	ENDBODY_NOT = 0,
	ENDBODY_SPACE	/* Is broken: append a space. */
};

struct	roff_node {
	struct roff_node *parent;  /* Parent AST node. */
	struct roff_node *child;   /* First child AST node. */
	struct roff_node *last;    /* Last child AST node. */
	struct roff_node *next;    /* Sibling AST node. */
	struct roff_node *prev;    /* Prior sibling AST node. */
	struct roff_node *head;    /* BLOCK */
	struct roff_node *body;    /* BLOCK/ENDBODY */
	struct roff_node *tail;    /* BLOCK */
	struct mdoc_arg	 *args;    /* BLOCK/ELEM */
	union mdoc_data	 *norm;    /* Normalized arguments. */
	char		 *string;  /* TEXT */
	const struct tbl_span *span; /* TBL */
	const struct eqn *eqn;	   /* EQN */
	int		  line;    /* Input file line number. */
	int		  pos;     /* Input file column number. */
	int		  tok;     /* Request or macro ID. */
#define	TOKEN_NONE	 (-1)	   /* No request or macro. */
	int		  flags;
#define	NODE_VALID	 (1 << 0)  /* Has been validated. */
#define	NODE_ENDED	 (1 << 1)  /* Gone past body end mark. */
#define	NODE_EOS	 (1 << 2)  /* At sentence boundary. */
#define	NODE_LINE	 (1 << 3)  /* First macro/text on line. */
#define	NODE_SYNPRETTY	 (1 << 4)  /* SYNOPSIS-style formatting. */
#define	NODE_BROKEN	 (1 << 5)  /* Must validate parent when ending. */
#define	NODE_DELIMO	 (1 << 6)
#define	NODE_DELIMC	 (1 << 7)
#define	NODE_NOSRC	 (1 << 8)  /* Generated node, not in input file. */
#define	NODE_NOPRT	 (1 << 9)  /* Shall not print anything. */
	int		  prev_font; /* Before entering this node. */
	int		  aux;     /* Decoded node data, type-dependent. */
	enum roff_type	  type;    /* AST node type. */
	enum roff_sec	  sec;     /* Current named section. */
	enum mdoc_endbody end;     /* BODY */
};

struct	roff_meta {
	char		 *msec;    /* Manual section, usually a digit. */
	char		 *vol;     /* Manual volume title. */
	char		 *os;      /* Operating system. */
	char		 *arch;    /* Machine architecture. */
	char		 *title;   /* Manual title, usually CAPS. */
	char		 *name;    /* Leading manual name. */
	char		 *date;    /* Normalized date. */
	int		  hasbody; /* Document is not empty. */
};

struct	roff_man {
	struct roff_meta  meta;    /* Document meta-data. */
	struct mparse	 *parse;   /* Parse pointer. */
	struct roff	 *roff;    /* Roff parser state data. */
	const char	 *defos;   /* Default operating system. */
	struct roff_node *first;   /* The first node parsed. */
	struct roff_node *last;    /* The last node parsed. */
	struct roff_node *last_es; /* The most recent Es node. */
	int		  quick;   /* Abort parse early. */
	int		  flags;   /* Parse flags. */
#define	MDOC_LITERAL	 (1 << 1)  /* In a literal scope. */
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
#define	MAN_LITERAL	  MDOC_LITERAL
#define	MAN_NEWLINE	  MDOC_NEWLINE
	enum roff_macroset macroset; /* Kind of high-level macros used. */
	enum roff_sec	  lastsec; /* Last section seen. */
	enum roff_sec	  lastnamed; /* Last standard section seen. */
	enum roff_next	  next;    /* Where to put the next node. */
};


void		 deroff(char **, const struct roff_node *);
