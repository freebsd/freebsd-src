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

#include <sys/param.h>
#include <sys/errno.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fetch.h"
#include "common.h"

auth_t	 fetchAuthMethod;
int	 fetchLastErrCode;
char	 fetchLastErrString[MAXERRSTRING];
int	 fetchTimeout;
int	 fetchRestartCalls = 1;


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
 * Also fill out the struct url_stat.
 */
FILE *
fetchXGet(struct url *URL, struct url_stat *us, const char *flags)
{
    int direct;

    direct = CHECK_FLAG('d');
    if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
	return fetchXGetFile(URL, us, flags);
    else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
	return fetchXGetHTTP(URL, us, flags);
    else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0) {
	return fetchXGetFTP(URL, us, flags);
    } else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * read-only stream connected to the document referenced by the URL.
 */
FILE *
fetchGet(struct url *URL, const char *flags)
{
    return fetchXGet(URL, NULL, flags);
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * write-only stream connected to the document referenced by the URL.
 */
FILE *
fetchPut(struct url *URL, const char *flags)
{
    int direct;

    direct = CHECK_FLAG('d');
    if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
	return fetchPutFile(URL, flags);
    else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
	return fetchPutHTTP(URL, flags);
    else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0) {
	return fetchPutFTP(URL, flags);
    } else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return the
 * size of the document referenced by the URL if it exists.
 */
int
fetchStat(struct url *URL, struct url_stat *us, const char *flags)
{
    int direct;

    direct = CHECK_FLAG('d');
    if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
	return fetchStatFile(URL, us, flags);
    else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
	return fetchStatHTTP(URL, us, flags);
    else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0) {
	return fetchStatFTP(URL, us, flags);
    } else {
	_url_seterr(URL_BAD_SCHEME);
	return -1;
    }
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * list of files in the directory pointed to by the URL.
 */
struct url_ent *
fetchList(struct url *URL, const char *flags)
{
    int direct;

    direct = CHECK_FLAG('d');
    if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
	return fetchListFile(URL, flags);
    else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
	return fetchListHTTP(URL, flags);
    else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0) {
	return fetchListFTP(URL, flags);
    } else {
	_url_seterr(URL_BAD_SCHEME);
	return NULL;
    }
}

/*
 * Attempt to parse the given URL; if successful, call fetchXGet().
 */
FILE *
fetchXGetURL(const char *URL, struct url_stat *us, const char *flags)
{
    struct url *u;
    FILE *f;

    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    f = fetchXGet(u, us, flags);
    
    fetchFreeURL(u);
    return f;
}

/*
 * Attempt to parse the given URL; if successful, call fetchGet().
 */
FILE *
fetchGetURL(const char *URL, const char *flags)
{
    return fetchXGetURL(URL, NULL, flags);
}

/*
 * Attempt to parse the given URL; if successful, call fetchPut().
 */
FILE *
fetchPutURL(const char *URL, const char *flags)
{
    struct url *u;
    FILE *f;
    
    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;
    
    f = fetchPut(u, flags);
    
    fetchFreeURL(u);
    return f;
}

/*
 * Attempt to parse the given URL; if successful, call fetchStat().
 */
int
fetchStatURL(const char *URL, struct url_stat *us, const char *flags)
{
    struct url *u;
    int s;

    if ((u = fetchParseURL(URL)) == NULL)
	return -1;

    s = fetchStat(u, us, flags);

    fetchFreeURL(u);
    return s;
}

/*
 * Attempt to parse the given URL; if successful, call fetchList().
 */
struct url_ent *
fetchListURL(const char *URL, const char *flags)
{
    struct url *u;
    struct url_ent *ue;

    if ((u = fetchParseURL(URL)) == NULL)
	return NULL;

    ue = fetchList(u, flags);

    fetchFreeURL(u);
    return ue;
}

/*
 * Make a URL
 */
struct url *
fetchMakeURL(const char *scheme, const char *host, int port, const char *doc,
    const char *user, const char *pwd)
{
    struct url *u;

    if (!scheme || (!host && !doc)) {
	_url_seterr(URL_MALFORMED);
	return NULL;
    }
	
    if (port < 0 || port > 65535) {
	_url_seterr(URL_BAD_PORT);
	return NULL;
    }
    
    /* allocate struct url */
    if ((u = calloc(1, sizeof *u)) == NULL) {
	_fetch_syserr();
	return NULL;
    }

    if ((u->doc = strdup(doc ? doc : "/")) == NULL) {
	_fetch_syserr();
	free(u);
	return NULL;
    }
    
#define seturl(x) snprintf(u->x, sizeof u->x, "%s", x)
    seturl(scheme);
    seturl(host);
    seturl(user);
    seturl(pwd);
#undef seturl
    u->port = port;

    return u;
}

/*
 * Split an URL into components. URL syntax is:
 * [method:/][/[user[:pwd]@]host[:port]/][document]
 * This almost, but not quite, RFC1738 URL syntax.
 */
struct url *
fetchParseURL(const char *URL)
{
    char *doc;
    const char *p, *q;
    struct url *u;
    int i;

    /* allocate struct url */
    if ((u = calloc(1, sizeof *u)) == NULL) {
	_fetch_syserr();
	return NULL;
    }

    /* scheme name */
    if ((p = strstr(URL, ":/"))) {
	snprintf(u->scheme, URL_SCHEMELEN+1, "%.*s", p - URL, URL);
	URL = ++p;
	/*
	 * Only one slash: no host, leave slash as part of document
	 * Two slashes: host follows, strip slashes
	 */
	if (URL[1] == '/')
	    URL = (p += 2);
    } else {
	p = URL;
    }
    if (!*URL || *URL == '/')
	goto nohost;

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
#ifdef INET6
    if (*p == '[' && (q = strchr(p + 1, ']')) != NULL &&
	(*++q == '\0' || *q == '/' || *q == ':')) {
	if ((i = q - p - 2) > MAXHOSTNAMELEN)
	    i = MAXHOSTNAMELEN;
	strncpy(u->host, ++p, i);
	p = q;
    } else
#endif
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
    if (!*p)
	p = "/";
    
    if (strcasecmp(u->scheme, SCHEME_HTTP) == 0 ||
	strcasecmp(u->scheme, SCHEME_HTTPS) == 0) {
	const char hexnums[] = "0123456789abcdef";

	/* percent-escape whitespace. */
	if ((doc = malloc(strlen(p) * 3 + 1)) == NULL) {
	    _fetch_syserr();
	    goto ouch;
	}
	u->doc = doc;
	while (*p != '\0') {
	    if (!isspace(*p)) {
		*doc++ = *p++;
            } else {
		*doc++ = '%';
		*doc++ = hexnums[((unsigned int)*p) >> 4];
		*doc++ = hexnums[((unsigned int)*p) & 0xf];
		p++;
            }
	}
	*doc = '\0';
    } else if ((u->doc = strdup(p)) == NULL) {
	_fetch_syserr();
	goto ouch;
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

/*
 * Free a URL
 */
void
fetchFreeURL(struct url *u)
{
    free(u->doc);
    free(u);
}
