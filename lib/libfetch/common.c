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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "fetch.h"
#include "common.h"

/*** Local data **************************************************************/

/*
 * Error messages for resolver errors
 */
static struct fetcherr _netdb_errlist[] = {
    { HOST_NOT_FOUND,	"Host not found" },
    { TRY_AGAIN,	"Transient resolver failure" },
    { NO_RECOVERY,	"Non-recoverable resolver failure" },
    { NO_DATA,		"No address record" },
    { -1,		"Unknown resolver error" }
};
#define _netdb_errstring(n)	_fetch_errstring(_netdb_errlist, n)
#define _netdb_seterr(n)	_fetch_seterr(_netdb_errlist, n)


/*** Error-reporting functions ***********************************************/

/*
 * Map error code to string
 */
const char *
_fetch_errstring(struct fetcherr *p, int e)
{
    while ((p->num != -1) && (p->num != e))
	p++;
    
    return p->string;
}

/*
 * Set error code
 */
void
_fetch_seterr(struct fetcherr *p, int e)
{
    fetchLastErrCode = e;
    fetchLastErrText = _fetch_errstring(p, e);
}

/*
 * Set error code according to errno
 */
void
_fetch_syserr(void)
{
    fetchLastErrCode = errno;
    fetchLastErrText = strerror(errno);
}


/*** Network-related utility functions ***************************************/

/*
 * Establish a TCP connection to the specified port on the specified host.
 */
int
fetchConnect(char *host, int port)
{
    struct sockaddr_in sin;
    struct hostent *he;
    int sd;

#ifndef NDEBUG
    fprintf(stderr, "\033[1m---> %s:%d\033[m\n", host, port);
#endif
    
    /* look up host name */
    if ((he = gethostbyname(host)) == NULL) {
	_netdb_seterr(h_errno);
	return -1;
    }

    /* set up socket address structure */
    bzero(&sin, sizeof(sin));
    bcopy(he->h_addr, (char *)&sin.sin_addr, he->h_length);
    sin.sin_family = he->h_addrtype;
    sin.sin_port = htons(port);

    /* try to connect */
    if ((sd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	_fetch_syserr();
	return -1;
    }
    if (connect(sd, (struct sockaddr *)&sin, sizeof sin) == -1) {
	_fetch_syserr();
	close(sd);
	return -1;
    }

    return sd;
}
