/*	$Id: mdoc_man.c,v 1.9 2011/10/24 21:47:59 schwarze Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "man.h"
#include "mdoc.h"
#include "main.h"

#define	DECL_ARGS const struct mdoc_meta *m, \
		  const struct mdoc_node *n, \
		  struct mman *mm

struct	mman {
	int		  need_space; /* next word needs prior ws */
	int		  need_nl; /* next word needs prior nl */
};

struct	manact {
	int		(*cond)(DECL_ARGS); /* DON'T run actions */
	int		(*pre)(DECL_ARGS); /* pre-node action */
	void		(*post)(DECL_ARGS); /* post-node action */
	const char	 *prefix; /* pre-node string constant */
	const char	 *suffix; /* post-node string constant */
};

static	int	  cond_body(DECL_ARGS);
static	int	  cond_head(DECL_ARGS);
static	void	  post_bd(DECL_ARGS);
static	void	  post_dl(DECL_ARGS);
static	void	  post_enc(DECL_ARGS);
static	void	  post_nm(DECL_ARGS);
static	void	  post_percent(DECL_ARGS);
static	void	  post_pf(DECL_ARGS);
static	void	  post_sect(DECL_ARGS);
static	void	  post_sp(DECL_ARGS);
static	int	  pre_ap(DECL_ARGS);
static	int	  pre_bd(DECL_ARGS);
static	int	  pre_br(DECL_ARGS);
static	int	  pre_bx(DECL_ARGS);
static	int	  pre_dl(DECL_ARGS);
static	int	  pre_enc(DECL_ARGS);
static	int	  pre_it(DECL_ARGS);
static	int	  pre_nm(DECL_ARGS);
static	int	  pre_ns(DECL_ARGS);
static	int	  pre_pp(DECL_ARGS);
static	int	  pre_sp(DECL_ARGS);
static	int	  pre_sect(DECL_ARGS);
static	int	  pre_ux(DECL_ARGS);
static	int	  pre_xr(DECL_ARGS);
static	void	  print_word(struct mman *, const char *);
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
	{ NULL, NULL, NULL, NULL, NULL }, /* Bl */
	{ NULL, NULL, NULL, NULL, NULL }, /* El */
	{ NULL, pre_it, NULL, NULL, NULL }, /* _It */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ad */
	{ NULL, NULL, NULL, NULL, NULL }, /* _An */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ar */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Cd */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Cm */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Dv */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Er */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Ev */
	{ NULL, pre_enc, post_enc, "The \\fB",
	    "\\fP\nutility exits 0 on success, and >0 if an error occurs."
	    }, /* Ex */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fa */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fd */
	{ NULL, pre_enc, post_enc, "\\fB-", "\\fP" }, /* Fl */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fn */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ft */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Ic */
	{ NULL, NULL, NULL, NULL, NULL }, /* _In */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Li */
	{ cond_head, pre_enc, NULL, "\\- ", NULL }, /* Nd */
	{ NULL, pre_nm, post_nm, NULL, NULL }, /* Nm */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Op */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ot */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Pa */
	{ NULL, pre_enc, post_enc, "The \\fB",
		"\\fP\nfunction returns the value 0 if successful;\n"
		"otherwise the value -1 is returned and the global\n"
		"variable \\fIerrno\\fP is set to indicate the error."
		}, /* Rv */
	{ NULL, NULL, NULL, NULL, NULL }, /* St */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Va */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Vt */
	{ NULL, pre_xr, NULL, NULL, NULL }, /* Xr */
	{ NULL, NULL, post_percent, NULL, NULL }, /* _%A */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%B */
	{ NULL, NULL, post_percent, NULL, NULL }, /* _%D */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%I */
	{ NULL, pre_enc, post_percent, "\\fI", "\\fP" }, /* %J */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%N */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%O */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%P */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%R */
	{ NULL, pre_enc, post_percent, "\"", "\"" }, /* %T */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%V */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ac */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Ao */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Aq */
	{ NULL, NULL, NULL, NULL, NULL }, /* At */
	{ NULL, NULL, NULL, NULL, NULL }, /* Bc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bf */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bo */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bq */
	{ NULL, pre_ux, NULL, "BSD/OS", NULL }, /* Bsx */
	{ NULL, pre_bx, NULL, NULL, NULL }, /* Bx */
	{ NULL, NULL, NULL, NULL, NULL }, /* Db */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dc */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Do */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Dq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ec */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ef */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Em */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Eo */
	{ NULL, pre_ux, NULL, "FreeBSD", NULL }, /* Fx */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Ms */
	{ NULL, NULL, NULL, NULL, NULL }, /* No */
	{ NULL, pre_ns, NULL, NULL, NULL }, /* Ns */
	{ NULL, pre_ux, NULL, "NetBSD", NULL }, /* Nx */
	{ NULL, pre_ux, NULL, "OpenBSD", NULL }, /* Ox */
	{ NULL, NULL, NULL, NULL, NULL }, /* Pc */
	{ NULL, NULL, post_pf, NULL, NULL }, /* Pf */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Po */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Pq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Qc */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Ql */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qo */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Re */
	{ cond_body, pre_pp, NULL, NULL, NULL }, /* Rs */
	{ NULL, NULL, NULL, NULL, NULL }, /* Sc */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* So */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Sq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Sm */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Sx */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Sy */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Tn */
	{ NULL, pre_ux, NULL, "UNIX", NULL }, /* Ux */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fc */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Oo */
	{ NULL, NULL, NULL, NULL, NULL }, /* Oc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bk */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ek */
	{ NULL, pre_ux, NULL, "is currently in beta test.", NULL }, /* Bt */
	{ NULL, NULL, NULL, NULL, NULL }, /* Hf */
	{ NULL, NULL, NULL, NULL, NULL }, /* Fr */
	{ NULL, pre_ux, NULL, "currently under development.", NULL }, /* Ud */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lb */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Lp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lk */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Mt */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Brq */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Bro */
	{ NULL, NULL, NULL, NULL, NULL }, /* Brc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%C */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Es */
	{ NULL, NULL, NULL, NULL, NULL }, /* _En */
	{ NULL, pre_ux, NULL, "DragonFly", NULL }, /* Dx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%Q */
	{ NULL, pre_br, NULL, NULL, NULL }, /* br */
	{ NULL, pre_sp, post_sp, NULL, NULL }, /* sp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%U */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ta */
	{ NULL, NULL, NULL, NULL, NULL }, /* ROOT */
};

static void
print_word(struct mman *mm, const char *s)
{

	if (mm->need_nl) {
		/* 
		 * If we need a newline, print it now and start afresh.
		 */
		putchar('\n');
		mm->need_space = 0;
		mm->need_nl = 0;
	} else if (mm->need_space && '\0' != s[0])
		/*
		 * If we need a space, only print it before
		 * (1) a nonzero length word;
		 * (2) a word that is non-punctuation; and
		 * (3) if punctuation, non-terminating puncutation.
		 */
		if (NULL == strchr(".,:;)]?!", s[0]) || '\0' != s[1])
			putchar(' ');

	/*
	 * Reassign needing space if we're not following opening
	 * punctuation.
	 */
	mm->need_space = 
		('(' != s[0] && '[' != s[0]) || '\0' != s[1];

	for ( ; *s; s++) {
		switch (*s) {
		case (ASCII_NBRSP):
			printf("\\~");
			break;
		case (ASCII_HYPH):
			putchar('-');
			break;
		default:
			putchar((unsigned char)*s);
			break;
		}
	}
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
	const struct mdoc_meta *m;
	const struct mdoc_node *n;
	struct mman	        mm;

	m = mdoc_meta(mdoc);
	n = mdoc_node(mdoc);

	printf(".TH \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
			m->title, m->msec, m->date, m->os, m->vol);

	memset(&mm, 0, sizeof(struct mman));

	mm.need_nl = 1;
	print_node(m, n, &mm);
	putchar('\n');
}

static void
print_node(DECL_ARGS)
{
	const struct mdoc_node	*prev, *sub;
	const struct manact	*act;
	int			 cond, do_sub;
	
	/*
	 * Break the line if we were parsed subsequent the current node.
	 * This makes the page structure be more consistent.
	 */
	prev = n->prev ? n->prev : n->parent;
	if (prev && prev->line < n->line)
		mm->need_nl = 1;

	act = NULL;
	cond = 0;
	do_sub = 1;

	if (MDOC_TEXT == n->type) {
		/*
		 * Make sure that we don't happen to start with a
		 * control character at the start of a line.
		 */
		if (mm->need_nl && ('.' == *n->string || 
					'\'' == *n->string)) {
			print_word(mm, "\\&");
			mm->need_space = 0;
		}
		print_word(mm, n->string);
	} else {
		/*
		 * Conditionally run the pre-node action handler for a
		 * node.
		 */
		act = manacts + n->tok;
		cond = NULL == act->cond || (*act->cond)(m, n, mm);
		if (cond && act->pre)
			do_sub = (*act->pre)(m, n, mm);
	}

	/* 
	 * Conditionally run all child nodes.
	 * Note that this iterates over children instead of using
	 * recursion.  This prevents unnecessary depth in the stack.
	 */
	if (do_sub)
		for (sub = n->child; sub; sub = sub->next)
			print_node(m, sub, mm);

	/*
	 * Lastly, conditionally run the post-node handler.
	 */
	if (cond && act->post)
		(*act->post)(m, n, mm);
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

/*
 * Output a font encoding before a node, e.g., \fR.
 * This obviously has no trailing space.
 */
static int
pre_enc(DECL_ARGS)
{
	const char	*prefix;

	prefix = manacts[n->tok].prefix;
	if (NULL == prefix)
		return(1);
	print_word(mm, prefix);
	mm->need_space = 0;
	return(1);
}

/*
 * Output a font encoding subsequent a node, e.g., \fP.
 */
static void
post_enc(DECL_ARGS)
{
	const char *suffix;

	suffix = manacts[n->tok].suffix;
	if (NULL == suffix)
		return;
	mm->need_space = 0;
	print_word(mm, suffix);
}

/*
 * Used in listings (percent = %A, e.g.).
 * FIXME: this is incomplete. 
 * It doesn't print a nice ", and" for lists.
 */
static void
post_percent(DECL_ARGS)
{

	post_enc(m, n, mm);
	if (n->next)
		print_word(mm, ",");
	else {
		print_word(mm, ".");
		mm->need_nl = 1;
	}
}

/*
 * Print before a section header.
 */
static int
pre_sect(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return(1);
	mm->need_nl = 1;
	print_word(mm, manacts[n->tok].prefix);
	print_word(mm, "\"");
	mm->need_space = 0;
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
	mm->need_space = 0;
	print_word(mm, "\"");
	mm->need_nl = 1;
}

static int
pre_ap(DECL_ARGS)
{

	mm->need_space = 0;
	print_word(mm, "'");
	mm->need_space = 0;
	return(0);
}

static int
pre_bd(DECL_ARGS)
{

	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type) {
		mm->need_nl = 1;
		print_word(mm, ".nf");
	}
	mm->need_nl = 1;
	return(1);
}

static void
post_bd(DECL_ARGS)
{

	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type) {
		mm->need_nl = 1;
		print_word(mm, ".fi");
	}
	mm->need_nl = 1;
}

static int
pre_br(DECL_ARGS)
{

	mm->need_nl = 1;
	print_word(mm, ".br");
	mm->need_nl = 1;
	return(0);
}

static int
pre_bx(DECL_ARGS)
{

	n = n->child;
	if (n) {
		print_word(mm, n->string);
		mm->need_space = 0;
		n = n->next;
	}
	print_word(mm, "BSD");
	if (NULL == n)
		return(0);
	mm->need_space = 0;
	print_word(mm, "-");
	mm->need_space = 0;
	print_word(mm, n->string);
	return(0);
}

static int
pre_dl(DECL_ARGS)
{

	mm->need_nl = 1;
	print_word(mm, ".RS 6n");
	mm->need_nl = 1;
	return(1);
}

static void
post_dl(DECL_ARGS)
{

	mm->need_nl = 1;
	print_word(mm, ".RE");
	mm->need_nl = 1;
}

static int
pre_it(DECL_ARGS)
{
	const struct mdoc_node *bln;

	if (MDOC_HEAD == n->type) {
		mm->need_nl = 1;
		print_word(mm, ".TP");
		bln = n->parent->parent->prev;
		switch (bln->norm->Bl.type) {
		case (LIST_bullet):
			print_word(mm, "4n");
			mm->need_nl = 1;
			print_word(mm, "\\fBo\\fP");
			break;
		default:
			if (bln->norm->Bl.width)
				print_word(mm, bln->norm->Bl.width);
			break;
		}
		mm->need_nl = 1;
	}
	return(1);
}

static int
pre_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return(1);
	print_word(mm, "\\fB");
	mm->need_space = 0;
	if (NULL == n->child)
		print_word(mm, m->name);
	return(1);
}

static void
post_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return;
	mm->need_space = 0;
	print_word(mm, "\\fP");
}

static int
pre_ns(DECL_ARGS)
{

	mm->need_space = 0;
	return(0);
}

static void
post_pf(DECL_ARGS)
{

	mm->need_space = 0;
}

static int
pre_pp(DECL_ARGS)
{

	mm->need_nl = 1;
	if (MDOC_It == n->parent->tok)
		print_word(mm, ".sp");
	else
		print_word(mm, ".PP");
	mm->need_nl = 1;
	return(1);
}

static int
pre_sp(DECL_ARGS)
{

	mm->need_nl = 1;
	print_word(mm, ".sp");
	return(1);
}

static void
post_sp(DECL_ARGS)
{

	mm->need_nl = 1;
}

static int
pre_xr(DECL_ARGS)
{

	n = n->child;
	if (NULL == n)
		return(0);
	print_node(m, n, mm);
	n = n->next;
	if (NULL == n)
		return(0);
	mm->need_space = 0;
	print_word(mm, "(");
	print_node(m, n, mm);
	print_word(mm, ")");
	return(0);
}

static int
pre_ux(DECL_ARGS)
{

	print_word(mm, manacts[n->tok].prefix);
	if (NULL == n->child)
		return(0);
	mm->need_space = 0;
	print_word(mm, "\\~");
	mm->need_space = 0;
	return(1);
}
