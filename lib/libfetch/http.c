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
 *	$Id: http.c,v 1.1.1.1 1998/07/09 16:52:41 des Exp $
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <err.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fetch.h"
#include "httperr.c"

#ifndef NDEBUG
#define DEBUG(x) do x; while (0)
#else
#define DEBUG(x) do { } while (0)
#endif

extern char *__progname;

extern int fprint64(FILE *f, const unsigned char *buf);

#define ENDL "\r\n"

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

static const char *
_http_errstring(int e)
{
    struct httperr *p = _http_errlist;

    while ((p->num != -1) && (p->num != e))
	p++;
    
    return p->string;
}

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

static int
_http_writefn(struct cookie *c, const char *buf, int len)
{
    size_t r = fwrite(buf, 1, (size_t)len, c->real_f);
    return r ? r : -1;
}

static int
_http_closefn(struct cookie *c)
{
    int r = fclose(c->real_f);
    free(c);
    return (r == EOF) ? -1 : 0;
}

char *
fetchContentType(FILE *f)
{
    /*
     * We have no way of making sure this really *is* one of our cookies,
     * so just check for a null pointer and hope for the best.
     */
    return f->_cookie ? (((struct cookie *)f->_cookie)->content_type) : NULL;
}

FILE *
fetchGetHTTP(url_t *URL, char *flags)
{
    int sd = -1, err, i, enc = ENC_NONE;
    struct cookie *c;
    char *ln, *p, *q;
    FILE *f, *cf;
    size_t len;

    /* allocate cookie */
    if ((c = calloc(1, sizeof(struct cookie))) == NULL)
	return NULL;

    /* check port */
    if (!URL->port)
	URL->port = 80; /* default HTTP port */
    
    /* attempt to connect to proxy server */
    if (getenv("HTTP_PROXY")) {
	char *px, host[MAXHOSTNAMELEN];
	int port = 3128; /* XXX I think 3128 is default... check? */
	size_t len;

	/* measure length */
	px = getenv("HTTP_PROXY");
	len = strcspn(px, ":");

	/* get port (atoi is a little too tolerant perhaps?) */
	if (px[len] == ':')
	    port = atoi(px+len+1);
	
	/* get host name */
	if (len >= MAXHOSTNAMELEN)
	    len = MAXHOSTNAMELEN - 1;
	strncpy(host, px, len);
	host[len] = 0;

	/* connect */
	sd = fetchConnect(host, port);
    }

    /* if no proxy is configured or could be contacted, try direct */
    if (sd < 0) {
	if ((sd = fetchConnect(URL->host, URL->port)) < 0)
	    goto ouch;
    }

    /* reopen as stream */
    if ((f = fdopen(sd, "r+")) == NULL)
	goto ouch;
    c->real_f = f;

    /* send request (proxies require absolute form, so use that) */
    fprintf(f, "GET http://%s:%d/%s HTTP/1.1" ENDL,
	    URL->host, URL->port, URL->doc);

    /* start sending headers away */
    if (URL->user[0] || URL->pwd[0]) {
	fprintf(f, "Authorization: Basic ");
	fprint64(f, (const unsigned char *)URL->user);
	fputc(':', f);
	fprint64(f, (const unsigned char *)URL->pwd);
	fputs(ENDL, f);
    }
    fprintf(f, "Host: %s:%d" ENDL, URL->host, URL->port);
    fprintf(f, "User-Agent: %s " _LIBFETCH_VER ENDL, __progname);
    fprintf(f, "Connection: close" ENDL ENDL);

    /* get response */
    if ((ln = fgetln(f, &len)) == NULL)
	goto fouch;
    DEBUG(fprintf(stderr, "response: [\033[1m%*.*s\033[m]\n",
		  (int)len-2, (int)len-2, ln));
    
    /* we can't use strchr() and friends since ln isn't NUL-terminated */
    p = ln;
    while ((p < ln + len) && !isspace(*p))
	p++;
    while ((p < ln + len) && !isdigit(*p))
	p++;
    if (!isdigit(*p))
	goto fouch;
    err = atoi(p);
    DEBUG(fprintf(stderr, "code:     [\033[1m%d\033[m]\n", err));
    
    /* add code to handle redirects later */
    if (err != 200) {
	fetchLastErrCode = err;
	fetchLastErrText = _http_errstring(err);
	goto fouch;
    }

    /* browse through header */
    while (1) {
	if ((ln = fgetln(f, &len)) == NULL)
	    goto fouch;
	if ((ln[0] == '\r') || (ln[0] == '\n'))
	    break;
	DEBUG(fprintf(stderr, "header:   [\033[1m%*.*s\033[m]\n",
		      (int)len-2, (int)len-2, ln));
#define XFERENC "Transfer-Encoding:"
	if (strncasecmp(ln, XFERENC, sizeof(XFERENC)-1) == 0) {
	    p = ln + sizeof(XFERENC) - 1;
	    while ((p < ln + len) && isspace(*p))
		p++;
	    for (q = p; (q < ln + len) && !isspace(*q); q++)
		/* VOID */ ;
	    *q = 0;
	    if (strcasecmp(p, "chunked") == 0)
		enc = ENC_CHUNKED;
	    DEBUG(fprintf(stderr, "xferenc:  [\033[1m%s\033[m]\n", p));
#undef XFERENC
#define CONTTYPE "Content-Type:"
	} else if (strncasecmp(ln, CONTTYPE, sizeof(CONTTYPE)-1) == 0) {
	    p = ln + sizeof(CONTTYPE) - 1;
	    while ((p < ln + len) && isspace(*p))
		p++;
	    for (i = 0; p < ln + len; p++)
		if (i < HTTPCTYPELEN)
		    c->content_type[i++] = *p;
	    do c->content_type[i--] = 0; while (isspace(c->content_type[i]));
	    DEBUG(fprintf(stderr, "conttype: [\033[1m%s\033[m]\n",
			  c->content_type));
#undef CONTTYPE
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
    return cf;
    
ouch:
    if (sd >= 0)
	close(sd);
    free(c);
    return NULL;
fouch:
    fclose(f);
    free(c);
    return NULL;
}

FILE *
fetchPutHTTP(url_t *URL, char *flags)
{
    warnx("fetchPutHTTP(): not implemented");
    return NULL;
}
