/*	$Id: man_html.c,v 1.120 2016/01/08 17:48:09 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "roff.h"
#include "man.h"
#include "out.h"
#include "html.h"
#include "main.h"

/* TODO: preserve ident widths. */
/* FIXME: have PD set the default vspace width. */

#define	INDENT		  5

#define	MAN_ARGS	  const struct roff_meta *man, \
			  const struct roff_node *n, \
			  struct mhtml *mh, \
			  struct html *h

struct	mhtml {
	int		  fl;
#define	MANH_LITERAL	 (1 << 0) /* literal context */
};

struct	htmlman {
	int		(*pre)(MAN_ARGS);
	int		(*post)(MAN_ARGS);
};

static	void		  print_bvspace(struct html *,
				const struct roff_node *);
static	void		  print_man_head(MAN_ARGS);
static	void		  print_man_nodelist(MAN_ARGS);
static	void		  print_man_node(MAN_ARGS);
static	int		  a2width(const struct roff_node *,
				struct roffsu *);
static	int		  man_B_pre(MAN_ARGS);
static	int		  man_HP_pre(MAN_ARGS);
static	int		  man_IP_pre(MAN_ARGS);
static	int		  man_I_pre(MAN_ARGS);
static	int		  man_OP_pre(MAN_ARGS);
static	int		  man_PP_pre(MAN_ARGS);
static	int		  man_RS_pre(MAN_ARGS);
static	int		  man_SH_pre(MAN_ARGS);
static	int		  man_SM_pre(MAN_ARGS);
static	int		  man_SS_pre(MAN_ARGS);
static	int		  man_UR_pre(MAN_ARGS);
static	int		  man_alt_pre(MAN_ARGS);
static	int		  man_br_pre(MAN_ARGS);
static	int		  man_ign_pre(MAN_ARGS);
static	int		  man_in_pre(MAN_ARGS);
static	int		  man_literal_pre(MAN_ARGS);
static	void		  man_root_post(MAN_ARGS);
static	void		  man_root_pre(MAN_ARGS);

static	const struct htmlman mans[MAN_MAX] = {
	{ man_br_pre, NULL }, /* br */
	{ NULL, NULL }, /* TH */
	{ man_SH_pre, NULL }, /* SH */
	{ man_SS_pre, NULL }, /* SS */
	{ man_IP_pre, NULL }, /* TP */
	{ man_PP_pre, NULL }, /* LP */
	{ man_PP_pre, NULL }, /* PP */
	{ man_PP_pre, NULL }, /* P */
	{ man_IP_pre, NULL }, /* IP */
	{ man_HP_pre, NULL }, /* HP */
	{ man_SM_pre, NULL }, /* SM */
	{ man_SM_pre, NULL }, /* SB */
	{ man_alt_pre, NULL }, /* BI */
	{ man_alt_pre, NULL }, /* IB */
	{ man_alt_pre, NULL }, /* BR */
	{ man_alt_pre, NULL }, /* RB */
	{ NULL, NULL }, /* R */
	{ man_B_pre, NULL }, /* B */
	{ man_I_pre, NULL }, /* I */
	{ man_alt_pre, NULL }, /* IR */
	{ man_alt_pre, NULL }, /* RI */
	{ man_br_pre, NULL }, /* sp */
	{ man_literal_pre, NULL }, /* nf */
	{ man_literal_pre, NULL }, /* fi */
	{ NULL, NULL }, /* RE */
	{ man_RS_pre, NULL }, /* RS */
	{ man_ign_pre, NULL }, /* DT */
	{ man_ign_pre, NULL }, /* UC */
	{ man_ign_pre, NULL }, /* PD */
	{ man_ign_pre, NULL }, /* AT */
	{ man_in_pre, NULL }, /* in */
	{ man_ign_pre, NULL }, /* ft */
	{ man_OP_pre, NULL }, /* OP */
	{ man_literal_pre, NULL }, /* EX */
	{ man_literal_pre, NULL }, /* EE */
	{ man_UR_pre, NULL }, /* UR */
	{ NULL, NULL }, /* UE */
	{ man_ign_pre, NULL }, /* ll */
};


/*
 * Printing leading vertical space before a block.
 * This is used for the paragraph macros.
 * The rules are pretty simple, since there's very little nesting going
 * on here.  Basically, if we're the first within another block (SS/SH),
 * then don't emit vertical space.  If we are (RS), then do.  If not the
 * first, print it.
 */
static void
print_bvspace(struct html *h, const struct roff_node *n)
{

	if (n->body && n->body->child)
		if (n->body->child->type == ROFFT_TBL)
			return;

	if (n->parent->type == ROFFT_ROOT || n->parent->tok != MAN_RS)
		if (NULL == n->prev)
			return;

	print_paragraph(h);
}

void
html_man(void *arg, const struct roff_man *man)
{
	struct mhtml	 mh;
	struct htmlpair	 tag;
	struct html	*h;
	struct tag	*t, *tt;

	memset(&mh, 0, sizeof(mh));
	PAIR_CLASS_INIT(&tag, "mandoc");
	h = (struct html *)arg;

	if ( ! (HTML_FRAGMENT & h->oflags)) {
		print_gen_decls(h);
		t = print_otag(h, TAG_HTML, 0, NULL);
		tt = print_otag(h, TAG_HEAD, 0, NULL);
		print_man_head(&man->meta, man->first, &mh, h);
		print_tagq(h, tt);
		print_otag(h, TAG_BODY, 0, NULL);
		print_otag(h, TAG_DIV, 1, &tag);
	} else
		t = print_otag(h, TAG_DIV, 1, &tag);

	print_man_nodelist(&man->meta, man->first, &mh, h);
	print_tagq(h, t);
	putchar('\n');
}

static void
print_man_head(MAN_ARGS)
{

	print_gen_head(h);
	assert(man->title);
	assert(man->msec);
	bufcat_fmt(h, "%s(%s)", man->title, man->msec);
	print_otag(h, TAG_TITLE, 0, NULL);
	print_text(h, h->buf);
}

static void
print_man_nodelist(MAN_ARGS)
{

	while (n != NULL) {
		print_man_node(man, n, mh, h);
		n = n->next;
	}
}

static void
print_man_node(MAN_ARGS)
{
	int		 child;
	struct tag	*t;

	child = 1;
	t = h->tags.head;

	switch (n->type) {
	case ROFFT_ROOT:
		man_root_pre(man, n, mh, h);
		break;
	case ROFFT_TEXT:
		if ('\0' == *n->string) {
			print_paragraph(h);
			return;
		}
		if (n->flags & MAN_LINE && (*n->string == ' ' ||
		    (n->prev != NULL && mh->fl & MANH_LITERAL &&
		     ! (h->flags & HTML_NONEWLINE))))
			print_otag(h, TAG_BR, 0, NULL);
		print_text(h, n->string);
		return;
	case ROFFT_EQN:
		if (n->flags & MAN_LINE)
			putchar('\n');
		print_eqn(h, n->eqn);
		break;
	case ROFFT_TBL:
		/*
		 * This will take care of initialising all of the table
		 * state data for the first table, then tearing it down
		 * for the last one.
		 */
		print_tbl(h, n->span);
		return;
	default:
		/*
		 * Close out scope of font prior to opening a macro
		 * scope.
		 */
		if (HTMLFONT_NONE != h->metac) {
			h->metal = h->metac;
			h->metac = HTMLFONT_NONE;
		}

		/*
		 * Close out the current table, if it's open, and unset
		 * the "meta" table state.  This will be reopened on the
		 * next table element.
		 */
		if (h->tblt) {
			print_tblclose(h);
			t = h->tags.head;
		}
		if (mans[n->tok].pre)
			child = (*mans[n->tok].pre)(man, n, mh, h);
		break;
	}

	if (child && n->child)
		print_man_nodelist(man, n->child, mh, h);

	/* This will automatically close out any font scope. */
	print_stagq(h, t);

	switch (n->type) {
	case ROFFT_ROOT:
		man_root_post(man, n, mh, h);
		break;
	case ROFFT_EQN:
		break;
	default:
		if (mans[n->tok].post)
			(*mans[n->tok].post)(man, n, mh, h);
		break;
	}
}

static int
a2width(const struct roff_node *n, struct roffsu *su)
{

	if (n->type != ROFFT_TEXT)
		return 0;
	if (a2roffsu(n->string, su, SCALE_EN))
		return 1;

	return 0;
}

static void
man_root_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t, *tt;
	char		*title;

	assert(man->title);
	assert(man->msec);
	mandoc_asprintf(&title, "%s(%s)", man->title, man->msec);

	PAIR_CLASS_INIT(&tag, "head");
	t = print_otag(h, TAG_TABLE, 1, &tag);

	print_otag(h, TAG_TBODY, 0, NULL);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "head-ltitle");
	print_otag(h, TAG_TD, 1, &tag);
	print_text(h, title);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag, "head-vol");
	print_otag(h, TAG_TD, 1, &tag);
	if (NULL != man->vol)
		print_text(h, man->vol);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag, "head-rtitle");
	print_otag(h, TAG_TD, 1, &tag);
	print_text(h, title);
	print_tagq(h, t);
	free(title);
}

static void
man_root_post(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t, *tt;

	PAIR_CLASS_INIT(&tag, "foot");
	t = print_otag(h, TAG_TABLE, 1, &tag);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "foot-date");
	print_otag(h, TAG_TD, 1, &tag);

	assert(man->date);
	print_text(h, man->date);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag, "foot-os");
	print_otag(h, TAG_TD, 1, &tag);

	if (man->os)
		print_text(h, man->os);
	print_tagq(h, t);
}


static int
man_br_pre(MAN_ARGS)
{
	struct roffsu	 su;
	struct htmlpair	 tag;

	SCALE_VS_INIT(&su, 1);

	if (MAN_sp == n->tok) {
		if (NULL != (n = n->child))
			if ( ! a2roffsu(n->string, &su, SCALE_VS))
				su.scale = 1.0;
	} else
		su.scale = 0.0;

	bufinit(h);
	bufcat_su(h, "height", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);

	/* So the div isn't empty: */
	print_text(h, "\\~");

	return 0;
}

static int
man_SH_pre(MAN_ARGS)
{
	struct htmlpair	 tag;

	if (n->type == ROFFT_BLOCK) {
		mh->fl &= ~MANH_LITERAL;
		PAIR_CLASS_INIT(&tag, "section");
		print_otag(h, TAG_DIV, 1, &tag);
		return 1;
	} else if (n->type == ROFFT_BODY)
		return 1;

	print_otag(h, TAG_H1, 0, NULL);
	return 1;
}

static int
man_alt_pre(MAN_ARGS)
{
	const struct roff_node	*nn;
	int		 i, savelit;
	enum htmltag	 fp;
	struct tag	*t;

	if ((savelit = mh->fl & MANH_LITERAL))
		print_otag(h, TAG_BR, 0, NULL);

	mh->fl &= ~MANH_LITERAL;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		t = NULL;
		switch (n->tok) {
		case MAN_BI:
			fp = i % 2 ? TAG_I : TAG_B;
			break;
		case MAN_IB:
			fp = i % 2 ? TAG_B : TAG_I;
			break;
		case MAN_RI:
			fp = i % 2 ? TAG_I : TAG_MAX;
			break;
		case MAN_IR:
			fp = i % 2 ? TAG_MAX : TAG_I;
			break;
		case MAN_BR:
			fp = i % 2 ? TAG_MAX : TAG_B;
			break;
		case MAN_RB:
			fp = i % 2 ? TAG_B : TAG_MAX;
			break;
		default:
			abort();
		}

		if (i)
			h->flags |= HTML_NOSPACE;

		if (TAG_MAX != fp)
			t = print_otag(h, fp, 0, NULL);

		print_man_node(man, nn, mh, h);

		if (t)
			print_tagq(h, t);
	}

	if (savelit)
		mh->fl |= MANH_LITERAL;

	return 0;
}

static int
man_SM_pre(MAN_ARGS)
{

	print_otag(h, TAG_SMALL, 0, NULL);
	if (MAN_SB == n->tok)
		print_otag(h, TAG_B, 0, NULL);
	return 1;
}

static int
man_SS_pre(MAN_ARGS)
{
	struct htmlpair	 tag;

	if (n->type == ROFFT_BLOCK) {
		mh->fl &= ~MANH_LITERAL;
		PAIR_CLASS_INIT(&tag, "subsection");
		print_otag(h, TAG_DIV, 1, &tag);
		return 1;
	} else if (n->type == ROFFT_BODY)
		return 1;

	print_otag(h, TAG_H2, 0, NULL);
	return 1;
}

static int
man_PP_pre(MAN_ARGS)
{

	if (n->type == ROFFT_HEAD)
		return 0;
	else if (n->type == ROFFT_BLOCK)
		print_bvspace(h, n);

	return 1;
}

static int
man_IP_pre(MAN_ARGS)
{
	const struct roff_node	*nn;

	if (n->type == ROFFT_BODY) {
		print_otag(h, TAG_DD, 0, NULL);
		return 1;
	} else if (n->type != ROFFT_HEAD) {
		print_otag(h, TAG_DL, 0, NULL);
		return 1;
	}

	/* FIXME: width specification. */

	print_otag(h, TAG_DT, 0, NULL);

	/* For IP, only print the first header element. */

	if (MAN_IP == n->tok && n->child)
		print_man_node(man, n->child, mh, h);

	/* For TP, only print next-line header elements. */

	if (MAN_TP == n->tok) {
		nn = n->child;
		while (NULL != nn && 0 == (MAN_LINE & nn->flags))
			nn = nn->next;
		while (NULL != nn) {
			print_man_node(man, nn, mh, h);
			nn = nn->next;
		}
	}

	return 0;
}

static int
man_HP_pre(MAN_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;
	const struct roff_node *np;

	if (n->type == ROFFT_HEAD)
		return 0;
	else if (n->type != ROFFT_BLOCK)
		return 1;

	np = n->head->child;

	if (NULL == np || ! a2width(np, &su))
		SCALE_HS_INIT(&su, INDENT);

	bufinit(h);

	print_bvspace(h, n);
	bufcat_su(h, "margin-left", &su);
	su.scale = -su.scale;
	bufcat_su(h, "text-indent", &su);
	PAIR_STYLE_INIT(&tag[0], h);
	PAIR_CLASS_INIT(&tag[1], "spacer");
	print_otag(h, TAG_DIV, 2, tag);
	return 1;
}

static int
man_OP_pre(MAN_ARGS)
{
	struct tag	*tt;
	struct htmlpair	 tag;

	print_text(h, "[");
	h->flags |= HTML_NOSPACE;
	PAIR_CLASS_INIT(&tag, "opt");
	tt = print_otag(h, TAG_SPAN, 1, &tag);

	if (NULL != (n = n->child)) {
		print_otag(h, TAG_B, 0, NULL);
		print_text(h, n->string);
	}

	print_stagq(h, tt);

	if (NULL != n && NULL != n->next) {
		print_otag(h, TAG_I, 0, NULL);
		print_text(h, n->next->string);
	}

	print_stagq(h, tt);
	h->flags |= HTML_NOSPACE;
	print_text(h, "]");
	return 0;
}

static int
man_B_pre(MAN_ARGS)
{

	print_otag(h, TAG_B, 0, NULL);
	return 1;
}

static int
man_I_pre(MAN_ARGS)
{

	print_otag(h, TAG_I, 0, NULL);
	return 1;
}

static int
man_literal_pre(MAN_ARGS)
{

	if (MAN_fi == n->tok || MAN_EE == n->tok) {
		print_otag(h, TAG_BR, 0, NULL);
		mh->fl &= ~MANH_LITERAL;
	} else
		mh->fl |= MANH_LITERAL;

	return 0;
}

static int
man_in_pre(MAN_ARGS)
{

	print_otag(h, TAG_BR, 0, NULL);
	return 0;
}

static int
man_ign_pre(MAN_ARGS)
{

	return 0;
}

static int
man_RS_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;

	if (n->type == ROFFT_HEAD)
		return 0;
	else if (n->type == ROFFT_BODY)
		return 1;

	SCALE_HS_INIT(&su, INDENT);
	if (n->head->child)
		a2width(n->head->child, &su);

	bufinit(h);
	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	return 1;
}

static int
man_UR_pre(MAN_ARGS)
{
	struct htmlpair		 tag[2];

	n = n->child;
	assert(n->type == ROFFT_HEAD);
	if (n->child != NULL) {
		assert(n->child->type == ROFFT_TEXT);
		PAIR_CLASS_INIT(&tag[0], "link-ext");
		PAIR_HREF_INIT(&tag[1], n->child->string);
		print_otag(h, TAG_A, 2, tag);
	}

	assert(n->next->type == ROFFT_BODY);
	if (n->next->child != NULL)
		n = n->next;

	print_man_nodelist(man, n->child, mh, h);

	return 0;
}
