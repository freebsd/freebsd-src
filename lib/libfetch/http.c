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
 * The base64 code in this file is based on code from MIT fetch, which
 * has the following copyright and license:
 *
 *-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.	 M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#include <sys/param.h>

#include <err.h>
#include <ctype.h>
#include <locale.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fetch.h"
#include "common.h"
#include "httperr.h"

extern char *__progname;

#define ENDL "\r\n"

#define HTTP_OK		200
#define HTTP_PARTIAL	206

struct cookie
{
    FILE *real_f;
#define ENC_NONE 0
#define ENC_CHUNKED 1
    int encoding;			/* 1 = chunked, 0 = none */
#define HTTPCTYPELEN 59
    char content_type[HTTPCTYPELEN+1];
    char *buf;
    int b_cur, eof;
    unsigned b_len, chunksize;
};

/*
 * Send a formatted line; optionally echo to terminal
 */
static int
_http_cmd(FILE *f, char *fmt, ...)
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
    
    return 0; /* XXX */
}

/*
 * Fill the input buffer, do chunk decoding on the fly
 */
static char *
_http_fillbuf(struct cookie *c)
{
    char *ln;
    unsigned int len;

    if (c->eof)
	return NULL;

    if (c->encoding == ENC_NONE) {
	c->buf = fgetln(c->real_f, &(c->b_len));
	c->b_cur = 0;
    } else if (c->encoding == ENC_CHUNKED) {
	if (c->chunksize == 0) {
	    ln = fgetln(c->real_f, &len);
	    DEBUG(fprintf(stderr, "\033[1m_http_fillbuf(): new chunk: "
			  "%*.*s\033[m\n", (int)len-2, (int)len-2, ln));
	    sscanf(ln, "%x", &(c->chunksize));
	    if (!c->chunksize) {
		DEBUG(fprintf(stderr, "\033[1m_http_fillbuf(): "
			      "end of last chunk\033[m\n"));
		c->eof = 1;
		return NULL;
	    }
	    DEBUG(fprintf(stderr, "\033[1m_http_fillbuf(): "
			  "new chunk: %X\033[m\n", c->chunksize));
	}
	c->buf = fgetln(c->real_f, &(c->b_len));
	if (c->b_len > c->chunksize)
	    c->b_len = c->chunksize;
	c->chunksize -= c->b_len;
	c->b_cur = 0;
    }
    else return NULL; /* unknown encoding */
    return c->buf;
}

/*
 * Read function
 */
static int
_http_readfn(struct cookie *c, char *buf, int len)
{
    int l, pos = 0;
    while (len) {
	/* empty buffer */
	if (!c->buf || (c->b_cur == c->b_len))
	    if (!_http_fillbuf(c))
		break;

	l = c->b_len - c->b_cur;
	if (len < l) l = len;
	memcpy(buf + pos, c->buf + c->b_cur, l);
	c->b_cur += l;
	pos += l;
	len -= l;
    }
    
    if (ferror(c->real_f))
	return -1;
    else return pos;
}

/*
 * Write function
 */
static int
_http_writefn(struct cookie *c, const char *buf, int len)
{
    size_t r = fwrite(buf, 1, (size_t)len, c->real_f);
    return r ? r : -1;
}

/*
 * Close function
 */
static int
_http_closefn(struct cookie *c)
{
    int r = fclose(c->real_f);
    free(c);
    return (r == EOF) ? -1 : 0;
}

/*
 * Extract content type from cookie
 */
char *
fetchContentType(FILE *f)
{
    /*
     * We have no way of making sure this really *is* one of our cookies,
     * so just check for a null pointer and hope for the best.
     */
    return f->_cookie ? (((struct cookie *)f->_cookie)->content_type) : NULL;
}

/*
 * Base64 encoding
 */
int
_http_base64(char *dst, char *src, int l)
{
    static const char base64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";
    int t, r = 0;
    
    while (l >= 3) {
	t = (src[0] << 16) | (src[1] << 8) | src[2];
	dst[0] = base64[(t >> 18) & 0x3f];
	dst[1] = base64[(t >> 12) & 0x3f];
	dst[2] = base64[(t >> 6) & 0x3f];
	dst[3] = base64[(t >> 0) & 0x3f];
	src += 3; l -= 3;
	dst += 4; r += 4;
    }

    switch (l) {
    case 2:
	t = (src[0] << 16) | (src[1] << 8);
	dst[0] = base64[(t >> 18) & 0x3f];
	dst[1] = base64[(t >> 12) & 0x3f];
	dst[2] = base64[(t >> 6) & 0x3f];
	dst[3] = '=';
	dst += 4;
	r += 4;
	break;
    case 1:
	t = src[0] << 16;
	dst[0] = base64[(t >> 18) & 0x3f];
	dst[1] = base64[(t >> 12) & 0x3f];
	dst[2] = dst[3] = '=';
	dst += 4;
	r += 4;
	break;
    case 0:
	break;
    }

    *dst = 0;
    return r;
}

/*
 * Encode username and password
 */
char *
_http_auth(char *usr, char *pwd)
{
    int len, lu, lp;
    char *str, *s;

    lu = strlen(usr);
    lp = strlen(pwd);
		
    len = (lu * 4 + 2) / 3	/* user name, round up */
	+ 1			/* colon */
	+ (lp * 4 + 2) / 3	/* password, round up */
	+ 1;			/* null */
    
    if ((s = str = (char *)malloc(len)) == NULL)
	return NULL;

    s += _http_base64(s, usr, lu);
    *s++ = ':';
    s += _http_base64(s, pwd, lp);
    *s = 0;

    return str;
}

/*
 * Connect to server or proxy
 */
FILE *
_http_connect(struct url *URL, char *flags)
{
    int direct, sd = -1, verbose;
    size_t len;
    char *px;
    FILE *f;
    
    direct = (flags && strchr(flags, 'd'));
    verbose = (flags && strchr(flags, 'v'));
    
    /* check port */
    if (!URL->port) {
	struct servent *se;

	if ((se = getservbyname("http", "tcp")) != NULL)
	    URL->port = ntohs(se->s_port);
	else
	    URL->port = 80;
    }
    
    /* attempt to connect to proxy server */
    if (!direct && (px = getenv("HTTP_PROXY")) != NULL) {
	char host[MAXHOSTNAMELEN];
	int port = 0;

	/* measure length */
	len = strcspn(px, ":");

	/* get port (XXX atoi is a little too tolerant perhaps?) */
	if (px[len] == ':') {
	    if (strspn(px+len+1, "0123456789") != strlen(px+len+1)
		|| strlen(px+len+1) > 5) {
		/* XXX we should emit some kind of warning */
	    }
	    port = atoi(px+len+1);
	    if (port < 1 || port > 65535) {
		/* XXX we should emit some kind of warning */
	    }
	}
	if (!port) {
#if 0
	    /*
	     * commented out, since there is currently no service name
	     * for HTTP proxies
	     */
	    struct servent *se;
	    
	    if ((se = getservbyname("xxxx", "tcp")) != NULL)
		port = ntohs(se->s_port);
	    else
#endif
		port = 3128;
	}
	
	/* get host name */
	if (len >= MAXHOSTNAMELEN)
	    len = MAXHOSTNAMELEN - 1;
	strncpy(host, px, len);
	host[len] = 0;

	/* connect */
	sd = _fetch_connect(host, port, verbose);
    }

    /* if no proxy is configured or could be contacted, try direct */
    if (sd == -1) {
	if ((sd = _fetch_connect(URL->host, URL->port, verbose)) == -1)
	    goto ouch;
    }

    /* reopen as stream */
    if ((f = fdopen(sd, "r+")) == NULL)
	goto ouch;
    
    return f;

ouch:
    if (sd >= 0)
	close(sd);
    _http_seterr(999); /* XXX do this properly RSN */
    return NULL;
}

/*
 * Send a HEAD or GET request
 */
int
_http_request(FILE *f, char *op, struct url *URL, char *flags)
{
    int e, verbose;
    char *ln, *p;
    size_t len;
    
    verbose = (flags && strchr(flags, 'v'));
    
    /* send request (proxies require absolute form, so use that) */
    if (verbose)
	_fetch_info("requesting http://%s:%d%s",
		    URL->host, URL->port, URL->doc);
    _http_cmd(f, "%s %s://%s:%d%s HTTP/1.1" ENDL,
	      op, URL->scheme, URL->host, URL->port, URL->doc);

    /* start sending headers away */
    if (URL->user[0] || URL->pwd[0]) {
	char *auth_str = _http_auth(URL->user, URL->pwd);
	if (!auth_str)
	    return 999; /* XXX wrong */
	_http_cmd(f, "Authorization: Basic %s" ENDL, auth_str);
	free(auth_str);
    }
    _http_cmd(f, "Host: %s:%d" ENDL, URL->host, URL->port);
    _http_cmd(f, "User-Agent: %s " _LIBFETCH_VER ENDL, __progname);
    if (URL->offset)
	_http_cmd(f, "Range: bytes=%lld-" ENDL, URL->offset);
    _http_cmd(f, "Connection: close" ENDL ENDL);

    /* get response */
    if ((ln = fgetln(f, &len)) == NULL)
	return 999;
    DEBUG(fprintf(stderr, "response: [\033[1m%*.*s\033[m]\n",
		  (int)len-2, (int)len-2, ln));
    
    /* we can't use strchr() and friends since ln isn't NUL-terminated */
    p = ln;
    while ((p < ln + len) && !isspace(*p))
	p++;
    while ((p < ln + len) && !isdigit(*p))
	p++;
    if (!isdigit(*p))
	return 999;
    
    e = atoi(p);
    DEBUG(fprintf(stderr, "code:     [\033[1m%d\033[m]\n", e));
    return e;
}

/*
 * Check a header line
 */
char *
_http_match(char *str, char *hdr)
{
    while (*str && *hdr && tolower(*str++) == tolower(*hdr++))
	/* nothing */;
    if (*str || *hdr != ':')
	return NULL;
    while (*hdr && isspace(*++hdr))
	/* nothing */;
    return hdr;
}

/*
 * Retrieve a file by HTTP
 */
FILE *
fetchGetHTTP(struct url *URL, char *flags)
{
    int e, enc = ENC_NONE, i;
    struct cookie *c;
    char *ln, *p, *q;
    FILE *f, *cf;
    size_t len;
    off_t pos = 0;

    /* allocate cookie */
    if ((c = calloc(1, sizeof *c)) == NULL)
	return NULL;

    /* connect */
    if ((f = _http_connect(URL, flags)) == NULL) {
	free(c);
	return NULL;
    }
    c->real_f = f;

    e = _http_request(f, "GET", URL, flags);
    
    /* add code to handle redirects later */
    if (e != (URL->offset ? HTTP_PARTIAL : HTTP_OK)) {
	_http_seterr(e);
	goto fouch;
    }

    /* browse through header */
    while (1) {
	if ((ln = fgetln(f, &len)) == NULL)
	    goto fouch;
	if ((ln[0] == '\r') || (ln[0] == '\n'))
	    break;
	while (isspace(ln[len-1]))
	    --len;
	ln[len] = '\0'; /* XXX */
	DEBUG(fprintf(stderr, "header:	 [\033[1m%s\033[m]\n", ln));
	if ((p = _http_match("Transfer-Encoding", ln)) != NULL) {
	    for (q = p; *q && !isspace(*q); q++)
		/* VOID */ ;
	    *q = 0;
	    if (strcasecmp(p, "chunked") == 0)
		enc = ENC_CHUNKED;
	    DEBUG(fprintf(stderr, "transfer encoding:  [\033[1m%s\033[m]\n", p));
	} else if ((p = _http_match("Content-Type", ln)) != NULL) {
	    for (i = 0; *p && i < HTTPCTYPELEN; p++, i++)
		    c->content_type[i] = *p;
	    do c->content_type[i--] = 0; while (isspace(c->content_type[i]));
	    DEBUG(fprintf(stderr, "content type: [\033[1m%s\033[m]\n",
			  c->content_type));
	} else if ((p = _http_match("Content-Range", ln)) != NULL) {
	    if (strncasecmp(p, "bytes ", 6) != 0)
		goto fouch;
	    p += 6;
	    while (*p && isdigit(*p))
		pos = pos * 10 + (*p++ - '0');
	    /* XXX wouldn't hurt to be slightly more paranoid here */
	    DEBUG(fprintf(stderr, "content range: [\033[1m%lld-\033[m]\n", pos));
	    if (pos > URL->offset)
		goto fouch;
	}
    }

    /* only body remains */
    c->encoding = enc;
    cf = funopen(c,
		 (int (*)(void *, char *, int))_http_readfn,
		 (int (*)(void *, const char *, int))_http_writefn,
		 (fpos_t (*)(void *, fpos_t, int))NULL,
		 (int (*)(void *))_http_closefn);
    if (cf == NULL)
	goto fouch;

    while (pos < URL->offset)
	if (fgetc(cf) == EOF)
	    goto cfouch;
		
    return cf;
    
fouch:
    fclose(f);
    free(c);
    _http_seterr(999); /* XXX do this properly RSN */
    return NULL;
cfouch:
    fclose(cf);
    _http_seterr(999); /* XXX do this properly RSN */
    return NULL;
}

FILE *
fetchPutHTTP(struct url *URL, char *flags)
{
    warnx("fetchPutHTTP(): not implemented");
    return NULL;
}

/*
 * Get an HTTP document's metadata
 */
int
fetchStatHTTP(struct url *URL, struct url_stat *us, char *flags)
{
    int e;
    size_t len;
    char *ln, *p;
    FILE *f;
    
    us->size = -1;
    us->atime = us->mtime = 0;
    
    /* connect */
    if ((f = _http_connect(URL, flags)) == NULL)
	return -1;

    if ((e = _http_request(f, "HEAD", URL, flags)) != HTTP_OK) {
	_http_seterr(e);
	goto ouch;
    }

    while (1) {
	if ((ln = fgetln(f, &len)) == NULL)
	    goto fouch;
	if ((ln[0] == '\r') || (ln[0] == '\n'))
	    break;
	while (isspace(ln[len-1]))
	    --len;
	ln[len] = '\0'; /* XXX */
	DEBUG(fprintf(stderr, "header:	 [\033[1m%s\033[m]\n", ln));
	if ((p = _http_match("Last-Modified", ln)) != NULL) {
	    struct tm tm;
	    char locale[64];

	    strncpy(locale, setlocale(LC_TIME, NULL), sizeof locale);
	    setlocale(LC_TIME, "C");
	    strptime(p, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	    /* XXX should add support for date-2 and date-3 */
	    setlocale(LC_TIME, locale);
	    us->atime = us->mtime = timegm(&tm);
	    DEBUG(fprintf(stderr, "last modified: [\033[1m%04d-%02d-%02d "
			  "%02d:%02d:%02d\033[m]\n",
			  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			  tm.tm_hour, tm.tm_min, tm.tm_sec));
	} else if ((p = _http_match("Content-Length", ln)) != NULL) {
	    us->size = 0;
	    while (*p && isdigit(*p))
		us->size = us->size * 10 + (*p++ - '0');
	    DEBUG(fprintf(stderr, "content length: [\033[1m%lld\033[m]\n", us->size));
	}
    }

    fclose(f);
    return 0;
 ouch:
    _http_seterr(999); /* XXX do this properly RSN */
 fouch:
    fclose(f);
    return -1;    
}

/*
 * List a directory
 */
struct url_ent *
fetchListHTTP(struct url *url, char *flags)
{
    warnx("fetchListHTTP(): not implemented");
    return NULL;
}
