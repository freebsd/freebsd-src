/* $Header: /src/pub/tcsh/sh.exec.c,v 3.51 2000/11/11 23:03:36 christos Exp $ */
/*
 * sh.exec.c: Search, find, and execute a command!
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

RCSID("$Id: sh.exec.c,v 3.51 2000/11/11 23:03:36 christos Exp $")

#include "tc.h"
#include "tw.h"
#ifdef WINNT_NATIVE
#include <nt.const.h>
#endif /*WINNT_NATIVE*/

/*
 * C shell
 */

#ifndef OLDHASH
# define FASTHASH	/* Fast hashing is the default */
#endif /* OLDHASH */

/*
 * System level search and execute of a command.
 * We look in each directory for the specified command name.
 * If the name contains a '/' then we execute only the full path name.
 * If there is no search path then we execute only full path names.
 */

/*
 * As we search for the command we note the first non-trivial error
 * message for presentation to the user.  This allows us often
 * to show that a file has the wrong mode/no access when the file
 * is not in the last component of the search path, so we must
 * go on after first detecting the error.
 */
static char *exerr;		/* Execution error message */
static Char *expath;		/* Path for exerr */

/*
 * The two part hash function is designed to let texec() call the
 * more expensive hashname() only once and the simple hash() several
 * times (once for each path component checked).
 * Byte size is assumed to be 8.
 */
#define BITS_PER_BYTE	8

#ifdef FASTHASH
/*
 * xhash is an array of hash buckets which are used to hash execs.  If
 * it is allocated (havhash true), then to tell if ``name'' is
 * (possibly) presend in the i'th component of the variable path, look
 * at the [hashname(name)] bucket of size [hashwidth] bytes, in the [i
 * mod size*8]'th bit.  The cache size is defaults to a length of 1024
 * buckets, each 1 byte wide.  This implementation guarantees that
 * objects n bytes wide will be aligned on n byte boundaries.
 */
# define HSHMUL		241

static unsigned long *xhash = NULL;
static unsigned int hashlength = 0, uhashlength = 0;
static unsigned int hashwidth = 0, uhashwidth = 0;
static int hashdebug = 0;

# define hash(a, b)	(((a) * HSHMUL + (b)) % (hashlength))
# define widthof(t)	(sizeof(t) * BITS_PER_BYTE)
# define tbit(f, i, t)	(((t *) xhash)[(f)] &  \
			 (1L << (i & (widthof(t) - 1))))
# define tbis(f, i, t)	(((t *) xhash)[(f)] |= \
			 (1L << (i & (widthof(t) - 1))))
# define cbit(f, i)	tbit(f, i, unsigned char)
# define cbis(f, i)	tbis(f, i, unsigned char)
# define sbit(f, i)	tbit(f, i, unsigned short)
# define sbis(f, i)	tbis(f, i, unsigned short)
# define ibit(f, i)	tbit(f, i, unsigned int)
# define ibis(f, i)	tbis(f, i, unsigned int)
# define lbit(f, i)	tbit(f, i, unsigned long)
# define lbis(f, i)	tbis(f, i, unsigned long)

# define bit(f, i) (hashwidth==sizeof(unsigned char)  ? cbit(f,i) : \
 		    ((hashwidth==sizeof(unsigned short) ? sbit(f,i) : \
		     ((hashwidth==sizeof(unsigned int)   ? ibit(f,i) : \
		     lbit(f,i))))))
# define bis(f, i) (hashwidth==sizeof(unsigned char)  ? cbis(f,i) : \
 		    ((hashwidth==sizeof(unsigned short) ? sbis(f,i) : \
		     ((hashwidth==sizeof(unsigned int)   ? ibis(f,i) : \
		     lbis(f,i))))))
#else /* OLDHASH */
/*
 * Xhash is an array of HSHSIZ bits (HSHSIZ / 8 chars), which are used
 * to hash execs.  If it is allocated (havhash true), then to tell
 * whether ``name'' is (possibly) present in the i'th component
 * of the variable path, you look at the bit in xhash indexed by
 * hash(hashname("name"), i).  This is setup automatically
 * after .login is executed, and recomputed whenever ``path'' is
 * changed.
 */
# define HSHSIZ		8192	/* 1k bytes */
# define HSHMASK		(HSHSIZ - 1)
# define HSHMUL		243
static char xhash[HSHSIZ / BITS_PER_BYTE];

# define hash(a, b)	(((a) * HSHMUL + (b)) & HSHMASK)
# define bit(h, b)	((h)[(b) >> 3] & 1 << ((b) & 7))	/* bit test */
# define bis(h, b)	((h)[(b) >> 3] |= 1 << ((b) & 7))	/* bit set */

#endif /* FASTHASH */

#ifdef VFORK
static int hits, misses;
#endif /* VFORK */

/* Dummy search path for just absolute search when no path */
static Char *justabs[] = {STRNULL, 0};

static	void	pexerr		__P((void));
static	void	texec		__P((Char *, Char **));
int	hashname	__P((Char *));
static	int 	iscommand	__P((Char *));

void
doexec(t)
    register struct command *t;
{
    register Char *dp, **pv, **av, *sav;
    register struct varent *v;
    register bool slash;
    register int hashval, i;
    Char   *blk[2];

    /*
     * Glob the command name. We will search $path even if this does something,
     * as in sh but not in csh.  One special case: if there is no PATH, then we
     * execute only commands which start with '/'.
     */
    blk[0] = t->t_dcom[0];
    blk[1] = 0;
    gflag = 0, tglob(blk);
    if (gflag) {
	pv = globall(blk);
	if (pv == 0) {
	    setname(short2str(blk[0]));
	    stderror(ERR_NAME | ERR_NOMATCH);
	}
	gargv = 0;
    }
    else
	pv = saveblk(blk);

    trim(pv);

    exerr = 0;
    expath = Strsave(pv[0]);
#ifdef VFORK
    Vexpath = expath;
#endif /* VFORK */

    v = adrof(STRpath);
    if (v == 0 && expath[0] != '/' && expath[0] != '.') {
	blkfree(pv);
	pexerr();
    }
    slash = any(short2str(expath), '/');

    /*
     * Glob the argument list, if necessary. Otherwise trim off the quote bits.
     */
    gflag = 0;
    av = &t->t_dcom[1];
    tglob(av);
    if (gflag) {
	av = globall(av);
	if (av == 0) {
	    blkfree(pv);
	    setname(short2str(expath));
	    stderror(ERR_NAME | ERR_NOMATCH);
	}
	gargv = 0;
    }
    else
	av = saveblk(av);

    blkfree(t->t_dcom);
    t->t_dcom = blkspl(pv, av);
    xfree((ptr_t) pv);
    xfree((ptr_t) av);
    av = t->t_dcom;
    trim(av);

    if (*av == NULL || **av == '\0')
	pexerr();

    xechoit(av);		/* Echo command if -x */
#ifdef CLOSE_ON_EXEC
    /*
     * Since all internal file descriptors are set to close on exec, we don't
     * need to close them explicitly here.  Just reorient ourselves for error
     * messages.
     */
    SHIN = 0;
    SHOUT = 1;
    SHDIAG = 2;
    OLDSTD = 0;
    isoutatty = isatty(SHOUT);
    isdiagatty = isatty(SHDIAG);
#else
    closech();			/* Close random fd's */
#endif
    /*
     * We must do this AFTER any possible forking (like `foo` in glob) so that
     * this shell can still do subprocesses.
     */
#ifdef BSDSIGS
    (void) sigsetmask((sigmask_t) 0);
#else /* BSDSIGS */
    (void) sigrelse(SIGINT);
    (void) sigrelse(SIGCHLD);
#endif /* BSDSIGS */

    /*
     * If no path, no words in path, or a / in the filename then restrict the
     * command search.
     */
    if (v == 0 || v->vec[0] == 0 || slash)
	pv = justabs;
    else
	pv = v->vec;
    sav = Strspl(STRslash, *av);/* / command name for postpending */
#ifdef VFORK
    Vsav = sav;
#endif /* VFORK */
    hashval = havhash ? hashname(*av) : 0;

    i = 0;
#ifdef VFORK
    hits++;
#endif /* VFORK */
    do {
	/*
	 * Try to save time by looking at the hash table for where this command
	 * could be.  If we are doing delayed hashing, then we put the names in
	 * one at a time, as the user enters them.  This is kinda like Korn
	 * Shell's "tracked aliases".
	 */
	if (!slash && ABSOLUTEP(pv[0]) && havhash) {
#ifdef FASTHASH
	    if (!bit(hashval, i))
		goto cont;
#else /* OLDHASH */
	    int hashval1 = hash(hashval, i);
	    if (!bit(xhash, hashval1))
		goto cont;
#endif /* FASTHASH */
	}
	if (pv[0][0] == 0 || eq(pv[0], STRdot))	/* don't make ./xxx */
	{

#ifdef COHERENT
	    if (t->t_dflg & F_AMPERSAND) {
# ifdef JOBDEBUG
    	        xprintf("set SIGINT to SIG_IGN\n");
    	        xprintf("set SIGQUIT to SIG_DFL\n");
# endif /* JOBDEBUG */
    	        (void) signal(SIGINT,SIG_IGN); /* may not be necessary */
	        (void) signal(SIGQUIT,SIG_DFL);
	    }

	    if (gointr && eq(gointr, STRminus)) {
# ifdef JOBDEBUG
    	        xprintf("set SIGINT to SIG_IGN\n");
    	        xprintf("set SIGQUIT to SIG_IGN\n");
# endif /* JOBDEBUG */
    	        (void) signal(SIGINT,SIG_IGN); /* may not be necessary */
	        (void) signal(SIGQUIT,SIG_IGN);
	    }
#endif /* COHERENT */

	    texec(*av, av);
}
	else {
	    dp = Strspl(*pv, sav);
#ifdef VFORK
	    Vdp = dp;
#endif /* VFORK */

#ifdef COHERENT
	    if ((t->t_dflg & F_AMPERSAND)) {
# ifdef JOBDEBUG
    	        xprintf("set SIGINT to SIG_IGN\n");
# endif /* JOBDEBUG */
		/* 
		 * this is necessary on Coherent or all background 
		 * jobs are killed by CTRL-C 
		 * (there must be a better fix for this) 
		 */
    	        (void) signal(SIGINT,SIG_IGN); 
	    }
	    if (gointr && eq(gointr,STRminus)) {
# ifdef JOBDEBUG
    	        xprintf("set SIGINT to SIG_IGN\n");
    	        xprintf("set SIGQUIT to SIG_IGN\n");
# endif /* JOBDEBUG */
    	        (void) signal(SIGINT,SIG_IGN); /* may not be necessary */
	        (void) signal(SIGQUIT,SIG_IGN);
	    }
#endif /* COHERENT */

	    texec(dp, av);
#ifdef VFORK
	    Vdp = 0;
#endif /* VFORK */
	    xfree((ptr_t) dp);
	}
#ifdef VFORK
	misses++;
#endif /* VFORK */
cont:
	pv++;
	i++;
    } while (*pv);
#ifdef VFORK
    hits--;
    Vsav = 0;
#endif /* VFORK */
    xfree((ptr_t) sav);
    pexerr();
}

static void
pexerr()
{
    /* Couldn't find the damn thing */
    if (expath) {
	setname(short2str(expath));
#ifdef VFORK
	Vexpath = 0;
#endif /* VFORK */
	xfree((ptr_t) expath);
	expath = 0;
    }
    else
	setname("");
    if (exerr)
	stderror(ERR_NAME | ERR_STRING, exerr);
    stderror(ERR_NAME | ERR_COMMAND);
}

/*
 * Execute command f, arg list t.
 * Record error message if not found.
 * Also do shell scripts here.
 */
static void
texec(sf, st)
    Char   *sf;
    register Char **st;
{
    register char **t;
    register char *f;
    register struct varent *v;
    Char  **vp;
    Char   *lastsh[2];
    char    pref[2];
    int     fd;
    Char   *st0, **ost;

    /* The order for the conversions is significant */
    t = short2blk(st);
    f = short2str(sf);
#ifdef VFORK
    Vt = t;
#endif /* VFORK */
    errno = 0;			/* don't use a previous error */
#ifdef apollo
    /*
     * If we try to execute an nfs mounted directory on the apollo, we
     * hang forever. So until apollo fixes that..
     */
    {
	struct stat stb;
	if (stat(f, &stb) == 0 && S_ISDIR(stb.st_mode))
	    errno = EISDIR;
    }
    if (errno == 0)
#endif /* apollo */
    {
#ifdef ISC_POSIX_EXEC_BUG
	__setostype(0);		/* "0" is "__OS_SYSV" in <sys/user.h> */
#endif /* ISC_POSIX_EXEC_BUG */
	(void) execv(f, t);
#ifdef ISC_POSIX_EXEC_BUG
	__setostype(1);		/* "1" is "__OS_POSIX" in <sys/user.h> */
#endif /* ISC_POSIX_EXEC_BUG */
    }
#ifdef VFORK
    Vt = 0;
#endif /* VFORK */
    blkfree((Char **) t);
    switch (errno) {

    case ENOEXEC:
#ifdef WINNT_NATIVE
		nt_feed_to_cmd(f,t);
#endif /* WINNT_NATIVE */
	/*
	 * From: casper@fwi.uva.nl (Casper H.S. Dik) If we could not execute
	 * it, don't feed it to the shell if it looks like a binary!
	 */
	if ((fd = open(f, O_RDONLY)) != -1) {
	    int nread;
	    if ((nread = read(fd, (char *) pref, 2)) == 2) {
		if (!Isprint(pref[0]) && (pref[0] != '\n' && pref[0] != '\t')) {
		    (void) close(fd);
		    /*
		     * We *know* what ENOEXEC means.
		     */
		    stderror(ERR_ARCH, f, strerror(errno));
		}
	    }
	    else if (nread < 0 && errno != EINTR) {
#ifdef convex
		/* need to print error incase the file is migrated */
		stderror(ERR_SYSTEM, f, strerror(errno));
#endif
	    }
#ifdef _PATH_BSHELL
	    else {
		pref[0] = '#';
		pref[1] = '\0';
	    }
#endif
	}
#ifdef HASHBANG
	if (fd == -1 ||
	    pref[0] != '#' || pref[1] != '!' || hashbang(fd, &vp) == -1) {
#endif /* HASHBANG */
	/*
	 * If there is an alias for shell, then put the words of the alias in
	 * front of the argument list replacing the command name. Note no
	 * interpretation of the words at this point.
	 */
	    v = adrof1(STRshell, &aliases);
	    if (v == 0) {
		vp = lastsh;
		vp[0] = adrof(STRshell) ? varval(STRshell) : STR_SHELLPATH;
		vp[1] = NULL;
#ifdef _PATH_BSHELL
		if (fd != -1 
# ifndef ISC	/* Compatible with ISC's /bin/csh */
		    && pref[0] != '#'
# endif /* ISC */
		    )
		    vp[0] = STR_BSHELL;
#endif
		vp = saveblk(vp);
	    }
	    else
		vp = saveblk(v->vec);
#ifdef HASHBANG
	}
#endif /* HASHBANG */
	if (fd != -1)
	    (void) close(fd);

	st0 = st[0];
	st[0] = sf;
	ost = st;
	st = blkspl(vp, st);	/* Splice up the new arglst */
	ost[0] = st0;
	sf = *st;
	/* The order for the conversions is significant */
	t = short2blk(st);
	f = short2str(sf);
	xfree((ptr_t) st);
	blkfree((Char **) vp);
#ifdef VFORK
	Vt = t;
#endif /* VFORK */
#ifdef ISC_POSIX_EXEC_BUG
	__setostype(0);		/* "0" is "__OS_SYSV" in <sys/user.h> */
#endif /* ISC_POSIX_EXEC_BUG */
	(void) execv(f, t);
#ifdef ISC_POSIX_EXEC_BUG
	__setostype(1);		/* "1" is "__OS_POSIX" in <sys/user.h> */
#endif /* ISC_POSIX_EXEC_BUG */
#ifdef VFORK
	Vt = 0;
#endif /* VFORK */
	blkfree((Char **) t);
	/* The sky is falling, the sky is falling! */
	stderror(ERR_SYSTEM, f, strerror(errno));
	break;

    case ENOMEM:
	stderror(ERR_SYSTEM, f, strerror(errno));
	break;

#ifdef _IBMR2
    case 0:			/* execv fails and returns 0! */
#endif /* _IBMR2 */
    case ENOENT:
	break;

    default:
	if (exerr == 0) {
	    exerr = strerror(errno);
	    if (expath)
		xfree((ptr_t) expath);
	    expath = Strsave(sf);
#ifdef VFORK
	    Vexpath = expath;
#endif /* VFORK */
	}
	break;
    }
}

/*ARGSUSED*/
void
execash(t, kp)
    Char  **t;
    register struct command *kp;
{
    int     saveIN, saveOUT, saveDIAG, saveSTD;
    int     oSHIN;
    int     oSHOUT;
    int     oSHDIAG;
    int     oOLDSTD;
    jmp_buf_t osetexit;
    int	    my_reenter;
    int     odidfds;
#ifndef CLOSE_ON_EXEC
    int	    odidcch;
#endif /* CLOSE_ON_EXEC */
    signalfun_t osigint, osigquit, osigterm;

    USE(t);
    if (chkstop == 0 && setintr)
	panystop(0);
    /*
     * Hmm, we don't really want to do that now because we might
     * fail, but what is the choice
     */
    rechist(NULL, adrof(STRsavehist) != NULL);


    osigint  = signal(SIGINT, parintr);
    osigquit = signal(SIGQUIT, parintr);
    osigterm = signal(SIGTERM, parterm);

    odidfds = didfds;
#ifndef CLOSE_ON_EXEC
    odidcch = didcch;
#endif /* CLOSE_ON_EXEC */
    oSHIN = SHIN;
    oSHOUT = SHOUT;
    oSHDIAG = SHDIAG;
    oOLDSTD = OLDSTD;

    saveIN = dcopy(SHIN, -1);
    saveOUT = dcopy(SHOUT, -1);
    saveDIAG = dcopy(SHDIAG, -1);
    saveSTD = dcopy(OLDSTD, -1);
	
    lshift(kp->t_dcom, 1);

    getexit(osetexit);

    /* PWP: setjmp/longjmp bugfix for optimizing compilers */
#ifdef cray
    my_reenter = 1;             /* assume non-zero return val */
    if (setexit() == 0) {
        my_reenter = 0;         /* Oh well, we were wrong */
#else /* !cray */
    if ((my_reenter = setexit()) == 0) {
#endif /* cray */
	SHIN = dcopy(0, -1);
	SHOUT = dcopy(1, -1);
	SHDIAG = dcopy(2, -1);
#ifndef CLOSE_ON_EXEC
	didcch = 0;
#endif /* CLOSE_ON_EXEC */
	didfds = 0;
	/*
	 * Decrement the shell level
	 */
	shlvl(-1);
#ifdef WINNT_NATIVE
	__nt_really_exec=1;
#endif /* WINNT_NATIVE */
	doexec(kp);
    }

    (void) sigset(SIGINT, osigint);
    (void) sigset(SIGQUIT, osigquit);
    (void) sigset(SIGTERM, osigterm);

    doneinp = 0;
#ifndef CLOSE_ON_EXEC
    didcch = odidcch;
#endif /* CLOSE_ON_EXEC */
    didfds = odidfds;
    (void) close(SHIN);
    (void) close(SHOUT);
    (void) close(SHDIAG);
    (void) close(OLDSTD);
    SHIN = dmove(saveIN, oSHIN);
    SHOUT = dmove(saveOUT, oSHOUT);
    SHDIAG = dmove(saveDIAG, oSHDIAG);
    OLDSTD = dmove(saveSTD, oOLDSTD);

    resexit(osetexit);
    if (my_reenter)
	stderror(ERR_SILENT);
}

void
xechoit(t)
    Char  **t;
{
    if (adrof(STRecho)) {
	flush();
	haderr = 1;
	blkpr(t), xputchar('\n');
	haderr = 0;
    }
}

/*ARGSUSED*/
void
dohash(vv, c)
    Char **vv;
    struct command *c;
{
#ifdef COMMENT
    struct stat stb;
#endif
    DIR    *dirp;
    register struct dirent *dp;
    int     i = 0;
    struct varent *v = adrof(STRpath);
    Char  **pv;
    int hashval;
#ifdef WINNT_NATIVE
    int is_windir; /* check if it is the windows directory */
    USE(hashval);
#endif /* WINNT_NATIVE */

    USE(c);
#ifdef FASTHASH
    if (vv && vv[1]) {
        uhashlength = atoi(short2str(vv[1]));
        if (vv[2]) {
	    uhashwidth = atoi(short2str(vv[2]));
	    if ((uhashwidth != sizeof(unsigned char)) && 
	        (uhashwidth != sizeof(unsigned short)) && 
	        (uhashwidth != sizeof(unsigned long)))
	        uhashwidth = 0;
	    if (vv[3])
		hashdebug = atoi(short2str(vv[3]));
        }
    }

    if (uhashwidth)
	hashwidth = uhashwidth;
    else {
	hashwidth = 0;
	if (v == NULL)
	    return;
	for (pv = v->vec; *pv; pv++, hashwidth++)
	    continue;
	if (hashwidth <= widthof(unsigned char))
	    hashwidth = sizeof(unsigned char);
	else if (hashwidth <= widthof(unsigned short))
	    hashwidth = sizeof(unsigned short);
	else if (hashwidth <= widthof(unsigned int))
	    hashwidth = sizeof(unsigned int);
	else
	    hashwidth = sizeof(unsigned long);
    }

    if (uhashlength)
	hashlength = uhashlength;
    else
        hashlength = hashwidth * (8*64);/* "average" files per dir in path */
    
    if (xhash)
        xfree((ptr_t) xhash);
    xhash = (unsigned long *) xcalloc((size_t) (hashlength * hashwidth), 
				      (size_t) 1);
#endif /* FASTHASH */

    (void) getusername(NULL);	/* flush the tilde cashe */
    tw_cmd_free();
    havhash = 1;
    if (v == NULL)
	return;
    for (pv = v->vec; *pv; pv++, i++) {
	if (!ABSOLUTEP(pv[0]))
	    continue;
	dirp = opendir(short2str(*pv));
	if (dirp == NULL)
	    continue;
#ifdef COMMENT			/* this isn't needed.  opendir won't open
				 * non-dirs */
	if (fstat(dirp->dd_fd, &stb) < 0 || !S_ISDIR(stb.st_mode)) {
	    (void) closedir(dirp);
	    continue;
	}
#endif
#ifdef WINNT_NATIVE
	is_windir = nt_check_if_windir(short2str(*pv));
#endif /* WINNT_NATIVE */
	while ((dp = readdir(dirp)) != NULL) {
	    if (dp->d_ino == 0)
		continue;
	    if (dp->d_name[0] == '.' &&
		(dp->d_name[1] == '\0' ||
		 (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		continue;
#ifdef WINNT_NATIVE
	    nt_check_name_and_hash(is_windir, dp->d_name, i);
#else /* !WINNT_NATIVE*/
#if defined(_UWIN) || defined(__CYGWIN__)
	    /* Turn foo.{exe,com,bat} into foo since UWIN's readdir returns
	     * the file with the .exe, .com, .bat extension
	     */
	    {
		size_t	ext = strlen(dp->d_name) - 4;
		if ((ext > 0) && (strcmp(&dp->d_name[ext], ".exe") == 0 ||
				  strcmp(&dp->d_name[ext], ".bat") == 0 ||
				  strcmp(&dp->d_name[ext], ".com") == 0))
		    dp->d_name[ext] = '\0';
	    }
#endif /* _UWIN || __CYGWIN__ */
# ifdef FASTHASH
	    hashval = hashname(str2short(dp->d_name));
	    bis(hashval, i);
	    if (hashdebug & 1)
	        xprintf(CGETS(13, 1, "hash=%-4d dir=%-2d prog=%s\n"),
		        hashname(str2short(dp->d_name)), i, dp->d_name);
# else /* OLD HASH */
	    hashval = hash(hashname(str2short(dp->d_name)), i);
	    bis(xhash, hashval);
# endif /* FASTHASH */
	    /* tw_add_comm_name (dp->d_name); */
#endif /* WINNT_NATIVE */
	}
	(void) closedir(dirp);
    }
}

/*ARGSUSED*/
void
dounhash(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
    havhash = 0;
#ifdef FASTHASH
    if (xhash) {
       xfree((ptr_t) xhash);
       xhash = NULL;
    }
#endif /* FASTHASH */
}

/*ARGSUSED*/
void
hashstat(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    USE(v);
#ifdef FASTHASH 
   if (havhash && hashlength && hashwidth)
      xprintf(CGETS(13, 2, "%d hash buckets of %d bits each\n"),
	      hashlength, hashwidth*8);
   if (hashdebug)
      xprintf(CGETS(13, 3, "debug mask = 0x%08x\n"), hashdebug);
#endif /* FASTHASH */
#ifdef VFORK
   if (hits + misses)
      xprintf(CGETS(13, 4, "%d hits, %d misses, %d%%\n"),
	      hits, misses, 100 * hits / (hits + misses));
#endif
}


/*
 * Hash a command name.
 */
int
hashname(cp)
    register Char *cp;
{
    register unsigned long h;

    for (h = 0; *cp; cp++)
	h = hash(h, *cp);
    return ((int) h);
}

static int
iscommand(name)
    Char   *name;
{
    register Char **pv;
    register Char *sav;
    register struct varent *v;
    register bool slash = any(short2str(name), '/');
    register int hashval, i;

    v = adrof(STRpath);
    if (v == 0 || v->vec[0] == 0 || slash)
	pv = justabs;
    else
	pv = v->vec;
    sav = Strspl(STRslash, name);	/* / command name for postpending */
    hashval = havhash ? hashname(name) : 0;
    i = 0;
    do {
	if (!slash && ABSOLUTEP(pv[0]) && havhash) {
#ifdef FASTHASH
	    if (!bit(hashval, i))
		goto cont;
#else /* OLDHASH */
	    int hashval1 = hash(hashval, i);
	    if (!bit(xhash, hashval1))
		goto cont;
#endif /* FASTHASH */
	}
	if (pv[0][0] == 0 || eq(pv[0], STRdot)) {	/* don't make ./xxx */
	    if (executable(NULL, name, 0)) {
		xfree((ptr_t) sav);
		return i + 1;
	    }
	}
	else {
	    if (executable(*pv, sav, 0)) {
		xfree((ptr_t) sav);
		return i + 1;
	    }
	}
cont:
	pv++;
	i++;
    } while (*pv);
    xfree((ptr_t) sav);
    return 0;
}

/* Also by:
 *  Andreas Luik <luik@isaak.isa.de>
 *  I S A  GmbH - Informationssysteme fuer computerintegrierte Automatisierung
 *  Azenberstr. 35
 *  D-7000 Stuttgart 1
 *  West-Germany
 * is the executable() routine below and changes to iscommand().
 * Thanks again!!
 */

#ifndef WINNT_NATIVE
/*
 * executable() examines the pathname obtained by concatenating dir and name
 * (dir may be NULL), and returns 1 either if it is executable by us, or
 * if dir_ok is set and the pathname refers to a directory.
 * This is a bit kludgy, but in the name of optimization...
 */
int
executable(dir, name, dir_ok)
    Char   *dir, *name;
    bool    dir_ok;
{
    struct stat stbuf;
    Char    path[MAXPATHLEN + 1];
    char   *strname;
    (void) memset(path, 0, sizeof(path));

    if (dir && *dir) {
	copyn(path, dir, MAXPATHLEN);
	catn(path, name, MAXPATHLEN);
	strname = short2str(path);
    }
    else
	strname = short2str(name);
    
    return (stat(strname, &stbuf) != -1 &&
	    ((dir_ok && S_ISDIR(stbuf.st_mode)) ||
	     (S_ISREG(stbuf.st_mode) &&
    /* save time by not calling access() in the hopeless case */
	      (stbuf.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)) &&
	      access(strname, X_OK) == 0
	)));
}
#endif /*!WINNT_NATIVE*/

int
tellmewhat(lexp, str)
    struct wordent *lexp;
    Char *str;
{
    register int i;
    register struct biltins *bptr;
    register struct wordent *sp = lexp->next;
    bool    aliased = 0, found;
    Char   *s0, *s1, *s2, *cmd;
    Char    qc;

    if (adrof1(sp->word, &aliases)) {
	alias(lexp);
	sp = lexp->next;
	aliased = 1;
    }

    s0 = sp->word;		/* to get the memory freeing right... */

    /* handle quoted alias hack */
    if ((*(sp->word) & (QUOTE | TRIM)) == QUOTE)
	(sp->word)++;

    /* do quoting, if it hasn't been done */
    s1 = s2 = sp->word;
    while (*s2)
	switch (*s2) {
	case '\'':
	case '"':
	    qc = *s2++;
	    while (*s2 && *s2 != qc)
		*s1++ = *s2++ | QUOTE;
	    if (*s2)
		s2++;
	    break;
	case '\\':
	    if (*++s2)
		*s1++ = *s2++ | QUOTE;
	    break;
	default:
	    *s1++ = *s2++;
	}
    *s1 = '\0';

    for (bptr = bfunc; bptr < &bfunc[nbfunc]; bptr++) {
	if (eq(sp->word, str2short(bptr->bname))) {
	    if (str == NULL) {
		if (aliased)
		    prlex(lexp);
		xprintf(CGETS(13, 5, "%S: shell built-in command.\n"),
			      sp->word);
		flush();
	    }
	    else 
		(void) Strcpy(str, sp->word);
	    sp->word = s0;	/* we save and then restore this */
	    return TRUE;
	}
    }
#ifdef WINNT_NATIVE
    for (bptr = nt_bfunc; bptr < &nt_bfunc[nt_nbfunc]; bptr++) {
	if (eq(sp->word, str2short(bptr->bname))) {
	    if (str == NULL) {
		if (aliased)
		    prlex(lexp);
		xprintf(CGETS(13, 5, "%S: shell built-in command.\n"),
			      sp->word);
		flush();
	    }
	    else 
		(void) Strcpy(str, sp->word);
	    sp->word = s0;	/* we save and then restore this */
	    return TRUE;
	}
    }
#endif /* WINNT_NATIVE*/

    sp->word = cmd = globone(sp->word, G_IGNORE);

    if ((i = iscommand(sp->word)) != 0) {
	register Char **pv;
	register struct varent *v;
	bool    slash = any(short2str(sp->word), '/');

	v = adrof(STRpath);
	if (v == 0 || v->vec[0] == 0 || slash)
	    pv = justabs;
	else
	    pv = v->vec;

	while (--i)
	    pv++;
	if (pv[0][0] == 0 || eq(pv[0], STRdot)) {
	    if (!slash) {
		sp->word = Strspl(STRdotsl, sp->word);
		prlex(lexp);
		xfree((ptr_t) sp->word);
	    }
	    else
		prlex(lexp);
	}
	else {
	    s1 = Strspl(*pv, STRslash);
	    sp->word = Strspl(s1, sp->word);
	    xfree((ptr_t) s1);
	    if (str == NULL)
		prlex(lexp);
	    else
		(void) Strcpy(str, sp->word);
	    xfree((ptr_t) sp->word);
	}
	found = 1;
    }
    else {
	if (str == NULL) {
	    if (aliased)
		prlex(lexp);
	    xprintf(CGETS(13, 6, "%S: Command not found.\n"), sp->word);
	    flush();
	}
	else
	    (void) Strcpy(str, sp->word);
	found = 0;
    }
    sp->word = s0;		/* we save and then restore this */
    xfree((ptr_t) cmd);
    return found;
}

/*
 * Builtin to look at and list all places a command may be defined:
 * aliases, shell builtins, and the path.
 *
 * Marc Horowitz <marc@mit.edu>
 * MIT Student Information Processing Board
 */

/*ARGSUSED*/
void
dowhere(v, c)
    register Char **v;
    struct command *c;
{
    int found = 1;
    USE(c);
    for (v++; *v; v++)
	found &= find_cmd(*v, 1);
    /* Make status nonzero if any command is not found. */
    if (!found)
      set(STRstatus, Strsave(STR1), VAR_READWRITE);
}

int
find_cmd(cmd, prt)
    Char *cmd;
    int prt;
{
    struct varent *var;
    struct biltins *bptr;
    Char **pv;
    Char *sv;
    int hashval, i, ex, rval = 0;

    if (prt && any(short2str(cmd), '/')) {
	xprintf(CGETS(13, 7, "where: / in command makes no sense\n"));
	return rval;
    }

    /* first, look for an alias */

    if (prt && adrof1(cmd, &aliases)) {
	if ((var = adrof1(cmd, &aliases)) != NULL) {
	    xprintf(CGETS(13, 8, "%S is aliased to "), cmd);
	    blkpr(var->vec);
	    xputchar('\n');
	    rval = 1;
	}
    }

    /* next, look for a shell builtin */

    for (bptr = bfunc; bptr < &bfunc[nbfunc]; bptr++) {
	if (eq(cmd, str2short(bptr->bname))) {
	    rval = 1;
	    if (prt)
		xprintf(CGETS(13, 9, "%S is a shell built-in\n"), cmd);
	    else
		return rval;
	}
    }
#ifdef WINNT_NATIVE
    for (bptr = nt_bfunc; bptr < &nt_bfunc[nt_nbfunc]; bptr++) {
	if (eq(cmd, str2short(bptr->bname))) {
	    rval = 1;
	    if (prt)
		xprintf(CGETS(13, 9, "%S is a shell built-in\n"), cmd);
	    else
		return rval;
	}
    }
#endif /* WINNT_NATIVE*/

    /* last, look through the path for the command */

    if ((var = adrof(STRpath)) == NULL)
	return rval;

    hashval = havhash ? hashname(cmd) : 0;

    sv = Strspl(STRslash, cmd);

    for (pv = var->vec, i = 0; *pv; pv++, i++) {
	if (havhash && !eq(*pv, STRdot)) {
#ifdef FASTHASH
	    if (!bit(hashval, i))
		continue;
#else /* OLDHASH */
	    int hashval1 = hash(hashval, i);
	    if (!bit(xhash, hashval1))
		continue;
#endif /* FASTHASH */
	}
	ex = executable(*pv, sv, 0);
#ifdef FASTHASH
	if (!ex && (hashdebug & 2)) {
	    xprintf(CGETS(13, 10, "hash miss: "));
	    ex = 1;	/* Force printing */
	}
#endif /* FASTHASH */
	if (ex) {
	    rval = 1;
	    if (prt) {
		xprintf("%S/", *pv);
		xprintf("%S\n", cmd);
	    }
	    else
		return rval;
	}
    }
    xfree((ptr_t) sv);
    return rval;
}
#ifdef WINNT_NATIVE
int hashval_extern(cp)
	Char *cp;
{
	return havhash?hashname(cp):0;
}
int bit_extern(val,i)
	int val;
	int i;
{
	return bit(val,i);
}
void bis_extern(val,i)
	int val;
	int i;
{
	bis(val,i);
}
#endif /* WINNT_NATIVE */

