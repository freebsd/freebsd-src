/*	$Id: mdoc_term.c,v 1.258 2013/12/25 21:24:12 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2012, 2013 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2013 Franco Fichtner <franco@lastsummer.de>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"
#include "mdoc.h"
#include "main.h"

struct	termpair {
	struct termpair	 *ppair;
	int		  count;
};

#define	DECL_ARGS struct termp *p, \
		  struct termpair *pair, \
	  	  const struct mdoc_meta *meta, \
		  struct mdoc_node *n

struct	termact {
	int	(*pre)(DECL_ARGS);
	void	(*post)(DECL_ARGS);
};

static	size_t	  a2width(const struct termp *, const char *);
static	size_t	  a2height(const struct termp *, const char *);
static	size_t	  a2offs(const struct termp *, const char *);

static	void	  print_bvspace(struct termp *,
			const struct mdoc_node *,
			const struct mdoc_node *);
static	void  	  print_mdoc_node(DECL_ARGS);
static	void	  print_mdoc_nodelist(DECL_ARGS);
static	void	  print_mdoc_head(struct termp *, const void *);
static	void	  print_mdoc_foot(struct termp *, const void *);
static	void	  synopsis_pre(struct termp *, 
			const struct mdoc_node *);

static	void	  termp____post(DECL_ARGS);
static	void	  termp__t_post(DECL_ARGS);
static	void	  termp_an_post(DECL_ARGS);
static	void	  termp_bd_post(DECL_ARGS);
static	void	  termp_bk_post(DECL_ARGS);
static	void	  termp_bl_post(DECL_ARGS);
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

static	int	  termp__a_pre(DECL_ARGS);
static	int	  termp__t_pre(DECL_ARGS);
static	int	  termp_an_pre(DECL_ARGS);
static	int	  termp_ap_pre(DECL_ARGS);
static	int	  termp_bd_pre(DECL_ARGS);
static	int	  termp_bf_pre(DECL_ARGS);
static	int	  termp_bk_pre(DECL_ARGS);
static	int	  termp_bl_pre(DECL_ARGS);
static	int	  termp_bold_pre(DECL_ARGS);
static	int	  termp_bt_pre(DECL_ARGS);
static	int	  termp_bx_pre(DECL_ARGS);
static	int	  termp_cd_pre(DECL_ARGS);
static	int	  termp_d1_pre(DECL_ARGS);
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
static	int	  termp_rv_pre(DECL_ARGS);
static	int	  termp_sh_pre(DECL_ARGS);
static	int	  termp_sm_pre(DECL_ARGS);
static	int	  termp_sp_pre(DECL_ARGS);
static	int	  termp_ss_pre(DECL_ARGS);
static	int	  termp_under_pre(DECL_ARGS);
static	int	  termp_ud_pre(DECL_ARGS);
static	int	  termp_vt_pre(DECL_ARGS);
static	int	  termp_xr_pre(DECL_ARGS);
static	int	  termp_xx_pre(DECL_ARGS);

static	const struct termact termacts[MDOC_MAX] = {
	{ termp_ap_pre, NULL }, /* Ap */
	{ NULL, NULL }, /* Dd */
	{ NULL, NULL }, /* Dt */
	{ NULL, NULL }, /* Os */
	{ termp_sh_pre, termp_sh_post }, /* Sh */
	{ termp_ss_pre, termp_ss_post }, /* Ss */ 
	{ termp_sp_pre, NULL }, /* Pp */ 
	{ termp_d1_pre, termp_bl_post }, /* D1 */
	{ termp_d1_pre, termp_bl_post }, /* Dl */
	{ termp_bd_pre, termp_bd_post }, /* Bd */
	{ NULL, NULL }, /* Ed */
	{ termp_bl_pre, termp_bl_post }, /* Bl */
	{ NULL, NULL }, /* El */
	{ termp_it_pre, termp_it_post }, /* It */
	{ termp_under_pre, NULL }, /* Ad */ 
	{ termp_an_pre, termp_an_post }, /* An */
	{ termp_under_pre, NULL }, /* Ar */
	{ termp_cd_pre, NULL }, /* Cd */
	{ termp_bold_pre, NULL }, /* Cm */
	{ NULL, NULL }, /* Dv */ 
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
	{ NULL, NULL }, /* Ot */
	{ termp_under_pre, NULL }, /* Pa */
	{ termp_rv_pre, NULL }, /* Rv */
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
	{ termp_xx_pre, NULL }, /* Bsx */
	{ termp_bx_pre, NULL }, /* Bx */
	{ NULL, NULL }, /* Db */
	{ NULL, NULL }, /* Dc */
	{ termp_quote_pre, termp_quote_post }, /* Do */
	{ termp_quote_pre, termp_quote_post }, /* Dq */
	{ NULL, NULL }, /* Ec */ /* FIXME: no space */
	{ NULL, NULL }, /* Ef */
	{ termp_under_pre, NULL }, /* Em */ 
	{ termp_quote_pre, termp_quote_post }, /* Eo */
	{ termp_xx_pre, NULL }, /* Fx */
	{ termp_bold_pre, NULL }, /* Ms */
	{ NULL, NULL }, /* No */
	{ termp_ns_pre, NULL }, /* Ns */
	{ termp_xx_pre, NULL }, /* Nx */
	{ termp_xx_pre, NULL }, /* Ox */
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
	{ termp_xx_pre, NULL }, /* Ux */
	{ NULL, NULL }, /* Xc */
	{ NULL, NULL }, /* Xo */
	{ termp_fo_pre, termp_fo_post }, /* Fo */ 
	{ NULL, NULL }, /* Fc */ 
	{ termp_quote_pre, termp_quote_post }, /* Oo */
	{ NULL, NULL }, /* Oc */
	{ termp_bk_pre, termp_bk_post }, /* Bk */
	{ NULL, NULL }, /* Ek */
	{ termp_bt_pre, NULL }, /* Bt */
	{ NULL, NULL }, /* Hf */
	{ NULL, NULL }, /* Fr */
	{ termp_ud_pre, NULL }, /* Ud */
	{ NULL, termp_lb_post }, /* Lb */
	{ termp_sp_pre, NULL }, /* Lp */ 
	{ termp_lk_pre, NULL }, /* Lk */ 
	{ termp_under_pre, NULL }, /* Mt */ 
	{ termp_quote_pre, termp_quote_post }, /* Brq */ 
	{ termp_quote_pre, termp_quote_post }, /* Bro */ 
	{ NULL, NULL }, /* Brc */ 
	{ NULL, termp____post }, /* %C */ 
	{ NULL, NULL }, /* Es */ /* TODO */
	{ NULL, NULL }, /* En */ /* TODO */
	{ termp_xx_pre, NULL }, /* Dx */ 
	{ NULL, termp____post }, /* %Q */ 
	{ termp_sp_pre, NULL }, /* br */
	{ termp_sp_pre, NULL }, /* sp */ 
	{ NULL, termp____post }, /* %U */ 
	{ NULL, NULL }, /* Ta */ 
};


void
terminal_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_node	*n;
	const struct mdoc_meta	*meta;
	struct termp		*p;

	p = (struct termp *)arg;

	if (0 == p->defindent)
		p->defindent = 5;

	p->overstep = 0;
	p->maxrmargin = p->defrmargin;
	p->tabwidth = term_len(p, 5);

	if (NULL == p->symtab)
		p->symtab = mchars_alloc();

	n = mdoc_node(mdoc);
	meta = mdoc_meta(mdoc);

	term_begin(p, print_mdoc_head, print_mdoc_foot, meta);

	if (n->child)
		print_mdoc_nodelist(p, NULL, meta, n->child);

	term_end(p);
}


static void
print_mdoc_nodelist(DECL_ARGS)
{

	print_mdoc_node(p, pair, meta, n);
	if (n->next)
		print_mdoc_nodelist(p, pair, meta, n->next);
}


/* ARGSUSED */
static void
print_mdoc_node(DECL_ARGS)
{
	int		 chld;
	struct termpair	 npair;
	size_t		 offset, rmargin;

	chld = 1;
	offset = p->offset;
	rmargin = p->rmargin;
	n->prev_font = term_fontq(p);

	memset(&npair, 0, sizeof(struct termpair));
	npair.ppair = pair;

	/*
	 * Keeps only work until the end of a line.  If a keep was
	 * invoked in a prior line, revert it to PREKEEP.
	 */

	if (TERMP_KEEP & p->flags) {
		if (n->prev ? (n->prev->lastline != n->line) :
		    (n->parent && n->parent->line != n->line)) {
			p->flags &= ~TERMP_KEEP;
			p->flags |= TERMP_PREKEEP;
		}
	}

	/*
	 * After the keep flags have been set up, we may now
	 * produce output.  Note that some pre-handlers do so.
	 */

	switch (n->type) {
	case (MDOC_TEXT):
		if (' ' == *n->string && MDOC_LINE & n->flags)
			term_newln(p);
		if (MDOC_DELIMC & n->flags)
			p->flags |= TERMP_NOSPACE;
		term_word(p, n->string);
		if (MDOC_DELIMO & n->flags)
			p->flags |= TERMP_NOSPACE;
		break;
	case (MDOC_EQN):
		term_eqn(p, n->eqn);
		break;
	case (MDOC_TBL):
		term_tbl(p, n->span);
		break;
	default:
		if (termacts[n->tok].pre && ENDBODY_NOT == n->end)
			chld = (*termacts[n->tok].pre)
				(p, &npair, meta, n);
		break;
	}

	if (chld && n->child)
		print_mdoc_nodelist(p, &npair, meta, n->child);

	term_fontpopq(p,
	    (ENDBODY_NOT == n->end ? n : n->pending)->prev_font);

	switch (n->type) {
	case (MDOC_TEXT):
		break;
	case (MDOC_TBL):
		break;
	case (MDOC_EQN):
		break;
	default:
		if ( ! termacts[n->tok].post || MDOC_ENDED & n->flags)
			break;
		(void)(*termacts[n->tok].post)(p, &npair, meta, n);

		/*
		 * Explicit end tokens not only call the post
		 * handler, but also tell the respective block
		 * that it must not call the post handler again.
		 */
		if (ENDBODY_NOT != n->end)
			n->pending->flags |= MDOC_ENDED;

		/*
		 * End of line terminating an implicit block
		 * while an explicit block is still open.
		 * Continue the explicit block without spacing.
		 */
		if (ENDBODY_NOSPACE == n->end)
			p->flags |= TERMP_NOSPACE;
		break;
	}

	if (MDOC_EOS & n->flags)
		p->flags |= TERMP_SENTENCE;

	p->offset = offset;
	p->rmargin = rmargin;
}


static void
print_mdoc_foot(struct termp *p, const void *arg)
{
	const struct mdoc_meta *meta;

	meta = (const struct mdoc_meta *)arg;

	term_fontrepl(p, TERMFONT_NONE);

	/* 
	 * Output the footer in new-groff style, that is, three columns
	 * with the middle being the manual date and flanking columns
	 * being the operating system:
	 *
	 * SYSTEM                  DATE                    SYSTEM
	 */

	term_vspace(p);

	p->offset = 0;
	p->rmargin = (p->maxrmargin - 
			term_strlen(p, meta->date) + term_len(p, 1)) / 2;
	p->trailspace = 1;
	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;

	term_word(p, meta->os);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - term_strlen(p, meta->os);
	p->flags |= TERMP_NOSPACE;

	term_word(p, meta->date);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->trailspace = 0;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOSPACE;

	term_word(p, meta->os);
	term_flushln(p);

	p->offset = 0;
	p->rmargin = p->maxrmargin;
	p->flags = 0;
}


static void
print_mdoc_head(struct termp *p, const void *arg)
{
	char		buf[BUFSIZ], title[BUFSIZ];
	size_t		buflen, titlen;
	const struct mdoc_meta *meta;

	meta = (const struct mdoc_meta *)arg;

	/*
	 * The header is strange.  It has three components, which are
	 * really two with the first duplicated.  It goes like this:
	 *
	 * IDENTIFIER              TITLE                   IDENTIFIER
	 *
	 * The IDENTIFIER is NAME(SECTION), which is the command-name
	 * (if given, or "unknown" if not) followed by the manual page
	 * section.  These are given in `Dt'.  The TITLE is a free-form
	 * string depending on the manual volume.  If not specified, it
	 * switches on the manual section.
	 */

	p->offset = 0;
	p->rmargin = p->maxrmargin;

	assert(meta->vol);
	strlcpy(buf, meta->vol, BUFSIZ);
	buflen = term_strlen(p, buf);

	if (meta->arch) {
		strlcat(buf, " (", BUFSIZ);
		strlcat(buf, meta->arch, BUFSIZ);
		strlcat(buf, ")", BUFSIZ);
	}

	snprintf(title, BUFSIZ, "%s(%s)", meta->title, meta->msec);
	titlen = term_strlen(p, title);

	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;
	p->trailspace = 1;
	p->offset = 0;
	p->rmargin = 2 * (titlen+1) + buflen < p->maxrmargin ?
	    (p->maxrmargin -
	     term_strlen(p, buf) + term_len(p, 1)) / 2 :
	    p->maxrmargin - buflen;

	term_word(p, title);
	term_flushln(p);

	p->flags |= TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->offset + buflen + titlen < p->maxrmargin ?
	    p->maxrmargin - titlen : p->maxrmargin;

	term_word(p, buf);
	term_flushln(p);

	p->flags &= ~TERMP_NOBREAK;
	p->trailspace = 0;
	if (p->rmargin + titlen <= p->maxrmargin) {
		p->flags |= TERMP_NOSPACE;
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		term_word(p, title);
		term_flushln(p);
	}

	p->flags &= ~TERMP_NOSPACE;
	p->offset = 0;
	p->rmargin = p->maxrmargin;
}


static size_t
a2height(const struct termp *p, const char *v)
{
	struct roffsu	 su;


	assert(v);
	if ( ! a2roffsu(v, &su, SCALE_VS))
		SCALE_VS_INIT(&su, atoi(v));

	return(term_vspan(p, &su));
}


static size_t
a2width(const struct termp *p, const char *v)
{
	struct roffsu	 su;

	assert(v);
	if ( ! a2roffsu(v, &su, SCALE_MAX))
		SCALE_HS_INIT(&su, term_strlen(p, v));

	return(term_hspan(p, &su));
}


static size_t
a2offs(const struct termp *p, const char *v)
{
	struct roffsu	 su;

	if ('\0' == *v)
		return(0);
	else if (0 == strcmp(v, "left"))
		return(0);
	else if (0 == strcmp(v, "indent"))
		return(term_len(p, p->defindent + 1));
	else if (0 == strcmp(v, "indent-two"))
		return(term_len(p, (p->defindent + 1) * 2));
	else if ( ! a2roffsu(v, &su, SCALE_MAX))
		SCALE_HS_INIT(&su, term_strlen(p, v));

	return(term_hspan(p, &su));
}


/*
 * Determine how much space to print out before block elements of `It'
 * (and thus `Bl') and `Bd'.  And then go ahead and print that space,
 * too.
 */
static void
print_bvspace(struct termp *p, 
		const struct mdoc_node *bl, 
		const struct mdoc_node *n)
{
	const struct mdoc_node	*nn;

	assert(n);

	term_newln(p);

	if (MDOC_Bd == bl->tok && bl->norm->Bd.comp)
		return;
	if (MDOC_Bl == bl->tok && bl->norm->Bl.comp)
		return;

	/* Do not vspace directly after Ss/Sh. */

	for (nn = n; nn; nn = nn->parent) {
		if (MDOC_BLOCK != nn->type)
			continue;
		if (MDOC_Ss == nn->tok)
			return;
		if (MDOC_Sh == nn->tok)
			return;
		if (NULL == nn->prev)
			continue;
		break;
	}

	/* A `-column' does not assert vspace within the list. */

	if (MDOC_Bl == bl->tok && LIST_column == bl->norm->Bl.type)
		if (n->prev && MDOC_It == n->prev->tok)
			return;

	/* A `-diag' without body does not vspace. */

	if (MDOC_Bl == bl->tok && LIST_diag == bl->norm->Bl.type)
		if (n->prev && MDOC_It == n->prev->tok) {
			assert(n->prev->body);
			if (NULL == n->prev->body->child)
				return;
		}

	term_vspace(p);
}


/* ARGSUSED */
static int
termp_it_pre(DECL_ARGS)
{
	const struct mdoc_node *bl, *nn;
	char		        buf[7];
	int		        i;
	size_t		        width, offset, ncols, dcol;
	enum mdoc_list		type;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n->parent->parent, n);
		return(1);
	}

	bl = n->parent->parent->parent;
	type = bl->norm->Bl.type;

	/* 
	 * First calculate width and offset.  This is pretty easy unless
	 * we're a -column list, in which case all prior columns must
	 * be accounted for.
	 */

	width = offset = 0;

	if (bl->norm->Bl.offs)
		offset = a2offs(p, bl->norm->Bl.offs);

	switch (type) {
	case (LIST_column):
		if (MDOC_HEAD == n->type)
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

		/* LINTED */
		dcol = ncols < 5 ? term_len(p, 4) : 
			ncols == 5 ? term_len(p, 3) : term_len(p, 1);

		/*
		 * Calculate the offset by applying all prior MDOC_BODY,
		 * so we stop at the MDOC_HEAD (NULL == nn->prev).
		 */

		for (i = 0, nn = n->prev; 
				nn->prev && i < (int)ncols; 
				nn = nn->prev, i++)
			offset += dcol + a2width
				(p, bl->norm->Bl.cols[i]);

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
		width = a2width(p, bl->norm->Bl.cols[i]) + dcol;
		break;
	default:
		if (NULL == bl->norm->Bl.width)
			break;

		/* 
		 * Note: buffer the width by 2, which is groff's magic
		 * number for buffering single arguments.  See the above
		 * handling for column for how this changes.
		 */
		assert(bl->norm->Bl.width);
		width = a2width(p, bl->norm->Bl.width) + term_len(p, 2);
		break;
	}

	/* 
	 * List-type can override the width in the case of fixed-head
	 * values (bullet, dash/hyphen, enum).  Tags need a non-zero
	 * offset.
	 */

	switch (type) {
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_enum):
		if (width < term_len(p, 2))
			width = term_len(p, 2);
		break;
	case (LIST_hang):
		if (0 == width)
			width = term_len(p, 8);
		break;
	case (LIST_column):
		/* FALLTHROUGH */
	case (LIST_tag):
		if (0 == width)
			width = term_len(p, 10);
		break;
	default:
		break;
	}

	/* 
	 * Whitespace control.  Inset bodies need an initial space,
	 * while diagonal bodies need two.
	 */

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case (LIST_diag):
		if (MDOC_BODY == n->type)
			term_word(p, "\\ \\ ");
		break;
	case (LIST_inset):
		if (MDOC_BODY == n->type) 
			term_word(p, "\\ ");
		break;
	default:
		break;
	}

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case (LIST_diag):
		if (MDOC_HEAD == n->type)
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
	case (LIST_enum):
		/*
		 * Weird special case.
		 * Very narrow enum lists actually hang.
		 */
		if (width == term_len(p, 2))
			p->flags |= TERMP_HANG;
		/* FALLTHROUGH */
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		if (MDOC_HEAD != n->type)
			break;
		p->flags |= TERMP_NOBREAK;
		p->trailspace = 1;
		break;
	case (LIST_hang):
		if (MDOC_HEAD != n->type)
			break;

		/*
		 * This is ugly.  If `-hang' is specified and the body
		 * is a `Bl' or `Bd', then we want basically to nullify
		 * the "overstep" effect in term_flushln() and treat
		 * this as a `-ohang' list instead.
		 */
		if (n->next->child && 
				(MDOC_Bl == n->next->child->tok ||
				 MDOC_Bd == n->next->child->tok))
			break;

		p->flags |= TERMP_NOBREAK | TERMP_HANG;
		p->trailspace = 1;
		break;
	case (LIST_tag):
		if (MDOC_HEAD != n->type)
			break;

		p->flags |= TERMP_NOBREAK;
		p->trailspace = 2;

		if (NULL == n->next || NULL == n->next->child)
			p->flags |= TERMP_DANGLE;
		break;
	case (LIST_column):
		if (MDOC_HEAD == n->type)
			break;

		if (NULL == n->next) {
			p->flags &= ~TERMP_NOBREAK;
			p->trailspace = 0;
		} else {
			p->flags |= TERMP_NOBREAK;
			p->trailspace = 1;
		}

		break;
	case (LIST_diag):
		if (MDOC_HEAD != n->type)
			break;
		p->flags |= TERMP_NOBREAK;
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

	p->offset += offset;

	switch (type) {
	case (LIST_hang):
		/*
		 * Same stipulation as above, regarding `-hang'.  We
		 * don't want to recalculate rmargin and offsets when
		 * using `Bd' or `Bl' within `-hang' overstep lists.
		 */
		if (MDOC_HEAD == n->type && n->next->child &&
				(MDOC_Bl == n->next->child->tok || 
				 MDOC_Bd == n->next->child->tok))
			break;
		/* FALLTHROUGH */
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_enum):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_tag):
		assert(width);
		if (MDOC_HEAD == n->type)
			p->rmargin = p->offset + width;
		else 
			p->offset += width;
		break;
	case (LIST_column):
		assert(width);
		p->rmargin = p->offset + width;
		/* 
		 * XXX - this behaviour is not documented: the
		 * right-most column is filled to the right margin.
		 */
		if (MDOC_HEAD == n->type)
			break;
		if (NULL == n->next && p->rmargin < p->maxrmargin)
			p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	/* 
	 * The dash, hyphen, bullet and enum lists all have a special
	 * HEAD character (temporarily bold, in some cases).  
	 */

	if (MDOC_HEAD == n->type)
		switch (type) {
		case (LIST_bullet):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\[bu]");
			term_fontpop(p);
			break;
		case (LIST_dash):
			/* FALLTHROUGH */
		case (LIST_hyphen):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\(hy");
			term_fontpop(p);
			break;
		case (LIST_enum):
			(pair->ppair->ppair->count)++;
			snprintf(buf, sizeof(buf), "%d.", 
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
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_enum):
		if (MDOC_HEAD == n->type)
			return(0);
		break;
	case (LIST_column):
		if (MDOC_HEAD == n->type)
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
termp_it_post(DECL_ARGS)
{
	enum mdoc_list	   type;

	if (MDOC_BLOCK == n->type)
		return;

	type = n->parent->parent->parent->norm->Bl.type;

	switch (type) {
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_diag):
		/* FALLTHROUGH */
	case (LIST_inset):
		if (MDOC_BODY == n->type)
			term_newln(p);
		break;
	case (LIST_column):
		if (MDOC_BODY == n->type)
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

	p->flags &= ~TERMP_DANGLE;
	p->flags &= ~TERMP_NOBREAK;
	p->flags &= ~TERMP_HANG;
	p->trailspace = 0;
}


/* ARGSUSED */
static int
termp_nm_pre(DECL_ARGS)
{

	if (MDOC_BLOCK == n->type) {
		p->flags |= TERMP_PREKEEP;
		return(1);
	}

	if (MDOC_BODY == n->type) {
		if (NULL == n->child)
			return(0);
		p->flags |= TERMP_NOSPACE;
		p->offset += term_len(p, 1) +
		    (NULL == n->prev->child ?
		     term_strlen(p, meta->name) :
		     MDOC_TEXT == n->prev->child->type ?
		     term_strlen(p, n->prev->child->string) :
		     term_len(p, 5));
		return(1);
	}

	if (NULL == n->child && NULL == meta->name)
		return(0);

	if (MDOC_HEAD == n->type)
		synopsis_pre(p, n->parent);

	if (MDOC_HEAD == n->type && n->next->child) {
		p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;
		p->trailspace = 1;
		p->rmargin = p->offset + term_len(p, 1);
		if (NULL == n->child) {
			p->rmargin += term_strlen(p, meta->name);
		} else if (MDOC_TEXT == n->child->type) {
			p->rmargin += term_strlen(p, n->child->string);
			if (n->child->next)
				p->flags |= TERMP_HANG;
		} else {
			p->rmargin += term_len(p, 5);
			p->flags |= TERMP_HANG;
		}
	}

	term_fontpush(p, TERMFONT_BOLD);
	if (NULL == n->child)
		term_word(p, meta->name);
	return(1);
}


/* ARGSUSED */
static void
termp_nm_post(DECL_ARGS)
{

	if (MDOC_BLOCK == n->type) {
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
	} else if (MDOC_HEAD == n->type && n->next->child) {
		term_flushln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_HANG);
		p->trailspace = 0;
	} else if (MDOC_BODY == n->type && n->child)
		term_flushln(p);
}

		
/* ARGSUSED */
static int
termp_fl_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, "\\-");

	if (n->child)
		p->flags |= TERMP_NOSPACE;
	else if (n->next && n->next->line == n->line)
		p->flags |= TERMP_NOSPACE;

	return(1);
}


/* ARGSUSED */
static int
termp__a_pre(DECL_ARGS)
{

	if (n->prev && MDOC__A == n->prev->tok)
		if (NULL == n->next || MDOC__A != n->next->tok)
			term_word(p, "and");

	return(1);
}


/* ARGSUSED */
static int
termp_an_pre(DECL_ARGS)
{

	if (NULL == n->child)
		return(1);

	/*
	 * If not in the AUTHORS section, `An -split' will cause
	 * newlines to occur before the author name.  If in the AUTHORS
	 * section, by default, the first `An' invocation is nosplit,
	 * then all subsequent ones, regardless of whether interspersed
	 * with other macros/text, are split.  -split, in this case,
	 * will override the condition of the implied first -nosplit.
	 */
	
	if (n->sec == SEC_AUTHORS) {
		if ( ! (TERMP_ANPREC & p->flags)) {
			if (TERMP_SPLIT & p->flags)
				term_newln(p);
			return(1);
		}
		if (TERMP_NOSPLIT & p->flags)
			return(1);
		term_newln(p);
		return(1);
	}

	if (TERMP_SPLIT & p->flags)
		term_newln(p);

	return(1);
}


/* ARGSUSED */
static void
termp_an_post(DECL_ARGS)
{

	if (n->child) {
		if (SEC_AUTHORS == n->sec)
			p->flags |= TERMP_ANPREC;
		return;
	}

	if (AUTH_split == n->norm->An.auth) {
		p->flags &= ~TERMP_NOSPLIT;
		p->flags |= TERMP_SPLIT;
	} else if (AUTH_nosplit == n->norm->An.auth) {
		p->flags &= ~TERMP_SPLIT;
		p->flags |= TERMP_NOSPLIT;
	}

}


/* ARGSUSED */
static int
termp_ns_pre(DECL_ARGS)
{

	if ( ! (MDOC_LINE & n->flags))
		p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
termp_rs_pre(DECL_ARGS)
{

	if (SEC_SEE_ALSO != n->sec)
		return(1);
	if (MDOC_BLOCK == n->type && n->prev)
		term_vspace(p);
	return(1);
}


/* ARGSUSED */
static int
termp_rv_pre(DECL_ARGS)
{
	int		 nchild;

	term_newln(p);
	term_word(p, "The");

	nchild = n->nchild;
	for (n = n->child; n; n = n->next) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, n->string);
		term_fontpop(p);

		p->flags |= TERMP_NOSPACE;
		term_word(p, "()");

		if (nchild > 2 && n->next) {
			p->flags |= TERMP_NOSPACE;
			term_word(p, ",");
		}

		if (n->next && NULL == n->next->next)
			term_word(p, "and");
	}

	if (nchild > 1)
		term_word(p, "functions return");
	else
		term_word(p, "function returns");

       	term_word(p, "the value 0 if successful; otherwise the value "
			"-1 is returned and the global variable");

	term_fontpush(p, TERMFONT_UNDER);
	term_word(p, "errno");
	term_fontpop(p);

       	term_word(p, "is set to indicate the error.");
	p->flags |= TERMP_SENTENCE;

	return(0);
}


/* ARGSUSED */
static int
termp_ex_pre(DECL_ARGS)
{
	int		 nchild;

	term_newln(p);
	term_word(p, "The");

	nchild = n->nchild;
	for (n = n->child; n; n = n->next) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, n->string);
		term_fontpop(p);

		if (nchild > 2 && n->next) {
			p->flags |= TERMP_NOSPACE;
			term_word(p, ",");
		}

		if (n->next && NULL == n->next->next)
			term_word(p, "and");
	}

	if (nchild > 1)
		term_word(p, "utilities exit");
	else
		term_word(p, "utility exits");

       	term_word(p, "0 on success, and >0 if an error occurs.");

	p->flags |= TERMP_SENTENCE;
	return(0);
}


/* ARGSUSED */
static int
termp_nd_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);

#if defined(__OpenBSD__) || defined(__linux__)
	term_word(p, "\\(en");
#else
	term_word(p, "\\(em");
#endif
	return(1);
}


/* ARGSUSED */
static int
termp_bl_pre(DECL_ARGS)
{

	return(MDOC_HEAD != n->type);
}


/* ARGSUSED */
static void
termp_bl_post(DECL_ARGS)
{

	if (MDOC_BLOCK == n->type)
		term_newln(p);
}

/* ARGSUSED */
static int
termp_xr_pre(DECL_ARGS)
{

	if (NULL == (n = n->child))
		return(0);

	assert(MDOC_TEXT == n->type);
	term_word(p, n->string);

	if (NULL == (n = n->next)) 
		return(0);

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;

	assert(MDOC_TEXT == n->type);
	term_word(p, n->string);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	return(0);
}

/*
 * This decides how to assert whitespace before any of the SYNOPSIS set
 * of macros (which, as in the case of Ft/Fo and Ft/Fn, may contain
 * macro combos).
 */
static void
synopsis_pre(struct termp *p, const struct mdoc_node *n)
{
	/* 
	 * Obviously, if we're not in a SYNOPSIS or no prior macros
	 * exist, do nothing.
	 */
	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	/*
	 * If we're the second in a pair of like elements, emit our
	 * newline and return.  UNLESS we're `Fo', `Fn', `Fn', in which
	 * case we soldier on.
	 */
	if (n->prev->tok == n->tok && 
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
		term_vspace(p);
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
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

	if (MDOC_ELEM == n->type) {
		synopsis_pre(p, n);
		return(termp_under_pre(p, pair, meta, n));
	} else if (MDOC_BLOCK == n->type) {
		synopsis_pre(p, n);
		return(1);
	} else if (MDOC_HEAD == n->type)
		return(0);

	return(termp_under_pre(p, pair, meta, n));
}


/* ARGSUSED */
static int
termp_bold_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static int
termp_fd_pre(DECL_ARGS)
{

	synopsis_pre(p, n);
	return(termp_bold_pre(p, pair, meta, n));
}


/* ARGSUSED */
static void
termp_fd_post(DECL_ARGS)
{

	term_newln(p);
}


/* ARGSUSED */
static int
termp_sh_pre(DECL_ARGS)
{

	/* No vspace between consecutive `Sh' calls. */

	switch (n->type) {
	case (MDOC_BLOCK):
		if (n->prev && MDOC_Sh == n->prev->tok)
			if (NULL == n->prev->body->child)
				break;
		term_vspace(p);
		break;
	case (MDOC_HEAD):
		term_fontpush(p, TERMFONT_BOLD);
		break;
	case (MDOC_BODY):
		p->offset = term_len(p, p->defindent);
		if (SEC_AUTHORS == n->sec)
			p->flags &= ~(TERMP_SPLIT|TERMP_NOSPLIT);
		break;
	default:
		break;
	}
	return(1);
}


/* ARGSUSED */
static void
termp_sh_post(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_HEAD):
		term_newln(p);
		break;
	case (MDOC_BODY):
		term_newln(p);
		p->offset = 0;
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
termp_bt_pre(DECL_ARGS)
{

	term_word(p, "is currently in beta test.");
	p->flags |= TERMP_SENTENCE;
	return(0);
}


/* ARGSUSED */
static void
termp_lb_post(DECL_ARGS)
{

	if (SEC_LIBRARY == n->sec && MDOC_LINE & n->flags)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_ud_pre(DECL_ARGS)
{

	term_word(p, "currently under development.");
	p->flags |= TERMP_SENTENCE;
	return(0);
}


/* ARGSUSED */
static int
termp_d1_pre(DECL_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	term_newln(p);
	p->offset += term_len(p, p->defindent + 1);
	return(1);
}


/* ARGSUSED */
static int
termp_ft_pre(DECL_ARGS)
{

	/* NB: MDOC_LINE does not effect this! */
	synopsis_pre(p, n);
	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}


/* ARGSUSED */
static int
termp_fn_pre(DECL_ARGS)
{
	size_t		 rmargin = 0;
	int		 pretty;

	pretty = MDOC_SYNPRETTY & n->flags;

	synopsis_pre(p, n);

	if (NULL == (n = n->child))
		return(0);

	if (pretty) {
		rmargin = p->rmargin;
		p->rmargin = p->offset + term_len(p, 4);
		p->flags |= TERMP_NOBREAK | TERMP_HANG;
	}

	assert(MDOC_TEXT == n->type);
	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, n->string);
	term_fontpop(p);

	if (pretty) {
		term_flushln(p);
		p->flags &= ~(TERMP_NOBREAK | TERMP_HANG);
		p->offset = p->rmargin;
		p->rmargin = rmargin;
	}

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;

	for (n = n->next; n; n = n->next) {
		assert(MDOC_TEXT == n->type);
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

	return(0);
}


/* ARGSUSED */
static int
termp_fa_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	if (n->parent->tok != MDOC_Fo) {
		term_fontpush(p, TERMFONT_UNDER);
		return(1);
	}

	for (nn = n->child; nn; nn = nn->next) {
		term_fontpush(p, TERMFONT_UNDER);
		p->flags |= TERMP_NBRWORD;
		term_word(p, nn->string);
		term_fontpop(p);

		if (nn->next || (n->next && n->next->tok == MDOC_Fa)) {
			p->flags |= TERMP_NOSPACE;
			term_word(p, ",");
		}
	}

	return(0);
}


/* ARGSUSED */
static int
termp_bd_pre(DECL_ARGS)
{
	size_t			 tabwidth, rm, rmax;
	struct mdoc_node	*nn;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n, n);
		return(1);
	} else if (MDOC_HEAD == n->type)
		return(0);

	if (n->norm->Bd.offs)
		p->offset += a2offs(p, n->norm->Bd.offs);

	/*
	 * If -ragged or -filled are specified, the block does nothing
	 * but change the indentation.  If -unfilled or -literal are
	 * specified, text is printed exactly as entered in the display:
	 * for macro lines, a newline is appended to the line.  Blank
	 * lines are allowed.
	 */
	
	if (DISP_literal != n->norm->Bd.type && 
			DISP_unfilled != n->norm->Bd.type)
		return(1);

	tabwidth = p->tabwidth;
	if (DISP_literal == n->norm->Bd.type)
		p->tabwidth = term_len(p, 8);

	rm = p->rmargin;
	rmax = p->maxrmargin;
	p->rmargin = p->maxrmargin = TERM_MAXMARGIN;

	for (nn = n->child; nn; nn = nn->next) {
		print_mdoc_node(p, pair, meta, nn);
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
		term_flushln(p);
		p->flags |= TERMP_NOSPACE;
	}

	p->tabwidth = tabwidth;
	p->rmargin = rm;
	p->maxrmargin = rmax;
	return(0);
}


/* ARGSUSED */
static void
termp_bd_post(DECL_ARGS)
{
	size_t		 rm, rmax;

	if (MDOC_BODY != n->type) 
		return;

	rm = p->rmargin;
	rmax = p->maxrmargin;

	if (DISP_literal == n->norm->Bd.type || 
			DISP_unfilled == n->norm->Bd.type)
		p->rmargin = p->maxrmargin = TERM_MAXMARGIN;

	p->flags |= TERMP_NOSPACE;
	term_newln(p);

	p->rmargin = rm;
	p->maxrmargin = rmax;
}


/* ARGSUSED */
static int
termp_bx_pre(DECL_ARGS)
{

	if (NULL != (n = n->child)) {
		term_word(p, n->string);
		p->flags |= TERMP_NOSPACE;
		term_word(p, "BSD");
	} else {
		term_word(p, "BSD");
		return(0);
	}

	if (NULL != (n = n->next)) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, "-");
		p->flags |= TERMP_NOSPACE;
		term_word(p, n->string);
	}

	return(0);
}


/* ARGSUSED */
static int
termp_xx_pre(DECL_ARGS)
{
	const char	*pp;
	int		 flags;

	pp = NULL;
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
		abort();
		/* NOTREACHED */
	}

	term_word(p, pp);
	if (n->child) {
		flags = p->flags;
		p->flags |= TERMP_KEEP;
		term_word(p, n->child->string);
		p->flags = flags;
	}
	return(0);
}


/* ARGSUSED */
static void
termp_pf_post(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
}


/* ARGSUSED */
static int
termp_ss_pre(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		term_newln(p);
		if (n->prev)
			term_vspace(p);
		break;
	case (MDOC_HEAD):
		term_fontpush(p, TERMFONT_BOLD);
		p->offset = term_len(p, (p->defindent+1)/2);
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
termp_ss_post(DECL_ARGS)
{

	if (MDOC_HEAD == n->type)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_cd_pre(DECL_ARGS)
{

	synopsis_pre(p, n);
	term_fontpush(p, TERMFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static int
termp_in_pre(DECL_ARGS)
{

	synopsis_pre(p, n);

	if (MDOC_SYNPRETTY & n->flags && MDOC_LINE & n->flags) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, "#include");
		term_word(p, "<");
	} else {
		term_word(p, "<");
		term_fontpush(p, TERMFONT_UNDER);
	}

	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_in_post(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags)
		term_fontpush(p, TERMFONT_BOLD);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");

	if (MDOC_SYNPRETTY & n->flags)
		term_fontpop(p);
}


/* ARGSUSED */
static int
termp_sp_pre(DECL_ARGS)
{
	size_t		 i, len;

	switch (n->tok) {
	case (MDOC_sp):
		len = n->child ? a2height(p, n->child->string) : 1;
		break;
	case (MDOC_br):
		len = 0;
		break;
	default:
		len = 1;
		break;
	}

	if (0 == len)
		term_newln(p);
	for (i = 0; i < len; i++)
		term_vspace(p);

	return(0);
}


/* ARGSUSED */
static int
termp_quote_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type && MDOC_ELEM != n->type)
		return(1);

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		term_word(p, "<");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		term_word(p, "{");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		term_word(p, "[");
		break;
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		term_word(p, "\\(lq");
		break;
	case (MDOC_Eo):
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		term_word(p, "(");
		break;
	case (MDOC__T):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		term_word(p, "\"");
		break;
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		term_word(p, "\\(oq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_quote_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type && MDOC_ELEM != n->type)
		return;

	p->flags |= TERMP_NOSPACE;

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		term_word(p, ">");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		term_word(p, "}");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		term_word(p, "]");
		break;
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		term_word(p, "\\(rq");
		break;
	case (MDOC_Eo):
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		term_word(p, ")");
		break;
	case (MDOC__T):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		term_word(p, "\"");
		break;
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		term_word(p, "\\(cq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}


/* ARGSUSED */
static int
termp_fo_pre(DECL_ARGS)
{
	size_t		 rmargin = 0;
	int		 pretty;

	pretty = MDOC_SYNPRETTY & n->flags;

	if (MDOC_BLOCK == n->type) {
		synopsis_pre(p, n);
		return(1);
	} else if (MDOC_BODY == n->type) {
		if (pretty) {
			rmargin = p->rmargin;
			p->rmargin = p->offset + term_len(p, 4);
			p->flags |= TERMP_NOBREAK | TERMP_HANG;
		}
		p->flags |= TERMP_NOSPACE;
		term_word(p, "(");
		p->flags |= TERMP_NOSPACE;
		if (pretty) {
			term_flushln(p);
			p->flags &= ~(TERMP_NOBREAK | TERMP_HANG);
			p->offset = p->rmargin;
			p->rmargin = rmargin;
		}
		return(1);
	}

	if (NULL == n->child)
		return(0);

	/* XXX: we drop non-initial arguments as per groff. */

	assert(n->child->string);
	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, n->child->string);
	return(0);
}


/* ARGSUSED */
static void
termp_fo_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type) 
		return;

	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	if (MDOC_SYNPRETTY & n->flags) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, ";");
		term_flushln(p);
	}
}


/* ARGSUSED */
static int
termp_bf_pre(DECL_ARGS)
{

	if (MDOC_HEAD == n->type)
		return(0);
	else if (MDOC_BODY != n->type)
		return(1);

	if (FONT_Em == n->norm->Bf.font) 
		term_fontpush(p, TERMFONT_UNDER);
	else if (FONT_Sy == n->norm->Bf.font) 
		term_fontpush(p, TERMFONT_BOLD);
	else 
		term_fontpush(p, TERMFONT_NONE);

	return(1);
}


/* ARGSUSED */
static int
termp_sm_pre(DECL_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string)) {
		if (p->col)
			p->flags &= ~TERMP_NOSPACE;
		p->flags &= ~TERMP_NONOSPACE;
	} else
		p->flags |= TERMP_NONOSPACE;

	return(0);
}


/* ARGSUSED */
static int
termp_ap_pre(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
	term_word(p, "'");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp____post(DECL_ARGS)
{

	/*
	 * Handle lists of authors.  In general, print each followed by
	 * a comma.  Don't print the comma if there are only two
	 * authors.
	 */
	if (MDOC__A == n->tok && n->next && MDOC__A == n->next->tok)
		if (NULL == n->next->next || MDOC__A != n->next->next->tok)
			if (NULL == n->prev || MDOC__A != n->prev->tok)
				return;

	/* TODO: %U. */

	if (NULL == n->parent || MDOC_Rs != n->parent->tok)
		return;

	p->flags |= TERMP_NOSPACE;
	if (NULL == n->next) {
		term_word(p, ".");
		p->flags |= TERMP_SENTENCE;
	} else
		term_word(p, ",");
}


/* ARGSUSED */
static int
termp_li_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_NONE);
	return(1);
}


/* ARGSUSED */
static int
termp_lk_pre(DECL_ARGS)
{
	const struct mdoc_node *link, *descr;

	if (NULL == (link = n->child))
		return(0);

	if (NULL != (descr = link->next)) {
		term_fontpush(p, TERMFONT_UNDER);
		while (NULL != descr) {
			term_word(p, descr->string);
			descr = descr->next;
		}
		p->flags |= TERMP_NOSPACE;
		term_word(p, ":");
		term_fontpop(p);
	}

	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, link->string);
	term_fontpop(p);

	return(0);
}


/* ARGSUSED */
static int
termp_bk_pre(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		break;
	case (MDOC_HEAD):
		return(0);
	case (MDOC_BODY):
		if (n->parent->args || 0 == n->prev->nchild)
			p->flags |= TERMP_PREKEEP;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}


/* ARGSUSED */
static void
termp_bk_post(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
}

/* ARGSUSED */
static void
termp__t_post(DECL_ARGS)
{

	/*
	 * If we're in an `Rs' and there's a journal present, then quote
	 * us instead of underlining us (for disambiguation).
	 */
	if (n->parent && MDOC_Rs == n->parent->tok &&
			n->parent->norm->Rs.quote_T)
		termp_quote_post(p, pair, meta, n);

	termp____post(p, pair, meta, n);
}

/* ARGSUSED */
static int
termp__t_pre(DECL_ARGS)
{

	/*
	 * If we're in an `Rs' and there's a journal present, then quote
	 * us instead of underlining us (for disambiguation).
	 */
	if (n->parent && MDOC_Rs == n->parent->tok &&
			n->parent->norm->Rs.quote_T)
		return(termp_quote_pre(p, pair, meta, n));

	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}

/* ARGSUSED */
static int
termp_under_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}
