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
 *	$Id: fetch.c,v 1.1.1.1 1998/07/09 16:52:42 des Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fetch.h"

#ifndef NDEBUG
#define DEBUG(x) do x; while (0)
#else
#define DEBUG(x) do { } while (0)
#endif

int fetchLastErrCode;
const char *fetchLastErrText;

/* get URL */
FILE *
fetchGetURL(char *URL, char *flags)
{
    url_t *u;
    FILE *f;
    
    /* parse URL */
    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    /* select appropriate function */
    if (strcasecmp(u->scheme, "file") == 0)
	f = fetchGetFile(u, flags);
    else if (strcasecmp(u->scheme, "http") == 0)
	f = fetchGetHTTP(u, flags);
    else if (strcasecmp(u->scheme, "ftp") == 0)
	f = fetchGetFTP(u, flags);
    else f = NULL;

    fetchFreeURL(u);
    return f;
}


/* put URL */
FILE *
fetchPutURL(char *URL, char *flags)
{
    url_t *u;
    FILE *f;
    
    /* parse URL */
    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    /* select appropriate function */
    if (strcasecmp(u->scheme, "file") == 0)
	f = fetchPutFile(u, flags);
    else if (strcasecmp(u->scheme, "http") == 0)
	f = fetchPutHTTP(u, flags);
    else if (strcasecmp(u->scheme, "ftp") == 0)
	f = fetchPutFTP(u, flags);
    else f = NULL;

    fetchFreeURL(u);
    return f;
}

/*
 * Split an URL into components. URL syntax is:
 * method:[//[user[:pwd]@]host[:port]]/[document]
 * This almost, but not quite, RFC1738 URL syntax.
 */
url_t *
fetchParseURL(char *URL)
{
    char *p, *q;
    url_t *u;
    int i;

    /* allocate url_t */
    if ((u = calloc(1, sizeof(url_t))) == NULL)
	return NULL;

    /* scheme name */
    for (i = 0; *URL && (*URL != ':'); URL++)
	if (i < URL_SCHEMELEN)
	    u->scheme[i++] = *URL;
    if (!URL[0] || (URL[1] != '/'))
	goto ouch;
    else URL++;
    if (URL[1] != '/') {
	p = URL;
	goto nohost;
    }
    else URL += 2;

    p = strpbrk(URL, "/@");
    if (*p == '@') {
	/* username */
	for (q = URL, i = 0; (*q != ':') && (*q != '@'); q++)
	    if (i < URL_USERLEN)
		u->user[i++] = *q;
	
	/* password */
	if (*q == ':')
	    for (q++, i = 0; (*q != ':') && (*q != '@'); q++)
		if (i < URL_PWDLEN)
		    u->pwd[i++] = *q;
	
	p++;
    } else p = URL;
    
    /* hostname */
    for (i = 0; *p && (*p != '/') && (*p != ':'); p++)
	if (i < MAXHOSTNAMELEN)
	    u->host[i++] = *p;

    /* port */
    if (*p == ':') {
	for (q = ++p; *q && (*q != '/'); q++)
	    if (isdigit(*q))
		u->port = u->port * 10 + (*q - '0');
	    else return 0; /* invalid port */
	while (*p && (*p != '/'))
	    p++;
    }

nohost:
    /* document */
    if (*p)
	u->doc = strdup(p);
    u->doc = strdup(*p ? p : "/");
    if (!u->doc)
	goto ouch;
    
    DEBUG(fprintf(stderr,
		  "scheme:   [\033[1m%s\033[m]\n"
		  "user:     [\033[1m%s\033[m]\n"
		  "password: [\033[1m%s\033[m]\n"
		  "host:     [\033[1m%s\033[m]\n"
		  "port:     [\033[1m%d\033[m]\n"
		  "document: [\033[1m%s\033[m]\n",
		  u->scheme, u->user, u->pwd,
		  u->host, u->port, u->doc));
    
    return u;

ouch:
    free(u);
    return NULL;
}

void
fetchFreeURL(url_t *u)
{
    if (u) {
	if (u->doc)
	    free(u->doc);
	free(u);
    }
}

int
fetchConnect(char *host, int port)
{
    struct sockaddr_in sin;
    struct hostent *he;
    int sd;

    /* look up host name */
    if ((he = gethostbyname(host)) == NULL)
	return -1;

    /* set up socket address structure */
    bzero(&sin, sizeof(sin));
    bcopy(he->h_addr, (char *)&sin.sin_addr, he->h_length);
    sin.sin_family = he->h_addrtype;
    sin.sin_port = htons(port);

    /* try to connect */
    if ((sd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
	return -1;
    if (connect(sd, (struct sockaddr *)&sin, sizeof sin) < 0) {
	close(sd);
	return -1;
    }

    return sd;
}
