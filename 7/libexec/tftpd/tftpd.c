/*
 * Copyright (c) 1983, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tftpd.c	8.1 (Berkeley) 6/4/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton
 * <guyton@rand-unix>.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/tftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tftpsubs.h"

#define	TIMEOUT		5
#define	MAX_TIMEOUTS	5

int	peer;
int	rexmtval = TIMEOUT;
int	max_rexmtval = 2*TIMEOUT;

#define	PKTSIZE	SEGSIZE+4
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_storage from;

void	tftp(struct tftphdr *, int);
static void unmappedaddr(struct sockaddr_in6 *);

/*
 * Null-terminated directory prefix list for absolute pathname requests and
 * search list for relative pathname requests.
 *
 * MAXDIRS should be at least as large as the number of arguments that
 * inetd allows (currently 20).
 */
#define MAXDIRS	20
static struct dirlist {
	const char	*name;
	int	len;
} dirs[MAXDIRS+1];
static int	suppress_naks;
static int	logging;
static int	ipchroot;
static int	create_new = 0;
static mode_t	mask = S_IWGRP|S_IWOTH;

static const char *errtomsg(int);
static void  nak(int);
static void  oack(void);

static void  timer(int);
static void  justquit(int);

int
main(int argc, char *argv[])
{
	struct tftphdr *tp;
	socklen_t fromlen, len;
	int n;
	int ch, on;
	struct sockaddr_storage me;
	char *chroot_dir = NULL;
	struct passwd *nobody;
	const char *chuser = "nobody";

	tzset();			/* syslog in localtime */

	openlog("tftpd", LOG_PID | LOG_NDELAY, LOG_FTP);
	while ((ch = getopt(argc, argv, "cClns:u:U:w")) != -1) {
		switch (ch) {
		case 'c':
			ipchroot = 1;
			break;
		case 'C':
			ipchroot = 2;
			break;
		case 'l':
			logging = 1;
			break;
		case 'n':
			suppress_naks = 1;
			break;
		case 's':
			chroot_dir = optarg;
			break;
		case 'u':
			chuser = optarg;
			break;
		case 'U':
			mask = strtol(optarg, NULL, 0);
			break;
		case 'w':
			create_new = 1;
			break;
		default:
			syslog(LOG_WARNING, "ignoring unknown option -%c", ch);
		}
	}
	if (optind < argc) {
		struct dirlist *dirp;

		/* Get list of directory prefixes. Skip relative pathnames. */
		for (dirp = dirs; optind < argc && dirp < &dirs[MAXDIRS];
		     optind++) {
			if (argv[optind][0] == '/') {
				dirp->name = argv[optind];
				dirp->len  = strlen(dirp->name);
				dirp++;
			}
		}
	}
	else if (chroot_dir) {
		dirs->name = "/";
		dirs->len = 1;
	}
	if (ipchroot > 0 && chroot_dir == NULL) {
		syslog(LOG_ERR, "-c requires -s");
		exit(1);
	}

	umask(mask);

	on = 1;
	if (ioctl(0, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m");
		exit(1);
	}
	fromlen = sizeof (from);
	n = recvfrom(0, buf, sizeof (buf), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m");
		exit(1);
	}
	/*
	 * Now that we have read the message out of the UDP
	 * socket, we fork and exit.  Thus, inetd will go back
	 * to listening to the tftp port, and the next request
	 * to come in will start up a new instance of tftpd.
	 *
	 * We do this so that inetd can run tftpd in "wait" mode.
	 * The problem with tftpd running in "nowait" mode is that
	 * inetd may get one or more successful "selects" on the
	 * tftp port before we do our receive, so more than one
	 * instance of tftpd may be started up.  Worse, if tftpd
	 * break before doing the above "recvfrom", inetd would
	 * spawn endless instances, clogging the system.
	 */
	{
		int i, pid;

		for (i = 1; i < 20; i++) {
		    pid = fork();
		    if (pid < 0) {
				sleep(i);
				/*
				 * flush out to most recently sent request.
				 *
				 * This may drop some request, but those
				 * will be resent by the clients when
				 * they timeout.  The positive effect of
				 * this flush is to (try to) prevent more
				 * than one tftpd being started up to service
				 * a single request from a single client.
				 */
				fromlen = sizeof from;
				i = recvfrom(0, buf, sizeof (buf), 0,
				    (struct sockaddr *)&from, &fromlen);
				if (i > 0) {
					n = i;
				}
		    } else {
				break;
		    }
		}
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
	}

	/*
	 * Since we exit here, we should do that only after the above
	 * recvfrom to keep inetd from constantly forking should there
	 * be a problem.  See the above comment about system clogging.
	 */
	if (chroot_dir) {
		if (ipchroot > 0) {
			char *tempchroot;
			struct stat sb;
			int statret;
			struct sockaddr_storage ss;
			char hbuf[NI_MAXHOST];

			memcpy(&ss, &from, from.ss_len);
			unmappedaddr((struct sockaddr_in6 *)&ss);
			getnameinfo((struct sockaddr *)&ss, ss.ss_len,
				    hbuf, sizeof(hbuf), NULL, 0,
				    NI_NUMERICHOST);
			asprintf(&tempchroot, "%s/%s", chroot_dir, hbuf);
			if (ipchroot == 2)
				statret = stat(tempchroot, &sb);
			if (ipchroot == 1 ||
			    (statret == 0 && (sb.st_mode & S_IFDIR)))
				chroot_dir = tempchroot;
		}
		/* Must get this before chroot because /etc might go away */
		if ((nobody = getpwnam(chuser)) == NULL) {
			syslog(LOG_ERR, "%s: no such user", chuser);
			exit(1);
		}
		if (chroot(chroot_dir)) {
			syslog(LOG_ERR, "chroot: %s: %m", chroot_dir);
			exit(1);
		}
		chdir("/");
		setgroups(1, &nobody->pw_gid);
		setuid(nobody->pw_uid);
	}

	len = sizeof(me);
	if (getsockname(0, (struct sockaddr *)&me, &len) == 0) {
		switch (me.ss_family) {
		case AF_INET:
			((struct sockaddr_in *)&me)->sin_port = 0;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)&me)->sin6_port = 0;
			break;
		default:
			/* unsupported */
			break;
		}
	} else {
		memset(&me, 0, sizeof(me));
		me.ss_family = from.ss_family;
		me.ss_len = from.ss_len;
	}
	alarm(0);
	close(0);
	close(1);
	peer = socket(from.ss_family, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	if (bind(peer, (struct sockaddr *)&me, me.ss_len) < 0) {
		syslog(LOG_ERR, "bind: %m");
		exit(1);
	}
	if (connect(peer, (struct sockaddr *)&from, from.ss_len) < 0) {
		syslog(LOG_ERR, "connect: %m");
		exit(1);
	}
	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
		tftp(tp, n);
	exit(1);
}

static void
reduce_path(char *fn)
{
	char *slash, *ptr;

	/* Reduce all "/+./" to "/" (just in case we've got "/./../" later */
	while ((slash = strstr(fn, "/./")) != NULL) {
		for (ptr = slash; ptr > fn && ptr[-1] == '/'; ptr--)
			;
		slash += 2;
		while (*slash)
			*++ptr = *++slash;
	}

	/* Now reduce all "/something/+../" to "/" */
	while ((slash = strstr(fn, "/../")) != NULL) {
		if (slash == fn)
			break;
		for (ptr = slash; ptr > fn && ptr[-1] == '/'; ptr--)
			;
		for (ptr--; ptr >= fn; ptr--)
			if (*ptr == '/')
				break;
		if (ptr < fn)
			break;
		slash += 3;
		while (*slash)
			*++ptr = *++slash;
	}
}

struct formats;
int	validate_access(char **, int);
void	xmitfile(struct formats *);
void	recvfile(struct formats *);

struct formats {
	const char	*f_mode;
	int	(*f_validate)(char **, int);
	void	(*f_send)(struct formats *);
	void	(*f_recv)(struct formats *);
	int	f_convert;
} formats[] = {
	{ "netascii",	validate_access,	xmitfile,	recvfile, 1 },
	{ "octet",	validate_access,	xmitfile,	recvfile, 0 },
#ifdef notdef
	{ "mail",	validate_user,		sendmail,	recvmail, 1 },
#endif
	{ 0,		NULL,			NULL,		NULL,	  0 }
};

struct options {
	const char	*o_type;
	char	*o_request;
	int	o_reply;	/* turn into union if need be */
} options[] = {
	{ "tsize",	NULL, 0 },		/* OPT_TSIZE */
	{ "timeout",	NULL, 0 },		/* OPT_TIMEOUT */
	{ NULL,		NULL, 0 }
};

enum opt_enum {
	OPT_TSIZE = 0,
	OPT_TIMEOUT,
};

/*
 * Handle initial connection protocol.
 */
void
tftp(struct tftphdr *tp, int size)
{
	char *cp;
	int i, first = 1, has_options = 0, ecode;
	struct formats *pf;
	char *filename, *mode, *option, *ccp;
	char fnbuf[PATH_MAX];

	cp = tp->th_stuff;
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		nak(EBADOP);
		exit(1);
	}
	i = cp - tp->th_stuff;
	if (i >= sizeof(fnbuf)) {
		nak(EBADOP);
		exit(1);
	}
	memcpy(fnbuf, tp->th_stuff, i);
	fnbuf[i] = '\0';
	reduce_path(fnbuf);
	filename = fnbuf;
	if (first) {
		mode = ++cp;
		first = 0;
		goto again;
	}
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		nak(EBADOP);
		exit(1);
	}
	while (++cp < buf + size) {
		for (i = 2, ccp = cp; i > 0; ccp++) {
			if (ccp >= buf + size) {
				/*
				 * Don't reject the request, just stop trying
				 * to parse the option and get on with it.
				 * Some Apple Open Firmware versions have
				 * trailing garbage on the end of otherwise
				 * valid requests.
				 */
				goto option_fail;
			} else if (*ccp == '\0')
				i--;
		}
		for (option = cp; *cp; cp++)
			if (isupper(*cp))
				*cp = tolower(*cp);
		for (i = 0; options[i].o_type != NULL; i++)
			if (strcmp(option, options[i].o_type) == 0) {
				options[i].o_request = ++cp;
				has_options = 1;
			}
		cp = ccp-1;
	}

option_fail:
	if (options[OPT_TIMEOUT].o_request) {
		int to = atoi(options[OPT_TIMEOUT].o_request);
		if (to < 1 || to > 255) {
			nak(EBADOP);
			exit(1);
		}
		else if (to <= max_rexmtval)
			options[OPT_TIMEOUT].o_reply = rexmtval = to;
		else
			options[OPT_TIMEOUT].o_request = NULL;
	}

	ecode = (*pf->f_validate)(&filename, tp->th_opcode);
	if (has_options && ecode == 0)
		oack();
	if (logging) {
		char hbuf[NI_MAXHOST];

		getnameinfo((struct sockaddr *)&from, from.ss_len,
			    hbuf, sizeof(hbuf), NULL, 0, 0);
		syslog(LOG_INFO, "%s: %s request for %s: %s", hbuf,
			tp->th_opcode == WRQ ? "write" : "read",
			filename, errtomsg(ecode));
	}
	if (ecode) {
		/*
		 * Avoid storms of naks to a RRQ broadcast for a relative
		 * bootfile pathname from a diskless Sun.
		 */
		if (suppress_naks && *filename != '/' && ecode == ENOTFOUND)
			exit(0);
		nak(ecode);
		exit(1);
	}
	if (tp->th_opcode == WRQ)
		(*pf->f_recv)(pf);
	else
		(*pf->f_send)(pf);
	exit(0);
}


FILE *file;

/*
 * Validate file access.  Since we
 * have no uid or gid, for now require
 * file to exist and be publicly
 * readable/writable.
 * If we were invoked with arguments
 * from inetd then the file must also be
 * in one of the given directory prefixes.
 * Note also, full path name must be
 * given as we have no login directory.
 */
int
validate_access(char **filep, int mode)
{
	struct stat stbuf;
	int	fd;
	struct dirlist *dirp;
	static char pathname[MAXPATHLEN];
	char *filename = *filep;

	/*
	 * Prevent tricksters from getting around the directory restrictions
	 */
	if (strstr(filename, "/../"))
		return (EACCESS);

	if (*filename == '/') {
		/*
		 * Allow the request if it's in one of the approved locations.
		 * Special case: check the null prefix ("/") by looking
		 * for length = 1 and relying on the arg. processing that
		 * it's a /.
		 */
		for (dirp = dirs; dirp->name != NULL; dirp++) {
			if (dirp->len == 1 ||
			    (!strncmp(filename, dirp->name, dirp->len) &&
			     filename[dirp->len] == '/'))
				    break;
		}
		/* If directory list is empty, allow access to any file */
		if (dirp->name == NULL && dirp != dirs)
			return (EACCESS);
		if (stat(filename, &stbuf) < 0)
			return (errno == ENOENT ? ENOTFOUND : EACCESS);
		if ((stbuf.st_mode & S_IFMT) != S_IFREG)
			return (ENOTFOUND);
		if (mode == RRQ) {
			if ((stbuf.st_mode & S_IROTH) == 0)
				return (EACCESS);
		} else {
			if ((stbuf.st_mode & S_IWOTH) == 0)
				return (EACCESS);
		}
	} else {
		int err;

		/*
		 * Relative file name: search the approved locations for it.
		 * Don't allow write requests that avoid directory
		 * restrictions.
		 */

		if (!strncmp(filename, "../", 3))
			return (EACCESS);

		/*
		 * If the file exists in one of the directories and isn't
		 * readable, continue looking. However, change the error code
		 * to give an indication that the file exists.
		 */
		err = ENOTFOUND;
		for (dirp = dirs; dirp->name != NULL; dirp++) {
			snprintf(pathname, sizeof(pathname), "%s/%s",
				dirp->name, filename);
			if (stat(pathname, &stbuf) == 0 &&
			    (stbuf.st_mode & S_IFMT) == S_IFREG) {
				if ((stbuf.st_mode & S_IROTH) != 0) {
					break;
				}
				err = EACCESS;
			}
		}
		if (dirp->name != NULL)
			*filep = filename = pathname;
		else if (mode == RRQ)
			return (err);
	}
	if (options[OPT_TSIZE].o_request) {
		if (mode == RRQ) 
			options[OPT_TSIZE].o_reply = stbuf.st_size;
		else
			/* XXX Allows writes of all sizes. */
			options[OPT_TSIZE].o_reply =
				atoi(options[OPT_TSIZE].o_request);
	}
	if (mode == RRQ)
		fd = open(filename, O_RDONLY);
	else {
		if (create_new)
			fd = open(filename, O_WRONLY|O_TRUNC|O_CREAT, 0666);
		else
			fd = open(filename, O_WRONLY|O_TRUNC);
	}
	if (fd < 0)
		return (errno + 100);
	file = fdopen(fd, (mode == RRQ)? "r":"w");
	if (file == NULL) {
		close(fd);
		return (errno + 100);
	}
	return (0);
}

int	timeouts;
jmp_buf	timeoutbuf;

void
timer(int sig __unused)
{
	if (++timeouts > MAX_TIMEOUTS)
		exit(1);
	longjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
void
xmitfile(struct formats *pf)
{
	struct tftphdr *dp;
	struct tftphdr *ap;    /* ack packet */
	int size, n;
	volatile unsigned short block;

	signal(SIGALRM, timer);
	dp = r_init();
	ap = (struct tftphdr *)ackbuf;
	block = 1;
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons((u_short)block);
		timeouts = 0;
		(void)setjmp(timeoutbuf);

send_data:
		{
			int i, t = 1;
			for (i = 0; ; i++){
				if (send(peer, dp, size + 4, 0) != size + 4) {
					sleep(t);
					t = (t < 32) ? t<< 1 : t;
					if (i >= 12) {
						syslog(LOG_ERR, "write: %m");
						goto abort;
					}
				}
				break;
			}
		}
		read_ahead(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);        /* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "read: %m");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs((u_short)ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;

			if (ap->th_opcode == ACK) {
				if (ap->th_block == block)
					break;
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1))
					goto send_data;
			}

		}
		block++;
	} while (size == SEGSIZE);
abort:
	(void) fclose(file);
}

void
justquit(int sig __unused)
{
	exit(0);
}


/*
 * Receive a file.
 */
void
recvfile(struct formats *pf)
{
	struct tftphdr *dp;
	struct tftphdr *ap;    /* ack buffer */
	int n, size;
	volatile unsigned short block;

	signal(SIGALRM, timer);
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	block = 0;
	do {
		timeouts = 0;
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htons((u_short)block);
		block++;
		(void) setjmp(timeoutbuf);
send_ack:
		if (send(peer, ackbuf, 4, 0) != 4) {
			syslog(LOG_ERR, "write: %m");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) {            /* really? */
				syslog(LOG_ERR, "read: %m");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohs((u_short)dp->th_block);
			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == DATA) {
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
					goto send_ack;          /* rexmit */
			}
		}
		/*  size = write(file, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n-4)) {                    /* ahem */
			if (size < 0) nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (size == SEGSIZE);
	write_behind(file, pf->f_convert);
	(void) fclose(file);            /* close data file */

	ap->th_opcode = htons((u_short)ACK);    /* send the "final" ack */
	ap->th_block = htons((u_short)(block));
	(void) send(peer, ackbuf, 4, 0);

	signal(SIGALRM, justquit);      /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, sizeof (buf), 0); /* normally times out and quits */
	alarm(0);
	if (n >= 4 &&                   /* if read some data */
	    dp->th_opcode == DATA &&    /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		(void) send(peer, ackbuf, 4, 0);     /* resend final ack */
	}
abort:
	return;
}

struct errmsg {
	int	e_code;
	const char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ EOPTNEG,	"Option negotiation" },
	{ -1,		0 }
};

static const char *
errtomsg(int error)
{
	static char ebuf[20];
	struct errmsg *pe;
	if (error == 0)
		return "success";
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			return pe->e_msg;
	snprintf(ebuf, sizeof(buf), "error %d", error);
	return ebuf;
}

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(int error)
{
	struct tftphdr *tp;
	int length;
	struct errmsg *pe;

	tp = (struct tftphdr *)buf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	strcpy(tp->th_msg, pe->e_msg);
	length = strlen(pe->e_msg);
	tp->th_msg[length] = '\0';
	length += 5;
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m");
}

/* translate IPv4 mapped IPv6 address to IPv4 address */
static void
unmappedaddr(struct sockaddr_in6 *sin6)
{
	struct sockaddr_in *sin4;
	u_int32_t addr;
	int port;

	if (sin6->sin6_family != AF_INET6 ||
	    !IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;
	sin4 = (struct sockaddr_in *)sin6;
	addr = *(u_int32_t *)&sin6->sin6_addr.s6_addr[12];
	port = sin6->sin6_port;
	memset(sin4, 0, sizeof(struct sockaddr_in));
	sin4->sin_addr.s_addr = addr;
	sin4->sin_port = port;
	sin4->sin_family = AF_INET;
	sin4->sin_len = sizeof(struct sockaddr_in);
}

/*
 * Send an oack packet (option acknowledgement).
 */
static void
oack(void)
{
	struct tftphdr *tp, *ap;
	int size, i, n;
	char *bp;

	tp = (struct tftphdr *)buf;
	bp = buf + 2;
	size = sizeof(buf) - 2;
	tp->th_opcode = htons((u_short)OACK);
	for (i = 0; options[i].o_type != NULL; i++) {
		if (options[i].o_request) {
			n = snprintf(bp, size, "%s%c%d", options[i].o_type,
				     0, options[i].o_reply);
			bp += n+1;
			size -= n+1;
			if (size < 0) {
				syslog(LOG_ERR, "oack: buffer overflow");
				exit(1);
			}
		}
	}
	size = bp - buf;
	ap = (struct tftphdr *)ackbuf;
	signal(SIGALRM, timer);
	timeouts = 0;

	(void)setjmp(timeoutbuf);
	if (send(peer, buf, size, 0) != size) {
		syslog(LOG_INFO, "oack: %m");
		exit(1);
	}

	for (;;) {
		alarm(rexmtval);
		n = recv(peer, ackbuf, sizeof (ackbuf), 0);
		alarm(0);
		if (n < 0) {
			syslog(LOG_ERR, "recv: %m");
			exit(1);
		}
		ap->th_opcode = ntohs((u_short)ap->th_opcode);
		ap->th_block = ntohs((u_short)ap->th_block);
		if (ap->th_opcode == ERROR)
			exit(1);
		if (ap->th_opcode == ACK && ap->th_block == 0)
			break;
	}
}
