/* $Id: mdoc_term.c,v 1.387 2025/07/27 15:27:28 schwarze Exp $ */
/*
 * Copyright (c) 2010,2012-2020,2022,2025 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013 Franco Fichtner <franco@lastsummer.de>
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
 * Plain text formatter for mdoc(7), used by mandoc(1)
 * for ASCII, UTF-8, PostScript, and PDF output.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "roff.h"
#include "mdoc.h"
#include "out.h"
#include "term.h"
#include "term_tag.h"
#include "main.h"

struct	termpair {
	struct termpair	 *ppair;
	int		  count;
};

#define	DECL_ARGS struct termp *p, \
		  struct termpair *pair, \
		  const struct roff_meta *meta, \
		  struct roff_node *n

struct	mdoc_term_act {
	int	(*pre)(DECL_ARGS);
	void	(*post)(DECL_ARGS);
};

static	int	  a2width(const struct termp *, const char *);

static	void	  print_bvspace(struct termp *,
			struct roff_node *, struct roff_node *);
static	void	  print_mdoc_node(DECL_ARGS);
static	void	  print_mdoc_nodelist(DECL_ARGS);
static	void	  print_mdoc_head(struct termp *, const struct roff_meta *);
static	void	  print_mdoc_foot(struct termp *, const struct roff_meta *);
static	void	  synopsis_pre(struct termp *, struct roff_node *);

static	void	  termp____post(DECL_ARGS);
static	void	  termp__t_post(DECL_ARGS);
static	void	  termp_bd_post(DECL_ARGS);
static	void	  termp_bk_post(DECL_ARGS);
static	void	  termp_bl_post(DECL_ARGS);
static	void	  termp_eo_post(DECL_ARGS);
static	void	  termp_fd_post(DECL_ARGS);
static	void	  termp_fo_post(DECL_ARGS);
static	void	  termp_in_post(DECL_ARGS);
static	void	  termp_it_post(DECL_ARGS);
static	void	  termp_lb_post(DECL_ARGS);
static	void	  termp_nm_post(DECL_ARGS);
static	void	  termp_pf_post(DECL_ARGS);
static	void	  termp_quote_post(DECL_ARGS);
static	void	  termp_sh_post(DECL_ARGS);
static	void	  termp_ss_post(DECL_ARGS);
static	void	  termp_xx_post(DECL_ARGS);

static	int	  termp__a_pre(DECL_ARGS);
static	int	  termp__t_pre(DECL_ARGS);
static	int	  termp_abort_pre(DECL_ARGS);
static	int	  termp_an_pre(DECL_ARGS);
static	int	  termp_ap_pre(DECL_ARGS);
static	int	  termp_bd_pre(DECL_ARGS);
static	int	  termp_bf_pre(DECL_ARGS);
static	int	  termp_bk_pre(DECL_ARGS);
static	int	  termp_bl_pre(DECL_ARGS);
static	int	  termp_bold_pre(DECL_ARGS);
static	int	  termp_d1_pre(DECL_ARGS);
static	int	  termp_eo_pre(DECL_ARGS);
static	int	  termp_ex_pre(DECL_ARGS);
static	int	  termp_fa_pre(DECL_ARGS);
static	int	  termp_fd_pre(DECL_ARGS);
static	int	  termp_fl_pre(DECL_ARGS);
static	int	  termp_fn_pre(DECL_ARGS);
static	int	  termp_fo_pre(DECL_ARGS);
static	int	  termp_ft_pre(DECL_ARGS);
static	int	  termp_in_pre(DECL_ARGS);
static	int	  termp_it_pre(DECL_ARGS);
static	int	  termp_li_pre(DECL_ARGS);
static	int	  termp_lk_pre(DECL_ARGS);
static	int	  termp_nd_pre(DECL_ARGS);
static	int	  termp_nm_pre(DECL_ARGS);
static	int	  termp_ns_pre(DECL_ARGS);
static	int	  termp_quote_pre(DECL_ARGS);
static	int	  termp_rs_pre(DECL_ARGS);
static	int	  termp_sh_pre(DECL_ARGS);
static	int	  termp_skip_pre(DECL_ARGS);
static	int	  termp_sm_pre(DECL_ARGS);
static	int	  termp_pp_pre(DECL_ARGS);
static	int	  termp_ss_pre(DECL_ARGS);
static	int	  termp_under_pre(DECL_ARGS);
static	int	  termp_vt_pre(DECL_ARGS);
static	int	  termp_xr_pre(DECL_ARGS);
static	int	  termp_xx_pre(DECL_ARGS);

static const struct mdoc_term_act mdoc_term_acts[MDOC_MAX - MDOC_Dd] = {
	{ NULL, NULL }, /* Dd */
	{ NULL, NULL }, /* Dt */
	{ NULL, NULL }, /* Os */
	{ termp_sh_pre, termp_sh_post }, /* Sh */
	{ termp_ss_pre, termp_ss_post }, /* Ss */
	{ termp_pp_pre, NULL }, /* Pp */
	{ termp_d1_pre, termp_bl_post }, /* D1 */
	{ termp_d1_pre, termp_bl_post }, /* Dl */
	{ termp_bd_pre, termp_bd_post }, /* Bd */
	{ NULL, NULL }, /* Ed */
	{ termp_bl_pre, termp_bl_post }, /* Bl */
	{ NULL, NULL }, /* El */
	{ termp_it_pre, termp_it_post }, /* It */
	{ termp_under_pre, NULL }, /* Ad */
	{ termp_an_pre, NULL }, /* An */
	{ termp_ap_pre, NULL }, /* Ap */
	{ termp_under_pre, NULL }, /* Ar */
	{ termp_fd_pre, NULL }, /* Cd */
	{ termp_bold_pre, NULL }, /* Cm */
	{ termp_li_pre, NULL }, /* Dv */
	{ NULL, NULL }, /* Er */
	{ NULL, NULL }, /* Ev */
	{ termp_ex_pre, NULL }, /* Ex */
	{ termp_fa_pre, NULL }, /* Fa */
	{ termp_fd_pre, termp_fd_post }, /* Fd */
	{ termp_fl_pre, NULL }, /* Fl */
	{ termp_fn_pre, NULL }, /* Fn */
	{ termp_ft_pre, NULL }, /* Ft */
	{ termp_bold_pre, NULL }, /* Ic */
	{ termp_in_pre, termp_in_post }, /* In */
	{ termp_li_pre, NULL }, /* Li */
	{ termp_nd_pre, NULL }, /* Nd */
	{ termp_nm_pre, termp_nm_post }, /* Nm */
	{ termp_quote_pre, termp_quote_post }, /* Op */
	{ termp_abort_pre, NULL }, /* Ot */
	{ termp_under_pre, NULL }, /* Pa */
	{ termp_ex_pre, NULL }, /* Rv */
	{ NULL, NULL }, /* St */
	{ termp_under_pre, NULL }, /* Va */
	{ termp_vt_pre, NULL }, /* Vt */
	{ termp_xr_pre, NULL }, /* Xr */
	{ termp__a_pre, termp____post }, /* %A */
	{ termp_under_pre, termp____post }, /* %B */
	{ NULL, termp____post }, /* %D */
	{ termp_under_pre, termp____post }, /* %I */
	{ termp_under_pre, termp____post }, /* %J */
	{ NULL, termp____post }, /* %N */
	{ NULL, termp____post }, /* %O */
	{ NULL, termp____post }, /* %P */
	{ NULL, termp____post }, /* %R */
	{ termp__t_pre, termp__t_post }, /* %T */
	{ NULL, termp____post }, /* %V */
	{ NULL, NULL }, /* Ac */
	{ termp_quote_pre, termp_quote_post }, /* Ao */
	{ termp_quote_pre, termp_quote_post }, /* Aq */
	{ NULL, NULL }, /* At */
	{ NULL, NULL }, /* Bc */
	{ termp_bf_pre, NULL }, /* Bf */
	{ termp_quote_pre, termp_quote_post }, /* Bo */
	{ termp_quote_pre, termp_quote_post }, /* Bq */
	{ termp_xx_pre, termp_xx_post }, /* Bsx */
	{ NULL, NULL }, /* Bx */
	{ termp_skip_pre, NULL }, /* Db */
	{ NULL, NULL }, /* Dc */
	{ termp_quote_pre, termp_quote_post }, /* Do */
	{ termp_quote_pre, termp_quote_post }, /* Dq */
	{ NULL, NULL }, /* Ec */ /* FIXME: no space */
	{ NULL, NULL }, /* Ef */
	{ termp_under_pre, NULL }, /* Em */
	{ termp_eo_pre, termp_eo_post }, /* Eo */
	{ termp_xx_pre, termp_xx_post }, /* Fx */
	{ termp_bold_pre, NULL }, /* Ms */
	{ termp_li_pre, NULL }, /* No */
	{ termp_ns_pre, NULL }, /* Ns */
	{ termp_xx_pre, termp_xx_post }, /* Nx */
	{ termp_xx_pre, termp_xx_post }, /* Ox */
	{ NULL, NULL }, /* Pc */
	{ NULL, termp_pf_post }, /* Pf */
	{ termp_quote_pre, termp_quote_post }, /* Po */
	{ termp_quote_pre, termp_quote_post }, /* Pq */
	{ NULL, NULL }, /* Qc */
	{ termp_quote_pre, termp_quote_post }, /* Ql */
	{ termp_quote_pre, termp_quote_post }, /* Qo */
	{ termp_quote_pre, termp_quote_post }, /* Qq */
	{ NULL, NULL }, /* Re */
	{ termp_rs_pre, NULL }, /* Rs */
	{ NULL, NULL }, /* Sc */
	{ termp_quote_pre, termp_quote_post }, /* So */
	{ termp_quote_pre, termp_quote_post }, /* Sq */
	{ termp_sm_pre, NULL }, /* Sm */
	{ termp_under_pre, NULL }, /* Sx */
	{ termp_bold_pre, NULL }, /* Sy */
	{ NULL, NULL }, /* Tn */
	{ termp_xx_pre, termp_xx_post }, /* Ux */
	{ NULL, NULL }, /* Xc */
	{ NULL, NULL }, /* Xo */
	{ termp_fo_pre, termp_fo_post }, /* Fo */
	{ NULL, NULL }, /* Fc */
	{ termp_quote_pre, termp_quote_post }, /* Oo */
	{ NULL, NULL }, /* Oc */
	{ termp_bk_pre, termp_bk_post }, /* Bk */
	{ NULL, NULL }, /* Ek */
	{ NULL, NULL }, /* Bt */
	{ NULL, NULL }, /* Hf */
	{ termp_under_pre, NULL }, /* Fr */
	{ NULL, NULL }, /* Ud */
	{ NULL, termp_lb_post }, /* Lb */
	{ termp_abort_pre, NULL }, /* Lp */
	{ termp_lk_pre, NULL }, /* Lk */
	{ termp_under_pre, NULL }, /* Mt */
	{ termp_quote_pre, termp_quote_post }, /* Brq */
	{ termp_quote_pre, termp_quote_post }, /* Bro */
	{ NULL, NULL }, /* Brc */
	{ NULL, termp____post }, /* %C */
	{ termp_skip_pre, NULL }, /* Es */
	{ termp_quote_pre, termp_quote_post }, /* En */
	{ termp_xx_pre, termp_xx_post }, /* Dx */
	{ NULL, termp____post }, /* %Q */
	{ NULL, termp____post }, /* %U */
	{ NULL, NULL }, /* Ta */
	{ termp_skip_pre, NULL }, /* Tg */
};


void
terminal_mdoc(void *arg, const struct roff_meta *mdoc)
{
	struct roff_node	*n, *nn;
	struct termp		*p;

	p = (struct termp *)arg;
	p->tcol->rmargin = p->maxrmargin = p->defrmargin;
	term_tab_set(p, NULL);
	term_tab_set(p, "T");
	term_tab_set(p, ".5i");

	n = mdoc->first->child;
	if (p->synopsisonly) {
		for (nn = NULL; n != NULL; n = n->next) {
			if (n->tok != MDOC_Sh)
				continue;
			if (n->sec == SEC_SYNOPSIS)
				break;
			if (nn == NULL && n->sec == SEC_NAME)
				nn = n;
		}
		if (n == NULL)
			n = nn;
		p->flags |= TERMP_NOSPACE;
		if (n != NULL && (n = n->child->next->child) != NULL)
			print_mdoc_nodelist(p, NULL, mdoc, n);
		term_newln(p);
	} else {
		term_begin(p, print_mdoc_head, print_mdoc_foot, mdoc);
		while (n != NULL &&
		    (n->type == ROFFT_COMMENT ||
		     n->flags & NODE_NOPRT))
			n = n->next;
		if (n != NULL) {
			if (n->tok != MDOC_Sh)
				term_vspace(p);
			print_mdoc_nodelist(p, NULL, mdoc, n);
		}
		term_end(p);
	}
}

static void
print_mdoc_nodelist(DECL_ARGS)
{
	while (n != NULL) {
		print_mdoc_node(p, pair, meta, n);
		n = n->next;
	}
}

static void
print_mdoc_node(DECL_ARGS)
{
	const struct mdoc_term_act *act;
	struct termpair	 npair;
	size_t		 offset, rmargin;  /* In basic units. */
	int		 chld;

	/*
	 * In no-fill mode, break the output line at the beginning
	 * of new input lines except after \c, and nowhere else.
	 */

	if (n->flags & NODE_NOFILL) {
		if (n->flags & NODE_LINE &&
		    (p->flags & TERMP_NONEWLINE) == 0)
			term_newln(p);
		p->flags |= TERMP_BRNEVER;
	} else {
		if (n->flags & NODE_LINE)
			term_tab_ref(p);
		p->flags &= ~TERMP_BRNEVER;
	}

	if (n->type == ROFFT_COMMENT || n->flags & NODE_NOPRT)
		return;

	chld = 1;
	offset = p->tcol->offset;
	rmargin = p->tcol->rmargin;
	n->flags &= ~NODE_ENDED;
	n->prev_font = p->fonti;

	memset(&npair, 0, sizeof(struct termpair));
	npair.ppair = pair;

	if (n->flags & NODE_ID && n->tok != MDOC_Pp &&
	    (n->tok != MDOC_It || n->type != ROFFT_BLOCK))
		term_tag_write(n, p->line);

	/*
	 * Keeps only work until the end of a line.  If a keep was
	 * invoked in a prior line, revert it to PREKEEP.
	 */

	if (p->flags & TERMP_KEEP && n->flags & NODE_LINE) {
		p->flags &= ~TERMP_KEEP;
		p->flags |= TERMP_PREKEEP;
	}

	/*
	 * After the keep flags have been set up, we may now
	 * produce output.  Note that some pre-handlers do so.
	 */

	act = NULL;
	switch (n->type) {
	case ROFFT_TEXT:
		if (n->flags & NODE_LINE) {
			switch (*n->string) {
			case '\0':
				if (p->flags & TERMP_NONEWLINE)
					term_newln(p);
				else
					term_vspace(p);
				return;
			case ' ':
				if ((p->flags & TERMP_NONEWLINE) == 0)
					term_newln(p);
				break;
			default:
				break;
			}
		}
		if (NODE_DELIMC & n->flags)
			p->flags |= TERMP_NOSPACE;
		term_word(p, n->string);
		if (NODE_DELIMO & n->flags)
			p->flags |= TERMP_NOSPACE;
		break;
	case ROFFT_EQN:
		if ( ! (n->flags & NODE_LINE))
			p->flags |= TERMP_NOSPACE;
		term_eqn(p, n->eqn);
		if (n->next != NULL && ! (n->next->flags & NODE_LINE))
			p->flags |= TERMP_NOSPACE;
		break;
	case ROFFT_TBL:
		if (p->tbl.cols == NULL)
			term_newln(p);
		term_tbl(p, n->span);
		break;
	default:
		if (n->tok < ROFF_MAX) {
			roff_term_pre(p, n);
			return;
		}
		assert(n->tok >= MDOC_Dd && n->tok < MDOC_MAX);
		act = mdoc_term_acts + (n->tok - MDOC_Dd);
		if (act->pre != NULL &&
		    (n->end == ENDBODY_NOT || n->child != NULL))
			chld = (*act->pre)(p, &npair, meta, n);
		break;
	}

	if (chld && n->child)
		print_mdoc_nodelist(p, &npair, meta, n->child);

	term_fontpopq(p,
	    (ENDBODY_NOT == n->end ? n : n->body)->prev_font);

	switch (n->type) {
	case ROFFT_TEXT:
		break;
	case ROFFT_TBL:
		break;
	case ROFFT_EQN:
		break;
	default:
		if (act->post == NULL || n->flags & NODE_ENDED)
			break;
		(void)(*act->post)(p, &npair, meta, n);

		/*
		 * Explicit end tokens not only call the post
		 * handler, but also tell the respective block
		 * that it must not call the post handler again.
		 */
		if (ENDBODY_NOT != n->end)
			n->body->flags |= NODE_ENDED;
		break;
	}

	if (NODE_EOS & n->flags)
		p->flags |= TERMP_SENTENCE;

	if (n->type != ROFFT_TEXT)
		p->tcol->offset = offset;
	p->tcol->rmargin = rmargin;
}

static void
print_mdoc_foot(struct termp *p, const struct roff_meta *meta)
{
	char	*title;
	size_t	 datelen, titlen;  /* In basic units. */

	assert(meta->title != NULL);
	datelen = term_strlen(p, meta->date);
	if (meta->msec == NULL)
		title = mandoc_strdup(meta->title);
	else
		mandoc_asprintf(&title, "%s(%s)", meta->title, meta->msec);
	titlen = term_strlen(p, title);

	term_fontrepl(p, TERMFONT_NONE);
	term_vspace(p);

	/* Bottom left corner: operating system. */

	p->tcol->offset = 0;
	p->tcol->rmargin = p->maxrmargin > datelen ?
	    (p->maxrmargin + term_len(p, 1) - datelen) / 2 : 0;
	p->trailspace = 1;
	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;

	term_word(p, meta->os);
	term_flushln(p);

	/* At the bottom in the middle: manual date. */

	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->maxrmargin > titlen ?
	    p->maxrmargin - titlen : 0;
	p->flags |= TERMP_NOSPACE;

	term_word(p, meta->date);
	term_flushln(p);

	/* Bottom right corner: manual title and section. */

	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->maxrmargin;
	p->trailspace = 0;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->tcol->offset = 0;
	p->flags = 0;
	free(title);
}

static void
print_mdoc_head(struct termp *p, const struct roff_meta *meta)
{
	char			*volume, *title;
	size_t			 vollen, titlen;  /* In basic units. */

	assert(meta->vol);
	if (NULL == meta->arch)
		volume = mandoc_strdup(meta->vol);
	else
		mandoc_asprintf(&volume, "%s (%s)",
		    meta->vol, meta->arch);
	vollen = term_strlen(p, volume);

	/* Top left corner: manual title and section. */

	if (NULL == meta->msec)
		title = mandoc_strdup(meta->title);
	else
		mandoc_asprintf(&title, "%s(%s)",
		    meta->title, meta->msec);
	titlen = term_strlen(p, title);

	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;
	p->trailspace = 1;
	p->tcol->offset = 0;
	p->tcol->rmargin =
	    titlen * 2 + term_len(p, 2) + vollen < p->maxrmargin ?
	    (p->maxrmargin - vollen + term_len(p, 1)) / 2 :
	    vollen < p->maxrmargin ? p->maxrmargin - vollen : 0;

	term_word(p, title);
	term_flushln(p);

	/* At the top in the middle: manual volume. */

	p->flags |= TERMP_NOSPACE;
	p->tcol->offset = p->tcol->rmargin;
	p->tcol->rmargin = p->tcol->offset + vollen + titlen <
	    p->maxrmargin ? p->maxrmargin - titlen : p->maxrmargin;

	term_word(p, volume);
	term_flushln(p);

	/* Top right corner: title and section, again. */

	p->flags &= ~TERMP_NOBREAK;
	p->trailspace = 0;
	if (p->tcol->rmargin + titlen <= p->maxrmargin) {
		p->flags |= TERMP_NOSPACE;
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = p->maxrmargin;
		term_word(p, title);
		term_flushln(p);
	}

	p->flags &= ~TERMP_NOSPACE;
	p->tcol->offset = 0;
	p->tcol->rmargin = p->maxrmargin;
	free(title);
	free(volume);
}

/*
 * Interpret the string v as a scaled width or, if the syntax is invalid,
 * measure how much width it takes up when printed.  In both cases,
 * return the width in basic units.
 */
static int
a2width(const struct termp *p, const char *v)
{
	struct roffsu	 su;
	const char	*end;

	end = a2roffsu(v, &su, SCALE_MAX);
	if (end == NULL || *end != '\0') {
		su.unit = SCALE_BU;
		su.scale = term_strlen(p, v);
	}
	return term_hspan(p, &su);
}

/*
 * Determine how much space to print out before block elements of `It'
 * (and thus `Bl') and `Bd'.  And then go ahead and print that space,
 * too.
 */
static void
print_bvspace(struct termp *p, struct roff_node *bl, struct roff_node *n)
{
	struct roff_node *nn;

	term_newln(p);

	if ((bl->tok == MDOC_Bd && bl->norm->Bd.comp) ||
	    (bl->tok == MDOC_Bl && bl->norm->Bl.comp))
		return;

	/* Do not vspace directly after Ss/Sh. */

	nn = n;
	while (roff_node_prev(nn) == NULL) {
		do {
			nn = nn->parent;
			if (nn->type == ROFFT_ROOT)
				return;
		} while (nn->type != ROFFT_BLOCK);
		if (nn->tok == MDOC_Sh || nn->tok == MDOC_Ss)
			return;
		if (nn->tok == MDOC_It &&
		    nn->parent->parent->norm->Bl.type != LIST_item)
			break;
	}

	/*
	 * No vertical space after:
	 * items in .Bl -column
	 * items without a body in .Bl -diag
	 */

	if (bl->tok != MDOC_Bl ||
	    n->prev == NULL || n->prev->tok != MDOC_It ||
	    (bl->norm->Bl.type != LIST_column &&
	     (bl->norm->Bl.type != LIST_diag ||
	      n->prev->body->child != NULL)))
		term_vspace(p);
}


static int
termp_it_pre(DECL_ARGS)
{
	struct roffsu		su;
	char			buf[24];
	const struct roff_node *bl, *nn;
	size_t			ncols;	/* Number of columns in .Bl -column. */
	size_t			dcol;	/* Column spacing in basic units. */
	int			i;	/* Zero-based column index. */
	int			offset;	/* Start of column in basic units. */
	int			width;	/* Column width in basic units. */
	enum mdoc_list		type;

	if (n->type == ROFFT_BLOCK) {
		print_bvspace(p, n->parent->parent, n);
		if (n->flags & NODE_ID)
			term_tag_write(n, p->line);
		return 1;
	}

	bl = n->parent->parent->parent;
	type = bl->norm->Bl.type;

	/*
	 * Defaults for specific list types.
	 */

	switch (type) {
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
	case LIST_enum:
		width = term_len(p, 2);
		break;
	case LIST_hang:
	case LIST_tag:
		width = term_len(p, 8);
		break;
	case LIST_column:
		width = term_len(p, 10);
		break;
	default:
		width = 0;
		break;
	}
	offset = 0;

	/*
	 * First calculate width and offset.  This is pretty easy unless
	 * we're a -column list, in which case all prior columns must
	 * be accounted for.
	 */

	if (bl->norm->Bl.offs != NULL) {
		offset = a2width(p, bl->norm->Bl.offs);
		if (offset < 0 && (size_t)(-offset) > p->tcol->offset)
			offset = -p->tcol->offset;
		else if (offset > SHRT_MAX)
			offset = 0;
	}

	switch (type) {
	case LIST_column:
		if (n->type == ROFFT_HEAD)
			break;

		/*
		 * Imitate groff's column handling:
		 * - For each earlier column, add its width.
		 * - For less than 5 columns, add four more blanks per
		 *   column.
		 * - For exactly 5 columns, add three more blank per
		 *   column.
		 * - For more than 5 columns, add only one column.
		 */
		ncols = bl->norm->Bl.ncols;
		dcol = ncols < 5 ? term_len(p, 4) :
		    ncols == 5 ? term_len(p, 3) : term_len(p, 1);

		/*
		 * Calculate the offset by applying all prior ROFFT_BODY,
		 * so we stop at the ROFFT_HEAD (nn->prev == NULL).
		 */

		for (i = 0, nn = n->prev;
		    nn->prev && i < (int)ncols;
		    nn = nn->prev, i++) {
			su.unit = SCALE_BU;
			su.scale = term_strlen(p, bl->norm->Bl.cols[i]);
			offset += term_hspan(p, &su) + dcol;
		}

		/*
		 * When exceeding the declared number of columns, leave
		 * the remaining widths at 0.  This will later be
		 * adjusted to the default width of 10, or, for the last
		 * column, stretched to the right margin.
		 */
		if (i >= (int)ncols)
			break;

		/*
		 * Use the declared column widths, extended as explained
		 * in the preceding paragraph.
		 */
		su.unit = SCALE_BU;
		su.scale = term_strlen(p, bl->norm->Bl.cols[i]);
		width = term_hspan(p, &su) + dcol;
		break;
	default:
		if (NULL == bl->norm->Bl.width)
			break;

		/*
		 * Note: buffer the width by 2, which is groff's magic
		 * number for buffering single arguments.  See the above
		 * handling for column for how this changes.
		 */
		width = a2width(p, bl->norm->Bl.width) + term_len(p, 2);
		if (width < 0 && (size_t)(-width) > p->tcol->offset)
			width = -p->tcol->offset;
		else if (width > SHRT_MAX)
			width = 0;
		break;
	}

	/*
	 * Whitespace control.  Inset bodies need an initial space,
	 * while diagonal bodies need two.
	 */

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case LIST_diag:
		if (n->type == ROFFT_BODY)
			term_word(p, "\\ \\ ");
		break;
	case LIST_inset:
		if (n->type == ROFFT_BODY && n->parent->head->child != NULL)
			term_word(p, "\\ ");
		break;
	default:
		break;
	}

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case LIST_diag:
		if (n->type == ROFFT_HEAD)
			term_fontpush(p, TERMFONT_BOLD);
		break;
	default:
		break;
	}

	/*
	 * Pad and break control.  This is the tricky part.  These flags
	 * are documented in term_flushln() in term.c.  Note that we're
	 * going to unset all of these flags in termp_it_post() when we
	 * exit.
	 */

	switch (type) {
	case LIST_enum:
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
		if (n->type == ROFFT_HEAD) {
			p->flags |= TERMP_NOBREAK | TERMP_HANG;
			p->trailspace = 1;
		} else if (width <= (int)term_len(p, 2))
			p->flags |= TERMP_NOPAD;
		break;
	case LIST_hang:
		if (n->type != ROFFT_HEAD)
			break;
		p->flags |= TERMP_NOBREAK | TERMP_BRIND | TERMP_HANG;
		p->trailspace = 1;
		break;
	case LIST_tag:
		if (n->type != ROFFT_HEAD)
			break;

		p->flags |= TERMP_NOBREAK | TERMP_BRTRSP | TERMP_BRIND;
		p->trailspace = 2;

		if (NULL == n->next || NULL == n->next->child)
			p->flags |= TERMP_HANG;
		break;
	case LIST_column:
		if (n->type == ROFFT_HEAD)
			break;

		if (NULL == n->next) {
			p->flags &= ~TERMP_NOBREAK;
			p->trailspace = 0;
		} else {
			p->flags |= TERMP_NOBREAK;
			p->trailspace = 1;
		}

		break;
	case LIST_diag:
		if (n->type != ROFFT_HEAD)
			break;
		p->flags |= TERMP_NOBREAK | TERMP_BRIND;
		p->trailspace = 1;
		break;
	default:
		break;
	}

	/*
	 * Margin control.  Set-head-width lists have their right
	 * margins shortened.  The body for these lists has the offset
	 * necessarily lengthened.  Everybody gets the offset.
	 */

	p->tcol->offset += offset;

	switch (type) {
	case LIST_bullet:
	case LIST_dash:
	case LIST_enum:
	case LIST_hyphen:
	case LIST_hang:
	case LIST_tag:
		if (n->type == ROFFT_HEAD)
			p->tcol->rmargin = p->tcol->offset + width;
		else
			p->tcol->offset += width;
		break;
	case LIST_column:
		assert(width);
		p->tcol->rmargin = p->tcol->offset + width;
		/*
		 * XXX - this behaviour is not documented: the
		 * right-most column is filled to the right margin.
		 */
		if (n->type == ROFFT_HEAD)
			break;
		if (n->next == NULL && p->tcol->rmargin < p->maxrmargin)
			p->tcol->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	/*
	 * The dash, hyphen, bullet and enum lists all have a special
	 * HEAD character (temporarily bold, in some cases).
	 */

	if (n->type == ROFFT_HEAD)
		switch (type) {
		case LIST_bullet:
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\[bu]");
			term_fontpop(p);
			break;
		case LIST_dash:
		case LIST_hyphen:
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "-");
			term_fontpop(p);
			break;
		case LIST_enum:
			(pair->ppair->ppair->count)++;
			(void)snprintf(buf, sizeof(buf), "%d.",
			    pair->ppair->ppair->count);
			term_word(p, buf);
			break;
		default:
			break;
		}

	/*
	 * If we're not going to process our children, indicate so here.
	 */

	switch (type) {
	case LIST_bullet:
	case LIST_item:
	case LIST_dash:
	case LIST_hyphen:
	case LIST_enum:
		if (n->type == ROFFT_HEAD)
			return 0;
		break;
	case LIST_column:
		if (n->type == ROFFT_HEAD)
			return 0;
		p->minbl = 0;
		break;
	default:
		break;
	}

	return 1;
}

static void
termp_it_post(DECL_ARGS)
{
	enum mdoc_list	   type;

	if (n->type == ROFFT_BLOCK)
		return;

	type = n->parent->parent->parent->norm->Bl.type;

	switch (type) {
	case LIST_item:
	case LIST_diag:
	case LIST_inset:
		if (n->type == ROFFT_BODY)
			term_newln(p);
		break;
	case LIST_column:
		if (n->type == ROFFT_BODY)
			term_flushln(p);
		break;
	default:
		term_newln(p);
		break;
	}

	/*
	 * Now that our output is flushed, we can reset our tags.  Since
	 * only `It' sets these flags, we're free to assume that nobody
	 * has munged them in the meanwhile.
	 */

	p->flags &= ~(TERMP_NOBREAK | TERMP_BRTRSP | TERMP_BRIND | TERMP_HANG);
	p->trailspace = 0;
}

static int
termp_nm_pre(DECL_ARGS)
{
	const char	*cp;

	if (n->type == ROFFT_BLOCK) {
		p->flags |= TERMP_PREKEEP;
		return 1;
	}

	if (n->type == ROFFT_BODY) {
		if (n->child == NULL)
			return 0;
		p->flags |= TERMP_NOSPACE;
		cp = NULL;
		if (n->prev->child != NULL)
		    cp = n->prev->child->string;
		if (cp == NULL)
			cp = meta->name;
		if (cp == NULL)
			p->tcol->offset += term_len(p, 6);
		else
			p->tcol->offset += term_len(p, 1) +
			    term_strlen(p, cp);
		return 1;
	}

	if (n->child == NULL)
		return 0;

	if (n->type == ROFFT_HEAD)
		synopsis_pre(p, n->parent);

	if (n->type == ROFFT_HEAD &&
	    n->next != NULL && n->next->child != NULL) {
		p->flags |= TERMP_NOSPACE | TERMP_NOBREAK | TERMP_BRIND;
		p->trailspace = 1;
		p->tcol->rmargin = p->tcol->offset + term_len(p, 1);
		if (n->child == NULL)
			p->tcol->rmargin += term_strlen(p, meta->name);
		else if (n->child->type == ROFFT_TEXT) {
			p->tcol->rmargin += term_strlen(p, n->child->string);
			if (n->child->next != NULL)
				p->flags |= TERMP_HANG;
		} else {
			p->tcol->rmargin += term_len(p, 5);
			p->flags |= TERMP_HANG;
		}
	}
	return termp_bold_pre(p, pair, meta, n);
}

static void
termp_nm_post(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
		break;
	case ROFFT_HEAD:
		if (n->next == NULL || n->next->child == NULL)
			break;
		term_flushln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND | TERMP_HANG);
		p->trailspace = 0;
		break;
	case ROFFT_BODY:
		if (n->child != NULL)
			term_flushln(p);
		break;
	default:
		break;
	}
}

static int
termp_fl_pre(DECL_ARGS)
{
	struct roff_node *nn;

	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, "\\-");

	if (n->child != NULL ||
	    ((nn = roff_node_next(n)) != NULL &&
	     nn->type != ROFFT_TEXT &&
	     (nn->flags & NODE_LINE) == 0))
		p->flags |= TERMP_NOSPACE;

	return 1;
}

static int
termp__a_pre(DECL_ARGS)
{
	struct roff_node *nn;

	if ((nn = roff_node_prev(n)) != NULL && nn->tok == MDOC__A &&
	    ((nn = roff_node_next(n)) == NULL || nn->tok != MDOC__A))
		term_word(p, "and");

	return 1;
}

static int
termp_an_pre(DECL_ARGS)
{

	if (n->norm->An.auth == AUTH_split) {
		p->flags &= ~TERMP_NOSPLIT;
		p->flags |= TERMP_SPLIT;
		return 0;
	}
	if (n->norm->An.auth == AUTH_nosplit) {
		p->flags &= ~TERMP_SPLIT;
		p->flags |= TERMP_NOSPLIT;
		return 0;
	}

	if (p->flags & TERMP_SPLIT)
		term_newln(p);

	if (n->sec == SEC_AUTHORS && ! (p->flags & TERMP_NOSPLIT))
		p->flags |= TERMP_SPLIT;

	return 1;
}

static int
termp_ns_pre(DECL_ARGS)
{

	if ( ! (NODE_LINE & n->flags))
		p->flags |= TERMP_NOSPACE;
	return 1;
}

static int
termp_rs_pre(DECL_ARGS)
{
	if (SEC_SEE_ALSO != n->sec)
		return 1;
	if (n->type == ROFFT_BLOCK && roff_node_prev(n) != NULL)
		term_vspace(p);
	return 1;
}

static int
termp_ex_pre(DECL_ARGS)
{
	term_newln(p);
	return 1;
}

static int
termp_nd_pre(DECL_ARGS)
{
	if (n->type == ROFFT_BODY)
		term_word(p, "\\(en");
	return 1;
}

static int
termp_bl_pre(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		term_newln(p);
		return 1;
	case ROFFT_HEAD:
		return 0;
	default:
		return 1;
	}
}

static void
termp_bl_post(DECL_ARGS)
{
	if (n->type != ROFFT_BLOCK)
		return;
	term_newln(p);
	if (n->tok != MDOC_Bl || n->norm->Bl.type != LIST_column)
		return;
	term_tab_set(p, NULL);
	term_tab_set(p, "T");
	term_tab_set(p, ".5i");
}

static int
termp_xr_pre(DECL_ARGS)
{
	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);
	term_word(p, n->string);

	if (NULL == (n = n->next))
		return 0;

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;

	assert(n->type == ROFFT_TEXT);
	term_word(p, n->string);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	return 0;
}

/*
 * This decides how to assert whitespace before any of the SYNOPSIS set
 * of macros (which, as in the case of Ft/Fo and Ft/Fn, may contain
 * macro combos).
 */
static void
synopsis_pre(struct termp *p, struct roff_node *n)
{
	struct roff_node	*np;

	if ((n->flags & NODE_SYNPRETTY) == 0 ||
	    (np = roff_node_prev(n)) == NULL)
		return;

	/*
	 * If we're the second in a pair of like elements, emit our
	 * newline and return.  UNLESS we're `Fo', `Fn', `Fn', in which
	 * case we soldier on.
	 */
	if (np->tok == n->tok &&
	    MDOC_Ft != n->tok &&
	    MDOC_Fo != n->tok &&
	    MDOC_Fn != n->tok) {
		term_newln(p);
		return;
	}

	/*
	 * If we're one of the SYNOPSIS set and non-like pair-wise after
	 * another (or Fn/Fo, which we've let slip through) then assert
	 * vertical space, else only newline and move on.
	 */
	switch (np->tok) {
	case MDOC_Fd:
	case MDOC_Fn:
	case MDOC_Fo:
	case MDOC_In:
	case MDOC_Vt:
		term_vspace(p);
		break;
	case MDOC_Ft:
		if (n->tok != MDOC_Fn && n->tok != MDOC_Fo) {
			term_vspace(p);
			break;
		}
		/* FALLTHROUGH */
	default:
		term_newln(p);
		break;
	}
}

static int
termp_vt_pre(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_ELEM:
		return termp_ft_pre(p, pair, meta, n);
	case ROFFT_BLOCK:
		synopsis_pre(p, n);
		return 1;
	case ROFFT_HEAD:
		return 0;
	default:
		return termp_under_pre(p, pair, meta, n);
	}
}

static int
termp_bold_pre(DECL_ARGS)
{
	term_fontpush(p, TERMFONT_BOLD);
	return 1;
}

static int
termp_fd_pre(DECL_ARGS)
{
	synopsis_pre(p, n);
	return termp_bold_pre(p, pair, meta, n);
}

static void
termp_fd_post(DECL_ARGS)
{
	term_newln(p);
}

static int
termp_sh_pre(DECL_ARGS)
{
	struct roff_node	*np;

	switch (n->type) {
	case ROFFT_BLOCK:
		/*
		 * Vertical space before sections, except
		 * when the previous section was empty.
		 */
		if ((np = roff_node_prev(n)) == NULL ||
		    np->tok != MDOC_Sh ||
		    (np->body != NULL && np->body->child != NULL))
			term_vspace(p);
		break;
	case ROFFT_HEAD:
		p->fontibi = 1;
		return termp_bold_pre(p, pair, meta, n);
	case ROFFT_BODY:
		p->tcol->offset = term_len(p, p->defindent);
		term_tab_set(p, NULL);
		term_tab_set(p, "T");
		term_tab_set(p, ".5i");
		if (n->sec == SEC_AUTHORS)
			p->flags &= ~(TERMP_SPLIT|TERMP_NOSPLIT);
		break;
	default:
		break;
	}
	return 1;
}

static void
termp_sh_post(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_HEAD:
		p->fontibi = 0;
		term_newln(p);
		break;
	case ROFFT_BODY:
		term_newln(p);
		p->tcol->offset = 0;
		break;
	default:
		break;
	}
}

static void
termp_lb_post(DECL_ARGS)
{
	if (n->sec == SEC_LIBRARY && n->flags & NODE_LINE)
		term_newln(p);
}

static int
termp_d1_pre(DECL_ARGS)
{
	if (n->type != ROFFT_BLOCK)
		return 1;
	term_newln(p);
	p->tcol->offset += term_len(p, p->defindent + 1);
	term_tab_set(p, NULL);
	term_tab_set(p, "T");
	term_tab_set(p, ".5i");
	return 1;
}

static int
termp_ft_pre(DECL_ARGS)
{
	synopsis_pre(p, n);
	return termp_under_pre(p, pair, meta, n);
}

static int
termp_fn_pre(DECL_ARGS)
{
	size_t		 rmargin = 0;
	int		 pretty;

	synopsis_pre(p, n);
	pretty = n->flags & NODE_SYNPRETTY;
	if ((n = n->child) == NULL)
		return 0;

	if (pretty) {
		rmargin = p->tcol->rmargin;
		p->tcol->rmargin = p->tcol->offset + term_len(p, 4);
		p->flags |= TERMP_NOBREAK | TERMP_BRIND | TERMP_HANG;
	}

	assert(n->type == ROFFT_TEXT);
	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, n->string);
	term_fontpop(p);

	if (pretty) {
		term_flushln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND | TERMP_HANG);
		p->flags |= TERMP_NOPAD;
		p->tcol->offset = p->tcol->rmargin;
		p->tcol->rmargin = rmargin;
	}

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;

	for (n = n->next; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		term_fontpush(p, TERMFONT_UNDER);
		if (pretty)
			p->flags |= TERMP_NBRWORD;
		term_word(p, n->string);
		term_fontpop(p);

		if (n->next) {
			p->flags |= TERMP_NOSPACE;
			term_word(p, ",");
		}
	}

	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	if (pretty) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, ";");
		term_flushln(p);
	}
	return 0;
}

static int
termp_fa_pre(DECL_ARGS)
{
	const struct roff_node	*nn;

	if (n->parent->tok != MDOC_Fo)
		return termp_under_pre(p, pair, meta, n);

	for (nn = n->child; nn != NULL; nn = nn->next) {
		term_fontpush(p, TERMFONT_UNDER);
		p->flags |= TERMP_NBRWORD;
		term_word(p, nn->string);
		term_fontpop(p);
		if (nn->next != NULL) {
			p->flags |= TERMP_NOSPACE;
			term_word(p, ",");
		}
	}
	if (n->child != NULL &&
	    (nn = roff_node_next(n)) != NULL &&
	    nn->tok == MDOC_Fa) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, ",");
	}
	return 0;
}

static int
termp_bd_pre(DECL_ARGS)
{
	int			 offset;  /* In basic units. */

	if (n->type == ROFFT_BLOCK) {
		print_bvspace(p, n, n);
		return 1;
	} else if (n->type == ROFFT_HEAD)
		return 0;

	/* Handle the -offset argument. */

	if (n->norm->Bd.offs == NULL ||
	    ! strcmp(n->norm->Bd.offs, "left"))
		/* nothing */;
	else if ( ! strcmp(n->norm->Bd.offs, "indent"))
		p->tcol->offset += term_len(p, p->defindent + 1);
	else if ( ! strcmp(n->norm->Bd.offs, "indent-two"))
		p->tcol->offset += term_len(p, (p->defindent + 1) * 2);
	else {
		offset = a2width(p, n->norm->Bd.offs);
		if (offset < 0 && (size_t)(-offset) > p->tcol->offset)
			p->tcol->offset = 0;
		else if (offset < SHRT_MAX)
			p->tcol->offset += offset;
	}

	switch (n->norm->Bd.type) {
	case DISP_literal:
		term_tab_set(p, NULL);
		term_tab_set(p, "T");
		term_tab_set(p, "8n");
		break;
	case DISP_centered:
		p->flags |= TERMP_CENTER;
		break;
	default:
		break;
	}
	return 1;
}

static void
termp_bd_post(DECL_ARGS)
{
	if (n->type != ROFFT_BODY)
		return;
	if (n->norm->Bd.type == DISP_unfilled ||
	    n->norm->Bd.type == DISP_literal)
		p->flags |= TERMP_BRNEVER;
	p->flags |= TERMP_NOSPACE;
	term_newln(p);
	p->flags &= ~TERMP_BRNEVER;
	if (n->norm->Bd.type == DISP_centered)
		p->flags &= ~TERMP_CENTER;
}

static int
termp_xx_pre(DECL_ARGS)
{
	if ((n->aux = p->flags & TERMP_PREKEEP) == 0)
		p->flags |= TERMP_PREKEEP;
	return 1;
}

static void
termp_xx_post(DECL_ARGS)
{
	if (n->aux == 0)
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
}

static void
termp_pf_post(DECL_ARGS)
{
	if (n->next != NULL && (n->next->flags & NODE_LINE) == 0)
		p->flags |= TERMP_NOSPACE;
}

static int
termp_ss_pre(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		if (roff_node_prev(n) == NULL)
			term_newln(p);
		else
			term_vspace(p);
		break;
	case ROFFT_HEAD:
		p->tcol->offset = term_len(p, p->defindent) / 2 + 1;
		p->fontibi = 1;
		return termp_bold_pre(p, pair, meta, n);
	case ROFFT_BODY:
		p->tcol->offset = term_len(p, p->defindent);
		term_tab_set(p, NULL);
		term_tab_set(p, "T");
		term_tab_set(p, ".5i");
		break;
	default:
		break;
	}
	return 1;
}

static void
termp_ss_post(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_HEAD:
		p->fontibi = 0;
		/* FALLTHROUGH */
	case ROFFT_BODY:
		term_newln(p);
		break;
	default:
		break;
	}
}

static int
termp_in_pre(DECL_ARGS)
{
	synopsis_pre(p, n);
	if (n->flags & NODE_SYNPRETTY && n->flags & NODE_LINE) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, "#include");
		term_word(p, "<");
	} else {
		term_word(p, "<");
		term_fontpush(p, TERMFONT_UNDER);
	}
	p->flags |= TERMP_NOSPACE;
	return 1;
}

static void
termp_in_post(DECL_ARGS)
{
	if (n->flags & NODE_SYNPRETTY)
		term_fontpush(p, TERMFONT_BOLD);
	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");
	if (n->flags & NODE_SYNPRETTY)
		term_fontpop(p);
}

static int
termp_pp_pre(DECL_ARGS)
{
	term_vspace(p);
	if (n->flags & NODE_ID)
		term_tag_write(n, p->line);
	return 0;
}

static int
termp_skip_pre(DECL_ARGS)
{
	return 0;
}

static int
termp_quote_pre(DECL_ARGS)
{
	if (n->type != ROFFT_BODY && n->type != ROFFT_ELEM)
		return 1;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		term_word(p, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ? "<" : "\\(la");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		term_word(p, "{");
		break;
	case MDOC_Oo:
	case MDOC_Op:
	case MDOC_Bo:
	case MDOC_Bq:
		term_word(p, "[");
		break;
	case MDOC__T:
		/* FALLTHROUGH */
	case MDOC_Do:
	case MDOC_Dq:
		term_word(p, "\\(lq");
		break;
	case MDOC_En:
		if (NULL == n->norm->Es ||
		    NULL == n->norm->Es->child)
			return 1;
		term_word(p, n->norm->Es->child->string);
		break;
	case MDOC_Po:
	case MDOC_Pq:
		term_word(p, "(");
		break;
	case MDOC_Qo:
	case MDOC_Qq:
		term_word(p, "\"");
		break;
	case MDOC_Ql:
	case MDOC_So:
	case MDOC_Sq:
		term_word(p, "\\(oq");
		break;
	default:
		abort();
	}

	p->flags |= TERMP_NOSPACE;
	return 1;
}

static void
termp_quote_post(DECL_ARGS)
{

	if (n->type != ROFFT_BODY && n->type != ROFFT_ELEM)
		return;

	p->flags |= TERMP_NOSPACE;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		term_word(p, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ? ">" : "\\(ra");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		term_word(p, "}");
		break;
	case MDOC_Oo:
	case MDOC_Op:
	case MDOC_Bo:
	case MDOC_Bq:
		term_word(p, "]");
		break;
	case MDOC__T:
		/* FALLTHROUGH */
	case MDOC_Do:
	case MDOC_Dq:
		term_word(p, "\\(rq");
		break;
	case MDOC_En:
		if (n->norm->Es == NULL ||
		    n->norm->Es->child == NULL ||
		    n->norm->Es->child->next == NULL)
			p->flags &= ~TERMP_NOSPACE;
		else
			term_word(p, n->norm->Es->child->next->string);
		break;
	case MDOC_Po:
	case MDOC_Pq:
		term_word(p, ")");
		break;
	case MDOC_Qo:
	case MDOC_Qq:
		term_word(p, "\"");
		break;
	case MDOC_Ql:
	case MDOC_So:
	case MDOC_Sq:
		term_word(p, "\\(cq");
		break;
	default:
		abort();
	}
}

static int
termp_eo_pre(DECL_ARGS)
{

	if (n->type != ROFFT_BODY)
		return 1;

	if (n->end == ENDBODY_NOT &&
	    n->parent->head->child == NULL &&
	    n->child != NULL &&
	    n->child->end != ENDBODY_NOT)
		term_word(p, "\\&");
	else if (n->end != ENDBODY_NOT ? n->child != NULL :
	     n->parent->head->child != NULL && (n->child != NULL ||
	     (n->parent->tail != NULL && n->parent->tail->child != NULL)))
		p->flags |= TERMP_NOSPACE;

	return 1;
}

static void
termp_eo_post(DECL_ARGS)
{
	int	 body, tail;

	if (n->type != ROFFT_BODY)
		return;

	if (n->end != ENDBODY_NOT) {
		p->flags &= ~TERMP_NOSPACE;
		return;
	}

	body = n->child != NULL || n->parent->head->child != NULL;
	tail = n->parent->tail != NULL && n->parent->tail->child != NULL;

	if (body && tail)
		p->flags |= TERMP_NOSPACE;
	else if ( ! (body || tail))
		term_word(p, "\\&");
	else if ( ! tail)
		p->flags &= ~TERMP_NOSPACE;
}

static int
termp_fo_pre(DECL_ARGS)
{
	size_t rmargin;

	switch (n->type) {
	case ROFFT_BLOCK:
		synopsis_pre(p, n);
		return 1;
	case ROFFT_BODY:
		rmargin = p->tcol->rmargin;
		if (n->flags & NODE_SYNPRETTY) {
			p->tcol->rmargin = p->tcol->offset + term_len(p, 4);
			p->flags |= TERMP_NOBREAK | TERMP_BRIND |
					TERMP_HANG;
		}
		p->flags |= TERMP_NOSPACE;
		term_word(p, "(");
		p->flags |= TERMP_NOSPACE;
		if (n->flags & NODE_SYNPRETTY) {
			term_flushln(p);
			p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND |
					TERMP_HANG);
			p->flags |= TERMP_NOPAD;
			p->tcol->offset = p->tcol->rmargin;
			p->tcol->rmargin = rmargin;
		}
		return 1;
	default:
		return termp_bold_pre(p, pair, meta, n);
	}
}

static void
termp_fo_post(DECL_ARGS)
{
	if (n->type != ROFFT_BODY)
		return;

	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	if (n->flags & NODE_SYNPRETTY) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, ";");
		term_flushln(p);
	}
}

static int
termp_bf_pre(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		break;
	default:
		return 1;
	}
	switch (n->norm->Bf.font) {
	case FONT_Em:
		return termp_under_pre(p, pair, meta, n);
	case FONT_Sy:
		return termp_bold_pre(p, pair, meta, n);
	default:
		return termp_li_pre(p, pair, meta, n);
	}
}

static int
termp_sm_pre(DECL_ARGS)
{
	if (n->child == NULL)
		p->flags ^= TERMP_NONOSPACE;
	else if (strcmp(n->child->string, "on") == 0)
		p->flags &= ~TERMP_NONOSPACE;
	else
		p->flags |= TERMP_NONOSPACE;

	if (p->col && ! (TERMP_NONOSPACE & p->flags))
		p->flags &= ~TERMP_NOSPACE;

	return 0;
}

static int
termp_ap_pre(DECL_ARGS)
{
	p->flags |= TERMP_NOSPACE;
	term_word(p, "'");
	p->flags |= TERMP_NOSPACE;
	return 1;
}

static void
termp____post(DECL_ARGS)
{
	struct roff_node *nn;

	/*
	 * Handle lists of authors.  In general, print each followed by
	 * a comma.  Don't print the comma if there are only two
	 * authors.
	 */
	if (n->tok == MDOC__A &&
	    (nn = roff_node_next(n)) != NULL && nn->tok == MDOC__A &&
	    ((nn = roff_node_next(nn)) == NULL || nn->tok != MDOC__A) &&
	    ((nn = roff_node_prev(n)) == NULL || nn->tok != MDOC__A))
		return;

	/* TODO: %U. */

	if (n->parent == NULL || n->parent->tok != MDOC_Rs)
		return;

	p->flags |= TERMP_NOSPACE;
	if (roff_node_next(n) == NULL) {
		term_word(p, ".");
		p->flags |= TERMP_SENTENCE;
	} else
		term_word(p, ",");
}

static int
termp_li_pre(DECL_ARGS)
{
	term_fontpush(p, TERMFONT_NONE);
	return 1;
}

static int
termp_lk_pre(DECL_ARGS)
{
	const struct roff_node *link, *descr, *punct;

	if ((link = n->child) == NULL)
		return 0;

	/* Find beginning of trailing punctuation. */
	punct = n->last;
	while (punct != link && punct->flags & NODE_DELIMC)
		punct = punct->prev;
	punct = punct->next;

	/* Link text. */
	if ((descr = link->next) != NULL && descr != punct) {
		term_fontpush(p, TERMFONT_UNDER);
		while (descr != punct) {
			if (descr->flags & (NODE_DELIMC | NODE_DELIMO))
				p->flags |= TERMP_NOSPACE;
			term_word(p, descr->string);
			descr = descr->next;
		}
		term_fontpop(p);
		p->flags |= TERMP_NOSPACE;
		term_word(p, ":");
	}

	/* Link target. */
	term_word(p, link->string);

	/* Trailing punctuation. */
	while (punct != NULL) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, punct->string);
		punct = punct->next;
	}
	return 0;
}

static int
termp_bk_pre(DECL_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		if (n->parent->args != NULL || n->prev->child == NULL)
			p->flags |= TERMP_PREKEEP;
		break;
	default:
		abort();
	}
	return 1;
}

static void
termp_bk_post(DECL_ARGS)
{
	if (n->type == ROFFT_BODY)
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
}

/*
 * If we are in an `Rs' and there is a journal present,
 * then quote us instead of underlining us (for disambiguation).
 */
static void
termp__t_post(DECL_ARGS)
{
	if (n->parent != NULL && n->parent->tok == MDOC_Rs &&
	    n->parent->norm->Rs.quote_T)
		termp_quote_post(p, pair, meta, n);
	termp____post(p, pair, meta, n);
}

static int
termp__t_pre(DECL_ARGS)
{
	if (n->parent != NULL && n->parent->tok == MDOC_Rs &&
	    n->parent->norm->Rs.quote_T)
		return termp_quote_pre(p, pair, meta, n);
	else
		return termp_under_pre(p, pair, meta, n);
}

static int
termp_under_pre(DECL_ARGS)
{
	term_fontpush(p, TERMFONT_UNDER);
	return 1;
}

static int
termp_abort_pre(DECL_ARGS)
{
	abort();
}
