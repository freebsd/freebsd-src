/*-
 * Copyright (c) 2005-2008 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/endian.h>

#include <zlib.h>

#include "fifolog.h"
#include "libfifolog.h"
#include "libfifolog_int.h"
#include "fifolog_write.h"
#include "miniobj.h"

#define ALLOC(ptr, size) do {                   \
	(*(ptr)) = calloc(size, 1);             \
	assert(*(ptr) != NULL);                 \
} while (0)


const char *fifolog_write_statnames[] = {
[FIFOLOG_PT_BYTES_PRE] =	"Bytes before compression",
[FIFOLOG_PT_BYTES_POST] =	"Bytes after compression",
[FIFOLOG_PT_WRITES] =		"Writes",
[FIFOLOG_PT_FLUSH] =		"Flushes",
[FIFOLOG_PT_SYNC] =		"Syncs",
[FIFOLOG_PT_RUNTIME] =		"Runtime"
};

/*
 * Check that everything is all right
 */
static void
fifolog_write_assert(const struct fifolog_writer *f)
{

	CHECK_OBJ_NOTNULL(f, FIFOLOG_WRITER_MAGIC);
	assert(f->iptr == f->ff->zs->next_in + f->ff->zs->avail_in);
	assert(f->ff->zs->next_out + f->ff->zs->avail_out == \
	    f->ff->recbuf + f->ff->recsize);
}

struct fifolog_writer *
fifolog_write_new(void)
{
	struct fifolog_writer *f;

	ALLOC(&f, sizeof *f);
	f->magic = FIFOLOG_WRITER_MAGIC;
	return (f);
}

void
fifolog_write_destroy(struct fifolog_writer *f)
{
	CHECK_OBJ_NOTNULL(f, FIFOLOG_WRITER_MAGIC);
	free(f);
}

void
fifolog_write_close(struct fifolog_writer *f)
{

	CHECK_OBJ_NOTNULL(f, FIFOLOG_WRITER_MAGIC);
	fifolog_int_close(&f->ff);
	free(f->ff);
	if (f->ibuf != NULL)
		free(f->ibuf);
	free(f);
}

static void
fifo_prepobuf(struct fifolog_writer *f, time_t now, int flag)
{

	memset(f->ff->recbuf, 0, f->ff->recsize);
	f->ff->zs->next_out = f->ff->recbuf + 5;
	f->ff->zs->avail_out = f->ff->recsize - 5;
	if (f->recno == 0 && f->seq == 0) {
		srandomdev();
		do {
			f->seq = random();
		} while (f->seq == 0);
	}
	be32enc(f->ff->recbuf, f->seq++);
	f->ff->recbuf[4] = f->flag;
	f->flag = 0;
	if (flag) {
		f->ff->recbuf[4] |= FIFOLOG_FLG_SYNC;
		be32enc(f->ff->recbuf + 5, (u_int)now);
		f->ff->zs->next_out += 4;
		f->ff->zs->avail_out -= 4;
	}
	fifolog_write_assert(f);
}

const char *
fifolog_write_open(struct fifolog_writer *f, const char *fn, unsigned writerate, unsigned syncrate, int compression)
{
	const char *es;
	int i;
	time_t now;
	off_t o;

	CHECK_OBJ_NOTNULL(f, FIFOLOG_WRITER_MAGIC);

	/* Check for legal compression value */
	if (compression < Z_DEFAULT_COMPRESSION ||
	    compression > Z_BEST_COMPRESSION)
		return ("Illegal compression value");

	f->writerate = writerate;
	f->syncrate = syncrate;
	f->compression = compression;

	/* Reset statistics */
	memset(f->cnt, 0, sizeof f->cnt);

	es = fifolog_int_open(&f->ff, fn, 1);
	if (es != NULL)
		return (es);
	es = fifolog_int_findend(f->ff, &o);
	if (es != NULL)
		return (es);
	if (o == 0) {
		f->seq = 0;
		f->recno = 0;
	} else {
		i = fifolog_int_read(f->ff, o);
		if (i)
			return ("Read error, looking for seq");
		f->seq = be32dec(f->ff->recbuf) + 1;
		f->recno = o + 1;
	}

	f->ibufsize = 32768;
	ALLOC(&f->ibuf, f->ibufsize);
	f->iptr = f->ibuf;
	f->ff->zs->next_in = f->iptr;
	i = deflateInit(f->ff->zs, (int)f->compression);
	assert(i == Z_OK);

	f->flag |= FIFOLOG_FLG_RESTART;

	time(&now);
	fifo_prepobuf(f, now, 1);
	f->starttime = now;

	fifolog_write_assert(f);
	return (NULL);
}

static void
fifo_writerec(struct fifolog_writer *f)
{
	int i;
	time_t t;

	fifolog_write_assert(f);
	f->writes_since_sync++;

	assert(f->recno < f->ff->logsize);
	f->cnt[FIFOLOG_PT_BYTES_POST] += f->ff->recsize - f->ff->zs->avail_out;
	if (f->ff->zs->avail_out == 0) {
		/* nothing */
	} else if (f->ff->zs->avail_out <= 255) {
		f->ff->recbuf[f->ff->recsize - 1] = 
		    (u_char)f->ff->zs->avail_out;
		f->ff->recbuf[4] |= FIFOLOG_FLG_1BYTE;
	} else {
		be32enc(f->ff->recbuf + f->ff->recsize - 4,
		    f->ff->zs->avail_out);
		f->ff->recbuf[4] |= FIFOLOG_FLG_4BYTE;
	}
	i = pwrite(f->ff->fd, f->ff->recbuf, f->ff->recsize,
		(f->recno + 1) * f->ff->recsize);
	assert (i == (int)f->ff->recsize);
	if (++f->recno == f->ff->logsize)
		f->recno = 0;
	f->cnt[FIFOLOG_PT_WRITES]++;
	time(&t);
	f->cnt[FIFOLOG_PT_RUNTIME] = t - f->starttime; /*lint !e776 */
	fifolog_write_assert(f);
}

int
fifolog_write_poll(struct fifolog_writer *f, time_t now)
{
	int i, fl, bo, bf;

	if (now == 0)
		time(&now);

	fifolog_write_assert(f);
	if (f->cleanup || now >= (int)(f->lastsync + f->syncrate)) {
		/*
		 * We always check the sync timer, otherwise a flood of data
		 * would not get any sync records at all
		 */
		f->cleanup = 0;
		fl = Z_FINISH;
		f->lastsync = now;
		f->lastwrite = now;
		f->cnt[FIFOLOG_PT_SYNC]++;
	} else if (f->ff->zs->avail_in == 0 &&
	    now >= (int)(f->lastwrite + f->writerate)) {
		/*
		 * We only check for writerate timeouts when the input 
		 * buffer is empty.  It would be silly to force a write if
		 * pending input could cause it to happen on its own.
		 */
		fl = Z_SYNC_FLUSH;
		f->lastwrite = now;
		f->cnt[FIFOLOG_PT_FLUSH]++;
	} else if (f->ff->zs->avail_in == 0)
		return (0);			/* nothing to do */
	else
		fl = Z_NO_FLUSH;

	for (;;) {
		assert(f->ff->zs->avail_out > 0);

		bf = f->ff->zs->avail_out;

		i = deflate(f->ff->zs, fl);
		assert (i == Z_OK || i == Z_BUF_ERROR || i == Z_STREAM_END);

		bo = f->ff->zs->avail_out;

		/* If we have output space and not in a hurry.. */
		if (bo > 0 && fl == Z_NO_FLUSH)
			break;
	
		/* Write output buffer, if anything in it */
		if (bo != bf)
			fifo_writerec(f);

		/* If the buffer were full, we need to check again */
		if (bo == 0) {
			fifo_prepobuf(f, now, 0);
			continue;
		}

		if (fl == Z_FINISH) {
			/* Make next record a SYNC record */
			fifo_prepobuf(f, now, 1);
			/* And reset the zlib engine */
			i = deflateReset(f->ff->zs);
			assert(i == Z_OK);
			f->writes_since_sync = 0;
		} else {
			fifo_prepobuf(f, now, 0);
		}
		break;
	}

	if (f->ff->zs->avail_in == 0) {
		/* Reset input buffer when empty */
		f->iptr = f->ibuf;
		f->ff->zs->next_in = f->iptr;
	}

	fifolog_write_assert(f);
	return (1);
}

static void
fifolog_acct(struct fifolog_writer *f, unsigned bytes)
{

	f->ff->zs->avail_in += bytes;
	f->iptr += bytes;
	f->cnt[FIFOLOG_PT_BYTES_PRE] += bytes;
}

/*
 * Attempt to write an entry.
 * Return zero if there is no space, one otherwise
 */

int
fifolog_write_bytes(struct fifolog_writer *f, uint32_t id, time_t now, const void *ptr, unsigned len)
{
	u_int l;
	const unsigned char *p;

	fifolog_write_assert(f);
	assert(!(id & (FIFOLOG_TIMESTAMP|FIFOLOG_LENGTH)));
	assert(ptr != NULL);

	p = ptr;
	if (len == 0) {
		len = strlen(ptr) + 1;
		l = 4 + len;		/* id */
	} else {
		assert(len <= 255);
		id |= FIFOLOG_LENGTH;
		l = 5 + len;		/* id + len */
	}

	l += 4; 		/* A timestamp may be necessary */

	/* Now do timestamp, if needed */
	if (now == 0)
		time(&now);

	assert(l < f->ibufsize);

	/* Return if there is not enough space */
	if (f->iptr + l > f->ibuf + f->ibufsize)
		return (0);

	if (now != f->last) {
		id |= FIFOLOG_TIMESTAMP;
		f->last = now;
	} 

	/* Emit instance+flag and length */
	be32enc(f->iptr, id);
	fifolog_acct(f, 4);

	if (id & FIFOLOG_TIMESTAMP) {
		be32enc(f->iptr, (uint32_t)f->last);
		fifolog_acct(f, 4);
	}
	if (id & FIFOLOG_LENGTH) {
		f->iptr[0] = (u_char)len;
		fifolog_acct(f, 1);
	}

	assert (len > 0);
	memcpy(f->iptr, p, len);
	fifolog_acct(f, len);
	fifolog_write_assert(f);
	return (1);
}

/*
 * Write an entry, polling until success.
 * Long binary entries are broken into 255 byte chunks.
 */

void
fifolog_write_bytes_poll(struct fifolog_writer *f, uint32_t id, time_t now, const void *ptr, unsigned len)
{
	u_int l;
	const unsigned char *p;

	fifolog_write_assert(f);

	assert(!(id & (FIFOLOG_TIMESTAMP|FIFOLOG_LENGTH)));
	assert(ptr != NULL);

	if (len == 0) {
		while (!fifolog_write_bytes(f, id, now, ptr, len)) {
			(void)fifolog_write_poll(f, now);
			(void)usleep(10000);
		}
	} else {
		p = ptr;
		for (p = ptr; len > 0; len -= l, p += l) {
			l = len;
			if (l > 255)
				l = 255;
			while (!fifolog_write_bytes(f, id, now, p, l)) {
				(void)fifolog_write_poll(f, now);
				(void)usleep(10000);
			}
		}
	}
	fifolog_write_assert(f);
}

int
fifolog_write_flush(struct fifolog_writer *f)
{
	int i;

	fifolog_write_assert(f);

	f->cleanup = 1;
	for (i = 0; fifolog_write_poll(f, 0); i = 1)
		continue;
	fifolog_write_assert(f);
	return (i);
}
