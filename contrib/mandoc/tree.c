/*	$Id: tree.c,v 1.84 2019/01/01 05:56:34 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013-2015, 2017-2019 Ingo Schwarze <schwarze@openbsd.org>
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "tbl.h"
#include "eqn.h"
#include "main.h"

static	void	print_box(const struct eqn_box *, int);
static	void	print_man(const struct roff_node *, int);
static	void	print_meta(const struct roff_meta *);
static	void	print_mdoc(const struct roff_node *, int);
static	void	print_span(const struct tbl_span *, int);


void
tree_mdoc(void *arg, const struct roff_meta *mdoc)
{
	print_meta(mdoc);
	putchar('\n');
	print_mdoc(mdoc->first->child, 0);
}

void
tree_man(void *arg, const struct roff_meta *man)
{
	print_meta(man);
	if (man->hasbody == 0)
		puts("body  = empty");
	putchar('\n');
	print_man(man->first->child, 0);
}

static void
print_meta(const struct roff_meta *meta)
{
	if (meta->title != NULL)
		printf("title = \"%s\"\n", meta->title);
	if (meta->name != NULL)
		printf("name  = \"%s\"\n", meta->name);
	if (meta->msec != NULL)
		printf("sec   = \"%s\"\n", meta->msec);
	if (meta->vol != NULL)
		printf("vol   = \"%s\"\n", meta->vol);
	if (meta->arch != NULL)
		printf("arch  = \"%s\"\n", meta->arch);
	if (meta->os != NULL)
		printf("os    = \"%s\"\n", meta->os);
	if (meta->date != NULL)
		printf("date  = \"%s\"\n", meta->date);
}

static void
print_mdoc(const struct roff_node *n, int indent)
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
	case ROFFT_ROOT:
		t = "root";
		break;
	case ROFFT_BLOCK:
		t = "block";
		break;
	case ROFFT_HEAD:
		t = "head";
		break;
	case ROFFT_BODY:
		if (n->end)
			t = "body-end";
		else
			t = "body";
		break;
	case ROFFT_TAIL:
		t = "tail";
		break;
	case ROFFT_ELEM:
		t = "elem";
		break;
	case ROFFT_TEXT:
		t = "text";
		break;
	case ROFFT_COMMENT:
		t = "comment";
		break;
	case ROFFT_TBL:
		break;
	case ROFFT_EQN:
		t = "eqn";
		break;
	default:
		abort();
	}

	switch (n->type) {
	case ROFFT_TEXT:
	case ROFFT_COMMENT:
		p = n->string;
		break;
	case ROFFT_BODY:
		p = roff_name[n->tok];
		break;
	case ROFFT_HEAD:
		p = roff_name[n->tok];
		break;
	case ROFFT_TAIL:
		p = roff_name[n->tok];
		break;
	case ROFFT_ELEM:
		p = roff_name[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case ROFFT_BLOCK:
		p = roff_name[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case ROFFT_TBL:
		break;
	case ROFFT_EQN:
		p = "EQ";
		break;
	case ROFFT_ROOT:
		p = "root";
		break;
	default:
		abort();
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
		if (n->flags & NODE_DELIMO)
			putchar('(');
		if (n->flags & NODE_LINE)
			putchar('*');
		printf("%d:%d", n->line, n->pos + 1);
		if (n->flags & NODE_DELIMC)
			putchar(')');
		if (n->flags & NODE_EOS)
			putchar('.');
		if (n->flags & NODE_BROKEN)
			printf(" BROKEN");
		if (n->flags & NODE_NOFILL)
			printf(" NOFILL");
		if (n->flags & NODE_NOSRC)
			printf(" NOSRC");
		if (n->flags & NODE_NOPRT)
			printf(" NOPRT");
		putchar('\n');
	}

	if (n->eqn)
		print_box(n->eqn->first, indent + 4);
	if (n->child)
		print_mdoc(n->child, indent +
		    (n->type == ROFFT_BLOCK ? 2 : 4));
	if (n->next)
		print_mdoc(n->next, indent);
}

static void
print_man(const struct roff_node *n, int indent)
{
	const char	 *p, *t;
	int		  i;

	if (n == NULL)
		return;

	t = p = NULL;

	switch (n->type) {
	case ROFFT_ROOT:
		t = "root";
		break;
	case ROFFT_ELEM:
		t = "elem";
		break;
	case ROFFT_TEXT:
		t = "text";
		break;
	case ROFFT_COMMENT:
		t = "comment";
		break;
	case ROFFT_BLOCK:
		t = "block";
		break;
	case ROFFT_HEAD:
		t = "head";
		break;
	case ROFFT_BODY:
		t = "body";
		break;
	case ROFFT_TBL:
		break;
	case ROFFT_EQN:
		t = "eqn";
		break;
	default:
		abort();
	}

	switch (n->type) {
	case ROFFT_TEXT:
	case ROFFT_COMMENT:
		p = n->string;
		break;
	case ROFFT_ELEM:
	case ROFFT_BLOCK:
	case ROFFT_HEAD:
	case ROFFT_BODY:
		p = roff_name[n->tok];
		break;
	case ROFFT_ROOT:
		p = "root";
		break;
	case ROFFT_TBL:
		break;
	case ROFFT_EQN:
		p = "EQ";
		break;
	default:
		abort();
	}

	if (n->span) {
		assert(NULL == p && NULL == t);
		print_span(n->span, indent);
	} else {
		for (i = 0; i < indent; i++)
			putchar(' ');
		printf("%s (%s) ", p, t);
		if (n->flags & NODE_LINE)
			putchar('*');
		printf("%d:%d", n->line, n->pos + 1);
		if (n->flags & NODE_DELIMC)
			putchar(')');
		if (n->flags & NODE_EOS)
			putchar('.');
		if (n->flags & NODE_NOFILL)
			printf(" NOFILL");
		putchar('\n');
	}

	if (n->eqn)
		print_box(n->eqn->first, indent + 4);
	if (n->child)
		print_man(n->child, indent +
		    (n->type == ROFFT_BLOCK ? 2 : 4));
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
		putchar(' ');
		break;
	case TBL_SPAN_DHORIZ:
		putchar('=');
		putchar(' ');
		break;
	default:
		for (dp = sp->first; dp; dp = dp->next) {
			switch (dp->pos) {
			case TBL_DATA_HORIZ:
			case TBL_DATA_NHORIZ:
				putchar('-');
				putchar(' ');
				continue;
			case TBL_DATA_DHORIZ:
			case TBL_DATA_NDHORIZ:
				putchar('=');
				putchar(' ');
				continue;
			default:
				break;
			}
			printf("[\"%s\"", dp->string ? dp->string : "");
			if (dp->hspans)
				printf(">%d", dp->hspans);
			if (dp->vspans)
				printf("v%d", dp->vspans);
			if (dp->layout == NULL)
				putchar('*');
			else if (dp->layout->pos == TBL_CELL_DOWN)
				putchar('^');
			putchar(']');
			putchar(' ');
		}
		break;
	}
	printf("(tbl) %d:1\n", sp->line);
}
