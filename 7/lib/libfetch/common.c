/*-
 * Copyright (c) 1998-2004 Dag-Erling Coïdan Smørgrav
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

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
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
static struct fetcherr netdb_errlist[] = {
#ifdef EAI_NODATA
	{ EAI_NODATA,	FETCH_RESOLV,	"Host not found" },
#endif
	{ EAI_AGAIN,	FETCH_TEMP,	"Transient resolver failure" },
	{ EAI_FAIL,	FETCH_RESOLV,	"Non-recoverable resolver failure" },
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
fetch_finderr(struct fetcherr *p, int e)
{
	while (p->num != -1 && p->num != e)
		p++;
	return (p);
}

/*
 * Set error code
 */
void
fetch_seterr(struct fetcherr *p, int e)
{
	p = fetch_finderr(p, e);
	fetchLastErrCode = p->cat;
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", p->string);
}

/*
 * Set error code according to errno
 */
void
fetch_syserr(void)
{
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
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", strerror(errno));
}


/*
 * Emit status message
 */
void
fetch_info(const char *fmt, ...)
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
fetch_default_port(const char *scheme)
{
	struct servent *se;

	if ((se = getservbyname(scheme, "tcp")) != NULL)
		return (ntohs(se->s_port));
	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PORT);
	return (0);
}

/*
 * Return the default proxy port for a scheme
 */
int
fetch_default_proxy_port(const char *scheme)
{
	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PROXY_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PROXY_PORT);
	return (0);
}


/*
 * Create a connection for an existing descriptor.
 */
conn_t *
fetch_reopen(int sd)
{
	conn_t *conn;

	/* allocate and fill connection structure */
	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		return (NULL);
	conn->sd = sd;
	++conn->ref;
	return (conn);
}


/*
 * Bump a connection's reference count.
 */
conn_t *
fetch_ref(conn_t *conn)
{

	++conn->ref;
	return (conn);
}


/*
 * Bind a socket to a specific local address
 */
int
fetch_bind(int sd, int af, const char *addr)
{
	struct addrinfo hints, *res, *res0;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if ((err = getaddrinfo(addr, NULL, &hints, &res0)) != 0)
		return (-1);
	for (res = res0; res; res = res->ai_next)
		if (bind(sd, res->ai_addr, res->ai_addrlen) == 0)
			return (0);
	return (-1);
}


/*
 * Establish a TCP connection to the specified port on the specified host.
 */
conn_t *
fetch_connect(const char *host, int port, int af, int verbose)
{
	conn_t *conn;
	char pbuf[10];
	const char *bindaddr;
	struct addrinfo hints, *res, *res0;
	int sd, err;

	DEBUG(fprintf(stderr, "---> %s:%d\n", host, port));

	if (verbose)
		fetch_info("looking up %s", host);

	/* look up host name and set up socket address structure */
	snprintf(pbuf, sizeof(pbuf), "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if ((err = getaddrinfo(host, pbuf, &hints, &res0)) != 0) {
		netdb_seterr(err);
		return (NULL);
	}
	bindaddr = getenv("FETCH_BIND_ADDRESS");

	if (verbose)
		fetch_info("connecting to %s:%d", host, port);

	/* try to connect */
	for (sd = -1, res = res0; res; sd = -1, res = res->ai_next) {
		if ((sd = socket(res->ai_family, res->ai_socktype,
			 res->ai_protocol)) == -1)
			continue;
		if (bindaddr != NULL && *bindaddr != '\0' &&
		    fetch_bind(sd, res->ai_family, bindaddr) != 0) {
			fetch_info("failed to bind to '%s'", bindaddr);
			close(sd);
			continue;
		}
		if (connect(sd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close(sd);
	}
	freeaddrinfo(res0);
	if (sd == -1) {
		fetch_syserr();
		return (NULL);
	}

	if ((conn = fetch_reopen(sd)) == NULL) {
		fetch_syserr();
		close(sd);
	}
	return (conn);
}


/*
 * Enable SSL on a connection.
 */
int
fetch_ssl(conn_t *conn, int verbose)
{

#ifdef WITH_SSL
	/* Init the SSL library and context */
	if (!SSL_library_init()){
		fprintf(stderr, "SSL library init failed\n");
		return (-1);
	}

	SSL_load_error_strings();

	conn->ssl_meth = SSLv23_client_method();
	conn->ssl_ctx = SSL_CTX_new(conn->ssl_meth);
	SSL_CTX_set_mode(conn->ssl_ctx, SSL_MODE_AUTO_RETRY);

	conn->ssl = SSL_new(conn->ssl_ctx);
	if (conn->ssl == NULL){
		fprintf(stderr, "SSL context creation failed\n");
		return (-1);
	}
	SSL_set_fd(conn->ssl, conn->sd);
	if (SSL_connect(conn->ssl) == -1){
		ERR_print_errors_fp(stderr);
		return (-1);
	}

	if (verbose) {
		X509_NAME *name;
		char *str;

		fprintf(stderr, "SSL connection established using %s\n",
		    SSL_get_cipher(conn->ssl));
		conn->ssl_cert = SSL_get_peer_certificate(conn->ssl);
		name = X509_get_subject_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		printf("Certificate subject: %s\n", str);
		free(str);
		name = X509_get_issuer_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		printf("Certificate issuer: %s\n", str);
		free(str);
	}

	return (0);
#else
	(void)conn;
	(void)verbose;
	fprintf(stderr, "SSL support disabled\n");
	return (-1);
#endif
}


/*
 * Read a character from a connection w/ timeout
 */
ssize_t
fetch_read(conn_t *conn, char *buf, size_t len)
{
	struct timeval now, timeout, wait;
	fd_set readfds;
	ssize_t rlen, total;
	int r;

	if (fetchTimeout) {
		FD_ZERO(&readfds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	total = 0;
	while (len > 0) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &readfds)) {
			FD_SET(conn->sd, &readfds);
			gettimeofday(&now, NULL);
			wait.tv_sec = timeout.tv_sec - now.tv_sec;
			wait.tv_usec = timeout.tv_usec - now.tv_usec;
			if (wait.tv_usec < 0) {
				wait.tv_usec += 1000000;
				wait.tv_sec--;
			}
			if (wait.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
			r = select(conn->sd + 1, &readfds, NULL, NULL, &wait);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				fetch_syserr();
				return (-1);
			}
		}
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			rlen = SSL_read(conn->ssl, buf, len);
		else
#endif
			rlen = read(conn->sd, buf, len);
		if (rlen == 0)
			break;
		if (rlen < 0) {
			if (errno == EINTR && fetchRestartCalls)
				continue;
			return (-1);
		}
		len -= rlen;
		buf += rlen;
		total += rlen;
	}
	return (total);
}


/*
 * Read a line of text from a connection w/ timeout
 */
#define MIN_BUF_SIZE 1024

int
fetch_getln(conn_t *conn)
{
	char *tmp;
	size_t tmpsize;
	ssize_t len;
	char c;

	if (conn->buf == NULL) {
		if ((conn->buf = malloc(MIN_BUF_SIZE)) == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		conn->bufsize = MIN_BUF_SIZE;
	}

	conn->buf[0] = '\0';
	conn->buflen = 0;

	do {
		len = fetch_read(conn, &c, 1);
		if (len == -1)
			return (-1);
		if (len == 0)
			break;
		conn->buf[conn->buflen++] = c;
		if (conn->buflen == conn->bufsize) {
			tmp = conn->buf;
			tmpsize = conn->bufsize * 2 + 1;
			if ((tmp = realloc(tmp, tmpsize)) == NULL) {
				errno = ENOMEM;
				return (-1);
			}
			conn->buf = tmp;
			conn->bufsize = tmpsize;
		}
	} while (c != '\n');

	conn->buf[conn->buflen] = '\0';
	DEBUG(fprintf(stderr, "<<< %s", conn->buf));
	return (0);
}


/*
 * Write to a connection w/ timeout
 */
ssize_t
fetch_write(conn_t *conn, const char *buf, size_t len)
{
	struct iovec iov;

	iov.iov_base = __DECONST(char *, buf);
	iov.iov_len = len;
	return fetch_writev(conn, &iov, 1);
}

/*
 * Write a vector to a connection w/ timeout
 * Note: can modify the iovec.
 */
ssize_t
fetch_writev(conn_t *conn, struct iovec *iov, int iovcnt)
{
	struct timeval now, timeout, wait;
	fd_set writefds;
	ssize_t wlen, total;
	int r;

	if (fetchTimeout) {
		FD_ZERO(&writefds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	total = 0;
	while (iovcnt > 0) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &writefds)) {
			FD_SET(conn->sd, &writefds);
			gettimeofday(&now, NULL);
			wait.tv_sec = timeout.tv_sec - now.tv_sec;
			wait.tv_usec = timeout.tv_usec - now.tv_usec;
			if (wait.tv_usec < 0) {
				wait.tv_usec += 1000000;
				wait.tv_sec--;
			}
			if (wait.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
			r = select(conn->sd + 1, NULL, &writefds, NULL, &wait);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				return (-1);
			}
		}
		errno = 0;
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			wlen = SSL_write(conn->ssl,
			    iov->iov_base, iov->iov_len);
		else
#endif
			wlen = writev(conn->sd, iov, iovcnt);
		if (wlen == 0) {
			/* we consider a short write a failure */
			errno = EPIPE;
			fetch_syserr();
			return (-1);
		}
		if (wlen < 0) {
			if (errno == EINTR && fetchRestartCalls)
				continue;
			return (-1);
		}
		total += wlen;
		while (iovcnt > 0 && wlen >= (ssize_t)iov->iov_len) {
			wlen -= iov->iov_len;
			iov++;
			iovcnt--;
		}
		if (iovcnt > 0) {
			iov->iov_len -= wlen;
			iov->iov_base = __DECONST(char *, iov->iov_base) + wlen;
		}
	}
	return (total);
}


/*
 * Write a line of text to a connection w/ timeout
 */
int
fetch_putln(conn_t *conn, const char *str, size_t len)
{
	struct iovec iov[2];
	int ret;

	DEBUG(fprintf(stderr, ">>> %s\n", str));
	iov[0].iov_base = __DECONST(char *, str);
	iov[0].iov_len = len;
	iov[1].iov_base = __DECONST(char *, ENDL);
	iov[1].iov_len = sizeof(ENDL);
	if (len == 0)
		ret = fetch_writev(conn, &iov[1], 1);
	else
		ret = fetch_writev(conn, iov, 2);
	if (ret == -1)
		return (-1);
	return (0);
}


/*
 * Close connection
 */
int
fetch_close(conn_t *conn)
{
	int ret;

	if (--conn->ref > 0)
		return (0);
	ret = close(conn->sd);
	free(conn->buf);
	free(conn);
	return (ret);
}


/*** Directory-related utility functions *************************************/

int
fetch_add_entry(struct url_ent **p, int *size, int *len,
    const char *name, struct url_stat *us)
{
	struct url_ent *tmp;

	if (*p == NULL) {
		*size = 0;
		*len = 0;
	}

	if (*len >= *size - 1) {
		tmp = realloc(*p, (*size * 2 + 1) * sizeof(**p));
		if (tmp == NULL) {
			errno = ENOMEM;
			fetch_syserr();
			return (-1);
		}
		*size = (*size * 2 + 1);
		*p = tmp;
	}

	tmp = *p + *len;
	snprintf(tmp->name, PATH_MAX, "%s", name);
	memcpy(&tmp->stat, us, sizeof(*us));

	(*len)++;
	(++tmp)->name[0] = 0;

	return (0);
}


/*** Authentication-related utility functions ********************************/

static const char *
fetch_read_word(FILE *f)
{
	static char word[1024];

	if (fscanf(f, " %1023s ", word) != 1)
		return (NULL);
	return (word);
}

/*
 * Get authentication data for a URL from .netrc
 */
int
fetch_netrc_auth(struct url *url)
{
	char fn[PATH_MAX];
	const char *word;
	char *p;
	FILE *f;

	if ((p = getenv("NETRC")) != NULL) {
		if (snprintf(fn, sizeof(fn), "%s", p) >= (int)sizeof(fn)) {
			fetch_info("$NETRC specifies a file name "
			    "longer than PATH_MAX");
			return (-1);
		}
	} else {
		if ((p = getenv("HOME")) != NULL) {
			struct passwd *pwd;

			if ((pwd = getpwuid(getuid())) == NULL ||
			    (p = pwd->pw_dir) == NULL)
				return (-1);
		}
		if (snprintf(fn, sizeof(fn), "%s/.netrc", p) >= (int)sizeof(fn))
			return (-1);
	}

	if ((f = fopen(fn, "r")) == NULL)
		return (-1);
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "default") == 0) {
			DEBUG(fetch_info("Using default .netrc settings"));
			break;
		}
		if (strcmp(word, "machine") == 0 &&
		    (word = fetch_read_word(f)) != NULL &&
		    strcasecmp(word, url->host) == 0) {
			DEBUG(fetch_info("Using .netrc settings for %s", word));
			break;
		}
	}
	if (word == NULL)
		goto ferr;
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "login") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->user, sizeof(url->user),
				"%s", word) > (int)sizeof(url->user)) {
				fetch_info("login name in .netrc is too long");
				url->user[0] = '\0';
			}
		} else if (strcmp(word, "password") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->pwd, sizeof(url->pwd),
				"%s", word) > (int)sizeof(url->pwd)) {
				fetch_info("password in .netrc is too long");
				url->pwd[0] = '\0';
			}
		} else if (strcmp(word, "account") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			/* XXX not supported! */
		} else {
			break;
		}
	}
	fclose(f);
	return (0);
 ferr:
	fclose(f);
	return (-1);
}

/*
 * The no_proxy environment variable specifies a set of domains for
 * which the proxy should not be consulted; the contents is a comma-,
 * or space-separated list of domain names.  A single asterisk will
 * override all proxy variables and no transactions will be proxied
 * (for compatability with lynx and curl, see the discussion at
 * <http://curl.haxx.se/mail/archive_pre_oct_99/0009.html>).
 */
int
fetch_no_proxy_match(const char *host)
{
	const char *no_proxy, *p, *q;
	size_t h_len, d_len;

	if ((no_proxy = getenv("NO_PROXY")) == NULL &&
	    (no_proxy = getenv("no_proxy")) == NULL)
		return (0);

	/* asterisk matches any hostname */
	if (strcmp(no_proxy, "*") == 0)
		return (1);

	h_len = strlen(host);
	p = no_proxy;
	do {
		/* position p at the beginning of a domain suffix */
		while (*p == ',' || isspace((unsigned char)*p))
			p++;

		/* position q at the first separator character */
		for (q = p; *q; ++q)
			if (*q == ',' || isspace((unsigned char)*q))
				break;

		d_len = q - p;
		if (d_len > 0 && h_len >= d_len &&
		    strncasecmp(host + h_len - d_len,
			p, d_len) == 0) {
			/* domain name matches */
			return (1);
		}

		p = q + 1;
	} while (*q);

	return (0);
}
