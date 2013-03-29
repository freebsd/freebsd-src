/*	$Id: mandoc.h,v 1.99 2012/02/16 20:51:31 joerg Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef MANDOC_H
#define MANDOC_H

#define ASCII_NBRSP	 31  /* non-breaking space */
#define	ASCII_HYPH	 30  /* breakable hyphen */

/*
 * Status level.  This refers to both internal status (i.e., whilst
 * running, when warnings/errors are reported) and an indicator of a
 * threshold of when to halt (when said internal state exceeds the
 * threshold).
 */
enum	mandoclevel {
	MANDOCLEVEL_OK = 0,
	MANDOCLEVEL_RESERVED,
	MANDOCLEVEL_WARNING, /* warnings: syntax, whitespace, etc. */
	MANDOCLEVEL_ERROR, /* input has been thrown away */
	MANDOCLEVEL_FATAL, /* input is borked */
	MANDOCLEVEL_BADARG, /* bad argument in invocation */
	MANDOCLEVEL_SYSERR, /* system error */
	MANDOCLEVEL_MAX
};

/*
 * All possible things that can go wrong within a parse, be it libroff,
 * libmdoc, or libman.
 */
enum	mandocerr {
	MANDOCERR_OK,

	MANDOCERR_WARNING, /* ===== start of warnings ===== */

	/* related to the prologue */
	MANDOCERR_NOTITLE, /* no title in document */
	MANDOCERR_UPPERCASE, /* document title should be all caps */
	MANDOCERR_BADMSEC, /* unknown manual section */
	MANDOCERR_NODATE, /* date missing, using today's date */
	MANDOCERR_BADDATE, /* cannot parse date, using it verbatim */
	MANDOCERR_PROLOGOOO, /* prologue macros out of order */
	MANDOCERR_PROLOGREP, /* duplicate prologue macro */
	MANDOCERR_BADPROLOG, /* macro not allowed in prologue */
	MANDOCERR_BADBODY, /* macro not allowed in body */

	/* related to document structure */
	MANDOCERR_SO, /* .so is fragile, better use ln(1) */
	MANDOCERR_NAMESECFIRST, /* NAME section must come first */
	MANDOCERR_BADNAMESEC, /* bad NAME section contents */
	MANDOCERR_NONAME, /* manual name not yet set */
	MANDOCERR_SECOOO, /* sections out of conventional order */
	MANDOCERR_SECREP, /* duplicate section name */
	MANDOCERR_SECMSEC, /* section not in conventional manual section */

	/* related to macros and nesting */
	MANDOCERR_MACROOBS, /* skipping obsolete macro */
	MANDOCERR_IGNPAR, /* skipping paragraph macro */
	MANDOCERR_IGNNS, /* skipping no-space macro */
	MANDOCERR_SCOPENEST, /* blocks badly nested */
	MANDOCERR_CHILD, /* child violates parent syntax */
	MANDOCERR_NESTEDDISP, /* nested displays are not portable */
	MANDOCERR_SCOPEREP, /* already in literal mode */
	MANDOCERR_LINESCOPE, /* line scope broken */

	/* related to missing macro arguments */
	MANDOCERR_MACROEMPTY, /* skipping empty macro */
	MANDOCERR_ARGCWARN, /* argument count wrong */
	MANDOCERR_DISPTYPE, /* missing display type */
	MANDOCERR_LISTFIRST, /* list type must come first */
	MANDOCERR_NOWIDTHARG, /* tag lists require a width argument */
	MANDOCERR_FONTTYPE, /* missing font type */
	MANDOCERR_WNOSCOPE, /* skipping end of block that is not open */

	/* related to bad macro arguments */
	MANDOCERR_IGNARGV, /* skipping argument */
	MANDOCERR_ARGVREP, /* duplicate argument */
	MANDOCERR_DISPREP, /* duplicate display type */
	MANDOCERR_LISTREP, /* duplicate list type */
	MANDOCERR_BADATT, /* unknown AT&T UNIX version */
	MANDOCERR_BADBOOL, /* bad Boolean value */
	MANDOCERR_BADFONT, /* unknown font */
	MANDOCERR_BADSTANDARD, /* unknown standard specifier */
	MANDOCERR_BADWIDTH, /* bad width argument */

	/* related to plain text */
	MANDOCERR_NOBLANKLN, /* blank line in non-literal context */
	MANDOCERR_BADTAB, /* tab in non-literal context */
	MANDOCERR_EOLNSPACE, /* end of line whitespace */
	MANDOCERR_BADCOMMENT, /* bad comment style */
	MANDOCERR_BADESCAPE, /* unknown escape sequence */
	MANDOCERR_BADQUOTE, /* unterminated quoted string */

	/* related to equations */
	MANDOCERR_EQNQUOTE, /* unexpected literal in equation */

	MANDOCERR_ERROR, /* ===== start of errors ===== */

	/* related to equations */
	MANDOCERR_EQNNSCOPE, /* unexpected equation scope closure*/
	MANDOCERR_EQNSCOPE, /* equation scope open on exit */
	MANDOCERR_EQNBADSCOPE, /* overlapping equation scopes */
	MANDOCERR_EQNEOF, /* unexpected end of equation */
	MANDOCERR_EQNSYNT, /* equation syntax error */

	/* related to tables */
	MANDOCERR_TBL, /* bad table syntax */
	MANDOCERR_TBLOPT, /* bad table option */
	MANDOCERR_TBLLAYOUT, /* bad table layout */
	MANDOCERR_TBLNOLAYOUT, /* no table layout cells specified */
	MANDOCERR_TBLNODATA, /* no table data cells specified */
	MANDOCERR_TBLIGNDATA, /* ignore data in cell */
	MANDOCERR_TBLBLOCK, /* data block still open */
	MANDOCERR_TBLEXTRADAT, /* ignoring extra data cells */

	MANDOCERR_ROFFLOOP, /* input stack limit exceeded, infinite loop? */
	MANDOCERR_BADCHAR, /* skipping bad character */
	MANDOCERR_NAMESC, /* escaped character not allowed in a name */
	MANDOCERR_NOTEXT, /* skipping text before the first section header */
	MANDOCERR_MACRO, /* skipping unknown macro */
	MANDOCERR_REQUEST, /* NOT IMPLEMENTED: skipping request */
	MANDOCERR_ARGCOUNT, /* argument count wrong */
	MANDOCERR_NOSCOPE, /* skipping end of block that is not open */
	MANDOCERR_SCOPEBROKEN, /* missing end of block */
	MANDOCERR_SCOPEEXIT, /* scope open on exit */
	MANDOCERR_UNAME, /* uname(3) system call failed */
	/* FIXME: merge following with MANDOCERR_ARGCOUNT */
	MANDOCERR_NOARGS, /* macro requires line argument(s) */
	MANDOCERR_NOBODY, /* macro requires body argument(s) */
	MANDOCERR_NOARGV, /* macro requires argument(s) */
	MANDOCERR_LISTTYPE, /* missing list type */
	MANDOCERR_ARGSLOST, /* line argument(s) will be lost */
	MANDOCERR_BODYLOST, /* body argument(s) will be lost */

	MANDOCERR_FATAL, /* ===== start of fatal errors ===== */

	MANDOCERR_NOTMANUAL, /* manual isn't really a manual */
	MANDOCERR_COLUMNS, /* column syntax is inconsistent */
	MANDOCERR_BADDISP, /* NOT IMPLEMENTED: .Bd -file */
	MANDOCERR_SYNTARGVCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SYNTCHILD, /* child violates parent syntax */
	MANDOCERR_SYNTARGCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SOPATH, /* NOT IMPLEMENTED: .so with absolute path or ".." */
	MANDOCERR_NODOCBODY, /* no document body */
	MANDOCERR_NODOCPROLOG, /* no document prologue */
	MANDOCERR_MEM, /* static buffer exhausted */
	MANDOCERR_MAX
};

struct	tbl {
	char		  tab; /* cell-separator */
	char		  decimal; /* decimal point */
	int		  linesize;
	int		  opts;
#define	TBL_OPT_CENTRE	 (1 << 0)
#define	TBL_OPT_EXPAND	 (1 << 1)
#define	TBL_OPT_BOX	 (1 << 2)
#define	TBL_OPT_DBOX	 (1 << 3)
#define	TBL_OPT_ALLBOX	 (1 << 4)
#define	TBL_OPT_NOKEEP	 (1 << 5)
#define	TBL_OPT_NOSPACE	 (1 << 6)
	int		  cols; /* number of columns */
};

enum	tbl_headt {
	TBL_HEAD_DATA, /* plug in data from tbl_dat */
	TBL_HEAD_VERT, /* vertical spacer */
	TBL_HEAD_DVERT  /* double-vertical spacer */
};

/*
 * The head of a table specifies all of its columns.  When formatting a
 * tbl_span, iterate over these and plug in data from the tbl_span when
 * appropriate, using tbl_cell as a guide to placement.
 */
struct	tbl_head {
	enum tbl_headt	  pos;
	int		  ident; /* 0 <= unique id < cols */
	struct tbl_head	 *next;
	struct tbl_head	 *prev;
};

enum	tbl_cellt {
	TBL_CELL_CENTRE, /* c, C */
	TBL_CELL_RIGHT, /* r, R */
	TBL_CELL_LEFT, /* l, L */
	TBL_CELL_NUMBER, /* n, N */
	TBL_CELL_SPAN, /* s, S */
	TBL_CELL_LONG, /* a, A */
	TBL_CELL_DOWN, /* ^ */
	TBL_CELL_HORIZ, /* _, - */
	TBL_CELL_DHORIZ, /* = */
	TBL_CELL_VERT, /* | */
	TBL_CELL_DVERT, /* || */
	TBL_CELL_MAX
};

/*
 * A cell in a layout row.
 */
struct	tbl_cell {
	struct tbl_cell	 *next;
	enum tbl_cellt	  pos;
	size_t		  spacing;
	int		  flags;
#define	TBL_CELL_TALIGN	 (1 << 0) /* t, T */
#define	TBL_CELL_BALIGN	 (1 << 1) /* d, D */
#define	TBL_CELL_BOLD	 (1 << 2) /* fB, B, b */
#define	TBL_CELL_ITALIC	 (1 << 3) /* fI, I, i */
#define	TBL_CELL_EQUAL	 (1 << 4) /* e, E */
#define	TBL_CELL_UP	 (1 << 5) /* u, U */
#define	TBL_CELL_WIGN	 (1 << 6) /* z, Z */
	struct tbl_head	 *head;
};

/*
 * A layout row.
 */
struct	tbl_row {
	struct tbl_row	 *next;
	struct tbl_cell	 *first;
	struct tbl_cell	 *last;
};

enum	tbl_datt {
	TBL_DATA_NONE, /* has no data */
	TBL_DATA_DATA, /* consists of data/string */
	TBL_DATA_HORIZ, /* horizontal line */
	TBL_DATA_DHORIZ, /* double-horizontal line */
	TBL_DATA_NHORIZ, /* squeezed horizontal line */
	TBL_DATA_NDHORIZ /* squeezed double-horizontal line */
};

/*
 * A cell within a row of data.  The "string" field contains the actual
 * string value that's in the cell.  The rest is layout.
 */
struct	tbl_dat {
	struct tbl_cell	 *layout; /* layout cell */
	int		  spans; /* how many spans follow */
	struct tbl_dat	 *next;
	char		 *string; /* data (NULL if not TBL_DATA_DATA) */
	enum tbl_datt	  pos;
};

enum	tbl_spant {
	TBL_SPAN_DATA, /* span consists of data */
	TBL_SPAN_HORIZ, /* span is horizontal line */
	TBL_SPAN_DHORIZ /* span is double horizontal line */
};

/*
 * A row of data in a table.
 */
struct	tbl_span {
	struct tbl	 *tbl;
	struct tbl_head	 *head;
	struct tbl_row	 *layout; /* layout row */
	struct tbl_dat	 *first;
	struct tbl_dat	 *last;
	int		  line; /* parse line */
	int		  flags;
#define	TBL_SPAN_FIRST	 (1 << 0)
#define	TBL_SPAN_LAST	 (1 << 1)
	enum tbl_spant	  pos;
	struct tbl_span	 *next;
};

enum	eqn_boxt {
	EQN_ROOT, /* root of parse tree */
	EQN_TEXT, /* text (number, variable, whatever) */
	EQN_SUBEXPR, /* nested `eqn' subexpression */
	EQN_LIST, /* subexpressions list */
	EQN_MATRIX /* matrix subexpression */
};

enum	eqn_markt {
	EQNMARK_NONE = 0,
	EQNMARK_DOT,
	EQNMARK_DOTDOT,
	EQNMARK_HAT,
	EQNMARK_TILDE,
	EQNMARK_VEC,
	EQNMARK_DYAD,
	EQNMARK_BAR,
	EQNMARK_UNDER,
	EQNMARK__MAX
};

enum	eqn_fontt {
	EQNFONT_NONE = 0,
	EQNFONT_ROMAN,
	EQNFONT_BOLD,
	EQNFONT_FAT,
	EQNFONT_ITALIC,
	EQNFONT__MAX
};

enum	eqn_post {
	EQNPOS_NONE = 0,
	EQNPOS_OVER,
	EQNPOS_SUP,
	EQNPOS_SUB,
	EQNPOS_TO,
	EQNPOS_FROM,
	EQNPOS__MAX
};

enum	eqn_pilet {
	EQNPILE_NONE = 0,
	EQNPILE_PILE,
	EQNPILE_CPILE,
	EQNPILE_RPILE,
	EQNPILE_LPILE,
	EQNPILE_COL,
	EQNPILE_CCOL,
	EQNPILE_RCOL,
	EQNPILE_LCOL,
	EQNPILE__MAX
};

 /*
 * A "box" is a parsed mathematical expression as defined by the eqn.7
 * grammar.
 */
struct	eqn_box {
	int		  size; /* font size of expression */
#define	EQN_DEFSIZE	  INT_MIN
	enum eqn_boxt	  type; /* type of node */
	struct eqn_box	 *first; /* first child node */
	struct eqn_box	 *last; /* last child node */
	struct eqn_box	 *next; /* node sibling */
	struct eqn_box	 *parent; /* node sibling */
	char		 *text; /* text (or NULL) */
	char		 *left;
	char		 *right;
	enum eqn_post	  pos; /* position of next box */
	enum eqn_markt	  mark; /* a mark about the box */
	enum eqn_fontt	  font; /* font of box */
	enum eqn_pilet	  pile; /* equation piling */
};

/*
 * An equation consists of a tree of expressions starting at a given
 * line and position. 
 */
struct	eqn {
	char		 *name; /* identifier (or NULL) */
	struct eqn_box	 *root; /* root mathematical expression */
	int		  ln; /* invocation line */
	int		  pos; /* invocation position */
};

/*
 * The type of parse sequence.  This value is usually passed via the
 * mandoc(1) command line of -man and -mdoc.  It's almost exclusively
 * -mandoc but the others have been retained for compatibility.
 */
enum	mparset {
	MPARSE_AUTO, /* magically determine the document type */
	MPARSE_MDOC, /* assume -mdoc */
	MPARSE_MAN /* assume -man */
};

enum	mandoc_esc {
	ESCAPE_ERROR = 0, /* bail! unparsable escape */
	ESCAPE_IGNORE, /* escape to be ignored */
	ESCAPE_SPECIAL, /* a regular special character */
	ESCAPE_FONT, /* a generic font mode */
	ESCAPE_FONTBOLD, /* bold font mode */
	ESCAPE_FONTITALIC, /* italic font mode */
	ESCAPE_FONTROMAN, /* roman font mode */
	ESCAPE_FONTPREV, /* previous font mode */
	ESCAPE_NUMBERED, /* a numbered glyph */
	ESCAPE_UNICODE, /* a unicode codepoint */
	ESCAPE_NOSPACE /* suppress space if the last on a line */
};

typedef	void	(*mandocmsg)(enum mandocerr, enum mandoclevel,
			const char *, int, int, const char *);

struct	mparse;
struct	mchars;
struct	mdoc;
struct	man;

__BEGIN_DECLS

void		 *mandoc_calloc(size_t, size_t);
enum mandoc_esc	  mandoc_escape(const char **, const char **, int *);
void		 *mandoc_malloc(size_t);
void		 *mandoc_realloc(void *, size_t);
char		 *mandoc_strdup(const char *);
char		 *mandoc_strndup(const char *, size_t);
struct mchars	 *mchars_alloc(void);
void		  mchars_free(struct mchars *);
char	 	  mchars_num2char(const char *, size_t);
int		  mchars_num2uc(const char *, size_t);
int		  mchars_spec2cp(const struct mchars *, 
			const char *, size_t);
const char	 *mchars_spec2str(const struct mchars *, 
			const char *, size_t, size_t *);
struct mparse	 *mparse_alloc(enum mparset, 
			enum mandoclevel, mandocmsg, void *);
void		  mparse_free(struct mparse *);
void		  mparse_keep(struct mparse *);
enum mandoclevel  mparse_readfd(struct mparse *, int, const char *);
enum mandoclevel  mparse_readmem(struct mparse *, const void *, size_t,
			const char *);
void		  mparse_reset(struct mparse *);
void		  mparse_result(struct mparse *, 
			struct mdoc **, struct man **);
const char	 *mparse_getkeep(const struct mparse *);
const char	 *mparse_strerror(enum mandocerr);
const char	 *mparse_strlevel(enum mandoclevel);

__END_DECLS

#endif /*!MANDOC_H*/
