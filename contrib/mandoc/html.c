/* $Id: html.c,v 1.275 2021/09/09 14:47:24 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2015, 2017-2021 Ingo Schwarze <schwarze@openbsd.org>
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
 * Common functions for mandoc(1) HTML formatters.
 * For use by individual formatters and by the main program.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc_ohash.h"
#include "mandoc.h"
#include "roff.h"
#include "out.h"
#include "html.h"
#include "manconf.h"
#include "main.h"

struct	htmldata {
	const char	 *name;
	int		  flags;
#define	HTML_INPHRASE	 (1 << 0)  /* Can appear in phrasing context. */
#define	HTML_TOPHRASE	 (1 << 1)  /* Establishes phrasing context. */
#define	HTML_NOSTACK	 (1 << 2)  /* Does not have an end tag. */
#define	HTML_NLBEFORE	 (1 << 3)  /* Output line break before opening. */
#define	HTML_NLBEGIN	 (1 << 4)  /* Output line break after opening. */
#define	HTML_NLEND	 (1 << 5)  /* Output line break before closing. */
#define	HTML_NLAFTER	 (1 << 6)  /* Output line break after closing. */
#define	HTML_NLAROUND	 (HTML_NLBEFORE | HTML_NLAFTER)
#define	HTML_NLINSIDE	 (HTML_NLBEGIN | HTML_NLEND)
#define	HTML_NLALL	 (HTML_NLAROUND | HTML_NLINSIDE)
#define	HTML_INDENT	 (1 << 7)  /* Indent content by two spaces. */
#define	HTML_NOINDENT	 (1 << 8)  /* Exception: never indent content. */
};

static	const struct htmldata htmltags[TAG_MAX] = {
	{"html",	HTML_NLALL},
	{"head",	HTML_NLALL | HTML_INDENT},
	{"meta",	HTML_NOSTACK | HTML_NLALL},
	{"link",	HTML_NOSTACK | HTML_NLALL},
	{"style",	HTML_NLALL | HTML_INDENT},
	{"title",	HTML_NLAROUND},
	{"body",	HTML_NLALL},
	{"div",		HTML_NLAROUND},
	{"section",	HTML_NLALL},
	{"table",	HTML_NLALL | HTML_INDENT},
	{"tr",		HTML_NLALL | HTML_INDENT},
	{"td",		HTML_NLAROUND},
	{"li",		HTML_NLAROUND | HTML_INDENT},
	{"ul",		HTML_NLALL | HTML_INDENT},
	{"ol",		HTML_NLALL | HTML_INDENT},
	{"dl",		HTML_NLALL | HTML_INDENT},
	{"dt",		HTML_NLAROUND},
	{"dd",		HTML_NLAROUND | HTML_INDENT},
	{"h1",		HTML_TOPHRASE | HTML_NLAROUND},
	{"h2",		HTML_TOPHRASE | HTML_NLAROUND},
	{"p",		HTML_TOPHRASE | HTML_NLAROUND | HTML_INDENT},
	{"pre",		HTML_TOPHRASE | HTML_NLAROUND | HTML_NOINDENT},
	{"a",		HTML_INPHRASE | HTML_TOPHRASE},
	{"b",		HTML_INPHRASE | HTML_TOPHRASE},
	{"cite",	HTML_INPHRASE | HTML_TOPHRASE},
	{"code",	HTML_INPHRASE | HTML_TOPHRASE},
	{"i",		HTML_INPHRASE | HTML_TOPHRASE},
	{"small",	HTML_INPHRASE | HTML_TOPHRASE},
	{"span",	HTML_INPHRASE | HTML_TOPHRASE},
	{"var",		HTML_INPHRASE | HTML_TOPHRASE},
	{"br",		HTML_INPHRASE | HTML_NOSTACK | HTML_NLALL},
	{"hr",		HTML_INPHRASE | HTML_NOSTACK},
	{"mark",	HTML_INPHRASE },
	{"math",	HTML_INPHRASE | HTML_NLALL | HTML_INDENT},
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

/* Avoid duplicate HTML id= attributes. */

struct	id_entry {
	int	 ord;	/* Ordinal number of the latest occurrence. */
	char	 id[];	/* The id= attribute without any ordinal suffix. */
};
static	struct ohash	 id_unique;

static	void	 html_reset_internal(struct html *);
static	void	 print_byte(struct html *, char);
static	void	 print_endword(struct html *);
static	void	 print_indent(struct html *);
static	void	 print_word(struct html *, const char *);

static	void	 print_ctag(struct html *, struct tag *);
static	int	 print_escape(struct html *, char);
static	int	 print_encode(struct html *, const char *, const char *, int);
static	void	 print_href(struct html *, const char *, const char *, int);
static	void	 print_metaf(struct html *);


void *
html_alloc(const struct manoutput *outopts)
{
	struct html	*h;

	h = mandoc_calloc(1, sizeof(struct html));

	h->tag = NULL;
	h->metac = h->metal = ESCAPE_FONTROMAN;
	h->style = outopts->style;
	if ((h->base_man1 = outopts->man) == NULL)
		h->base_man2 = NULL;
	else if ((h->base_man2 = strchr(h->base_man1, ';')) != NULL)
		*h->base_man2++ = '\0';
	h->base_includes = outopts->includes;
	if (outopts->fragment)
		h->oflags |= HTML_FRAGMENT;
	if (outopts->toc)
		h->oflags |= HTML_TOC;

	mandoc_ohash_init(&id_unique, 4, offsetof(struct id_entry, id));

	return h;
}

static void
html_reset_internal(struct html *h)
{
	struct tag	*tag;
	struct id_entry	*entry;
	unsigned int	 slot;

	while ((tag = h->tag) != NULL) {
		h->tag = tag->next;
		free(tag);
	}
	entry = ohash_first(&id_unique, &slot);
	while (entry != NULL) {
		free(entry);
		entry = ohash_next(&id_unique, &slot);
	}
	ohash_delete(&id_unique);
}

void
html_reset(void *p)
{
	html_reset_internal(p);
	mandoc_ohash_init(&id_unique, 4, offsetof(struct id_entry, id));
}

void
html_free(void *p)
{
	html_reset_internal(p);
	free(p);
}

void
print_gen_head(struct html *h)
{
	struct tag	*t;

	print_otag(h, TAG_META, "?", "charset", "utf-8");
	print_otag(h, TAG_META, "??", "name", "viewport",
	    "content", "width=device-width, initial-scale=1.0");
	if (h->style != NULL) {
		print_otag(h, TAG_LINK, "?h??", "rel", "stylesheet",
		    h->style, "type", "text/css", "media", "all");
		return;
	}

	/*
	 * Print a minimal embedded style sheet.
	 */

	t = print_otag(h, TAG_STYLE, "");
	print_text(h, "table.head, table.foot { width: 100%; }");
	print_endline(h);
	print_text(h, "td.head-rtitle, td.foot-os { text-align: right; }");
	print_endline(h);
	print_text(h, "td.head-vol { text-align: center; }");
	print_endline(h);
	print_text(h, ".Nd, .Bf, .Op { display: inline; }");
	print_endline(h);
	print_text(h, ".Pa, .Ad { font-style: italic; }");
	print_endline(h);
	print_text(h, ".Ms { font-weight: bold; }");
	print_endline(h);
	print_text(h, ".Bl-diag ");
	print_byte(h, '>');
	print_text(h, " dt { font-weight: bold; }");
	print_endline(h);
	print_text(h, "code.Nm, .Fl, .Cm, .Ic, code.In, .Fd, .Fn, .Cd "
	    "{ font-weight: bold; font-family: inherit; }");
	print_tagq(h, t);
}

int
html_setfont(struct html *h, enum mandoc_esc font)
{
	switch (font) {
	case ESCAPE_FONTPREV:
		font = h->metal;
		break;
	case ESCAPE_FONTITALIC:
	case ESCAPE_FONTBOLD:
	case ESCAPE_FONTBI:
	case ESCAPE_FONTROMAN:
	case ESCAPE_FONTCR:
	case ESCAPE_FONTCB:
	case ESCAPE_FONTCI:
		break;
	case ESCAPE_FONT:
		font = ESCAPE_FONTROMAN;
		break;
	default:
		return 0;
	}
	h->metal = h->metac;
	h->metac = font;
	return 1;
}

static void
print_metaf(struct html *h)
{
	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}
	switch (h->metac) {
	case ESCAPE_FONTITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case ESCAPE_FONTBOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case ESCAPE_FONTBI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	case ESCAPE_FONTCR:
		h->metaf = print_otag(h, TAG_SPAN, "c", "Li");
		break;
	case ESCAPE_FONTCB:
		h->metaf = print_otag(h, TAG_SPAN, "c", "Li");
		print_otag(h, TAG_B, "");
		break;
	case ESCAPE_FONTCI:
		h->metaf = print_otag(h, TAG_SPAN, "c", "Li");
		print_otag(h, TAG_I, "");
		break;
	default:
		break;
	}
}

void
html_close_paragraph(struct html *h)
{
	struct tag	*this, *next;
	int		 flags;

	this = h->tag;
	for (;;) {
		next = this->next;
		flags = htmltags[this->tag].flags;
		if (flags & (HTML_INPHRASE | HTML_TOPHRASE))
			print_ctag(h, this);
		if ((flags & HTML_INPHRASE) == 0)
			break;
		this = next;
	}
}

/*
 * ROFF_nf switches to no-fill mode, ROFF_fi to fill mode.
 * TOKEN_NONE does not switch.  The old mode is returned.
 */
enum roff_tok
html_fillmode(struct html *h, enum roff_tok want)
{
	struct tag	*t;
	enum roff_tok	 had;

	for (t = h->tag; t != NULL; t = t->next)
		if (t->tag == TAG_PRE)
			break;

	had = t == NULL ? ROFF_fi : ROFF_nf;

	if (want != had) {
		switch (want) {
		case ROFF_fi:
			print_tagq(h, t);
			break;
		case ROFF_nf:
			html_close_paragraph(h);
			print_otag(h, TAG_PRE, "");
			break;
		case TOKEN_NONE:
			break;
		default:
			abort();
		}
	}
	return had;
}

/*
 * Allocate a string to be used for the "id=" attribute of an HTML
 * element and/or as a segment identifier for a URI in an <a> element.
 * The function may fail and return NULL if the node lacks text data
 * to create the attribute from.
 * The caller is responsible for free(3)ing the returned string.
 *
 * If the "unique" argument is non-zero, the "id_unique" ohash table
 * is used for de-duplication.  If the "unique" argument is 1,
 * it is the first time the function is called for this tag and
 * location, so if an ordinal suffix is needed, it is incremented.
 * If the "unique" argument is 2, it is the second time the function
 * is called for this tag and location, so the ordinal suffix
 * remains unchanged.
 */
char *
html_make_id(const struct roff_node *n, int unique)
{
	const struct roff_node	*nch;
	struct id_entry		*entry;
	char			*buf, *cp;
	size_t			 len;
	unsigned int		 slot;

	if (n->tag != NULL)
		buf = mandoc_strdup(n->tag);
	else {
		switch (n->tok) {
		case MDOC_Sh:
		case MDOC_Ss:
		case MDOC_Sx:
		case MAN_SH:
		case MAN_SS:
			for (nch = n->child; nch != NULL; nch = nch->next)
				if (nch->type != ROFFT_TEXT)
					return NULL;
			buf = NULL;
			deroff(&buf, n);
			if (buf == NULL)
				return NULL;
			break;
		default:
			if (n->child == NULL || n->child->type != ROFFT_TEXT)
				return NULL;
			buf = mandoc_strdup(n->child->string);
			break;
		}
	}

	/*
	 * In ID attributes, only use ASCII characters that are
	 * permitted in URL-fragment strings according to the
	 * explicit list at:
	 * https://url.spec.whatwg.org/#url-fragment-string
	 * In addition, reserve '~' for ordinal suffixes.
	 */

	for (cp = buf; *cp != '\0'; cp++)
		if (isalnum((unsigned char)*cp) == 0 &&
		    strchr("!$&'()*+,-./:;=?@_", *cp) == NULL)
			*cp = '_';

	if (unique == 0)
		return buf;

	/* Avoid duplicate HTML id= attributes. */

	slot = ohash_qlookup(&id_unique, buf);
	if ((entry = ohash_find(&id_unique, slot)) == NULL) {
		len = strlen(buf) + 1;
		entry = mandoc_malloc(sizeof(*entry) + len);
		entry->ord = 1;
		memcpy(entry->id, buf, len);
		ohash_insert(&id_unique, slot, entry);
	} else if (unique == 1)
		entry->ord++;

	if (entry->ord > 1) {
		cp = buf;
		mandoc_asprintf(&buf, "%s~%d", cp, entry->ord);
		free(cp);
	}
	return buf;
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
			print_otag(h, TAG_BR, "");
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
		switch (esc) {
		case ESCAPE_FONT:
		case ESCAPE_FONTPREV:
		case ESCAPE_FONTBOLD:
		case ESCAPE_FONTITALIC:
		case ESCAPE_FONTBI:
		case ESCAPE_FONTROMAN:
		case ESCAPE_FONTCR:
		case ESCAPE_FONTCB:
		case ESCAPE_FONTCI:
			if (0 == norecurse) {
				h->flags |= HTML_NOSPACE;
				if (html_setfont(h, esc))
					print_metaf(h);
				h->flags &= ~HTML_NOSPACE;
			}
			continue;
		case ESCAPE_SKIPCHAR:
			h->flags |= HTML_SKIPCHAR;
			continue;
		case ESCAPE_ERROR:
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
		case ESCAPE_UNDEF:
			c = *seq;
			break;
		case ESCAPE_DEVICE:
			print_word(h, "html");
			continue;
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
	struct stat	 sb;
	const char	*p, *pp;
	char		*filename;

	if (man) {
		pp = h->base_man1;
		if (h->base_man2 != NULL) {
			mandoc_asprintf(&filename, "%s.%s", name, sec);
			if (stat(filename, &sb) == -1)
				pp = h->base_man2;
			free(filename);
		}
	} else
		pp = h->base_includes;

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
	struct tag	*t;
	const char	*attr;
	char		*arg1, *arg2;
	int		 style_written, tflags;

	tflags = htmltags[tag].flags;

	/* Flow content is not allowed in phrasing context. */

	if ((tflags & HTML_INPHRASE) == 0) {
		for (t = h->tag; t != NULL; t = t->next) {
			if (t->closed)
				continue;
			assert((htmltags[t->tag].flags & HTML_TOPHRASE) == 0);
			break;
		}

	/*
	 * Always wrap phrasing elements in a paragraph
	 * unless already contained in some flow container;
	 * never put them directly into a section.
	 */

	} else if (tflags & HTML_TOPHRASE && h->tag->tag == TAG_SECTION)
		print_otag(h, TAG_P, "c", "Pp");

	/* Push this tag onto the stack of open scopes. */

	if ((tflags & HTML_NOSTACK) == 0) {
		t = mandoc_malloc(sizeof(struct tag));
		t->tag = tag;
		t->next = h->tag;
		t->refcnt = 0;
		t->closed = 0;
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

	while (*fmt != '\0' && *fmt != 's') {

		/* Parse attributes and arguments. */

		arg1 = va_arg(ap, char *);
		arg2 = NULL;
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
		if (*fmt == 'M')
			arg2 = va_arg(ap, char *);
		if (arg1 == NULL)
			continue;

		/* Print the attributes. */

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
		default:
			print_encode(h, arg1, NULL, 1);
			break;
		}
		print_byte(h, '"');
	}

	style_written = 0;
	while (*fmt++ == 's') {
		arg1 = va_arg(ap, char *);
		arg2 = va_arg(ap, char *);
		if (arg2 == NULL)
			continue;
		print_byte(h, ' ');
		if (style_written == 0) {
			print_word(h, "style=\"");
			style_written = 1;
		}
		print_word(h, arg1);
		print_byte(h, ':');
		print_byte(h, ' ');
		print_word(h, arg2);
		print_byte(h, ';');
	}
	if (style_written)
		print_byte(h, '"');

	va_end(ap);

	/* Accommodate for "well-formed" singleton escaping. */

	if (htmltags[tag].flags & HTML_NOSTACK)
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

/*
 * Print an element with an optional "id=" attribute.
 * If the element has phrasing content and an "id=" attribute,
 * also add a permalink: outside if it can be in phrasing context,
 * inside otherwise.
 */
struct tag *
print_otag_id(struct html *h, enum htmltag elemtype, const char *cattr,
    struct roff_node *n)
{
	struct roff_node *nch;
	struct tag	*ret, *t;
	char		*id, *href;

	ret = NULL;
	id = href = NULL;
	if (n->flags & NODE_ID)
		id = html_make_id(n, 1);
	if (n->flags & NODE_HREF)
		href = id == NULL ? html_make_id(n, 2) : id;
	if (href != NULL && htmltags[elemtype].flags & HTML_INPHRASE)
		ret = print_otag(h, TAG_A, "chR", "permalink", href);
	t = print_otag(h, elemtype, "ci", cattr, id);
	if (ret == NULL) {
		ret = t;
		if (href != NULL && (nch = n->child) != NULL) {
			/* man(7) is safe, it tags phrasing content only. */
			if (n->tok > MDOC_MAX ||
			    htmltags[elemtype].flags & HTML_TOPHRASE)
				nch = NULL;
			else  /* For mdoc(7), beware of nested blocks. */
				while (nch != NULL && nch->type == ROFFT_TEXT)
					nch = nch->next;
			if (nch == NULL)
				print_otag(h, TAG_A, "chR", "permalink", href);
		}
	}
	free(id);
	if (id == NULL)
		free(href);
	return ret;
}

static void
print_ctag(struct html *h, struct tag *tag)
{
	int	 tflags;

	if (tag->closed == 0) {
		tag->closed = 1;
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
	}
	if (tag->refcnt == 0) {
		h->tag = tag->next;
		free(tag);
	}
}

void
print_gen_decls(struct html *h)
{
	print_word(h, "<!DOCTYPE html>");
	print_endline(h);
}

void
print_gen_comment(struct html *h, struct roff_node *n)
{
	int	 wantblank;

	print_word(h, "<!-- This is an automatically generated file."
	    "  Do not edit.");
	h->indent = 1;
	wantblank = 0;
	while (n != NULL && n->type == ROFFT_COMMENT) {
		if (strstr(n->string, "-->") == NULL &&
		    (wantblank || *n->string != '\0')) {
			print_endline(h);
			print_indent(h);
			print_word(h, n->string);
			wantblank = *n->string != '\0';
		}
		n = n->next;
	}
	if (wantblank)
		print_endline(h);
	print_word(h, " -->");
	print_endline(h);
	h->indent = 0;
}

void
print_text(struct html *h, const char *word)
{
	print_tagged_text(h, word, NULL);
}

void
print_tagged_text(struct html *h, const char *word, struct roff_node *n)
{
	struct tag	*t;
	char		*href;

	/*
	 * Always wrap text in a paragraph unless already contained in
	 * some flow container; never put it directly into a section.
	 */

	if (h->tag->tag == TAG_SECTION)
		print_otag(h, TAG_P, "c", "Pp");

	/* Output whitespace before this text? */

	if (h->col && (h->flags & HTML_NOSPACE) == 0) {
		if ( ! (HTML_KEEP & h->flags)) {
			if (HTML_PREKEEP & h->flags)
				h->flags |= HTML_KEEP;
			print_endword(h);
		} else
			print_word(h, "&#x00A0;");
	}

	/*
	 * Optionally switch fonts, optionally write a permalink, then
	 * print the text, optionally surrounded by HTML whitespace.
	 */

	assert(h->metaf == NULL);
	print_metaf(h);
	print_indent(h);

	if (n != NULL && (href = html_make_id(n, 2)) != NULL) {
		t = print_otag(h, TAG_A, "chR", "permalink", href);
		free(href);
	} else
		t = NULL;

	if ( ! print_encode(h, word, NULL, 0)) {
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONEWLINE;
	} else
		h->flags |= HTML_NOSPACE | HTML_NONEWLINE;

	if (h->metaf != NULL) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	} else if (t != NULL)
		print_tagq(h, t);

	h->flags &= ~HTML_IGNDELIM;
}

void
print_tagq(struct html *h, const struct tag *until)
{
	struct tag	*this, *next;

	for (this = h->tag; this != NULL; this = next) {
		next = this == until ? NULL : this->next;
		print_ctag(h, this);
	}
}

/*
 * Close out all open elements up to but excluding suntil.
 * Note that a paragraph just inside stays open together with it
 * because paragraphs include subsequent phrasing content.
 */
void
print_stagq(struct html *h, const struct tag *suntil)
{
	struct tag	*this, *next;

	for (this = h->tag; this != NULL; this = next) {
		next = this->next;
		if (this == suntil || (next == suntil &&
		    (this->tag == TAG_P || this->tag == TAG_PRE)))
			break;
		print_ctag(h, this);
	}
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

	if (h->col || h->noindent)
		return;

	h->col = h->indent * 2;
	for (i = 0; i < h->col; i++)
		putchar(' ');
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
