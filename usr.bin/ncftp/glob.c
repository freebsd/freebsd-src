/* glob.c */

/*  $RCSfile: glob.c,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/05/21 05:44:32 $
 */

#include "sys.h"

#include <sys/stat.h>

/* Dir.h.  Try <sys/dir.h> (add -DSYSDIRH) if <dirent.h> doesn't exist. */

#ifndef SYSDIRH
#   include <dirent.h>
#else
#   include <sys/dir.h>
#endif

#ifdef SCO324
#   define direct dirent
#endif

#include <errno.h>
#include <pwd.h>
#include "util.h"
#include "glob.h"
#include "cmds.h"
#include "copyright.h"

#ifndef NCARGS
#	define NCARGS  4096 /* # characters in exec arglist */
#endif

#define	L_CURLY	'{'
#define	R_CURLY	'}'

#define	QUOTE 0200
#define	TRIM 0177
#define	eq(a,b)		(strcmp(a, b)==0)
#define	GAVSIZ		(NCARGS/6)
#define	isdir(d)	((d.st_mode & S_IFMT) == S_IFDIR)

static void ginit(char **agargv);
static void collect(char *as);
static void acollect(char *as);
static void sort(void);
static void expand(char *as);
static void matchdir(char *pattern);
static int execbrc(char *p, char *s);
static match(char *s, char *p);
static amatch(char *s, char *p);
#if UNUSED
static Gmatch(char *s, char *p);
#endif
static void Gcat(char *s1, char *s2);
static void addpath(char c);
static void rscan(char **t, int (*f )(char));
static tglob(char c);
static char *strspl(char *cp, char *dp);
static char *strend(char *cp);

static	char **gargv;	/* Pointer to the (stack) arglist */
static	int gargc;		/* Number args in gargv */
static	int gnleft;
static	short gflag;
char	*globerr;
char	*home;			/* you must initialize this elsewhere! */
extern	int errno;

static	int globcnt;

char	*globchars = "`{[*?";

static	char *gpath, *gpathp, *lastgpathp;
static	int globbed;
static	char *entp;
static	char **sortbas;

char **
glob(char *v)
{
	char agpath[BUFSIZ];
	char *agargv[GAVSIZ];
	char *vv[2];
	vv[0] = v;
	vv[1] = 0;
	gflag = (short) 0;
	rscan(vv, tglob);
	if (gflag == (short) 0)
		return (copyblk(vv));

	globerr = 0;
	gpath = agpath; gpathp = gpath; *gpathp = 0;
	lastgpathp = &gpath[sizeof agpath - 2];
	ginit(agargv); globcnt = 0;
	collect(v);
	if (globcnt == 0 && (gflag & (short)1)) {
		blkfree(gargv), gargv = 0;
		return (0);
	} else
		return (gargv = copyblk(gargv));
}

static
void ginit(char **agargv)
{
	agargv[0] = 0; gargv = agargv; sortbas = agargv; gargc = 0;
	gnleft = NCARGS - 4;
}

static
void collect(char *as)
{
	if (eq(as, "{") || eq(as, "{}")) {
		Gcat(as, "");
		sort();
	} else
		acollect(as);
}

static
void acollect(char *as)
{
	register int ogargc = gargc;

	gpathp = gpath; *gpathp = 0; globbed = 0;
	expand(as);
	if (gargc != ogargc)
		sort();
}

static
void sort(void)
{
	register char **p1, **p2, *c;
	char **Gvp = &gargv[gargc];

	p1 = sortbas;
	while (p1 < Gvp-1) {
		p2 = p1;
		while (++p2 < Gvp)
			if (strcmp(*p1, *p2) > 0)
				c = *p1, *p1 = *p2, *p2 = c;
		p1++;
	}
	sortbas = Gvp;
}

static
void expand(char *as)
{
	register char *cs;
	register char *sgpathp, *oldcs;
	struct stat stb;

	sgpathp = gpathp;
	cs = as;
	if (*cs == '~' && gpathp == gpath) {
		addpath('~');
		for (cs++; letter(*cs) || digit(*cs) || *cs == '-';)
			addpath(*cs++);
		if (!*cs || *cs == '/') {
			if (gpathp != gpath + 1) {
				*gpathp = 0;
				if (gethdir(gpath + 1))
					globerr = "Unknown user name after ~";
				(void) strcpy(gpath, gpath + 1);
			} else
				(void) strcpy(gpath, home);
			gpathp = strend(gpath);
		}
	}
	while (!any(*cs, globchars)) {
		if (*cs == 0) {
			if (!globbed)
				Gcat(gpath, "");
			else if (stat(gpath, &stb) >= 0) {
				Gcat(gpath, "");
				globcnt++;
			}
			goto endit;
		}
		addpath(*cs++);
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, gpathp--;
	if (*cs == '/')
		cs++, gpathp++;
	*gpathp = 0;
	if (*oldcs == L_CURLY) {
		(void) execbrc(cs, ((char *)0));
		return;
	}
	matchdir(cs);
endit:
	gpathp = sgpathp;
	*gpathp = 0;
}

static
void matchdir(char *pattern)
{
	struct stat stb;
#ifdef SYSDIRH
	register struct direct *dp;
#else
	register struct dirent *dp;
#endif
	DIR *dirp;

	dirp = opendir((*gpath ? gpath : "."));
	if (dirp == NULL) {
		if (globbed)
			return;
		goto patherr2;
	}
	if (fstat(dirp->dd_fd, &stb) < 0)
		goto patherr1;
	if (!isdir(stb)) {
		errno = ENOTDIR;
		goto patherr1;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (match(dp->d_name, pattern)) {
			Gcat(gpath, dp->d_name);
			globcnt++;
		}
	}
	(void) closedir(dirp);
	return;

patherr1:
	(void) closedir(dirp);
patherr2:
	globerr = "Bad directory components";
}

static
int execbrc(char *p, char *s)
{
	char restbuf[BUFSIZ + 2];
	register char *pe, *pm, *pl;
	int brclev = 0;
	char *lm, savec, *sgpathp;

	for (lm = restbuf; *p != L_CURLY; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++)
	switch (*pe) {

	case L_CURLY:
		brclev++;
		continue;

	case R_CURLY:
		if (brclev == 0)
			goto pend;
		brclev--;
		continue;

	case '[':
		for (pe++; *pe && *pe != ']'; pe++)
			continue;
		continue;
	}
pend:
	brclev = 0;
	for (pl = pm = p; pm <= pe; pm++)
	switch (*pm & (QUOTE|TRIM)) {

	case L_CURLY:
		brclev++;
		continue;

	case R_CURLY:
		if (brclev) {
			brclev--;
			continue;
		}
		goto doit;

	case ','|QUOTE:
	case ',':
		if (brclev)
			continue;
doit:
		savec = *pm;
		*pm = 0;
		(void) strcpy(lm, pl);
		(void) strcat(restbuf, pe + 1);
		*pm = savec;
		if (s == 0) {
			sgpathp = gpathp;
			expand(restbuf);
			gpathp = sgpathp;
			*gpathp = 0;
		} else if (amatch(s, restbuf))
			return (1);
		sort();
		pl = pm + 1;
		if (brclev)
			return (0);
		continue;

	case '[':
		for (pm++; *pm && *pm != ']'; pm++)
			continue;
		if (!*pm)
			pm--;
		continue;
	}
	if (brclev)
		goto doit;
	return (0);
}

static
int match(char *s, char *p)
{
	register int c;
	register char *sentp;
	char sglobbed = globbed;

	if (*s == '.' && *p != '.')
		return (0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	globbed = sglobbed;
	return (c);
}

static
int amatch(char *s, char *p)
{
	register int scc;
	int ok, lc;
	char *sgpathp;
	struct stat stb;
	int c, cc;

	globbed = 1;
	for (;;) {
		scc = *s++ & TRIM;
		switch (c = *p++) {

		case L_CURLY:
			return (execbrc(p - 1, s - 1));

		case '[':
			ok = 0;
			lc = 077777;
			while ((cc = *p++) != '\0') {
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0)
				if (ok)
					p--;
				else
					return 0;
			continue;

		case '*':
			if (!*p)
				return (1);
			if (*p == '/') {
				p++;
				goto slash;
			}
			s--;
			do {
				if (amatch(s, p))
					return (1);
			} while (*s++);
			return (0);

		case 0:
			return (scc == 0);

		default:
			if (c != scc)
				return (0);
			continue;

		case '?':
			if (scc == 0)
				return (0);
			continue;

		case '/':
			if (scc)
				return (0);
slash:
			s = entp;
			sgpathp = gpathp;
			while (*s)
				addpath(*s++);
			addpath('/');
			if (stat(gpath, &stb) == 0 && isdir(stb))
				if (*p == 0) {
					Gcat(gpath, "");
					globcnt++;
				} else
					expand(p);
			gpathp = sgpathp;
			*gpathp = 0;
			return (0);
		}
	}
}

#if UNUSED
static
Gmatch(char *s, char *p)
{
	register int scc;
	int ok, lc;
	int c, cc;

	for (;;) {
		scc = *s++ & TRIM;
		switch (c = *p++) {

		case '[':
			ok = 0;
			lc = 077777;
			while (cc = *p++) {
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0)
				if (ok)
					p--;
				else
					return 0;
			continue;

		case '*':
			if (!*p)
				return (1);
			for (s--; *s; s++)
				if (Gmatch(s, p))
					return (1);
			return (0);

		case 0:
			return (scc == 0);

		default:
			if ((c & TRIM) != scc)
				return (0);
			continue;

		case '?':
			if (scc == 0)
				return (0);
			continue;

		}
	}
}
#endif

static
void Gcat(char *s1, char *s2)
{
	register int len = strlen(s1) + strlen(s2) + 1;

	if (len >= gnleft || gargc >= GAVSIZ - 1)
		globerr = "Arguments too long";
	else {
		gargc++;
		gnleft -= len;
		gargv[gargc] = 0;
		gargv[gargc - 1] = strspl(s1, s2);
	}
}

static
void addpath(char c)
{

	if (gpathp >= lastgpathp)
		globerr = "Pathname too long";
	else {
		*gpathp++ = c;
		*gpathp = 0;
	}
}

static
void rscan(char **t, int (*f )(char))
{
	register char *p, c;

	while ((p = *t++) != 0) {
		if (f == tglob)
			if (*p == '~')
				gflag |= (short) 2;
			else if (eq(p, "{") || eq(p, "{}"))
				continue;
		while ((c = *p++) != '\0')
			(*f)(c);
	}
}
/*
static
scan(t, f)
	register char **t;
	int (*f)(char);
{
	register char *p, c;

	while (p = *t++)
		while (c = *p)
			*p++ = (*f)(c);
} */

static
int tglob(char c)
{

	if (any(c, globchars))
		gflag |= (c == L_CURLY ? (short)2 : (short)1);
	return (c);
}
/*
static
trim(c)
	char c;
{

	return (c & TRIM);
} */


int letter(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

int digit(char c)
{
	return (c >= '0' && c <= '9');
}

int any(int c, char *s)
{
	while (*s)
		if (*s++ == c)
			return(1);
	return(0);
}

int blklen(char **av)
{
	register int i = 0;

	while (*av++)
		i++;
	return (i);
}

char **
blkcpy(char **oav, char **bv)
{
	register char **av = oav;

	while ((*av++ = *bv++) != 0)
		continue;
	return (oav);
}

void blkfree(char **av0)
{
	register char **av = av0;

	while (*av)
		free(*av++);
}

static
char *
strspl(char *cp, char *dp)
{
	register char *ep = (char *) malloc((size_t)(strlen(cp) + strlen(dp) + 1L));

	if (ep == (char *)0)
		fatal("Out of memory");
	(void) strcpy(ep, cp);
	(void) strcat(ep, dp);
	return (ep);
}

char **
copyblk(char **v)
{
	register char **nv = (char **)malloc((size_t)((blklen(v) + 1) *
						sizeof(char **)));
	if (nv == (char **)0)
		fatal("Out of memory");

	return (blkcpy(nv, v));
}

static
char *
strend(char *cp)
{
	while (*cp)
		cp++;
	return (cp);
}

/*
 * Extract a home directory from the password file
 * The argument points to a buffer where the name of the
 * user whose home directory is sought is currently.
 * We write the home directory of the user back there.
 */
int gethdir(char *home_dir)
{
	register struct passwd *pp = getpwnam(home_dir);

	if (pp == 0)
		return (1);
	(void) strcpy(home_dir, pp->pw_dir);
	return (0);
}	/* gethdir */

/* eof glob.c */
