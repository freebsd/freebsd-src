/*
 * scp - secure remote copy.  This is basically patched BSD rcp which
 * uses ssh to do the data transfer (instead of using rcmd).
 *
 * NOTE: This version should NOT be suid root.  (This uses ssh to
 * do the transfer and ssh has the necessary privileges.)
 *
 * 1995 Timo Rinne <tri@iki.fi>, Tatu Ylonen <ylo@cs.hut.fi>
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
/*
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1999 Aaron Campbell.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Parts from:
 *
 * Copyright (c) 1983, 1990, 1992, 1993, 1995
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
 *
 */

#include "includes.h"
RCSID("$OpenBSD: scp.c,v 1.68 2001/04/22 12:34:05 markus Exp $");

#include "xmalloc.h"
#include "atomicio.h"
#include "pathnames.h"
#include "log.h"
#include "scp-common.h"

/* For progressmeter() -- number of seconds before xfer considered "stalled" */
#define STALLTIME	5

/* Visual statistics about files as they are transferred. */
void progressmeter(int);

/* Returns width of the terminal (for progress meter calculations). */
int getttywidth(void);
int do_cmd(char *host, char *remuser, char *cmd, int *fdin, int *fdout, int argc);

/* setup arguments for the call to ssh */
void addargs(char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Time a transfer started. */
static struct timeval start;

/* Number of bytes of current file transferred so far. */
volatile off_t statbytes;

/* Total size of current file. */
off_t totalbytes = 0;

/* Name of current file being transferred. */
char *curfile;

/* This is set to non-zero to enable verbose mode. */
int verbose_mode = 0;

/* This is set to zero if the progressmeter is not desired. */
int showprogress = 1;

/* This is the program to execute for the secured connection. ("ssh" or -S) */
char *ssh_program = _PATH_SSH_PROGRAM;

/* This is the list of arguments that scp passes to ssh */
struct {
	char	**list;
	int	num;
	int	nalloc;
} args;

/*
 * This function executes the given command as the specified user on the
 * given host.  This returns < 0 if execution fails, and >= 0 otherwise. This
 * assigns the input and output file descriptors on success.
 */

int
do_cmd(char *host, char *remuser, char *cmd, int *fdin, int *fdout, int argc)
{
	int pin[2], pout[2], reserved[2];

	if (verbose_mode)
		fprintf(stderr, "Executing: program %s host %s, user %s, command %s\n",
		    ssh_program, host, remuser ? remuser : "(unspecified)", cmd);

	/*
	 * Reserve two descriptors so that the real pipes won't get
	 * descriptors 0 and 1 because that will screw up dup2 below.
	 */
	pipe(reserved);

	/* Create a socket pair for communicating with ssh. */
	if (pipe(pin) < 0)
		fatal("pipe: %s", strerror(errno));
	if (pipe(pout) < 0)
		fatal("pipe: %s", strerror(errno));

	/* Free the reserved descriptors. */
	close(reserved[0]);
	close(reserved[1]);

	/* For a child to execute the command on the remote host using ssh. */
	if (fork() == 0)  {
		/* Child. */
		close(pin[1]);
		close(pout[0]);
		dup2(pin[0], 0);
		dup2(pout[1], 1);
		close(pin[0]);
		close(pout[1]);

		args.list[0] = ssh_program;
		if (remuser != NULL)
			addargs("-l%s", remuser);
		addargs("%s", host);
		addargs("%s", cmd);

		execvp(ssh_program, args.list);
		perror(ssh_program);
		exit(1);
	}
	/* Parent.  Close the other side, and return the local side. */
	close(pin[0]);
	*fdout = pin[1];
	close(pout[1]);
	*fdin = pout[0];
	return 0;
}

typedef struct {
	int cnt;
	char *buf;
} BUF;

BUF *allocbuf(BUF *, int, int);
void lostconn(int);
void nospace(void);
int okname(char *);
void run_err(const char *,...);
void verifydir(char *);

struct passwd *pwd;
uid_t userid;
int errs, remin, remout;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

int response(void);
void rsource(char *, struct stat *);
void sink(int, char *[]);
void source(int, char *[]);
void tolocal(int, char *[]);
void toremote(char *, int, char *[]);
void usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, fflag, tflag;
	char *targ;
	extern char *optarg;
	extern int optind;

	args.list = NULL;
	addargs("ssh");	 	/* overwritten with ssh_program */
	addargs("-x");
	addargs("-oFallBackToRsh no");

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, "dfprtvBCc:i:P:q46S:o:")) != -1)
		switch (ch) {
		/* User-visible flags. */
		case '4':
		case '6':
		case 'C':
			addargs("-%c", ch);
			break;
		case 'o':
		case 'c':
		case 'i':
			addargs("-%c%s", ch, optarg);
			break;
		case 'P':
			addargs("-p%s", optarg);
			break;
		case 'B':
			addargs("-oBatchmode yes");
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			iamrecursive = 1;
			break;
		case 'S':
			ssh_program = xstrdup(optarg);
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 'q':
			showprogress = 0;
			break;

		/* Server options. */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':	/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':	/* "to" */
			iamremote = 1;
			tflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((pwd = getpwuid(userid = getuid())) == NULL)
		fatal("unknown user %d", (int) userid);

	if (!isatty(STDERR_FILENO))
		showprogress = 0;

	remin = STDIN_FILENO;
	remout = STDOUT_FILENO;

	if (fflag) {
		/* Follow "protocol", send data. */
		(void) response();
		source(argc, argv);
		exit(errs != 0);
	}
	if (tflag) {
		/* Receive data. */
		sink(argc, argv);
		exit(errs != 0);
	}
	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	remin = remout = -1;
	/* Command to be executed on remote system using "ssh". */
	(void) snprintf(cmd, sizeof cmd, "scp%s%s%s%s",
	    verbose_mode ? " -v" : "",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "",
	    targetshouldbedirectory ? " -d" : "");

	(void) signal(SIGPIPE, lostconn);

	if ((targ = colon(argv[argc - 1])))	/* Dest is remote host. */
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);	/* Dest is local host. */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs != 0);
}

void
toremote(targ, argc, argv)
	char *targ, *argv[];
	int argc;
{
	int i, len;
	char *bp, *host, *src, *suser, *thost, *tuser;

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if ((thost = strchr(argv[argc - 1], '@'))) {
		/* user@host */
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {	/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = strchr(argv[i], '@');
			len = strlen(ssh_program) + strlen(argv[i]) +
			    strlen(src) + (tuser ? strlen(tuser) : 0) +
			    strlen(thost) + strlen(targ) + CMDNEEDS + 32;
			bp = xmalloc(len);
			if (host) {
				*host++ = 0;
				host = cleanhostname(host);
				suser = argv[i];
				if (*suser == '\0')
					suser = pwd->pw_name;
				else if (!okname(suser))
					continue;
				snprintf(bp, len,
				    "%s%s -x -o'FallBackToRsh no' -n "
				    "-l %s %s %s %s '%s%s%s:%s'",
				    ssh_program, verbose_mode ? " -v" : "",
				    suser, host, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			} else {
				host = cleanhostname(argv[i]);
				snprintf(bp, len,
				    "exec %s%s -x -o'FallBackToRsh no' -n %s "
				    "%s %s '%s%s%s:%s'",
				    ssh_program, verbose_mode ? " -v" : "",
				    host, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			}
			if (verbose_mode)
				fprintf(stderr, "Executing: %s\n", bp);
			(void) system(bp);
			(void) xfree(bp);
		} else {	/* local to remote */
			if (remin == -1) {
				len = strlen(targ) + CMDNEEDS + 20;
				bp = xmalloc(len);
				(void) snprintf(bp, len, "%s -t %s", cmd, targ);
				host = cleanhostname(thost);
				if (do_cmd(host, tuser, bp, &remin,
				    &remout, argc) < 0)
					exit(1);
				if (response() < 0)
					exit(1);
				(void) xfree(bp);
			}
			source(1, argv + i);
		}
	}
}

void
tolocal(argc, argv)
	int argc;
	char *argv[];
{
	int i, len;
	char *bp, *host, *src, *suser;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {	/* Local to local. */
			len = strlen(_PATH_CP) + strlen(argv[i]) +
			    strlen(argv[argc - 1]) + 20;
			bp = xmalloc(len);
			(void) snprintf(bp, len, "exec %s%s%s %s %s", _PATH_CP,
			    iamrecursive ? " -r" : "", pflag ? " -p" : "",
			    argv[i], argv[argc - 1]);
			if (verbose_mode)
				fprintf(stderr, "Executing: %s\n", bp);
			if (system(bp))
				++errs;
			(void) xfree(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strchr(argv[i], '@')) == NULL) {
			host = argv[i];
			suser = NULL;
		} else {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwd->pw_name;
			else if (!okname(suser))
				continue;
		}
		host = cleanhostname(host);
		len = strlen(src) + CMDNEEDS + 20;
		bp = xmalloc(len);
		(void) snprintf(bp, len, "%s -f %s", cmd, src);
		if (do_cmd(host, suser, bp, &remin, &remout, argc) < 0) {
			(void) xfree(bp);
			++errs;
			continue;
		}
		xfree(bp);
		sink(1, argv + argc - 1);
		(void) close(remin);
		remin = remout = -1;
	}
}

void
source(argc, argv)
	int argc;
	char *argv[];
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i, amt, result;
	int fd, haderr, indx;
	char *last, *name, buf[2048];
	int len;

	for (indx = 0; indx < argc; ++indx) {
		name = argv[indx];
		statbytes = 0;
		len = strlen(name);
		while (len > 1 && name[len-1] == '/')
			name[--len] = '\0';
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;
		if (fstat(fd, &stb) < 0) {
syserr:			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
		switch (stb.st_mode & S_IFMT) {
		case S_IFREG:
			break;
		case S_IFDIR:
			if (iamrecursive) {
				rsource(name, &stb);
				goto next;
			}
			/* FALLTHROUGH */
		default:
			run_err("%s: not a regular file", name);
			goto next;
		}
		if ((last = strrchr(name, '/')) == NULL)
			last = name;
		else
			++last;
		curfile = last;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void) snprintf(buf, sizeof buf, "T%lu 0 %lu 0\n",
			    (u_long) stb.st_mtime,
			    (u_long) stb.st_atime);
			(void) atomicio(write, remout, buf, strlen(buf));
			if (response() < 0)
				goto next;
		}
#define	FILEMODEMASK	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)
		snprintf(buf, sizeof buf, "C%04o %lld %s\n",
		    (u_int) (stb.st_mode & FILEMODEMASK),
		    (long long)stb.st_size, last);
		if (verbose_mode) {
			fprintf(stderr, "Sending file modes: %s", buf);
			fflush(stderr);
		}
		(void) atomicio(write, remout, buf, strlen(buf));
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer, fd, 2048)) == NULL) {
next:			(void) close(fd);
			continue;
		}
		if (showprogress) {
			totalbytes = stb.st_size;
			progressmeter(-1);
		}
		/* Keep writing after an error so that we stay sync'd up. */
		for (haderr = i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!haderr) {
				result = atomicio(read, fd, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
			if (haderr)
				(void) atomicio(write, remout, bp->buf, amt);
			else {
				result = atomicio(write, remout, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
				statbytes += result;
			}
		}
		if (showprogress)
			progressmeter(1);

		if (close(fd) < 0 && !haderr)
			haderr = errno;
		if (!haderr)
			(void) atomicio(write, remout, "", 1);
		else
			run_err("%s: %s", name, strerror(haderr));
		(void) response();
	}
}

void
rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[1100];

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void) snprintf(path, sizeof(path), "T%lu 0 %lu 0\n",
		    (u_long) statp->st_mtime,
		    (u_long) statp->st_atime);
		(void) atomicio(write, remout, path, strlen(path));
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	(void) snprintf(path, sizeof path, "D%04o %d %.1024s\n",
	    (u_int) (statp->st_mode & FILEMODEMASK), 0, last);
	if (verbose_mode)
		fprintf(stderr, "Entering directory: %s", path);
	(void) atomicio(write, remout, path, strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= sizeof(path) - 1) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		(void) snprintf(path, sizeof path, "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	(void) closedir(dirp);
	(void) atomicio(write, remout, "E\n", 2);
	(void) response();
}

void
sink(argc, argv)
	int argc;
	char *argv[];
{
	static BUF buffer;
	struct stat stb;
	enum {
		YES, NO, DISPLAYED
	} wrerr;
	BUF *bp;
	off_t i, j;
	int amt, count, exists, first, mask, mode, ofd, omode;
	off_t size;
	int setimes, targisdir, wrerrno = 0;
	char ch, *cp, *np, *targ, *why, *vect[1], buf[2048];
	struct timeval tv[2];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void) umask(mask);
	if (argc != 1) {
		run_err("ambiguous target");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);

	(void) atomicio(write, remout, "", 1);
	if (stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (atomicio(read, remin, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (atomicio(read, remin, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[sizeof(buf) - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void) atomicio(write, STDERR_FILENO,
				    buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
			(void) atomicio(write, remout, "", 1);
			return;
		}
		if (ch == '\n')
			*--cp = 0;

		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			mtime.tv_sec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			mtime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			atime.tv_sec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			atime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void) atomicio(write, remout, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				run_err("%s", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");

		for (size = 0; isdigit(*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (namebuf)
					xfree(namebuf);
				namebuf = xmalloc(need);
				cursize = need;
			}
			(void) snprintf(namebuf, need, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		curfile = cp;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void) chmod(np, mode);
			} else {
				/* Handle copying from a read-only
				   directory */
				mod_flag = 1;
				if (mkdir(np, mode | S_IRWXU) < 0)
					goto bad;
			}
			vect[0] = xstrdup(np);
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(vect[0], tv) < 0)
					run_err("%s: set times: %s",
					    vect[0], strerror(errno));
			}
			if (mod_flag)
				(void) chmod(vect[0], mode);
			if (vect[0])
				xfree(vect[0]);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;
		if ((ofd = open(np, O_WRONLY | O_CREAT | O_TRUNC, mode)) < 0) {
bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}
		(void) atomicio(write, remout, "", 1);
		if ((bp = allocbuf(&buffer, ofd, 4096)) == NULL) {
			(void) close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;

		if (showprogress) {
			totalbytes = size;
			progressmeter(-1);
		}
		statbytes = 0;
		for (count = i = 0; i < size; i += 4096) {
			amt = 4096;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(remin, cp, amt);
				if (j == -1 && (errno == EINTR || errno == EAGAIN)) {
					continue;
				} else if (j <= 0) {
					run_err("%s", j ? strerror(errno) :
					    "dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
				statbytes += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					j = atomicio(write, ofd, bp->buf, count);
					if (j != count) {
						wrerr = YES;
						wrerrno = j >= 0 ? EIO : errno;
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (showprogress)
			progressmeter(1);
		if (count != 0 && wrerr == NO &&
		    (j = atomicio(write, ofd, bp->buf, count)) != count) {
			wrerr = YES;
			wrerrno = j >= 0 ? EIO : errno;
		}
#if 0
		if (ftruncate(ofd, size)) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
#endif
		if (pflag) {
			if (exists || omode != mode)
				if (fchmod(ofd, omode))
					run_err("%s: set mode: %s",
					    np, strerror(errno));
		} else {
			if (!exists && omode != mode)
				if (fchmod(ofd, omode & ~mask))
					run_err("%s: set mode: %s",
					    np, strerror(errno));
		}
		if (close(ofd) == -1) {
			wrerr = YES;
			wrerrno = errno;
		}
		(void) response();
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				run_err("%s: set times: %s",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch (wrerr) {
		case YES:
			run_err("%s: %s", np, strerror(wrerrno));
			break;
		case NO:
			(void) atomicio(write, remout, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
	exit(1);
}

int
response()
{
	char ch, *cp, resp, rbuf[2048];

	if (atomicio(read, remin, &resp, sizeof(resp)) != sizeof(resp))
		lostconn(0);

	cp = rbuf;
	switch (resp) {
	case 0:		/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:		/* error, followed by error msg */
	case 2:		/* fatal error, "" */
		do {
			if (atomicio(read, remin, &ch, sizeof(ch)) != sizeof(ch))
				lostconn(0);
			*cp++ = ch;
		} while (cp < &rbuf[sizeof(rbuf) - 1] && ch != '\n');

		if (!iamremote)
			(void) atomicio(write, STDERR_FILENO, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/* NOTREACHED */
}

void
usage()
{
	(void) fprintf(stderr, "usage: scp "
	    "[-pqrvBC46] [-S ssh] [-P port] [-c cipher] [-i identity] f1 f2\n"
	    "   or: scp [options] f1 ... fn directory\n");
	exit(1);
}

void
run_err(const char *fmt,...)
{
	static FILE *fp;
	va_list ap;
	va_start(ap, fmt);

	++errs;
	if (fp == NULL && !(fp = fdopen(remout, "w")))
		return;
	(void) fprintf(fp, "%c", 0x01);
	(void) fprintf(fp, "scp: ");
	(void) vfprintf(fp, fmt, ap);
	(void) fprintf(fp, "\n");
	(void) fflush(fp);

	if (!iamremote) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
verifydir(cp)
	char *cp;
{
	struct stat stb;

	if (!stat(cp, &stb)) {
		if (S_ISDIR(stb.st_mode))
			return;
		errno = ENOTDIR;
	}
	run_err("%s: %s", cp, strerror(errno));
	exit(1);
}

int
okname(cp0)
	char *cp0;
{
	int c;
	char *cp;

	cp = cp0;
	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) &&
		    c != '_' && c != '-' && c != '.' && c != '+')
			goto bad;
	} while (*++cp);
	return (1);

bad:	fprintf(stderr, "%s: invalid user name\n", cp0);
	return (0);
}

BUF *
allocbuf(bp, fd, blksize)
	BUF *bp;
	int fd, blksize;
{
	size_t size;
	struct stat stb;

	if (fstat(fd, &stb) < 0) {
		run_err("fstat: %s", strerror(errno));
		return (0);
	}
	if (stb.st_blksize == 0)
		size = blksize;
	else
		size = blksize + (stb.st_blksize - blksize % stb.st_blksize) %
		    stb.st_blksize;
	if (bp->cnt >= size)
		return (bp);
	if (bp->buf == NULL)
		bp->buf = xmalloc(size);
	else
		bp->buf = xrealloc(bp->buf, size);
	bp->cnt = size;
	return (bp);
}

void
lostconn(signo)
	int signo;
{
	if (!iamremote)
		fprintf(stderr, "lost connection\n");
	exit(1);
}


void
alarmtimer(int wait)
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

void
updateprogressmeter(int ignore)
{
	int save_errno = errno;

	progressmeter(0);
	errno = save_errno;
}

int
foregroundproc(void)
{
	static pid_t pgrp = -1;
	int ctty_pgrp;

	if (pgrp == -1)
		pgrp = getpgrp();

	return ((ioctl(STDOUT_FILENO, TIOCGPGRP, &ctty_pgrp) != -1 &&
		 ctty_pgrp == pgrp));
}

void
progressmeter(int flag)
{
	static const char prefixes[] = " KMGTP";
	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, remaining;
	char buf[256];

	if (flag == -1) {
		(void) gettimeofday(&start, (struct timezone *) 0);
		lastupdate = start;
		lastsize = 0;
	}
	if (foregroundproc() == 0)
		return;

	(void) gettimeofday(&now, (struct timezone *) 0);
	cursize = statbytes;
	if (totalbytes != 0) {
		ratio = 100.0 * cursize / totalbytes;
		ratio = MAX(ratio, 0);
		ratio = MIN(ratio, 100);
	} else
		ratio = 100;

	snprintf(buf, sizeof(buf), "\r%-20.20s %3d%% ", curfile, ratio);

	barlength = getttywidth() - 51;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "|%.*s%*s|", i,
		    "*****************************************************************************"
		    "*****************************************************************************",
		    barlength - i, "");
	}
	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %5llu %c%c ",
	    (unsigned long long) abbrevsize, prefixes[i],
	    prefixes[i] == ' ' ? ' ' : 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (flag != 1 &&
	    (statbytes <= 0 || elapsed <= 0.0 || cursize > totalbytes)) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " - stalled -");
	} else {
		if (flag != 1)
			remaining = (int)(totalbytes / (statbytes / elapsed) -
			    elapsed);
		else
			remaining = elapsed;

		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%02d:%02d%s", i / 60, i % 60,
		    (flag != 1) ? " ETA" : "    ");
	}
	atomicio(write, fileno(stdout), buf, strlen(buf));

	if (flag == -1) {
		signal(SIGALRM, updateprogressmeter);
		alarmtimer(1);
	} else if (flag == 1) {
		alarmtimer(0);
		atomicio(write, fileno(stdout), "\n", 1);
		statbytes = 0;
	}
}

int
getttywidth(void)
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		return (winsize.ws_col ? winsize.ws_col : 80);
	else
		return (80);
}

void
addargs(char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (args.list == NULL) {
		args.nalloc = 32;
		args.num = 0;
		args.list = xmalloc(args.nalloc * sizeof(char *));
	} else if (args.num+2 >= args.nalloc) {
		args.nalloc *= 2;
		args.list = xrealloc(args.list, args.nalloc * sizeof(char *));
	}
	args.list[args.num++] = xstrdup(buf);
	args.list[args.num] = NULL;
}
