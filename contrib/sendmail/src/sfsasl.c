/*
 * Copyright (c) 1999-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: sfsasl.c,v 8.86 2001/09/11 04:05:16 gshapiro Exp $")
#include <stdlib.h>
#include <sendmail.h>
#include <errno.h>
#if SASL
# include <sasl.h>
# include "sfsasl.h"

/* Structure used by the "sasl" file type */
struct sasl_obj
{
	SM_FILE_T *fp;
	sasl_conn_t *conn;
};

struct sasl_info
{
	SM_FILE_T *fp;
	sasl_conn_t *conn;
};

/*
**  SASL_GETINFO - returns requested information about a "sasl" file
**		  descriptor.
**
**	Parameters:
**		fp -- the file descriptor
**		what -- the type of information requested
**		valp -- the thang to return the information in
**
**	Returns:
**		-1 for unknown requests
**		>=0 on success with valp filled in (if possible).
*/

static int sasl_getinfo __P((SM_FILE_T *, int, void *));

static int
sasl_getinfo(fp, what, valp)
	SM_FILE_T *fp;
	int what;
	void *valp;
{
	struct sasl_obj *so = (struct sasl_obj *) fp->f_cookie;

	switch (what)
	{
	  case SM_IO_WHAT_FD:
		if (so->fp == NULL)
			return -1;
		return so->fp->f_file; /* for stdio fileno() compatability */

	  case SM_IO_IS_READABLE:
		if (so->fp == NULL)
			return 0;

		/* get info from underlying file */
		return sm_io_getinfo(so->fp, what, valp);

	  default:
		return -1;
	}
}

/*
**  SASL_OPEN -- creates the sasl specific information for opening a
**		file of the sasl type.
**
**	Parameters:
**		fp -- the file pointer associated with the new open
**		info -- contains the sasl connection information pointer and
**			the original SM_FILE_T that holds the open
**		flags -- ignored
**		rpool -- ignored
**
**	Returns:
**		0 on success
*/

static int sasl_open __P((SM_FILE_T *, const void *, int, const void *));

/* ARGSUSED2 */
static int
sasl_open(fp, info, flags, rpool)
	SM_FILE_T *fp;
	const void *info;
	int flags;
	const void *rpool;
{
	struct sasl_obj *so;
	struct sasl_info *si = (struct sasl_info *) info;

	so = (struct sasl_obj *) sm_malloc(sizeof(struct sasl_obj));
	so->fp = si->fp;
	so->conn = si->conn;

	/*
	**  The underlying 'fp' is set to SM_IO_NOW so that the entire
	**  encoded string is written in one chunk. Otherwise there is
	**  the possibility that it may appear illegal, bogus or
	**  mangled to the other side of the connection.
	**  We will read or write through 'fp' since it is the opaque
	**  connection for the communications. We need to treat it this
	**  way in case the encoded string is to be sent down a TLS
	**  connection rather than, say, sm_io's stdio.
	*/

	(void) sm_io_setvbuf(so->fp, SM_TIME_DEFAULT, NULL, SM_IO_NOW, 0);
	fp->f_cookie = so;
	return 0;
}

/*
**  SASL_CLOSE -- close the sasl specific parts of the sasl file pointer
**
**	Parameters:
**		fp -- the file pointer to close
**
**	Returns:
**		0 on success
*/

static int sasl_close __P((SM_FILE_T *));

static int
sasl_close(fp)
	SM_FILE_T *fp;
{
	struct sasl_obj *so;

	so = (struct sasl_obj *) fp->f_cookie;
	if (so->fp != NULL)
	{
		sm_io_close(so->fp, SM_TIME_DEFAULT);
		so->fp = NULL;
	}
	sm_free(so);
	so = NULL;
	return 0;
}

/* how to deallocate a buffer allocated by SASL */
extern void	sm_sasl_free __P((void *));
# define SASL_DEALLOC(b)	sm_sasl_free(b)

/*
**  SASL_READ -- read encrypted information and decrypt it for the caller
**
**	Parameters:
**		fp -- the file pointer
**		buf -- the location to place the decrypted information
**		size -- the number of bytes to read after decryption
**
**	Results:
**		-1 on error
**		otherwise the number of bytes read
*/

static ssize_t sasl_read __P((SM_FILE_T *, char *, size_t));

static ssize_t
sasl_read(fp, buf, size)
	SM_FILE_T *fp;
	char *buf;
	size_t size;
{
	int result;
	ssize_t len;
	static char *outbuf = NULL;
	static unsigned int outlen = 0;
	static unsigned int offset = 0;
	struct sasl_obj *so = (struct sasl_obj *) fp->f_cookie;

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
		len = sm_io_read(so->fp, SM_TIME_DEFAULT, buf, size);
		if (len <= 0)
			return len;
		result = sasl_decode(so->conn, buf,
				     (unsigned int) len, &outbuf, &outlen);
		if (result != SASL_OK)
		{
			outbuf = NULL;
			offset = 0;
			outlen = 0;
			return -1;
		}
	}

	if (outbuf == NULL)
	{
		/* be paranoid: outbuf == NULL but outlen != 0 */
		syserr("@sasl_read failure: outbuf == NULL but outlen != 0");
		/* NOTREACHED */
	}
	if (outlen - offset > size)
	{
		/* return another part of the buffer */
		(void) memcpy(buf, outbuf + offset, size);
		offset += size;
		len = size;
	}
	else
	{
		/* return the rest of the buffer */
		len = outlen - offset;
		(void) memcpy(buf, outbuf + offset, (size_t) len);
		SASL_DEALLOC(outbuf);
		outbuf = NULL;
		offset = 0;
		outlen = 0;
	}
	return len;
}

/*
**  SASL_WRITE -- write information out after encrypting it
**
**	Parameters:
**		fp -- the file pointer
**		buf -- holds the data to be encrypted and written
**		size -- the number of bytes to have encrypted and written
**
**	Returns:
**		-1 on error
**		otherwise number of bytes written
*/

static ssize_t sasl_write __P((SM_FILE_T *, const char *, size_t));

static ssize_t
sasl_write(fp, buf, size)
	SM_FILE_T *fp;
	const char *buf;
	size_t size;
{
	int result;
	char *outbuf;
	unsigned int outlen;
	size_t ret = 0, total = 0;
	struct sasl_obj *so = (struct sasl_obj *) fp->f_cookie;

	result = sasl_encode(so->conn, buf,
			     (unsigned int) size, &outbuf, &outlen);

	if (result != SASL_OK)
		return -1;

	if (outbuf != NULL)
	{
		while (outlen > 0)
		{
			ret = sm_io_write(so->fp, SM_TIME_DEFAULT,
					  &outbuf[total], outlen);
			outlen -= ret;
			total += ret;
		}
		SASL_DEALLOC(outbuf);
	}
	return size;
}

/*
**  SFDCSASL -- create sasl file type and open in and out file pointers
**	       for sendmail to read from and write to.
**
**	Parameters:
**		fin -- the sm_io file encrypted data to be read from
**		fout -- the sm_io file encrypted data to be writen to
**		conn -- the sasl connection pointer
**
**	Returns:
**		-1 on error
**		0 on success
**
**	Side effects:
**		The arguments "fin" and "fout" are replaced with the new
**		SM_FILE_T pointers.
*/

int
sfdcsasl(fin, fout, conn)
	SM_FILE_T **fin;
	SM_FILE_T **fout;
	sasl_conn_t *conn;
{
	SM_FILE_T *newin, *newout;
	SM_FILE_T  SM_IO_SET_TYPE(sasl_vector, "sasl", sasl_open, sasl_close,
		sasl_read, sasl_write, NULL, sasl_getinfo, NULL,
		SM_TIME_FOREVER);
	struct sasl_info info;

	if (conn == NULL)
	{
		/* no need to do anything */
		return 0;
	}

	SM_IO_INIT_TYPE(sasl_vector, "sasl", sasl_open, sasl_close,
		sasl_read, sasl_write, NULL, sasl_getinfo, NULL,
		SM_TIME_FOREVER);
	info.fp = *fin;
	info.conn = conn;
	newin = sm_io_open(&sasl_vector, SM_TIME_DEFAULT, &info, SM_IO_RDONLY,
			   NULL);

	if (newin == NULL)
		return -1;

	info.fp = *fout;
	info.conn = conn;
	newout = sm_io_open(&sasl_vector, SM_TIME_DEFAULT, &info, SM_IO_WRONLY,
			    NULL);

	if (newout == NULL)
	{
		(void) sm_io_close(newin, SM_TIME_DEFAULT);
		return -1;
	}
	sm_io_automode(newin, newout);

	*fin = newin;
	*fout = newout;
	return 0;
}
#endif /* SASL */

#if STARTTLS
# include "sfsasl.h"
#  include <openssl/err.h>

/* Structure used by the "tls" file type */
struct tls_obj
{
	SM_FILE_T *fp;
	SSL *con;
};

struct tls_info
{
	SM_FILE_T *fp;
	SSL *con;
};

/*
**  TLS_GETINFO - returns requested information about a "tls" file
**		 descriptor.
**
**	Parameters:
**		fp -- the file descriptor
**		what -- the type of information requested
**		valp -- the thang to return the information in (unused)
**
**	Returns:
**		-1 for unknown requests
**		>=0 on success with valp filled in (if possible).
*/

static int tls_getinfo __P((SM_FILE_T *, int, void *));

/* ARGSUSED2 */
static int
tls_getinfo(fp, what, valp)
	SM_FILE_T *fp;
	int what;
	void *valp;
{
	struct tls_obj *so = (struct tls_obj *) fp->f_cookie;

	switch (what)
	{
	  case SM_IO_WHAT_FD:
		if (so->fp == NULL)
			return -1;
		return so->fp->f_file; /* for stdio fileno() compatability */

	  case SM_IO_IS_READABLE:
		return SSL_pending(so->con) > 0;

	  default:
		return -1;
	}
}

/*
**  TLS_OPEN -- creates the tls specific information for opening a
**	       file of the tls type.
**
**	Parameters:
**		fp -- the file pointer associated with the new open
**		info -- the sm_io file pointer holding the open and the
**			TLS encryption connection to be read from or written to
**		flags -- ignored
**		rpool -- ignored
**
**	Returns:
**		0 on success
*/

static int tls_open __P((SM_FILE_T *, const void *, int, const void *));

/* ARGSUSED2 */
static int
tls_open(fp, info, flags, rpool)
	SM_FILE_T *fp;
	const void *info;
	int flags;
	const void *rpool;
{
	struct tls_obj *so;
	struct tls_info *ti = (struct tls_info *) info;

	so = (struct tls_obj *) sm_malloc(sizeof(struct tls_obj));
	so->fp = ti->fp;
	so->con = ti->con;

	/*
	**  We try to get the "raw" file descriptor that TLS uses to
	**  do the actual read/write with. This is to allow us control
	**  over the file descriptor being a blocking or non-blocking type.
	**  Under the covers TLS handles the change and this allows us
	**  to do timeouts with sm_io.
	*/

	fp->f_file = sm_io_getinfo(so->fp, SM_IO_WHAT_FD, NULL);
	(void) sm_io_setvbuf(so->fp, SM_TIME_DEFAULT, NULL, SM_IO_NOW, 0);
	fp->f_cookie = so;
	return 0;
}

/*
**  TLS_CLOSE -- close the tls specific parts of the tls file pointer
**
**	Parameters:
**		fp -- the file pointer to close
**
**	Returns:
**		0 on success
*/

static int tls_close __P((SM_FILE_T *));

static int
tls_close(fp)
	SM_FILE_T *fp;
{
	struct tls_obj *so;

	so = (struct tls_obj *) fp->f_cookie;
	if (so->fp != NULL)
	{
		sm_io_close(so->fp, SM_TIME_DEFAULT);
		so->fp = NULL;
	}
	sm_free(so);
	so = NULL;
	return 0;
}

/* maximum number of retries for TLS related I/O due to handshakes */
# define MAX_TLS_IOS	4

/*
**  TLS_READ -- read secured information for the caller
**
**	Parameters:
**		fp -- the file pointer
**		buf -- the location to place the data
**		size -- the number of bytes to read from connection
**
**	Results:
**		-1 on error
**		otherwise the number of bytes read
*/

static ssize_t tls_read __P((SM_FILE_T *, char *, size_t));

static ssize_t
tls_read(fp, buf, size)
	SM_FILE_T *fp;
	char *buf;
	size_t size;
{
	int r;
	static int again = MAX_TLS_IOS;
	struct tls_obj *so = (struct tls_obj *) fp->f_cookie;
	char *err;

	r = SSL_read(so->con, (char *) buf, size);

	if (r > 0)
	{
		again = MAX_TLS_IOS;
		return r;
	}

	err = NULL;
	switch (SSL_get_error(so->con, r))
	{
	  case SSL_ERROR_NONE:
	  case SSL_ERROR_ZERO_RETURN:
		again = MAX_TLS_IOS;
		break;
	  case SSL_ERROR_WANT_WRITE:
		if (--again <= 0)
			err = "read W BLOCK";
		else
			errno = EAGAIN;
		break;
	  case SSL_ERROR_WANT_READ:
		if (--again <= 0)
			err = "read R BLOCK";
		else
			errno = EAGAIN;
		break;
	  case SSL_ERROR_WANT_X509_LOOKUP:
		err = "write X BLOCK";
		break;
	  case SSL_ERROR_SYSCALL:
		if (r == 0 && errno == 0) /* out of protocol EOF found */
			break;
		err = "syscall error";
/*
		get_last_socket_error());
*/
		break;
	  case SSL_ERROR_SSL:
		err = "generic SSL error";
		if (LogLevel > 9)
			tlslogerr("read");
		break;
	}
	if (err != NULL)
	{
		again = MAX_TLS_IOS;
		if (errno == 0)
			errno = EIO;
		if (LogLevel > 7)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: read error=%s (%d)", err, r);
	}
	return r;
}

/*
**  TLS_WRITE -- write information out through secure connection
**
**	Parameters:
**		fp -- the file pointer
**		buf -- holds the data to be securely written
**		size -- the number of bytes to write
**
**	Returns:
**		-1 on error
**		otherwise number of bytes written
*/

static ssize_t tls_write __P((SM_FILE_T *, const char *, size_t));

static ssize_t
tls_write(fp, buf, size)
	SM_FILE_T *fp;
	const char *buf;
	size_t size;
{
	int r;
	static int again = MAX_TLS_IOS;
	struct tls_obj *so = (struct tls_obj *) fp->f_cookie;
	char *err;

	r = SSL_write(so->con, (char *) buf, size);

	if (r > 0)
	{
		again = MAX_TLS_IOS;
		return r;
	}
	err = NULL;
	switch (SSL_get_error(so->con, r))
	{
	  case SSL_ERROR_NONE:
	  case SSL_ERROR_ZERO_RETURN:
		again = MAX_TLS_IOS;
		break;
	  case SSL_ERROR_WANT_WRITE:
		if (--again <= 0)
			err = "write W BLOCK";
		else
			errno = EAGAIN;
		break;
	  case SSL_ERROR_WANT_READ:
		if (--again <= 0)
			err = "write R BLOCK";
		else
			errno = EAGAIN;
		break;
	  case SSL_ERROR_WANT_X509_LOOKUP:
		err = "write X BLOCK";
		break;
	  case SSL_ERROR_SYSCALL:
		if (r == 0 && errno == 0) /* out of protocol EOF found */
			break;
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
		if (LogLevel > 9)
			tlslogerr("write");
		break;
	}
	if (err != NULL)
	{
		again = MAX_TLS_IOS;
		if (errno == 0)
			errno = EIO;
		if (LogLevel > 7)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: write error=%s (%d)", err, r);
	}
	return r;
}

/*
**  SFDCTLS -- create tls file type and open in and out file pointers
**	      for sendmail to read from and write to.
**
**	Parameters:
**		fin -- data input source being replaced
**		fout -- data output source being replaced
**		conn -- the tls connection pointer
**
**	Returns:
**		-1 on error
**		0 on success
**
**	Side effects:
**		The arguments "fin" and "fout" are replaced with the new
**		SM_FILE_T pointers.
**		The original "fin" and "fout" are preserved in the tls file
**		type but are not actually used because of the design of TLS.
*/

int
sfdctls(fin, fout, con)
	SM_FILE_T **fin;
	SM_FILE_T **fout;
	SSL *con;
{
	SM_FILE_T *tlsin, *tlsout;
	SM_FILE_T SM_IO_SET_TYPE(tls_vector, "tls", tls_open, tls_close,
		tls_read, tls_write, NULL, tls_getinfo, NULL,
		SM_TIME_FOREVER);
	struct tls_info info;

	SM_ASSERT(con != NULL);

	SM_IO_INIT_TYPE(tls_vector, "tls", tls_open, tls_close,
		tls_read, tls_write, NULL, tls_getinfo, NULL,
		SM_TIME_FOREVER);
	info.fp = *fin;
	info.con = con;
	tlsin = sm_io_open(&tls_vector, SM_TIME_DEFAULT, &info, SM_IO_RDONLY,
			   NULL);
	if (tlsin == NULL)
		return -1;

	info.fp = *fout;
	tlsout = sm_io_open(&tls_vector, SM_TIME_DEFAULT, &info, SM_IO_WRONLY,
			    NULL);
	if (tlsout == NULL)
	{
		(void) sm_io_close(tlsin, SM_TIME_DEFAULT);
		return -1;
	}
	sm_io_automode(tlsin, tlsout);

	*fin = tlsin;
	*fout = tlsout;
	return 0;
}
#endif /* STARTTLS */
