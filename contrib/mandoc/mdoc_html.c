/* $Id: mdoc_html.c,v 1.342 2021/03/30 19:26:20 schwarze Exp $ */
/*
 * Copyright (c) 2014-2021 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * HTML formatter for mdoc(7) used by mandoc(1).
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "out.h"
#include "html.h"
#include "main.h"

#define	MDOC_ARGS	  const struct roff_meta *meta, \
			  struct roff_node *n, \
			  struct html *h

#ifndef MIN
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif

struct	mdoc_html_act {
	int		(*pre)(MDOC_ARGS);
	void		(*post)(MDOC_ARGS);
};

static	void		  print_mdoc_head(const struct roff_meta *,
				struct html *);
static	void		  print_mdoc_node(MDOC_ARGS);
static	void		  print_mdoc_nodelist(MDOC_ARGS);
static	void		  synopsis_pre(struct html *, struct roff_node *);

static	void		  mdoc_root_post(const struct roff_meta *,
				struct html *);
static	int		  mdoc_root_pre(const struct roff_meta *,
				struct html *);

static	void		  mdoc__x_post(MDOC_ARGS);
static	int		  mdoc__x_pre(MDOC_ARGS);
static	int		  mdoc_abort_pre(MDOC_ARGS);
static	int		  mdoc_ad_pre(MDOC_ARGS);
static	int		  mdoc_an_pre(MDOC_ARGS);
static	int		  mdoc_ap_pre(MDOC_ARGS);
static	int		  mdoc_ar_pre(MDOC_ARGS);
static	int		  mdoc_bd_pre(MDOC_ARGS);
static	int		  mdoc_bf_pre(MDOC_ARGS);
static	void		  mdoc_bk_post(MDOC_ARGS);
static	int		  mdoc_bk_pre(MDOC_ARGS);
static	int		  mdoc_bl_pre(MDOC_ARGS);
static	int		  mdoc_cd_pre(MDOC_ARGS);
static	int		  mdoc_code_pre(MDOC_ARGS);
static	int		  mdoc_d1_pre(MDOC_ARGS);
static	int		  mdoc_fa_pre(MDOC_ARGS);
static	int		  mdoc_fd_pre(MDOC_ARGS);
static	int		  mdoc_fl_pre(MDOC_ARGS);
static	int		  mdoc_fn_pre(MDOC_ARGS);
static	int		  mdoc_ft_pre(MDOC_ARGS);
static	int		  mdoc_em_pre(MDOC_ARGS);
static	void		  mdoc_eo_post(MDOC_ARGS);
static	int		  mdoc_eo_pre(MDOC_ARGS);
static	int		  mdoc_ex_pre(MDOC_ARGS);
static	void		  mdoc_fo_post(MDOC_ARGS);
static	int		  mdoc_fo_pre(MDOC_ARGS);
static	int		  mdoc_igndelim_pre(MDOC_ARGS);
static	int		  mdoc_in_pre(MDOC_ARGS);
static	int		  mdoc_it_pre(MDOC_ARGS);
static	int		  mdoc_lb_pre(MDOC_ARGS);
static	int		  mdoc_lk_pre(MDOC_ARGS);
static	int		  mdoc_mt_pre(MDOC_ARGS);
static	int		  mdoc_nd_pre(MDOC_ARGS);
static	int		  mdoc_nm_pre(MDOC_ARGS);
static	int		  mdoc_no_pre(MDOC_ARGS);
static	int		  mdoc_ns_pre(MDOC_ARGS);
static	int		  mdoc_pa_pre(MDOC_ARGS);
static	void		  mdoc_pf_post(MDOC_ARGS);
static	int		  mdoc_pp_pre(MDOC_ARGS);
static	void		  mdoc_quote_post(MDOC_ARGS);
static	int		  mdoc_quote_pre(MDOC_ARGS);
static	int		  mdoc_rs_pre(MDOC_ARGS);
static	int		  mdoc_sh_pre(MDOC_ARGS);
static	int		  mdoc_skip_pre(MDOC_ARGS);
static	int		  mdoc_sm_pre(MDOC_ARGS);
static	int		  mdoc_ss_pre(MDOC_ARGS);
static	int		  mdoc_st_pre(MDOC_ARGS);
static	int		  mdoc_sx_pre(MDOC_ARGS);
static	int		  mdoc_sy_pre(MDOC_ARGS);
static	int		  mdoc_tg_pre(MDOC_ARGS);
static	int		  mdoc_va_pre(MDOC_ARGS);
static	int		  mdoc_vt_pre(MDOC_ARGS);
static	int		  mdoc_xr_pre(MDOC_ARGS);
static	int		  mdoc_xx_pre(MDOC_ARGS);

static const struct mdoc_html_act mdoc_html_acts[MDOC_MAX - MDOC_Dd] = {
	{NULL, NULL}, /* Dd */
	{NULL, NULL}, /* Dt */
	{NULL, NULL}, /* Os */
	{mdoc_sh_pre, NULL }, /* Sh */
	{mdoc_ss_pre, NULL }, /* Ss */
	{mdoc_pp_pre, NULL}, /* Pp */
	{mdoc_d1_pre, NULL}, /* D1 */
	{mdoc_d1_pre, NULL}, /* Dl */
	{mdoc_bd_pre, NULL}, /* Bd */
	{NULL, NULL}, /* Ed */
	{mdoc_bl_pre, NULL}, /* Bl */
	{NULL, NULL}, /* El */
	{mdoc_it_pre, NULL}, /* It */
	{mdoc_ad_pre, NULL}, /* Ad */
	{mdoc_an_pre, NULL}, /* An */
	{mdoc_ap_pre, NULL}, /* Ap */
	{mdoc_ar_pre, NULL}, /* Ar */
	{mdoc_cd_pre, NULL}, /* Cd */
	{mdoc_code_pre, NULL}, /* Cm */
	{mdoc_code_pre, NULL}, /* Dv */
	{mdoc_code_pre, NULL}, /* Er */
	{mdoc_code_pre, NULL}, /* Ev */
	{mdoc_ex_pre, NULL}, /* Ex */
	{mdoc_fa_pre, NULL}, /* Fa */
	{mdoc_fd_pre, NULL}, /* Fd */
	{mdoc_fl_pre, NULL}, /* Fl */
	{mdoc_fn_pre, NULL}, /* Fn */
	{mdoc_ft_pre, NULL}, /* Ft */
	{mdoc_code_pre, NULL}, /* Ic */
	{mdoc_in_pre, NULL}, /* In */
	{mdoc_code_pre, NULL}, /* Li */
	{mdoc_nd_pre, NULL}, /* Nd */
	{mdoc_nm_pre, NULL}, /* Nm */
	{mdoc_quote_pre, mdoc_quote_post}, /* Op */
	{mdoc_abort_pre, NULL}, /* Ot */
	{mdoc_pa_pre, NULL}, /* Pa */
	{mdoc_ex_pre, NULL}, /* Rv */
	{mdoc_st_pre, NULL}, /* St */
	{mdoc_va_pre, NULL}, /* Va */
	{mdoc_vt_pre, NULL}, /* Vt */
	{mdoc_xr_pre, NULL}, /* Xr */
	{mdoc__x_pre, mdoc__x_post}, /* %A */
	{mdoc__x_pre, mdoc__x_post}, /* %B */
	{mdoc__x_pre, mdoc__x_post}, /* %D */
	{mdoc__x_pre, mdoc__x_post}, /* %I */
	{mdoc__x_pre, mdoc__x_post}, /* %J */
	{mdoc__x_pre, mdoc__x_post}, /* %N */
	{mdoc__x_pre, mdoc__x_post}, /* %O */
	{mdoc__x_pre, mdoc__x_post}, /* %P */
	{mdoc__x_pre, mdoc__x_post}, /* %R */
	{mdoc__x_pre, mdoc__x_post}, /* %T */
	{mdoc__x_pre, mdoc__x_post}, /* %V */
	{NULL, NULL}, /* Ac */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ao */
	{mdoc_quote_pre, mdoc_quote_post}, /* Aq */
	{mdoc_xx_pre, NULL}, /* At */
	{NULL, NULL}, /* Bc */
	{mdoc_bf_pre, NULL}, /* Bf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bq */
	{mdoc_xx_pre, NULL}, /* Bsx */
	{mdoc_xx_pre, NULL}, /* Bx */
	{mdoc_skip_pre, NULL}, /* Db */
	{NULL, NULL}, /* Dc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Do */
	{mdoc_quote_pre, mdoc_quote_post}, /* Dq */
	{NULL, NULL}, /* Ec */ /* FIXME: no space */
	{NULL, NULL}, /* Ef */
	{mdoc_em_pre, NULL}, /* Em */
	{mdoc_eo_pre, mdoc_eo_post}, /* Eo */
	{mdoc_xx_pre, NULL}, /* Fx */
	{mdoc_no_pre, NULL}, /* Ms */
	{mdoc_no_pre, NULL}, /* No */
	{mdoc_ns_pre, NULL}, /* Ns */
	{mdoc_xx_pre, NULL}, /* Nx */
	{mdoc_xx_pre, NULL}, /* Ox */
	{NULL, NULL}, /* Pc */
	{mdoc_igndelim_pre, mdoc_pf_post}, /* Pf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Po */
	{mdoc_quote_pre, mdoc_quote_post}, /* Pq */
	{NULL, NULL}, /* Qc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ql */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qq */
	{NULL, NULL}, /* Re */
	{mdoc_rs_pre, NULL}, /* Rs */
	{NULL, NULL}, /* Sc */
	{mdoc_quote_pre, mdoc_quote_post}, /* So */
	{mdoc_quote_pre, mdoc_quote_post}, /* Sq */
	{mdoc_sm_pre, NULL}, /* Sm */
	{mdoc_sx_pre, NULL}, /* Sx */
	{mdoc_sy_pre, NULL}, /* Sy */
	{NULL, NULL}, /* Tn */
	{mdoc_xx_pre, NULL}, /* Ux */
	{NULL, NULL}, /* Xc */
	{NULL, NULL}, /* Xo */
	{mdoc_fo_pre, mdoc_fo_post}, /* Fo */
	{NULL, NULL}, /* Fc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Oo */
	{NULL, NULL}, /* Oc */
	{mdoc_bk_pre, mdoc_bk_post}, /* Bk */
	{NULL, NULL}, /* Ek */
	{NULL, NULL}, /* Bt */
	{NULL, NULL}, /* Hf */
	{mdoc_em_pre, NULL}, /* Fr */
	{NULL, NULL}, /* Ud */
	{mdoc_lb_pre, NULL}, /* Lb */
	{mdoc_abort_pre, NULL}, /* Lp */
	{mdoc_lk_pre, NULL}, /* Lk */
	{mdoc_mt_pre, NULL}, /* Mt */
	{mdoc_quote_pre, mdoc_quote_post}, /* Brq */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bro */
	{NULL, NULL}, /* Brc */
	{mdoc__x_pre, mdoc__x_post}, /* %C */
	{mdoc_skip_pre, NULL}, /* Es */
	{mdoc_quote_pre, mdoc_quote_post}, /* En */
	{mdoc_xx_pre, NULL}, /* Dx */
	{mdoc__x_pre, mdoc__x_post}, /* %Q */
	{mdoc__x_pre, mdoc__x_post}, /* %U */
	{NULL, NULL}, /* Ta */
	{mdoc_tg_pre, NULL}, /* Tg */
};


/*
 * See the same function in mdoc_term.c for documentation.
 */
static void
synopsis_pre(struct html *h, struct roff_node *n)
{
	struct roff_node *np;

	if ((n->flags & NODE_SYNPRETTY) == 0 ||
	    (np = roff_node_prev(n)) == NULL)
		return;

	if (np->tok == n->tok &&
	    MDOC_Fo != n->tok &&
	    MDOC_Ft != n->tok &&
	    MDOC_Fn != n->tok) {
		print_otag(h, TAG_BR, "");
		return;
	}

	switch (np->tok) {
	case MDOC_Fd:
	case MDOC_Fn:
	case MDOC_Fo:
	case MDOC_In:
	case MDOC_Vt:
		break;
	case MDOC_Ft:
		if (n->tok != MDOC_Fn && n->tok != MDOC_Fo)
			break;
		/* FALLTHROUGH */
	default:
		print_otag(h, TAG_BR, "");
		return;
	}
	html_close_paragraph(h);
	print_otag(h, TAG_P, "c", "Pp");
}

void
html_mdoc(void *arg, const struct roff_meta *mdoc)
{
	struct html		*h;
	struct roff_node	*n;
	struct tag		*t;

	h = (struct html *)arg;
	n = mdoc->first->child;

	if ((h->oflags & HTML_FRAGMENT) == 0) {
		print_gen_decls(h);
		print_otag(h, TAG_HTML, "");
		if (n != NULL && n->type == ROFFT_COMMENT)
			print_gen_comment(h, n);
		t = print_otag(h, TAG_HEAD, "");
		print_mdoc_head(mdoc, h);
		print_tagq(h, t);
		print_otag(h, TAG_BODY, "");
	}

	mdoc_root_pre(mdoc, h);
	t = print_otag(h, TAG_DIV, "c", "manual-text");
	print_mdoc_nodelist(mdoc, n, h);
	print_tagq(h, t);
	mdoc_root_post(mdoc, h);
	print_tagq(h, NULL);
}

static void
print_mdoc_head(const struct roff_meta *meta, struct html *h)
{
	char	*cp;

	print_gen_head(h);

	if (meta->arch != NULL && meta->msec != NULL)
		mandoc_asprintf(&cp, "%s(%s) (%s)", meta->title,
		    meta->msec, meta->arch);
	else if (meta->msec != NULL)
		mandoc_asprintf(&cp, "%s(%s)", meta->title, meta->msec);
	else if (meta->arch != NULL)
		mandoc_asprintf(&cp, "%s (%s)", meta->title, meta->arch);
	else
		cp = mandoc_strdup(meta->title);

	print_otag(h, TAG_TITLE, "");
	print_text(h, cp);
	free(cp);
}

static void
print_mdoc_nodelist(MDOC_ARGS)
{

	while (n != NULL) {
		print_mdoc_node(meta, n, h);
		n = n->next;
	}
}

static void
print_mdoc_node(MDOC_ARGS)
{
	struct tag	*t;
	int		 child;

	if (n->type == ROFFT_COMMENT || n->flags & NODE_NOPRT)
		return;

	if ((n->flags & NODE_NOFILL) == 0)
		html_fillmode(h, ROFF_fi);
	else if (html_fillmode(h, ROFF_nf) == ROFF_nf &&
	    n->tok != ROFF_fi && n->flags & NODE_LINE)
		print_endline(h);

	child = 1;
	n->flags &= ~NODE_ENDED;
	switch (n->type) {
	case ROFFT_TEXT:
		if (n->flags & NODE_LINE) {
			switch (*n->string) {
			case '\0':
				h->col = 1;
				print_endline(h);
				return;
			case ' ':
				if ((h->flags & HTML_NONEWLINE) == 0 &&
				    (n->flags & NODE_NOFILL) == 0)
					print_otag(h, TAG_BR, "");
				break;
			default:
				break;
			}
		}
		t = h->tag;
		t->refcnt++;
		if (n->flags & NODE_DELIMC)
			h->flags |= HTML_NOSPACE;
		if (n->flags & NODE_HREF)
			print_tagged_text(h, n->string, n);
		else
			print_text(h, n->string);
		if (n->flags & NODE_DELIMO)
			h->flags |= HTML_NOSPACE;
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
		 * Close out the current table, if it's open, and unset
		 * the "meta" table state.  This will be reopened on the
		 * next table element.
		 */
		if (h->tblt != NULL)
			print_tblclose(h);
		assert(h->tblt == NULL);
		t = h->tag;
		t->refcnt++;
		if (n->tok < ROFF_MAX) {
			roff_html_pre(h, n);
			t->refcnt--;
			print_stagq(h, t);
			return;
		}
		assert(n->tok >= MDOC_Dd && n->tok < MDOC_MAX);
		if (mdoc_html_acts[n->tok - MDOC_Dd].pre != NULL &&
		    (n->end == ENDBODY_NOT || n->child != NULL))
			child = (*mdoc_html_acts[n->tok - MDOC_Dd].pre)(meta,
			    n, h);
		break;
	}

	if (h->flags & HTML_KEEP && n->flags & NODE_LINE) {
		h->flags &= ~HTML_KEEP;
		h->flags |= HTML_PREKEEP;
	}

	if (child && n->child != NULL)
		print_mdoc_nodelist(meta, n->child, h);

	t->refcnt--;
	print_stagq(h, t);

	switch (n->type) {
	case ROFFT_TEXT:
	case ROFFT_EQN:
		break;
	default:
		if (mdoc_html_acts[n->tok - MDOC_Dd].post == NULL ||
		    n->flags & NODE_ENDED)
			break;
		(*mdoc_html_acts[n->tok - MDOC_Dd].post)(meta, n, h);
		if (n->end != ENDBODY_NOT)
			n->body->flags |= NODE_ENDED;
		break;
	}
}

static void
mdoc_root_post(const struct roff_meta *meta, struct html *h)
{
	struct tag	*t, *tt;

	t = print_otag(h, TAG_TABLE, "c", "foot");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "foot-date");
	print_text(h, meta->date);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "foot-os");
	print_text(h, meta->os);
	print_tagq(h, t);
}

static int
mdoc_root_pre(const struct roff_meta *meta, struct html *h)
{
	struct tag	*t, *tt;
	char		*volume, *title;

	if (NULL == meta->arch)
		volume = mandoc_strdup(meta->vol);
	else
		mandoc_asprintf(&volume, "%s (%s)",
		    meta->vol, meta->arch);

	if (NULL == meta->msec)
		title = mandoc_strdup(meta->title);
	else
		mandoc_asprintf(&title, "%s(%s)",
		    meta->title, meta->msec);

	t = print_otag(h, TAG_TABLE, "c", "head");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "head-ltitle");
	print_text(h, title);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-vol");
	print_text(h, volume);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-rtitle");
	print_text(h, title);
	print_tagq(h, t);

	free(title);
	free(volume);
	return 1;
}

static int
mdoc_code_pre(MDOC_ARGS)
{
	print_otag_id(h, TAG_CODE, roff_name[n->tok], n);
	return 1;
}

static int
mdoc_sh_pre(MDOC_ARGS)
{
	struct roff_node	*sn, *subn;
	struct tag		*t, *tsec, *tsub;
	char			*id;
	int			 sc;

	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		if ((h->oflags & HTML_TOC) == 0 ||
		    h->flags & HTML_TOCDONE ||
		    n->sec <= SEC_SYNOPSIS) {
			print_otag(h, TAG_SECTION, "c", "Sh");
			break;
		}
		h->flags |= HTML_TOCDONE;
		sc = 0;
		for (sn = n->next; sn != NULL; sn = sn->next)
			if (sn->sec == SEC_CUSTOM)
				if (++sc == 2)
					break;
		if (sc < 2)
			break;
		t = print_otag(h, TAG_H1, "c", "Sh");
		print_text(h, "TABLE OF CONTENTS");
		print_tagq(h, t);
		t = print_otag(h, TAG_UL, "c", "Bl-compact");
		for (sn = n; sn != NULL; sn = sn->next) {
			tsec = print_otag(h, TAG_LI, "");
			id = html_make_id(sn->head, 0);
			tsub = print_otag(h, TAG_A, "hR", id);
			free(id);
			print_mdoc_nodelist(meta, sn->head->child, h);
			print_tagq(h, tsub);
			tsub = NULL;
			for (subn = sn->body->child; subn != NULL;
			    subn = subn->next) {
				if (subn->tok != MDOC_Ss)
					continue;
				id = html_make_id(subn->head, 0);
				if (id == NULL)
					continue;
				if (tsub == NULL)
					print_otag(h, TAG_UL,
					    "c", "Bl-compact");
				tsub = print_otag(h, TAG_LI, "");
				print_otag(h, TAG_A, "hR", id);
				free(id);
				print_mdoc_nodelist(meta,
				    subn->head->child, h);
				print_tagq(h, tsub);
			}
			print_tagq(h, tsec);
		}
		print_tagq(h, t);
		print_otag(h, TAG_SECTION, "c", "Sh");
		break;
	case ROFFT_HEAD:
		print_otag_id(h, TAG_H1, "Sh", n);
		break;
	case ROFFT_BODY:
		if (n->sec == SEC_AUTHORS)
			h->flags &= ~(HTML_SPLIT|HTML_NOSPLIT);
		break;
	default:
		break;
	}
	return 1;
}

static int
mdoc_ss_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		print_otag(h, TAG_SECTION, "c", "Ss");
		break;
	case ROFFT_HEAD:
		print_otag_id(h, TAG_H2, "Ss", n);
		break;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}
	return 1;
}

static int
mdoc_fl_pre(MDOC_ARGS)
{
	struct roff_node	*nn;

	print_otag_id(h, TAG_CODE, "Fl", n);
	print_text(h, "\\-");
	if (n->child != NULL ||
	    ((nn = roff_node_next(n)) != NULL &&
	     nn->type != ROFFT_TEXT &&
	     (nn->flags & NODE_LINE) == 0))
		h->flags |= HTML_NOSPACE;

	return 1;
}

static int
mdoc_nd_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}
	print_text(h, "\\(em");
	print_otag(h, TAG_SPAN, "c", "Nd");
	return 1;
}

static int
mdoc_nm_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		print_otag(h, TAG_TD, "");
		/* FALLTHROUGH */
	case ROFFT_ELEM:
		print_otag(h, TAG_CODE, "c", "Nm");
		return 1;
	case ROFFT_BODY:
		print_otag(h, TAG_TD, "");
		return 1;
	default:
		abort();
	}
	html_close_paragraph(h);
	synopsis_pre(h, n);
	print_otag(h, TAG_TABLE, "c", "Nm");
	print_otag(h, TAG_TR, "");
	return 1;
}

static int
mdoc_xr_pre(MDOC_ARGS)
{
	if (NULL == n->child)
		return 0;

	if (h->base_man1)
		print_otag(h, TAG_A, "chM", "Xr",
		    n->child->string, n->child->next == NULL ?
		    NULL : n->child->next->string);
	else
		print_otag(h, TAG_A, "c", "Xr");

	n = n->child;
	print_text(h, n->string);

	if (NULL == (n = n->next))
		return 0;

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;
	print_text(h, n->string);
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	return 0;
}

static int
mdoc_tg_pre(MDOC_ARGS)
{
	char	*id;

	if ((id = html_make_id(n, 1)) != NULL) {
		print_tagq(h, print_otag(h, TAG_MARK, "i", id));
		free(id);
	}
	return 0;
}

static int
mdoc_ns_pre(MDOC_ARGS)
{

	if ( ! (NODE_LINE & n->flags))
		h->flags |= HTML_NOSPACE;
	return 1;
}

static int
mdoc_ar_pre(MDOC_ARGS)
{
	print_otag(h, TAG_VAR, "c", "Ar");
	return 1;
}

static int
mdoc_xx_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "Ux");
	return 1;
}

static int
mdoc_it_pre(MDOC_ARGS)
{
	const struct roff_node	*bl;
	enum mdoc_list		 type;

	bl = n->parent;
	while (bl->tok != MDOC_Bl)
		bl = bl->parent;
	type = bl->norm->Bl.type;

	switch (type) {
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
	case LIST_item:
	case LIST_enum:
		switch (n->type) {
		case ROFFT_HEAD:
			return 0;
		case ROFFT_BODY:
			print_otag_id(h, TAG_LI, NULL, n);
			break;
		default:
			break;
		}
		break;
	case LIST_diag:
	case LIST_hang:
	case LIST_inset:
	case LIST_ohang:
		switch (n->type) {
		case ROFFT_HEAD:
			print_otag_id(h, TAG_DT, NULL, n);
			break;
		case ROFFT_BODY:
			print_otag(h, TAG_DD, "");
			break;
		default:
			break;
		}
		break;
	case LIST_tag:
		switch (n->type) {
		case ROFFT_HEAD:
			print_otag_id(h, TAG_DT, NULL, n);
			break;
		case ROFFT_BODY:
			if (n->child == NULL) {
				print_otag(h, TAG_DD, "s", "width", "auto");
				print_text(h, "\\ ");
			} else
				print_otag(h, TAG_DD, "");
			break;
		default:
			break;
		}
		break;
	case LIST_column:
		switch (n->type) {
		case ROFFT_HEAD:
			break;
		case ROFFT_BODY:
			print_otag(h, TAG_TD, "");
			break;
		default:
			print_otag_id(h, TAG_TR, NULL, n);
		}
	default:
		break;
	}

	return 1;
}

static int
mdoc_bl_pre(MDOC_ARGS)
{
	char		 cattr[32];
	struct mdoc_bl	*bl;
	enum htmltag	 elemtype;

	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		return 1;
	default:
		abort();
	}

	bl = &n->norm->Bl;
	switch (bl->type) {
	case LIST_bullet:
		elemtype = TAG_UL;
		(void)strlcpy(cattr, "Bl-bullet", sizeof(cattr));
		break;
	case LIST_dash:
	case LIST_hyphen:
		elemtype = TAG_UL;
		(void)strlcpy(cattr, "Bl-dash", sizeof(cattr));
		break;
	case LIST_item:
		elemtype = TAG_UL;
		(void)strlcpy(cattr, "Bl-item", sizeof(cattr));
		break;
	case LIST_enum:
		elemtype = TAG_OL;
		(void)strlcpy(cattr, "Bl-enum", sizeof(cattr));
		break;
	case LIST_diag:
		elemtype = TAG_DL;
		(void)strlcpy(cattr, "Bl-diag", sizeof(cattr));
		break;
	case LIST_hang:
		elemtype = TAG_DL;
		(void)strlcpy(cattr, "Bl-hang", sizeof(cattr));
		break;
	case LIST_inset:
		elemtype = TAG_DL;
		(void)strlcpy(cattr, "Bl-inset", sizeof(cattr));
		break;
	case LIST_ohang:
		elemtype = TAG_DL;
		(void)strlcpy(cattr, "Bl-ohang", sizeof(cattr));
		break;
	case LIST_tag:
		if (bl->offs)
			print_otag(h, TAG_DIV, "c", "Bd-indent");
		print_otag_id(h, TAG_DL,
		    bl->comp ? "Bl-tag Bl-compact" : "Bl-tag", n->body);
		return 1;
	case LIST_column:
		elemtype = TAG_TABLE;
		(void)strlcpy(cattr, "Bl-column", sizeof(cattr));
		break;
	default:
		abort();
	}
	if (bl->offs != NULL)
		(void)strlcat(cattr, " Bd-indent", sizeof(cattr));
	if (bl->comp)
		(void)strlcat(cattr, " Bl-compact", sizeof(cattr));
	print_otag_id(h, elemtype, cattr, n->body);
	return 1;
}

static int
mdoc_ex_pre(MDOC_ARGS)
{
	if (roff_node_prev(n) != NULL)
		print_otag(h, TAG_BR, "");
	return 1;
}

static int
mdoc_st_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "St");
	return 1;
}

static int
mdoc_em_pre(MDOC_ARGS)
{
	print_otag_id(h, TAG_I, "Em", n);
	return 1;
}

static int
mdoc_d1_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}
	print_otag_id(h, TAG_DIV, "Bd Bd-indent", n);
	if (n->tok == MDOC_Dl)
		print_otag(h, TAG_CODE, "c", "Li");
	return 1;
}

static int
mdoc_sx_pre(MDOC_ARGS)
{
	char	*id;

	id = html_make_id(n, 0);
	print_otag(h, TAG_A, "chR", "Sx", id);
	free(id);
	return 1;
}

static int
mdoc_bd_pre(MDOC_ARGS)
{
	char			 buf[20];
	struct roff_node	*nn;
	int			 comp;

	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}

	/* Handle preceding whitespace. */

	comp = n->norm->Bd.comp;
	for (nn = n; nn != NULL && comp == 0; nn = nn->parent) {
		if (nn->type != ROFFT_BLOCK)
			continue;
		if (nn->tok == MDOC_Sh || nn->tok == MDOC_Ss)
			comp = 1;
		if (roff_node_prev(nn) != NULL)
			break;
	}
	(void)strlcpy(buf, "Bd", sizeof(buf));
	if (comp == 0)
		(void)strlcat(buf, " Pp", sizeof(buf));

	/* Handle the -offset argument. */

	if (n->norm->Bd.offs != NULL &&
	    strcmp(n->norm->Bd.offs, "left") != 0)
		(void)strlcat(buf, " Bd-indent", sizeof(buf));

	if (n->norm->Bd.type == DISP_literal)
		(void)strlcat(buf, " Li", sizeof(buf));

	print_otag_id(h, TAG_DIV, buf, n);
	return 1;
}

static int
mdoc_pa_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "Pa");
	return 1;
}

static int
mdoc_ad_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "Ad");
	return 1;
}

static int
mdoc_an_pre(MDOC_ARGS)
{
	if (n->norm->An.auth == AUTH_split) {
		h->flags &= ~HTML_NOSPLIT;
		h->flags |= HTML_SPLIT;
		return 0;
	}
	if (n->norm->An.auth == AUTH_nosplit) {
		h->flags &= ~HTML_SPLIT;
		h->flags |= HTML_NOSPLIT;
		return 0;
	}

	if (h->flags & HTML_SPLIT)
		print_otag(h, TAG_BR, "");

	if (n->sec == SEC_AUTHORS && ! (h->flags & HTML_NOSPLIT))
		h->flags |= HTML_SPLIT;

	print_otag(h, TAG_SPAN, "c", "An");
	return 1;
}

static int
mdoc_cd_pre(MDOC_ARGS)
{
	synopsis_pre(h, n);
	print_otag(h, TAG_CODE, "c", "Cd");
	return 1;
}

static int
mdoc_fa_pre(MDOC_ARGS)
{
	const struct roff_node	*nn;
	struct tag		*t;

	if (n->parent->tok != MDOC_Fo) {
		print_otag(h, TAG_VAR, "c", "Fa");
		return 1;
	}
	for (nn = n->child; nn != NULL; nn = nn->next) {
		t = print_otag(h, TAG_VAR, "c", "Fa");
		print_text(h, nn->string);
		print_tagq(h, t);
		if (nn->next != NULL) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}
	}
	if (n->child != NULL &&
	    (nn = roff_node_next(n)) != NULL &&
	    nn->tok == MDOC_Fa) {
		h->flags |= HTML_NOSPACE;
		print_text(h, ",");
	}
	return 0;
}

static int
mdoc_fd_pre(MDOC_ARGS)
{
	struct tag	*t;
	char		*buf, *cp;

	synopsis_pre(h, n);

	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);

	if (strcmp(n->string, "#include")) {
		print_otag(h, TAG_CODE, "c", "Fd");
		return 1;
	}

	print_otag(h, TAG_CODE, "c", "In");
	print_text(h, n->string);

	if (NULL != (n = n->next)) {
		assert(n->type == ROFFT_TEXT);

		if (h->base_includes) {
			cp = n->string;
			if (*cp == '<' || *cp == '"')
				cp++;
			buf = mandoc_strdup(cp);
			cp = strchr(buf, '\0') - 1;
			if (cp >= buf && (*cp == '>' || *cp == '"'))
				*cp = '\0';
			t = print_otag(h, TAG_A, "chI", "In", buf);
			free(buf);
		} else
			t = print_otag(h, TAG_A, "c", "In");

		print_text(h, n->string);
		print_tagq(h, t);

		n = n->next;
	}

	for ( ; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		print_text(h, n->string);
	}

	return 0;
}

static int
mdoc_vt_pre(MDOC_ARGS)
{
	if (n->type == ROFFT_BLOCK) {
		synopsis_pre(h, n);
		return 1;
	} else if (n->type == ROFFT_ELEM) {
		synopsis_pre(h, n);
	} else if (n->type == ROFFT_HEAD)
		return 0;

	print_otag(h, TAG_VAR, "c", "Vt");
	return 1;
}

static int
mdoc_ft_pre(MDOC_ARGS)
{
	synopsis_pre(h, n);
	print_otag(h, TAG_VAR, "c", "Ft");
	return 1;
}

static int
mdoc_fn_pre(MDOC_ARGS)
{
	struct tag	*t;
	char		 nbuf[BUFSIZ];
	const char	*sp, *ep;
	int		 sz, pretty;

	pretty = NODE_SYNPRETTY & n->flags;
	synopsis_pre(h, n);

	/* Split apart into type and name. */
	assert(n->child->string);
	sp = n->child->string;

	ep = strchr(sp, ' ');
	if (NULL != ep) {
		t = print_otag(h, TAG_VAR, "c", "Ft");

		while (ep) {
			sz = MIN((int)(ep - sp), BUFSIZ - 1);
			(void)memcpy(nbuf, sp, (size_t)sz);
			nbuf[sz] = '\0';
			print_text(h, nbuf);
			sp = ++ep;
			ep = strchr(sp, ' ');
		}
		print_tagq(h, t);
	}

	t = print_otag_id(h, TAG_CODE, "Fn", n);

	if (sp)
		print_text(h, sp);

	print_tagq(h, t);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;

	for (n = n->child->next; n; n = n->next) {
		if (NODE_SYNPRETTY & n->flags)
			t = print_otag(h, TAG_VAR, "cs", "Fa",
			    "white-space", "nowrap");
		else
			t = print_otag(h, TAG_VAR, "c", "Fa");
		print_text(h, n->string);
		print_tagq(h, t);
		if (n->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}
	}

	h->flags |= HTML_NOSPACE;
	print_text(h, ")");

	if (pretty) {
		h->flags |= HTML_NOSPACE;
		print_text(h, ";");
	}

	return 0;
}

static int
mdoc_sm_pre(MDOC_ARGS)
{

	if (NULL == n->child)
		h->flags ^= HTML_NONOSPACE;
	else if (0 == strcmp("on", n->child->string))
		h->flags &= ~HTML_NONOSPACE;
	else
		h->flags |= HTML_NONOSPACE;

	if ( ! (HTML_NONOSPACE & h->flags))
		h->flags &= ~HTML_NOSPACE;

	return 0;
}

static int
mdoc_skip_pre(MDOC_ARGS)
{

	return 0;
}

static int
mdoc_pp_pre(MDOC_ARGS)
{
	char	*id;

	if (n->flags & NODE_NOFILL) {
		print_endline(h);
		if (n->flags & NODE_ID)
			mdoc_tg_pre(meta, n, h);
		else {
			h->col = 1;
			print_endline(h);
		}
	} else {
		html_close_paragraph(h);
		id = n->flags & NODE_ID ? html_make_id(n, 1) : NULL;
		print_otag(h, TAG_P, "ci", "Pp", id);
		free(id);
	}
	return 0;
}

static int
mdoc_lk_pre(MDOC_ARGS)
{
	const struct roff_node *link, *descr, *punct;
	struct tag	*t;

	if ((link = n->child) == NULL)
		return 0;

	/* Find beginning of trailing punctuation. */
	punct = n->last;
	while (punct != link && punct->flags & NODE_DELIMC)
		punct = punct->prev;
	punct = punct->next;

	/* Link target and link text. */
	descr = link->next;
	if (descr == punct)
		descr = link;  /* no text */
	t = print_otag(h, TAG_A, "ch", "Lk", link->string);
	do {
		if (descr->flags & (NODE_DELIMC | NODE_DELIMO))
			h->flags |= HTML_NOSPACE;
		print_text(h, descr->string);
		descr = descr->next;
	} while (descr != punct);
	print_tagq(h, t);

	/* Trailing punctuation. */
	while (punct != NULL) {
		h->flags |= HTML_NOSPACE;
		print_text(h, punct->string);
		punct = punct->next;
	}
	return 0;
}

static int
mdoc_mt_pre(MDOC_ARGS)
{
	struct tag	*t;
	char		*cp;

	for (n = n->child; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		mandoc_asprintf(&cp, "mailto:%s", n->string);
		t = print_otag(h, TAG_A, "ch", "Mt", cp);
		print_text(h, n->string);
		print_tagq(h, t);
		free(cp);
	}
	return 0;
}

static int
mdoc_fo_pre(MDOC_ARGS)
{
	struct tag	*t;

	switch (n->type) {
	case ROFFT_BLOCK:
		synopsis_pre(h, n);
		return 1;
	case ROFFT_HEAD:
		if (n->child != NULL) {
			t = print_otag_id(h, TAG_CODE, "Fn", n);
			print_text(h, n->child->string);
			print_tagq(h, t);
		}
		return 0;
	case ROFFT_BODY:
		h->flags |= HTML_NOSPACE;
		print_text(h, "(");
		h->flags |= HTML_NOSPACE;
		return 1;
	default:
		abort();
	}
}

static void
mdoc_fo_post(MDOC_ARGS)
{
	if (n->type != ROFFT_BODY)
		return;
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	h->flags |= HTML_NOSPACE;
	print_text(h, ";");
}

static int
mdoc_in_pre(MDOC_ARGS)
{
	struct tag	*t;

	synopsis_pre(h, n);
	print_otag(h, TAG_CODE, "c", "In");

	/*
	 * The first argument of the `In' gets special treatment as
	 * being a linked value.  Subsequent values are printed
	 * afterward.  groff does similarly.  This also handles the case
	 * of no children.
	 */

	if (NODE_SYNPRETTY & n->flags && NODE_LINE & n->flags)
		print_text(h, "#include");

	print_text(h, "<");
	h->flags |= HTML_NOSPACE;

	if (NULL != (n = n->child)) {
		assert(n->type == ROFFT_TEXT);

		if (h->base_includes)
			t = print_otag(h, TAG_A, "chI", "In", n->string);
		else
			t = print_otag(h, TAG_A, "c", "In");
		print_text(h, n->string);
		print_tagq(h, t);

		n = n->next;
	}

	h->flags |= HTML_NOSPACE;
	print_text(h, ">");

	for ( ; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		print_text(h, n->string);
	}
	return 0;
}

static int
mdoc_va_pre(MDOC_ARGS)
{
	print_otag(h, TAG_VAR, "c", "Va");
	return 1;
}

static int
mdoc_ap_pre(MDOC_ARGS)
{
	h->flags |= HTML_NOSPACE;
	print_text(h, "\\(aq");
	h->flags |= HTML_NOSPACE;
	return 1;
}

static int
mdoc_bf_pre(MDOC_ARGS)
{
	const char	*cattr;

	switch (n->type) {
	case ROFFT_BLOCK:
		html_close_paragraph(h);
		return 1;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		abort();
	}

	if (FONT_Em == n->norm->Bf.font)
		cattr = "Bf Em";
	else if (FONT_Sy == n->norm->Bf.font)
		cattr = "Bf Sy";
	else if (FONT_Li == n->norm->Bf.font)
		cattr = "Bf Li";
	else
		cattr = "Bf No";

	/* Cannot use TAG_SPAN because it may contain blocks. */
	print_otag(h, TAG_DIV, "c", cattr);
	return 1;
}

static int
mdoc_igndelim_pre(MDOC_ARGS)
{
	h->flags |= HTML_IGNDELIM;
	return 1;
}

static void
mdoc_pf_post(MDOC_ARGS)
{
	if ( ! (n->next == NULL || n->next->flags & NODE_LINE))
		h->flags |= HTML_NOSPACE;
}

static int
mdoc_rs_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		if (n->sec == SEC_SEE_ALSO)
			html_close_paragraph(h);
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		if (n->sec == SEC_SEE_ALSO)
			print_otag(h, TAG_P, "c", "Pp");
		print_otag(h, TAG_CITE, "c", "Rs");
		break;
	default:
		abort();
	}
	return 1;
}

static int
mdoc_no_pre(MDOC_ARGS)
{
	print_otag_id(h, TAG_SPAN, roff_name[n->tok], n);
	return 1;
}

static int
mdoc_sy_pre(MDOC_ARGS)
{
	print_otag_id(h, TAG_B, "Sy", n);
	return 1;
}

static int
mdoc_lb_pre(MDOC_ARGS)
{
	if (n->sec == SEC_LIBRARY &&
	    n->flags & NODE_LINE &&
	    roff_node_prev(n) != NULL)
		print_otag(h, TAG_BR, "");

	print_otag(h, TAG_SPAN, "c", "Lb");
	return 1;
}

static int
mdoc__x_pre(MDOC_ARGS)
{
	struct roff_node	*nn;
	const char		*cattr;
	enum htmltag		 t;

	t = TAG_SPAN;

	switch (n->tok) {
	case MDOC__A:
		cattr = "RsA";
		if ((nn = roff_node_prev(n)) != NULL && nn->tok == MDOC__A &&
		    ((nn = roff_node_next(n)) == NULL || nn->tok != MDOC__A))
			print_text(h, "and");
		break;
	case MDOC__B:
		t = TAG_I;
		cattr = "RsB";
		break;
	case MDOC__C:
		cattr = "RsC";
		break;
	case MDOC__D:
		cattr = "RsD";
		break;
	case MDOC__I:
		t = TAG_I;
		cattr = "RsI";
		break;
	case MDOC__J:
		t = TAG_I;
		cattr = "RsJ";
		break;
	case MDOC__N:
		cattr = "RsN";
		break;
	case MDOC__O:
		cattr = "RsO";
		break;
	case MDOC__P:
		cattr = "RsP";
		break;
	case MDOC__Q:
		cattr = "RsQ";
		break;
	case MDOC__R:
		cattr = "RsR";
		break;
	case MDOC__T:
		cattr = "RsT";
		break;
	case MDOC__U:
		print_otag(h, TAG_A, "ch", "RsU", n->child->string);
		return 1;
	case MDOC__V:
		cattr = "RsV";
		break;
	default:
		abort();
	}

	print_otag(h, t, "c", cattr);
	return 1;
}

static void
mdoc__x_post(MDOC_ARGS)
{
	struct roff_node *nn;

	if (n->tok == MDOC__A &&
	    (nn = roff_node_next(n)) != NULL && nn->tok == MDOC__A &&
	    ((nn = roff_node_next(nn)) == NULL || nn->tok != MDOC__A) &&
	    ((nn = roff_node_prev(n)) == NULL || nn->tok != MDOC__A))
		return;

	/* TODO: %U */

	if (n->parent == NULL || n->parent->tok != MDOC_Rs)
		return;

	h->flags |= HTML_NOSPACE;
	print_text(h, roff_node_next(n) ? "," : ".");
}

static int
mdoc_bk_pre(MDOC_ARGS)
{

	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		if (n->parent->args != NULL || n->prev->child == NULL)
			h->flags |= HTML_PREKEEP;
		break;
	default:
		abort();
	}

	return 1;
}

static void
mdoc_bk_post(MDOC_ARGS)
{

	if (n->type == ROFFT_BODY)
		h->flags &= ~(HTML_KEEP | HTML_PREKEEP);
}

static int
mdoc_quote_pre(MDOC_ARGS)
{
	if (n->type != ROFFT_BODY)
		return 1;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		print_text(h, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ?  "<" : "\\(la");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		print_text(h, "\\(lC");
		break;
	case MDOC_Bo:
	case MDOC_Bq:
		print_text(h, "\\(lB");
		break;
	case MDOC_Oo:
	case MDOC_Op:
		print_text(h, "\\(lB");
		/*
		 * Give up on semantic markup for now.
		 * We cannot use TAG_SPAN because .Oo may contain blocks.
		 * We cannot use TAG_DIV because we might be in a
		 * phrasing context (like .Dl or .Pp); we cannot
		 * close out a .Pp at this point either because
		 * that would break the line.
		 */
		/* XXX print_otag(h, TAG_???, "c", "Op"); */
		break;
	case MDOC_En:
		if (NULL == n->norm->Es ||
		    NULL == n->norm->Es->child)
			return 1;
		print_text(h, n->norm->Es->child->string);
		break;
	case MDOC_Do:
	case MDOC_Dq:
		print_text(h, "\\(lq");
		break;
	case MDOC_Qo:
	case MDOC_Qq:
		print_text(h, "\"");
		break;
	case MDOC_Po:
	case MDOC_Pq:
		print_text(h, "(");
		break;
	case MDOC_Ql:
		print_text(h, "\\(oq");
		h->flags |= HTML_NOSPACE;
		print_otag(h, TAG_CODE, "c", "Li");
		break;
	case MDOC_So:
	case MDOC_Sq:
		print_text(h, "\\(oq");
		break;
	default:
		abort();
	}

	h->flags |= HTML_NOSPACE;
	return 1;
}

static void
mdoc_quote_post(MDOC_ARGS)
{

	if (n->type != ROFFT_BODY && n->type != ROFFT_ELEM)
		return;

	h->flags |= HTML_NOSPACE;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		print_text(h, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ?  ">" : "\\(ra");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		print_text(h, "\\(rC");
		break;
	case MDOC_Oo:
	case MDOC_Op:
	case MDOC_Bo:
	case MDOC_Bq:
		print_text(h, "\\(rB");
		break;
	case MDOC_En:
		if (n->norm->Es == NULL ||
		    n->norm->Es->child == NULL ||
		    n->norm->Es->child->next == NULL)
			h->flags &= ~HTML_NOSPACE;
		else
			print_text(h, n->norm->Es->child->next->string);
		break;
	case MDOC_Do:
	case MDOC_Dq:
		print_text(h, "\\(rq");
		break;
	case MDOC_Qo:
	case MDOC_Qq:
		print_text(h, "\"");
		break;
	case MDOC_Po:
	case MDOC_Pq:
		print_text(h, ")");
		break;
	case MDOC_Ql:
	case MDOC_So:
	case MDOC_Sq:
		print_text(h, "\\(cq");
		break;
	default:
		abort();
	}
}

static int
mdoc_eo_pre(MDOC_ARGS)
{

	if (n->type != ROFFT_BODY)
		return 1;

	if (n->end == ENDBODY_NOT &&
	    n->parent->head->child == NULL &&
	    n->child != NULL &&
	    n->child->end != ENDBODY_NOT)
		print_text(h, "\\&");
	else if (n->end != ENDBODY_NOT ? n->child != NULL :
	    n->parent->head->child != NULL && (n->child != NULL ||
	    (n->parent->tail != NULL && n->parent->tail->child != NULL)))
		h->flags |= HTML_NOSPACE;
	return 1;
}

static void
mdoc_eo_post(MDOC_ARGS)
{
	int	 body, tail;

	if (n->type != ROFFT_BODY)
		return;

	if (n->end != ENDBODY_NOT) {
		h->flags &= ~HTML_NOSPACE;
		return;
	}

	body = n->child != NULL || n->parent->head->child != NULL;
	tail = n->parent->tail != NULL && n->parent->tail->child != NULL;

	if (body && tail)
		h->flags |= HTML_NOSPACE;
	else if ( ! tail)
		h->flags &= ~HTML_NOSPACE;
}

static int
mdoc_abort_pre(MDOC_ARGS)
{
	abort();
}
