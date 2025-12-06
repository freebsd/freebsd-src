/* $Id: man_term.c,v 1.248 2025/07/27 15:27:28 schwarze Exp $ */
/*
 * Copyright (c) 2010-2020,2022-23,2025 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Plain text formatter for man(7), used by mandoc(1)
 * for ASCII, UTF-8, PostScript, and PDF output.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "man.h"
#include "out.h"
#include "term.h"
#include "term_tag.h"
#include "main.h"

#define	MAXMARGINS	  64 /* Maximum number of indented scopes. */

struct	mtermp {
	int		  lmargin[MAXMARGINS]; /* Margins in basic units. */
	int		  lmargincur; /* Index of current margin. */
	int		  lmarginsz; /* Actual number of nested margins. */
	size_t		  offset; /* Default offset in basic units. */
	int		  pardist; /* Vert. space before par., unit: [v]. */
};

#define	DECL_ARGS	  struct termp *p, \
			  struct mtermp *mt, \
			  struct roff_node *n, \
			  const struct roff_meta *meta

struct	man_term_act {
	int		(*pre)(DECL_ARGS);
	void		(*post)(DECL_ARGS);
	int		  flags;
#define	MAN_NOTEXT	 (1 << 0) /* Never has text children. */
};

static	void		  print_man_nodelist(DECL_ARGS);
static	void		  print_man_node(DECL_ARGS);
static	void		  print_man_head(struct termp *,
				const struct roff_meta *);
static	void		  print_man_foot(struct termp *,
				const struct roff_meta *);
static	void		  print_bvspace(struct termp *,
				struct roff_node *, int);

static	int		  pre_B(DECL_ARGS);
static	int		  pre_DT(DECL_ARGS);
static	int		  pre_HP(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_MR(DECL_ARGS);
static	int		  pre_OP(DECL_ARGS);
static	int		  pre_PD(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RS(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_SY(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);
static	int		  pre_UR(DECL_ARGS);
static	int		  pre_alternate(DECL_ARGS);
static	int		  pre_ign(DECL_ARGS);
static	int		  pre_in(DECL_ARGS);
static	int		  pre_literal(DECL_ARGS);

static	void		  post_IP(DECL_ARGS);
static	void		  post_HP(DECL_ARGS);
static	void		  post_RS(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SY(DECL_ARGS);
static	void		  post_TP(DECL_ARGS);
static	void		  post_UR(DECL_ARGS);

static const struct man_term_act man_term_acts[MAN_MAX - MAN_TH] = {
	{ NULL, NULL, 0 }, /* TH */
	{ pre_SH, post_SH, 0 }, /* SH */
	{ pre_SS, post_SH, 0 }, /* SS */
	{ pre_TP, post_TP, 0 }, /* TP */
	{ pre_TP, post_TP, 0 }, /* TQ */
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
	{ NULL, NULL, 0 }, /* RE */
	{ pre_RS, post_RS, 0 }, /* RS */
	{ pre_DT, NULL, MAN_NOTEXT }, /* DT */
	{ pre_ign, NULL, MAN_NOTEXT }, /* UC */
	{ pre_PD, NULL, MAN_NOTEXT }, /* PD */
	{ pre_ign, NULL, MAN_NOTEXT }, /* AT */
	{ pre_in, NULL, MAN_NOTEXT }, /* in */
	{ pre_SY, post_SY, 0 }, /* SY */
	{ NULL, NULL, 0 }, /* YS */
	{ pre_OP, NULL, 0 }, /* OP */
	{ pre_literal, NULL, 0 }, /* EX */
	{ pre_literal, NULL, 0 }, /* EE */
	{ pre_UR, post_UR, 0 }, /* UR */
	{ NULL, NULL, 0 }, /* UE */
	{ pre_UR, post_UR, 0 }, /* MT */
	{ NULL, NULL, 0 }, /* ME */
	{ pre_MR, NULL, 0 }, /* MR */
};
static const struct man_term_act *man_term_act(enum roff_tok);


static const struct man_term_act *
man_term_act(enum roff_tok tok)
{
	assert(tok >= MAN_TH && tok <= MAN_MAX);
	return man_term_acts + (tok - MAN_TH);
}

void
terminal_man(void *arg, const struct roff_meta *man)
{
	struct mtermp		 mt;
	struct termp		*p;
	struct roff_node	*n, *nc, *nn;

	p = (struct termp *)arg;
	p->tcol->rmargin = p->maxrmargin = p->defrmargin;
	term_tab_set(p, NULL);
	term_tab_set(p, "T");
	term_tab_set(p, ".5i");

	memset(&mt, 0, sizeof(mt));
	mt.lmargin[mt.lmargincur] = term_len(p, 7);
	mt.offset = term_len(p, p->defindent);
	mt.pardist = 1;

	n = man->first->child;
	if (p->synopsisonly) {
		for (nn = NULL; n != NULL; n = n->next) {
			if (n->tok != MAN_SH)
				continue;
			nc = n->child->child;
			if (nc->type != ROFFT_TEXT)
				continue;
			if (strcmp(nc->string, "SYNOPSIS") == 0)
				break;
			if (nn == NULL && strcmp(nc->string, "NAME") == 0)
				nn = n;
		}
		if (n == NULL)
			n = nn;
		p->flags |= TERMP_NOSPACE;
		if (n != NULL && (n = n->child->next->child) != NULL)
			print_man_nodelist(p, &mt, n, man);
		term_newln(p);
	} else {
		term_begin(p, print_man_head, print_man_foot, man);
		p->flags |= TERMP_NOSPACE;
		if (n != NULL)
			print_man_nodelist(p, &mt, n, man);
		term_end(p);
	}
}

/*
 * Print leading vertical space before a paragraph, unless
 * it is the first paragraph in a section or subsection.
 * If it is the first paragraph in an .RS block, consider
 * that .RS block instead of the paragraph, recursively.
 */
static void
print_bvspace(struct termp *p, struct roff_node *n, int pardist)
{
	struct roff_node	*nch;
	int			 i;

	term_newln(p);

	if (n->body != NULL &&
	    (nch = roff_node_child(n->body)) != NULL &&
	    nch->type == ROFFT_TBL)
		return;

	while (roff_node_prev(n) == NULL) {
		n = n->parent;
		if (n->tok != MAN_RS)
			return;
		if (n->type == ROFFT_BODY)
			n = n->parent;
	}
	for (i = 0; i < pardist; i++)
		term_vspace(p);
}

static int
pre_ign(DECL_ARGS)
{
	return 0;
}

static int
pre_I(DECL_ARGS)
{
	term_fontrepl(p, TERMFONT_UNDER);
	return 1;
}

static int
pre_literal(DECL_ARGS)
{
	term_newln(p);

	/*
	 * Unlike .IP and .TP, .HP does not have a HEAD.
	 * So in case a second call to term_flushln() is needed,
	 * indentation has to be set up explicitly.
	 */
	if (n->parent->tok == MAN_HP && p->tcol->rmargin < p->maxrmargin) {
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		p->flags |= TERMP_NOSPACE;
	}
	return 0;
}

static int
pre_PD(DECL_ARGS)
{
	struct roffsu	 su;

	n = n->child;
	if (n == NULL) {
		mt->pardist = 1;
		return 0;
	}
	assert(n->type == ROFFT_TEXT);
	if (a2roffsu(n->string, &su, SCALE_VS) != NULL)
		mt->pardist = term_vspan(p, &su);
	return 0;
}

static int
pre_alternate(DECL_ARGS)
{
	enum termfont		 font[2];
	struct roff_node	*nn;
	int			 i;

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
	for (i = 0, nn = n->child; nn != NULL; nn = nn->next, i = 1 - i) {
		term_fontrepl(p, font[i]);
		assert(nn->type == ROFFT_TEXT);
		term_word(p, nn->string);
		if (nn->flags & NODE_EOS)
			p->flags |= TERMP_SENTENCE;
		if (nn->next != NULL)
			p->flags |= TERMP_NOSPACE;
	}
	return 0;
}

static int
pre_B(DECL_ARGS)
{
	term_fontrepl(p, TERMFONT_BOLD);
	return 1;
}

static int
pre_MR(DECL_ARGS)
{
	term_fontrepl(p, TERMFONT_NONE);
	n = n->child;
	if (n != NULL) {
		term_word(p, n->string);   /* name */
		p->flags |= TERMP_NOSPACE;
	}
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;
	if (n != NULL && (n = n->next) != NULL) {
		term_word(p, n->string);   /* section */
		p->flags |= TERMP_NOSPACE;
	}
	term_word(p, ")");
	if (n != NULL && (n = n->next) != NULL) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, n->string);   /* suffix */
	}
	return 0;
}

static int
pre_OP(DECL_ARGS)
{
	term_word(p, "[");
	p->flags |= TERMP_KEEP | TERMP_NOSPACE;

	if ((n = n->child) != NULL) {
		term_fontrepl(p, TERMFONT_BOLD);
		term_word(p, n->string);
	}
	if (n != NULL && n->next != NULL) {
		term_fontrepl(p, TERMFONT_UNDER);
		term_word(p, n->next->string);
	}
	term_fontrepl(p, TERMFONT_NONE);
	p->flags &= ~TERMP_KEEP;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "]");
	return 0;
}

static int
pre_in(DECL_ARGS)
{
	struct roffsu	 su;
	const char	*cp;	/* Request argument. */
	size_t		 v;	/* Indentation in basic units. */
	int		 less;

	term_newln(p);

	if (n->child == NULL) {
		p->tcol->offset = mt->offset;
		return 0;
	}

	cp = n->child->string;
	less = 0;

	if (*cp == '-') {
		less = -1;
		cp++;
	} else if (*cp == '+') {
		less = 1;
		cp++;
	}

	if (a2roffsu(cp, &su, SCALE_EN) == NULL)
		return 0;

	v = term_hspan(p, &su);

	if (less < 0)
		p->tcol->offset -= p->tcol->offset > v ? v : p->tcol->offset;
	else if (less > 0)
		p->tcol->offset += v;
	else
		p->tcol->offset = v;
	if (p->tcol->offset > SHRT_MAX)
		p->tcol->offset = term_len(p, p->defindent);

	return 0;
}

static int
pre_DT(DECL_ARGS)
{
	term_tab_set(p, NULL);
	term_tab_set(p, "T");
	term_tab_set(p, ".5i");
	return 0;
}

static int
pre_HP(DECL_ARGS)
{
	struct roffsu		 su;
	const struct roff_node	*nn;
	int			 len;	/* Indentation in basic units. */

	switch (n->type) {
	case ROFFT_BLOCK:
		print_bvspace(p, n, mt->pardist);
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}

	if (n->child == NULL)
		return 0;

	if ((n->child->flags & NODE_NOFILL) == 0) {
		p->flags |= TERMP_NOBREAK | TERMP_BRIND;
		p->trailspace = 2;
	}

	/* Calculate offset. */

	if ((nn = n->parent->head->child) != NULL &&
	    a2roffsu(nn->string, &su, SCALE_EN) != NULL) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	p->tcol->offset = mt->offset;
	p->tcol->rmargin = mt->offset + len;
	return 1;
}

static void
post_HP(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
	case ROFFT_HEAD:
		break;
	case ROFFT_BODY:
		term_newln(p);

		/*
		 * Compatibility with a groff bug.
		 * The .HP macro uses the undocumented .tag request
		 * which causes a line break and cancels no-space
		 * mode even if there isn't any output.
		 */

		if (n->child == NULL)
			term_vspace(p);

		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		p->trailspace = 0;
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = p->maxrmargin;
		break;
	default:
		abort();
	}
}

static int
pre_PP(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		mt->lmargin[mt->lmargincur] = term_len(p, 7);
		print_bvspace(p, n, mt->pardist);
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		p->tcol->offset = mt->offset;
		break;
	default:
		abort();
	}
	return 1;
}

static int
pre_IP(DECL_ARGS)
{
	struct roffsu		 su;
	const struct roff_node	*nn;
	int			 len;	/* Indentation in basic units. */

	switch (n->type) {
	case ROFFT_BLOCK:
		print_bvspace(p, n, mt->pardist);
		return 1;
	case ROFFT_HEAD:
		p->flags |= TERMP_NOBREAK;
		p->trailspace = 1;
		break;
	case ROFFT_BODY:
		p->flags |= TERMP_NOSPACE | TERMP_NONEWLINE;
		break;
	default:
		abort();
	}

	/* Calculate the offset from the optional second argument. */
	if ((nn = n->parent->head->child) != NULL &&
	    (nn = nn->next) != NULL &&
	    a2roffsu(nn->string, &su, SCALE_EN) != NULL) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	switch (n->type) {
	case ROFFT_HEAD:
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = mt->offset + len;
		if (n->child != NULL)
			print_man_node(p, mt, n->child, meta);
		return 0;
	case ROFFT_BODY:
		p->tcol->offset = mt->offset + len;
		p->tcol->rmargin = p->maxrmargin;
		break;
	default:
		abort();
	}
	return 1;
}

static void
post_IP(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		p->trailspace = 0;
		p->tcol->rmargin = p->maxrmargin;
		break;
	case ROFFT_BODY:
		term_newln(p);
		p->tcol->offset = mt->offset;
		break;
	default:
		abort();
	}
}

static int
pre_TP(DECL_ARGS)
{
	struct roffsu		 su;
	struct roff_node	*nn;
	int			 len;	/* Indentation in basic units. */

	switch (n->type) {
	case ROFFT_BLOCK:
		if (n->tok == MAN_TP)
			print_bvspace(p, n, mt->pardist);
		return 1;
	case ROFFT_HEAD:
		p->flags |= TERMP_NOBREAK | TERMP_BRTRSP;
		p->trailspace = 1;
		break;
	case ROFFT_BODY:
		p->flags |= TERMP_NOSPACE | TERMP_NONEWLINE;
		break;
	default:
		abort();
	}

	/* Calculate offset. */

	if ((nn = n->parent->head->child) != NULL &&
	    nn->string != NULL && ! (NODE_LINE & nn->flags) &&
	    a2roffsu(nn->string, &su, SCALE_EN) != NULL) {
		len = term_hspan(p, &su);
		if (len < 0 && (size_t)(-len) > mt->offset)
			len = -mt->offset;
		else if (len > SHRT_MAX)
			len = term_len(p, p->defindent);
		mt->lmargin[mt->lmargincur] = len;
	} else
		len = mt->lmargin[mt->lmargincur];

	switch (n->type) {
	case ROFFT_HEAD:
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = mt->offset + len;

		/* Don't print same-line elements. */
		nn = n->child;
		while (nn != NULL && (nn->flags & NODE_LINE) == 0)
			nn = nn->next;

		while (nn != NULL) {
			print_man_node(p, mt, nn, meta);
			nn = nn->next;
		}
		return 0;
	case ROFFT_BODY:
		p->tcol->offset = mt->offset + len;
		p->tcol->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRTRSP);
		break;
	default:
		abort();
	}
	return 1;
}

static void
post_TP(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		term_flushln(p);
		break;
	case ROFFT_BODY:
		term_newln(p);
		p->tcol->offset = mt->offset;
		break;
	default:
		abort();
	}
}

static int
pre_SS(DECL_ARGS)
{
	int	 i;

	switch (n->type) {
	case ROFFT_BLOCK:
		mt->lmargin[mt->lmargincur] = term_len(p, 7);
		mt->offset = term_len(p, p->defindent);

		/*
		 * No vertical space before the first subsection
		 * and after an empty subsection.
		 */

		if ((n = roff_node_prev(n)) == NULL ||
		    (n->tok == MAN_SS && roff_node_child(n->body) == NULL))
			break;

		for (i = 0; i < mt->pardist; i++)
			term_vspace(p);
		break;
	case ROFFT_HEAD:
		p->fontibi = 1;
		term_fontrepl(p, TERMFONT_BOLD);
		p->tcol->offset = term_len(p, p->defindent) / 2 + 1;
		p->tcol->rmargin = mt->offset;
		p->trailspace = mt->offset / term_len(p, 1);
		p->flags |= TERMP_NOBREAK | TERMP_BRIND;
		break;
	case ROFFT_BODY:
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		break;
	default:
		break;
	}
	return 1;
}

static int
pre_SH(DECL_ARGS)
{
	int	 i;

	switch (n->type) {
	case ROFFT_BLOCK:
		mt->lmargin[mt->lmargincur] = term_len(p, 7);
		mt->offset = term_len(p, p->defindent);

		/*
		 * No vertical space before the first section
		 * and after an empty section.
		 */

		if ((n = roff_node_prev(n)) == NULL ||
		    (n->tok == MAN_SH && roff_node_child(n->body) == NULL))
			break;

		for (i = 0; i < mt->pardist; i++)
			term_vspace(p);
		break;
	case ROFFT_HEAD:
		p->fontibi = 1;
		term_fontrepl(p, TERMFONT_BOLD);
		p->tcol->offset = 0;
		p->tcol->rmargin = mt->offset;
		p->trailspace = mt->offset / term_len(p, 1);
		p->flags |= TERMP_NOBREAK | TERMP_BRIND;
		break;
	case ROFFT_BODY:
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = p->maxrmargin;
		p->trailspace = 0;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
		break;
	default:
		abort();
	}
	return 1;
}

static void
post_SH(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		p->fontibi = 0;
		/* FALLTHROUGH */
	case ROFFT_BODY:
		term_newln(p);
		break;
	default:
		abort();
	}
}

static int
pre_RS(DECL_ARGS)
{
	struct roffsu	 su;

	switch (n->type) {
	case ROFFT_BLOCK:
		term_newln(p);
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}

	n = n->parent->head;
	n->aux = SHRT_MAX + 1;
	if (n->child == NULL)
		n->aux = mt->lmargin[mt->lmargincur];
	else if (a2roffsu(n->child->string, &su, SCALE_EN) != NULL)
		n->aux = term_hspan(p, &su);
	if (n->aux < 0 && (size_t)(-n->aux) > mt->offset)
		n->aux = -mt->offset;
	else if (n->aux > SHRT_MAX)
		n->aux = term_len(p, p->defindent);

	mt->offset += n->aux;
	p->tcol->offset = mt->offset;
	p->tcol->rmargin = p->maxrmargin;

	if (++mt->lmarginsz < MAXMARGINS)
		mt->lmargincur = mt->lmarginsz;

	mt->lmargin[mt->lmargincur] = term_len(p, 7);
	return 1;
}

static void
post_RS(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
	case ROFFT_HEAD:
		return;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}
	term_newln(p);
	mt->offset -= n->parent->head->aux;
	p->tcol->offset = mt->offset;
	if (--mt->lmarginsz < MAXMARGINS)
		mt->lmargincur = mt->lmarginsz;
}

static int
pre_SY(DECL_ARGS)
{
	const struct roff_node	*nn;
	int			 len;	/* Indentation in basic units. */

	switch (n->type) {
	case ROFFT_BLOCK:
		if ((nn = roff_node_prev(n)) == NULL || nn->tok != MAN_SY)
			print_bvspace(p, n, mt->pardist);
		return 1;
	case ROFFT_HEAD:
	case ROFFT_BODY:
		break;
	default:
		abort();
	}

	nn = n->parent->head->child;
	len = term_len(p, 1);
	if (nn != NULL)
		len += term_strlen(p, nn->string);

	switch (n->type) {
	case ROFFT_HEAD:
		p->tcol->offset = mt->offset;
		p->tcol->rmargin = mt->offset + len;
		if (n->next->child == NULL ||
		    (n->next->child->flags & NODE_NOFILL) == 0)
			p->flags |= TERMP_NOBREAK;
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case ROFFT_BODY:
		mt->lmargin[mt->lmargincur] = len;
		p->tcol->offset = mt->offset + len;
		p->tcol->rmargin = p->maxrmargin;
		p->flags |= TERMP_NOSPACE;
		break;
	default:
		abort();
	}
	return 1;
}

static void
post_SY(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		break;
	case ROFFT_BODY:
		term_newln(p);
		p->tcol->offset = mt->offset;
		break;
	default:
		abort();
	}
}

static int
pre_UR(DECL_ARGS)
{
	return n->type != ROFFT_HEAD;
}

static void
post_UR(DECL_ARGS)
{
	if (n->type != ROFFT_BLOCK)
		return;

	term_word(p, "<");
	p->flags |= TERMP_NOSPACE;

	if (n->child->child != NULL)
		print_man_node(p, mt, n->child->child, meta);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");
}

static void
print_man_node(DECL_ARGS)
{
	const struct man_term_act *act;
	int c;

	/*
	 * In no-fill mode, break the output line at the beginning
	 * of new input lines except after \c, and nowhere else.
	 */

	if (n->flags & NODE_NOFILL) {
		if (n->flags & NODE_LINE &&
		    (p->flags & TERMP_NONEWLINE) == 0)
			term_newln(p);
		p->flags |= TERMP_BRNEVER;
	} else {
		if (n->flags & NODE_LINE)
			term_tab_ref(p);
		p->flags &= ~TERMP_BRNEVER;
	}

	if (n->flags & NODE_ID)
		term_tag_write(n, p->line);

	switch (n->type) {
	case ROFFT_TEXT:
		/*
		 * If we have a blank line, output a vertical space.
		 * If we have a space as the first character, break
		 * before printing the line's data.
		 */
		if (*n->string == '\0') {
			if (p->flags & TERMP_NONEWLINE)
				term_newln(p);
			else
				term_vspace(p);
			return;
		} else if (*n->string == ' ' && n->flags & NODE_LINE &&
		    (p->flags & TERMP_NONEWLINE) == 0)
			term_newln(p);
		else if (n->flags & NODE_DELIMC)
			p->flags |= TERMP_NOSPACE;

		term_word(p, n->string);
		goto out;
	case ROFFT_COMMENT:
		return;
	case ROFFT_EQN:
		if ( ! (n->flags & NODE_LINE))
			p->flags |= TERMP_NOSPACE;
		term_eqn(p, n->eqn);
		if (n->next != NULL && ! (n->next->flags & NODE_LINE))
			p->flags |= TERMP_NOSPACE;
		return;
	case ROFFT_TBL:
		if (p->tbl.cols == NULL)
			term_newln(p);
		term_tbl(p, n->span);
		return;
	default:
		break;
	}

	if (n->tok < ROFF_MAX) {
		roff_term_pre(p, n);
		return;
	}

	act = man_term_act(n->tok);
	if ((act->flags & MAN_NOTEXT) == 0 && n->tok != MAN_SM)
		term_fontrepl(p, TERMFONT_NONE);

	c = 1;
	if (act->pre != NULL)
		c = (*act->pre)(p, mt, n, meta);

	if (c && n->child != NULL)
		print_man_nodelist(p, mt, n->child, meta);

	if (act->post != NULL)
		(*act->post)(p, mt, n, meta);
	if ((act->flags & MAN_NOTEXT) == 0 && n->tok != MAN_SM)
		term_fontrepl(p, TERMFONT_NONE);

out:
	if (n->parent->tok == MAN_HP && n->parent->type == ROFFT_BODY &&
	    n->prev == NULL && n->flags & NODE_NOFILL) {
		term_newln(p);
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = p->maxrmargin;
	}
	if (n->flags & NODE_EOS)
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
print_man_foot(struct termp *p, const struct roff_meta *meta)
{
	char			*title;
	size_t			 datelen, titlen;  /* In basic units. */

	assert(meta->title != NULL);
	assert(meta->msec != NULL);

	term_fontrepl(p, TERMFONT_NONE);
	if (meta->hasbody)
		term_vspace(p);

	datelen = term_strlen(p, meta->date);
	mandoc_asprintf(&title, "%s(%s)", meta->title, meta->msec);
	titlen = term_strlen(p, title);

	/* Bottom left corner: operating system. */

	p->tcol->offset = 0;
	p->tcol->rmargin = p->maxrmargin > datelen ?
	    (p->maxrmargin + term_len(p, 1) - datelen) / 2 : 0;
	p->trailspace = 1;
	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;

	if (meta->os)
		term_word(p, meta->os);
	term_flushln(p);

	/* At the bottom in the middle: manual date. */

	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->maxrmargin > titlen ?
	    p->maxrmargin - titlen : 0;
	p->flags |= TERMP_NOSPACE;

	term_word(p, meta->date);
	term_flushln(p);

	/* Bottom right corner: manual title and section. */

	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->maxrmargin;
	p->trailspace = 0;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	/*
	 * Reset the terminal state for more output after the footer:
	 * Some output modes, in particular PostScript and PDF, print
	 * the header and the footer into a buffer such that it can be
	 * reused for multiple output pages, then go on to format the
	 * main text.
	 */

        p->tcol->offset = 0;
        p->flags = 0;
	free(title);
}

static void
print_man_head(struct termp *p, const struct roff_meta *meta)
{
	const char		*volume;
	char			*title;
	size_t			 vollen, titlen;  /* In basic units. */

	assert(meta->title);
	assert(meta->msec);

	volume = NULL == meta->vol ? "" : meta->vol;
	vollen = term_strlen(p, volume);

	/* Top left corner: manual title and section. */

	mandoc_asprintf(&title, "%s(%s)", meta->title, meta->msec);
	titlen = term_strlen(p, title);

	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;
	p->trailspace = 1;
	p->tcol->offset = 0;
	p->tcol->rmargin =
	    titlen * 2 + term_len(p, 2) + vollen < p->maxrmargin ?
	    (p->maxrmargin - vollen + term_len(p, 1)) / 2 :
	    vollen < p->maxrmargin ? p->maxrmargin - vollen : 0;

	term_word(p, title);
	term_flushln(p);

	/* At the top in the middle: manual volume. */

	p->flags |= TERMP_NOSPACE;
	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->tcol->offset + vollen + titlen <
	    p->maxrmargin ? p->maxrmargin - titlen : p->maxrmargin;

	term_word(p, volume);
	term_flushln(p);

	/* Top right corner: title and section, again. */

	p->flags &= ~TERMP_NOBREAK;
	p->trailspace = 0;
	if (p->tcol->rmargin + titlen <= p->maxrmargin) {
		p->flags |= TERMP_NOSPACE;
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = p->maxrmargin;
		term_word(p, title);
		term_flushln(p);
	}

	p->flags &= ~TERMP_NOSPACE;
	p->tcol->offset = 0;
	p->tcol->rmargin = p->maxrmargin;
	term_vspace(p);
	free(title);
}
