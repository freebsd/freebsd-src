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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	$Id: parser.c,v 1.6.4.4 1996/06/05 02:36:22 jkh Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)parser.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "shell.h"
#include "parser.h"
#include "nodes.h"
#include "expand.h"	/* defines rmescapes() */
#include "redir.h"	/* defines copyfd() */
#include "syntax.h"
#include "options.h"
#include "input.h"
#include "output.h"
#include "var.h"
#include "error.h"
#include "memalloc.h"
#include "mystring.h"
#include "alias.h"
#include "myhistedit.h"


/*
 * Shell command parser.
 */

#define EOFMARKLEN 79

/* values returned by readtoken */
#include "token.def"



struct heredoc {
	struct heredoc *next;	/* next here document in list */
	union node *here;		/* redirection node */
	char *eofmark;		/* string indicating end of input */
	int striptabs;		/* if set, strip leading tabs */
};



struct heredoc *heredoclist;	/* list of here documents to read */
int parsebackquote;		/* nonzero if we are inside backquotes */
int doprompt;			/* if set, prompt the user */
int needprompt;			/* true if interactive and at start of line */
int lasttoken;			/* last token read */
MKINIT int tokpushback;		/* last token pushed back */
char *wordtext;			/* text of last word returned by readtoken */
MKINIT int checkkwd;            /* 1 == check for kwds, 2 == also eat newlines */
struct nodelist *backquotelist;
union node *redirnode;
struct heredoc *heredoc;
int quoteflag;			/* set if (part of) last token was quoted */
int startlinno;			/* line # where last token started */


#define GDB_HACK 1 /* avoid local declarations which gdb can't handle */
#ifdef GDB_HACK
static const char argvars[5] = {CTLVAR, VSNORMAL|VSQUOTE, '@', '=', '\0'};
static const char types[] = "}-+?=";
#endif


STATIC union node *list __P((int));
STATIC union node *andor __P((void));
STATIC union node *pipeline __P((void));
STATIC union node *command __P((void));
STATIC union node *simplecmd __P((union node **, union node *));
STATIC void parsefname __P((void));
STATIC void parseheredoc __P((void));
STATIC int readtoken __P((void));
STATIC int readtoken1 __P((int, char const *, char *, int));
STATIC void attyline __P((void));
STATIC int noexpand __P((char *));
STATIC void synexpect __P((int));
STATIC void synerror __P((char *));
STATIC void setprompt __P((int));

/*
 * Read and parse a command.  Returns NEOF on end of file.  (NULL is a
 * valid parse tree indicating a blank line.)
 */

union node *
parsecmd(interact) {
	int t;

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
	return list(1);
}


STATIC union node *
list(nlflag) {
	union node *n1, *n2, *n3;

	checkkwd = 2;
	if (nlflag == 0 && tokendlist[peektoken()])
		return NULL;
	n1 = andor();
	for (;;) {
		switch (readtoken()) {
		case TBACKGND:
		case TNL:
			parseheredoc();
			if (nlflag)
				return n1;
			/* fall through */
		case TSEMI:
			checkkwd = 2;
			if (tokendlist[peektoken()])
				return n1;
			n2 = andor();
			n3 = (union node *)stalloc(sizeof (struct nbinary));
			n3->type = NSEMI;
			n3->nbinary.ch1 = n1;
			n3->nbinary.ch2 = n2;
			n1 = n3;
			break;
		case TEOF:
			if (heredoclist)
				parseheredoc();
			else
				pungetc();		/* push back EOF on input */
			return n1;
		default:
			if (nlflag)
				synexpect(-1);
			tokpushback++;
			return n1;
		}
	}
}



STATIC union node *
andor() {
	union node *n1, *n2, *n3;
	int t;

	n1 = pipeline();
	for (;;) {
		if ((t = readtoken()) == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			if (t == TBACKGND) {
				if (n1->type == NCMD || n1->type == NPIPE) {
					n1->ncmd.backgnd = 1;
				} else if (n1->type == NREDIR) {
					n1->type = NBACKGND;
				} else {
					n3 = (union node *)stalloc(sizeof (struct nredir));
					n3->type = NBACKGND;
					n3->nredir.n = n1;
					n3->nredir.redirect = NULL;
					n1 = n3;
				}
			}
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



STATIC union node *
pipeline() {
	union node *n1, *pipenode, *notnode;
	struct nodelist *lp, *prev;
	int negate = 0;

	TRACE(("pipeline: entered\n"));
	while (readtoken() == TNOT) {
		TRACE(("pipeline: TNOT recognized\n"));
		negate = !negate;
	}
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
			lp->n = command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback++;
	if (negate) {
		notnode = (union node *)stalloc(sizeof (struct nnot));
		notnode->type = NNOT;
		notnode->nnot.com = n1;
		n1 = notnode;
	}
	return n1;
}



STATIC union node *
command() {
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	int t;

	checkkwd = 2;
	redir = 0;
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
		n1->nif.test = list(0);
		if (readtoken() != TTHEN)
			synexpect(TTHEN);
		n1->nif.ifpart = list(0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = (union node *)stalloc(sizeof (struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			n2->nif.test = list(0);
			if (readtoken() != TTHEN)
				synexpect(TTHEN);
			n2->nif.ifpart = list(0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback++;
		}
		if (readtoken() != TFI)
			synexpect(TFI);
		checkkwd = 1;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = (union node *)stalloc(sizeof (struct nbinary));
		n1->type = (lasttoken == TWHILE)? NWHILE : NUNTIL;
		n1->nbinary.ch1 = list(0);
		if ((got=readtoken()) != TDO) {
TRACE(("expecting DO got %s %s\n", tokname[got], got == TWORD ? wordtext : ""));
			synexpect(TDO);
		}
		n1->nbinary.ch2 = list(0);
		if (readtoken() != TDONE)
			synexpect(TDONE);
		checkkwd = 1;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || ! goodname(wordtext))
			synerror("Bad for loop variable");
		n1 = (union node *)stalloc(sizeof (struct nfor));
		n1->type = NFOR;
		n1->nfor.var = wordtext;
		if (readtoken() == TWORD && ! quoteflag && equal(wordtext, "in")) {
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
#ifndef GDB_HACK
			static const char argvars[5] = {CTLVAR, VSNORMAL|VSQUOTE,
								   '@', '=', '\0'};
#endif
			n2 = (union node *)stalloc(sizeof (struct narg));
			n2->type = NARG;
			n2->narg.text = (char *)argvars;
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
		checkkwd = 2;
		if ((t = readtoken()) == TDO)
			t = TDONE;
		else if (t == TBEGIN)
			t = TEND;
		else
			synexpect(-1);
		n1->nfor.body = list(0);
		if (readtoken() != t)
			synexpect(t);
		checkkwd = 1;
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
		checkkwd = 2, readtoken();
		do {
			*cpp = cp = (union node *)stalloc(sizeof (struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			for (;;) {
				*app = ap = (union node *)stalloc(sizeof (struct narg));
				ap->type = NARG;
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				if (checkkwd = 2, readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			ap->narg.next = NULL;
			if (lasttoken != TRP)
				synexpect(TRP);
			cp->nclist.body = list(0);

			checkkwd = 2;
			if ((t = readtoken()) != TESAC) {
				if (t != TENDCASE)
					synexpect(TENDCASE);
				else
					checkkwd = 2, readtoken();
			}
			cpp = &cp->nclist.next;
		} while(lasttoken != TESAC);
		*cpp = NULL;
		checkkwd = 1;
		break;
	case TLP:
		n1 = (union node *)stalloc(sizeof (struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.n = list(0);
		n1->nredir.redirect = NULL;
		if (readtoken() != TRP)
			synexpect(TRP);
		checkkwd = 1;
		break;
	case TBEGIN:
		n1 = list(0);
		if (readtoken() != TEND)
			synexpect(TEND);
		checkkwd = 1;
		break;
	/* Handle an empty command like other simple commands.  */
	case TAND:
	case TOR:
	case TNL:
	case TSEMI:
	/* Handle EOF as an empty command, too */
	case TEOF:
	case TWORD:
		tokpushback++;
		return simplecmd(rpp, redir);
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


STATIC union node *
simplecmd(rpp, redir)
	union node **rpp, *redir;
	{
	union node *args, **app;
	union node **orig_rpp = rpp;
	union node *n;

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
#ifdef notdef
			if (! goodname(n->narg.text))
				synerror("Bad function name");
#endif
			n->type = NDEFUN;
			n->narg.next = command();
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


STATIC void
parsefname() {
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
		if (is_digit(wordtext[0]))
			n->ndup.dupfd = digit_val(wordtext[0]);
		else if (wordtext[0] == '-')
			n->ndup.dupfd = -1;
		else
			goto bad;
		if (wordtext[1] != '\0') {
bad:
			synerror("Bad fd number");
		}
	} else {
		n->nfile.fname = (union node *)stalloc(sizeof (struct narg));
		n = n->nfile.fname;
		n->type = NARG;
		n->narg.next = NULL;
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
	}
}


/*
 * Input any here documents.
 */

STATIC void
parseheredoc() {
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

STATIC int
peektoken() {
	int t;

	t = readtoken();
	tokpushback++;
	return (t);
}

STATIC int xxreadtoken();

STATIC int
readtoken() {
	int t;
	int savecheckkwd = checkkwd;
	struct alias *ap;
#ifdef DEBUG
	int alreadyseen = tokpushback;
#endif

	top:
	t = xxreadtoken();

	if (checkkwd) {
		/*
		 * eat newlines
		 */
		if (checkkwd == 2) {
			checkkwd = 0;
			while (t == TNL) {
				parseheredoc();
				t = xxreadtoken();
			}
		} else
			checkkwd = 0;
		/*
		 * check for keywords and aliases
		 */
		if (t == TWORD && !quoteflag) {
			register char * const *pp, *s;

			for (pp = (char **)parsekwd; *pp; pp++) {
				if (**pp == *wordtext && equal(*pp, wordtext)) {
					lasttoken = t = pp - parsekwd + KWDOFFSET;
					TRACE(("keyword %s recognized\n", tokname[t]));
					goto out;
				}
			}
			if (ap = lookupalias(wordtext, 1)) {
				pushstring(ap->val, strlen(ap->val), ap);
				checkkwd = savecheckkwd;
				goto top;
			}
		}
out:
		checkkwd = 0;
	}
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

STATIC int
xxreadtoken() {
	register c;

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
		if (c == ' ' || c == '\t')
			continue;		/* quick check for white space first */
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
#define PARSEBACKQOLD()	{oldstyle = 1; goto parsebackq; parsebackq_oldreturn:;}
#define PARSEBACKQNEW()	{oldstyle = 0; goto parsebackq; parsebackq_newreturn:;}
#define	PARSEARITH()	{goto parsearith; parsearith_return:;}

STATIC int
readtoken1(firstc, syntax, eofmark, striptabs)
	int firstc;
	char const *syntax;
	char *eofmark;
	int striptabs;
	{
	register c = firstc;
	register char *out;
	int len;
	char line[EOFMARKLEN + 1];
	struct nodelist *bqlist;
	int quotef;
	int dblquote;
	int varnest;	/* levels of variables expansion */
	int arinest;	/* levels of arithmetic expansion */
	int parenlevel;	/* levels of parens in arithmetic */
	int oldstyle;
	char const *prevsyntax;	/* syntax before arithmetic */

	startlinno = plinno;
	dblquote = 0;
	if (syntax == DQSYNTAX)
		dblquote = 1;
	quotef = 0;
	bqlist = NULL;
	varnest = 0;
	arinest = 0;
	parenlevel = 0;

	STARTSTACKSTR(out);
	loop: {	/* for each line, until end of word */
#if ATTY
		if (c == '\034' && doprompt
		 && attyset() && ! equal(termval(), "emacs")) {
			attyline();
			if (syntax == BASESYNTAX)
				return readtoken();
			c = pgetc();
			goto loop;
		}
#endif
		CHECKEND();	/* set c to PEOF if at end of here document */
		for (;;) {	/* until end of line or end of word */
			CHECKSTRSPACE(3, out);	/* permit 3 calls to USTPUTC */
			if (parsebackquote && c == '\\') {
				c = pgetc();	/* XXX - compat with old /bin/sh */
				if (/*c != '\\' && */c != '`' && c != '$') {
					pungetc();
					c = '\\';
				}
			}
			switch(syntax[c]) {
			case CNL:	/* '\n' */
				if (syntax == BASESYNTAX)
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
				if (eofmark == NULL || dblquote)
					USTPUTC(CTLESC, out);
				USTPUTC(c, out);
				break;
			case CBACK:	/* backslash */
				c = pgetc();
				if (c == PEOF) {
					USTPUTC('\\', out);
					pungetc();
				} else if (c == '\n') {
					if (doprompt)
						setprompt(2);
					else
						setprompt(0);
				} else {
					if (dblquote && c != '\\' && c != '`' && c != '$'
							 && (c != '"' || eofmark != NULL))
						USTPUTC('\\', out);
					if (SQSYNTAX[c] == CCTL)
						USTPUTC(CTLESC, out);
					USTPUTC(c, out);
					quotef++;
				}
				break;
			case CSQUOTE:
				syntax = SQSYNTAX;
				break;
			case CDQUOTE:
				syntax = DQSYNTAX;
				dblquote = 1;
				break;
			case CENDQUOTE:
				if (eofmark) {
					USTPUTC(c, out);
				} else {
					if (arinest)
						syntax = ARISYNTAX;
					else
						syntax = BASESYNTAX;
					quotef++;
					dblquote = 0;
				}
				break;
			case CVAR:	/* '$' */
				PARSESUB();		/* parse substitution */
				break;
			case CENDVAR:	/* '}' */
				if (varnest > 0) {
					varnest--;
					USTPUTC(CTLENDVAR, out);
				} else {
					USTPUTC(c, out);
				}
				break;
			case CLP:	/* '(' in arithmetic */
				parenlevel++;
				USTPUTC(c, out);
				break;
			case CRP:	/* ')' in arithmetic */
				if (parenlevel > 0) {
					USTPUTC(c, out);
					--parenlevel;
				} else {
					if (pgetc() == ')') {
						if (--arinest == 0) {
							USTPUTC(CTLENDARI, out);
							syntax = prevsyntax;
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
				PARSEBACKQOLD();
				break;
			case CEOF:
				goto endword;		/* exit outer loop */
			default:
				if (varnest == 0)
					goto endword;	/* exit outer loop */
				USTPUTC(c, out);
			}
			c = pgetc_macro();
		}
	}
endword:
	if (syntax == ARISYNTAX)
		synerror("Missing '))'");
	if (syntax != BASESYNTAX && ! parsebackquote && eofmark == NULL)
		synerror("Unterminated quoted string");
	if (varnest != 0) {
		startlinno = plinno;
		synerror("Missing '}'");
	}
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
				register char *p, *q;

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
	int subtype;
	int typeloc;
	int flags;
	char *p;
#ifndef GDB_HACK
	static const char types[] = "}-+?=";
#endif

	c = pgetc();
	if (c != '(' && c != '{' && !is_name(c) && !is_special(c)) {
		USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {	/* $(command) or $((arith)) */
		if (pgetc() == '(') {
			PARSEARITH();
		} else {
			pungetc();
			PARSEBACKQNEW();
		}
	} else {
		USTPUTC(CTLVAR, out);
		typeloc = out - stackblock();
		USTPUTC(VSNORMAL, out);
		subtype = VSNORMAL;
		if (c == '{') {
			c = pgetc();
			subtype = 0;
		}
		if (is_name(c)) {
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (is_in_name(c));
		} else {
			if (! is_special(c))
badsub:				synerror("Bad substitution");
			USTPUTC(c, out);
			c = pgetc();
		}
		STPUTC('=', out);
		flags = 0;
		if (subtype == 0) {
			if (c == ':') {
				flags = VSNUL;
				c = pgetc();
			}
			p = strchr(types, c);
			if (p == NULL)
				goto badsub;
			subtype = p - types + VSNORMAL;
		} else {
			pungetc();
		}
		if (dblquote || arinest)
			flags |= VSQUOTE;
		*(stackblock() + typeloc) = subtype | flags;
		if (subtype != VSNORMAL)
			varnest++;
	}
	goto parsesub_return;
}


/*
 * Called to parse command substitutions.  Newstyle is set if the command
 * is enclosed inside $(...); nlpp is a pointer to the head of the linked
 * list of commands (passed by reference), and savelen is the number of
 * characters on the top of the stack which must be preserved.
 */

parsebackq: {
	struct nodelist **nlpp;
	int savepbq;
	union node *n;
	char *volatile str;
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	int savelen;

	savepbq = parsebackquote;
	if (setjmp(jmploc.loc)) {
		if (str)
			ckfree(str);
		parsebackquote = 0;
		handler = savehandler;
		longjmp(handler->loc, 1);
	}
	INTOFF;
	str = NULL;
	savelen = out - stackblock();
	if (savelen > 0) {
		str = ckmalloc(savelen);
		bcopy(stackblock(), str, savelen);
	}
	savehandler = handler;
	handler = &jmploc;
	INTON;
        if (oldstyle) {
                /* We must read until the closing backquote, giving special
                   treatment to some slashes, and then push the string and
                   reread it as input, interpreting it normally.  */
                register char *out;
                register c;
                int savelen;
                char *str;

                STARTSTACKSTR(out);
                while ((c = pgetc ()) != '`') {
                       if (c == PEOF) {
                                startlinno = plinno;
                                synerror("EOF in backquote substitution");
                       }
                       if (c == '\\') {
                                c = pgetc ();
                                if ( c != '\\' && c != '`' && c != '$'
                                    && (!dblquote || c != '"'))
                                        STPUTC('\\', out);
                       }
                       STPUTC(c, out);
                }
                STPUTC('\0', out);
                savelen = out - stackblock();
                if (savelen > 0) {
                        str = ckmalloc(savelen);
                        bcopy(stackblock(), str, savelen);
                }
                setinputstring(str, 1);
        }
	nlpp = &bqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = (struct nodelist *)stalloc(sizeof (struct nodelist));
	(*nlpp)->next = NULL;
	parsebackquote = oldstyle;
	n = list(0);
        if (!oldstyle && (readtoken() != TRP))
                synexpect(TRP);
	(*nlpp)->n = n;
        /* Start reading from old file again.  */
        if (oldstyle)
                popfile();
	while (stackblocksize() <= savelen)
		growstackblock();
	STARTSTACKSTR(out);
	if (str) {
		bcopy(str, out, savelen);
		STADJUST(savelen, out);
		INTOFF;
		ckfree(str);
		str = NULL;
		INTON;
	}
	parsebackquote = savepbq;
	handler = savehandler;
	if (arinest || dblquote)
		USTPUTC(CTLBACKQ | CTLQUOTE, out);
	else
		USTPUTC(CTLBACKQ, out);
	if (oldstyle)
		goto parsebackq_oldreturn;
	else
		goto parsebackq_newreturn;
}

/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {

	if (++arinest == 1) {
		prevsyntax = syntax;
		syntax = ARISYNTAX;
		USTPUTC(CTLARI, out);
	} else {
		/*
		 * we collapse embedded arithmetic expansion to
		 * parenthesis, which should be equivalent
		 */
		USTPUTC('(', out);
	}
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

STATIC int
noexpand(text)
	char *text;
	{
	register char *p;
	register char c;

	p = text;
	while ((c = *p++) != '\0') {
		if (c == CTLESC)
			p++;
		else if (BASESYNTAX[c] == CCTL)
			return 0;
	}
	return 1;
}


/*
 * Return true if the argument is a legal variable name (a letter or
 * underscore followed by zero or more letters, underscores, and digits).
 */

int
goodname(name)
	char *name;
	{
	register char *p;

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

STATIC void
synexpect(token) {
	char msg[64];

	if (token >= 0) {
		fmtstr(msg, 64, "%s unexpected (expecting %s)",
			tokname[lasttoken], tokname[token]);
	} else {
		fmtstr(msg, 64, "%s unexpected", tokname[lasttoken]);
	}
	synerror(msg);
}


STATIC void
synerror(msg)
	char *msg;
	{
	if (commandname)
		outfmt(&errout, "%s: %d: ", commandname, startlinno);
	outfmt(&errout, "Syntax error: %s\n", msg);
	error((char *)NULL);
}

STATIC void
setprompt(which)
	int which;
	{
	whichprompt = which;

	if (!el)
		out2str(getprompt(NULL));
}

/*
 * called by editline -- any expansions to the prompt
 *    should be added here.
 */
char *
getprompt(unused)
	void *unused;
	{
	switch (whichprompt) {
	case 0:
		return "";
	case 1:
		return ps1val();
	case 2:
		return ps2val();
	default:
		return "<internal prompt error>";
	}
}
