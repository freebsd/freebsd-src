/*
 * Copyright (c) 1983 Regents of the University of California.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1989 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)lpr.c	5.9 (Berkeley) 3/2/91";
#endif /* not lint */
/*
 *      lpr -- off line print
 *
 * Allows multiple printers and printers on remote machines by
 * using information from a printer data base.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>
#include "lp.local.h"
#include "pathnames.h"

char    *tfname;		/* tmp copy of cf before linking */
char    *cfname;		/* daemon control files, linked from tf's */
char    *dfname;		/* data files */

int	nact;			/* number of jobs to act on */
int	tfd;			/* control file descriptor */
int     mailflg;		/* send mail */
int	qflag;			/* q job, but don't exec daemon */
char	format = 'f';		/* format char for printing files */
int	rflag;			/* remove files upon completion */	
int	sflag;			/* symbolic link flag */
int	inchar;			/* location to increment char in file names */
int     ncopies = 1;		/* # of copies to make */
int	iflag;			/* indentation wanted */
int	indent;			/* amount to indent */
int	hdr = 1;		/* print header or not (default is yes) */
int     userid;			/* user id */
char	*person;		/* user name */
char	*title;			/* pr'ing title */
char	*fonts[4];		/* troff font names */
char	*width;			/* width for versatec printing */
char	host[32];		/* host name */
char	*class = host;		/* class title on header page */
char    *jobname;		/* job name on header page */
char	*name;			/* program name */
char	*printer;		/* printer name */
struct	stat statb;

int	MX;			/* maximum number of blocks to copy */
int	MC;			/* maximum number of copies allowed */
int	DU;			/* daemon user-id */
char	*SD;			/* spool directory */
char	*LO;			/* lock file name */
char	*RG;			/* restrict group */
short	SC;			/* suppress multiple copies */

char	*getenv();
char	*rindex();
char	*linked();
void	cleanup();

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	struct passwd *pw;
	struct group *gptr;
	extern char *itoa();
	register char *arg, *cp;
	char buf[BUFSIZ];
	int i, f;
	struct stat stb;

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, cleanup);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, cleanup);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, cleanup);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, cleanup);

	name = argv[0];
	gethostname(host, sizeof (host));
	openlog("lpd", 0, LOG_LPR);

	while (argc > 1 && argv[1][0] == '-') {
		argc--;
		arg = *++argv;
		switch (arg[1]) {

		case 'P':		/* specifiy printer name */
			if (arg[2])
				printer = &arg[2];
			else if (argc > 1) {
				argc--;
				printer = *++argv;
			}
			break;

		case 'C':		/* classification spec */
			hdr++;
			if (arg[2])
				class = &arg[2];
			else if (argc > 1) {
				argc--;
				class = *++argv;
			}
			break;

		case 'U':		/* user name */
			hdr++;
			if (arg[2])
				person = &arg[2];
			else if (argc > 1) {
				argc--;
				person = *++argv;
			}
			break;

		case 'J':		/* job name */
			hdr++;
			if (arg[2])
				jobname = &arg[2];
			else if (argc > 1) {
				argc--;
				jobname = *++argv;
			}
			break;

		case 'T':		/* pr's title line */
			if (arg[2])
				title = &arg[2];
			else if (argc > 1) {
				argc--;
				title = *++argv;
			}
			break;

		case 'l':		/* literal output */
		case 'p':		/* print using ``pr'' */
		case 't':		/* print troff output (cat files) */
		case 'n':		/* print ditroff output */
		case 'd':		/* print tex output (dvi files) */
		case 'g':		/* print graph(1G) output */
		case 'c':		/* print cifplot output */
		case 'v':		/* print vplot output */
			format = arg[1];
			break;

		case 'f':		/* print fortran output */
			format = 'r';
			break;

		case '4':		/* troff fonts */
		case '3':
		case '2':
		case '1':
			if (argc > 1) {
				argc--;
				fonts[arg[1] - '1'] = *++argv;
			}
			break;

		case 'w':		/* versatec page width */
			width = arg+2;
			break;

		case 'r':		/* remove file when done */
			rflag++;
			break;

		case 'm':		/* send mail when done */
			mailflg++;
			break;

		case 'h':		/* toggle want of header page */
			hdr = !hdr;
			break;

		case 's':		/* try to link files */
			sflag++;
			break;

		case 'q':		/* just q job */
			qflag++;
			break;

		case 'i':		/* indent output */
			iflag++;
			indent = arg[2] ? atoi(&arg[2]) : 8;
			break;

		case '#':		/* n copies */
			if (isdigit(arg[2])) {
				i = atoi(&arg[2]);
				if (i > 0)
					ncopies = i;
			}
		}
	}
	if (printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;
	chkprinter(printer);
	if (SC && ncopies > 1)
		fatal("multiple copies are not allowed");
	if (MC > 0 && ncopies > MC)
		fatal("only %d copies are allowed", MC);
	/*
	 * Get the identity of the person doing the lpr using the same
	 * algorithm as lprm. 
	 */
	userid = getuid();
	if (userid != DU || person == 0) {
		if ((pw = getpwuid(userid)) == NULL)
			fatal("Who are you?");
		person = pw->pw_name;
	}
	/*
	 * Check for restricted group access.
	 */
	if (RG != NULL && userid != DU) {
		if ((gptr = getgrnam(RG)) == NULL)
			fatal("Restricted group specified incorrectly");
		if (gptr->gr_gid != getgid()) {
			while (*gptr->gr_mem != NULL) {
				if ((strcmp(person, *gptr->gr_mem)) == 0)
					break;
				gptr->gr_mem++;
			}
			if (*gptr->gr_mem == NULL)
				fatal("Not a member of the restricted group");
		}
	}
	/*
	 * Check to make sure queuing is enabled if userid is not root.
	 */
	(void) sprintf(buf, "%s/%s", SD, LO);
	if (userid && stat(buf, &stb) == 0 && (stb.st_mode & 010))
		fatal("Printer queue is disabled");
	/*
	 * Initialize the control file.
	 */
	mktemps();
	tfd = nfile(tfname);
	(void) fchown(tfd, DU, -1);	/* owned by daemon for protection */
	card('H', host);
	card('P', person);
	if (hdr) {
		if (jobname == NULL) {
			if (argc == 1)
				jobname = "stdin";
			else
				jobname = (arg = rindex(argv[1], '/')) ? arg+1 : argv[1];
		}
		card('J', jobname);
		card('C', class);
		card('L', person);
	}
	if (iflag)
		card('I', itoa(indent));
	if (mailflg)
		card('M', person);
	if (format == 't' || format == 'n' || format == 'd')
		for (i = 0; i < 4; i++)
			if (fonts[i] != NULL)
				card('1'+i, fonts[i]);
	if (width != NULL)
		card('W', width);

	/*
	 * Read the files and spool them.
	 */
	if (argc == 1)
		copy(0, " ");
	else while (--argc) {
		if ((f = test(arg = *++argv)) < 0)
			continue;	/* file unreasonable */

		if (sflag && (cp = linked(arg)) != NULL) {
			(void) sprintf(buf, "%d %d", statb.st_dev, statb.st_ino);
			card('S', buf);
			if (format == 'p')
				card('T', title ? title : arg);
			for (i = 0; i < ncopies; i++)
				card(format, &dfname[inchar-2]);
			card('U', &dfname[inchar-2]);
			if (f)
				card('U', cp);
			card('N', arg);
			dfname[inchar]++;
			nact++;
			continue;
		}
		if (sflag)
			printf("%s: %s: not linked, copying instead\n", name, arg);
		if ((i = open(arg, O_RDONLY)) < 0) {
			printf("%s: cannot open %s\n", name, arg);
			continue;
		}
		copy(i, arg);
		(void) close(i);
		if (f && unlink(arg) < 0)
			printf("%s: %s: not removed\n", name, arg);
	}

	if (nact) {
		(void) close(tfd);
		tfname[inchar]--;
		/*
		 * Touch the control file to fix position in the queue.
		 */
		if ((tfd = open(tfname, O_RDWR)) >= 0) {
			char c;

			if (read(tfd, &c, 1) == 1 && lseek(tfd, 0L, 0) == 0 &&
			    write(tfd, &c, 1) != 1) {
				printf("%s: cannot touch %s\n", name, tfname);
				tfname[inchar]++;
				cleanup();
			}
			(void) close(tfd);
		}
		if (link(tfname, cfname) < 0) {
			printf("%s: cannot rename %s\n", name, cfname);
			tfname[inchar]++;
			cleanup();
		}
		unlink(tfname);
		if (qflag)		/* just q things up */
			exit(0);
		if (!startdaemon(printer))
			printf("jobs queued, but cannot start daemon.\n");
		exit(0);
	}
	cleanup();
	/* NOTREACHED */
}

/*
 * Create the file n and copy from file descriptor f.
 */
copy(f, n)
	int f;
	char n[];
{
	register int fd, i, nr, nc;
	char buf[BUFSIZ];

	if (format == 'p')
		card('T', title ? title : n);
	for (i = 0; i < ncopies; i++)
		card(format, &dfname[inchar-2]);
	card('U', &dfname[inchar-2]);
	card('N', n);
	fd = nfile(dfname);
	nr = nc = 0;
	while ((i = read(f, buf, BUFSIZ)) > 0) {
		if (write(fd, buf, i) != i) {
			printf("%s: %s: temp file write error\n", name, n);
			break;
		}
		nc += i;
		if (nc >= BUFSIZ) {
			nc -= BUFSIZ;
			nr++;
			if (MX > 0 && nr > MX) {
				printf("%s: %s: copy file is too large\n", name, n);
				break;
			}
		}
	}
	(void) close(fd);
	if (nc==0 && nr==0) 
		printf("%s: %s: empty input file\n", name, f ? n : "stdin");
	else
		nact++;
}

/*
 * Try and link the file to dfname. Return a pointer to the full
 * path name if successful.
 */
char *
linked(file)
	register char *file;
{
	register char *cp;
	static char buf[BUFSIZ];

	if (*file != '/') {
		if (getwd(buf) == NULL)
			return(NULL);
		while (file[0] == '.') {
			switch (file[1]) {
			case '/':
				file += 2;
				continue;
			case '.':
				if (file[2] == '/') {
					if ((cp = rindex(buf, '/')) != NULL)
						*cp = '\0';
					file += 3;
					continue;
				}
			}
			break;
		}
		strcat(buf, "/");
		strcat(buf, file);
		file = buf;
	}
	return(symlink(file, dfname) ? NULL : file);
}

/*
 * Put a line into the control file.
 */
card(c, p2)
	register char c, *p2;
{
	char buf[BUFSIZ];
	register char *p1 = buf;
	register int len = 2;

	*p1++ = c;
	while ((c = *p2++) != '\0') {
		*p1++ = (c == '\n') ? ' ' : c;
		len++;
	}
	*p1++ = '\n';
	write(tfd, buf, len);
}

/*
 * Create a new file in the spool directory.
 */
nfile(n)
	char *n;
{
	register f;
	int oldumask = umask(0);		/* should block signals */

	f = creat(n, FILMOD);
	(void) umask(oldumask);
	if (f < 0) {
		printf("%s: cannot create %s\n", name, n);
		cleanup();
	}
	if (fchown(f, userid, -1) < 0) {
		printf("%s: cannot chown %s\n", name, n);
		cleanup();
	}
	if (++n[inchar] > 'z') {
		if (++n[inchar-2] == 't') {
			printf("too many files - break up the job\n");
			cleanup();
		}
		n[inchar] = 'A';
	} else if (n[inchar] == '[')
		n[inchar] = 'a';
	return(f);
}

/*
 * Cleanup after interrupts and errors.
 */
void
cleanup()
{
	register i;

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	i = inchar;
	if (tfname)
		do
			unlink(tfname);
		while (tfname[i]-- != 'A');
	if (cfname)
		do
			unlink(cfname);
		while (cfname[i]-- != 'A');
	if (dfname)
		do {
			do
				unlink(dfname);
			while (dfname[i]-- != 'A');
			dfname[i] = 'z';
		} while (dfname[i-2]-- != 'd');
	exit(1);
}

/*
 * Test to see if this is a printable file.
 * Return -1 if it is not, 0 if its printable, and 1 if
 * we should remove it after printing.
 */
test(file)
	char *file;
{
	struct exec execb;
	register int fd;
	register char *cp;

	if (access(file, 4) < 0) {
		printf("%s: cannot access %s\n", name, file);
		return(-1);
	}
	if (stat(file, &statb) < 0) {
		printf("%s: cannot stat %s\n", name, file);
		return(-1);
	}
	if ((statb.st_mode & S_IFMT) == S_IFDIR) {
		printf("%s: %s is a directory\n", name, file);
		return(-1);
	}
	if (statb.st_size == 0) {
		printf("%s: %s is an empty file\n", name, file);
		return(-1);
 	}
	if ((fd = open(file, O_RDONLY)) < 0) {
		printf("%s: cannot open %s\n", name, file);
		return(-1);
	}
	if (read(fd, &execb, sizeof(execb)) == sizeof(execb))
		switch(execb.a_magic) {
		case A_MAGIC1:
		case A_MAGIC2:
		case A_MAGIC3:
#ifdef A_MAGIC4
		case A_MAGIC4:
#endif
			printf("%s: %s is an executable program", name, file);
			goto error1;

		case ARMAG:
			printf("%s: %s is an archive file", name, file);
			goto error1;
		}
	(void) close(fd);
	if (rflag) {
		if ((cp = rindex(file, '/')) == NULL) {
			if (access(".", 2) == 0)
				return(1);
		} else {
			*cp = '\0';
			fd = access(file, 2);
			*cp = '/';
			if (fd == 0)
				return(1);
		}
		printf("%s: %s: is not removable by you\n", name, file);
	}
	return(0);

error1:
	printf(" and is unprintable\n");
	(void) close(fd);
	return(-1);
}

/*
 * itoa - integer to string conversion
 */
char *
itoa(i)
	register int i;
{
	static char b[10] = "########";
	register char *p;

	p = &b[8];
	do
		*p-- = i%10 + '0';
	while (i /= 10);
	return(++p);
}

/*
 * Perform lookup for printer name or abbreviation --
 */
chkprinter(s)
	char *s;
{
	int status;
	char buf[BUFSIZ];
	static char pbuf[BUFSIZ/2];
	char *bp = pbuf;
	extern char *pgetstr();

	if ((status = pgetent(buf, s)) < 0)
		fatal("cannot open printer description file");
	else if (status == 0)
		fatal("%s: unknown printer", s);
	if ((SD = pgetstr("sd", &bp)) == NULL)
		SD = _PATH_DEFSPOOL;
	if ((LO = pgetstr("lo", &bp)) == NULL)
		LO = DEFLOCK;
	RG = pgetstr("rg", &bp);
	if ((MX = pgetnum("mx")) < 0)
		MX = DEFMX;
	if ((MC = pgetnum("mc")) < 0)
		MC = DEFMAXCOPIES;
	if ((DU = pgetnum("du")) < 0)
		DU = DEFUID;
	SC = pgetflag("sc");
}

/*
 * Make the temp files.
 */
mktemps()
{
	register int c, len, fd, n;
	register char *cp;
	char buf[BUFSIZ];
	char *lmktemp();

	(void) sprintf(buf, "%s/.seq", SD);
	if ((fd = open(buf, O_RDWR|O_CREAT, 0661)) < 0) {
		printf("%s: cannot create %s\n", name, buf);
		exit(1);
	}
	if (flock(fd, LOCK_EX)) {
		printf("%s: cannot lock %s\n", name, buf);
		exit(1);
	}
	n = 0;
	if ((len = read(fd, buf, sizeof(buf))) > 0) {
		for (cp = buf; len--; ) {
			if (*cp < '0' || *cp > '9')
				break;
			n = n * 10 + (*cp++ - '0');
		}
	}
	len = strlen(SD) + strlen(host) + 8;
	tfname = lmktemp("tf", n, len);
	cfname = lmktemp("cf", n, len);
	dfname = lmktemp("df", n, len);
	inchar = strlen(SD) + 3;
	n = (n + 1) % 1000;
	(void) lseek(fd, 0L, 0);
	sprintf(buf, "%03d\n", n);
	(void) write(fd, buf, strlen(buf));
	(void) close(fd);	/* unlocks as well */
}

/*
 * Make a temp file name.
 */
char *
lmktemp(id, num, len)
	char	*id;
	int	num, len;
{
	register char *s;
	extern char *malloc();

	if ((s = malloc(len)) == NULL)
		fatal("out of memory");
	(void) sprintf(s, "%s/%sA%03d%s", SD, id, num, host);
	return(s);
}

/*VARARGS1*/
fatal(msg, a1, a2, a3)
	char *msg;
{
	printf("%s: ", name);
	printf(msg, a1, a2, a3);
	putchar('\n');
	exit(1);
}
