/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)buf.h	8.9 (Berkeley) 3/30/95
 * $FreeBSD$
 */

#ifndef _SYS_BIO_H_
#define	_SYS_BIO_H_

#include <sys/queue.h>

struct bio;
struct buf;
struct g_consumer;
struct g_provider;

struct iodone_chain {
	long	ic_prev_flags;
	void	(*ic_prev_iodone)(struct bio *);
	void	*ic_prev_iodone_chain;
	struct {
		long	ia_long;
		void	*ia_ptr;
	}	ic_args[5];
};

/*
 * The bio structure describes an I/O operation in the kernel.
 */
struct bio {
	u_int	bio_cmd;		/* I/O operation. */
	dev_t	bio_dev;		/* Device to do I/O on. */
	daddr64_t bio_blkno;		/* Underlying physical block number. */
	off_t	bio_offset;		/* Offset into file. */
	long	bio_bcount;		/* Valid bytes in buffer. */
	caddr_t	bio_data;		/* Memory, superblocks, indirect etc. */
	u_int	bio_flags;		/* BIO_ flags. */
	struct buf	*_bio_buf;	/* Parent buffer. */
	int	bio_error;		/* Errno for BIO_ERROR. */
	long	bio_resid;		/* Remaining I/0 in bytes. */
	void	(*bio_done)(struct bio *);
	void	*bio_driver1;		/* Private use by the callee. */
	void	*bio_driver2;		/* Private use by the callee. */
	void	*bio_caller1;		/* Private use by the caller. */
	void	*bio_caller2;		/* Private use by the caller. */
	TAILQ_ENTRY(bio) bio_queue;	/* Disksort queue. */

	/* XXX: these go away when bio chaining is introduced */
	daddr64_t bio_pblkno;               /* physical block number */
	struct	iodone_chain *bio_done_chain;
	struct bio *bio_linkage;
	off_t	bio_length;
	const char *bio_attribute;
	off_t	bio_completed;
	struct g_consumer *bio_from;
	struct g_provider *bio_to;
};

/* bio_cmd */
#define BIO_READ	0x00000001
#define BIO_WRITE	0x00000002
#define BIO_DELETE	0x00000004
#define BIO_GETATTR	0x00000008
#define BIO_SETATTR	0x00000010
#define BIO_CMD1	0x40000000	/* Available for local hacks */
#define BIO_CMD2	0x80000000	/* Available for local hacks */

/* bio_flags */
#define BIO_ERROR	0x00000001
#define BIO_DONE	0x00000004
#define BIO_FLAG2	0x40000000	/* Available for local hacks */
#define BIO_FLAG1	0x80000000	/* Available for local hacks */

#ifdef _KERNEL

static __inline__ void
biodone(struct bio *bp)
{
	bp->bio_flags |= BIO_DONE;
	bp->bio_done(bp);
}

#ifndef _DEVICESTAT_H
struct devstat;
void devstat_end_transaction_bio(struct devstat *, struct bio *);
#endif

static __inline__ void
biofinish(struct bio *bp, struct devstat *stat, int error)
{
	
	if (error) {
		bp->bio_error = error;
		bp->bio_flags |= BIO_ERROR;
	}
	if (stat != NULL)
		devstat_end_transaction_bio(stat, bp);
	biodone(bp);
}

struct bio_queue_head {
	TAILQ_HEAD(bio_queue, bio) queue;
	daddr64_t last_pblkno;
	struct	bio *insert_point;
	struct	bio *switch_point;
	int busy;
};

static __inline void bioq_init(struct bio_queue_head *head);
static __inline void bioq_insert_tail(struct bio_queue_head *head,
				      struct bio *bp);
static __inline void bioq_remove(struct bio_queue_head *head,
				 struct bio *bp);
static __inline struct bio *bioq_first(struct bio_queue_head *head);

static __inline void
bioq_init(struct bio_queue_head *head)
{
	TAILQ_INIT(&head->queue);
	head->last_pblkno = 0;
	head->insert_point = NULL;
	head->switch_point = NULL;
}

static __inline void
bioq_insert_tail(struct bio_queue_head *head, struct bio *bp)
{

	TAILQ_INSERT_TAIL(&head->queue, bp, bio_queue);
}

static __inline void
bioq_remove(struct bio_queue_head *head, struct bio *bp)
{
	if (bp == head->switch_point)
		head->switch_point = TAILQ_NEXT(bp, bio_queue);
	if (bp == head->insert_point) {
		head->insert_point = TAILQ_PREV(bp, bio_queue, bio_queue);
		if (head->insert_point == NULL)
			head->last_pblkno = 0;
	} else if (bp == TAILQ_FIRST(&head->queue))
		head->last_pblkno = bp->bio_pblkno;
	TAILQ_REMOVE(&head->queue, bp, bio_queue);
	if (TAILQ_FIRST(&head->queue) == head->switch_point)
		head->switch_point = NULL;
}

static __inline struct bio *
bioq_first(struct bio_queue_head *head)
{
	return (TAILQ_FIRST(&head->queue));
}

/*
 * Zero out the bio's data area.
 */
#define	clrbio(bp) {							\
	bzero((bp)->bio_data, (u_int)(bp)->bio_bcount);			\
	(bp)->bio_resid = 0;						\
}

int	physio(dev_t dev, struct uio *uio, int ioflag);
#define physread physio
#define physwrite physio

#endif /* _KERNEL */

#endif /* !_SYS_BIO_H_ */
