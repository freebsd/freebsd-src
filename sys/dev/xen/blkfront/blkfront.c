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
#include <sys/bus_dma.h>

#include <machine/_inttypes.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>
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

/* prototypes */
static void xb_free_command(struct xb_command *cm);
static void xb_startio(struct xb_softc *sc);
static void blkfront_connect(struct xb_softc *);
static void blkfront_closing(device_t);
static int blkfront_detach(device_t);
static int setup_blkring(struct xb_softc *);
static void blkif_int(void *);
static void blkfront_initialize(struct xb_softc *);
static int blkif_completion(struct xb_command *);
static void blkif_free(struct xb_softc *);
static void blkif_queue_cb(void *, bus_dma_segment_t *, int, int);

MALLOC_DEFINE(M_XENBLOCKFRONT, "xbd", "Xen Block Front driver data");

#define GRANT_INVALID_REF 0

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

#if 0
#define DPRINTK(fmt, args...) printf("[XEN] %s:%d: " fmt ".\n", __func__, __LINE__, ##args)
#else
#define DPRINTK(fmt, args...) 
#endif

static int blkif_open(struct disk *dp);
static int blkif_close(struct disk *dp);
static int blkif_ioctl(struct disk *dp, u_long cmd, void *addr, int flag, struct thread *td);
static int blkif_queue_request(struct xb_softc *sc, struct xb_command *cm);
static void xb_strategy(struct bio *bp);

// In order to quiesce the device during kernel dumps, outstanding requests to
// DOM0 for disk reads/writes need to be accounted for.
static	int	xb_dump(void *, void *, vm_offset_t, off_t, size_t);

/* XXX move to xb_vbd.c when VBD update support is added */
#define MAX_VBDS 64

#define XBD_SECTOR_SIZE		512	/* XXX: assume for now */
#define XBD_SECTOR_SHFT		9

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
xlvbd_add(struct xb_softc *sc, blkif_sector_t sectors,
    int vdevice, uint16_t vdisk_info, unsigned long sector_size)
{
	int	unit, error = 0;
	const char *name;

	blkfront_vdevice_to_unit(vdevice, &unit, &name);

	sc->xb_unit = unit;

	if (strcmp(name, "xbd"))
		device_printf(sc->xb_dev, "attaching as %s%d\n", name, unit);

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

	sc->xb_disk->d_mediasize = sectors * sector_size;
	sc->xb_disk->d_maxsize = sc->max_request_size;
	sc->xb_disk->d_flags = 0;
	disk_create(sc->xb_disk, DISK_VERSION_00);

	return error;
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
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
		return;
	}

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	mtx_lock(&sc->xb_io_lock);

	xb_enqueue_bio(sc, bp);
	xb_startio(sc);

	mtx_unlock(&sc->xb_io_lock);
	return;
}

static void
xb_bio_complete(struct xb_softc *sc, struct xb_command *cm)
{
	struct bio *bp;

	bp = cm->bp;

	if ( unlikely(cm->status != BLKIF_RSP_OKAY) ) {
		disk_err(bp, "disk error" , -1, 0);
		printf(" status: %x\n", cm->status);
		bp->bio_flags |= BIO_ERROR;
	}

	if (bp->bio_flags & BIO_ERROR)
		bp->bio_error = EIO;
	else
		bp->bio_resid = 0;

	xb_free_command(cm);
	biodone(bp);
}

// Quiesce the disk writes for a dump file before allowing the next buffer.
static void
xb_quiesce(struct xb_softc *sc)
{
	int		mtd;

	// While there are outstanding requests
	while (!TAILQ_EMPTY(&sc->cm_busy)) {
		RING_FINAL_CHECK_FOR_RESPONSES(&sc->ring, mtd);
		if (mtd) {
			/* Recieved request completions, update queue. */
			blkif_int(sc);
		}
		if (!TAILQ_EMPTY(&sc->cm_busy)) {
			/*
			 * Still pending requests, wait for the disk i/o
			 * to complete.
			 */
			HYPERVISOR_yield();
		}
	}
}

/* Kernel dump function for a paravirtualized disk device */
static void
xb_dump_complete(struct xb_command *cm)
{

	xb_enqueue_complete(cm);
}

static int
xb_dump(void *arg, void *virtual, vm_offset_t physical, off_t offset,
        size_t length)
{
	struct	disk   	*dp = arg;
	struct xb_softc	*sc = (struct xb_softc *) dp->d_drv1;
	struct xb_command *cm;
	size_t		chunk;
	int		sbp;
	int		rc = 0;

	if (length <= 0)
		return (rc);

	xb_quiesce(sc);	/* All quiet on the western front. */

	/*
	 * If this lock is held, then this module is failing, and a
	 * successful kernel dump is highly unlikely anyway.
	 */
	mtx_lock(&sc->xb_io_lock);

	/* Split the 64KB block as needed */
	for (sbp=0; length > 0; sbp++) {
		cm = xb_dequeue_free(sc);
		if (cm == NULL) {
			mtx_unlock(&sc->xb_io_lock);
			device_printf(sc->xb_dev, "dump: no more commands?\n");
			return (EBUSY);
		}

		if (gnttab_alloc_grant_references(sc->max_request_segments,
						  &cm->gref_head) != 0) {
			xb_free_command(cm);
			mtx_unlock(&sc->xb_io_lock);
			device_printf(sc->xb_dev, "no more grant allocs?\n");
			return (EBUSY);
		}

		chunk = length > sc->max_request_size
		      ? sc->max_request_size : length;
		cm->data = virtual;
		cm->datalen = chunk;
		cm->operation = BLKIF_OP_WRITE;
		cm->sector_number = offset / dp->d_sectorsize;
		cm->cm_complete = xb_dump_complete;

		xb_enqueue_ready(cm);

		length -= chunk;
		offset += chunk;
		virtual = (char *) virtual + chunk;
	}

	/* Tell DOM0 to do the I/O */
	xb_startio(sc);
	mtx_unlock(&sc->xb_io_lock);

	/* Poll for the completion. */
	xb_quiesce(sc);	/* All quite on the eastern front */

	/* If there were any errors, bail out... */
	while ((cm = xb_dequeue_complete(sc)) != NULL) {
		if (cm->status != BLKIF_RSP_OKAY) {
			device_printf(sc->xb_dev,
			    "Dump I/O failed at sector %jd\n",
			    cm->sector_number);
			rc = EIO;
		}
		xb_free_command(cm);
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
	struct xb_softc *sc;
	const char *name;
	int error;
	int vdevice;
	int i;
	int unit;

	/* FIXME: Use dynamic device id if this is not set. */
	error = xs_scanf(XST_NIL, xenbus_get_node(dev),
	    "virtual-device", NULL, "%i", &vdevice);
	if (error) {
		xenbus_dev_fatal(dev, error, "reading virtual-device");
		device_printf(dev, "Couldn't determine virtual device.\n");
		return (error);
	}

	blkfront_vdevice_to_unit(vdevice, &unit, &name);
	if (!strcmp(name, "xbd"))
		device_set_unit(dev, unit);

	sc = device_get_softc(dev);
	mtx_init(&sc->xb_io_lock, "blkfront i/o lock", NULL, MTX_DEF);
	xb_initq_free(sc);
	xb_initq_busy(sc);
	xb_initq_ready(sc);
	xb_initq_complete(sc);
	xb_initq_bio(sc);
	for (i = 0; i < XBF_MAX_RING_PAGES; i++)
		sc->ring_ref[i] = GRANT_INVALID_REF;

	sc->xb_dev = dev;
	sc->vdevice = vdevice;
	sc->connected = BLKIF_STATE_DISCONNECTED;

	/* Wait for backend device to publish its protocol capabilities. */
	xenbus_set_state(dev, XenbusStateInitialising);

	return (0);
}

static int
blkfront_suspend(device_t dev)
{
	struct xb_softc *sc = device_get_softc(dev);
	int retval;
	int saved_state;

	/* Prevent new requests being issued until we fix things up. */
	mtx_lock(&sc->xb_io_lock);
	saved_state = sc->connected;
	sc->connected = BLKIF_STATE_SUSPENDED;

	/* Wait for outstanding I/O to drain. */
	retval = 0;
	while (TAILQ_EMPTY(&sc->cm_busy) == 0) {
		if (msleep(&sc->cm_busy, &sc->xb_io_lock,
			   PRIBIO, "blkf_susp", 30 * hz) == EWOULDBLOCK) {
			retval = EBUSY;
			break;
		}
	}
	mtx_unlock(&sc->xb_io_lock);

	if (retval != 0)
		sc->connected = saved_state;

	return (retval);
}

static int
blkfront_resume(device_t dev)
{
	struct xb_softc *sc = device_get_softc(dev);

	DPRINTK("blkfront_resume: %s\n", xenbus_get_node(dev));

	blkif_free(sc);
	blkfront_initialize(sc);
	return (0);
}

static void
blkfront_initialize(struct xb_softc *sc)
{
	const char *otherend_path;
	const char *node_path;
	int error;
	int i;

	if (xenbus_get_state(sc->xb_dev) != XenbusStateInitialising) {
		/* Initialization has already been performed. */
		return;
	}

	/*
	 * Protocol defaults valid even if negotiation for a
	 * setting fails.
	 */
	sc->ring_pages = 1;
	sc->max_requests = BLKIF_MAX_RING_REQUESTS(PAGE_SIZE);
	sc->max_request_segments = BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK;
	sc->max_request_size = (sc->max_request_segments - 1) * PAGE_SIZE;
	sc->max_request_blocks = BLKIF_SEGS_TO_BLOCKS(sc->max_request_segments);

	/*
	 * Protocol negotiation.
	 *
	 * \note xs_gather() returns on the first encountered error, so
	 *       we must use independant calls in order to guarantee
	 *       we don't miss information in a sparsly populated back-end
	 *       tree.
	 */
	otherend_path = xenbus_get_otherend_path(sc->xb_dev);
	node_path = xenbus_get_node(sc->xb_dev);
	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-ring-pages", NULL, "%" PRIu32,
		       &sc->ring_pages);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-requests", NULL, "%" PRIu32,
		       &sc->max_requests);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-request-segments", NULL, "%" PRIu32,
		       &sc->max_request_segments);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-request-size", NULL, "%" PRIu32,
		       &sc->max_request_size);

	if (sc->ring_pages > XBF_MAX_RING_PAGES) {
		device_printf(sc->xb_dev, "Back-end specified ring-pages of "
			      "%u limited to front-end limit of %zu.\n",
			      sc->ring_pages, XBF_MAX_RING_PAGES);
		sc->ring_pages = XBF_MAX_RING_PAGES;
	}

	if (sc->max_requests > XBF_MAX_REQUESTS) {
		device_printf(sc->xb_dev, "Back-end specified max_requests of "
			      "%u limited to front-end limit of %u.\n",
			      sc->max_requests, XBF_MAX_REQUESTS);
		sc->max_requests = XBF_MAX_REQUESTS;
	}

	if (sc->max_request_segments > XBF_MAX_SEGMENTS_PER_REQUEST) {
		device_printf(sc->xb_dev, "Back-end specificed "
			      "max_requests_segments of %u limited to "
			      "front-end limit of %u.\n",
			      sc->max_request_segments,
			      XBF_MAX_SEGMENTS_PER_REQUEST);
		sc->max_request_segments = XBF_MAX_SEGMENTS_PER_REQUEST;
	}

	if (sc->max_request_size > XBF_MAX_REQUEST_SIZE) {
		device_printf(sc->xb_dev, "Back-end specificed "
			      "max_request_size of %u limited to front-end "
			      "limit of %u.\n", sc->max_request_size,
			      XBF_MAX_REQUEST_SIZE);
		sc->max_request_size = XBF_MAX_REQUEST_SIZE;
	}
	sc->max_request_blocks = BLKIF_SEGS_TO_BLOCKS(sc->max_request_segments);

	/* Allocate datastructures based on negotiated values. */
	error = bus_dma_tag_create(NULL,		/* parent */
				   512, PAGE_SIZE,	/* algnmnt, boundary */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   sc->max_request_size,
				   sc->max_request_segments,
				   PAGE_SIZE,		/* maxsegsize */
				   BUS_DMA_ALLOCNOW,	/* flags */
				   busdma_lock_mutex,	/* lockfunc */
				   &sc->xb_io_lock,	/* lockarg */
				   &sc->xb_io_dmat);
	if (error != 0) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "Cannot allocate parent DMA tag\n");
		return;
	}

	/* Per-transaction data allocation. */
	sc->shadow = malloc(sizeof(*sc->shadow) * sc->max_requests,
			    M_XENBLOCKFRONT, M_NOWAIT|M_ZERO);
	if (sc->shadow == NULL) {
		bus_dma_tag_destroy(sc->xb_io_dmat);
		xenbus_dev_fatal(sc->xb_dev, error,
				 "Cannot allocate request structures\n");
		return;
	}

	for (i = 0; i < sc->max_requests; i++) {
		struct xb_command *cm;

		cm = &sc->shadow[i];
		cm->sg_refs = malloc(sizeof(grant_ref_t)
				   * sc->max_request_segments,
				     M_XENBLOCKFRONT, M_NOWAIT);
		if (cm->sg_refs == NULL)
			break;
		cm->id = i;
		cm->cm_sc = sc;
		if (bus_dmamap_create(sc->xb_io_dmat, 0, &cm->map) != 0)
			break;
		xb_free_command(cm);
	}

	if (setup_blkring(sc) != 0)
		return;

	error = xs_printf(XST_NIL, node_path,
			 "ring-pages","%u", sc->ring_pages);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/ring-pages",
				 node_path);
		return;
	}

	error = xs_printf(XST_NIL, node_path,
			 "max-requests","%u", sc->max_requests);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/max-requests",
				 node_path);
		return;
	}

	error = xs_printf(XST_NIL, node_path,
			 "max-request-segments","%u", sc->max_request_segments);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/max-request-segments",
				 node_path);
		return;
	}

	error = xs_printf(XST_NIL, node_path,
			 "max-request-size","%u", sc->max_request_size);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/max-request-size",
				 node_path);
		return;
	}

	error = xs_printf(XST_NIL, node_path, "event-channel",
			  "%u", irq_to_evtchn_port(sc->irq));
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/event-channel",
				 node_path);
		return;
	}

	error = xs_printf(XST_NIL, node_path,
			  "protocol", "%s", XEN_IO_PROTO_ABI_NATIVE);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
				 "writing %s/protocol",
				 node_path);
		return;
	}

	xenbus_set_state(sc->xb_dev, XenbusStateInitialised);
}

static int 
setup_blkring(struct xb_softc *sc)
{
	blkif_sring_t *sring;
	uintptr_t sring_page_addr;
	int error;
	int i;

	sring = malloc(sc->ring_pages * PAGE_SIZE, M_XENBLOCKFRONT,
		       M_NOWAIT|M_ZERO);
	if (sring == NULL) {
		xenbus_dev_fatal(sc->xb_dev, ENOMEM, "allocating shared ring");
		return (ENOMEM);
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&sc->ring, sring, sc->ring_pages * PAGE_SIZE);

	for (i = 0, sring_page_addr = (uintptr_t)sring;
	     i < sc->ring_pages;
	     i++, sring_page_addr += PAGE_SIZE) {

		error = xenbus_grant_ring(sc->xb_dev,
		    (vtomach(sring_page_addr) >> PAGE_SHIFT), &sc->ring_ref[i]);
		if (error) {
			xenbus_dev_fatal(sc->xb_dev, error,
					 "granting ring_ref(%d)", i);
			return (error);
		}
	}
	error = xs_printf(XST_NIL, xenbus_get_node(sc->xb_dev),
			  "ring-ref","%u", sc->ring_ref[0]);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error, "writing %s/ring-ref",
				 xenbus_get_node(sc->xb_dev));
		return (error);
	}
	for (i = 1; i < sc->ring_pages; i++) {
		char ring_ref_name[]= "ring_refXX";

		snprintf(ring_ref_name, sizeof(ring_ref_name), "ring-ref%u", i);
		error = xs_printf(XST_NIL, xenbus_get_node(sc->xb_dev),
				 ring_ref_name, "%u", sc->ring_ref[i]);
		if (error) {
			xenbus_dev_fatal(sc->xb_dev, error, "writing %s/%s",
					 xenbus_get_node(sc->xb_dev),
					 ring_ref_name);
			return (error);
		}
	}

	error = bind_listening_port_to_irqhandler(
	    xenbus_get_otherend_id(sc->xb_dev),
	    "xbd", (driver_intr_t *)blkif_int, sc,
	    INTR_TYPE_BIO | INTR_MPSAFE, &sc->irq);
	if (error) {
		xenbus_dev_fatal(sc->xb_dev, error,
		    "bind_evtchn_to_irqhandler failed");
		return (error);
	}

	return (0);
}

/**
 * Callback received when the backend's state changes.
 */
static void
blkfront_backend_changed(device_t dev, XenbusState backend_state)
{
	struct xb_softc *sc = device_get_softc(dev);

	DPRINTK("backend_state=%d\n", backend_state);

	switch (backend_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
	case XenbusStateReconfigured:
	case XenbusStateReconfiguring:
	case XenbusStateClosed:
		break;

	case XenbusStateInitWait:
	case XenbusStateInitialised:
		blkfront_initialize(sc);
		break;

	case XenbusStateConnected:
		blkfront_initialize(sc);
		blkfront_connect(sc);
		break;

	case XenbusStateClosing:
		if (sc->users > 0)
			xenbus_dev_error(dev, -EBUSY,
					 "Device in use; refusing to close");
		else
			blkfront_closing(dev);
		break;	
	}
}

/* 
** Invoked when the backend is finally 'ready' (and has published
** the details about the physical device - #sectors, size, etc). 
*/
static void 
blkfront_connect(struct xb_softc *sc)
{
	device_t dev = sc->xb_dev;
	unsigned long sectors, sector_size;
	unsigned int binfo;
	int err, feature_barrier;

        if( (sc->connected == BLKIF_STATE_CONNECTED) || 
	    (sc->connected == BLKIF_STATE_SUSPENDED) )
		return;

	DPRINTK("blkfront.c:connect:%s.\n", xenbus_get_otherend_path(dev));

	err = xs_gather(XST_NIL, xenbus_get_otherend_path(dev),
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
	err = xs_gather(XST_NIL, xenbus_get_otherend_path(dev),
			"feature-barrier", "%lu", &feature_barrier,
			NULL);
	if (!err || feature_barrier)
		sc->xb_flags |= XB_BARRIER;

	if (sc->xb_disk == NULL) {
		device_printf(dev, "%juMB <%s> at %s",
		    (uintmax_t) sectors / (1048576 / sector_size),
		    device_get_desc(dev),
		    xenbus_get_node(dev));
		bus_print_child_footer(device_get_parent(dev), dev);

		xlvbd_add(sc, sectors, sc->vdevice, binfo, sector_size);
	}

	(void)xenbus_set_state(dev, XenbusStateConnected); 

	/* Kick pending requests. */
	mtx_lock(&sc->xb_io_lock);
	sc->connected = BLKIF_STATE_CONNECTED;
	xb_startio(sc);
	sc->xb_flags |= XB_READY;
	mtx_unlock(&sc->xb_io_lock);
}

/**
 * Handle the change of state of the backend to Closing.  We must delete our
 * device-layer structures now, to ensure that writes are flushed through to
 * the backend.  Once this is done, we can switch to Closed in
 * acknowledgement.
 */
static void
blkfront_closing(device_t dev)
{
	struct xb_softc *sc = device_get_softc(dev);

	xenbus_set_state(dev, XenbusStateClosing);

	DPRINTK("blkfront_closing: %s removed\n", xenbus_get_node(dev));

	if (sc->xb_disk != NULL) {
		disk_destroy(sc->xb_disk);
		sc->xb_disk = NULL;
	}

	xenbus_set_state(dev, XenbusStateClosed); 
}


static int
blkfront_detach(device_t dev)
{
	struct xb_softc *sc = device_get_softc(dev);

	DPRINTK("blkfront_remove: %s removed\n", xenbus_get_node(dev));

	blkif_free(sc);
	mtx_destroy(&sc->xb_io_lock);

	return 0;
}


static inline void 
flush_requests(struct xb_softc *sc)
{
	int notify;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&sc->ring, notify);

	if (notify)
		notify_remote_via_irq(sc->irq);
}

static void
blkif_restart_queue_callback(void *arg)
{
	struct xb_softc *sc = arg;

	mtx_lock(&sc->xb_io_lock);

	xb_startio(sc);

	mtx_unlock(&sc->xb_io_lock);
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
	sc->users++;
	return (0);
}

static int
blkif_close(struct disk *dp)
{
	struct xb_softc	*sc = (struct xb_softc *)dp->d_drv1;

	if (sc == NULL)
		return (ENXIO);
	sc->xb_flags &= ~XB_OPEN;
	if (--(sc->users) == 0) {
		/* Check whether we have been instructed to close.  We will
		   have ignored this request initially, as the device was
		   still mounted. */
		device_t dev = sc->xb_dev;
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

static void
xb_free_command(struct xb_command *cm)
{

	KASSERT((cm->cm_flags & XB_ON_XBQ_MASK) == 0,
	    ("Freeing command that is still on a queue\n"));

	cm->cm_flags = 0;
	cm->bp = NULL;
	cm->cm_complete = NULL;
	xb_enqueue_free(cm);
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
static struct xb_command *
xb_bio_command(struct xb_softc *sc)
{
	struct xb_command *cm;
	struct bio *bp;

	if (unlikely(sc->connected != BLKIF_STATE_CONNECTED))
		return (NULL);

	bp = xb_dequeue_bio(sc);
	if (bp == NULL)
		return (NULL);

	if ((cm = xb_dequeue_free(sc)) == NULL) {
		xb_requeue_bio(sc, bp);
		return (NULL);
	}

	if (gnttab_alloc_grant_references(sc->max_request_segments,
	    &cm->gref_head) != 0) {
		gnttab_request_free_callback(&sc->callback,
			blkif_restart_queue_callback, sc,
			sc->max_request_segments);
		xb_requeue_bio(sc, bp);
		xb_enqueue_free(cm);
		sc->xb_flags |= XB_FROZEN;
		return (NULL);
	}

	cm->bp = bp;
	cm->data = bp->bio_data;
	cm->datalen = bp->bio_bcount;
	cm->operation = (bp->bio_cmd == BIO_READ) ? BLKIF_OP_READ :
	    BLKIF_OP_WRITE;
	cm->sector_number = (blkif_sector_t)bp->bio_pblkno;

	return (cm);
}

static int
blkif_queue_request(struct xb_softc *sc, struct xb_command *cm)
{
	int	error;

	error = bus_dmamap_load(sc->xb_io_dmat, cm->map, cm->data, cm->datalen,
	    blkif_queue_cb, cm, 0);
	if (error == EINPROGRESS) {
		printf("EINPROGRESS\n");
		sc->xb_flags |= XB_FROZEN;
		cm->cm_flags |= XB_CMD_FROZEN;
		return (0);
	}

	return (error);
}

static void
blkif_queue_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct xb_softc *sc;
	struct xb_command *cm;
	blkif_request_t	*ring_req;
	struct blkif_request_segment *sg;
        struct blkif_request_segment *last_block_sg;
	grant_ref_t *sg_ref;
	vm_paddr_t buffer_ma;
	uint64_t fsect, lsect;
	int ref;
	int op;
	int block_segs;

	cm = arg;
	sc = cm->cm_sc;

//printf("%s: Start\n", __func__);
	if (error) {
		printf("error %d in blkif_queue_cb\n", error);
		cm->bp->bio_error = EIO;
		biodone(cm->bp);
		xb_free_command(cm);
		return;
	}

	/* Fill out a communications ring structure. */
	ring_req = RING_GET_REQUEST(&sc->ring, sc->ring.req_prod_pvt);
	sc->ring.req_prod_pvt++;
	ring_req->id = cm->id;
	ring_req->operation = cm->operation;
	ring_req->sector_number = cm->sector_number;
	ring_req->handle = (blkif_vdev_t)(uintptr_t)sc->xb_disk;
	ring_req->nr_segments = nsegs;
	cm->nseg = nsegs;

	block_segs    = MIN(nsegs, BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK);
	sg            = ring_req->seg;
	last_block_sg = sg + block_segs;
	sg_ref        = cm->sg_refs;

	while (1) {

		while (sg < last_block_sg) {
			buffer_ma = segs->ds_addr;
			fsect = (buffer_ma & PAGE_MASK) >> XBD_SECTOR_SHFT;
			lsect = fsect + (segs->ds_len  >> XBD_SECTOR_SHFT) - 1;

			KASSERT(lsect <= 7, ("XEN disk driver data cannot "
				"cross a page boundary"));

			/* install a grant reference. */
			ref = gnttab_claim_grant_reference(&cm->gref_head);

			/*
			 * GNTTAB_LIST_END == 0xffffffff, but it is private
			 * to gnttab.c.
			 */
			KASSERT(ref != ~0, ("grant_reference failed"));

			gnttab_grant_foreign_access_ref(
				ref,
				xenbus_get_otherend_id(sc->xb_dev),
				buffer_ma >> PAGE_SHIFT,
				ring_req->operation == BLKIF_OP_WRITE);

			*sg_ref = ref;
			*sg = (struct blkif_request_segment) {
				.gref       = ref,
				.first_sect = fsect, 
				.last_sect  = lsect };
			sg++;
			sg_ref++;
			segs++;
			nsegs--;
		}
		block_segs = MIN(nsegs, BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK);
                if (block_segs == 0)
                        break;

                sg = BLKRING_GET_SG_REQUEST(&sc->ring, sc->ring.req_prod_pvt);
		sc->ring.req_prod_pvt++;
                last_block_sg = sg + block_segs;
	}

	if (cm->operation == BLKIF_OP_READ)
		op = BUS_DMASYNC_PREREAD;
	else if (cm->operation == BLKIF_OP_WRITE)
		op = BUS_DMASYNC_PREWRITE;
	else
		op = 0;
	bus_dmamap_sync(sc->xb_io_dmat, cm->map, op);

	gnttab_free_grant_references(cm->gref_head);

	xb_enqueue_busy(cm);

	/*
	 * This flag means that we're probably executing in the busdma swi
	 * instead of in the startio context, so an explicit flush is needed.
	 */
	if (cm->cm_flags & XB_CMD_FROZEN)
		flush_requests(sc);

//printf("%s: Done\n", __func__);
	return;
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
	struct xb_command *cm;
	int error, queued = 0;

	mtx_assert(&sc->xb_io_lock, MA_OWNED);

	if (sc->connected != BLKIF_STATE_CONNECTED)
		return;

	while (RING_FREE_REQUESTS(&sc->ring) >= sc->max_request_blocks) {
		if (sc->xb_flags & XB_FROZEN)
			break;

		cm = xb_dequeue_ready(sc);

		if (cm == NULL)
		    cm = xb_bio_command(sc);

		if (cm == NULL)
			break;

		if ((error = blkif_queue_request(sc, cm)) != 0) {
			printf("blkif_queue_request returned %d\n", error);
			break;
		}
		queued++;
	}

	if (queued != 0) 
		flush_requests(sc);
}

static void
blkif_int(void *xsc)
{
	struct xb_softc *sc = xsc;
	struct xb_command *cm;
	blkif_response_t *bret;
	RING_IDX i, rp;
	int op;

	mtx_lock(&sc->xb_io_lock);

	if (unlikely(sc->connected == BLKIF_STATE_DISCONNECTED)) {
		mtx_unlock(&sc->xb_io_lock);
		return;
	}

 again:
	rp = sc->ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = sc->ring.rsp_cons; i != rp;) {
		bret = RING_GET_RESPONSE(&sc->ring, i);
		cm   = &sc->shadow[bret->id];

		xb_remove_busy(cm);
		i += blkif_completion(cm);

		if (cm->operation == BLKIF_OP_READ)
			op = BUS_DMASYNC_POSTREAD;
		else if (cm->operation == BLKIF_OP_WRITE)
			op = BUS_DMASYNC_POSTWRITE;
		else
			op = 0;
		bus_dmamap_sync(sc->xb_io_dmat, cm->map, op);
		bus_dmamap_unload(sc->xb_io_dmat, cm->map);

		/*
		 * If commands are completing then resources are probably
		 * being freed as well.  It's a cheap assumption even when
		 * wrong.
		 */
		sc->xb_flags &= ~XB_FROZEN;

		/*
		 * Directly call the i/o complete routine to save an
		 * an indirection in the common case.
		 */
		cm->status = bret->status;
		if (cm->bp)
			xb_bio_complete(sc, cm);
		else if (cm->cm_complete)
			(cm->cm_complete)(cm);
		else
			xb_free_command(cm);
	}

	sc->ring.rsp_cons = i;

	if (i != sc->ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&sc->ring, more_to_do);
		if (more_to_do)
			goto again;
	} else {
		sc->ring.sring->rsp_event = i + 1;
	}

	xb_startio(sc);

	if (unlikely(sc->connected == BLKIF_STATE_SUSPENDED))
		wakeup(&sc->cm_busy);

	mtx_unlock(&sc->xb_io_lock);
}

static void 
blkif_free(struct xb_softc *sc)
{
	uint8_t *sring_page_ptr;
	int i;
	
	/* Prevent new requests being issued until we fix things up. */
	mtx_lock(&sc->xb_io_lock);
	sc->connected = BLKIF_STATE_DISCONNECTED; 
	mtx_unlock(&sc->xb_io_lock);

	/* Free resources associated with old device channel. */
	if (sc->ring.sring != NULL) {
		sring_page_ptr = (uint8_t *)sc->ring.sring;
		for (i = 0; i < sc->ring_pages; i++) {
			if (sc->ring_ref[i] != GRANT_INVALID_REF) {
				gnttab_end_foreign_access_ref(sc->ring_ref[i]);
				sc->ring_ref[i] = GRANT_INVALID_REF;
			}
			sring_page_ptr += PAGE_SIZE;
		}
		free(sc->ring.sring, M_XENBLOCKFRONT);
		sc->ring.sring = NULL;
	}

	if (sc->shadow) {

		for (i = 0; i < sc->max_requests; i++) {
			struct xb_command *cm;

			cm = &sc->shadow[i];
			if (cm->sg_refs != NULL) {
				free(cm->sg_refs, M_XENBLOCKFRONT);
				cm->sg_refs = NULL;
			}

			bus_dmamap_destroy(sc->xb_io_dmat, cm->map);
		}
		free(sc->shadow, M_XENBLOCKFRONT);
		sc->shadow = NULL;

		bus_dma_tag_destroy(sc->xb_io_dmat);
		
		xb_initq_free(sc);
		xb_initq_ready(sc);
		xb_initq_complete(sc);
	}
		
	if (sc->irq) {
		unbind_from_irqhandler(sc->irq);
		sc->irq = 0;
	}
}

static int
blkif_completion(struct xb_command *s)
{
//printf("%s: Req %p(%d)\n", __func__, s, s->nseg);
	gnttab_end_foreign_access_references(s->nseg, s->sg_refs);
	return (BLKIF_SEGS_TO_BLOCKS(s->nseg));
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
	DEVMETHOD(xenbus_otherend_changed, blkfront_backend_changed),

	{ 0, 0 } 
}; 

static driver_t blkfront_driver = { 
	"xbd", 
	blkfront_methods, 
	sizeof(struct xb_softc),                      
}; 
devclass_t blkfront_devclass; 
 
DRIVER_MODULE(xbd, xenbusb_front, blkfront_driver, blkfront_devclass, 0, 0); 
