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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fetch.h"
#include "common.h"


/*** Local data **************************************************************/

/*
 * Error messages for resolver errors
 */
static struct fetcherr _netdb_errlist[] = {
    { EAI_NODATA,	FETCH_RESOLV,	"Host not found" },
    { EAI_AGAIN,	FETCH_TEMP,	"Transient resolver failure" },
    { EAI_FAIL,		FETCH_RESOLV,	"Non-recoverable resolver failure" },
    { EAI_NONAME,	FETCH_RESOLV,	"No address record" },
    { -1,		FETCH_UNKNOWN,	"Unknown resolver error" }
};

/* End-of-Line */
static const char ENDL[2] = "\r\n";


/*** Error-reporting functions ***********************************************/

/*
 * Map error code to string
 */
static struct fetcherr *
_fetch_finderr(struct fetcherr *p, int e)
{
    while (p->num != -1 && p->num != e)
	p++;
    return p;
}

/*
 * Set error code
 */
void
_fetch_seterr(struct fetcherr *p, int e)
{
    p = _fetch_finderr(p, e);
    fetchLastErrCode = p->cat;
    snprintf(fetchLastErrString, MAXERRSTRING, "%s", p->string);
}

/*
 * Set error code according to errno
 */
void
_fetch_syserr(void)
{
    int e;
    e = errno;
    
    switch (errno) {
    case 0:
	fetchLastErrCode = FETCH_OK;
	break;
    case EPERM:
    case EACCES:
    case EROFS:
    case EAUTH:
    case ENEEDAUTH:
	fetchLastErrCode = FETCH_AUTH;
	break;
    case ENOENT:
    case EISDIR: /* XXX */
	fetchLastErrCode = FETCH_UNAVAIL;
	break;
    case ENOMEM:
	fetchLastErrCode = FETCH_MEMORY;
	break;
    case EBUSY:
    case EAGAIN:	
	fetchLastErrCode = FETCH_TEMP;
	break;
    case EEXIST:
	fetchLastErrCode = FETCH_EXISTS;
	break;
    case ENOSPC:
	fetchLastErrCode = FETCH_FULL;
	break;
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ENETDOWN:
    case ENETUNREACH:
    case ENETRESET:
    case EHOSTUNREACH:
	fetchLastErrCode = FETCH_NETWORK;
	break;
    case ECONNABORTED:
    case ECONNRESET:
	fetchLastErrCode = FETCH_ABORT;
	break;
    case ETIMEDOUT:
	fetchLastErrCode = FETCH_TIMEOUT;
	break;
    case ECONNREFUSED:
    case EHOSTDOWN:
	fetchLastErrCode = FETCH_DOWN;
	break;
    default:
	fetchLastErrCode = FETCH_UNKNOWN;
    }
    snprintf(fetchLastErrString, MAXERRSTRING, "%s", strerror(e));
}


/*
 * Emit status message
 */
void
_fetch_info(const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}


/*** Network-related utility functions ***************************************/

/*
 * Return the default port for a scheme
 */
int
_fetch_default_port(const char *scheme)
{
    struct servent *se;

    if ((se = getservbyname(scheme, "tcp")) != NULL)
	return ntohs(se->s_port);
    if (strcasecmp(scheme, SCHEME_FTP) == 0)
	return FTP_DEFAULT_PORT;
    if (strcasecmp(scheme, SCHEME_HTTP) == 0)
	return HTTP_DEFAULT_PORT;
    return 0;
}

/*
 * Return the default proxy port for a scheme
 */
int
_fetch_default_proxy_port(const char *scheme)
{
    if (strcasecmp(scheme, SCHEME_FTP) == 0)
	return FTP_DEFAULT_PROXY_PORT;
    if (strcasecmp(scheme, SCHEME_HTTP) == 0)
	return HTTP_DEFAULT_PROXY_PORT;
    return 0;
}

/*
 * Establish a TCP connection to the specified port on the specified host.
 */
int
_fetch_connect(const char *host, int port, int af, int verbose)
{
    char pbuf[10];
    struct addrinfo hints, *res, *res0;
    int sd, err;

    DEBUG(fprintf(stderr, "\033[1m---> %s:%d\033[m\n", host, port));

    if (verbose)
	_fetch_info("looking up %s", host);
    
    /* look up host name and set up socket address structure */
    snprintf(pbuf, sizeof(pbuf), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    if ((err = getaddrinfo(host, pbuf, &hints, &res0)) != 0) {
	_netdb_seterr(err);
	return -1;
    }

    if (verbose)
	_fetch_info("connecting to %s:%d", host, port);
    
    /* try to connect */
    for (sd = -1, res = res0; res; res = res->ai_next) {
	if ((sd = socket(res->ai_family, res->ai_socktype,
			 res->ai_protocol)) == -1)
	    continue;
	if (connect(sd, res->ai_addr, res->ai_addrlen) != -1)
	    break;
	close(sd);
	sd = -1;
    }
    freeaddrinfo(res0);
    if (sd == -1) {
	_fetch_syserr();
	return -1;
    }

    return sd;
}


/*
 * Read a line of text from a socket w/ timeout
 */
#define MIN_BUF_SIZE 1024

int
_fetch_getln(int fd, char **buf, size_t *size, size_t *len)
{
    struct timeval now, timeout, wait;
    fd_set readfds;
    int r;
    char c;
    
    if (*buf == NULL) {
	if ((*buf = malloc(MIN_BUF_SIZE)) == NULL) {
	    errno = ENOMEM;
	    return -1;
	}
	*size = MIN_BUF_SIZE;
    }

    **buf = '\0';
    *len = 0;

    if (fetchTimeout) {
	gettimeofday(&timeout, NULL);
	timeout.tv_sec += fetchTimeout;
	FD_ZERO(&readfds);
    }
    
    do {
	if (fetchTimeout) {
	    FD_SET(fd, &readfds);
	    gettimeofday(&now, NULL);
	    wait.tv_sec = timeout.tv_sec - now.tv_sec;
	    wait.tv_usec = timeout.tv_usec - now.tv_usec;
	    if (wait.tv_usec < 0) {
		wait.tv_usec += 1000000;
		wait.tv_sec--;
	    }
	    if (wait.tv_sec < 0) {
		errno = ETIMEDOUT;
		return -1;
	    }
	    r = select(fd+1, &readfds, NULL, NULL, &wait);
	    if (r == -1) {
		if (errno == EINTR && fetchRestartCalls)
		    continue;
		/* EBADF or EINVAL: shouldn't happen */
		return -1;
	    }
	    if (!FD_ISSET(fd, &readfds))
		continue;
	}
	r = read(fd, &c, 1);
	if (r == 0)
	    break;
	if (r == -1) {
	    if (errno == EINTR && fetchRestartCalls)
		continue;
	    /* any other error is bad news */
	    return -1;
	}
	(*buf)[*len] = c;
	*len += 1;
	if (*len == *size) {
	    char *tmp;
	    
	    if ((tmp = realloc(*buf, *size * 2 + 1)) == NULL) {
		errno = ENOMEM;
		return -1;
	    }
	    *buf = tmp;
	    *size = *size * 2 + 1;
	}
    } while (c != '\n');
    
    DEBUG(fprintf(stderr, "\033[1m<<< %.*s\033[m", (int)*len, *buf));
    return 0;
}


/*
 * Write a line of text to a socket w/ timeout
 * XXX currently does not enforce timeout
 */
int
_fetch_putln(int fd, const char *str, size_t len)
{
    struct iovec iov[2];
    ssize_t wlen;

    /* XXX should enforce timeout */
    (const char *)iov[0].iov_base = str; /* XXX */
    iov[0].iov_len = len;
    (const char *)iov[1].iov_base = ENDL; /* XXX */
    iov[1].iov_len = sizeof ENDL;
    len += sizeof ENDL;
    wlen = writev(fd, iov, 2);
    if (wlen < 0 || (size_t)wlen != len)
	return -1;
    DEBUG(fprintf(stderr, "\033[1m>>> %s\n\033[m", str));
    return 0;
}


/*** Directory-related utility functions *************************************/

int
_fetch_add_entry(struct url_ent **p, int *size, int *len,
		 const char *name, struct url_stat *us)
{
    struct url_ent *tmp;

    if (*p == NULL) {
#define INITIAL_SIZE 8
	if ((*p = malloc(INITIAL_SIZE * sizeof **p)) == NULL) {
	    errno = ENOMEM;
	    _fetch_syserr();
	    return -1;
	}
	*size = INITIAL_SIZE;
	*len = 0;
#undef INITIAL_SIZE
    }
    
    if (*len >= *size - 1) {
	tmp = realloc(*p, *size * 2 * sizeof **p);
	if (tmp == NULL) {
	    errno = ENOMEM;
	    _fetch_syserr();
	    return -1;
	}
	*size *= 2;
	*p = tmp;
    }

    tmp = *p + *len;
    snprintf(tmp->name, PATH_MAX, "%s", name);
    bcopy(us, &tmp->stat, sizeof *us);

    (*len)++;
    (++tmp)->name[0] = 0;

    return 0;
}
