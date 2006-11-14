/* $Header: /src/pub/tcsh/sh.exp.c,v 3.45 2005/01/18 20:24:50 christos Exp $ */
/*
 * sh.exp.c: Expression evaluations
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

RCSID("$Id: sh.exp.c,v 3.45 2005/01/18 20:24:50 christos Exp $")

#include "tw.h"

/*
 * C shell
 */

#define TEXP_IGNORE 1	/* in ignore, it means to ignore value, just parse */
#define TEXP_NOGLOB 2	/* in ignore, it means not to globone */

#define	ADDOP	1
#define	MULOP	2
#define	EQOP	4
#define	RELOP	8
#define	RESTOP	16
#define	ANYOP	31

#define	EQEQ	1
#define	GTR	2
#define	LSS	4
#define	NOTEQ	6
#define EQMATCH 7
#define NOTEQMATCH 8

static	int	 sh_access	__P((Char *, int));
static	int	 exp1		__P((Char ***, int));
static	int	 exp2x		__P((Char ***, int));
static	int	 exp2a		__P((Char ***, int));
static	int	 exp2b		__P((Char ***, int));
static	int	 exp2c		__P((Char ***, int));
static	Char 	*exp3		__P((Char ***, int));
static	Char 	*exp3a		__P((Char ***, int));
static	Char 	*exp4		__P((Char ***, int));
static	Char 	*exp5		__P((Char ***, int));
static	Char 	*exp6		__P((Char ***, int));
static	void	 evalav		__P((Char **));
static	int	 isa		__P((Char *, int));
static	int	 egetn		__P((Char *));


#ifdef EDEBUG
static	void	 etracc		__P((char *, Char *, Char ***));
static	void	 etraci		__P((char *, int, Char ***));
#endif /* EDEBUG */


/*
 * shell access function according to POSIX and non POSIX
 * From Beto Appleton (beto@aixwiz.aix.ibm.com)
 */
static int
sh_access(fname, mode)
    Char *fname;
    int mode;
{
#if defined(POSIX) && !defined(USE_ACCESS)
    struct stat     statb;
#endif /* POSIX */
    char *name = short2str(fname);

    if (*name == '\0')
	return 1;

#if !defined(POSIX) || defined(USE_ACCESS)
    return access(name, mode);
#else /* POSIX */

    /*
     * POSIX 1003.2-d11.2 
     *	-r file		True if file exists and is readable. 
     *	-w file		True if file exists and is writable. 
     *			True shall indicate only that the write flag is on. 
     *			The file shall not be writable on a read-only file
     *			system even if this test indicates true.
     *	-x file		True if file exists and is executable. 
     *			True shall indicate only that the execute flag is on. 
     *			If file is a directory, true indicates that the file 
     *			can be searched.
     */
    if (mode != W_OK && mode != X_OK)
	return access(name, mode);

    if (stat(name, &statb) == -1) 
	return 1;

    if (access(name, mode) == 0) {
#ifdef S_ISDIR
	if (S_ISDIR(statb.st_mode) && mode == X_OK)
	    return 0;
#endif /* S_ISDIR */

	/* root needs permission for someone */
	switch (mode) {
	case W_OK:
	    mode = S_IWUSR | S_IWGRP | S_IWOTH;
	    break;
	case X_OK:
	    mode = S_IXUSR | S_IXGRP | S_IXOTH;
	    break;
	default:
	    abort();
	    break;
	}

    } 

    else if (euid == statb.st_uid)
	mode <<= 6;

    else if (egid == statb.st_gid)
	mode <<= 3;

# ifdef NGROUPS_MAX
    else {
	/* you can be in several groups */
	long	n;
	GETGROUPS_T *groups;

	/*
	 * Try these things to find a positive maximum groups value:
	 *   1) sysconf(_SC_NGROUPS_MAX)
	 *   2) NGROUPS_MAX
	 *   3) getgroups(0, unused)
	 * Then allocate and scan the groups array if one of these worked.
	 */
#  if defined (HAVE_SYSCONF) && defined (_SC_NGROUPS_MAX)
	if ((n = sysconf(_SC_NGROUPS_MAX)) == -1)
#  endif /* _SC_NGROUPS_MAX */
	    n = NGROUPS_MAX;
	if (n <= 0)
	    n = getgroups(0, (GETGROUPS_T *) NULL);

	if (n > 0) {
	    groups = xmalloc((size_t) (n * sizeof(*groups)));
	    n = getgroups((int) n, groups);
	    while (--n >= 0)
		if (groups[n] == statb.st_gid) {
		    mode <<= 3;
		    break;
		}
	}
    }
# endif /* NGROUPS_MAX */

    if (statb.st_mode & mode)
	return 0;
    else
	return 1;
#endif /* !POSIX */
}

int
expr(vp)
    Char ***vp;
{
    return (exp0(vp, 0));
}

int
exp0(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int p1 = exp1(vp, ignore);

#ifdef EDEBUG
    etraci("exp0 p1", p1, vp);
#endif /* EDEBUG */
    if (**vp && eq(**vp, STRor2)) {
	int p2;

	(*vp)++;
	p2 = exp0(vp, (ignore & TEXP_IGNORE) || p1);
#ifdef EDEBUG
	etraci("exp0 p2", p2, vp);
#endif /* EDEBUG */
	return (p1 || p2);
    }
    return (p1);
}

static int
exp1(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int p1 = exp2x(vp, ignore);

#ifdef EDEBUG
    etraci("exp1 p1", p1, vp);
#endif /* EDEBUG */
    if (**vp && eq(**vp, STRand2)) {
	int p2;

	(*vp)++;
	p2 = exp1(vp, (ignore & TEXP_IGNORE) || !p1);
#ifdef EDEBUG
	etraci("exp1 p2", p2, vp);
#endif /* EDEBUG */
	return (p1 && p2);
    }
    return (p1);
}

static int
exp2x(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int p1 = exp2a(vp, ignore);

#ifdef EDEBUG
    etraci("exp3 p1", p1, vp);
#endif /* EDEBUG */
    if (**vp && eq(**vp, STRor)) {
	int p2;

	(*vp)++;
	p2 = exp2x(vp, ignore);
#ifdef EDEBUG
	etraci("exp3 p2", p2, vp);
#endif /* EDEBUG */
	return (p1 | p2);
    }
    return (p1);
}

static int
exp2a(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int p1 = exp2b(vp, ignore);

#ifdef EDEBUG
    etraci("exp2a p1", p1, vp);
#endif /* EDEBUG */
    if (**vp && eq(**vp, STRcaret)) {
	int p2;

	(*vp)++;
	p2 = exp2a(vp, ignore);
#ifdef EDEBUG
	etraci("exp2a p2", p2, vp);
#endif /* EDEBUG */
	return (p1 ^ p2);
    }
    return (p1);
}

static int
exp2b(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int p1 = exp2c(vp, ignore);

#ifdef EDEBUG
    etraci("exp2b p1", p1, vp);
#endif /* EDEBUG */
    if (**vp && eq(**vp, STRand)) {
	int p2;

	(*vp)++;
	p2 = exp2b(vp, ignore);
#ifdef EDEBUG
	etraci("exp2b p2", p2, vp);
#endif /* EDEBUG */
	return (p1 & p2);
    }
    return (p1);
}

static int
exp2c(vp, ignore)
    Char ***vp;
    int    ignore;
{
    Char *p1 = exp3(vp, ignore);
    Char *p2;
    int i;

#ifdef EDEBUG
    etracc("exp2c p1", p1, vp);
#endif /* EDEBUG */
    if ((i = isa(**vp, EQOP)) != 0) {
	(*vp)++;
	if (i == EQMATCH || i == NOTEQMATCH)
	    ignore |= TEXP_NOGLOB;
	p2 = exp3(vp, ignore);
#ifdef EDEBUG
	etracc("exp2c p2", p2, vp);
#endif /* EDEBUG */
	if (!(ignore & TEXP_IGNORE))
	    switch (i) {

	    case EQEQ:
		i = eq(p1, p2);
		break;

	    case NOTEQ:
		i = !eq(p1, p2);
		break;

	    case EQMATCH:
		i = Gmatch(p1, p2);
		break;

	    case NOTEQMATCH:
		i = !Gmatch(p1, p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (i);
    }
    i = egetn(p1);
    xfree((ptr_t) p1);
    return (i);
}

static Char *
exp3(vp, ignore)
    Char ***vp;
    int    ignore;
{
    Char *p1, *p2;
    int i;

    p1 = exp3a(vp, ignore);
#ifdef EDEBUG
    etracc("exp3 p1", p1, vp);
#endif /* EDEBUG */
    if ((i = isa(**vp, RELOP)) != 0) {
	(*vp)++;
	if (**vp && eq(**vp, STRequal))
	    i |= 1, (*vp)++;
	p2 = exp3(vp, ignore);
#ifdef EDEBUG
	etracc("exp3 p2", p2, vp);
#endif /* EDEBUG */
	if (!(ignore & TEXP_IGNORE))
	    switch (i) {

	    case GTR:
		i = egetn(p1) > egetn(p2);
		break;

	    case GTR | 1:
		i = egetn(p1) >= egetn(p2);
		break;

	    case LSS:
		i = egetn(p1) < egetn(p2);
		break;

	    case LSS | 1:
		i = egetn(p1) <= egetn(p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp3a(vp, ignore)
    Char ***vp;
    int    ignore;
{
    Char *p1, *p2, *op;
    int i;

    p1 = exp4(vp, ignore);
#ifdef EDEBUG
    etracc("exp3a p1", p1, vp);
#endif /* EDEBUG */
    op = **vp;
    if (op && any("<>", op[0]) && op[0] == op[1]) {
	(*vp)++;
	p2 = exp3a(vp, ignore);
#ifdef EDEBUG
	etracc("exp3a p2", p2, vp);
#endif /* EDEBUG */
	if (op[0] == '<')
	    i = egetn(p1) << egetn(p2);
	else
	    i = egetn(p1) >> egetn(p2);
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp4(vp, ignore)
    Char ***vp;
    int    ignore;
{
    Char *p1, *p2;
    int i = 0;

    p1 = exp5(vp, ignore);
#ifdef EDEBUG
    etracc("exp4 p1", p1, vp);
#endif /* EDEBUG */
    if (isa(**vp, ADDOP)) {
	Char *op = *(*vp)++;

	p2 = exp4(vp, ignore);
#ifdef EDEBUG
	etracc("exp4 p2", p2, vp);
#endif /* EDEBUG */
	if (!(ignore & TEXP_IGNORE))
	    switch (op[0]) {

	    case '+':
		i = egetn(p1) + egetn(p2);
		break;

	    case '-':
		i = egetn(p1) - egetn(p2);
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp5(vp, ignore)
    Char ***vp;
    int    ignore;
{
    Char *p1, *p2;
    int i = 0;

    p1 = exp6(vp, ignore);
#ifdef EDEBUG
    etracc("exp5 p1", p1, vp);
#endif /* EDEBUG */

    if (isa(**vp, MULOP)) {
	Char *op = *(*vp)++;
	if ((ignore & TEXP_NOGLOB) != 0) 
	    /* 
	     * We are just trying to get the right side of
	     * a =~ or !~ operator 
	     */
	    return Strsave(op);

	p2 = exp5(vp, ignore);
#ifdef EDEBUG
	etracc("exp5 p2", p2, vp);
#endif /* EDEBUG */
	if (!(ignore & TEXP_IGNORE))
	    switch (op[0]) {

	    case '*':
		i = egetn(p1) * egetn(p2);
		break;

	    case '/':
		i = egetn(p2);
		if (i == 0)
		    stderror(ERR_DIV0);
		i = egetn(p1) / i;
		break;

	    case '%':
		i = egetn(p2);
		if (i == 0)
		    stderror(ERR_MOD0);
		i = egetn(p1) % i;
		break;
	    }
	xfree((ptr_t) p1);
	xfree((ptr_t) p2);
	return (putn(i));
    }
    return (p1);
}

static Char *
exp6(vp, ignore)
    Char ***vp;
    int    ignore;
{
    int     ccode, i = 0;
    Char *cp;

    if (**vp == 0)
	stderror(ERR_NAME | ERR_EXPRESSION);
    if (eq(**vp, STRbang)) {
	(*vp)++;
	cp = exp6(vp, ignore);
#ifdef EDEBUG
	etracc("exp6 ! cp", cp, vp);
#endif /* EDEBUG */
	i = egetn(cp);
	xfree((ptr_t) cp);
	return (putn(!i));
    }
    if (eq(**vp, STRtilde)) {
	(*vp)++;
	cp = exp6(vp, ignore);
#ifdef EDEBUG
	etracc("exp6 ~ cp", cp, vp);
#endif /* EDEBUG */
	i = egetn(cp);
	xfree((ptr_t) cp);
	return (putn(~i));
    }
    if (eq(**vp, STRLparen)) {
	(*vp)++;
	ccode = exp0(vp, ignore);
#ifdef EDEBUG
	etraci("exp6 () ccode", ccode, vp);
#endif /* EDEBUG */
	if (*vp == 0 || **vp == 0 || ***vp != ')')
	    stderror(ERR_NAME | ERR_EXPRESSION);
	(*vp)++;
	return (putn(ccode));
    }
    if (eq(**vp, STRLbrace)) {
	Char **v;
	struct command faket;
	Char   *fakecom[2];

	faket.t_dtyp = NODE_COMMAND;
	faket.t_dflg = F_BACKQ;
	faket.t_dcar = faket.t_dcdr = faket.t_dspr = NULL;
	faket.t_dcom = fakecom;
	fakecom[0] = STRfakecom;
	fakecom[1] = NULL;
	(*vp)++;
	v = *vp;
	for (;;) {
	    if (!**vp)
		stderror(ERR_NAME | ERR_MISSING, '}');
	    if (eq(*(*vp)++, STRRbrace))
		break;
	}
	if (ignore & TEXP_IGNORE)
	    return (Strsave(STRNULL));
	psavejob();
	if (pfork(&faket, -1) == 0) {
	    *--(*vp) = 0;
	    evalav(v);
	    exitstat();
	}
	pwait();
	prestjob();
#ifdef EDEBUG
	etraci("exp6 {} status", egetn(varval(STRstatus)), vp);
#endif /* EDEBUG */
	return (putn(egetn(varval(STRstatus)) == 0));
    }
    if (isa(**vp, ANYOP))
	return (Strsave(STRNULL));
    cp = *(*vp)++;
#ifdef convex
# define FILETESTS "erwxfdzoplstSXLbcugkmKR"
#else
# define FILETESTS "erwxfdzoplstSXLbcugkmK"
#endif /* convex */
#define FILEVALS  "ZAMCDIUGNFPL"
    if (*cp == '-' && (any(FILETESTS, cp[1]) || any(FILEVALS, cp[1])))
        return(filetest(cp, vp, ignore));
#ifdef EDEBUG
    etracc("exp6 default", cp, vp);
#endif /* EDEBUG */
    return (ignore & TEXP_NOGLOB ? Strsave(cp) : globone(cp, G_APPEND));
}


/* 
 * Extended file tests
 * From: John Rowe <rowe@excc.exeter.ac.uk>
 */
Char *
filetest(cp, vp, ignore)
    Char *cp, ***vp;
    int ignore;
{
#ifdef convex
    struct cvxstat stb, *st = NULL;
# define TCSH_STAT	stat64
#else
# define TCSH_STAT	stat
    struct stat stb, *st = NULL;
#endif /* convex */

#ifdef S_IFLNK
# ifdef convex
    struct cvxstat lstb, *lst = NULL;
#  define TCSH_LSTAT lstat64
# else
#  define TCSH_LSTAT lstat
    struct stat lstb, *lst = NULL;
# endif /* convex */
    char *filnam;
#endif /* S_IFLNK */

    int i = 0;
    unsigned pmask = 0xffff;
    int altout = 0;
    Char *ft = cp, *dp, *ep, *strdev, *strino, *strF, *str, valtest = '\0',
    *errval = STR0;
    char *string, string0[8];
    time_t footime;
    struct passwd *pw;
    struct group *gr;

    while(any(FILETESTS, *++ft))
	continue;

    if (!*ft && *(ft - 1) == 'L')
	--ft;

    if (any(FILEVALS, *ft)) {
	valtest = *ft++;
	/*
	 * Value tests return '-1' on failure as 0 is
	 * a legitimate value for many of them.
	 * 'F' returns ':' for compatibility.
	 */
	errval = valtest == 'F' ? STRcolon : STRminus1;

	if (valtest == 'P' && *ft >= '0' && *ft <= '7') {
	    pmask = (char) *ft - '0';
	    while ( *++ft >= '0' && *ft <= '7' )
		pmask = 8 * pmask + ((char) *ft - '0');
	}
	if (Strcmp(ft, STRcolon) == 0 && any("AMCUGP", valtest)) {
	    altout = 1;
	    ++ft;
	}
    }

    if (*ft || ft == cp + 1)
	stderror(ERR_NAME | ERR_FILEINQ);

    /*
     * Detect missing file names by checking for operator in the file name
     * position.  However, if an operator name appears there, we must make
     * sure that there's no file by that name (e.g., "/") before announcing
     * an error.  Even this check isn't quite right, since it doesn't take
     * globbing into account.
     */

    if (isa(**vp, ANYOP) && TCSH_STAT(short2str(**vp), &stb))
	stderror(ERR_NAME | ERR_FILENAME);

    dp = *(*vp)++;
    if (ignore & TEXP_IGNORE)
	return (Strsave(STRNULL));
    ep = globone(dp, G_APPEND);
    ft = &cp[1];
    do 
	switch (*ft) {

	case 'r':
	    i = !sh_access(ep, R_OK);
	    break;

	case 'w':
	    i = !sh_access(ep, W_OK);
	    break;

	case 'x':
	    i = !sh_access(ep, X_OK);
	    break;

	case 'X':	/* tcsh extension, name is an executable in the path
			 * or a tcsh builtin command 
			 */
	    i = find_cmd(ep, 0);
	    break;

	case 't':	/* SGI extension, true when file is a tty */
	    i = isatty(atoi(short2str(ep)));
	    break;

	default:

#ifdef S_IFLNK
	    if (tolower(*ft) == 'l') {
		/* 
		 * avoid convex compiler bug.
		 */
		if (!lst) {
		    lst = &lstb;
		    if (TCSH_LSTAT(short2str(ep), lst) == -1) {
			xfree((ptr_t) ep);
			return (Strsave(errval));
		    }
		}
		if (*ft == 'L')
		    st = lst;
	    }
	    else 
#endif /* S_IFLNK */
		/* 
		 * avoid convex compiler bug.
		 */
		if (!st) {
		    st = &stb;
		    if (TCSH_STAT(short2str(ep), st) == -1) {
			xfree((ptr_t) ep);
			return (Strsave(errval));
		    }
		}

	    switch (*ft) {

	    case 'f':
#ifdef S_ISREG
		i = S_ISREG(st->st_mode);
#else /* !S_ISREG */
		i = 0;
#endif /* S_ISREG */
		break;

	    case 'd':
#ifdef S_ISDIR
		i = S_ISDIR(st->st_mode);
#else /* !S_ISDIR */
		i = 0;
#endif /* S_ISDIR */
		break;

	    case 'p':
#ifdef S_ISFIFO
		i = S_ISFIFO(st->st_mode);
#else /* !S_ISFIFO */
		i = 0;
#endif /* S_ISFIFO */
		break;

	    case 'm' :
#ifdef S_ISOFL
	      i = S_ISOFL(st->st_dm_mode);
#else /* !S_ISOFL */
	      i = 0;
#endif /* S_ISOFL */
	      break ;

	    case 'K' :
#ifdef S_ISOFL
	      i = stb.st_dm_key;
#else /* !S_ISOFL */
	      i = 0;
#endif /* S_ISOFL */
	      break ;
  

	    case 'l':
#ifdef S_ISLNK
		i = S_ISLNK(lst->st_mode);
#else /* !S_ISLNK */
		i = 0;
#endif /* S_ISLNK */
		break;

	    case 'S':
# ifdef S_ISSOCK
		i = S_ISSOCK(st->st_mode);
# else /* !S_ISSOCK */
		i = 0;
# endif /* S_ISSOCK */
		break;

	    case 'b':
#ifdef S_ISBLK
		i = S_ISBLK(st->st_mode);
#else /* !S_ISBLK */
		i = 0;
#endif /* S_ISBLK */
		break;

	    case 'c':
#ifdef S_ISCHR
		i = S_ISCHR(st->st_mode);
#else /* !S_ISCHR */
		i = 0;
#endif /* S_ISCHR */
		break;

	    case 'u':
		i = (S_ISUID & st->st_mode) != 0;
		break;

	    case 'g':
		i = (S_ISGID & st->st_mode) != 0;
		break;

	    case 'k':
		i = (S_ISVTX & st->st_mode) != 0;
		break;

	    case 'z':
		i = st->st_size == 0;
		break;

#ifdef convex
	    case 'R':
		i = (stb.st_dmonflags & IMIGRATED) == IMIGRATED;
		break;
#endif /* convex */

	    case 's':
		i = stb.st_size != 0;
		break;

	    case 'e':
		i = 1;
		break;

	    case 'o':
		i = st->st_uid == uid;
		break;

		/*
		 * Value operators are a tcsh extension.
		 */

	    case 'D':
		i = (int) st->st_dev;
		break;

	    case 'I':
		i = (int) st->st_ino;
		break;
		
	    case 'F':
		strdev = putn( (int) st->st_dev);
		strino = putn( (int) st->st_ino);
		strF = (Char *) xmalloc((size_t) (2 + Strlen(strdev) + 
					 Strlen(strino)) * sizeof(Char));
		(void) Strcat(Strcat(Strcpy(strF, strdev), STRcolon), strino);
		xfree((ptr_t) strdev);
		xfree((ptr_t) strino);
		xfree((ptr_t) ep);
		return(strF);
		
	    case 'L':
		if ( *(ft + 1) ) {
		    i = 1;
		    break;
		}
#ifdef S_ISLNK
		filnam = short2str(ep);
#ifdef PATH_MAX
# define MY_PATH_MAX PATH_MAX
#else /* !PATH_MAX */
/* 
 * I can't think of any more sensible alterative; readlink doesn't give 
 * us an errno if the buffer isn't large enough :-(
 */
# define MY_PATH_MAX  2048
#endif /* PATH_MAX */
		i = readlink(filnam, string = (char *) 
		      xmalloc((size_t) (1 + MY_PATH_MAX) * sizeof(char)),
			MY_PATH_MAX);
		if (i >= 0 && i <= MY_PATH_MAX)
		    string[i] = '\0'; /* readlink does not null terminate */
		strF = (i < 0) ? errval : str2short(string);
		xfree((ptr_t) string);
		xfree((ptr_t) ep);
		return(Strsave(strF));

#else /* !S_ISLNK */
		i = 0;
		break;
#endif /* S_ISLNK */
		

	    case 'N':
		i = (int) st->st_nlink;
		break;

	    case 'P':
		string = string0 + 1;
		(void) xsnprintf(string, sizeof(string0) - 1, "%o",
		    pmask & (unsigned int) 
		    ((S_IRWXU|S_IRWXG|S_IRWXO|S_ISUID|S_ISGID) & st->st_mode));
		if (altout && *string != '0')
		    *--string = '0';
		xfree((ptr_t) ep);
		return(Strsave(str2short(string)));

	    case 'U':
		if (altout && (pw = getpwuid(st->st_uid))) {
		    xfree((ptr_t) ep);
		    return(Strsave(str2short(pw->pw_name)));
		}
		i = (int) st->st_uid;
		break;

	    case 'G':
		if ( altout && (gr = getgrgid(st->st_gid))) {
		    xfree((ptr_t) ep);
		    return(Strsave(str2short(gr->gr_name)));
		}
		i = (int) st->st_gid;
		break;

	    case 'Z':
		i = (int) st->st_size;
		break;

	    case 'A': case 'M': case 'C':
		footime = *ft == 'A' ? st->st_atime :
		    *ft == 'M' ? st->st_mtime : st->st_ctime;
		if (altout) {
		    strF = str2short(ctime(&footime));
		    if ((str = Strchr(strF, '\n')) != NULL)
			*str = (Char) '\0';
		    xfree((ptr_t) ep);
		    return(Strsave(strF));
		}
		i = (int) footime;
		break;

	    }
	}
    while (*++ft && i);
#ifdef EDEBUG
    etraci("exp6 -? i", i, vp);
#endif /* EDEBUG */
    xfree((ptr_t) ep);
    return (putn(i));
}


static void
evalav(v)
    Char **v;
{
    struct wordent paraml1;
    struct wordent *hp = &paraml1;
    struct command *t;
    struct wordent *wdp = hp;

    set(STRstatus, Strsave(STR0), VAR_READWRITE);
    hp->prev = hp->next = hp;
    hp->word = STRNULL;
    while (*v) {
	struct wordent *new =
	(struct wordent *) xcalloc(1, sizeof *wdp);

	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = Strsave(*v++);
    }
    hp->prev = wdp;
    alias(&paraml1);
    t = syntax(paraml1.next, &paraml1, 0);
    if (seterr)
	stderror(ERR_OLD);
    execute(t, -1, NULL, NULL, TRUE);
    freelex(&paraml1), freesyn(t);
}

static int
isa(cp, what)
    Char *cp;
    int what;
{
    if (cp == 0)
	return ((what & RESTOP) != 0);
    if (*cp == '\0')
    	return 0;
    if (cp[1] == 0) {
	if (what & ADDOP && (*cp == '+' || *cp == '-'))
	    return (1);
	if (what & MULOP && (*cp == '*' || *cp == '/' || *cp == '%'))
	    return (1);
	if (what & RESTOP && (*cp == '(' || *cp == ')' || *cp == '!' ||
			      *cp == '~' || *cp == '^' || *cp == '"'))
	    return (1);
    }
    else if (cp[2] == 0) {
	if (what & RESTOP) {
	    if (cp[0] == '|' && cp[1] == '&')
		return (1);
	    if (cp[0] == '<' && cp[1] == '<')
		return (1);
	    if (cp[0] == '>' && cp[1] == '>')
		return (1);
	}
	if (what & EQOP) {
	    if (cp[0] == '=') {
		if (cp[1] == '=')
		    return (EQEQ);
		if (cp[1] == '~')
		    return (EQMATCH);
	    }
	    else if (cp[0] == '!') {
		if (cp[1] == '=')
		    return (NOTEQ);
		if (cp[1] == '~')
		    return (NOTEQMATCH);
	    }
	}
    }
    if (what & RELOP) {
	if (*cp == '<')
	    return (LSS);
	if (*cp == '>')
	    return (GTR);
    }
    return (0);
}

static int
egetn(cp)
    Char *cp;
{
    if (*cp && *cp != '-' && !Isdigit(*cp))
	stderror(ERR_NAME | ERR_EXPRESSION);
    return (getn(cp));
}

/* Phew! */

#ifdef EDEBUG
static void
etraci(str, i, vp)
    char   *str;
    int     i;
    Char ***vp;
{
    xprintf("%s=%d\t", str, i);
    blkpr(*vp);
    xputchar('\n');
}
static void
etracc(str, cp, vp)
    char   *str;
    Char   *cp;
    Char ***vp;
{
    xprintf("%s=%s\t", str, cp);
    blkpr(*vp);
    xputchar('\n');
}
#endif /* EDEBUG */
