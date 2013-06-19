/*	$Id: term_ascii.c,v 1.20 2011/12/04 23:10:52 schwarze Exp $ */
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#ifdef USE_WCHAR
# include <locale.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef USE_WCHAR
# include <wchar.h>
#endif

#include "mandoc.h"
#include "out.h"
#include "term.h"
#include "main.h"

/* 
 * Sadly, this doesn't seem to be defined on systems even when they
 * support it.  For the time being, remove it and let those compiling
 * the software decide for themselves what to use.
 */
#if 0
#if ! defined(__STDC_ISO_10646__)
# undef USE_WCHAR
#endif
#endif

static	struct termp	 *ascii_init(enum termenc, char *);
static	double		  ascii_hspan(const struct termp *,
				const struct roffsu *);
static	size_t		  ascii_width(const struct termp *, int);
static	void		  ascii_advance(struct termp *, size_t);
static	void		  ascii_begin(struct termp *);
static	void		  ascii_end(struct termp *);
static	void		  ascii_endline(struct termp *);
static	void		  ascii_letter(struct termp *, int);

#ifdef	USE_WCHAR
static	void		  locale_advance(struct termp *, size_t);
static	void		  locale_endline(struct termp *);
static	void		  locale_letter(struct termp *, int);
static	size_t		  locale_width(const struct termp *, int);
#endif

static struct termp *
ascii_init(enum termenc enc, char *outopts)
{
	const char	*toks[4];
	char		*v;
	struct termp	*p;

	p = mandoc_calloc(1, sizeof(struct termp));
	p->enc = enc;

	p->tabwidth = 5;
	p->defrmargin = 78;

	p->begin = ascii_begin;
	p->end = ascii_end;
	p->hspan = ascii_hspan;
	p->type = TERMTYPE_CHAR;

	p->enc = TERMENC_ASCII;
	p->advance = ascii_advance;
	p->endline = ascii_endline;
	p->letter = ascii_letter;
	p->width = ascii_width;

#ifdef	USE_WCHAR
	if (TERMENC_ASCII != enc) {
		v = TERMENC_LOCALE == enc ?
			setlocale(LC_ALL, "") :
			setlocale(LC_CTYPE, "UTF-8");
		if (NULL != v && MB_CUR_MAX > 1) {
			p->enc = enc;
			p->advance = locale_advance;
			p->endline = locale_endline;
			p->letter = locale_letter;
			p->width = locale_width;
		}
	}
#endif

	toks[0] = "indent";
	toks[1] = "width";
	toks[2] = "mdoc";
	toks[3] = NULL;

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case (0):
			p->defindent = (size_t)atoi(v);
			break;
		case (1):
			p->defrmargin = (size_t)atoi(v);
			break;
		case (2):
			/*
			 * Temporary, undocumented mode
			 * to imitate mdoc(7) output style.
			 */
			p->mdocstyle = 1;
			p->defindent = 5;
			break;
		default:
			break;
		}

	/* Enforce a lower boundary. */
	if (p->defrmargin < 58)
		p->defrmargin = 58;

	return(p);
}

void *
ascii_alloc(char *outopts)
{

	return(ascii_init(TERMENC_ASCII, outopts));
}

void *
utf8_alloc(char *outopts)
{

	return(ascii_init(TERMENC_UTF8, outopts));
}


void *
locale_alloc(char *outopts)
{

	return(ascii_init(TERMENC_LOCALE, outopts));
}

/* ARGSUSED */
static size_t
ascii_width(const struct termp *p, int c)
{

	return(1);
}

void
ascii_free(void *arg)
{

	term_free((struct termp *)arg);
}

/* ARGSUSED */
static void
ascii_letter(struct termp *p, int c)
{
	
	putchar(c);
}

static void
ascii_begin(struct termp *p)
{

	(*p->headf)(p, p->argf);
}

static void
ascii_end(struct termp *p)
{

	(*p->footf)(p, p->argf);
}

/* ARGSUSED */
static void
ascii_endline(struct termp *p)
{

	putchar('\n');
}

/* ARGSUSED */
static void
ascii_advance(struct termp *p, size_t len)
{
	size_t	 	i;

	for (i = 0; i < len; i++)
		putchar(' ');
}

/* ARGSUSED */
static double
ascii_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;

	/*
	 * Approximate based on character width.  These are generated
	 * entirely by eyeballing the screen, but appear to be correct.
	 */

	switch (su->unit) {
	case (SCALE_CM):
		r = 4 * su->scale;
		break;
	case (SCALE_IN):
		r = 10 * su->scale;
		break;
	case (SCALE_PC):
		r = (10 * su->scale) / 6;
		break;
	case (SCALE_PT):
		r = (10 * su->scale) / 72;
		break;
	case (SCALE_MM):
		r = su->scale / 1000;
		break;
	case (SCALE_VS):
		r = su->scale * 2 - 1;
		break;
	default:
		r = su->scale;
		break;
	}

	return(r);
}

#ifdef USE_WCHAR
/* ARGSUSED */
static size_t
locale_width(const struct termp *p, int c)
{
	int		rc;

	return((rc = wcwidth(c)) < 0 ? 0 : rc);
}

/* ARGSUSED */
static void
locale_advance(struct termp *p, size_t len)
{
	size_t	 	i;

	for (i = 0; i < len; i++)
		putwchar(L' ');
}

/* ARGSUSED */
static void
locale_endline(struct termp *p)
{

	putwchar(L'\n');
}

/* ARGSUSED */
static void
locale_letter(struct termp *p, int c)
{
	
	putwchar(c);
}
#endif
