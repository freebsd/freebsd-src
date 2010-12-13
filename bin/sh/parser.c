/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)parser.c	8.7 (Berkeley) 5/16/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "shell.h"
#include "parser.h"
#include "nodes.h"
#include "expand.h"	/* defines rmescapes() */
#include "syntax.h"
#include "options.h"
#include "input.h"
#include "output.h"
#include "var.h"
#include "error.h"
#include "memalloc.h"
#include "mystring.h"
#include "alias.h"
#include "show.h"
#include "eval.h"
#include "exec.h"	/* to check for special builtins */
#ifndef NO_HISTORY
#include "myhistedit.h"
#endif

/*
 * Shell command parser.
 */

#define	EOFMARKLEN	79
#define	PROMPTLEN	128

/* values of checkkwd variable */
#define CHKALIAS	0x1
#define CHKKWD		0x2
#define CHKNL		0x4

/* values returned by readtoken */
#include "token.h"



struct heredoc {
	struct heredoc *next;	/* next here document in list */
	union node *here;		/* redirection node */
	char *eofmark;		/* string indicating end of input */
	int striptabs;		/* if set, strip leading tabs */
};

struct parser_temp {
	struct parser_temp *next;
	void *data;
};


static struct heredoc *heredoclist;	/* list of here documents to read */
static int doprompt;		/* if set, prompt the user */
static int needprompt;		/* true if interactive and at start of line */
static int lasttoken;		/* last token read */
MKINIT int tokpushback;		/* last token pushed back */
static char *wordtext;		/* text of last word returned by readtoken */
MKINIT int checkkwd;            /* 1 == check for kwds, 2 == also eat newlines */
static struct nodelist *backquotelist;
static union node *redirnode;
static struct heredoc *heredoc;
static int quoteflag;		/* set if (part of) last token was quoted */
static int startlinno;		/* line # where last token started */
static int funclinno;		/* line # where the current function started */
static struct parser_temp *parser_temp;


static union node *list(int, int);
static union node *andor(void);
static union node *pipeline(void);
static union node *command(void);
static union node *simplecmd(union node **, union node *);
static union node *makename(void);
static void parsefname(void);
static void parseheredoc(void);
static int peektoken(void);
static int readtoken(void);
static int xxreadtoken(void);
static int readtoken1(int, char const *, char *, int);
static int noexpand(char *);
static void synexpect(int) __dead2;
static void synerror(const char *) __dead2;
static void setprompt(int);


static void *
parser_temp_alloc(size_t len)
{
	struct parser_temp *t;

	INTOFF;
	t = ckmalloc(sizeof(*t));
	t->data = NULL;
	t->next = parser_temp;
	parser_temp = t;
	t->data = ckmalloc(len);
	INTON;
	return t->data;
}


static void *
parser_temp_realloc(void *ptr, size_t len)
{
	struct parser_temp *t;

	INTOFF;
	t = parser_temp;
	if (ptr != t->data)
		error("bug: parser_temp_realloc misused");
	t->data = ckrealloc(t->data, len);
	INTON;
	return t->data;
}


static void
parser_temp_free_upto(void *ptr)
{
	struct parser_temp *t;
	int done = 0;

	INTOFF;
	while (parser_temp != NULL && !done) {
		t = parser_temp;
		parser_temp = t->next;
		done = t->data == ptr;
		ckfree(t->data);
		ckfree(t);
	}
	INTON;
	if (!done)
		error("bug: parser_temp_free_upto misused");
}


static void
parser_temp_free_all(void)
{
	struct parser_temp *t;

	INTOFF;
	while (parser_temp != NULL) {
		t = parser_temp;
		parser_temp = t->next;
		ckfree(t->data);
		ckfree(t);
	}
	INTON;
}


/*
 * Read and parse a command.  Returns NEOF on end of file.  (NULL is a
 * valid parse tree indicating a blank line.)
 */

union node *
parsecmd(int interact)
{
	int t;

	/* This assumes the parser is not re-entered,
	 * which could happen if we add command substitution on PS1/PS2.
	 */
	parser_temp_free_all();
	heredoclist = NULL;

	tokpushback = 0;
	doprompt = interact;
	if (doprompt)
		setprompt(1);
	else
		setprompt(0);
	needprompt = 0;
	t = readtoken();
	if (t == TEOF)
		return NEOF;
	if (t == TNL)
		return NULL;
	tokpushback++;
	return list(1, 1);
}


static union node *
list(int nlflag, int erflag)
{
	union node *ntop, *n1, *n2, *n3;
	int tok;

	checkkwd = CHKNL | CHKKWD | CHKALIAS;
	if (!nlflag && !erflag && tokendlist[peektoken()])
		return NULL;
	ntop = n1 = NULL;
	for (;;) {
		n2 = andor();
		tok = readtoken();
		if (tok == TBACKGND) {
			if (n2->type == NCMD || n2->type == NPIPE) {
				n2->ncmd.backgnd = 1;
			} else if (n2->type == NREDIR) {
				n2->type = NBACKGND;
			} else {
				n3 = (union node *)stalloc(sizeof (struct nredir));
				n3->type = NBACKGND;
				n3->nredir.n = n2;
				n3->nredir.redirect = NULL;
				n2 = n3;
			}
		}
		if (ntop == NULL)
			ntop = n2;
		else if (n1 == NULL) {
			n1 = (union node *)stalloc(sizeof (struct nbinary));
			n1->type = NSEMI;
			n1->nbinary.ch1 = ntop;
			n1->nbinary.ch2 = n2;
			ntop = n1;
		}
		else {
			n3 = (union node *)stalloc(sizeof (struct nbinary));
			n3->type = NSEMI;
			n3->nbinary.ch1 = n1->nbinary.ch2;
			n3->nbinary.ch2 = n2;
			n1->nbinary.ch2 = n3;
			n1 = n3;
		}
		switch (tok) {
		case TBACKGND:
		case TSEMI:
			tok = readtoken();
			/* FALLTHROUGH */
		case TNL:
			if (tok == TNL) {
				parseheredoc();
				if (nlflag)
					return ntop;
			} else if (tok == TEOF && nlflag) {
				parseheredoc();
				return ntop;
			} else {
				tokpushback++;
			}
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			if (!nlflag && !erflag && tokendlist[peektoken()])
				return ntop;
			break;
		case TEOF:
			if (heredoclist)
				parseheredoc();
			else
				pungetc();		/* push back EOF on input */
			return ntop;
		default:
			if (nlflag || erflag)
				synexpect(-1);
			tokpushback++;
			return ntop;
		}
	}
}



static union node *
andor(void)
{
	union node *n1, *n2, *n3;
	int t;

	n1 = pipeline();
	for (;;) {
		if ((t = readtoken()) == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			tokpushback++;
			return n1;
		}
		n2 = pipeline();
		n3 = (union node *)stalloc(sizeof (struct nbinary));
		n3->type = t;
		n3->nbinary.ch1 = n1;
		n3->nbinary.ch2 = n2;
		n1 = n3;
	}
}



static union node *
pipeline(void)
{
	union node *n1, *n2, *pipenode;
	struct nodelist *lp, *prev;
	int negate, t;

	negate = 0;
	checkkwd = CHKNL | CHKKWD | CHKALIAS;
	TRACE(("pipeline: entered\n"));
	while (readtoken() == TNOT)
		negate = !negate;
	tokpushback++;
	n1 = command();
	if (readtoken() == TPIPE) {
		pipenode = (union node *)stalloc(sizeof (struct npipe));
		pipenode->type = NPIPE;
		pipenode->npipe.backgnd = 0;
		lp = (struct nodelist *)stalloc(sizeof (struct nodelist));
		pipenode->npipe.cmdlist = lp;
		lp->n = n1;
		do {
			prev = lp;
			lp = (struct nodelist *)stalloc(sizeof (struct nodelist));
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			t = readtoken();
			tokpushback++;
			if (t == TNOT)
				lp->n = pipeline();
			else
				lp->n = command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback++;
	if (negate) {
		n2 = (union node *)stalloc(sizeof (struct nnot));
		n2->type = NNOT;
		n2->nnot.com = n1;
		return n2;
	} else
		return n1;
}



static union node *
command(void)
{
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	int t;

	checkkwd = CHKNL | CHKKWD | CHKALIAS;
	redir = NULL;
	n1 = NULL;
	rpp = &redir;

	/* Check for redirection which may precede command */
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback++;

	switch (readtoken()) {
	case TIF:
		n1 = (union node *)stalloc(sizeof (struct nif));
		n1->type = NIF;
		if ((n1->nif.test = list(0, 0)) == NULL)
			synexpect(-1);
		if (readtoken() != TTHEN)
			synexpect(TTHEN);
		n1->nif.ifpart = list(0, 0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = (union node *)stalloc(sizeof (struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			if ((n2->nif.test = list(0, 0)) == NULL)
				synexpect(-1);
			if (readtoken() != TTHEN)
				synexpect(TTHEN);
			n2->nif.ifpart = list(0, 0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0, 0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback++;
		}
		if (readtoken() != TFI)
			synexpect(TFI);
		checkkwd = CHKKWD | CHKALIAS;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = (union node *)stalloc(sizeof (struct nbinary));
		n1->type = (lasttoken == TWHILE)? NWHILE : NUNTIL;
		if ((n1->nbinary.ch1 = list(0, 0)) == NULL)
			synexpect(-1);
		if ((got=readtoken()) != TDO) {
TRACE(("expecting DO got %s %s\n", tokname[got], got == TWORD ? wordtext : ""));
			synexpect(TDO);
		}
		n1->nbinary.ch2 = list(0, 0);
		if (readtoken() != TDONE)
			synexpect(TDONE);
		checkkwd = CHKKWD | CHKALIAS;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || ! goodname(wordtext))
			synerror("Bad for loop variable");
		n1 = (union node *)stalloc(sizeof (struct nfor));
		n1->type = NFOR;
		n1->nfor.var = wordtext;
		while (readtoken() == TNL)
			;
		if (lasttoken == TWORD && ! quoteflag && equal(wordtext, "in")) {
			app = &ap;
			while (readtoken() == TWORD) {
				n2 = (union node *)stalloc(sizeof (struct narg));
				n2->type = NARG;
				n2->narg.text = wordtext;
				n2->narg.backquote = backquotelist;
				*app = n2;
				app = &n2->narg.next;
			}
			*app = NULL;
			n1->nfor.args = ap;
			if (lasttoken != TNL && lasttoken != TSEMI)
				synexpect(-1);
		} else {
			static char argvars[5] = {
				CTLVAR, VSNORMAL|VSQUOTE, '@', '=', '\0'
			};
			n2 = (union node *)stalloc(sizeof (struct narg));
			n2->type = NARG;
			n2->narg.text = argvars;
			n2->narg.backquote = NULL;
			n2->narg.next = NULL;
			n1->nfor.args = n2;
			/*
			 * Newline or semicolon here is optional (but note
			 * that the original Bourne shell only allowed NL).
			 */
			if (lasttoken != TNL && lasttoken != TSEMI)
				tokpushback++;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if ((t = readtoken()) == TDO)
			t = TDONE;
		else if (t == TBEGIN)
			t = TEND;
		else
			synexpect(-1);
		n1->nfor.body = list(0, 0);
		if (readtoken() != t)
			synexpect(t);
		checkkwd = CHKKWD | CHKALIAS;
		break;
	case TCASE:
		n1 = (union node *)stalloc(sizeof (struct ncase));
		n1->type = NCASE;
		if (readtoken() != TWORD)
			synexpect(TWORD);
		n1->ncase.expr = n2 = (union node *)stalloc(sizeof (struct narg));
		n2->type = NARG;
		n2->narg.text = wordtext;
		n2->narg.backquote = backquotelist;
		n2->narg.next = NULL;
		while (readtoken() == TNL);
		if (lasttoken != TWORD || ! equal(wordtext, "in"))
			synerror("expecting \"in\"");
		cpp = &n1->ncase.cases;
		checkkwd = CHKNL | CHKKWD, readtoken();
		while (lasttoken != TESAC) {
			*cpp = cp = (union node *)stalloc(sizeof (struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			if (lasttoken == TLP)
				readtoken();
			for (;;) {
				*app = ap = (union node *)stalloc(sizeof (struct narg));
				ap->type = NARG;
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				checkkwd = CHKNL | CHKKWD;
				if (readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			ap->narg.next = NULL;
			if (lasttoken != TRP)
				synexpect(TRP);
			cp->nclist.body = list(0, 0);

			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			if ((t = readtoken()) != TESAC) {
				if (t != TENDCASE)
					synexpect(TENDCASE);
				else
					checkkwd = CHKNL | CHKKWD, readtoken();
			}
			cpp = &cp->nclist.next;
		}
		*cpp = NULL;
		checkkwd = CHKKWD | CHKALIAS;
		break;
	case TLP:
		n1 = (union node *)stalloc(sizeof (struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.n = list(0, 0);
		n1->nredir.redirect = NULL;
		if (readtoken() != TRP)
			synexpect(TRP);
		checkkwd = CHKKWD | CHKALIAS;
		break;
	case TBEGIN:
		n1 = list(0, 0);
		if (readtoken() != TEND)
			synexpect(TEND);
		checkkwd = CHKKWD | CHKALIAS;
		break;
	/* Handle an empty command like other simple commands.  */
	case TBACKGND:
	case TSEMI:
	case TAND:
	case TOR:
		/*
		 * An empty command before a ; doesn't make much sense, and
		 * should certainly be disallowed in the case of `if ;'.
		 */
		if (!redir)
			synexpect(-1);
	case TNL:
	case TEOF:
	case TWORD:
	case TRP:
		tokpushback++;
		n1 = simplecmd(rpp, redir);
		return n1;
	default:
		synexpect(-1);
	}

	/* Now check for redirection which may follow command */
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback++;
	*rpp = NULL;
	if (redir) {
		if (n1->type != NSUBSHELL) {
			n2 = (union node *)stalloc(sizeof (struct nredir));
			n2->type = NREDIR;
			n2->nredir.n = n1;
			n1 = n2;
		}
		n1->nredir.redirect = redir;
	}

	return n1;
}


static union node *
simplecmd(union node **rpp, union node *redir)
{
	union node *args, **app;
	union node **orig_rpp = rpp;
	union node *n = NULL;
	int special;

	/* If we don't have any redirections already, then we must reset */
	/* rpp to be the address of the local redir variable.  */
	if (redir == 0)
		rpp = &redir;

	args = NULL;
	app = &args;
	/*
	 * We save the incoming value, because we need this for shell
	 * functions.  There can not be a redirect or an argument between
	 * the function name and the open parenthesis.
	 */
	orig_rpp = rpp;

	for (;;) {
		if (readtoken() == TWORD) {
			n = (union node *)stalloc(sizeof (struct narg));
			n->type = NARG;
			n->narg.text = wordtext;
			n->narg.backquote = backquotelist;
			*app = n;
			app = &n->narg.next;
		} else if (lasttoken == TREDIR) {
			*rpp = n = redirnode;
			rpp = &n->nfile.next;
			parsefname();	/* read name of redirection file */
		} else if (lasttoken == TLP && app == &args->narg.next
					    && rpp == orig_rpp) {
			/* We have a function */
			if (readtoken() != TRP)
				synexpect(TRP);
			funclinno = plinno;
			/*
			 * - Require plain text.
			 * - Functions with '/' cannot be called.
			 * - Reject name=().
			 * - Reject ksh extended glob patterns.
			 */
			if (!noexpand(n->narg.text) || quoteflag ||
			    strchr(n->narg.text, '/') ||
			    strchr("!%*+-=?@}~",
				n->narg.text[strlen(n->narg.text) - 1]))
				synerror("Bad function name");
			rmescapes(n->narg.text);
			if (find_builtin(n->narg.text, &special) >= 0 &&
			    special)
				synerror("Cannot override a special builtin with a function");
			n->type = NDEFUN;
			n->narg.next = command();
			funclinno = 0;
			return n;
		} else {
			tokpushback++;
			break;
		}
	}
	*app = NULL;
	*rpp = NULL;
	n = (union node *)stalloc(sizeof (struct ncmd));
	n->type = NCMD;
	n->ncmd.backgnd = 0;
	n->ncmd.args = args;
	n->ncmd.redirect = redir;
	return n;
}

static union node *
makename(void)
{
	union node *n;

	n = (union node *)stalloc(sizeof (struct narg));
	n->type = NARG;
	n->narg.next = NULL;
	n->narg.text = wordtext;
	n->narg.backquote = backquotelist;
	return n;
}

void
fixredir(union node *n, const char *text, int err)
{
	TRACE(("Fix redir %s %d\n", text, err));
	if (!err)
		n->ndup.vname = NULL;

	if (is_digit(text[0]) && text[1] == '\0')
		n->ndup.dupfd = digit_val(text[0]);
	else if (text[0] == '-' && text[1] == '\0')
		n->ndup.dupfd = -1;
	else {

		if (err)
			synerror("Bad fd number");
		else
			n->ndup.vname = makename();
	}
}


static void
parsefname(void)
{
	union node *n = redirnode;

	if (readtoken() != TWORD)
		synexpect(-1);
	if (n->type == NHERE) {
		struct heredoc *here = heredoc;
		struct heredoc *p;
		int i;

		if (quoteflag == 0)
			n->type = NXHERE;
		TRACE(("Here document %d\n", n->type));
		if (here->striptabs) {
			while (*wordtext == '\t')
				wordtext++;
		}
		if (! noexpand(wordtext) || (i = strlen(wordtext)) == 0 || i > EOFMARKLEN)
			synerror("Illegal eof marker for << redirection");
		rmescapes(wordtext);
		here->eofmark = wordtext;
		here->next = NULL;
		if (heredoclist == NULL)
			heredoclist = here;
		else {
			for (p = heredoclist ; p->next ; p = p->next);
			p->next = here;
		}
	} else if (n->type == NTOFD || n->type == NFROMFD) {
		fixredir(n, wordtext, 0);
	} else {
		n->nfile.fname = makename();
	}
}


/*
 * Input any here documents.
 */

static void
parseheredoc(void)
{
	struct heredoc *here;
	union node *n;

	while (heredoclist) {
		here = heredoclist;
		heredoclist = here->next;
		if (needprompt) {
			setprompt(2);
			needprompt = 0;
		}
		readtoken1(pgetc(), here->here->type == NHERE? SQSYNTAX : DQSYNTAX,
				here->eofmark, here->striptabs);
		n = (union node *)stalloc(sizeof (struct narg));
		n->narg.type = NARG;
		n->narg.next = NULL;
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
		here->here->nhere.doc = n;
	}
}

static int
peektoken(void)
{
	int t;

	t = readtoken();
	tokpushback++;
	return (t);
}

static int
readtoken(void)
{
	int t;
	struct alias *ap;
#ifdef DEBUG
	int alreadyseen = tokpushback;
#endif

	top:
	t = xxreadtoken();

	/*
	 * eat newlines
	 */
	if (checkkwd & CHKNL) {
		while (t == TNL) {
			parseheredoc();
			t = xxreadtoken();
		}
	}

	/*
	 * check for keywords and aliases
	 */
	if (t == TWORD && !quoteflag)
	{
		const char * const *pp;

		if (checkkwd & CHKKWD)
			for (pp = parsekwd; *pp; pp++) {
				if (**pp == *wordtext && equal(*pp, wordtext))
				{
					lasttoken = t = pp - parsekwd + KWDOFFSET;
					TRACE(("keyword %s recognized\n", tokname[t]));
					goto out;
				}
			}
		if (checkkwd & CHKALIAS &&
		    (ap = lookupalias(wordtext, 1)) != NULL) {
			pushstring(ap->val, strlen(ap->val), ap);
			goto top;
		}
	}
out:
	if (t != TNOT)
		checkkwd = 0;

#ifdef DEBUG
	if (!alreadyseen)
	    TRACE(("token %s %s\n", tokname[t], t == TWORD ? wordtext : ""));
	else
	    TRACE(("reread token %s %s\n", tokname[t], t == TWORD ? wordtext : ""));
#endif
	return (t);
}


/*
 * Read the next input token.
 * If the token is a word, we set backquotelist to the list of cmds in
 *	backquotes.  We set quoteflag to true if any part of the word was
 *	quoted.
 * If the token is TREDIR, then we set redirnode to a structure containing
 *	the redirection.
 * In all cases, the variable startlinno is set to the number of the line
 *	on which the token starts.
 *
 * [Change comment:  here documents and internal procedures]
 * [Readtoken shouldn't have any arguments.  Perhaps we should make the
 *  word parsing code into a separate routine.  In this case, readtoken
 *  doesn't need to have any internal procedures, but parseword does.
 *  We could also make parseoperator in essence the main routine, and
 *  have parseword (readtoken1?) handle both words and redirection.]
 */

#define RETURN(token)	return lasttoken = token

static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	if (needprompt) {
		setprompt(2);
		needprompt = 0;
	}
	startlinno = plinno;
	for (;;) {	/* until token or start of word found */
		c = pgetc_macro();
		switch (c) {
		case ' ': case '\t':
			continue;
		case '#':
			while ((c = pgetc()) != '\n' && c != PEOF);
			pungetc();
			continue;
		case '\\':
			if (pgetc() == '\n') {
				startlinno = ++plinno;
				if (doprompt)
					setprompt(2);
				else
					setprompt(0);
				continue;
			}
			pungetc();
			goto breakloop;
		case '\n':
			plinno++;
			needprompt = doprompt;
			RETURN(TNL);
		case PEOF:
			RETURN(TEOF);
		case '&':
			if (pgetc() == '&')
				RETURN(TAND);
			pungetc();
			RETURN(TBACKGND);
		case '|':
			if (pgetc() == '|')
				RETURN(TOR);
			pungetc();
			RETURN(TPIPE);
		case ';':
			if (pgetc() == ';')
				RETURN(TENDCASE);
			pungetc();
			RETURN(TSEMI);
		case '(':
			RETURN(TLP);
		case ')':
			RETURN(TRP);
		default:
			goto breakloop;
		}
	}
breakloop:
	return readtoken1(c, BASESYNTAX, (char *)NULL, 0);
#undef RETURN
}


#define MAXNEST_static 8
struct tokenstate
{
	const char *syntax; /* *SYNTAX */
	int parenlevel; /* levels of parentheses in arithmetic */
	enum tokenstate_category
	{
		TSTATE_TOP,
		TSTATE_VAR_OLD, /* ${var+-=?}, inherits dquotes */
		TSTATE_VAR_NEW, /* other ${var...}, own dquote state */
		TSTATE_ARITH
	} category;
};


/*
 * Called to parse command substitutions.
 */

static char *
parsebackq(char *out, struct nodelist **pbqlist,
		int oldstyle, int dblquote, int quoted)
{
	struct nodelist **nlpp;
	union node *n;
	char *volatile str;
	struct jmploc jmploc;
	struct jmploc *const savehandler = handler;
	int savelen;
	int saveprompt;
	const int bq_startlinno = plinno;
	char *volatile ostr = NULL;
	struct parsefile *const savetopfile = getcurrentfile();
	struct heredoc *const saveheredoclist = heredoclist;
	struct heredoc *here;

	str = NULL;
	if (setjmp(jmploc.loc)) {
		popfilesupto(savetopfile);
		if (str)
			ckfree(str);
		if (ostr)
			ckfree(ostr);
		heredoclist = saveheredoclist;
		handler = savehandler;
		if (exception == EXERROR) {
			startlinno = bq_startlinno;
			synerror("Error in command substitution");
		}
		longjmp(handler->loc, 1);
	}
	INTOFF;
	savelen = out - stackblock();
	if (savelen > 0) {
		str = ckmalloc(savelen);
		memcpy(str, stackblock(), savelen);
	}
	handler = &jmploc;
	heredoclist = NULL;
	INTON;
        if (oldstyle) {
                /* We must read until the closing backquote, giving special
                   treatment to some slashes, and then push the string and
                   reread it as input, interpreting it normally.  */
                char *oout;
                int c;
                int olen;


                STARTSTACKSTR(oout);
		for (;;) {
			if (needprompt) {
				setprompt(2);
				needprompt = 0;
			}
			CHECKSTRSPACE(2, oout);
			switch (c = pgetc()) {
			case '`':
				goto done;

			case '\\':
                                if ((c = pgetc()) == '\n') {
					plinno++;
					if (doprompt)
						setprompt(2);
					else
						setprompt(0);
					/*
					 * If eating a newline, avoid putting
					 * the newline into the new character
					 * stream (via the USTPUTC after the
					 * switch).
					 */
					continue;
				}
                                if (c != '\\' && c != '`' && c != '$'
                                    && (!dblquote || c != '"'))
                                        USTPUTC('\\', oout);
				break;

			case '\n':
				plinno++;
				needprompt = doprompt;
				break;

			case PEOF:
			        startlinno = plinno;
				synerror("EOF in backquote substitution");
 				break;

			default:
				break;
			}
			USTPUTC(c, oout);
                }
done:
                USTPUTC('\0', oout);
                olen = oout - stackblock();
		INTOFF;
		ostr = ckmalloc(olen);
		memcpy(ostr, stackblock(), olen);
		setinputstring(ostr, 1);
		INTON;
        }
	nlpp = pbqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = (struct nodelist *)stalloc(sizeof (struct nodelist));
	(*nlpp)->next = NULL;

	if (oldstyle) {
		saveprompt = doprompt;
		doprompt = 0;
	}

	n = list(0, oldstyle);

	if (oldstyle)
		doprompt = saveprompt;
	else {
		if (readtoken() != TRP)
			synexpect(TRP);
	}

	(*nlpp)->n = n;
        if (oldstyle) {
		/*
		 * Start reading from old file again, ignoring any pushed back
		 * tokens left from the backquote parsing
		 */
                popfile();
		tokpushback = 0;
	}
	while (stackblocksize() <= savelen)
		growstackblock();
	STARTSTACKSTR(out);
	INTOFF;
	if (str) {
		memcpy(out, str, savelen);
		STADJUST(savelen, out);
		ckfree(str);
		str = NULL;
	}
	if (ostr) {
		ckfree(ostr);
		ostr = NULL;
	}
	here = saveheredoclist;
	if (here != NULL) {
		while (here->next != NULL)
			here = here->next;
		here->next = heredoclist;
		heredoclist = saveheredoclist;
	}
	handler = savehandler;
	INTON;
	if (quoted)
		USTPUTC(CTLBACKQ | CTLQUOTE, out);
	else
		USTPUTC(CTLBACKQ, out);
	return out;
}


/*
 * If eofmark is NULL, read a word or a redirection symbol.  If eofmark
 * is not NULL, read a here document.  In the latter case, eofmark is the
 * word which marks the end of the document and striptabs is true if
 * leading tabs should be stripped from the document.  The argument firstc
 * is the first character of the input token or document.
 *
 * Because C does not have internal subroutines, I have simulated them
 * using goto's to implement the subroutine linkage.  The following macros
 * will run code that appears at the end of readtoken1.
 */

#define CHECKEND()	{goto checkend; checkend_return:;}
#define PARSEREDIR()	{goto parseredir; parseredir_return:;}
#define PARSESUB()	{goto parsesub; parsesub_return:;}
#define	PARSEARITH()	{goto parsearith; parsearith_return:;}

static int
readtoken1(int firstc, char const *initialsyntax, char *eofmark, int striptabs)
{
	int c = firstc;
	char *out;
	int len;
	char line[EOFMARKLEN + 1];
	struct nodelist *bqlist;
	int quotef;
	int newvarnest;
	int level;
	int synentry;
	struct tokenstate state_static[MAXNEST_static];
	int maxnest = MAXNEST_static;
	struct tokenstate *state = state_static;

	startlinno = plinno;
	quotef = 0;
	bqlist = NULL;
	newvarnest = 0;
	level = 0;
	state[level].syntax = initialsyntax;
	state[level].parenlevel = 0;
	state[level].category = TSTATE_TOP;

	STARTSTACKSTR(out);
	loop: {	/* for each line, until end of word */
		CHECKEND();	/* set c to PEOF if at end of here document */
		for (;;) {	/* until end of line or end of word */
			CHECKSTRSPACE(4, out);	/* permit 4 calls to USTPUTC */

			synentry = state[level].syntax[c];

			switch(synentry) {
			case CNL:	/* '\n' */
				if (state[level].syntax == BASESYNTAX)
					goto endword;	/* exit outer loop */
				USTPUTC(c, out);
				plinno++;
				if (doprompt)
					setprompt(2);
				else
					setprompt(0);
				c = pgetc();
				goto loop;		/* continue outer loop */
			case CWORD:
				USTPUTC(c, out);
				break;
			case CCTL:
				if (eofmark == NULL || initialsyntax != SQSYNTAX)
					USTPUTC(CTLESC, out);
				USTPUTC(c, out);
				break;
			case CBACK:	/* backslash */
				c = pgetc();
				if (c == PEOF) {
					USTPUTC('\\', out);
					pungetc();
				} else if (c == '\n') {
					plinno++;
					if (doprompt)
						setprompt(2);
					else
						setprompt(0);
				} else {
					if (state[level].syntax == DQSYNTAX &&
					    c != '\\' && c != '`' && c != '$' &&
					    (c != '"' || (eofmark != NULL &&
						newvarnest == 0)) &&
					    (c != '}' || state[level].category != TSTATE_VAR_OLD))
						USTPUTC('\\', out);
					if ((eofmark == NULL ||
					    newvarnest > 0) &&
					    state[level].syntax == BASESYNTAX)
						USTPUTC(CTLQUOTEMARK, out);
					if (SQSYNTAX[c] == CCTL)
						USTPUTC(CTLESC, out);
					USTPUTC(c, out);
					if ((eofmark == NULL ||
					    newvarnest > 0) &&
					    state[level].syntax == BASESYNTAX &&
					    state[level].category == TSTATE_VAR_OLD)
						USTPUTC(CTLQUOTEEND, out);
					quotef++;
				}
				break;
			case CSQUOTE:
				USTPUTC(CTLQUOTEMARK, out);
				state[level].syntax = SQSYNTAX;
				break;
			case CDQUOTE:
				USTPUTC(CTLQUOTEMARK, out);
				state[level].syntax = DQSYNTAX;
				break;
			case CENDQUOTE:
				if (eofmark != NULL && newvarnest == 0)
					USTPUTC(c, out);
				else {
					if (state[level].category == TSTATE_VAR_OLD)
						USTPUTC(CTLQUOTEEND, out);
					state[level].syntax = BASESYNTAX;
					quotef++;
				}
				break;
			case CVAR:	/* '$' */
				PARSESUB();		/* parse substitution */
				break;
			case CENDVAR:	/* '}' */
				if (level > 0 &&
				    ((state[level].category == TSTATE_VAR_OLD &&
				      state[level].syntax ==
				      state[level - 1].syntax) ||
				    (state[level].category == TSTATE_VAR_NEW &&
				     state[level].syntax == BASESYNTAX))) {
					if (state[level].category == TSTATE_VAR_NEW)
						newvarnest--;
					level--;
					USTPUTC(CTLENDVAR, out);
				} else {
					USTPUTC(c, out);
				}
				break;
			case CLP:	/* '(' in arithmetic */
				state[level].parenlevel++;
				USTPUTC(c, out);
				break;
			case CRP:	/* ')' in arithmetic */
				if (state[level].parenlevel > 0) {
					USTPUTC(c, out);
					--state[level].parenlevel;
				} else {
					if (pgetc() == ')') {
						if (level > 0 &&
						    state[level].category == TSTATE_ARITH) {
							level--;
							USTPUTC(CTLENDARI, out);
						} else
							USTPUTC(')', out);
					} else {
						/*
						 * unbalanced parens
						 *  (don't 2nd guess - no error)
						 */
						pungetc();
						USTPUTC(')', out);
					}
				}
				break;
			case CBQUOTE:	/* '`' */
				out = parsebackq(out, &bqlist, 1,
				    state[level].syntax == DQSYNTAX &&
				    (eofmark == NULL || newvarnest > 0),
				    state[level].syntax == DQSYNTAX || state[level].syntax == ARISYNTAX);
				break;
			case CEOF:
				goto endword;		/* exit outer loop */
			case CIGN:
				break;
			default:
				if (level == 0)
					goto endword;	/* exit outer loop */
				USTPUTC(c, out);
			}
			c = pgetc_macro();
		}
	}
endword:
	if (state[level].syntax == ARISYNTAX)
		synerror("Missing '))'");
	if (state[level].syntax != BASESYNTAX && eofmark == NULL)
		synerror("Unterminated quoted string");
	if (state[level].category == TSTATE_VAR_OLD ||
	    state[level].category == TSTATE_VAR_NEW) {
		startlinno = plinno;
		synerror("Missing '}'");
	}
	if (state != state_static)
		parser_temp_free_upto(state);
	USTPUTC('\0', out);
	len = out - stackblock();
	out = stackblock();
	if (eofmark == NULL) {
		if ((c == '>' || c == '<')
		 && quotef == 0
		 && len <= 2
		 && (*out == '\0' || is_digit(*out))) {
			PARSEREDIR();
			return lasttoken = TREDIR;
		} else {
			pungetc();
		}
	}
	quoteflag = quotef;
	backquotelist = bqlist;
	grabstackblock(len);
	wordtext = out;
	return lasttoken = TWORD;
/* end of readtoken routine */


/*
 * Check to see whether we are at the end of the here document.  When this
 * is called, c is set to the first character of the next input line.  If
 * we are at the end of the here document, this routine sets the c to PEOF.
 */

checkend: {
	if (eofmark) {
		if (striptabs) {
			while (c == '\t')
				c = pgetc();
		}
		if (c == *eofmark) {
			if (pfgets(line, sizeof line) != NULL) {
				char *p, *q;

				p = line;
				for (q = eofmark + 1 ; *q && *p == *q ; p++, q++);
				if (*p == '\n' && *q == '\0') {
					c = PEOF;
					plinno++;
					needprompt = doprompt;
				} else {
					pushstring(line, strlen(line), NULL);
				}
			}
		}
	}
	goto checkend_return;
}


/*
 * Parse a redirection operator.  The variable "out" points to a string
 * specifying the fd to be redirected.  The variable "c" contains the
 * first character of the redirection operator.
 */

parseredir: {
	char fd = *out;
	union node *np;

	np = (union node *)stalloc(sizeof (struct nfile));
	if (c == '>') {
		np->nfile.fd = 1;
		c = pgetc();
		if (c == '>')
			np->type = NAPPEND;
		else if (c == '&')
			np->type = NTOFD;
		else if (c == '|')
			np->type = NCLOBBER;
		else {
			np->type = NTO;
			pungetc();
		}
	} else {	/* c == '<' */
		np->nfile.fd = 0;
		c = pgetc();
		if (c == '<') {
			if (sizeof (struct nfile) != sizeof (struct nhere)) {
				np = (union node *)stalloc(sizeof (struct nhere));
				np->nfile.fd = 0;
			}
			np->type = NHERE;
			heredoc = (struct heredoc *)stalloc(sizeof (struct heredoc));
			heredoc->here = np;
			if ((c = pgetc()) == '-') {
				heredoc->striptabs = 1;
			} else {
				heredoc->striptabs = 0;
				pungetc();
			}
		} else if (c == '&')
			np->type = NFROMFD;
		else if (c == '>')
			np->type = NFROMTO;
		else {
			np->type = NFROM;
			pungetc();
		}
	}
	if (fd != '\0')
		np->nfile.fd = digit_val(fd);
	redirnode = np;
	goto parseredir_return;
}


/*
 * Parse a substitution.  At this point, we have read the dollar sign
 * and nothing else.
 */

parsesub: {
	char buf[10];
	int subtype;
	int typeloc;
	int flags;
	char *p;
	static const char types[] = "}-+?=";
	int bracketed_name = 0; /* used to handle ${[0-9]*} variables */
	int linno;
	int length;

	c = pgetc();
	if (c != '(' && c != '{' && (is_eof(c) || !is_name(c)) &&
	    !is_special(c)) {
		USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {	/* $(command) or $((arith)) */
		if (pgetc() == '(') {
			PARSEARITH();
		} else {
			pungetc();
			out = parsebackq(out, &bqlist, 0,
			    state[level].syntax == DQSYNTAX &&
			    (eofmark == NULL || newvarnest > 0),
			    state[level].syntax == DQSYNTAX ||
			    state[level].syntax == ARISYNTAX);
		}
	} else {
		USTPUTC(CTLVAR, out);
		typeloc = out - stackblock();
		USTPUTC(VSNORMAL, out);
		subtype = VSNORMAL;
		flags = 0;
		if (c == '{') {
			bracketed_name = 1;
			c = pgetc();
			if (c == '#') {
				if ((c = pgetc()) == '}')
					c = '#';
				else
					subtype = VSLENGTH;
			}
			else
				subtype = 0;
		}
		if (!is_eof(c) && is_name(c)) {
			length = 0;
			do {
				STPUTC(c, out);
				c = pgetc();
				length++;
			} while (!is_eof(c) && is_in_name(c));
			if (length == 6 &&
			    strncmp(out - length, "LINENO", length) == 0) {
				/* Replace the variable name with the
				 * current line number. */
				linno = plinno;
				if (funclinno != 0)
					linno -= funclinno - 1;
				snprintf(buf, sizeof(buf), "%d", linno);
				STADJUST(-6, out);
				STPUTS(buf, out);
				flags |= VSLINENO;
			}
		} else if (is_digit(c)) {
			if (bracketed_name) {
				do {
					STPUTC(c, out);
					c = pgetc();
				} while (is_digit(c));
			} else {
				STPUTC(c, out);
				c = pgetc();
			}
		} else {
			if (! is_special(c)) {
				subtype = VSERROR;
				if (c == '}')
					pungetc();
				else if (c == '\n' || c == PEOF)
					synerror("Unexpected end of line in substitution");
				else
					USTPUTC(c, out);
			} else {
				USTPUTC(c, out);
				c = pgetc();
			}
		}
		if (subtype == 0) {
			switch (c) {
			case ':':
				flags |= VSNUL;
				c = pgetc();
				/*FALLTHROUGH*/
			default:
				p = strchr(types, c);
				if (p == NULL) {
					if (c == '\n' || c == PEOF)
						synerror("Unexpected end of line in substitution");
					if (flags == VSNUL)
						STPUTC(':', out);
					STPUTC(c, out);
					subtype = VSERROR;
				} else
					subtype = p - types + VSNORMAL;
				break;
			case '%':
			case '#':
				{
					int cc = c;
					subtype = c == '#' ? VSTRIMLEFT :
							     VSTRIMRIGHT;
					c = pgetc();
					if (c == cc)
						subtype++;
					else
						pungetc();
					break;
				}
			}
		} else if (subtype != VSERROR) {
			pungetc();
		}
		STPUTC('=', out);
		if (subtype != VSLENGTH && (state[level].syntax == DQSYNTAX ||
		    state[level].syntax == ARISYNTAX))
			flags |= VSQUOTE;
		*(stackblock() + typeloc) = subtype | flags;
		if (subtype != VSNORMAL) {
			if (level + 1 >= maxnest) {
				maxnest *= 2;
				if (state == state_static) {
					state = parser_temp_alloc(
					    maxnest * sizeof(*state));
					memcpy(state, state_static,
					    MAXNEST_static * sizeof(*state));
				} else
					state = parser_temp_realloc(state,
					    maxnest * sizeof(*state));
			}
			level++;
			state[level].parenlevel = 0;
			if (subtype == VSMINUS || subtype == VSPLUS ||
			    subtype == VSQUESTION || subtype == VSASSIGN) {
				/*
				 * For operators that were in the Bourne shell,
				 * inherit the double-quote state.
				 */
				state[level].syntax = state[level - 1].syntax;
				state[level].category = TSTATE_VAR_OLD;
			} else {
				/*
				 * The other operators take a pattern,
				 * so go to BASESYNTAX.
				 * Also, ' and " are now special, even
				 * in here documents.
				 */
				state[level].syntax = BASESYNTAX;
				state[level].category = TSTATE_VAR_NEW;
				newvarnest++;
			}
		}
	}
	goto parsesub_return;
}


/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {

	if (level + 1 >= maxnest) {
		maxnest *= 2;
		if (state == state_static) {
			state = parser_temp_alloc(
			    maxnest * sizeof(*state));
			memcpy(state, state_static,
			    MAXNEST_static * sizeof(*state));
		} else
			state = parser_temp_realloc(state,
			    maxnest * sizeof(*state));
	}
	level++;
	state[level].syntax = ARISYNTAX;
	state[level].parenlevel = 0;
	state[level].category = TSTATE_ARITH;
	USTPUTC(CTLARI, out);
	if (state[level - 1].syntax == DQSYNTAX)
		USTPUTC('"',out);
	else
		USTPUTC(' ',out);
	goto parsearith_return;
}

} /* end of readtoken */



#ifdef mkinit
RESET {
	tokpushback = 0;
	checkkwd = 0;
}
#endif

/*
 * Returns true if the text contains nothing to expand (no dollar signs
 * or backquotes).
 */

static int
noexpand(char *text)
{
	char *p;
	char c;

	p = text;
	while ((c = *p++) != '\0') {
		if ( c == CTLQUOTEMARK)
			continue;
		if (c == CTLESC)
			p++;
		else if (BASESYNTAX[(int)c] == CCTL)
			return 0;
	}
	return 1;
}


/*
 * Return true if the argument is a legal variable name (a letter or
 * underscore followed by zero or more letters, underscores, and digits).
 */

int
goodname(const char *name)
{
	const char *p;

	p = name;
	if (! is_name(*p))
		return 0;
	while (*++p) {
		if (! is_in_name(*p))
			return 0;
	}
	return 1;
}


/*
 * Called when an unexpected token is read during the parse.  The argument
 * is the token that is expected, or -1 if more than one type of token can
 * occur at this point.
 */

static void
synexpect(int token)
{
	char msg[64];

	if (token >= 0) {
		fmtstr(msg, 64, "%s unexpected (expecting %s)",
			tokname[lasttoken], tokname[token]);
	} else {
		fmtstr(msg, 64, "%s unexpected", tokname[lasttoken]);
	}
	synerror(msg);
}


static void
synerror(const char *msg)
{
	if (commandname)
		outfmt(out2, "%s: %d: ", commandname, startlinno);
	outfmt(out2, "Syntax error: %s\n", msg);
	error((char *)NULL);
}

static void
setprompt(int which)
{
	whichprompt = which;

#ifndef NO_HISTORY
	if (!el)
#endif
	{
		out2str(getprompt(NULL));
		flushout(out2);
	}
}

/*
 * called by editline -- any expansions to the prompt
 *    should be added here.
 */
char *
getprompt(void *unused __unused)
{
	static char ps[PROMPTLEN];
	char *fmt;
	const char *pwd;
	int i, trim;
	static char internal_error[] = "??";

	/*
	 * Select prompt format.
	 */
	switch (whichprompt) {
	case 0:
		fmt = nullstr;
		break;
	case 1:
		fmt = ps1val();
		break;
	case 2:
		fmt = ps2val();
		break;
	default:
		return internal_error;
	}

	/*
	 * Format prompt string.
	 */
	for (i = 0; (i < 127) && (*fmt != '\0'); i++, fmt++)
		if (*fmt == '\\')
			switch (*++fmt) {

				/*
				 * Hostname.
				 *
				 * \h specifies just the local hostname,
				 * \H specifies fully-qualified hostname.
				 */
			case 'h':
			case 'H':
				ps[i] = '\0';
				gethostname(&ps[i], PROMPTLEN - i);
				/* Skip to end of hostname. */
				trim = (*fmt == 'h') ? '.' : '\0';
				while ((ps[i+1] != '\0') && (ps[i+1] != trim))
					i++;
				break;

				/*
				 * Working directory.
				 *
				 * \W specifies just the final component,
				 * \w specifies the entire path.
				 */
			case 'W':
			case 'w':
				pwd = lookupvar("PWD");
				if (pwd == NULL)
					pwd = "?";
				if (*fmt == 'W' &&
				    *pwd == '/' && pwd[1] != '\0')
					strlcpy(&ps[i], strrchr(pwd, '/') + 1,
					    PROMPTLEN - i);
				else
					strlcpy(&ps[i], pwd, PROMPTLEN - i);
				/* Skip to end of path. */
				while (ps[i + 1] != '\0')
					i++;
				break;

				/*
				 * Superuser status.
				 *
				 * '$' for normal users, '#' for root.
				 */
			case '$':
				ps[i] = (geteuid() != 0) ? '$' : '#';
				break;

				/*
				 * A literal \.
				 */
			case '\\':
				ps[i] = '\\';
				break;

				/*
				 * Emit unrecognized formats verbatim.
				 */
			default:
				ps[i++] = '\\';
				ps[i] = *fmt;
				break;
			}
		else
			ps[i] = *fmt;
	ps[i] = '\0';
	return (ps);
}
