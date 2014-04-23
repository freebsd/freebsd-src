/*	$Id: man.c,v 1.121 2013/11/10 22:54:40 schwarze Exp $ */
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "man.h"
#include "mandoc.h"
#include "libman.h"
#include "libmandoc.h"

const	char *const __man_macronames[MAN_MAX] = {		 
	"br",		"TH",		"SH",		"SS",
	"TP", 		"LP",		"PP",		"P",
	"IP",		"HP",		"SM",		"SB",
	"BI",		"IB",		"BR",		"RB",
	"R",		"B",		"I",		"IR",
	"RI",		"na",		"sp",		"nf",
	"fi",		"RE",		"RS",		"DT",
	"UC",		"PD",		"AT",		"in",
	"ft",		"OP",		"EX",		"EE",
	"UR",		"UE"
	};

const	char * const *man_macronames = __man_macronames;

static	struct man_node	*man_node_alloc(struct man *, int, int, 
				enum man_type, enum mant);
static	int		 man_node_append(struct man *, 
				struct man_node *);
static	void		 man_node_free(struct man_node *);
static	void		 man_node_unlink(struct man *, 
				struct man_node *);
static	int		 man_ptext(struct man *, int, char *, int);
static	int		 man_pmacro(struct man *, int, char *, int);
static	void		 man_free1(struct man *);
static	void		 man_alloc1(struct man *);
static	int		 man_descope(struct man *, int, int);


const struct man_node *
man_node(const struct man *man)
{

	assert( ! (MAN_HALT & man->flags));
	return(man->first);
}


const struct man_meta *
man_meta(const struct man *man)
{

	assert( ! (MAN_HALT & man->flags));
	return(&man->meta);
}


void
man_reset(struct man *man)
{

	man_free1(man);
	man_alloc1(man);
}


void
man_free(struct man *man)
{

	man_free1(man);
	free(man);
}


struct man *
man_alloc(struct roff *roff, struct mparse *parse)
{
	struct man	*p;

	p = mandoc_calloc(1, sizeof(struct man));

	man_hash_init();
	p->parse = parse;
	p->roff = roff;

	man_alloc1(p);
	return(p);
}


int
man_endparse(struct man *man)
{

	assert( ! (MAN_HALT & man->flags));
	if (man_macroend(man))
		return(1);
	man->flags |= MAN_HALT;
	return(0);
}


int
man_parseln(struct man *man, int ln, char *buf, int offs)
{

	man->flags |= MAN_NEWLINE;

	assert( ! (MAN_HALT & man->flags));

	return (roff_getcontrol(man->roff, buf, &offs) ?
			man_pmacro(man, ln, buf, offs) : 
			man_ptext(man, ln, buf, offs));
}


static void
man_free1(struct man *man)
{

	if (man->first)
		man_node_delete(man, man->first);
	if (man->meta.title)
		free(man->meta.title);
	if (man->meta.source)
		free(man->meta.source);
	if (man->meta.date)
		free(man->meta.date);
	if (man->meta.vol)
		free(man->meta.vol);
	if (man->meta.msec)
		free(man->meta.msec);
}


static void
man_alloc1(struct man *man)
{

	memset(&man->meta, 0, sizeof(struct man_meta));
	man->flags = 0;
	man->last = mandoc_calloc(1, sizeof(struct man_node));
	man->first = man->last;
	man->last->type = MAN_ROOT;
	man->last->tok = MAN_MAX;
	man->next = MAN_NEXT_CHILD;
}


static int
man_node_append(struct man *man, struct man_node *p)
{

	assert(man->last);
	assert(man->first);
	assert(MAN_ROOT != p->type);

	switch (man->next) {
	case (MAN_NEXT_SIBLING):
		man->last->next = p;
		p->prev = man->last;
		p->parent = man->last->parent;
		break;
	case (MAN_NEXT_CHILD):
		man->last->child = p;
		p->parent = man->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	
	assert(p->parent);
	p->parent->nchild++;

	if ( ! man_valid_pre(man, p))
		return(0);

	switch (p->type) {
	case (MAN_HEAD):
		assert(MAN_BLOCK == p->parent->type);
		p->parent->head = p;
		break;
	case (MAN_TAIL):
		assert(MAN_BLOCK == p->parent->type);
		p->parent->tail = p;
		break;
	case (MAN_BODY):
		assert(MAN_BLOCK == p->parent->type);
		p->parent->body = p;
		break;
	default:
		break;
	}

	man->last = p;

	switch (p->type) {
	case (MAN_TBL):
		/* FALLTHROUGH */
	case (MAN_TEXT):
		if ( ! man_valid_post(man))
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


static struct man_node *
man_node_alloc(struct man *man, int line, int pos, 
		enum man_type type, enum mant tok)
{
	struct man_node *p;

	p = mandoc_calloc(1, sizeof(struct man_node));
	p->line = line;
	p->pos = pos;
	p->type = type;
	p->tok = tok;

	if (MAN_NEWLINE & man->flags)
		p->flags |= MAN_LINE;
	man->flags &= ~MAN_NEWLINE;
	return(p);
}


int
man_elem_alloc(struct man *man, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(man, line, pos, MAN_ELEM, tok);
	if ( ! man_node_append(man, p))
		return(0);
	man->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_tail_alloc(struct man *man, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(man, line, pos, MAN_TAIL, tok);
	if ( ! man_node_append(man, p))
		return(0);
	man->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_head_alloc(struct man *man, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(man, line, pos, MAN_HEAD, tok);
	if ( ! man_node_append(man, p))
		return(0);
	man->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_body_alloc(struct man *man, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(man, line, pos, MAN_BODY, tok);
	if ( ! man_node_append(man, p))
		return(0);
	man->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_block_alloc(struct man *man, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(man, line, pos, MAN_BLOCK, tok);
	if ( ! man_node_append(man, p))
		return(0);
	man->next = MAN_NEXT_CHILD;
	return(1);
}

int
man_word_alloc(struct man *man, int line, int pos, const char *word)
{
	struct man_node	*n;

	n = man_node_alloc(man, line, pos, MAN_TEXT, MAN_MAX);
	n->string = roff_strdup(man->roff, word);

	if ( ! man_node_append(man, n))
		return(0);

	man->next = MAN_NEXT_SIBLING;
	return(1);
}


/*
 * Free all of the resources held by a node.  This does NOT unlink a
 * node from its context; for that, see man_node_unlink().
 */
static void
man_node_free(struct man_node *p)
{

	if (p->string)
		free(p->string);
	free(p);
}


void
man_node_delete(struct man *man, struct man_node *p)
{

	while (p->child)
		man_node_delete(man, p->child);

	man_node_unlink(man, p);
	man_node_free(p);
}

int
man_addeqn(struct man *man, const struct eqn *ep)
{
	struct man_node	*n;

	assert( ! (MAN_HALT & man->flags));

	n = man_node_alloc(man, ep->ln, ep->pos, MAN_EQN, MAN_MAX);
	n->eqn = ep;

	if ( ! man_node_append(man, n))
		return(0);

	man->next = MAN_NEXT_SIBLING;
	return(man_descope(man, ep->ln, ep->pos));
}

int
man_addspan(struct man *man, const struct tbl_span *sp)
{
	struct man_node	*n;

	assert( ! (MAN_HALT & man->flags));

	n = man_node_alloc(man, sp->line, 0, MAN_TBL, MAN_MAX);
	n->span = sp;

	if ( ! man_node_append(man, n))
		return(0);

	man->next = MAN_NEXT_SIBLING;
	return(man_descope(man, sp->line, 0));
}

static int
man_descope(struct man *man, int line, int offs)
{
	/*
	 * Co-ordinate what happens with having a next-line scope open:
	 * first close out the element scope (if applicable), then close
	 * out the block scope (also if applicable).
	 */

	if (MAN_ELINE & man->flags) {
		man->flags &= ~MAN_ELINE;
		if ( ! man_unscope(man, man->last->parent, MANDOCERR_MAX))
			return(0);
	}

	if ( ! (MAN_BLINE & man->flags))
		return(1);
	man->flags &= ~MAN_BLINE;

	if ( ! man_unscope(man, man->last->parent, MANDOCERR_MAX))
		return(0);
	return(man_body_alloc(man, line, offs, man->last->tok));
}

static int
man_ptext(struct man *man, int line, char *buf, int offs)
{
	int		 i;

	/* Literal free-form text whitespace is preserved. */

	if (MAN_LITERAL & man->flags) {
		if ( ! man_word_alloc(man, line, offs, buf + offs))
			return(0);
		return(man_descope(man, line, offs));
	}

	for (i = offs; ' ' == buf[i]; i++)
		/* Skip leading whitespace. */ ;

	/*
	 * Blank lines are ignored right after headings
	 * but add a single vertical space elsewhere.
	 */

	if ('\0' == buf[i]) {
		/* Allocate a blank entry. */
		if (MAN_SH != man->last->tok &&
		    MAN_SS != man->last->tok) {
			if ( ! man_elem_alloc(man, line, offs, MAN_sp))
				return(0);
			man->next = MAN_NEXT_SIBLING;
		}
		return(1);
	}

	/* 
	 * Warn if the last un-escaped character is whitespace. Then
	 * strip away the remaining spaces (tabs stay!).   
	 */

	i = (int)strlen(buf);
	assert(i);

	if (' ' == buf[i - 1] || '\t' == buf[i - 1]) {
		if (i > 1 && '\\' != buf[i - 2])
			man_pmsg(man, line, i - 1, MANDOCERR_EOLNSPACE);

		for (--i; i && ' ' == buf[i]; i--)
			/* Spin back to non-space. */ ;

		/* Jump ahead of escaped whitespace. */
		i += '\\' == buf[i] ? 2 : 1;

		buf[i] = '\0';
	}

	if ( ! man_word_alloc(man, line, offs, buf + offs))
		return(0);

	/*
	 * End-of-sentence check.  If the last character is an unescaped
	 * EOS character, then flag the node as being the end of a
	 * sentence.  The front-end will know how to interpret this.
	 */

	assert(i);
	if (mandoc_eos(buf, (size_t)i, 0))
		man->last->flags |= MAN_EOS;

	return(man_descope(man, line, offs));
}

static int
man_pmacro(struct man *man, int ln, char *buf, int offs)
{
	int		 i, ppos;
	enum mant	 tok;
	char		 mac[5];
	struct man_node	*n;

	if ('"' == buf[offs]) {
		man_pmsg(man, ln, offs, MANDOCERR_BADCOMMENT);
		return(1);
	} else if ('\0' == buf[offs])
		return(1);

	ppos = offs;

	/*
	 * Copy the first word into a nil-terminated buffer.
	 * Stop copying when a tab, space, or eoln is encountered.
	 */

	i = 0;
	while (i < 4 && '\0' != buf[offs] && 
			' ' != buf[offs] && '\t' != buf[offs])
		mac[i++] = buf[offs++];

	mac[i] = '\0';

	tok = (i > 0 && i < 4) ? man_hash_find(mac) : MAN_MAX;

	if (MAN_MAX == tok) {
		mandoc_vmsg(MANDOCERR_MACRO, man->parse, ln, 
				ppos, "%s", buf + ppos - 1);
		return(1);
	}

	/* The macro is sane.  Jump to the next word. */

	while (buf[offs] && ' ' == buf[offs])
		offs++;

	/* 
	 * Trailing whitespace.  Note that tabs are allowed to be passed
	 * into the parser as "text", so we only warn about spaces here.
	 */

	if ('\0' == buf[offs] && ' ' == buf[offs - 1])
		man_pmsg(man, ln, offs - 1, MANDOCERR_EOLNSPACE);

	/* 
	 * Remove prior ELINE macro, as it's being clobbered by a new
	 * macro.  Note that NSCOPED macros do not close out ELINE
	 * macros---they don't print text---so we let those slip by.
	 */

	if ( ! (MAN_NSCOPED & man_macros[tok].flags) &&
			man->flags & MAN_ELINE) {
		n = man->last;
		assert(MAN_TEXT != n->type);

		/* Remove repeated NSCOPED macros causing ELINE. */

		if (MAN_NSCOPED & man_macros[n->tok].flags)
			n = n->parent;

		mandoc_vmsg(MANDOCERR_LINESCOPE, man->parse, n->line, 
		    n->pos, "%s breaks %s", man_macronames[tok],
		    man_macronames[n->tok]);

		man_node_delete(man, n);
		man->flags &= ~MAN_ELINE;
	}

	/*
	 * Remove prior BLINE macro that is being clobbered.
	 */
	if ((man->flags & MAN_BLINE) &&
	    (MAN_BSCOPE & man_macros[tok].flags)) {
		n = man->last;

		/* Might be a text node like 8 in
		 * .TP 8
		 * .SH foo
		 */
		if (MAN_TEXT == n->type)
			n = n->parent;

		/* Remove element that didn't end BLINE, if any. */
		if ( ! (MAN_BSCOPE & man_macros[n->tok].flags))
			n = n->parent;

		assert(MAN_HEAD == n->type);
		n = n->parent;
		assert(MAN_BLOCK == n->type);
		assert(MAN_SCOPED & man_macros[n->tok].flags);

		mandoc_vmsg(MANDOCERR_LINESCOPE, man->parse, n->line, 
		    n->pos, "%s breaks %s", man_macronames[tok],
		    man_macronames[n->tok]);

		man_node_delete(man, n);
		man->flags &= ~MAN_BLINE;
	}

	/*
	 * Save the fact that we're in the next-line for a block.  In
	 * this way, embedded roff instructions can "remember" state
	 * when they exit.
	 */

	if (MAN_BLINE & man->flags)
		man->flags |= MAN_BPLINE;

	/* Call to handler... */

	assert(man_macros[tok].fp);
	if ( ! (*man_macros[tok].fp)(man, tok, ln, ppos, &offs, buf))
		goto err;

	/* 
	 * We weren't in a block-line scope when entering the
	 * above-parsed macro, so return.
	 */

	if ( ! (MAN_BPLINE & man->flags)) {
		man->flags &= ~MAN_ILINE; 
		return(1);
	}
	man->flags &= ~MAN_BPLINE;

	/*
	 * If we're in a block scope, then allow this macro to slip by
	 * without closing scope around it.
	 */

	if (MAN_ILINE & man->flags) {
		man->flags &= ~MAN_ILINE;
		return(1);
	}

	/* 
	 * If we've opened a new next-line element scope, then return
	 * now, as the next line will close out the block scope.
	 */

	if (MAN_ELINE & man->flags)
		return(1);

	/* Close out the block scope opened in the prior line.  */

	assert(MAN_BLINE & man->flags);
	man->flags &= ~MAN_BLINE;

	if ( ! man_unscope(man, man->last->parent, MANDOCERR_MAX))
		return(0);
	return(man_body_alloc(man, ln, ppos, man->last->tok));

err:	/* Error out. */

	man->flags |= MAN_HALT;
	return(0);
}

/*
 * Unlink a node from its context.  If "man" is provided, the last parse
 * point will also be adjusted accordingly.
 */
static void
man_node_unlink(struct man *man, struct man_node *n)
{

	/* Adjust siblings. */

	if (n->prev)
		n->prev->next = n->next;
	if (n->next)
		n->next->prev = n->prev;

	/* Adjust parent. */

	if (n->parent) {
		n->parent->nchild--;
		if (n->parent->child == n)
			n->parent->child = n->prev ? n->prev : n->next;
	}

	/* Adjust parse point, if applicable. */

	if (man && man->last == n) {
		/*XXX: this can occur when bailing from validation. */
		/*assert(NULL == n->next);*/
		if (n->prev) {
			man->last = n->prev;
			man->next = MAN_NEXT_SIBLING;
		} else {
			man->last = n->parent;
			man->next = MAN_NEXT_CHILD;
		}
	}

	if (man && man->first == n)
		man->first = NULL;
}

const struct mparse *
man_mparse(const struct man *man)
{

	assert(man && man->parse);
	return(man->parse);
}
