/*	$Id: html.h,v 1.72 2015/11/07 14:01:16 schwarze Exp $ */
/*
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
 */

enum	htmltag {
	TAG_HTML,
	TAG_HEAD,
	TAG_BODY,
	TAG_META,
	TAG_TITLE,
	TAG_DIV,
	TAG_H1,
	TAG_H2,
	TAG_SPAN,
	TAG_LINK,
	TAG_BR,
	TAG_A,
	TAG_TABLE,
	TAG_TBODY,
	TAG_COL,
	TAG_TR,
	TAG_TD,
	TAG_LI,
	TAG_UL,
	TAG_OL,
	TAG_DL,
	TAG_DT,
	TAG_DD,
	TAG_BLOCKQUOTE,
	TAG_PRE,
	TAG_B,
	TAG_I,
	TAG_CODE,
	TAG_SMALL,
	TAG_STYLE,
	TAG_MATH,
	TAG_MROW,
	TAG_MI,
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

enum	htmlattr {
	ATTR_NAME,
	ATTR_REL,
	ATTR_HREF,
	ATTR_TYPE,
	ATTR_MEDIA,
	ATTR_CLASS,
	ATTR_STYLE,
	ATTR_ID,
	ATTR_COLSPAN,
	ATTR_CHARSET,
	ATTR_OPEN,
	ATTR_CLOSE,
	ATTR_MATHVARIANT,
	ATTR_MAX
};

enum	htmlfont {
	HTMLFONT_NONE = 0,
	HTMLFONT_BOLD,
	HTMLFONT_ITALIC,
	HTMLFONT_BI,
	HTMLFONT_MAX
};

struct	tag {
	struct tag	 *next;
	enum htmltag	  tag;
};

struct tagq {
	struct tag	 *head;
};

struct	htmlpair {
	enum htmlattr	  key;
	const char	 *val;
};

#define	PAIR_INIT(p, t, v) \
	do { \
		(p)->key = (t); \
		(p)->val = (v); \
	} while (/* CONSTCOND */ 0)

#define	PAIR_ID_INIT(p, v)	PAIR_INIT(p, ATTR_ID, v)
#define	PAIR_CLASS_INIT(p, v)	PAIR_INIT(p, ATTR_CLASS, v)
#define	PAIR_HREF_INIT(p, v)	PAIR_INIT(p, ATTR_HREF, v)
#define	PAIR_STYLE_INIT(p, h)	PAIR_INIT(p, ATTR_STYLE, (h)->buf)

struct	html {
	int		  flags;
#define	HTML_NOSPACE	 (1 << 0) /* suppress next space */
#define	HTML_IGNDELIM	 (1 << 1)
#define	HTML_KEEP	 (1 << 2)
#define	HTML_PREKEEP	 (1 << 3)
#define	HTML_NONOSPACE	 (1 << 4) /* never add spaces */
#define	HTML_LITERAL	 (1 << 5) /* literal (e.g., <PRE>) context */
#define	HTML_SKIPCHAR	 (1 << 6) /* skip the next character */
#define	HTML_NOSPLIT	 (1 << 7) /* do not break line before .An */
#define	HTML_SPLIT	 (1 << 8) /* break line before .An */
#define	HTML_NONEWLINE	 (1 << 9) /* No line break in nofill mode. */
	struct tagq	  tags; /* stack of open tags */
	struct rofftbl	  tbl; /* current table */
	struct tag	 *tblt; /* current open table scope */
	char		 *base_man; /* base for manpage href */
	char		 *base_includes; /* base for include href */
	char		 *style; /* style-sheet URI */
	char		  buf[BUFSIZ]; /* see bufcat and friends */
	size_t		  buflen;
	struct tag	 *metaf; /* current open font scope */
	enum htmlfont	  metal; /* last used font */
	enum htmlfont	  metac; /* current font mode */
	int		  oflags; /* output options */
#define	HTML_FRAGMENT	 (1 << 0) /* don't emit HTML/HEAD/BODY */
};


struct	tbl_span;
struct	eqn;

void		  print_gen_decls(struct html *);
void		  print_gen_head(struct html *);
struct tag	 *print_otag(struct html *, enum htmltag,
				int, const struct htmlpair *);
void		  print_tagq(struct html *, const struct tag *);
void		  print_stagq(struct html *, const struct tag *);
void		  print_text(struct html *, const char *);
void		  print_tblclose(struct html *);
void		  print_tbl(struct html *, const struct tbl_span *);
void		  print_eqn(struct html *, const struct eqn *);
void		  print_paragraph(struct html *);

#if __GNUC__ - 0 >= 4
__attribute__((__format__ (__printf__, 2, 3)))
#endif
void		  bufcat_fmt(struct html *, const char *, ...);
void		  bufcat(struct html *, const char *);
void		  bufcat_id(struct html *, const char *);
void		  bufcat_style(struct html *,
			const char *, const char *);
void		  bufcat_su(struct html *, const char *,
			const struct roffsu *);
void		  bufinit(struct html *);
void		  buffmt_man(struct html *,
			const char *, const char *);
void		  buffmt_includes(struct html *, const char *);

int		  html_strlen(const char *);
