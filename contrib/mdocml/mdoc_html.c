/*	$Id: mdoc_html.c,v 1.240 2016/01/08 17:48:09 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015, 2016 Ingo Schwarze <schwarze@openbsd.org>
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
#include <unistd.h>

#include "mandoc_aux.h"
#include "roff.h"
#include "mdoc.h"
#include "out.h"
#include "html.h"
#include "main.h"

#define	INDENT		 5

#define	MDOC_ARGS	  const struct roff_meta *meta, \
			  struct roff_node *n, \
			  struct html *h

#ifndef MIN
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif

struct	htmlmdoc {
	int		(*pre)(MDOC_ARGS);
	void		(*post)(MDOC_ARGS);
};

static	void		  print_mdoc_head(MDOC_ARGS);
static	void		  print_mdoc_node(MDOC_ARGS);
static	void		  print_mdoc_nodelist(MDOC_ARGS);
static	void		  synopsis_pre(struct html *,
				const struct roff_node *);

static	void		  a2width(const char *, struct roffsu *);

static	void		  mdoc_root_post(MDOC_ARGS);
static	int		  mdoc_root_pre(MDOC_ARGS);

static	void		  mdoc__x_post(MDOC_ARGS);
static	int		  mdoc__x_pre(MDOC_ARGS);
static	int		  mdoc_ad_pre(MDOC_ARGS);
static	int		  mdoc_an_pre(MDOC_ARGS);
static	int		  mdoc_ap_pre(MDOC_ARGS);
static	int		  mdoc_ar_pre(MDOC_ARGS);
static	int		  mdoc_bd_pre(MDOC_ARGS);
static	int		  mdoc_bf_pre(MDOC_ARGS);
static	void		  mdoc_bk_post(MDOC_ARGS);
static	int		  mdoc_bk_pre(MDOC_ARGS);
static	int		  mdoc_bl_pre(MDOC_ARGS);
static	int		  mdoc_bt_pre(MDOC_ARGS);
static	int		  mdoc_bx_pre(MDOC_ARGS);
static	int		  mdoc_cd_pre(MDOC_ARGS);
static	int		  mdoc_d1_pre(MDOC_ARGS);
static	int		  mdoc_dv_pre(MDOC_ARGS);
static	int		  mdoc_fa_pre(MDOC_ARGS);
static	int		  mdoc_fd_pre(MDOC_ARGS);
static	int		  mdoc_fl_pre(MDOC_ARGS);
static	int		  mdoc_fn_pre(MDOC_ARGS);
static	int		  mdoc_ft_pre(MDOC_ARGS);
static	int		  mdoc_em_pre(MDOC_ARGS);
static	void		  mdoc_eo_post(MDOC_ARGS);
static	int		  mdoc_eo_pre(MDOC_ARGS);
static	int		  mdoc_er_pre(MDOC_ARGS);
static	int		  mdoc_ev_pre(MDOC_ARGS);
static	int		  mdoc_ex_pre(MDOC_ARGS);
static	void		  mdoc_fo_post(MDOC_ARGS);
static	int		  mdoc_fo_pre(MDOC_ARGS);
static	int		  mdoc_ic_pre(MDOC_ARGS);
static	int		  mdoc_igndelim_pre(MDOC_ARGS);
static	int		  mdoc_in_pre(MDOC_ARGS);
static	int		  mdoc_it_pre(MDOC_ARGS);
static	int		  mdoc_lb_pre(MDOC_ARGS);
static	int		  mdoc_li_pre(MDOC_ARGS);
static	int		  mdoc_lk_pre(MDOC_ARGS);
static	int		  mdoc_mt_pre(MDOC_ARGS);
static	int		  mdoc_ms_pre(MDOC_ARGS);
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
static	int		  mdoc_rv_pre(MDOC_ARGS);
static	int		  mdoc_sh_pre(MDOC_ARGS);
static	int		  mdoc_skip_pre(MDOC_ARGS);
static	int		  mdoc_sm_pre(MDOC_ARGS);
static	int		  mdoc_sp_pre(MDOC_ARGS);
static	int		  mdoc_ss_pre(MDOC_ARGS);
static	int		  mdoc_sx_pre(MDOC_ARGS);
static	int		  mdoc_sy_pre(MDOC_ARGS);
static	int		  mdoc_ud_pre(MDOC_ARGS);
static	int		  mdoc_va_pre(MDOC_ARGS);
static	int		  mdoc_vt_pre(MDOC_ARGS);
static	int		  mdoc_xr_pre(MDOC_ARGS);
static	int		  mdoc_xx_pre(MDOC_ARGS);

static	const struct htmlmdoc mdocs[MDOC_MAX] = {
	{mdoc_ap_pre, NULL}, /* Ap */
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
	{mdoc_ar_pre, NULL}, /* Ar */
	{mdoc_cd_pre, NULL}, /* Cd */
	{mdoc_fl_pre, NULL}, /* Cm */
	{mdoc_dv_pre, NULL}, /* Dv */
	{mdoc_er_pre, NULL}, /* Er */
	{mdoc_ev_pre, NULL}, /* Ev */
	{mdoc_ex_pre, NULL}, /* Ex */
	{mdoc_fa_pre, NULL}, /* Fa */
	{mdoc_fd_pre, NULL}, /* Fd */
	{mdoc_fl_pre, NULL}, /* Fl */
	{mdoc_fn_pre, NULL}, /* Fn */
	{mdoc_ft_pre, NULL}, /* Ft */
	{mdoc_ic_pre, NULL}, /* Ic */
	{mdoc_in_pre, NULL}, /* In */
	{mdoc_li_pre, NULL}, /* Li */
	{mdoc_nd_pre, NULL}, /* Nd */
	{mdoc_nm_pre, NULL}, /* Nm */
	{mdoc_quote_pre, mdoc_quote_post}, /* Op */
	{mdoc_ft_pre, NULL}, /* Ot */
	{mdoc_pa_pre, NULL}, /* Pa */
	{mdoc_rv_pre, NULL}, /* Rv */
	{NULL, NULL}, /* St */
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
	{NULL, NULL}, /* At */
	{NULL, NULL}, /* Bc */
	{mdoc_bf_pre, NULL}, /* Bf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bq */
	{mdoc_xx_pre, NULL}, /* Bsx */
	{mdoc_bx_pre, NULL}, /* Bx */
	{mdoc_skip_pre, NULL}, /* Db */
	{NULL, NULL}, /* Dc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Do */
	{mdoc_quote_pre, mdoc_quote_post}, /* Dq */
	{NULL, NULL}, /* Ec */ /* FIXME: no space */
	{NULL, NULL}, /* Ef */
	{mdoc_em_pre, NULL}, /* Em */
	{mdoc_eo_pre, mdoc_eo_post}, /* Eo */
	{mdoc_xx_pre, NULL}, /* Fx */
	{mdoc_ms_pre, NULL}, /* Ms */
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
	{mdoc_bt_pre, NULL}, /* Bt */
	{NULL, NULL}, /* Hf */
	{mdoc_em_pre, NULL}, /* Fr */
	{mdoc_ud_pre, NULL}, /* Ud */
	{mdoc_lb_pre, NULL}, /* Lb */
	{mdoc_pp_pre, NULL}, /* Lp */
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
	{mdoc_sp_pre, NULL}, /* br */
	{mdoc_sp_pre, NULL}, /* sp */
	{mdoc__x_pre, mdoc__x_post}, /* %U */
	{NULL, NULL}, /* Ta */
	{mdoc_skip_pre, NULL}, /* ll */
};

static	const char * const lists[LIST_MAX] = {
	NULL,
	"list-bul",
	"list-col",
	"list-dash",
	"list-diag",
	"list-enum",
	"list-hang",
	"list-hyph",
	"list-inset",
	"list-item",
	"list-ohang",
	"list-tag"
};


/*
 * Calculate the scaling unit passed in a `-width' argument.  This uses
 * either a native scaling unit (e.g., 1i, 2m) or the string length of
 * the value.
 */
static void
a2width(const char *p, struct roffsu *su)
{

	if (a2roffsu(p, su, SCALE_MAX) < 2) {
		su->unit = SCALE_EN;
		su->scale = html_strlen(p);
	} else if (su->scale < 0.0)
		su->scale = 0.0;
}

/*
 * See the same function in mdoc_term.c for documentation.
 */
static void
synopsis_pre(struct html *h, const struct roff_node *n)
{

	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	if (n->prev->tok == n->tok &&
	    MDOC_Fo != n->tok &&
	    MDOC_Ft != n->tok &&
	    MDOC_Fn != n->tok) {
		print_otag(h, TAG_BR, 0, NULL);
		return;
	}

	switch (n->prev->tok) {
	case MDOC_Fd:
	case MDOC_Fn:
	case MDOC_Fo:
	case MDOC_In:
	case MDOC_Vt:
		print_paragraph(h);
		break;
	case MDOC_Ft:
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			print_paragraph(h);
			break;
		}
		/* FALLTHROUGH */
	default:
		print_otag(h, TAG_BR, 0, NULL);
		break;
	}
}

void
html_mdoc(void *arg, const struct roff_man *mdoc)
{
	struct htmlpair	 tag;
	struct html	*h;
	struct tag	*t, *tt;

	PAIR_CLASS_INIT(&tag, "mandoc");
	h = (struct html *)arg;

	if ( ! (HTML_FRAGMENT & h->oflags)) {
		print_gen_decls(h);
		t = print_otag(h, TAG_HTML, 0, NULL);
		tt = print_otag(h, TAG_HEAD, 0, NULL);
		print_mdoc_head(&mdoc->meta, mdoc->first->child, h);
		print_tagq(h, tt);
		print_otag(h, TAG_BODY, 0, NULL);
		print_otag(h, TAG_DIV, 1, &tag);
	} else
		t = print_otag(h, TAG_DIV, 1, &tag);

	mdoc_root_pre(&mdoc->meta, mdoc->first->child, h);
	print_mdoc_nodelist(&mdoc->meta, mdoc->first->child, h);
	mdoc_root_post(&mdoc->meta, mdoc->first->child, h);
	print_tagq(h, t);
	putchar('\n');
}

static void
print_mdoc_head(MDOC_ARGS)
{

	print_gen_head(h);
	bufinit(h);
	bufcat(h, meta->title);
	if (meta->msec)
		bufcat_fmt(h, "(%s)", meta->msec);
	if (meta->arch)
		bufcat_fmt(h, " (%s)", meta->arch);

	print_otag(h, TAG_TITLE, 0, NULL);
	print_text(h, h->buf);
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
	int		 child;
	struct tag	*t;

	child = 1;
	t = h->tags.head;
	n->flags &= ~MDOC_ENDED;

	switch (n->type) {
	case ROFFT_TEXT:
		/* No tables in this mode... */
		assert(NULL == h->tblt);

		/*
		 * Make sure that if we're in a literal mode already
		 * (i.e., within a <PRE>) don't print the newline.
		 */
		if (' ' == *n->string && MDOC_LINE & n->flags)
			if ( ! (HTML_LITERAL & h->flags))
				print_otag(h, TAG_BR, 0, NULL);
		if (MDOC_DELIMC & n->flags)
			h->flags |= HTML_NOSPACE;
		print_text(h, n->string);
		if (MDOC_DELIMO & n->flags)
			h->flags |= HTML_NOSPACE;
		return;
	case ROFFT_EQN:
		if (n->flags & MDOC_LINE)
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
		 * Close out the current table, if it's open, and unset
		 * the "meta" table state.  This will be reopened on the
		 * next table element.
		 */
		if (h->tblt != NULL) {
			print_tblclose(h);
			t = h->tags.head;
		}
		assert(h->tblt == NULL);
		if (mdocs[n->tok].pre && (n->end == ENDBODY_NOT || n->child))
			child = (*mdocs[n->tok].pre)(meta, n, h);
		break;
	}

	if (h->flags & HTML_KEEP && n->flags & MDOC_LINE) {
		h->flags &= ~HTML_KEEP;
		h->flags |= HTML_PREKEEP;
	}

	if (child && n->child)
		print_mdoc_nodelist(meta, n->child, h);

	print_stagq(h, t);

	switch (n->type) {
	case ROFFT_EQN:
		break;
	default:
		if ( ! mdocs[n->tok].post || n->flags & MDOC_ENDED)
			break;
		(*mdocs[n->tok].post)(meta, n, h);
		if (n->end != ENDBODY_NOT)
			n->body->flags |= MDOC_ENDED;
		if (n->end == ENDBODY_NOSPACE)
			h->flags |= HTML_NOSPACE;
		break;
	}
}

static void
mdoc_root_post(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t, *tt;

	PAIR_CLASS_INIT(&tag, "foot");
	t = print_otag(h, TAG_TABLE, 1, &tag);

	print_otag(h, TAG_TBODY, 0, NULL);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "foot-date");
	print_otag(h, TAG_TD, 1, &tag);
	print_text(h, meta->date);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag, "foot-os");
	print_otag(h, TAG_TD, 1, &tag);
	print_text(h, meta->os);
	print_tagq(h, t);
}

static int
mdoc_root_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
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
	print_text(h, volume);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag, "head-rtitle");
	print_otag(h, TAG_TD, 1, &tag);
	print_text(h, title);
	print_tagq(h, t);

	free(title);
	free(volume);
	return 1;
}

static int
mdoc_sh_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	switch (n->type) {
	case ROFFT_BLOCK:
		PAIR_CLASS_INIT(&tag, "section");
		print_otag(h, TAG_DIV, 1, &tag);
		return 1;
	case ROFFT_BODY:
		if (n->sec == SEC_AUTHORS)
			h->flags &= ~(HTML_SPLIT|HTML_NOSPLIT);
		return 1;
	default:
		break;
	}

	bufinit(h);

	for (n = n->child; n != NULL && n->type == ROFFT_TEXT; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n) {
		PAIR_ID_INIT(&tag, h->buf);
		print_otag(h, TAG_H1, 1, &tag);
	} else
		print_otag(h, TAG_H1, 0, NULL);

	return 1;
}

static int
mdoc_ss_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (n->type == ROFFT_BLOCK) {
		PAIR_CLASS_INIT(&tag, "subsection");
		print_otag(h, TAG_DIV, 1, &tag);
		return 1;
	} else if (n->type == ROFFT_BODY)
		return 1;

	bufinit(h);

	for (n = n->child; n != NULL && n->type == ROFFT_TEXT; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n) {
		PAIR_ID_INIT(&tag, h->buf);
		print_otag(h, TAG_H2, 1, &tag);
	} else
		print_otag(h, TAG_H2, 0, NULL);

	return 1;
}

static int
mdoc_fl_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	PAIR_CLASS_INIT(&tag, "flag");
	print_otag(h, TAG_B, 1, &tag);

	/* `Cm' has no leading hyphen. */

	if (MDOC_Cm == n->tok)
		return 1;

	print_text(h, "\\-");

	if (!(n->child == NULL &&
	    (n->next == NULL ||
	     n->next->type == ROFFT_TEXT ||
	     n->next->flags & MDOC_LINE)))
		h->flags |= HTML_NOSPACE;

	return 1;
}

static int
mdoc_nd_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (n->type != ROFFT_BODY)
		return 1;

	/* XXX: this tag in theory can contain block elements. */

	print_text(h, "\\(em");
	PAIR_CLASS_INIT(&tag, "desc");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_nm_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	int		 len;

	switch (n->type) {
	case ROFFT_HEAD:
		print_otag(h, TAG_TD, 0, NULL);
		/* FALLTHROUGH */
	case ROFFT_ELEM:
		PAIR_CLASS_INIT(&tag, "name");
		print_otag(h, TAG_B, 1, &tag);
		if (n->child == NULL && meta->name != NULL)
			print_text(h, meta->name);
		return 1;
	case ROFFT_BODY:
		print_otag(h, TAG_TD, 0, NULL);
		return 1;
	default:
		break;
	}

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "synopsis");
	print_otag(h, TAG_TABLE, 1, &tag);

	for (len = 0, n = n->head->child; n; n = n->next)
		if (n->type == ROFFT_TEXT)
			len += html_strlen(n->string);

	if (len == 0 && meta->name != NULL)
		len = html_strlen(meta->name);

	SCALE_HS_INIT(&su, len);
	bufinit(h);
	bufcat_su(h, "width", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_COL, 1, &tag);
	print_otag(h, TAG_COL, 0, NULL);
	print_otag(h, TAG_TBODY, 0, NULL);
	print_otag(h, TAG_TR, 0, NULL);
	return 1;
}

static int
mdoc_xr_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	if (NULL == n->child)
		return 0;

	PAIR_CLASS_INIT(&tag[0], "link-man");

	if (h->base_man) {
		buffmt_man(h, n->child->string,
		    n->child->next ?
		    n->child->next->string : NULL);
		PAIR_HREF_INIT(&tag[1], h->buf);
		print_otag(h, TAG_A, 2, tag);
	} else
		print_otag(h, TAG_A, 1, tag);

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
mdoc_ns_pre(MDOC_ARGS)
{

	if ( ! (MDOC_LINE & n->flags))
		h->flags |= HTML_NOSPACE;
	return 1;
}

static int
mdoc_ar_pre(MDOC_ARGS)
{
	struct htmlpair tag;

	PAIR_CLASS_INIT(&tag, "arg");
	print_otag(h, TAG_I, 1, &tag);
	return 1;
}

static int
mdoc_xx_pre(MDOC_ARGS)
{
	const char	*pp;
	struct htmlpair	 tag;
	int		 flags;

	switch (n->tok) {
	case MDOC_Bsx:
		pp = "BSD/OS";
		break;
	case MDOC_Dx:
		pp = "DragonFly";
		break;
	case MDOC_Fx:
		pp = "FreeBSD";
		break;
	case MDOC_Nx:
		pp = "NetBSD";
		break;
	case MDOC_Ox:
		pp = "OpenBSD";
		break;
	case MDOC_Ux:
		pp = "UNIX";
		break;
	default:
		return 1;
	}

	PAIR_CLASS_INIT(&tag, "unix");
	print_otag(h, TAG_SPAN, 1, &tag);

	print_text(h, pp);
	if (n->child) {
		flags = h->flags;
		h->flags |= HTML_KEEP;
		print_text(h, n->child->string);
		h->flags = flags;
	}
	return 0;
}

static int
mdoc_bx_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	PAIR_CLASS_INIT(&tag, "unix");
	print_otag(h, TAG_SPAN, 1, &tag);

	if (NULL != (n = n->child)) {
		print_text(h, n->string);
		h->flags |= HTML_NOSPACE;
		print_text(h, "BSD");
	} else {
		print_text(h, "BSD");
		return 0;
	}

	if (NULL != (n = n->next)) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "-");
		h->flags |= HTML_NOSPACE;
		print_text(h, n->string);
	}

	return 0;
}

static int
mdoc_it_pre(MDOC_ARGS)
{
	struct roffsu	 su;
	enum mdoc_list	 type;
	struct htmlpair	 tag[2];
	const struct roff_node *bl;

	bl = n->parent;
	while (bl && MDOC_Bl != bl->tok)
		bl = bl->parent;

	assert(bl);

	type = bl->norm->Bl.type;

	assert(lists[type]);
	PAIR_CLASS_INIT(&tag[0], lists[type]);

	bufinit(h);

	if (n->type == ROFFT_HEAD) {
		switch (type) {
		case LIST_bullet:
		case LIST_dash:
		case LIST_item:
		case LIST_hyphen:
		case LIST_enum:
			return 0;
		case LIST_diag:
		case LIST_hang:
		case LIST_inset:
		case LIST_ohang:
		case LIST_tag:
			SCALE_VS_INIT(&su, ! bl->norm->Bl.comp);
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DT, 2, tag);
			if (LIST_diag != type)
				break;
			PAIR_CLASS_INIT(&tag[0], "diag");
			print_otag(h, TAG_B, 1, tag);
			break;
		case LIST_column:
			break;
		default:
			break;
		}
	} else if (n->type == ROFFT_BODY) {
		switch (type) {
		case LIST_bullet:
		case LIST_hyphen:
		case LIST_dash:
		case LIST_enum:
		case LIST_item:
			SCALE_VS_INIT(&su, ! bl->norm->Bl.comp);
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_LI, 2, tag);
			break;
		case LIST_diag:
		case LIST_hang:
		case LIST_inset:
		case LIST_ohang:
		case LIST_tag:
			if (NULL == bl->norm->Bl.width) {
				print_otag(h, TAG_DD, 1, tag);
				break;
			}
			a2width(bl->norm->Bl.width, &su);
			bufcat_su(h, "margin-left", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DD, 2, tag);
			break;
		case LIST_column:
			SCALE_VS_INIT(&su, ! bl->norm->Bl.comp);
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_TD, 2, tag);
			break;
		default:
			break;
		}
	} else {
		switch (type) {
		case LIST_column:
			print_otag(h, TAG_TR, 1, tag);
			break;
		default:
			break;
		}
	}

	return 1;
}

static int
mdoc_bl_pre(MDOC_ARGS)
{
	int		 i;
	struct htmlpair	 tag[3];
	struct roffsu	 su;
	char		 buf[BUFSIZ];

	if (n->type == ROFFT_BODY) {
		if (LIST_column == n->norm->Bl.type)
			print_otag(h, TAG_TBODY, 0, NULL);
		return 1;
	}

	if (n->type == ROFFT_HEAD) {
		if (LIST_column != n->norm->Bl.type)
			return 0;

		/*
		 * For each column, print out the <COL> tag with our
		 * suggested width.  The last column gets min-width, as
		 * in terminal mode it auto-sizes to the width of the
		 * screen and we want to preserve that behaviour.
		 */

		for (i = 0; i < (int)n->norm->Bl.ncols; i++) {
			bufinit(h);
			a2width(n->norm->Bl.cols[i], &su);
			if (i < (int)n->norm->Bl.ncols - 1)
				bufcat_su(h, "width", &su);
			else
				bufcat_su(h, "min-width", &su);
			PAIR_STYLE_INIT(&tag[0], h);
			print_otag(h, TAG_COL, 1, tag);
		}

		return 0;
	}

	SCALE_VS_INIT(&su, 0);
	bufinit(h);
	bufcat_su(h, "margin-top", &su);
	bufcat_su(h, "margin-bottom", &su);
	PAIR_STYLE_INIT(&tag[0], h);

	assert(lists[n->norm->Bl.type]);
	(void)strlcpy(buf, "list ", BUFSIZ);
	(void)strlcat(buf, lists[n->norm->Bl.type], BUFSIZ);
	PAIR_INIT(&tag[1], ATTR_CLASS, buf);

	/* Set the block's left-hand margin. */

	if (n->norm->Bl.offs) {
		a2width(n->norm->Bl.offs, &su);
		bufcat_su(h, "margin-left", &su);
	}

	switch (n->norm->Bl.type) {
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
	case LIST_item:
		print_otag(h, TAG_UL, 2, tag);
		break;
	case LIST_enum:
		print_otag(h, TAG_OL, 2, tag);
		break;
	case LIST_diag:
	case LIST_hang:
	case LIST_inset:
	case LIST_ohang:
	case LIST_tag:
		print_otag(h, TAG_DL, 2, tag);
		break;
	case LIST_column:
		print_otag(h, TAG_TABLE, 2, tag);
		break;
	default:
		abort();
	}

	return 1;
}

static int
mdoc_ex_pre(MDOC_ARGS)
{
	struct htmlpair	  tag;
	struct tag	 *t;
	struct roff_node *nch;

	if (n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "utility");

	print_text(h, "The");

	for (nch = n->child; nch != NULL; nch = nch->next) {
		assert(nch->type == ROFFT_TEXT);

		t = print_otag(h, TAG_B, 1, &tag);
		print_text(h, nch->string);
		print_tagq(h, t);

		if (nch->next == NULL)
			continue;

		if (nch->prev != NULL || nch->next->next != NULL) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}

		if (nch->next->next == NULL)
			print_text(h, "and");
	}

	if (n->child != NULL && n->child->next != NULL)
		print_text(h, "utilities exit\\~0");
	else
		print_text(h, "utility exits\\~0");

	print_text(h, "on success, and\\~>0 if an error occurs.");
	return 0;
}

static int
mdoc_em_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "emph");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_d1_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (n->type != ROFFT_BLOCK)
		return 1;

	SCALE_VS_INIT(&su, 0);
	bufinit(h);
	bufcat_su(h, "margin-top", &su);
	bufcat_su(h, "margin-bottom", &su);
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_BLOCKQUOTE, 1, tag);

	/* BLOCKQUOTE needs a block body. */

	PAIR_CLASS_INIT(&tag[0], "display");
	print_otag(h, TAG_DIV, 1, tag);

	if (MDOC_Dl == n->tok) {
		PAIR_CLASS_INIT(&tag[0], "lit");
		print_otag(h, TAG_CODE, 1, tag);
	}

	return 1;
}

static int
mdoc_sx_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	bufinit(h);
	bufcat(h, "#");

	for (n = n->child; n; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	PAIR_CLASS_INIT(&tag[0], "link-sec");
	PAIR_HREF_INIT(&tag[1], h->buf);

	print_otag(h, TAG_I, 1, tag);
	print_otag(h, TAG_A, 2, tag);
	return 1;
}

static int
mdoc_bd_pre(MDOC_ARGS)
{
	struct htmlpair		 tag[2];
	int			 comp, sv;
	struct roff_node	*nn;
	struct roffsu		 su;

	if (n->type == ROFFT_HEAD)
		return 0;

	if (n->type == ROFFT_BLOCK) {
		comp = n->norm->Bd.comp;
		for (nn = n; nn && ! comp; nn = nn->parent) {
			if (nn->type != ROFFT_BLOCK)
				continue;
			if (MDOC_Ss == nn->tok || MDOC_Sh == nn->tok)
				comp = 1;
			if (nn->prev)
				break;
		}
		if ( ! comp)
			print_paragraph(h);
		return 1;
	}

	/* Handle the -offset argument. */

	if (n->norm->Bd.offs == NULL ||
	    ! strcmp(n->norm->Bd.offs, "left"))
		SCALE_HS_INIT(&su, 0);
	else if ( ! strcmp(n->norm->Bd.offs, "indent"))
		SCALE_HS_INIT(&su, INDENT);
	else if ( ! strcmp(n->norm->Bd.offs, "indent-two"))
		SCALE_HS_INIT(&su, INDENT * 2);
	else
		a2width(n->norm->Bd.offs, &su);

	bufinit(h);
	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag[0], h);

	if (DISP_unfilled != n->norm->Bd.type &&
	    DISP_literal != n->norm->Bd.type) {
		PAIR_CLASS_INIT(&tag[1], "display");
		print_otag(h, TAG_DIV, 2, tag);
		return 1;
	}

	PAIR_CLASS_INIT(&tag[1], "lit display");
	print_otag(h, TAG_PRE, 2, tag);

	/* This can be recursive: save & set our literal state. */

	sv = h->flags & HTML_LITERAL;
	h->flags |= HTML_LITERAL;

	for (nn = n->child; nn; nn = nn->next) {
		print_mdoc_node(meta, nn, h);
		/*
		 * If the printed node flushes its own line, then we
		 * needn't do it here as well.  This is hacky, but the
		 * notion of selective eoln whitespace is pretty dumb
		 * anyway, so don't sweat it.
		 */
		switch (nn->tok) {
		case MDOC_Sm:
		case MDOC_br:
		case MDOC_sp:
		case MDOC_Bl:
		case MDOC_D1:
		case MDOC_Dl:
		case MDOC_Lp:
		case MDOC_Pp:
			continue;
		default:
			break;
		}
		if (h->flags & HTML_NONEWLINE ||
		    (nn->next && ! (nn->next->flags & MDOC_LINE)))
			continue;
		else if (nn->next)
			print_text(h, "\n");

		h->flags |= HTML_NOSPACE;
	}

	if (0 == sv)
		h->flags &= ~HTML_LITERAL;

	return 0;
}

static int
mdoc_pa_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "file");
	print_otag(h, TAG_I, 1, &tag);
	return 1;
}

static int
mdoc_ad_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "addr");
	print_otag(h, TAG_I, 1, &tag);
	return 1;
}

static int
mdoc_an_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

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
		print_otag(h, TAG_BR, 0, NULL);

	if (n->sec == SEC_AUTHORS && ! (h->flags & HTML_NOSPLIT))
		h->flags |= HTML_SPLIT;

	PAIR_CLASS_INIT(&tag, "author");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_cd_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "config");
	print_otag(h, TAG_B, 1, &tag);
	return 1;
}

static int
mdoc_dv_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "define");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_ev_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "env");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_er_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "errno");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_fa_pre(MDOC_ARGS)
{
	const struct roff_node	*nn;
	struct htmlpair		 tag;
	struct tag		*t;

	PAIR_CLASS_INIT(&tag, "farg");
	if (n->parent->tok != MDOC_Fo) {
		print_otag(h, TAG_I, 1, &tag);
		return 1;
	}

	for (nn = n->child; nn; nn = nn->next) {
		t = print_otag(h, TAG_I, 1, &tag);
		print_text(h, nn->string);
		print_tagq(h, t);
		if (nn->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}
	}

	if (n->child && n->next && n->next->tok == MDOC_Fa) {
		h->flags |= HTML_NOSPACE;
		print_text(h, ",");
	}

	return 0;
}

static int
mdoc_fd_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	char		 buf[BUFSIZ];
	size_t		 sz;
	int		 i;
	struct tag	*t;

	synopsis_pre(h, n);

	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);

	if (strcmp(n->string, "#include")) {
		PAIR_CLASS_INIT(&tag[0], "macro");
		print_otag(h, TAG_B, 1, tag);
		return 1;
	}

	PAIR_CLASS_INIT(&tag[0], "includes");
	print_otag(h, TAG_B, 1, tag);
	print_text(h, n->string);

	if (NULL != (n = n->next)) {
		assert(n->type == ROFFT_TEXT);

		/*
		 * XXX This is broken and not easy to fix.
		 * When using -Oincludes, truncation may occur.
		 * Dynamic allocation wouldn't help because
		 * passing long strings to buffmt_includes()
		 * does not work either.
		 */

		strlcpy(buf, '<' == *n->string || '"' == *n->string ?
		    n->string + 1 : n->string, BUFSIZ);

		sz = strlen(buf);
		if (sz && ('>' == buf[sz - 1] || '"' == buf[sz - 1]))
			buf[sz - 1] = '\0';

		PAIR_CLASS_INIT(&tag[0], "link-includes");

		i = 1;
		if (h->base_includes) {
			buffmt_includes(h, buf);
			PAIR_HREF_INIT(&tag[i], h->buf);
			i++;
		}

		t = print_otag(h, TAG_A, i, tag);
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
	struct htmlpair	 tag;

	if (n->type == ROFFT_BLOCK) {
		synopsis_pre(h, n);
		return 1;
	} else if (n->type == ROFFT_ELEM) {
		synopsis_pre(h, n);
	} else if (n->type == ROFFT_HEAD)
		return 0;

	PAIR_CLASS_INIT(&tag, "type");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_ft_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "ftype");
	print_otag(h, TAG_I, 1, &tag);
	return 1;
}

static int
mdoc_fn_pre(MDOC_ARGS)
{
	struct tag	*t;
	struct htmlpair	 tag[2];
	char		 nbuf[BUFSIZ];
	const char	*sp, *ep;
	int		 sz, i, pretty;

	pretty = MDOC_SYNPRETTY & n->flags;
	synopsis_pre(h, n);

	/* Split apart into type and name. */
	assert(n->child->string);
	sp = n->child->string;

	ep = strchr(sp, ' ');
	if (NULL != ep) {
		PAIR_CLASS_INIT(&tag[0], "ftype");
		t = print_otag(h, TAG_I, 1, tag);

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

	PAIR_CLASS_INIT(&tag[0], "fname");

	/*
	 * FIXME: only refer to IDs that we know exist.
	 */

#if 0
	if (MDOC_SYNPRETTY & n->flags) {
		nbuf[0] = '\0';
		html_idcat(nbuf, sp, BUFSIZ);
		PAIR_ID_INIT(&tag[1], nbuf);
	} else {
		strlcpy(nbuf, "#", BUFSIZ);
		html_idcat(nbuf, sp, BUFSIZ);
		PAIR_HREF_INIT(&tag[1], nbuf);
	}
#endif

	t = print_otag(h, TAG_B, 1, tag);

	if (sp)
		print_text(h, sp);

	print_tagq(h, t);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;

	PAIR_CLASS_INIT(&tag[0], "farg");
	bufinit(h);
	bufcat_style(h, "white-space", "nowrap");
	PAIR_STYLE_INIT(&tag[1], h);

	for (n = n->child->next; n; n = n->next) {
		i = 1;
		if (MDOC_SYNPRETTY & n->flags)
			i = 2;
		t = print_otag(h, TAG_I, i, tag);
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

	print_paragraph(h);
	return 0;
}

static int
mdoc_sp_pre(MDOC_ARGS)
{
	struct roffsu	 su;
	struct htmlpair	 tag;

	SCALE_VS_INIT(&su, 1);

	if (MDOC_sp == n->tok) {
		if (NULL != (n = n->child)) {
			if ( ! a2roffsu(n->string, &su, SCALE_VS))
				su.scale = 1.0;
			else if (su.scale < 0.0)
				su.scale = 0.0;
		}
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
mdoc_lk_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);

	PAIR_CLASS_INIT(&tag[0], "link-ext");
	PAIR_HREF_INIT(&tag[1], n->string);

	print_otag(h, TAG_A, 2, tag);

	if (NULL == n->next)
		print_text(h, n->string);

	for (n = n->next; n; n = n->next)
		print_text(h, n->string);

	return 0;
}

static int
mdoc_mt_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct tag	*t;

	PAIR_CLASS_INIT(&tag[0], "link-mail");

	for (n = n->child; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);

		bufinit(h);
		bufcat(h, "mailto:");
		bufcat(h, n->string);

		PAIR_HREF_INIT(&tag[1], h->buf);
		t = print_otag(h, TAG_A, 2, tag);
		print_text(h, n->string);
		print_tagq(h, t);
	}

	return 0;
}

static int
mdoc_fo_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t;

	if (n->type == ROFFT_BODY) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "(");
		h->flags |= HTML_NOSPACE;
		return 1;
	} else if (n->type == ROFFT_BLOCK) {
		synopsis_pre(h, n);
		return 1;
	}

	if (n->child == NULL)
		return 0;

	assert(n->child->string);
	PAIR_CLASS_INIT(&tag, "fname");
	t = print_otag(h, TAG_B, 1, &tag);
	print_text(h, n->child->string);
	print_tagq(h, t);
	return 0;
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
	struct htmlpair	 tag[2];
	int		 i;

	synopsis_pre(h, n);

	PAIR_CLASS_INIT(&tag[0], "includes");
	print_otag(h, TAG_B, 1, tag);

	/*
	 * The first argument of the `In' gets special treatment as
	 * being a linked value.  Subsequent values are printed
	 * afterward.  groff does similarly.  This also handles the case
	 * of no children.
	 */

	if (MDOC_SYNPRETTY & n->flags && MDOC_LINE & n->flags)
		print_text(h, "#include");

	print_text(h, "<");
	h->flags |= HTML_NOSPACE;

	if (NULL != (n = n->child)) {
		assert(n->type == ROFFT_TEXT);

		PAIR_CLASS_INIT(&tag[0], "link-includes");

		i = 1;
		if (h->base_includes) {
			buffmt_includes(h, n->string);
			PAIR_HREF_INIT(&tag[i], h->buf);
			i++;
		}

		t = print_otag(h, TAG_A, i, tag);
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
mdoc_ic_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "cmd");
	print_otag(h, TAG_B, 1, &tag);
	return 1;
}

static int
mdoc_rv_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t;
	struct roff_node *nch;

	if (n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "fname");

	if (n->child != NULL) {
		print_text(h, "The");

		for (nch = n->child; nch != NULL; nch = nch->next) {
			t = print_otag(h, TAG_B, 1, &tag);
			print_text(h, nch->string);
			print_tagq(h, t);

			h->flags |= HTML_NOSPACE;
			print_text(h, "()");

			if (nch->next == NULL)
				continue;

			if (nch->prev != NULL || nch->next->next != NULL) {
				h->flags |= HTML_NOSPACE;
				print_text(h, ",");
			}
			if (nch->next->next == NULL)
				print_text(h, "and");
		}

		if (n->child != NULL && n->child->next != NULL)
			print_text(h, "functions return");
		else
			print_text(h, "function returns");

		print_text(h, "the value\\~0 if successful;");
	} else
		print_text(h, "Upon successful completion,"
                    " the value\\~0 is returned;");

	print_text(h, "otherwise the value\\~\\-1 is returned"
	   " and the global variable");

	PAIR_CLASS_INIT(&tag, "var");
	t = print_otag(h, TAG_B, 1, &tag);
	print_text(h, "errno");
	print_tagq(h, t);
	print_text(h, "is set to indicate the error.");
	return 0;
}

static int
mdoc_va_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "var");
	print_otag(h, TAG_B, 1, &tag);
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
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (n->type == ROFFT_HEAD)
		return 0;
	else if (n->type != ROFFT_BODY)
		return 1;

	if (FONT_Em == n->norm->Bf.font)
		PAIR_CLASS_INIT(&tag[0], "emph");
	else if (FONT_Sy == n->norm->Bf.font)
		PAIR_CLASS_INIT(&tag[0], "symb");
	else if (FONT_Li == n->norm->Bf.font)
		PAIR_CLASS_INIT(&tag[0], "lit");
	else
		PAIR_CLASS_INIT(&tag[0], "none");

	/*
	 * We want this to be inline-formatted, but needs to be div to
	 * accept block children.
	 */
	bufinit(h);
	bufcat_style(h, "display", "inline");
	SCALE_HS_INIT(&su, 1);
	/* Needs a left-margin for spacing. */
	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag[1], h);
	print_otag(h, TAG_DIV, 2, tag);
	return 1;
}

static int
mdoc_ms_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
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

	if ( ! (n->next == NULL || n->next->flags & MDOC_LINE))
		h->flags |= HTML_NOSPACE;
}

static int
mdoc_rs_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (n->type != ROFFT_BLOCK)
		return 1;

	if (n->prev && SEC_SEE_ALSO == n->sec)
		print_paragraph(h);

	PAIR_CLASS_INIT(&tag, "ref");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_no_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "none");
	print_otag(h, TAG_CODE, 1, &tag);
	return 1;
}

static int
mdoc_li_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "lit");
	print_otag(h, TAG_CODE, 1, &tag);
	return 1;
}

static int
mdoc_sy_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc_bt_pre(MDOC_ARGS)
{

	print_text(h, "is currently in beta test.");
	return 0;
}

static int
mdoc_ud_pre(MDOC_ARGS)
{

	print_text(h, "currently under development.");
	return 0;
}

static int
mdoc_lb_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	if (SEC_LIBRARY == n->sec && MDOC_LINE & n->flags && n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "lib");
	print_otag(h, TAG_SPAN, 1, &tag);
	return 1;
}

static int
mdoc__x_pre(MDOC_ARGS)
{
	struct htmlpair	tag[2];
	enum htmltag	t;

	t = TAG_SPAN;

	switch (n->tok) {
	case MDOC__A:
		PAIR_CLASS_INIT(&tag[0], "ref-auth");
		if (n->prev && MDOC__A == n->prev->tok)
			if (NULL == n->next || MDOC__A != n->next->tok)
				print_text(h, "and");
		break;
	case MDOC__B:
		PAIR_CLASS_INIT(&tag[0], "ref-book");
		t = TAG_I;
		break;
	case MDOC__C:
		PAIR_CLASS_INIT(&tag[0], "ref-city");
		break;
	case MDOC__D:
		PAIR_CLASS_INIT(&tag[0], "ref-date");
		break;
	case MDOC__I:
		PAIR_CLASS_INIT(&tag[0], "ref-issue");
		t = TAG_I;
		break;
	case MDOC__J:
		PAIR_CLASS_INIT(&tag[0], "ref-jrnl");
		t = TAG_I;
		break;
	case MDOC__N:
		PAIR_CLASS_INIT(&tag[0], "ref-num");
		break;
	case MDOC__O:
		PAIR_CLASS_INIT(&tag[0], "ref-opt");
		break;
	case MDOC__P:
		PAIR_CLASS_INIT(&tag[0], "ref-page");
		break;
	case MDOC__Q:
		PAIR_CLASS_INIT(&tag[0], "ref-corp");
		break;
	case MDOC__R:
		PAIR_CLASS_INIT(&tag[0], "ref-rep");
		break;
	case MDOC__T:
		PAIR_CLASS_INIT(&tag[0], "ref-title");
		break;
	case MDOC__U:
		PAIR_CLASS_INIT(&tag[0], "link-ref");
		break;
	case MDOC__V:
		PAIR_CLASS_INIT(&tag[0], "ref-vol");
		break;
	default:
		abort();
	}

	if (MDOC__U != n->tok) {
		print_otag(h, t, 1, tag);
		return 1;
	}

	PAIR_HREF_INIT(&tag[1], n->child->string);
	print_otag(h, TAG_A, 2, tag);

	return 1;
}

static void
mdoc__x_post(MDOC_ARGS)
{

	if (MDOC__A == n->tok && n->next && MDOC__A == n->next->tok)
		if (NULL == n->next->next || MDOC__A != n->next->next->tok)
			if (NULL == n->prev || MDOC__A != n->prev->tok)
				return;

	/* TODO: %U */

	if (NULL == n->parent || MDOC_Rs != n->parent->tok)
		return;

	h->flags |= HTML_NOSPACE;
	print_text(h, n->next ? "," : ".");
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
	struct htmlpair	tag;

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
		h->flags |= HTML_NOSPACE;
		PAIR_CLASS_INIT(&tag, "opt");
		print_otag(h, TAG_SPAN, 1, &tag);
		break;
	case MDOC_En:
		if (NULL == n->norm->Es ||
		    NULL == n->norm->Es->child)
			return 1;
		print_text(h, n->norm->Es->child->string);
		break;
	case MDOC_Do:
	case MDOC_Dq:
	case MDOC_Qo:
	case MDOC_Qq:
		print_text(h, "\\(lq");
		break;
	case MDOC_Po:
	case MDOC_Pq:
		print_text(h, "(");
		break;
	case MDOC_Ql:
		print_text(h, "\\(oq");
		h->flags |= HTML_NOSPACE;
		PAIR_CLASS_INIT(&tag, "lit");
		print_otag(h, TAG_CODE, 1, &tag);
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
	case MDOC_Qo:
	case MDOC_Qq:
	case MDOC_Do:
	case MDOC_Dq:
		print_text(h, "\\(rq");
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
