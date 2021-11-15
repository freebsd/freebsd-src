/* $Id: html.h,v 1.109 2021/09/09 14:47:24 schwarze Exp $ */
/*
 * Copyright (c) 2017, 2018, 2019, 2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 *
 * Internal interfaces for mandoc(1) HTML formatters.
 * For use by the individual HTML formatters only.
 */

enum	htmltag {
	TAG_HTML,
	TAG_HEAD,
	TAG_META,
	TAG_LINK,
	TAG_STYLE,
	TAG_TITLE,
	TAG_BODY,
	TAG_DIV,
	TAG_SECTION,
	TAG_TABLE,
	TAG_TR,
	TAG_TD,
	TAG_LI,
	TAG_UL,
	TAG_OL,
	TAG_DL,
	TAG_DT,
	TAG_DD,
	TAG_H1,
	TAG_H2,
	TAG_P,
	TAG_PRE,
	TAG_A,
	TAG_B,
	TAG_CITE,
	TAG_CODE,
	TAG_I,
	TAG_SMALL,
	TAG_SPAN,
	TAG_VAR,
	TAG_BR,
	TAG_HR,
	TAG_MARK,
	TAG_MATH,
	TAG_MROW,
	TAG_MI,
	TAG_MN,
	TAG_MO,
	TAG_MSUP,
	TAG_MSUB,
	TAG_MSUBSUP,
	TAG_MFRAC,
	TAG_MSQRT,
	TAG_MFENCED,
	TAG_MTABLE,
	TAG_MTR,
	TAG_MTD,
	TAG_MUNDEROVER,
	TAG_MUNDER,
	TAG_MOVER,
	TAG_MAX
};

struct	tag {
	struct tag	 *next;
	int		  refcnt;
	int		  closed;
	enum htmltag	  tag;
};

struct	html {
	int		  flags;
#define	HTML_NOSPACE	 (1 << 0) /* suppress next space */
#define	HTML_IGNDELIM	 (1 << 1)
#define	HTML_KEEP	 (1 << 2)
#define	HTML_PREKEEP	 (1 << 3)
#define	HTML_NONOSPACE	 (1 << 4) /* never add spaces */
#define	HTML_SKIPCHAR	 (1 << 6) /* skip the next character */
#define	HTML_NOSPLIT	 (1 << 7) /* do not break line before .An */
#define	HTML_SPLIT	 (1 << 8) /* break line before .An */
#define	HTML_NONEWLINE	 (1 << 9) /* No line break in nofill mode. */
#define	HTML_BUFFER	 (1 << 10) /* Collect a word to see if it fits. */
#define	HTML_TOCDONE	 (1 << 11) /* The TOC was already written. */
	size_t		  indent; /* current output indentation level */
	int		  noindent; /* indent disabled by <pre> */
	size_t		  col; /* current output byte position */
	size_t		  bufcol; /* current buf byte position */
	char		  buf[80]; /* output buffer */
	struct tag	 *tag; /* last open tag */
	struct rofftbl	  tbl; /* current table */
	struct tag	 *tblt; /* current open table scope */
	char		 *base_man1; /* bases for manpage href */
	char		 *base_man2;
	char		 *base_includes; /* base for include href */
	char		 *style; /* style-sheet URI */
	struct tag	 *metaf; /* current open font scope */
	enum mandoc_esc	  metal; /* last used font */
	enum mandoc_esc	  metac; /* current font mode */
	int		  oflags; /* output options */
#define	HTML_FRAGMENT	 (1 << 0) /* don't emit HTML/HEAD/BODY */
#define	HTML_TOC	 (1 << 1) /* emit a table of contents */
};


struct	roff_node;
struct	tbl_span;
struct	eqn_box;

void		  roff_html_pre(struct html *, const struct roff_node *);

void		  print_gen_comment(struct html *, struct roff_node *);
void		  print_gen_decls(struct html *);
void		  print_gen_head(struct html *);
struct tag	 *print_otag(struct html *, enum htmltag, const char *, ...);
struct tag	 *print_otag_id(struct html *, enum htmltag, const char *,
			struct roff_node *);
void		  print_tagq(struct html *, const struct tag *);
void		  print_stagq(struct html *, const struct tag *);
void		  print_tagged_text(struct html *, const char *,
			struct roff_node *);
void		  print_text(struct html *, const char *);
void		  print_tblclose(struct html *);
void		  print_tbl(struct html *, const struct tbl_span *);
void		  print_eqn(struct html *, const struct eqn_box *);
void		  print_endline(struct html *);

void		  html_close_paragraph(struct html *);
enum roff_tok	  html_fillmode(struct html *, enum roff_tok);
char		 *html_make_id(const struct roff_node *, int);
int		  html_setfont(struct html *, enum mandoc_esc);
