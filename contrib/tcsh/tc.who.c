/* $Header: /src/pub/tcsh/tc.who.c,v 3.32 2000/11/12 02:18:07 christos Exp $ */
/*
 * tc.who.c: Watch logins and logouts...
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
#include "sh.h"

RCSID("$Id: tc.who.c,v 3.32 2000/11/12 02:18:07 christos Exp $")

#include "tc.h"

#ifndef HAVENOUTMP
/*
 * kfk 26 Jan 1984 - for login watch functions.
 */
#include <ctype.h>

#ifdef HAVEUTMPX
# include <utmpx.h>
/* I just redefine a few words here.  Changing every occurrence below
 * seems like too much of work.  All UTMP functions have equivalent
 * UTMPX counterparts, so they can be added all here when needed.
 * Kimmo Suominen, Oct 14 1991
 */
# ifndef _PATH_UTMP
#  if defined(__UTMPX_FILE) && !defined(UTMPX_FILE)
#   define _PATH_UTMP __UTMPX_FILE
#  else
#   define _PATH_UTMP UTMPX_FILE
#  endif /* __UTMPX_FILE && !UTMPX_FILE */
# endif /* _PATH_UTMP */
# define utmp utmpx
# ifdef __MVS__
#  define ut_time ut_tv.tv_sec
#  define ut_name ut_user
# else
#  define ut_time ut_xtime
# endif /* __MVS__ */
#else /* !HAVEUTMPX */
# ifndef WINNT_NATIVE
#  include <utmp.h>
# endif /* WINNT_NATIVE */
#endif /* HAVEUTMPX */

#ifndef BROKEN_CC
# define UTNAMLEN	sizeof(((struct utmp *) 0)->ut_name)
# define UTLINLEN	sizeof(((struct utmp *) 0)->ut_line)
# ifdef UTHOST
#  ifdef _SEQUENT_
#   define UTHOSTLEN	100
#  else
#   define UTHOSTLEN	sizeof(((struct utmp *) 0)->ut_host)
#  endif
# endif	/* UTHOST */
#else
/* give poor cc a little help if it needs it */
struct utmp __ut;

# define UTNAMLEN	sizeof(__ut.ut_name)
# define UTLINLEN	sizeof(__ut.ut_line)
# ifdef UTHOST
#  ifdef _SEQUENT_
#   define UTHOSTLEN	100
#  else
#   define UTHOSTLEN	sizeof(__ut.ut_host)
#  endif
# endif /* UTHOST */
#endif /* BROKEN_CC */

#ifndef _PATH_UTMP
# ifdef	UTMP_FILE
#  define _PATH_UTMP UTMP_FILE
# else
#  define _PATH_UTMP "/etc/utmp"
# endif /* UTMP_FILE */
#endif /* _PATH_UTMP */


struct who {
    struct who *who_next;
    struct who *who_prev;
    char    who_name[UTNAMLEN + 1];
    char    who_new[UTNAMLEN + 1];
    char    who_tty[UTLINLEN + 1];
#ifdef UTHOST
    char    who_host[UTHOSTLEN + 1];
#endif /* UTHOST */
    time_t  who_time;
    int     who_status;
};

static struct who whohead, whotail;
static time_t watch_period = 0;
static time_t stlast = 0;
#ifdef WHODEBUG
static	void	debugwholist	__P((struct who *, struct who *));
#endif
static	void	print_who	__P((struct who *));


#define ONLINE		01
#define OFFLINE		02
#define CHANGED		04
#define STMASK		07
#define ANNOUNCE	010

/*
 * Karl Kleinpaste, 26 Jan 1984.
 * Initialize the dummy tty list for login watch.
 * This dummy list eliminates boundary conditions
 * when doing pointer-chase searches.
 */
void
initwatch()
{
    whohead.who_next = &whotail;
    whotail.who_prev = &whohead;
    stlast = 1;
#ifdef WHODEBUG
    debugwholist(NULL, NULL);
#endif /* WHODEBUG */
}

void
resetwatch()
{
    watch_period = 0;
    stlast = 0;
}

/*
 * Karl Kleinpaste, 26 Jan 1984.
 * Watch /etc/utmp for login/logout changes.
 */
void
watch_login(force)
    int force;
{
    int     utmpfd, comp = -1, alldone;
    int	    firsttime = stlast == 1;
#ifdef BSDSIGS
    sigmask_t omask;
#endif				/* BSDSIGS */
    struct utmp utmp;
    struct who *wp, *wpnew;
    struct varent *v;
    Char  **vp = NULL;
    time_t  t, interval = MAILINTVL;
    struct stat sta;
#if defined(UTHOST) && defined(_SEQUENT_)
    char   *host, *ut_find_host();
#endif
#ifdef WINNT_NATIVE
    static int ncbs_posted = 0;
    USE(utmp);
    USE(utmpfd);
    USE(sta);
    USE(wpnew);
#endif /* WINNT_NATIVE */

    /* stop SIGINT, lest our login list get trashed. */
#ifdef BSDSIGS
    omask = sigblock(sigmask(SIGINT));
#else
    (void) sighold(SIGINT);
#endif

    v = adrof(STRwatch);
    if (v == NULL && !force) {
#ifdef BSDSIGS
	(void) sigsetmask(omask);
#else
	(void) sigrelse(SIGINT);
#endif
	return;			/* no names to watch */
    }
    if (!force) {
	trim(vp = v->vec);
	if (blklen(vp) % 2)		/* odd # args: 1st == # minutes. */
	    interval = (number(*vp)) ? (getn(*vp++) * 60) : MAILINTVL;
    }
    else
	interval = 0;
	
    (void) time(&t);
#ifdef WINNT_NATIVE
	/*
	 * Since NCB_ASTATs take time, start em async at least 90 secs
	 * before we are due -amol 6/5/97
	 */
	if (!ncbs_posted) {
	    unsigned long tdiff = t - watch_period;
	    if (!watch_period || ((tdiff  > 0) && (tdiff > (interval - 90)))) {
		start_ncbs(vp);
 		ncbs_posted = 1;
	    }
	}
#endif /* WINNT_NATIVE */
    if (t - watch_period < interval) {
#ifdef BSDSIGS
	(void) sigsetmask(omask);
#else
	(void) sigrelse(SIGINT);
#endif
	return;			/* not long enough yet... */
    }
    watch_period = t;
#ifdef WINNT_NATIVE
    ncbs_posted = 0;
#else /* !WINNT_NATIVE */

    /*
     * From: Michael Schroeder <mlschroe@immd4.informatik.uni-erlangen.de>
     * Don't open utmp all the time, stat it first...
     */
    if (stat(_PATH_UTMP, &sta)) {
	if (!force)
	    xprintf(CGETS(26, 1,
			  "cannot stat %s.  Please \"unset watch\".\n"),
		    _PATH_UTMP);
# ifdef BSDSIGS
	(void) sigsetmask(omask);
# else
	(void) sigrelse(SIGINT);
# endif
	return;
    }
    if (stlast == sta.st_mtime) {
# ifdef BSDSIGS
	(void) sigsetmask(omask);
# else
	(void) sigrelse(SIGINT);
# endif
	return;
    }
    stlast = sta.st_mtime;
    if ((utmpfd = open(_PATH_UTMP, O_RDONLY)) < 0) {
	if (!force)
	    xprintf(CGETS(26, 2,
			  "%s cannot be opened.  Please \"unset watch\".\n"),
		    _PATH_UTMP);
# ifdef BSDSIGS
	(void) sigsetmask(omask);
# else
	(void) sigrelse(SIGINT);
# endif
	return;
    }

    /*
     * xterm clears the entire utmp entry - mark everyone on the status list
     * OFFLINE or we won't notice X "logouts"
     */
    for (wp = whohead.who_next; wp->who_next != NULL; wp = wp->who_next) {
	wp->who_status = OFFLINE;
	wp->who_time = 0;
    }

    /*
     * Read in the utmp file, sort the entries, and update existing entries or
     * add new entries to the status list.
     */
    while (read(utmpfd, (char *) &utmp, sizeof utmp) == sizeof utmp) {

# ifdef DEAD_PROCESS
#  ifndef IRIS4D
	if (utmp.ut_type != USER_PROCESS)
	    continue;
#  else
	/* Why is that? Cause the utmp file is always corrupted??? */
	if (utmp.ut_type != USER_PROCESS && utmp.ut_type != DEAD_PROCESS)
	    continue;
#  endif /* IRIS4D */
# endif /* DEAD_PROCESS */

	if (utmp.ut_name[0] == '\0' && utmp.ut_line[0] == '\0')
	    continue;	/* completely void entry */
# ifdef DEAD_PROCESS
	if (utmp.ut_type == DEAD_PROCESS && utmp.ut_line[0] == '\0')
	    continue;
# endif /* DEAD_PROCESS */
	wp = whohead.who_next;
	while (wp->who_next && (comp = strncmp(wp->who_tty, utmp.ut_line, UTLINLEN)) < 0)
	    wp = wp->who_next;/* find that tty! */

	if (wp->who_next && comp == 0) {	/* found the tty... */
# ifdef DEAD_PROCESS
	    if (utmp.ut_type == DEAD_PROCESS) {
		wp->who_time = utmp.ut_time;
		wp->who_status = OFFLINE;
	    }
	    else
# endif /* DEAD_PROCESS */
	    if (utmp.ut_name[0] == '\0') {
		wp->who_time = utmp.ut_time;
		wp->who_status = OFFLINE;
	    }
	    else if (strncmp(utmp.ut_name, wp->who_name, UTNAMLEN) == 0) {
		/* someone is logged in */ 
		wp->who_time = utmp.ut_time;
		wp->who_status = 0;	/* same guy */
	    }
	    else {
		(void) strncpy(wp->who_new, utmp.ut_name, UTNAMLEN);
# ifdef UTHOST
#  ifdef _SEQUENT_
		host = ut_find_host(wp->who_tty);
		if (host)
		    (void) strncpy(wp->who_host, host, UTHOSTLEN);
		else
		    wp->who_host[0] = 0;
#  else
		(void) strncpy(wp->who_host, utmp.ut_host, UTHOSTLEN);
#  endif
# endif /* UTHOST */
		wp->who_time = utmp.ut_time;
		if (wp->who_name[0] == '\0')
		    wp->who_status = ONLINE;
		else
		    wp->who_status = CHANGED;
	    }
	}
	else {		/* new tty in utmp */
	    wpnew = (struct who *) xcalloc(1, sizeof *wpnew);
	    (void) strncpy(wpnew->who_tty, utmp.ut_line, UTLINLEN);
# ifdef UTHOST
#  ifdef _SEQUENT_
	    host = ut_find_host(wpnew->who_tty);
	    if (host)
		(void) strncpy(wpnew->who_host, host, UTHOSTLEN);
	    else
		wpnew->who_host[0] = 0;
#  else
	    (void) strncpy(wpnew->who_host, utmp.ut_host, UTHOSTLEN);
#  endif
# endif /* UTHOST */
	    wpnew->who_time = utmp.ut_time;
# ifdef DEAD_PROCESS
	    if (utmp.ut_type == DEAD_PROCESS)
		wpnew->who_status = OFFLINE;
	    else
# endif /* DEAD_PROCESS */
	    if (utmp.ut_name[0] == '\0')
		wpnew->who_status = OFFLINE;
	    else {
		(void) strncpy(wpnew->who_new, utmp.ut_name, UTNAMLEN);
		wpnew->who_status = ONLINE;
	    }
# ifdef WHODEBUG
	    debugwholist(wpnew, wp);
# endif /* WHODEBUG */

	    wpnew->who_next = wp;	/* link in a new 'who' */
	    wpnew->who_prev = wp->who_prev;
	    wpnew->who_prev->who_next = wpnew;
	    wp->who_prev = wpnew;	/* linked in now */
	}
    }
    (void) close(utmpfd);
# if defined(UTHOST) && defined(_SEQUENT_)
    endutent();
# endif
#endif /* !WINNT_NATIVE */

    if (force || vp == NULL)
	return;

    /*
     * The state of all logins is now known, so we can search the user's list
     * of watchables to print the interesting ones.
     */
    for (alldone = 0; !alldone && *vp != NULL && **vp != '\0' &&
	 *(vp + 1) != NULL && **(vp + 1) != '\0';
	 vp += 2) {		/* args used in pairs... */

	if (eq(*vp, STRany) && eq(*(vp + 1), STRany))
	    alldone = 1;

	for (wp = whohead.who_next; wp->who_next != NULL; wp = wp->who_next) {
	    if (wp->who_status & ANNOUNCE ||
		(!eq(STRany, vp[0]) &&
		 !Gmatch(str2short(wp->who_name), vp[0]) &&
		 !Gmatch(str2short(wp->who_new),  vp[0])) ||
		(!Gmatch(str2short(wp->who_tty),  vp[1]) &&
		 !eq(STRany, vp[1])))
		continue;	/* entry doesn't qualify */
	    /* already printed or not right one to print */


	    if (wp->who_time == 0)/* utmp entry was cleared */
		wp->who_time = watch_period;

	    if ((wp->who_status & OFFLINE) &&
		(wp->who_name[0] != '\0')) {
		if (!firsttime)
		    print_who(wp);
		wp->who_name[0] = '\0';
		wp->who_status |= ANNOUNCE;
		continue;
	    }
	    if (wp->who_status & ONLINE) {
		if (!firsttime)
		    print_who(wp);
		(void) strcpy(wp->who_name, wp->who_new);
		wp->who_status |= ANNOUNCE;
		continue;
	    }
	    if (wp->who_status & CHANGED) {
		if (!firsttime)
		    print_who(wp);
		(void) strcpy(wp->who_name, wp->who_new);
		wp->who_status |= ANNOUNCE;
		continue;
	    }
	}
    }
#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else
    (void) sigrelse(SIGINT);
#endif
}

#ifdef WHODEBUG
static void
debugwholist(new, wp)
    register struct who *new, *wp;
{
    register struct who *a;

    a = whohead.who_next;
    while (a->who_next != NULL) {
	xprintf("%s/%s -> ", a->who_name, a->who_tty);
	a = a->who_next;
    }
    xprintf("TAIL\n");
    if (a != &whotail) {
	xprintf(CGETS(26, 3, "BUG! last element is not whotail!\n"));
	abort();
    }
    a = whotail.who_prev;
    xprintf(CGETS(26, 4, "backward: "));
    while (a->who_prev != NULL) {
	xprintf("%s/%s -> ", a->who_name, a->who_tty);
	a = a->who_prev;
    }
    xprintf("HEAD\n");
    if (a != &whohead) {
	xprintf(CGETS(26, 5, "BUG! first element is not whohead!\n"));
	abort();
    }
    if (new)
	xprintf(CGETS(26, 6, "new: %s/%s\n"), new->who_name, new->who_tty);
    if (wp)
	xprintf("wp: %s/%s\n", wp->who_name, wp->who_tty);
}
#endif /* WHODEBUG */


static void
print_who(wp)
    struct who *wp;
{
#ifdef UTHOST
    Char   *cp = str2short(CGETS(26, 7, "%n has %a %l from %m."));
#else
    Char   *cp = str2short(CGETS(26, 8, "%n has %a %l."));
#endif /* UTHOST */
    struct varent *vp = adrof(STRwho);
    Char buf[BUFSIZE];

    if (vp && vp->vec[0])
	cp = vp->vec[0];

    tprintf(FMT_WHO, buf, cp, BUFSIZE, NULL, wp->who_time, (ptr_t) wp);
    for (cp = buf; *cp;)
	xputchar(*cp++);
    xputchar('\n');
} /* end print_who */


const char *
who_info(ptr, c, wbuf, wbufsiz)
    ptr_t ptr;
    int c;
    char *wbuf;
    size_t wbufsiz;
{
    struct who *wp = (struct who *) ptr;
#ifdef UTHOST
    char *wb = wbuf;
    int flg;
    char *pb;
#endif /* UTHOST */

    switch (c) {
    case 'n':		/* user name */
	switch (wp->who_status & STMASK) {
	case ONLINE:
	case CHANGED:
	    return wp->who_new;
	case OFFLINE:
	    return wp->who_name;
	default:
	    break;
	}
	break;

    case 'a':
	switch (wp->who_status & STMASK) {
	case ONLINE:
	    return CGETS(26, 9, "logged on");
	case OFFLINE:
	    return CGETS(26, 10, "logged off");
	case CHANGED:
	    xsnprintf(wbuf, wbufsiz, CGETS(26, 11, "replaced %s on"),
		      wp->who_name);
	    return wbuf;
	default:
	    break;
	}
	break;

#ifdef UTHOST
    case 'm':
	if (wp->who_host[0] == '\0')
	    return CGETS(26, 12, "local");
	else {
	    /* the ':' stuff is for <host>:<display>.<screen> */
	    for (pb = wp->who_host, flg = Isdigit(*pb) ? '\0' : '.';
		 *pb != '\0' &&
		 (*pb != flg || ((pb = strchr(pb, ':')) != 0));
		 pb++) {
		if (*pb == ':')
		    flg = '\0';
		*wb++ = Isupper(*pb) ? Tolower(*pb) : *pb;
	    }
	    *wb = '\0';
	    return wbuf;
	}

    case 'M':
	if (wp->who_host[0] == '\0')
	    return CGETS(26, 12, "local");
	else {
	    for (pb = wp->who_host; *pb != '\0'; pb++)
		*wb++ = Isupper(*pb) ? Tolower(*pb) : *pb;
	    *wb = '\0';
	    return wbuf;
	}
#endif /* UTHOST */

    case 'l':
	return wp->who_tty;

    default:
	wbuf[0] = '%';
	wbuf[1] = (char) c;
	wbuf[2] = '\0';
	return wbuf;
    }
    return NULL;
}

void
/*ARGSUSED*/
dolog(v, c)
Char **v;
struct command *c;
{
    struct who *wp;
    struct varent *vp;

    USE(v);
    USE(c);
    vp = adrof(STRwatch);	/* lint insists vp isn't used unless we */
    if (vp == NULL)		/* unless we assign it outside the if */
	stderror(ERR_NOWATCH);
    resetwatch();
    wp = whohead.who_next;
    while (wp->who_next != NULL) {
	wp->who_name[0] = '\0';
	wp = wp->who_next;
    }
}

# ifdef UTHOST
size_t
utmphostsize()
{
    return UTHOSTLEN;
}

char *
utmphost()
{
    char *tty = short2str(varval(STRtty));
    struct who *wp;
    char *host = NULL;

    watch_login(1);
    
    for (wp = whohead.who_next; wp->who_next != NULL; wp = wp->who_next) {
	if (strcmp(tty, wp->who_tty) == 0)
	    host = wp->who_host;
	wp->who_name[0] = '\0';
    }
    resetwatch();
    return host;
}
# endif /* UTHOST */

#ifdef WINNT_NATIVE
void add_to_who_list(name, mach_nm)
    char *name;
    char *mach_nm;
{

    struct who *wp, *wpnew;
    int comp = -1;

    wp = whohead.who_next;
    while (wp->who_next && (comp = strncmp(wp->who_tty,mach_nm,UTLINLEN)) < 0)
	wp = wp->who_next;/* find that tty! */

    if (wp->who_next && comp == 0) {	/* found the tty... */

	if (*name == '\0') {
	    wp->who_time = 0;
	    wp->who_status = OFFLINE;
	}
	else if (strncmp(name, wp->who_name, UTNAMLEN) == 0) {
	    /* someone is logged in */ 
	    wp->who_time = 0;
	    wp->who_status = 0;	/* same guy */
	}
	else {
	    (void) strncpy(wp->who_new, name, UTNAMLEN);
	    wp->who_time = 0;
	    if (wp->who_name[0] == '\0')
		wp->who_status = ONLINE;
	    else
		wp->who_status = CHANGED;
	}
    }
    else {
	wpnew = (struct who *) xcalloc(1, sizeof *wpnew);
	(void) strncpy(wpnew->who_tty, mach_nm, UTLINLEN);
	wpnew->who_time = 0;
	if (*name == '\0')
	    wpnew->who_status = OFFLINE;
	else {
	    (void) strncpy(wpnew->who_new, name, UTNAMLEN);
	    wpnew->who_status = ONLINE;
	}
#ifdef WHODEBUG
	debugwholist(wpnew, wp);
#endif /* WHODEBUG */

	wpnew->who_next = wp;	/* link in a new 'who' */
	wpnew->who_prev = wp->who_prev;
	wpnew->who_prev->who_next = wpnew;
	wp->who_prev = wpnew;	/* linked in now */
    }
}
#endif /* WINNT_NATIVE */
#endif /* HAVENOUTMP */
