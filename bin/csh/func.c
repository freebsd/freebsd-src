/*-
 * Copyright (c) 1980, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)func.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] =
	"$Id: func.c,v 1.8 1997/08/07 21:42:09 steve Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"
#include "pathnames.h"

extern char **environ;

static int zlast = -1;
static void	islogin __P((void));
static void	reexecute __P((struct command *));
static void	preread __P((void));
static void	doagain __P((void));
static void	search __P((int, int, Char *));
static int	getword __P((Char *));
static int	keyword __P((Char *));
static void	toend __P((void));
static void	xecho __P((int, Char **));
static void	Unsetenv __P((Char *));

struct biltins *
isbfunc(t)
    struct command *t;
{
    Char *cp = t->t_dcom[0];
    struct biltins *bp, *bp1, *bp2;
    static struct biltins label = {"", dozip, 0, 0};
    static struct biltins foregnd = {"%job", dofg1, 0, 0};
    static struct biltins backgnd = {"%job &", dobg1, 0, 0};

    if (lastchr(cp) == ':') {
	label.bname = short2str(cp);
	return (&label);
    }
    if (*cp == '%') {
	if (t->t_dflg & F_AMPERSAND) {
	    t->t_dflg &= ~F_AMPERSAND;
	    backgnd.bname = short2str(cp);
	    return (&backgnd);
	}
	foregnd.bname = short2str(cp);
	return (&foregnd);
    }
    /*
     * Binary search Bp1 is the beginning of the current search range. Bp2 is
     * one past the end.
     */
    for (bp1 = bfunc, bp2 = bfunc + nbfunc; bp1 < bp2;) {
	int i;

	bp = bp1 + ((bp2 - bp1) >> 1);
	if ((i = *cp - *bp->bname) == 0 &&
	    (i = Strcmp(cp, str2short(bp->bname))) == 0)
	    return bp;
	if (i < 0)
	    bp2 = bp;
	else
	    bp1 = bp + 1;
    }
    return (0);
}

void
func(t, bp)
    struct command *t;
    struct biltins *bp;
{
    int     i;

    xechoit(t->t_dcom);
    setname(bp->bname);
    i = blklen(t->t_dcom) - 1;
    if (i < bp->minargs)
	stderror(ERR_NAME | ERR_TOOFEW);
    if (i > bp->maxargs)
	stderror(ERR_NAME | ERR_TOOMANY);
    (*bp->bfunct) (t->t_dcom, t);
}

void
/*ARGSUSED*/
doonintr(v, t)
    Char **v;
    struct command *t;
{
    Char *cp;
    Char *vv = v[1];

    if (parintr == SIG_IGN)
	return;
    if (setintr && intty)
	stderror(ERR_NAME | ERR_TERMINAL);
    cp = gointr;
    gointr = 0;
    xfree((ptr_t) cp);
    if (vv == 0) {
	if (setintr)
	    (void) sigblock(sigmask(SIGINT));
	else
	    (void) signal(SIGINT, SIG_DFL);
	gointr = 0;
    }
    else if (eq((vv = strip(vv)), STRminus)) {
	(void) signal(SIGINT, SIG_IGN);
	gointr = Strsave(STRminus);
    }
    else {
	gointr = Strsave(vv);
	(void) signal(SIGINT, pintr);
    }
}

void
/*ARGSUSED*/
donohup(v, t)
    Char **v;
    struct command *t;
{
    if (intty)
	stderror(ERR_NAME | ERR_TERMINAL);
    if (setintr == 0) {
	(void) signal(SIGHUP, SIG_IGN);
    }
}

void
/*ARGSUSED*/
dozip(v, t)
    Char **v;
    struct command *t;
{
    ;
}

void
prvars()
{
    plist(&shvhed);
}

void
/*ARGSUSED*/
doalias(v, t)
    Char **v;
    struct command *t;
{
    struct varent *vp;
    Char *p;

    v++;
    p = *v++;
    if (p == 0)
	plist(&aliases);
    else if (*v == 0) {
	vp = adrof1(strip(p), &aliases);
	if (vp) {
	    blkpr(cshout, vp->vec);
	    fputc('\n', cshout);
	}
    }
    else {
	if (eq(p, STRalias) || eq(p, STRunalias)) {
	    setname(vis_str(p));
	    stderror(ERR_NAME | ERR_DANGER);
	}
	set1(strip(p), saveblk(v), &aliases);
    }
}

void
/*ARGSUSED*/
unalias(v, t)
    Char **v;
    struct command *t;
{
    unset1(v, &aliases);
}

void
/*ARGSUSED*/
dologout(v, t)
    Char **v;
    struct command *t;
{
    islogin();
    goodbye();
}

void
/*ARGSUSED*/
dologin(v, t)
    Char **v;
    struct command *t;
{
    islogin();
    rechist();
    (void) signal(SIGTERM, parterm);
    (void) execl(_PATH_LOGIN, "login", short2str(v[1]), NULL);
    untty();
    xexit(1);
}

static void
islogin()
{
    if (chkstop == 0 && setintr)
	panystop(0);
    if (loginsh)
	return;
    stderror(ERR_NOTLOGIN);
}

void
doif(v, kp)
    Char  **v;
    struct command *kp;
{
    int i;
    Char **vv;

    v++;
    i = expr(&v);
    vv = v;
    if (*vv == NULL)
	stderror(ERR_NAME | ERR_EMPTYIF);
    if (eq(*vv, STRthen)) {
	if (*++vv)
	    stderror(ERR_NAME | ERR_IMPRTHEN);
	setname(vis_str(STRthen));
	/*
	 * If expression was zero, then scan to else, otherwise just fall into
	 * following code.
	 */
	if (!i)
	    search(T_IF, 0, NULL);
	return;
    }
    /*
     * Simple command attached to this if. Left shift the node in this tree,
     * munging it so we can reexecute it.
     */
    if (i) {
	lshift(kp->t_dcom, vv - kp->t_dcom);
	reexecute(kp);
	donefds();
    }
}

/*
 * Reexecute a command, being careful not
 * to redo i/o redirection, which is already set up.
 */
static void
reexecute(kp)
    struct command *kp;
{
    kp->t_dflg &= F_SAVE;
    kp->t_dflg |= F_REPEAT;
    /*
     * If tty is still ours to arbitrate, arbitrate it; otherwise dont even set
     * pgrp's as the jobs would then have no way to get the tty (we can't give
     * it to them, and our parent wouldn't know their pgrp, etc.
     */
    execute(kp, (tpgrp > 0 ? tpgrp : -1), NULL, NULL);
}

void
/*ARGSUSED*/
doelse(v, t)
    Char **v;
    struct command *t;
{
    search(T_ELSE, 0, NULL);
}

void
/*ARGSUSED*/
dogoto(v, t)
    Char **v;
    struct command *t;
{
    Char   *lp;

    gotolab(lp = globone(v[1], G_ERROR));
    xfree((ptr_t) lp);
}

void
gotolab(lab)
    Char *lab;
{
    struct whyle *wp;
    /*
     * While we still can, locate any unknown ends of existing loops. This
     * obscure code is the WORST result of the fact that we don't really parse.
     */
    zlast = T_GOTO;
    for (wp = whyles; wp; wp = wp->w_next)
	if (wp->w_end.type == F_SEEK && wp->w_end.f_seek == 0) {
	    search(T_BREAK, 0, NULL);
	    btell(&wp->w_end);
	}
	else
	    bseek(&wp->w_end);
    search(T_GOTO, 0, lab);
    /*
     * Eliminate loops which were exited.
     */
    wfree();
}

void
/*ARGSUSED*/
doswitch(v, t)
    Char **v;
    struct command *t;
{
    Char *cp, *lp;

    v++;
    if (!*v || *(*v++) != '(')
	stderror(ERR_SYNTAX);
    cp = **v == ')' ? STRNULL : *v++;
    if (*(*v++) != ')')
	v--;
    if (*v)
	stderror(ERR_SYNTAX);
    search(T_SWITCH, 0, lp = globone(cp, G_ERROR));
    xfree((ptr_t) lp);
}

void
/*ARGSUSED*/
dobreak(v, t)
    Char **v;
    struct command *t;
{
    if (whyles)
	toend();
    else
	stderror(ERR_NAME | ERR_NOTWHILE);
}

void
/*ARGSUSED*/
doexit(v, t)
    Char **v;
    struct command *t;
{
    if (chkstop == 0 && (intty || intact) && evalvec == 0)
	panystop(0);
    /*
     * Don't DEMAND parentheses here either.
     */
    v++;
    if (*v) {
	set(STRstatus, putn(expr(&v)));
	if (*v)
	    stderror(ERR_NAME | ERR_EXPRESSION);
    }
    btoeof();
    if (intty)
	(void) close(SHIN);
}

void
/*ARGSUSED*/
doforeach(v, t)
    Char **v;
    struct command *t;
{
    Char *cp, *sp;
    struct whyle *nwp;

    v++;
    sp = cp = strip(*v);
    if (!letter(*sp))
	stderror(ERR_NAME | ERR_VARBEGIN);
    while (*cp && alnum(*cp))
	cp++;
    if (*cp)
	stderror(ERR_NAME | ERR_VARALNUM);
    if ((cp - sp) > MAXVARLEN)
	stderror(ERR_NAME | ERR_VARTOOLONG);
    cp = *v++;
    if (v[0][0] != '(' || v[blklen(v) - 1][0] != ')')
	stderror(ERR_NAME | ERR_NOPAREN);
    v++;
    gflag = 0, tglob(v);
    v = globall(v);
    if (v == 0)
	stderror(ERR_NAME | ERR_NOMATCH);
    nwp = (struct whyle *) xcalloc(1, sizeof *nwp);
    nwp->w_fe = nwp->w_fe0 = v;
    gargv = 0;
    btell(&nwp->w_start);
    nwp->w_fename = Strsave(cp);
    nwp->w_next = whyles;
    nwp->w_end.type = F_SEEK;
    whyles = nwp;
    /*
     * Pre-read the loop so as to be more comprehensible to a terminal user.
     */
    zlast = T_FOREACH;
    if (intty)
	preread();
    doagain();
}

void
/*ARGSUSED*/
dowhile(v, t)
    Char **v;
    struct command *t;
{
    int status;
    bool again = whyles != 0 && SEEKEQ(&whyles->w_start, &lineloc) &&
    whyles->w_fename == 0;

    v++;
    /*
     * Implement prereading here also, taking care not to evaluate the
     * expression before the loop has been read up from a terminal.
     */
    if (intty && !again)
	status = !exp0(&v, 1);
    else
	status = !expr(&v);
    if (*v)
	stderror(ERR_NAME | ERR_EXPRESSION);
    if (!again) {
	struct whyle *nwp =
	(struct whyle *) xcalloc(1, sizeof(*nwp));

	nwp->w_start = lineloc;
	nwp->w_end.type = F_SEEK;
	nwp->w_end.f_seek = 0;
	nwp->w_next = whyles;
	whyles = nwp;
	zlast = T_WHILE;
	if (intty) {
	    /*
	     * The tty preread
	     */
	    preread();
	    doagain();
	    return;
	}
    }
    if (status)
	/* We ain't gonna loop no more, no more! */
	toend();
}

static void
preread()
{
    whyles->w_end.type = I_SEEK;
    if (setintr)
	(void) sigsetmask(sigblock((sigset_t) 0) & ~sigmask(SIGINT));

    search(T_BREAK, 0, NULL);		/* read the expression in */
    if (setintr)
	(void) sigblock(sigmask(SIGINT));
    btell(&whyles->w_end);
}

void
/*ARGSUSED*/
doend(v, t)
    Char **v;
    struct command *t;
{
    if (!whyles)
	stderror(ERR_NAME | ERR_NOTWHILE);
    btell(&whyles->w_end);
    doagain();
}

void
/*ARGSUSED*/
docontin(v, t)
    Char **v;
    struct command *t;
{
    if (!whyles)
	stderror(ERR_NAME | ERR_NOTWHILE);
    doagain();
}

static void
doagain()
{
    /* Repeating a while is simple */
    if (whyles->w_fename == 0) {
	bseek(&whyles->w_start);
	return;
    }
    /*
     * The foreach variable list actually has a spurious word ")" at the end of
     * the w_fe list.  Thus we are at the of the list if one word beyond this
     * is 0.
     */
    if (!whyles->w_fe[1]) {
	dobreak(NULL, NULL);
	return;
    }
    set(whyles->w_fename, Strsave(*whyles->w_fe++));
    bseek(&whyles->w_start);
}

void
dorepeat(v, kp)
    Char  **v;
    struct command *kp;
{
    int i;
    sigset_t omask = 0;

    i = getn(v[1]);
    if (setintr)
	omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
    lshift(v, 2);
    while (i > 0) {
	if (setintr)
	    (void) sigsetmask(omask);
	reexecute(kp);
	--i;
    }
    donefds();
    if (setintr)
	(void) sigsetmask(omask);
}

void
/*ARGSUSED*/
doswbrk(v, t)
    Char **v;
    struct command *t;
{
    search(T_BRKSW, 0, NULL);
}

int
srchx(cp)
    Char *cp;
{
    struct srch *sp, *sp1, *sp2;
    int i;

    /*
     * Binary search Sp1 is the beginning of the current search range. Sp2 is
     * one past the end.
     */
    for (sp1 = srchn, sp2 = srchn + nsrchn; sp1 < sp2;) {
	sp = sp1 + ((sp2 - sp1) >> 1);
	if ((i = *cp - *sp->s_name) == 0 &&
	    (i = Strcmp(cp, str2short(sp->s_name))) == 0)
	    return sp->s_value;
	if (i < 0)
	    sp2 = sp;
	else
	    sp1 = sp + 1;
    }
    return (-1);
}

static Char Stype;
static Char *Sgoal;

/*VARARGS2*/
static void
search(type, level, goal)
    int     type;
    int level;
    Char   *goal;
{
    Char    wordbuf[BUFSIZ];
    Char *aword = wordbuf;
    Char *cp;

    Stype = type;
    Sgoal = goal;
    if (type == T_GOTO) {
	struct Ain a;
	a.type = F_SEEK;
	a.f_seek = 0;
	bseek(&a);
    }
    do {
	if (intty && fseekp == feobp && aret == F_SEEK)
	    (void) fprintf(cshout, "? "), (void) fflush(cshout);
	aword[0] = 0;
	(void) getword(aword);
	switch (srchx(aword)) {

	case T_ELSE:
	    if (level == 0 && type == T_IF)
		return;
	    break;

	case T_IF:
	    while (getword(aword))
		continue;
	    if ((type == T_IF || type == T_ELSE) &&
		eq(aword, STRthen))
		level++;
	    break;

	case T_ENDIF:
	    if (type == T_IF || type == T_ELSE)
		level--;
	    break;

	case T_FOREACH:
	case T_WHILE:
	    if (type == T_BREAK)
		level++;
	    break;

	case T_END:
	    if (type == T_BREAK)
		level--;
	    break;

	case T_SWITCH:
	    if (type == T_SWITCH || type == T_BRKSW)
		level++;
	    break;

	case T_ENDSW:
	    if (type == T_SWITCH || type == T_BRKSW)
		level--;
	    break;

	case T_LABEL:
	    if (type == T_GOTO && getword(aword) && eq(aword, goal))
		level = -1;
	    break;

	default:
	    if (type != T_GOTO && (type != T_SWITCH || level != 0))
		break;
	    if (lastchr(aword) != ':')
		break;
	    aword[Strlen(aword) - 1] = 0;
	    if ((type == T_GOTO && eq(aword, goal)) ||
		(type == T_SWITCH && eq(aword, STRdefault)))
		level = -1;
	    break;

	case T_CASE:
	    if (type != T_SWITCH || level != 0)
		break;
	    (void) getword(aword);
	    if (lastchr(aword) == ':')
		aword[Strlen(aword) - 1] = 0;
	    cp = strip(Dfix1(aword));
	    if (Gmatch(goal, cp))
		level = -1;
	    xfree((ptr_t) cp);
	    break;

	case T_DEFAULT:
	    if (type == T_SWITCH && level == 0)
		level = -1;
	    break;
	}
	(void) getword(NULL);
    } while (level >= 0);
}

static int
getword(wp)
    Char *wp;
{
    int found = 0;
    int c, d;
    int     kwd = 0;
    Char   *owp = wp;

    c = readc(1);
    d = 0;
    do {
	while (c == ' ' || c == '\t')
	    c = readc(1);
	if (c == '#')
	    do
		c = readc(1);
	    while (c >= 0 && c != '\n');
	if (c < 0)
	    goto past;
	if (c == '\n') {
	    if (wp)
		break;
	    return (0);
	}
	unreadc(c);
	found = 1;
	do {
	    c = readc(1);
	    if (c == '\\' && (c = readc(1)) == '\n')
		c = ' ';
	    if (c == '\'' || c == '"')
		if (d == 0)
		    d = c;
		else if (d == c)
		    d = 0;
	    if (c < 0)
		goto past;
	    if (wp) {
		*wp++ = c;
		*wp = 0;	/* end the string b4 test */
	    }
	} while ((d || (!(kwd = keyword(owp)) && c != ' '
		  && c != '\t')) && c != '\n');
    } while (wp == 0);

    /*
     * if we have read a keyword ( "if", "switch" or "while" ) then we do not
     * need to unreadc the look-ahead char
     */
    if (!kwd) {
	unreadc(c);
	if (found)
	    *--wp = 0;
    }

    return (found);

past:
    switch (Stype) {

    case T_IF:
	stderror(ERR_NAME | ERR_NOTFOUND, "then/endif");

    case T_ELSE:
	stderror(ERR_NAME | ERR_NOTFOUND, "endif");

    case T_BRKSW:
    case T_SWITCH:
	stderror(ERR_NAME | ERR_NOTFOUND, "endsw");

    case T_BREAK:
	stderror(ERR_NAME | ERR_NOTFOUND, "end");

    case T_GOTO:
	setname(vis_str(Sgoal));
	stderror(ERR_NAME | ERR_NOTFOUND, "label");
    }
    /* NOTREACHED */
    return (0);
}

/*
 * keyword(wp) determines if wp is one of the built-n functions if,
 * switch or while. It seems that when an if statement looks like
 * "if(" then getword above sucks in the '(' and so the search routine
 * never finds what it is scanning for. Rather than rewrite doword, I hack
 * in a test to see if the string forms a keyword. Then doword stops
 * and returns the word "if" -strike
 */

static int
keyword(wp)
    Char   *wp;
{
    static Char STRif[] = {'i', 'f', '\0'};
    static Char STRwhile[] = {'w', 'h', 'i', 'l', 'e', '\0'};
    static Char STRswitch[] = {'s', 'w', 'i', 't', 'c', 'h', '\0'};

    if (!wp)
	return (0);

    if ((Strcmp(wp, STRif) == 0) || (Strcmp(wp, STRwhile) == 0)
	|| (Strcmp(wp, STRswitch) == 0))
	return (1);

    return (0);
}

static void
toend()
{
    if (whyles->w_end.type == F_SEEK && whyles->w_end.f_seek == 0) {
	search(T_BREAK, 0, NULL);
	btell(&whyles->w_end);
	whyles->w_end.f_seek--;
    }
    else
	bseek(&whyles->w_end);
    wfree();
}

void
wfree()
{
    struct Ain    o;
    struct whyle *nwp;

    btell(&o);

    for (; whyles; whyles = nwp) {
	struct whyle *wp = whyles;
	nwp = wp->w_next;

	/*
	 * We free loops that have different seek types.
	 */
	if (wp->w_end.type != I_SEEK && wp->w_start.type == wp->w_end.type &&
	    wp->w_start.type == o.type) {
	    if (wp->w_end.type == F_SEEK) {
		if (o.f_seek >= wp->w_start.f_seek &&
		    (wp->w_end.f_seek == 0 || o.f_seek < wp->w_end.f_seek))
		    break;
	    }
	    else {
		if (o.a_seek >= wp->w_start.a_seek &&
		    (wp->w_end.a_seek == 0 || o.a_seek < wp->w_end.a_seek))
		    break;
	    }
	}

	if (wp->w_fe0)
	    blkfree(wp->w_fe0);
	if (wp->w_fename)
	    xfree((ptr_t) wp->w_fename);
	xfree((ptr_t) wp);
    }
}

void
/*ARGSUSED*/
doecho(v, t)
    Char **v;
    struct command *t;
{
    xecho(' ', v);
}

void
/*ARGSUSED*/
doglob(v, t)
    Char **v;
    struct command *t;
{
    xecho(0, v);
    (void) fflush(cshout);
}

static void
xecho(sep, v)
    int    sep;
    Char **v;
{
    Char *cp;
    int     nonl = 0;

    if (setintr)
	(void) sigsetmask(sigblock((sigset_t) 0) & ~sigmask(SIGINT));
    v++;
    if (*v == 0)
	return;
    gflag = 0, tglob(v);
    if (gflag) {
	v = globall(v);
	if (v == 0)
	    stderror(ERR_NAME | ERR_NOMATCH);
    }
    else {
	v = gargv = saveblk(v);
	trim(v);
    }
    if (sep == ' ' && *v && eq(*v, STRmn))
	nonl++, v++;
    while ((cp = *v++) != NULL) {
	int c;

	while ((c = *cp++) != '\0')
	    (void) vis_fputc(c | QUOTE, cshout);

	if (*v)
	    (void) vis_fputc(sep | QUOTE, cshout);
    }
    if (sep && nonl == 0)
	(void) fputc('\n', cshout);
    else
	(void) fflush(cshout);
    if (setintr)
	(void) sigblock(sigmask(SIGINT));
    if (gargv)
	blkfree(gargv), gargv = 0;
}

void
/*ARGSUSED*/
dosetenv(v, t)
    Char **v;
    struct command *t;
{
    Char   *vp, *lp;

    v++;
    if ((vp = *v++) == 0) {
	Char **ep;

	if (setintr)
	    (void) sigsetmask(sigblock((sigset_t) 0) & ~sigmask(SIGINT));
	for (ep = STR_environ; *ep; ep++)
	    (void) fprintf(cshout, "%s\n", vis_str(*ep));
	return;
    }
    if ((lp = *v++) == 0)
	lp = STRNULL;
    Setenv(vp, lp = globone(lp, G_APPEND));
    if (eq(vp, STRPATH)) {
	importpath(lp);
	dohash(NULL, NULL);
    }
    else if (eq(vp, STRLANG) || eq(vp, STRLC_CTYPE) ||
	     eq(vp, STRLC_ALL) || eq(vp, STRLC_COLLATE)) {
#ifdef NLS
	int     k;

	(void) setlocale(LC_ALL, "");
	for (k = 0200; k <= 0377 && !Isprint(k); k++)
		continue;
	AsciiOnly = k > 0377;
#else
	AsciiOnly = 0;
#endif				/* NLS */
    }
    xfree((ptr_t) lp);
}

void
/*ARGSUSED*/
dounsetenv(v, t)
    Char **v;
    struct command *t;
{
    Char  **ep, *p, *n;
    int     i, maxi;
    static Char *name = NULL;

    if (name)
	xfree((ptr_t) name);
    /*
     * Find the longest environment variable
     */
    for (maxi = 0, ep = STR_environ; *ep; ep++) {
	for (i = 0, p = *ep; *p && *p != '='; p++, i++)
	    continue;
	if (i > maxi)
	    maxi = i;
    }

    name = (Char *) xmalloc((size_t) (maxi + 1) * sizeof(Char));

    while (++v && *v)
	for (maxi = 1; maxi;)
	    for (maxi = 0, ep = STR_environ; *ep; ep++) {
		for (n = name, p = *ep; *p && *p != '='; *n++ = *p++)
		    continue;
		*n = '\0';
		if (!Gmatch(name, *v))
		    continue;
		maxi = 1;
		/*
		 * Delete name, and start again cause the environment changes
		 */
		Unsetenv(name);
		if (eq(name, STRLANG) || eq(name, STRLC_CTYPE) ||
		    eq(name, STRLC_ALL) || eq(name, STRLC_COLLATE)) {
#ifdef NLS
		    int     k;

		    (void) setlocale(LC_ALL, "");
		    for (k = 0200; k <= 0377 && !Isprint(k); k++)
			continue;
		    AsciiOnly = k > 0377;
#else
		    AsciiOnly = getenv("LANG") == NULL &&
			getenv("LC_ALL") == NULL &&
			getenv("LC_CTYPE") == NULL;
#endif				/* NLS */
		}
		break;
	    }
    xfree((ptr_t) name);
    name = NULL;
}

void
Setenv(name, val)
    Char   *name, *val;
{
    Char **ep = STR_environ;
    Char *cp, *dp;
    Char   *blk[2];
    Char  **oep = ep;


    for (; *ep; ep++) {
	for (cp = name, dp = *ep; *cp && *cp == *dp; cp++, dp++)
	    continue;
	if (*cp != 0 || *dp != '=')
	    continue;
	cp = Strspl(STRequal, val);
	xfree((ptr_t) * ep);
	*ep = strip(Strspl(name, cp));
	xfree((ptr_t) cp);
	blkfree((Char **) environ);
	environ = short2blk(STR_environ);
	return;
    }
    cp = Strspl(name, STRequal);
    blk[0] = strip(Strspl(cp, val));
    xfree((ptr_t) cp);
    blk[1] = 0;
    STR_environ = blkspl(STR_environ, blk);
    blkfree((Char **) environ);
    environ = short2blk(STR_environ);
    xfree((ptr_t) oep);
}

static void
Unsetenv(name)
    Char   *name;
{
    Char **ep = STR_environ;
    Char *cp, *dp;
    Char  **oep = ep;

    for (; *ep; ep++) {
	for (cp = name, dp = *ep; *cp && *cp == *dp; cp++, dp++)
	    continue;
	if (*cp != 0 || *dp != '=')
	    continue;
	cp = *ep;
	*ep = 0;
	STR_environ = blkspl(STR_environ, ep + 1);
	environ = short2blk(STR_environ);
	*ep = cp;
	xfree((ptr_t) cp);
	xfree((ptr_t) oep);
	return;
    }
}

void
/*ARGSUSED*/
doumask(v, t)
    Char **v;
    struct command *t;
{
    Char *cp = v[1];
    int i;

    if (cp == 0) {
	i = umask(0);
	(void) umask(i);
	(void) fprintf(cshout, "%o\n", i);
	return;
    }
    i = 0;
    while (Isdigit(*cp) && *cp != '8' && *cp != '9')
	i = i * 8 + *cp++ - '0';
    if (*cp || i < 0 || i > 0777)
	stderror(ERR_NAME | ERR_MASK);
    (void) umask(i);
}

typedef quad_t RLIM_TYPE;

static struct limits {
    int     limconst;
    char   *limname;
    int     limdiv;
    char   *limscale;
}       limits[] = {
    { RLIMIT_CPU,	"cputime",	1,	"seconds" },
    { RLIMIT_FSIZE,	"filesize",	1024,	"kbytes" },
    { RLIMIT_DATA,	"datasize",	1024,	"kbytes" },
    { RLIMIT_STACK,	"stacksize",	1024,	"kbytes" },
    { RLIMIT_CORE,	"coredumpsize", 1024,	"kbytes" },
    { RLIMIT_RSS,	"memoryuse",	1024,	"kbytes" },
    { RLIMIT_MEMLOCK,	"memorylocked",	1024,	"kbytes" },
    { RLIMIT_NPROC,	"maxproc",	1,	"" },
    { RLIMIT_NOFILE,	"openfiles",	1,	"" },
    { -1,		NULL,		0,	NULL }
};

static struct limits *findlim();
static RLIM_TYPE getval();
static void limtail();
static void plim();
static int setlim();

static struct limits *
findlim(cp)
    Char   *cp;
{
    struct limits *lp, *res;

    res = (struct limits *) NULL;
    for (lp = limits; lp->limconst >= 0; lp++)
	if (prefix(cp, str2short(lp->limname))) {
	    if (res)
		stderror(ERR_NAME | ERR_AMBIG);
	    res = lp;
	}
    if (res)
	return (res);
    stderror(ERR_NAME | ERR_LIMIT);
    /* NOTREACHED */
    return (0);
}

void
/*ARGSUSED*/
dolimit(v, t)
    Char **v;
    struct command *t;
{
    struct limits *lp;
    RLIM_TYPE limit;
    char    hard = 0;

    v++;
    if (*v && eq(*v, STRmh)) {
	hard = 1;
	v++;
    }
    if (*v == 0) {
	for (lp = limits; lp->limconst >= 0; lp++)
	    plim(lp, hard);
	return;
    }
    lp = findlim(v[0]);
    if (v[1] == 0) {
	plim(lp, hard);
	return;
    }
    limit = getval(lp, v + 1);
    if (setlim(lp, hard, limit) < 0)
	stderror(ERR_SILENT);
}

static  RLIM_TYPE
getval(lp, v)
    struct limits *lp;
    Char  **v;
{
    float f;
    Char   *cp = *v++;

    f = atof(short2str(cp));

    while (Isdigit(*cp) || *cp == '.' || *cp == 'e' || *cp == 'E')
	cp++;
    if (*cp == 0) {
	if (*v == 0)
	    return ((RLIM_TYPE) ((f + 0.5) * lp->limdiv));
	cp = *v;
    }
    switch (*cp) {
    case ':':
	if (lp->limconst != RLIMIT_CPU)
	    goto badscal;
	return ((RLIM_TYPE) (f * 60.0 + atof(short2str(cp + 1))));
    case 'h':
	if (lp->limconst != RLIMIT_CPU)
	    goto badscal;
	limtail(cp, "hours");
	f *= 3600.0;
	break;
    case 'm':
	if (lp->limconst == RLIMIT_CPU) {
	    limtail(cp, "minutes");
	    f *= 60.0;
	    break;
	}
	*cp = 'm';
	limtail(cp, "megabytes");
	f *= 1024.0 * 1024.0;
	break;
    case 's':
	if (lp->limconst != RLIMIT_CPU)
	    goto badscal;
	limtail(cp, "seconds");
	break;
    case 'M':
	if (lp->limconst == RLIMIT_CPU)
	    goto badscal;
	*cp = 'm';
	limtail(cp, "megabytes");
	f *= 1024.0 * 1024.0;
	break;
    case 'k':
	if (lp->limconst == RLIMIT_CPU)
	    goto badscal;
	limtail(cp, "kbytes");
	f *= 1024.0;
	break;
    case 'u':
	limtail(cp, "unlimited");
	return (RLIM_INFINITY);
    default:
badscal:
	stderror(ERR_NAME | ERR_SCALEF);
    }
    f += 0.5;
    if (f > (float) RLIM_INFINITY)
	return RLIM_INFINITY;
    else
	return ((RLIM_TYPE) f);
}

static void
limtail(cp, str)
    Char   *cp;
    char   *str;
{
    while (*cp && *cp == *str)
	cp++, str++;
    if (*cp)
	stderror(ERR_BADSCALE, str);
}


/*ARGSUSED*/
static void
plim(lp, hard)
    struct limits *lp;
    Char    hard;
{
    struct rlimit rlim;
    RLIM_TYPE limit;

    (void) fprintf(cshout, "%s \t", lp->limname);

    (void) getrlimit(lp->limconst, &rlim);
    limit = hard ? rlim.rlim_max : rlim.rlim_cur;

    if (limit == RLIM_INFINITY)
	(void) fprintf(cshout, "unlimited");
    else if (lp->limconst == RLIMIT_CPU)
	psecs((long) limit);
    else
	(void) fprintf(cshout, "%ld %s", (long) (limit / lp->limdiv),
		       lp->limscale);
    (void) fputc('\n', cshout);
}

void
/*ARGSUSED*/
dounlimit(v, t)
    Char **v;
    struct command *t;
{
    struct limits *lp;
    int     lerr = 0;
    Char    hard = 0;

    v++;
    if (*v && eq(*v, STRmh)) {
	hard = 1;
	v++;
    }
    if (*v == 0) {
	for (lp = limits; lp->limconst >= 0; lp++)
	    if (setlim(lp, hard, (RLIM_TYPE) RLIM_INFINITY) < 0)
		lerr++;
	if (lerr)
	    stderror(ERR_SILENT);
	return;
    }
    while (*v) {
	lp = findlim(*v++);
	if (setlim(lp, hard, (RLIM_TYPE) RLIM_INFINITY) < 0)
	    stderror(ERR_SILENT);
    }
}

static int
setlim(lp, hard, limit)
    struct limits *lp;
    Char    hard;
    RLIM_TYPE limit;
{
    struct rlimit rlim;

    (void) getrlimit(lp->limconst, &rlim);

    if (hard)
	rlim.rlim_max = limit;
    else if (limit == RLIM_INFINITY && geteuid() != 0)
	rlim.rlim_cur = rlim.rlim_max;
    else
	rlim.rlim_cur = limit;

    if (setrlimit(lp->limconst, &rlim) < 0) {
	(void) fprintf(csherr, "%s: %s: Can't %s%s limit\n", bname, lp->limname,
		       limit == RLIM_INFINITY ? "remove" : "set",
		       hard ? " hard" : "");
	return (-1);
    }
    return (0);
}

void
/*ARGSUSED*/
dosuspend(v, t)
    Char **v;
    struct command *t;
{
    int     ctpgrp;

    void    (*old) __P((int));

    if (loginsh)
	stderror(ERR_SUSPLOG);
    untty();

    old = signal(SIGTSTP, SIG_DFL);
    (void) kill(0, SIGTSTP);
    /* the shell stops here */
    (void) signal(SIGTSTP, old);

    if (tpgrp != -1) {
	ctpgrp = tcgetpgrp(FSHTTY);
	while  (ctpgrp != opgrp) {
	    old = signal(SIGTTIN, SIG_DFL);
	    (void) kill(0, SIGTTIN);
	    (void) signal(SIGTTIN, old);
	}
	(void) setpgid(0, shpgrp);
	(void) tcsetpgrp(FSHTTY, shpgrp);
    }
}

/* This is the dreaded EVAL built-in.
 *   If you don't fiddle with file descriptors, and reset didfds,
 *   this command will either ignore redirection inside or outside
 *   its aguments, e.g. eval "date >x"  vs.  eval "date" >x
 *   The stuff here seems to work, but I did it by trial and error rather
 *   than really knowing what was going on.  If tpgrp is zero, we are
 *   probably a background eval, e.g. "eval date &", and we want to
 *   make sure that any processes we start stay in our pgrp.
 *   This is also the case for "time eval date" -- stay in same pgrp.
 *   Otherwise, under stty tostop, processes will stop in the wrong
 *   pgrp, with no way for the shell to get them going again.  -IAN!
 */
static Char **gv = NULL;
void
/*ARGSUSED*/
doeval(v, t)
    Char **v;
    struct command *t;
{
    Char  **oevalvec;
    Char   *oevalp;
    int     odidfds;
    jmp_buf osetexit;
    int     my_reenter;
    Char  **savegv = gv;
    int     saveIN;
    int     saveOUT;
    int     saveERR;
    int     oSHIN;
    int     oSHOUT;
    int     oSHERR;

    UNREGISTER(v);

    oevalvec = evalvec;
    oevalp = evalp;
    odidfds = didfds;
    oSHIN = SHIN;
    oSHOUT = SHOUT;
    oSHERR = SHERR;

    v++;
    if (*v == 0)
	return;
    gflag = 0, tglob(v);
    if (gflag) {
	gv = v = globall(v);
	gargv = 0;
	if (v == 0)
	    stderror(ERR_NOMATCH);
	v = copyblk(v);
    }
    else {
	gv = NULL;
	v = copyblk(v);
	trim(v);
    }

    saveIN = dcopy(SHIN, -1);
    saveOUT = dcopy(SHOUT, -1);
    saveERR = dcopy(SHERR, -1);

    getexit(osetexit);

    if ((my_reenter = setexit()) == 0) {
	evalvec = v;
	evalp = 0;
	SHIN = dcopy(0, -1);
	SHOUT = dcopy(1, -1);
	SHERR = dcopy(2, -1);
	didfds = 0;
	process(0);
    }

    evalvec = oevalvec;
    evalp = oevalp;
    doneinp = 0;
    didfds = odidfds;
    (void) close(SHIN);
    (void) close(SHOUT);
    (void) close(SHERR);
    SHIN = dmove(saveIN, oSHIN);
    SHOUT = dmove(saveOUT, oSHOUT);
    SHERR = dmove(saveERR, oSHERR);
    if (gv)
	blkfree(gv), gv = NULL;
    resexit(osetexit);
    gv = savegv;
    if (my_reenter)
	stderror(ERR_SILENT);
}

void
/*ARGSUSED*/
doprintf(v, t)
    Char **v;
    struct command *t;
{
    char **c;
    extern int progprintf __P((int, char **));
    int ret;

    ret = progprintf(blklen(v), c = short2blk(v));
    (void) fflush(cshout);
    (void) fflush(csherr);

    blkfree((Char **) c);
    if (ret)
	stderror(ERR_SILENT);
}
