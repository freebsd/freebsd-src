/*	$Id: mdoc_html.c,v 1.182 2011/11/03 20:37:00 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "out.h"
#include "html.h"
#include "mdoc.h"
#include "main.h"

#define	INDENT		 5

#define	MDOC_ARGS	  const struct mdoc_meta *m, \
			  const struct mdoc_node *n, \
			  struct html *h

#ifndef MIN
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif

struct	htmlmdoc {
	int		(*pre)(MDOC_ARGS);
	void		(*post)(MDOC_ARGS);
};

static	void		  print_mdoc(MDOC_ARGS);
static	void		  print_mdoc_head(MDOC_ARGS);
static	void		  print_mdoc_node(MDOC_ARGS);
static	void		  print_mdoc_nodelist(MDOC_ARGS);
static	void	  	  synopsis_pre(struct html *, 
				const struct mdoc_node *);

static	void		  a2width(const char *, struct roffsu *);
static	void		  a2offs(const char *, struct roffsu *);

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
static	int		  mdoc_ns_pre(MDOC_ARGS);
static	int		  mdoc_pa_pre(MDOC_ARGS);
static	void		  mdoc_pf_post(MDOC_ARGS);
static	int		  mdoc_pp_pre(MDOC_ARGS);
static	void		  mdoc_quote_post(MDOC_ARGS);
static	int		  mdoc_quote_pre(MDOC_ARGS);
static	int		  mdoc_rs_pre(MDOC_ARGS);
static	int		  mdoc_rv_pre(MDOC_ARGS);
static	int		  mdoc_sh_pre(MDOC_ARGS);
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
	{NULL, NULL}, /* Ot */
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
	{NULL, NULL}, /* Db */
	{NULL, NULL}, /* Dc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Do */
	{mdoc_quote_pre, mdoc_quote_post}, /* Dq */
	{NULL, NULL}, /* Ec */ /* FIXME: no space */
	{NULL, NULL}, /* Ef */
	{mdoc_em_pre, NULL}, /* Em */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Eo */
	{mdoc_xx_pre, NULL}, /* Fx */
	{mdoc_ms_pre, NULL}, /* Ms */
	{mdoc_igndelim_pre, NULL}, /* No */
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
	{NULL, NULL}, /* Fr */
	{mdoc_ud_pre, NULL}, /* Ud */
	{mdoc_lb_pre, NULL}, /* Lb */
	{mdoc_pp_pre, NULL}, /* Lp */ 
	{mdoc_lk_pre, NULL}, /* Lk */ 
	{mdoc_mt_pre, NULL}, /* Mt */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Brq */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Bro */ 
	{NULL, NULL}, /* Brc */ 
	{mdoc__x_pre, mdoc__x_post}, /* %C */ 
	{NULL, NULL}, /* Es */  /* TODO */
	{NULL, NULL}, /* En */  /* TODO */
	{mdoc_xx_pre, NULL}, /* Dx */ 
	{mdoc__x_pre, mdoc__x_post}, /* %Q */ 
	{mdoc_sp_pre, NULL}, /* br */
	{mdoc_sp_pre, NULL}, /* sp */ 
	{mdoc__x_pre, mdoc__x_post}, /* %U */ 
	{NULL, NULL}, /* Ta */ 
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

void
html_mdoc(void *arg, const struct mdoc *m)
{

	print_mdoc(mdoc_meta(m), mdoc_node(m), (struct html *)arg);
	putchar('\n');
}


/*
 * Calculate the scaling unit passed in a `-width' argument.  This uses
 * either a native scaling unit (e.g., 1i, 2m) or the string length of
 * the value.
 */
static void
a2width(const char *p, struct roffsu *su)
{

	if ( ! a2roffsu(p, su, SCALE_MAX)) {
		su->unit = SCALE_BU;
		su->scale = html_strlen(p);
	}
}


/*
 * See the same function in mdoc_term.c for documentation.
 */
static void
synopsis_pre(struct html *h, const struct mdoc_node *n)
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
	case (MDOC_Fd):
		/* FALLTHROUGH */
	case (MDOC_Fn):
		/* FALLTHROUGH */
	case (MDOC_Fo):
		/* FALLTHROUGH */
	case (MDOC_In):
		/* FALLTHROUGH */
	case (MDOC_Vt):
		print_otag(h, TAG_P, 0, NULL);
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			print_otag(h, TAG_P, 0, NULL);
			break;
		}
		/* FALLTHROUGH */
	default:
		print_otag(h, TAG_BR, 0, NULL);
		break;
	}
}


/*
 * Calculate the scaling unit passed in an `-offset' argument.  This
 * uses either a native scaling unit (e.g., 1i, 2m), one of a set of
 * predefined strings (indent, etc.), or the string length of the value.
 */
static void
a2offs(const char *p, struct roffsu *su)
{

	/* FIXME: "right"? */

	if (0 == strcmp(p, "left"))
		SCALE_HS_INIT(su, 0);
	else if (0 == strcmp(p, "indent"))
		SCALE_HS_INIT(su, INDENT);
	else if (0 == strcmp(p, "indent-two"))
		SCALE_HS_INIT(su, INDENT * 2);
	else if ( ! a2roffsu(p, su, SCALE_MAX))
		SCALE_HS_INIT(su, html_strlen(p));
}


static void
print_mdoc(MDOC_ARGS)
{
	struct tag	*t, *tt;
	struct htmlpair	 tag;

	PAIR_CLASS_INIT(&tag, "mandoc");

	if ( ! (HTML_FRAGMENT & h->oflags)) {
		print_gen_decls(h);
		t = print_otag(h, TAG_HTML, 0, NULL);
		tt = print_otag(h, TAG_HEAD, 0, NULL);
		print_mdoc_head(m, n, h);
		print_tagq(h, tt);
		print_otag(h, TAG_BODY, 0, NULL);
		print_otag(h, TAG_DIV, 1, &tag);
	} else 
		t = print_otag(h, TAG_DIV, 1, &tag);

	print_mdoc_nodelist(m, n, h);
	print_tagq(h, t);
}


/* ARGSUSED */
static void
print_mdoc_head(MDOC_ARGS)
{

	print_gen_head(h);
	bufinit(h);
	bufcat_fmt(h, "%s(%s)", m->title, m->msec);

	if (m->arch)
		bufcat_fmt(h, " (%s)", m->arch);

	print_otag(h, TAG_TITLE, 0, NULL);
	print_text(h, h->buf);
}


static void
print_mdoc_nodelist(MDOC_ARGS)
{

	print_mdoc_node(m, n, h);
	if (n->next)
		print_mdoc_nodelist(m, n->next, h);
}


static void
print_mdoc_node(MDOC_ARGS)
{
	int		 child;
	struct tag	*t;

	child = 1;
	t = h->tags.head;

	switch (n->type) {
	case (MDOC_ROOT):
		child = mdoc_root_pre(m, n, h);
		break;
	case (MDOC_TEXT):
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
	case (MDOC_EQN):
		print_eqn(h, n->eqn);
		break;
	case (MDOC_TBL):
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
		if (h->tblt) {
			print_tblclose(h);
			t = h->tags.head;
		}

		assert(NULL == h->tblt);
		if (mdocs[n->tok].pre && ENDBODY_NOT == n->end)
			child = (*mdocs[n->tok].pre)(m, n, h);
		break;
	}

	if (HTML_KEEP & h->flags) {
		if (n->prev && n->prev->line != n->line) {
			h->flags &= ~HTML_KEEP;
			h->flags |= HTML_PREKEEP;
		} else if (NULL == n->prev) {
			if (n->parent && n->parent->line != n->line) {
				h->flags &= ~HTML_KEEP;
				h->flags |= HTML_PREKEEP;
			}
		}
	}

	if (child && n->child)
		print_mdoc_nodelist(m, n->child, h);

	print_stagq(h, t);

	switch (n->type) {
	case (MDOC_ROOT):
		mdoc_root_post(m, n, h);
		break;
	case (MDOC_EQN):
		break;
	default:
		if (mdocs[n->tok].post && ENDBODY_NOT == n->end)
			(*mdocs[n->tok].post)(m, n, h);
		break;
	}
}

/* ARGSUSED */
static void
mdoc_root_post(MDOC_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;

	PAIR_SUMMARY_INIT(&tag[0], "Document Footer");
	PAIR_CLASS_INIT(&tag[1], "foot");
	PAIR_INIT(&tag[2], ATTR_WIDTH, "100%");
	t = print_otag(h, TAG_TABLE, 3, tag);
	PAIR_INIT(&tag[0], ATTR_WIDTH, "50%");
	print_otag(h, TAG_COL, 1, tag);
	print_otag(h, TAG_COL, 1, tag);

	print_otag(h, TAG_TBODY, 0, NULL);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag[0], "foot-date");
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, m->date);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "foot-os");
	PAIR_INIT(&tag[1], ATTR_ALIGN, "right");
	print_otag(h, TAG_TD, 2, tag);
	print_text(h, m->os);
	print_tagq(h, t);
}


/* ARGSUSED */
static int
mdoc_root_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;
	char		 b[BUFSIZ], title[BUFSIZ];

	strlcpy(b, m->vol, BUFSIZ);

	if (m->arch) {
		strlcat(b, " (", BUFSIZ);
		strlcat(b, m->arch, BUFSIZ);
		strlcat(b, ")", BUFSIZ);
	}

	snprintf(title, BUFSIZ - 1, "%s(%s)", m->title, m->msec);

	PAIR_SUMMARY_INIT(&tag[0], "Document Header");
	PAIR_CLASS_INIT(&tag[1], "head");
	PAIR_INIT(&tag[2], ATTR_WIDTH, "100%");
	t = print_otag(h, TAG_TABLE, 3, tag);
	PAIR_INIT(&tag[0], ATTR_WIDTH, "30%");
	print_otag(h, TAG_COL, 1, tag);
	print_otag(h, TAG_COL, 1, tag);
	print_otag(h, TAG_COL, 1, tag);

	print_otag(h, TAG_TBODY, 0, NULL);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag[0], "head-ltitle");
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, title);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "head-vol");
	PAIR_INIT(&tag[1], ATTR_ALIGN, "center");
	print_otag(h, TAG_TD, 2, tag);
	print_text(h, b);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "head-rtitle");
	PAIR_INIT(&tag[1], ATTR_ALIGN, "right");
	print_otag(h, TAG_TD, 2, tag);
	print_text(h, title);
	print_tagq(h, t);
	return(1);
}


/* ARGSUSED */
static int
mdoc_sh_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag, "section");
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	} else if (MDOC_BODY == n->type)
		return(1);

	bufinit(h);
	bufcat(h, "x");

	for (n = n->child; n && MDOC_TEXT == n->type; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n) {
		PAIR_ID_INIT(&tag, h->buf);
		print_otag(h, TAG_H1, 1, &tag);
	} else
		print_otag(h, TAG_H1, 0, NULL);

	return(1);
}

/* ARGSUSED */
static int
mdoc_ss_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag, "subsection");
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	} else if (MDOC_BODY == n->type)
		return(1);

	bufinit(h);
	bufcat(h, "x");

	for (n = n->child; n && MDOC_TEXT == n->type; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n) {
		PAIR_ID_INIT(&tag, h->buf);
		print_otag(h, TAG_H2, 1, &tag);
	} else
		print_otag(h, TAG_H2, 0, NULL);

	return(1);
}


/* ARGSUSED */
static int
mdoc_fl_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	PAIR_CLASS_INIT(&tag, "flag");
	print_otag(h, TAG_B, 1, &tag);

	/* `Cm' has no leading hyphen. */

	if (MDOC_Cm == n->tok)
		return(1);

	print_text(h, "\\-");

	if (n->child)
		h->flags |= HTML_NOSPACE;
	else if (n->next && n->next->line == n->line)
		h->flags |= HTML_NOSPACE;

	return(1);
}


/* ARGSUSED */
static int
mdoc_nd_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BODY != n->type)
		return(1);

	/* XXX: this tag in theory can contain block elements. */

	print_text(h, "\\(em");
	PAIR_CLASS_INIT(&tag, "desc");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


static int
mdoc_nm_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	int		 len;

	switch (n->type) {
	case (MDOC_ELEM):
		synopsis_pre(h, n);
		PAIR_CLASS_INIT(&tag, "name");
		print_otag(h, TAG_B, 1, &tag);
		if (NULL == n->child && m->name)
			print_text(h, m->name);
		return(1);
	case (MDOC_HEAD):
		print_otag(h, TAG_TD, 0, NULL);
		if (NULL == n->child && m->name)
			print_text(h, m->name);
		return(1);
	case (MDOC_BODY):
		print_otag(h, TAG_TD, 0, NULL);
		return(1);
	default:
		break;
	}

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "synopsis");
	print_otag(h, TAG_TABLE, 1, &tag);

	for (len = 0, n = n->child; n; n = n->next)
		if (MDOC_TEXT == n->type)
			len += html_strlen(n->string);

	if (0 == len && m->name)
		len = html_strlen(m->name);

	SCALE_HS_INIT(&su, (double)len);
	bufinit(h);
	bufcat_su(h, "width", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_COL, 1, &tag);
	print_otag(h, TAG_COL, 0, NULL);
	print_otag(h, TAG_TBODY, 0, NULL);
	print_otag(h, TAG_TR, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
mdoc_xr_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	if (NULL == n->child)
		return(0);

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
		return(0);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;
	print_text(h, n->string);
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	return(0);
}


/* ARGSUSED */
static int
mdoc_ns_pre(MDOC_ARGS)
{

	if ( ! (MDOC_LINE & n->flags))
		h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
mdoc_ar_pre(MDOC_ARGS)
{
	struct htmlpair tag;

	PAIR_CLASS_INIT(&tag, "arg");
	print_otag(h, TAG_I, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_xx_pre(MDOC_ARGS)
{
	const char	*pp;
	struct htmlpair	 tag;
	int		 flags;

	switch (n->tok) {
	case (MDOC_Bsx):
		pp = "BSD/OS";
		break;
	case (MDOC_Dx):
		pp = "DragonFly";
		break;
	case (MDOC_Fx):
		pp = "FreeBSD";
		break;
	case (MDOC_Nx):
		pp = "NetBSD";
		break;
	case (MDOC_Ox):
		pp = "OpenBSD";
		break;
	case (MDOC_Ux):
		pp = "UNIX";
		break;
	default:
		return(1);
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
	return(0);
}


/* ARGSUSED */
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
		return(0);
	}

	if (NULL != (n = n->next)) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "-");
		h->flags |= HTML_NOSPACE;
		print_text(h, n->string);
	}

	return(0);
}

/* ARGSUSED */
static int
mdoc_it_pre(MDOC_ARGS)
{
	struct roffsu	 su;
	enum mdoc_list	 type;
	struct htmlpair	 tag[2];
	const struct mdoc_node *bl;

	bl = n->parent;
	while (bl && MDOC_Bl != bl->tok)
		bl = bl->parent;

	assert(bl);

	type = bl->norm->Bl.type;

	assert(lists[type]);
	PAIR_CLASS_INIT(&tag[0], lists[type]);

	bufinit(h);

	if (MDOC_HEAD == n->type) {
		switch (type) {
		case(LIST_bullet):
			/* FALLTHROUGH */
		case(LIST_dash):
			/* FALLTHROUGH */
		case(LIST_item):
			/* FALLTHROUGH */
		case(LIST_hyphen):
			/* FALLTHROUGH */
		case(LIST_enum):
			return(0);
		case(LIST_diag):
			/* FALLTHROUGH */
		case(LIST_hang):
			/* FALLTHROUGH */
		case(LIST_inset):
			/* FALLTHROUGH */
		case(LIST_ohang):
			/* FALLTHROUGH */
		case(LIST_tag):
			SCALE_VS_INIT(&su, ! bl->norm->Bl.comp);
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DT, 2, tag);
			if (LIST_diag != type)
				break;
			PAIR_CLASS_INIT(&tag[0], "diag");
			print_otag(h, TAG_B, 1, tag);
			break;
		case(LIST_column):
			break;
		default:
			break;
		}
	} else if (MDOC_BODY == n->type) {
		switch (type) {
		case(LIST_bullet):
			/* FALLTHROUGH */
		case(LIST_hyphen):
			/* FALLTHROUGH */
		case(LIST_dash):
			/* FALLTHROUGH */
		case(LIST_enum):
			/* FALLTHROUGH */
		case(LIST_item):
			SCALE_VS_INIT(&su, ! bl->norm->Bl.comp);
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_LI, 2, tag);
			break;
		case(LIST_diag):
			/* FALLTHROUGH */
		case(LIST_hang):
			/* FALLTHROUGH */
		case(LIST_inset):
			/* FALLTHROUGH */
		case(LIST_ohang):
			/* FALLTHROUGH */
		case(LIST_tag):
			if (NULL == bl->norm->Bl.width) {
				print_otag(h, TAG_DD, 1, tag);
				break;
			}
			a2width(bl->norm->Bl.width, &su);
			bufcat_su(h, "margin-left", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DD, 2, tag);
			break;
		case(LIST_column):
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
		case (LIST_column):
			print_otag(h, TAG_TR, 1, tag);
			break;
		default:
			break;
		}
	}

	return(1);
}

/* ARGSUSED */
static int
mdoc_bl_pre(MDOC_ARGS)
{
	int		 i;
	struct htmlpair	 tag[3];
	struct roffsu	 su;
	char		 buf[BUFSIZ];

	bufinit(h);

	if (MDOC_BODY == n->type) {
		if (LIST_column == n->norm->Bl.type)
			print_otag(h, TAG_TBODY, 0, NULL);
		return(1);
	}

	if (MDOC_HEAD == n->type) {
		if (LIST_column != n->norm->Bl.type)
			return(0);

		/*
		 * For each column, print out the <COL> tag with our
		 * suggested width.  The last column gets min-width, as
		 * in terminal mode it auto-sizes to the width of the
		 * screen and we want to preserve that behaviour.
		 */

		for (i = 0; i < (int)n->norm->Bl.ncols; i++) {
			a2width(n->norm->Bl.cols[i], &su);
			if (i < (int)n->norm->Bl.ncols - 1)
				bufcat_su(h, "width", &su);
			else
				bufcat_su(h, "min-width", &su);
			PAIR_STYLE_INIT(&tag[0], h);
			print_otag(h, TAG_COL, 1, tag);
		}

		return(0);
	}

	SCALE_VS_INIT(&su, 0);
	bufcat_su(h, "margin-top", &su);
	bufcat_su(h, "margin-bottom", &su);
	PAIR_STYLE_INIT(&tag[0], h);

	assert(lists[n->norm->Bl.type]);
	strlcpy(buf, "list ", BUFSIZ);
	strlcat(buf, lists[n->norm->Bl.type], BUFSIZ);
	PAIR_INIT(&tag[1], ATTR_CLASS, buf);

	/* Set the block's left-hand margin. */

	if (n->norm->Bl.offs) {
		a2offs(n->norm->Bl.offs, &su);
		bufcat_su(h, "margin-left", &su);
	}

	switch (n->norm->Bl.type) {
	case(LIST_bullet):
		/* FALLTHROUGH */
	case(LIST_dash):
		/* FALLTHROUGH */
	case(LIST_hyphen):
		/* FALLTHROUGH */
	case(LIST_item):
		print_otag(h, TAG_UL, 2, tag);
		break;
	case(LIST_enum):
		print_otag(h, TAG_OL, 2, tag);
		break;
	case(LIST_diag):
		/* FALLTHROUGH */
	case(LIST_hang):
		/* FALLTHROUGH */
	case(LIST_inset):
		/* FALLTHROUGH */
	case(LIST_ohang):
		/* FALLTHROUGH */
	case(LIST_tag):
		print_otag(h, TAG_DL, 2, tag);
		break;
	case(LIST_column):
		print_otag(h, TAG_TABLE, 2, tag);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}

/* ARGSUSED */
static int
mdoc_ex_pre(MDOC_ARGS)
{
	struct tag	*t;
	struct htmlpair	 tag;
	int		 nchild;

	if (n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "utility");

	print_text(h, "The");

	nchild = n->nchild;
	for (n = n->child; n; n = n->next) {
		assert(MDOC_TEXT == n->type);

		t = print_otag(h, TAG_B, 1, &tag);
		print_text(h, n->string);
		print_tagq(h, t);

		if (nchild > 2 && n->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}

		if (n->next && NULL == n->next->next)
			print_text(h, "and");
	}

	if (nchild > 1)
		print_text(h, "utilities exit");
	else
		print_text(h, "utility exits");

       	print_text(h, "0 on success, and >0 if an error occurs.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_em_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "emph");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_d1_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (MDOC_BLOCK != n->type)
		return(1);

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

	return(1);
}


/* ARGSUSED */
static int
mdoc_sx_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	bufinit(h);
	bufcat(h, "#x");

	for (n = n->child; n; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	PAIR_CLASS_INIT(&tag[0], "link-sec");
	PAIR_HREF_INIT(&tag[1], h->buf);

	print_otag(h, TAG_I, 1, tag);
	print_otag(h, TAG_A, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_bd_pre(MDOC_ARGS)
{
	struct htmlpair	 	 tag[2];
	int		 	 comp, sv;
	const struct mdoc_node	*nn;
	struct roffsu		 su;

	if (MDOC_HEAD == n->type)
		return(0);

	if (MDOC_BLOCK == n->type) {
		comp = n->norm->Bd.comp;
		for (nn = n; nn && ! comp; nn = nn->parent) {
			if (MDOC_BLOCK != nn->type)
				continue;
			if (MDOC_Ss == nn->tok || MDOC_Sh == nn->tok)
				comp = 1;
			if (nn->prev)
				break;
		}
		if ( ! comp)
			print_otag(h, TAG_P, 0, NULL);
		return(1);
	}

	SCALE_HS_INIT(&su, 0);
	if (n->norm->Bd.offs)
		a2offs(n->norm->Bd.offs, &su);
	
	bufinit(h);
	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag[0], h);

	if (DISP_unfilled != n->norm->Bd.type && 
			DISP_literal != n->norm->Bd.type) {
		PAIR_CLASS_INIT(&tag[1], "display");
		print_otag(h, TAG_DIV, 2, tag);
		return(1);
	}

	PAIR_CLASS_INIT(&tag[1], "lit display");
	print_otag(h, TAG_PRE, 2, tag);

	/* This can be recursive: save & set our literal state. */

	sv = h->flags & HTML_LITERAL;
	h->flags |= HTML_LITERAL;

	for (nn = n->child; nn; nn = nn->next) {
		print_mdoc_node(m, nn, h);
		/*
		 * If the printed node flushes its own line, then we
		 * needn't do it here as well.  This is hacky, but the
		 * notion of selective eoln whitespace is pretty dumb
		 * anyway, so don't sweat it.
		 */
		switch (nn->tok) {
		case (MDOC_Sm):
			/* FALLTHROUGH */
		case (MDOC_br):
			/* FALLTHROUGH */
		case (MDOC_sp):
			/* FALLTHROUGH */
		case (MDOC_Bl):
			/* FALLTHROUGH */
		case (MDOC_D1):
			/* FALLTHROUGH */
		case (MDOC_Dl):
			/* FALLTHROUGH */
		case (MDOC_Lp):
			/* FALLTHROUGH */
		case (MDOC_Pp):
			continue;
		default:
			break;
		}
		if (nn->next && nn->next->line == nn->line)
			continue;
		else if (nn->next)
			print_text(h, "\n");

		h->flags |= HTML_NOSPACE;
	}

	if (0 == sv)
		h->flags &= ~HTML_LITERAL;

	return(0);
}


/* ARGSUSED */
static int
mdoc_pa_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "file");
	print_otag(h, TAG_I, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ad_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "addr");
	print_otag(h, TAG_I, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_an_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	/* TODO: -split and -nosplit (see termp_an_pre()). */

	PAIR_CLASS_INIT(&tag, "author");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_cd_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "config");
	print_otag(h, TAG_B, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_dv_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "define");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ev_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "env");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_er_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "errno");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_fa_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct htmlpair		 tag;
	struct tag		*t;

	PAIR_CLASS_INIT(&tag, "farg");
	if (n->parent->tok != MDOC_Fo) {
		print_otag(h, TAG_I, 1, &tag);
		return(1);
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

	return(0);
}


/* ARGSUSED */
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
		return(0);

	assert(MDOC_TEXT == n->type);

	if (strcmp(n->string, "#include")) {
		PAIR_CLASS_INIT(&tag[0], "macro");
		print_otag(h, TAG_B, 1, tag);
		return(1);
	}

	PAIR_CLASS_INIT(&tag[0], "includes");
	print_otag(h, TAG_B, 1, tag);
	print_text(h, n->string);

	if (NULL != (n = n->next)) {
		assert(MDOC_TEXT == n->type);
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
		assert(MDOC_TEXT == n->type);
		print_text(h, n->string);
	}

	return(0);
}


/* ARGSUSED */
static int
mdoc_vt_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK == n->type) {
		synopsis_pre(h, n);
		return(1);
	} else if (MDOC_ELEM == n->type) {
		synopsis_pre(h, n);
	} else if (MDOC_HEAD == n->type)
		return(0);

	PAIR_CLASS_INIT(&tag, "type");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ft_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "ftype");
	print_otag(h, TAG_I, 1, &tag);
	return(1);
}


/* ARGSUSED */
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

	if (sp) {
		strlcpy(nbuf, sp, BUFSIZ);
		print_text(h, nbuf);
	}

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

	return(0);
}


/* ARGSUSED */
static int
mdoc_sm_pre(MDOC_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string)) {
		/* 
		 * FIXME: no p->col to check.  Thus, if we have
		 *  .Bd -literal
		 *  .Sm off
		 *  1 2
		 *  .Sm on
		 *  3
		 *  .Ed
		 * the "3" is preceded by a space.
		 */
		h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONOSPACE;
	} else
		h->flags |= HTML_NONOSPACE;

	return(0);
}

/* ARGSUSED */
static int
mdoc_pp_pre(MDOC_ARGS)
{

	print_otag(h, TAG_P, 0, NULL);
	return(0);

}

/* ARGSUSED */
static int
mdoc_sp_pre(MDOC_ARGS)
{
	struct roffsu	 su;
	struct htmlpair	 tag;

	SCALE_VS_INIT(&su, 1);

	if (MDOC_sp == n->tok) {
		if (NULL != (n = n->child))
			if ( ! a2roffsu(n->string, &su, SCALE_VS))
				SCALE_VS_INIT(&su, atoi(n->string));
	} else
		su.scale = 0;

	bufinit(h);
	bufcat_su(h, "height", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);

	/* So the div isn't empty: */
	print_text(h, "\\~");

	return(0);

}

/* ARGSUSED */
static int
mdoc_lk_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];

	if (NULL == (n = n->child))
		return(0);

	assert(MDOC_TEXT == n->type);

	PAIR_CLASS_INIT(&tag[0], "link-ext");
	PAIR_HREF_INIT(&tag[1], n->string);

	print_otag(h, TAG_A, 2, tag);

	if (NULL == n->next)
		print_text(h, n->string);

	for (n = n->next; n; n = n->next)
		print_text(h, n->string);

	return(0);
}


/* ARGSUSED */
static int
mdoc_mt_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct tag	*t;

	PAIR_CLASS_INIT(&tag[0], "link-mail");

	for (n = n->child; n; n = n->next) {
		assert(MDOC_TEXT == n->type);

		bufinit(h);
		bufcat(h, "mailto:");
		bufcat(h, n->string);

		PAIR_HREF_INIT(&tag[1], h->buf);
		t = print_otag(h, TAG_A, 2, tag);
		print_text(h, n->string);
		print_tagq(h, t);
	}
	
	return(0);
}


/* ARGSUSED */
static int
mdoc_fo_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t;

	if (MDOC_BODY == n->type) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "(");
		h->flags |= HTML_NOSPACE;
		return(1);
	} else if (MDOC_BLOCK == n->type) {
		synopsis_pre(h, n);
		return(1);
	}

	/* XXX: we drop non-initial arguments as per groff. */

	assert(n->child);
	assert(n->child->string);

	PAIR_CLASS_INIT(&tag, "fname");
	t = print_otag(h, TAG_B, 1, &tag);
	print_text(h, n->child->string);
	print_tagq(h, t);
	return(0);
}


/* ARGSUSED */
static void
mdoc_fo_post(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	h->flags |= HTML_NOSPACE;
	print_text(h, ";");
}


/* ARGSUSED */
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
		assert(MDOC_TEXT == n->type);

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
		assert(MDOC_TEXT == n->type);
		print_text(h, n->string);
	}

	return(0);
}


/* ARGSUSED */
static int
mdoc_ic_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "cmd");
	print_otag(h, TAG_B, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_rv_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t;
	int		 nchild;

	if (n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "fname");

	print_text(h, "The");

	nchild = n->nchild;
	for (n = n->child; n; n = n->next) {
		assert(MDOC_TEXT == n->type);

		t = print_otag(h, TAG_B, 1, &tag);
		print_text(h, n->string);
		print_tagq(h, t);

		h->flags |= HTML_NOSPACE;
		print_text(h, "()");

		if (nchild > 2 && n->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}

		if (n->next && NULL == n->next->next)
			print_text(h, "and");
	}

	if (nchild > 1)
		print_text(h, "functions return");
	else
		print_text(h, "function returns");

       	print_text(h, "the value 0 if successful; otherwise the value "
			"-1 is returned and the global variable");

	PAIR_CLASS_INIT(&tag, "var");
	t = print_otag(h, TAG_B, 1, &tag);
	print_text(h, "errno");
	print_tagq(h, t);
       	print_text(h, "is set to indicate the error.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_va_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "var");
	print_otag(h, TAG_B, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ap_pre(MDOC_ARGS)
{
	
	h->flags |= HTML_NOSPACE;
	print_text(h, "\\(aq");
	h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
mdoc_bf_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (MDOC_HEAD == n->type)
		return(0);
	else if (MDOC_BODY != n->type)
		return(1);

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
	return(1);
}


/* ARGSUSED */
static int
mdoc_ms_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_igndelim_pre(MDOC_ARGS)
{

	h->flags |= HTML_IGNDELIM;
	return(1);
}


/* ARGSUSED */
static void
mdoc_pf_post(MDOC_ARGS)
{

	h->flags |= HTML_NOSPACE;
}


/* ARGSUSED */
static int
mdoc_rs_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK != n->type)
		return(1);

	if (n->prev && SEC_SEE_ALSO == n->sec)
		print_otag(h, TAG_P, 0, NULL);

	PAIR_CLASS_INIT(&tag, "ref");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}



/* ARGSUSED */
static int
mdoc_li_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "lit");
	print_otag(h, TAG_CODE, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_sy_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_bt_pre(MDOC_ARGS)
{

	print_text(h, "is currently in beta test.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_ud_pre(MDOC_ARGS)
{

	print_text(h, "currently under development.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_lb_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	if (SEC_LIBRARY == n->sec && MDOC_LINE & n->flags && n->prev)
		print_otag(h, TAG_BR, 0, NULL);

	PAIR_CLASS_INIT(&tag, "lib");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc__x_pre(MDOC_ARGS)
{
	struct htmlpair	tag[2];
	enum htmltag	t;

	t = TAG_SPAN;

	switch (n->tok) {
	case(MDOC__A):
		PAIR_CLASS_INIT(&tag[0], "ref-auth");
		if (n->prev && MDOC__A == n->prev->tok)
			if (NULL == n->next || MDOC__A != n->next->tok)
				print_text(h, "and");
		break;
	case(MDOC__B):
		PAIR_CLASS_INIT(&tag[0], "ref-book");
		t = TAG_I;
		break;
	case(MDOC__C):
		PAIR_CLASS_INIT(&tag[0], "ref-city");
		break;
	case(MDOC__D):
		PAIR_CLASS_INIT(&tag[0], "ref-date");
		break;
	case(MDOC__I):
		PAIR_CLASS_INIT(&tag[0], "ref-issue");
		t = TAG_I;
		break;
	case(MDOC__J):
		PAIR_CLASS_INIT(&tag[0], "ref-jrnl");
		t = TAG_I;
		break;
	case(MDOC__N):
		PAIR_CLASS_INIT(&tag[0], "ref-num");
		break;
	case(MDOC__O):
		PAIR_CLASS_INIT(&tag[0], "ref-opt");
		break;
	case(MDOC__P):
		PAIR_CLASS_INIT(&tag[0], "ref-page");
		break;
	case(MDOC__Q):
		PAIR_CLASS_INIT(&tag[0], "ref-corp");
		break;
	case(MDOC__R):
		PAIR_CLASS_INIT(&tag[0], "ref-rep");
		break;
	case(MDOC__T):
		PAIR_CLASS_INIT(&tag[0], "ref-title");
		break;
	case(MDOC__U):
		PAIR_CLASS_INIT(&tag[0], "link-ref");
		break;
	case(MDOC__V):
		PAIR_CLASS_INIT(&tag[0], "ref-vol");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (MDOC__U != n->tok) {
		print_otag(h, t, 1, tag);
		return(1);
	}

	PAIR_HREF_INIT(&tag[1], n->child->string);
	print_otag(h, TAG_A, 2, tag);

	return(1);
}


/* ARGSUSED */
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


/* ARGSUSED */
static int
mdoc_bk_pre(MDOC_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		break;
	case (MDOC_HEAD):
		return(0);
	case (MDOC_BODY):
		if (n->parent->args || 0 == n->prev->nchild)
			h->flags |= HTML_PREKEEP;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}


/* ARGSUSED */
static void
mdoc_bk_post(MDOC_ARGS)
{

	if (MDOC_BODY == n->type)
		h->flags &= ~(HTML_KEEP | HTML_PREKEEP);
}


/* ARGSUSED */
static int
mdoc_quote_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	if (MDOC_BODY != n->type)
		return(1);

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		print_text(h, "\\(la");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		print_text(h, "\\(lC");
		break;
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		print_text(h, "\\(lB");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		print_text(h, "\\(lB");
		h->flags |= HTML_NOSPACE;
		PAIR_CLASS_INIT(&tag, "opt");
		print_otag(h, TAG_SPAN, 1, &tag);
		break;
	case (MDOC_Eo):
		break;
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		print_text(h, "\\(lq");
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		print_text(h, "(");
		break;
	case (MDOC_Ql):
		print_text(h, "\\(oq");
		h->flags |= HTML_NOSPACE;
		PAIR_CLASS_INIT(&tag, "lit");
		print_otag(h, TAG_CODE, 1, &tag);
		break;
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		print_text(h, "\\(oq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
mdoc_quote_post(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return;

	h->flags |= HTML_NOSPACE;

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		print_text(h, "\\(ra");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		print_text(h, "\\(rC");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		print_text(h, "\\(rB");
		break;
	case (MDOC_Eo):
		break;
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		/* FALLTHROUGH */
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		print_text(h, "\\(rq");
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		print_text(h, ")");
		break;
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		print_text(h, "\\(aq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}


