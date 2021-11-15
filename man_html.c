/* $Id: man_html.c,v 1.179 2020/10/16 17:22:43 schwarze Exp $ */
/*
 * Copyright (c) 2013-2015, 2017-2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * HTML formatter for man(7) used by mandoc(1).
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "man.h"
#include "out.h"
#include "html.h"
#include "main.h"

#define	MAN_ARGS	  const struct roff_meta *man, \
			  struct roff_node *n, \
			  struct html *h

struct	man_html_act {
	int		(*pre)(MAN_ARGS);
	int		(*post)(MAN_ARGS);
};

static	void		  print_man_head(const struct roff_meta *,
				struct html *);
static	void		  print_man_nodelist(MAN_ARGS);
static	void		  print_man_node(MAN_ARGS);
static	char		  list_continues(const struct roff_node *,
				const struct roff_node *);
static	int		  man_B_pre(MAN_ARGS);
static	int		  man_IP_pre(MAN_ARGS);
static	int		  man_I_pre(MAN_ARGS);
static	int		  man_OP_pre(MAN_ARGS);
static	int		  man_PP_pre(MAN_ARGS);
static	int		  man_RS_pre(MAN_ARGS);
static	int		  man_SH_pre(MAN_ARGS);
static	int		  man_SM_pre(MAN_ARGS);
static	int		  man_SY_pre(MAN_ARGS);
static	int		  man_UR_pre(MAN_ARGS);
static	int		  man_abort_pre(MAN_ARGS);
static	int		  man_alt_pre(MAN_ARGS);
static	int		  man_ign_pre(MAN_ARGS);
static	int		  man_in_pre(MAN_ARGS);
static	void		  man_root_post(const struct roff_meta *,
				struct html *);
static	void		  man_root_pre(const struct roff_meta *,
				struct html *);

static	const struct man_html_act man_html_acts[MAN_MAX - MAN_TH] = {
	{ NULL, NULL }, /* TH */
	{ man_SH_pre, NULL }, /* SH */
	{ man_SH_pre, NULL }, /* SS */
	{ man_IP_pre, NULL }, /* TP */
	{ man_IP_pre, NULL }, /* TQ */
	{ man_abort_pre, NULL }, /* LP */
	{ man_PP_pre, NULL }, /* PP */
	{ man_abort_pre, NULL }, /* P */
	{ man_IP_pre, NULL }, /* IP */
	{ man_PP_pre, NULL }, /* HP */
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
	{ NULL, NULL }, /* RE */
	{ man_RS_pre, NULL }, /* RS */
	{ man_ign_pre, NULL }, /* DT */
	{ man_ign_pre, NULL }, /* UC */
	{ man_ign_pre, NULL }, /* PD */
	{ man_ign_pre, NULL }, /* AT */
	{ man_in_pre, NULL }, /* in */
	{ man_SY_pre, NULL }, /* SY */
	{ NULL, NULL }, /* YS */
	{ man_OP_pre, NULL }, /* OP */
	{ NULL, NULL }, /* EX */
	{ NULL, NULL }, /* EE */
	{ man_UR_pre, NULL }, /* UR */
	{ NULL, NULL }, /* UE */
	{ man_UR_pre, NULL }, /* MT */
	{ NULL, NULL }, /* ME */
};


void
html_man(void *arg, const struct roff_meta *man)
{
	struct html		*h;
	struct roff_node	*n;
	struct tag		*t;

	h = (struct html *)arg;
	n = man->first->child;

	if ((h->oflags & HTML_FRAGMENT) == 0) {
		print_gen_decls(h);
		print_otag(h, TAG_HTML, "");
		if (n != NULL && n->type == ROFFT_COMMENT)
			print_gen_comment(h, n);
		t = print_otag(h, TAG_HEAD, "");
		print_man_head(man, h);
		print_tagq(h, t);
		print_otag(h, TAG_BODY, "");
	}

	man_root_pre(man, h);
	t = print_otag(h, TAG_DIV, "c", "manual-text");
	print_man_nodelist(man, n, h);
	print_tagq(h, t);
	man_root_post(man, h);
	print_tagq(h, NULL);
}

static void
print_man_head(const struct roff_meta *man, struct html *h)
{
	char	*cp;

	print_gen_head(h);
	mandoc_asprintf(&cp, "%s(%s)", man->title, man->msec);
	print_otag(h, TAG_TITLE, "");
	print_text(h, cp);
	free(cp);
}

static void
print_man_nodelist(MAN_ARGS)
{
	while (n != NULL) {
		print_man_node(man, n, h);
		n = n->next;
	}
}

static void
print_man_node(MAN_ARGS)
{
	struct tag	*t;
	int		 child;

	if (n->type == ROFFT_COMMENT || n->flags & NODE_NOPRT)
		return;

	if ((n->flags & NODE_NOFILL) == 0)
		html_fillmode(h, ROFF_fi);
	else if (html_fillmode(h, ROFF_nf) == ROFF_nf &&
	    n->tok != ROFF_fi && n->flags & NODE_LINE &&
	    (n->prev == NULL || n->prev->tok != MAN_YS))
		print_endline(h);

	child = 1;
	switch (n->type) {
	case ROFFT_TEXT:
		if (*n->string == '\0') {
			print_endline(h);
			return;
		}
		if (*n->string == ' ' && n->flags & NODE_LINE &&
		    (h->flags & HTML_NONEWLINE) == 0)
			print_otag(h, TAG_BR, "");
		else if (n->flags & NODE_DELIMC)
			h->flags |= HTML_NOSPACE;
		t = h->tag;
		t->refcnt++;
		print_text(h, n->string);
		break;
	case ROFFT_EQN:
		t = h->tag;
		t->refcnt++;
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
		if (h->metac != ESCAPE_FONTROMAN) {
			h->metal = h->metac;
			h->metac = ESCAPE_FONTROMAN;
		}

		/*
		 * Close out the current table, if it's open, and unset
		 * the "meta" table state.  This will be reopened on the
		 * next table element.
		 */
		if (h->tblt != NULL)
			print_tblclose(h);
		t = h->tag;
		t->refcnt++;
		if (n->tok < ROFF_MAX) {
			roff_html_pre(h, n);
			t->refcnt--;
			print_stagq(h, t);
			return;
		}
		assert(n->tok >= MAN_TH && n->tok < MAN_MAX);
		if (man_html_acts[n->tok - MAN_TH].pre != NULL)
			child = (*man_html_acts[n->tok - MAN_TH].pre)(man,
			    n, h);
		break;
	}

	if (child && n->child != NULL)
		print_man_nodelist(man, n->child, h);

	/* This will automatically close out any font scope. */
	t->refcnt--;
	if (n->type == ROFFT_BLOCK &&
	    (n->tok == MAN_IP || n->tok == MAN_TP || n->tok == MAN_TQ)) {
		t = h->tag;
		while (t->tag != TAG_DL && t->tag != TAG_UL)
			t = t->next;
		/*
		 * Close the list if no further item of the same type
		 * follows; otherwise, close the item only.
		 */
		if (list_continues(n, roff_node_next(n)) == '\0') {
			print_tagq(h, t);
			t = NULL;
		}
	}
	if (t != NULL)
		print_stagq(h, t);
}

static void
man_root_pre(const struct roff_meta *man, struct html *h)
{
	struct tag	*t, *tt;
	char		*title;

	assert(man->title);
	assert(man->msec);
	mandoc_asprintf(&title, "%s(%s)", man->title, man->msec);

	t = print_otag(h, TAG_TABLE, "c", "head");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "head-ltitle");
	print_text(h, title);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-vol");
	if (man->vol != NULL)
		print_text(h, man->vol);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-rtitle");
	print_text(h, title);
	print_tagq(h, t);
	free(title);
}

static void
man_root_post(const struct roff_meta *man, struct html *h)
{
	struct tag	*t, *tt;

	t = print_otag(h, TAG_TABLE, "c", "foot");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "foot-date");
	print_text(h, man->date);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "foot-os");
	if (man->os != NULL)
		print_text(h, man->os);
	print_tagq(h, t);
}

static int
man_SH_pre(MAN_ARGS)
{
	const char	*class;
	enum htmltag	 tag;

	if (n->tok == MAN_SH) {
		tag = TAG_H1;
		class = "Sh";
	} else {
		tag = TAG_H2;
		class = "Ss";
	}
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		print_otag(h, TAG_SECTION, "c", class);
		break;
	case ROFFT_HEAD:
		print_otag_id(h, tag, class, n);
		break;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}
	return 1;
}

static int
man_alt_pre(MAN_ARGS)
{
	const struct roff_node	*nn;
	struct tag	*t;
	int		 i;
	enum htmltag	 fp;

	for (i = 0, nn = n->child; nn != NULL; nn = nn->next, i++) {
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

		if (fp != TAG_MAX)
			t = print_otag(h, fp, "");

		print_text(h, nn->string);

		if (fp != TAG_MAX)
			print_tagq(h, t);
	}
	return 0;
}

static int
man_SM_pre(MAN_ARGS)
{
	print_otag(h, TAG_SMALL, "");
	if (n->tok == MAN_SB)
		print_otag(h, TAG_B, "");
	return 1;
}

static int
man_PP_pre(MAN_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		if (n->child != NULL &&
		    (n->child->flags & NODE_NOFILL) == 0)
			print_otag(h, TAG_P, "c",
			    n->tok == MAN_PP ? "Pp" : "Pp HP");
		break;
	default:
		abort();
	}
	return 1;
}

static char
list_continues(const struct roff_node *n1, const struct roff_node *n2)
{
	const char *s1, *s2;
	char c1, c2;

	if (n1 == NULL || n1->type != ROFFT_BLOCK ||
	    n2 == NULL || n2->type != ROFFT_BLOCK)
		return '\0';
	if ((n1->tok == MAN_TP || n1->tok == MAN_TQ) &&
	    (n2->tok == MAN_TP || n2->tok == MAN_TQ))
		return ' ';
	if (n1->tok != MAN_IP || n2->tok != MAN_IP)
		return '\0';
	n1 = n1->head->child;
	n2 = n2->head->child;
	s1 = n1 == NULL ? "" : n1->string;
	s2 = n2 == NULL ? "" : n2->string;
	c1 = strcmp(s1, "*") == 0 ? '*' :
	     strcmp(s1, "\\-") == 0 ? '-' :
	     strcmp(s1, "\\(bu") == 0 ? 'b' : ' ';
	c2 = strcmp(s2, "*") == 0 ? '*' :
	     strcmp(s2, "\\-") == 0 ? '-' :
	     strcmp(s2, "\\(bu") == 0 ? 'b' : ' ';
	return c1 != c2 ? '\0' : c1 == 'b' ? '*' : c1;
}

static int
man_IP_pre(MAN_ARGS)
{
	struct roff_node	*nn;
	const char		*list_class;
	enum htmltag		 list_elem, body_elem;
	char			 list_type;

	nn = n->type == ROFFT_BLOCK ? n : n->parent;
	list_type = list_continues(roff_node_prev(nn), nn);
	if (list_type == '\0') {
		/* Start a new list. */
		list_type = list_continues(nn, roff_node_next(nn));
		if (list_type == '\0')
			list_type = ' ';
		switch (list_type) {
		case ' ':
			list_class = "Bl-tag";
			list_elem = TAG_DL;
			break;
		case '*':
			list_class = "Bl-bullet";
			list_elem = TAG_UL;
			break;
		case '-':
			list_class = "Bl-dash";
			list_elem = TAG_UL;
			break;
		default:
			abort();
		}
	} else {
		/* Continue a list that was started earlier. */
		list_class = NULL;
		list_elem = TAG_MAX;
	}
	body_elem = list_type == ' ' ? TAG_DD : TAG_LI;

	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		if (list_elem != TAG_MAX)
			print_otag(h, list_elem, "c", list_class);
		return 1;
	case ROFFT_HEAD:
		if (body_elem == TAG_LI)
			return 0;
		print_otag_id(h, TAG_DT, NULL, n);
		break;
	case ROFFT_BODY:
		print_otag(h, body_elem, "");
		return 1;
	default:
		abort();
	}
	switch(n->tok) {
	case MAN_IP:  /* Only print the first header element. */
		if (n->child != NULL)
			print_man_node(man, n->child, h);
		break;
	case MAN_TP:  /* Only print next-line header elements. */
	case MAN_TQ:
		nn = n->child;
		while (nn != NULL && (NODE_LINE & nn->flags) == 0)
			nn = nn->next;
		while (nn != NULL) {
			print_man_node(man, nn, h);
			nn = nn->next;
		}
		break;
	default:
		abort();
	}
	return 0;
}

static int
man_OP_pre(MAN_ARGS)
{
	struct tag	*tt;

	print_text(h, "[");
	h->flags |= HTML_NOSPACE;
	tt = print_otag(h, TAG_SPAN, "c", "Op");

	if ((n = n->child) != NULL) {
		print_otag(h, TAG_B, "");
		print_text(h, n->string);
	}

	print_stagq(h, tt);

	if (n != NULL && n->next != NULL) {
		print_otag(h, TAG_I, "");
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
	print_otag(h, TAG_B, "");
	return 1;
}

static int
man_I_pre(MAN_ARGS)
{
	print_otag(h, TAG_I, "");
	return 1;
}

static int
man_in_pre(MAN_ARGS)
{
	print_otag(h, TAG_BR, "");
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
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		print_otag(h, TAG_DIV, "c", "Bd-indent");
		break;
	default:
		abort();
	}
	return 1;
}

static int
man_SY_pre(MAN_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		print_otag(h, TAG_TABLE, "c", "Nm");
		print_otag(h, TAG_TR, "");
		break;
	case ROFFT_HEAD:
		print_otag(h, TAG_TD, "");
		print_otag(h, TAG_CODE, "c", "Nm");
		break;
	case ROFFT_BODY:
		print_otag(h, TAG_TD, "");
		break;
	default:
		abort();
	}
	return 1;
}

static int
man_UR_pre(MAN_ARGS)
{
	char *cp;

	n = n->child;
	assert(n->type == ROFFT_HEAD);
	if (n->child != NULL) {
		assert(n->child->type == ROFFT_TEXT);
		if (n->tok == MAN_MT) {
			mandoc_asprintf(&cp, "mailto:%s", n->child->string);
			print_otag(h, TAG_A, "ch", "Mt", cp);
			free(cp);
		} else
			print_otag(h, TAG_A, "ch", "Lk", n->child->string);
	}

	assert(n->next->type == ROFFT_BODY);
	if (n->next->child != NULL)
		n = n->next;

	print_man_nodelist(man, n->child, h);
	return 0;
}

static int
man_abort_pre(MAN_ARGS)
{
	abort();
}
