/*
 * Copyright (c) 2004 Lukas Ertl
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

#ifndef _GEOM_VINUM_RAID5_H_
#define	_GEOM_VINUM_RAID5_H_

/*
 * A single RAID5 request usually needs more than one I/O transaction,
 * depending on the state of the associated subdisks and the direction of the
 * transaction (read or write).  Every subrequest of a RAID5 request,
 * represented by a gv_raid_packet, is defined by a gv_raid5_bit.
 */

/* A subrequest of a RAID5 read/write operation. */
struct gv_raid5_bit {
	struct bio	*bio;		/* BIO of this subrequest. */
	caddr_t		buf;		/* Data buffer of this subrequest. */
	int		malloc;		/* Flag if data buffer was malloced. */
	struct g_consumer *consumer;	/* Consumer to send the BIO to. */
	TAILQ_ENTRY(gv_raid5_bit) list;	/* Entry in the list of this request. */
};

/* Container for one or more gv_raid5_bits; represents a RAID5 I/O request. */
struct gv_raid5_packet {
	caddr_t	buf;		/* Data buffer of this RAID5 request. */
	off_t	length;		/* Size of data buffer. */
	off_t	lockbase;	/* Deny access to our plex offset. */
	off_t	offset;		/* The drive offset of the subdisk. */
	int	bufmalloc;	/* Flag if data buffer was malloced. */
	int	active;		/* Count of active subrequests. */
	int	rqcount;	/* Count of subrequests. */

	struct bio	*bio;	/* Pointer to the original bio. */
	caddr_t		 data;	/* Pointer to the original data. */

	struct g_consumer *original;	/* Consumer to the data stripe. */
	struct g_consumer *parity;	/* Consumer to the parity stripe. */

	/* State of this RAID5 packet. */
	enum {
	    SETUP,		/* Newly created. */
	    VALID,		/* Ready for processing. */
	    IO,			/* Currently doing I/O. */
	    FINISH		/* Packet has finished. */
	} state;

	/* Type of this RAID5 transaction. */
	enum {
	    JUNK,		/* Newly created, not valid. */
	    NORMAL,		/* Normal read or write. */
	    ISPARITY,		/* Containing only parity data. */
	    NOPARITY,		/* Parity stripe not available. */
	    DEGRADED,		/* Data stripe not available. */
	    COMBINED		/* Data and parity stripes ok, others not. */
	} type;

	TAILQ_HEAD(,gv_raid5_bit)    bits; /* List of subrequests. */
	TAILQ_ENTRY(gv_raid5_packet) list; /* Entry in plex's packet list. */
};

int	gv_build_raid5_req(struct gv_raid5_packet *, struct bio *, caddr_t,
	    long, off_t);
void	gv_free_raid5_packet(struct gv_raid5_packet *);
void	gv_raid5_done(struct bio *);
void	gv_raid5_worker(void *);
struct gv_raid5_packet  *gv_new_raid5_packet(void);
struct gv_raid5_bit	*gv_new_raid5_bit(void);

#endif /* !_GEOM_VINUM_RAID5_H_ */
