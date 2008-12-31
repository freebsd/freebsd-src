/*-
 * Copyright (c) 2003 Ian Dowse.  All rights reserved.
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
 * $FreeBSD: src/sys/kern/subr_msgbuf.c,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Generic message buffer support routines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/msgbuf.h>

/* Read/write sequence numbers are modulo a multiple of the buffer size. */
#define SEQMOD(size) ((size) * 16)

static u_int msgbuf_cksum(struct msgbuf *mbp);

/*
 * Initialize a message buffer of the specified size at the specified
 * location. This also zeros the buffer area.
 */
void
msgbuf_init(struct msgbuf *mbp, void *ptr, int size)
{

	mbp->msg_ptr = ptr;
	mbp->msg_size = size;
	mbp->msg_seqmod = SEQMOD(size);
	msgbuf_clear(mbp);
	mbp->msg_magic = MSG_MAGIC;
}

/*
 * Reinitialize a message buffer, retaining its previous contents if
 * the size and checksum are correct. If the old contents cannot be
 * recovered, the message buffer is cleared.
 */
void
msgbuf_reinit(struct msgbuf *mbp, void *ptr, int size)
{
	u_int cksum;

	if (mbp->msg_magic != MSG_MAGIC || mbp->msg_size != size) {
		msgbuf_init(mbp, ptr, size);
		return;
	}
	mbp->msg_seqmod = SEQMOD(size);
	mbp->msg_wseq = MSGBUF_SEQNORM(mbp, mbp->msg_wseq);
	mbp->msg_rseq = MSGBUF_SEQNORM(mbp, mbp->msg_rseq);
        mbp->msg_ptr = ptr;
	cksum = msgbuf_cksum(mbp);
	if (cksum != mbp->msg_cksum) {
		if (bootverbose) {
			printf("msgbuf cksum mismatch (read %x, calc %x)\n",
			    mbp->msg_cksum, cksum);
			printf("Old msgbuf not recovered\n");
		}
		msgbuf_clear(mbp);
	}
}

/*
 * Clear the message buffer.
 */
void
msgbuf_clear(struct msgbuf *mbp)
{

	bzero(mbp->msg_ptr, mbp->msg_size);
	mbp->msg_wseq = 0;
	mbp->msg_rseq = 0;
	mbp->msg_cksum = 0;
}

/*
 * Get a count of the number of unread characters in the message buffer.
 */
int
msgbuf_getcount(struct msgbuf *mbp)
{
	u_int len;

	len = MSGBUF_SEQSUB(mbp, mbp->msg_wseq, mbp->msg_rseq);
	if (len > mbp->msg_size)
		len = mbp->msg_size;
	return (len);
}

/*
 * Append a character to a message buffer.  This function can be
 * considered fully reentrant so long as the number of concurrent
 * callers is less than the number of characters in the buffer.
 * However, the message buffer is only guaranteed to be consistent
 * for reading when there are no callers in this function.
 */
void
msgbuf_addchar(struct msgbuf *mbp, int c)
{
	u_int new_seq, pos, seq;

	do {
		seq = mbp->msg_wseq;
		new_seq = MSGBUF_SEQNORM(mbp, seq + 1);
	} while (atomic_cmpset_rel_int(&mbp->msg_wseq, seq, new_seq) == 0);
	pos = MSGBUF_SEQ_TO_POS(mbp, seq);
	atomic_add_int(&mbp->msg_cksum, (u_int)(u_char)c -
	    (u_int)(u_char)mbp->msg_ptr[pos]);
	mbp->msg_ptr[pos] = c;
}

/*
 * Read and mark as read a character from a message buffer.
 * Returns the character, or -1 if no characters are available.
 */
int
msgbuf_getchar(struct msgbuf *mbp)
{
	u_int len, wseq;
	int c;

	wseq = mbp->msg_wseq;
	len = MSGBUF_SEQSUB(mbp, wseq, mbp->msg_rseq);
	if (len == 0)
		return (-1);
	if (len > mbp->msg_size)
		mbp->msg_rseq = MSGBUF_SEQNORM(mbp, wseq - mbp->msg_size);
	c = (u_char)mbp->msg_ptr[MSGBUF_SEQ_TO_POS(mbp, mbp->msg_rseq)];
	mbp->msg_rseq = MSGBUF_SEQNORM(mbp, mbp->msg_rseq + 1);
	return (c);
}

/*
 * Read and mark as read a number of characters from a message buffer.
 * Returns the number of characters that were placed in `buf'.
 */
int
msgbuf_getbytes(struct msgbuf *mbp, char *buf, int buflen)
{
	u_int len, pos, wseq;

	wseq = mbp->msg_wseq;
	len = MSGBUF_SEQSUB(mbp, wseq, mbp->msg_rseq);
	if (len == 0)
		return (0);
	if (len > mbp->msg_size) {
		mbp->msg_rseq = MSGBUF_SEQNORM(mbp, wseq - mbp->msg_size);
		len = mbp->msg_size;
	}
	pos = MSGBUF_SEQ_TO_POS(mbp, mbp->msg_rseq);
	len = min(len, mbp->msg_size - pos);
	len = min(len, (u_int)buflen);

	bcopy(&mbp->msg_ptr[pos], buf, len);
	mbp->msg_rseq = MSGBUF_SEQNORM(mbp, mbp->msg_rseq + len);
	return (len);
}

/*
 * Peek at the full contents of a message buffer without marking any
 * data as read. `seqp' should point to an unsigned integer that
 * msgbuf_peekbytes() can use to retain state between calls so that
 * the whole message buffer can be read in multiple short reads.
 * To initialise this variable to the start of the message buffer,
 * call msgbuf_peekbytes() with a NULL `buf' parameter.
 *
 * Returns the number of characters that were placed in `buf'.
 */
int
msgbuf_peekbytes(struct msgbuf *mbp, char *buf, int buflen, u_int *seqp)
{
	u_int len, pos, wseq;

	if (buf == NULL) {
		/* Just initialise *seqp. */
		*seqp = MSGBUF_SEQNORM(mbp, mbp->msg_wseq - mbp->msg_size);
		return (0);
	}

	wseq = mbp->msg_wseq;
	len = MSGBUF_SEQSUB(mbp, wseq, *seqp);
	if (len == 0)
		return (0);
	if (len > mbp->msg_size) {
		*seqp = MSGBUF_SEQNORM(mbp, wseq - mbp->msg_size);
		len = mbp->msg_size;
	}
	pos = MSGBUF_SEQ_TO_POS(mbp, *seqp);
	len = min(len, mbp->msg_size - pos);
	len = min(len, (u_int)buflen);
	bcopy(&mbp->msg_ptr[MSGBUF_SEQ_TO_POS(mbp, *seqp)], buf, len);
	*seqp = MSGBUF_SEQNORM(mbp, *seqp + len);
	return (len);
}

/*
 * Compute the checksum for the complete message buffer contents.
 */
static u_int
msgbuf_cksum(struct msgbuf *mbp)
{
	u_int i, sum;

	sum = 0;
	for (i = 0; i < mbp->msg_size; i++)
		sum += (u_char)mbp->msg_ptr[i];
	return (sum);
}

/*
 * Copy from one message buffer to another.
 */
void
msgbuf_copy(struct msgbuf *src, struct msgbuf *dst)
{
	int c;

	while ((c = msgbuf_getchar(src)) >= 0)
		msgbuf_addchar(dst, c);
}
