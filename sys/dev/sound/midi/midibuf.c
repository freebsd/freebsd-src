/*
 * Copyright (C) 1999 Seigo Tanimura
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * This file implements a midi event/message queue. A midi
 * event/message queue holds midi events and messages to
 * transmit to or received from a midi interface.
 */

#include <dev/sound/midi/midi.h>

/* Some macros to handle the queue. */
#define DATA_AVAIL(dbuf) ((dbuf)->rl)
#define SPACE_AVAIL(dbuf) ((dbuf)->fl)

static void queuerawdata(midi_dbuf *dbuf, char *data, int len);
static void dequeuerawdata(midi_dbuf *dbuf, char *data, int len);
static void copyrawdata(midi_dbuf *dbuf, int offset, char *data, int len);
static void deleterawdata(midi_dbuf *dbuf, int len);

/*
 * Here are the functions to interact to the midi device drivers.
 * These are called from midi device driver functions under sys/i386/isa/snd.
 */

int
midibuf_init(midi_dbuf *dbuf)
{
	if (dbuf->buf == NULL) {
		dbuf->buf = malloc(MIDI_BUFFSIZE, M_DEVBUF, M_WAITOK | M_ZERO);
		cv_init(&dbuf->cv_in, "midi queue in");
		cv_init(&dbuf->cv_out, "midi queue out");
	}

	return (midibuf_clear(dbuf));
}

int
midibuf_destroy(midi_dbuf *dbuf)
{
	if (dbuf->buf != NULL) {
		free(dbuf->buf, M_DEVBUF);
		cv_destroy(&dbuf->cv_in);
		cv_destroy(&dbuf->cv_out);
	}

	return (0);
}

int
midibuf_clear(midi_dbuf *dbuf)
{
	bzero(dbuf->buf, MIDI_BUFFSIZE);
	dbuf->bufsize = MIDI_BUFFSIZE;
	dbuf->rp = dbuf->fp = 0;
	dbuf->dl = 0;
	dbuf->rl = 0;
	dbuf->fl = dbuf->bufsize;
	dbuf->int_count = 0;
	dbuf->chan = 0;
	/*dbuf->unit_size = 1;*/ /* The drivers are responsible. */
	bzero(&dbuf->sel, sizeof(dbuf->sel));
	dbuf->total = 0;
	dbuf->prev_total = 0;
	dbuf->blocksize = dbuf->bufsize / 4;

	return (0);
}

/* The sequencer calls this function to queue data. */
int
midibuf_seqwrite(midi_dbuf *dbuf, u_char* data, int len, int *lenw, midi_callback_t *cb, void *d, int reason, struct mtx *m)
{
	int i, lwrt;

	if (m != NULL)
		mtx_assert(m, MA_OWNED);

	if (lenw == NULL)
		return (EINVAL);
	*lenw = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Write down every single byte. */
	while (len > 0) {
		/* Find out the number of bytes to write. */
		lwrt = SPACE_AVAIL(dbuf);
		if (lwrt > len)
			lwrt = len;
		if (lwrt > 0) {
			/* We can write some now. Queue the data. */
			queuerawdata(dbuf, data, lwrt);

			*lenw += lwrt;
			len -= lwrt;
			data += lwrt;
		}

		if (cb != NULL)
			(*cb)(d, reason);

		/* Have we got still more data to write? */
		if (len > 0) {
			/* Sleep until we have enough space. */
			i = cv_wait_sig(&dbuf->cv_out, m);
			if (i == EINTR || i == ERESTART)
				return (i);
		}
	}

	return (0);
}

int
midibuf_output_intr(midi_dbuf *dbuf, u_char *data, int len, int *leno)
{
	if (leno == NULL)
		return (EINVAL);
	*leno = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Have we got any data in the queue? */
	*leno = DATA_AVAIL(dbuf);
	if (*leno == 0)
		return (EAGAIN);

	/* Dequeue the data. */
	if (*leno > len)
		*leno = len;
	dequeuerawdata(dbuf, data, *leno);

	return (0);
}

int
midibuf_input_intr(midi_dbuf *dbuf, u_char *data, int len, int *leni)
{
	if (leni == NULL)
		return (EINVAL);
	*leni = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Have we got any data to write? */
	if (len == 0)
		return (0);
	/* Can we write now? */
	if (SPACE_AVAIL(dbuf) < len)
		return (EAGAIN);

	/* We can write some now. Queue the data. */
	queuerawdata(dbuf, data, len);
	*leni = len;

	return (0);
}

/* The sequencer calls this function to dequeue data. */
int
midibuf_seqread(midi_dbuf *dbuf, u_char* data, int len, int *lenr, midi_callback_t *cb, void *d, int reason, struct mtx *m)
{
	int i, lrd;

	if (m != NULL)
		mtx_assert(m, MA_OWNED);

	if (lenr == NULL)
		return (EINVAL);
	*lenr = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Write down every single byte. */
	while (len > 0) {
		if (cb != NULL)
			(*cb)(d, reason);

		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* Sleep until we have data ready to read. */
			i = cv_wait_sig(&dbuf->cv_in, m);
			if (i == EINTR || i == ERESTART)
				return (i);
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Dequeue the data. */
			dequeuerawdata(dbuf, data, lrd);

			*lenr += lrd;
			len -= lrd;
			data += lrd;
		}
	}

	return (0);
}

/* The sequencer calls this function to copy data without dequeueing. */
int
midibuf_seqcopy(midi_dbuf *dbuf, u_char* data, int len, int *lenc, midi_callback_t *cb, void *d, int reason, struct mtx *m)
{
	int i, lrd;

	if (m != NULL)
		mtx_assert(m, MA_OWNED);

	if (lenc == NULL)
		return (EINVAL);
	*lenc = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Write down every single byte. */
	while (len > 0) {
		if (cb != NULL)
			(*cb)(d, reason);

		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* Sleep until we have data ready to read. */
			i = cv_wait_sig(&dbuf->cv_in, m);
			if (i == EINTR || i == ERESTART)
				return (i);
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Copy the data. */
			copyrawdata(dbuf, *lenc, data, lrd);

			*lenc += lrd;
			len -= lrd;
			data += lrd;
		}
	}

	return (0);
}

/*
 * The sequencer calls this function to delete the data
 * that the sequencer has already read.
 */
int
midibuf_seqdelete(midi_dbuf *dbuf, int len, int *lenr, midi_callback_t *cb, void *d, int reason, struct mtx *m)
{
	int i, lrd;

	if (m != NULL)
		mtx_assert(m, MA_OWNED);

	if (lenr == NULL)
		return (EINVAL);
	*lenr = 0;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (EINVAL);

	/* Write down every single byte. */
	while (len > 0) {
		if (cb != NULL)
			(*cb)(d, reason);

		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* Sleep until we have data ready to read. */
			i = cv_wait_sig(&dbuf->cv_in, m);
			if (i == EINTR || i == ERESTART)
				return (i);
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Delete the data. */
			deleterawdata(dbuf, lrd);

			*lenr += lrd;
			len -= lrd;
		}
	}

	return (0);
}

/*
 * The functions below here are the libraries for the above ones.
 */

static void
queuerawdata(midi_dbuf *dbuf, char *data, int len)
{
	/* dbuf->fp might wrap around dbuf->bufsize. */
	if (dbuf->bufsize - dbuf->fp < len) {
		/* The new data wraps, copy them twice. */
		bcopy(data, dbuf->buf + dbuf->fp, dbuf->bufsize - dbuf->fp);
		bcopy(data + dbuf->bufsize - dbuf->fp, dbuf->buf, len - (dbuf->bufsize - dbuf->fp));
	} else
		/* The new data do not wrap, once is enough. */
		bcopy(data, dbuf->buf + dbuf->fp, len);

	/* Adjust the pointer and the length counters. */
	dbuf->fp = (dbuf->fp + len) % dbuf->bufsize;
	dbuf->fl -= len;
	dbuf->rl += len;

	/* Wake up the processes sleeping on input data. */
	cv_broadcast(&dbuf->cv_in);
	if (dbuf->sel.si_pid && dbuf->rl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}

static void
dequeuerawdata(midi_dbuf *dbuf, char *data, int len)
{
	/* Copy the data. */
	copyrawdata(dbuf, 0, data, len);

	/* Delete the data. */
	deleterawdata(dbuf, len);
}

static void
copyrawdata(midi_dbuf *dbuf, int offset, char *data, int len)
{
	int rp;

	rp = (dbuf->rp + offset) % dbuf->bufsize;

	/* dbuf->rp might wrap around dbuf->bufsize. */
	if (dbuf->bufsize - rp < len) {
		/* The data to be read wraps, copy them twice. */
		bcopy(dbuf->buf + rp, data, dbuf->bufsize - rp);
		bcopy(dbuf->buf, data + dbuf->bufsize - rp, len - (dbuf->bufsize - rp));
	} else
		/* The new data do not wrap, once is enough. */
		bcopy(dbuf->buf + rp, data, len);
}

static void
deleterawdata(midi_dbuf *dbuf, int len)
{
	/* Adjust the pointer and the length counters. */
	dbuf->rp = (dbuf->rp + len) % dbuf->bufsize;
	dbuf->rl -= len;
	dbuf->fl += len;

	/* Wake up the processes sleeping on queueing. */
	cv_broadcast(&dbuf->cv_out);
	if (dbuf->sel.si_pid && dbuf->fl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}
