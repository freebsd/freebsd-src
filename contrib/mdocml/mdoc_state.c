/*	$Id: mdoc_state.c,v 1.4 2017/01/10 13:47:00 schwarze Exp $ */
/*
 * Copyright (c) 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "libmandoc.h"
#include "libmdoc.h"

#define STATE_ARGS  struct roff_man *mdoc, struct roff_node *n

typedef	void	(*state_handler)(STATE_ARGS);

static	void	 state_bd(STATE_ARGS);
static	void	 state_bl(STATE_ARGS);
static	void	 state_dl(STATE_ARGS);
static	void	 state_sh(STATE_ARGS);
static	void	 state_sm(STATE_ARGS);

static	const state_handler state_handlers[MDOC_MAX] = {
	NULL,		/* Ap */
	NULL,		/* Dd */
	NULL,		/* Dt */
	NULL,		/* Os */
	state_sh,	/* Sh */
	NULL,		/* Ss */
	NULL,		/* Pp */
	NULL,		/* D1 */
	state_dl,	/* Dl */
	state_bd,	/* Bd */
	NULL,		/* Ed */
	state_bl,	/* Bl */
	NULL,		/* El */
	NULL,		/* It */
	NULL,		/* Ad */
	NULL,		/* An */
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
	NULL,		/* br */
	NULL,		/* sp */
	NULL,		/* %U */
	NULL,		/* Ta */
	NULL,		/* ll */
};


void
mdoc_state(struct roff_man *mdoc, struct roff_node *n)
{
	state_handler handler;

	if (n->tok == TOKEN_NONE)
		return;

	if ( ! (mdoc_macros[n->tok].flags & MDOC_PROLOGUE))
		mdoc->flags |= MDOC_PBODY;

	handler = state_handlers[n->tok];
	if (*handler)
		(*handler)(mdoc, n);
}

void
mdoc_state_reset(struct roff_man *mdoc)
{

	roff_setreg(mdoc->roff, "nS", 0, '=');
	mdoc->flags = 0;
}

static void
state_bd(STATE_ARGS)
{
	enum mdocargt arg;

	if (n->type != ROFFT_HEAD &&
	    (n->type != ROFFT_BODY || n->end != ENDBODY_NOT))
		return;

	if (n->parent->args == NULL)
		return;

	arg = n->parent->args->argv[0].arg;
	if (arg != MDOC_Literal && arg != MDOC_Unfilled)
		return;

	state_dl(mdoc, n);
}

static void
state_bl(STATE_ARGS)
{

	if (n->type != ROFFT_HEAD || n->parent->args == NULL)
		return;

	switch(n->parent->args->argv[0].arg) {
	case MDOC_Diag:
		n->norm->Bl.type = LIST_diag;
		break;
	case MDOC_Column:
		n->norm->Bl.type = LIST_column;
		break;
	default:
		break;
	}
}

static void
state_dl(STATE_ARGS)
{

	switch (n->type) {
	case ROFFT_HEAD:
		mdoc->flags |= MDOC_LITERAL;
		break;
	case ROFFT_BODY:
		mdoc->flags &= ~MDOC_LITERAL;
		break;
	default:
		break;
	}
}

static void
state_sh(STATE_ARGS)
{
	struct roff_node *nch;
	char		 *secname;

	if (n->type != ROFFT_HEAD)
		return;

	if ( ! (n->flags & NODE_VALID)) {
		secname = NULL;
		deroff(&secname, n);

		/*
		 * Set the section attribute for the BLOCK, HEAD,
		 * and HEAD children; the latter can only be TEXT
		 * nodes, so no recursion is needed.  For other
		 * nodes, including the .Sh BODY, this is done
		 * when allocating the node data structures, but
		 * for .Sh BLOCK and HEAD, the section is still
		 * unknown at that time.
		 */

		n->sec = n->parent->sec = secname == NULL ?
		    SEC_CUSTOM : mdoc_a2sec(secname);
		for (nch = n->child; nch != NULL; nch = nch->next)
			nch->sec = n->sec;
		free(secname);
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
