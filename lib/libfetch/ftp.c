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
 *	$Id: ftp.c,v 1.10 1998/12/16 15:29:03 des Exp $
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
#include <netinet/in.h>

#include <ctype.h>
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

#define ENDL "\r\n"

static struct url cached_host;
static FILE *cached_socket;

static char _ftp_last_reply[1024];

/*
 * Get server response, check that first digit is a '2'
 */
static int
_ftp_chkerr(FILE *s)
{
    char *line;
    size_t len;

    do {
	if (((line = fgetln(s, &len)) == NULL) || (len < 4)) {
	    _fetch_syserr();
	    return -1;
	}
    } while (len >= 4 && line[3] == '-');

    while (len && isspace(line[len-1]))
	len--;
    snprintf(_ftp_last_reply, sizeof(_ftp_last_reply),
	     "%*.*s", (int)len, (int)len, line);
    
#ifndef NDEBUG
    fprintf(stderr, "\033[1m<<< ");
    fprintf(stderr, "%*.*s\n", (int)len, (int)len, line);
    fprintf(stderr, "\033[m");
#endif
    
    if (len < 4 || !isdigit(line[1]) || !isdigit(line[1])
	|| !isdigit(line[2]) || (line[3] != ' ')) {
	return -1;
    }

    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

/*
 * Send a command and check reply
 */
static int
_ftp_cmd(FILE *f, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
#ifndef NDEBUG
    fprintf(stderr, "\033[1m>>> ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\033[m");
#endif
    va_end(ap);
    
    return _ftp_chkerr(f);
}

/*
 * Transfer file
 */
static FILE *
_ftp_transfer(FILE *cf, char *oper, char *file, char *mode, int pasv)
{
    struct sockaddr_in sin;
    int e, sd = -1, l;
    char *s;
    FILE *df;
    
    /* change directory */
    if (((s = strrchr(file, '/')) != NULL) && (s != file)) {
	*s = 0;
	if ((e = _ftp_cmd(cf, "CWD %s" ENDL, file)) != FTP_FILE_ACTION_OK) {
	    *s = '/';
	    _ftp_seterr(e);
	    return NULL;
	}
	*s++ = '/';
    } else {
	if ((e = _ftp_cmd(cf, "CWD /" ENDL)) != FTP_FILE_ACTION_OK) {
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
	if ((e = _ftp_cmd(cf, "PASV" ENDL)) != FTP_PASSIVE_MODE)
	    goto ouch;

	/* find address and port number. The reply to the PASV command
           is IMHO the one and only weak point in the FTP protocol. */
	ln = _ftp_last_reply;
	for (p = ln + 3; !isdigit(*p); p++)
	    /* nothing */ ;
	for (p--, i = 0; i < 6; i++) {
	    p++; /* skip the comma */
	    addr[i] = strtol(p, &p, 10);
	}

	/* construct sockaddr for data socket */
	l = sizeof(sin);
	if (getpeername(fileno(cf), (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	bcopy(addr, (char *)&sin.sin_addr, 4);
	bcopy(addr + 4, (char *)&sin.sin_port, 2);	

	/* connect to data port */
	if (connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
	    goto sysouch;
	
	/* make the server initiate the transfer */
	e = _ftp_cmd(cf, "%s %s" ENDL, oper, s);
	if (e != FTP_OPEN_DATA_CONNECTION)
	    goto ouch;
	
    } else {
	u_int32_t a;
	u_short p;
	int d;
	
	/* find our own address, bind, and listen */
	l = sizeof(sin);
	if (getsockname(fileno(cf), (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	sin.sin_port = 0;
	if (bind(sd, (struct sockaddr *)&sin, l) == -1)
	    goto sysouch;
	if (listen(sd, 1) == -1)
	    goto sysouch;

	/* find what port we're on and tell the server */
	if (getsockname(sd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	a = ntohl(sin.sin_addr.s_addr);
	p = ntohs(sin.sin_port);
	e = _ftp_cmd(cf, "PORT %d,%d,%d,%d,%d,%d" ENDL,
		     (a >> 24) & 0xff, (a >> 16) & 0xff,
		     (a >> 8) & 0xff, a & 0xff,
		     (p >> 8) & 0xff, p & 0xff);
	if (e != FTP_OK)
	    goto ouch;

	/* make the server initiate the transfer */
	e = _ftp_cmd(cf, "%s %s" ENDL, oper, s);
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
    _ftp_seterr(e);
    close(sd);
    return NULL;
}

/*
 * Log on to FTP server
 */
static FILE *
_ftp_connect(char *host, int port, char *user, char *pwd, int verbose)
{
    int sd, e, pp = FTP_DEFAULT_PORT;
    char *p, *q;
    FILE *f;

    /* check for proxy */
    if ((p = getenv("FTP_PROXY")) != NULL) {
	if ((q = strchr(p, ':')) != NULL) {
	    /* XXX check that it's a valid number */
	    pp = atoi(q+1);
	}
	if (q)
	    *q = 0;
	sd = _fetch_connect(p, pp, verbose);
	if (q)
	    *q = ':';
    } else {
	/* no proxy, go straight to target */
	sd = _fetch_connect(host, port, verbose);
    }

    /* check connection */
    if (sd == -1) {
	_fetch_syserr();
	return NULL;
    }

    /* streams make life easier */
    if ((f = fdopen(sd, "r+")) == NULL) {
	_fetch_syserr();
	close(sd);
	return NULL;
    }

    /* expect welcome message */
    if ((e = _ftp_chkerr(f)) != FTP_SERVICE_READY)
	goto fouch;
    
    /* send user name and password */
    if (!user || !*user)
	user = FTP_ANONYMOUS_USER;
    e = p ? _ftp_cmd(f, "USER %s@%s@%d" ENDL, user, host, port)
	  : _ftp_cmd(f, "USER %s" ENDL, user);
    
    /* did the server request a password? */
    if (e == FTP_NEED_PASSWORD) {
	if (!pwd || !*pwd)
	    pwd = FTP_ANONYMOUS_PASSWORD;
	e = _ftp_cmd(f, "PASS %s" ENDL, pwd);
    }

    /* did the server request an account? */
    if (e == FTP_NEED_ACCOUNT)
	goto fouch;
    
    /* we should be done by now */
    if (e != FTP_LOGGED_IN)
	goto fouch;

    /* might as well select mode and type at once */
#ifdef FTP_FORCE_STREAM_MODE
    if ((e = _ftp_cmd(f, "MODE S" ENDL)) != FTP_OK) /* default is S */
	goto fouch;
#endif
    if ((e = _ftp_cmd(f, "TYPE I" ENDL)) != FTP_OK) /* default is A */
	goto fouch;

    /* done */
    return f;
    
fouch:
    _ftp_seterr(e);
    fclose(f);
    return NULL;
}

/*
 * Disconnect from server
 */
static void
_ftp_disconnect(FILE *f)
{
    (void)_ftp_cmd(f, "QUIT" ENDL);
    fclose(f);
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
static FILE *
_ftp_cached_connect(struct url *url, char *flags)
{
    FILE *cf;

    cf = NULL;
    
    /* set default port */
    if (!url->port)
	url->port = FTP_DEFAULT_PORT;
    
    /* try to use previously cached connection */
    if (_ftp_isconnected(url))
	if (_ftp_cmd(cached_socket, "NOOP" ENDL) != -1)
	    cf = cached_socket;

    /* connect to server */
    if (!cf) {
	cf = _ftp_connect(url->host, url->port, url->user, url->pwd,
			  (strchr(flags, 'v') != NULL));
	if (!cf)
	    return NULL;
	if (cached_socket)
	    _ftp_disconnect(cached_socket);
	cached_socket = cf;
	memcpy(&cached_host, url, sizeof(struct url));
    }

    return cf;
}

/*
 * Get file
 */
FILE *
fetchGetFTP(struct url *url, char *flags)
{
    FILE *cf;

    /* connect to server */
    if ((cf = _ftp_cached_connect(url, flags)) == NULL)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cf, "RETR", url->doc, "r",
			 (flags && strchr(flags, 'p')));
}

/*
 * Put file
 */
FILE *
fetchPutFTP(struct url *url, char *flags)
{
    FILE *cf;

    /* connect to server */
    if ((cf = _ftp_cached_connect(url, flags)) == NULL)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cf, (flags && strchr(flags, 'a')) ? "APPE" : "STOR",
			 url->doc, "w", (flags && strchr(flags, 'p')));
}

/*
 * Get file stats
 */
int
fetchStatFTP(struct url *url, struct url_stat *us, char *flags)
{
    FILE *cf;
    char *ln, *s;
    struct tm tm;
    time_t t;
    int e;

    /* connect to server */
    if ((cf = _ftp_cached_connect(url, flags)) == NULL)
	return -1;

    /* change directory */
    if (((s = strrchr(url->doc, '/')) != NULL) && (s != url->doc)) {
	*s = 0;
	if ((e = _ftp_cmd(cf, "CWD %s" ENDL, url->doc)) != FTP_FILE_ACTION_OK) {
	    *s = '/';
	    goto ouch;
	}
	*s++ = '/';
    } else {
	if ((e = _ftp_cmd(cf, "CWD /" ENDL)) != FTP_FILE_ACTION_OK)
	    goto ouch;
    }

    /* s now points to file name */
    
    if (_ftp_cmd(cf, "SIZE %s" ENDL, s) != FTP_FILE_STATUS)
	goto ouch;
    for (ln = _ftp_last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    for (us->size = 0; *ln && isdigit(*ln); ln++)
	us->size = us->size * 10 + *ln - '0';
    if (*ln && !isspace(*ln)) {
	_ftp_seterr(999); /* XXX should signal a FETCH_PROTO error */
	return -1;
    }

    if ((e = _ftp_cmd(cf, "MDTM %s" ENDL, s)) != FTP_FILE_STATUS)
	goto ouch;
    for (ln = _ftp_last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    t = time(NULL);
    us->mtime = localtime(&t)->tm_gmtoff;
    sscanf(ln, "%04d%02d%02d%02d%02d%02d",
	   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	   &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    /* XXX should check the return value from sscanf */
    tm.tm_mon--;
    tm.tm_year -= 1900;
    tm.tm_isdst = -1;
    tm.tm_gmtoff = 0;
    us->mtime += mktime(&tm);
    us->atime = us->mtime;
    return 0;

ouch:
    _ftp_seterr(e);
    return -1;
}
