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
 *	$Id: fetch.c,v 1.7 1998/12/16 10:24:54 des Exp $
 */

#include <sys/param.h>
#include <sys/errno.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fetch.h"
#include "common.h"


int fetchLastErrCode;


/*** Local data **************************************************************/

/*
 * Error messages for parser errors
 */
#define URL_MALFORMED		1
#define URL_BAD_SCHEME		2
#define URL_BAD_PORT		3
static struct fetcherr _url_errlist[] = {
    { URL_MALFORMED,	FETCH_URL,	"Malformed URL" },
    { URL_BAD_SCHEME,	FETCH_URL,	"Invalid URL scheme" },
    { URL_BAD_PORT,	FETCH_URL,	"Invalid server port" },
    { -1,		FETCH_UNKNOWN,	"Unknown parser error" }
};


/*** Public API **************************************************************/

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * read-only stream connected to the document referenced by the URL.
 */
FILE *
fetchGet(struct url *URL, char *flags)
{
    if (strcasecmp(URL->scheme, "file") == 0)
	return fetchGetFile(URL, flags);
    else if (strcasecmp(URL->scheme, "http") == 0)
	return fetchGetHTTP(URL, flags);
    else if (strcasecmp(URL->scheme, "ftp") == 0)
	return fetchGetFTP(URL, flags);
    else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * write-only stream connected to the document referenced by the URL.
 */
FILE *
fetchPut(struct url *URL, char *flags)
{
    if (strcasecmp(URL->scheme, "file") == 0)
	return fetchPutFile(URL, flags);
    else if (strcasecmp(URL->scheme, "http") == 0)
	return fetchPutHTTP(URL, flags);
    else if (strcasecmp(URL->scheme, "ftp") == 0)
	return fetchPutFTP(URL, flags);
    else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return the
 * size of the document referenced by the URL if it exists.
 */
int
fetchStat(struct url *URL, struct url_stat *us, char *flags)
{
    if (strcasecmp(URL->scheme, "file") == 0)
	return fetchStatFile(URL, us, flags);
    else if (strcasecmp(URL->scheme, "http") == 0)
	return fetchStatHTTP(URL, us, flags);
    else if (strcasecmp(URL->scheme, "ftp") == 0)
	return fetchStatFTP(URL, us, flags);
    else {
	_url_seterr(URL_BAD_SCHEME);
	return -1;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * list of files in the directory pointed to by the URL.
 */
struct url_ent *
fetchList(struct url *URL, char *flags)
{
    if (strcasecmp(URL->scheme, "file") == 0)
	return fetchListFile(URL, flags);
    else if (strcasecmp(URL->scheme, "http") == 0)
	return fetchListHTTP(URL, flags);
    else if (strcasecmp(URL->scheme, "ftp") == 0)
	return fetchListFTP(URL, flags);
    else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Attempt to parse the given URL; if successful, call fetchGet().
 */
FILE *
fetchGetURL(char *URL, char *flags)
{
    struct url *u;
    FILE *f;

    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    f = fetchGet(u, flags);
    
    free(u);
    return f;
}


/*
 * Attempt to parse the given URL; if successful, call fetchPut().
 */
FILE *
fetchPutURL(char *URL, char *flags)
{
    struct url *u;
    FILE *f;
    
    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    f = fetchPut(u, flags);
    
    free(u);
    return f;
}

/*
 * Attempt to parse the given URL; if successful, call fetchStat().
 */
int
fetchStatURL(char *URL, struct url_stat *us, char *flags)
{
    struct url *u;
    int s;

    if ((u = fetchParseURL(URL)) == NULL)
	return -1;

    s = fetchStat(u, us, flags);

    free(u);
    return s;
}

/*
 * Attempt to parse the given URL; if successful, call fetchList().
 */
struct url_ent *
fetchListURL(char *URL, char *flags)
{
    struct url *u;
    struct url_ent *ue;

    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;

    ue = fetchList(u, flags);

    free(u);
    return ue;
}

/*
 * Split an URL into components. URL syntax is:
 * method:[//[user[:pwd]@]host[:port]]/[document]
 * This almost, but not quite, RFC1738 URL syntax.
 */
struct url *
fetchParseURL(char *URL)
{
    char *p, *q;
    struct url *u;
    int i;

    /* allocate struct url */
    if ((u = calloc(1, sizeof(struct url))) == NULL) {
	errno = ENOMEM;
	_fetch_syserr();
	return NULL;
    }

    /* scheme name */
    for (i = 0; *URL && (*URL != ':'); URL++)
	if (i < URL_SCHEMELEN)
	    u->scheme[i++] = *URL;
    if (!URL[0] || (URL[1] != '/')) {
	_url_seterr(URL_BAD_SCHEME);
	goto ouch;
    }
    else URL++;
    if (URL[1] != '/') {
	p = URL;
	goto nohost;
    }
    else URL += 2;

    p = strpbrk(URL, "/@");
    if (p && *p == '@') {
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
	    else {
		/* invalid port */
		_url_seterr(URL_BAD_PORT);
		goto ouch;
	    }
	while (*p && (*p != '/'))
	    p++;
    }

nohost:
    /* document */
    if (*p) {
	struct url *t;
	t = realloc(u, sizeof(*u)+strlen(p)-1);
	if (t == NULL) {
	    errno = ENOMEM;
	    _fetch_syserr();
	    goto ouch;
	}
	u = t;
	strcpy(u->doc, p);
    } else {
	u->doc[0] = '/';
	u->doc[1] = 0;
    }
    
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
