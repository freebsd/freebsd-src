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
static void queueuiodata(midi_dbuf *dbuf, struct uio *buf, int len);
static void dequeuerawdata(midi_dbuf *dbuf, char *data, int len);
static void copyrawdata(midi_dbuf *dbuf, char *data, int len);
static void dequeueuiodata(midi_dbuf *dbuf, struct uio *buf, int len);

/*
 * Here are the functions to interact to the midi device drivers.
 * These are called from midi device driver functions under sys/i386/isa/snd.
 */

int
midibuf_init(midi_dbuf *dbuf)
{
	if (dbuf->buf != NULL)
		free(dbuf->buf, M_DEVBUF);
	dbuf->buf = malloc(MIDI_BUFFSIZE, M_DEVBUF, M_NOWAIT);
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
midibuf_seqwrite(midi_dbuf *dbuf, u_char* data, int len)
{
	int i, lwrt, lwritten;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lwritten = 0;
	/* Write down every single byte. */
	while (len > 0) {
		/* Find out the number of bytes to write. */
		lwrt = SPACE_AVAIL(dbuf);
		if (lwrt > len)
			lwrt = len;
		if (lwrt > 0) {
			/* We can write some now. Queue the data. */
			queuerawdata(dbuf, data, lwrt);

			lwritten += lwrt;
			len -= lwrt;
			data += lwrt;
		}

		/* Have we got still more data to write? */
		if (len > 0) {
			/* Yes, sleep until we have enough space. */
			i = tsleep((void *)&dbuf->tsleep_out, PRIBIO | PCATCH, "mbsqwt", 0);
			if (i == EINTR || i == ERESTART)
				return (-i);
		}
	}

	return (lwritten);
}

/* sndwrite calls this function to queue data. */
int
midibuf_uiowrite(midi_dbuf *dbuf, struct uio *buf, int len)
{
	int i, lwrt, lwritten;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lwritten = 0;
	/* Write down every single byte. */
	while (len > 0) {
		/* Find out the number of bytes to write. */
		lwrt = SPACE_AVAIL(dbuf);
		if (lwrt > len)
			lwrt = len;
		if (lwrt > 0) {
			/* We can write some now. Queue the data. */
			queueuiodata(dbuf, buf, lwrt);

			lwritten += lwrt;
			len -= lwrt;
		}

		/* Have we got still more data to write? */
		if (len > 0) {
			/* Yes, sleep until we have enough space. */
			i = tsleep(&dbuf->tsleep_out, PRIBIO | PCATCH, "mbuiwt", 0);
			if (i == EINTR || i == ERESTART)
				return (-i);
		}
	}

	return (lwritten);
}

int
midibuf_output_intr(midi_dbuf *dbuf, u_char *data, int len)
{
	int lrd;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	/* Have we got any data in the queue? */
	if ((lrd = DATA_AVAIL(dbuf)) == 0)
		return (0);

	/* Dequeue the data. */
	if (lrd > len)
		lrd = len;
	dequeuerawdata(dbuf, data, lrd);

	return (lrd);
}

int
midibuf_input_intr(midi_dbuf *dbuf, u_char *data, int len)
{
	int lwritten;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lwritten = 0;

	/* Have we got any data to write? */
	if (len == 0)
		return (0);
	/* Can we write now? */
	if (SPACE_AVAIL(dbuf) < len)
		return (-EAGAIN);

	/* We can write some now. Queue the data. */
	queuerawdata(dbuf, data, len);
	lwritten = len;

	/* Have we managed to write the whole data? */
	if (lwritten < len)
		printf("midibuf_input_intr: queue did not have enough space, discarded %d bytes out of %d bytes.\n", len - lwritten, len);

	return (lwritten);
}

/* The sequencer calls this function to dequeue data. */
int
midibuf_seqread(midi_dbuf *dbuf, u_char* data, int len)
{
	int i, lrd, lread;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lread = 0;
	/* Write down every single byte. */
	while (len > 0) {
		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* No, sleep until we have data ready to read. */
			i = tsleep(&dbuf->tsleep_in, PRIBIO | PCATCH, "mbsqrd", 0);
			if (i == EINTR || i == ERESTART)
				return (-i);
			if (i == EWOULDBLOCK)
				continue;
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Dequeue the data. */
			dequeuerawdata(dbuf, data, lrd);

			lread += lrd;
			len -= lrd;
			data += lrd;
		}
	}

	return (lread);
}

/* The sequencer calls this function to copy data without dequeueing. */
int
midibuf_seqcopy(midi_dbuf *dbuf, u_char* data, int len)
{
	int i, lrd, lread;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lread = 0;
	/* Write down every single byte. */
	while (len > 0) {
		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* No, sleep until we have data ready to read. */
			i = tsleep(&dbuf->tsleep_in, PRIBIO | PCATCH, "mbsqrd", 0);
			if (i == EINTR || i == ERESTART)
				return (-i);
			if (i == EWOULDBLOCK)
				continue;
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Copy the data. */
			copyrawdata(dbuf, data, lrd);

			lread += lrd;
			len -= lrd;
			data += lrd;
		}
	}

	return (lread);
}

/* sndread calls this function to dequeue data. */
int
midibuf_uioread(midi_dbuf *dbuf, struct uio *buf, int len)
{
	int i, lrd, lread;

	/* Is this a real queue? */
	if (dbuf == (midi_dbuf *)NULL)
		return (0);

	lread = 0;
	while (len > 0 && lread == 0) {
		/* Have we got data to read? */
		if ((lrd = DATA_AVAIL(dbuf)) == 0) {
			/* No, sleep until we have data ready to read. */
			i = tsleep(&dbuf->tsleep_in, PRIBIO | PCATCH, "mbuird", 0);
			if (i == EINTR || i == ERESTART)
				return (-i);
			if (i == EWOULDBLOCK)
				continue;
			/* Find out the number of bytes to read. */
			lrd = DATA_AVAIL(dbuf);
		}

		if (lrd > len)
			lrd = len;
		if (lrd > 0) {
			/* We can read some data now. Dequeue the data. */
			dequeueuiodata(dbuf, buf, lrd);

			lread += lrd;
			len -= lrd;
		}
	}

	return (lread);
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
		memcpy(dbuf->buf + dbuf->fp, data, dbuf->bufsize - dbuf->fp);
		memcpy(dbuf->buf, data + dbuf->bufsize - dbuf->fp, len - (dbuf->bufsize - dbuf->fp));
	} else
		/* The new data do not wrap, once is enough. */
		memcpy(dbuf->buf + dbuf->fp, data, len);

	/* Adjust the pointer and the length counters. */
	dbuf->fp = (dbuf->fp + len) % dbuf->bufsize;
	dbuf->fl -= len;
	dbuf->rl += len;

	/* Wake up the processes sleeping on input data. */
	wakeup(&dbuf->tsleep_in);
	if (dbuf->sel.si_pid && dbuf->rl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}

static void
queueuiodata(midi_dbuf *dbuf, struct uio *buf, int len)
{
	/* dbuf->fp might wrap around dbuf->bufsize. */
	if (dbuf->bufsize - dbuf->fp < len) {
		/* The new data wraps, copy them twice. */
		uiomove((caddr_t)(dbuf->buf + dbuf->fp), dbuf->bufsize - dbuf->fp, buf);
		uiomove((caddr_t)(dbuf->buf), len - (dbuf->bufsize - dbuf->fp), buf);
	} else
		/* The new data do not wrap, once is enough. */
		uiomove((caddr_t)(dbuf->buf + dbuf->fp), len, buf);

	/* Adjust the pointer and the length counters. */
	dbuf->fp = (dbuf->fp + len) % dbuf->bufsize;
	dbuf->fl -= len;
	dbuf->rl += len;

	/* Wake up the processes sleeping on queueing. */
	wakeup(&dbuf->tsleep_in);
	if (dbuf->sel.si_pid && dbuf->rl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}

static void
dequeuerawdata(midi_dbuf *dbuf, char *data, int len)
{
	/* Copy the data. */
	copyrawdata(dbuf, data, len);

	/* Adjust the pointer and the length counters. */
	dbuf->rp = (dbuf->rp + len) % dbuf->bufsize;
	dbuf->rl -= len;
	dbuf->fl += len;

	/* Wake up the processes sleeping on queueing. */
	wakeup(&dbuf->tsleep_out);
	if (dbuf->sel.si_pid && dbuf->fl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}

static void
copyrawdata(midi_dbuf *dbuf, char *data, int len)
{
	/* dbuf->rp might wrap around dbuf->bufsize. */
	if (dbuf->bufsize - dbuf->rp < len) {
		/* The data to be read wraps, copy them twice. */
		memcpy(data, dbuf->buf + dbuf->rp, dbuf->bufsize - dbuf->rp);
		memcpy(data + dbuf->bufsize - dbuf->rp, dbuf->buf, len - (dbuf->bufsize - dbuf->rp));
	} else
		/* The new data do not wrap, once is enough. */
		memcpy(data, dbuf->buf + dbuf->rp, len);
}

static void
dequeueuiodata(midi_dbuf *dbuf, struct uio *buf, int len)
{
	/* dbuf->rp might wrap around dbuf->bufsize. */
	if (dbuf->bufsize - dbuf->rp < len) {
		/* The new data wraps, copy them twice. */
		uiomove((caddr_t)(dbuf->buf + dbuf->rp), dbuf->bufsize - dbuf->rp, buf);
		uiomove((caddr_t)(dbuf->buf), len - (dbuf->bufsize - dbuf->rp), buf);
	} else
		/* The new data do not wrap, once is enough. */
		uiomove((caddr_t)(dbuf->buf + dbuf->rp), len, buf);

	/* Adjust the pointer and the length counters. */
	dbuf->rp = (dbuf->rp + len) % dbuf->bufsize;
	dbuf->rl -= len;
	dbuf->fl += len;

	/* Wake up the processes sleeping on queueing. */
	wakeup(&dbuf->tsleep_out);
	if (dbuf->sel.si_pid && dbuf->fl >= dbuf->blocksize)
		selwakeup(&dbuf->sel);
}
