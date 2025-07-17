/* $Id: mandoc.h,v 1.283 2025/01/05 18:14:39 schwarze Exp $ */
/*
 * Copyright (c) 2012-2022, 2025 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010, 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Error handling, escape sequence, and character utilities.
 * Can be used by all code in the mandoc package.
 */

#define ASCII_NBRSP	 31  /* non-breaking space */
#define ASCII_NBRZW	 30  /* non-breaking zero-width space */
#define ASCII_BREAK	 29  /* breakable zero-width space */
#define ASCII_HYPH	 28  /* breakable hyphen */
#define ASCII_TABREF	 26  /* reset tab reference position */

/*
 * Status level.  This refers to both internal status (i.e., whilst
 * running, when warnings/errors are reported) and an indicator of a
 * threshold of when to halt (when said internal state exceeds the
 * threshold).
 */
enum	mandoclevel {
	MANDOCLEVEL_OK = 0,
	MANDOCLEVEL_STYLE, /* style suggestions */
	MANDOCLEVEL_WARNING, /* warnings: syntax, whitespace, etc. */
	MANDOCLEVEL_ERROR, /* input has been thrown away */
	MANDOCLEVEL_UNSUPP, /* input needs unimplemented features */
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

	MANDOCERR_BASE, /* ===== start of base system conventions ===== */

	MANDOCERR_MDOCDATE, /* Mdocdate found: Dd ... */
	MANDOCERR_MDOCDATE_MISSING, /* Mdocdate missing: Dd ... */
	MANDOCERR_ARCH_BAD,  /* unknown architecture: Dt ... arch */
	MANDOCERR_OS_ARG,  /* operating system explicitly specified: Os ... */
	MANDOCERR_RCS_MISSING, /* RCS id missing */

	MANDOCERR_STYLE, /* ===== start of style suggestions ===== */

	MANDOCERR_DATE_LEGACY, /* legacy man(7) date format: Dd ... */
	MANDOCERR_DATE_NORM, /* normalizing date format to: ... */
	MANDOCERR_TITLE_CASE, /* lower case character in document title */
	MANDOCERR_RCS_REP, /* duplicate RCS id: ... */
	MANDOCERR_SEC_TYPO,  /* possible typo in section name: Sh ... */
	MANDOCERR_ARG_QUOTE, /* unterminated quoted argument */
	MANDOCERR_MACRO_USELESS, /* useless macro: macro */
	MANDOCERR_BX, /* consider using OS macro: macro */
	MANDOCERR_ER_ORDER, /* errnos out of order: Er ... */
	MANDOCERR_ER_REP, /* duplicate errno: Er ... */
	MANDOCERR_XR_BAD,  /* referenced manual not found: Xr name sec */
	MANDOCERR_DELIM, /* trailing delimiter: macro ... */
	MANDOCERR_DELIM_NB, /* no blank before trailing delimiter: macro ... */
	MANDOCERR_FI_SKIP, /* fill mode already enabled, skipping: fi */
	MANDOCERR_NF_SKIP, /* fill mode already disabled, skipping: nf */
	MANDOCERR_TEXT_LONG, /* input text line longer than 80 bytes */
	MANDOCERR_DASHDASH, /* verbatim "--", maybe consider using \(em */
	MANDOCERR_FUNC, /* function name without markup: name() */
	MANDOCERR_SPACE_EOL, /* whitespace at end of input line */
	MANDOCERR_COMMENT_BAD, /* bad comment style */

	MANDOCERR_WARNING, /* ===== start of warnings ===== */

	/* related to the prologue */
	MANDOCERR_DT_NOTITLE, /* missing manual title, using UNTITLED: line */
	MANDOCERR_TH_NOTITLE, /* missing manual title, using "": [macro] */
	MANDOCERR_MSEC_MISSING, /* missing manual section, using "": macro */
	MANDOCERR_MSEC_BAD, /* unknown manual section: Dt ... section */
	MANDOCERR_MSEC_FILE, /* filename/section mismatch: ... */
	MANDOCERR_DATE_MISSING, /* missing date, using "": [macro] */
	MANDOCERR_DATE_BAD, /* cannot parse date, using it verbatim: date */
	MANDOCERR_DATE_FUTURE, /* date in the future, using it anyway: date */
	MANDOCERR_OS_MISSING, /* missing Os macro, using "" */
	MANDOCERR_PROLOG_LATE, /* late prologue macro: macro */
	MANDOCERR_PROLOG_ORDER, /* prologue macros out of order: macros */

	/* related to document structure */
	MANDOCERR_SO, /* .so is fragile, better use ln(1): so path */
	MANDOCERR_DOC_EMPTY, /* no document body */
	MANDOCERR_SEC_BEFORE, /* content before first section header: macro */
	MANDOCERR_NAMESEC_FIRST, /* first section is not NAME: Sh title */
	MANDOCERR_NAMESEC_NONM, /* NAME section without Nm before Nd */
	MANDOCERR_NAMESEC_NOND, /* NAME section without description */
	MANDOCERR_NAMESEC_ND, /* description not at the end of NAME */
	MANDOCERR_NAMESEC_BAD, /* bad NAME section content: macro */
	MANDOCERR_NAMESEC_PUNCT, /* missing comma before name: Nm name */
	MANDOCERR_ND_EMPTY, /* missing description line, using "" */
	MANDOCERR_ND_LATE, /* description line outside NAME section */
	MANDOCERR_SEC_ORDER, /* sections out of conventional order: Sh title */
	MANDOCERR_SEC_REP, /* duplicate section title: Sh title */
	MANDOCERR_SEC_MSEC, /* unexpected section: Sh title for ... only */
	MANDOCERR_XR_SELF,  /* cross reference to self: Xr name sec */
	MANDOCERR_XR_ORDER, /* unusual Xr order: ... after ... */
	MANDOCERR_XR_PUNCT, /* unusual Xr punctuation: ... after ... */
	MANDOCERR_AN_MISSING, /* AUTHORS section without An macro */

	/* related to macros and nesting */
	MANDOCERR_MACRO_OBS, /* obsolete macro: macro */
	MANDOCERR_MACRO_CALL, /* macro neither callable nor escaped: macro */
	MANDOCERR_PAR_SKIP, /* skipping paragraph macro: macro ... */
	MANDOCERR_PAR_MOVE, /* moving paragraph macro out of list: macro */
	MANDOCERR_NS_SKIP, /* skipping no-space macro */
	MANDOCERR_BLK_NEST, /* blocks badly nested: macro ... */
	MANDOCERR_BD_NEST, /* nested displays are not portable: macro ... */
	MANDOCERR_BL_MOVE, /* moving content out of list: macro */
	MANDOCERR_TA_LINE, /* first macro on line: Ta */
	MANDOCERR_BLK_LINE, /* line scope broken: macro breaks macro */
	MANDOCERR_BLK_BLANK, /* skipping blank line in line scope */

	/* related to missing arguments */
	MANDOCERR_REQ_EMPTY, /* skipping empty request: request */
	MANDOCERR_COND_EMPTY, /* conditional request controls empty scope */
	MANDOCERR_MACRO_EMPTY, /* skipping empty macro: macro */
	MANDOCERR_BLK_EMPTY, /* empty block: macro */
	MANDOCERR_ARG_EMPTY, /* empty argument, using 0n: macro arg */
	MANDOCERR_BD_NOTYPE, /* missing display type, using -ragged: Bd */
	MANDOCERR_BL_LATETYPE, /* list type is not the first argument: Bl arg */
	MANDOCERR_BL_NOWIDTH, /* missing -width in -tag list, using 6n */
	MANDOCERR_EX_NONAME, /* missing utility name, using "": Ex */
	MANDOCERR_FO_NOHEAD, /* missing function name, using "": Fo */
	MANDOCERR_IT_NOHEAD, /* empty head in list item: Bl -type It */
	MANDOCERR_IT_NOBODY, /* empty list item: Bl -type It */
	MANDOCERR_IT_NOARG, /* missing argument, using next line: Bl -c It */
	MANDOCERR_BF_NOFONT, /* missing font type, using \fR: Bf */
	MANDOCERR_BF_BADFONT, /* unknown font type, using \fR: Bf font */
	MANDOCERR_PF_SKIP, /* nothing follows prefix: Pf arg */
	MANDOCERR_RS_EMPTY, /* empty reference block: Rs */
	MANDOCERR_XR_NOSEC, /* missing section argument: Xr arg */
	MANDOCERR_ARG_STD, /* missing -std argument, adding it: macro */
	MANDOCERR_OP_EMPTY, /* missing option string, using "": OP */
	MANDOCERR_UR_NOHEAD, /* missing resource identifier, using "": UR */
	MANDOCERR_EQN_NOBOX, /* missing eqn box, using "": op */

	/* related to bad arguments */
	MANDOCERR_ARG_REP, /* duplicate argument: macro arg */
	MANDOCERR_AN_REP, /* skipping duplicate argument: An -arg */
	MANDOCERR_BD_REP, /* skipping duplicate display type: Bd -type */
	MANDOCERR_BL_REP, /* skipping duplicate list type: Bl -type */
	MANDOCERR_BL_SKIPW, /* skipping -width argument: Bl -type */
	MANDOCERR_BL_COL, /* wrong number of cells */
	MANDOCERR_AT_BAD, /* unknown AT&T UNIX version: At version */
	MANDOCERR_FA_COMMA, /* comma in function argument: arg */
	MANDOCERR_FN_PAREN, /* parenthesis in function name: arg */
	MANDOCERR_LB_BAD, /* unknown library name: Lb ... */
	MANDOCERR_RS_BAD, /* invalid content in Rs block: macro */
	MANDOCERR_SM_BAD, /* invalid Boolean argument: macro arg */
	MANDOCERR_CHAR_FONT, /* argument contains two font escapes */
	MANDOCERR_FT_BAD, /* unknown font, skipping request: ft font */
	MANDOCERR_MC_DIST, /* ignoring distance argument: mc ... arg */
	MANDOCERR_TR_ODD, /* odd number of characters in request: tr char */

	/* related to plain text */
	MANDOCERR_FI_BLANK, /* blank line in fill mode, using .sp */
	MANDOCERR_FI_TAB, /* tab in filled text */
	MANDOCERR_EOS, /* new sentence, new line */
	MANDOCERR_ESC_ARG, /* invalid escape sequence argument: esc */
	MANDOCERR_ESC_UNDEF, /* undefined escape, printing literally: char */
	MANDOCERR_STR_UNDEF, /* undefined string, using "": name */

	/* related to tables */
	MANDOCERR_TBLLAYOUT_SPAN, /* tbl line starts with span */
	MANDOCERR_TBLLAYOUT_DOWN, /* tbl column starts with span */
	MANDOCERR_TBLLAYOUT_VERT, /* skipping vertical bar in tbl layout */

	MANDOCERR_ERROR, /* ===== start of errors ===== */

	/* related to tables */
	MANDOCERR_TBLOPT_ALPHA, /* non-alphabetic character in tbl options */
	MANDOCERR_TBLOPT_BAD, /* skipping unknown tbl option: option */
	MANDOCERR_TBLOPT_NOARG, /* missing tbl option argument: option */
	MANDOCERR_TBLOPT_ARGSZ, /* wrong tbl option argument size: option */
	MANDOCERR_TBLLAYOUT_NONE, /* empty tbl layout */
	MANDOCERR_TBLLAYOUT_CHAR, /* invalid character in tbl layout: char */
	MANDOCERR_TBLLAYOUT_PAR, /* unmatched parenthesis in tbl layout */
	MANDOCERR_TBLLAYOUT_WIDTH, /* invalid column width in tbl layout */
	MANDOCERR_TBLLAYOUT_SPC, /* ignoring excessive spacing in tbl layout */
	MANDOCERR_TBLDATA_NONE, /* tbl without any data cells */
	MANDOCERR_TBLDATA_SPAN, /* ignoring data in spanned tbl cell: data */
	MANDOCERR_TBLDATA_EXTRA, /* ignoring extra tbl data cells: data */
	MANDOCERR_TBLDATA_BLK, /* data block open at end of tbl: macro */

	/* related to document structure and macros */
	MANDOCERR_PROLOG_REP, /* duplicate prologue macro: macro */
	MANDOCERR_DT_LATE, /* skipping late title macro: Dt args */
	MANDOCERR_ROFFLOOP, /* input stack limit exceeded, infinite loop? */
	MANDOCERR_CHAR_BAD, /* skipping bad character: number */
	MANDOCERR_MACRO, /* skipping unknown macro: macro */
	MANDOCERR_REQ_NOMAC, /* skipping request outside macro: ... */
	MANDOCERR_REQ_INSEC, /* skipping insecure request: request */
	MANDOCERR_IT_STRAY, /* skipping item outside list: It ... */
	MANDOCERR_TA_STRAY, /* skipping column outside column list: Ta */
	MANDOCERR_BLK_NOTOPEN, /* skipping end of block that is not open */
	MANDOCERR_RE_NOTOPEN, /* fewer RS blocks open, skipping: RE arg */
	MANDOCERR_BLK_BROKEN, /* inserting missing end of block: macro ... */
	MANDOCERR_BLK_NOEND, /* appending missing end of block: macro */

	/* related to request and macro arguments */
	MANDOCERR_NAMESC, /* escaped character not allowed in a name: name */
	MANDOCERR_ARG_UNDEF, /* using macro argument outside macro */
	MANDOCERR_ARG_NONUM, /* argument number is not numeric */
	MANDOCERR_ARG_NEG, /* negative argument, using 0: request arg */
	MANDOCERR_BD_FILE, /* NOT IMPLEMENTED: Bd -file */
	MANDOCERR_BD_NOARG, /* skipping display without arguments: Bd */
	MANDOCERR_BL_NOTYPE, /* missing list type, using -item: Bl */
	MANDOCERR_CE_NONUM, /* argument is not numeric, using 1: ce ... */
	MANDOCERR_CHAR_ARG, /* argument is not a character: char ... */
	MANDOCERR_MC_ESC, /* skipping unusable escape sequence: mc arg */
	MANDOCERR_NM_NONAME, /* missing manual name, using "": Nm */
	MANDOCERR_OS_UNAME, /* uname(3) system call failed, using UNKNOWN */
	MANDOCERR_ST_BAD, /* unknown standard specifier: St standard */
	MANDOCERR_IT_NONUM, /* skipping request without numeric argument */
	MANDOCERR_SHIFT, /* excessive shift: ..., but max is ... */
	MANDOCERR_SO_PATH, /* NOT IMPLEMENTED: .so with absolute path or ".." */
	MANDOCERR_SO_FAIL, /* .so request failed */
	MANDOCERR_TG_SPC, /* skipping tag containing whitespace: tag */
	MANDOCERR_ARG_SKIP, /* skipping all arguments: macro args */
	MANDOCERR_ARG_EXCESS, /* skipping excess arguments: macro ... args */
	MANDOCERR_DIVZERO, /* divide by zero */

	/* related to escape sequences */
	MANDOCERR_ESC_INCOMPLETE, /* incomplete escape sequence: esc */
	MANDOCERR_ESC_BADCHAR, /* invalid special character: esc */
	MANDOCERR_ESC_UNKCHAR, /* unknown special character: esc */
	MANDOCERR_ESC_DELIM, /* invalid escape argument delimiter: esc */

	MANDOCERR_UNSUPP, /* ===== start of unsupported features ===== */

	MANDOCERR_TOOLARGE, /* input too large */
	MANDOCERR_CHAR_UNSUPP, /* unsupported control character: number */
	MANDOCERR_ESC_UNSUPP, /* unsupported escape sequence: escape */
	MANDOCERR_REQ_UNSUPP, /* unsupported roff request: request */
	MANDOCERR_WHILE_NEST, /* nested .while loops */
	MANDOCERR_WHILE_OUTOF, /* end of scope with open .while loop */
	MANDOCERR_WHILE_INTO, /* end of .while loop in inner scope */
	MANDOCERR_WHILE_FAIL, /* cannot continue this .while loop */
	MANDOCERR_TBLOPT_EQN, /* eqn delim option in tbl: arg */
	MANDOCERR_TBLLAYOUT_MOD, /* unsupported tbl layout modifier: m */
	MANDOCERR_TBLMACRO, /* ignoring macro in table: macro */
	MANDOCERR_TBL_TMAN, /* skipping tbl in -Tman mode */
	MANDOCERR_EQN_TMAN, /* skipping eqn in -Tman mode */

	MANDOCERR_BADARG, /* ===== start of bad invocations ===== */

	MANDOCERR_BADARG_BAD, /* bad argument */
	MANDOCERR_BADARG_DUPE, /* duplicate argument */
	MANDOCERR_BADVAL, /* does not take a value */
	MANDOCERR_BADVAL_MISS, /* missing argument value */
	MANDOCERR_BADVAL_BAD, /* bad argument value */
	MANDOCERR_BADVAL_DUPE, /* duplicate argument value */
	MANDOCERR_TAG, /* no such tag */
	MANDOCERR_MAN_TMARKDOWN, /* -Tmarkdown unsupported for man(7) input */

	MANDOCERR_SYSERR, /* ===== start of system errors ===== */

	MANDOCERR_DUP,
	MANDOCERR_EXEC,
	MANDOCERR_FDOPEN,
	MANDOCERR_FFLUSH,
	MANDOCERR_FORK,
	MANDOCERR_FSTAT,
	MANDOCERR_GETLINE,
	MANDOCERR_GLOB,
	MANDOCERR_GZCLOSE,
	MANDOCERR_GZDOPEN,
	MANDOCERR_MKSTEMP,
	MANDOCERR_OPEN,
	MANDOCERR_PLEDGE,
	MANDOCERR_READ,
	MANDOCERR_WAIT,
	MANDOCERR_WRITE,

	MANDOCERR_MAX
};

enum	mandoc_esc {
	ESCAPE_EXPAND = 0, /* interpolation and iterative call needed */
	ESCAPE_ERROR, /* non-fatal error: unparsable escape */
	ESCAPE_UNSUPP, /* unsupported escape: warn and ignore */
	ESCAPE_IGNORE, /* valid escape to be ignored */
	ESCAPE_UNDEF, /* undefined escape: print literal character */
	ESCAPE_SPECIAL, /* special character escape */
	ESCAPE_FONT, /* a generic font mode */
	ESCAPE_FONTBOLD, /* bold font mode */
	ESCAPE_FONTITALIC, /* italic font mode */
	ESCAPE_FONTBI, /* bold italic font mode */
	ESCAPE_FONTROMAN, /* roman font mode */
	ESCAPE_FONTCR, /* constant width font mode */
	ESCAPE_FONTCB, /* constant width bold font mode */
	ESCAPE_FONTCI, /* constant width italic font mode */
	ESCAPE_FONTPREV, /* previous font mode */
	ESCAPE_NUMBERED, /* a numbered glyph */
	ESCAPE_UNICODE, /* a unicode codepoint */
	ESCAPE_DEVICE, /* print the output device name */
	ESCAPE_BREAK, /* break the output line */
	ESCAPE_NOSPACE, /* suppress space if the last on a line */
	ESCAPE_HORIZ, /* horizontal movement */
	ESCAPE_HLINE, /* horizontal line drawing */
	ESCAPE_SKIPCHAR, /* skip the next character */
	ESCAPE_OVERSTRIKE /* overstrike all chars in the argument */
};


enum mandoc_esc	  mandoc_font(const char *, int);
enum mandoc_esc	  mandoc_escape(const char **, const char **, int *);
void		  mandoc_msg_setoutfile(FILE *);
const char	 *mandoc_msg_getinfilename(void);
void		  mandoc_msg_setinfilename(const char *);
enum mandocerr	  mandoc_msg_getmin(void);
void		  mandoc_msg_setmin(enum mandocerr);
enum mandoclevel  mandoc_msg_getrc(void);
void		  mandoc_msg_setrc(enum mandoclevel);
void		  mandoc_msg(enum mandocerr, int, int, const char *, ...)
			__attribute__((__format__ (__printf__, 4, 5)));
void		  mandoc_msg_summary(void);
void		  mchars_alloc(void);
void		  mchars_free(void);
int		  mchars_num2char(const char *, size_t);
const char	 *mchars_uc2str(int);
int		  mchars_num2uc(const char *, size_t);
int		  mchars_spec2cp(const char *, size_t);
const char	 *mchars_spec2str(const char *, size_t, size_t *);
