/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: sfsasl.c,v 8.17.4.8 2000/09/14 00:14:13 ca Exp $";
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

static ssize_t
sasl_read(f, buf, size, disc)
	Sfio_t *f;
	Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
{
	int len, result;
	char *outbuf;
	unsigned int outlen;
	Sasldisc_t *sd = (Sasldisc_t *) disc;

	len = sfrd(f, buf, size, disc);

	if (len <= 0)
		return len;

	result = sasl_decode(sd->conn, buf, len, &outbuf, &outlen);

	if (result != SASL_OK)
	{
		/* eventually, we'll want an exception here */
		return -1;
	}

	if (outbuf != NULL)
	{
		(void)memcpy(buf, outbuf, outlen);
		free(outbuf);
	}
	return outlen;
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
	{
		/* eventually, we'll want an exception here */
		return -1;
	}

	if (outbuf != NULL)
	{
		sfwr(f, outbuf, outlen, disc);
		free(outbuf);
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

	if ((saslin = (Sasldisc_t *) malloc(sizeof(Sasldisc_t))) == NULL)
		return -1;
	if ((saslout = (Sasldisc_t *) malloc(sizeof(Sasldisc_t))) == NULL)
	{
		free(saslin);
		return -1;
	}

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
		free(saslin);
		free(saslout);
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

static ssize_t
# if SFIO
tls_read(f, buf, size, disc)
	Sfio_t *f;
	Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
# else /* SFIO */
tls_read(disc, buf, size)
	void *disc;
	void *buf;
	size_t size;
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

static ssize_t
# if SFIO
tls_write(f, buf, size, disc)
	Sfio_t *f;
	const Void_t *buf;
	size_t size;
	Sfdisc_t *disc;
# else /* SFIO */
tls_write(disc, buf, size)
	void *disc;
	const void *buf;
	size_t size;
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

	free(tc);
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

	if ((tlsin = (Tlsdisc_t *) malloc(sizeof(Tlsdisc_t))) == NULL)
		return -1;
	if ((tlsout = (Tlsdisc_t *) malloc(sizeof(Tlsdisc_t))) == NULL)
	{
		free(tlsin);
		return -1;
	}

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
		free(tlsin);
		free(tlsout);
		return -1;
	}
	if (sfdisc(fin, (Sfdisc_t *) tlsin) != (Sfdisc_t *) tlsin ||
	    sfdisc(fout, (Sfdisc_t *) tlsout) != (Sfdisc_t *) tlsout)
	{
		free(tlsin);
		free(tlsout);
		return -1;
	}
# else /* SFIO */
	tlsin->fp = *fin;
	tlsin->con = con;
	fp = funopen(tlsin, tls_read, tls_write, NULL, tls_close);
	if (fp == NULL)
	{
		free(tlsin);
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
		free(tlsout);
		return -1;
	}
	*fout = fp;
	SSL_set_rfd(con, fileno(tlsin->fp));
	SSL_set_wfd(con, fileno(tlsout->fp));
# endif /* SFIO */
	return 0;
}
#endif /* STARTTLS && (SFIO || _FFR_TLS_TOREK) */
