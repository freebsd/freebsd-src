/* $Header: /src/pub/tcsh/tc.func.c,v 3.107 2003/05/16 18:10:29 christos Exp $ */
/*
 * tc.func.c: New tcsh builtins.
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

RCSID("$Id: tc.func.c,v 3.107 2003/05/16 18:10:29 christos Exp $")

#include "ed.h"
#include "ed.defns.h"		/* for the function names */
#include "tw.h"
#include "tc.h"
#ifdef WINNT_NATIVE
#include "nt.const.h"
#endif /* WINNT_NATIVE */

#ifdef AFS
#define PASSMAX 16
#include <afs/stds.h>
#include <afs/kautils.h>
long ka_UserAuthenticateGeneral();
#else
#ifndef PASSMAX
#define PASSMAX 8
#endif
#endif /* AFS */

#ifdef TESLA
extern int do_logout;
#endif /* TESLA */
extern time_t t_period;
extern int just_signaled;
static bool precmd_active = 0;
static bool jobcmd_active = 0; /* GrP */
static bool postcmd_active = 0;
static bool periodic_active = 0;
static bool cwdcmd_active = 0;	/* PWP: for cwd_cmd */
static bool beepcmd_active = 0;
static signalfun_t alm_fun = NULL;

static	void	 auto_logout	__P((int));
static	char	*xgetpass	__P((char *));
static	void	 auto_lock	__P((int));
#ifdef BSDJOBS
static	void	 insert		__P((struct wordent *, bool));
static	void	 insert_we	__P((struct wordent *, struct wordent *));
static	int	 inlist		__P((Char *, Char *));
#endif /* BSDJOBS */
struct tildecache;
static	int	 tildecompare	__P((struct tildecache *, struct tildecache *));
static  Char    *gethomedir	__P((Char *));
#ifdef REMOTEHOST
static	sigret_t palarm		__P((int));
static	void	 getremotehost	__P((void));
#endif /* REMOTEHOST */

/*
 * Tops-C shell
 */

/*
 * expand_lex: Take the given lex and put an expanded version of it in
 * the string buf. First guy in lex list is ignored; last guy is ^J
 * which we ignore. Only take lex'es from position 'from' to position
 * 'to' inclusive
 *
 * Note: csh sometimes sets bit 8 in characters which causes all kinds
 * of problems if we don't mask it here. Note: excl's in lexes have been
 * un-back-slashed and must be re-back-slashed
 *
 * (PWP: NOTE: this returns a pointer to the END of the string expanded
 *             (in other words, where the NUL is).)
 */
/* PWP: this is a combination of the old sprlex() and the expand_lex from
   the magic-space stuff */

Char   *
expand_lex(buf, bufsiz, sp0, from, to)
    Char   *buf;
    size_t  bufsiz;
    struct  wordent *sp0;
    int     from, to;
{
    register struct wordent *sp;
    register Char *s, *d, *e;
    register Char prev_c;
    register int i;

    /*
     * Make sure we have enough space to expand into.  E.g. we may have
     * "a|b" turn to "a | b" (from 3 to 5 characters) which is the worst
     * case scenario (even "a>&! b" turns into "a > & ! b", i.e. 6 to 9
     * characters -- am I missing any other cases?).
     */
    bufsiz = bufsiz / 2;

    buf[0] = '\0';
    prev_c = '\0';
    d = buf;
    e = &buf[bufsiz];		/* for bounds checking */

    if (!sp0)
	return (buf);		/* null lex */
    if ((sp = sp0->next) == sp0)
	return (buf);		/* nada */
    if (sp == (sp0 = sp0->prev))
	return (buf);		/* nada */

    for (i = 0; i < NCARGS; i++) {
	if ((i >= from) && (i <= to)) {	/* if in range */
	    for (s = sp->word; *s && d < e; s++) {
		/*
		 * bugfix by Michael Bloom: anything but the current history
		 * character {(PWP) and backslash} seem to be dealt with
		 * elsewhere.
		 */
		if ((*s & QUOTE)
		    && (((*s & TRIM) == HIST) ||
			(((*s & TRIM) == '\'') && (prev_c != '\\')) ||
			(((*s & TRIM) == '\"') && (prev_c != '\\')) ||
			(((*s & TRIM) == '\\') && (prev_c != '\\')))) {
		    *d++ = '\\';
		}
		if (d < e)
		    *d++ = (*s & TRIM);
		prev_c = *s;
	    }
	    if (d < e)
		*d++ = ' ';
	}
	sp = sp->next;
	if (sp == sp0)
	    break;
    }
    if (d > buf)
	d--;			/* get rid of trailing space */

    return (d);
}

Char   *
sprlex(buf, bufsiz, sp0)
    Char   *buf;
    size_t  bufsiz;
    struct wordent *sp0;
{
    Char   *cp;

    cp = expand_lex(buf, bufsiz, sp0, 0, NCARGS);
    *cp = '\0';
    return (buf);
}


Char *
Itoa(n, s, min_digits, attributes)
    int n;
    Char *s;
    int min_digits, attributes;
{
    /*
     * The array size here is derived from
     *	log8(UINT_MAX)
     * which is guaranteed to be enough for a decimal
     * representation.  We add 1 because integer divide
     * rounds down.
     */
#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif
    Char buf[CHAR_BIT * sizeof(int) / 3 + 1];
    Char *p;
    unsigned int un;	/* handle most negative # too */
    int pad = (min_digits != 0);

    if (sizeof(buf) - 1 < min_digits)
	min_digits = sizeof(buf) - 1;

    un = n;
    if (n < 0) {
	un = -n;
	*s++ = '-';
    }

    p = buf;
    do {
	*p++ = un % 10 + '0';
	un /= 10;
    } while ((pad && --min_digits > 0) || un != 0);

    while (p > buf)
	*s++ = *--p | attributes;

    *s = '\0';
    return s;
}


/*ARGSUSED*/
void
dolist(v, c)
    register Char **v;
    struct command *c;
{
    int     i, k;
    struct stat st;
#if defined(KANJI) && defined(SHORT_STRINGS) && defined(DSPMBYTE)
    extern bool dspmbyte_ls;
#endif
#ifdef COLOR_LS_F
    extern bool color_context_ls;
#endif /* COLOR_LS_F */

    USE(c);
    if (*++v == NULL) {
	(void) t_search(STRNULL, NULL, LIST, 0, TW_ZERO, 0, STRNULL, 0);
	return;
    }
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
    for (k = 0; v[k] != NULL && v[k][0] != '-'; k++)
	continue;
    if (v[k]) {
	/*
	 * We cannot process a flag therefore we let ls do it right.
	 */
	static Char STRls[] = {'l', 's', '\0'};
	static Char STRmCF[] = {'-', 'C', 'F', '\0', '\0' };
	Char *lspath;
	struct command *t;
	struct wordent cmd, *nextword, *lastword;
	Char   *cp;
	struct varent *vp;

#ifdef BSDSIGS
	sigmask_t omask = 0;

	if (setintr)
	    omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
#else /* !BSDSIGS */
	(void) sighold(SIGINT);
#endif /* BSDSIGS */
	if (seterr) {
	    xfree((ptr_t) seterr);
	    seterr = NULL;
	}

	lspath = STRls;
	STRmCF[1] = 'C';
	STRmCF[3] = '\0';
	/* Look at listflags, to add -A to the flags, to get a path
	   of ls if necessary */
	if ((vp = adrof(STRlistflags)) != NULL && vp->vec != NULL &&
	    vp->vec[0] != STRNULL) {
	    if (vp->vec[1] != NULL && vp->vec[1][0] != '\0')
		lspath = vp->vec[1];
	    for (cp = vp->vec[0]; *cp; cp++)
		switch (*cp) {
		case 'x':
		    STRmCF[1] = 'x';
		    break;
		case 'a':
		    STRmCF[3] = 'a';
		    break;
		case 'A':
		    STRmCF[3] = 'A';
		    break;
		default:
		    break;
		}
	}

	cmd.word = STRNULL;
	lastword = &cmd;
	nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	nextword->word = Strsave(lspath);
	lastword->next = nextword;
	nextword->prev = lastword;
	lastword = nextword;
	nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	nextword->word = Strsave(STRmCF);
	lastword->next = nextword;
	nextword->prev = lastword;
#if defined(KANJI) && defined(SHORT_STRINGS) && defined(DSPMBYTE)
	if (dspmbyte_ls) {
	    lastword = nextword;
	    nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	    nextword->word = Strsave(STRmmliteral);
	    lastword->next = nextword;
	    nextword->prev = lastword;
	}
#endif
#ifdef COLOR_LS_F
	if (color_context_ls) {
	    lastword = nextword;
	    nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	    nextword->word = Strsave(STRmmcolormauto);
	    lastword->next = nextword;
	    nextword->prev = lastword;
	}
#endif /* COLOR_LS_F */
	lastword = nextword;
	for (cp = *v; cp; cp = *++v) {
	    nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	    nextword->word = quote(Strsave(cp));
	    lastword->next = nextword;
	    nextword->prev = lastword;
	    lastword = nextword;
	}
	lastword->next = &cmd;
	cmd.prev = lastword;

	/* build a syntax tree for the command. */
	t = syntax(cmd.next, &cmd, 0);
	if (seterr)
	    stderror(ERR_OLD);
	/* expand aliases like process() does */
	/* alias(&cmd); */
	/* execute the parse tree. */
	execute(t, tpgrp > 0 ? tpgrp : -1, NULL, NULL, FALSE);
	/* done. free the lex list and parse tree. */
	freelex(&cmd), freesyn(t);
	if (setintr)
#ifdef BSDSIGS
	    (void) sigsetmask(omask);
#else /* !BSDSIGS */
	    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
    }
    else {
	Char   *dp, *tmp, buf[MAXPATHLEN];

	for (k = 0, i = 0; v[k] != NULL; k++) {
	    tmp = dnormalize(v[k], symlinks == SYM_IGNORE);
	    dp = &tmp[Strlen(tmp) - 1];
	    if (*dp == '/' && dp != tmp)
#ifdef apollo
		if (dp != &tmp[1])
#endif /* apollo */
		*dp = '\0';
		if (stat(short2str(tmp), &st) == -1) {
		if (k != i) {
		    if (i != 0)
			xputchar('\n');
		    print_by_column(STRNULL, &v[i], k - i, FALSE);
		}
		xprintf("%S: %s.\n", tmp, strerror(errno));
		i = k + 1;
	    }
	    else if (S_ISDIR(st.st_mode)) {
		Char   *cp;

		if (k != i) {
		    if (i != 0)
			xputchar('\n');
		    print_by_column(STRNULL, &v[i], k - i, FALSE);
		}
		if (k != 0 && v[1] != NULL)
		    xputchar('\n');
		xprintf("%S:\n", tmp);
		for (cp = tmp, dp = buf; *cp; *dp++ = (*cp++ | QUOTE))
		    continue;
		if (
#ifdef WINNT_NATIVE
		    (dp[-1] != (Char) (':' | QUOTE)) &&
#endif /* WINNT_NATIVE */
		    (dp[-1] != (Char) ('/' | QUOTE)))
		    *dp++ = '/';
		else 
		    dp[-1] &= TRIM;
		*dp = '\0';
		(void) t_search(buf, NULL, LIST, 0, TW_ZERO, 0, STRNULL, 0);
		i = k + 1;
	    }
	    xfree((ptr_t) tmp);
	}
	if (k != i) {
	    if (i != 0)
		xputchar('\n');
	    print_by_column(STRNULL, &v[i], k - i, FALSE);
	}
    }

    if (gargv) {
	blkfree(gargv);
	gargv = 0;
    }
}

static char *defaulttell = "ALL";
extern bool GotTermCaps;

/*ARGSUSED*/
void
dotelltc(v, c)
    register Char **v;
    struct command *c;
{
    USE(c);
    if (!GotTermCaps)
	GetTermCaps();

    /*
     * Avoid a compiler bug on hpux 9.05
     * Writing the following as func(a ? b : c) breaks
     */
    if (v[1])
	TellTC(short2str(v[1]));
    else
	TellTC(defaulttell);
}

/*ARGSUSED*/
void
doechotc(v, c)
    register Char **v;
    struct command *c;
{
    if (!GotTermCaps)
	GetTermCaps();
    EchoTC(++v);
}

/*ARGSUSED*/
void
dosettc(v, c)
    Char  **v;
    struct command *c;
{
    char    tv[2][BUFSIZE];

    if (!GotTermCaps)
	GetTermCaps();

    (void) strcpy(tv[0], short2str(v[1]));
    (void) strcpy(tv[1], short2str(v[2]));
    SetTC(tv[0], tv[1]);
}

/* The dowhich() is by:
 *  Andreas Luik <luik@isaak.isa.de>
 *  I S A  GmbH - Informationssysteme fuer computerintegrierte Automatisierung
 *  Azenberstr. 35
 *  D-7000 Stuttgart 1
 *  West-Germany
 * Thanks!!
 */
int
cmd_expand(cmd, str)
    Char *cmd;
    Char *str;
{
    struct wordent lexp[3];
    struct varent *vp;
    int rv = TRUE;

    lexp[0].next = &lexp[1];
    lexp[1].next = &lexp[2];
    lexp[2].next = &lexp[0];

    lexp[0].prev = &lexp[2];
    lexp[1].prev = &lexp[0];
    lexp[2].prev = &lexp[1];

    lexp[0].word = STRNULL;
    lexp[2].word = STRret;

    if ((vp = adrof1(cmd, &aliases)) != NULL && vp->vec != NULL) {
	if (str == NULL) {
	    xprintf(CGETS(22, 1, "%S: \t aliased to "), cmd);
	    blkpr(vp->vec);
	    xputchar('\n');
	}
	else 
	    blkexpand(vp->vec, str);
    }
    else {
	lexp[1].word = cmd;
	rv = tellmewhat(lexp, str);
    }
    return rv;
}


/*ARGSUSED*/
void
dowhich(v, c)
    register Char **v;
    struct command *c;
{
    int rv = TRUE;
    USE(c);

#ifdef notdef
    /* 
     * We don't want to glob dowhich args because we lose quoteing
     * E.g. which \ls if ls is aliased will not work correctly if
     * we glob here.
     */
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
#endif

    while (*++v) 
	rv &= cmd_expand(*v, NULL);

    if (!rv)
	set(STRstatus, Strsave(STR1), VAR_READWRITE);

#ifdef notdef
    /* Again look at the comment above; since we don't glob, we don't free */
    if (gargv)
	blkfree(gargv), gargv = 0;
#endif
}

/* PWP: a hack to start up your stopped editor on a single keystroke */
/* jbs - fixed hack so it worked :-) 3/28/89 */

struct process *
find_stop_ed()
{
    register struct process *pp, *retp;
    register char *ep, *vp, *cp, *p;
    int     epl, vpl, pstatus;

    if ((ep = getenv("EDITOR")) != NULL) {	/* if we have a value */
	if ((p = strrchr(ep, '/')) != NULL) 	/* if it has a path */
	    ep = p + 1;		/* then we want only the last part */
    }
    else 
	ep = "ed";

    if ((vp = getenv("VISUAL")) != NULL) {	/* if we have a value */
	if ((p = strrchr(vp, '/')) != NULL) 	/* and it has a path */
	    vp = p + 1;		/* then we want only the last part */
    }
    else 
	vp = "vi";

    for (vpl = 0; vp[vpl] && !Isspace(vp[vpl]); vpl++)
	continue;
    for (epl = 0; ep[epl] && !Isspace(ep[epl]); epl++)
	continue;

    if (pcurrent == NULL)	/* see if we have any jobs */
	return NULL;		/* nope */

    retp = NULL;
    for (pp = proclist.p_next; pp; pp = pp->p_next)
	if (pp->p_procid == pp->p_jobid) {

	    /*
	     * Only foreground an edit session if it is suspended.  Some GUI
	     * editors have may be happily running in a separate window, no
	     * point in foregrounding these if they're already running - webb
	     */
	    pstatus = (int) (pp->p_flags & PALLSTATES);
	    if (pstatus != PINTERRUPTED && pstatus != PSTOPPED &&
		pstatus != PSIGNALED)
		continue;

	    p = short2str(pp->p_command);
	    /* get the first word */
	    for (cp = p; *cp && !isspace((unsigned char) *cp); cp++)
		continue;
	    *cp = '\0';
		
	    if ((cp = strrchr(p, '/')) != NULL)	/* and it has a path */
		cp = cp + 1;		/* then we want only the last part */
	    else
		cp = p;			/* else we get all of it */

	    /* if we find either in the current name, fg it */
	    if (strncmp(ep, cp, (size_t) epl) == 0 ||
		strncmp(vp, cp, (size_t) vpl) == 0) {

		/*
		 * If there is a choice, then choose the current process if
		 * available, or the previous process otherwise, or else
		 * anything will do - Robert Webb (robertw@mulga.cs.mu.oz.au).
		 */
		if (pp == pcurrent)
		    return pp;
		else if (retp == NULL || pp == pprevious)
		    retp = pp;
	    }
	}

    return retp;		/* Will be NULL if we didn't find a job */
}

void
fg_proc_entry(pp)
    register struct process *pp;
{
#ifdef BSDSIGS
    sigmask_t omask;
#endif
    jmp_buf_t osetexit;
    bool    ohaderr;
    Char    oGettingInput;

    getexit(osetexit);

#ifdef BSDSIGS
    omask = sigblock(sigmask(SIGINT));
#else
    (void) sighold(SIGINT);
#endif
    oGettingInput = GettingInput;
    GettingInput = 0;

    ohaderr = haderr;		/* we need to ignore setting of haderr due to
				 * process getting stopped by a signal */
    if (setexit() == 0) {	/* come back here after pjwait */
	pendjob();
	(void) alarm(0);	/* No autologout */
	if (!pstart(pp, 1)) {
	    pp->p_procid = 0;
	    stderror(ERR_BADJOB, pp->p_command, strerror(errno));
	}
	pjwait(pp);
    }
    setalarm(1);		/* Autologout back on */
    resexit(osetexit);
    haderr = ohaderr;
    GettingInput = oGettingInput;

#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */

}

static char *
xgetpass(prm)
    char *prm;
{
    static char pass[PASSMAX + 1];
    int fd, i;
    signalfun_t sigint;

    sigint = (signalfun_t) sigset(SIGINT, SIG_IGN);
    (void) Rawmode();	/* Make sure, cause we want echo off */
    if ((fd = open("/dev/tty", O_RDWR|O_LARGEFILE)) == -1)
	fd = SHIN;

    xprintf("%s", prm); flush();
    for (i = 0;;)  {
	if (read(fd, &pass[i], 1) < 1 || pass[i] == '\n') 
	    break;
	if (i < PASSMAX)
	    i++;
    }
	
    pass[i] = '\0';

    if (fd != SHIN)
	(void) close(fd);
    (void) sigset(SIGINT, sigint);

    return(pass);
}
	
/*
 * Ask the user for his login password to continue working
 * On systems that have a shadow password, this will only 
 * work for root, but what can we do?
 *
 * If we fail to get the password, then we log the user out
 * immediately
 */
/*ARGSUSED*/
static void
auto_lock(n)
	int n;
{
#ifndef NO_CRYPT

    int i;
    char *srpp = NULL;
    struct passwd *pw;
#ifdef POSIX
    extern char *crypt __P((const char *, const char *));
#else
    extern char *crypt __P(());
#endif

#undef XCRYPT

#if defined(PW_AUTH) && !defined(XCRYPT)

    struct authorization *apw;
    extern char *crypt16 __P((const char *, const char *));

# define XCRYPT(a, b) crypt16(a, b)

    if ((pw = getpwuid(euid)) != NULL &&	/* effective user passwd  */
        (apw = getauthuid(euid)) != NULL) 	/* enhanced ultrix passwd */
	srpp = apw->a_password;

#endif /* PW_AUTH && !XCRYPT */

#if defined(PW_SHADOW) && !defined(XCRYPT)

    struct spwd *spw;

# define XCRYPT(a, b) crypt(a, b)

    if ((pw = getpwuid(euid)) != NULL &&	/* effective user passwd  */
	(spw = getspnam(pw->pw_name)) != NULL)	/* shadowed passwd	  */
	srpp = spw->sp_pwdp;

#endif /* PW_SHADOW && !XCRYPT */

#ifndef XCRYPT

#define XCRYPT(a, b) crypt(a, b)

#if !defined(__MVS__)
    if ((pw = getpwuid(euid)) != NULL)	/* effective user passwd  */
	srpp = pw->pw_passwd;
#endif /* !MVS */

#endif /* !XCRYPT */

    if (srpp == NULL) {
	auto_logout(0);
	/*NOTREACHED*/
	return;
    }

    setalarm(0);		/* Not for locking any more */
#ifdef BSDSIGS
    (void) sigsetmask(sigblock(0) & ~(sigmask(SIGALRM)));
#else /* !BSDSIGS */
    (void) sigrelse(SIGALRM);
#endif /* BSDSIGS */
    xputchar('\n'); 
    for (i = 0; i < 5; i++) {
	const char *crpp;
	char *pp;
#ifdef AFS
	char *afsname;
	Char *safs;

	if ((safs = varval(STRafsuser)) != STRNULL)
	    afsname = short2str(safs);
	else
	    if ((afsname = getenv("AFSUSER")) == NULL)
	        afsname = pw->pw_name;
#endif
	pp = xgetpass("Password:"); 

	crpp = XCRYPT(pp, srpp);
	if ((strcmp(crpp, srpp) == 0)
#ifdef AFS
	    || (ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION,
					   afsname,     /* name */
					   NULL,        /* instance */
					   NULL,        /* realm */
					   pp,          /* password */
					   0,           /* lifetime */
					   0, 0,         /* spare */
					   NULL)        /* reason */
	    == 0)
#endif /* AFS */
	    ) {
	    (void) memset(pp, 0, PASSMAX);
	    if (GettingInput && !just_signaled) {
		(void) Rawmode();
		ClearLines();	
		ClearDisp();	
		Refresh();
	    }
	    just_signaled = 0;
	    return;
	}
	xprintf(CGETS(22, 2, "\nIncorrect passwd for %s\n"), pw->pw_name);
    }
#endif /* NO_CRYPT */
    auto_logout(0);
    USE(n);
}


static void
auto_logout(n)
    int n;
{
    USE(n);
    xprintf("auto-logout\n");
    /* Don't leave the tty in raw mode */
    if (editing)
	(void) Cookedmode();
    (void) close(SHIN);
    set(STRlogout, Strsave(STRautomatic), VAR_READWRITE);
    child = 1;
#ifdef TESLA
    do_logout = 1;
#endif /* TESLA */
    GettingInput = FALSE; /* make flush() work to write hist files. Huber*/
    goodbye(NULL, NULL);
}

sigret_t
/*ARGSUSED*/
alrmcatch(snum)
int snum;
{
#ifdef UNRELSIGS
    if (snum)
	(void) sigset(SIGALRM, alrmcatch);
#endif /* UNRELSIGS */

    (*alm_fun)(0);

    setalarm(1);
#ifndef SIGVOID
    return (snum);
#endif /* !SIGVOID */
}

/*
 * Karl Kleinpaste, 21oct1983.
 * Added precmd(), which checks for the alias
 * precmd in aliases.  If it's there, the alias
 * is executed as a command.  This is done
 * after mailchk() and just before print-
 * ing the prompt.  Useful for things like printing
 * one's current directory just before each command.
 */
void
precmd()
{
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (precmd_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRprecmd);
	xprintf(CGETS(22, 3, "Faulty alias 'precmd' removed.\n"));
	goto leave;
    }
    precmd_active = 1;
    if (!whyles && adrof1(STRprecmd, &aliases))
	aliasrun(1, STRprecmd, NULL);
leave:
    precmd_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}

void
postcmd()
{
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (postcmd_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRpostcmd);
	xprintf(CGETS(22, 3, "Faulty alias 'postcmd' removed.\n"));
	goto leave;
    }
    postcmd_active = 1;
    if (!whyles && adrof1(STRpostcmd, &aliases))
	aliasrun(1, STRpostcmd, NULL);
leave:
    postcmd_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}

/*
 * Paul Placeway  11/24/87  Added cwd_cmd by hacking precmd() into
 * submission...  Run every time $cwd is set (after it is set).  Useful
 * for putting your machine and cwd (or anything else) in an xterm title
 * space.
 */
void
cwd_cmd()
{
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (cwdcmd_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRcwdcmd);
	xprintf(CGETS(22, 4, "Faulty alias 'cwdcmd' removed.\n"));
	goto leave;
    }
    cwdcmd_active = 1;
    if (!whyles && adrof1(STRcwdcmd, &aliases))
	aliasrun(1, STRcwdcmd, NULL);
leave:
    cwdcmd_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}

/*
 * Joachim Hoenig  07/16/91  Added beep_cmd, run every time tcsh wishes 
 * to beep the terminal bell. Useful for playing nice sounds instead.
 */
void
beep_cmd()
{
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (beepcmd_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRbeepcmd);
	xprintf(CGETS(22, 5, "Faulty alias 'beepcmd' removed.\n"));
    }
    else {
	beepcmd_active = 1;
	if (!whyles && adrof1(STRbeepcmd, &aliases))
	    aliasrun(1, STRbeepcmd, NULL);
    }
    beepcmd_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}


/*
 * Karl Kleinpaste, 18 Jan 1984.
 * Added period_cmd(), which executes the alias "periodic" every
 * $tperiod minutes.  Useful for occasional checking of msgs and such.
 */
void
period_cmd()
{
    register Char *vp;
    time_t  t, interval;
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (periodic_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRperiodic);
	xprintf(CGETS(22, 6, "Faulty alias 'periodic' removed.\n"));
	goto leave;
    }
    periodic_active = 1;
    if (!whyles && adrof1(STRperiodic, &aliases)) {
	vp = varval(STRtperiod);
	if (vp == STRNULL) {
	    aliasrun(1, STRperiodic, NULL);
	    goto leave;
	}
	interval = getn(vp);
	(void) time(&t);
	if (t - t_period >= interval * 60) {
	    t_period = t;
	    aliasrun(1, STRperiodic, NULL);
	}
    }
leave:
    periodic_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}


/* 
 * GrP Greg Parker May 2001
 * Added job_cmd(), which is run every time a job is started or 
 * foregrounded. The command is passed a single argument, the string 
 * used to start the job originally. With precmd, useful for setting 
 * xterm titles.
 * Cloned from cwd_cmd().
 */
void
job_cmd(args)
    Char *args;
{
#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT));
#else /* !BSDSIGS */
    (void) sighold(SIGINT);
#endif /* BSDSIGS */
    if (jobcmd_active) {	/* an error must have been caught */
	aliasrun(2, STRunalias, STRjobcmd);
	xprintf(CGETS(22, 14, "Faulty alias 'jobcmd' removed.\n"));
	goto leave;
    }
    jobcmd_active = 1;
    if (!whyles && adrof1(STRjobcmd, &aliases)) {
	struct process *pp = pcurrjob; /* put things back after the hook */
	aliasrun(2, STRjobcmd, args);
	pcurrjob = pp;
    }
leave:
    jobcmd_active = 0;
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else /* !BSDSIGS */
    (void) sigrelse(SIGINT);
#endif /* BSDSIGS */
}


/*
 * Karl Kleinpaste, 21oct1983.
 * Set up a one-word alias command, for use for special things.
 * This code is based on the mainline of process().
 */
void
aliasrun(cnt, s1, s2)
    int     cnt;
    Char   *s1, *s2;
{
    struct wordent w, *new1, *new2;	/* for holding alias name */
    struct command *t = NULL;
    jmp_buf_t osetexit;
    int status;

    getexit(osetexit);
    if (seterr) {
	xfree((ptr_t) seterr);
	seterr = NULL;	/* don't repeatedly print err msg. */
    }
    w.word = STRNULL;
    new1 = (struct wordent *) xcalloc(1, sizeof w);
    new1->word = Strsave(s1);
    if (cnt == 1) {
	/* build a lex list with one word. */
	w.next = w.prev = new1;
	new1->next = new1->prev = &w;
    }
    else {
	/* build a lex list with two words. */
	new2 = (struct wordent *) xcalloc(1, sizeof w);
	new2->word = Strsave(s2);
	w.next = new2->prev = new1;
	new1->next = w.prev = new2;
	new1->prev = new2->next = &w;
    }

    /* Save the old status */
    status = getn(varval(STRstatus));

    /* expand aliases like process() does. */
    alias(&w);
    /* build a syntax tree for the command. */
    t = syntax(w.next, &w, 0);
    if (seterr)
	stderror(ERR_OLD);

    psavejob();


    /* catch any errors here */
    if (setexit() == 0)
	/* execute the parse tree. */
	/*
	 * From: Michael Schroeder <mlschroe@immd4.informatik.uni-erlangen.de>
	 * was execute(t, tpgrp);
	 */
	execute(t, tpgrp > 0 ? tpgrp : -1, NULL, NULL, TRUE);
    /* done. free the lex list and parse tree. */
    freelex(&w), freesyn(t);
    if (haderr) {
	haderr = 0;
	/*
	 * Either precmd, or cwdcmd, or periodic had an error. Call it again so
	 * that it is removed
	 */
	if (precmd_active)
	    precmd();
	if (postcmd_active)
	    postcmd();
#ifdef notdef
	/*
	 * XXX: On the other hand, just interrupting them causes an error too.
	 * So if we hit ^C in the middle of cwdcmd or periodic the alias gets
	 * removed. We don't want that. Note that we want to remove precmd
	 * though, cause that could lead into an infinite loop. This should be
	 * fixed correctly, but then haderr should give us the whole exit
	 * status not just true or false.
	 */
	else if (cwdcmd_active)
	    cwd_cmd();
	else if (beepcmd_active)
	    beep_cmd();
	else if (periodic_active)
	    period_cmd();
#endif /* notdef */
    }
    /* reset the error catcher to the old place */
    resexit(osetexit);
    prestjob();
    pendjob();
    /* Restore status */
    set(STRstatus, putn(status), VAR_READWRITE);
}

void
setalarm(lck)
    int lck;
{
    struct varent *vp;
    Char   *cp;
    unsigned alrm_time = 0, logout_time, lock_time;
    time_t cl, nl, sched_dif;

    if ((vp = adrof(STRautologout)) != NULL && vp->vec != NULL) {
	if ((cp = vp->vec[0]) != 0) {
	    if ((logout_time = (unsigned) atoi(short2str(cp)) * 60) > 0) {
		alrm_time = logout_time;
		alm_fun = auto_logout;
	    }
	}
	if ((cp = vp->vec[1]) != 0) {
	    if ((lock_time = (unsigned) atoi(short2str(cp)) * 60) > 0) {
		if (lck) {
		    if (alrm_time == 0 || lock_time < alrm_time) {
			alrm_time = lock_time;
			alm_fun = auto_lock;
		    }
		}
		else /* lock_time always < alrm_time */
		    if (alrm_time)
			alrm_time -= lock_time;
	    }
	}
    }
    if ((nl = sched_next()) != -1) {
	(void) time(&cl);
	sched_dif = nl > cl ? nl - cl : 0;
	if ((alrm_time == 0) || ((unsigned) sched_dif < alrm_time)) {
	    alrm_time = ((unsigned) sched_dif) + 1;
	    alm_fun = sched_run;
	}
    }
    (void) alarm(alrm_time);	/* Autologout ON */
}

#undef RMDEBUG			/* For now... */

void
rmstar(cp)
    struct wordent *cp;
{
    struct wordent *we, *args;
    register struct wordent *tmp, *del;

#ifdef RMDEBUG
    static Char STRrmdebug[] = {'r', 'm', 'd', 'e', 'b', 'u', 'g', '\0'};
    Char   *tag;
#endif /* RMDEBUG */
    Char   *charac;
    char    c;
    int     ask, doit, star = 0, silent = 0;

    if (!adrof(STRrmstar))
	return;
#ifdef RMDEBUG
    tag = varval(STRrmdebug);
#endif /* RMDEBUG */
    we = cp->next;
    while (*we->word == ';' && we != cp)
	we = we->next;
    while (we != cp) {
#ifdef RMDEBUG
	if (*tag)
	    xprintf(CGETS(22, 7, "parsing command line\n"));
#endif /* RMDEBUG */
	if (!Strcmp(we->word, STRrm)) {
	    args = we->next;
	    ask = (*args->word != '-');
	    while (*args->word == '-' && !silent) {	/* check options */
		for (charac = (args->word + 1); *charac && !silent; charac++)
		    silent = (*charac == 'i' || *charac == 'f');
		args = args->next;
	    }
	    ask = (ask || (!ask && !silent));
	    if (ask) {
		for (; !star && *args->word != ';'
		     && args != cp; args = args->next)
		    if (!Strcmp(args->word, STRstar))
			star = 1;
		if (ask && star) {
		    xprintf(CGETS(22, 8,
			    "Do you really want to delete all files? [n/y] "));
		    flush();
		    (void) force_read(SHIN, &c, 1);
		    /* 
		     * Perhaps we should use the yesexpr from the
		     * actual locale
		     */
		    doit = (strchr(CGETS(22, 14, "Yy"), c) != NULL);
		    while (c != '\n' && force_read(SHIN, &c, 1) == 1)
			continue;
		    if (!doit) {
			/* remove the command instead */
#ifdef RMDEBUG
			if (*tag)
			    xprintf(CGETS(22, 9,
				    "skipping deletion of files!\n"));
#endif /* RMDEBUG */
			for (tmp = we;
			     *tmp->word != '\n' &&
			     *tmp->word != ';' && tmp != cp;) {
			    tmp->prev->next = tmp->next;
			    tmp->next->prev = tmp->prev;
			    xfree((ptr_t) tmp->word);
			    del = tmp;
			    tmp = tmp->next;
			    xfree((ptr_t) del);
			}
			if (*tmp->word == ';') {
			    tmp->prev->next = tmp->next;
			    tmp->next->prev = tmp->prev;
			    xfree((ptr_t) tmp->word);
			    del = tmp;
			    tmp = tmp->next;
			    xfree((ptr_t) del);
			}
			we = tmp;
			continue;
		    }
		}
	    }
	}
	for (we = we->next;
	     *we->word != ';' && we != cp;
	     we = we->next)
	    continue;
	if (*we->word == ';')
	    we = we->next;
    }
#ifdef RMDEBUG
    if (*tag) {
	xprintf(CGETS(22, 10, "command line now is:\n"));
	for (we = cp->next; we != cp; we = we->next)
	    xprintf("%S ", we->word);
    }
#endif /* RMDEBUG */
    return;
}

#ifdef BSDJOBS
/* Check if command is in continue list
   and do a "aliasing" if it exists as a job in background */

#undef CNDEBUG			/* For now */
void
continue_jobs(cp)
    struct wordent *cp;
{
    struct wordent *we;
    register struct process *pp, *np;
    Char   *cmd, *continue_list, *continue_args_list;

#ifdef CNDEBUG
    Char   *tag;
    static Char STRcndebug[] =
    {'c', 'n', 'd', 'e', 'b', 'u', 'g', '\0'};
#endif /* CNDEBUG */
    bool    in_cont_list, in_cont_arg_list;


#ifdef CNDEBUG
    tag = varval(STRcndebug);
#endif /* CNDEBUG */
    continue_list = varval(STRcontinue);
    continue_args_list = varval(STRcontinue_args);
    if (*continue_list == '\0' && *continue_args_list == '\0')
	return;

    we = cp->next;
    while (*we->word == ';' && we != cp)
	we = we->next;
    while (we != cp) {
#ifdef CNDEBUG
	if (*tag)
	    xprintf(CGETS(22, 11, "parsing command line\n"));
#endif /* CNDEBUG */
	cmd = we->word;
	in_cont_list = inlist(continue_list, cmd);
	in_cont_arg_list = inlist(continue_args_list, cmd);
	if (in_cont_list || in_cont_arg_list) {
#ifdef CNDEBUG
	    if (*tag)
		xprintf(CGETS(22, 12, "in one of the lists\n"));
#endif /* CNDEBUG */
	    np = NULL;
	    for (pp = proclist.p_next; pp; pp = pp->p_next) {
		if (prefix(cmd, pp->p_command)) {
		    if (pp->p_index) {
			np = pp;
			break;
		    }
		}
	    }
	    if (np) {
		insert(we, in_cont_arg_list);
	    }
	}
	for (we = we->next;
	     *we->word != ';' && we != cp;
	     we = we->next)
	    continue;
	if (*we->word == ';')
	    we = we->next;
    }
#ifdef CNDEBUG
    if (*tag) {
	xprintf(CGETS(22, 13, "command line now is:\n"));
	for (we = cp->next; we != cp; we = we->next)
	    xprintf("%S ", we->word);
    }
#endif /* CNDEBUG */
    return;
}

/* The actual "aliasing" of for backgrounds() is done here
   with the aid of insert_we().   */
static void
insert(pl, file_args)
    struct wordent *pl;
    bool    file_args;
{
    struct wordent *now, *last;
    Char   *cmd, *bcmd, *cp1, *cp2;
    int     cmd_len;
    Char   *pause = STRunderpause;
    int     p_len = (int) Strlen(pause);

    cmd_len = (int) Strlen(pl->word);
    cmd = (Char *) xcalloc(1, (size_t) ((cmd_len + 1) * sizeof(Char)));
    (void) Strcpy(cmd, pl->word);
/* Do insertions at beginning, first replace command word */

    if (file_args) {
	now = pl;
	xfree((ptr_t) now->word);
	now->word = (Char *) xcalloc(1, (size_t) (5 * sizeof(Char)));
	(void) Strcpy(now->word, STRecho);

	now = (struct wordent *) xcalloc(1, (size_t) sizeof(struct wordent));
	now->word = (Char *) xcalloc(1, (size_t) (6 * sizeof(Char)));
	(void) Strcpy(now->word, STRbackqpwd);
	insert_we(now, pl);

	for (last = now; *last->word != '\n' && *last->word != ';';
	     last = last->next)
	    continue;

	now = (struct wordent *) xcalloc(1, (size_t) sizeof(struct wordent));
	now->word = (Char *) xcalloc(1, (size_t) (2 * sizeof(Char)));
	(void) Strcpy(now->word, STRgt);
	insert_we(now, last->prev);

	now = (struct wordent *) xcalloc(1, (size_t) sizeof(struct wordent));
	now->word = (Char *) xcalloc(1, (size_t) (2 * sizeof(Char)));
	(void) Strcpy(now->word, STRbang);
	insert_we(now, last->prev);

	now = (struct wordent *) xcalloc(1, (size_t) sizeof(struct wordent));
	now->word = (Char *) xcalloc(1, (size_t) cmd_len + p_len + 4);
	cp1 = now->word;
	cp2 = cmd;
	*cp1++ = '~';
	*cp1++ = '/';
	*cp1++ = '.';
	while ((*cp1++ = *cp2++) != '\0')
	    continue;
	cp1--;
	cp2 = pause;
	while ((*cp1++ = *cp2++) != '\0')
	    continue;
	insert_we(now, last->prev);

	now = (struct wordent *) xcalloc(1, (size_t) sizeof(struct wordent));
	now->word = (Char *) xcalloc(1, (size_t) (2 * sizeof(Char)));
	(void) Strcpy(now->word, STRsemi);
	insert_we(now, last->prev);
	bcmd = (Char *) xcalloc(1, (size_t) ((cmd_len + 2) * sizeof(Char)));
	cp1 = bcmd;
	cp2 = cmd;
	*cp1++ = '%';
	while ((*cp1++ = *cp2++) != '\0')
	    continue;
	now = (struct wordent *) xcalloc(1, (size_t) (sizeof(struct wordent)));
	now->word = bcmd;
	insert_we(now, last->prev);
    }
    else {
	struct wordent *del;

	now = pl;
	xfree((ptr_t) now->word);
	now->word = (Char *) xcalloc(1, 
				     (size_t) ((cmd_len + 2) * sizeof(Char)));
	cp1 = now->word;
	cp2 = cmd;
	*cp1++ = '%';
	while ((*cp1++ = *cp2++) != '\0')
	    continue;
	for (now = now->next;
	     *now->word != '\n' && *now->word != ';' && now != pl;) {
	    now->prev->next = now->next;
	    now->next->prev = now->prev;
	    xfree((ptr_t) now->word);
	    del = now;
	    now = now->next;
	    xfree((ptr_t) del);
	}
    }
}

static void
insert_we(new, where)
    struct wordent *new, *where;
{

    new->prev = where;
    new->next = where->next;
    where->next = new;
    new->next->prev = new;
}

static int
inlist(list, name)
    Char   *list, *name;
{
    register Char *l, *n;

    l = list;
    n = name;

    while (*l && *n) {
	if (*l == *n) {
	    l++;
	    n++;
	    if (*n == '\0' && (*l == ' ' || *l == '\0'))
		return (1);
	    else
		continue;
	}
	else {
	    while (*l && *l != ' ')
		l++;		/* skip to blank */
	    while (*l && *l == ' ')
		l++;		/* and find first nonblank character */
	    n = name;
	}
    }
    return (0);
}

#endif /* BSDJOBS */


/*
 * Implement a small cache for tilde names. This is used primarily
 * to expand tilde names to directories, but also
 * we can find users from their home directories for the tilde
 * prompt, on machines where yp lookup is slow this can be a big win...
 * As with any cache this can run out of sync, rehash can sync it again.
 */
static struct tildecache {
    Char   *user;
    Char   *home;
    int     hlen;
}      *tcache = NULL;

#define TILINCR 10
int tlength = 0;
static int tsize = TILINCR;

static int
tildecompare(p1, p2)
    struct tildecache *p1, *p2;
{
    return Strcmp(p1->user, p2->user);
}

static Char *
gethomedir(us)
    Char   *us;
{
    register struct passwd *pp;
#ifdef HESIOD
    char **res, **res1, *cp;
    Char *rp;
#endif /* HESIOD */
    
    pp = getpwnam(short2str(us));
#ifdef YPBUGS
    fix_yp_bugs();
#endif /* YPBUGS */
    if (pp != NULL) {
#if 0
	/* Don't return if root */
	if (pp->pw_dir[0] == '/' && pp->pw_dir[1] == '\0')
	    return NULL;
	else
#endif
	    return Strsave(str2short(pp->pw_dir));
    }
#ifdef HESIOD
    res = hes_resolve(short2str(us), "filsys");
    rp = NULL;
    if (res != NULL) {
	if ((*res) != NULL) {
	    /*
	     * Look at the first token to determine how to interpret
	     * the rest of it.
	     * Yes, strtok is evil (it's not thread-safe), but it's also
	     * easy to use.
	     */
	    cp = strtok(*res, " ");
	    if (strcmp(cp, "AFS") == 0) {
		/* next token is AFS pathname.. */
		cp = strtok(NULL, " ");
		if (cp != NULL)
		    rp = Strsave(str2short(cp));
	    } else if (strcmp(cp, "NFS") == 0) {
		cp = NULL;
		if ((strtok(NULL, " ")) && /* skip remote pathname */
		    (strtok(NULL, " ")) && /* skip host */
		    (strtok(NULL, " ")) && /* skip mode */
		    (cp = strtok(NULL, " "))) {
		    rp = Strsave(str2short(cp));
		}
	    }
	}
	for (res1 = res; *res1; res1++)
	    free(*res1);
#if 0
	/* Don't return if root */
	if (rp != NULL && rp[0] == '/' && rp[1] == '\0') {
	    xfree((ptr_t)rp);
	    rp = NULL;
	}
#endif
	return rp;
    }
#endif /* HESIOD */
    return NULL;
}

Char   *
gettilde(us)
    Char   *us;
{
    struct tildecache *bp1, *bp2, *bp;
    Char *hd;

    /* Ignore NIS special names */
    if (*us == '+' || *us == '-')
	return NULL;

    if (tcache == NULL)
	tcache = (struct tildecache *) xmalloc((size_t) (TILINCR *
						  sizeof(struct tildecache)));
    /*
     * Binary search
     */
    for (bp1 = tcache, bp2 = tcache + tlength; bp1 < bp2;) {
	register int i;

	bp = bp1 + ((bp2 - bp1) >> 1);
	if ((i = *us - *bp->user) == 0 && (i = Strcmp(us, bp->user)) == 0)
	    return (bp->home);
	if (i < 0)
	    bp2 = bp;
	else
	    bp1 = bp + 1;
    }
    /*
     * Not in the cache, try to get it from the passwd file
     */
    hd = gethomedir(us);
    if (hd == NULL)
	return NULL;

    /*
     * Update the cache
     */
    tcache[tlength].user = Strsave(us);
    tcache[tlength].home = hd;
    tcache[tlength++].hlen = (int) Strlen(hd);

    qsort((ptr_t) tcache, (size_t) tlength, sizeof(struct tildecache),
	  (int (*) __P((const void *, const void *))) tildecompare);

    if (tlength == tsize) {
	tsize += TILINCR;
	tcache = (struct tildecache *) xrealloc((ptr_t) tcache,
						(size_t) (tsize *
						  sizeof(struct tildecache)));
    }
    return (hd);
}

/*
 * Return the username if the directory path passed contains a
 * user's home directory in the tilde cache, otherwise return NULL
 * hm points to the place where the path became different.
 * Special case: Our own home directory.
 * If we are passed a null pointer, then we flush the cache.
 */
Char   *
getusername(hm)
    Char  **hm;
{
    Char   *h, *p;
    int     i, j;

    if (hm == NULL) {
	for (i = 0; i < tlength; i++) {
	    xfree((ptr_t) tcache[i].home);
	    xfree((ptr_t) tcache[i].user);
	}
	xfree((ptr_t) tcache);
	tlength = 0;
	tsize = TILINCR;
	tcache = NULL;
	return NULL;
    }
    if (((h = varval(STRhome)) != STRNULL) &&
	(Strncmp(p = *hm, h, (size_t) (j = (int) Strlen(h))) == 0) &&
	(p[j] == '/' || p[j] == '\0')) {
	*hm = &p[j];
	return STRNULL;
    }
    for (i = 0; i < tlength; i++)
	if ((Strncmp(p = *hm, tcache[i].home, (size_t)
	    (j = tcache[i].hlen)) == 0) && (p[j] == '/' || p[j] == '\0')) {
	    *hm = &p[j];
	    return tcache[i].user;
	}
    return NULL;
}

#ifdef OBSOLETE
/*
 * PWP: read a bunch of aliases out of a file QUICKLY.  The format
 *  is almost the same as the result of saying "alias > FILE", except
 *  that saying "aliases > FILE" does not expand non-letters to printable
 *  sequences.
 */
/*ARGSUSED*/
void
doaliases(v, c)
    Char  **v;
    struct command *c;
{
    jmp_buf_t oldexit;
    Char  **vec, *lp;
    int     fd;
    Char    buf[BUFSIZE], line[BUFSIZE];
    char    tbuf[BUFSIZE + 1], *tmp;
    extern bool output_raw;	/* PWP: in sh.print.c */

    USE(c);
    v++;
    if (*v == 0) {
	output_raw = 1;
	plist(&aliases, VAR_ALL);
	output_raw = 0;
	return;
    }

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

    if ((fd = open(tmp = short2str(*v), O_RDONLY|O_LARGEFILE)) < 0)
	stderror(ERR_NAME | ERR_SYSTEM, tmp, strerror(errno));

    getexit(oldexit);
    if (setexit() == 0) {
	for (;;) {
	    Char   *p = NULL;
	    int     n = 0;
	    lp = line;
	    for (;;) {
		if (n <= 0) {
		    int     i;

		    if ((n = read(fd, tbuf, BUFSIZE)) <= 0) {
#ifdef convex
			stderror(ERR_SYSTEM, progname, strerror(errno));
#endif /* convex */
			goto eof;
		    }
		    for (i = 0; i < n; i++)
  			buf[i] = (Char) tbuf[i];
		    p = buf;
		}
		n--;
		if ((*lp++ = *p++) == '\n') {
		    lp[-1] = '\0';
		    break;
		}
	    }
	    for (lp = line; *lp; lp++) {
		if (isspc(*lp)) {
		    *lp++ = '\0';
		    while (isspc(*lp))
			lp++;
		    vec = (Char **) xmalloc((size_t)
					    (2 * sizeof(Char **)));
		    vec[0] = Strsave(lp);
		    vec[1] = NULL;
		    setq(strip(line), vec, &aliases, VAR_READWRITE);
		    break;
		}
	    }
	}
    }

eof:
    (void) close(fd);
    tw_cmd_free();
    if (gargv)
	blkfree(gargv), gargv = 0;
    resexit(oldexit);
}
#endif /* OBSOLETE */


/*
 * set the shell-level var to 1 or apply change to it.
 */
void
shlvl(val)
    int val;
{
    char *cp;

    if ((cp = getenv("SHLVL")) != NULL) {

	if (loginsh)
	    val = 1;
	else
	    val += atoi(cp);

	if (val <= 0) {
	    if (adrof(STRshlvl) != NULL)
		unsetv(STRshlvl);
	    Unsetenv(STRKSHLVL);
	}
	else {
	    Char    buff[BUFSIZE];

	    (void) Itoa(val, buff, 0, 0);
	    set(STRshlvl, Strsave(buff), VAR_READWRITE);
	    tsetenv(STRKSHLVL, buff);
	}
    }
    else {
	set(STRshlvl, SAVE("1"), VAR_READWRITE);
	tsetenv(STRKSHLVL, str2short("1"));
    }
}


/* fixio():
 *	Try to recover from a read error
 */
int
fixio(fd, e)
    int fd, e;
{
    switch (e) {
    case -1:	/* Make sure that the code is reachable */

#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
# define FDRETRY
#endif /* EWOULDBLOCK */

#if defined(POSIX) && defined(EAGAIN)
# if !defined(EWOULDBLOCK) || EWOULDBLOCK != EAGAIN
    case EAGAIN:
#  define FDRETRY
# endif /* !EWOULDBLOCK || EWOULDBLOCK != EAGAIN */
#endif /* POSIX && EAGAIN */

	e = 0;
#ifdef FDRETRY
# ifdef F_SETFL
/*
 * Great! we have on suns 3 flavors and 5 names...
 * I hope that will cover everything.
 * I added some more defines... many systems have different defines.
 * Rather than dealing with getting the right includes, we'll just
 * cover all the known possibilities here.  -- sterling@netcom.com
 */
#  ifndef O_NONBLOCK
#   define O_NONBLOCK 0
#  endif /* O_NONBLOCK */
#  ifndef O_NDELAY
#   define O_NDELAY 0
#  endif /* O_NDELAY */
#  ifndef FNBIO
#   define FNBIO 0
#  endif /* FNBIO */
#  ifndef _FNBIO
#   define _FNBIO 0
#  endif /* _FNBIO */
#  ifndef FNONBIO
#   define FNONBIO 0
#  endif /* FNONBIO */
#  ifndef FNONBLOCK
#   define FNONBLOCK 0
#  endif /* FNONBLOCK */
#  ifndef _FNONBLOCK
#   define _FNONBLOCK 0
#  endif /* _FNONBLOCK */
#  ifndef FNDELAY
#   define FNDELAY 0
#  endif /* FNDELAY */
#  ifndef _FNDELAY
#   define _FNDELAY 0
#  endif /* _FNDELAY */
#  ifndef FNDLEAY	/* Some linux versions have this typo */
#   define FNDLEAY 0
#  endif /* FNDLEAY */
	if ((e = fcntl(fd, F_GETFL, 0)) == -1)
	    return -1;

	e &= ~(O_NDELAY|O_NONBLOCK|FNBIO|_FNBIO|FNONBIO|FNONBLOCK|_FNONBLOCK|
	       FNDELAY|_FNDELAY|FNDLEAY);	/* whew! */
	if (fcntl(fd, F_SETFL, e) == -1)
	    return -1;
	else 
	    e = 1;
# endif /* F_SETFL */

# ifdef FIONBIO
	e = 0;
	if (ioctl(fd, FIONBIO, (ioctl_t) &e) == -1)
	    return -1;
	else
	    e = 1;
# endif	/* FIONBIO */

#endif /* FDRETRY */
	return e ? 0 : -1;

    case EINTR:
	return 0;

    default:
	return -1;
    }
}

/* collate():
 *	String collation
 */
int
collate(a, b)
    const Char *a;
    const Char *b;
{
    int rv;
#ifdef SHORT_STRINGS
    /* This strips the quote bit as a side effect */
    char *sa = strsave(short2str(a));
    char *sb = strsave(short2str(b));
#else
    char *sa = strip(strsave(a));
    char *sb = strip(strsave(b));
#endif /* SHORT_STRINGS */

#if defined(NLS) && !defined(NOSTRCOLL)
    errno = 0;	/* strcoll sets errno, another brain-damage */

    rv = strcoll(sa, sb);

    /*
     * We should be checking for errno != 0, but some systems
     * forget to reset errno to 0. So we only check for the 
     * only documented valid errno value for strcoll [EINVAL]
     */
    if (errno == EINVAL) {
	xfree((ptr_t) sa);
	xfree((ptr_t) sb);
	stderror(ERR_SYSTEM, "strcoll", strerror(errno));
    }
#else
    rv = strcmp(sa, sb);
#endif /* NLS && !NOSTRCOLL */

    xfree((ptr_t) sa);
    xfree((ptr_t) sb);

    return rv;
}

#ifdef HASHBANG
/*
 * From: peter@zeus.dialix.oz.au (Peter Wemm)
 * If exec() fails look first for a #! [word] [word] ....
 * If it is, splice the header into the argument list and retry.
 */
#define HACKBUFSZ 1024		/* Max chars in #! vector */
#define HACKVECSZ 128		/* Max words in #! vector */
int
hashbang(fd, vp)
    int fd;
    Char ***vp;
{
    unsigned char lbuf[HACKBUFSZ];
    char *sargv[HACKVECSZ];
    unsigned char *p, *ws;
    int sargc = 0;
#ifdef WINNT_NATIVE
    int fw = 0; 	/* found at least one word */
    int first_word = 0;
#endif /* WINNT_NATIVE */

    if (read(fd, (char *) lbuf, HACKBUFSZ) <= 0)
	return -1;

    ws = 0;	/* word started = 0 */

    for (p = lbuf; p < &lbuf[HACKBUFSZ]; )
	switch (*p) {
	case ' ':
	case '\t':
#ifdef WINNT_NATIVE
	case '\r':
#endif /* WINNT_NATIVE */
	    if (ws) {	/* a blank after a word.. save it */
		*p = '\0';
#ifndef WINNT_NATIVE
		if (sargc < HACKVECSZ - 1)
		    sargv[sargc++] = ws;
		ws = NULL;
#else /* WINNT_NATIVE */
		if (sargc < HACKVECSZ - 1) {
		    sargv[sargc] = first_word ? NULL: hb_subst(ws);
		    if (sargv[sargc] == NULL)
			sargv[sargc] = ws;
		    sargc++;
		}
		ws = NULL;
	    	fw = 1;
		first_word = 1;
#endif /* WINNT_NATIVE */
	    }
	    p++;
	    continue;

	case '\0':	/* Whoa!! what the hell happened */
	    return -1;

	case '\n':	/* The end of the line. */
	    if (
#ifdef WINNT_NATIVE
		fw ||
#endif /* WINNT_NATIVE */
		ws) {	/* terminate the last word */
		*p = '\0';
#ifndef WINNT_NATIVE
		if (sargc < HACKVECSZ - 1)
		    sargv[sargc++] = ws;
#else /* WINNT_NATIVE */
		if (sargc < HACKVECSZ - 1) { /* deal with the 1-word case */
		    sargv[sargc] = first_word? NULL : hb_subst(ws);
		    if (sargv[sargc] == NULL)
			sargv[sargc] = ws;
		    sargc++;
		}
#endif /* !WINNT_NATIVE */
	    }
	    sargv[sargc] = NULL;
	    ws = NULL;
	    if (sargc > 0) {
		*vp = blk2short(sargv);
		return 0;
	    }
	    else
		return -1;

	default:
	    if (!ws)	/* Start a new word? */
		ws = p; 
	    p++;
	    break;
	}
    return -1;
}
#endif /* HASHBANG */

#ifdef REMOTEHOST

static sigret_t
palarm(snum)
    int snum;
{
    USE(snum);
#ifdef UNRELSIGS
    if (snum)
	(void) sigset(snum, SIG_IGN);
#endif /* UNRELSIGS */
    (void) alarm(0);
    reset();

#ifndef SIGVOID
    return (snum);
#endif
}


static void
getremotehost()
{
    const char *host = NULL;
#ifdef INET6
    struct sockaddr_storage saddr;
    socklen_t len = sizeof(struct sockaddr_storage);
    static char hbuf[NI_MAXHOST];
#else
    struct hostent* hp;
    struct sockaddr_in saddr;
    int len = sizeof(struct sockaddr_in);
#endif
#if defined(UTHOST) && !defined(HAVENOUTMP)
    char *sptr = NULL;
#endif

#ifdef INET6
    if (getpeername(SHIN, (struct sockaddr *) &saddr, &len) != -1 &&
	(saddr.ss_family == AF_INET6 || saddr.ss_family == AF_INET)) {
	int flag = NI_NUMERICHOST;

#ifdef NI_WITHSCOPEID
	flag |= NI_WITHSCOPEID;
#endif
	getnameinfo((struct sockaddr *)&saddr, len, hbuf, sizeof(hbuf),
		    NULL, 0, flag);
	host = hbuf;
#else
    if (getpeername(SHIN, (struct sockaddr *) &saddr, &len) != -1 &&
	saddr.sin_family == AF_INET) {
#if 0
	if ((hp = gethostbyaddr((char *)&saddr.sin_addr, sizeof(struct in_addr),
				AF_INET)) != NULL)
	    host = hp->h_name;
	else
#endif
	    host = inet_ntoa(saddr.sin_addr);
#endif
    }
#if defined(UTHOST) && !defined(HAVENOUTMP)
    else {
	char *ptr;
	char *name = utmphost();
	/* Avoid empty names and local X displays */
	if (name != NULL && *name != '\0' && *name != ':') {
	    struct in_addr addr;

	    /* Look for host:display.screen */
	    /*
	     * There is conflict with IPv6 address and X DISPLAY.  So,
	     * we assume there is no IPv6 address in utmp and don't
	     * touch here.
	     */
	    if ((sptr = strchr(name, ':')) != NULL)
		*sptr = '\0';
	    /* Leave IPv4 address as is */
	    /*
	     * we use inet_addr here, not inet_aton because many systems
	     * have not caught up yet.
	     */
	    addr.s_addr = inet_addr(name);
	    if (addr.s_addr != (unsigned int)~0)
		host = name;
	    else {
		if (sptr != name) {
#ifdef INET6
		    char *s, *domain;
		    char dbuf[MAXHOSTNAMELEN], cbuf[MAXHOSTNAMELEN];
		    struct addrinfo hints, *res = NULL;

		    memset(&hints, 0, sizeof(hints));
		    hints.ai_family = PF_UNSPEC;
		    hints.ai_socktype = SOCK_STREAM;
		    hints.ai_flags = AI_PASSIVE | AI_CANONNAME;
#if defined(UTHOST) && !defined(HAVENOUTMP)
		    if (strlen(name) < utmphostsize())
#else
		    if (name != NULL)
#endif
		    {
			if (getaddrinfo(name, NULL, &hints, &res) != 0)
			    res = NULL;
		    } else if (gethostname(dbuf, sizeof(dbuf) - 1) == 0 &&
			       (domain = strchr(dbuf, '.')) != NULL) {
			for (s = strchr(name, '.');
			     s != NULL; s = strchr(s + 1, '.')) {
			    if (*(s + 1) != '\0' &&
				(ptr = strstr(domain, s)) != NULL) {
				len = s - name;
				if (len + strlen(ptr) >= sizeof(cbuf))
				    break;
				strncpy(cbuf, name, len);
				strcpy(cbuf + len, ptr);
				if (getaddrinfo(cbuf, NULL, &hints, &res) != 0)
				    res = NULL;
				break;
			    }
			}
		    }
		    if (res != NULL) {
			if (res->ai_canonname != NULL) {
			    strncpy(hbuf, res->ai_canonname, sizeof(hbuf));
			    host = hbuf;
			}
			freeaddrinfo(res);
		    }
#else
		    if ((hp = gethostbyname(name)) == NULL) {
			/* Try again eliminating the trailing domain */
			if ((ptr = strchr(name, '.')) != NULL) {
			    *ptr = '\0';
			    if ((hp = gethostbyname(name)) != NULL)
				host = hp->h_name;
			    *ptr = '.';
			}
		    }
		    else
			host = hp->h_name;
#endif
		}
	    }
	}
    }
#endif

    if (host)
	tsetenv(STRREMOTEHOST, str2short(host));

#if defined(UTHOST) && !defined(HAVENOUTMP)
    if (sptr)
	*sptr = ':';
#endif
}


/*
 * From: <lesv@ppvku.ericsson.se> (Lennart Svensson)
 */
void 
remotehost()
{
    /* Don't get stuck if the resolver does not work! */
    signalfun_t osig = sigset(SIGALRM, palarm);

    jmp_buf_t osetexit;
    getexit(osetexit);

    (void) alarm(2);

    if (setexit() == 0)
	getremotehost();

    resexit(osetexit);

    (void) alarm(0);
    (void) sigset(SIGALRM, osig);

#ifdef YPBUGS
    /* From: casper@fwi.uva.nl (Casper H.S. Dik), for Solaris 2.3 */
    fix_yp_bugs();
#endif /* YPBUGS */

}
#endif /* REMOTEHOST */
