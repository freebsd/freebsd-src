/*
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/intr_machdep.h>
#include <machine/vmparam.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <xen/evtchn.h>
#include <xen/gnttab.h>
#include <xen/interface/grant_table.h>
#include <xen/interface/io/protocols.h>
#include <xen/xenbus/xenbusvar.h>

#include <geom/geom_disk.h>

#include <dev/xen/blkfront/block.h>

#include "xenbus_if.h"

#define    ASSERT(S)       KASSERT(S, (#S))
/* prototypes */
struct xb_softc;
static void xb_startio(struct xb_softc *sc);
static void connect(device_t, struct blkfront_info *);
static void blkfront_closing(device_t);
static int blkfront_detach(device_t);
static int talk_to_backend(device_t, struct blkfront_info *);
static int setup_blkring(device_t, struct blkfront_info *);
static void blkif_int(void *);
#if 0
static void blkif_restart_queue(void *arg);
#endif
static void blkif_recover(struct blkfront_info *);
static void blkif_completion(struct blk_shadow *);
static void blkif_free(struct blkfront_info *, int);

#define GRANT_INVALID_REF 0
#define BLK_RING_SIZE __RING_SIZE((blkif_sring_t *)0, PAGE_SIZE)

LIST_HEAD(xb_softc_list_head, xb_softc) xbsl_head;

/* Control whether runtime update of vbds is enabled. */
#define ENABLE_VBD_UPDATE 0

#if ENABLE_VBD_UPDATE
static void vbd_update(void);
#endif


#define BLKIF_STATE_DISCONNECTED 0
#define BLKIF_STATE_CONNECTED    1
#define BLKIF_STATE_SUSPENDED    2

#ifdef notyet
static char *blkif_state_name[] = {
	[BLKIF_STATE_DISCONNECTED] = "disconnected",
	[BLKIF_STATE_CONNECTED]    = "connected",
	[BLKIF_STATE_SUSPENDED]    = "closed",
};

static char * blkif_status_name[] = {
	[BLKIF_INTERFACE_STATUS_CLOSED]       = "closed",
	[BLKIF_INTERFACE_STATUS_DISCONNECTED] = "disconnected",
	[BLKIF_INTERFACE_STATUS_CONNECTED]    = "connected",
	[BLKIF_INTERFACE_STATUS_CHANGED]      = "changed",
};
#endif
#define WPRINTK(fmt, args...) printf("[XEN] " fmt, ##args)
#if 0
#define DPRINTK(fmt, args...) printf("[XEN] %s:%d: " fmt ".\n", __func__, __LINE__, ##args)
#else
#define DPRINTK(fmt, args...) 
#endif

static grant_ref_t gref_head;
#define MAXIMUM_OUTSTANDING_BLOCK_REQS \
    (BLKIF_MAX_SEGMENTS_PER_REQUEST * BLK_RING_SIZE)

static void kick_pending_request_queues(struct blkfront_info *);
static int blkif_open(struct disk *dp);
static int blkif_close(struct disk *dp);
static int blkif_ioctl(struct disk *dp, u_long cmd, void *addr, int flag, struct thread *td);
static int blkif_queue_request(struct bio *bp);
static void xb_strategy(struct bio *bp);

// In order to quiesce the device during kernel dumps, outstanding requests to
// DOM0 for disk reads/writes need to be accounted for.
static	int	blkif_queued_requests;
static	int	xb_dump(void *, void *, vm_offset_t, off_t, size_t);


/* XXX move to xb_vbd.c when VBD update support is added */
#define MAX_VBDS 64

#define XBD_SECTOR_SIZE		512	/* XXX: assume for now */
#define XBD_SECTOR_SHFT		9

static struct mtx blkif_io_lock;

static vm_paddr_t
pfn_to_mfn(vm_paddr_t pfn)
{
	return (phystomach(pfn << PAGE_SHIFT) >> PAGE_SHIFT);
}

/*
 * Translate Linux major/minor to an appropriate name and unit
 * number. For HVM guests, this allows us to use the same drive names
 * with blkfront as the emulated drives, easing transition slightly.
 */
static void
blkfront_vdevice_to_unit(int vdevice, int *unit, const char **name)
{
	static struct vdev_info {
		int major;
		int shift;
		int base;
		const char *name;
	} info[] = {
		{3,	6,	0,	"ad"},	/* ide0 */
		{22,	6,	2,	"ad"},	/* ide1 */
		{33,	6,	4,	"ad"},	/* ide2 */
		{34,	6,	6,	"ad"},	/* ide3 */
		{56,	6,	8,	"ad"},	/* ide4 */
		{57,	6,	10,	"ad"},	/* ide5 */
		{88,	6,	12,	"ad"},	/* ide6 */
		{89,	6,	14,	"ad"},	/* ide7 */
		{90,	6,	16,	"ad"},	/* ide8 */
		{91,	6,	18,	"ad"},	/* ide9 */

		{8,	4,	0,	"da"},	/* scsi disk0 */
		{65,	4,	16,	"da"},	/* scsi disk1 */
		{66,	4,	32,	"da"},	/* scsi disk2 */
		{67,	4,	48,	"da"},	/* scsi disk3 */
		{68,	4,	64,	"da"},	/* scsi disk4 */
		{69,	4,	80,	"da"},	/* scsi disk5 */
		{70,	4,	96,	"da"},	/* scsi disk6 */
		{71,	4,	112,	"da"},	/* scsi disk7 */
		{128,	4,	128,	"da"},	/* scsi disk8 */
		{129,	4,	144,	"da"},	/* scsi disk9 */
		{130,	4,	160,	"da"},	/* scsi disk10 */
		{131,	4,	176,	"da"},	/* scsi disk11 */
		{132,	4,	192,	"da"},	/* scsi disk12 */
		{133,	4,	208,	"da"},	/* scsi disk13 */
		{134,	4,	224,	"da"},	/* scsi disk14 */
		{135,	4,	240,	"da"},	/* scsi disk15 */

		{202,	4,	0,	"xbd"},	/* xbd */

		{0,	0,	0,	NULL},
	};
	int major = vdevice >> 8;
	int minor = vdevice & 0xff;
	int i;

	if (vdevice & (1 << 28)) {
		*unit = (vdevice & ((1 << 28) - 1)) >> 8;
		*name = "xbd";
	}

	for (i = 0; info[i].major; i++) {
		if (info[i].major == major) {
			*unit = info[i].base + (minor >> info[i].shift);
			*name = info[i].name;
			return;
		}
	}

	*unit = minor >> 4;
	*name = "xbd";
}

int
xlvbd_add(device_t dev, blkif_sector_t capacity,
    int vdevice, uint16_t vdisk_info, uint16_t sector_size, 
    struct blkfront_info *info)
{
	struct xb_softc	*sc;
	int	unit, error = 0;
	const char *name;

	blkfront_vdevice_to_unit(vdevice, &unit, &name);

	sc = (struct xb_softc *)malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	sc->xb_unit = unit;
	sc->xb_info = info;
	info->sc = sc;

	if (strcmp(name, "xbd"))
		device_printf(dev, "attaching as %s%d\n", name, unit);

	memset(&sc->xb_disk, 0, sizeof(sc->xb_disk)); 
	sc->xb_disk = disk_alloc();
	sc->xb_disk->d_unit = sc->xb_unit;
	sc->xb_disk->d_open = blkif_open;
	sc->xb_disk->d_close = blkif_close;
	sc->xb_disk->d_ioctl = blkif_ioctl;
	sc->xb_disk->d_strategy = xb_strategy;
	sc->xb_disk->d_dump = xb_dump;
	sc->xb_disk->d_name = name;
	sc->xb_disk->d_drv1 = sc;
	sc->xb_disk->d_sectorsize = sector_size;

	/* XXX */
	sc->xb_disk->d_mediasize = capacity << XBD_SECTOR_SHFT;
#if 0
	sc->xb_disk->d_maxsize = DFLTPHYS;
#else /* XXX: xen can't handle large single i/o requests */
	sc->xb_disk->d_maxsize = 4096;
#endif
#ifdef notyet
	XENPRINTF("attaching device 0x%x unit %d capacity %llu\n",
		  xb_diskinfo[sc->xb_unit].device, sc->xb_unit,
		  sc->xb_disk->d_mediasize);
#endif
	sc->xb_disk->d_flags = 0;
	disk_create(sc->xb_disk, DISK_VERSION_00);
	bioq_init(&sc->xb_bioq);

	return error;
}

void
xlvbd_del(struct blkfront_info *info)
{
	struct xb_softc	*sc;

	sc = info->sc;
	disk_destroy(sc->xb_disk);
}
/************************ end VBD support *****************/

/*
 * Read/write routine for a buffer.  Finds the proper unit, place it on
 * the sortq and kick the controller.
 */
static void
xb_strategy(struct bio *bp)
{
	struct xb_softc	*sc = (struct xb_softc *)bp->bio_disk->d_drv1;

	/* bogus disk? */
	if (sc == NULL) {
		bp->bio_error = EINVAL;
		bp->bio_flags |= BIO_ERROR;
		goto bad;
	}

	DPRINTK("");

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	mtx_lock(&blkif_io_lock);

	bioq_disksort(&sc->xb_bioq, bp);
	xb_startio(sc);

	mtx_unlock(&blkif_io_lock);
	return;

 bad:
	/*
	 * Correctly set the bio to indicate a failed tranfer.
	 */
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static void xb_quiesce(struct blkfront_info *info);
// Quiesce the disk writes for a dump file before allowing the next buffer.
static void
xb_quiesce(struct blkfront_info *info)
{
	int		mtd;

	// While there are outstanding requests
	while (blkif_queued_requests) {
		RING_FINAL_CHECK_FOR_RESPONSES(&info->ring, mtd);
		if (mtd) {
			// Recieved request completions, update queue.
			blkif_int(info);
		}
		if (blkif_queued_requests) {
			// Still pending requests, wait for the disk i/o to complete
			HYPERVISOR_yield();
		}
	}
}

// Some bio structures for dumping core
#define DUMP_BIO_NO 16				// 16 * 4KB = 64KB dump block
static	struct bio		xb_dump_bp[DUMP_BIO_NO];

// Kernel dump function for a paravirtualized disk device
static int
xb_dump(void *arg, void *virtual, vm_offset_t physical, off_t offset,
        size_t length)
{
			int				 sbp;
  			int			     mbp;
			size_t			 chunk;
	struct	disk   			*dp = arg;
	struct	xb_softc		*sc = (struct xb_softc *) dp->d_drv1;
	        int	    		 rc = 0;

	xb_quiesce(sc->xb_info);		// All quiet on the western front.
	if (length > 0) {
		// If this lock is held, then this module is failing, and a successful
		// kernel dump is highly unlikely anyway.
		mtx_lock(&blkif_io_lock);
		// Split the 64KB block into 16 4KB blocks
		for (sbp=0; length>0 && sbp<DUMP_BIO_NO; sbp++) {
			chunk = length > PAGE_SIZE ? PAGE_SIZE : length;
			xb_dump_bp[sbp].bio_disk   = dp;
			xb_dump_bp[sbp].bio_pblkno = offset / dp->d_sectorsize;
			xb_dump_bp[sbp].bio_bcount = chunk;
			xb_dump_bp[sbp].bio_resid  = chunk;
			xb_dump_bp[sbp].bio_data   = virtual;
			xb_dump_bp[sbp].bio_cmd    = BIO_WRITE;
			xb_dump_bp[sbp].bio_done   = NULL;

			bioq_disksort(&sc->xb_bioq, &xb_dump_bp[sbp]);

			length -= chunk;
			offset += chunk;
			virtual = (char *) virtual + chunk;
		}
		// Tell DOM0 to do the I/O
		xb_startio(sc);
		mtx_unlock(&blkif_io_lock);

		// Must wait for the completion: the dump routine reuses the same
		//                               16 x 4KB buffer space.
		xb_quiesce(sc->xb_info);	// All quite on the eastern front
		// If there were any errors, bail out...
		for (mbp=0; mbp<sbp; mbp++) {
			if ((rc = xb_dump_bp[mbp].bio_error)) break;
		}
	}
	return (rc);
}


static int
blkfront_probe(device_t dev)
{

	if (!strcmp(xenbus_get_type(dev), "vbd")) {
		device_set_desc(dev, "Virtual Block Device");
		device_quiet(dev);
		return (0);
	}

	return (ENXIO);
}

/*
 * Setup supplies the backend dir, virtual device.  We place an event
 * channel and shared frame entries.  We watch backend to wait if it's
 * ok.
 */
static int
blkfront_attach(device_t dev)
{
	int error, vdevice, i, unit;
	struct blkfront_info *info;
	const char *name;

	/* FIXME: Use dynamic device id if this is not set. */
	error = xenbus_scanf(XBT_NIL, xenbus_get_node(dev),
	    "virtual-device", NULL, "%i", &vdevice);
	if (error) {
		xenbus_dev_fatal(dev, error, "reading virtual-device");
		printf("couldn't find virtual device");
		return (error);
	}

	blkfront_vdevice_to_unit(vdevice, &unit, &name);
	if (!strcmp(name, "xbd"))
		device_set_unit(dev, unit);

	info = device_get_softc(dev);
	
	/*
	 * XXX debug only
	 */
	for (i = 0; i < sizeof(*info); i++)
			if (((uint8_t *)info)[i] != 0)
					panic("non-null memory");

	info->shadow_free = 0;
	info->xbdev = dev;
	info->vdevice = vdevice;
	info->connected = BLKIF_STATE_DISCONNECTED;

	/* work queue needed ? */
	for (i = 0; i < BLK_RING_SIZE; i++)
		info->shadow[i].req.id = i+1;
	info->shadow[BLK_RING_SIZE-1].req.id = 0x0fffffff;

	/* Front end dir is a number, which is used as the id. */
	info->handle = strtoul(strrchr(xenbus_get_node(dev),'/')+1, NULL, 0);

	error = talk_to_backend(dev, info);
	if (error)
		return (error);

	return (0);
}

static int
blkfront_suspend(device_t dev)
{
	struct blkfront_info *info = device_get_softc(dev);

	/* Prevent new requests being issued until we fix things up. */
	mtx_lock(&blkif_io_lock);
	info->connected = BLKIF_STATE_SUSPENDED;
	mtx_unlock(&blkif_io_lock);

	return (0);
}

static int
blkfront_resume(device_t dev)
{
	struct blkfront_info *info = device_get_softc(dev);
	int err;

	DPRINTK("blkfront_resume: %s\n", xenbus_get_node(dev));

	blkif_free(info, 1);
	err = talk_to_backend(dev, info);
	if (info->connected == BLKIF_STATE_SUSPENDED && !err)
		blkif_recover(info);

	return (err);
}

/* Common code used when first setting up, and when resuming. */
static int
talk_to_backend(device_t dev, struct blkfront_info *info)
{
	const char *message = NULL;
	struct xenbus_transaction xbt;
	int err;

	/* Create shared ring, alloc event channel. */
	err = setup_blkring(dev, info);
	if (err)
		goto out;

 again:
	err = xenbus_transaction_start(&xbt);
	if (err) {
		xenbus_dev_fatal(dev, err, "starting transaction");
		goto destroy_blkring;
	}

	err = xenbus_printf(xbt, xenbus_get_node(dev),
			    "ring-ref","%u", info->ring_ref);
	if (err) {
		message = "writing ring-ref";
		goto abort_transaction;
	}
	err = xenbus_printf(xbt, xenbus_get_node(dev),
		"event-channel", "%u", irq_to_evtchn_port(info->irq));
	if (err) {
		message = "writing event-channel";
		goto abort_transaction;
	}
	err = xenbus_printf(xbt, xenbus_get_node(dev),
		"protocol", "%s", XEN_IO_PROTO_ABI_NATIVE);
	if (err) {
		message = "writing protocol";
		goto abort_transaction;
	}

	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, err, "completing transaction");
		goto destroy_blkring;
	}
	xenbus_set_state(dev, XenbusStateInitialised);
	
	return 0;

 abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (message)
		xenbus_dev_fatal(dev, err, "%s", message);
 destroy_blkring:
	blkif_free(info, 0);
 out:
	return err;
}

static int 
setup_blkring(device_t dev, struct blkfront_info *info)
{
	blkif_sring_t *sring;
	int error;

	info->ring_ref = GRANT_INVALID_REF;

	sring = (blkif_sring_t *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sring == NULL) {
		xenbus_dev_fatal(dev, ENOMEM, "allocating shared ring");
		return ENOMEM;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&info->ring, sring, PAGE_SIZE);

	error = xenbus_grant_ring(dev,
	    (vtomach(info->ring.sring) >> PAGE_SHIFT), &info->ring_ref);
	if (error) {
		free(sring, M_DEVBUF);
		info->ring.sring = NULL;
		goto fail;
	}
	
	error = bind_listening_port_to_irqhandler(xenbus_get_otherend_id(dev),
	    "xbd", (driver_intr_t *)blkif_int, info,
	    INTR_TYPE_BIO | INTR_MPSAFE, &info->irq);
	if (error) {
		xenbus_dev_fatal(dev, error,
		    "bind_evtchn_to_irqhandler failed");
		goto fail;
	}

	return (0);
 fail:
	blkif_free(info, 0);
	return (error);
}


/**
 * Callback received when the backend's state changes.
 */
static int
blkfront_backend_changed(device_t dev, XenbusState backend_state)
{
	struct blkfront_info *info = device_get_softc(dev);

	DPRINTK("backend_state=%d\n", backend_state);

	switch (backend_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
	case XenbusStateClosed:
	case XenbusStateReconfigured:
	case XenbusStateReconfiguring:
		break;

	case XenbusStateConnected:
		connect(dev, info);
		break;

	case XenbusStateClosing:
		if (info->users > 0)
			xenbus_dev_error(dev, -EBUSY,
					 "Device in use; refusing to close");
		else
			blkfront_closing(dev);
#ifdef notyet
		bd = bdget(info->dev);
		if (bd == NULL)
			xenbus_dev_fatal(dev, -ENODEV, "bdget failed");

		down(&bd->bd_sem);
		if (info->users > 0)
			xenbus_dev_error(dev, -EBUSY,
					 "Device in use; refusing to close");
		else
			blkfront_closing(dev);
		up(&bd->bd_sem);
		bdput(bd);
#endif
	}

	return (0);
}

/* 
** Invoked when the backend is finally 'ready' (and has told produced 
** the details about the physical device - #sectors, size, etc). 
*/
static void 
connect(device_t dev, struct blkfront_info *info)
{
	unsigned long sectors, sector_size;
	unsigned int binfo;
	int err;

        if( (info->connected == BLKIF_STATE_CONNECTED) || 
	    (info->connected == BLKIF_STATE_SUSPENDED) )
		return;

	DPRINTK("blkfront.c:connect:%s.\n", xenbus_get_otherend_path(dev));

	err = xenbus_gather(XBT_NIL, xenbus_get_otherend_path(dev),
			    "sectors", "%lu", &sectors,
			    "info", "%u", &binfo,
			    "sector-size", "%lu", &sector_size,
			    NULL);
	if (err) {
		xenbus_dev_fatal(dev, err,
		    "reading backend fields at %s",
		    xenbus_get_otherend_path(dev));
		return;
	}
	err = xenbus_gather(XBT_NIL, xenbus_get_otherend_path(dev),
			    "feature-barrier", "%lu", &info->feature_barrier,
			    NULL);
	if (err)
		info->feature_barrier = 0;

	device_printf(dev, "%juMB <%s> at %s",
	    (uintmax_t) sectors / (1048576 / sector_size),
	    device_get_desc(dev),
	    xenbus_get_node(dev));
	bus_print_child_footer(device_get_parent(dev), dev);

	xlvbd_add(dev, sectors, info->vdevice, binfo, sector_size, info);

	(void)xenbus_set_state(dev, XenbusStateConnected); 

	/* Kick pending requests. */
	mtx_lock(&blkif_io_lock);
	info->connected = BLKIF_STATE_CONNECTED;
	kick_pending_request_queues(info);
	mtx_unlock(&blkif_io_lock);
	info->is_ready = 1;
	
#if 0
	add_disk(info->gd);
#endif
}

/**
 * Handle the change of state of the backend to Closing.  We must delete our
 * device-layer structures now, to ensure that writes are flushed through to
 * the backend.  Once is this done, we can switch to Closed in
 * acknowledgement.
 */
static void
blkfront_closing(device_t dev)
{
	struct blkfront_info *info = device_get_softc(dev);

	DPRINTK("blkfront_closing: %s removed\n", xenbus_get_node(dev));

	if (info->mi) {
		DPRINTK("Calling xlvbd_del\n");
		xlvbd_del(info);
		info->mi = NULL;
	}

	xenbus_set_state(dev, XenbusStateClosed);
}


static int
blkfront_detach(device_t dev)
{
	struct blkfront_info *info = device_get_softc(dev);

	DPRINTK("blkfront_remove: %s removed\n", xenbus_get_node(dev));

	blkif_free(info, 0);

	return 0;
}


static inline int 
GET_ID_FROM_FREELIST(struct blkfront_info *info)
{
	unsigned long nfree = info->shadow_free;
	
	KASSERT(nfree <= BLK_RING_SIZE, ("free %lu > RING_SIZE", nfree));
	info->shadow_free = info->shadow[nfree].req.id;
	info->shadow[nfree].req.id = 0x0fffffee; /* debug */
	atomic_add_int(&blkif_queued_requests, 1);
	return nfree;
}

static inline void 
ADD_ID_TO_FREELIST(struct blkfront_info *info, unsigned long id)
{
	info->shadow[id].req.id  = info->shadow_free;
	info->shadow[id].request = 0;
	info->shadow_free = id;
	atomic_subtract_int(&blkif_queued_requests, 1);
}

static inline void 
flush_requests(struct blkfront_info *info)
{
	int notify;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&info->ring, notify);

	if (notify)
		notify_remote_via_irq(info->irq);
}

static void 
kick_pending_request_queues(struct blkfront_info *info)
{
	/* XXX check if we can't simplify */
#if 0
	if (!RING_FULL(&info->ring)) {
		/* Re-enable calldowns. */
		blk_start_queue(info->rq);
		/* Kick things off immediately. */
		do_blkif_request(info->rq);
	}
#endif
	if (!RING_FULL(&info->ring)) {
#if 0
		sc = LIST_FIRST(&xbsl_head);
		LIST_REMOVE(sc, entry);
		/* Re-enable calldowns. */
		blk_start_queue(di->rq);
#endif
		/* Kick things off immediately. */
		xb_startio(info->sc);
	}
}

#if 0
/* XXX */
static void blkif_restart_queue(void *arg)
{
	struct blkfront_info *info = (struct blkfront_info *)arg;

	mtx_lock(&blkif_io_lock);
	kick_pending_request_queues(info);
	mtx_unlock(&blkif_io_lock);
}
#endif

static void blkif_restart_queue_callback(void *arg)
{
#if 0
	struct blkfront_info *info = (struct blkfront_info *)arg;
	/* XXX BSD equiv ? */

	schedule_work(&info->work);
#endif
}

static int
blkif_open(struct disk *dp)
{
	struct xb_softc	*sc = (struct xb_softc *)dp->d_drv1;

	if (sc == NULL) {
		printf("xb%d: not found", sc->xb_unit);
		return (ENXIO);
	}

	sc->xb_flags |= XB_OPEN;
	sc->xb_info->users++;
	return (0);
}

static int
blkif_close(struct disk *dp)
{
	struct xb_softc	*sc = (struct xb_softc *)dp->d_drv1;

	if (sc == NULL)
		return (ENXIO);
	sc->xb_flags &= ~XB_OPEN;
	if (--(sc->xb_info->users) == 0) {
		/* Check whether we have been instructed to close.  We will
		   have ignored this request initially, as the device was
		   still mounted. */
		device_t dev = sc->xb_info->xbdev;
		XenbusState state =
			xenbus_read_driver_state(xenbus_get_otherend_path(dev));

		if (state == XenbusStateClosing)
			blkfront_closing(dev);
	}
	return (0);
}

static int
blkif_ioctl(struct disk *dp, u_long cmd, void *addr, int flag, struct thread *td)
{
	struct xb_softc	*sc = (struct xb_softc *)dp->d_drv1;

	if (sc == NULL)
		return (ENXIO);

	return (ENOTTY);
}


/*
 * blkif_queue_request
 *
 * request block io
 * 
 * id: for guest use only.
 * operation: BLKIF_OP_{READ,WRITE,PROBE}
 * buffer: buffer to read/write into. this should be a
 *   virtual address in the guest os.
 */
static int blkif_queue_request(struct bio *bp)
{
	caddr_t alignbuf;
	vm_paddr_t buffer_ma;
	blkif_request_t     *ring_req;
	unsigned long id;
	uint64_t fsect, lsect;
	struct xb_softc *sc = (struct xb_softc *)bp->bio_disk->d_drv1;
	struct blkfront_info *info = sc->xb_info;
	int ref;

	if (unlikely(sc->xb_info->connected != BLKIF_STATE_CONNECTED))
		return 1;

	if (gnttab_alloc_grant_references(
		    BLKIF_MAX_SEGMENTS_PER_REQUEST, &gref_head) < 0) {
		gnttab_request_free_callback(
			&info->callback,
			blkif_restart_queue_callback,
			info,
			BLKIF_MAX_SEGMENTS_PER_REQUEST);
		return 1;
	}

	/* Check if the buffer is properly aligned */
	if ((vm_offset_t)bp->bio_data & PAGE_MASK) {
		int align = (bp->bio_bcount < PAGE_SIZE/2) ? XBD_SECTOR_SIZE : 
			PAGE_SIZE;
		caddr_t newbuf = malloc(bp->bio_bcount + align, M_DEVBUF, 
					M_NOWAIT);

		alignbuf = (char *)roundup2((u_long)newbuf, align);

		/* save a copy of the current buffer */
		bp->bio_driver1 = newbuf;
		bp->bio_driver2 = alignbuf;

		/* Copy the data for a write */
		if (bp->bio_cmd == BIO_WRITE)
			bcopy(bp->bio_data, alignbuf, bp->bio_bcount);
	} else
		alignbuf = bp->bio_data;
	
	/* Fill out a communications ring structure. */
	ring_req 	         = RING_GET_REQUEST(&info->ring, 
						    info->ring.req_prod_pvt);
	id		         = GET_ID_FROM_FREELIST(info);
	info->shadow[id].request = (unsigned long)bp;
	
	ring_req->id 	         = id;
	ring_req->operation 	 = (bp->bio_cmd == BIO_READ) ? BLKIF_OP_READ :
		BLKIF_OP_WRITE;
	
	ring_req->sector_number= (blkif_sector_t)bp->bio_pblkno;
	ring_req->handle 	  = (blkif_vdev_t)(uintptr_t)sc->xb_disk;
	
	ring_req->nr_segments  = 0;	/* XXX not doing scatter/gather since buffer
					 * chaining is not supported.
					 */

	buffer_ma = vtomach(alignbuf);
	fsect = (buffer_ma & PAGE_MASK) >> XBD_SECTOR_SHFT;
	lsect = fsect + (bp->bio_bcount >> XBD_SECTOR_SHFT) - 1;
	/* install a grant reference. */
	ref = gnttab_claim_grant_reference(&gref_head);
	KASSERT( ref != -ENOSPC, ("grant_reference failed") );

	gnttab_grant_foreign_access_ref(
		ref,
		xenbus_get_otherend_id(info->xbdev),
		buffer_ma >> PAGE_SHIFT,
		ring_req->operation & 1 ); /* ??? */
	info->shadow[id].frame[ring_req->nr_segments] = 
		buffer_ma >> PAGE_SHIFT;

	ring_req->seg[ring_req->nr_segments] =
		(struct blkif_request_segment) {
			.gref       = ref,
			.first_sect = fsect, 
			.last_sect  = lsect };

	ring_req->nr_segments++;
	KASSERT((buffer_ma & (XBD_SECTOR_SIZE-1)) == 0,
		("XEN buffer must be sector aligned"));
	KASSERT(lsect <= 7, 
		("XEN disk driver data cannot cross a page boundary"));
	
	buffer_ma &= ~PAGE_MASK;

	info->ring.req_prod_pvt++;

	/* Keep a private copy so we can reissue requests when recovering. */
	info->shadow[id].req = *ring_req;

	gnttab_free_grant_references(gref_head);

	return 0;
}



/*
 * Dequeue buffers and place them in the shared communication ring.
 * Return when no more requests can be accepted or all buffers have 
 * been queued.
 *
 * Signal XEN once the ring has been filled out.
 */
static void
xb_startio(struct xb_softc *sc)
{
	struct bio		*bp;
	int			queued = 0;
	struct blkfront_info *info = sc->xb_info;
	DPRINTK("");

	mtx_assert(&blkif_io_lock, MA_OWNED);

	while ((bp = bioq_takefirst(&sc->xb_bioq)) != NULL) {

		if (RING_FULL(&info->ring)) 
			goto wait;
    	
		if (blkif_queue_request(bp)) {
		wait:
			bioq_insert_head(&sc->xb_bioq, bp);
			break;
		}
		queued++;
	}

	if (queued != 0) 
		flush_requests(sc->xb_info);
}

static void
blkif_int(void *xsc)
{
	struct xb_softc *sc = NULL;
	struct bio *bp;
	blkif_response_t *bret;
	RING_IDX i, rp;
	struct blkfront_info *info = xsc;
	DPRINTK("");

	TRACE_ENTER;

	mtx_lock(&blkif_io_lock);

	if (unlikely(info->connected != BLKIF_STATE_CONNECTED)) {
		mtx_unlock(&blkif_io_lock);
		return;
	}

 again:
	rp = info->ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = info->ring.rsp_cons; i != rp; i++) {
		unsigned long id;

		bret = RING_GET_RESPONSE(&info->ring, i);
		id   = bret->id;
		bp   = (struct bio *)info->shadow[id].request;

		blkif_completion(&info->shadow[id]);

		ADD_ID_TO_FREELIST(info, id);

		switch (bret->operation) {
		case BLKIF_OP_READ:
			/* had an unaligned buffer that needs to be copied */
			if (bp->bio_driver1)
				bcopy(bp->bio_driver2, bp->bio_data, bp->bio_bcount);
			/* FALLTHROUGH */
		case BLKIF_OP_WRITE:

			/* free the copy buffer */
			if (bp->bio_driver1) {
				free(bp->bio_driver1, M_DEVBUF);
				bp->bio_driver1 = NULL;
			}

			if ( unlikely(bret->status != BLKIF_RSP_OKAY) ) {
					printf("Bad return from blkdev data request: %x\n", 
					  bret->status);
				bp->bio_flags |= BIO_ERROR;
			}

			sc = (struct xb_softc *)bp->bio_disk->d_drv1;

			if (bp->bio_flags & BIO_ERROR)
				bp->bio_error = EIO;
			else
				bp->bio_resid = 0;

			biodone(bp);
			break;
		default:
			panic("received invalid operation");
			break;
		}
	}

	info->ring.rsp_cons = i;

	if (i != info->ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&info->ring, more_to_do);
		if (more_to_do)
			goto again;
	} else {
		info->ring.sring->rsp_event = i + 1;
	}

	kick_pending_request_queues(info);

	mtx_unlock(&blkif_io_lock);
}

static void 
blkif_free(struct blkfront_info *info, int suspend)
{
	
/* Prevent new requests being issued until we fix things up. */
	mtx_lock(&blkif_io_lock);
	info->connected = suspend ? 
		BLKIF_STATE_SUSPENDED : BLKIF_STATE_DISCONNECTED; 
	mtx_unlock(&blkif_io_lock);

	/* Free resources associated with old device channel. */
	if (info->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(info->ring_ref, 
					  info->ring.sring);
		info->ring_ref = GRANT_INVALID_REF;
		info->ring.sring = NULL;
	}
	if (info->irq)
		unbind_from_irqhandler(info->irq);
	info->irq = 0;

}

static void 
blkif_completion(struct blk_shadow *s)
{
	int i;

	for (i = 0; i < s->req.nr_segments; i++)
		gnttab_end_foreign_access(s->req.seg[i].gref, 0UL);
}

static void 
blkif_recover(struct blkfront_info *info)
{
	int i, j;
	blkif_request_t *req;
	struct blk_shadow *copy;

	if (!info->sc)
		return;

	/* Stage 1: Make a safe copy of the shadow state. */
	copy = (struct blk_shadow *)malloc(sizeof(info->shadow), M_DEVBUF, M_NOWAIT|M_ZERO);
	memcpy(copy, info->shadow, sizeof(info->shadow));

	/* Stage 2: Set up free list. */
	memset(&info->shadow, 0, sizeof(info->shadow));
	for (i = 0; i < BLK_RING_SIZE; i++)
		info->shadow[i].req.id = i+1;
	info->shadow_free = info->ring.req_prod_pvt;
	info->shadow[BLK_RING_SIZE-1].req.id = 0x0fffffff;

	/* Stage 3: Find pending requests and requeue them. */
	for (i = 0; i < BLK_RING_SIZE; i++) {
		/* Not in use? */
		if (copy[i].request == 0)
			continue;

		/* Grab a request slot and copy shadow state into it. */
		req = RING_GET_REQUEST(
			&info->ring, info->ring.req_prod_pvt);
		*req = copy[i].req;

		/* We get a new request id, and must reset the shadow state. */
		req->id = GET_ID_FROM_FREELIST(info);
		memcpy(&info->shadow[req->id], &copy[i], sizeof(copy[i]));

		/* Rewrite any grant references invalidated by suspend/resume. */
		for (j = 0; j < req->nr_segments; j++)
			gnttab_grant_foreign_access_ref(
				req->seg[j].gref,
				xenbus_get_otherend_id(info->xbdev),
				pfn_to_mfn(info->shadow[req->id].frame[j]),
				0 /* assume not readonly */);

		info->shadow[req->id].req = *req;

		info->ring.req_prod_pvt++;
	}

	free(copy, M_DEVBUF);

	xenbus_set_state(info->xbdev, XenbusStateConnected); 
	
	/* Now safe for us to use the shared ring */
	mtx_lock(&blkif_io_lock);
	info->connected = BLKIF_STATE_CONNECTED;
	mtx_unlock(&blkif_io_lock);

	/* Send off requeued requests */
	mtx_lock(&blkif_io_lock);
	flush_requests(info);

	/* Kick any other new requests queued since we resumed */
	kick_pending_request_queues(info);
	mtx_unlock(&blkif_io_lock);
}

/* ** Driver registration ** */
static device_method_t blkfront_methods[] = { 
	/* Device interface */ 
	DEVMETHOD(device_probe,         blkfront_probe), 
	DEVMETHOD(device_attach,        blkfront_attach), 
	DEVMETHOD(device_detach,        blkfront_detach), 
	DEVMETHOD(device_shutdown,      bus_generic_shutdown), 
	DEVMETHOD(device_suspend,       blkfront_suspend), 
	DEVMETHOD(device_resume,        blkfront_resume), 
 
	/* Xenbus interface */
	DEVMETHOD(xenbus_backend_changed, blkfront_backend_changed),

	{ 0, 0 } 
}; 

static driver_t blkfront_driver = { 
	"xbd", 
	blkfront_methods, 
	sizeof(struct blkfront_info),                      
}; 
devclass_t blkfront_devclass; 
 
DRIVER_MODULE(xbd, xenbus, blkfront_driver, blkfront_devclass, 0, 0); 

MTX_SYSINIT(ioreq, &blkif_io_lock, "BIO LOCK", MTX_NOWITNESS); /* XXX how does one enroll a lock? */

