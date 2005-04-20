/* $Header: /src/pub/tcsh/sh.func.c,v 3.111 2004/05/13 15:23:39 christos Exp $ */
/*
 * sh.func.c: csh builtin functions
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

RCSID("$Id: sh.func.c,v 3.111 2004/05/13 15:23:39 christos Exp $")

#include "ed.h"
#include "tw.h"
#include "tc.h"
#ifdef WINNT_NATIVE
#include "nt.const.h"
#endif /* WINNT_NATIVE */

/*
 * C shell
 */
extern int just_signaled;
extern char **environ;

extern bool MapsAreInited;
extern bool NLSMapsAreInited;
extern bool NoNLSRebind;
extern bool GotTermCaps;

static int zlast = -1;

static	void	islogin		__P((void));
static	void	preread		__P((void));
static	void	doagain		__P((void));
static  char   *isrchx		__P((int));
static	void	search		__P((int, int, Char *));
static	int	getword		__P((Char *));
static	void	toend		__P((void));
static	void	xecho		__P((int, Char **));
static	bool	islocale_var	__P((Char *));
static	void	wpfree		__P((struct whyle *));

struct biltins *
isbfunc(t)
    struct command *t;
{
    register Char *cp = t->t_dcom[0];
    register struct biltins *bp, *bp1, *bp2;
    static struct biltins label = {"", dozip, 0, 0};
    static struct biltins foregnd = {"%job", dofg1, 0, 0};
    static struct biltins backgnd = {"%job &", dobg1, 0, 0};

    /*
     * We never match a builtin that has quoted the first
     * character; this has been the traditional way to escape 
     * builtin commands.
     */
    if (*cp & QUOTE)
	return NULL;

    if (*cp != ':' && lastchr(cp) == ':') {
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
#ifdef WARP
    /*
     * This is a perhaps kludgy way to determine if the warp builtin is to be
     * acknowledged or not.  If checkwarp() fails, then we are to assume that
     * the warp command is invalid, and carry on as we would handle any other
     * non-builtin command.         -- JDK 2/4/88
     */
    if (eq(STRwarp, cp) && !checkwarp()) {
	return (0);		/* this builtin disabled */
    }
#endif /* WARP */
    /*
     * Binary search Bp1 is the beginning of the current search range. Bp2 is
     * one past the end.
     */
    for (bp1 = bfunc, bp2 = bfunc + nbfunc; bp1 < bp2;) {
	int i;

	bp = bp1 + ((bp2 - bp1) >> 1);
	if ((i = ((char) *cp) - *bp->bname) == 0 &&
	    (i = StrQcmp(cp, str2short(bp->bname))) == 0)
	    return bp;
	if (i < 0)
	    bp2 = bp;
	else
	    bp1 = bp + 1;
    }
#ifdef WINNT_NATIVE
    return nt_check_additional_builtins(cp);
#endif /*WINNT_NATIVE*/
    return (0);
}

void
func(t, bp)
    register struct command *t;
    register struct biltins *bp;
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

/*ARGSUSED*/
void
doonintr(v, c)
    Char  **v;
    struct command *c;
{
    register Char *cp;
    register Char *vv = v[1];

    USE(c);
    if (parintr == SIG_IGN)
	return;
    if (setintr && intty)
	stderror(ERR_NAME | ERR_TERMINAL);
    cp = gointr;
    gointr = 0;
    xfree((ptr_t) cp);
    if (vv == 0) {
#ifdef BSDSIGS
	if (setintr) {
	    (void) sigblock(sigmask(SIGINT));
	    (void) signal(SIGINT, pintr);
	}
	else 
	    (void) signal(SIGINT, SIG_DFL);
#else /* !BSDSIGS */
	if (setintr) {
	    (void) sighold(SIGINT);
	    (void) sigset(SIGINT, pintr);
	}
	else
	    (void) sigset(SIGINT, SIG_DFL);
#endif /* BSDSIGS */
	gointr = 0;
    }
    else if (eq((vv = strip(vv)), STRminus)) {
#ifdef BSDSIGS
	(void) signal(SIGINT, SIG_IGN);
#else /* !BSDSIGS */
	(void) sigset(SIGINT, SIG_IGN);
#endif /* BSDSIGS */
	gointr = Strsave(STRminus);
    }
    else {
	gointr = Strsave(vv);
#ifdef BSDSIGS
	(void) signal(SIGINT, pintr);
#else /* !BSDSIGS */
	(void) sigset(SIGINT, pintr);
#endif /* BSDSIGS */
    }
}

/*ARGSUSED*/
void
donohup(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
    if (intty)
	stderror(ERR_NAME | ERR_TERMINAL);
    if (setintr == 0) {
	(void) signal(SIGHUP, SIG_IGN);
#ifdef CC
	submit(getpid());
#endif /* CC */
    }
}

/*ARGSUSED*/
void
dohup(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
    if (intty)
	stderror(ERR_NAME | ERR_TERMINAL);
    if (setintr == 0)
	(void) signal(SIGHUP, SIG_DFL);
}


/*ARGSUSED*/
void
dozip(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
}

/*ARGSUSED*/
void
dofiletest(v, c)
    Char **v;
    struct command *c;
{
    Char **fileptr, *ftest, *res;

    if (*(ftest = *++v) != '-')
	stderror(ERR_NAME | ERR_FILEINQ);
    ++v;

    gflag = 0;
    tglob(v);
    if (gflag) {
	v = globall(v);
	if (v == 0)
	    stderror(ERR_NAME | ERR_NOMATCH);
    }
    else
	v = gargv = saveblk(v);
    trim(v);

    while (*(fileptr = v++) != '\0') {
	xprintf("%S", res = filetest(ftest, &fileptr, 0));
	xfree((ptr_t) res);
	if (*v)
	    xprintf(" ");
    }
    xprintf("\n");

    if (gargv) {
	blkfree(gargv);
	gargv = 0;
    }
}

void
prvars()
{
    plist(&shvhed, VAR_ALL);
}

/*ARGSUSED*/
void
doalias(v, c)
    register Char **v;
    struct command *c;
{
    register struct varent *vp;
    register Char *p;

    USE(c);
    v++;
    p = *v++;
    if (p == 0)
	plist(&aliases, VAR_ALL);
    else if (*v == 0) {
	vp = adrof1(strip(p), &aliases);
	if (vp && vp->vec)
	    blkpr(vp->vec), xputchar('\n');
    }
    else {
	if (eq(p, STRalias) || eq(p, STRunalias)) {
	    setname(short2str(p));
	    stderror(ERR_NAME | ERR_DANGER);
	}
	set1(strip(p), saveblk(v), &aliases, VAR_READWRITE);
	tw_cmd_free();
    }
}

/*ARGSUSED*/
void
unalias(v, c)
    Char  **v;
    struct command *c;
{
    USE(c);
    unset1(v, &aliases);
    tw_cmd_free();
}

/*ARGSUSED*/
void
dologout(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
    islogin();
    goodbye(NULL, NULL);
}

/*ARGSUSED*/
void
dologin(v, c)
    Char  **v;
    struct command *c;
{
#ifdef WINNT_NATIVE
    USE(c);
    USE(v);
#else /* !WINNT_NATIVE */
    char **p = short2blk(v);
    USE(c);
    islogin();
    rechist(NULL, adrof(STRsavehist) != NULL);
    (void) signal(SIGTERM, parterm);
    (void) execv(_PATH_BIN_LOGIN, p);
    (void) execv(_PATH_USRBIN_LOGIN, p);
    blkfree((Char **) p);
    untty();
    xexit(1);
#endif /* !WINNT_NATIVE */
}


#ifdef NEWGRP
/*ARGSUSED*/
void
donewgrp(v, c)
    Char  **v;
    struct command *c;
{
    char **p;
    if (chkstop == 0 && setintr)
	panystop(0);
    (void) signal(SIGTERM, parterm);
    p = short2blk(v);
    /*
     * From Beto Appleton (beto@aixwiz.austin.ibm.com)
     * Newgrp can take 2 arguments...
     */
    (void) execv(_PATH_BIN_NEWGRP, p);
    (void) execv(_PATH_USRBIN_NEWGRP, p);
    blkfree((Char **) p);
    untty();
    xexit(1);
}
#endif /* NEWGRP */

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
    register int i;
    register Char **vv;

    v++;
    i = expr(&v);
    vv = v;
    if (*vv == NULL)
	stderror(ERR_NAME | ERR_EMPTYIF);
    if (eq(*vv, STRthen)) {
	if (*++vv)
	    stderror(ERR_NAME | ERR_IMPRTHEN);
	setname(short2str(STRthen));
	/*
	 * If expression was zero, then scan to else , otherwise just fall into
	 * following code.
	 */
	if (!i)
	    search(TC_IF, 0, NULL);
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
void
reexecute(kp)
    register struct command *kp;
{
    kp->t_dflg &= F_SAVE;
    kp->t_dflg |= F_REPEAT;
    /*
     * If tty is still ours to arbitrate, arbitrate it; otherwise dont even set
     * pgrp's as the jobs would then have no way to get the tty (we can't give
     * it to them, and our parent wouldn't know their pgrp, etc.
     */
    execute(kp, (tpgrp > 0 ? tpgrp : -1), NULL, NULL, TRUE);
}

/*ARGSUSED*/
void
doelse (v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
    search(TC_ELSE, 0, NULL);
}

/*ARGSUSED*/
void
dogoto(v, c)
    Char  **v;
    struct command *c;
{
    Char   *lp;

    USE(c);
    gotolab(lp = globone(v[1], G_ERROR));
    xfree((ptr_t) lp);
}

void
gotolab(lab)
    Char *lab;
{
    register struct whyle *wp;
    /*
     * While we still can, locate any unknown ends of existing loops. This
     * obscure code is the WORST result of the fact that we don't really parse.
     */
    zlast = TC_GOTO;
    for (wp = whyles; wp; wp = wp->w_next)
	if (wp->w_end.type == TCSH_F_SEEK && wp->w_end.f_seek == 0) {
	    search(TC_BREAK, 0, NULL);
	    btell(&wp->w_end);
	}
	else {
	    bseek(&wp->w_end);
	}
    search(TC_GOTO, 0, lab);
    /*
     * Eliminate loops which were exited.
     */
    wfree();
}

/*ARGSUSED*/
void
doswitch(v, c)
    register Char **v;
    struct command *c;
{
    register Char *cp, *lp;

    USE(c);
    v++;
    if (!*v || *(*v++) != '(')
	stderror(ERR_SYNTAX);
    cp = **v == ')' ? STRNULL : *v++;
    if (*(*v++) != ')')
	v--;
    if (*v)
	stderror(ERR_SYNTAX);
    search(TC_SWITCH, 0, lp = globone(cp, G_ERROR));
    xfree((ptr_t) lp);
}

/*ARGSUSED*/
void
dobreak(v, c)
    Char **v;
    struct command *c;
{
    USE(v);
    USE(c);
    if (whyles)
	toend();
    else
	stderror(ERR_NAME | ERR_NOTWHILE);
}

/*ARGSUSED*/
void
doexit(v, c)
    Char  **v;
    struct command *c;
{
    USE(c);

    if (chkstop == 0 && (intty || intact) && evalvec == 0)
	panystop(0);
    /*
     * Don't DEMAND parentheses here either.
     */
    v++;
    if (*v) {
	set(STRstatus, putn(expr(&v)), VAR_READWRITE);
	if (*v)
	    stderror(ERR_NAME | ERR_EXPRESSION);
    }
    btoeof();
#if 0
    if (intty)
#endif
    /* Always close, why only on ttys? */
	(void) close(SHIN);
}

/*ARGSUSED*/
void
doforeach(v, c)
    register Char **v;
    struct command *c;
{
    register Char *cp, *sp;
    register struct whyle *nwp;

    USE(c);
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
    if (gflag) {
	v = globall(v);
	if (v == 0)
	    stderror(ERR_NAME | ERR_NOMATCH);
    }
    else {
	v = gargv = saveblk(v);
	trim(v);
    }
    nwp = (struct whyle *) xcalloc(1, sizeof *nwp);
    nwp->w_fe = nwp->w_fe0 = v;
    gargv = 0;
    btell(&nwp->w_start);
    nwp->w_fename = Strsave(cp);
    nwp->w_next = whyles;
    nwp->w_end.type = TCSH_F_SEEK;
    whyles = nwp;
    /*
     * Pre-read the loop so as to be more comprehensible to a terminal user.
     */
    zlast = TC_FOREACH;
    if (intty)
	preread();
    doagain();
}

/*ARGSUSED*/
void
dowhile(v, c)
    Char  **v;
    struct command *c;
{
    register int status;
    register bool again = whyles != 0 && 
			  SEEKEQ(&whyles->w_start, &lineloc) &&
			  whyles->w_fename == 0;

    USE(c);
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
	register struct whyle *nwp =
	(struct whyle *) xcalloc(1, sizeof(*nwp));

	nwp->w_start = lineloc;
	nwp->w_end.type = TCSH_F_SEEK;
	nwp->w_end.f_seek = 0;
	nwp->w_next = whyles;
	whyles = nwp;
	zlast = TC_WHILE;
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
    whyles->w_end.type = TCSH_I_SEEK;
    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sigrelse (SIGINT);
#endif /* BSDSIGS */
    search(TC_BREAK, 0, NULL);		/* read the expression in */
    if (setintr)
#ifdef BSDSIGS
	(void) sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sighold(SIGINT);
#endif /* BSDSIGS */
    btell(&whyles->w_end);
}

/*ARGSUSED*/
void
doend(v, c)
    Char **v;
    struct command *c;
{
    USE(v);
    USE(c);
    if (!whyles)
	stderror(ERR_NAME | ERR_NOTWHILE);
    btell(&whyles->w_end);
    doagain();
}

/*ARGSUSED*/
void
docontin(v, c)
    Char **v;
    struct command *c;
{
    USE(v);
    USE(c);
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
    set(whyles->w_fename, quote(Strsave(*whyles->w_fe++)), VAR_READWRITE);
    bseek(&whyles->w_start);
}

void
dorepeat(v, kp)
    Char  **v;
    struct command *kp;
{
    int i = 1;

#ifdef BSDSIGS
    register sigmask_t omask = 0;
#endif /* BSDSIGS */

    do {
	i *= getn(v[1]);
	lshift(v, 2);
    } while (v[0] != NULL && Strcmp(v[0], STRrepeat) == 0);

    if (setintr)
#ifdef BSDSIGS
	omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
#else /* !BSDSIGS */
	(void) sighold(SIGINT);
#endif /* BSDSIGS */
    while (i > 0) {
	if (setintr)
#ifdef BSDSIGS
	    (void) sigsetmask(omask);
#else /* !BSDSIGS */
	    (void) sigrelse (SIGINT);
#endif /* BSDSIGS */
	reexecute(kp);
	--i;
    }
    donefds();
    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(omask);
#else /* !BSDSIGS */
	(void) sigrelse (SIGINT);
#endif /* BSDSIGS */
}

/*ARGSUSED*/
void
doswbrk(v, c)
    Char **v;
    struct command *c;
{
    USE(v);
    USE(c);
    search(TC_BRKSW, 0, NULL);
}

int
srchx(cp)
    Char *cp;
{
    struct srch *sp, *sp1, *sp2;
    int i;

    /*
     * Ignore keywords inside heredocs
     */
    if (inheredoc)
	return -1;

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

static char *
isrchx(n)
    register int n;
{
    register struct srch *sp, *sp2;

    for (sp = srchn, sp2 = srchn + nsrchn; sp < sp2; sp++)
	if (sp->s_value == n)
	    return (sp->s_name);
    return ("");
}


static Char Stype;
static Char *Sgoal;

static void
search(type, level, goal)
    int     type;
    register int level;
    Char   *goal;
{
    Char    wordbuf[BUFSIZE];
    register Char *aword = wordbuf;
    register Char *cp;
    struct whyle *wp;
    int wlevel = 0;

    Stype = (Char) type;
    Sgoal = goal;
    if (type == TC_GOTO) {
	struct Ain a;
	a.type = TCSH_F_SEEK;
	a.f_seek = 0;
	bseek(&a);
    }
    do {
	if (intty && fseekp == feobp && aret == TCSH_F_SEEK)
	    printprompt(1, isrchx(type == TC_BREAK ? zlast : type));
	/* xprintf("? "), flush(); */
	aword[0] = 0;
	(void) getword(aword);
	switch (srchx(aword)) {

	case TC_ELSE:
	    if (level == 0 && type == TC_IF)
		return;
	    break;

	case TC_IF:
	    while (getword(aword))
		continue;
	    if ((type == TC_IF || type == TC_ELSE) &&
		eq(aword, STRthen))
		level++;
	    break;

	case TC_ENDIF:
	    if (type == TC_IF || type == TC_ELSE)
		level--;
	    break;

	case TC_FOREACH:
	case TC_WHILE:
	    wlevel++;
	    if (type == TC_BREAK)
		level++;
	    break;

	case TC_END:
	    if (type == TC_BRKSW) {
		if (wlevel == 0) {
		    wp = whyles;
		    if (wp) {
			    whyles = wp->w_next;
			    wpfree(wp);
		    }
		}
	    }
	    if (type == TC_BREAK)
		level--;
	    wlevel--;
	    break;

	case TC_SWITCH:
	    if (type == TC_SWITCH || type == TC_BRKSW)
		level++;
	    break;

	case TC_ENDSW:
	    if (type == TC_SWITCH || type == TC_BRKSW)
		level--;
	    break;

	case TC_LABEL:
	    if (type == TC_GOTO && getword(aword) && eq(aword, goal))
		level = -1;
	    break;

	default:
	    if (type != TC_GOTO && (type != TC_SWITCH || level != 0))
		break;
	    if (lastchr(aword) != ':')
		break;
	    aword[Strlen(aword) - 1] = 0;
	    if ((type == TC_GOTO && eq(aword, goal)) ||
		(type == TC_SWITCH && eq(aword, STRdefault)))
		level = -1;
	    break;

	case TC_CASE:
	    if (type != TC_SWITCH || level != 0)
		break;
	    (void) getword(aword);
	    if (lastchr(aword) == ':')
		aword[Strlen(aword) - 1] = 0;
	    cp = strip(Dfix1(aword));
	    if (Gmatch(goal, cp))
		level = -1;
	    xfree((ptr_t) cp);
	    break;

	case TC_DEFAULT:
	    if (type == TC_SWITCH && level == 0)
		level = -1;
	    break;
	}
	(void) getword(NULL);
    } while (level >= 0);
}

static int
getword(wp)
    register Char *wp;
{
    int found = 0, first;
    int c, d;

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
	first = 1;
	do {
	    c = readc(1);
	    if (c == '\\' && (c = readc(1)) == '\n')
		c = ' ';
	    if (c == '\'' || c == '"') {
		if (d == 0)
		    d = c;
		else if (d == c)
		    d = 0;
	    }
	    if (c < 0)
		goto past;
	    if (wp) {
		*wp++ = (Char) c;
		*wp = '\0';
	    }
	    if (!first && !d && c == '(') {
		if (wp) {
		    unreadc(c);
		    *--wp = '\0';
		    return found;
		}
		else 
		    break;
	    }
	    first = 0;
	} while ((d || (c != ' ' && c != '\t')) && c != '\n');
    } while (wp == 0);

    unreadc(c);
    if (found)
	*--wp = '\0';

    return (found);

past:
    switch (Stype) {

    case TC_IF:
	stderror(ERR_NAME | ERR_NOTFOUND, "then/endif");
	break;

    case TC_ELSE:
	stderror(ERR_NAME | ERR_NOTFOUND, "endif");
	break;

    case TC_BRKSW:
    case TC_SWITCH:
	stderror(ERR_NAME | ERR_NOTFOUND, "endsw");
	break;

    case TC_BREAK:
	stderror(ERR_NAME | ERR_NOTFOUND, "end");
	break;

    case TC_GOTO:
	setname(short2str(Sgoal));
	stderror(ERR_NAME | ERR_NOTFOUND, "label");
	break;

    default:
	break;
    }
    /* NOTREACHED */
    return (0);
}

static void
toend()
{
    if (whyles->w_end.type == TCSH_F_SEEK && whyles->w_end.f_seek == 0) {
	search(TC_BREAK, 0, NULL);
	btell(&whyles->w_end);
	whyles->w_end.f_seek--;
    }
    else {
	bseek(&whyles->w_end);
    }
    wfree();
}

static void
wpfree(wp)
    struct whyle *wp;
{
	if (wp->w_fe0)
	    blkfree(wp->w_fe0);
	if (wp->w_fename)
	    xfree((ptr_t) wp->w_fename);
	xfree((ptr_t) wp);
}

void
wfree()
{
    struct Ain    o;
    struct whyle *nwp;
#ifdef lint
    nwp = NULL;	/* sun lint is dumb! */
#endif

#ifdef FDEBUG
    static char foo[] = "IAFE";
#endif /* FDEBUG */

    btell(&o);

#ifdef FDEBUG
    xprintf("o->type %c o->a_seek %d o->f_seek %d\n", 
	    foo[o.type + 1], o.a_seek, o.f_seek);
#endif /* FDEBUG */

    for (; whyles; whyles = nwp) {
	register struct whyle *wp = whyles;
	nwp = wp->w_next;

#ifdef FDEBUG
	xprintf("start->type %c start->a_seek %d start->f_seek %d\n", 
		foo[wp->w_start.type+1], 
		wp->w_start.a_seek, wp->w_start.f_seek);
	xprintf("end->type %c end->a_seek %d end->f_seek %d\n", 
		foo[wp->w_end.type + 1], wp->w_end.a_seek, wp->w_end.f_seek);
#endif /* FDEBUG */

	/*
	 * XXX: We free loops that have different seek types.
	 */
	if (wp->w_end.type != TCSH_I_SEEK && wp->w_start.type == wp->w_end.type &&
	    wp->w_start.type == o.type) {
	    if (wp->w_end.type == TCSH_F_SEEK) {
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

	wpfree(wp);
    }
}

/*ARGSUSED*/
void
doecho(v, c)
    Char  **v;
    struct command *c;
{
    USE(c);
    xecho(' ', v);
}

/*ARGSUSED*/
void
doglob(v, c)
    Char  **v;
    struct command *c;
{
    USE(c);
    xecho(0, v);
    flush();
}

static void
xecho(sep, v)
    int    sep;
    register Char **v;
{
    register Char *cp;
    int     nonl = 0;
#ifdef ECHO_STYLE
    int	    echo_style = ECHO_STYLE;
#else /* !ECHO_STYLE */
# if SYSVREL > 0
    int	    echo_style = SYSV_ECHO;
# else /* SYSVREL == 0 */
    int	    echo_style = BSD_ECHO;
# endif /* SYSVREL */
#endif /* ECHO_STYLE */
    struct varent *vp;

    if ((vp = adrof(STRecho_style)) != NULL && vp->vec != NULL &&
	vp->vec[0] != NULL) {
	if (Strcmp(vp->vec[0], STRbsd) == 0)
	    echo_style = BSD_ECHO;
	else if (Strcmp(vp->vec[0], STRsysv) == 0)
	    echo_style = SYSV_ECHO;
	else if (Strcmp(vp->vec[0], STRboth) == 0)
	    echo_style = BOTH_ECHO;
	else if (Strcmp(vp->vec[0], STRnone) == 0)
	    echo_style = NONE_ECHO;
    }

    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sigrelse (SIGINT);
#endif /* BSDSIGS */
    v++;
    if (*v == 0)
	goto done;
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

    if ((echo_style & BSD_ECHO) != 0 && sep == ' ' && *v && eq(*v, STRmn))
	nonl++, v++;

    while ((cp = *v++) != 0) {
	register int c;

	while ((c = *cp++) != 0) {
	    if ((echo_style & SYSV_ECHO) != 0 && c == '\\') {
		switch (c = *cp++) {
		case 'a':
		    c = '\a';
		    break;
		case 'b':
		    c = '\b';
		    break;
		case 'c':
		    nonl = 1;
		    goto done;
		case 'e':
#if 0			/* Windows does not understand \e */
		    c = '\e';
#else
		    c = '\033';
#endif
		    break;
		case 'f':
		    c = '\f';
		    break;
		case 'n':
		    c = '\n';
		    break;
		case 'r':
		    c = '\r';
		    break;
		case 't':
		    c = '\t';
		    break;
		case 'v':
		    c = '\v';
		    break;
		case '\\':
		    c = '\\';
		    break;
		case '0':
		    c = 0;
		    if (*cp >= '0' && *cp < '8')
			c = c * 8 + *cp++ - '0';
		    if (*cp >= '0' && *cp < '8')
			c = c * 8 + *cp++ - '0';
		    if (*cp >= '0' && *cp < '8')
			c = c * 8 + *cp++ - '0';
		    break;
		case '\0':
		    c = '\\';
		    cp--;
		    break;
		default:
		    xputchar('\\' | QUOTE);
		    break;
		}
	    }
	    xputchar(c | QUOTE);

	}
	if (*v)
	    xputchar(sep | QUOTE);
    }
done:
    if (sep && nonl == 0)
	xputchar('\n');
    else
	flush();
    if (setintr)
#ifdef BSDSIGS
	(void) sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (gargv)
	blkfree(gargv), gargv = 0;
}

/* check whether an environment variable should invoke 'set_locale()' */
static bool
islocale_var(var)
    Char *var;
{
    static Char *locale_vars[] = {
	STRLANG,	STRLC_ALL, 	STRLC_CTYPE,	STRLC_NUMERIC,
	STRLC_TIME,	STRLC_COLLATE,	STRLC_MESSAGES,	STRLC_MONETARY, 0
    };
    register Char **v;

    for (v = locale_vars; *v; ++v)
	if (eq(var, *v))
	    return 1;
    return 0;
}

/*ARGSUSED*/
void
doprintenv(v, c)
    register Char **v;
    struct command *c;
{
    Char   *e;
    extern bool output_raw;
    extern bool xlate_cr;

    USE(c);
    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else /* !BSDSIGS */
	(void) sigrelse (SIGINT);
#endif /* BSDSIGS */

    v++;
    if (*v == 0) {
	register Char **ep;

	xlate_cr = 1;
	for (ep = STR_environ; *ep; ep++)
	    xprintf("%S\n", *ep);
	xlate_cr = 0;
    }
    else if ((e = tgetenv(*v)) != NULL) {
	output_raw = 1;
	xprintf("%S\n", e);
	output_raw = 0;
    }
    else
	set(STRstatus, Strsave(STR1), VAR_READWRITE);
}

/* from "Karl Berry." <karl%mote.umb.edu@relay.cs.net> -- for NeXT things
   (and anything else with a modern compiler) */

/*ARGSUSED*/
void
dosetenv(v, c)
    register Char **v;
    struct command *c;
{
    Char   *vp, *lp;

    USE(c);
    if (*++v == 0) {
	doprintenv(--v, 0);
	return;
    }

    vp = *v++;

    lp = vp;
 
    for (; *lp != '\0' ; lp++) {
	if (*lp == '=')
	    stderror(ERR_NAME | ERR_SYNTAX);
    }
    if ((lp = *v++) == 0)
	lp = STRNULL;

    tsetenv(vp, lp = globone(lp, G_APPEND));
    if (eq(vp, STRKPATH)) {
	importpath(lp);
	dohash(NULL, NULL);
	xfree((ptr_t) lp);
	return;
    }

#ifdef apollo
    if (eq(vp, STRSYSTYPE)) {
	dohash(NULL, NULL);
	xfree((ptr_t) lp);
	return;
    }
#endif /* apollo */

    /* dspkanji/dspmbyte autosetting */
    /* PATCH IDEA FROM Issei.Suzuki VERY THANKS */
#if defined(DSPMBYTE)
    if(eq(vp, STRLANG) && !adrof(CHECK_MBYTEVAR)) {
	autoset_dspmbyte(lp);
    }
#endif

    if (islocale_var(vp)) {
#ifdef NLS
	int     k;

# ifdef SETLOCALEBUG
	dont_free = 1;
# endif /* SETLOCALEBUG */
	(void) setlocale(LC_ALL, "");
# ifdef LC_COLLATE
	(void) setlocale(LC_COLLATE, "");
# endif
# ifdef NLS_CATALOGS
#  ifdef LC_MESSAGES
	(void) setlocale(LC_MESSAGES, "");
#  endif /* LC_MESSAGES */
	(void) catclose(catd);
	nlsinit();
# endif /* NLS_CATALOGS */
# ifdef LC_CTYPE
	(void) setlocale(LC_CTYPE, ""); /* for iscntrl */
# endif /* LC_CTYPE */
# ifdef SETLOCALEBUG
	dont_free = 0;
# endif /* SETLOCALEBUG */
# ifdef STRCOLLBUG
	fix_strcoll_bug();
# endif /* STRCOLLBUG */
	tw_cmd_free();	/* since the collation sequence has changed */
	for (k = 0200; k <= 0377 && !Isprint(k); k++)
	    continue;
	AsciiOnly = k > 0377;
#else /* !NLS */
	AsciiOnly = 0;
#endif /* NLS */
	NLSMapsAreInited = 0;
	ed_Init();
	if (MapsAreInited && !NLSMapsAreInited)
	    ed_InitNLSMaps();
	xfree((ptr_t) lp);
	return;
    }

#ifdef NLS_CATALOGS
    if (eq(vp, STRNLSPATH)) {
	(void) catclose(catd);
	nlsinit();
    }
#endif

    if (eq(vp, STRNOREBIND)) {
	NoNLSRebind = 1;
	MapsAreInited = 0;
	NLSMapsAreInited = 0;
	ed_InitMaps();
	xfree((ptr_t) lp);
	return;
    }
#ifdef WINNT_NATIVE
    if (eq(vp, STRtcshlang)) {
	nlsinit();
	xfree((ptr_t) lp);
	return;
    }
#endif /* WINNT_NATIVE */
    if (eq(vp, STRKTERM)) {
	char *t;
	set(STRterm, quote(lp), VAR_READWRITE);	/* lp memory used here */
	t = short2str(lp);
	if (noediting && strcmp(t, "unknown") != 0 && strcmp(t,"dumb") != 0) {
	    editing = 1;
	    noediting = 0;
	    set(STRedit, Strsave(STRNULL), VAR_READWRITE);
	}
	GotTermCaps = 0;
	ed_Init();
	return;
    }

    if (eq(vp, STRKHOME)) {
	/*
	 * convert to canonical pathname (possibly resolving symlinks)
	 */
	lp = dcanon(lp, lp);
	set(STRhome, quote(lp), VAR_READWRITE);	/* cp memory used here */

	/* fix directory stack for new tilde home */
	dtilde();
	return;
    }

    if (eq(vp, STRKSHLVL)) {
	/* lp memory used here */
	set(STRshlvl, quote(lp), VAR_READWRITE);
	return;
    }

    if (eq(vp, STRKUSER)) {
	set(STRuser, quote(lp), VAR_READWRITE);	/* lp memory used here */
	return;
    }

    if (eq(vp, STRKGROUP)) {
	set(STRgroup, quote(lp), VAR_READWRITE);	/* lp memory used here */
	return;
    }

#ifdef COLOR_LS_F
    if (eq(vp, STRLS_COLORS)) {
        parseLS_COLORS(lp);
	return;
    }
#endif /* COLOR_LS_F */

#ifdef SIG_WINDOW
    /*
     * Load/Update $LINES $COLUMNS
     */
    if ((eq(lp, STRNULL) && (eq(vp, STRLINES) || eq(vp, STRCOLUMNS))) ||
	eq(vp, STRTERMCAP)) {
	xfree((ptr_t) lp);
	check_window_size(1);
	return;
    }

    /*
     * Change the size to the one directed by $LINES and $COLUMNS
     */
    if (eq(vp, STRLINES) || eq(vp, STRCOLUMNS)) {
#if 0
	GotTermCaps = 0;
#endif
	xfree((ptr_t) lp);
	ed_Init();
	return;
    }
#endif /* SIG_WINDOW */
    xfree((ptr_t) lp);
}

/*ARGSUSED*/
void
dounsetenv(v, c)
    register Char **v;
    struct command *c;
{
    Char  **ep, *p, *n;
    int     i, maxi;
    static Char *name = NULL;

    USE(c);
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

    name = (Char *) xmalloc((size_t) ((maxi + 1) * sizeof(Char)));

    while (++v && *v) 
	for (maxi = 1; maxi;)
	    for (maxi = 0, ep = STR_environ; *ep; ep++) {
		for (n = name, p = *ep; *p && *p != '='; *n++ = *p++)
		    continue;
		*n = '\0';
		if (!Gmatch(name, *v))
		    continue;
		maxi = 1;

		/* Unset the name. This wasn't being done until
		 * later but most of the stuff following won't
		 * work (particularly the setlocale() and getenv()
		 * stuff) as intended until the name is actually
		 * removed. (sg)
		 */
		Unsetenv(name);

		if (eq(name, STRNOREBIND)) {
		    NoNLSRebind = 0;
		    MapsAreInited = 0;
		    NLSMapsAreInited = 0;
		    ed_InitMaps();
		}
#ifdef apollo
		else if (eq(name, STRSYSTYPE))
		    dohash(NULL, NULL);
#endif /* apollo */
		else if (islocale_var(name)) {
#ifdef NLS
		    int     k;

# ifdef SETLOCALEBUG
		    dont_free = 1;
# endif /* SETLOCALEBUG */
		    (void) setlocale(LC_ALL, "");
# ifdef LC_COLLATE
		    (void) setlocale(LC_COLLATE, "");
# endif
# ifdef NLS_CATALOGS
#  ifdef LC_MESSAGES
		    (void) setlocale(LC_MESSAGES, "");
#  endif /* LC_MESSAGES */
		    (void) catclose(catd);
		    nlsinit();
# endif /* NLS_CATALOGS */
# ifdef LC_CTYPE
	(void) setlocale(LC_CTYPE, ""); /* for iscntrl */
# endif /* LC_CTYPE */
# ifdef SETLOCALEBUG
		    dont_free = 0;
# endif /* SETLOCALEBUG */
# ifdef STRCOLLBUG
		    fix_strcoll_bug();
# endif /* STRCOLLBUG */
		    tw_cmd_free();/* since the collation sequence has changed */
		    for (k = 0200; k <= 0377 && !Isprint(k); k++)
			continue;
		    AsciiOnly = k > 0377;
#else /* !NLS */
		    AsciiOnly = getenv("LANG") == NULL &&
			getenv("LC_CTYPE") == NULL;
#endif /* NLS */
		    NLSMapsAreInited = 0;
		    ed_Init();
		    if (MapsAreInited && !NLSMapsAreInited)
			ed_InitNLSMaps();

		}
#ifdef WINNT_NATIVE
		else if (eq(name,(STRtcshlang))) {
		    nls_dll_unload();
		    nlsinit();
		}
#endif /* WINNT_NATIVE */
#ifdef COLOR_LS_F
		else if (eq(name, STRLS_COLORS))
		    parseLS_COLORS(n);
#endif /* COLOR_LS_F */
#ifdef NLS_CATALOGS
		else if (eq(name, STRNLSPATH)) {
		    (void) catclose(catd);
		    nlsinit();
		}
#endif
		/*
		 * start again cause the environment changes
		 */
		break;
	    }
    xfree((ptr_t) name); name = NULL;
}

void
tsetenv(name, val)
    Char   *name, *val;
{
#ifdef SETENV_IN_LIB
/*
 * XXX: This does not work right, since tcsh cannot track changes to
 * the environment this way. (the builtin setenv without arguments does
 * not print the right stuff neither does unsetenv). This was for Mach,
 * it is not needed anymore.
 */
#undef setenv
    char    nameBuf[BUFSIZE];
    char   *cname = short2str(name);

    if (cname == NULL)
	return;
    (void) strcpy(nameBuf, cname);
    setenv(nameBuf, short2str(val), 1);
#else /* !SETENV_IN_LIB */
    register Char **ep = STR_environ;
    register Char *cp, *dp;
    Char   *blk[2];
    Char  **oep = ep;

#ifdef WINNT_NATIVE
	nt_set_env(name,val);
#endif /* WINNT_NATIVE */
    for (; *ep; ep++) {
#ifdef WINNT_NATIVE
	for (cp = name, dp = *ep; *cp && Tolower(*cp & TRIM) == Tolower(*dp);
				cp++, dp++)
#else
	for (cp = name, dp = *ep; *cp && (*cp & TRIM) == *dp; cp++, dp++)
#endif /* WINNT_NATIVE */
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
#endif /* SETENV_IN_LIB */
}

void
Unsetenv(name)
    Char   *name;
{
    register Char **ep = STR_environ;
    register Char *cp, *dp;
    Char **oep = ep;

#ifdef WINNT_NATIVE
	nt_set_env(name,NULL);
#endif /*WINNT_NATIVE */
    for (; *ep; ep++) {
	for (cp = name, dp = *ep; *cp && *cp == *dp; cp++, dp++)
	    continue;
	if (*cp != 0 || *dp != '=')
	    continue;
	cp = *ep;
	*ep = 0;
	STR_environ = blkspl(STR_environ, ep + 1);
	blkfree((Char **) environ);
	environ = short2blk(STR_environ);
	*ep = cp;
	xfree((ptr_t) cp);
	xfree((ptr_t) oep);
	return;
    }
}

/*ARGSUSED*/
void
doumask(v, c)
    register Char **v;
    struct command *c;
{
    register Char *cp = v[1];
    register int i;

    USE(c);
    if (cp == 0) {
	i = (int)umask(0);
	(void) umask(i);
	xprintf("%o\n", i);
	return;
    }
    i = 0;
    while (Isdigit(*cp) && *cp != '8' && *cp != '9')
	i = i * 8 + *cp++ - '0';
    if (*cp || i < 0 || i > 0777)
	stderror(ERR_NAME | ERR_MASK);
    (void) umask(i);
}

#ifndef HAVENOLIMIT
# ifndef BSDLIMIT
   typedef long RLIM_TYPE;
#  ifndef RLIM_INFINITY
#   if !defined(_MINIX) && !defined(__clipper__) && !defined(_CRAY)
    extern RLIM_TYPE ulimit();
#   endif /* ! _MINIX && !__clipper__ */
#   define RLIM_INFINITY 0x003fffff
#   define RLIMIT_FSIZE 1
#  endif /* RLIM_INFINITY */
#  ifdef aiws
#   define toset(a) (((a) == 3) ? 1004 : (a) + 1)
#   define RLIMIT_DATA	3
#   define RLIMIT_STACK 1005
#  else /* aiws */
#   define toset(a) ((a) + 1)
#  endif /* aiws */
# else /* BSDLIMIT */
#  if (defined(BSD4_4) || defined(__linux__) || (HPUXVERSION >= 1100)) && !defined(__386BSD__)
    typedef rlim_t RLIM_TYPE;
#  else
#   if defined(SOLARIS2) || (defined(sgi) && SYSVREL > 3)
     typedef rlim_t RLIM_TYPE;
#   else
#    if defined(_SX)
      typedef long long RLIM_TYPE;
#    else /* !_SX */
      typedef unsigned long RLIM_TYPE;
#    endif /* _SX */
#   endif /* SOLARIS2 || (sgi && SYSVREL > 3) */
#  endif /* BSD4_4 && !__386BSD__  */
# endif /* BSDLIMIT */

# if (HPUXVERSION > 700) && (HPUXVERSION < 1100) && defined(BSDLIMIT)
/* Yes hpux8.0 has limits but <sys/resource.h> does not make them public */
/* Yes, we could have defined _KERNEL, and -I/etc/conf/h, but is that better? */
#  ifndef RLIMIT_CPU
#   define RLIMIT_CPU		0
#   define RLIMIT_FSIZE		1
#   define RLIMIT_DATA		2
#   define RLIMIT_STACK		3
#   define RLIMIT_CORE		4
#   define RLIMIT_RSS		5
#   define RLIMIT_NOFILE	6
#  endif /* RLIMIT_CPU */
#  ifndef RLIM_INFINITY
#   define RLIM_INFINITY	0x7fffffff
#  endif /* RLIM_INFINITY */
   /*
    * old versions of HP/UX counted limits in 512 bytes
    */
#  ifndef SIGRTMIN
#   define FILESIZE512
#  endif /* SIGRTMIN */
# endif /* (HPUXVERSION > 700) && (HPUXVERSION < 1100) && BSDLIMIT */

# if SYSVREL > 3 && defined(BSDLIMIT) && !defined(_SX)
/* In order to use rusage, we included "/usr/ucbinclude/sys/resource.h" in */
/* sh.h.  However, some SVR4 limits are defined in <sys/resource.h>.  Rather */
/* than include both and get warnings, we define the extra SVR4 limits here. */
/* XXX: I don't understand if RLIMIT_AS is defined, why don't we define */
/* RLIMIT_VMEM based on it? */
#  ifndef RLIMIT_VMEM
#   define RLIMIT_VMEM	6
#  endif
#  ifndef RLIMIT_AS
#   define RLIMIT_AS	RLIMIT_VMEM
#  endif
# endif /* SYSVREL > 3 && BSDLIMIT */

# if defined(__linux__) && defined(RLIMIT_AS) && !defined(RLIMIT_VMEM)
#  define RLIMIT_VMEM	RLIMIT_AS
# endif

struct limits limits[] = 
{
# ifdef RLIMIT_CPU
    { RLIMIT_CPU, 	"cputime",	1,	"seconds"	},
# endif /* RLIMIT_CPU */

# ifdef RLIMIT_FSIZE
#  ifndef aiws
    { RLIMIT_FSIZE, 	"filesize",	1024,	"kbytes"	},
#  else
    { RLIMIT_FSIZE, 	"filesize",	512,	"blocks"	},
#  endif /* aiws */
# endif /* RLIMIT_FSIZE */

# ifdef RLIMIT_DATA
    { RLIMIT_DATA, 	"datasize",	1024,	"kbytes"	},
# endif /* RLIMIT_DATA */

# ifdef RLIMIT_STACK
#  ifndef aiws
    { RLIMIT_STACK, 	"stacksize",	1024,	"kbytes"	},
#  else
    { RLIMIT_STACK, 	"stacksize",	1024 * 1024,	"kbytes"},
#  endif /* aiws */
# endif /* RLIMIT_STACK */

# ifdef RLIMIT_CORE
    { RLIMIT_CORE, 	"coredumpsize",	1024,	"kbytes"	},
# endif /* RLIMIT_CORE */

# ifdef RLIMIT_RSS
    { RLIMIT_RSS, 	"memoryuse",	1024,	"kbytes"	},
# endif /* RLIMIT_RSS */

# ifdef RLIMIT_UMEM
    { RLIMIT_UMEM, 	"memoryuse",	1024,	"kbytes"	},
# endif /* RLIMIT_UMEM */

# ifdef RLIMIT_VMEM
    { RLIMIT_VMEM, 	"vmemoryuse",	1024,	"kbytes"	},
# endif /* RLIMIT_VMEM */

# ifdef RLIMIT_NOFILE
    { RLIMIT_NOFILE, 	"descriptors", 1,	""		},
# endif /* RLIMIT_NOFILE */

# ifdef RLIMIT_CONCUR
    { RLIMIT_CONCUR, 	"concurrency", 1,	"thread(s)"	},
# endif /* RLIMIT_CONCUR */

# ifdef RLIMIT_MEMLOCK
    { RLIMIT_MEMLOCK,	"memorylocked",	1024,	"kbytes"	},
# endif /* RLIMIT_MEMLOCK */

# ifdef RLIMIT_NPROC
    { RLIMIT_NPROC,	"maxproc",	1,	""		},
# endif /* RLIMIT_NPROC */

# if defined(RLIMIT_OFILE) && !defined(RLIMIT_NOFILE)
    { RLIMIT_OFILE,	"openfiles",	1,	""		},
# endif /* RLIMIT_OFILE && !defined(RLIMIT_NOFILE) */

# ifdef RLIMIT_SBSIZE
    { RLIMIT_SBSIZE,	"sbsize",	1,	""		},
# endif /* RLIMIT_SBSIZE */

    { -1, 		NULL, 		0, 	NULL		}
};

static struct limits *findlim	__P((Char *));
static RLIM_TYPE getval		__P((struct limits *, Char **));
static void limtail		__P((Char *, char*));
static void plim		__P((struct limits *, int));
static int setlim		__P((struct limits *, int, RLIM_TYPE));

#ifdef convex
static  RLIM_TYPE
restrict_limit(value)
    double  value;
{
    /*
     * is f too large to cope with? return the maximum or minimum int
     */
    if (value > (double) INT_MAX)
	return (RLIM_TYPE) INT_MAX;
    else if (value < (double) INT_MIN)
	return (RLIM_TYPE) INT_MIN;
    else
	return (RLIM_TYPE) value;
}
#else /* !convex */
# define restrict_limit(x)	((RLIM_TYPE) (x))
#endif /* convex */


static struct limits *
findlim(cp)
    Char   *cp;
{
    register struct limits *lp, *res;

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

/*ARGSUSED*/
void
dolimit(v, c)
    register Char **v;
    struct command *c;
{
    register struct limits *lp;
    register RLIM_TYPE limit;
    int    hard = 0;

    USE(c);
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
    register struct limits *lp;
    Char  **v;
{
    register float f;
#ifndef atof	/* This can be a macro on linux */
    extern double  atof __P((const char *));
#endif /* atof */
    Char   *cp = *v++;

    f = atof(short2str(cp));

# ifdef convex
    /*
     * is f too large to cope with. limit f to minint, maxint  - X-6768 by
     * strike
     */
    if ((f < (double) INT_MIN) || (f > (double) INT_MAX)) {
	stderror(ERR_NAME | ERR_TOOLARGE);
    }
# endif /* convex */

    while (Isdigit(*cp) || *cp == '.' || *cp == 'e' || *cp == 'E')
	cp++;
    if (*cp == 0) {
	if (*v == 0)
	    return restrict_limit((f * lp->limdiv) + 0.5);
	cp = *v;
    }
    switch (*cp) {
# ifdef RLIMIT_CPU
    case ':':
	if (lp->limconst != RLIMIT_CPU)
	    goto badscal;
	return f == 0.0 ? (RLIM_TYPE) 0 : restrict_limit((f * 60.0 + atof(short2str(cp + 1))));
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
# endif /* RLIMIT_CPU */
    case 'M':
# ifdef RLIMIT_CPU
	if (lp->limconst == RLIMIT_CPU)
	    goto badscal;
# endif /* RLIMIT_CPU */
	*cp = 'm';
	limtail(cp, "megabytes");
	f *= 1024.0 * 1024.0;
	break;
    case 'k':
# ifdef RLIMIT_CPU
	if (lp->limconst == RLIMIT_CPU)
	    goto badscal;
# endif /* RLIMIT_CPU */
	limtail(cp, "kbytes");
	f *= 1024.0;
	break;
    case 'b':
# ifdef RLIMIT_CPU
	if (lp->limconst == RLIMIT_CPU)
	    goto badscal;
# endif /* RLIMIT_CPU */
	limtail(cp, "blocks");
	f *= 512.0;
	break;
    case 'u':
	limtail(cp, "unlimited");
	return ((RLIM_TYPE) RLIM_INFINITY);
    default:
# ifdef RLIMIT_CPU
badscal:
# endif /* RLIMIT_CPU */
	stderror(ERR_NAME | ERR_SCALEF);
    }
# ifdef convex
    return f == 0.0 ? (RLIM_TYPE) 0 : restrict_limit((f + 0.5));
# else
    f += 0.5;
    if (f > (float) RLIM_INFINITY)
	return ((RLIM_TYPE) RLIM_INFINITY);
    else
	return ((RLIM_TYPE) f);
# endif /* convex */
}

static void
limtail(cp, str)
    Char   *cp;
    char   *str;
{
    char *sp;

    sp = str;
    while (*cp && *cp == *str)
	cp++, str++;
    if (*cp)
	stderror(ERR_BADSCALE, sp);
}


/*ARGSUSED*/
static void
plim(lp, hard)
    register struct limits *lp;
    int hard;
{
# ifdef BSDLIMIT
    struct rlimit rlim;
# endif /* BSDLIMIT */
    RLIM_TYPE limit;
    int     div = lp->limdiv;

    xprintf("%-13.13s", lp->limname);

# ifndef BSDLIMIT
    limit = ulimit(lp->limconst, 0);
#  ifdef aiws
    if (lp->limconst == RLIMIT_DATA)
	limit -= 0x20000000;
#  endif /* aiws */
# else /* BSDLIMIT */
    (void) getrlimit(lp->limconst, &rlim);
    limit = hard ? rlim.rlim_max : rlim.rlim_cur;
# endif /* BSDLIMIT */

# if !defined(BSDLIMIT) || defined(FILESIZE512)
    /*
     * Christos: filesize comes in 512 blocks. we divide by 2 to get 1024
     * blocks. Note we cannot pre-multiply cause we might overflow (A/UX)
     */
    if (lp->limconst == RLIMIT_FSIZE) {
	if (limit >= (RLIM_INFINITY / 512))
	    limit = RLIM_INFINITY;
	else
	    div = (div == 1024 ? 2 : 1);
    }
# endif /* !BSDLIMIT || FILESIZE512 */

    if (limit == RLIM_INFINITY)
	xprintf("unlimited");
    else
# ifdef RLIMIT_CPU
    if (lp->limconst == RLIMIT_CPU)
	psecs((long) limit);
    else
# endif /* RLIMIT_CPU */
	xprintf("%ld %s", (long) (limit / div), lp->limscale);
    xputchar('\n');
}

/*ARGSUSED*/
void
dounlimit(v, c)
    register Char **v;
    struct command *c;
{
    register struct limits *lp;
    int    lerr = 0;
    int    hard = 0;
    int	   force = 0;

    USE(c);
    while (*++v && **v == '-') {
	Char   *vp = *v;
	while (*++vp)
	    switch (*vp) {
	    case 'f':
		force = 1;
		break;
	    case 'h':
		hard = 1;
		break;
	    default:
		stderror(ERR_ULIMUS);
		break;
	    }
    }

    if (*v == 0) {
	for (lp = limits; lp->limconst >= 0; lp++)
	    if (setlim(lp, hard, (RLIM_TYPE) RLIM_INFINITY) < 0)
		lerr++;
	if (!force && lerr)
	    stderror(ERR_SILENT);
	return;
    }
    while (*v) {
	lp = findlim(*v++);
	if (setlim(lp, hard, (RLIM_TYPE) RLIM_INFINITY) < 0 && !force)
	    stderror(ERR_SILENT);
    }
}

static int
setlim(lp, hard, limit)
    register struct limits *lp;
    int    hard;
    RLIM_TYPE limit;
{
# ifdef BSDLIMIT
    struct rlimit rlim;

    (void) getrlimit(lp->limconst, &rlim);

#  ifdef FILESIZE512
    /* Even though hpux has setrlimit(), it expects fsize in 512 byte blocks */
    if (limit != RLIM_INFINITY && lp->limconst == RLIMIT_FSIZE)
	limit /= 512;
#  endif /* FILESIZE512 */
    if (hard)
	rlim.rlim_max = limit;
    else if (limit == RLIM_INFINITY && euid != 0)
	rlim.rlim_cur = rlim.rlim_max;
    else
	rlim.rlim_cur = limit;

    if (rlim.rlim_cur > rlim.rlim_max)
	rlim.rlim_max = rlim.rlim_cur;

    if (setrlimit(lp->limconst, &rlim) < 0) {
# else /* BSDLIMIT */
    if (limit != RLIM_INFINITY && lp->limconst == RLIMIT_FSIZE)
	limit /= 512;
# ifdef aiws
    if (lp->limconst == RLIMIT_DATA)
	limit += 0x20000000;
# endif /* aiws */
    if (ulimit(toset(lp->limconst), limit) < 0) {
# endif /* BSDLIMIT */
	xprintf(CGETS(15, 1, "%s: %s: Can't %s%s limit (%s)\n"), bname,
	    lp->limname, limit == RLIM_INFINITY ? CGETS(15, 2, "remove") :
	    CGETS(15, 3, "set"), hard ? CGETS(14, 4, " hard") : "",
	    strerror(errno));
	return (-1);
    }
    return (0);
}

#endif /* !HAVENOLIMIT */

/*ARGSUSED*/
void
dosuspend(v, c)
    Char **v;
    struct command *c;
{
#ifdef BSDJOBS
    int     ctpgrp;

    signalfun_t old;
#endif /* BSDJOBS */
    
    USE(c);
    USE(v);

    if (loginsh)
	stderror(ERR_SUSPLOG);
    untty();

#ifdef BSDJOBS
    old = signal(SIGTSTP, SIG_DFL);
    (void) kill(0, SIGTSTP);
    /* the shell stops here */
    (void) signal(SIGTSTP, old);
#else /* !BSDJOBS */
    stderror(ERR_JOBCONTROL);
#endif /* BSDJOBS */

#ifdef BSDJOBS
    if (tpgrp != -1) {
retry:
	ctpgrp = tcgetpgrp(FSHTTY);
	if (ctpgrp == -1)
	    stderror(ERR_SYSTEM, "tcgetpgrp", strerror(errno));
	if (ctpgrp != opgrp) {
	    old = signal(SIGTTIN, SIG_DFL);
	    (void) kill(0, SIGTTIN);
	    (void) signal(SIGTTIN, old);
	    goto retry;
	}
	(void) setpgid(0, shpgrp);
	(void) tcsetpgrp(FSHTTY, shpgrp);
    }
#endif /* BSDJOBS */
    (void) setdisc(FSHTTY);
}

/* This is the dreaded EVAL built-in.
 *   If you don't fiddle with file descriptors, and reset didfds,
 *   this command will either ignore redirection inside or outside
 *   its arguments, e.g. eval "date >x"  vs.  eval "date" >x
 *   The stuff here seems to work, but I did it by trial and error rather
 *   than really knowing what was going on.  If tpgrp is zero, we are
 *   probably a background eval, e.g. "eval date &", and we want to
 *   make sure that any processes we start stay in our pgrp.
 *   This is also the case for "time eval date" -- stay in same pgrp.
 *   Otherwise, under stty tostop, processes will stop in the wrong
 *   pgrp, with no way for the shell to get them going again.  -IAN!
 */

static Char **gv = NULL, **gav = NULL;

/*ARGSUSED*/
void
doeval(v, c)
    Char  **v;
    struct command *c;
{
    Char  **oevalvec;
    Char   *oevalp;
    int     odidfds;
#ifndef CLOSE_ON_EXEC
    int     odidcch;
#endif /* CLOSE_ON_EXEC */
    jmp_buf_t osetexit;
    int     my_reenter;
    Char  **savegv;
    int     saveIN, saveOUT, saveDIAG;
    int     oSHIN, oSHOUT, oSHDIAG;

    USE(c);
    oevalvec = evalvec;
    oevalp = evalp;
    odidfds = didfds;
#ifndef CLOSE_ON_EXEC
    odidcch = didcch;
#endif /* CLOSE_ON_EXEC */
    oSHIN = SHIN;
    oSHOUT = SHOUT;
    oSHDIAG = SHDIAG;

    savegv = gv;
    gav = v;

    gav++;
    if (*gav == 0)
	return;
    gflag = 0, tglob(gav);
    if (gflag) {
	gv = gav = globall(gav);
	gargv = 0;
	if (gav == 0)
	    stderror(ERR_NOMATCH);
	gav = copyblk(gav);
    }
    else {
	gv = NULL;
	gav = copyblk(gav);
	trim(gav);
    }

    saveIN = dcopy(SHIN, -1);
    saveOUT = dcopy(SHOUT, -1);
    saveDIAG = dcopy(SHDIAG, -1);

    getexit(osetexit);

    /* PWP: setjmp/longjmp bugfix for optimizing compilers */
#ifdef cray
    my_reenter = 1;             /* assume non-zero return val */
    if (setexit() == 0) {
	my_reenter = 0;         /* Oh well, we were wrong */
#else /* !cray */
    if ((my_reenter = setexit()) == 0) {
#endif /* cray */
	evalvec = gav;
	evalp = 0;
	SHIN = dcopy(0, -1);
	SHOUT = dcopy(1, -1);
	SHDIAG = dcopy(2, -1);
#ifndef CLOSE_ON_EXEC
	didcch = 0;
#endif /* CLOSE_ON_EXEC */
	didfds = 0;
	process(0);
    }

    evalvec = oevalvec;
    evalp = oevalp;
    doneinp = 0;
#ifndef CLOSE_ON_EXEC
    didcch = odidcch;
#endif /* CLOSE_ON_EXEC */
    didfds = odidfds;
    (void) close(SHIN);
    (void) close(SHOUT);
    (void) close(SHDIAG);
    SHIN = dmove(saveIN, oSHIN);
    SHOUT = dmove(saveOUT, oSHOUT);
    SHDIAG = dmove(saveDIAG, oSHDIAG);

    if (gv)
	blkfree(gv);

    gv = savegv;
    resexit(osetexit);
    if (my_reenter)
	stderror(ERR_SILENT);
}

/*************************************************************************/
/* print list of builtin commands */

/*ARGSUSED*/
void
dobuiltins(v, c)
Char **v;
struct command *c;
{
    /* would use print_by_column() in tw.parse.c but that assumes
     * we have an array of Char * to pass.. (sg)
     */
    extern int Tty_raw_mode;
    extern int TermH;		/* from the editor routines */
    extern int lbuffed;		/* from sh.print.c */

    register struct biltins *b;
    register int row, col, columns, rows;
    unsigned int w, maxwidth;

    USE(c);
    USE(v);
    lbuffed = 0;		/* turn off line buffering */

    /* find widest string */
    for (maxwidth = 0, b = bfunc; b < &bfunc[nbfunc]; ++b)
	maxwidth = max(maxwidth, strlen(b->bname));
    ++maxwidth;					/* for space */

    columns = (TermH + 1) / maxwidth;	/* PWP: terminal size change */
    if (!columns)
	columns = 1;
    rows = (nbfunc + (columns - 1)) / columns;

    for (b = bfunc, row = 0; row < rows; row++) {
	for (col = 0; col < columns; col++) {
	    if (b < &bfunc[nbfunc]) {
		w = strlen(b->bname);
		xprintf("%s", b->bname);
		if (col < (columns - 1))	/* Not last column? */
		    for (; w < maxwidth; w++)
			xputchar(' ');
		++b;
	    }
	}
	if (row < (rows - 1)) {
	    if (Tty_raw_mode)
		xputchar('\r');
	    xputchar('\n');
	}
    }
#ifdef WINNT_NATIVE
    nt_print_builtins(maxwidth);
#else
    if (Tty_raw_mode)
	xputchar('\r');
    xputchar('\n');
#endif /* WINNT_NATIVE */

    lbuffed = 1;		/* turn back on line buffering */
    flush();
}

void
nlsinit()
{
#ifdef NLS_CATALOGS
    char catalog[ 256 ] = { 't', 'c', 's', 'h', '\0' };

    if (adrof(STRcatalog) != NULL)
        xsnprintf((char *)catalog, sizeof(catalog), "tcsh.%s",
		  short2str(varval(STRcatalog)));
    catd = catopen(catalog, MCLoadBySet);
#endif /* NLS_CATALOGS */
#ifdef WINNT_NATIVE
    nls_dll_init();
#endif /* WINNT_NATIVE */
    errinit();		/* init the errorlist in correct locale */
    mesginit();		/* init the messages for signals */
    dateinit();		/* init the messages for dates */
    editinit();		/* init the editor messages */
    terminit();		/* init the termcap messages */
}
