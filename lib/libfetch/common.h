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

/* Structure used for error message lists */
struct fetcherr {  
    const int num, cat;
    const char *string;
};

void		 _fetch_seterr(struct fetcherr *p, int e);
void		 _fetch_syserr(void);
void		 _fetch_info(char *fmt, ...);
int		 _fetch_connect(char *host, int port, int af, int verbose);
int		 _fetch_getln(int fd, char **buf, size_t *size, size_t *len);
int		 _fetch_putln(int fd, char *str, size_t len);
int		 _fetch_add_entry(struct url_ent **p, int *size, int *len,
				  char *name, struct url_stat *stat);

#define _ftp_seterr(n)	 _fetch_seterr(_ftp_errlist, n)
#define _http_seterr(n)	 _fetch_seterr(_http_errlist, n)
#define _netdb_seterr(n) _fetch_seterr(_netdb_errlist, n)
#define _url_seterr(n)	 _fetch_seterr(_url_errlist, n)

#ifndef NDEBUG
#define DEBUG(x) do x; while (0)
#else
#define DEBUG(x) do { } while (0)
#endif

/*
 * I don't really like exporting _http_request() from http.c, but ftp.c
 * occasionally needs to use an HTTP proxy, and this saves me from adding
 * a lot of special-case code to http.c to handle those cases.
 *
 * Note that _http_request() frees purl, which is way ugly but saves us a
 * whole lot of trouble.
 */
FILE		*_http_request(struct url *URL, char *op, struct url_stat *us,
			       struct url *purl, char *flags);

/*
 * Check whether a particular flag is set
 */
#define CHECK_FLAG(x)	(flags && strchr(flags, (x)))

#endif
