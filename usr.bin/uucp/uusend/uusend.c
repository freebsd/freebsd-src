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
static char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)uusend.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * uusend: primitive operation to allow uucp like copy of binary files
 * but handle indirection over systems.
 *
 * usage: uusend [-r] [-m ooo] localfile sysname1!sysname2!...!destfile
 *        uusend [-r] [-m ooo]     -     sysname1!sysname2!...!destfile
 *
 * Author: Mark Horton, May 1980.
 *
 * "-r" switch added.  Has same effect as "-r" in uux. 11/82  CCW
 *
 * Error recovery (a la uucp) added & ifdefs for ruusend (as in rmail).
 * Checks for illegal access to /usr/lib/uucp.
 *				February 1983  Christopher Woodbury
 * Fixed mode set[ug]id loophole. 4/8/83  CCW
 *
 * Add '-f' to make uusend syntax more similar to UUCP.  "destname"
 * can now be a directory.	June 1983  CCW
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <pwd.h>

/*
 * define RECOVER to permit requests like 'uusend file sys1!sys2!~uucp'
 * (abbreviation for 'uusend file sys1!sys2!~uucp/file').
 * define DEBUG to keep log of uusend uusage.
 * define RUUSEND if neighboring sites permit 'ruusend',
 * which they certainly should to avoid security holes
 */
#define	RECOVER
/*#define	DEBUG	"/usr/spool/uucp/uusend.log"/**/

FILE	*in, *out;
FILE	*dout;

extern FILE	*popen();
extern char	*index(), *strcpy(), *strcat(), *ctime();

#ifdef	RUUSEND
int	rsend;
#endif  RUUSEND
int	mode = -1;	/* mode to chmod new file to */
char	*nextsys;	/* next system in the chain */
char	dnbuf[200];	/* buffer for result of ~user/file */
char	cmdbuf[256];	/* buffer to build uux command in */
char	*rflg = "";	/* default value of rflg  ccw -- 1 Nov '82 */

struct	passwd *user;	/* entry  in /etc/passwd for ~user */
struct	passwd *getpwnam();
struct	stat	stbuf;

char	*excl;		/* location of first ! in destname */
char	*sl;		/* location of first / in destname */
char	*sourcename;	/* argv[1] */
char	*destname;	/* argv[2] */
char	*UULIB = "/usr/lib/uucp";	  /* UUCP lib directory */

#ifdef	RECOVER
char	*UUPUB = "/usr/spool/uucppublic/";  /* public UUCP directory */
char	*filename;	/* file name from end of destname */
char	*getfname();	/* routine to get filename from destname */
int	fflg;
char	f[100];		/* name of default output file */
#else	!RECOVER
char	*f	= "";	/* so we waste a little space */
#endif	!RECOVER

main(argc, argv)
int	argc;
char	**argv;
{
	register int c;
	long count;
	extern char **environ;

#ifdef DEBUG
	long t;
	umask(022);
	dout = fopen(DEBUG, "a");
	if (dout == NULL) {
		printf("Cannot append to %s\n", DEBUG);
		exit(1);
	}
	freopen(DEBUG, "a", stdout);
	fprintf(dout, "\nuusend run: ");
	for (c=0; c<argc; c++)
		fprintf(dout, "%s ", argv[c]);
	time(&t);
	fprintf(dout, "%s", ctime(&t));
#endif DEBUG

#ifdef	RUUSEND
	if(argv[0][0] == 'r')
		rsend++;
#endif RUUSEND
	while (argc > 1 && argv[1][0] == '-' && argv[1][1]) {
		switch(argv[1][1]) {
		case 'm':
			sscanf(argv[2], "%o", &mode);
			mode &= 0777;  /* fix set[ug]id loophole */
			argc--; argv++;
			break;
		case 'r':		/* -r flag for uux */
			rflg = "-r ";
			break;
#ifdef	RECOVER
		case 'f':
			fflg++;
			strcpy(f, argv[1]);
			break;
#endif RECOVER
		default:
			fprintf(stderr, "Bad flag: %s\n", argv[1]);
			break;
		}
		argc--; argv++;
	}

	if (argc != 3) {
		fprintf(stderr, "Usage: uusend [-m ooo] [-r] -/file sys!sys!..!rfile\n");
		exit(1);
	}

	sourcename = argv[1];
	destname = argv[2];

	if (sourcename[0] == '-')
		in = stdin;
	else {
#ifdef	RUUSEND
		if (rsend) {
			fprintf(stderr, "illegal input\n");
			exit(2);
		}
#endif RUUSEND
		in = fopen(sourcename, "r");
		if (in == NULL) {
			perror(argv[1]);
			exit(2);
		}
		if (!fflg || f[2] == '\0') {
			strcpy(f, "-f");
			strcat(f, getfname(sourcename));
			fflg++;
		}
	}

	excl = index(destname, '!');
	if (excl) {
		/*
		 * destname is on a remote system.
		 */
		nextsys = destname;
		*excl++ = 0;
		destname = excl;
		if (mode < 0) {
			fstat(fileno(in), &stbuf);
			mode = stbuf.st_mode & 0777;
		}
#ifdef	RUUSEND
		sprintf(cmdbuf,"uux -gn -z %s- \"%s!ruusend %s -m %o - (%s)\"",
#else !RUUSEND
		sprintf(cmdbuf, "uux -gn -z %s- \"%s!uusend %s -m %o - (%s)\"",
#endif !RUUSEND
			rflg, nextsys, f, mode, destname);
#ifdef DEBUG
		fprintf(dout, "remote: nextsys='%s', destname='%s', cmd='%s'\n", nextsys, destname, cmdbuf);
#endif DEBUG
		out = popen(cmdbuf, "w");
	} else {
		/*
		 * destname is local.
		 */
		if (destname[0] == '~') {
#ifdef DEBUG
			fprintf(dout, "before ~: '%s'\n", destname);
fflush(dout);
#endif DEBUG
			sl = index(destname, '/');
#ifdef	RECOVER
			if (sl == NULL && !fflg) {
				fprintf(stderr, "Illegal ~user\n");
				exit(3);
			}
			for (sl = destname; *sl != '\0'; sl++)
				;	/* boy, is this a hack! */
#else !RECOVER
			if (sl == NULL) {
				fprintf(stderr, "Illegal ~user\n");
				exit(3);
			}
			*sl++ = 0;
#endif !RECOVER
			user = getpwnam(destname+1);
			if (user == NULL) {
				fprintf(stderr, "No such user as %s\n",
					destname);
#ifdef	RECOVER
				if ((filename =getfname(sl)) == NULL &&
				     !fflg)
					exit(4);
				strcpy(dnbuf, UUPUB);
				if (fflg)
					strcat(dnbuf, &f[2]);
				else
					strcat(dnbuf, filename);
			}
			else {
				strcpy(dnbuf, user->pw_dir);
				strcat(dnbuf, "/");
				strcat(dnbuf, sl);
			}
#else !RECOVER
				exit(4);
			}
			strcpy(dnbuf, user->pw_dir);
			strcat(dnbuf, "/");
			strcat(dnbuf, sl);
#endif !RECOVER
			destname = dnbuf;
		}
#ifdef	RECOVER
		else
			destname = strcpy(dnbuf, destname);
#endif !RECOVER
		if(strncmp(UULIB, destname, strlen(UULIB)) == 0) {
			fprintf(stderr, "illegal file: %s", destname);
			exit(4);
		}
#ifdef	RECOVER
		if (stat(destname, &stbuf) == 0 &&
		    (stbuf.st_mode & S_IFMT) == S_IFDIR &&
		     fflg) {
			strcat(destname, "/");
			strcat(destname, &f[2]);
		}
#endif RECOVER
		out = fopen(destname, "w");
#ifdef DEBUG
		fprintf(dout, "local, file='%s'\n", destname);
#endif DEBUG
		if (out == NULL) {
			perror(destname);
#ifdef	RECOVER
			if (strncmp(destname,UUPUB,strlen(UUPUB)) == 0)
				exit(5);	/* forget it! */
			filename = getfname(destname);
			if (destname == dnbuf) /* cmdbuf is scratch */
				filename = strcpy(cmdbuf, filename);
			destname = strcpy(dnbuf, UUPUB);
			if (user != NULL) {
				strcat(destname, user->pw_name);
				if (stat(destname, &stbuf) == -1) {
					mkdir(destname, 0777);
				}
				strcat(destname, "/");
			}
			if (fflg)
				strcat(destname, &f[2]);
			else
				strcat(destname, filename);
			if ((out = fopen(destname, "w")) == NULL)
				exit(5); /* all for naught! */
#else !RECOVER
			exit(5);
#endif !RECOVER
		}
		if (mode > 0)
			chmod(destname, mode);	/* don't bother to check it */
	}

	/*
	 * Now, in any case, copy from in to out.
	 */

	count = 0;
	while ((c=getc(in)) != EOF) {
		putc(c, out);
		count++;
	}
#ifdef DEBUG
	fprintf(dout, "count %ld bytes\n", count);
	fclose(dout);
#endif DEBUG

	fclose(in);
	fclose(out);	/* really should pclose in that case */
	exit(0);
}

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found.  Included so I don't have to fight the
 * index/strchr battle.
 */

#define	NULL	0

char *
index(sp, c)
register char *sp, c;
{
	do {
		if (*sp == c)
			return(sp);
	} while (*sp++);
	return(NULL);
}

#ifdef	RECOVER
char *
getfname(p)
register char *p;
{
	register char *s;
	s = p;
	while (*p != '\0')
		p++;
	if (p == s)
		return (NULL);
	for (;p != s; p--)
		if (*p == '/') {
			p++;
			break;
		}
	return (p);
}

#ifndef BSD4_2
makedir(dirname, mode)
char *dirname;
int mode;
{
	register int pid;
	int retcode, status;
	switch ((pid = fork())) {
	    case -1:		/* error */
		return (-1);
	    case 0:		/* child */
		umask(0);
		execl("/bin/mkdir", "mkdir", dirname, (char *)0);
		exit(1);
		/* NOTREACHED */
	    default:		/* parent */
		while ((retcode=wait(&status)) != pid && retcode != -1)
			;
		if (retcode == -1)
			return  -1;
		else {
			chmod(dirname, mode);
			return status;
		}
	}
	/* NOTREACHED */
}
#endif !BSD4_2
#endif RECOVER
