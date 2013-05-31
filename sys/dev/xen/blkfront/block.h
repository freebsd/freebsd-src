/*
 * XenBSD block device driver
 *
 * Copyright (c) 2010-2013 Spectra Logic Corporation
 * Copyright (c) 2009 Scott Long, Yahoo!
 * Copyright (c) 2009 Frank Suchomel, Citrix
 * Copyright (c) 2009 Doug F. Rabson, Citrix
 * Copyright (c) 2005 Kip Macy
 * Copyright (c) 2003-2004, Keir Fraser & Steve Hand
 * Modifications by Mark A. Williamson are (c) Intel Research Cambridge
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * $FreeBSD$
 */


#ifndef __XEN_BLKFRONT_BLOCK_H__
#define __XEN_BLKFRONT_BLOCK_H__
#include <xen/blkif.h>

/**
 * Given a number of blkif segments, compute the maximum I/O size supported.
 *
 * \note This calculation assumes that all but the first and last segments 
 *       of the I/O are fully utilized.
 *
 * \note We reserve a segement from the maximum supported by the transport to
 *       guarantee we can handle an unaligned transfer without the need to
 *       use a bounce buffer.
 */
#define	XBD_SEGS_TO_SIZE(segs)						\
	(((segs) - 1) * PAGE_SIZE)

/**
 * Compute the maximum number of blkif segments requried to represent
 * an I/O of the given size.
 *
 * \note This calculation assumes that all but the first and last segments
 *       of the I/O are fully utilized.
 *
 * \note We reserve a segement to guarantee we can handle an unaligned
 *       transfer without the need to use a bounce buffer.
 */
#define	XBD_SIZE_TO_SEGS(size)						\
	((size / PAGE_SIZE) + 1)

/**
 * The maximum number of outstanding requests blocks (request headers plus
 * additional segment blocks) we will allow in a negotiated block-front/back
 * communication channel.
 */
#define XBD_MAX_REQUESTS		256

/**
 * The maximum mapped region size per request we will allow in a negotiated
 * block-front/back communication channel.
 */
#define	XBD_MAX_REQUEST_SIZE						\
	MIN(MAXPHYS, XBD_SEGS_TO_SIZE(BLKIF_MAX_SEGMENTS_PER_REQUEST))

/**
 * The maximum number of segments (within a request header and accompanying
 * segment blocks) per request we will allow in a negotiated block-front/back
 * communication channel.
 */
#define	XBD_MAX_SEGMENTS_PER_REQUEST					\
	(MIN(BLKIF_MAX_SEGMENTS_PER_REQUEST,				\
	     XBD_SIZE_TO_SEGS(XBD_MAX_REQUEST_SIZE)))

/**
 * The maximum number of shared memory ring pages we will allow in a
 * negotiated block-front/back communication channel.  Allow enough
 * ring space for all requests to be  XBD_MAX_REQUEST_SIZE'd.
 */
#define XBD_MAX_RING_PAGES						    \
	BLKIF_RING_PAGES(BLKIF_SEGS_TO_BLOCKS(XBD_MAX_SEGMENTS_PER_REQUEST) \
		       * XBD_MAX_REQUESTS)

struct xbd_command;
typedef void xbd_cbcf_t(struct xbd_command *);

struct xbd_command {
	TAILQ_ENTRY(xbd_command) cm_link;
	struct xbd_softc	*cm_sc;
	u_int			 cm_flags;
#define XBD_CMD_FROZEN		(1<<0)
#define XBD_CMD_POLLED		(1<<1)
#define XBD_ON_XBDQ_FREE	(1<<2)
#define XBD_ON_XBDQ_READY	(1<<3)
#define XBD_ON_XBDQ_BUSY	(1<<4)
#define XBD_ON_XBDQ_COMPLETE	(1<<5)
#define XBD_ON_XBDQ_MASK	((1<<2)|(1<<3)|(1<<4)|(1<<5))
	bus_dmamap_t		 cm_map;
	uint64_t		 cm_id;
	grant_ref_t		*cm_sg_refs;
	struct bio		*cm_bp;
	grant_ref_t		 cm_gref_head;
	void			*cm_data;
	size_t			 cm_datalen;
	u_int			 cm_nseg;
	int			 cm_operation;
	blkif_sector_t		 cm_sector_number;
	int			 cm_status;
	xbd_cbcf_t		*cm_complete;
};

#define XBDQ_FREE	0
#define XBDQ_BIO	1
#define XBDQ_READY	2
#define XBDQ_BUSY	3
#define XBDQ_COMPLETE	4
#define XBDQ_COUNT	5

struct xbd_qstat {
	uint32_t	q_length;
	uint32_t	q_max;
};

union xbd_statrequest {
	uint32_t		ms_item;
	struct xbd_qstat	ms_qstat;
};

/*
 * We have one of these per vbd, whether ide, scsi or 'other'.
 */
struct xbd_softc {
	device_t			 xbd_dev;
	struct disk			*xbd_disk;	/* disk params */
	struct bio_queue_head 		 xbd_bioq;	/* sort queue */
	int				 xbd_unit;
	int				 xbd_flags;
#define XBD_OPEN	(1<<0)		/* drive is open (can't shut down) */
#define XBD_BARRIER	(1 << 1)	/* backend supports barriers */
#define XBD_READY	(1 << 2)	/* Is ready */
#define XBD_FROZEN	(1 << 3)	/* Waiting for resources */
	int				 xbd_vdevice;
	int				 xbd_connected;
	u_int				 xbd_ring_pages;
	uint32_t			 xbd_max_requests;
	uint32_t			 xbd_max_request_segments;
	uint32_t			 xbd_max_request_blocks;
	uint32_t			 xbd_max_request_size;
	grant_ref_t			 xbd_ring_ref[XBD_MAX_RING_PAGES];
	blkif_front_ring_t		 xbd_ring;
	unsigned int			 xbd_irq;
	struct gnttab_free_callback	 xbd_callback;
	TAILQ_HEAD(,xbd_command)	 xbd_cm_free;
	TAILQ_HEAD(,xbd_command)	 xbd_cm_ready;
	TAILQ_HEAD(,xbd_command)	 xbd_cm_busy;
	TAILQ_HEAD(,xbd_command)	 xbd_cm_complete;
	struct xbd_qstat		 xbd_qstat[XBDQ_COUNT];
	bus_dma_tag_t			 xbd_io_dmat;

	/**
	 * The number of people holding this device open.  We won't allow a
	 * hot-unplug unless this is 0.
	 */
	int				 xbd_users;
	struct mtx			 xbd_io_lock;

	struct xbd_command		*xbd_shadow;
};

int xbd_instance_create(struct xbd_softc *, blkif_sector_t sectors, int device,
			uint16_t vdisk_info, unsigned long sector_size);

#define XBDQ_ADD(sc, qname)					\
	do {							\
		struct xbd_qstat *qs;				\
								\
		qs = &(sc)->xbd_qstat[qname];			\
		qs->q_length++;					\
		if (qs->q_length > qs->q_max)			\
			qs->q_max = qs->q_length;		\
	} while (0)

#define XBDQ_REMOVE(sc, qname)	(sc)->xbd_qstat[qname].q_length--

#define XBDQ_INIT(sc, qname)					\
	do {							\
		sc->xbd_qstat[qname].q_length = 0;		\
		sc->xbd_qstat[qname].q_max = 0;			\
	} while (0)

#define XBDQ_COMMAND_QUEUE(name, index)					\
	static __inline void						\
	xbd_initq_ ## name (struct xbd_softc *sc)			\
	{								\
		TAILQ_INIT(&sc->xbd_cm_ ## name);			\
		XBDQ_INIT(sc, index);					\
	}								\
	static __inline void						\
	xbd_enqueue_ ## name (struct xbd_command *cm)			\
	{								\
		if ((cm->cm_flags & XBD_ON_XBDQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_TAIL(&cm->cm_sc->xbd_cm_ ## name, cm, cm_link); \
		cm->cm_flags |= XBD_ON_ ## index;			\
		XBDQ_ADD(cm->cm_sc, index);				\
	}								\
	static __inline void						\
	xbd_requeue_ ## name (struct xbd_command *cm)			\
	{								\
		if ((cm->cm_flags & XBD_ON_XBDQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_HEAD(&cm->cm_sc->xbd_cm_ ## name, cm, cm_link); \
		cm->cm_flags |= XBD_ON_ ## index;			\
		XBDQ_ADD(cm->cm_sc, index);				\
	}								\
	static __inline struct xbd_command *				\
	xbd_dequeue_ ## name (struct xbd_softc *sc)			\
	{								\
		struct xbd_command *cm;					\
									\
		if ((cm = TAILQ_FIRST(&sc->xbd_cm_ ## name)) != NULL) {	\
			if ((cm->cm_flags & XBD_ON_XBDQ_MASK) !=		\
			     XBD_ON_ ## index) {				\
				printf("command %p not in queue, "	\
				    "flags = %#x, bit = %#x\n", cm,	\
				    cm->cm_flags, XBD_ON_ ## index);	\
				panic("command not in queue");		\
			}						\
			TAILQ_REMOVE(&sc->xbd_cm_ ## name, cm, cm_link);\
			cm->cm_flags &= ~XBD_ON_ ## index;		\
			XBDQ_REMOVE(sc, index);				\
		}							\
		return (cm);						\
	}								\
	static __inline void						\
	xbd_remove_ ## name (struct xbd_command *cm)			\
	{								\
		if ((cm->cm_flags & XBD_ON_XBDQ_MASK) != XBD_ON_ ## index){\
			printf("command %p not in queue, flags = %#x, " \
			    "bit = %#x\n", cm, cm->cm_flags,		\
			    XBD_ON_ ## index);				\
			panic("command not in queue");			\
		}							\
		TAILQ_REMOVE(&cm->cm_sc->xbd_cm_ ## name, cm, cm_link);	\
		cm->cm_flags &= ~XBD_ON_ ## index;			\
		XBDQ_REMOVE(cm->cm_sc, index);				\
	}								\
struct hack

XBDQ_COMMAND_QUEUE(free, XBDQ_FREE);
XBDQ_COMMAND_QUEUE(ready, XBDQ_READY);
XBDQ_COMMAND_QUEUE(busy, XBDQ_BUSY);
XBDQ_COMMAND_QUEUE(complete, XBDQ_COMPLETE);

static __inline void
xbd_initq_bio(struct xbd_softc *sc)
{
	bioq_init(&sc->xbd_bioq);
	XBDQ_INIT(sc, XBDQ_BIO);
}

static __inline void
xbd_enqueue_bio(struct xbd_softc *sc, struct bio *bp)
{
	bioq_insert_tail(&sc->xbd_bioq, bp);
	XBDQ_ADD(sc, XBDQ_BIO);
}

static __inline void
xbd_requeue_bio(struct xbd_softc *sc, struct bio *bp)
{
	bioq_insert_head(&sc->xbd_bioq, bp);
	XBDQ_ADD(sc, XBDQ_BIO);
}

static __inline struct bio *
xbd_dequeue_bio(struct xbd_softc *sc)
{
	struct bio *bp;

	if ((bp = bioq_first(&sc->xbd_bioq)) != NULL) {
		bioq_remove(&sc->xbd_bioq, bp);
		XBDQ_REMOVE(sc, XBDQ_BIO);
	}
	return (bp);
}

#endif /* __XEN_BLKFRONT_BLOCK_H__ */
