/* $Header: /src/pub/tcsh/sh.dir.c,v 3.60 2002/07/08 21:03:04 christos Exp $ */
/*
 * sh.dir.c: Directory manipulation functions
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

RCSID("$Id: sh.dir.c,v 3.60 2002/07/08 21:03:04 christos Exp $")

/*
 * C Shell - directory management
 */

static	void			 dstart		__P((const char *));
static	struct directory	*dfind		__P((Char *));
static	Char 			*dfollow	__P((Char *));
static	void 	 	 	 printdirs	__P((int));
static	Char 			*dgoto		__P((Char *));
static	void 	 	 	 dnewcwd	__P((struct directory *, int));
static	void 	 	 	 dset		__P((Char *));
static  void 			 dextract	__P((struct directory *));
static  int 			 skipargs	__P((Char ***, char *, char *));
static	void			 dgetstack	__P((void));

static struct directory dhead INIT_ZERO_STRUCT;		/* "head" of loop */
static int    printd;			/* force name to be printed */

int     bequiet = 0;		/* do not print dir stack -strike */

static void
dstart(from)
    const char *from;
{
    xprintf(CGETS(12, 1, "%s: Trying to start from \"%s\"\n"), progname, from);
}

/*
 * dinit - initialize current working directory
 */
void
dinit(hp)
    Char   *hp;
{
    register char *tcp;
    register Char *cp;
    register struct directory *dp;
    char    path[MAXPATHLEN];

    /* Don't believe the login shell home, because it may be a symlink */
    tcp = (char *) getcwd(path, sizeof(path));
    if (tcp == NULL || *tcp == '\0') {
	xprintf("%s: %s\n", progname, strerror(errno));
	if (hp && *hp) {
	    tcp = short2str(hp);
	    dstart(tcp);
	    if (chdir(tcp) == -1)
		cp = NULL;
	    else
		cp = Strsave(hp);
	}
	else
	    cp = NULL;
	if (cp == NULL) {
	    dstart("/");
	    if (chdir("/") == -1)
		/* I am not even try to print an error message! */
		xexit(1);
	    cp = SAVE("/");
	}
    }
    else {
#ifdef S_IFLNK
	struct stat swd, shp;

	/*
	 * See if $HOME is the working directory we got and use that
	 */
	if (hp && *hp &&
	    stat(tcp, &swd) != -1 && stat(short2str(hp), &shp) != -1 &&
	    DEV_DEV_COMPARE(swd.st_dev, shp.st_dev)  &&
		swd.st_ino == shp.st_ino)
	    cp = Strsave(hp);
	else {
	    char   *cwd;

	    /*
	     * use PWD if we have it (for subshells)
	     */
	    if ((cwd = getenv("PWD")) != NULL) {
		if (stat(cwd, &shp) != -1 && 
			DEV_DEV_COMPARE(swd.st_dev, shp.st_dev) &&
		    swd.st_ino == shp.st_ino)
		    tcp = cwd;
	    }
	    cp = dcanon(SAVE(tcp), STRNULL);
	}
#else /* S_IFLNK */
	cp = dcanon(SAVE(tcp), STRNULL);
#endif /* S_IFLNK */
    }

    dp = (struct directory *) xcalloc(sizeof(struct directory), 1);
    dp->di_name = cp;
    dp->di_count = 0;
    dhead.di_next = dhead.di_prev = dp;
    dp->di_next = dp->di_prev = &dhead;
    printd = 0;
    dnewcwd(dp, 0);
    set(STRdirstack, Strsave(dp->di_name), VAR_READWRITE|VAR_NOGLOB);
}

static void
dset(dp)
Char *dp;
{
    /*
     * Don't call set() directly cause if the directory contains ` or
     * other junk characters glob will fail. 
     */
    set(STRowd, Strsave(varval(STRcwd)), VAR_READWRITE|VAR_NOGLOB);
    set(STRcwd, Strsave(dp), VAR_READWRITE|VAR_NOGLOB);

    tsetenv(STRPWD, dp);
}

#define DIR_PRINT	0x01	/* -p */
#define DIR_LONG  	0x02	/* -l */
#define DIR_VERT  	0x04	/* -v */
#define DIR_LINE  	0x08	/* -n */
#define DIR_SAVE 	0x10	/* -S */
#define DIR_LOAD	0x20	/* -L */
#define DIR_CLEAR	0x40	/* -c */
#define DIR_OLD	  	0x80	/* - */

static int
skipargs(v, dstr, str)
    Char ***v;
    char   *dstr;
    char   *str;
{
    Char  **n = *v, *s;

    int dflag = 0, loop = 1;
    for (n++; loop && *n != NULL && (*n)[0] == '-'; n++) 
	if (*(s = &((*n)[1])) == '\0')	/* test for bare "-" argument */
	    dflag |= DIR_OLD;
	else {
	    char *p;
	    while (loop && *s != '\0')	/* examine flags */
	    {
		if ((p = strchr(dstr, *s++)) != NULL)
		    dflag |= (1 << (p - dstr));
	        else {
		    stderror(ERR_DIRUS, short2str(**v), dstr, str);
		    loop = 0;	/* break from both loops */
		    break;
	        }
	    }
	}
    if (*n && (dflag & DIR_OLD))
	stderror(ERR_DIRUS, short2str(**v), dstr, str);
    *v = n;
    /* make -l, -v, and -n imply -p */
    if (dflag & (DIR_LONG|DIR_VERT|DIR_LINE))
	dflag |= DIR_PRINT;
    return dflag;
}

/*
 * dodirs - list all directories in directory loop
 */
/*ARGSUSED*/
void
dodirs(v, c)
    Char  **v;
    struct command *c;
{
    static char flags[] = "plvnSLc";
    int dflag = skipargs(&v, flags, "");

    USE(c);
    if ((dflag & DIR_CLEAR) != 0) {
	struct directory *dp, *fdp;
	for (dp = dcwd->di_next; dp != dcwd; ) {
	    fdp = dp;
	    dp = dp->di_next;
	    if (fdp != &dhead)
		dfree(fdp);
	}
	dhead.di_next = dhead.di_prev = dp;
	dp->di_next = dp->di_prev = &dhead;
    }
    if ((dflag & DIR_LOAD) != 0) 
	loaddirs(*v);
    else if ((dflag & DIR_SAVE) != 0)
	recdirs(*v, 1);

    if (*v && (dflag & (DIR_SAVE|DIR_LOAD)))
	v++;

    if (*v != NULL || (dflag & DIR_OLD))
	stderror(ERR_DIRUS, "dirs", flags, "");
    if ((dflag & (DIR_CLEAR|DIR_LOAD|DIR_SAVE)) == 0 || (dflag & DIR_PRINT))
	printdirs(dflag);
}

static void
printdirs(dflag)
    int dflag;
{
    register struct directory *dp;
    Char   *s, *user;
    int     idx, len, cur;
    extern int T_Cols;

    dp = dcwd;
    idx = 0;
    cur = 0;
    do {
	if (dp == &dhead)
	    continue;
	if (dflag & DIR_VERT) {
	    xprintf("%d\t", idx++);
	    cur = 0;
	}
	s = dp->di_name;		
	user = NULL;
	if (!(dflag & DIR_LONG) && (user = getusername(&s)) != NULL)
	    len = (int) (Strlen(user) + Strlen(s) + 2);
	else
	    len = (int) (Strlen(s) + 1);

	cur += len;
	if ((dflag & DIR_LINE) && cur >= T_Cols - 1 && len < T_Cols) {
	    xputchar('\n');
	    cur = len;
	}
	if (user) 
	    xprintf("~%S", user);
	xprintf("%S%c", s, (dflag & DIR_VERT) ? '\n' : ' ');
    } while ((dp = dp->di_prev) != dcwd);
    if (!(dflag & DIR_VERT))
	xputchar('\n');
}

void
dtildepr(dir)
    Char *dir;
{
    Char* user;
    if ((user = getusername(&dir)) != NULL)
	xprintf("~%S%S", user, dir);
    else
	xprintf("%S", dir);
}

void
dtilde()
{
    struct directory *d = dcwd;

    do {
	if (d == &dhead)
	    continue;
	d->di_name = dcanon(d->di_name, STRNULL);
    } while ((d = d->di_prev) != dcwd);

    dset(dcwd->di_name);
}


/* dnormalize():
 *	The path will be normalized if it
 *	1) is "..",
 *	2) or starts with "../",
 *	3) or ends with "/..",
 *	4) or contains the string "/../",
 *	then it will be normalized, unless those strings are quoted. 
 *	Otherwise, a copy is made and sent back.
 */
Char   *
dnormalize(cp, exp)
    Char   *cp;
    int exp;
{

/* return true if dp is of the form "../xxx" or "/../xxx" */
#define IS_DOTDOT(sp, p) (ISDOTDOT(p) && ((p) == (sp) || *((p) - 1) == '/'))
#define IS_DOT(sp, p) (ISDOT(p) && ((p) == (sp) || *((p) - 1) == '/'))

#ifdef S_IFLNK
    if (exp) {
 	int     dotdot = 0;
	Char   *dp, *cwd, *start = cp, buf[MAXPATHLEN];
	struct stat sb;
# ifdef apollo
	bool slashslash;
# endif /* apollo */

	/*
	 * count the number of "../xxx" or "xxx/../xxx" in the path
	 */
	for (dp=start; *dp && *(dp+1); dp++)
	    if (IS_DOTDOT(start, dp))
	        dotdot++;
	/*
	 * if none, we are done.
	 */
        if (dotdot == 0)
	    return (Strsave(cp));

	/*
	 * If the path doesn't exist, we are done too.
	 */
	if (lstat(short2str(cp), &sb) != 0 && errno == ENOENT)
	    return (Strsave(cp));
	

	cwd = (Char *) xmalloc((size_t) (((int) Strlen(dcwd->di_name) + 3) *
					   sizeof(Char)));
	(void) Strcpy(cwd, dcwd->di_name);

	/*
	 * If the path starts with a slash, we are not relative to
	 * the current working directory.
	 */
	if (ABSOLUTEP(start))
	    *cwd = '\0';
# ifdef apollo
	slashslash = cwd[0] == '/' && cwd[1] == '/';
# endif /* apollo */

	/*
	 * Ignore . and count ..'s
	 */
	for (;;) {
	    dotdot = 0;
	    buf[0] = '\0';
	    dp = buf; 
	    while (*cp) 
	        if (IS_DOT(start, cp)) {
	            if (*++cp)
	                cp++;
	        }
	        else if (IS_DOTDOT(start, cp)) {
		    if (buf[0])
		        break; /* finish analyzing .././../xxx/[..] */
		    dotdot++;
		    cp += 2;
		    if (*cp)
		        cp++;
	        }
	        else 
			*dp++ = *cp++;

	    *dp = '\0';
	    while (dotdot > 0) 
	        if ((dp = Strrchr(cwd, '/')) != NULL) {
# ifdef apollo
		    if (dp == &cwd[1]) 
		        slashslash = 1;
# endif /* apollo */
		        *dp = '\0';
		        dotdot--;
	        }
	        else
		    break;

	    if (!*cwd) {	/* too many ..'s, starts with "/" */
	        cwd[0] = '/';
# ifdef apollo
		cwd[1] = '/';
		cwd[2] = '\0';
# else /* !apollo */
		cwd[1] = '\0';
# endif /* apollo */
	    }
# ifdef apollo
	    else if (slashslash && cwd[1] == '\0') {
		cwd[1] = '/';
		cwd[2] = '\0';
	    }
# endif /* apollo */

	    if (buf[0]) {
	        if ((TRM(cwd[(dotdot = (int) Strlen(cwd)) - 1])) != '/')
		    cwd[dotdot++] = '/';
	        cwd[dotdot] = '\0';
	        dp = Strspl(cwd, TRM(buf[0]) == '/' ? &buf[1] : buf);
	        xfree((ptr_t) cwd);
	        cwd = dp;
	        if ((TRM(cwd[(dotdot = (int) Strlen(cwd)) - 1])) == '/')
		    cwd[--dotdot] = '\0';
	    }
	    /* Reduction of ".." following the stuff we collected in buf
	     * only makes sense if the directory item in buf really exists.
	     * Avoid reduction of "-I../.." (typical compiler call) to ""
	     * or "/usr/nonexistant/../bin" to "/usr/bin":
	     */
	    if (cwd[0]) {
	        struct stat exists;
		if (0 != stat(short2str(cwd), &exists)) {
		    xfree((ptr_t) cwd);
		    return Strsave(start);
		}
	    }
	    if (!*cp)
	        break;
	}
	return cwd;
    }
#endif /* S_IFLNK */
    return Strsave(cp);
}


/*
 * dochngd - implement chdir command.
 */
/*ARGSUSED*/
void
dochngd(v, c)
    Char  **v;
    struct command *c;
{
    register Char *cp;
    register struct directory *dp;
    int dflag = skipargs(&v, "plvn", "[-|<dir>]");

    USE(c);
    printd = 0;
    cp = (dflag & DIR_OLD) ? varval(STRowd) : *v;

    if (cp == NULL) {
	if ((cp = varval(STRhome)) == STRNULL || *cp == 0)
	    stderror(ERR_NAME | ERR_NOHOMEDIR);
	if (chdir(short2str(cp)) < 0)
	    stderror(ERR_NAME | ERR_CANTCHANGE);
	cp = Strsave(cp);
    }
    else if ((dflag & DIR_OLD) == 0 && v[1] != NULL) {
	stderror(ERR_NAME | ERR_TOOMANY);
	/* NOTREACHED */
	return;
    }
    else if ((dp = dfind(cp)) != 0) {
	char   *tmp;

	printd = 1;
	if (chdir(tmp = short2str(dp->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
	dcwd->di_prev->di_next = dcwd->di_next;
	dcwd->di_next->di_prev = dcwd->di_prev;
	dfree(dcwd);
	dnewcwd(dp, dflag);
	return;
    }
    else
	if ((cp = dfollow(cp)) == NULL)
	    return;
    dp = (struct directory *) xcalloc(sizeof(struct directory), 1);
    dp->di_name = cp;
    dp->di_count = 0;
    dp->di_next = dcwd->di_next;
    dp->di_prev = dcwd->di_prev;
    dp->di_prev->di_next = dp;
    dp->di_next->di_prev = dp;
    dfree(dcwd);
    dnewcwd(dp, dflag);
}

static Char *
dgoto(cp)
    Char   *cp;
{
    Char   *dp;

    if (!ABSOLUTEP(cp))
    {
	register Char *p, *q;
	int     cwdlen;

	for (p = dcwd->di_name; *p++;)
	    continue;
	if ((cwdlen = (int) (p - dcwd->di_name - 1)) == 1)	/* root */
	    cwdlen = 0;
	for (p = cp; *p++;)
	    continue;
	dp = (Char *) xmalloc((size_t)((cwdlen + (p - cp) + 1) * sizeof(Char)));
	for (p = dp, q = dcwd->di_name; (*p++ = *q++) != '\0';)
	    continue;
	if (cwdlen)
	    p[-1] = '/';
	else
	    p--;		/* don't add a / after root */
	for (q = cp; (*p++ = *q++) != '\0';)
	    continue;
	xfree((ptr_t) cp);
	cp = dp;
	dp += cwdlen;
    }
    else
	dp = cp;

#ifdef WINNT_NATIVE
    cp = SAVE(getcwd(NULL, 0));
#else /* !WINNT_NATIVE */
    cp = dcanon(cp, dp);
#endif /* WINNT_NATIVE */
    return cp;
}

/*
 * dfollow - change to arg directory; fall back on cdpath if not valid
 */
static Char *
dfollow(cp)
    register Char *cp;
{
    register Char *dp;
    struct varent *c;
    char    ebuf[MAXPATHLEN];
    int serrno;

    cp = globone(cp, G_ERROR);
#ifdef apollo
    if (Strchr(cp, '`')) {
	char *dptr, *ptr;
	if (chdir(dptr = short2str(cp)) < 0) 
	    stderror(ERR_SYSTEM, dptr, strerror(errno));
	else if ((ptr = getcwd(ebuf, sizeof(ebuf))) && *ptr != '\0') {
		xfree((ptr_t) cp);
		cp = Strsave(str2short(ptr));
		return dgoto(cp);
	}
	else 
	    stderror(ERR_SYSTEM, dptr, ebuf);
    }
#endif /* apollo */
	    
    (void) strncpy(ebuf, short2str(cp), MAXPATHLEN);
    ebuf[MAXPATHLEN-1] = '\0';
    /*
     * if we are ignoring symlinks, try to fix relatives now.
     * if we are expading symlinks, it should be done by now.
     */ 
    dp = dnormalize(cp, symlinks == SYM_IGNORE);
    if (chdir(short2str(dp)) >= 0) {
        xfree((ptr_t) cp);
        return dgoto(dp);
    }
    else {
        xfree((ptr_t) dp);
        if (chdir(short2str(cp)) >= 0)
	    return dgoto(cp);
	else if (errno != ENOENT && errno != ENOTDIR)
	    stderror(ERR_SYSTEM, ebuf, strerror(errno));
	serrno = errno;
    }

    if (cp[0] != '/' && !prefix(STRdotsl, cp) && !prefix(STRdotdotsl, cp)
	&& (c = adrof(STRcdpath)) && c->vec != NULL) {
	Char  **cdp;
	register Char *p;
	Char    buf[MAXPATHLEN];

	for (cdp = c->vec; *cdp; cdp++) {
	    for (dp = buf, p = *cdp; (*dp++ = *p++) != '\0';)
		continue;
	    dp[-1] = '/';
	    for (p = cp; (*dp++ = *p++) != '\0';)
		continue;
	    /*
	     * We always want to fix the directory here
	     * If we are normalizing symlinks
	     */
	    dp = dnormalize(buf, symlinks == SYM_IGNORE || 
				 symlinks == SYM_EXPAND);
	    if (chdir(short2str(dp)) >= 0) {
		printd = 1;
		xfree((ptr_t) cp);
		return dgoto(dp);
	    }
	    else if (chdir(short2str(cp)) >= 0) {
		printd = 1;
		xfree((ptr_t) dp);
		return dgoto(cp);
	    }
	}
    }
    dp = varval(cp);
    if ((dp[0] == '/' || dp[0] == '.') && chdir(short2str(dp)) >= 0) {
	xfree((ptr_t) cp);
	cp = Strsave(dp);
	printd = 1;
	return dgoto(cp);
    }
    xfree((ptr_t) cp);
    /*
     * on login source of ~/.cshdirs, errors are eaten. the dir stack is all
     * directories we could get to.
     */
    if (!bequiet) {
	stderror(ERR_SYSTEM, ebuf, strerror(serrno));
	return (NULL);
    }
    else
	return (NULL);
}


/*
 * dopushd - push new directory onto directory stack.
 *	with no arguments exchange top and second.
 *	with numeric argument (+n) bring it to top.
 */
/*ARGSUSED*/
void
dopushd(v, c)
    Char  **v;
    struct command *c;
{
    register struct directory *dp;
    register Char *cp;
    int dflag = skipargs(&v, "plvn", " [-|<dir>|+<n>]");
    
    USE(c);
    printd = 1;
    cp = (dflag & DIR_OLD) ? varval(STRowd) : *v;

    if (cp == NULL) {
	if (adrof(STRpushdtohome)) {
	    if ((cp = varval(STRhome)) == STRNULL || *cp == 0)
		stderror(ERR_NAME | ERR_NOHOMEDIR);
	    if (chdir(short2str(cp)) < 0)
		stderror(ERR_NAME | ERR_CANTCHANGE);
	    cp = Strsave(cp);	/* hmmm... PWP */
	    if ((cp = dfollow(cp)) == NULL)
		return;
	    dp = (struct directory *) xcalloc(sizeof(struct directory), 1);
	    dp->di_name = cp;
	    dp->di_count = 0;
	    dp->di_prev = dcwd;
	    dp->di_next = dcwd->di_next;
	    dcwd->di_next = dp;
	    dp->di_next->di_prev = dp;
	}
	else {
	    char   *tmp;

	    if ((dp = dcwd->di_prev) == &dhead)
		dp = dhead.di_prev;
	    if (dp == dcwd)
		stderror(ERR_NAME | ERR_NODIR);
	    if (chdir(tmp = short2str(dp->di_name)) < 0)
		stderror(ERR_SYSTEM, tmp, strerror(errno));
	    dp->di_prev->di_next = dp->di_next;
	    dp->di_next->di_prev = dp->di_prev;
	    dp->di_next = dcwd->di_next;
	    dp->di_prev = dcwd;
	    dcwd->di_next->di_prev = dp;
	    dcwd->di_next = dp;
	}
    }
    else if ((dflag & DIR_OLD) == 0 && v[1] != NULL) {
	stderror(ERR_NAME | ERR_TOOMANY);
	/* NOTREACHED */
	return;
    }
    else if ((dp = dfind(cp)) != NULL) {
	char   *tmp;

	if (chdir(tmp = short2str(dp->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
	/*
	 * kfk - 10 Feb 1984 - added new "extraction style" pushd +n
	 */
	if (adrof(STRdextract))
	    dextract(dp);
    }
    else {
	register Char *ccp;

	if ((ccp = dfollow(cp)) == NULL)
	    return;
	dp = (struct directory *) xcalloc(sizeof(struct directory), 1);
	dp->di_name = ccp;
	dp->di_count = 0;
	dp->di_prev = dcwd;
	dp->di_next = dcwd->di_next;
	dcwd->di_next = dp;
	dp->di_next->di_prev = dp;
    }
    dnewcwd(dp, dflag);
}

/*
 * dfind - find a directory if specified by numeric (+n) argument
 */
static struct directory *
dfind(cp)
    register Char *cp;
{
    register struct directory *dp;
    register int i;
    register Char *ep;

    if (*cp++ != '+')
	return (0);
    for (ep = cp; Isdigit(*ep); ep++)
	continue;
    if (*ep)
	return (0);
    i = getn(cp);
    if (i <= 0)
	return (0);
    for (dp = dcwd; i != 0; i--) {
	if ((dp = dp->di_prev) == &dhead)
	    dp = dp->di_prev;
	if (dp == dcwd)
	    stderror(ERR_NAME | ERR_DEEP);
    }
    return (dp);
}

/*
 * dopopd - pop a directory out of the directory stack
 *	with a numeric argument just discard it.
 */
/*ARGSUSED*/
void
dopopd(v, c)
    Char  **v;
    struct command *c;
{
    Char *cp;
    register struct directory *dp, *p = NULL;
    int dflag = skipargs(&v, "plvn", " [-|+<n>]");

    USE(c);
    printd = 1;
    cp = (dflag & DIR_OLD) ? varval(STRowd) : *v;

    if (cp == NULL)
	dp = dcwd;
    else if ((dflag & DIR_OLD) == 0 && v[1] != NULL) {
	stderror(ERR_NAME | ERR_TOOMANY);
	/* NOTREACHED */
	return;
    }
    else if ((dp = dfind(cp)) == 0)
	stderror(ERR_NAME | ERR_BADDIR);
    if (dp->di_prev == &dhead && dp->di_next == &dhead)
	stderror(ERR_NAME | ERR_EMPTY);
    if (dp == dcwd) {
	char   *tmp;

	if ((p = dp->di_prev) == &dhead)
	    p = dhead.di_prev;
	if (chdir(tmp = short2str(p->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
    }
    dp->di_prev->di_next = dp->di_next;
    dp->di_next->di_prev = dp->di_prev;
    if (dp == dcwd) {
	dnewcwd(p, dflag);
    }
    else {
	printdirs(dflag);
    }
    dfree(dp);
}

/*
 * dfree - free the directory (or keep it if it still has ref count)
 */
void
dfree(dp)
    register struct directory *dp;
{

    if (dp->di_count != 0) {
	dp->di_next = dp->di_prev = 0;
    }
    else {
	xfree((ptr_t) dp->di_name);
	xfree((ptr_t) dp);
    }
}

/*
 * dcanon - canonicalize the pathname, removing excess ./ and ../ etc.
 *	we are of course assuming that the file system is standardly
 *	constructed (always have ..'s, directories have links)
 */
Char   *
dcanon(cp, p)
    register Char *cp, *p;
{
    register Char *sp;
    register Char *p1, *p2;	/* general purpose */
    bool    slash;
#ifdef apollo
    bool    slashslash;
#endif /* apollo */
    size_t  clen;

#ifdef S_IFLNK			/* if we have symlinks */
    Char    link[MAXPATHLEN];
    char    tlink[MAXPATHLEN];
    int     cc;
    Char   *newcp;
#endif /* S_IFLNK */

    /*
     * if the path given is too long truncate it!
     */
    if ((clen = Strlen(cp)) >= MAXPATHLEN)
	cp[clen = MAXPATHLEN - 1] = '\0';

    /*
     * christos: if the path given does not start with a slash prepend cwd. If
     * cwd does not start with a slash or the result would be too long try to
     * correct it.
     */
    if (!ABSOLUTEP(cp)) {
	Char    tmpdir[MAXPATHLEN];
	size_t	len;

	p1 = varval(STRcwd);
	if (p1 == STRNULL || !ABSOLUTEP(p1)) {
	    char *tmp = (char *)getcwd((char *)tmpdir, sizeof(tmpdir));
	    if (tmp == NULL || *tmp == '\0') {
		xprintf("%s: %s\n", progname, strerror(errno));
		set(STRcwd, SAVE("/"), VAR_READWRITE|VAR_NOGLOB);
	    } else {
		set(STRcwd, SAVE(tmp), VAR_READWRITE|VAR_NOGLOB);
	    }
	    p1 = varval(STRcwd);
	}
	len = Strlen(p1);
	if (len + clen + 1 >= MAXPATHLEN)
	    cp[MAXPATHLEN - (len + 1)] = '\0';
	(void) Strcpy(tmpdir, p1);
	(void) Strcat(tmpdir, STRslash);
	(void) Strcat(tmpdir, cp);
	xfree((ptr_t) cp);
	cp = p = Strsave(tmpdir);
    }

#ifdef apollo
    slashslash = (cp[0] == '/' && cp[1] == '/');
#endif /* apollo */

    while (*p) {		/* for each component */
	sp = p;			/* save slash address */
	while (*++p == '/')	/* flush extra slashes */
	    continue;
	if (p != ++sp)
	    for (p1 = sp, p2 = p; (*p1++ = *p2++) != '\0';)
		continue;
	p = sp;			/* save start of component */
	slash = 0;
	if (*p) 
	    while (*++p)	/* find next slash or end of path */
		if (*p == '/') {
		    slash = 1;
		    *p = 0;
		    break;
		}

#ifdef apollo
	if (&cp[1] == sp && sp[0] == '.' && sp[1] == '.' && sp[2] == '\0')
	    slashslash = 1;
#endif /* apollo */
	if (*sp == '\0') {	/* if component is null */
	    if (--sp == cp)	/* if path is one char (i.e. /) */ 
		break;
	    else
		*sp = '\0';
	}
	else if (sp[0] == '.' && sp[1] == 0) {
	    if (slash) {
		for (p1 = sp, p2 = p + 1; (*p1++ = *p2++) != '\0';)
		    continue;
		p = --sp;
	    }
	    else if (--sp != cp)
		*sp = '\0';
	    else
		sp[1] = '\0';
	}
	else if (sp[0] == '.' && sp[1] == '.' && sp[2] == 0) {
	    /*
	     * We have something like "yyy/xxx/..", where "yyy" can be null or
	     * a path starting at /, and "xxx" is a single component. Before
	     * compressing "xxx/..", we want to expand "yyy/xxx", if it is a
	     * symbolic link.
	     */
	    *--sp = 0;		/* form the pathname for readlink */
#ifdef S_IFLNK			/* if we have symlinks */
	    if (sp != cp && /* symlinks != SYM_IGNORE && */
		(cc = readlink(short2str(cp), tlink,
			       sizeof tlink)) >= 0) {
		tlink[cc] = '\0';
		(void) Strncpy(link, str2short(tlink),
		    sizeof(link) / sizeof(Char));
		link[sizeof(link) / sizeof(Char) - 1] = '\0';

		if (slash)
		    *p = '/';
		/*
		 * Point p to the '/' in "/..", and restore the '/'.
		 */
		*(p = sp) = '/';
		/*
		 * find length of p
		 */
		for (p1 = p; *p1++;)
		    continue;
		if (*link != '/') {
		    /*
		     * Relative path, expand it between the "yyy/" and the
		     * "/..". First, back sp up to the character past "yyy/".
		     */
		    while (*--sp != '/')
			continue;
		    sp++;
		    *sp = 0;
		    /*
		     * New length is "yyy/" + link + "/.." and rest
		     */
		    p1 = newcp = (Char *) xmalloc((size_t)
						(((sp - cp) + cc + (p1 - p)) *
						 sizeof(Char)));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = cp; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = link; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at expanded "/xxx".
		     */
		    p = sp - cp - 1 + newcp;
		}
		else {
		    /*
		     * New length is link + "/.." and rest
		     */
		    p1 = newcp = (Char *) xmalloc((size_t)
					    ((cc + (p1 - p)) * sizeof(Char)));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = link; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at beginning
		     */
		    p = newcp;
		}
		xfree((ptr_t) cp);
		cp = newcp;
#ifdef apollo
                slashslash = (cp[0] == '/' && cp[1] == '/');
#endif /* apollo */
		continue;	/* canonicalize the link */
	    }
#endif /* S_IFLNK */
	    *sp = '/';
	    if (sp != cp)
		while (*--sp != '/')
		    continue;
	    if (slash) {
		for (p1 = sp + 1, p2 = p + 1; (*p1++ = *p2++) != '\0';)
		    continue;
		p = sp;
	    }
	    else if (cp == sp)
		*++sp = '\0';
	    else
		*sp = '\0';
	}
	else {			/* normal dir name (not . or .. or nothing) */

#ifdef S_IFLNK			/* if we have symlinks */
	    if (sp != cp && symlinks == SYM_CHASE &&
		(cc = readlink(short2str(cp), tlink,
			       sizeof tlink)) >= 0) {
		tlink[cc] = '\0';
		(void) Strncpy(link, str2short(tlink),
		    sizeof(link) / sizeof(Char));
		link[sizeof(link) / sizeof(Char) - 1] = '\0';

		/*
		 * restore the '/'.
		 */
		if (slash)
		    *p = '/';

		/*
		 * point sp to p (rather than backing up).
		 */
		sp = p;

		/*
		 * find length of p
		 */
		for (p1 = p; *p1++;)
		    continue;
		if (*link != '/') {
		    /*
		     * Relative path, expand it between the "yyy/" and the
		     * remainder. First, back sp up to the character past
		     * "yyy/".
		     */
		    while (*--sp != '/')
			continue;
		    sp++;
		    *sp = 0;
		    /*
		     * New length is "yyy/" + link + "/.." and rest
		     */
		    p1 = newcp = (Char *) xmalloc((size_t)
						  (((sp - cp) + cc + (p1 - p))
						   * sizeof(Char)));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = cp; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = link; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at expanded "/xxx".
		     */
		    p = sp - cp - 1 + newcp;
		}
		else {
		    /*
		     * New length is link + the rest
		     */
		    p1 = newcp = (Char *) xmalloc((size_t)
					    ((cc + (p1 - p)) * sizeof(Char)));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = link; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at beginning
		     */
		    p = newcp;
		}
		xfree((ptr_t) cp);
		cp = newcp;
#ifdef apollo
                slashslash = (cp[0] == '/' && cp[1] == '/');
#endif /* apollo */
		continue;	/* canonicalize the link */
	    }
#endif /* S_IFLNK */
	    if (slash)
		*p = '/';
	}
    }

    /*
     * fix home...
     */
#ifdef S_IFLNK
    p1 = varval(STRhome);
    cc = (int) Strlen(p1);
    /*
     * See if we're not in a subdir of STRhome
     */
    if (p1 && *p1 == '/' && (Strncmp(p1, cp, (size_t) cc) != 0 ||
	(cp[cc] != '/' && cp[cc] != '\0'))) {
	static ino_t home_ino = (ino_t) -1;
	static dev_t home_dev = (dev_t) -1;
	static Char *home_ptr = NULL;
	struct stat statbuf;
	int found;

	/*
	 * Get dev and ino of STRhome
	 */
	if (home_ptr != p1 &&
	    stat(short2str(p1), &statbuf) != -1) {
	    home_dev = statbuf.st_dev;
	    home_ino = statbuf.st_ino;
	    home_ptr = p1;
	}
	/*
	 * Start comparing dev & ino backwards
	 */
	p2 = Strncpy(link, cp, sizeof(link) / sizeof(Char));
	link[sizeof(link) / sizeof(Char) - 1] = '\0';
	found = 0;
	while (*p2 && stat(short2str(p2), &statbuf) != -1) {
	    if (DEV_DEV_COMPARE(statbuf.st_dev, home_dev) &&
			statbuf.st_ino == home_ino) {
			found = 1;
			break;
	    }
	    if ((sp = Strrchr(p2, '/')) != NULL)
		*sp = '\0';
	}
	/*
	 * See if we found it
	 */
	if (*p2 && found) {
	    /*
	     * Use STRhome to make '~' work
	     */
	    newcp = Strspl(p1, cp + Strlen(p2));
	    xfree((ptr_t) cp);
	    cp = newcp;
	}
    }
#endif /* S_IFLNK */

#ifdef apollo
    if (slashslash) {
	if (cp[1] != '/') {
	    p = (Char *) xmalloc((size_t) (Strlen(cp) + 2) * sizeof(Char));
	    *p = '/';
	    (void) Strcpy(&p[1], cp);
	    xfree((ptr_t) cp);
	    cp = p;
	}
    }
    if (cp[1] == '/' && cp[2] == '/') 
	(void) Strcpy(&cp[1], &cp[2]);
#endif /* apollo */
    return cp;
}


/*
 * dnewcwd - make a new directory in the loop the current one
 */
static void
dnewcwd(dp, dflag)
    register struct directory *dp;
    int dflag;
{
    int print;

    if (adrof(STRdunique)) {
	struct directory *dn;

	for (dn = dhead.di_prev; dn != &dhead; dn = dn->di_prev) 
	    if (dn != dp && Strcmp(dn->di_name, dp->di_name) == 0) {
		dn->di_next->di_prev = dn->di_prev;
		dn->di_prev->di_next = dn->di_next;
		dfree(dn);
		break;
	    }
    }
    dcwd = dp;
    dset(dcwd->di_name);
    dgetstack();
    print = printd;		/* if printd is set, print dirstack... */
    if (adrof(STRpushdsilent))	/* but pushdsilent overrides printd... */
	print = 0;
    if (dflag & DIR_PRINT)	/* but DIR_PRINT overrides pushdsilent... */
	print = 1;
    if (bequiet)		/* and bequiet overrides everything */
	print = 0;
    if (print)
	printdirs(dflag);
    cwd_cmd();			/* PWP: run the defined cwd command */
}

void
dsetstack()
{
    Char **cp;
    struct varent *vp;
    struct directory *dn, *dp;

    if ((vp = adrof(STRdirstack)) == NULL || vp->vec == NULL)
	return;

    /* Free the whole stack */
    while ((dn = dhead.di_prev) != &dhead) {
	dn->di_next->di_prev = dn->di_prev;
	dn->di_prev->di_next = dn->di_next;
	if (dn != dcwd)
	    dfree(dn);
    }

    /* thread the current working directory */
    dhead.di_prev = dhead.di_next = dcwd;
    dcwd->di_next = dcwd->di_prev = &dhead;

    /* put back the stack */
    for (cp = vp->vec; cp && *cp && **cp; cp++) {
	dp = (struct directory *) xcalloc(sizeof(struct directory), 1);
	dp->di_name = Strsave(*cp);
	dp->di_count = 0;
	dp->di_prev = dcwd;
	dp->di_next = dcwd->di_next;
	dcwd->di_next = dp;
	dp->di_next->di_prev = dp;
    }
    dgetstack();	/* Make $dirstack reflect the current state */
}

static void
dgetstack()
{
    int i = 0;
    Char **dblk, **dbp;
    struct directory *dn;

    if (adrof(STRdirstack) == NULL) 
    	return;

    for (dn = dhead.di_prev; dn != &dhead; dn = dn->di_prev, i++) 
	continue;
    dbp = dblk = (Char**) xmalloc((size_t) (i + 1) * sizeof(Char *));
    for (dn = dhead.di_prev; dn != &dhead; dn = dn->di_prev, dbp++) 
	 *dbp = Strsave(dn->di_name);
    *dbp = NULL;
    setq(STRdirstack, dblk, &shvhed, VAR_READWRITE);
}

/*
 * getstakd - added by kfk 17 Jan 1984
 * Support routine for the stack hack.  Finds nth directory in
 * the directory stack, or finds last directory in stack.
 */
int
getstakd(s, cnt)
    Char   *s;
    int     cnt;
{
    struct directory *dp;

    dp = dcwd;
    if (cnt < 0) {		/* < 0 ==> last dir requested. */
	dp = dp->di_next;
	if (dp == &dhead)
	    dp = dp->di_next;
    }
    else {
	while (cnt-- > 0) {
	    dp = dp->di_prev;
	    if (dp == &dhead)
		dp = dp->di_prev;
	    if (dp == dcwd)
		return (0);
	}
    }
    (void) Strncpy(s, dp->di_name, BUFSIZE);
    s[BUFSIZE - 1] = '\0';
    return (1);
}

/*
 * Karl Kleinpaste - 10 Feb 1984
 * Added dextract(), which is used in pushd +n.
 * Instead of just rotating the entire stack around, dextract()
 * lets the user have the nth dir extracted from its current
 * position, and pushes it onto the top.
 */
static void
dextract(dp)
    struct directory *dp;
{
    if (dp == dcwd)
	return;
    dp->di_next->di_prev = dp->di_prev;
    dp->di_prev->di_next = dp->di_next;
    dp->di_next = dcwd->di_next;
    dp->di_prev = dcwd;
    dp->di_next->di_prev = dp;
    dcwd->di_next = dp;
}

void
loaddirs(fname)
    Char *fname;
{
    static Char *loaddirs_cmd[] = { STRsource, NULL, NULL };

    bequiet = 1;
    if (fname) 
	loaddirs_cmd[1] = fname;
    else if ((fname = varval(STRdirsfile)) != STRNULL)
	loaddirs_cmd[1] = fname;
    else
	loaddirs_cmd[1] = STRtildotdirs;
    dosource(loaddirs_cmd, (struct command *)0);
    bequiet = 0;
}

/*
 * create a file called ~/.cshdirs which has a sequence
 * of pushd commands which will restore the dir stack to
 * its state before exit/logout. remember that the order
 * is reversed in the file because we are pushing.
 * -strike
 */
void
recdirs(fname, def)
    Char *fname;
    int def;
{
    int     fp, ftmp, oldidfds;
    int     cdflag = 0;
    extern struct directory *dcwd;
    struct directory *dp;
    unsigned int    num;
    Char   *snum;
    Char    qname[MAXPATHLEN*2];

    if (fname == NULL && !def) 
	return;

    if (fname == NULL) {
	if ((fname = varval(STRdirsfile)) == STRNULL)
	    fname = Strspl(varval(STRhome), &STRtildotdirs[1]);
	else
	    fname = Strsave(fname);
    }
    else 
	fname = globone(fname, G_ERROR);
		
    if ((fp = creat(short2str(fname), 0600)) == -1) {
	xfree((ptr_t) fname);
	return;
    }

    if ((snum = varval(STRsavedirs)) == STRNULL || snum[0] == '\0') 
	num = (unsigned int) ~0;
    else
	num = (unsigned int) atoi(short2str(snum));

    oldidfds = didfds;
    didfds = 0;
    ftmp = SHOUT;
    SHOUT = fp;

    dp = dcwd->di_next;
    do {
	if (dp == &dhead)
	    continue;

	if (cdflag == 0) {
	    cdflag = 1;
	    xprintf("cd %S\n", quote_meta(qname, dp->di_name));
	}
	else
	    xprintf("pushd %S\n", quote_meta(qname, dp->di_name));

	if (num-- == 0)
	    break;

    } while ((dp = dp->di_next) != dcwd->di_next);

    (void) close(fp);
    SHOUT = ftmp;
    didfds = oldidfds;
    xfree((ptr_t) fname);
}
