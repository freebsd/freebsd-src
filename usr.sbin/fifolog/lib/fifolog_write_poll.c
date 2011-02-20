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
#if 0
#include <sys/uio.h>
#endif

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
	assert(f->ff->zs->next_out + f->ff->zs->avail_out == \
	    f->obuf + f->obufsize);
}

struct fifolog_writer *
fifolog_write_new(void)
{
	struct fifolog_writer *f;

	ALLOC_OBJ(f, FIFOLOG_WRITER_MAGIC);
	assert(f != NULL);
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
	if (f->obuf != NULL)
		free(f->obuf);
	free(f);
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
	i = fifolog_int_read(f->ff, o);
	if (i)
		return ("Read error, looking for seq");
	f->seq = be32dec(f->ff->recbuf);
	if (f->seq == 0) {
		/* Empty fifolog */
		f->seq = random();
	} else {
		f->recno = o + 1;
		f->seq++;
	}

	f->obufsize = f->ff->recsize;
	ALLOC(&f->obuf, f->obufsize);

	i = deflateInit(f->ff->zs, (int)f->compression);
	assert(i == Z_OK);

	f->flag |= FIFOLOG_FLG_RESTART;
	f->flag |= FIFOLOG_FLG_SYNC;
	f->ff->zs->next_out = f->obuf + 9;
	f->ff->zs->avail_out = f->obufsize - 9;

	time(&now);
	f->starttime = now;
	f->lastsync = now;
	f->lastwrite = now;

	fifolog_write_assert(f);
	return (NULL);
}

static int
fifolog_write_output(struct fifolog_writer *f, int fl, time_t now)
{
	long h, l = f->ff->zs->next_out - f->obuf;
	int i, w;

	h = 4;					/* seq */
	be32enc(f->obuf, f->seq);
	f->obuf[h] = f->flag;
	h += 1;					/* flag */
	if (f->flag & FIFOLOG_FLG_SYNC) {
		be32enc(f->obuf + h, now);
		h += 4;				/* timestamp */
	}

	assert(l <= (long)f->ff->recsize);
	assert(l >= h);
	if (l == h)
		return (0);


	if (h + l < (long)f->ff->recsize && fl == Z_NO_FLUSH) 
		return (0);

	w = f->ff->recsize - l;
	if (w >  255) {
		be32enc(f->obuf + f->ff->recsize - 4, w);
		f->obuf[4] |= FIFOLOG_FLG_4BYTE;
	} else if (w > 0) {
		f->obuf[f->ff->recsize - 1] = w;
		f->obuf[4] |= FIFOLOG_FLG_1BYTE;
	}

	f->cnt[FIFOLOG_PT_BYTES_POST] += w;

#ifdef DBG
fprintf(stderr, "W: fl=%d h=%ld l=%ld w=%d recno=%jd fx %02x\n",
    fl, h, l, w, f->recno, f->obuf[4]);
#endif

	i = pwrite(f->ff->fd, f->obuf, f->ff->recsize,
	    (f->recno + 1) * f->ff->recsize);
	assert(i == (int)f->ff->recsize);

	f->cnt[FIFOLOG_PT_WRITES]++;

	f->lastwrite = now;
	f->seq++;
	f->recno++;
#ifdef DBG
if (f->flag)
fprintf(stderr, "SYNC- %d\n", __LINE__);
#endif
	f->flag = 0;

	memset(f->obuf, 0, f->obufsize);
	f->ff->zs->next_out = f->obuf + 5;
	f->ff->zs->avail_out = f->obufsize - 5;
	return (1);
}

static void
fifolog_write_gzip(struct fifolog_writer *f, const void *p, int len, time_t now, int fin)
{
	int i, fl;

	f->cnt[FIFOLOG_PT_BYTES_PRE] += len;

	if (fin == 0)
		fl = Z_NO_FLUSH;
	else if (f->cleanup || now >= (int)(f->lastsync + f->syncrate)) {
		f->cleanup = 0;
		fl = Z_FINISH;
		f->cnt[FIFOLOG_PT_SYNC]++;
	} else if (now >= (int)(f->lastwrite + f->writerate)) {
		fl = Z_SYNC_FLUSH;
		f->cnt[FIFOLOG_PT_FLUSH]++;
	} else if (p == NULL)
		return;
	else
		fl = Z_NO_FLUSH;

	f->ff->zs->avail_in = len;
	f->ff->zs->next_in = (void*)(uintptr_t)p;
#ifdef DBG
if (fl != Z_NO_FLUSH)
fprintf(stderr, "Z len %3d fin %d now %ld fl %d ai %u ao %u\n",
    len, fin, now, fl,
    f->ff->zs->avail_in,
    f->ff->zs->avail_out);
#endif

	while (1) {
		i = deflate(f->ff->zs, fl);

#ifdef DBG
if (i || f->ff->zs->avail_in)
fprintf(stderr, "fl = %d, i = %d ai = %u ao = %u fx=%02x\n", fl, i, 
    f->ff->zs->avail_in,
    f->ff->zs->avail_out, f->flag);
#endif

		assert(i == Z_OK || i == Z_BUF_ERROR || i == Z_STREAM_END);
		assert(f->ff->zs->avail_in == 0);

		if (!fifolog_write_output(f, fl, now))
			break;
	}
	assert(f->ff->zs->avail_in == 0);
	if (fl == Z_FINISH) {
		f->flag |= FIFOLOG_FLG_SYNC;
		f->ff->zs->next_out = f->obuf + 9;
		f->ff->zs->avail_out = f->obufsize - 9;
		f->lastsync = now;
#ifdef DBG
fprintf(stderr, "SYNC %d\n", __LINE__);
#endif
		assert(Z_OK == deflateReset(f->ff->zs));
	}
}

int
fifolog_write_poll(struct fifolog_writer *f, time_t now)
{
	if (now == 0)
		time(&now);
	fifolog_write_gzip(f, NULL, 0, now, 1);
	return (0);
}

/*
 * Attempt to write an entry.
 * Return zero if there is no space, one otherwise
 */

int
fifolog_write_bytes(struct fifolog_writer *f, uint32_t id, time_t now, const void *ptr, unsigned len)
{
	const unsigned char *p;
	uint8_t buf[4];

	fifolog_write_assert(f);
	assert(!(id & (FIFOLOG_TIMESTAMP|FIFOLOG_LENGTH)));
	assert(ptr != NULL);

	p = ptr;
	if (len == 0) {
		len = strlen(ptr) + 1;
	} else {
		assert(len <= 255);
		id |= FIFOLOG_LENGTH;
	}

	/* Now do timestamp, if needed */
	if (now == 0)
		time(&now);

	if (now != f->last) {
		id |= FIFOLOG_TIMESTAMP;
		f->last = now;
	} 

	/* Emit instance+flag */
	be32enc(buf, id);
	fifolog_write_gzip(f, buf, 4, now, 0);

	if (id & FIFOLOG_TIMESTAMP) {
		be32enc(buf, (uint32_t)f->last);
		fifolog_write_gzip(f, buf, 4, now, 0);
	}
	if (id & FIFOLOG_LENGTH) {
		buf[0] = (u_char)len;
		fifolog_write_gzip(f, buf, 1, now, 0);
	}

	assert (len > 0);
#if 1
	if (len > f->ibufsize) {
		free(f->ibuf);
		f->ibufsize = len;
		ALLOC(&f->ibuf, f->ibufsize);
	}
	memcpy(f->ibuf, p, len);
	fifolog_write_gzip(f, f->ibuf, len, now, 1);
#else
	fifolog_write_gzip(f, p, len, now, 1);
#endif
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
			(void)usleep(10000);
		}
	} else {
		p = ptr;
		for (p = ptr; len > 0; len -= l, p += l) {
			l = len;
			if (l > 255)
				l = 255;
			while (!fifolog_write_bytes(f, id, now, p, l)) {
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
