/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ftp.c,v 1.15 1995/12/07 10:33:47 peter Exp $
 *
 * Return values have been sanitized:
 *	-1	error, but you (still) have a session.
 *	-2	error, your session is dead.
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "ftp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Handy global for us to stick the port # */
int FtpPort;

/* How to see by a given code whether or not the connection has timed out */
#define FTP_TIMEOUT(code)	(code == 421)

#ifndef STANDALONE_FTP
#include "sysinstall.h"
#endif /*STANDALONE_FTP*/

static void
debug(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
#ifdef STANDALONE_FTP
    strcpy(p,"LIBFTP: ");
    (void) vsnprintf(p + strlen(p), sizeof p - strlen(p), fmt, ap);
    va_end(ap);
    write(ftp->fd_debug, p, strlen(p));
#else
    if (isDebug()) {
	(void) vsnprintf(p, sizeof p - strlen(p), fmt, ap);
	msgDebug(p);
    }	
#endif
}

static int
writes(int fd, char *s)
{
    int i = strlen(s);
    if (i != write(fd, s, i))
	return -2;
    return 0;
}

static __inline char*
get_a_line(FTP_t ftp)
{
    static char buf[BUFSIZ];
    int i,j;

    for(i=0;i<BUFSIZ;) {
	j = read(ftp->fd_ctrl, buf+i, 1);
	if (j != 1)
	    return 0;
	if (buf[i] == '\r' || buf[i] == '\n') {
	    if (!i)
		continue;
	    buf[i] = '\0';
	    debug(ftp, "received <%s>\n", buf);
	    return buf;
	}
	i++;
    }
    return buf;
}

static int
get_a_number(FTP_t ftp, char **q)
{
    char *p;
    int i = -1,j;

    while(1) {
	p = get_a_line(ftp);
	if (!p)
	    return -2;
	if (!(isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2])))
	    continue;
	if (i == -1 && p[3] == '-') {
	    i = strtol(p, 0, 0);
	    continue;
	}
	if (p[3] != ' ' && p[3] != '\t')
	    continue;
	j = strtol(p, 0, 0);
	if (i == -1) {
	    if (q) *q = p+4;
	    return j;
	} else if (j == i) {
	    if (q) *q = p+4;
	    return j;
	}
    }
}

static int
zap(FTP_t ftp)
{
    int i;

    i = writes(ftp->fd_ctrl,"QUIT\r\n");
    if (isDebug())
	msgDebug("Zapping ftp connection on %d returns %d\n", ftp->fd_ctrl, i);
    close(ftp->fd_ctrl); ftp->fd_ctrl = -1;
    close(ftp->fd_xfer); ftp->fd_xfer = -1;
    ftp->state = init;
    return -2;
}

static int
botch(FTP_t ftp, char *func, char *state)
{
    debug(ftp, "Botch: %s called outside state %s\n",func,state);
    return -2;
}

static int
cmd(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    int i;

    va_list ap;
    va_start(ap, fmt);
    (void) vsnprintf(p, sizeof p, fmt, ap);
    va_end(ap);

    debug(ftp, "send <%s>\n", p);
    strcat(p,"\r\n");
    if (writes(ftp->fd_ctrl, p))
	return -2;
    i = get_a_number(ftp, 0);
    return i;
}

FTP_t
FtpInit()
{
    FTP_t ftp;

    ftp = malloc(sizeof *ftp);
    if (!ftp)
	return ftp;
    memset(ftp, 0, sizeof *ftp);
    ftp->fd_ctrl = -1;
    ftp->fd_xfer = -1;
    ftp->fd_debug = -1;
    ftp->state = init;
    return ftp;
}

#ifdef STANDALONE_FTP
void
FtpDebug(FTP_t ftp, int i)
{
    ftp->fd_debug = i;
}
#endif

int
FtpOpen(FTP_t ftp, char *host, char *user, char *passwd)
{
    struct hostent	*he = NULL;
    struct sockaddr_in 	sin;
    int 		s;
    unsigned long 	temp;
    int			i;

    if (ftp->state != init)
	return botch(ftp,"FtpOpen","init");

    if (!user)
	user = "ftp";

    if (!passwd)
	passwd = "??@??(FreeBSD:libftp)";	/* XXX */

    debug(ftp, "FtpOpen(ftp, %s, %s, %s)\n", host, user, passwd);

    temp = inet_addr(host);
    if (temp != INADDR_NONE) {
	debug(ftp, "Using dotted IP address `%s'\n", host);
	ftp->addrtype = sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = temp;
    }
    else {
	debug(ftp, "Trying to resolve `%s'\n", host);
	he = gethostbyname(host);
	if (!he) {
	    debug(ftp, "Lookup of `%s' failed!\n", host);
	    return zap(ftp);
	}
	ftp->addrtype = sin.sin_family = he->h_addrtype;
	bcopy(he->h_addr, (char *)&sin.sin_addr, he->h_length);
    }

    sin.sin_port = htons(FtpPort ? FtpPort : 21);

    if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0)
    {
	debug(ftp, "Socket open failed: %s (%i)\n", strerror(errno), errno);
	return zap(ftp);
    }

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	debug(ftp,"Connection failed: %s (%i)\n", strerror(errno), errno);
	(void)close(s);
	return zap(ftp);
    }

    ftp->fd_ctrl = s;

    debug(ftp, "open (%d)\n",get_a_number(ftp,0));

    i = cmd(ftp, "USER %s", user);
    if (i >= 300 && i < 400)
	i = cmd(ftp,"PASS %s",passwd);
    if (i >= 299 || i < 0) {
	close(ftp->fd_ctrl);
	ftp->fd_ctrl = -1;
	return zap(ftp);
    }
    ftp->state = isopen;
    return 0;
}

void
FtpClose(FTP_t ftp)
{
    if (ftp->state != init)
	return;

    if (ftp->state != isopen)
	botch(ftp,"FtpClose","open or init");

    debug(ftp, "FtpClose(ftp)\n");
    zap(ftp);
}

int
FtpChdir(FTP_t ftp, char *dir)
{
    int i;

    if (ftp->state != isopen)
	return botch(ftp,"FtpChdir","open");
    i = cmd(ftp, "CWD %s", dir);
    if (i < 0)
	return i;
    else if (i != 250)
	return -1;
    return 0;
}

int
FtpGet(FTP_t ftp, char *file)
{
    int i,s;
    char *q;
    unsigned char addr[64];
    struct sockaddr_in sin;
    u_long a;

    debug(ftp, "FtpGet(ftp,%s)\n", file);
    if (ftp->state != isopen)
	return botch(ftp, "FtpGet", "open");
    if (ftp->binary) {
	i = cmd(ftp, "TYPE I");
	if (i < 0 || FTP_TIMEOUT(i))
	    return zap(ftp);
	if (i > 299)
	    return -1;
    }
    else
	return -1;

    if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0)
	return zap(ftp);

    if (ftp->passive) {
	debug(ftp, "send <%s>\n", "PASV");
	if (writes(ftp->fd_ctrl, "PASV\r\n"))
	    return zap(ftp);
	i = get_a_number(ftp, &q);
	if (i < 0)
	    return zap(ftp);
	if (i != 227)
	    return zap(ftp);
	while (*q && !isdigit(*q))
	    q++;
	if (!*q)
	    return zap(ftp);
	q--;
	for(i = 0; i < 6; i++) {
	    q++;
	    addr[i] = strtol(q, &q, 10);
	}

	sin.sin_family = ftp->addrtype;
	bcopy(addr, (char *)&sin.sin_addr, 4);
	bcopy(addr + 4, (char *)&sin.sin_port, 2);
	debug(ftp, "Opening active socket to %s : %u\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));

	debug(ftp, "Connecting to %s:%u\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    (void)close(s);
	    debug(ftp, "connect: %s (%d)\n", strerror(errno), errno);
	    return -1;
	}
	ftp->fd_xfer = s;
	i = cmd(ftp,"RETR %s", file);
	if (i < 0 || FTP_TIMEOUT(i))  {
	    close(s);
	    return zap(ftp);
	}
	else if (i > 299) {
	    if (isDebug())
		msgDebug("FTP: No such file %s, moving on.\n", file);
	    close(s);
	    return -1;
	}
	ftp->state = xfer;
	return s;
    } else {
	i = sizeof sin;
	getsockname(ftp->fd_ctrl, (struct sockaddr *)&sin, &i);
	sin.sin_port = 0;
	i = sizeof sin;
	if (bind(s,(struct sockaddr *)&sin, i) < 0) {
	    close (s);	
	    debug(ftp, "bind failed %d\n", errno);
	    return zap(ftp);
	}
	getsockname(s, (struct sockaddr *)&sin, &i);
	if (listen(s, 1) < 0) {
	    close (s);
	    debug(ftp, "listen failed %d\n", errno);
	    return zap(ftp);
	}
	a = ntohl(sin.sin_addr.s_addr);
	i = cmd(ftp, "PORT %d,%d,%d,%d,%d,%d",
		(a                   >> 24) & 0xff,
		(a                   >> 16) & 0xff,
		(a                   >>  8) & 0xff,
		a                          & 0xff,
		(ntohs(sin.sin_port) >>  8) & 0xff,
		ntohs(sin.sin_port)        & 0xff);
	if (i != 200)
	    return -1;
	i = cmd(ftp,"RETR %s", file);
	if (i < 0) {
	    close(s);
	    return zap(ftp);
	}
	else if (i > 299 || FTP_TIMEOUT(i)) {
	    if (isDebug())
		msgDebug("FTP: No such file %s, moving on.\n", file);
	    close(s);
	    if (FTP_TIMEOUT(i))
		return zap(ftp);
	    else
		return -1;
        }
	ftp->fd_xfer = accept(s, 0, 0);
	if (ftp->fd_xfer < 0) {
	    close(s);
	    return zap(ftp);
	}
	ftp->state = xfer;
	close(s);
	return(ftp->fd_xfer);
    }
}

int
FtpEOF(FTP_t ftp)
{
    int i;

    if (ftp->state != xfer)
	return botch(ftp, "FtpEOF", "xfer");
    debug(ftp, "FtpEOF(ftp)\n");
    close(ftp->fd_xfer);
    ftp->fd_xfer = -1;
    ftp->state = isopen;
    i = get_a_number(ftp,0);
    if (i < 0)
	return zap(ftp);
    else if (i != 250 && i != 226)
	return -1;
    else
	return 0;
}

#ifdef STANDALONE_FTP

/* main.c */
int
main(int argc, char **argv)
{
    FTP_t ftp;
    int i;
    char c;

    ftp = FtpInit();
    if (!ftp)
	err(1, "FtpInit()");

    FtpDebug(ftp, 1);
    i = FtpOpen(ftp, "freefall.cdrom.com", "ftp", "phk-libftp@");
    FtpBinary(ftp, 1);
    FtpPassive(ftp, 0);
    FtpChdir(ftp, "/pub");
    FtpChdir(ftp, "FreeBSD");
    i = FtpGet(ftp, "README");
    while (1 == read(i, &c, 1))
	putchar(c);
    FtpEOF(ftp);
    return 0;
}

#endif /*STANDALONE_FTP*/
