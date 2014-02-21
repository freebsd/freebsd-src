/*	$Id: html.c,v 1.152 2013/08/08 20:07:47 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012, 2013 Ingo Schwarze <schwarze@openbsd.org>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "out.h"
#include "html.h"
#include "main.h"

struct	htmldata {
	const char	 *name;
	int		  flags;
#define	HTML_CLRLINE	 (1 << 0)
#define	HTML_NOSTACK	 (1 << 1)
#define	HTML_AUTOCLOSE	 (1 << 2) /* Tag has auto-closure. */
};

static	const struct htmldata htmltags[TAG_MAX] = {
	{"html",	HTML_CLRLINE}, /* TAG_HTML */
	{"head",	HTML_CLRLINE}, /* TAG_HEAD */
	{"body",	HTML_CLRLINE}, /* TAG_BODY */
	{"meta",	HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_META */
	{"title",	HTML_CLRLINE}, /* TAG_TITLE */
	{"div",		HTML_CLRLINE}, /* TAG_DIV */
	{"h1",		0}, /* TAG_H1 */
	{"h2",		0}, /* TAG_H2 */
	{"span",	0}, /* TAG_SPAN */
	{"link",	HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_LINK */
	{"br",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_BR */
	{"a",		0}, /* TAG_A */
	{"table",	HTML_CLRLINE}, /* TAG_TABLE */
	{"tbody",	HTML_CLRLINE}, /* TAG_TBODY */
	{"col",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_COL */
	{"tr",		HTML_CLRLINE}, /* TAG_TR */
	{"td",		HTML_CLRLINE}, /* TAG_TD */
	{"li",		HTML_CLRLINE}, /* TAG_LI */
	{"ul",		HTML_CLRLINE}, /* TAG_UL */
	{"ol",		HTML_CLRLINE}, /* TAG_OL */
	{"dl",		HTML_CLRLINE}, /* TAG_DL */
	{"dt",		HTML_CLRLINE}, /* TAG_DT */
	{"dd",		HTML_CLRLINE}, /* TAG_DD */
	{"blockquote",	HTML_CLRLINE}, /* TAG_BLOCKQUOTE */
	{"p",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_P */
	{"pre",		HTML_CLRLINE }, /* TAG_PRE */
	{"b",		0 }, /* TAG_B */
	{"i",		0 }, /* TAG_I */
	{"code",	0 }, /* TAG_CODE */
	{"small",	0 }, /* TAG_SMALL */
};

static	const char	*const htmlattrs[ATTR_MAX] = {
	"http-equiv", /* ATTR_HTTPEQUIV */
	"content", /* ATTR_CONTENT */
	"name", /* ATTR_NAME */
	"rel", /* ATTR_REL */
	"href", /* ATTR_HREF */
	"type", /* ATTR_TYPE */
	"media", /* ATTR_MEDIA */
	"class", /* ATTR_CLASS */
	"style", /* ATTR_STYLE */
	"width", /* ATTR_WIDTH */
	"id", /* ATTR_ID */
	"summary", /* ATTR_SUMMARY */
	"align", /* ATTR_ALIGN */
	"colspan", /* ATTR_COLSPAN */
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

static	void	 bufncat(struct html *, const char *, size_t);
static	void	 print_ctag(struct html *, enum htmltag);
static	int	 print_encode(struct html *, const char *, int);
static	void	 print_metaf(struct html *, enum mandoc_esc);
static	void	 print_attr(struct html *, const char *, const char *);
static	void	 *ml_alloc(char *, enum htmltype);

static void *
ml_alloc(char *outopts, enum htmltype type)
{
	struct html	*h;
	const char	*toks[5];
	char		*v;

	toks[0] = "style";
	toks[1] = "man";
	toks[2] = "includes";
	toks[3] = "fragment";
	toks[4] = NULL;

	h = mandoc_calloc(1, sizeof(struct html));

	h->type = type;
	h->tags.head = NULL;
	h->symtab = mchars_alloc();

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case (0):
			h->style = v;
			break;
		case (1):
			h->base_man = v;
			break;
		case (2):
			h->base_includes = v;
			break;
		case (3):
			h->oflags |= HTML_FRAGMENT;
			break;
		default:
			break;
		}

	return(h);
}

void *
html_alloc(char *outopts)
{

	return(ml_alloc(outopts, HTML_HTML_4_01_STRICT));
}


void *
xhtml_alloc(char *outopts)
{

	return(ml_alloc(outopts, HTML_XHTML_1_0_STRICT));
}


void
html_free(void *p)
{
	struct tag	*tag;
	struct html	*h;

	h = (struct html *)p;

	while ((tag = h->tags.head) != NULL) {
		h->tags.head = tag->next;	
		free(tag);
	}
	
	if (h->symtab)
		mchars_free(h->symtab);

	free(h);
}


void
print_gen_head(struct html *h)
{
	struct htmlpair	 tag[4];

	tag[0].key = ATTR_HTTPEQUIV;
	tag[0].val = "Content-Type";
	tag[1].key = ATTR_CONTENT;
	tag[1].val = "text/html; charset=utf-8";
	print_otag(h, TAG_META, 2, tag);

	tag[0].key = ATTR_NAME;
	tag[0].val = "resource-type";
	tag[1].key = ATTR_CONTENT;
	tag[1].val = "document";
	print_otag(h, TAG_META, 2, tag);

	if (h->style) {
		tag[0].key = ATTR_REL;
		tag[0].val = "stylesheet";
		tag[1].key = ATTR_HREF;
		tag[1].val = h->style;
		tag[2].key = ATTR_TYPE;
		tag[2].val = "text/css";
		tag[3].key = ATTR_MEDIA;
		tag[3].val = "all";
		print_otag(h, TAG_LINK, 4, tag);
	}
}

static void
print_metaf(struct html *h, enum mandoc_esc deco)
{
	enum htmlfont	 font;

	switch (deco) {
	case (ESCAPE_FONTPREV):
		font = h->metal;
		break;
	case (ESCAPE_FONTITALIC):
		font = HTMLFONT_ITALIC;
		break;
	case (ESCAPE_FONTBOLD):
		font = HTMLFONT_BOLD;
		break;
	case (ESCAPE_FONTBI):
		font = HTMLFONT_BI;
		break;
	case (ESCAPE_FONT):
		/* FALLTHROUGH */
	case (ESCAPE_FONTROMAN):
		font = HTMLFONT_NONE;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->metal = h->metac;
	h->metac = font;

	switch (font) {
	case (HTMLFONT_ITALIC):
		h->metaf = print_otag(h, TAG_I, 0, NULL);
		break;
	case (HTMLFONT_BOLD):
		h->metaf = print_otag(h, TAG_B, 0, NULL);
		break;
	case (HTMLFONT_BI):
		h->metaf = print_otag(h, TAG_B, 0, NULL);
		print_otag(h, TAG_I, 0, NULL);
		break;
	default:
		break;
	}
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
		case (ESCAPE_ERROR):
			return(sz);
		case (ESCAPE_UNICODE):
			/* FALLTHROUGH */
		case (ESCAPE_NUMBERED):
			/* FALLTHROUGH */
		case (ESCAPE_SPECIAL):
			if (skip)
				skip = 0;
			else
				sz++;
			break;
		case (ESCAPE_SKIPCHAR):
			skip = 1;
			break;
		default:
			break;
		}
	}
	return(sz);
}

static int
print_encode(struct html *h, const char *p, int norecurse)
{
	size_t		 sz;
	int		 c, len, nospace;
	const char	*seq;
	enum mandoc_esc	 esc;
	static const char rejs[6] = { '\\', '<', '>', '&', ASCII_HYPH, '\0' };

	nospace = 0;

	while ('\0' != *p) {
		if (HTML_SKIPCHAR & h->flags && '\\' != *p) {
			h->flags &= ~HTML_SKIPCHAR;
			p++;
			continue;
		}

		sz = strcspn(p, rejs);

		fwrite(p, 1, sz, stdout);
		p += (int)sz;

		if ('\0' == *p)
			break;

		switch (*p++) {
		case ('<'):
			printf("&lt;");
			continue;
		case ('>'):
			printf("&gt;");
			continue;
		case ('&'):
			printf("&amp;");
			continue;
		case (ASCII_HYPH):
			putchar('-');
			continue;
		default:
			break;
		}

		esc = mandoc_escape(&p, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		switch (esc) {
		case (ESCAPE_FONT):
			/* FALLTHROUGH */
		case (ESCAPE_FONTPREV):
			/* FALLTHROUGH */
		case (ESCAPE_FONTBOLD):
			/* FALLTHROUGH */
		case (ESCAPE_FONTITALIC):
			/* FALLTHROUGH */
		case (ESCAPE_FONTBI):
			/* FALLTHROUGH */
		case (ESCAPE_FONTROMAN):
			if (0 == norecurse)
				print_metaf(h, esc);
			continue;
		case (ESCAPE_SKIPCHAR):
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
		case (ESCAPE_UNICODE):
			/* Skip passed "u" header. */
			c = mchars_num2uc(seq + 1, len - 1);
			if ('\0' != c)
				printf("&#x%x;", c);
			break;
		case (ESCAPE_NUMBERED):
			c = mchars_num2char(seq, len);
			if ('\0' != c)
				putchar(c);
			break;
		case (ESCAPE_SPECIAL):
			c = mchars_spec2cp(h->symtab, seq, len);
			if (c > 0)
				printf("&#%d;", c);
			else if (-1 == c && 1 == len)
				putchar((int)*seq);
			break;
		case (ESCAPE_NOSPACE):
			if ('\0' == *p)
				nospace = 1;
			break;
		default:
			break;
		}
	}

	return(nospace);
}


static void
print_attr(struct html *h, const char *key, const char *val)
{
	printf(" %s=\"", key);
	(void)print_encode(h, val, 1);
	putchar('\"');
}


struct tag *
print_otag(struct html *h, enum htmltag tag, 
		int sz, const struct htmlpair *p)
{
	int		 i;
	struct tag	*t;

	/* Push this tags onto the stack of open scopes. */

	if ( ! (HTML_NOSTACK & htmltags[tag].flags)) {
		t = mandoc_malloc(sizeof(struct tag));
		t->tag = tag;
		t->next = h->tags.head;
		h->tags.head = t;
	} else
		t = NULL;

	if ( ! (HTML_NOSPACE & h->flags))
		if ( ! (HTML_CLRLINE & htmltags[tag].flags)) {
			/* Manage keeps! */
			if ( ! (HTML_KEEP & h->flags)) {
				if (HTML_PREKEEP & h->flags)
					h->flags |= HTML_KEEP;
				putchar(' ');
			} else
				printf("&#160;");
		}

	if ( ! (h->flags & HTML_NONOSPACE))
		h->flags &= ~HTML_NOSPACE;
	else
		h->flags |= HTML_NOSPACE;

	/* Print out the tag name and attributes. */

	printf("<%s", htmltags[tag].name);
	for (i = 0; i < sz; i++)
		print_attr(h, htmlattrs[p[i].key], p[i].val);

	/* Add non-overridable attributes. */

	if (TAG_HTML == tag && HTML_XHTML_1_0_STRICT == h->type) {
		print_attr(h, "xmlns", "http://www.w3.org/1999/xhtml");
		print_attr(h, "xml:lang", "en");
		print_attr(h, "lang", "en");
	}

	/* Accommodate for XML "well-formed" singleton escaping. */

	if (HTML_AUTOCLOSE & htmltags[tag].flags)
		switch (h->type) {
		case (HTML_XHTML_1_0_STRICT):
			putchar('/');
			break;
		default:
			break;
		}

	putchar('>');

	h->flags |= HTML_NOSPACE;

	if ((HTML_AUTOCLOSE | HTML_CLRLINE) & htmltags[tag].flags)
		putchar('\n');

	return(t);
}


static void
print_ctag(struct html *h, enum htmltag tag)
{
	
	printf("</%s>", htmltags[tag].name);
	if (HTML_CLRLINE & htmltags[tag].flags) {
		h->flags |= HTML_NOSPACE;
		putchar('\n');
	} 
}

void
print_gen_decls(struct html *h)
{
	const char	*doctype;
	const char	*dtd;
	const char	*name;

	switch (h->type) {
	case (HTML_HTML_4_01_STRICT):
		name = "HTML";
		doctype = "-//W3C//DTD HTML 4.01//EN";
		dtd = "http://www.w3.org/TR/html4/strict.dtd";
		break;
	default:
		puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		name = "html";
		doctype = "-//W3C//DTD XHTML 1.0 Strict//EN";
		dtd = "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd";
		break;
	}

	printf("<!DOCTYPE %s PUBLIC \"%s\" \"%s\">\n", 
			name, doctype, dtd);
}

void
print_text(struct html *h, const char *word)
{

	if ( ! (HTML_NOSPACE & h->flags)) {
		/* Manage keeps! */
		if ( ! (HTML_KEEP & h->flags)) {
			if (HTML_PREKEEP & h->flags)
				h->flags |= HTML_KEEP;
			putchar(' ');
		} else
			printf("&#160;");
	}

	assert(NULL == h->metaf);
	switch (h->metac) {
	case (HTMLFONT_ITALIC):
		h->metaf = print_otag(h, TAG_I, 0, NULL);
		break;
	case (HTMLFONT_BOLD):
		h->metaf = print_otag(h, TAG_B, 0, NULL);
		break;
	case (HTMLFONT_BI):
		h->metaf = print_otag(h, TAG_B, 0, NULL);
		print_otag(h, TAG_I, 0, NULL);
		break;
	default:
		break;
	}

	assert(word);
	if ( ! print_encode(h, word, 0)) {
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;
	} else
		h->flags |= HTML_NOSPACE;

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

	while ((tag = h->tags.head) != NULL) {
		/* 
		 * Remember to close out and nullify the current
		 * meta-font and table, if applicable.
		 */
		if (tag == h->metaf)
			h->metaf = NULL;
		if (tag == h->tblt)
			h->tblt = NULL;
		print_ctag(h, tag->tag);
		h->tags.head = tag->next;
		free(tag);
		if (until && tag == until)
			return;
	}
}


void
print_stagq(struct html *h, const struct tag *suntil)
{
	struct tag	*tag;

	while ((tag = h->tags.head) != NULL) {
		if (suntil && tag == suntil)
			return;
		/* 
		 * Remember to close out and nullify the current
		 * meta-font and table, if applicable.
		 */
		if (tag == h->metaf)
			h->metaf = NULL;
		if (tag == h->tblt)
			h->tblt = NULL;
		print_ctag(h, tag->tag);
		h->tags.head = tag->next;
		free(tag);
	}
}

void
bufinit(struct html *h)
{

	h->buf[0] = '\0';
	h->buflen = 0;
}

void
bufcat_style(struct html *h, const char *key, const char *val)
{

	bufcat(h, key);
	bufcat(h, ":");
	bufcat(h, val);
	bufcat(h, ";");
}

void
bufcat(struct html *h, const char *p)
{

	h->buflen = strlcat(h->buf, p, BUFSIZ);
	assert(h->buflen < BUFSIZ);
}

void
bufcat_fmt(struct html *h, const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	(void)vsnprintf(h->buf + (int)h->buflen, 
			BUFSIZ - h->buflen - 1, fmt, ap);
	va_end(ap);
	h->buflen = strlen(h->buf);
}

static void
bufncat(struct html *h, const char *p, size_t sz)
{

	assert(h->buflen + sz + 1 < BUFSIZ);
	strncat(h->buf, p, sz);
	h->buflen += sz;
}

void
buffmt_includes(struct html *h, const char *name)
{
	const char	*p, *pp;

	pp = h->base_includes;
	
	bufinit(h);
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case('I'):
			bufcat(h, name);
			break;
		default:
			bufncat(h, p, 2);
			break;
		}
		pp = p + 2;
	}
	if (pp)
		bufcat(h, pp);
}

void
buffmt_man(struct html *h, 
		const char *name, const char *sec)
{
	const char	*p, *pp;

	pp = h->base_man;
	
	bufinit(h);
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case('S'):
			bufcat(h, sec ? sec : "1");
			break;
		case('N'):
			bufcat_fmt(h, name);
			break;
		default:
			bufncat(h, p, 2);
			break;
		}
		pp = p + 2;
	}
	if (pp)
		bufcat(h, pp);
}

void
bufcat_su(struct html *h, const char *p, const struct roffsu *su)
{
	double		 v;

	v = su->scale;
	if (SCALE_MM == su->unit && 0.0 == (v /= 100.0))
		v = 1.0;

	bufcat_fmt(h, "%s: %.2f%s;", p, v, roffscales[su->unit]);
}

void
bufcat_id(struct html *h, const char *src)
{

	/* Cf. <http://www.w3.org/TR/html4/types.html#h-6.2>. */

	while ('\0' != *src)
		bufcat_fmt(h, "%.2x", *src++);
}
