/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $FreeBSD$
 */

/*
 * Portions of this code were taken from or based on ftpio.c:
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Dag-Erling Coïdan Smørgrav
 * 9 Jun 1998
 *
 * Incorporated into libfetch
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 *
 * $ftpioId: ftpio.c,v 1.30 1998/04/11 07:28:53 phk Exp $
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fetch.h"
#include "common.h"
#include "ftperr.h"

#define FTP_ANONYMOUS_USER	"ftp"
#define FTP_ANONYMOUS_PASSWORD	"ftp"
#define FTP_DEFAULT_PORT 21

#define FTP_OPEN_DATA_CONNECTION	150
#define FTP_OK				200
#define FTP_FILE_STATUS			213
#define FTP_SERVICE_READY		220
#define FTP_PASSIVE_MODE		227
#define FTP_LOGGED_IN			230
#define FTP_FILE_ACTION_OK		250
#define FTP_NEED_PASSWORD		331
#define FTP_NEED_ACCOUNT		332
#define FTP_FILE_OK			350
#define FTP_SYNTAX_ERROR		500

static char ENDL[2] = "\r\n";

static struct url cached_host;
static int cached_socket;

static char *last_reply;
static size_t lr_size, lr_length;
static int last_code;

#define isftpreply(foo) (isdigit(foo[0]) && isdigit(foo[1]) \
			 && isdigit(foo[2]) && foo[3] == ' ')
#define isftpinfo(foo) (isdigit(foo[0]) && isdigit(foo[1]) \
			&& isdigit(foo[2]) && foo[3] == '-')

/*
 * Get server response
 */
static int
_ftp_chkerr(int cd)
{
    do {
	if (_fetch_getln(cd, &last_reply, &lr_size, &lr_length) == -1) {
	    _fetch_syserr();
	    return -1;
	}
#ifndef NDEBUG
	_fetch_info("got reply '%.*s'", lr_length - 2, last_reply);
#endif
    } while (isftpinfo(last_reply));

    while (lr_length && isspace(last_reply[lr_length-1]))
	lr_length--;
    last_reply[lr_length] = 0;
    
    if (!isftpreply(last_reply)) {
	_ftp_seterr(999);
	return -1;
    }

    last_code = (last_reply[0] - '0') * 100
	+ (last_reply[1] - '0') * 10
	+ (last_reply[2] - '0');

    return last_code;
}

/*
 * Send a command and check reply
 */
static int
_ftp_cmd(int cd, char *fmt, ...)
{
    va_list ap;
    struct iovec iov[2];
    char *msg;
    int r;

    va_start(ap, fmt);
    vasprintf(&msg, fmt, ap);
    va_end(ap);
    
    if (msg == NULL) {
	errno = ENOMEM;
	_fetch_syserr();
	return -1;
    }
#ifndef NDEBUG
    _fetch_info("sending '%s'", msg);
#endif
    iov[0].iov_base = msg;
    iov[0].iov_len = strlen(msg);
    iov[1].iov_base = ENDL;
    iov[1].iov_len = sizeof ENDL;
    r = writev(cd, iov, 2);
    free(msg);
    if (r == -1) {
	_fetch_syserr();
	return -1;
    }
    
    return _ftp_chkerr(cd);
}

/*
 * Transfer file
 */
static FILE *
_ftp_transfer(int cd, char *oper, char *file,
	      char *mode, off_t offset, char *flags)
{
    struct sockaddr_in sin;
    int pasv, high, verbose;
    int e, sd = -1;
    socklen_t l;
    char *s;
    FILE *df;

    /* check flags */
    pasv = (flags && strchr(flags, 'p'));
    high = (flags && strchr(flags, 'h'));
    verbose = (flags && strchr(flags, 'v'));

    /* change directory */
    if (((s = strrchr(file, '/')) != NULL) && (s != file)) {
	*s = 0;
	if (verbose)
	    _fetch_info("changing directory to %s", file);
	if ((e = _ftp_cmd(cd, "CWD %s", file)) != FTP_FILE_ACTION_OK) {
	    *s = '/';
	    if (e != -1)
		_ftp_seterr(e);
	    return NULL;
	}
	*s++ = '/';
    } else {
	if (verbose)
	    _fetch_info("changing directory to /");
	if ((e = _ftp_cmd(cd, "CWD /")) != FTP_FILE_ACTION_OK) {
	    if (e != -1)
		_ftp_seterr(e);
	    return NULL;
	}
    }

    /* s now points to file name */

    /* open data socket */
    if ((sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	_fetch_syserr();
	return NULL;
    }
    
    if (pasv) {
	u_char addr[6];
	char *ln, *p;
	int i;
	
	/* send PASV command */
	if (verbose)
	    _fetch_info("setting passive mode");
	if ((e = _ftp_cmd(cd, "PASV")) != FTP_PASSIVE_MODE)
	    goto ouch;

	/*
	 * Find address and port number. The reply to the PASV command
         * is IMHO the one and only weak point in the FTP protocol.
	 */
	ln = last_reply;
	for (p = ln + 3; !isdigit(*p); p++)
	    /* nothing */ ;
	for (p--, i = 0; i < 6; i++) {
	    p++; /* skip the comma */
	    addr[i] = strtol(p, &p, 10);
	}

	/* seek to required offset */
	if (offset)
	    if (_ftp_cmd(cd, "REST %lu", (u_long)offset) != FTP_FILE_OK)
		goto sysouch;
	
	/* construct sockaddr for data socket */
	l = sizeof sin;
	if (getpeername(cd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	bcopy(addr, (char *)&sin.sin_addr, 4);
	bcopy(addr + 4, (char *)&sin.sin_port, 2);	

	/* connect to data port */
	if (verbose)
	    _fetch_info("opening data connection");
	if (connect(sd, (struct sockaddr *)&sin, sizeof sin) == -1)
	    goto sysouch;

	/* make the server initiate the transfer */
	if (verbose)
	    _fetch_info("initiating transfer");
	e = _ftp_cmd(cd, "%s %s", oper, s);
	if (e != FTP_OPEN_DATA_CONNECTION)
	    goto ouch;
	
    } else {
	u_int32_t a;
	u_short p;
	int arg, d;
	
	/* find our own address, bind, and listen */
	l = sizeof sin;
	if (getsockname(cd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	sin.sin_port = 0;
	arg = high ? IP_PORTRANGE_HIGH : IP_PORTRANGE_DEFAULT;
	if (setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
		       (char *)&arg, sizeof arg) == -1)
	    goto sysouch;
	if (verbose)
	    _fetch_info("binding data socket");
	if (bind(sd, (struct sockaddr *)&sin, l) == -1)
	    goto sysouch;
	if (listen(sd, 1) == -1)
	    goto sysouch;

	/* find what port we're on and tell the server */
	if (getsockname(sd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	a = ntohl(sin.sin_addr.s_addr);
	p = ntohs(sin.sin_port);
	e = _ftp_cmd(cd, "PORT %d,%d,%d,%d,%d,%d",
		     (a >> 24) & 0xff, (a >> 16) & 0xff,
		     (a >> 8) & 0xff, a & 0xff,
		     (p >> 8) & 0xff, p & 0xff);
	if (e != FTP_OK)
	    goto ouch;

	/* make the server initiate the transfer */
	if (verbose)
	    _fetch_info("initiating transfer");
	e = _ftp_cmd(cd, "%s %s", oper, s);
	if (e != FTP_OPEN_DATA_CONNECTION)
	    goto ouch;
	
	/* accept the incoming connection and go to town */
	if ((d = accept(sd, NULL, NULL)) == -1)
	    goto sysouch;
	close(sd);
	sd = d;
    }

    if ((df = fdopen(sd, mode)) == NULL)
	goto sysouch;
    return df;

sysouch:
    _fetch_syserr();
    close(sd);
    return NULL;

ouch:
    if (e != -1)
	_ftp_seterr(e);
    close(sd);
    return NULL;
}

/*
 * Log on to FTP server
 */
static int
_ftp_connect(char *host, int port, char *user, char *pwd, char *flags)
{
    int cd, e, pp = 0, direct, verbose;
    char *p, *q;

    direct = (flags && strchr(flags, 'd'));
    verbose = (flags && strchr(flags, 'v'));
    
    /* check for proxy */
    if (!direct && (p = getenv("FTP_PROXY")) != NULL) {
	if ((q = strchr(p, ':')) != NULL) {
	    if (strspn(q+1, "0123456789") != strlen(q+1) || strlen(q+1) > 5) {
		/* XXX we should emit some kind of warning */
	    }
	    pp = atoi(q+1);
	    if (pp < 1 || pp > 65535) {
		/* XXX we should emit some kind of warning */
	    }
	}
	if (!pp) {
	    struct servent *se;
	    
	    if ((se = getservbyname("ftp", "tcp")) != NULL)
		pp = ntohs(se->s_port);
	    else
		pp = FTP_DEFAULT_PORT;
	}
	if (q)
	    *q = 0;
	cd = _fetch_connect(p, pp, verbose);
	if (q)
	    *q = ':';
    } else {
	/* no proxy, go straight to target */
	cd = _fetch_connect(host, port, verbose);
	p = NULL;
    }

    /* check connection */
    if (cd == -1) {
	_fetch_syserr();
	return NULL;
    }

    /* expect welcome message */
    if ((e = _ftp_chkerr(cd)) != FTP_SERVICE_READY)
	goto fouch;
    
    /* send user name and password */
    if (!user || !*user)
	user = FTP_ANONYMOUS_USER;
    e = p ? _ftp_cmd(cd, "USER %s@%s@%d", user, host, port)
	  : _ftp_cmd(cd, "USER %s", user);
    
    /* did the server request a password? */
    if (e == FTP_NEED_PASSWORD) {
	if (!pwd || !*pwd)
	    pwd = FTP_ANONYMOUS_PASSWORD;
	e = _ftp_cmd(cd, "PASS %s", pwd);
    }

    /* did the server request an account? */
    if (e == FTP_NEED_ACCOUNT)
	goto fouch;
    
    /* we should be done by now */
    if (e != FTP_LOGGED_IN)
	goto fouch;

    /* might as well select mode and type at once */
#ifdef FTP_FORCE_STREAM_MODE
    if ((e = _ftp_cmd(cd, "MODE S")) != FTP_OK) /* default is S */
	goto fouch;
#endif
    if ((e = _ftp_cmd(cd, "TYPE I")) != FTP_OK) /* default is A */
	goto fouch;

    /* done */
    return cd;
    
fouch:
    if (e != -1)
	_ftp_seterr(e);
    close(cd);
    return NULL;
}

/*
 * Disconnect from server
 */
static void
_ftp_disconnect(int cd)
{
    (void)_ftp_cmd(cd, "QUIT");
    close(cd);
}

/*
 * Check if we're already connected
 */
static int
_ftp_isconnected(struct url *url)
{
    return (cached_socket
	    && (strcmp(url->host, cached_host.host) == 0)
	    && (strcmp(url->user, cached_host.user) == 0)
	    && (strcmp(url->pwd, cached_host.pwd) == 0)
	    && (url->port == cached_host.port));
}

/*
 * Check the cache, reconnect if no luck
 */
static int
_ftp_cached_connect(struct url *url, char *flags)
{
    int e, cd;

    cd = -1;
    
    /* set default port */
    if (!url->port) {
	struct servent *se;
	
	if ((se = getservbyname("ftp", "tcp")) != NULL)
	    url->port = ntohs(se->s_port);
	else
	    url->port = FTP_DEFAULT_PORT;
    }
    
    /* try to use previously cached connection */
    if (_ftp_isconnected(url)) {
	e = _ftp_cmd(cached_socket, "NOOP");
	if (e == FTP_OK || e == FTP_SYNTAX_ERROR)
	    cd = cached_socket;
    }

    /* connect to server */
    if (cd == -1) {
	cd = _ftp_connect(url->host, url->port, url->user, url->pwd, flags);
	if (cd == -1)
	    return -1;
	if (cached_socket)
	    _ftp_disconnect(cached_socket);
	cached_socket = cd;
	memcpy(&cached_host, url, sizeof *url);
    }

    return cd;
}

/*
 * Get file
 */
FILE *
fetchGetFTP(struct url *url, char *flags)
{
    int cd;
    
    /* connect to server */
    if ((cd = _ftp_cached_connect(url, flags)) == NULL)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cd, "RETR", url->doc, "r", url->offset, flags);
}

/*
 * Put file
 */
FILE *
fetchPutFTP(struct url *url, char *flags)
{
    int cd;

    /* connect to server */
    if ((cd = _ftp_cached_connect(url, flags)) == NULL)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cd, (flags && strchr(flags, 'a')) ? "APPE" : "STOR",
			 url->doc, "w", url->offset, flags);
}

/*
 * Get file stats
 */
int
fetchStatFTP(struct url *url, struct url_stat *us, char *flags)
{
    char *ln, *s;
    struct tm tm;
    time_t t;
    int e, cd;

    /* connect to server */
    if ((cd = _ftp_cached_connect(url, flags)) == NULL)
	return -1;

    /* change directory */
    if (((s = strrchr(url->doc, '/')) != NULL) && (s != url->doc)) {
	*s = 0;
	if ((e = _ftp_cmd(cd, "CWD %s", url->doc)) != FTP_FILE_ACTION_OK) {
	    *s = '/';
	    goto ouch;
	}
	*s++ = '/';
    } else {
	if ((e = _ftp_cmd(cd, "CWD /")) != FTP_FILE_ACTION_OK)
	    goto ouch;
    }

    /* s now points to file name */
    
    if (_ftp_cmd(cd, "SIZE %s", s) != FTP_FILE_STATUS)
	goto ouch;
    for (ln = last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    for (us->size = 0; *ln && isdigit(*ln); ln++)
	us->size = us->size * 10 + *ln - '0';
    if (*ln && !isspace(*ln)) {
	_ftp_seterr(999);
	return -1;
    }

    if ((e = _ftp_cmd(cd, "MDTM %s", s)) != FTP_FILE_STATUS)
	goto ouch;
    for (ln = last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    sscanf(ln, "%04d%02d%02d%02d%02d%02d",
	   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	   &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    /* XXX should check the return value from sscanf */
    tm.tm_mon--;
    tm.tm_year -= 1900;
    tm.tm_isdst = -1;
    t = mktime(&tm);
    if (t == (time_t)-1)
	t = time(NULL);
    else
	t += tm.tm_gmtoff;
    us->mtime = t;
    us->atime = t;
    return 0;

ouch:
    if (e != -1)
	_ftp_seterr(e);
    return -1;
}

/*
 * List a directory
 */
extern void warnx(char *, ...);
struct url_ent *
fetchListFTP(struct url *url, char *flags)
{
    warnx("fetchListFTP(): not implemented");
    return NULL;
}
