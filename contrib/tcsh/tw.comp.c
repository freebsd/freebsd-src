/* $Header: /src/pub/tcsh/tw.comp.c,v 1.34 2004/02/21 20:34:25 christos Exp $ */
/*
 * tw.comp.c: File completion builtin
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#include "sh.h"

RCSID("$Id: tw.comp.c,v 1.34 2004/02/21 20:34:25 christos Exp $")

#include "tw.h"
#include "ed.h"
#include "tc.h"

/* #define TDEBUG */
struct varent completions;

static int 	 	  tw_result	__P((Char *, Char **));
static Char		**tw_find	__P((Char *, struct varent *, int));
static Char 		 *tw_tok	__P((Char *));
static bool	 	  tw_pos	__P((Char *, int));
static void	  	  tw_pr		__P((Char **));
static int	  	  tw_match	__P((Char *, Char *));
static void	 	  tw_prlist	__P((struct varent *));
static Char  		 *tw_dollar	__P((Char *,Char **, int, Char *, 
					     int, const char *));

/* docomplete():
 *	Add or list completions in the completion list
 */
/*ARGSUSED*/
void
docomplete(v, t)
    Char **v;
    struct command *t;
{
    register struct varent *vp;
    register Char *p;
    Char **pp;

    USE(t);
    v++;
    p = *v++;
    if (p == 0)
	tw_prlist(&completions);
    else if (*v == 0) {
	vp = adrof1(strip(p), &completions);
	if (vp && vp->vec)
	    tw_pr(vp->vec), xputchar('\n');
	else
	{
#ifdef TDEBUG
	    xprintf("tw_find(%s) \n", short2str(strip(p)));
#endif /* TDEBUG */
	    pp = tw_find(strip(p), &completions, FALSE);
	    if (pp)
		tw_pr(pp), xputchar('\n');
	}
    }
    else
	set1(strip(p), saveblk(v), &completions, VAR_READWRITE);
} /* end docomplete */


/* douncomplete():
 *	Remove completions from the completion list
 */
/*ARGSUSED*/
void
douncomplete(v, t)
    Char **v;
    struct command *t;
{
    USE(t);
    unset1(v, &completions);
} /* end douncomplete */


/* tw_prlist():
 *	Pretty print a list of variables
 */
static void
tw_prlist(p)
    struct varent *p;
{
    register struct varent *c;

    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else				/* BSDSIGS */
	(void) sigrelse(SIGINT);
#endif				/* BSDSIGS */

    for (;;) {
	while (p->v_left)
	    p = p->v_left;
x:
	if (p->v_parent == 0)	/* is it the header? */
	    return;
	xprintf("%s\t", short2str(p->v_name));
	if (p->vec)
	    tw_pr(p->vec);
	xputchar('\n');
	if (p->v_right) {
	    p = p->v_right;
	    continue;
	}
	do {
	    c = p;
	    p = p->v_parent;
	} while (p->v_right == c);
	goto x;
    }
} /* end tw_prlist */


/* tw_pr():
 *	Pretty print a completion, adding single quotes around 
 *	a completion argument and collapsing multiple spaces to one.
 */
static void
tw_pr(cmp)
    Char **cmp;
{
    bool sp, osp;
    Char *ptr;

    for (; *cmp; cmp++) {
	xputchar('\'');
	for (osp = 0, ptr = *cmp; *ptr; ptr++) {
	    sp = Isspace(*ptr);
	    if (sp && osp)
		continue;
	    xputchar(*ptr);
	    osp = sp;
	}
	xputchar('\'');
	if (cmp[1])
	    xputchar(' ');
    }
} /* end tw_pr */


/* tw_find():
 *	Find the first matching completion. 
 *	For commands we only look at names that start with -
 */
static Char **
tw_find(nam, vp, cmd)
    Char   *nam;
    register struct varent *vp;
    int cmd;
{
    register Char **rv;

    for (vp = vp->v_left; vp; vp = vp->v_right) {
	if (vp->v_left && (rv = tw_find(nam, vp, cmd)) != NULL)
	    return rv;
	if (cmd) {
	    if (vp->v_name[0] != '-')
		continue;
	    if (Gmatch(nam, &vp->v_name[1]) && vp->vec != NULL)
		return vp->vec;
	}
	else
	    if (Gmatch(nam, vp->v_name) && vp->vec != NULL)
		return vp->vec;
    }
    return NULL;
} /* end tw_find */


/* tw_pos():
 *	Return true if the position is within the specified range
 */
static bool
tw_pos(ran, wno)
    Char *ran;
    int	  wno;
{
    Char *p;

    if (ran[0] == '*' && ran[1] == '\0')
	return 1;

    for (p = ran; *p && *p != '-'; p++)
	continue;

    if (*p == '\0')			/* range == <number> */
	return wno == getn(ran);
    
    if (ran == p)			/* range = - <number> */
	return wno <= getn(&ran[1]);
    *p++ = '\0';

    if (*p == '\0')			/* range = <number> - */
	return getn(ran) <= wno;
    else				/* range = <number> - <number> */
	return (getn(ran) <= wno) && (wno <= getn(p));
	   
} /* end tw_pos */


/* tw_tok():
 *	Return the next word from string, unquoteing it.
 */
static Char *
tw_tok(str)
    Char *str;
{
    static Char *bf = NULL;

    if (str != NULL)
	bf = str;
    
    /* skip leading spaces */
    for (; *bf && Isspace(*bf); bf++)
	continue;

    for (str = bf; *bf && !Isspace(*bf); bf++) {
	if (ismeta(*bf))
	    return INVPTR;
	*bf = *bf & ~QUOTE;
    }
    if (*bf != '\0')
	*bf++ = '\0';

    return *str ? str : NULL;
} /* end tw_tok */


/* tw_match():
 *	Match a string against the pattern given.
 *	and return the number of matched characters
 *	in a prefix of the string.
 */
static int
tw_match(str, pat)
    Char *str, *pat;
{
    Char *estr;
    int rv = Gnmatch(str, pat, &estr);
#ifdef TDEBUG
    xprintf("Gnmatch(%s, ", short2str(str));
    xprintf("%s, ", short2str(pat));
    xprintf("%s) = %d [%d]\n", short2str(estr), rv, estr - str);
#endif /* TDEBUG */
    return (int) (rv ? estr - str : -1);
}


/* tw_result():
 *	Return what the completion action should be depending on the
 *	string
 */
static int
tw_result(act, pat)
    Char *act, **pat;
{
    int looking;
    static Char* res = NULL;

    if (res != NULL)
	xfree((ptr_t) res), res = NULL;

    switch (act[0] & ~QUOTE) {
    case 'X':
	looking = TW_COMPLETION;
	break;
    case 'S':
	looking = TW_SIGNAL;
	break;
    case 'a':
	looking = TW_ALIAS;
	break;
    case 'b':
	looking = TW_BINDING;
	break;
    case 'c':
	looking = TW_COMMAND;
	break;
    case 'C':
	looking = TW_PATH | TW_COMMAND;
	break;
    case 'd':
	looking = TW_DIRECTORY;
	break;
    case 'D':
	looking = TW_PATH | TW_DIRECTORY;
	break;
    case 'e':
	looking = TW_ENVVAR;
	break;
    case 'f':
	looking = TW_FILE;
	break;
#ifdef COMPAT
    case 'p':
#endif /* COMPAT */
    case 'F':
	looking = TW_PATH | TW_FILE;
	break;
    case 'g':
	looking = TW_GRPNAME;
	break;
    case 'j':
	looking = TW_JOB;
	break;
    case 'l':
	looking = TW_LIMIT;
	break;
    case 'n':
	looking = TW_NONE;
	break;
    case 's':
	looking = TW_SHELLVAR;
	break;
    case 't':
	looking = TW_TEXT;
	break;
    case 'T':
	looking = TW_PATH | TW_TEXT;
	break;
    case 'v':
	looking = TW_VARIABLE;
	break;
    case 'u':
	looking = TW_USER;
	break;
    case 'x':
	looking = TW_EXPLAIN;
	break;

    case '$':
	*pat = res = Strsave(&act[1]);
	(void) strip(res);
	return(TW_VARLIST);

    case '(':
	*pat = res = Strsave(&act[1]);
	if ((act = Strchr(res, ')')) != NULL)
	    *act = '\0';
	(void) strip(res);
	return TW_WORDLIST;

    case '`':
	res = Strsave(act);
	if ((act = Strchr(&res[1], '`')) != NULL)
	    *++act = '\0';
	
	if (didfds == 0) {
	    /*
	     * Make sure that we have some file descriptors to
	     * play with, so that the processes have at least 0, 1, 2
	     * open
	     */
	    (void) dcopy(SHIN, 0);
	    (void) dcopy(SHOUT, 1);
	    (void) dcopy(SHDIAG, 2);
	}
	if ((act = globone(res, G_APPEND)) != NULL) {
	    xfree((ptr_t) res), res = NULL;
	    *pat = res = Strsave(act);
	    xfree((ptr_t) act);
	    return TW_WORDLIST;
	}
	return TW_ZERO;

    default:
	stderror(ERR_COMPCOM, short2str(act));
	return TW_ZERO;
    }

    switch (act[1] & ~QUOTE) {
    case '\0':
	return looking;

    case ':':
	*pat = res = Strsave(&act[2]);
	(void) strip(res);
	return looking;

    default:
	stderror(ERR_COMPCOM, short2str(act));
	return TW_ZERO;
    }
} /* end tw_result */
		

/* tw_dollar():
 *	Expand $<n> args in buffer
 */
static Char *
tw_dollar(str, wl, nwl, buffer, sep, msg)
    Char *str, **wl;
    int nwl;
    Char *buffer;
    int sep;
    const char *msg;
{
    Char *sp, *bp = buffer, *ebp = &buffer[MAXPATHLEN];

    for (sp = str; *sp && *sp != sep && bp < ebp;)
	if (sp[0] == '$' && sp[1] == ':' && Isdigit(sp[sp[2] == '-' ? 3 : 2])) {
	    int num, neg = 0;
	    sp += 2;
	    if (*sp == '-') {
		neg = 1;
		sp++;
	    }
	    for (num = *sp++ - '0'; Isdigit(*sp); num += 10 * num + *sp++ - '0')
		continue;
	    if (neg)
		num = nwl - num - 1;
	    if (num >= 0 && num < nwl) {
		Char *ptr;
		for (ptr = wl[num]; *ptr && bp < ebp - 1; *bp++ = *ptr++)
		    continue;
		
	    }
	}
	else
	    *bp++ = *sp++;

    *bp = '\0';

    if (*sp++ == sep)
	return sp;

    stderror(ERR_COMPMIS, sep, msg, short2str(str));
    return --sp;
} /* end tw_dollar */
		

/* tw_complete():
 *	Return the appropriate completion for the command
 *
 *	valid completion strings are:
 *	p/<range>/<completion>/[<suffix>/]	positional
 *	c/<pattern>/<completion>/[<suffix>/]	current word ignore pattern
 *	C/<pattern>/<completion>/[<suffix>/]	current word with pattern
 *	n/<pattern>/<completion>/[<suffix>/]	next word
 *	N/<pattern>/<completion>/[<suffix>/]	next-next word
 */
int
tw_complete(line, word, pat, looking, suf)
    Char *line, **word, **pat;
    int looking, *suf;
{
    Char buf[MAXPATHLEN + 1], **vec, *ptr; 
    Char *wl[MAXPATHLEN/6];
    static Char nomatch[2] = { (Char) ~0, 0x00 };
    int wordno, n;

    copyn(buf, line, MAXPATHLEN);

    /* find the command */
    if ((wl[0] = tw_tok(buf)) == NULL || wl[0] == INVPTR)
	return TW_ZERO;

    /*
     * look for hardwired command completions using a globbing
     * search and for arguments using a normal search.
     */
    if ((vec = tw_find(wl[0], &completions, (looking == TW_COMMAND))) == NULL)
	return looking;

    /* tokenize the line one more time :-( */
    for (wordno = 1; (wl[wordno] = tw_tok(NULL)) != NULL &&
		      wl[wordno] != INVPTR; wordno++)
	continue;

    if (wl[wordno] == INVPTR)		/* Found a meta character */
	return TW_ZERO;			/* de-activate completions */
#ifdef TDEBUG
    {
	int i;
	for (i = 0; i < wordno; i++)
	    xprintf("'%s' ", short2str(wl[i]));
	xprintf("\n");
    }
#endif /* TDEBUG */

    /* if the current word is empty move the last word to the next */
    if (**word == '\0') {
	wl[wordno] = *word;
	wordno++;
    }
    wl[wordno] = NULL;
	

#ifdef TDEBUG
    xprintf("\r\n");
    xprintf("  w#: %d\n", wordno);
    xprintf("line: %s\n", short2str(line));
    xprintf(" cmd: %s\n", short2str(wl[0]));
    xprintf("word: %s\n", short2str(*word));
    xprintf("last: %s\n", wordno - 2 >= 0 ? short2str(wl[wordno-2]) : "n/a");
    xprintf("this: %s\n", wordno - 1 >= 0 ? short2str(wl[wordno-1]) : "n/a");
#endif /* TDEBUG */
    
    for (;vec != NULL && (ptr = vec[0]) != NULL; vec++) {
	Char  ran[MAXPATHLEN+1],/* The pattern or range X/<range>/XXXX/ */
	      com[MAXPATHLEN+1],/* The completion X/XXXXX/<completion>/ */
	     *pos = NULL;	/* scratch pointer 			*/
	int   cmd, sep;		/* the command and separator characters */

	if (ptr[0] == '\0')
	    continue;

#ifdef TDEBUG
	xprintf("match %s\n", short2str(ptr));
#endif /* TDEBUG */

	switch (cmd = ptr[0]) {
	case 'N':
	    pos = (wordno - 3 < 0) ? nomatch : wl[wordno - 3];
	    break;
	case 'n':
	    pos = (wordno - 2 < 0) ? nomatch : wl[wordno - 2];
	    break;
	case 'c':
	case 'C':
	    pos = (wordno - 1 < 0) ? nomatch : wl[wordno - 1];
	    break;
	case 'p':
	    break;
	default:
	    stderror(ERR_COMPINV, CGETS(27, 1, "command"), cmd);
	    return TW_ZERO;
	}

	sep = ptr[1];
	if (!Ispunct(sep)) {
	    stderror(ERR_COMPINV, CGETS(27, 2, "separator"), sep);
	    return TW_ZERO;
	}

	ptr = tw_dollar(&ptr[2], wl, wordno, ran, sep,
			CGETS(27, 3, "pattern"));
	if (ran[0] == '\0')	/* check for empty pattern (disallowed) */
	{
	    stderror(ERR_COMPINC, cmd == 'p' ?  CGETS(27, 4, "range") :
		     CGETS(27, 3, "pattern"), "");
	    return TW_ZERO;
	}

	ptr = tw_dollar(ptr, wl, wordno, com, sep, CGETS(27, 5, "completion")); 

	if (*ptr != '\0') {
	    if (*ptr == sep)
		*suf = ~0;
	    else
		*suf = *ptr;
	}
	else
	    *suf = '\0';

#ifdef TDEBUG
	xprintf("command:    %c\nseparator:  %c\n", cmd, sep);
	xprintf("pattern:    %s\n", short2str(ran));
	xprintf("completion: %s\n", short2str(com));
	xprintf("suffix:     ");
        switch (*suf) {
	case 0:
	    xprintf("*auto suffix*\n");
	    break;
	case ~0:
	    xprintf("*no suffix*\n");
	    break;
	default:
	    xprintf("%c\n", *suf);
	    break;
	}
#endif /* TDEBUG */

	switch (cmd) {
	case 'p':			/* positional completion */
#ifdef TDEBUG
	    xprintf("p: tw_pos(%s, %d) = ", short2str(ran), wordno - 1);
	    xprintf("%d\n", tw_pos(ran, wordno - 1));
#endif /* TDEBUG */
	    if (!tw_pos(ran, wordno - 1))
		continue;
	    return tw_result(com, pat);

	case 'N':			/* match with the next-next word */
	case 'n':			/* match with the next word */
	case 'c':			/* match with the current word */
	case 'C':
#ifdef TDEBUG
	    xprintf("%c: ", cmd);
#endif /* TDEBUG */
	    if ((n = tw_match(pos, ran)) < 0)
		continue;
	    if (cmd == 'c')
		*word += n;
	    return tw_result(com, pat);

	default:
	    return TW_ZERO;	/* Cannot happen */
	}
    }
    *suf = '\0';
    return TW_ZERO;
} /* end tw_complete */
