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

#ifndef _COMMON_H_INCLUDED
#define _COMMON_H_INCLUDED

#define FTP_DEFAULT_PORT	21
#define HTTP_DEFAULT_PORT	80
#define FTP_DEFAULT_PROXY_PORT	21
#define HTTP_DEFAULT_PROXY_PORT	3128

#ifdef WITH_SSL
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/* Connection */
typedef struct fetchconn conn_t;
struct fetchconn {
	int		 sd;		/* socket descriptor */
	char		*buf;		/* buffer */
	size_t		 bufsize;	/* buffer size */
	size_t		 buflen;	/* length of buffer contents */
	int		 err;		/* last protocol reply code */
#ifdef WITH_SSL
	SSL		*ssl;		/* SSL handle */
	SSL_CTX		*ssl_ctx;	/* SSL context */
	X509		*ssl_cert;	/* server certificate */
	SSL_METHOD	*ssl_meth;	/* SSL method */
#endif
	int		 ref;		/* reference count */
};

/* Structure used for error message lists */
struct fetcherr {
	const int	 num;
	const int	 cat;
	const char	*string;
};

/* for _fetch_writev */
struct iovec;

void		 _fetch_seterr(struct fetcherr *, int);
void		 _fetch_syserr(void);
void		 _fetch_info(const char *, ...);
int		 _fetch_default_port(const char *);
int		 _fetch_default_proxy_port(const char *);
conn_t		*_fetch_connect(const char *, int, int, int);
conn_t		*_fetch_reopen(int);
conn_t		*_fetch_ref(conn_t *);
int		 _fetch_ssl(conn_t *, int);
ssize_t		 _fetch_read(conn_t *, char *, size_t);
int		 _fetch_getln(conn_t *);
ssize_t		 _fetch_write(conn_t *, const char *, size_t);
ssize_t		 _fetch_writev(conn_t *, struct iovec *, int);
int		 _fetch_putln(conn_t *, const char *, size_t);
int		 _fetch_close(conn_t *);
int		 _fetch_add_entry(struct url_ent **, int *, int *,
		     const char *, struct url_stat *);
int		 _fetch_netrc_auth(struct url *url);

#define _ftp_seterr(n)	 _fetch_seterr(_ftp_errlist, n)
#define _http_seterr(n)	 _fetch_seterr(_http_errlist, n)
#define _netdb_seterr(n) _fetch_seterr(_netdb_errlist, n)
#define _url_seterr(n)	 _fetch_seterr(_url_errlist, n)

#ifndef NDEBUG
#define DEBUG(x) do { if (fetchDebug) { x; } } while (0)
#else
#define DEBUG(x) do { } while (0)
#endif

/*
 * I don't really like exporting _http_request() and _ftp_request(),
 * but the HTTP and FTP code occasionally needs to cross-call
 * eachother, and this saves me from adding a lot of special-case code
 * to handle those cases.
 *
 * Note that _*_request() free purl, which is way ugly but saves us a
 * whole lot of trouble.
 */
FILE		*_http_request(struct url *, const char *,
		     struct url_stat *, struct url *, const char *);
FILE		*_ftp_request(struct url *, const char *,
		     struct url_stat *, struct url *, const char *);

/*
 * Check whether a particular flag is set
 */
#define CHECK_FLAG(x)	(flags && strchr(flags, (x)))

#endif
