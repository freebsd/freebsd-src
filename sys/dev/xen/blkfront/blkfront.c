/*-
 * All rights reserved.
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
 */

/*
 * XenoBSD block device driver
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

#include <machine/xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xen_intr.h>
#include <machine/xen/xenbus.h>
#include <machine/xen/evtchn.h>
#include <xen/interface/grant_table.h>

#include <geom/geom_disk.h>
#include <machine/xen/xenfunc.h>
#include <xen/gnttab.h>

#include <dev/xen/blkfront/block.h>

#define    ASSERT(S)       KASSERT(S, (#S))
/* prototypes */
struct xb_softc;
static void xb_startio(struct xb_softc *sc);
static void connect(struct blkfront_info *);
static void blkfront_closing(struct xenbus_device *);
static int blkfront_remove(struct xenbus_device *);
static int talk_to_backend(struct xenbus_device *, struct blkfront_info *);
static int setup_blkring(struct xenbus_device *, struct blkfront_info *);
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
#define DPRINTK(fmt, args...) printf("[XEN] %s:%d" fmt ".\n", __FUNCTION__, __LINE__,##args)
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


int
xlvbd_add(blkif_sector_t capacity, int unit, uint16_t vdisk_info, uint16_t sector_size, 
	  struct blkfront_info *info)
{
	struct xb_softc	*sc;
	int			error = 0;
	int unitno = unit - 767;

	sc = (struct xb_softc *)malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	sc->xb_unit = unitno;
	sc->xb_info = info;
	info->sc = sc;

	memset(&sc->xb_disk, 0, sizeof(sc->xb_disk)); 
	sc->xb_disk = disk_alloc();
	sc->xb_disk->d_unit = unitno;
	sc->xb_disk->d_open = blkif_open;
	sc->xb_disk->d_close = blkif_close;
	sc->xb_disk->d_ioctl = blkif_ioctl;
	sc->xb_disk->d_strategy = xb_strategy;
	sc->xb_disk->d_name = "xbd";
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


/* Setup supplies the backend dir, virtual device.

We place an event channel and shared frame entries.
We watch backend to wait if it's ok. */
static int blkfront_probe(struct xenbus_device *dev,
			  const struct xenbus_device_id *id)
{
	int err, vdevice, i;
	struct blkfront_info *info;

	/* FIXME: Use dynamic device id if this is not set. */
	err = xenbus_scanf(XBT_NIL, dev->nodename,
			   "virtual-device", "%i", &vdevice);
	if (err != 1) {
		xenbus_dev_fatal(dev, err, "reading virtual-device");
		printf("couldn't find virtual device");
		return (err);
	}

	info = malloc(sizeof(*info), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (info == NULL) {
		xenbus_dev_fatal(dev, ENOMEM, "allocating info structure");
		return ENOMEM;
	}
	
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
	info->handle = strtoul(strrchr(dev->nodename,'/')+1, NULL, 0);
	dev->dev_driver_data = info;

	err = talk_to_backend(dev, info);
	if (err) {
		free(info, M_DEVBUF);
		dev->dev_driver_data = NULL;
		return err;
	}

	return 0;
}


static int blkfront_resume(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev_driver_data;
	int err;

	DPRINTK("blkfront_resume: %s\n", dev->nodename);

	blkif_free(info, 1);

	err = talk_to_backend(dev, info);
	if (!err)
		blkif_recover(info);

	return err;
}

/* Common code used when first setting up, and when resuming. */
static int talk_to_backend(struct xenbus_device *dev,
			   struct blkfront_info *info)
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

	err = xenbus_printf(xbt, dev->nodename,
			    "ring-ref","%u", info->ring_ref);
	if (err) {
		message = "writing ring-ref";
		goto abort_transaction;
	}
	err = xenbus_printf(xbt, dev->nodename,
		"event-channel", "%u", irq_to_evtchn_port(info->irq));
	if (err) {
		message = "writing event-channel";
		goto abort_transaction;
	}

	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == -EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, err, "completing transaction");
		goto destroy_blkring;
	}
	xenbus_switch_state(dev, XenbusStateInitialised);
	
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
setup_blkring(struct xenbus_device *dev, struct blkfront_info *info)
{
	blkif_sring_t *sring;
	int err;

	info->ring_ref = GRANT_INVALID_REF;

	sring = (blkif_sring_t *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sring == NULL) {
		xenbus_dev_fatal(dev, ENOMEM, "allocating shared ring");
		return ENOMEM;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&info->ring, sring, PAGE_SIZE);

	err = xenbus_grant_ring(dev, (vtomach(info->ring.sring) >> PAGE_SHIFT));
	if (err < 0) {
		free(sring, M_DEVBUF);
		info->ring.sring = NULL;
		goto fail;
	}
	info->ring_ref = err;
	
	err = bind_listening_port_to_irqhandler(dev->otherend_id,
		"xbd", (driver_intr_t *)blkif_int, info,
					INTR_TYPE_BIO | INTR_MPSAFE, NULL);
	if (err <= 0) {
		xenbus_dev_fatal(dev, err,
				 "bind_evtchn_to_irqhandler failed");
		goto fail;
	}
	info->irq = err;

	return 0;
 fail:
	blkif_free(info, 0);
	return err;
}


/**
 * Callback received when the backend's state changes.
 */
static void backend_changed(struct xenbus_device *dev,
			    XenbusState backend_state)
{
	struct blkfront_info *info = dev->dev_driver_data;

	DPRINTK("blkfront:backend_changed.\n");

	switch (backend_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
	case XenbusStateClosed:
		break;

	case XenbusStateConnected:
		connect(info);
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
}

/* 
** Invoked when the backend is finally 'ready' (and has told produced 
** the details about the physical device - #sectors, size, etc). 
*/
static void 
connect(struct blkfront_info *info)
{
	unsigned long sectors, sector_size;
	unsigned int binfo;
	int err;

        if( (info->connected == BLKIF_STATE_CONNECTED) || 
	    (info->connected == BLKIF_STATE_SUSPENDED) )
		return;

	DPRINTK("blkfront.c:connect:%s.\n", info->xbdev->otherend);

	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
			    "sectors", "%lu", &sectors,
			    "info", "%u", &binfo,
			    "sector-size", "%lu", &sector_size,
			    NULL);
	if (err) {
		xenbus_dev_fatal(info->xbdev, err,
				 "reading backend fields at %s",
				 info->xbdev->otherend);
		return;
	}
	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
			    "feature-barrier", "%lu", &info->feature_barrier,
			    NULL);
	if (err)
		info->feature_barrier = 0;

	xlvbd_add(sectors, info->vdevice, binfo, sector_size, info);

	(void)xenbus_switch_state(info->xbdev, XenbusStateConnected); 

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
static void blkfront_closing(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev_driver_data;

	DPRINTK("blkfront_closing: %s removed\n", dev->nodename);

	if (info->mi) {
		DPRINTK("Calling xlvbd_del\n");
		xlvbd_del(info);
		info->mi = NULL;
	}

	xenbus_switch_state(dev, XenbusStateClosed);
}


static int blkfront_remove(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev_driver_data;

	DPRINTK("blkfront_remove: %s removed\n", dev->nodename);

	blkif_free(info, 0);

	free(info, M_DEVBUF);

	return 0;
}


static inline int 
GET_ID_FROM_FREELIST(struct blkfront_info *info)
{
	unsigned long nfree = info->shadow_free;
	
	KASSERT(nfree <= BLK_RING_SIZE, ("free %lu > RING_SIZE", nfree));
	info->shadow_free = info->shadow[nfree].req.id;
	info->shadow[nfree].req.id = 0x0fffffee; /* debug */
	return nfree;
}

static inline void 
ADD_ID_TO_FREELIST(struct blkfront_info *info, unsigned long id)
{
	info->shadow[id].req.id  = info->shadow_free;
	info->shadow[id].request = 0;
	info->shadow_free = id;
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
		printk("xb%d: not found", sc->xb_unit);
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
		struct xenbus_device * dev = sc->xb_info->xbdev;
		XenbusState state = xenbus_read_driver_state(dev->otherend);

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
		info->xbdev->otherend_id,
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
		gnttab_end_foreign_access(info->ring_ref, 0,
					  info->ring.sring);
		info->ring_ref = GRANT_INVALID_REF;
		info->ring.sring = NULL;
	}
	if (info->irq)
		unbind_from_irqhandler(info->irq, info); 
	info->irq = 0;

}

static void 
blkif_completion(struct blk_shadow *s)
{
	int i;

	for (i = 0; i < s->req.nr_segments; i++)
		gnttab_end_foreign_access(s->req.seg[i].gref, 0, 0UL);
}

static void 
blkif_recover(struct blkfront_info *info)
{
	int i, j;
	blkif_request_t *req;
	struct blk_shadow *copy;

	/* Stage 1: Make a safe copy of the shadow state. */
	copy = (struct blk_shadow *)malloc(sizeof(info->shadow), M_DEVBUF, M_NOWAIT|M_ZERO);
	PANIC_IF(copy == NULL);
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
				info->xbdev->otherend_id,
				pfn_to_mfn(info->shadow[req->id].frame[j]),
				0 /* assume not readonly */);

		info->shadow[req->id].req = *req;

		info->ring.req_prod_pvt++;
	}

	free(copy, M_DEVBUF);

	xenbus_switch_state(info->xbdev, XenbusStateConnected); 
	
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

static int
blkfront_is_ready(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev->dev_driver_data;

	return info->is_ready;
}

static struct xenbus_device_id blkfront_ids[] = {
	{ "vbd" },
	{ "" }
};


static struct xenbus_driver blkfront = {
	.name             = "vbd",
	.ids              = blkfront_ids,
	.probe            = blkfront_probe,
	.remove           = blkfront_remove,
	.resume           = blkfront_resume,
	.otherend_changed = backend_changed,
	.is_ready		  = blkfront_is_ready,
};



static void
xenbus_init(void)
{
	xenbus_register_frontend(&blkfront);
}

MTX_SYSINIT(ioreq, &blkif_io_lock, "BIO LOCK", MTX_NOWITNESS); /* XXX how does one enroll a lock? */
SYSINIT(xbdev, SI_SUB_PSEUDO, SI_ORDER_SECOND, xenbus_init, NULL);


/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 8
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
