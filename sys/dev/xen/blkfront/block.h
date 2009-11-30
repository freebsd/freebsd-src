/*
 * 
 * XenBSD block device driver
 *
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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

struct blk_shadow {
	blkif_request_t req;
	unsigned long request;
	unsigned long frame[BLKIF_MAX_SEGMENTS_PER_REQUEST];
};

#define BLK_RING_SIZE __RING_SIZE((blkif_sring_t *)0, PAGE_SIZE)


struct xb_softc {
	device_t		  xb_dev;
	struct disk		  *xb_disk;		/* disk params */
	struct bio_queue_head     xb_bioq;		/* sort queue */
	int			  xb_unit;
	int			  xb_flags;
	struct blkfront_info      *xb_info;
	LIST_ENTRY(xb_softc)      entry;
#define XB_OPEN	(1<<0)		/* drive is open (can't shut down) */
};


/*
 * We have one of these per vbd, whether ide, scsi or 'other'.  They
 * hang in private_data off the gendisk structure. We may end up
 * putting all kinds of interesting stuff here :-)
 */
struct blkfront_info
{
	device_t xbdev;
	dev_t dev;
 	struct gendisk *gd;
	int vdevice;
	blkif_vdev_t handle;
	int connected;
	int ring_ref;
	blkif_front_ring_t ring;
	unsigned int irq;
	struct xlbd_major_info *mi;
#if 0
	request_queue_t *rq;
	struct work_struct work;
#endif
	struct gnttab_free_callback callback;
	struct blk_shadow shadow[BLK_RING_SIZE];
	unsigned long shadow_free;
	struct xb_softc *sc;
	int feature_barrier;
	int is_ready;
	/**
	 * The number of people holding this device open.  We won't allow a
	 * hot-unplug unless this is 0.
	 */
	int users;
};
/* Note that xlvbd_add doesn't call add_disk for you: you're expected
   to call add_disk on info->gd once the disk is properly connected
   up. */
int xlvbd_add(device_t, blkif_sector_t capacity, int device,
	      uint16_t vdisk_info, uint16_t sector_size, struct blkfront_info *info);
void xlvbd_del(struct blkfront_info *info);

#endif /* __XEN_DRIVERS_BLOCK_H__ */

