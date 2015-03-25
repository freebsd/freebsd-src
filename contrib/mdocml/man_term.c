/*	$Id: man_term.c,v 1.169 2015/03/06 15:48:52 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "man.h"
#include "term.h"
#include "main.h"

#define	MAXMARGINS	  64 /* maximum number of indented scopes */

struct	mtermp {
	int		  fl;
#define	MANT_LITERAL	 (1 << 0)
	int		  lmargin[MAXMARGINS]; /* margins (incl. vis. page) */
	int		  lmargincur; /* index of current margin */
	int		  lmarginsz; /* actual number of nested margins */
	size_t		  offset; /* default offset to visible page */
	int		  pardist; /* vert. space before par., unit: [v] */
};

#define	DECL_ARGS	  struct termp *p, \
			  struct mtermp *mt, \
			  struct man_node *n, \
			  const struct man_meta *meta

struct	termact {
	int		(*pre)(DECL_ARGS);
	void		(*post)(DECL_ARGS);
	int		  flags;
#define	MAN_NOTEXT	 (1 << 0) /* Never has text children. */
};

static	void		  print_man_nodelist(DECL_ARGS);
static	void		  print_man_node(DECL_ARGS);
static	void		  print_man_head(struct termp *, const void *);
static	void		  print_man_foot(struct termp *, const void *);
static	void		  print_bvspace(struct termp *,
				const struct man_node *, int);

static	int		  pre_B(DECL_ARGS);
static	int		  pre_HP(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_OP(DECL_ARGS);
static	int		  pre_PD(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RS(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);
static	int		  pre_UR(DECL_ARGS);
static	int		  pre_alternate(DECL_ARGS);
static	int		  pre_ft(DECL_ARGS);
static	int		  pre_ign(DECL_ARGS);
static	int		  pre_in(DECL_ARGS);
static	int		  pre_literal(DECL_ARGS);
static	int		  pre_ll(DECL_ARGS);
static	int		  pre_sp(DECL_ARGS);

static	void		  post_IP(DECL_ARGS);
static	void		  post_HP(DECL_ARGS);
static	void		  post_RS(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SS(DECL_ARGS);
static	void		  post_TP(DECL_ARGS);
static	void		  post_UR(DECL_ARGS);

static	const struct termact termacts[MAN_MAX] = {
	{ pre_sp, NULL, MAN_NOTEXT }, /* br */
	{ NULL, NULL, 0 }, /* TH */
	{ pre_SH, post_SH, 0 }, /* SH */
	{ pre_SS, post_SS, 0 }, /* SS */
	{ pre_TP, post_TP, 0 }, /* TP */
	{ pre_PP, NULL, 0 }, /* LP */
	{ pre_PP, NULL, 0 }, /* PP */
	{ pre_PP, NULL, 0 }, /* P */
	{ pre_IP, post_IP, 0 }, /* IP */
	{ pre_HP, post_HP, 0 }, /* HP */
	{ NULL, NULL, 0 }, /* SM */
	{ pre_B, NULL, 0 }, /* SB */
	{ pre_alternate, NULL, 0 }, /* BI */
	{ pre_alternate, NULL, 0 }, /* IB */
	{ pre_alternate, NULL, 0 }, /* BR */
	{ pre_alternate, NULL, 0 }, /* RB */
	{ NULL, NULL, 0 }, /* R */
	{ pre_B, NULL, 0 }, /* B */
	{ pre_I, NULL, 0 }, /* I */
	{ pre_alternate, NULL, 0 }, /* IR */
	{ pre_alternate, NULL, 0 }, /* RI */
	{ pre_sp, NULL, MAN_NOTEXT }, /* sp */
	{ pre_literal, NULL, 0 }, /* nf */
	{ pre_literal, NULL, 0 }, /* fi */
	{ NULL, NULL, 0 }, /* RE */
	{ pre_RS, post_RS, 0 }, /* RS */
	{ pre_ign, NULL, 0 }, /* DT */
	{ pre_ign, NULL, MAN_NOTEXT }, /* UC */
	{ pre_PD, NULL, MAN_NOTEXT }, /* PD */
	{ pre_ign, NULL, 0 }, /* AT */
	{ pre_in, NULL, MAN_NOTEXT }, /* in */
	{ pre_ft, NULL, MAN_NOTEXT }, /* ft */
	{ pre_OP, NULL, 0 }, /* OP */
	{ pre_literal, NULL, 0 }, /* EX */
	{ pre_literal, NULL, 0 }, /* EE */
	{ pre_UR, post_UR, 0 }, /* UR */
	{ NULL, NULL, 0 }, /* UE */
	{ pre_ll, NULL, MAN_NOTEXT }, /* ll */
};


void
terminal_man(void *arg, const struct man *man)
{
	struct termp		*p;
	const struct man_meta	*meta;
	struct man_node		*n;
	struct mtermp		 mt;

	p = (struct termp *)arg;

	p->overstep = 0;
	p->rmargin = p->maxrmargin = p->defrmargin;
	p->tabwidth = term_len(p, 5);

	n = man_node(man)->child;
	meta = man_meta(man);

	memset(&mt, 0, sizeof(struct mtermp));

	mt.lmargin[mt.lmargincur] = term_len(p, p->defindent);
	mt.offset = term_len(p, p->defindent);
	mt.pardist = 1;

	if (p->synopsisonly) {
		while (n != NULL) {
			if (n->tok == MAN_SH &&
			    n->child->child->type == MAN_TEXT &&
			    !strcmp(n->child->child->string, "SYNOPSIS")) {
				if (n->child->next->child != NULL)
					print_man_nodelist(p, &mt,
					    n->child->next->child, meta);
				term_newln(p);
				break;
			}
			n = n->next;
		}
	} else {
		if (p->defindent == 0)
			p->defindent = 7;
		term_begin(p, print_man_head, print_man_foot, meta);
		p->flags |= TERMP_NOSPACE;
		if (n != NULL)
			print_man_nodelist(p, &mt, n, meta);
		term_end(p);
	}
}

/*
 * Printing leading vertical space before a block.
 * This is used for the paragraph macros.
 * The rules are pretty simple, since there's very little nesting going
 * on here.  Basically, if we're the first within another block (SS/SH),
 * then don't emit vertical space.  If we are (RS), then do.  If not the
 * first, print it.
 */
static void
print_bvspace(struct termp *p, const struct man_node *n, int pardist)
{
	int	 i;

	term_newln(p);

	if (n->body && n->body->child)
		if (MAN_TBL == n->body->child->type)
			return;

	if (MAN_ROOT == n->parent->type || MAN_RS != n->parent->tok)
		if (NULL == n->prev)
			return;

	for (i = 0; i < pardist; i++)
		term_vspace(p);
}


static int
pre_ign(DECL_ARGS)
{

	return(0);
}

static int
pre_ll(DECL_ARGS)
{

	term_setwidth(p, n->nchild ? n->child->string : NULL);
	return(0);
}

static int
pre_I(DECL_ARGS)
{

	term_fontrepl(p, TERMFONT_UNDER);
	return(1);
}

static int
pre_literal(DECL_ARGS)
{

	term_newln(p);

	if (MAN_nf == n->tok || MAN_EX == n->tok)
		mt->fl |= MANT_LITERAL;
	else
		mt->fl &= ~MANT_LITERAL;

	/*
	 * Unlike .IP and .TP, .HP does not have a HEAD.
	 * So in case a second call to term_flushln() is needed,
	 * indentation has to be set up explicitly.
	 */
	if (MAN_HP == n->parent->tok && p->rmargin < p->maxrmargin) {
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		p->flags |= TERMP_NOSPACE;
	}

	return(0);
}

static int
pre_PD(DECL_ARGS)
{
	struct roffsu	 su;

	n = n->child;
	if (n == NULL) {
		mt->pardist = 1;
		return(0);
	}
	assert(MAN_TEXT == n->type);
	if (a2roffsu(n->string, &su, SCALE_VS))
		mt->pardist = term_vspan(p, &su);
	return(0);
}

static int
pre_alternate(DECL_ARGS)
{
	enum termfont		 font[2];
	struct man_node		*nn;
	int			 savelit, i;

	switch (n->tok) {
	case MAN_RB:
		font[0] = TERMFONT_NONE;
		font[1] = TERMFONT_BOLD;
		break;
	case MAN_RI:
		font[0] = TERMFONT_NONE;
		font[1] = TERMFONT_UNDER;
		break;
	case MAN_BR:
		font[0] = TERMFONT_BOLD;
		font[1] = TERMFONT_NONE;
		break;
	case MAN_BI:
		font[0] = TERMFONT_BOLD;
		font[1] = TERMFONT_UNDER;
		break;
	case MAN_IR:
		font[0] = TERMFONT_UNDER;
		font[1] = TERMFONT_NONE;
		break;
	case MAN_IB:
		font[0] = TERMFONT_UNDER;
		font[1] = TERMFONT_BOLD;
		break;
	default:
		abort();
	}

	savelit = MANT_LITERAL & mt->fl;
	mt->fl &= ~MANT_LITERAL;

	for (i = 0, nn = n->child; nn; nn = nn->next, i = 1 - i) {
		term_fontrepl(p, font[i]);
		if (savelit && NULL == nn->next)
			mt->fl |= MANT_LITERAL;
		print_man_node(p, mt, nn, meta);
		if (nn->next)
			p->flags |= TERMP_NOSPACE;
	}

	return(0);
}

static int
pre_B(DECL_ARGS)
{

	term_fontrepl(p, TERMFONT_BOLD);
	return(1);
}

static int
pre_OP(DECL_ARGS)
{

	term_word(p, "[");
	p->flags |= TERMP_NOSPACE;

	if (NULL != (n = n->child)) {
		term_fontrepl(p, TERMFONT_BOLD);
		term_word(p, n->string);
	}
	if (NULL != n && NULL != n->next) {
		term_fontrepl(p, TERMFONT_UNDER);
		term_word(p, n->next->string);
	}

	term_fontrepl(p, TERMFONT_NONE);
	p->flags |= TERMP_NOSPACE;
	term_word(p, "]");
	return(0);
}

static int
pre_ft(DECL_ARGS)
{
	const char	*cp;

	if (NULL == n->child) {
		term_fontlast(p);
		return(0);
	}

	cp = n->child->string;
	switch (*cp) {
	case '4':
		/* FALLTHROUGH */
	case '3':
		/* FALLTHROUGH */
	case 'B':
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case '2':
		/* FALLTHROUGH */
	case 'I':
		term_fontrepl(p, TERMFONT_UNDER);
		break;
	case 'P':
		term_fontlast(p);
		break;
	case '1':
		/* FALLTHROUGH */
	case 'C':
		/* FALLTHROUGH */
	case 'R':
		term_fontrepl(p, TERMFONT_NONE);
		break;
	default:
		break;
	}
	return(0);
}

static int
pre_in(DECL_ARGS)
{
	struct roffsu	 su;
	const char	*cp;
	size_t		 v;
	int		 less;

	term_newln(p);

	if (NULL == n->child) {
		p->offset = mt->offset;
		return(0);
	}

	cp = n->child->string;
	less = 0;

	if ('-' == *cp)
		less = -1;
	else if ('+' == *cp)
		less = 1;
	else
		cp--;

	if ( ! a2roffsu(++cp, &su, SCALE_EN))
		return(0);

	v = term_hspan(p, &su);

	if (less < 0)
		p->offset -= p->offset > v ? v : p->offset;
	else if (less > 0)
		p->offset += v;
	else
		p->offset = v;
	if (p->offset > SHRT_MAX)
		p->offset = term_len(p, p->defindent);

	return(0);
}

static int
pre_sp(DECL_ARGS)
{
	struct roffsu	 su;
	int		 i, len;

	if ((NULL == n->prev && n->parent)) {
		switch (n->parent->tok) {
		case MAN_SH:
			/* FALLTHROUGH */
		case MAN_SS:
			/* FALLTHROUGH */
		case MAN_PP:
			/* FALLTHROUGH */
		case MAN_LP:
			/* FALLTHROUGH */
		case MAN_P:
			/* FALLTHROUGH */
			return(0);
		default:
			break;
		}
	}

	if (n->tok == MAN_br)
		len = 0;
	else if (n->child == NULL)
		len = 1;
	else {
		if ( ! a2roffsu(n->child->string, &su, SCALE_VS))
			su.scale = 1.0;
		len = term_vspan(p, &su);
	}

	if (len == 0)
		term_newln(p);
	else if (len < 0)
		p->skipvsp -= len;
	else
		for (i = 0; i < len; i++)
			term_vspace(p);

	return(0);
}

static int
pre_HP(DECL_ARGS)
{
	struct roffsu		 su;
	const struct man_node	*nn;
	int			 len;

	switch (n->type) {
	case MAN_BLOCK:
		print_bvspace(p, n, mt->pardist);
		return(1);
	case MAN_BODY:
		break;
	default:
		return(0);
	}

	if ( ! (MANT_LITERAL & mt->fl)) {
		p->flags |= TERMP_NOBREAK | TERMP_BRIND;
		p->trailspace = 2;
	}

	/* Calculate offset. */

	if ((nn = n->parent->head->child) != NULL &&
	    a2roffsu(nn->string, &su, SCALE_EN)) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	p->offset = mt->offset;
	p->rmargin = mt->offset + len;
	return(1);
}

static void
post_HP(DECL_ARGS)
{

	switch (n->type) {
	case MAN_BODY:
		term_newln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		p->trailspace = 0;
		p->offset = mt->offset;
		p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}
}

static int
pre_PP(DECL_ARGS)
{

	switch (n->type) {
	case MAN_BLOCK:
		mt->lmargin[mt->lmargincur] = term_len(p, p->defindent);
		print_bvspace(p, n, mt->pardist);
		break;
	default:
		p->offset = mt->offset;
		break;
	}

	return(MAN_HEAD != n->type);
}

static int
pre_IP(DECL_ARGS)
{
	struct roffsu		 su;
	const struct man_node	*nn;
	int			 len, savelit;

	switch (n->type) {
	case MAN_BODY:
		p->flags |= TERMP_NOSPACE;
		break;
	case MAN_HEAD:
		p->flags |= TERMP_NOBREAK;
		p->trailspace = 1;
		break;
	case MAN_BLOCK:
		print_bvspace(p, n, mt->pardist);
		/* FALLTHROUGH */
	default:
		return(1);
	}

	/* Calculate the offset from the optional second argument. */
	if ((nn = n->parent->head->child) != NULL &&
	    (nn = nn->next) != NULL &&
	    a2roffsu(nn->string, &su, SCALE_EN)) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	switch (n->type) {
	case MAN_HEAD:
		p->offset = mt->offset;
		p->rmargin = mt->offset + len;

		savelit = MANT_LITERAL & mt->fl;
		mt->fl &= ~MANT_LITERAL;

		if (n->child)
			print_man_node(p, mt, n->child, meta);

		if (savelit)
			mt->fl |= MANT_LITERAL;

		return(0);
	case MAN_BODY:
		p->offset = mt->offset + len;
		p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	return(1);
}

static void
post_IP(DECL_ARGS)
{

	switch (n->type) {
	case MAN_HEAD:
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		p->trailspace = 0;
		p->rmargin = p->maxrmargin;
		break;
	case MAN_BODY:
		term_newln(p);
		p->offset = mt->offset;
		break;
	default:
		break;
	}
}

static int
pre_TP(DECL_ARGS)
{
	struct roffsu		 su;
	struct man_node		*nn;
	int			 len, savelit;

	switch (n->type) {
	case MAN_HEAD:
		p->flags |= TERMP_NOBREAK;
		p->trailspace = 1;
		break;
	case MAN_BODY:
		p->flags |= TERMP_NOSPACE;
		break;
	case MAN_BLOCK:
		print_bvspace(p, n, mt->pardist);
		/* FALLTHROUGH */
	default:
		return(1);
	}

	/* Calculate offset. */

	if ((nn = n->parent->head->child) != NULL &&
	    nn->string != NULL && ! (MAN_LINE & nn->flags) &&
	    a2roffsu(nn->string, &su, SCALE_EN)) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	switch (n->type) {
	case MAN_HEAD:
		p->offset = mt->offset;
		p->rmargin = mt->offset + len;

		savelit = MANT_LITERAL & mt->fl;
		mt->fl &= ~MANT_LITERAL;

		/* Don't print same-line elements. */
		nn = n->child;
		while (NULL != nn && 0 == (MAN_LINE & nn->flags))
			nn = nn->next;

		while (NULL != nn) {
			print_man_node(p, mt, nn, meta);
			nn = nn->next;
		}

		if (savelit)
			mt->fl |= MANT_LITERAL;
		return(0);
	case MAN_BODY:
		p->offset = mt->offset + len;
		p->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~TERMP_NOBREAK;
		break;
	default:
		break;
	}

	return(1);
}

static void
post_TP(DECL_ARGS)
{

	switch (n->type) {
	case MAN_HEAD:
		term_flushln(p);
		break;
	case MAN_BODY:
		term_newln(p);
		p->offset = mt->offset;
		break;
	default:
		break;
	}
}

static int
pre_SS(DECL_ARGS)
{
	int	 i;

	switch (n->type) {
	case MAN_BLOCK:
		mt->fl &= ~MANT_LITERAL;
		mt->lmargin[mt->lmargincur] = term_len(p, p->defindent);
		mt->offset = term_len(p, p->defindent);

		/*
		 * No vertical space before the first subsection
		 * and after an empty subsection.
		 */

		do {
			n = n->prev;
		} while (n != NULL && termacts[n->tok].flags & MAN_NOTEXT);
		if (n == NULL || (n->tok == MAN_SS && n->body->child == NULL))
			break;

		for (i = 0; i < mt->pardist; i++)
			term_vspace(p);
		break;
	case MAN_HEAD:
		term_fontrepl(p, TERMFONT_BOLD);
		p->offset = term_len(p, 3);
		break;
	case MAN_BODY:
		p->offset = mt->offset;
		break;
	default:
		break;
	}

	return(1);
}

static void
post_SS(DECL_ARGS)
{

	switch (n->type) {
	case MAN_HEAD:
		term_newln(p);
		break;
	case MAN_BODY:
		term_newln(p);
		break;
	default:
		break;
	}
}

static int
pre_SH(DECL_ARGS)
{
	int	 i;

	switch (n->type) {
	case MAN_BLOCK:
		mt->fl &= ~MANT_LITERAL;
		mt->lmargin[mt->lmargincur] = term_len(p, p->defindent);
		mt->offset = term_len(p, p->defindent);

		/*
		 * No vertical space before the first section
		 * and after an empty section.
		 */

		do {
			n = n->prev;
		} while (n != NULL && termacts[n->tok].flags & MAN_NOTEXT);
		if (n == NULL || (n->tok == MAN_SH && n->body->child == NULL))
			break;

		for (i = 0; i < mt->pardist; i++)
			term_vspace(p);
		break;
	case MAN_HEAD:
		term_fontrepl(p, TERMFONT_BOLD);
		p->offset = 0;
		break;
	case MAN_BODY:
		p->offset = mt->offset;
		break;
	default:
		break;
	}

	return(1);
}

static void
post_SH(DECL_ARGS)
{

	switch (n->type) {
	case MAN_HEAD:
		term_newln(p);
		break;
	case MAN_BODY:
		term_newln(p);
		break;
	default:
		break;
	}
}

static int
pre_RS(DECL_ARGS)
{
	struct roffsu	 su;

	switch (n->type) {
	case MAN_BLOCK:
		term_newln(p);
		return(1);
	case MAN_HEAD:
		return(0);
	default:
		break;
	}

	n = n->parent->head;
	n->aux = SHRT_MAX + 1;
	if (n->child != NULL && a2roffsu(n->child->string, &su, SCALE_EN))
		n->aux = term_hspan(p, &su);
	if (n->aux < 0 && (size_t)(-n->aux) > mt->offset)
		n->aux = -mt->offset;
	else if (n->aux > SHRT_MAX)
		n->aux = term_len(p, p->defindent);

	mt->offset += n->aux;
	p->offset = mt->offset;
	p->rmargin = p->maxrmargin;

	if (++mt->lmarginsz < MAXMARGINS)
		mt->lmargincur = mt->lmarginsz;

	mt->lmargin[mt->lmargincur] = mt->lmargin[mt->lmargincur - 1];
	return(1);
}

static void
post_RS(DECL_ARGS)
{

	switch (n->type) {
	case MAN_BLOCK:
		return;
	case MAN_HEAD:
		return;
	default:
		term_newln(p);
		break;
	}

	mt->offset -= n->parent->head->aux;
	p->offset = mt->offset;

	if (--mt->lmarginsz < MAXMARGINS)
		mt->lmargincur = mt->lmarginsz;
}

static int
pre_UR(DECL_ARGS)
{

	return (MAN_HEAD != n->type);
}

static void
post_UR(DECL_ARGS)
{

	if (MAN_BLOCK != n->type)
		return;

	term_word(p, "<");
	p->flags |= TERMP_NOSPACE;

	if (NULL != n->child->child)
		print_man_node(p, mt, n->child->child, meta);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");
}

static void
print_man_node(DECL_ARGS)
{
	size_t		 rm, rmax;
	int		 c;

	switch (n->type) {
	case MAN_TEXT:
		/*
		 * If we have a blank line, output a vertical space.
		 * If we have a space as the first character, break
		 * before printing the line's data.
		 */
		if ('\0' == *n->string) {
			term_vspace(p);
			return;
		} else if (' ' == *n->string && MAN_LINE & n->flags)
			term_newln(p);

		term_word(p, n->string);
		goto out;

	case MAN_EQN:
		if ( ! (n->flags & MAN_LINE))
			p->flags |= TERMP_NOSPACE;
		term_eqn(p, n->eqn);
		if (n->next != NULL && ! (n->next->flags & MAN_LINE))
			p->flags |= TERMP_NOSPACE;
		return;
	case MAN_TBL:
		if (p->tbl.cols == NULL)
			term_vspace(p);
		term_tbl(p, n->span);
		return;
	default:
		break;
	}

	if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
		term_fontrepl(p, TERMFONT_NONE);

	c = 1;
	if (termacts[n->tok].pre)
		c = (*termacts[n->tok].pre)(p, mt, n, meta);

	if (c && n->child)
		print_man_nodelist(p, mt, n->child, meta);

	if (termacts[n->tok].post)
		(*termacts[n->tok].post)(p, mt, n, meta);
	if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
		term_fontrepl(p, TERMFONT_NONE);

out:
	/*
	 * If we're in a literal context, make sure that words
	 * together on the same line stay together.  This is a
	 * POST-printing call, so we check the NEXT word.  Since
	 * -man doesn't have nested macros, we don't need to be
	 * more specific than this.
	 */
	if (mt->fl & MANT_LITERAL &&
	    ! (p->flags & (TERMP_NOBREAK | TERMP_NONEWLINE)) &&
	    (n->next == NULL || n->next->flags & MAN_LINE)) {
		rm = p->rmargin;
		rmax = p->maxrmargin;
		p->rmargin = p->maxrmargin = TERM_MAXMARGIN;
		p->flags |= TERMP_NOSPACE;
		if (n->string != NULL && *n->string != '\0')
			term_flushln(p);
		else
			term_newln(p);
		if (rm < rmax && n->parent->tok == MAN_HP) {
			p->offset = rm;
			p->rmargin = rmax;
		} else
			p->rmargin = rm;
		p->maxrmargin = rmax;
	}
	if (MAN_EOS & n->flags)
		p->flags |= TERMP_SENTENCE;
}


static void
print_man_nodelist(DECL_ARGS)
{

	while (n != NULL) {
		print_man_node(p, mt, n, meta);
		n = n->next;
	}
}

static void
print_man_foot(struct termp *p, const void *arg)
{
	const struct man_meta	*meta;
	char			*title;
	size_t			 datelen, titlen;

	meta = (const struct man_meta *)arg;
	assert(meta->title);
	assert(meta->msec);
	assert(meta->date);

	term_fontrepl(p, TERMFONT_NONE);

	if (meta->hasbody)
		term_vspace(p);

	/*
	 * Temporary, undocumented option to imitate mdoc(7) output.
	 * In the bottom right corner, use the source instead of
	 * the title.
	 */

	if ( ! p->mdocstyle) {
		if (meta->hasbody) {
			term_vspace(p);
			term_vspace(p);
		}
		mandoc_asprintf(&title, "%s(%s)",
		    meta->title, meta->msec);
	} else if (meta->source) {
		title = mandoc_strdup(meta->source);
	} else {
		title = mandoc_strdup("");
	}
	datelen = term_strlen(p, meta->date);

	/* Bottom left corner: manual source. */

	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;
	p->trailspace = 1;
	p->offset = 0;
	p->rmargin = p->maxrmargin > datelen ?
	    (p->maxrmargin + term_len(p, 1) - datelen) / 2 : 0;

	if (meta->source)
		term_word(p, meta->source);
	term_flushln(p);

	/* At the bottom in the middle: manual date. */

	p->offset = p->rmargin;
	titlen = term_strlen(p, title);
	p->rmargin = p->maxrmargin > titlen ? p->maxrmargin - titlen : 0;
	p->flags |= TERMP_NOSPACE;

	term_word(p, meta->date);
	term_flushln(p);

	/* Bottom right corner: manual title and section. */

	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOSPACE;
	p->trailspace = 0;
	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;

	term_word(p, title);
	term_flushln(p);
	free(title);
}

static void
print_man_head(struct termp *p, const void *arg)
{
	const struct man_meta	*meta;
	const char		*volume;
	char			*title;
	size_t			 vollen, titlen;

	meta = (const struct man_meta *)arg;
	assert(meta->title);
	assert(meta->msec);

	volume = NULL == meta->vol ? "" : meta->vol;
	vollen = term_strlen(p, volume);

	/* Top left corner: manual title and section. */

	mandoc_asprintf(&title, "%s(%s)", meta->title, meta->msec);
	titlen = term_strlen(p, title);

	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;
	p->trailspace = 1;
	p->offset = 0;
	p->rmargin = 2 * (titlen+1) + vollen < p->maxrmargin ?
	    (p->maxrmargin - vollen + term_len(p, 1)) / 2 :
	    vollen < p->maxrmargin ? p->maxrmargin - vollen : 0;

	term_word(p, title);
	term_flushln(p);

	/* At the top in the middle: manual volume. */

	p->flags |= TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->offset + vollen + titlen < p->maxrmargin ?
	    p->maxrmargin - titlen : p->maxrmargin;

	term_word(p, volume);
	term_flushln(p);

	/* Top right corner: title and section, again. */

	p->flags &= ~TERMP_NOBREAK;
	p->trailspace = 0;
	if (p->rmargin + titlen <= p->maxrmargin) {
		p->flags |= TERMP_NOSPACE;
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		term_word(p, title);
		term_flushln(p);
	}

	p->flags &= ~TERMP_NOSPACE;
	p->offset = 0;
	p->rmargin = p->maxrmargin;

	/*
	 * Groff prints three blank lines before the content.
	 * Do the same, except in the temporary, undocumented
	 * mode imitating mdoc(7) output.
	 */

	term_vspace(p);
	if ( ! p->mdocstyle) {
		term_vspace(p);
		term_vspace(p);
	}
	free(title);
}
