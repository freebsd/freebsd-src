/*	$Id: mdoc_man.c,v 1.57 2013/12/25 22:00:45 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012, 2013 Ingo Schwarze <schwarze@openbsd.org>
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "man.h"
#include "mdoc.h"
#include "main.h"

#define	DECL_ARGS const struct mdoc_meta *meta, \
		  const struct mdoc_node *n

struct	manact {
	int		(*cond)(DECL_ARGS); /* DON'T run actions */
	int		(*pre)(DECL_ARGS); /* pre-node action */
	void		(*post)(DECL_ARGS); /* post-node action */
	const char	 *prefix; /* pre-node string constant */
	const char	 *suffix; /* post-node string constant */
};

static	int	  cond_body(DECL_ARGS);
static	int	  cond_head(DECL_ARGS);
static  void	  font_push(char);
static	void	  font_pop(void);
static	void	  mid_it(void);
static	void	  post__t(DECL_ARGS);
static	void	  post_bd(DECL_ARGS);
static	void	  post_bf(DECL_ARGS);
static	void	  post_bk(DECL_ARGS);
static	void	  post_bl(DECL_ARGS);
static	void	  post_dl(DECL_ARGS);
static	void	  post_enc(DECL_ARGS);
static	void	  post_eo(DECL_ARGS);
static	void	  post_fa(DECL_ARGS);
static	void	  post_fd(DECL_ARGS);
static	void	  post_fl(DECL_ARGS);
static	void	  post_fn(DECL_ARGS);
static	void	  post_fo(DECL_ARGS);
static	void	  post_font(DECL_ARGS);
static	void	  post_in(DECL_ARGS);
static	void	  post_it(DECL_ARGS);
static	void	  post_lb(DECL_ARGS);
static	void	  post_nm(DECL_ARGS);
static	void	  post_percent(DECL_ARGS);
static	void	  post_pf(DECL_ARGS);
static	void	  post_sect(DECL_ARGS);
static	void	  post_sp(DECL_ARGS);
static	void	  post_vt(DECL_ARGS);
static	int	  pre__t(DECL_ARGS);
static	int	  pre_an(DECL_ARGS);
static	int	  pre_ap(DECL_ARGS);
static	int	  pre_bd(DECL_ARGS);
static	int	  pre_bf(DECL_ARGS);
static	int	  pre_bk(DECL_ARGS);
static	int	  pre_bl(DECL_ARGS);
static	int	  pre_br(DECL_ARGS);
static	int	  pre_bx(DECL_ARGS);
static	int	  pre_dl(DECL_ARGS);
static	int	  pre_enc(DECL_ARGS);
static	int	  pre_em(DECL_ARGS);
static	int	  pre_fa(DECL_ARGS);
static	int	  pre_fd(DECL_ARGS);
static	int	  pre_fl(DECL_ARGS);
static	int	  pre_fn(DECL_ARGS);
static	int	  pre_fo(DECL_ARGS);
static	int	  pre_ft(DECL_ARGS);
static	int	  pre_in(DECL_ARGS);
static	int	  pre_it(DECL_ARGS);
static	int	  pre_lk(DECL_ARGS);
static	int	  pre_li(DECL_ARGS);
static	int	  pre_nm(DECL_ARGS);
static	int	  pre_no(DECL_ARGS);
static	int	  pre_ns(DECL_ARGS);
static	int	  pre_pp(DECL_ARGS);
static	int	  pre_rs(DECL_ARGS);
static	int	  pre_sm(DECL_ARGS);
static	int	  pre_sp(DECL_ARGS);
static	int	  pre_sect(DECL_ARGS);
static	int	  pre_sy(DECL_ARGS);
static	void	  pre_syn(const struct mdoc_node *);
static	int	  pre_vt(DECL_ARGS);
static	int	  pre_ux(DECL_ARGS);
static	int	  pre_xr(DECL_ARGS);
static	void	  print_word(const char *);
static	void	  print_line(const char *, int);
static	void	  print_block(const char *, int);
static	void	  print_offs(const char *);
static	void	  print_width(const char *,
				const struct mdoc_node *, size_t);
static	void	  print_count(int *);
static	void	  print_node(DECL_ARGS);

static	const struct manact manacts[MDOC_MAX + 1] = {
	{ NULL, pre_ap, NULL, NULL, NULL }, /* Ap */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dd */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dt */
	{ NULL, NULL, NULL, NULL, NULL }, /* Os */
	{ NULL, pre_sect, post_sect, ".SH", NULL }, /* Sh */
	{ NULL, pre_sect, post_sect, ".SS", NULL }, /* Ss */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Pp */
	{ cond_body, pre_dl, post_dl, NULL, NULL }, /* D1 */
	{ cond_body, pre_dl, post_dl, NULL, NULL }, /* Dl */
	{ cond_body, pre_bd, post_bd, NULL, NULL }, /* Bd */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ed */
	{ cond_body, pre_bl, post_bl, NULL, NULL }, /* Bl */
	{ NULL, NULL, NULL, NULL, NULL }, /* El */
	{ NULL, pre_it, post_it, NULL, NULL }, /* It */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Ad */
	{ NULL, pre_an, NULL, NULL, NULL }, /* An */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Ar */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Cd */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Cm */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Dv */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Er */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Ev */
	{ NULL, pre_enc, post_enc, "The \\fB",
	    "\\fP\nutility exits 0 on success, and >0 if an error occurs."
	    }, /* Ex */
	{ NULL, pre_fa, post_fa, NULL, NULL }, /* Fa */
	{ NULL, pre_fd, post_fd, NULL, NULL }, /* Fd */
	{ NULL, pre_fl, post_fl, NULL, NULL }, /* Fl */
	{ NULL, pre_fn, post_fn, NULL, NULL }, /* Fn */
	{ NULL, pre_ft, post_font, NULL, NULL }, /* Ft */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Ic */
	{ NULL, pre_in, post_in, NULL, NULL }, /* In */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Li */
	{ cond_head, pre_enc, NULL, "\\- ", NULL }, /* Nd */
	{ NULL, pre_nm, post_nm, NULL, NULL }, /* Nm */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Op */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ot */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Pa */
	{ NULL, pre_enc, post_enc, "The \\fB",
		"\\fP\nfunction returns the value 0 if successful;\n"
		"otherwise the value -1 is returned and the global\n"
		"variable \\fIerrno\\fP is set to indicate the error."
		}, /* Rv */
	{ NULL, NULL, NULL, NULL, NULL }, /* St */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Va */
	{ NULL, pre_vt, post_vt, NULL, NULL }, /* Vt */
	{ NULL, pre_xr, NULL, NULL, NULL }, /* Xr */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %A */
	{ NULL, pre_em, post_percent, NULL, NULL }, /* %B */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %D */
	{ NULL, pre_em, post_percent, NULL, NULL }, /* %I */
	{ NULL, pre_em, post_percent, NULL, NULL }, /* %J */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %N */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %O */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %P */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %R */
	{ NULL, pre__t, post__t, NULL, NULL }, /* %T */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %V */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ac */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Ao */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Aq */
	{ NULL, NULL, NULL, NULL, NULL }, /* At */
	{ NULL, NULL, NULL, NULL, NULL }, /* Bc */
	{ NULL, pre_bf, post_bf, NULL, NULL }, /* Bf */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bo */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bq */
	{ NULL, pre_ux, NULL, "BSD/OS", NULL }, /* Bsx */
	{ NULL, pre_bx, NULL, NULL, NULL }, /* Bx */
	{ NULL, NULL, NULL, NULL, NULL }, /* Db */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dc */
	{ cond_body, pre_enc, post_enc, "\\(lq", "\\(rq" }, /* Do */
	{ cond_body, pre_enc, post_enc, "\\(lq", "\\(rq" }, /* Dq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ec */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ef */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Em */
	{ NULL, NULL, post_eo, NULL, NULL }, /* Eo */
	{ NULL, pre_ux, NULL, "FreeBSD", NULL }, /* Fx */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Ms */
	{ NULL, pre_no, NULL, NULL, NULL }, /* No */
	{ NULL, pre_ns, NULL, NULL, NULL }, /* Ns */
	{ NULL, pre_ux, NULL, "NetBSD", NULL }, /* Nx */
	{ NULL, pre_ux, NULL, "OpenBSD", NULL }, /* Ox */
	{ NULL, NULL, NULL, NULL, NULL }, /* Pc */
	{ NULL, NULL, post_pf, NULL, NULL }, /* Pf */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Po */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Pq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Qc */
	{ cond_body, pre_enc, post_enc, "\\(oq", "\\(cq" }, /* Ql */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qo */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Re */
	{ cond_body, pre_rs, NULL, NULL, NULL }, /* Rs */
	{ NULL, NULL, NULL, NULL, NULL }, /* Sc */
	{ cond_body, pre_enc, post_enc, "\\(oq", "\\(cq" }, /* So */
	{ cond_body, pre_enc, post_enc, "\\(oq", "\\(cq" }, /* Sq */
	{ NULL, pre_sm, NULL, NULL, NULL }, /* Sm */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Sx */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Sy */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Tn */
	{ NULL, pre_ux, NULL, "UNIX", NULL }, /* Ux */
	{ NULL, NULL, NULL, NULL, NULL }, /* Xc */
	{ NULL, NULL, NULL, NULL, NULL }, /* Xo */
	{ NULL, pre_fo, post_fo, NULL, NULL }, /* Fo */
	{ NULL, NULL, NULL, NULL, NULL }, /* Fc */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Oo */
	{ NULL, NULL, NULL, NULL, NULL }, /* Oc */
	{ NULL, pre_bk, post_bk, NULL, NULL }, /* Bk */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ek */
	{ NULL, pre_ux, NULL, "is currently in beta test.", NULL }, /* Bt */
	{ NULL, NULL, NULL, NULL, NULL }, /* Hf */
	{ NULL, NULL, NULL, NULL, NULL }, /* Fr */
	{ NULL, pre_ux, NULL, "currently under development.", NULL }, /* Ud */
	{ NULL, NULL, post_lb, NULL, NULL }, /* Lb */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Lp */
	{ NULL, pre_lk, NULL, NULL, NULL }, /* Lk */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Mt */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Brq */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Bro */
	{ NULL, NULL, NULL, NULL, NULL }, /* Brc */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %C */
	{ NULL, NULL, NULL, NULL, NULL }, /* Es */
	{ NULL, NULL, NULL, NULL, NULL }, /* En */
	{ NULL, pre_ux, NULL, "DragonFly", NULL }, /* Dx */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %Q */
	{ NULL, pre_br, NULL, NULL, NULL }, /* br */
	{ NULL, pre_sp, post_sp, NULL, NULL }, /* sp */
	{ NULL, NULL, post_percent, NULL, NULL }, /* %U */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ta */
	{ NULL, NULL, NULL, NULL, NULL }, /* ROOT */
};

static	int		outflags;
#define	MMAN_spc	(1 << 0)  /* blank character before next word */
#define	MMAN_spc_force	(1 << 1)  /* even before trailing punctuation */
#define	MMAN_nl		(1 << 2)  /* break man(7) code line */
#define	MMAN_br		(1 << 3)  /* break output line */
#define	MMAN_sp		(1 << 4)  /* insert a blank output line */
#define	MMAN_PP		(1 << 5)  /* reset indentation etc. */
#define	MMAN_Sm		(1 << 6)  /* horizontal spacing mode */
#define	MMAN_Bk		(1 << 7)  /* word keep mode */
#define	MMAN_Bk_susp	(1 << 8)  /* suspend this (after a macro) */
#define	MMAN_An_split	(1 << 9)  /* author mode is "split" */
#define	MMAN_An_nosplit	(1 << 10) /* author mode is "nosplit" */
#define	MMAN_PD		(1 << 11) /* inter-paragraph spacing disabled */
#define	MMAN_nbrword	(1 << 12) /* do not break the next word */

#define	BL_STACK_MAX	32

static	size_t		Bl_stack[BL_STACK_MAX];  /* offsets [chars] */
static	int		Bl_stack_post[BL_STACK_MAX];  /* add final .RE */
static	int		Bl_stack_len;  /* number of nested Bl blocks */
static	int		TPremain;  /* characters before tag is full */

static	struct {
	char	*head;
	char	*tail;
	size_t	 size;
}	fontqueue;

static void
font_push(char newfont)
{

	if (fontqueue.head + fontqueue.size <= ++fontqueue.tail) {
		fontqueue.size += 8;
		fontqueue.head = mandoc_realloc(fontqueue.head,
				fontqueue.size);
	}
	*fontqueue.tail = newfont;
	print_word("");
	printf("\\f");
	putchar(newfont);
	outflags &= ~MMAN_spc;
}

static void
font_pop(void)
{

	if (fontqueue.tail > fontqueue.head)
		fontqueue.tail--;
	outflags &= ~MMAN_spc;
	print_word("");
	printf("\\f");
	putchar(*fontqueue.tail);
}

static void
print_word(const char *s)
{

	if ((MMAN_PP | MMAN_sp | MMAN_br | MMAN_nl) & outflags) {
		/* 
		 * If we need a newline, print it now and start afresh.
		 */
		if (MMAN_PP & outflags) {
			if (MMAN_sp & outflags) {
				if (MMAN_PD & outflags) {
					printf("\n.PD");
					outflags &= ~MMAN_PD;
				}
			} else if ( ! (MMAN_PD & outflags)) {
				printf("\n.PD 0");
				outflags |= MMAN_PD;
			}
			printf("\n.PP\n");
		} else if (MMAN_sp & outflags)
			printf("\n.sp\n");
		else if (MMAN_br & outflags)
			printf("\n.br\n");
		else if (MMAN_nl & outflags)
			putchar('\n');
		outflags &= ~(MMAN_PP|MMAN_sp|MMAN_br|MMAN_nl|MMAN_spc);
		if (1 == TPremain)
			printf(".br\n");
		TPremain = 0;
	} else if (MMAN_spc & outflags) {
		/*
		 * If we need a space, only print it if
		 * (1) it is forced by `No' or
		 * (2) what follows is not terminating punctuation or
		 * (3) what follows is longer than one character.
		 */
		if (MMAN_spc_force & outflags || '\0' == s[0] ||
		    NULL == strchr(".,:;)]?!", s[0]) || '\0' != s[1]) {
			if (MMAN_Bk & outflags &&
			    ! (MMAN_Bk_susp & outflags))
				putchar('\\');
			putchar(' ');
			if (TPremain)
				TPremain--;
		}
	}

	/*
	 * Reassign needing space if we're not following opening
	 * punctuation.
	 */
	if (MMAN_Sm & outflags && ('\0' == s[0] ||
	    (('(' != s[0] && '[' != s[0]) || '\0' != s[1])))
		outflags |= MMAN_spc;
	else
		outflags &= ~MMAN_spc;
	outflags &= ~(MMAN_spc_force | MMAN_Bk_susp);

	for ( ; *s; s++) {
		switch (*s) {
		case (ASCII_NBRSP):
			printf("\\ ");
			break;
		case (ASCII_HYPH):
			putchar('-');
			break;
		case (' '):
			if (MMAN_nbrword & outflags) {
				printf("\\ ");
				break;
			}
			/* FALLTHROUGH */
		default:
			putchar((unsigned char)*s);
			break;
		}
		if (TPremain)
			TPremain--;
	}
	outflags &= ~MMAN_nbrword;
}

static void
print_line(const char *s, int newflags)
{

	outflags &= ~MMAN_br;
	outflags |= MMAN_nl;
	print_word(s);
	outflags |= newflags;
}

static void
print_block(const char *s, int newflags)
{

	outflags &= ~MMAN_PP;
	if (MMAN_sp & outflags) {
		outflags &= ~(MMAN_sp | MMAN_br);
		if (MMAN_PD & outflags) {
			print_line(".PD", 0);
			outflags &= ~MMAN_PD;
		}
	} else if (! (MMAN_PD & outflags))
		print_line(".PD 0", MMAN_PD);
	outflags |= MMAN_nl;
	print_word(s);
	outflags |= MMAN_Bk_susp | newflags;
}

static void
print_offs(const char *v)
{
	char		  buf[24];
	struct roffsu	  su;
	size_t		  sz;

	print_line(".RS", MMAN_Bk_susp);

	/* Convert v into a number (of characters). */
	if (NULL == v || '\0' == *v || 0 == strcmp(v, "left"))
		sz = 0;
	else if (0 == strcmp(v, "indent"))
		sz = 6;
	else if (0 == strcmp(v, "indent-two"))
		sz = 12;
	else if (a2roffsu(v, &su, SCALE_MAX)) {
		if (SCALE_EN == su.unit)
			sz = su.scale;
		else {
			/*
			 * XXX
			 * If we are inside an enclosing list,
			 * there is no easy way to add the two
			 * indentations because they are provided
			 * in terms of different units.
			 */
			print_word(v);
			outflags |= MMAN_nl;
			return;
		}
	} else
		sz = strlen(v);

	/*
	 * We are inside an enclosing list.
	 * Add the two indentations.
	 */
	if (Bl_stack_len)
		sz += Bl_stack[Bl_stack_len - 1];

	snprintf(buf, sizeof(buf), "%zun", sz);
	print_word(buf);
	outflags |= MMAN_nl;
}

/*
 * Set up the indentation for a list item; used from pre_it().
 */
void
print_width(const char *v, const struct mdoc_node *child, size_t defsz)
{
	char		  buf[24];
	struct roffsu	  su;
	size_t		  sz, chsz;
	int		  numeric, remain;

	numeric = 1;
	remain = 0;

	/* Convert v into a number (of characters). */
	if (NULL == v)
		sz = defsz;
	else if (a2roffsu(v, &su, SCALE_MAX)) {
		if (SCALE_EN == su.unit)
			sz = su.scale;
		else {
			sz = 0;
			numeric = 0;
		}
	} else
		sz = strlen(v);

	/* XXX Rough estimation, might have multiple parts. */
	chsz = (NULL != child && MDOC_TEXT == child->type) ?
			strlen(child->string) : 0;

	/* Maybe we are inside an enclosing list? */
	mid_it();

	/*
	 * Save our own indentation,
	 * such that child lists can use it.
	 */
	Bl_stack[Bl_stack_len++] = sz + 2;

	/* Set up the current list. */
	if (defsz && chsz > sz)
		print_block(".HP", 0);
	else {
		print_block(".TP", 0);
		remain = sz + 2;
	}
	if (numeric) {
		snprintf(buf, sizeof(buf), "%zun", sz + 2);
		print_word(buf);
	} else
		print_word(v);
	TPremain = remain;
}

void
print_count(int *count)
{
	char		  buf[12];

	snprintf(buf, sizeof(buf), "%d.", ++*count);
	print_word(buf);
}

void
man_man(void *arg, const struct man *man)
{

	/*
	 * Dump the keep buffer.
	 * We're guaranteed by now that this exists (is non-NULL).
	 * Flush stdout afterward, just in case.
	 */
	fputs(mparse_getkeep(man_mparse(man)), stdout);
	fflush(stdout);
}

void
man_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_meta *meta;
	const struct mdoc_node *n;

	meta = mdoc_meta(mdoc);
	n = mdoc_node(mdoc);

	printf(".TH \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n",
			meta->title, meta->msec, meta->date,
			meta->os, meta->vol);

	/* Disable hyphenation and if nroff, disable justification. */
	printf(".nh\n.if n .ad l");

	outflags = MMAN_nl | MMAN_Sm;
	if (0 == fontqueue.size) {
		fontqueue.size = 8;
		fontqueue.head = fontqueue.tail = mandoc_malloc(8);
		*fontqueue.tail = 'R';
	}
	print_node(meta, n);
	putchar('\n');
}

static void
print_node(DECL_ARGS)
{
	const struct mdoc_node	*sub;
	const struct manact	*act;
	int			 cond, do_sub;

	/*
	 * Break the line if we were parsed subsequent the current node.
	 * This makes the page structure be more consistent.
	 */
	if (MMAN_spc & outflags && MDOC_LINE & n->flags)
		outflags |= MMAN_nl;

	act = NULL;
	cond = 0;
	do_sub = 1;

	if (MDOC_TEXT == n->type) {
		/*
		 * Make sure that we don't happen to start with a
		 * control character at the start of a line.
		 */
		if (MMAN_nl & outflags && ('.' == *n->string || 
					'\'' == *n->string)) {
			print_word("");
			printf("\\&");
			outflags &= ~MMAN_spc;
		}
		print_word(n->string);
	} else {
		/*
		 * Conditionally run the pre-node action handler for a
		 * node.
		 */
		act = manacts + n->tok;
		cond = NULL == act->cond || (*act->cond)(meta, n);
		if (cond && act->pre)
			do_sub = (*act->pre)(meta, n);
	}

	/* 
	 * Conditionally run all child nodes.
	 * Note that this iterates over children instead of using
	 * recursion.  This prevents unnecessary depth in the stack.
	 */
	if (do_sub)
		for (sub = n->child; sub; sub = sub->next)
			print_node(meta, sub);

	/*
	 * Lastly, conditionally run the post-node handler.
	 */
	if (cond && act->post)
		(*act->post)(meta, n);
}

static int
cond_head(DECL_ARGS)
{

	return(MDOC_HEAD == n->type);
}

static int
cond_body(DECL_ARGS)
{

	return(MDOC_BODY == n->type);
}

static int
pre_enc(DECL_ARGS)
{
	const char	*prefix;

	prefix = manacts[n->tok].prefix;
	if (NULL == prefix)
		return(1);
	print_word(prefix);
	outflags &= ~MMAN_spc;
	return(1);
}

static void
post_enc(DECL_ARGS)
{
	const char *suffix;

	suffix = manacts[n->tok].suffix;
	if (NULL == suffix)
		return;
	outflags &= ~MMAN_spc;
	print_word(suffix);
}

static void
post_font(DECL_ARGS)
{

	font_pop();
}

static void
post_percent(DECL_ARGS)
{

	if (pre_em == manacts[n->tok].pre)
		font_pop();
	if (n->next) {
		print_word(",");
		if (n->prev &&	n->prev->tok == n->tok &&
				n->next->tok == n->tok)
			print_word("and");
	} else {
		print_word(".");
		outflags |= MMAN_nl;
	}
}

static int
pre__t(DECL_ARGS)
{

        if (n->parent && MDOC_Rs == n->parent->tok &&
                        n->parent->norm->Rs.quote_T) {
		print_word("");
		putchar('\"');
		outflags &= ~MMAN_spc;
	} else
		font_push('I');
	return(1);
}

static void
post__t(DECL_ARGS)
{

        if (n->parent && MDOC_Rs == n->parent->tok &&
                        n->parent->norm->Rs.quote_T) {
		outflags &= ~MMAN_spc;
		print_word("");
		putchar('\"');
	} else
		font_pop();
	post_percent(meta, n);
}

/*
 * Print before a section header.
 */
static int
pre_sect(DECL_ARGS)
{

	if (MDOC_HEAD == n->type) {
		outflags |= MMAN_sp;
		print_block(manacts[n->tok].prefix, 0);
		print_word("");
		putchar('\"');
		outflags &= ~MMAN_spc;
	}
	return(1);
}

/*
 * Print subsequent a section header.
 */
static void
post_sect(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return;
	outflags &= ~MMAN_spc;
	print_word("");
	putchar('\"');
	outflags |= MMAN_nl;
	if (MDOC_Sh == n->tok && SEC_AUTHORS == n->sec)
		outflags &= ~(MMAN_An_split | MMAN_An_nosplit);
}

/* See mdoc_term.c, synopsis_pre() for comments. */
static void
pre_syn(const struct mdoc_node *n)
{

	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	if (n->prev->tok == n->tok &&
			MDOC_Ft != n->tok &&
			MDOC_Fo != n->tok &&
			MDOC_Fn != n->tok) {
		outflags |= MMAN_br;
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
		outflags |= MMAN_sp;
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			outflags |= MMAN_sp;
			break;
		}
		/* FALLTHROUGH */
	default:
		outflags |= MMAN_br;
		break;
	}
}

static int
pre_an(DECL_ARGS)
{

	switch (n->norm->An.auth) {
	case (AUTH_split):
		outflags &= ~MMAN_An_nosplit;
		outflags |= MMAN_An_split;
		return(0);
	case (AUTH_nosplit):
		outflags &= ~MMAN_An_split;
		outflags |= MMAN_An_nosplit;
		return(0);
	default:
		if (MMAN_An_split & outflags)
			outflags |= MMAN_br;
		else if (SEC_AUTHORS == n->sec &&
		    ! (MMAN_An_nosplit & outflags))
			outflags |= MMAN_An_split;
		return(1);
	}
}

static int
pre_ap(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
	print_word("'");
	outflags &= ~MMAN_spc;
	return(0);
}

static int
pre_bd(DECL_ARGS)
{

	outflags &= ~(MMAN_PP | MMAN_sp | MMAN_br);

	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type)
		print_line(".nf", 0);
	if (0 == n->norm->Bd.comp && NULL != n->parent->prev)
		outflags |= MMAN_sp;
	print_offs(n->norm->Bd.offs);
	return(1);
}

static void
post_bd(DECL_ARGS)
{

	/* Close out this display. */
	print_line(".RE", MMAN_nl);
	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type)
		print_line(".fi", MMAN_nl);

	/* Maybe we are inside an enclosing list? */
	if (NULL != n->parent->next)
		mid_it();
}

static int
pre_bf(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		return(1);
	case (MDOC_BODY):
		break;
	default:
		return(0);
	}
	switch (n->norm->Bf.font) {
	case (FONT_Em):
		font_push('I');
		break;
	case (FONT_Sy):
		font_push('B');
		break;
	default:
		font_push('R');
		break;
	}
	return(1);
}

static void
post_bf(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		font_pop();
}

static int
pre_bk(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		return(1);
	case (MDOC_BODY):
		outflags |= MMAN_Bk;
		return(1);
	default:
		return(0);
	}
}

static void
post_bk(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		outflags &= ~MMAN_Bk;
}

static int
pre_bl(DECL_ARGS)
{
	size_t		 icol;

	/*
	 * print_offs() will increase the -offset to account for
	 * a possible enclosing .It, but any enclosed .It blocks
	 * just nest and do not add up their indentation.
	 */
	if (n->norm->Bl.offs) {
		print_offs(n->norm->Bl.offs);
		Bl_stack[Bl_stack_len++] = 0;
	}

	switch (n->norm->Bl.type) {
	case (LIST_enum):
		n->norm->Bl.count = 0;
		return(1);
	case (LIST_column):
		break;
	default:
		return(1);
	}

	print_line(".TS", MMAN_nl);
	for (icol = 0; icol < n->norm->Bl.ncols; icol++)
		print_word("l");
	print_word(".");
	outflags |= MMAN_nl;
	return(1);
}

static void
post_bl(DECL_ARGS)
{

	switch (n->norm->Bl.type) {
	case (LIST_column):
		print_line(".TE", 0);
		break;
	case (LIST_enum):
		n->norm->Bl.count = 0;
		break;
	default:
		break;
	}

	if (n->norm->Bl.offs) {
		print_line(".RE", MMAN_nl);
		assert(Bl_stack_len);
		Bl_stack_len--;
		assert(0 == Bl_stack[Bl_stack_len]);
	} else {
		outflags |= MMAN_PP | MMAN_nl;
		outflags &= ~(MMAN_sp | MMAN_br);
	}

	/* Maybe we are inside an enclosing list? */
	if (NULL != n->parent->next)
		mid_it();

}

static int
pre_br(DECL_ARGS)
{

	outflags |= MMAN_br;
	return(0);
}

static int
pre_bx(DECL_ARGS)
{

	n = n->child;
	if (n) {
		print_word(n->string);
		outflags &= ~MMAN_spc;
		n = n->next;
	}
	print_word("BSD");
	if (NULL == n)
		return(0);
	outflags &= ~MMAN_spc;
	print_word("-");
	outflags &= ~MMAN_spc;
	print_word(n->string);
	return(0);
}

static int
pre_dl(DECL_ARGS)
{

	print_offs("6n");
	return(1);
}

static void
post_dl(DECL_ARGS)
{

	print_line(".RE", MMAN_nl);

	/* Maybe we are inside an enclosing list? */
	if (NULL != n->parent->next)
		mid_it();
}

static int
pre_em(DECL_ARGS)
{

	font_push('I');
	return(1);
}

static void
post_eo(DECL_ARGS)
{

	if (MDOC_HEAD == n->type || MDOC_BODY == n->type)
		outflags &= ~MMAN_spc;
}

static int
pre_fa(DECL_ARGS)
{
	int	 am_Fa;

	am_Fa = MDOC_Fa == n->tok;

	if (am_Fa)
		n = n->child;

	while (NULL != n) {
		font_push('I');
		if (am_Fa || MDOC_SYNPRETTY & n->flags)
			outflags |= MMAN_nbrword;
		print_node(meta, n);
		font_pop();
		if (NULL != (n = n->next))
			print_word(",");
	}
	return(0);
}

static void
post_fa(DECL_ARGS)
{

	if (NULL != n->next && MDOC_Fa == n->next->tok)
		print_word(",");
}

static int
pre_fd(DECL_ARGS)
{

	pre_syn(n);
	font_push('B');
	return(1);
}

static void
post_fd(DECL_ARGS)
{

	font_pop();
	outflags |= MMAN_br;
}

static int
pre_fl(DECL_ARGS)
{

	font_push('B');
	print_word("\\-");
	outflags &= ~MMAN_spc;
	return(1);
}

static void
post_fl(DECL_ARGS)
{

	font_pop();
	if (0 == n->nchild && NULL != n->next &&
			n->next->line == n->line)
		outflags &= ~MMAN_spc;
}

static int
pre_fn(DECL_ARGS)
{

	pre_syn(n);

	n = n->child;
	if (NULL == n)
		return(0);

	if (MDOC_SYNPRETTY & n->flags)
		print_block(".HP 4n", MMAN_nl);

	font_push('B');
	print_node(meta, n);
	font_pop();
	outflags &= ~MMAN_spc;
	print_word("(");
	outflags &= ~MMAN_spc;

	n = n->next;
	if (NULL != n)
		pre_fa(meta, n);
	return(0);
}

static void
post_fn(DECL_ARGS)
{

	print_word(")");
	if (MDOC_SYNPRETTY & n->flags) {
		print_word(";");
		outflags |= MMAN_PP;
	}
}

static int
pre_fo(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		pre_syn(n);
		break;
	case (MDOC_HEAD):
		if (MDOC_SYNPRETTY & n->flags)
			print_block(".HP 4n", MMAN_nl);
		font_push('B');
		break;
	case (MDOC_BODY):
		outflags &= ~MMAN_spc;
		print_word("(");
		outflags &= ~MMAN_spc;
		break;
	default:
		break;
	}
	return(1);
}

static void
post_fo(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_HEAD):
		font_pop();
		break;
	case (MDOC_BODY):
		post_fn(meta, n);
		break;
	default:
		break;
	}
}

static int
pre_ft(DECL_ARGS)
{

	pre_syn(n);
	font_push('I');
	return(1);
}

static int
pre_in(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		pre_syn(n);
		font_push('B');
		print_word("#include <");
		outflags &= ~MMAN_spc;
	} else {
		print_word("<");
		outflags &= ~MMAN_spc;
		font_push('I');
	}
	return(1);
}

static void
post_in(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		outflags &= ~MMAN_spc;
		print_word(">");
		font_pop();
		outflags |= MMAN_br;
	} else {
		font_pop();
		outflags &= ~MMAN_spc;
		print_word(">");
	}
}

static int
pre_it(DECL_ARGS)
{
	const struct mdoc_node *bln;

	switch (n->type) {
	case (MDOC_HEAD):
		outflags |= MMAN_PP | MMAN_nl;
		bln = n->parent->parent;
		if (0 == bln->norm->Bl.comp ||
		    (NULL == n->parent->prev &&
		     NULL == bln->parent->prev))
			outflags |= MMAN_sp;
		outflags &= ~MMAN_br;
		switch (bln->norm->Bl.type) {
		case (LIST_item):
			return(0);
		case (LIST_inset):
			/* FALLTHROUGH */
		case (LIST_diag):
			/* FALLTHROUGH */
		case (LIST_ohang):
			if (bln->norm->Bl.type == LIST_diag)
				print_line(".B \"", 0);
			else
				print_line(".R \"", 0);
			outflags &= ~MMAN_spc;
			return(1);
		case (LIST_bullet):
			/* FALLTHROUGH */
		case (LIST_dash):
			/* FALLTHROUGH */
		case (LIST_hyphen):
			print_width(bln->norm->Bl.width, NULL, 0);
			TPremain = 0;
			outflags |= MMAN_nl;
			font_push('B');
			if (LIST_bullet == bln->norm->Bl.type)
				print_word("o");
			else
				print_word("-");
			font_pop();
			break;
		case (LIST_enum):
			print_width(bln->norm->Bl.width, NULL, 0);
			TPremain = 0;
			outflags |= MMAN_nl;
			print_count(&bln->norm->Bl.count);
			break;
		case (LIST_hang):
			print_width(bln->norm->Bl.width, n->child, 6);
			TPremain = 0;
			break;
		case (LIST_tag):
			print_width(bln->norm->Bl.width, n->child, 0);
			putchar('\n');
			outflags &= ~MMAN_spc;
			return(1);
		default:
			return(1);
		}
		outflags |= MMAN_nl;
	default:
		break;
	}
	return(1);
}

/*
 * This function is called after closing out an indented block.
 * If we are inside an enclosing list, restore its indentation.
 */
static void
mid_it(void)
{
	char		 buf[24];

	/* Nothing to do outside a list. */
	if (0 == Bl_stack_len || 0 == Bl_stack[Bl_stack_len - 1])
		return;

	/* The indentation has already been set up. */
	if (Bl_stack_post[Bl_stack_len - 1])
		return;

	/* Restore the indentation of the enclosing list. */
	print_line(".RS", MMAN_Bk_susp);
	snprintf(buf, sizeof(buf), "%zun", Bl_stack[Bl_stack_len - 1]);
	print_word(buf);

	/* Remeber to close out this .RS block later. */
	Bl_stack_post[Bl_stack_len - 1] = 1;
}

static void
post_it(DECL_ARGS)
{
	const struct mdoc_node *bln;

	bln = n->parent->parent;

	switch (n->type) {
	case (MDOC_HEAD):
		switch (bln->norm->Bl.type) {
		case (LIST_diag):
			outflags &= ~MMAN_spc;
			print_word("\\ ");
			break;
		case (LIST_ohang):
			outflags |= MMAN_br;
			break;
		default:
			break;
		}
		break;
	case (MDOC_BODY):
		switch (bln->norm->Bl.type) {
		case (LIST_bullet):
			/* FALLTHROUGH */
		case (LIST_dash):
			/* FALLTHROUGH */
		case (LIST_hyphen):
			/* FALLTHROUGH */
		case (LIST_enum):
			/* FALLTHROUGH */
		case (LIST_hang):
			/* FALLTHROUGH */
		case (LIST_tag):
			assert(Bl_stack_len);
			Bl_stack[--Bl_stack_len] = 0;

			/*
			 * Our indentation had to be restored
			 * after a child display or child list.
			 * Close out that indentation block now.
			 */
			if (Bl_stack_post[Bl_stack_len]) {
				print_line(".RE", MMAN_nl);
				Bl_stack_post[Bl_stack_len] = 0;
			}
			break;
		case (LIST_column):
			if (NULL != n->next) {
				putchar('\t');
				outflags &= ~MMAN_spc;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void
post_lb(DECL_ARGS)
{

	if (SEC_LIBRARY == n->sec)
		outflags |= MMAN_br;
}

static int
pre_lk(DECL_ARGS)
{
	const struct mdoc_node *link, *descr;

	if (NULL == (link = n->child))
		return(0);

	if (NULL != (descr = link->next)) {
		font_push('I');
		while (NULL != descr) {
			print_word(descr->string);
			descr = descr->next;
		}
		print_word(":");
		font_pop();
	}

	font_push('B');
	print_word(link->string);
	font_pop();
	return(0);
}

static int
pre_li(DECL_ARGS)
{

	font_push('R');
	return(1);
}

static int
pre_nm(DECL_ARGS)
{
	char	*name;

	if (MDOC_BLOCK == n->type) {
		outflags |= MMAN_Bk;
		pre_syn(n);
	}
	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return(1);
	name = n->child ? n->child->string : meta->name;
	if (NULL == name)
		return(0);
	if (MDOC_HEAD == n->type) {
		if (NULL == n->parent->prev)
			outflags |= MMAN_sp;
		print_block(".HP", 0);
		printf(" %zun", strlen(name) + 1);
		outflags |= MMAN_nl;
	}
	font_push('B');
	if (NULL == n->child)
		print_word(meta->name);
	return(1);
}

static void
post_nm(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		outflags &= ~MMAN_Bk;
		break;
	case (MDOC_HEAD):
		/* FALLTHROUGH */
	case (MDOC_ELEM):
		font_pop();
		break;
	default:
		break;
	}
}

static int
pre_no(DECL_ARGS)
{

	outflags |= MMAN_spc_force;
	return(1);
}

static int
pre_ns(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
	return(0);
}

static void
post_pf(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
}

static int
pre_pp(DECL_ARGS)
{

	if (MDOC_It != n->parent->tok)
		outflags |= MMAN_PP;
	outflags |= MMAN_sp | MMAN_nl;
	outflags &= ~MMAN_br;
	return(0);
}

static int
pre_rs(DECL_ARGS)
{

	if (SEC_SEE_ALSO == n->sec) {
		outflags |= MMAN_PP | MMAN_sp | MMAN_nl;
		outflags &= ~MMAN_br;
	}
	return(1);
}

static int
pre_sm(DECL_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string))
		outflags |= MMAN_Sm | MMAN_spc;
	else
		outflags &= ~MMAN_Sm;
	return(0);
}

static int
pre_sp(DECL_ARGS)
{

	if (MMAN_PP & outflags) {
		outflags &= ~MMAN_PP;
		print_line(".PP", 0);
	} else
		print_line(".sp", 0);
	return(1);
}

static void
post_sp(DECL_ARGS)
{

	outflags |= MMAN_nl;
}

static int
pre_sy(DECL_ARGS)
{

	font_push('B');
	return(1);
}

static int
pre_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		switch (n->type) {
		case (MDOC_BLOCK):
			pre_syn(n);
			return(1);
		case (MDOC_BODY):
			break;
		default:
			return(0);
		}
	}
	font_push('I');
	return(1);
}

static void
post_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags && MDOC_BODY != n->type)
		return;
	font_pop();
}

static int
pre_xr(DECL_ARGS)
{

	n = n->child;
	if (NULL == n)
		return(0);
	print_node(meta, n);
	n = n->next;
	if (NULL == n)
		return(0);
	outflags &= ~MMAN_spc;
	print_word("(");
	print_node(meta, n);
	print_word(")");
	return(0);
}

static int
pre_ux(DECL_ARGS)
{

	print_word(manacts[n->tok].prefix);
	if (NULL == n->child)
		return(0);
	outflags &= ~MMAN_spc;
	print_word("\\ ");
	outflags &= ~MMAN_spc;
	return(1);
}
