/* $Id: roff_term.c,v 1.27 2025/08/21 15:38:51 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2014, 2015, 2017-2021, 2025
 *               Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "out.h"
#include "term.h"

#define	ROFF_TERM_ARGS struct termp *p, const struct roff_node *n

typedef	void	(*roff_term_pre_fp)(ROFF_TERM_ARGS);

static	void	  roff_term_pre_br(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ce(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ft(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ll(ROFF_TERM_ARGS);
static	void	  roff_term_pre_mc(ROFF_TERM_ARGS);
static	void	  roff_term_pre_po(ROFF_TERM_ARGS);
static	void	  roff_term_pre_sp(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ta(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ti(ROFF_TERM_ARGS);

static	const roff_term_pre_fp roff_term_pre_acts[ROFF_MAX] = {
	roff_term_pre_br,  /* br */
	roff_term_pre_ce,  /* ce */
	roff_term_pre_br,  /* fi */
	roff_term_pre_ft,  /* ft */
	roff_term_pre_ll,  /* ll */
	roff_term_pre_mc,  /* mc */
	roff_term_pre_br,  /* nf */
	roff_term_pre_po,  /* po */
	roff_term_pre_ce,  /* rj */
	roff_term_pre_sp,  /* sp */
	roff_term_pre_ta,  /* ta */
	roff_term_pre_ti,  /* ti */
};


void
roff_term_pre(struct termp *p, const struct roff_node *n)
{
	assert(n->tok < ROFF_MAX);
	(*roff_term_pre_acts[n->tok])(p, n);
}

static void
roff_term_pre_br(ROFF_TERM_ARGS)
{
	term_newln(p);
	if (p->flags & TERMP_BRIND) {
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		p->flags |= TERMP_NOSPACE;
	}
}

static void
roff_term_pre_ce(ROFF_TERM_ARGS)
{
	const struct roff_node	*nc1, *nc2;

	roff_term_pre_br(p, n);
	p->flags |= n->tok == ROFF_ce ? TERMP_CENTER : TERMP_RIGHT;
	nc1 = n->child->next;
	while (nc1 != NULL) {
		nc2 = nc1;
		do {
			nc2 = nc2->next;
		} while (nc2 != NULL && (nc2->type != ROFFT_TEXT ||
		    (nc2->flags & NODE_LINE) == 0));
		while (nc1 != nc2) {
			if (nc1->type == ROFFT_TEXT)
				term_word(p, nc1->string);
			else
				roff_term_pre(p, nc1);
			nc1 = nc1->next;
		}
		p->flags |= TERMP_NOSPACE;
		term_flushln(p);
	}
	p->flags &= ~(TERMP_CENTER | TERMP_RIGHT);
}

static void
roff_term_pre_ft(ROFF_TERM_ARGS)
{
	const char	*cp;

	cp = n->child->string;
	switch (mandoc_font(cp, (int)strlen(cp))) {
	case ESCAPE_FONTBOLD:
	case ESCAPE_FONTCB:
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case ESCAPE_FONTITALIC:
	case ESCAPE_FONTCI:
		term_fontrepl(p, TERMFONT_UNDER);
		break;
	case ESCAPE_FONTBI:
		term_fontrepl(p, TERMFONT_BI);
		break;
	case ESCAPE_FONTPREV:
		term_fontlast(p);
		break;
	case ESCAPE_FONTROMAN:
	case ESCAPE_FONTCR:
		term_fontrepl(p, TERMFONT_NONE);
		break;
	default:
		break;
	}
}

static void
roff_term_pre_ll(ROFF_TERM_ARGS)
{
	term_setwidth(p, n->child != NULL ? n->child->string : NULL);
}

static void
roff_term_pre_mc(ROFF_TERM_ARGS)
{
	if (p->col) {
		p->flags |= TERMP_NOBREAK;
		term_flushln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_NOSPACE);
	}
	if (n->child != NULL) {
		p->mc = n->child->string;
		p->flags |= TERMP_NEWMC;
	} else
		p->flags |= TERMP_ENDMC;
}

static void
roff_term_pre_po(ROFF_TERM_ARGS)
{
	struct roffsu	 su;

	/* Page offsets in basic units. */
	static int	 polast;  /* Previously requested. */
	static int	 po;      /* Currently requested. */
	static int	 pouse;   /* Currently used. */
	int		 pomin;   /* Minimum to be used. */
	int		 pomax;   /* Maximum to be used. */
	int		 ponew;   /* Newly requested. */

	/* Revert the currently active page offset. */
	p->tcol->offset -= pouse;

	/* Determine the requested page offset. */
	if (n->child != NULL &&
	    a2roffsu(n->child->string, &su, SCALE_EM) != NULL) {
		ponew = term_hspan(p, &su);
		if (*n->child->string == '+' ||
		    *n->child->string == '-')
			ponew += po;
	} else
		ponew = polast;

	/* Remember both the previous and the newly requested offset. */
	polast = po;
	po = ponew;

	/* Truncate to the range [-offset, 60], remember, and apply it. */
	pomin = -p->tcol->offset;
	pomax = term_len(p, 60);
	pouse = po > pomax ? pomax : po < pomin ? pomin : po;
	p->tcol->offset += pouse;
}

static void
roff_term_pre_sp(ROFF_TERM_ARGS)
{
	struct roffsu	 su;
	int		 len;

	if (n->child != NULL) {
		if (a2roffsu(n->child->string, &su, SCALE_VS) == NULL)
			su.scale = 1.0;
		len = term_vspan(p, &su);
	} else
		len = 1;

	if (len < 0)
		p->skipvsp -= len;
	else
		while (len--)
			term_vspace(p);

	roff_term_pre_br(p, n);
}

static void
roff_term_pre_ta(ROFF_TERM_ARGS)
{
	term_tab_set(p, NULL);
	for (n = n->child; n != NULL; n = n->next)
		term_tab_set(p, n->string);
}

static void
roff_term_pre_ti(ROFF_TERM_ARGS)
{
	struct roffsu	 su;
	const char	*cp;      /* Request argument. */
	size_t		 maxoff;  /* Maximum indentation in basic units. */
	int		 len;	  /* Request argument in basic units. */
	int		 sign;

	roff_term_pre_br(p, n);

	if (n->child == NULL)
		return;
	cp = n->child->string;
	if (*cp == '+') {
		sign = 1;
		cp++;
	} else if (*cp == '-') {
		sign = -1;
		cp++;
	} else
		sign = 0;

	if (a2roffsu(cp, &su, SCALE_EM) == NULL)
		return;
	len = term_hspan(p, &su);
	maxoff = term_len(p, 72);

	switch (sign) {
	case 1:
		if (p->tcol->offset + len <= maxoff)
			p->ti = len;
		else if (p->tcol->offset < maxoff)
			p->ti = maxoff - p->tcol->offset;
		else
			p->ti = 0;
		break;
	case -1:
		if ((size_t)len < p->tcol->offset)
			p->ti = -len;
		else
			p->ti = -p->tcol->offset;
		break;
	default:
		if ((size_t)len > maxoff)
			len = maxoff;
		p->ti = len - p->tcol->offset;
		break;
	}
	p->tcol->offset += p->ti;
}
