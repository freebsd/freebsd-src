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
 *	$Id$
 */

/*
 * Portions of this code were taken from ftpio.c:
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

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "fetch.h"
#include "ftperr.c"

#define FTP_ANONYMOUS_USER	"ftp"
#define FTP_ANONYMOUS_PASSWORD	"ftp"

static url_t cached_host;
static FILE *cached_socket;
static int _ftp_errcode;

static int
_ftp_isconnected(url_t *url)
{
    return (cached_socket
	    && (strcmp(url->host, cached_host.host) == 0)
	    && (strcmp(url->user, cached_host.user) == 0)
	    && (strcmp(url->pwd, cached_host.pwd) == 0)
	    && (url->port == cached_host.port));
}

/*
 * Get server response, check that first digit is a '2'
 */
static int
_ftp_chkerr(FILE *s, char *e)
{
    char *line;
    size_t len;

    do {
	if (((line = fgetln(s, &len)) == NULL) || (len < 4))
	    return -1;
    } while (line[3] == '-');
    
    if (!isdigit(line[0]) || !isdigit(line[1]) || !isdigit(line[2]) || (line[3] != ' '))
	return -1;

    _ftp_errcode = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');

    if (e)
	*e = _ftp_errcode;

    return (line[0] == '2') - 1;
}

/*
 * Map error code to string
 */
static const char *
_ftp_errstring(int e)
{
    struct ftperr *p = _ftp_errlist;

    while ((p->num) && (p->num != e))
	p++;
    
    return p->string;
}

/*
 * Change remote working directory
 */
static int
_ftp_cwd(FILE *s, char *dir)
{
    fprintf(s, "CWD %s\n", dir);
    if (ferror(s))
	return -1;
    return _ftp_chkerr(s, NULL); /* expecting 250 */
}

/*
 * Retrieve file
 */
static FILE *
_ftp_retr(FILE *s, char *file, int pasv)
{
    char *p;

    /* change directory */
    if (((p = strrchr(file, '/')) != NULL) && (p != file)) {
	*p = 0;
	if (_ftp_cwd(s, file) < 0) {
	    *p = '/';
	    return NULL;
	}
	*p++ = '/';
    } else {
	if (_ftp_cwd(s, "/") < 0)
	    return NULL;
    }

    /* retrieve file; p now points to file name */
    return NULL;
}


/*
 * XXX rewrite these
 */
#if 0
FILE *
fetchGetFTP(url_t *url, char *flags)
{
    int retcode = 0;
    static FILE *fp = NULL;
    static char *prev_host = NULL;
    FILE *fp2;

#ifdef DEFAULT_TO_ANONYMOUS
    if (!url->user[0]) {
	strcpy(url->user, FTP_ANONYMOUS_USER);
	strcpy(url->pwd, FTP_ANONYMOUS_PASSWORD);
    }
#endif
    
    if (fp && prev_host) {
	if (!strcmp(prev_host, url->host)) {
	    /* Try to use cached connection */
	    fp2 = ftpGet(fp, url->doc, NULL);
	    if (!fp2) {
		/* Connection timed out or was no longer valid */
		fclose(fp);
		free(prev_host);
		prev_host = NULL;
	    }
	    else
		return fp2;
	}
	else {
	    /* It's a different host now, flush old */
	    fclose(fp);
	    free(prev_host);
	    prev_host = NULL;
	}
    }
    fp = ftpLogin(url->host, url->user, url->pwd, url->port, 0, &retcode);
    if (fp) {
	if (strchr(flags, 'p')) {
	    if (ftpPassive(fp, 1) != SUCCESS)
		/* XXX what should we do? */ ;
	}
	fp2 = ftpGet(fp, url->doc, NULL);
	if (!fp2) {
	    /* Connection timed out or was no longer valid */
	    retcode = ftpErrno(fp);
	    fclose(fp);
	    fp = NULL;
	}
	else
	    prev_host = strdup(url->host);
	return fp2;
    }
    return NULL;
}

FILE *
fetchPutFTP(url_t *url, char *flags)
{
    static FILE *fp = NULL;
    FILE *fp2;
    int retcode = 0;

    if (fp) {	/* Close previous managed connection */
	fclose(fp);
	fp = NULL;
    }
    fp = ftpLogin(url->host, url->user, url->pwd, url->port, 0, &retcode);
    if (fp) {
	if (strchr(flags, 'p')) {
	    if (ftpPassive(fp, 1) != SUCCESS)
		/* XXX what should we do? */ ;
	}
	fp2 = ftpPut(fp, url->doc);
	if (!fp2) {
	    retcode = ftpErrno(fp);
	    fclose(fp);
	    fp = NULL;
	}
	return fp2;
    }
    return NULL;
}
#endif
