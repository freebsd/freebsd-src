/*
 * XenBSD block device driver
 *
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


#ifndef __XEN_DRIVERS_BLOCK_H__
#define __XEN_DRIVERS_BLOCK_H__
#include <xen/interface/io/blkif.h>

struct xlbd_type_info
{
	int partn_shift;
	int disks_per_major;
	char *devname;
	char *diskname;
};

struct xlbd_major_info
{
	int major;
	int index;
	int usage;
	struct xlbd_type_info *type;
};

struct xb_command {
	TAILQ_ENTRY(xb_command)	cm_link;
	struct xb_softc		*cm_sc;
	u_int			cm_flags;
#define XB_CMD_FROZEN		(1<<0)
#define XB_CMD_POLLED		(1<<1)
#define XB_ON_XBQ_FREE		(1<<2)
#define XB_ON_XBQ_READY		(1<<3)
#define XB_ON_XBQ_BUSY		(1<<4)
#define XB_ON_XBQ_COMPLETE	(1<<5)
#define XB_ON_XBQ_MASK		((1<<2)|(1<<3)|(1<<4)|(1<<5))
	bus_dmamap_t		map;
	blkif_request_t		req;
	struct bio		*bp;
	grant_ref_t		gref_head;
	void			*data;
	size_t			datalen;
	int			operation;
	blkif_sector_t		sector_number;
	int			status;
	void			(* cm_complete)(struct xb_command *);
};

#define BLK_RING_SIZE __RING_SIZE((blkif_sring_t *)0, PAGE_SIZE)

#define XBQ_FREE	0
#define XBQ_BIO		1
#define XBQ_READY	2
#define XBQ_BUSY	3
#define XBQ_COMPLETE	4
#define XBQ_COUNT	5

struct xb_qstat {
	uint32_t	q_length;
	uint32_t	q_max;
};

union xb_statrequest {
	uint32_t		ms_item;
	struct xb_qstat		ms_qstat;
};

/*
 * We have one of these per vbd, whether ide, scsi or 'other'.
 */
struct xb_softc {
	device_t		xb_dev;
	struct disk		*xb_disk;		/* disk params */
	struct bio_queue_head   xb_bioq;		/* sort queue */
	int			xb_unit;
	int			xb_flags;
#define XB_OPEN		(1<<0)		/* drive is open (can't shut down) */
#define XB_BARRIER	(1 << 1)	/* backend supports barriers */
#define XB_READY	(1 << 2)	/* Is ready */
#define XB_FROZEN	(1 << 3)	/* Waiting for resources */
	int			vdevice;
	blkif_vdev_t		handle;
	int			connected;
	int			ring_ref;
	blkif_front_ring_t	ring;
	unsigned int		irq;
	struct xlbd_major_info	*mi;
	struct gnttab_free_callback	callback;
	TAILQ_HEAD(,xb_command)	cm_free;
	TAILQ_HEAD(,xb_command)	cm_ready;
	TAILQ_HEAD(,xb_command)	cm_busy;
	TAILQ_HEAD(,xb_command)	cm_complete;
	struct xb_qstat		xb_qstat[XBQ_COUNT];
	bus_dma_tag_t		xb_io_dmat;

	/**
	 * The number of people holding this device open.  We won't allow a
	 * hot-unplug unless this is 0.
	 */
	int			users;
	struct mtx		xb_io_lock;
	struct xb_command	shadow[BLK_RING_SIZE];
};

int xlvbd_add(struct xb_softc *, blkif_sector_t capacity, int device,
	      uint16_t vdisk_info, uint16_t sector_size);
void xlvbd_del(struct xb_softc *);

#define XBQ_ADD(sc, qname)					\
	do {							\
		struct xb_qstat *qs;				\
								\
		qs = &(sc)->xb_qstat[qname];			\
		qs->q_length++;					\
		if (qs->q_length > qs->q_max)			\
			qs->q_max = qs->q_length;		\
	} while (0)

#define XBQ_REMOVE(sc, qname)	(sc)->xb_qstat[qname].q_length--

#define XBQ_INIT(sc, qname)					\
	do {							\
		sc->xb_qstat[qname].q_length = 0;		\
		sc->xb_qstat[qname].q_max = 0;			\
	} while (0)

#define XBQ_COMMAND_QUEUE(name, index)					\
	static __inline void						\
	xb_initq_ ## name (struct xb_softc *sc)				\
	{								\
		TAILQ_INIT(&sc->cm_ ## name);				\
		XBQ_INIT(sc, index);					\
	}								\
	static __inline void						\
	xb_enqueue_ ## name (struct xb_command *cm)			\
	{								\
		if ((cm->cm_flags & XB_ON_XBQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_TAIL(&cm->cm_sc->cm_ ## name, cm, cm_link); \
		cm->cm_flags |= XB_ON_ ## index;			\
		XBQ_ADD(cm->cm_sc, index);				\
	}								\
	static __inline void						\
	xb_requeue_ ## name (struct xb_command *cm)			\
	{								\
		if ((cm->cm_flags & XB_ON_XBQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_HEAD(&cm->cm_sc->cm_ ## name, cm, cm_link); \
		cm->cm_flags |= XB_ON_ ## index;			\
		XBQ_ADD(cm->cm_sc, index);				\
	}								\
	static __inline struct xb_command *				\
	xb_dequeue_ ## name (struct xb_softc *sc)			\
	{								\
		struct xb_command *cm;					\
									\
		if ((cm = TAILQ_FIRST(&sc->cm_ ## name)) != NULL) {	\
			if ((cm->cm_flags & XB_ON_ ## index) == 0) {	\
				printf("command %p not in queue, "	\
				    "flags = %#x, bit = %#x\n", cm,	\
				    cm->cm_flags, XB_ON_ ## index);	\
				panic("command not in queue");		\
			}						\
			TAILQ_REMOVE(&sc->cm_ ## name, cm, cm_link);	\
			cm->cm_flags &= ~XB_ON_ ## index;		\
			XBQ_REMOVE(sc, index);				\
		}							\
		return (cm);						\
	}								\
	static __inline void						\
	xb_remove_ ## name (struct xb_command *cm)			\
	{								\
		if ((cm->cm_flags & XB_ON_ ## index) == 0) {		\
			printf("command %p not in queue, flags = %#x, " \
			    "bit = %#x\n", cm, cm->cm_flags,		\
			    XB_ON_ ## index);				\
			panic("command not in queue");			\
		}							\
		TAILQ_REMOVE(&cm->cm_sc->cm_ ## name, cm, cm_link);	\
		cm->cm_flags &= ~XB_ON_ ## index;			\
		XBQ_REMOVE(cm->cm_sc, index);				\
	}								\
struct hack

XBQ_COMMAND_QUEUE(free, XBQ_FREE);
XBQ_COMMAND_QUEUE(ready, XBQ_READY);
XBQ_COMMAND_QUEUE(busy, XBQ_BUSY);
XBQ_COMMAND_QUEUE(complete, XBQ_COMPLETE);

static __inline void
xb_initq_bio(struct xb_softc *sc)
{
	bioq_init(&sc->xb_bioq);
	XBQ_INIT(sc, XBQ_BIO);
}

static __inline void
xb_enqueue_bio(struct xb_softc *sc, struct bio *bp)
{
	bioq_insert_tail(&sc->xb_bioq, bp);
	XBQ_ADD(sc, XBQ_BIO);
}

static __inline void
xb_requeue_bio(struct xb_softc *sc, struct bio *bp)
{
	bioq_insert_head(&sc->xb_bioq, bp);
	XBQ_ADD(sc, XBQ_BIO);
}

static __inline struct bio *
xb_dequeue_bio(struct xb_softc *sc)
{
	struct bio *bp;

	if ((bp = bioq_first(&sc->xb_bioq)) != NULL) {
		bioq_remove(&sc->xb_bioq, bp);
		XBQ_REMOVE(sc, XBQ_BIO);
	}
	return (bp);
}

#endif /* __XEN_DRIVERS_BLOCK_H__ */

