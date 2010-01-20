/*-
 * Copyright (c) 2000-2004 Dag-Erling Coïdan Smørgrav
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
 *    derived from this software without specific prior written permission.
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

/*
 * The following copyright applies to the base64 code:
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
 * software without specific, written prior permission.  M.I.T. makes
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
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "fetch.h"
#include "common.h"
#include "httperr.h"

/* Maximum number of redirects to follow */
#define MAX_REDIRECT 5

/* Symbolic names for reply codes we care about */
#define HTTP_OK			200
#define HTTP_PARTIAL		206
#define HTTP_MOVED_PERM		301
#define HTTP_MOVED_TEMP		302
#define HTTP_SEE_OTHER		303
#define HTTP_TEMP_REDIRECT	307
#define HTTP_NEED_AUTH		401
#define HTTP_NEED_PROXY_AUTH	407
#define HTTP_BAD_RANGE		416
#define HTTP_PROTOCOL_ERROR	999

#define HTTP_REDIRECT(xyz) ((xyz) == HTTP_MOVED_PERM \
			    || (xyz) == HTTP_MOVED_TEMP \
			    || (xyz) == HTTP_TEMP_REDIRECT \
			    || (xyz) == HTTP_SEE_OTHER)

#define HTTP_ERROR(xyz) ((xyz) > 400 && (xyz) < 599)


/*****************************************************************************
 * I/O functions for decoding chunked streams
 */

struct httpio
{
	conn_t		*conn;		/* connection */
	int		 chunked;	/* chunked mode */
	char		*buf;		/* chunk buffer */
	size_t		 bufsize;	/* size of chunk buffer */
	ssize_t		 buflen;	/* amount of data currently in buffer */
	int		 bufpos;	/* current read offset in buffer */
	int		 eof;		/* end-of-file flag */
	int		 error;		/* error flag */
	size_t		 chunksize;	/* remaining size of current chunk */
#ifndef NDEBUG
	size_t		 total;
#endif
};

/*
 * Get next chunk header
 */
static int
http_new_chunk(struct httpio *io)
{
	char *p;

	if (fetch_getln(io->conn) == -1)
		return (-1);

	if (io->conn->buflen < 2 || !isxdigit((unsigned char)*io->conn->buf))
		return (-1);

	for (p = io->conn->buf; *p && !isspace((unsigned char)*p); ++p) {
		if (*p == ';')
			break;
		if (!isxdigit((unsigned char)*p))
			return (-1);
		if (isdigit((unsigned char)*p)) {
			io->chunksize = io->chunksize * 16 +
			    *p - '0';
		} else {
			io->chunksize = io->chunksize * 16 +
			    10 + tolower(*p) - 'a';
		}
	}

#ifndef NDEBUG
	if (fetchDebug) {
		io->total += io->chunksize;
		if (io->chunksize == 0)
			fprintf(stderr, "%s(): end of last chunk\n", __func__);
		else
			fprintf(stderr, "%s(): new chunk: %lu (%lu)\n",
			    __func__, (unsigned long)io->chunksize,
			    (unsigned long)io->total);
	}
#endif

	return (io->chunksize);
}

/*
 * Grow the input buffer to at least len bytes
 */
static inline int
http_growbuf(struct httpio *io, size_t len)
{
	char *tmp;

	if (io->bufsize >= len)
		return (0);

	if ((tmp = realloc(io->buf, len)) == NULL)
		return (-1);
	io->buf = tmp;
	io->bufsize = len;
	return (0);
}

/*
 * Fill the input buffer, do chunk decoding on the fly
 */
static int
http_fillbuf(struct httpio *io, size_t len)
{
	if (io->error)
		return (-1);
	if (io->eof)
		return (0);

	if (io->chunked == 0) {
		if (http_growbuf(io, len) == -1)
			return (-1);
		if ((io->buflen = fetch_read(io->conn, io->buf, len)) == -1) {
			io->error = 1;
			return (-1);
		}
		io->bufpos = 0;
		return (io->buflen);
	}

	if (io->chunksize == 0) {
		switch (http_new_chunk(io)) {
		case -1:
			io->error = 1;
			return (-1);
		case 0:
			io->eof = 1;
			return (0);
		}
	}

	if (len > io->chunksize)
		len = io->chunksize;
	if (http_growbuf(io, len) == -1)
		return (-1);
	if ((io->buflen = fetch_read(io->conn, io->buf, len)) == -1) {
		io->error = 1;
		return (-1);
	}
	io->chunksize -= io->buflen;

	if (io->chunksize == 0) {
		char endl[2];

		if (fetch_read(io->conn, endl, 2) != 2 ||
		    endl[0] != '\r' || endl[1] != '\n')
			return (-1);
	}

	io->bufpos = 0;

	return (io->buflen);
}

/*
 * Read function
 */
static int
http_readfn(void *v, char *buf, int len)
{
	struct httpio *io = (struct httpio *)v;
	int l, pos;

	if (io->error)
		return (-1);
	if (io->eof)
		return (0);

	for (pos = 0; len > 0; pos += l, len -= l) {
		/* empty buffer */
		if (!io->buf || io->bufpos == io->buflen)
			if (http_fillbuf(io, len) < 1)
				break;
		l = io->buflen - io->bufpos;
		if (len < l)
			l = len;
		bcopy(io->buf + io->bufpos, buf + pos, l);
		io->bufpos += l;
	}

	if (!pos && io->error)
		return (-1);
	return (pos);
}

/*
 * Write function
 */
static int
http_writefn(void *v, const char *buf, int len)
{
	struct httpio *io = (struct httpio *)v;

	return (fetch_write(io->conn, buf, len));
}

/*
 * Close function
 */
static int
http_closefn(void *v)
{
	struct httpio *io = (struct httpio *)v;
	int r;

	r = fetch_close(io->conn);
	if (io->buf)
		free(io->buf);
	free(io);
	return (r);
}

/*
 * Wrap a file descriptor up
 */
static FILE *
http_funopen(conn_t *conn, int chunked)
{
	struct httpio *io;
	FILE *f;

	if ((io = calloc(1, sizeof(*io))) == NULL) {
		fetch_syserr();
		return (NULL);
	}
	io->conn = conn;
	io->chunked = chunked;
	f = funopen(io, http_readfn, http_writefn, NULL, http_closefn);
	if (f == NULL) {
		fetch_syserr();
		free(io);
		return (NULL);
	}
	return (f);
}


/*****************************************************************************
 * Helper functions for talking to the server and parsing its replies
 */

/* Header types */
typedef enum {
	hdr_syserror = -2,
	hdr_error = -1,
	hdr_end = 0,
	hdr_unknown = 1,
	hdr_content_length,
	hdr_content_range,
	hdr_last_modified,
	hdr_location,
	hdr_transfer_encoding,
	hdr_www_authenticate
} hdr_t;

/* Names of interesting headers */
static struct {
	hdr_t		 num;
	const char	*name;
} hdr_names[] = {
	{ hdr_content_length,		"Content-Length" },
	{ hdr_content_range,		"Content-Range" },
	{ hdr_last_modified,		"Last-Modified" },
	{ hdr_location,			"Location" },
	{ hdr_transfer_encoding,	"Transfer-Encoding" },
	{ hdr_www_authenticate,		"WWW-Authenticate" },
	{ hdr_unknown,			NULL },
};

/*
 * Send a formatted line; optionally echo to terminal
 */
static int
http_cmd(conn_t *conn, const char *fmt, ...)
{
	va_list ap;
	size_t len;
	char *msg;
	int r;

	va_start(ap, fmt);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (msg == NULL) {
		errno = ENOMEM;
		fetch_syserr();
		return (-1);
	}

	r = fetch_putln(conn, msg, len);
	free(msg);

	if (r == -1) {
		fetch_syserr();
		return (-1);
	}

	return (0);
}

/*
 * Get and parse status line
 */
static int
http_get_reply(conn_t *conn)
{
	char *p;

	if (fetch_getln(conn) == -1)
		return (-1);
	/*
	 * A valid status line looks like "HTTP/m.n xyz reason" where m
	 * and n are the major and minor protocol version numbers and xyz
	 * is the reply code.
	 * Unfortunately, there are servers out there (NCSA 1.5.1, to name
	 * just one) that do not send a version number, so we can't rely
	 * on finding one, but if we do, insist on it being 1.0 or 1.1.
	 * We don't care about the reason phrase.
	 */
	if (strncmp(conn->buf, "HTTP", 4) != 0)
		return (HTTP_PROTOCOL_ERROR);
	p = conn->buf + 4;
	if (*p == '/') {
		if (p[1] != '1' || p[2] != '.' || (p[3] != '0' && p[3] != '1'))
			return (HTTP_PROTOCOL_ERROR);
		p += 4;
	}
	if (*p != ' ' ||
	    !isdigit((unsigned char)p[1]) ||
	    !isdigit((unsigned char)p[2]) ||
	    !isdigit((unsigned char)p[3]))
		return (HTTP_PROTOCOL_ERROR);

	conn->err = (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
	return (conn->err);
}

/*
 * Check a header; if the type matches the given string, return a pointer
 * to the beginning of the value.
 */
static const char *
http_match(const char *str, const char *hdr)
{
	while (*str && *hdr && tolower(*str++) == tolower(*hdr++))
		/* nothing */;
	if (*str || *hdr != ':')
		return (NULL);
	while (*hdr && isspace((unsigned char)*++hdr))
		/* nothing */;
	return (hdr);
}

/*
 * Get the next header and return the appropriate symbolic code.
 */
static hdr_t
http_next_header(conn_t *conn, const char **p)
{
	int i;

	if (fetch_getln(conn) == -1)
		return (hdr_syserror);
	while (conn->buflen && isspace((unsigned char)conn->buf[conn->buflen - 1]))
		conn->buflen--;
	conn->buf[conn->buflen] = '\0';
	if (conn->buflen == 0)
		return (hdr_end);
	/*
	 * We could check for malformed headers but we don't really care.
	 * A valid header starts with a token immediately followed by a
	 * colon; a token is any sequence of non-control, non-whitespace
	 * characters except "()<>@,;:\\\"{}".
	 */
	for (i = 0; hdr_names[i].num != hdr_unknown; i++)
		if ((*p = http_match(hdr_names[i].name, conn->buf)) != NULL)
			return (hdr_names[i].num);
	return (hdr_unknown);
}

/*
 * Parse a last-modified header
 */
static int
http_parse_mtime(const char *p, time_t *mtime)
{
	char locale[64], *r;
	struct tm tm;

	strncpy(locale, setlocale(LC_TIME, NULL), sizeof(locale));
	setlocale(LC_TIME, "C");
	r = strptime(p, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	/* XXX should add support for date-2 and date-3 */
	setlocale(LC_TIME, locale);
	if (r == NULL)
		return (-1);
	DEBUG(fprintf(stderr, "last modified: [%04d-%02d-%02d "
		  "%02d:%02d:%02d]\n",
		  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec));
	*mtime = timegm(&tm);
	return (0);
}

/*
 * Parse a content-length header
 */
static int
http_parse_length(const char *p, off_t *length)
{
	off_t len;

	for (len = 0; *p && isdigit((unsigned char)*p); ++p)
		len = len * 10 + (*p - '0');
	if (*p)
		return (-1);
	DEBUG(fprintf(stderr, "content length: [%lld]\n",
	    (long long)len));
	*length = len;
	return (0);
}

/*
 * Parse a content-range header
 */
static int
http_parse_range(const char *p, off_t *offset, off_t *length, off_t *size)
{
	off_t first, last, len;

	if (strncasecmp(p, "bytes ", 6) != 0)
		return (-1);
	p += 6;
	if (*p == '*') {
		first = last = -1;
		++p;
	} else {
		for (first = 0; *p && isdigit((unsigned char)*p); ++p)
			first = first * 10 + *p - '0';
		if (*p != '-')
			return (-1);
		for (last = 0, ++p; *p && isdigit((unsigned char)*p); ++p)
			last = last * 10 + *p - '0';
	}
	if (first > last || *p != '/')
		return (-1);
	for (len = 0, ++p; *p && isdigit((unsigned char)*p); ++p)
		len = len * 10 + *p - '0';
	if (*p || len < last - first + 1)
		return (-1);
	if (first == -1) {
		DEBUG(fprintf(stderr, "content range: [*/%lld]\n",
		    (long long)len));
		*length = 0;
	} else {
		DEBUG(fprintf(stderr, "content range: [%lld-%lld/%lld]\n",
		    (long long)first, (long long)last, (long long)len));
		*length = last - first + 1;
	}
	*offset = first;
	*size = len;
	return (0);
}


/*****************************************************************************
 * Helper functions for authorization
 */

/*
 * Base64 encoding
 */
static char *
http_base64(const char *src)
{
	static const char base64[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789+/";
	char *str, *dst;
	size_t l;
	int t, r;

	l = strlen(src);
	if ((str = malloc(((l + 2) / 3) * 4 + 1)) == NULL)
		return (NULL);
	dst = str;
	r = 0;

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
	return (str);
}

/*
 * Encode username and password
 */
static int
http_basic_auth(conn_t *conn, const char *hdr, const char *usr, const char *pwd)
{
	char *upw, *auth;
	int r;

	DEBUG(fprintf(stderr, "usr: [%s]\n", usr));
	DEBUG(fprintf(stderr, "pwd: [%s]\n", pwd));
	if (asprintf(&upw, "%s:%s", usr, pwd) == -1)
		return (-1);
	auth = http_base64(upw);
	free(upw);
	if (auth == NULL)
		return (-1);
	r = http_cmd(conn, "%s: Basic %s", hdr, auth);
	free(auth);
	return (r);
}

/*
 * Send an authorization header
 */
static int
http_authorize(conn_t *conn, const char *hdr, const char *p)
{
	/* basic authorization */
	if (strncasecmp(p, "basic:", 6) == 0) {
		char *user, *pwd, *str;
		int r;

		/* skip realm */
		for (p += 6; *p && *p != ':'; ++p)
			/* nothing */ ;
		if (!*p || strchr(++p, ':') == NULL)
			return (-1);
		if ((str = strdup(p)) == NULL)
			return (-1); /* XXX */
		user = str;
		pwd = strchr(str, ':');
		*pwd++ = '\0';
		r = http_basic_auth(conn, hdr, user, pwd);
		free(str);
		return (r);
	}
	return (-1);
}


/*****************************************************************************
 * Helper functions for connecting to a server or proxy
 */

/*
 * Connect to the correct HTTP server or proxy.
 */
static conn_t *
http_connect(struct url *URL, struct url *purl, const char *flags)
{
	conn_t *conn;
	int verbose;
	int af, val;

#ifdef INET6
	af = AF_UNSPEC;
#else
	af = AF_INET;
#endif

	verbose = CHECK_FLAG('v');
	if (CHECK_FLAG('4'))
		af = AF_INET;
#ifdef INET6
	else if (CHECK_FLAG('6'))
		af = AF_INET6;
#endif

	if (purl && strcasecmp(URL->scheme, SCHEME_HTTPS) != 0) {
		URL = purl;
	} else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0) {
		/* can't talk http to an ftp server */
		/* XXX should set an error code */
		return (NULL);
	}

	if ((conn = fetch_connect(URL->host, URL->port, af, verbose)) == NULL)
		/* fetch_connect() has already set an error code */
		return (NULL);
	if (strcasecmp(URL->scheme, SCHEME_HTTPS) == 0 &&
	    fetch_ssl(conn, verbose) == -1) {
		fetch_close(conn);
		/* grrr */
		errno = EAUTH;
		fetch_syserr();
		return (NULL);
	}

	val = 1;
	setsockopt(conn->sd, IPPROTO_TCP, TCP_NOPUSH, &val, sizeof(val));

	return (conn);
}

static struct url *
http_get_proxy(struct url * url, const char *flags)
{
	struct url *purl;
	char *p;

	if (flags != NULL && strchr(flags, 'd') != NULL)
		return (NULL);
	if (fetch_no_proxy_match(url->host))
		return (NULL);
	if (((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy"))) &&
	    *p && (purl = fetchParseURL(p))) {
		if (!*purl->scheme)
			strcpy(purl->scheme, SCHEME_HTTP);
		if (!purl->port)
			purl->port = fetch_default_proxy_port(purl->scheme);
		if (strcasecmp(purl->scheme, SCHEME_HTTP) == 0)
			return (purl);
		fetchFreeURL(purl);
	}
	return (NULL);
}

static void
http_print_html(FILE *out, FILE *in)
{
	size_t len;
	char *line, *p, *q;
	int comment, tag;

	comment = tag = 0;
	while ((line = fgetln(in, &len)) != NULL) {
		while (len && isspace((unsigned char)line[len - 1]))
			--len;
		for (p = q = line; q < line + len; ++q) {
			if (comment && *q == '-') {
				if (q + 2 < line + len &&
				    strcmp(q, "-->") == 0) {
					tag = comment = 0;
					q += 2;
				}
			} else if (tag && !comment && *q == '>') {
				p = q + 1;
				tag = 0;
			} else if (!tag && *q == '<') {
				if (q > p)
					fwrite(p, q - p, 1, out);
				tag = 1;
				if (q + 3 < line + len &&
				    strcmp(q, "<!--") == 0) {
					comment = 1;
					q += 3;
				}
			}
		}
		if (!tag && q > p)
			fwrite(p, q - p, 1, out);
		fputc('\n', out);
	}
}


/*****************************************************************************
 * Core
 */

/*
 * Send a request and process the reply
 *
 * XXX This function is way too long, the do..while loop should be split
 * XXX off into a separate function.
 */
FILE *
http_request(struct url *URL, const char *op, struct url_stat *us,
    struct url *purl, const char *flags)
{
	conn_t *conn;
	struct url *url, *new;
	int chunked, direct, need_auth, noredirect, verbose;
	int e, i, n, val;
	off_t offset, clength, length, size;
	time_t mtime;
	const char *p;
	FILE *f;
	hdr_t h;
	char hbuf[MAXHOSTNAMELEN + 7], *host;

	direct = CHECK_FLAG('d');
	noredirect = CHECK_FLAG('A');
	verbose = CHECK_FLAG('v');

	if (direct && purl) {
		fetchFreeURL(purl);
		purl = NULL;
	}

	/* try the provided URL first */
	url = URL;

	/* if the A flag is set, we only get one try */
	n = noredirect ? 1 : MAX_REDIRECT;
	i = 0;

	e = HTTP_PROTOCOL_ERROR;
	need_auth = 0;
	do {
		new = NULL;
		chunked = 0;
		offset = 0;
		clength = -1;
		length = -1;
		size = -1;
		mtime = 0;

		/* check port */
		if (!url->port)
			url->port = fetch_default_port(url->scheme);

		/* were we redirected to an FTP URL? */
		if (purl == NULL && strcmp(url->scheme, SCHEME_FTP) == 0) {
			if (strcmp(op, "GET") == 0)
				return (ftp_request(url, "RETR", us, purl, flags));
			else if (strcmp(op, "HEAD") == 0)
				return (ftp_request(url, "STAT", us, purl, flags));
		}

		/* connect to server or proxy */
		if ((conn = http_connect(url, purl, flags)) == NULL)
			goto ouch;

		host = url->host;
#ifdef INET6
		if (strchr(url->host, ':')) {
			snprintf(hbuf, sizeof(hbuf), "[%s]", url->host);
			host = hbuf;
		}
#endif
		if (url->port != fetch_default_port(url->scheme)) {
			if (host != hbuf) {
				strcpy(hbuf, host);
				host = hbuf;
			}
			snprintf(hbuf + strlen(hbuf),
			    sizeof(hbuf) - strlen(hbuf), ":%d", url->port);
		}

		/* send request */
		if (verbose)
			fetch_info("requesting %s://%s%s",
			    url->scheme, host, url->doc);
		if (purl) {
			http_cmd(conn, "%s %s://%s%s HTTP/1.1",
			    op, url->scheme, host, url->doc);
		} else {
			http_cmd(conn, "%s %s HTTP/1.1",
			    op, url->doc);
		}

		/* virtual host */
		http_cmd(conn, "Host: %s", host);

		/* proxy authorization */
		if (purl) {
			if (*purl->user || *purl->pwd)
				http_basic_auth(conn, "Proxy-Authorization",
				    purl->user, purl->pwd);
			else if ((p = getenv("HTTP_PROXY_AUTH")) != NULL && *p != '\0')
				http_authorize(conn, "Proxy-Authorization", p);
		}

		/* server authorization */
		if (need_auth || *url->user || *url->pwd) {
			if (*url->user || *url->pwd)
				http_basic_auth(conn, "Authorization", url->user, url->pwd);
			else if ((p = getenv("HTTP_AUTH")) != NULL && *p != '\0')
				http_authorize(conn, "Authorization", p);
			else if (fetchAuthMethod && fetchAuthMethod(url) == 0) {
				http_basic_auth(conn, "Authorization", url->user, url->pwd);
			} else {
				http_seterr(HTTP_NEED_AUTH);
				goto ouch;
			}
		}

		/* other headers */
		if ((p = getenv("HTTP_REFERER")) != NULL && *p != '\0') {
			if (strcasecmp(p, "auto") == 0)
				http_cmd(conn, "Referer: %s://%s%s",
				    url->scheme, host, url->doc);
			else
				http_cmd(conn, "Referer: %s", p);
		}
		if ((p = getenv("HTTP_USER_AGENT")) != NULL && *p != '\0')
			http_cmd(conn, "User-Agent: %s", p);
		else
			http_cmd(conn, "User-Agent: %s " _LIBFETCH_VER, getprogname());
		if (url->offset > 0)
			http_cmd(conn, "Range: bytes=%lld-", (long long)url->offset);
		http_cmd(conn, "Connection: close");
		http_cmd(conn, "");

		/*
		 * Force the queued request to be dispatched.  Normally, one
		 * would do this with shutdown(2) but squid proxies can be
		 * configured to disallow such half-closed connections.  To
		 * be compatible with such configurations, fiddle with socket
		 * options to force the pending data to be written.
		 */
		val = 0;
		setsockopt(conn->sd, IPPROTO_TCP, TCP_NOPUSH, &val,
			   sizeof(val));
		val = 1;
		setsockopt(conn->sd, IPPROTO_TCP, TCP_NODELAY, &val,
			   sizeof(val));

		/* get reply */
		switch (http_get_reply(conn)) {
		case HTTP_OK:
		case HTTP_PARTIAL:
			/* fine */
			break;
		case HTTP_MOVED_PERM:
		case HTTP_MOVED_TEMP:
		case HTTP_SEE_OTHER:
			/*
			 * Not so fine, but we still have to read the
			 * headers to get the new location.
			 */
			break;
		case HTTP_NEED_AUTH:
			if (need_auth) {
				/*
				 * We already sent out authorization code,
				 * so there's nothing more we can do.
				 */
				http_seterr(conn->err);
				goto ouch;
			}
			/* try again, but send the password this time */
			if (verbose)
				fetch_info("server requires authorization");
			break;
		case HTTP_NEED_PROXY_AUTH:
			/*
			 * If we're talking to a proxy, we already sent
			 * our proxy authorization code, so there's
			 * nothing more we can do.
			 */
			http_seterr(conn->err);
			goto ouch;
		case HTTP_BAD_RANGE:
			/*
			 * This can happen if we ask for 0 bytes because
			 * we already have the whole file.  Consider this
			 * a success for now, and check sizes later.
			 */
			break;
		case HTTP_PROTOCOL_ERROR:
			/* fall through */
		case -1:
			fetch_syserr();
			goto ouch;
		default:
			http_seterr(conn->err);
			if (!verbose)
				goto ouch;
			/* fall through so we can get the full error message */
		}

		/* get headers */
		do {
			switch ((h = http_next_header(conn, &p))) {
			case hdr_syserror:
				fetch_syserr();
				goto ouch;
			case hdr_error:
				http_seterr(HTTP_PROTOCOL_ERROR);
				goto ouch;
			case hdr_content_length:
				http_parse_length(p, &clength);
				break;
			case hdr_content_range:
				http_parse_range(p, &offset, &length, &size);
				break;
			case hdr_last_modified:
				http_parse_mtime(p, &mtime);
				break;
			case hdr_location:
				if (!HTTP_REDIRECT(conn->err))
					break;
				if (new)
					free(new);
				if (verbose)
					fetch_info("%d redirect to %s", conn->err, p);
				if (*p == '/')
					/* absolute path */
					new = fetchMakeURL(url->scheme, url->host, url->port, p,
					    url->user, url->pwd);
				else
					new = fetchParseURL(p);
				if (new == NULL) {
					/* XXX should set an error code */
					DEBUG(fprintf(stderr, "failed to parse new URL\n"));
					goto ouch;
				}
				if (!*new->user && !*new->pwd) {
					strcpy(new->user, url->user);
					strcpy(new->pwd, url->pwd);
				}
				new->offset = url->offset;
				new->length = url->length;
				break;
			case hdr_transfer_encoding:
				/* XXX weak test*/
				chunked = (strcasecmp(p, "chunked") == 0);
				break;
			case hdr_www_authenticate:
				if (conn->err != HTTP_NEED_AUTH)
					break;
				/* if we were smarter, we'd check the method and realm */
				break;
			case hdr_end:
				/* fall through */
			case hdr_unknown:
				/* ignore */
				break;
			}
		} while (h > hdr_end);

		/* we need to provide authentication */
		if (conn->err == HTTP_NEED_AUTH) {
			e = conn->err;
			need_auth = 1;
			fetch_close(conn);
			conn = NULL;
			continue;
		}

		/* requested range not satisfiable */
		if (conn->err == HTTP_BAD_RANGE) {
			if (url->offset == size && url->length == 0) {
				/* asked for 0 bytes; fake it */
				offset = url->offset;
				conn->err = HTTP_OK;
				break;
			} else {
				http_seterr(conn->err);
				goto ouch;
			}
		}

		/* we have a hit or an error */
		if (conn->err == HTTP_OK || conn->err == HTTP_PARTIAL || HTTP_ERROR(conn->err))
			break;

		/* all other cases: we got a redirect */
		e = conn->err;
		need_auth = 0;
		fetch_close(conn);
		conn = NULL;
		if (!new) {
			DEBUG(fprintf(stderr, "redirect with no new location\n"));
			break;
		}
		if (url != URL)
			fetchFreeURL(url);
		url = new;
	} while (++i < n);

	/* we failed, or ran out of retries */
	if (conn == NULL) {
		http_seterr(e);
		goto ouch;
	}

	DEBUG(fprintf(stderr, "offset %lld, length %lld,"
		  " size %lld, clength %lld\n",
		  (long long)offset, (long long)length,
		  (long long)size, (long long)clength));

	/* check for inconsistencies */
	if (clength != -1 && length != -1 && clength != length) {
		http_seterr(HTTP_PROTOCOL_ERROR);
		goto ouch;
	}
	if (clength == -1)
		clength = length;
	if (clength != -1)
		length = offset + clength;
	if (length != -1 && size != -1 && length != size) {
		http_seterr(HTTP_PROTOCOL_ERROR);
		goto ouch;
	}
	if (size == -1)
		size = length;

	/* fill in stats */
	if (us) {
		us->size = size;
		us->atime = us->mtime = mtime;
	}

	/* too far? */
	if (URL->offset > 0 && offset > URL->offset) {
		http_seterr(HTTP_PROTOCOL_ERROR);
		goto ouch;
	}

	/* report back real offset and size */
	URL->offset = offset;
	URL->length = clength;

	/* wrap it up in a FILE */
	if ((f = http_funopen(conn, chunked)) == NULL) {
		fetch_syserr();
		goto ouch;
	}

	if (url != URL)
		fetchFreeURL(url);
	if (purl)
		fetchFreeURL(purl);

	if (HTTP_ERROR(conn->err)) {
		http_print_html(stderr, f);
		fclose(f);
		f = NULL;
	}

	return (f);

ouch:
	if (url != URL)
		fetchFreeURL(url);
	if (purl)
		fetchFreeURL(purl);
	if (conn != NULL)
		fetch_close(conn);
	return (NULL);
}


/*****************************************************************************
 * Entry points
 */

/*
 * Retrieve and stat a file by HTTP
 */
FILE *
fetchXGetHTTP(struct url *URL, struct url_stat *us, const char *flags)
{
	return (http_request(URL, "GET", us, http_get_proxy(URL, flags), flags));
}

/*
 * Retrieve a file by HTTP
 */
FILE *
fetchGetHTTP(struct url *URL, const char *flags)
{
	return (fetchXGetHTTP(URL, NULL, flags));
}

/*
 * Store a file by HTTP
 */
FILE *
fetchPutHTTP(struct url *URL __unused, const char *flags __unused)
{
	warnx("fetchPutHTTP(): not implemented");
	return (NULL);
}

/*
 * Get an HTTP document's metadata
 */
int
fetchStatHTTP(struct url *URL, struct url_stat *us, const char *flags)
{
	FILE *f;

	f = http_request(URL, "HEAD", us, http_get_proxy(URL, flags), flags);
	if (f == NULL)
		return (-1);
	fclose(f);
	return (0);
}

/*
 * List a directory
 */
struct url_ent *
fetchListHTTP(struct url *url __unused, const char *flags __unused)
{
	warnx("fetchListHTTP(): not implemented");
	return (NULL);
}
