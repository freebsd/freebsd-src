/* $Id: mdoc_state.c,v 1.19 2022/08/19 12:59:26 schwarze Exp $ */
/*
 * Copyright (c) 2014,2015,2017,2018,2022 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdlib.h>
#include <string.h>

#if DEBUG_MEMORY
#include "mandoc_dbg.h"
#endif
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libmdoc.h"

#define STATE_ARGS  struct roff_man *mdoc, struct roff_node *n

typedef	void	(*state_handler)(STATE_ARGS);

static	void	 setsec(struct roff_node *, enum roff_sec);
static	void	 state_bl(STATE_ARGS);
static	void	 state_sh(STATE_ARGS);
static	void	 state_sm(STATE_ARGS);

static	const state_handler state_handlers[MDOC_MAX - MDOC_Dd] = {
	NULL,		/* Dd */
	NULL,		/* Dt */
	NULL,		/* Os */
	state_sh,	/* Sh */
	NULL,		/* Ss */
	NULL,		/* Pp */
	NULL,		/* D1 */
	NULL,		/* Dl */
	NULL,		/* Bd */
	NULL,		/* Ed */
	state_bl,	/* Bl */
	NULL,		/* El */
	NULL,		/* It */
	NULL,		/* Ad */
	NULL,		/* An */
	NULL,		/* Ap */
	NULL,		/* Ar */
	NULL,		/* Cd */
	NULL,		/* Cm */
	NULL,		/* Dv */
	NULL,		/* Er */
	NULL,		/* Ev */
	NULL,		/* Ex */
	NULL,		/* Fa */
	NULL,		/* Fd */
	NULL,		/* Fl */
	NULL,		/* Fn */
	NULL,		/* Ft */
	NULL,		/* Ic */
	NULL,		/* In */
	NULL,		/* Li */
	NULL,		/* Nd */
	NULL,		/* Nm */
	NULL,		/* Op */
	NULL,		/* Ot */
	NULL,		/* Pa */
	NULL,		/* Rv */
	NULL,		/* St */
	NULL,		/* Va */
	NULL,		/* Vt */
	NULL,		/* Xr */
	NULL,		/* %A */
	NULL,		/* %B */
	NULL,		/* %D */
	NULL,		/* %I */
	NULL,		/* %J */
	NULL,		/* %N */
	NULL,		/* %O */
	NULL,		/* %P */
	NULL,		/* %R */
	NULL,		/* %T */
	NULL,		/* %V */
	NULL,		/* Ac */
	NULL,		/* Ao */
	NULL,		/* Aq */
	NULL,		/* At */
	NULL,		/* Bc */
	NULL,		/* Bf */
	NULL,		/* Bo */
	NULL,		/* Bq */
	NULL,		/* Bsx */
	NULL,		/* Bx */
	NULL,		/* Db */
	NULL,		/* Dc */
	NULL,		/* Do */
	NULL,		/* Dq */
	NULL,		/* Ec */
	NULL,		/* Ef */
	NULL,		/* Em */
	NULL,		/* Eo */
	NULL,		/* Fx */
	NULL,		/* Ms */
	NULL,		/* No */
	NULL,		/* Ns */
	NULL,		/* Nx */
	NULL,		/* Ox */
	NULL,		/* Pc */
	NULL,		/* Pf */
	NULL,		/* Po */
	NULL,		/* Pq */
	NULL,		/* Qc */
	NULL,		/* Ql */
	NULL,		/* Qo */
	NULL,		/* Qq */
	NULL,		/* Re */
	NULL,		/* Rs */
	NULL,		/* Sc */
	NULL,		/* So */
	NULL,		/* Sq */
	state_sm,	/* Sm */
	NULL,		/* Sx */
	NULL,		/* Sy */
	NULL,		/* Tn */
	NULL,		/* Ux */
	NULL,		/* Xc */
	NULL,		/* Xo */
	NULL,		/* Fo */
	NULL,		/* Fc */
	NULL,		/* Oo */
	NULL,		/* Oc */
	NULL,		/* Bk */
	NULL,		/* Ek */
	NULL,		/* Bt */
	NULL,		/* Hf */
	NULL,		/* Fr */
	NULL,		/* Ud */
	NULL,		/* Lb */
	NULL,		/* Lp */
	NULL,		/* Lk */
	NULL,		/* Mt */
	NULL,		/* Brq */
	NULL,		/* Bro */
	NULL,		/* Brc */
	NULL,		/* %C */
	NULL,		/* Es */
	NULL,		/* En */
	NULL,		/* Dx */
	NULL,		/* %Q */
	NULL,		/* %U */
	NULL,		/* Ta */
	NULL,		/* Tg */
};


void
mdoc_state(struct roff_man *mdoc, struct roff_node *n)
{
	state_handler handler;

	if (n->tok == TOKEN_NONE || n->tok < ROFF_MAX)
		return;

	assert(n->tok >= MDOC_Dd && n->tok < MDOC_MAX);
	if ((mdoc_macro(n->tok)->flags & MDOC_PROLOGUE) == 0)
		mdoc->flags |= MDOC_PBODY;

	handler = state_handlers[n->tok - MDOC_Dd];
	if (*handler)
		(*handler)(mdoc, n);
}

static void
state_bl(STATE_ARGS)
{
	struct mdoc_arg	*args;
	size_t		 i;

	if (n->type != ROFFT_HEAD || n->parent->args == NULL)
		return;

	args = n->parent->args;
	for (i = 0; i < args->argc; i++) {
		switch(args->argv[i].arg) {
		case MDOC_Diag:
			n->norm->Bl.type = LIST_diag;
			return;
		case MDOC_Column:
			n->norm->Bl.type = LIST_column;
			return;
		default:
			break;
		}
	}
}

static void
setsec(struct roff_node *n, enum roff_sec sec)
{
	struct roff_node *nch;

	n->sec = sec;
	for (nch = n->child; nch != NULL; nch = nch->next)
		setsec(nch, sec);
}

/*
 * Set the section attribute for the BLOCK, HEAD, and HEAD children.
 * For other nodes, including the .Sh BODY, this is done when allocating
 * the node data structures, but for .Sh BLOCK and HEAD, the section is
 * still unknown at that time.
 */
static void
state_sh(STATE_ARGS)
{
	enum roff_sec sec;

	if (n->type != ROFFT_HEAD)
		return;

	if ((n->flags & NODE_VALID) == 0) {
		sec = n->child != NULL && n->child->type == ROFFT_TEXT &&
		    n->child->next == NULL ? mdoc_a2sec(n->child->string) :
		    SEC_CUSTOM;
		n->parent->sec = sec;
		setsec(n, sec);
	}
	if ((mdoc->lastsec = n->sec) == SEC_SYNOPSIS) {
		roff_setreg(mdoc->roff, "nS", 1, '=');
		mdoc->flags |= MDOC_SYNOPSIS;
	} else {
		roff_setreg(mdoc->roff, "nS", 0, '=');
		mdoc->flags &= ~MDOC_SYNOPSIS;
	}
}

static void
state_sm(STATE_ARGS)
{

	if (n->child == NULL)
		mdoc->flags ^= MDOC_SMOFF;
	else if ( ! strcmp(n->child->string, "on"))
		mdoc->flags &= ~MDOC_SMOFF;
	else if ( ! strcmp(n->child->string, "off"))
		mdoc->flags |= MDOC_SMOFF;
}
