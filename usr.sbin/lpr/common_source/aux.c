/*
 * Copyright (c) 1995
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

/*
 * Auxillary functions to aid portability to other systems.
 * These are 4.4BSD routines that are often not found on other systems.
 *
 * !!!USE THIS FILE ONLY IF YOU ARE NOT RUNNING 4.4BSD!!!
 */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef NO_SNPRINTF
#if __STDC__
snprintf(char *str, size_t n, const char *fmt, ...)
#else
snprintf(str, n, fmt, va_alist)
	char *str;
	size_t n;
	char *fmt;
	va_dcl
#endif
{
	int ret;
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = vsprintf(str, fmt, ap);
	va_end(ap);
	if (strlen(str) > n)
		fatal("memory corrupted");
	return (ret);
}

vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	char *fmt;
	va_list ap;
{
	int ret;

	ret = vsprintf(str, fmt, ap);
	if (strlen(str) > n)
		fatal("memory corrupted");
	return (ret);
}
#endif

#ifdef NO_STRERROR
char *
strerror(num)
	int num;
{
	extern int sys_nerr;
	extern char *sys_errlist[];
#define	UPREFIX	"Unknown error: "
	static char ebuf[40] = UPREFIX;		/* 64-bit number + slop */
	register unsigned int errnum;
	register char *p, *t;
	char tmp[40];

	errnum = num;				/* convert to unsigned */
	if (errnum < sys_nerr)
		return(sys_errlist[errnum]);

	/* Do this by hand, so we don't include stdio(3). */
	t = tmp;
	do {
		*t++ = "0123456789"[errnum % 10];
	} while (errnum /= 10);
	for (p = ebuf + sizeof(UPREFIX) - 1;;) {
		*p++ = *--t;
		if (t <= tmp)
			break;
	}
	return(ebuf);
}
#endif

#ifdef NO_STRDUP
char *
strdup(str)
	char *str;
{
	int n;
	char *sp;

	n = strlen(str) + 1;
	if (sp = (char *) malloc(n))
		memcpy(sp, str, n);
	return (sp);
}
#endif

#ifdef NO_DAEMON
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <sgtty.h>
#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

int
daemon(nochdir, noclose)
	int nochdir, noclose;
{
	int fd;

	switch (fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		_exit(0);
	}

	if (setsid() == -1)
		return (-1);

	if (!nochdir)
		(void)chdir("/");

	if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close (fd);
	}
	return (0);
}
#endif


#ifdef NO_SETSID
int
setsid()
{
	int f;

	f = open("/dev/tty", O_RDWR);
	if (f > 0) {
		ioctl(f, TIOCNOTTY, 0);
		(void) close(f);
	}
	return f;
}
#endif


#ifdef NO_VSYSLOG
#include <stdio.h>
#include <errno.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

vsyslog(pri, fmt, ap)
	int pri;
	const char *fmt;
	va_list ap;
{
	char buf[2048], fmt_cpy[1024];

	/* substitute error message for %m */
	{
		register char ch, *t1, *t2;
		char *strerror();

		for (t1 = fmt_cpy; ch = *fmt; ++fmt)
			if (ch == '%' && fmt[1] == 'm') {
				++fmt;
				for (t2 = strerror(errno);
				    *t1 = *t2++; ++t1);
			}
			else
				*t1++ = ch;
		*t1 = '\0';
	}
	vsprintf(buf, fmt_cpy, ap);
	syslog(pri, "%s", buf);
}
#endif


#ifdef NO_IVALIDUSER
#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/param.h>
#include "pathnames.h"

/*
 * Returns 0 if ok, -1 if not ok.
 */
int
__ivaliduser(hostf, raddr, luser, ruser)
	FILE *hostf;
	struct in_addr raddr;
	const char *luser, *ruser;
{
	register char *user, *p;
	int ch;
	char buf[MAXHOSTNAMELEN + 128];		/* host + login */

	while (fgets(buf, sizeof(buf), hostf)) {
		p = buf;
		/* Skip lines that are too long. */
		if (strchr(p, '\n') == NULL) {
			while ((ch = getc(hostf)) != '\n' && ch != EOF);
			continue;
		}
		while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0') {
			*p = isupper(*p) ? tolower(*p) : *p;
			p++;
		}
		if (*p == ' ' || *p == '\t') {
			*p++ = '\0';
			while (*p == ' ' || *p == '\t')
				p++;
			user = p;
			while (*p != '\n' && *p != ' ' &&
			    *p != '\t' && *p != '\0')
				p++;
		} else
			user = p;
		*p = '\0';
		if (__icheckhost(raddr, buf) &&
		    strcmp(ruser, *user ? user : luser) == 0) {
			return (0);
		}
	}
	return (-1);
}

/*
 * Returns "true" if match, 0 if no match.
 */
__icheckhost(raddr, lhost)
	struct in_addr raddr;
	register char *lhost;
{
	register struct hostent *hp;
	struct in_addr laddr;
	register char **pp;

	/* Try for raw ip address first. */
	if (isdigit(*lhost) && (laddr.s_addr = inet_addr(lhost)) != INADDR_NONE)
		return (raddr.s_addr == laddr.s_addr);

	/* Better be a hostname. */
	if ((hp = gethostbyname(lhost)) == NULL)
		return (0);

	/* Spin through ip addresses. */
	for (pp = hp->h_addr_list; *pp; ++pp)
		if (!bcmp(&raddr, *pp, sizeof(struct in_addr)))
			return (1);

	/* No match. */
	return (0);
}
#endif /* NO_IVALIDUSER */


#ifdef	NO_STATFS
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <ufs/fs.h>

/*
 * Check to see if there is enough space on the disk for size bytes.
 * 1 == OK, 0 == Not OK.
 */
static int
chksize(size)
	int size;
{
	struct stat stb;
	int spacefree;
	struct fs fs;
	static int dfd;
	static char *find_dev();

#ifndef SBOFF
#define SBOFF ((off_t)(BBSIZE))
#endif
	if (dfd <= 0) {
		char *ddev;

		if (stat(".", &stb) < 0) {
			syslog(LOG_ERR, "%s: %m", "statfs(\".\")");
			return (1);
		}
		ddev = find_dev(stb.st_dev, S_IFBLK);
		if ((dfd = open(ddev, O_RDONLY)) < 0) {
			syslog(LOG_WARNING, "%s: %s: %m", printer, ddev);
			return (1);
		}
	}
	if (lseek(dfd, (off_t)(SBOFF), 0) < 0)
		return(1);
	if (read(dfd, (char *)&fs, sizeof fs) != sizeof fs
	    || fs.fs_magic != FS_MAGIC) {
		syslog(LOG_ERR, "Can't calculate free space on spool device");
		return(1);
	}
	spacefree = freespace(&fs, fs.fs_minfree) * fs.fs_fsize / 512;
	size = (size + 511) / 512;
	if (minfree + size > spacefree)
		return(0);
	return(1);
}

static char *
find_dev(dev, type)
	register dev_t dev;
	register int type;
{
	register DIR *dfd;
	struct direct *dir;
	struct stat stb;
	char devname[MAXNAMLEN+6];
	char *dp;
	int n;

	strcpy(devname, "/dev/dsk");
	if ((dfd = opendir(devname)) == NULL) {
		strcpy(devname, "/dev");
		dfd = opendir(devname);
	}
	strcat(devname, "/");
	n = strlen(devname);

	while ((dir = readdir(dfd))) {
		strcpy(devname + n, dir->d_name);
		if (stat(devname, &stb))
			continue;
		if ((stb.st_mode & S_IFMT) != type)
			continue;
		if (dev == stb.st_rdev) {
			closedir(dfd);
			dp = (char *)malloc(strlen(devname)+1);
			strcpy(dp, devname);
			return(dp);
		}
	}
	closedir(dfd);
	frecverr("cannot find device %d, %d", major(dev), minor(dev));
	/*NOTREACHED*/
}
#endif	/* NOSTATFS */
