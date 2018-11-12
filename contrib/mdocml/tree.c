/*	$Id: tree.c,v 1.62 2015/02/05 00:14:13 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mandoc.h"
#include "mdoc.h"
#include "man.h"
#include "main.h"

static	void	print_box(const struct eqn_box *, int);
static	void	print_man(const struct man_node *, int);
static	void	print_mdoc(const struct mdoc_node *, int);
static	void	print_span(const struct tbl_span *, int);


void
tree_mdoc(void *arg, const struct mdoc *mdoc)
{

	print_mdoc(mdoc_node(mdoc)->child, 0);
}

void
tree_man(void *arg, const struct man *man)
{

	print_man(man_node(man)->child, 0);
}

static void
print_mdoc(const struct mdoc_node *n, int indent)
{
	const char	 *p, *t;
	int		  i, j;
	size_t		  argc;
	struct mdoc_argv *argv;

	if (n == NULL)
		return;

	argv = NULL;
	argc = 0;
	t = p = NULL;

	switch (n->type) {
	case MDOC_ROOT:
		t = "root";
		break;
	case MDOC_BLOCK:
		t = "block";
		break;
	case MDOC_HEAD:
		t = "block-head";
		break;
	case MDOC_BODY:
		if (n->end)
			t = "body-end";
		else
			t = "block-body";
		break;
	case MDOC_TAIL:
		t = "block-tail";
		break;
	case MDOC_ELEM:
		t = "elem";
		break;
	case MDOC_TEXT:
		t = "text";
		break;
	case MDOC_TBL:
		break;
	case MDOC_EQN:
		t = "eqn";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	switch (n->type) {
	case MDOC_TEXT:
		p = n->string;
		break;
	case MDOC_BODY:
		p = mdoc_macronames[n->tok];
		break;
	case MDOC_HEAD:
		p = mdoc_macronames[n->tok];
		break;
	case MDOC_TAIL:
		p = mdoc_macronames[n->tok];
		break;
	case MDOC_ELEM:
		p = mdoc_macronames[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case MDOC_BLOCK:
		p = mdoc_macronames[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case MDOC_TBL:
		break;
	case MDOC_EQN:
		p = "EQ";
		break;
	case MDOC_ROOT:
		p = "root";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (n->span) {
		assert(NULL == p && NULL == t);
		print_span(n->span, indent);
	} else {
		for (i = 0; i < indent; i++)
			putchar(' ');

		printf("%s (%s)", p, t);

		for (i = 0; i < (int)argc; i++) {
			printf(" -%s", mdoc_argnames[argv[i].arg]);
			if (argv[i].sz > 0)
				printf(" [");
			for (j = 0; j < (int)argv[i].sz; j++)
				printf(" [%s]", argv[i].value[j]);
			if (argv[i].sz > 0)
				printf(" ]");
		}

		putchar(' ');
		if (MDOC_LINE & n->flags)
			putchar('*');
		printf("%d:%d\n", n->line, n->pos + 1);
	}

	if (n->eqn)
		print_box(n->eqn->root->first, indent + 4);
	if (n->child)
		print_mdoc(n->child, indent +
		    (n->type == MDOC_BLOCK ? 2 : 4));
	if (n->next)
		print_mdoc(n->next, indent);
}

static void
print_man(const struct man_node *n, int indent)
{
	const char	 *p, *t;
	int		  i;

	if (n == NULL)
		return;

	t = p = NULL;

	switch (n->type) {
	case MAN_ROOT:
		t = "root";
		break;
	case MAN_ELEM:
		t = "elem";
		break;
	case MAN_TEXT:
		t = "text";
		break;
	case MAN_BLOCK:
		t = "block";
		break;
	case MAN_HEAD:
		t = "block-head";
		break;
	case MAN_BODY:
		t = "block-body";
		break;
	case MAN_TBL:
		break;
	case MAN_EQN:
		t = "eqn";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	switch (n->type) {
	case MAN_TEXT:
		p = n->string;
		break;
	case MAN_ELEM:
		/* FALLTHROUGH */
	case MAN_BLOCK:
		/* FALLTHROUGH */
	case MAN_HEAD:
		/* FALLTHROUGH */
	case MAN_BODY:
		p = man_macronames[n->tok];
		break;
	case MAN_ROOT:
		p = "root";
		break;
	case MAN_TBL:
		break;
	case MAN_EQN:
		p = "EQ";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (n->span) {
		assert(NULL == p && NULL == t);
		print_span(n->span, indent);
	} else {
		for (i = 0; i < indent; i++)
			putchar(' ');
		printf("%s (%s) ", p, t);
		if (MAN_LINE & n->flags)
			putchar('*');
		printf("%d:%d\n", n->line, n->pos + 1);
	}

	if (n->eqn)
		print_box(n->eqn->root->first, indent + 4);
	if (n->child)
		print_man(n->child, indent +
		    (n->type == MAN_BLOCK ? 2 : 4));
	if (n->next)
		print_man(n->next, indent);
}

static void
print_box(const struct eqn_box *ep, int indent)
{
	int		 i;
	const char	*t;

	static const char *posnames[] = {
	    NULL, "sup", "subsup", "sub",
	    "to", "from", "fromto",
	    "over", "sqrt", NULL };

	if (NULL == ep)
		return;
	for (i = 0; i < indent; i++)
		putchar(' ');

	t = NULL;
	switch (ep->type) {
	case EQN_ROOT:
		t = "eqn-root";
		break;
	case EQN_LISTONE:
	case EQN_LIST:
		t = "eqn-list";
		break;
	case EQN_SUBEXPR:
		t = "eqn-expr";
		break;
	case EQN_TEXT:
		t = "eqn-text";
		break;
	case EQN_PILE:
		t = "eqn-pile";
		break;
	case EQN_MATRIX:
		t = "eqn-matrix";
		break;
	}

	fputs(t, stdout);
	if (ep->pos)
		printf(" pos=%s", posnames[ep->pos]);
	if (ep->left)
		printf(" left=\"%s\"", ep->left);
	if (ep->right)
		printf(" right=\"%s\"", ep->right);
	if (ep->top)
		printf(" top=\"%s\"", ep->top);
	if (ep->bottom)
		printf(" bottom=\"%s\"", ep->bottom);
	if (ep->text)
		printf(" text=\"%s\"", ep->text);
	if (ep->font)
		printf(" font=%d", ep->font);
	if (ep->size != EQN_DEFSIZE)
		printf(" size=%d", ep->size);
	if (ep->expectargs != UINT_MAX && ep->expectargs != ep->args)
		printf(" badargs=%zu(%zu)", ep->args, ep->expectargs);
	else if (ep->args)
		printf(" args=%zu", ep->args);
	putchar('\n');

	print_box(ep->first, indent + 4);
	print_box(ep->next, indent);
}

static void
print_span(const struct tbl_span *sp, int indent)
{
	const struct tbl_dat *dp;
	int		 i;

	for (i = 0; i < indent; i++)
		putchar(' ');

	switch (sp->pos) {
	case TBL_SPAN_HORIZ:
		putchar('-');
		return;
	case TBL_SPAN_DHORIZ:
		putchar('=');
		return;
	default:
		break;
	}

	for (dp = sp->first; dp; dp = dp->next) {
		switch (dp->pos) {
		case TBL_DATA_HORIZ:
			/* FALLTHROUGH */
		case TBL_DATA_NHORIZ:
			putchar('-');
			continue;
		case TBL_DATA_DHORIZ:
			/* FALLTHROUGH */
		case TBL_DATA_NDHORIZ:
			putchar('=');
			continue;
		default:
			break;
		}
		printf("[\"%s\"", dp->string ? dp->string : "");
		if (dp->spans)
			printf("(%d)", dp->spans);
		if (NULL == dp->layout)
			putchar('*');
		putchar(']');
		putchar(' ');
	}

	printf("(tbl) %d:1\n", sp->line);
}
