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
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tftpd.c	5.13 (Berkeley) 2/26/91";
#endif /* not lint */

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/tftp.h>
#include <netdb.h>

#include <setjmp.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define	TIMEOUT		5

extern	int errno;
struct	sockaddr_in s_in = { AF_INET };
int	peer;
int	rexmtval = TIMEOUT;
int	maxtimeout = 5*TIMEOUT;

#define	PKTSIZE	SEGSIZE+4
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_in from;
int	fromlen;

#define MAXARG	4
char	*dirs[MAXARG+1];

main(ac, av)
	char **av;
{
	register struct tftphdr *tp;
	register int n = 0;
	int on = 1;

	ac--; av++;
	while (ac-- > 0 && n < MAXARG)
		dirs[n++] = *av++;
	openlog("tftpd", LOG_PID, LOG_DAEMON);
	if (ioctl(0, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m\n");
		exit(1);
	}
	fromlen = sizeof (from);
	n = recvfrom(0, buf, sizeof (buf), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m\n");
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
		int pid;
		int i, j;

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
				j = sizeof from;
				i = recvfrom(0, buf, sizeof (buf), 0,
				    (struct sockaddr *)&from, &j);
				if (i > 0) {
					n = i;
					fromlen = j;
				}
		    } else {
				break;
		    }
		}
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m\n");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
	}
	from.sin_family = AF_INET;
	alarm(0);
	close(0);
	close(1);
	peer = socket(AF_INET, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m\n");
		exit(1);
	}
	if (bind(peer, (struct sockaddr *)&s_in, sizeof (s_in)) < 0) {
		syslog(LOG_ERR, "bind: %m\n");
		exit(1);
	}
	if (connect(peer, (struct sockaddr *)&from, sizeof(from)) < 0) {
		syslog(LOG_ERR, "connect: %m\n");
		exit(1);
	}
	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
		tftp(tp, n);
	exit(1);
}

int	validate_access();
int	sendfile(), recvfile();

struct formats {
	char	*f_mode;
	int	(*f_validate)();
	int	(*f_send)();
	int	(*f_recv)();
	int	f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile,	recvfile, 1 },
	{ "octet",	validate_access,	sendfile,	recvfile, 0 },
#ifdef notdef
	{ "mail",	validate_user,		sendmail,	recvmail, 1 },
#endif
	{ 0 }
};

/*
 * Handle initial connection protocol.
 */
tftp(tp, size)
	struct tftphdr *tp;
	int size;
{
	register char *cp;
	int first = 1, ecode;
	register struct formats *pf;
	char *filename, *mode;

	filename = cp = tp->th_stuff;
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
	ecode = (*pf->f_validate)(filename, tp->th_opcode);
	if (ecode) {
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
validate_access(filename, mode)
	char *filename;
	int mode;
{
	struct stat stbuf;
	int	fd;
	char *cp, **dirp;

	if (*filename != '/')
		return (EACCESS);
	/*
	 * prevent tricksters from getting around the directory restrictions
	 */
	for (cp = filename + 1; *cp; cp++)
		if(*cp == '.' && strncmp(cp-1, "/../", 4) == 0)
			return(EACCESS);
	for (dirp = dirs; *dirp; dirp++)
		if (strncmp(filename, *dirp, strlen(*dirp)) == 0)
			break;
	if (*dirp==0 && dirp!=dirs)
		return (EACCESS);
	if (stat(filename, &stbuf) < 0)
		return (errno == ENOENT ? ENOTFOUND : EACCESS);
	if (mode == RRQ) {
		if ((stbuf.st_mode&(S_IREAD >> 6)) == 0)
			return (EACCESS);
	} else {
		if ((stbuf.st_mode&(S_IWRITE >> 6)) == 0)
			return (EACCESS);
	}
	fd = open(filename, mode == RRQ ? 0 : 1);
	if (fd < 0)
		return (errno + 100);
	file = fdopen(fd, (mode == RRQ)? "r":"w");
	if (file == NULL) {
		return errno+100;
	}
	return (0);
}

int	timeout;
jmp_buf	timeoutbuf;

void
timer()
{

	timeout += rexmtval;
	if (timeout >= maxtimeout)
		exit(1);
	longjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
sendfile(pf)
	struct formats *pf;
{
	struct tftphdr *dp, *r_init();
	register struct tftphdr *ap;    /* ack packet */
	register int block = 1, size, n;

	signal(SIGALRM, timer);
	dp = r_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons((u_short)block);
		timeout = 0;
		(void) setjmp(timeoutbuf);

send_data:
		if (send(peer, dp, size + 4, 0) != size + 4) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		read_ahead(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);        /* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs((u_short)ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;
			
			if (ap->th_opcode == ACK) {
				if (ap->th_block == block) {
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1)) {
					goto send_data;
				}
			}

		}
		block++;
	} while (size == SEGSIZE);
abort:
	(void) fclose(file);
}

void
justquit()
{
	exit(0);
}


/*
 * Receive a file.
 */
recvfile(pf)
	struct formats *pf;
{
	struct tftphdr *dp, *w_init();
	register struct tftphdr *ap;    /* ack buffer */
	register int block = 0, n, size;

	signal(SIGALRM, timer);
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		timeout = 0;
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htons((u_short)block);
		block++;
		(void) setjmp(timeoutbuf);
send_ack:
		if (send(peer, ackbuf, 4, 0) != 4) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) {            /* really? */
				syslog(LOG_ERR, "tftpd: read: %m\n");
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
	char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		0 }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
nak(error)
	int error;
{
	register struct tftphdr *tp;
	int length;
	register struct errmsg *pe;

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
		syslog(LOG_ERR, "nak: %m\n");
}
