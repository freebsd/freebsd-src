/*
 * Copyright (c) 1983, 1990, 1992, 1993
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

#include "bsd_locl.h"

RCSID("$Id: rcp.c,v 1.52.2.1 2000/06/23 02:35:16 assar Exp $");

/* Globals */
static char	dst_realm_buf[REALM_SZ];
static char	*dest_realm = NULL;
static int	use_kerberos = 1;

static int	doencrypt = 0;
#define	OPTIONS	"dfKk:prtxl:"

static char *user_name = NULL;	/* Given as -l option. */

static int errs, rem;
static struct passwd *pwd;
static u_short	port;
static uid_t	userid;
static int pflag, iamremote, iamrecursive, targetshouldbedirectory;

static int argc_copy;
static char **argv_copy;

#define	CMDNEEDS	64
static char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

void rsource(char *name, struct stat *statp);

#define	SERVICE_NAME	"rcmd"

CREDENTIALS cred;
MSG_DAT msg_data;
struct sockaddr_in foreign_addr, local_addr;
Key_schedule schedule;

KTEXT_ST ticket;
AUTH_DAT kdata;

static void
send_auth(char *h, char *r)
{
    int lslen, fslen, status;
    long opts;

    lslen = sizeof(struct sockaddr_in);
    if (getsockname(rem, (struct sockaddr *)&local_addr, &lslen) < 0)
	err(1, "getsockname");
    fslen = sizeof(struct sockaddr_in);
    if (getpeername(rem, (struct sockaddr *)&foreign_addr, &fslen) < 0)
	err(1, "getpeername");
    if ((r == NULL) || (*r == '\0'))
	r = krb_realmofhost(h);
    opts = KOPT_DO_MUTUAL;
    if ((status = krb_sendauth(opts, rem, &ticket, SERVICE_NAME, h, r, 
			       (unsigned long)getpid(), &msg_data, &cred, 
			       schedule, &local_addr, 
			       &foreign_addr, "KCMDV0.1")) != KSUCCESS)
	errx(1, "krb_sendauth failure: %s", krb_get_err_text(status));
}

static void
answer_auth(void)
{
    int lslen, fslen, status;
    long opts;
    char inst[INST_SZ], v[9];

    lslen = sizeof(struct sockaddr_in);
    if (getsockname(rem, (struct sockaddr *)&local_addr, &lslen) < 0)
	err(1, "getsockname");
    fslen = sizeof(struct sockaddr_in);
    if(getpeername(rem, (struct sockaddr *)&foreign_addr, &fslen) < 0)
	    err(1, "getperrname");
    k_getsockinst(rem, inst, sizeof(inst));
    opts = KOPT_DO_MUTUAL;
    if ((status = krb_recvauth(opts, rem, &ticket, SERVICE_NAME, inst,
			       &foreign_addr, &local_addr, 
			       &kdata, "", schedule, v)) != KSUCCESS)
	errx(1, "krb_recvauth failure: %s", krb_get_err_text(status));
}

static int
des_read(int fd, char *buf, int len)
{
    if (doencrypt)
	return(des_enc_read(fd, buf, len, schedule, 
			    (iamremote? &kdata.session : &cred.session)));
    else
	return(read(fd, buf, len));
}

static int
des_write(int fd, char *buf, int len)
{
    if (doencrypt)
	return(des_enc_write(fd, buf, len, schedule, 
			     (iamremote? &kdata.session : &cred.session)));
    else
	return(write(fd, buf, len));
}

static void run_err(const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;


static void
run_err(const char *fmt, ...)
{
	char errbuf[1024];

	va_list args;
	va_start(args, fmt);
	++errs;
#define RCPERR "\001rcp: "
	strlcpy (errbuf, RCPERR, sizeof(errbuf));
	vsnprintf (errbuf + strlen(errbuf),
		   sizeof(errbuf) - strlen(errbuf),
		   fmt, args);
	strlcat (errbuf, "\n", sizeof(errbuf));
	des_write (rem, errbuf, strlen(errbuf));
	if (!iamremote)
		vwarnx(fmt, args);
	va_end(args);
}

static void
verifydir(char *cp)
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

#define ROUNDUP(x, y)   ((((x)+((y)-1))/(y))*(y))

static BUF *
allocbuf(BUF *bp, int fd, int blksize)
{
	struct stat stb;
	size_t size;

	if (fstat(fd, &stb) < 0) {
		run_err("fstat: %s", strerror(errno));
		return (0);
	}
#ifdef HAVE_ST_BLKSIZE
	size = ROUNDUP(stb.st_blksize, blksize);
#else
	size = blksize;
#endif
	if (size == 0)
		size = blksize;
	if (bp->cnt >= size)
		return (bp);
	if (bp->buf == NULL)
		bp->buf = malloc(size);
	else
		bp->buf = realloc(bp->buf, size);
	if (bp->buf == NULL) {
		bp->cnt = 0;
		run_err("%s", strerror(errno));
		return (0);
	}
	bp->cnt = size;
	return (bp);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n\t%s\n",
	    "usage: rcp [-Kpx] [-k realm] f1 f2",
	    "or: rcp [-Kprx] [-k realm] f1 ... fn directory");
	exit(1);
}

static void
oldw(const char *s)
{
    char *rstar_no_warn = getenv("RSTAR_NO_WARN");
    if (rstar_no_warn == 0)
	rstar_no_warn = "";
    if (strncmp(rstar_no_warn, "yes", 3) != 0)
	warnx("%s, using standard rcp", s);
}

static RETSIGTYPE
lostconn(int signo)
{
	if (!iamremote)
		warnx("lost connection");
	exit(1);
}

static int
response(void)
{
	char ch, *cp, resp, rbuf[BUFSIZ];

	if (des_read(rem, &resp, sizeof(resp)) != sizeof(resp))
		lostconn(0);

	cp = rbuf;
	switch(resp) {
	case 0:				/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by error msg */
	case 2:				/* fatal error, "" */
		do {
			if (des_read(rem, &ch, sizeof(ch)) != sizeof(ch))
				lostconn(0);
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			write(STDERR_FILENO, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/* NOTREACHED */
}

static void
source(int argc, char **argv)
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i;
	int amt, fd, haderr, indx, result;
	char *last, *name, buf[BUFSIZ];

	for (indx = 0; indx < argc; ++indx) {
                name = argv[indx];
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;
		if (fstat(fd, &stb)) {
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
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			snprintf(buf, sizeof(buf), "T%ld 0 %ld 0\n",
			    (long)stb.st_mtime, (long)stb.st_atime);
			des_write(rem, buf, strlen(buf));
			if (response() < 0)
				goto next;
		}
		snprintf(buf, sizeof(buf), "C%04o %ld %s\n",
		    (int)stb.st_mode & MODEMASK, (long) stb.st_size, last);
		des_write(rem, buf, strlen(buf));
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer, fd, BUFSIZ)) == NULL) {
next:			close(fd);
			continue;
		}

		/* Keep writing after an error so that we stay sync'd up. */
		for (haderr = i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!haderr) {
				result = read(fd, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
			if (haderr)
				des_write(rem, bp->buf, amt);
			else {
				result = des_write(rem, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
		}
		if (close(fd) && !haderr)
			haderr = errno;
		if (!haderr)
			des_write(rem, "", 1);
		else
			run_err("%s: %s", name, strerror(haderr));
		response();
	}
}

void
rsource(char *name, struct stat *statp)
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[MaxPathLen];
	char *p;

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	for (p = name + strlen(name) - 1; p >= name && *p == '/'; --p)
	    *p = '\0';

	last = strrchr(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		snprintf(path, sizeof(path), "T%ld 0 %ld 0\n",
		    (long)statp->st_mtime, (long)statp->st_atime);
		des_write(rem, path, strlen(path));
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	snprintf(path, sizeof(path),
		 "D%04o %d %s\n", (int)statp->st_mode & MODEMASK, 0, last);
	des_write(rem, path, strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while ((dp = readdir(dirp))) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= MaxPathLen - 1) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		if (snprintf(path, sizeof(path),
			     "%s/%s", name, dp->d_name) >= sizeof(path)) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		vect[0] = path;
		source(1, vect);
	}
	closedir(dirp);
	des_write(rem, "E\n", 2);
	response();
}

static int
kerberos(char **host, char *bp, char *locuser, char *user)
{
        int sock = -1, err;

	if (use_kerberos) {
	        paranoid_setuid(getuid());
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(*host);

#if 0
		rem = krcmd(host, port, user, bp, 0, dest_realm);
#else
		err = kcmd(
		    &sock,
		    host,
		    port,
		    NULL,	/* locuser not used */
		    user,
		    bp,
		    0,
		    &ticket,
		    SERVICE_NAME,
		    dest_realm,
		    (CREDENTIALS *) NULL, /* credentials not used */
		    0,		/* key schedule not used */
		    (MSG_DAT *) NULL, /* MSG_DAT not used */
		    (struct sockaddr_in *) NULL, /* local addr not used */
		    (struct sockaddr_in *) NULL, /* foreign addr not used */
		    0L);	/* authopts */
		if (err > KSUCCESS && err < MAX_KRB_ERRORS) {
		    warnx("kcmd: %s", krb_get_err_text(err));
		    rem = -1;
		} else if (err < 0)
		    rem = -1;
		else
		    rem = sock;
#endif
		if (rem < 0) {
			if (errno == ECONNREFUSED)
			    oldw("remote host doesn't support Kerberos");
			else if (errno == ENOENT)
			    oldw("can't provide Kerberos authentication data");
			execv(_PATH_RCP, argv_copy);
		}
	} else {
		if (doencrypt)
			errx(1,
			   "the -x option requires Kerberos authentication");
		if (geteuid() != 0) {
		    errx(1, "not installed setuid root, "
			 "only root may use non kerberized rcp");
		}
		rem = rcmd(host, port, locuser, user, bp, 0);
	}
	return (rem);
}

static void
toremote(char *targ, int argc, char **argv)
{
	int i, len;
#ifdef IP_TOS
	int tos;
#endif
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
		tuser = user_name;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = strchr(argv[i], '@');
			if (host) {
			    *host++ = 0;
			    suser = argv[i];
			    if (*suser == '\0')
				suser = pwd->pw_name;
			    else if (!okname(suser))
				continue;
			    asprintf(&bp, "%s %s -l %s -n %s %s '%s%s%s:%s'",
				     _PATH_RSH, host, suser, cmd, src,
				     tuser ? tuser : "", tuser ? "@" : "",
				     thost, targ);
			} else
			    asprintf(&bp, "exec %s %s -n %s %s '%s%s%s:%s'",
				     _PATH_RSH, argv[i], cmd, src,
				     tuser ? tuser : "", tuser ? "@" : "",
				     thost, targ);
			if(bp == NULL)
			    errx(1, "out of memory");
			susystem(bp, userid);
			free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				len = strlen(targ) + CMDNEEDS + 20;
				if (!(bp = malloc(len)))
					err(1, " ");
				snprintf(bp, len, "%s -t %s", cmd, targ);
				host = thost;
				if (use_kerberos)
					rem = kerberos(&host, bp,
#ifdef __CYGWIN32__
						       tuser,
#else
					    pwd->pw_name,
#endif
					    tuser ? tuser : pwd->pw_name);
				else
					rem = rcmd(&host, port,
#ifdef __CYGWIN32__
						   tuser,
#else
						   pwd->pw_name,
#endif
					    tuser ? tuser : pwd->pw_name,
					    bp, 0);
				if (rem < 0)
					exit(1);
#if defined(IP_TOS) && defined(HAVE_SETSOCKOPT)
				tos = IPTOS_THROUGHPUT;
				if (setsockopt(rem, IPPROTO_IP, IP_TOS,
				    (void *)&tos, sizeof(int)) < 0)
					warn("TOS (ignored)");
#endif /* IP_TOS */
				if (doencrypt)
				    send_auth(host, dest_realm);
				if (response() < 0)
					exit(1);
				free(bp);
				paranoid_setuid(userid);
			}
			source(1, argv+i);
		}
	}
}

static void
sink(int argc, char **argv)
{
	static BUF buffer;
	struct stat stb;
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	BUF *bp;
	off_t i, j;
	int amt, count, exists, first, mask, mode, ofd, omode;
	int setimes, size, targisdir, wrerrno=0;
	char ch, *cp, *np, *targ, *why, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		umask(mask);
	if (argc != 1) {
		run_err("ambiguous target");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	des_write(rem, "", 1);
	if (stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (des_read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (des_read(rem, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				write(STDERR_FILENO,
				    buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
			des_write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define getnum(t)					\
		do {					\
		    (t) = 0;				\
		    while (isdigit((unsigned char)*cp))	\
			(t) = (t) * 10 + (*cp++ - '0');	\
		} while(0)

		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			des_write(rem, "", 1);
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

		for (size = 0; isdigit((unsigned char)*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc(need)))
					run_err("%s", strerror(errno));
			}
			snprintf(namebuf, need, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					chmod(np, mode);
			} else {
				/* Handle copying from a read-only directory */
				mod_flag = 1;
				if (mkdir(np, mode | S_IRWXU) < 0)
					goto bad;
			}
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
			        struct utimbuf times;
				times.actime = atime.tv_sec;
				times.modtime = mtime.tv_sec;
				setimes = 0;
				if (utime(np, &times) < 0)
				    run_err("%s: set times: %s",
					    np, strerror(errno));
			}
			if (mod_flag)
				chmod(np, mode);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}
		des_write(rem, "", 1);
		if ((bp = allocbuf(&buffer, ofd, BUFSIZ)) == NULL) {
			close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;
		for (count = i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = des_read(rem, cp, amt);
				if (j <= 0) {
					run_err("%s", j ? strerror(errno) :
						"dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					j = write(ofd, bp->buf, count);
					if (j != count) {
						wrerr = YES;
						wrerrno = j >= 0 ? EIO : errno; 
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == NO &&
		    (j = write(ofd, bp->buf, count)) != count) {
			wrerr = YES;
			wrerrno = j >= 0 ? EIO : errno; 
		}
		if (ftruncate(ofd, size)) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
#ifdef HAVE_FCHMOD
				if (fchmod(ofd, omode))
#else
				if (chmod(np, omode))
#endif
					run_err("%s: set mode: %s",
						np, strerror(errno));
		} else {
			if (!exists && omode != mode)
#ifdef HAVE_FCHMOD
				if (fchmod(ofd, omode & ~mask))
#else
				if (chmod(np, omode & ~mask))
#endif
					run_err("%s: set mode: %s",
						np, strerror(errno));
		}
		close(ofd);
		response();
		if (setimes && wrerr == NO) {
		        struct utimbuf times;
			times.actime = atime.tv_sec;
			times.modtime = mtime.tv_sec;
			setimes = 0;
			if (utime(np, &times) < 0) {
				run_err("%s: set times: %s",
					np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch(wrerr) {
		case YES:
			run_err("%s: %s", np, strerror(wrerrno));
			break;
		case NO:
			des_write(rem, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
	exit(1);
}

static void
tolocal(int argc, char **argv)
{
	int i, len;
#ifdef IP_TOS
	int tos;
#endif
	char *bp, *host, *src, *suser;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {		/* Local to local. */
			len = strlen(_PATH_CP) + strlen(argv[i]) +
			    strlen(argv[argc - 1]) + 20;
			if (!(bp = malloc(len)))
				err(1, " ");
			snprintf(bp, len, "exec %s%s%s %s %s", _PATH_CP,
				 iamrecursive ? " -r" : "", pflag ? " -p" : "",
				 argv[i], argv[argc - 1]);
			if (susystem(bp, userid))
				++errs;
			free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strchr(argv[i], '@')) == NULL) {
#ifdef __CYGWIN32__
			errx (1, "Sorry, you need to specify the username");
#else
			host = argv[i];
			suser = pwd->pw_name;
			if (user_name)
			    suser = user_name;
#endif
		} else {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
#ifdef __CYGWIN32__
			errx (1, "Sorry, you need to specify the username");
#else
				suser = pwd->pw_name;
#endif
			else if (!okname(suser))
				continue;
		}
		len = strlen(src) + CMDNEEDS + 20;
		if ((bp = malloc(len)) == NULL)
			err(1, " ");
		snprintf(bp, len, "%s -f %s", cmd, src);
		rem = 
		    use_kerberos ? 
			kerberos(&host, bp,
#ifndef __CYGWIN32__
				 pwd->pw_name,
#else
				 suser,
#endif
				 suser) : 
			rcmd(&host, port,
#ifndef __CYGWIN32__
			     pwd->pw_name,
#else
			     suser,
#endif
			     suser, bp, 0);
		free(bp);
		if (rem < 0) {
			++errs;
			continue;
		}
		seteuid(userid);
#if defined(IP_TOS) && defined(HAVE_SETSOCKOPT)
		tos = IPTOS_THROUGHPUT;
		if (setsockopt(rem, IPPROTO_IP, IP_TOS, (void *)&tos,
			       sizeof(int)) < 0)
			warn("TOS (ignored)");
#endif /* IP_TOS */
		if (doencrypt)
			send_auth(host, dest_realm);
		sink(1, argv + argc - 1);
		seteuid(0);
		close(rem);
		rem = -1;
	}
}


int
main(int argc, char **argv)
{
	int ch, fflag, tflag;
	char *targ;
	int i;

	set_progname(argv[0]);

	/*
	 * Prepare for execing ourselves.
	 */

	argc_copy = argc + 1;
	argv_copy = malloc((argc_copy + 1) * sizeof(*argv_copy));
	if (argv_copy == NULL)
	    err(1, "malloc");
	argv_copy[0] = argv[0];
	argv_copy[1] = "-K";
	for(i = 1; i < argc; ++i) {
	    argv_copy[i + 1] = strdup(argv[i]);
	    if (argv_copy[i + 1] == NULL)
		errx(1, "strdup: out of memory");
	}
	argv_copy[argc + 1] = NULL;


	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch(ch) {			/* User-visible flags. */
		case 'K':
			use_kerberos = 0;
			break;
		case 'k':
			dest_realm = dst_realm_buf;
			strlcpy(dst_realm_buf, optarg, REALM_SZ);
			break;
		case 'x':
			doencrypt = 1;
			LEFT_JUSTIFIED = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			iamrecursive = 1;
			break;
						/* Server options. */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':			/* "to" */
			iamremote = 1;
			tflag = 1;
			break;
		case 'l':
		        user_name = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Rcp implements encrypted file transfer without using the
	 * kshell service, pass 0 for no encryption */
	port = get_shell_port(use_kerberos, 0);

	userid = getuid();

#ifndef __CYGWIN32__
	if ((pwd = k_getpwuid(userid)) == NULL)
		errx(1, "unknown user %d", (int)userid);
#endif

	rem = STDIN_FILENO;		/* XXX */

	if (fflag || tflag) {
	    if (doencrypt)
		answer_auth();
	    if(fflag)
		response();
	    if(do_osfc2_magic(pwd->pw_uid))
		exit(1);
	    paranoid_setuid(userid);
	    if (k_hasafs()) {
		/* Sometimes we will need cell specific tokens
		 * to be able to read and write files, thus,
		 * the token stuff done in rshd might not
		 * suffice.
		 */
		char cell[64];
		if (k_afs_cell_of_file(pwd->pw_dir,
				       cell, sizeof(cell)) == 0)
		    krb_afslog(cell, 0);
		krb_afslog(0, 0);
	    }
	    if(fflag)
		source(argc, argv);
	    else
		sink(argc, argv);
	    exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	/* Command to be executed on remote system using "rsh". */
	snprintf(cmd, sizeof(cmd),
		 "rcp%s%s%s%s", iamrecursive ? " -r" : "",
		 (doencrypt && use_kerberos ? " -x" : ""),
		 pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");

	signal(SIGPIPE, lostconn);

	if ((targ = colon(argv[argc - 1])))	/* Dest is remote host. */
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);		/* Dest is local host. */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
}
