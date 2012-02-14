/* zlib.c --- interface to the zlib compression library
   Ian Lance Taylor <ian@cygnus.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* The routines in this file are the interface between the CVS
   client/server support and the zlib compression library.  */

#include <assert.h>
#include "cvs.h"
#include "buffer.h"

#if defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT)

#include "zlib.h"

/* OS/2 doesn't have EIO.  FIXME: this whole notion of turning
   a different error into EIO strikes me as pretty dubious.  */
#if !defined (EIO)
#define EIO EBADPOS
#endif

/* The compression interface is built upon the buffer data structure.
   We provide a buffer type which compresses or decompresses the data
   which passes through it.  An input buffer decompresses the data
   read from an underlying buffer, and an output buffer compresses the
   data before writing it to an underlying buffer.  */

/* This structure is the closure field of the buffer.  */

struct compress_buffer
{
    /* The underlying buffer.  */
    struct buffer *buf;
    /* The compression information.  */
    z_stream zstr;
};

static void compress_error PROTO((int, int, z_stream *, const char *));
static int compress_buffer_input PROTO((void *, char *, int, int, int *));
static int compress_buffer_output PROTO((void *, const char *, int, int *));
static int compress_buffer_flush PROTO((void *));
static int compress_buffer_block PROTO((void *, int));
static int compress_buffer_shutdown_input PROTO((struct buffer *));
static int compress_buffer_shutdown_output PROTO((struct buffer *));

/* Report an error from one of the zlib functions.  */

static void
compress_error (status, zstatus, zstr, msg)
     int status;
     int zstatus;
     z_stream *zstr;
     const char *msg;
{
    int hold_errno;
    const char *zmsg;
    char buf[100];

    hold_errno = errno;

    zmsg = zstr->msg;
    if (zmsg == NULL)
    {
        sprintf (buf, "error %d", zstatus);
	zmsg = buf;
    }

    error (status,
	   zstatus == Z_ERRNO ? hold_errno : 0,
	   "%s: %s", msg, zmsg);
}

/* Create a compression buffer.  */

struct buffer *
compress_buffer_initialize (buf, input, level, memory)
     struct buffer *buf;
     int input;
     int level;
     void (*memory) PROTO((struct buffer *));
{
    struct compress_buffer *n;
    int zstatus;

    n = (struct compress_buffer *) xmalloc (sizeof *n);
    memset (n, 0, sizeof *n);

    n->buf = buf;

    if (input)
	zstatus = inflateInit (&n->zstr);
    else
	zstatus = deflateInit (&n->zstr, level);
    if (zstatus != Z_OK)
	compress_error (1, zstatus, &n->zstr, "compression initialization");

    /* There may already be data buffered on BUF.  For an output
       buffer, this is OK, because these routines will just use the
       buffer routines to append data to the (uncompressed) data
       already on BUF.  An input buffer expects to handle a single
       buffer_data of buffered input to be uncompressed, so that is OK
       provided there is only one buffer.  At present that is all
       there ever will be; if this changes, compress_buffer_input must
       be modified to handle multiple input buffers.  */
    assert (! input || buf->data == NULL || buf->data->next == NULL);

    return buf_initialize (input ? compress_buffer_input : NULL,
			   input ? NULL : compress_buffer_output,
			   input ? NULL : compress_buffer_flush,
			   compress_buffer_block,
			   (input
			    ? compress_buffer_shutdown_input
			    : compress_buffer_shutdown_output),
			   memory,
			   n);
}

/* Input data from a compression buffer.  */

static int
compress_buffer_input (closure, data, need, size, got)
     void *closure;
     char *data;
     int need;
     int size;
     int *got;
{
    struct compress_buffer *cb = (struct compress_buffer *) closure;
    struct buffer_data *bd;

    if (cb->buf->input == NULL)
	abort ();

    /* We use a single buffer_data structure to buffer up data which
       the z_stream structure won't use yet.  We can safely store this
       on cb->buf->data, because we never call the buffer routines on
       cb->buf; we only call the buffer input routine, since that
       gives us the semantics we want.  As noted in
       compress_buffer_initialize, the buffer_data structure may
       already exist, and hold data which was already read and
       buffered before the decompression began.  */
    bd = cb->buf->data;
    if (bd == NULL)
    {
	bd = ((struct buffer_data *) xmalloc (sizeof (struct buffer_data)));
	if (bd == NULL)
	    return -2;
	bd->text = (char *) xmalloc (BUFFER_DATA_SIZE);
	if (bd->text == NULL)
	{
	    free (bd);
	    return -2;
	}
	bd->bufp = bd->text;
	bd->size = 0;
	cb->buf->data = bd;
    }

    cb->zstr.avail_out = size;
    cb->zstr.next_out = (Bytef *) data;

    while (1)
    {
	int zstatus, sofar, status, nread;

	/* First try to inflate any data we already have buffered up.
	   This is useful even if we don't have any buffered data,
	   because there may be data buffered inside the z_stream
	   structure.  */

	cb->zstr.avail_in = bd->size;
	cb->zstr.next_in = (Bytef *) bd->bufp;

	do
	{
	    zstatus = inflate (&cb->zstr, Z_NO_FLUSH);
	    if (zstatus == Z_STREAM_END)
		break;
	    if (zstatus != Z_OK && zstatus != Z_BUF_ERROR)
	    {
		compress_error (0, zstatus, &cb->zstr, "inflate");
		return EIO;
	    }
	} while (cb->zstr.avail_in > 0
		 && cb->zstr.avail_out > 0);

	bd->size = cb->zstr.avail_in;
	bd->bufp = (char *) cb->zstr.next_in;

	if (zstatus == Z_STREAM_END)
	    return -1;

	/* If we have obtained NEED bytes, then return, unless NEED is
           zero and we haven't obtained anything at all.  If NEED is
           zero, we will keep reading from the underlying buffer until
           we either can't read anything, or we have managed to
           inflate at least one byte.  */
	sofar = size - cb->zstr.avail_out;
	if (sofar > 0 && sofar >= need)
	    break;

	/* All our buffered data should have been processed at this
           point.  */
	assert (bd->size == 0);

	/* This will work well in the server, because this call will
	   do an unblocked read and fetch all the available data.  In
	   the client, this will read a single byte from the stdio
	   stream, which will cause us to call inflate once per byte.
	   It would be more efficient if we could make a call which
	   would fetch all the available bytes, and at least one byte.  */

	status = (*cb->buf->input) (cb->buf->closure, bd->text,
				    need > 0 ? 1 : 0,
				    BUFFER_DATA_SIZE, &nread);
	if (status != 0)
	    return status;

	/* If we didn't read anything, then presumably the buffer is
           in nonblocking mode, and we should just get out now with
           whatever we've inflated.  */
	if (nread == 0)
	{
	    assert (need == 0);
	    break;
	}

	bd->bufp = bd->text;
	bd->size = nread;
    }

    *got = size - cb->zstr.avail_out;

    return 0;
}

/* Output data to a compression buffer.  */

static int
compress_buffer_output (closure, data, have, wrote)
     void *closure;
     const char *data;
     int have;
     int *wrote;
{
    struct compress_buffer *cb = (struct compress_buffer *) closure;

    cb->zstr.avail_in = have;
    cb->zstr.next_in = (unsigned char *) data;

    while (cb->zstr.avail_in > 0)
    {
	char buffer[BUFFER_DATA_SIZE];
	int zstatus;

	cb->zstr.avail_out = BUFFER_DATA_SIZE;
	cb->zstr.next_out = (unsigned char *) buffer;

	zstatus = deflate (&cb->zstr, Z_NO_FLUSH);
	if (zstatus != Z_OK)
	{
	    compress_error (0, zstatus, &cb->zstr, "deflate");
	    return EIO;
	}

	if (cb->zstr.avail_out != BUFFER_DATA_SIZE)
	    buf_output (cb->buf, buffer,
			BUFFER_DATA_SIZE - cb->zstr.avail_out);
    }

    *wrote = have;

    /* We will only be here because buf_send_output was called on the
       compression buffer.  That means that we should now call
       buf_send_output on the underlying buffer.  */
    return buf_send_output (cb->buf);
}

/* Flush a compression buffer.  */

static int
compress_buffer_flush (closure)
     void *closure;
{
    struct compress_buffer *cb = (struct compress_buffer *) closure;

    cb->zstr.avail_in = 0;
    cb->zstr.next_in = NULL;

    while (1)
    {
	char buffer[BUFFER_DATA_SIZE];
	int zstatus;

	cb->zstr.avail_out = BUFFER_DATA_SIZE;
	cb->zstr.next_out = (unsigned char *) buffer;

	zstatus = deflate (&cb->zstr, Z_SYNC_FLUSH);

	/* The deflate function will return Z_BUF_ERROR if it can't do
           anything, which in this case means that all data has been
           flushed.  */
	if (zstatus == Z_BUF_ERROR)
	    break;

	if (zstatus != Z_OK)
	{
	    compress_error (0, zstatus, &cb->zstr, "deflate flush");
	    return EIO;
	}

	if (cb->zstr.avail_out != BUFFER_DATA_SIZE)
	    buf_output (cb->buf, buffer,
			BUFFER_DATA_SIZE - cb->zstr.avail_out);

	/* If the deflate function did not fill the output buffer,
           then all data has been flushed.  */
	if (cb->zstr.avail_out > 0)
	    break;
    }

    /* Now flush the underlying buffer.  Note that if the original
       call to buf_flush passed 1 for the BLOCK argument, then the
       buffer will already have been set into blocking mode, so we
       should always pass 0 here.  */
    return buf_flush (cb->buf, 0);
}

/* The block routine for a compression buffer.  */

static int
compress_buffer_block (closure, block)
     void *closure;
     int block;
{
    struct compress_buffer *cb = (struct compress_buffer *) closure;

    if (block)
	return set_block (cb->buf);
    else
	return set_nonblock (cb->buf);
}

/* Shut down an input buffer.  */

static int
compress_buffer_shutdown_input (buf)
     struct buffer *buf;
{
    struct compress_buffer *cb = (struct compress_buffer *) buf->closure;
    int zstatus;

    /* Don't make any attempt to pick up trailing data since we are shutting
     * down.  If the client doesn't know we are shutting down, we might not
     * see the EOF we are expecting.
     */

    zstatus = inflateEnd (&cb->zstr);
    if (zstatus != Z_OK)
    {
	compress_error (0, zstatus, &cb->zstr, "inflateEnd");
	return EIO;
    }

    return buf_shutdown (cb->buf);
}

/* Shut down an output buffer.  */

static int
compress_buffer_shutdown_output (buf)
     struct buffer *buf;
{
    struct compress_buffer *cb = (struct compress_buffer *) buf->closure;
    int zstatus, status;

    do
    {
	char buffer[BUFFER_DATA_SIZE];

	cb->zstr.avail_out = BUFFER_DATA_SIZE;
	cb->zstr.next_out = (unsigned char *) buffer;

	zstatus = deflate (&cb->zstr, Z_FINISH);
	if (zstatus != Z_OK && zstatus != Z_STREAM_END)
	{
	    compress_error (0, zstatus, &cb->zstr, "deflate finish");
	    return EIO;
	}

	if (cb->zstr.avail_out != BUFFER_DATA_SIZE)
	    buf_output (cb->buf, buffer,
			BUFFER_DATA_SIZE - cb->zstr.avail_out);
    } while (zstatus != Z_STREAM_END);

    zstatus = deflateEnd (&cb->zstr);
    if (zstatus != Z_OK)
    {
	compress_error (0, zstatus, &cb->zstr, "deflateEnd");
	return EIO;
    }

    status = buf_flush (cb->buf, 1);
    if (status != 0)
	return status;

    return buf_shutdown (cb->buf);
}



/* Here is our librarified gzip implementation.  It is very minimal
   but attempts to be RFC1952 compliant.  */

/* GZIP ID byte values */
#define GZIP_ID1	31
#define GZIP_ID2	139

/* Compression methods */
#define GZIP_CDEFLATE	8

/* Flags */
#define GZIP_FTEXT	1
#define GZIP_FHCRC	2
#define GZIP_FEXTRA	4
#define GZIP_FNAME	8
#define GZIP_FCOMMENT	16

/* BUF should contain SIZE bytes of gzipped data (RFC1952/RFC1951).
   We are to uncompress the data and write the result to the file
   descriptor FD.  If something goes wrong, give a nonfatal error message
   mentioning FULLNAME as the name of the file for FD.  Return 1 if
   it is an error we can't recover from.  */

int
gunzip_and_write (fd, fullname, buf, size)
    int fd;
    char *fullname;
    unsigned char *buf;
    size_t size;
{
    size_t pos;
    z_stream zstr;
    int zstatus;
    unsigned char outbuf[32768];
    unsigned long crc;

    if (size < 10)
    {
	error (0, 0, "gzipped data too small - lacks complete header");
	return 1;
    }
    if (buf[0] != GZIP_ID1 || buf[1] != GZIP_ID2)
    {
	error (0, 0, "gzipped data does not start with gzip identification");
	return 1;
    }
    if (buf[2] != GZIP_CDEFLATE)
    {
	error (0, 0, "only the deflate compression method is supported");
	return 1;
    }

    /* Skip over the fixed header, and then skip any of the variable-length
       fields.  As we skip each field, we keep pos <= size. The checks
       on positions and lengths are really checks for malformed or 
       incomplete gzip data.  */
    pos = 10;
    if (buf[3] & GZIP_FEXTRA)
    {
	if (pos + 2 >= size) 
	{
	    error (0, 0, "%s lacks proper gzip XLEN field", fullname);
	    return 1;
	}
	pos += buf[pos] + (buf[pos + 1] << 8) + 2;
	if (pos > size) 
	{
	    error (0, 0, "%s lacks proper gzip \"extra field\"", fullname);
	    return 1;
	}

    }
    if (buf[3] & GZIP_FNAME)
    {
	unsigned char *p = memchr(buf + pos, '\0', size - pos);
	if (p == NULL)
	{
	    error (0, 0, "%s has bad gzip filename field", fullname);
	    return 1;
	}
	pos = p - buf + 1;
    }
    if (buf[3] & GZIP_FCOMMENT)
    {
	unsigned char *p = memchr(buf + pos, '\0', size - pos);
	if (p == NULL)
	{
	    error (0, 0, "%s has bad gzip comment field", fullname);
	    return 1;
	}
	pos = p - buf + 1;
    }
    if (buf[3] & GZIP_FHCRC)
    {
	pos += 2;
	if (pos > size) 
	{
	    error (0, 0, "%s has bad gzip CRC16 field", fullname);
	    return 1;
	}
    }

    /* There could be no data to decompress - check and short circuit.  */
    if (pos >= size)
    {
	error (0, 0, "gzip data incomplete for %s (no data)", fullname);
	return 1;
    }

    memset (&zstr, 0, sizeof zstr);
    /* Passing a negative argument tells zlib not to look for a zlib
       (RFC1950) header.  This is an undocumented feature; I suppose if
       we wanted to be anal we could synthesize a header instead,
       but why bother?  */
    zstatus = inflateInit2 (&zstr, -15);

    if (zstatus != Z_OK)
	compress_error (1, zstatus, &zstr, fullname);

    /* I don't see why we should have to include the 8 byte trailer in
       avail_in.  But I see that zlib/gzio.c does, and it seemed to fix
       a fairly rare bug in which we'd get a Z_BUF_ERROR for no obvious
       reason.  */
    zstr.avail_in = size - pos;
    zstr.next_in = buf + pos;

    crc = crc32 (0, NULL, 0);

    do
    {
	zstr.avail_out = sizeof (outbuf);
	zstr.next_out = outbuf;
	zstatus = inflate (&zstr, Z_NO_FLUSH);
	if (zstatus != Z_STREAM_END && zstatus != Z_OK)
	{
	    compress_error (0, zstatus, &zstr, fullname);
	    return 1;
	}
	if (write (fd, outbuf, sizeof (outbuf) - zstr.avail_out) < 0)
	{
	    error (0, errno, "writing decompressed file %s", fullname);
	    return 1;
	}
	crc = crc32 (crc, outbuf, sizeof (outbuf) - zstr.avail_out);
    } while (zstatus != Z_STREAM_END);
    zstatus = inflateEnd (&zstr);
    if (zstatus != Z_OK)
	compress_error (0, zstatus, &zstr, fullname);

    /* Check that there is still 8 trailer bytes remaining (CRC32
       and ISIZE).  Check total decomp. data, plus header len (pos)
       against input buffer total size.  */
    pos += zstr.total_in;
    if (size - pos != 8)
    {
	error (0, 0, "gzip data incomplete for %s (no trailer)", fullname);
	return 1;
    }

    if (crc != ((unsigned long)buf[pos]
		+ ((unsigned long)buf[pos + 1] << 8)
		+ ((unsigned long)buf[pos + 2] << 16)
		+ ((unsigned long)buf[pos + 3] << 24)))
    {
	error (0, 0, "CRC error uncompressing %s", fullname);
	return 1;
    }

    if (zstr.total_out != ((unsigned long)buf[pos + 4]
			   + ((unsigned long)buf[pos + 5] << 8)
			   + ((unsigned long)buf[pos + 6] << 16)
			   + ((unsigned long)buf[pos + 7] << 24)))
    {
	error (0, 0, "invalid length uncompressing %s", fullname);
	return 1;
    }

    return 0;
}

/* Read all of FD and put the gzipped data (RFC1952/RFC1951) into *BUF,
   replacing previous contents of *BUF.  *BUF is xmalloc'd and *SIZE is
   its allocated size.  Put the actual number of bytes of data in
   *LEN.  If something goes wrong, give a nonfatal error mentioning
   FULLNAME as the name of the file for FD, and return 1 if we can't
   recover from it).  LEVEL is the compression level (1-9).  */

int
read_and_gzip (fd, fullname, buf, size, len, level)
    int fd;
    const char *fullname;
    unsigned char **buf;
    size_t *size;
    size_t *len;
    int level;
{
    z_stream zstr;
    int zstatus;
    unsigned char inbuf[8192];
    int nread;
    unsigned long crc;

    if (*size < 1024)
    {
	unsigned char *newbuf;

	*size = 1024;
	newbuf = xrealloc (*buf, *size);
	if (newbuf == NULL)
	{
	    error (0, 0, "out of memory");
	    return 1;
	}
	*buf = newbuf;
    }
    (*buf)[0] = GZIP_ID1;
    (*buf)[1] = GZIP_ID2;
    (*buf)[2] = GZIP_CDEFLATE;
    (*buf)[3] = 0;
    (*buf)[4] = (*buf)[5] = (*buf)[6] = (*buf)[7] = 0;
    /* Could set this based on level, but why bother?  */
    (*buf)[8] = 0;
    (*buf)[9] = 255;

    memset (&zstr, 0, sizeof zstr);
    zstatus = deflateInit2 (&zstr, level, Z_DEFLATED, -15, 8,
			    Z_DEFAULT_STRATEGY);
    crc = crc32 (0, NULL, 0);
    if (zstatus != Z_OK)
    {
	compress_error (0, zstatus, &zstr, fullname);
	return 1;
    }
    
    /* Adjust for 10-byte output header (filled in above) */
    zstr.total_out = 10;
    zstr.avail_out = *size - 10;
    zstr.next_out = *buf + 10;

    while (1)
    {
	int finish = 0;

	nread = read (fd, inbuf, sizeof inbuf);
	if (nread < 0)
	{
	    error (0, errno, "cannot read %s", fullname);
	    return 1;
	}
	else if (nread == 0)
	    /* End of file.  */
	    finish = 1;
	crc = crc32 (crc, inbuf, nread);
	zstr.next_in = inbuf;
	zstr.avail_in = nread;

	do
	{
	    /* I don't see this documented anywhere, but deflate seems
	       to tend to dump core sometimes if we pass it Z_FINISH and
	       a small (e.g. 2147 byte) avail_out.  So we insist on at
	       least 4096 bytes (that is what zlib/gzio.c uses).  */

	    if (zstr.avail_out < 4096)
	    {
		unsigned char *newbuf;

		assert(zstr.avail_out + zstr.total_out == *size);
		assert(zstr.next_out == *buf + zstr.total_out);
		*size *= 2;
		newbuf = xrealloc (*buf, *size);
		if (newbuf == NULL)
		{
		    error (0, 0, "out of memory");
		    return 1;
		}
		*buf = newbuf;
		zstr.next_out = *buf + zstr.total_out;
		zstr.avail_out = *size - zstr.total_out;
		assert(zstr.avail_out + zstr.total_out == *size);
		assert(zstr.next_out == *buf + zstr.total_out);
	    }

	    zstatus = deflate (&zstr, finish ? Z_FINISH : 0);
	    if (zstatus == Z_STREAM_END)
		goto done;
	    else if (zstatus != Z_OK)
		compress_error (0, zstatus, &zstr, fullname);
	} while (zstr.avail_out == 0);
    }
 done:
    /* Need to add the CRC information (8 bytes)
       to the end of the gzip'd output.
       Ensure there is enough space in the output buffer
       to do so.  */
    if (zstr.avail_out < 8)
    {
	unsigned char *newbuf;

	assert(zstr.avail_out + zstr.total_out == *size);
	assert(zstr.next_out == *buf + zstr.total_out);
	*size += 8 - zstr.avail_out;
	newbuf = realloc (*buf, *size);
	if (newbuf == NULL)
	{
	    error (0, 0, "out of memory");
	    return 1;
	}
	*buf = newbuf;
	zstr.next_out = *buf + zstr.total_out;
	zstr.avail_out = *size - zstr.total_out;
	assert(zstr.avail_out + zstr.total_out == *size);
	assert(zstr.next_out == *buf + zstr.total_out);
    } 
    *zstr.next_out++ = (unsigned char)(crc & 0xff);
    *zstr.next_out++ = (unsigned char)((crc >> 8) & 0xff);
    *zstr.next_out++ = (unsigned char)((crc >> 16) & 0xff);
    *zstr.next_out++ = (unsigned char)((crc >> 24) & 0xff);

    *zstr.next_out++ = (unsigned char)(zstr.total_in & 0xff);
    *zstr.next_out++ = (unsigned char)((zstr.total_in >> 8) & 0xff);
    *zstr.next_out++ = (unsigned char)((zstr.total_in >> 16) & 0xff);
    *zstr.next_out++ = (unsigned char)((zstr.total_in >> 24) & 0xff);

    zstr.total_out += 8;
    zstr.avail_out -= 8;
    assert(zstr.avail_out + zstr.total_out == *size);
    assert(zstr.next_out == *buf + zstr.total_out);

    *len = zstr.total_out;

    zstatus = deflateEnd (&zstr);
    if (zstatus != Z_OK)
	compress_error (0, zstatus, &zstr, fullname);

    return 0;
}
#endif /* defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT) */
