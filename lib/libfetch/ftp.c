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
 *	$Id: ftp.c,v 1.1.1.1 1998/07/09 16:52:42 des Exp $
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fetch.h"
#include "ftperr.c"

#define FTP_DEFAULT_TO_ANONYMOUS
#define FTP_ANONYMOUS_USER	"ftp"
#define FTP_ANONYMOUS_PASSWORD	"ftp"

#define ENDL "\r\n"

static url_t cached_host;
static FILE *cached_socket;

#ifndef NDEBUG
#define TRACE fprintf(stderr, "TRACE on line %d in " __FILE__ "\n", __LINE__);
#else
#define TRACE
#endif

/*
 * Map error code to string
 */
static const char *
_ftp_errstring(int e)
{
    struct ftperr *p = _ftp_errlist;

    while ((p->num != -1) && (p->num != e))
	p++;
    
    return p->string;
}

/*
 * Set error code
 */
static void
_ftp_seterr(int e)
{
    fetchLastErrCode = e;
    fetchLastErrText = _ftp_errstring(e);
}

/*
 * Set error code according to errno
 */
static void
_ftp_syserr(void)
{
    fetchLastErrCode = errno;
    fetchLastErrText = strerror(errno);
}

/*
 * Get server response, check that first digit is a '2'
 */
static int
_ftp_chkerr(FILE *s, int *e)
{
    char *line;
    size_t len;

    TRACE;
    
    if (e)
	*e = 0;
    
    do {
	if (((line = fgetln(s, &len)) == NULL) || (len < 4))
	{
	    _ftp_syserr();
	    return -1;
	}
    } while (line[3] == '-');
    
    if (!isdigit(line[1]) || !isdigit(line[1])
	|| !isdigit(line[2]) || (line[3] != ' ')) {
	_ftp_seterr(-1);
	return -1;
    }

    _ftp_seterr((line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0'));

    if (e)
	*e = fetchLastErrCode;

    return (line[0] == '2') - 1;
}

/*
 * Change remote working directory
 */
static int
_ftp_cwd(FILE *s, char *dir)
{
    TRACE;
    
    fprintf(s, "CWD %s\n", dir);
    if (ferror(s)) {
	_ftp_syserr();
	return -1;
    }
    return _ftp_chkerr(s, NULL); /* expecting 250 */
}

/*
 * Retrieve file
 */
static FILE *
_ftp_retrieve(FILE *cf, char *file, int pasv)
{
    char *p;

    TRACE;
    
    /* change directory */
    if (((p = strrchr(file, '/')) != NULL) && (p != file)) {
	*p = 0;
	if (_ftp_cwd(cf, file) < 0) {
	    *p = '/';
	    return NULL;
	}
	*p++ = '/';
    } else {
	if (_ftp_cwd(cf, "/") < 0)
	    return NULL;
    }

    /* retrieve file; p now points to file name */
    fprintf(stderr, "Arrrgh! No! No! I can't do it! Leave me alone!\n");
    return NULL;
}

/*
 * Store file
 */
static FILE *
_ftp_store(FILE *cf, char *file, int pasv)
{
    TRACE;
    
    cf = cf;
    file = file;
    pasv = pasv;
    return NULL;
}

/*
 * Log on to FTP server
 */
static FILE *
_ftp_connect(char *host, int port, char *user, char *pwd)
{
    int sd, e;
    FILE *f;

    TRACE;
    
    /* establish control connection */
    if ((sd = fetchConnect(host, port)) < 0) {
	_ftp_syserr();
	return NULL;
    }
    if ((f = fdopen(sd, "r+")) == NULL) {
	_ftp_syserr();
	goto ouch;
    }

    /* expect welcome message */
    if (_ftp_chkerr(f, NULL) < 0)
	goto fouch;
    
    /* send user name and password */
    fprintf(f, "USER %s" ENDL, user);
    _ftp_chkerr(f, &e);
    if (e == 331) {
	/* server requested a password */
	fprintf(f, "PASS %s" ENDL, pwd);
	_ftp_chkerr(f, &e);
    }
    if (e == 332) {
	/* server requested an account */
    }
    if (e != 230) /* won't let us near the WaReZ */
	goto fouch;

    /* might as well select mode and type at once */
#ifdef FTP_FORCE_STREAM_MODE
    fprintf(f, "MODE S" ENDL);
    if (_ftp_chkerr(f, NULL) < 0)
	goto ouch;
#endif
    fprintf(f, "TYPE I" ENDL);
    if (_ftp_chkerr(f, NULL) < 0)
	goto ouch;

    /* done */
    return f;
    
ouch:
    close(sd);
    return NULL;
fouch:
    fclose(f);
    return NULL;
}

/*
 * Disconnect from server
 */
static void
_ftp_disconnect(FILE *f)
{
    TRACE;
    
    fprintf(f, "QUIT" ENDL);
    _ftp_chkerr(f, NULL);
    fclose(f);
}

/*
 * Check if we're already connected
 */
static int
_ftp_isconnected(url_t *url)
{
    TRACE;
    
    return (cached_socket
	    && (strcmp(url->host, cached_host.host) == 0)
	    && (strcmp(url->user, cached_host.user) == 0)
	    && (strcmp(url->pwd, cached_host.pwd) == 0)
	    && (url->port == cached_host.port));
}

FILE *
fetchGetFTP(url_t *url, char *flags)
{
    FILE *cf = NULL;
    int e;

    TRACE;
    
#ifdef DEFAULT_TO_ANONYMOUS
    if (!url->user[0]) {
	strcpy(url->user, FTP_ANONYMOUS_USER);
	strcpy(url->pwd, FTP_ANONYMOUS_PASSWORD);
    }
#endif

    /* set default port */
    if (!url->port)
	url->port = 21;
    
    /* try to use previously cached connection */
    if (_ftp_isconnected(url)) {
	fprintf(cached_socket, "PWD" ENDL);
	_ftp_chkerr(cached_socket, &e);
	if (e > 0)
	    cf = cached_socket;
    }

    /* connect to server */
    if (!cf) {
	cf = _ftp_connect(url->host, url->port, url->user, url->pwd);
	if (!cf)
	    return NULL;
	if (cached_socket)
	    _ftp_disconnect(cached_socket);
	cached_socket = cf;
	memcpy(&cached_host, url, sizeof(url_t));
    }

    /* initiate the transfer */
    return _ftp_retrieve(cf, url->doc, (flags && strchr(flags, 'p')));
}

/*
 * Upload a file.
 * Hmmm, that's almost an exact duplicate of the above...
 */
FILE *
fetchPutFTP(url_t *url, char *flags)
{
    FILE *cf = NULL;
    int e;
   
#ifdef DEFAULT_TO_ANONYMOUS
    if (!url->user[0]) {
	strcpy(url->user, FTP_ANONYMOUS_USER);
	strcpy(url->pwd, FTP_ANONYMOUS_PASSWORD);
    }
#endif

    /* set default port */
    if (!url->port)
	url->port = 21;
    
    /* try to use previously cached connection */
    if (_ftp_isconnected(url)) {
	fprintf(cached_socket, "PWD" ENDL);
	_ftp_chkerr(cached_socket, &e);
	if (e > 0)
	    cf = cached_socket;
    }

    /* connect to server */
    if (!cf) {
	cf = _ftp_connect(url->host, url->port, url->user, url->pwd);
	if (!cf)
	    return NULL;
	if (cached_socket)
	    _ftp_disconnect(cached_socket);
	cached_socket = cf;
	memcpy(&cached_host, url, sizeof(url_t));
    }


    /* initiate the transfer */
    return _ftp_store(cf, url->doc, (flags && strchr(flags, 'p')));
}
