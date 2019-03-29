/*	$Id: roff_validate.c,v 1.18 2018/12/31 09:02:37 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2017, 2018 Ingo Schwarze <schwarze@openbsd.org>
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
#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "libmandoc.h"
#include "roff_int.h"

#define	ROFF_VALID_ARGS struct roff_man *man, struct roff_node *n

typedef	void	(*roff_valid_fp)(ROFF_VALID_ARGS);

static	void	  roff_valid_br(ROFF_VALID_ARGS);
static	void	  roff_valid_fi(ROFF_VALID_ARGS);
static	void	  roff_valid_ft(ROFF_VALID_ARGS);
static	void	  roff_valid_nf(ROFF_VALID_ARGS);
static	void	  roff_valid_sp(ROFF_VALID_ARGS);

static	const roff_valid_fp roff_valids[ROFF_MAX] = {
	roff_valid_br,  /* br */
	NULL,  /* ce */
	roff_valid_fi,  /* fi */
	roff_valid_ft,  /* ft */
	NULL,  /* ll */
	NULL,  /* mc */
	roff_valid_nf,  /* nf */
	NULL,  /* po */
	NULL,  /* rj */
	roff_valid_sp,  /* sp */
	NULL,  /* ta */
	NULL,  /* ti */
};


void
roff_validate(struct roff_man *man)
{
	struct roff_node	*n;

	n = man->last;
	assert(n->tok < ROFF_MAX);
	if (roff_valids[n->tok] != NULL)
		(*roff_valids[n->tok])(man, n);
}

static void
roff_valid_br(ROFF_VALID_ARGS)
{
	struct roff_node	*np;

	if (n->next != NULL && n->next->type == ROFFT_TEXT &&
	    *n->next->string == ' ') {
		mandoc_msg(MANDOCERR_PAR_SKIP, n->line, n->pos,
		    "br before text line with leading blank");
		roff_node_delete(man, n);
		return;
	}

	if ((np = n->prev) == NULL)
		return;

	switch (np->tok) {
	case ROFF_br:
	case ROFF_sp:
	case MDOC_Pp:
		mandoc_msg(MANDOCERR_PAR_SKIP,
		    n->line, n->pos, "br after %s", roff_name[np->tok]);
		roff_node_delete(man, n);
		break;
	default:
		break;
	}
}

static void
roff_valid_fi(ROFF_VALID_ARGS)
{
	if ((n->flags & NODE_NOFILL) == 0)
		mandoc_msg(MANDOCERR_FI_SKIP, n->line, n->pos, "fi");
}

static void
roff_valid_ft(ROFF_VALID_ARGS)
{
	const char		*cp;

	if (n->child == NULL) {
		man->next = ROFF_NEXT_CHILD;
		roff_word_alloc(man, n->line, n->pos, "P");
		man->last = n;
		return;
	}

	cp = n->child->string;
	if (mandoc_font(cp, (int)strlen(cp)) != ESCAPE_ERROR)
		return;
	mandoc_msg(MANDOCERR_FT_BAD, n->line, n->pos, "ft %s", cp);
	roff_node_delete(man, n);
}

static void
roff_valid_nf(ROFF_VALID_ARGS)
{
	if (n->flags & NODE_NOFILL)
		mandoc_msg(MANDOCERR_NF_SKIP, n->line, n->pos, "nf");
}

static void
roff_valid_sp(ROFF_VALID_ARGS)
{
	struct roff_node	*np;

	if ((np = n->prev) == NULL)
		return;

	switch (np->tok) {
	case ROFF_br:
		mandoc_msg(MANDOCERR_PAR_SKIP,
		    np->line, np->pos, "br before sp");
		roff_node_delete(man, np);
		break;
	case MDOC_Pp:
		mandoc_msg(MANDOCERR_PAR_SKIP,
		    n->line, n->pos, "sp after Pp");
		roff_node_delete(man, n);
		break;
	default:
		break;
	}
}
