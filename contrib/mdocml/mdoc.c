/*	$Id: mdoc.c,v 1.238 2015/02/12 13:00:52 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2012-2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mdoc.h"
#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmdoc.h"
#include "libmandoc.h"

const	char *const __mdoc_macronames[MDOC_MAX + 1] = {
	"Ap",		"Dd",		"Dt",		"Os",
	"Sh",		"Ss",		"Pp",		"D1",
	"Dl",		"Bd",		"Ed",		"Bl",
	"El",		"It",		"Ad",		"An",
	"Ar",		"Cd",		"Cm",		"Dv",
	"Er",		"Ev",		"Ex",		"Fa",
	"Fd",		"Fl",		"Fn",		"Ft",
	"Ic",		"In",		"Li",		"Nd",
	"Nm",		"Op",		"Ot",		"Pa",
	"Rv",		"St",		"Va",		"Vt",
	"Xr",		"%A",		"%B",		"%D",
	"%I",		"%J",		"%N",		"%O",
	"%P",		"%R",		"%T",		"%V",
	"Ac",		"Ao",		"Aq",		"At",
	"Bc",		"Bf",		"Bo",		"Bq",
	"Bsx",		"Bx",		"Db",		"Dc",
	"Do",		"Dq",		"Ec",		"Ef",
	"Em",		"Eo",		"Fx",		"Ms",
	"No",		"Ns",		"Nx",		"Ox",
	"Pc",		"Pf",		"Po",		"Pq",
	"Qc",		"Ql",		"Qo",		"Qq",
	"Re",		"Rs",		"Sc",		"So",
	"Sq",		"Sm",		"Sx",		"Sy",
	"Tn",		"Ux",		"Xc",		"Xo",
	"Fo",		"Fc",		"Oo",		"Oc",
	"Bk",		"Ek",		"Bt",		"Hf",
	"Fr",		"Ud",		"Lb",		"Lp",
	"Lk",		"Mt",		"Brq",		"Bro",
	"Brc",		"%C",		"Es",		"En",
	"Dx",		"%Q",		"br",		"sp",
	"%U",		"Ta",		"ll",		"text",
	};

const	char *const __mdoc_argnames[MDOC_ARG_MAX] = {
	"split",		"nosplit",		"ragged",
	"unfilled",		"literal",		"file",
	"offset",		"bullet",		"dash",
	"hyphen",		"item",			"enum",
	"tag",			"diag",			"hang",
	"ohang",		"inset",		"column",
	"width",		"compact",		"std",
	"filled",		"words",		"emphasis",
	"symbolic",		"nested",		"centered"
	};

const	char * const *mdoc_macronames = __mdoc_macronames;
const	char * const *mdoc_argnames = __mdoc_argnames;

static	void		  mdoc_node_free(struct mdoc_node *);
static	void		  mdoc_node_unlink(struct mdoc *,
				struct mdoc_node *);
static	void		  mdoc_free1(struct mdoc *);
static	void		  mdoc_alloc1(struct mdoc *);
static	struct mdoc_node *node_alloc(struct mdoc *, int, int,
				enum mdoct, enum mdoc_type);
static	void		  node_append(struct mdoc *, struct mdoc_node *);
static	int		  mdoc_ptext(struct mdoc *, int, char *, int);
static	int		  mdoc_pmacro(struct mdoc *, int, char *, int);


const struct mdoc_node *
mdoc_node(const struct mdoc *mdoc)
{

	return(mdoc->first);
}

const struct mdoc_meta *
mdoc_meta(const struct mdoc *mdoc)
{

	return(&mdoc->meta);
}

/*
 * Frees volatile resources (parse tree, meta-data, fields).
 */
static void
mdoc_free1(struct mdoc *mdoc)
{

	if (mdoc->first)
		mdoc_node_delete(mdoc, mdoc->first);
	free(mdoc->meta.msec);
	free(mdoc->meta.vol);
	free(mdoc->meta.arch);
	free(mdoc->meta.date);
	free(mdoc->meta.title);
	free(mdoc->meta.os);
	free(mdoc->meta.name);
}

/*
 * Allocate all volatile resources (parse tree, meta-data, fields).
 */
static void
mdoc_alloc1(struct mdoc *mdoc)
{

	memset(&mdoc->meta, 0, sizeof(struct mdoc_meta));
	mdoc->flags = 0;
	mdoc->lastnamed = mdoc->lastsec = SEC_NONE;
	mdoc->last = mandoc_calloc(1, sizeof(struct mdoc_node));
	mdoc->first = mdoc->last;
	mdoc->last->type = MDOC_ROOT;
	mdoc->last->tok = MDOC_MAX;
	mdoc->next = MDOC_NEXT_CHILD;
}

/*
 * Free up volatile resources (see mdoc_free1()) then re-initialises the
 * data with mdoc_alloc1().  After invocation, parse data has been reset
 * and the parser is ready for re-invocation on a new tree; however,
 * cross-parse non-volatile data is kept intact.
 */
void
mdoc_reset(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	mdoc_alloc1(mdoc);
}

/*
 * Completely free up all volatile and non-volatile parse resources.
 * After invocation, the pointer is no longer usable.
 */
void
mdoc_free(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	free(mdoc);
}

/*
 * Allocate volatile and non-volatile parse resources.
 */
struct mdoc *
mdoc_alloc(struct roff *roff, struct mparse *parse,
	const char *defos, int quick)
{
	struct mdoc	*p;

	p = mandoc_calloc(1, sizeof(struct mdoc));

	p->parse = parse;
	p->defos = defos;
	p->quick = quick;
	p->roff = roff;

	mdoc_hash_init();
	mdoc_alloc1(p);
	return(p);
}

void
mdoc_endparse(struct mdoc *mdoc)
{

	mdoc_macroend(mdoc);
}

void
mdoc_addeqn(struct mdoc *mdoc, const struct eqn *ep)
{
	struct mdoc_node *n;

	n = node_alloc(mdoc, ep->ln, ep->pos, MDOC_MAX, MDOC_EQN);
	n->eqn = ep;
	if (ep->ln > mdoc->last->line)
		n->flags |= MDOC_LINE;
	node_append(mdoc, n);
	mdoc->next = MDOC_NEXT_SIBLING;
}

void
mdoc_addspan(struct mdoc *mdoc, const struct tbl_span *sp)
{
	struct mdoc_node *n;

	n = node_alloc(mdoc, sp->line, 0, MDOC_MAX, MDOC_TBL);
	n->span = sp;
	node_append(mdoc, n);
	mdoc->next = MDOC_NEXT_SIBLING;
}

/*
 * Main parse routine.  Parses a single line -- really just hands off to
 * the macro (mdoc_pmacro()) or text parser (mdoc_ptext()).
 */
int
mdoc_parseln(struct mdoc *mdoc, int ln, char *buf, int offs)
{

	if (mdoc->last->type != MDOC_EQN || ln > mdoc->last->line)
		mdoc->flags |= MDOC_NEWLINE;

	/*
	 * Let the roff nS register switch SYNOPSIS mode early,
	 * such that the parser knows at all times
	 * whether this mode is on or off.
	 * Note that this mode is also switched by the Sh macro.
	 */
	if (roff_getreg(mdoc->roff, "nS"))
		mdoc->flags |= MDOC_SYNOPSIS;
	else
		mdoc->flags &= ~MDOC_SYNOPSIS;

	return(roff_getcontrol(mdoc->roff, buf, &offs) ?
	    mdoc_pmacro(mdoc, ln, buf, offs) :
	    mdoc_ptext(mdoc, ln, buf, offs));
}

void
mdoc_macro(MACRO_PROT_ARGS)
{
	assert(tok < MDOC_MAX);

	if (mdoc->flags & MDOC_PBODY) {
		if (tok == MDOC_Dt) {
			mandoc_vmsg(MANDOCERR_DT_LATE,
			    mdoc->parse, line, ppos,
			    "Dt %s", buf + *pos);
			return;
		}
	} else if ( ! (mdoc_macros[tok].flags & MDOC_PROLOGUE)) {
		if (mdoc->meta.title == NULL) {
			mandoc_vmsg(MANDOCERR_DT_NOTITLE,
			    mdoc->parse, line, ppos, "%s %s",
			    mdoc_macronames[tok], buf + *pos);
			mdoc->meta.title = mandoc_strdup("UNTITLED");
		}
		if (NULL == mdoc->meta.vol)
			mdoc->meta.vol = mandoc_strdup("LOCAL");
		mdoc->flags |= MDOC_PBODY;
	}
	(*mdoc_macros[tok].fp)(mdoc, tok, line, ppos, pos, buf);
}


static void
node_append(struct mdoc *mdoc, struct mdoc_node *p)
{

	assert(mdoc->last);
	assert(mdoc->first);
	assert(MDOC_ROOT != p->type);

	switch (mdoc->next) {
	case MDOC_NEXT_SIBLING:
		mdoc->last->next = p;
		p->prev = mdoc->last;
		p->parent = mdoc->last->parent;
		break;
	case MDOC_NEXT_CHILD:
		mdoc->last->child = p;
		p->parent = mdoc->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	p->parent->nchild++;

	/*
	 * Copy over the normalised-data pointer of our parent.  Not
	 * everybody has one, but copying a null pointer is fine.
	 */

	switch (p->type) {
	case MDOC_BODY:
		if (ENDBODY_NOT != p->end)
			break;
		/* FALLTHROUGH */
	case MDOC_TAIL:
		/* FALLTHROUGH */
	case MDOC_HEAD:
		p->norm = p->parent->norm;
		break;
	default:
		break;
	}

	mdoc_valid_pre(mdoc, p);

	switch (p->type) {
	case MDOC_HEAD:
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->head = p;
		break;
	case MDOC_TAIL:
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->tail = p;
		break;
	case MDOC_BODY:
		if (p->end)
			break;
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->body = p;
		break;
	default:
		break;
	}

	mdoc->last = p;

	switch (p->type) {
	case MDOC_TBL:
		/* FALLTHROUGH */
	case MDOC_TEXT:
		mdoc_valid_post(mdoc);
		break;
	default:
		break;
	}
}

static struct mdoc_node *
node_alloc(struct mdoc *mdoc, int line, int pos,
		enum mdoct tok, enum mdoc_type type)
{
	struct mdoc_node *p;

	p = mandoc_calloc(1, sizeof(struct mdoc_node));
	p->sec = mdoc->lastsec;
	p->line = line;
	p->pos = pos;
	p->tok = tok;
	p->type = type;

	/* Flag analysis. */

	if (MDOC_SYNOPSIS & mdoc->flags)
		p->flags |= MDOC_SYNPRETTY;
	else
		p->flags &= ~MDOC_SYNPRETTY;
	if (MDOC_NEWLINE & mdoc->flags)
		p->flags |= MDOC_LINE;
	mdoc->flags &= ~MDOC_NEWLINE;

	return(p);
}

void
mdoc_tail_alloc(struct mdoc *mdoc, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_TAIL);
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_CHILD;
}

struct mdoc_node *
mdoc_head_alloc(struct mdoc *mdoc, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	assert(mdoc->first);
	assert(mdoc->last);
	p = node_alloc(mdoc, line, pos, tok, MDOC_HEAD);
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_CHILD;
	return(p);
}

struct mdoc_node *
mdoc_body_alloc(struct mdoc *mdoc, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_BODY);
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_CHILD;
	return(p);
}

struct mdoc_node *
mdoc_endbody_alloc(struct mdoc *mdoc, int line, int pos, enum mdoct tok,
		struct mdoc_node *body, enum mdoc_endbody end)
{
	struct mdoc_node *p;

	body->flags |= MDOC_ENDED;
	body->parent->flags |= MDOC_ENDED;
	p = node_alloc(mdoc, line, pos, tok, MDOC_BODY);
	p->body = body;
	p->norm = body->norm;
	p->end = end;
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_SIBLING;
	return(p);
}

struct mdoc_node *
mdoc_block_alloc(struct mdoc *mdoc, int line, int pos,
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_BLOCK);
	p->args = args;
	if (p->args)
		(args->refcnt)++;

	switch (tok) {
	case MDOC_Bd:
		/* FALLTHROUGH */
	case MDOC_Bf:
		/* FALLTHROUGH */
	case MDOC_Bl:
		/* FALLTHROUGH */
	case MDOC_En:
		/* FALLTHROUGH */
	case MDOC_Rs:
		p->norm = mandoc_calloc(1, sizeof(union mdoc_data));
		break;
	default:
		break;
	}
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_CHILD;
	return(p);
}

void
mdoc_elem_alloc(struct mdoc *mdoc, int line, int pos,
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_ELEM);
	p->args = args;
	if (p->args)
		(args->refcnt)++;

	switch (tok) {
	case MDOC_An:
		p->norm = mandoc_calloc(1, sizeof(union mdoc_data));
		break;
	default:
		break;
	}
	node_append(mdoc, p);
	mdoc->next = MDOC_NEXT_CHILD;
}

void
mdoc_word_alloc(struct mdoc *mdoc, int line, int pos, const char *p)
{
	struct mdoc_node *n;

	n = node_alloc(mdoc, line, pos, MDOC_MAX, MDOC_TEXT);
	n->string = roff_strdup(mdoc->roff, p);
	node_append(mdoc, n);
	mdoc->next = MDOC_NEXT_SIBLING;
}

void
mdoc_word_append(struct mdoc *mdoc, const char *p)
{
	struct mdoc_node	*n;
	char			*addstr, *newstr;

	n = mdoc->last;
	addstr = roff_strdup(mdoc->roff, p);
	mandoc_asprintf(&newstr, "%s %s", n->string, addstr);
	free(addstr);
	free(n->string);
	n->string = newstr;
	mdoc->next = MDOC_NEXT_SIBLING;
}

static void
mdoc_node_free(struct mdoc_node *p)
{

	if (MDOC_BLOCK == p->type || MDOC_ELEM == p->type)
		free(p->norm);
	if (p->string)
		free(p->string);
	if (p->args)
		mdoc_argv_free(p->args);
	free(p);
}

static void
mdoc_node_unlink(struct mdoc *mdoc, struct mdoc_node *n)
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
		if (n->parent->last == n)
			n->parent->last = n->prev ? n->prev : NULL;
	}

	/* Adjust parse point, if applicable. */

	if (mdoc && mdoc->last == n) {
		if (n->prev) {
			mdoc->last = n->prev;
			mdoc->next = MDOC_NEXT_SIBLING;
		} else {
			mdoc->last = n->parent;
			mdoc->next = MDOC_NEXT_CHILD;
		}
	}

	if (mdoc && mdoc->first == n)
		mdoc->first = NULL;
}

void
mdoc_node_delete(struct mdoc *mdoc, struct mdoc_node *p)
{

	while (p->child) {
		assert(p->nchild);
		mdoc_node_delete(mdoc, p->child);
	}
	assert(0 == p->nchild);

	mdoc_node_unlink(mdoc, p);
	mdoc_node_free(p);
}

void
mdoc_node_relink(struct mdoc *mdoc, struct mdoc_node *p)
{

	mdoc_node_unlink(mdoc, p);
	node_append(mdoc, p);
}

/*
 * Parse free-form text, that is, a line that does not begin with the
 * control character.
 */
static int
mdoc_ptext(struct mdoc *mdoc, int line, char *buf, int offs)
{
	char		 *c, *ws, *end;
	struct mdoc_node *n;

	assert(mdoc->last);
	n = mdoc->last;

	/*
	 * Divert directly to list processing if we're encountering a
	 * columnar MDOC_BLOCK with or without a prior MDOC_BLOCK entry
	 * (a MDOC_BODY means it's already open, in which case we should
	 * process within its context in the normal way).
	 */

	if (n->tok == MDOC_Bl && n->type == MDOC_BODY &&
	    n->end == ENDBODY_NOT && n->norm->Bl.type == LIST_column) {
		/* `Bl' is open without any children. */
		mdoc->flags |= MDOC_FREECOL;
		mdoc_macro(mdoc, MDOC_It, line, offs, &offs, buf);
		return(1);
	}

	if (MDOC_It == n->tok && MDOC_BLOCK == n->type &&
	    NULL != n->parent &&
	    MDOC_Bl == n->parent->tok &&
	    LIST_column == n->parent->norm->Bl.type) {
		/* `Bl' has block-level `It' children. */
		mdoc->flags |= MDOC_FREECOL;
		mdoc_macro(mdoc, MDOC_It, line, offs, &offs, buf);
		return(1);
	}

	/*
	 * Search for the beginning of unescaped trailing whitespace (ws)
	 * and for the first character not to be output (end).
	 */

	/* FIXME: replace with strcspn(). */
	ws = NULL;
	for (c = end = buf + offs; *c; c++) {
		switch (*c) {
		case ' ':
			if (NULL == ws)
				ws = c;
			continue;
		case '\t':
			/*
			 * Always warn about trailing tabs,
			 * even outside literal context,
			 * where they should be put on the next line.
			 */
			if (NULL == ws)
				ws = c;
			/*
			 * Strip trailing tabs in literal context only;
			 * outside, they affect the next line.
			 */
			if (MDOC_LITERAL & mdoc->flags)
				continue;
			break;
		case '\\':
			/* Skip the escaped character, too, if any. */
			if (c[1])
				c++;
			/* FALLTHROUGH */
		default:
			ws = NULL;
			break;
		}
		end = c + 1;
	}
	*end = '\0';

	if (ws)
		mandoc_msg(MANDOCERR_SPACE_EOL, mdoc->parse,
		    line, (int)(ws-buf), NULL);

	if (buf[offs] == '\0' && ! (mdoc->flags & MDOC_LITERAL)) {
		mandoc_msg(MANDOCERR_FI_BLANK, mdoc->parse,
		    line, (int)(c - buf), NULL);

		/*
		 * Insert a `sp' in the case of a blank line.  Technically,
		 * blank lines aren't allowed, but enough manuals assume this
		 * behaviour that we want to work around it.
		 */
		mdoc_elem_alloc(mdoc, line, offs, MDOC_sp, NULL);
		mdoc->next = MDOC_NEXT_SIBLING;
		mdoc_valid_post(mdoc);
		return(1);
	}

	mdoc_word_alloc(mdoc, line, offs, buf+offs);

	if (mdoc->flags & MDOC_LITERAL)
		return(1);

	/*
	 * End-of-sentence check.  If the last character is an unescaped
	 * EOS character, then flag the node as being the end of a
	 * sentence.  The front-end will know how to interpret this.
	 */

	assert(buf < end);

	if (mandoc_eos(buf+offs, (size_t)(end-buf-offs)))
		mdoc->last->flags |= MDOC_EOS;
	return(1);
}

/*
 * Parse a macro line, that is, a line beginning with the control
 * character.
 */
static int
mdoc_pmacro(struct mdoc *mdoc, int ln, char *buf, int offs)
{
	struct mdoc_node *n;
	const char	 *cp;
	enum mdoct	  tok;
	int		  i, sv;
	char		  mac[5];

	sv = offs;

	/*
	 * Copy the first word into a nil-terminated buffer.
	 * Stop when a space, tab, escape, or eoln is encountered.
	 */

	i = 0;
	while (i < 4 && strchr(" \t\\", buf[offs]) == NULL)
		mac[i++] = buf[offs++];

	mac[i] = '\0';

	tok = (i > 1 && i < 4) ? mdoc_hash_find(mac) : MDOC_MAX;

	if (tok == MDOC_MAX) {
		mandoc_msg(MANDOCERR_MACRO, mdoc->parse,
		    ln, sv, buf + sv - 1);
		return(1);
	}

	/* Skip a leading escape sequence or tab. */

	switch (buf[offs]) {
	case '\\':
		cp = buf + offs + 1;
		mandoc_escape(&cp, NULL, NULL);
		offs = cp - buf;
		break;
	case '\t':
		offs++;
		break;
	default:
		break;
	}

	/* Jump to the next non-whitespace word. */

	while (buf[offs] && ' ' == buf[offs])
		offs++;

	/*
	 * Trailing whitespace.  Note that tabs are allowed to be passed
	 * into the parser as "text", so we only warn about spaces here.
	 */

	if ('\0' == buf[offs] && ' ' == buf[offs - 1])
		mandoc_msg(MANDOCERR_SPACE_EOL, mdoc->parse,
		    ln, offs - 1, NULL);

	/*
	 * If an initial macro or a list invocation, divert directly
	 * into macro processing.
	 */

	if (NULL == mdoc->last || MDOC_It == tok || MDOC_El == tok) {
		mdoc_macro(mdoc, tok, ln, sv, &offs, buf);
		return(1);
	}

	n = mdoc->last;
	assert(mdoc->last);

	/*
	 * If the first macro of a `Bl -column', open an `It' block
	 * context around the parsed macro.
	 */

	if (n->tok == MDOC_Bl && n->type == MDOC_BODY &&
	    n->end == ENDBODY_NOT && n->norm->Bl.type == LIST_column) {
		mdoc->flags |= MDOC_FREECOL;
		mdoc_macro(mdoc, MDOC_It, ln, sv, &sv, buf);
		return(1);
	}

	/*
	 * If we're following a block-level `It' within a `Bl -column'
	 * context (perhaps opened in the above block or in ptext()),
	 * then open an `It' block context around the parsed macro.
	 */

	if (MDOC_It == n->tok && MDOC_BLOCK == n->type &&
	    NULL != n->parent &&
	    MDOC_Bl == n->parent->tok &&
	    LIST_column == n->parent->norm->Bl.type) {
		mdoc->flags |= MDOC_FREECOL;
		mdoc_macro(mdoc, MDOC_It, ln, sv, &sv, buf);
		return(1);
	}

	/* Normal processing of a macro. */

	mdoc_macro(mdoc, tok, ln, sv, &offs, buf);

	/* In quick mode (for mandocdb), abort after the NAME section. */

	if (mdoc->quick && MDOC_Sh == tok &&
	    SEC_NAME != mdoc->last->sec)
		return(2);

	return(1);
}

enum mdelim
mdoc_isdelim(const char *p)
{

	if ('\0' == p[0])
		return(DELIM_NONE);

	if ('\0' == p[1])
		switch (p[0]) {
		case '(':
			/* FALLTHROUGH */
		case '[':
			return(DELIM_OPEN);
		case '|':
			return(DELIM_MIDDLE);
		case '.':
			/* FALLTHROUGH */
		case ',':
			/* FALLTHROUGH */
		case ';':
			/* FALLTHROUGH */
		case ':':
			/* FALLTHROUGH */
		case '?':
			/* FALLTHROUGH */
		case '!':
			/* FALLTHROUGH */
		case ')':
			/* FALLTHROUGH */
		case ']':
			return(DELIM_CLOSE);
		default:
			return(DELIM_NONE);
		}

	if ('\\' != p[0])
		return(DELIM_NONE);

	if (0 == strcmp(p + 1, "."))
		return(DELIM_CLOSE);
	if (0 == strcmp(p + 1, "fR|\\fP"))
		return(DELIM_MIDDLE);

	return(DELIM_NONE);
}

void
mdoc_deroff(char **dest, const struct mdoc_node *n)
{
	char	*cp;
	size_t	 sz;

	if (MDOC_TEXT != n->type) {
		for (n = n->child; n; n = n->next)
			mdoc_deroff(dest, n);
		return;
	}

	/* Skip leading whitespace. */

	for (cp = n->string; '\0' != *cp; cp++)
		if (0 == isspace((unsigned char)*cp))
			break;

	/* Skip trailing whitespace. */

	for (sz = strlen(cp); sz; sz--)
		if (0 == isspace((unsigned char)cp[sz-1]))
			break;

	/* Skip empty strings. */

	if (0 == sz)
		return;

	if (NULL == *dest) {
		*dest = mandoc_strndup(cp, sz);
		return;
	}

	mandoc_asprintf(&cp, "%s %*s", *dest, (int)sz, cp);
	free(*dest);
	*dest = cp;
}
