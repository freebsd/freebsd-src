/*
 * Copyright (c) 1999-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: sfsasl.c,v 8.17.4.15 2001/07/11 17:37:07 gshapiro Exp $";
#endif /* ! lint */

#if SFIO
# include <sfio/stdio.h>
#endif /* SFIO */

#include <stdlib.h>
#include <sendmail.h>

#if SASL && SFIO
/*
**  SASL
*/

# include <sasl.h>
# include "sfsasl.h"

/* how to deallocate a buffer allocated by SASL */
#  define SASL_DEALLOC(b)	sm_free(b)

static ssize_t
sasl_read(f, buf, size, disc)
	Sfio_t *f;
	Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
{
	int len, result;
	static char *outbuf = NULL;
	static unsigned int outlen = 0;
	static unsigned int offset = 0;
	Sasldisc_t *sd = (Sasldisc_t *) disc;

	/*
	**  sasl_decode() may require more data than a single read() returns.
	**  Hence we have to put a loop around the decoding.
	**  This also requires that we may have to split up the returned
	**  data since it might be larger than the allowed size.
	**  Therefore we use a static pointer and return portions of it
	**  if necessary.
	*/

	while (outbuf == NULL && outlen == 0)
	{
		len = sfrd(f, buf, size, disc);
		if (len <= 0)
			return len;
		result = sasl_decode(sd->conn, buf, len, &outbuf, &outlen);
		if (result != SASL_OK)
		{
			outbuf = NULL;
			offset = 0;
			outlen = 0;
			return -1;
		}
	}

	if (outbuf != NULL)
	{
		if (outlen - offset > size)
		{
			/* return another part of the buffer */
			(void) memcpy(buf, outbuf + offset, (size_t) size);
			offset += size;
			result = size;
		}
		else
		{
			/* return the rest of the buffer */
			result = outlen - offset;
			(void) memcpy(buf, outbuf + offset, (size_t) result);
			SASL_DEALLOC(outbuf);
			outbuf = NULL;
			offset = 0;
			outlen = 0;
		}
	}
	else
	{
		/* be paranoid: outbuf == NULL but outlen != 0 */
		syserr("!sasl_read failure: outbuf == NULL but outlen != 0");
	}
	return result;
}

static ssize_t
sasl_write(f, buf, size, disc)
	Sfio_t *f;
	const Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
{
	int result;
	char *outbuf;
	unsigned int outlen;
	Sasldisc_t *sd = (Sasldisc_t *) disc;

	result = sasl_encode(sd->conn, buf, size, &outbuf, &outlen);

	if (result != SASL_OK)
		return -1;

	if (outbuf != NULL)
	{
		sfwr(f, outbuf, outlen, disc);
		SASL_DEALLOC(outbuf);
	}
	return size;
}

int
sfdcsasl(fin, fout, conn)
	Sfio_t *fin;
	Sfio_t *fout;
	sasl_conn_t *conn;
{
	Sasldisc_t *saslin, *saslout;

	if (conn == NULL)
	{
		/* no need to do anything */
		return 0;
	}

	saslin = (Sasldisc_t *) xalloc(sizeof(Sasldisc_t));
	saslout = (Sasldisc_t *) xalloc(sizeof(Sasldisc_t));
	saslin->disc.readf = sasl_read;
	saslin->disc.writef = sasl_write;
	saslin->disc.seekf = NULL;
	saslin->disc.exceptf = NULL;

	saslout->disc.readf = sasl_read;
	saslout->disc.writef = sasl_write;
	saslout->disc.seekf = NULL;
	saslout->disc.exceptf = NULL;

	saslin->conn = conn;
	saslout->conn = conn;

	if (sfdisc(fin, (Sfdisc_t *) saslin) != (Sfdisc_t *) saslin ||
	    sfdisc(fout, (Sfdisc_t *) saslout) != (Sfdisc_t *) saslout)
	{
		sm_free(saslin);
		sm_free(saslout);
		return -1;
	}
	return 0;
}
#endif /* SASL && SFIO */

#if STARTTLS && (SFIO || _FFR_TLS_TOREK)
/*
**  STARTTLS
*/

# include "sfsasl.h"
#  include <openssl/err.h>

# if SFIO
static ssize_t
tls_read(f, buf, size, disc)
	Sfio_t *f;
	Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
# else /* SFIO */
static int
tls_read(disc, buf, size)
	void *disc;
	char *buf;
	int size;
# endif /* SFIO */
{
	int r;
	Tlsdisc_t *sd;

	/* Cast back to correct type */
	sd = (Tlsdisc_t *) disc;

	r = SSL_read(sd->con, (char *) buf, size);
	if (r < 0 && LogLevel > 7)
	{
		char *err;

		err = NULL;
		switch (SSL_get_error(sd->con, r))
		{
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_WANT_WRITE:
				err = "write W BLOCK";
				break;
			case SSL_ERROR_WANT_READ:
				err = "write R BLOCK";
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				err = "write X BLOCK";
				break;
			case SSL_ERROR_ZERO_RETURN:
				break;
			case SSL_ERROR_SYSCALL:
				err = "syscall error";
/*
				get_last_socket_error());
*/
				break;
			case SSL_ERROR_SSL:
				err = "generic SSL error";
				break;
		}
		if (err != NULL)
			sm_syslog(LOG_WARNING, NOQID, "TLS: read error:  %s",
				  err);
	}
	return r;
}

# if SFIO
static ssize_t
tls_write(f, buf, size, disc)
	Sfio_t *f;
	const Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
# else /* SFIO */
static int
tls_write(disc, buf, size)
	void *disc;
	const char *buf;
	int size;
# endif /* SFIO */
{
	int r;
	Tlsdisc_t *sd;

	/* Cast back to correct type */
	sd = (Tlsdisc_t *) disc;

	r = SSL_write(sd->con, (char *)buf, size);
	if (r < 0 && LogLevel > 7)
	{
		char *err;

		err = NULL;
		switch (SSL_get_error(sd->con, r))
		{
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_WANT_WRITE:
				err = "write W BLOCK";
				break;
			case SSL_ERROR_WANT_READ:
				err = "write R BLOCK";
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				err = "write X BLOCK";
				break;
			case SSL_ERROR_ZERO_RETURN:
				break;
			case SSL_ERROR_SYSCALL:
				err = "syscall error";
/*
				get_last_socket_error());
*/
				break;
			case SSL_ERROR_SSL:
				err = "generic SSL error";
/*
				ERR_GET_REASON(ERR_peek_error()));
*/
				break;
		}
		if (err != NULL)
			sm_syslog(LOG_WARNING, NOQID, "TLS: write error:  %s",
				  err);
	}
	return r;
}

# if !SFIO
static int
tls_close(cookie)
	void *cookie;
{
	int retval = 0;
	Tlsdisc_t *tc;

	/* Cast back to correct type */
	tc = (Tlsdisc_t *)cookie;

	if (tc->fp != NULL)
	{
		retval = fclose(tc->fp);
		tc->fp = NULL;
	}

	sm_free(tc);
	return retval;
}
# endif /* !SFIO */

int
sfdctls(fin, fout, con)
# if SFIO
	Sfio_t *fin;
	Sfio_t *fout;
# else /* SFIO */
	FILE **fin;
	FILE **fout;
# endif /* SFIO */
	SSL *con;
{
	Tlsdisc_t *tlsin, *tlsout;
# if !SFIO
	FILE *fp;
# else /* !SFIO */
	int rfd, wfd;
# endif /* !SFIO */

	if (con == NULL)
		return 0;

	tlsin = (Tlsdisc_t *) xalloc(sizeof(Tlsdisc_t));
	tlsout = (Tlsdisc_t *) xalloc(sizeof(Tlsdisc_t));
# if SFIO
	tlsin->disc.readf = tls_read;
	tlsin->disc.writef = tls_write;
	tlsin->disc.seekf = NULL;
	tlsin->disc.exceptf = NULL;
	tlsin->con = con;

	tlsout->disc.readf = tls_read;
	tlsout->disc.writef = tls_write;
	tlsout->disc.seekf = NULL;
	tlsout->disc.exceptf = NULL;
	tlsout->con = con;

	rfd = fileno(fin);
	wfd = fileno(fout);
	if (rfd < 0 || wfd < 0 ||
	    SSL_set_rfd(con, rfd) <= 0 || SSL_set_wfd(con, wfd) <= 0)
	{
		sm_free(tlsin);
		sm_free(tlsout);
		return -1;
	}
	if (sfdisc(fin, (Sfdisc_t *) tlsin) != (Sfdisc_t *) tlsin ||
	    sfdisc(fout, (Sfdisc_t *) tlsout) != (Sfdisc_t *) tlsout)
	{
		sm_free(tlsin);
		sm_free(tlsout);
		return -1;
	}
# else /* SFIO */
	tlsin->fp = *fin;
	tlsin->con = con;
	fp = funopen(tlsin, tls_read, tls_write, NULL, tls_close);
	if (fp == NULL)
	{
		sm_free(tlsin);
		return -1;
	}
	*fin = fp;

	tlsout->fp = *fout;
	tlsout->con = con;
	fp = funopen(tlsout, tls_read, tls_write, NULL, tls_close);
	if (fp == NULL)
	{
		FILE *save;

		/* Hack: Don't close underlying fp */
		save = tlsin->fp;
		tlsin->fp = NULL;
		fclose(*fin);
		*fin = save;
		sm_free(tlsout);
		return -1;
	}
	*fout = fp;
	SSL_set_rfd(con, fileno(tlsin->fp));
	SSL_set_wfd(con, fileno(tlsout->fp));
# endif /* SFIO */
	return 0;
}
#endif /* STARTTLS && (SFIO || _FFR_TLS_TOREK) */
