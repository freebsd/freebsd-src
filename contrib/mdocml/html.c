/*	$Id: html.c,v 1.219 2017/07/15 17:57:51 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2015, 2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "out.h"
#include "html.h"
#include "manconf.h"
#include "main.h"

struct	htmldata {
	const char	 *name;
	int		  flags;
#define	HTML_NOSTACK	 (1 << 0)
#define	HTML_AUTOCLOSE	 (1 << 1)
#define	HTML_NLBEFORE	 (1 << 2)
#define	HTML_NLBEGIN	 (1 << 3)
#define	HTML_NLEND	 (1 << 4)
#define	HTML_NLAFTER	 (1 << 5)
#define	HTML_NLAROUND	 (HTML_NLBEFORE | HTML_NLAFTER)
#define	HTML_NLINSIDE	 (HTML_NLBEGIN | HTML_NLEND)
#define	HTML_NLALL	 (HTML_NLAROUND | HTML_NLINSIDE)
#define	HTML_INDENT	 (1 << 6)
#define	HTML_NOINDENT	 (1 << 7)
};

static	const struct htmldata htmltags[TAG_MAX] = {
	{"html",	HTML_NLALL},
	{"head",	HTML_NLALL | HTML_INDENT},
	{"body",	HTML_NLALL},
	{"meta",	HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"title",	HTML_NLAROUND},
	{"div",		HTML_NLAROUND},
	{"h1",		HTML_NLAROUND},
	{"h2",		HTML_NLAROUND},
	{"span",	0},
	{"link",	HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"br",		HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"a",		0},
	{"table",	HTML_NLALL | HTML_INDENT},
	{"colgroup",	HTML_NLALL | HTML_INDENT},
	{"col",		HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"tr",		HTML_NLALL | HTML_INDENT},
	{"td",		HTML_NLAROUND},
	{"li",		HTML_NLAROUND | HTML_INDENT},
	{"ul",		HTML_NLALL | HTML_INDENT},
	{"ol",		HTML_NLALL | HTML_INDENT},
	{"dl",		HTML_NLALL | HTML_INDENT},
	{"dt",		HTML_NLAROUND},
	{"dd",		HTML_NLAROUND | HTML_INDENT},
	{"pre",		HTML_NLALL | HTML_NOINDENT},
	{"var",		0},
	{"cite",	0},
	{"b",		0},
	{"i",		0},
	{"code",	0},
	{"small",	0},
	{"style",	HTML_NLALL | HTML_INDENT},
	{"math",	HTML_NLALL | HTML_INDENT},
	{"mrow",	0},
	{"mi",		0},
	{"mn",		0},
	{"mo",		0},
	{"msup",	0},
	{"msub",	0},
	{"msubsup",	0},
	{"mfrac",	0},
	{"msqrt",	0},
	{"mfenced",	0},
	{"mtable",	0},
	{"mtr",		0},
	{"mtd",		0},
	{"munderover",	0},
	{"munder",	0},
	{"mover",	0},
};

static	const char	*const roffscales[SCALE_MAX] = {
	"cm", /* SCALE_CM */
	"in", /* SCALE_IN */
	"pc", /* SCALE_PC */
	"pt", /* SCALE_PT */
	"em", /* SCALE_EM */
	"em", /* SCALE_MM */
	"ex", /* SCALE_EN */
	"ex", /* SCALE_BU */
	"em", /* SCALE_VS */
	"ex", /* SCALE_FS */
};

static	void	 a2width(const char *, struct roffsu *);
static	void	 print_byte(struct html *, char);
static	void	 print_endword(struct html *);
static	void	 print_indent(struct html *);
static	void	 print_word(struct html *, const char *);

static	void	 print_ctag(struct html *, struct tag *);
static	int	 print_escape(struct html *, char);
static	int	 print_encode(struct html *, const char *, const char *, int);
static	void	 print_href(struct html *, const char *, const char *, int);
static	void	 print_metaf(struct html *, enum mandoc_esc);


void *
html_alloc(const struct manoutput *outopts)
{
	struct html	*h;

	h = mandoc_calloc(1, sizeof(struct html));

	h->tag = NULL;
	h->style = outopts->style;
	h->base_man = outopts->man;
	h->base_includes = outopts->includes;
	if (outopts->fragment)
		h->oflags |= HTML_FRAGMENT;

	return h;
}

void
html_free(void *p)
{
	struct tag	*tag;
	struct html	*h;

	h = (struct html *)p;

	while ((tag = h->tag) != NULL) {
		h->tag = tag->next;
		free(tag);
	}

	free(h);
}

void
print_gen_head(struct html *h)
{
	struct tag	*t;

	print_otag(h, TAG_META, "?", "charset", "utf-8");

	/*
	 * Print a default style-sheet.
	 */

	t = print_otag(h, TAG_STYLE, "");
	print_text(h, "table.head, table.foot { width: 100%; }");
	print_endline(h);
	print_text(h, "td.head-rtitle, td.foot-os { text-align: right; }");
	print_endline(h);
	print_text(h, "td.head-vol { text-align: center; }");
	print_endline(h);
	print_text(h, "div.Pp { margin: 1ex 0ex; }");
	print_tagq(h, t);

	if (h->style)
		print_otag(h, TAG_LINK, "?h??", "rel", "stylesheet",
		    h->style, "type", "text/css", "media", "all");
}

static void
print_metaf(struct html *h, enum mandoc_esc deco)
{
	enum htmlfont	 font;

	switch (deco) {
	case ESCAPE_FONTPREV:
		font = h->metal;
		break;
	case ESCAPE_FONTITALIC:
		font = HTMLFONT_ITALIC;
		break;
	case ESCAPE_FONTBOLD:
		font = HTMLFONT_BOLD;
		break;
	case ESCAPE_FONTBI:
		font = HTMLFONT_BI;
		break;
	case ESCAPE_FONT:
	case ESCAPE_FONTROMAN:
		font = HTMLFONT_NONE;
		break;
	default:
		abort();
	}

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->metal = h->metac;
	h->metac = font;

	switch (font) {
	case HTMLFONT_ITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case HTMLFONT_BOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case HTMLFONT_BI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	default:
		break;
	}
}

char *
html_make_id(const struct roff_node *n)
{
	const struct roff_node	*nch;
	char			*buf, *cp;

	for (nch = n->child; nch != NULL; nch = nch->next)
		if (nch->type != ROFFT_TEXT)
			return NULL;

	buf = NULL;
	deroff(&buf, n);

	/* http://www.w3.org/TR/html5/dom.html#the-id-attribute */

	for (cp = buf; *cp != '\0'; cp++)
		if (*cp == ' ')
			*cp = '_';

	return buf;
}

int
html_strlen(const char *cp)
{
	size_t		 rsz;
	int		 skip, sz;

	/*
	 * Account for escaped sequences within string length
	 * calculations.  This follows the logic in term_strlen() as we
	 * must calculate the width of produced strings.
	 * Assume that characters are always width of "1".  This is
	 * hacky, but it gets the job done for approximation of widths.
	 */

	sz = 0;
	skip = 0;
	while (1) {
		rsz = strcspn(cp, "\\");
		if (rsz) {
			cp += rsz;
			if (skip) {
				skip = 0;
				rsz--;
			}
			sz += rsz;
		}
		if ('\0' == *cp)
			break;
		cp++;
		switch (mandoc_escape(&cp, NULL, NULL)) {
		case ESCAPE_ERROR:
			return sz;
		case ESCAPE_UNICODE:
		case ESCAPE_NUMBERED:
		case ESCAPE_SPECIAL:
		case ESCAPE_OVERSTRIKE:
			if (skip)
				skip = 0;
			else
				sz++;
			break;
		case ESCAPE_SKIPCHAR:
			skip = 1;
			break;
		default:
			break;
		}
	}
	return sz;
}

static int
print_escape(struct html *h, char c)
{

	switch (c) {
	case '<':
		print_word(h, "&lt;");
		break;
	case '>':
		print_word(h, "&gt;");
		break;
	case '&':
		print_word(h, "&amp;");
		break;
	case '"':
		print_word(h, "&quot;");
		break;
	case ASCII_NBRSP:
		print_word(h, "&nbsp;");
		break;
	case ASCII_HYPH:
		print_byte(h, '-');
		break;
	case ASCII_BREAK:
		break;
	default:
		return 0;
	}
	return 1;
}

static int
print_encode(struct html *h, const char *p, const char *pend, int norecurse)
{
	char		 numbuf[16];
	struct tag	*t;
	const char	*seq;
	size_t		 sz;
	int		 c, len, breakline, nospace;
	enum mandoc_esc	 esc;
	static const char rejs[10] = { ' ', '\\', '<', '>', '&', '"',
		ASCII_NBRSP, ASCII_HYPH, ASCII_BREAK, '\0' };

	if (pend == NULL)
		pend = strchr(p, '\0');

	breakline = 0;
	nospace = 0;

	while (p < pend) {
		if (HTML_SKIPCHAR & h->flags && '\\' != *p) {
			h->flags &= ~HTML_SKIPCHAR;
			p++;
			continue;
		}

		for (sz = strcspn(p, rejs); sz-- && p < pend; p++)
			print_byte(h, *p);

		if (breakline &&
		    (p >= pend || *p == ' ' || *p == ASCII_NBRSP)) {
			t = print_otag(h, TAG_DIV, "");
			print_text(h, "\\~");
			print_tagq(h, t);
			breakline = 0;
			while (p < pend && (*p == ' ' || *p == ASCII_NBRSP))
				p++;
			continue;
		}

		if (p >= pend)
			break;

		if (*p == ' ') {
			print_endword(h);
			p++;
			continue;
		}

		if (print_escape(h, *p++))
			continue;

		esc = mandoc_escape(&p, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		switch (esc) {
		case ESCAPE_FONT:
		case ESCAPE_FONTPREV:
		case ESCAPE_FONTBOLD:
		case ESCAPE_FONTITALIC:
		case ESCAPE_FONTBI:
		case ESCAPE_FONTROMAN:
			if (0 == norecurse)
				print_metaf(h, esc);
			continue;
		case ESCAPE_SKIPCHAR:
			h->flags |= HTML_SKIPCHAR;
			continue;
		default:
			break;
		}

		if (h->flags & HTML_SKIPCHAR) {
			h->flags &= ~HTML_SKIPCHAR;
			continue;
		}

		switch (esc) {
		case ESCAPE_UNICODE:
			/* Skip past "u" header. */
			c = mchars_num2uc(seq + 1, len - 1);
			break;
		case ESCAPE_NUMBERED:
			c = mchars_num2char(seq, len);
			if (c < 0)
				continue;
			break;
		case ESCAPE_SPECIAL:
			c = mchars_spec2cp(seq, len);
			if (c <= 0)
				continue;
			break;
		case ESCAPE_BREAK:
			breakline = 1;
			continue;
		case ESCAPE_NOSPACE:
			if ('\0' == *p)
				nospace = 1;
			continue;
		case ESCAPE_OVERSTRIKE:
			if (len == 0)
				continue;
			c = seq[len - 1];
			break;
		default:
			continue;
		}
		if ((c < 0x20 && c != 0x09) ||
		    (c > 0x7E && c < 0xA0))
			c = 0xFFFD;
		if (c > 0x7E) {
			(void)snprintf(numbuf, sizeof(numbuf), "&#x%.4X;", c);
			print_word(h, numbuf);
		} else if (print_escape(h, c) == 0)
			print_byte(h, c);
	}

	return nospace;
}

static void
print_href(struct html *h, const char *name, const char *sec, int man)
{
	const char	*p, *pp;

	pp = man ? h->base_man : h->base_includes;
	while ((p = strchr(pp, '%')) != NULL) {
		print_encode(h, pp, p, 1);
		if (man && p[1] == 'S') {
			if (sec == NULL)
				print_byte(h, '1');
			else
				print_encode(h, sec, NULL, 1);
		} else if ((man && p[1] == 'N') ||
		    (man == 0 && p[1] == 'I'))
			print_encode(h, name, NULL, 1);
		else
			print_encode(h, p, p + 2, 1);
		pp = p + 2;
	}
	if (*pp != '\0')
		print_encode(h, pp, NULL, 1);
}

struct tag *
print_otag(struct html *h, enum htmltag tag, const char *fmt, ...)
{
	va_list		 ap;
	struct roffsu	 mysu, *su;
	char		 numbuf[16];
	struct tag	*t;
	const char	*attr;
	char		*arg1, *arg2;
	double		 v;
	int		 i, have_style, tflags;

	tflags = htmltags[tag].flags;

	/* Push this tag onto the stack of open scopes. */

	if ((tflags & HTML_NOSTACK) == 0) {
		t = mandoc_malloc(sizeof(struct tag));
		t->tag = tag;
		t->next = h->tag;
		h->tag = t;
	} else
		t = NULL;

	if (tflags & HTML_NLBEFORE)
		print_endline(h);
	if (h->col == 0)
		print_indent(h);
	else if ((h->flags & HTML_NOSPACE) == 0) {
		if (h->flags & HTML_KEEP)
			print_word(h, "&#x00A0;");
		else {
			if (h->flags & HTML_PREKEEP)
				h->flags |= HTML_KEEP;
			print_endword(h);
		}
	}

	if ( ! (h->flags & HTML_NONOSPACE))
		h->flags &= ~HTML_NOSPACE;
	else
		h->flags |= HTML_NOSPACE;

	/* Print out the tag name and attributes. */

	print_byte(h, '<');
	print_word(h, htmltags[tag].name);

	va_start(ap, fmt);

	have_style = 0;
	while (*fmt != '\0') {
		if (*fmt == 's') {
			have_style = 1;
			fmt++;
			break;
		}

		/* Parse a non-style attribute and its arguments. */

		arg1 = va_arg(ap, char *);
		switch (*fmt++) {
		case 'c':
			attr = "class";
			break;
		case 'h':
			attr = "href";
			break;
		case 'i':
			attr = "id";
			break;
		case '?':
			attr = arg1;
			arg1 = va_arg(ap, char *);
			break;
		default:
			abort();
		}
		arg2 = NULL;
		if (*fmt == 'M')
			arg2 = va_arg(ap, char *);
		if (arg1 == NULL)
			continue;

		/* Print the non-style attributes. */

		print_byte(h, ' ');
		print_word(h, attr);
		print_byte(h, '=');
		print_byte(h, '"');
		switch (*fmt) {
		case 'I':
			print_href(h, arg1, NULL, 0);
			fmt++;
			break;
		case 'M':
			print_href(h, arg1, arg2, 1);
			fmt++;
			break;
		case 'R':
			print_byte(h, '#');
			print_encode(h, arg1, NULL, 1);
			fmt++;
			break;
		case 'T':
			print_encode(h, arg1, NULL, 1);
			print_word(h, "\" title=\"");
			print_encode(h, arg1, NULL, 1);
			fmt++;
			break;
		default:
			print_encode(h, arg1, NULL, 1);
			break;
		}
		print_byte(h, '"');
	}

	/* Print out styles. */

	while (*fmt != '\0') {
		arg1 = NULL;
		su = NULL;

		/* First letter: input argument type. */

		switch (*fmt++) {
		case 'h':
			i = va_arg(ap, int);
			su = &mysu;
			SCALE_HS_INIT(su, i);
			break;
		case 's':
			arg1 = va_arg(ap, char *);
			break;
		case 'u':
			su = va_arg(ap, struct roffsu *);
			break;
		case 'w':
			if ((arg2 = va_arg(ap, char *)) != NULL) {
				su = &mysu;
				a2width(arg2, su);
			}
			if (*fmt == '*') {
				if (su != NULL && su->unit == SCALE_EN &&
				    su->scale > 5.9 && su->scale < 6.1)
					su = NULL;
				fmt++;
			}
			if (*fmt == '+') {
				if (su != NULL) {
					/* Make even bold text fit. */
					su->scale *= 1.2;
					/* Add padding. */
					su->scale += 3.0;
				}
				fmt++;
			}
			if (*fmt == '-') {
				if (su != NULL)
					su->scale *= -1.0;
				fmt++;
			}
			break;
		default:
			abort();
		}

		/* Second letter: style name. */

		switch (*fmt++) {
		case 'h':
			attr = "height";
			break;
		case 'i':
			attr = "text-indent";
			break;
		case 'l':
			attr = "margin-left";
			break;
		case 'w':
			attr = "width";
			break;
		case 'W':
			attr = "min-width";
			break;
		case '?':
			attr = arg1;
			arg1 = va_arg(ap, char *);
			break;
		default:
			abort();
		}
		if (su == NULL && arg1 == NULL)
			continue;

		if (have_style == 1)
			print_word(h, " style=\"");
		else
			print_byte(h, ' ');
		print_word(h, attr);
		print_byte(h, ':');
		print_byte(h, ' ');
		if (su != NULL) {
			v = su->scale;
			if (su->unit == SCALE_MM && (v /= 100.0) == 0.0)
				v = 1.0;
			else if (su->unit == SCALE_BU)
				v /= 24.0;
			(void)snprintf(numbuf, sizeof(numbuf), "%.2f", v);
			print_word(h, numbuf);
			print_word(h, roffscales[su->unit]);
		} else
			print_word(h, arg1);
		print_byte(h, ';');
		have_style = 2;
	}
	if (have_style == 2)
		print_byte(h, '"');

	va_end(ap);

	/* Accommodate for "well-formed" singleton escaping. */

	if (HTML_AUTOCLOSE & htmltags[tag].flags)
		print_byte(h, '/');

	print_byte(h, '>');

	if (tflags & HTML_NLBEGIN)
		print_endline(h);
	else
		h->flags |= HTML_NOSPACE;

	if (tflags & HTML_INDENT)
		h->indent++;
	if (tflags & HTML_NOINDENT)
		h->noindent++;

	return t;
}

static void
print_ctag(struct html *h, struct tag *tag)
{
	int	 tflags;

	/*
	 * Remember to close out and nullify the current
	 * meta-font and table, if applicable.
	 */
	if (tag == h->metaf)
		h->metaf = NULL;
	if (tag == h->tblt)
		h->tblt = NULL;

	tflags = htmltags[tag->tag].flags;

	if (tflags & HTML_INDENT)
		h->indent--;
	if (tflags & HTML_NOINDENT)
		h->noindent--;
	if (tflags & HTML_NLEND)
		print_endline(h);
	print_indent(h);
	print_byte(h, '<');
	print_byte(h, '/');
	print_word(h, htmltags[tag->tag].name);
	print_byte(h, '>');
	if (tflags & HTML_NLAFTER)
		print_endline(h);

	h->tag = tag->next;
	free(tag);
}

void
print_gen_decls(struct html *h)
{
	print_word(h, "<!DOCTYPE html>");
	print_endline(h);
}

void
print_text(struct html *h, const char *word)
{
	if (h->col && (h->flags & HTML_NOSPACE) == 0) {
		if ( ! (HTML_KEEP & h->flags)) {
			if (HTML_PREKEEP & h->flags)
				h->flags |= HTML_KEEP;
			print_endword(h);
		} else
			print_word(h, "&#x00A0;");
	}

	assert(NULL == h->metaf);
	switch (h->metac) {
	case HTMLFONT_ITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case HTMLFONT_BOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case HTMLFONT_BI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	default:
		print_indent(h);
		break;
	}

	assert(word);
	if ( ! print_encode(h, word, NULL, 0)) {
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONEWLINE;
	} else
		h->flags |= HTML_NOSPACE | HTML_NONEWLINE;

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->flags &= ~HTML_IGNDELIM;
}

void
print_tagq(struct html *h, const struct tag *until)
{
	struct tag	*tag;

	while ((tag = h->tag) != NULL) {
		print_ctag(h, tag);
		if (until && tag == until)
			return;
	}
}

void
print_stagq(struct html *h, const struct tag *suntil)
{
	struct tag	*tag;

	while ((tag = h->tag) != NULL) {
		if (suntil && tag == suntil)
			return;
		print_ctag(h, tag);
	}
}

void
print_paragraph(struct html *h)
{
	struct tag	*t;

	t = print_otag(h, TAG_DIV, "c", "Pp");
	print_tagq(h, t);
}


/***********************************************************************
 * Low level output functions.
 * They implement line breaking using a short static buffer.
 ***********************************************************************/

/*
 * Buffer one HTML output byte.
 * If the buffer is full, flush and deactivate it and start a new line.
 * If the buffer is inactive, print directly.
 */
static void
print_byte(struct html *h, char c)
{
	if ((h->flags & HTML_BUFFER) == 0) {
		putchar(c);
		h->col++;
		return;
	}

	if (h->col + h->bufcol < sizeof(h->buf)) {
		h->buf[h->bufcol++] = c;
		return;
	}

	putchar('\n');
	h->col = 0;
	print_indent(h);
	putchar(' ');
	putchar(' ');
	fwrite(h->buf, h->bufcol, 1, stdout);
	putchar(c);
	h->col = (h->indent + 1) * 2 + h->bufcol + 1;
	h->bufcol = 0;
	h->flags &= ~HTML_BUFFER;
}

/*
 * If something was printed on the current output line, end it.
 * Not to be called right after print_indent().
 */
void
print_endline(struct html *h)
{
	if (h->col == 0)
		return;

	if (h->bufcol) {
		putchar(' ');
		fwrite(h->buf, h->bufcol, 1, stdout);
		h->bufcol = 0;
	}
	putchar('\n');
	h->col = 0;
	h->flags |= HTML_NOSPACE;
	h->flags &= ~HTML_BUFFER;
}

/*
 * Flush the HTML output buffer.
 * If it is inactive, activate it.
 */
static void
print_endword(struct html *h)
{
	if (h->noindent) {
		print_byte(h, ' ');
		return;
	}

	if ((h->flags & HTML_BUFFER) == 0) {
		h->col++;
		h->flags |= HTML_BUFFER;
	} else if (h->bufcol) {
		putchar(' ');
		fwrite(h->buf, h->bufcol, 1, stdout);
		h->col += h->bufcol + 1;
	}
	h->bufcol = 0;
}

/*
 * If at the beginning of a new output line,
 * perform indentation and mark the line as containing output.
 * Make sure to really produce some output right afterwards,
 * but do not use print_otag() for producing it.
 */
static void
print_indent(struct html *h)
{
	size_t	 i;

	if (h->col)
		return;

	if (h->noindent == 0) {
		h->col = h->indent * 2;
		for (i = 0; i < h->col; i++)
			putchar(' ');
	}
	h->flags &= ~HTML_NOSPACE;
}

/*
 * Print or buffer some characters
 * depending on the current HTML output buffer state.
 */
static void
print_word(struct html *h, const char *cp)
{
	while (*cp != '\0')
		print_byte(h, *cp++);
}

/*
 * Calculate the scaling unit passed in a `-width' argument.  This uses
 * either a native scaling unit (e.g., 1i, 2m) or the string length of
 * the value.
 */
static void
a2width(const char *p, struct roffsu *su)
{
	const char	*end;

	end = a2roffsu(p, su, SCALE_MAX);
	if (end == NULL || *end != '\0') {
		su->unit = SCALE_EN;
		su->scale = html_strlen(p);
	} else if (su->scale < 0.0)
		su->scale = 0.0;
}
