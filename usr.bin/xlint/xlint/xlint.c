/*	$NetBSD: xlint.c,v 1.3 1995/10/23 14:29:30 jpo Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>

#include "lint.h"
#include "pathnames.h"

/* directory for temporary files */
static	const	char *tmpdir;

/* path name for cpp output */
static	char	*cppout;

/* files created by 1st pass */
static	char	**p1out;

/* input files for 2nd pass (without libraries) */
static	char	**p2in;

/* library which will be created by 2nd pass */
static	char	*p2out;

/* flags always passed to cpp */
static	char	**cppflags;

/* flags for cpp, controled by sflag/tflag */
static	char	**lcppflgs;

/* flags for lint1 */
static	char	**l1flags;

/* flags for lint2 */
static	char	**l2flags;

/* libraries for lint2 */
static	char	**l2libs;

/* default libraries */
static	char	**deflibs;

/* additional libraries */
static	char	**libs;

/* search path for libraries */
static	char	**libsrchpath;

/* flags */
static	int	iflag, oflag, Cflag, sflag, tflag, Fflag;

/* print the commands executed to run the stages of compilation */
static	int	Vflag;

/* filename for oflag */
static	char	*outputfn;

/* reset after first .c source has been processed */
static	int	first = 1;

/*
 * name of a file which is currently written by a child and should
 * be removed after abnormal termination of the child
 */
static	const	char *currfn;


static	void	appstrg __P((char ***, char *));
static	void	appcstrg __P((char ***, const char *));
static	void	applst __P((char ***, char *const *));
static	void	freelst __P((char ***));
static	char	*concat2 __P((const char *, const char *));
static	char	*concat3 __P((const char *, const char *, const char *));
static	void	terminate __P((int));
static	const	char *basename __P((const char *, int));
static	void	appdef __P((char ***, const char *));
static	void	usage __P((void));
static	void	fname __P((const char *, int));
static	void	runchild __P((const char *, char *const *, const char *));
static	void	findlibs __P((char *const *));
static	int	rdok __P((const char *));
static	void	lint2 __P((void));
static	void	cat __P((char *const *, const char *));

/*
 * Some functions to deal with lists of strings.
 * Take care that we get no surprises in case of asyncron signals.
 */
static void
appstrg(lstp, s)
	char	***lstp, *s;
{
	char	**lst, **olst;
	int	i;

	olst = *lstp;
	for (i = 0; olst[i] != NULL; i++) ;
	lst = xmalloc((i + 2) * sizeof (char *));
	(void)memcpy(lst, olst, i * sizeof (char *));
	lst[i] = s;
	lst[i + 1] = NULL;
	*lstp = lst;
}	

static void
appcstrg(lstp, s)
	char	***lstp;
	const	char *s;
{
	appstrg(lstp, xstrdup(s));
}

static void
applst(destp, src)
	char	***destp;
	char	*const *src;
{
	int	i, k;
	char	**dest, **odest;

	odest = *destp;
	for (i = 0; odest[i] != NULL; i++) ;
	for (k = 0; src[k] != NULL; k++) ;
	dest = xmalloc((i + k + 1) * sizeof (char *));
	(void)memcpy(dest, odest, i * sizeof (char *));
	for (k = 0; src[k] != NULL; k++)
		dest[i + k] = xstrdup(src[k]);
	dest[i + k] = NULL;
	*destp = dest;
	free(odest);
}

static void
freelst(lstp)
	char	***lstp;
{
	char	*s;
	int	i;

	for (i = 0; (*lstp)[i] != NULL; i++) ;
	while (i-- > 0) {
		s = (*lstp)[i];
		(*lstp)[i] = NULL;
		free(s);
	}
}

static char *
concat2(s1, s2)
	const	char *s1, *s2;
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + 1);
	(void)strcpy(s, s1);
	(void)strcat(s, s2);

	return (s);
}

static char *
concat3(s1, s2, s3)
	const	char *s1, *s2, *s3;
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
	(void)strcpy(s, s1);
	(void)strcat(s, s2);
	(void)strcat(s, s3);

	return (s);
}

/*
 * Clean up after a signal.
 */
static void
terminate(signo)
	int	signo;
{
	int	i;

	if (cppout != NULL)
		(void)remove(cppout);

	if (p1out != NULL) {
		for (i = 0; p1out[i] != NULL; i++)
			(void)remove(p1out[i]);
	}

	if (p2out != NULL)
		(void)remove(p2out);

	if (currfn != NULL)
		(void)remove(currfn);

	exit(signo != 0 ? 1 : 0);
}

/*
 * Returns a pointer to the last component of strg after delim.
 * Returns strg if the string does not contain delim.
 */
static const char *
basename(strg, delim)
	const	char *strg;
	int	delim;
{
	const	char *cp, *cp1, *cp2;

	cp = cp1 = cp2 = strg;
	while (*cp != '\0') {
		if (*cp++ == delim) {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

static void
appdef(lstp, def)
	char	***lstp;
	const	char *def;
{
	appstrg(lstp, concat2("-D__", def));
	appstrg(lstp, concat3("-D__", def, "__"));
}

static void
usage()
{
	(void)printf("lint [-abceghprvxzHF] [-s|-t] [-i|-nu] [-Dname[=def]] [-Uname]\n");
	(void)printf("     [-Idirectory] [-Ldirectory] [-llibrary] [-ooutputfile] file ...\n");
	(void)printf("\n");
	(void)printf("lint [-abceghprvzHF] [-s|-t] -Clibrary [-Dname[=def]]\n");
	(void)printf("     [-Idirectory] [-Uname] file ...\n");
	terminate(-1);
}

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c;
	char	flgbuf[3], *tmp, *s;
	size_t	len;
	struct	utsname un;

	if ((tmp = getenv("TMPDIR")) == NULL || (len = strlen(tmp)) == 0) {
		tmpdir = xstrdup(_PATH_TMP);
	} else {
		s = xmalloc(len + 2);
		(void)sprintf(s, "%s%s", tmp, tmp[len - 1] == '/' ? "" : "/");
		tmpdir = s;
	}

	cppout = xmalloc(strlen(tmpdir) + sizeof ("lint0.XXXXXX"));
	(void)sprintf(cppout, "%slint0.XXXXXX", tmpdir);
	if (mktemp(cppout) == NULL) {
		warn("can't make temp");
		terminate(-1);
	}

	p1out = xcalloc(1, sizeof (char *));
	p2in = xcalloc(1, sizeof (char *));
	cppflags = xcalloc(1, sizeof (char *));
	lcppflgs = xcalloc(1, sizeof (char *));
	l1flags = xcalloc(1, sizeof (char *));
	l2flags = xcalloc(1, sizeof (char *));
	l2libs = xcalloc(1, sizeof (char *));
	deflibs = xcalloc(1, sizeof (char *));
	libs = xcalloc(1, sizeof (char *));
	libsrchpath = xcalloc(1, sizeof (char *));

	appcstrg(&cppflags, "-lang-c");
	appcstrg(&cppflags, "-$");
	appcstrg(&cppflags, "-C");
	appcstrg(&cppflags, "-Wcomment");
#ifdef __FreeBSD__
	appcstrg(&cppflags, "-D__FreeBSD__=" __XSTRING(__FreeBSD__));
#else
#	error "This ain't NetBSD.  You lose!"
	appcstrg(&cppflags, "-D__NetBSD__");
#endif
	appcstrg(&cppflags, "-Dlint");		/* XXX don't def. with -s */
	appdef(&cppflags, "lint");
	appdef(&cppflags, "unix");

	appcstrg(&lcppflgs, "-Wtraditional");

	if (uname(&un) == -1)
		err(1, "uname");
	appdef(&cppflags, un.machine);
	appstrg(&lcppflgs, concat2("-D", un.machine));

#ifdef MACHINE_ARCH
	if (strcmp(un.machine, MACHINE_ARCH) != 0) {
		appdef(&cppflags, MACHINE_ARCH);
		appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
	}
#endif
	
	appcstrg(&deflibs, "c");

	if (signal(SIGHUP, terminate) == SIG_IGN)
		(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, terminate);
	(void)signal(SIGQUIT, terminate);
	(void)signal(SIGTERM, terminate);

	while (argc > optind) {

		argc -= optind;
		argv += optind;
		optind = 0;

		c = getopt(argc, argv, "abceghil:no:prstuvxzC:D:FHI:L:U:V");

		switch (c) {

		case 'a':
		case 'b':
		case 'c':
		case 'e':
		case 'g':
		case 'r':
		case 'v':
		case 'z':
			(void)sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			break;

		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'u':
		case 'h':
			(void)sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			appcstrg(&l2flags, flgbuf);
			break;

		case 'i':
			if (Cflag)
				usage();
			iflag = 1;
			break;

		case 'n':
			freelst(&deflibs);
			break;

		case 'p':
			appcstrg(&l1flags, "-p");
			appcstrg(&l2flags, "-p");
			if (*deflibs != NULL) {
				freelst(&deflibs);
				appcstrg(&deflibs, "c");
			}
			break;

		case 's':
			if (tflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-trigraphs");
			appcstrg(&lcppflgs, "-Wtrigraphs");
			appcstrg(&lcppflgs, "-pedantic");
			appcstrg(&lcppflgs, "-D__STRICT_ANSI__");
			appcstrg(&l1flags, "-s");
			appcstrg(&l2flags, "-s");
			sflag = 1;
			break;

		case 't':
			if (sflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-traditional");
			appstrg(&lcppflgs, concat2("-D", MACHINE));
#ifdef MACHINE_ARCH
			appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
#endif
			appcstrg(&l1flags, "-t");
			appcstrg(&l2flags, "-t");
			tflag = 1;
			break;

		case 'x':
			appcstrg(&l2flags, "-x");
			break;

		case 'C':
			if (Cflag || oflag || iflag)
				usage();
			Cflag = 1;
			appstrg(&l2flags, concat2("-C", optarg));
			p2out = xmalloc(sizeof ("llib-l.ln") + strlen(optarg));
			(void)sprintf(p2out, "llib-l%s.ln", optarg);
			freelst(&deflibs);
			break;

		case 'D':
		case 'I':
		case 'U':
			(void)sprintf(flgbuf, "-%c", c);
			appstrg(&cppflags, concat2(flgbuf, optarg));
			break;

		case 'l':
			appcstrg(&libs, optarg);
			break;

		case 'o':
			if (Cflag || oflag)
				usage();
			oflag = 1;
			outputfn = xstrdup(optarg);
			break;

		case 'L':
			appcstrg(&libsrchpath, optarg);
			break;

		case 'H':
			appcstrg(&l2flags, "-H");
			break;

		case 'V':
			Vflag = 1;
			break;

		case '?':
			usage();
			/* NOTREACHED */

		case -1:
			/* filename */
			fname(argv[0], argc == 1);
			first = 0;
			optind = 1;
		}

	}

	if (first)
		usage();

	if (iflag)
		terminate(0);

	if (!oflag) {
		if ((s = getenv("LIBDIR")) == NULL || strlen(s) == 0)
			s = PATH_LINTLIB;
		appcstrg(&libsrchpath, s);
		findlibs(libs);
		findlibs(deflibs);
	}

	(void)printf("Lint pass2:\n");
	lint2();

	if (oflag)
		cat(p2in, outputfn);

	if (Cflag)
		p2out = NULL;

	terminate(0);
	/* NOTREACHED */
	return 0;
}

/*
 * Read a file name from the command line
 * and pass it through lint1 if it is a C source.
 */
static void
fname(name, last)
	const	char *name;
	int	last;
{
	const	char *bn, *suff;
	char	**args, *ofn, *path;
	size_t	len;

	bn = basename(name, '/');
	suff = basename(bn, '.');

	if (strcmp(suff, "ln") == 0) {
		/* only for lint2 */
		if (!iflag)
			appcstrg(&p2in, name);
		return;
	}

	if (strcmp(suff, "c") != 0 &&
	    (strncmp(bn, "llib-l", 6) != 0 || bn != suff)) {
		warnx("unknown file type: %s\n", name);
		return;
	}

	if (!iflag || !first || !last)
		(void)printf("%s:\n", Fflag ? name : bn);

	/* build the name of the output file of lint1 */
	if (oflag) {
		ofn = outputfn;
		outputfn = NULL;
		oflag = 0;
	} else if (iflag) {
		ofn = xmalloc(strlen(bn) + (bn == suff ? 4 : 2));
		len = bn == suff ? strlen(bn) : (suff - 1) - bn;
		(void)sprintf(ofn, "%.*s", (int)len, bn);
		(void)strcat(ofn, ".ln");
	} else {
		ofn = xmalloc(strlen(tmpdir) + sizeof ("lint1.XXXXXX"));
		(void)sprintf(ofn, "%slint1.XXXXXX", tmpdir);
		if (mktemp(ofn) == NULL) {
			warn("can't make temp");
			terminate(-1);
		}
	}
	if (!iflag)
		appcstrg(&p1out, ofn);

	args = xcalloc(1, sizeof (char *));

	/* run cpp */

	path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/cpp"));
	(void)sprintf(path, "%s/cpp", PATH_LIBEXEC);

	appcstrg(&args, path);
	applst(&args, cppflags);
	applst(&args, lcppflgs);
	appcstrg(&args, name);
	appcstrg(&args, cppout);

	runchild(path, args, cppout);
	free(path);
	freelst(&args);

	/* run lint1 */

	path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint1"));
	(void)sprintf(path, "%s/lint1", PATH_LIBEXEC);

	appcstrg(&args, path);
	applst(&args, l1flags);
	appcstrg(&args, cppout);
	appcstrg(&args, ofn);

	runchild(path, args, ofn);
	free(path);
	freelst(&args);

	appcstrg(&p2in, ofn);
	free(ofn);

	free(args);
}

static void
runchild(path, args, crfn)
	const	char *path, *crfn;
	char	*const *args;
{
	int	status, rv, signo, i;

	if (Vflag) {
		for (i = 0; args[i] != NULL; i++)
			(void)printf("%s ", args[i]);
		(void)printf("\n");
	}

	currfn = crfn;

	(void)fflush(stdout);

	switch (fork()) {
	case -1:
		warn("cannot fork");
		terminate(-1);
		/* NOTREACHED */
	default:
		/* parent */
		break;
	case 0:
		/* child */
		(void)execv(path, args);
		warn("cannot exec %s", path);
		exit(1);
		/* NOTREACHED */
	}

	while ((rv = wait(&status)) == -1 && errno == EINTR) ;
	if (rv == -1) {
		warn("wait");
		terminate(-1);
	}
	if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		warnx("%s got SIG%s", path, sys_signame[signo]);
		terminate(-1);
	}
	if (WEXITSTATUS(status) != 0)
		terminate(-1);
	currfn = NULL;
}

static void
findlibs(liblst)
	char	*const *liblst;
{
	int	i, k;
	const	char *lib, *path;
	char	*lfn;
	size_t	len;

	lfn = NULL;

	for (i = 0; (lib = liblst[i]) != NULL; i++) {
		for (k = 0; (path = libsrchpath[k]) != NULL; k++) {
			len = strlen(path) + strlen(lib);
			lfn = xrealloc(lfn, len + sizeof ("/llib-l.ln"));
			(void)sprintf(lfn, "%s/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
			lfn = xrealloc(lfn, len + sizeof ("/lint/llib-l.ln"));
			(void)sprintf(lfn, "%s/lint/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
		}
		if (path != NULL) {
			appstrg(&l2libs, concat2("-l", lfn));
		} else {
			warnx("cannot find llib-l%s.ln", lib);
		}
	}

	free(lfn);
}

static int
rdok(path)
	const	char *path;
{
	struct	stat sbuf;

	if (stat(path, &sbuf) == -1)
		return (0);
	if ((sbuf.st_mode & S_IFMT) != S_IFREG)
		return (0);
	if (access(path, R_OK) == -1)
		return (0);
	return (1);
}

static void
lint2()
{
	char	*path, **args;

	args = xcalloc(1, sizeof (char *));

	path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint2"));
	(void)sprintf(path, "%s/lint2", PATH_LIBEXEC);
	
	appcstrg(&args, path);
	applst(&args, l2flags);
	applst(&args, l2libs);
	applst(&args, p2in);

	runchild(path, args, p2out);
	free(path);
	freelst(&args);
	free(args);
}

static void
cat(srcs, dest)
	char	*const *srcs;
	const	char *dest;
{
	int	ifd, ofd, i;
	char	*src, *buf;
	ssize_t	rlen;

	if ((ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		warn("cannot open %s", dest);
		terminate(-1);
	}

	buf = xmalloc(MBLKSIZ);

	for (i = 0; (src = srcs[i]) != NULL; i++) {
		if ((ifd = open(src, O_RDONLY)) == -1) {
			free(buf);
			warn("cannot open %s", src);
			terminate(-1);
		}
		do {
			if ((rlen = read(ifd, buf, MBLKSIZ)) == -1) {
				free(buf);
				warn("read error on %s", src);
				terminate(-1);
			}
			if (write(ofd, buf, (size_t)rlen) == -1) {
				free(buf);
				warn("write error on %s", dest);
				terminate(-1);
			}
		} while (rlen == MBLKSIZ);
		(void)close(ifd);
	}
	(void)close(ofd);
	free(buf);
}

