/*
 * Copyright (c) 2006, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of Cisco Systems, Inc. nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/disk.h>
#include <sys/bio.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <geom/geom.h>

#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/xen-os.h>
#include <machine/hypervisor.h>
#include <machine/hypervisor-ifs.h>
#include <machine/xen_intr.h>
#include <machine/evtchn.h>
#include <machine/xenbus.h>
#include <machine/gnttab.h>
#include <machine/xen-public/memory.h>
#include <dev/xen/xenbus/xenbus_comms.h>


#if XEN_BLKBACK_DEBUG
#define DPRINTF(fmt, args...) \
    printf("blkback (%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define DPRINTF(fmt, args...) ((void)0)
#endif

#define WPRINTF(fmt, args...) \
    printf("blkback (%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)

#define BLKBACK_INVALID_HANDLE (~0)

struct ring_ref {
	vm_offset_t va;
	grant_handle_t handle;
	uint64_t bus_addr;
};

typedef struct blkback_info {

	/* Schedule lists */
	STAILQ_ENTRY(blkback_info) next_req;
	int on_req_sched_list;

	struct xenbus_device *xdev;
	XenbusState frontend_state;

	domid_t domid;

	int state;
	int ring_connected;
	struct ring_ref rr;
	blkif_back_ring_t ring;
	evtchn_port_t evtchn;
	int irq;
	void *irq_cookie;

	int ref_cnt;

	int handle;
	char *mode;
	char *type;
	char *dev_name;

	struct vnode *vn;
	struct cdev *cdev;
	struct cdevsw *csw;
	u_int sector_size;
	int sector_size_shift;
	off_t media_size;
	u_int media_num_sectors;
	int major;
	int minor;
	int read_only;

	struct mtx blk_ring_lock;

	device_t ndev;

	/* Stats */
	int st_rd_req;
	int st_wr_req;
	int st_oo_req;
	int st_err_req;
} blkif_t;

/*
 * These are rather arbitrary. They are fairly large because adjacent requests
 * pulled from a communication ring are quite likely to end up being part of
 * the same scatter/gather request at the disc.
 * 
 * ** TRY INCREASING 'blkif_reqs' IF WRITE SPEEDS SEEM TOO LOW **
 * 
 * This will increase the chances of being able to write whole tracks.
 * 64 should be enough to keep us competitive with Linux.
 */
static int blkif_reqs = 64;
TUNABLE_INT("xen.vbd.blkif_reqs", &blkif_reqs);

static int mmap_pages;

/*
 * Each outstanding request that we've passed to the lower device layers has a 
 * 'pending_req' allocated to it. Each buffer_head that completes decrements 
 * the pendcnt towards zero. When it hits zero, the specified domain has a 
 * response queued for it, with the saved 'id' passed back.
 */
typedef struct pending_req {
	blkif_t       *blkif;
	uint64_t       id;
	int            nr_pages;
	int            pendcnt;
	unsigned short operation;
	int            status;
	STAILQ_ENTRY(pending_req) free_list;
} pending_req_t;

static pending_req_t *pending_reqs;
static STAILQ_HEAD(pending_reqs_list, pending_req) pending_free =
	STAILQ_HEAD_INITIALIZER(pending_free);
static struct mtx pending_free_lock;

static STAILQ_HEAD(blkback_req_sched_list, blkback_info) req_sched_list =
	STAILQ_HEAD_INITIALIZER(req_sched_list);
static struct mtx req_sched_list_lock;

static unsigned long mmap_vstart;
static unsigned long *pending_vaddrs;
static grant_handle_t *pending_grant_handles;

static struct task blk_req_task;

/* Protos */
static void disconnect_ring(blkif_t *blkif);
static int vbd_add_dev(struct xenbus_device *xdev);

static inline int vaddr_pagenr(pending_req_t *req, int seg)
{
	return (req - pending_reqs) * BLKIF_MAX_SEGMENTS_PER_REQUEST + seg;
}

static inline unsigned long vaddr(pending_req_t *req, int seg)
{
	return pending_vaddrs[vaddr_pagenr(req, seg)];
}

#define pending_handle(_req, _seg) \
	(pending_grant_handles[vaddr_pagenr(_req, _seg)])

static unsigned long
alloc_empty_page_range(unsigned long nr_pages)
{
	void *pages;
	int i = 0, j = 0;
	multicall_entry_t mcl[17];
	unsigned long mfn_list[16];
	struct xen_memory_reservation reservation = {
		.extent_start = mfn_list,
		.nr_extents   = 0,
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	pages = malloc(nr_pages*PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (pages == NULL)
		return 0;

	memset(mcl, 0, sizeof(mcl));

	while (i < nr_pages) {
		unsigned long va = (unsigned long)pages + (i++ * PAGE_SIZE);

		mcl[j].op = __HYPERVISOR_update_va_mapping;
		mcl[j].args[0] = va;

		mfn_list[j++] = vtomach(va) >> PAGE_SHIFT;

		xen_phys_machine[(vtophys(va) >> PAGE_SHIFT)] = INVALID_P2M_ENTRY;

		if (j == 16 || i == nr_pages) {
			mcl[j-1].args[MULTI_UVMFLAGS_INDEX] = UVMF_TLB_FLUSH|UVMF_LOCAL;

			reservation.nr_extents = j;

			mcl[j].op = __HYPERVISOR_memory_op;
			mcl[j].args[0] = XENMEM_decrease_reservation;
			mcl[j].args[1] =  (unsigned long)&reservation;
			
			(void)HYPERVISOR_multicall(mcl, j+1);

			mcl[j-1].args[MULTI_UVMFLAGS_INDEX] = 0;
			j = 0;
		}
	}

	return (unsigned long)pages;
}

static pending_req_t *
alloc_req(void)
{
	pending_req_t *req;
	mtx_lock(&pending_free_lock);
	if ((req = STAILQ_FIRST(&pending_free))) {
		STAILQ_REMOVE(&pending_free, req, pending_req, free_list);
		STAILQ_NEXT(req, free_list) = NULL;
	}
	mtx_unlock(&pending_free_lock);
	return req;
}

static void
free_req(pending_req_t *req)
{
	int was_empty;

	mtx_lock(&pending_free_lock);
	was_empty = STAILQ_EMPTY(&pending_free);
	STAILQ_INSERT_TAIL(&pending_free, req, free_list);
	mtx_unlock(&pending_free_lock);
	if (was_empty)
		taskqueue_enqueue(taskqueue_swi, &blk_req_task); 
}

static void
fast_flush_area(pending_req_t *req)
{
	struct gnttab_unmap_grant_ref unmap[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	unsigned int i, invcount = 0;
	grant_handle_t handle;
	int ret;

	for (i = 0; i < req->nr_pages; i++) {
		handle = pending_handle(req, i);
		if (handle == BLKBACK_INVALID_HANDLE)
			continue;
		unmap[invcount].host_addr    = vaddr(req, i);
		unmap[invcount].dev_bus_addr = 0;
		unmap[invcount].handle       = handle;
		pending_handle(req, i) = BLKBACK_INVALID_HANDLE;
		invcount++;
	}

	ret = HYPERVISOR_grant_table_op(
		GNTTABOP_unmap_grant_ref, unmap, invcount);
	PANIC_IF(ret);
}

static void
blkif_get(blkif_t *blkif)
{
	atomic_add_int(&blkif->ref_cnt, 1);
}

static void
blkif_put(blkif_t *blkif)
{
	if (atomic_fetchadd_int(&blkif->ref_cnt, -1) == 1) {
		DPRINTF("Removing %x\n", (unsigned int)blkif);
		disconnect_ring(blkif);
		if (blkif->mode)
			free(blkif->mode, M_DEVBUF);			
		if (blkif->type)
			free(blkif->type, M_DEVBUF);			
		if (blkif->dev_name)
			free(blkif->dev_name, M_DEVBUF);			
		free(blkif, M_DEVBUF);
	}
}

static int
blkif_create(struct xenbus_device *xdev, long handle, char *mode, char *type, char *params)
{
	blkif_t *blkif;

	blkif = (blkif_t *)malloc(sizeof(*blkif), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!blkif)
		return ENOMEM;
	
	DPRINTF("Created %x\n", (unsigned int)blkif);

	blkif->ref_cnt = 1;
	blkif->domid = xdev->otherend_id;
	blkif->handle = handle;
	blkif->mode = mode;
	blkif->type = type;
	blkif->dev_name = params;
	blkif->xdev = xdev;
	xdev->data = blkif;

	mtx_init(&blkif->blk_ring_lock, "blk_ring_ock", "blkback ring lock", MTX_DEF);

	if (strcmp(mode, "w"))
		blkif->read_only = 1;

	return 0;
}

static void
add_to_req_schedule_list_tail(blkif_t *blkif)
{
	if (!blkif->on_req_sched_list) {
		mtx_lock(&req_sched_list_lock);
		if (!blkif->on_req_sched_list && (blkif->state == XenbusStateConnected)) {
			blkif_get(blkif);
			STAILQ_INSERT_TAIL(&req_sched_list, blkif, next_req);
			blkif->on_req_sched_list = 1;
			taskqueue_enqueue(taskqueue_swi, &blk_req_task); 
		}
		mtx_unlock(&req_sched_list_lock);
	}
}

/* This routine does not call blkif_get(), does not schedule the blk_req_task to run,
   and assumes that the state is connected */
static void
add_to_req_schedule_list_tail2(blkif_t *blkif)
{
	mtx_lock(&req_sched_list_lock);
	if (!blkif->on_req_sched_list) {
		STAILQ_INSERT_TAIL(&req_sched_list, blkif, next_req);
		blkif->on_req_sched_list = 1;
	}
	mtx_unlock(&req_sched_list_lock);
}

/* Removes blkif from front of list and does not call blkif_put() (caller must) */
static blkif_t *
remove_from_req_schedule_list(void)
{
	blkif_t *blkif;

	mtx_lock(&req_sched_list_lock);

	if ((blkif = STAILQ_FIRST(&req_sched_list))) {
		STAILQ_REMOVE(&req_sched_list, blkif, blkback_info, next_req);
		STAILQ_NEXT(blkif, next_req) = NULL;
		blkif->on_req_sched_list = 0;
	}

	mtx_unlock(&req_sched_list_lock);

	return blkif;
}

static void
make_response(blkif_t *blkif, uint64_t id, 
			  unsigned short op, int st)
{
	blkif_response_t *resp;
	blkif_back_ring_t *blk_ring = &blkif->ring;
	int more_to_do = 0;
	int notify;

	mtx_lock(&blkif->blk_ring_lock);


	/* Place on the response ring for the relevant domain. */ 
	resp = RING_GET_RESPONSE(blk_ring, blk_ring->rsp_prod_pvt);
	resp->id        = id;
	resp->operation = op;
	resp->status    = st;
	blk_ring->rsp_prod_pvt++;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(blk_ring, notify);

	if (blk_ring->rsp_prod_pvt == blk_ring->req_cons) {
		/*
		 * Tail check for pending requests. Allows frontend to avoid
		 * notifications if requests are already in flight (lower
		 * overheads and promotes batching).
		 */
		RING_FINAL_CHECK_FOR_REQUESTS(blk_ring, more_to_do);

	} else if (RING_HAS_UNCONSUMED_REQUESTS(blk_ring))
		more_to_do = 1;

	mtx_unlock(&blkif->blk_ring_lock);

	if (more_to_do)
		add_to_req_schedule_list_tail(blkif);

	if (notify)
		notify_remote_via_irq(blkif->irq);
}

static void
end_block_io_op(struct bio *bio)
{
	pending_req_t *pending_req = bio->bio_caller2;

	if (bio->bio_error) {
		DPRINTF("BIO returned error %d for operation on device %s\n",
				bio->bio_error, pending_req->blkif->dev_name);
		pending_req->status = BLKIF_RSP_ERROR;
		pending_req->blkif->st_err_req++;
	}

#if 0
	printf("done: bio=%x error=%x completed=%llu resid=%lu flags=%x\n",
		   (unsigned int)bio, bio->bio_error, bio->bio_completed, bio->bio_resid, bio->bio_flags);
#endif

	if (atomic_fetchadd_int(&pending_req->pendcnt, -1) == 1) {
		fast_flush_area(pending_req);
		make_response(pending_req->blkif, pending_req->id,
			      pending_req->operation, pending_req->status);
		blkif_put(pending_req->blkif);
		free_req(pending_req);
	}

	g_destroy_bio(bio);
}

static void
dispatch_rw_block_io(blkif_t *blkif, blkif_request_t *req, pending_req_t *pending_req)
{
	struct gnttab_map_grant_ref map[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	struct { 
		unsigned long buf; unsigned int nsec;
	} seg[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	unsigned int nseg = req->nr_segments, nr_sects = 0;
	struct bio *biolist[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	int operation, ret, i, nbio = 0;

	/* Check that number of segments is sane. */
	if (unlikely(nseg == 0) || 
	    unlikely(nseg > BLKIF_MAX_SEGMENTS_PER_REQUEST)) {
		DPRINTF("Bad number of segments in request (%d)\n", nseg);
		goto fail_response;
	}

	if (req->operation == BLKIF_OP_WRITE) {
		if (blkif->read_only) {
			DPRINTF("Attempt to write to read only device %s\n", blkif->dev_name);
			goto fail_response;
		}
		operation = BIO_WRITE;
	} else
		operation = BIO_READ;

	pending_req->blkif     = blkif;
	pending_req->id        = req->id;
	pending_req->operation = req->operation;
	pending_req->status    = BLKIF_RSP_OKAY;
	pending_req->nr_pages  = nseg;

	for (i = 0; i < nseg; i++) {
		seg[i].nsec = req->seg[i].last_sect - 
			req->seg[i].first_sect + 1;

		if ((req->seg[i].last_sect >= (PAGE_SIZE >> 9)) ||
		    (seg[i].nsec <= 0))
			goto fail_response;
		nr_sects += seg[i].nsec;

		map[i].host_addr = vaddr(pending_req, i);
		map[i].dom = blkif->domid;
		map[i].ref = req->seg[i].gref;
		map[i].flags = GNTMAP_host_map;
		if (operation == BIO_WRITE)
			map[i].flags |= GNTMAP_readonly;
	}

	/* Convert to the disk's sector size */
	nr_sects = (nr_sects << 9) >> blkif->sector_size_shift;

	ret = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, map, nseg);
	PANIC_IF(ret);

	for (i = 0; i < nseg; i++) {
		if (unlikely(map[i].status != 0)) {
			DPRINTF("invalid buffer -- could not remap it\n");
			goto fail_flush;
		}

		pending_handle(pending_req, i) = map[i].handle;
#if 0
		/* Can't do this in FreeBSD since vtophys() returns the pfn */
		/* of the remote domain who loaned us the machine page - DPT */
		xen_phys_machine[(vtophys(vaddr(pending_req, i)) >> PAGE_SHIFT)] =
			map[i]dev_bus_addr >> PAGE_SHIFT;
#endif
		seg[i].buf  = map[i].dev_bus_addr | 
			(req->seg[i].first_sect << 9);
	}

	if (req->sector_number + nr_sects > blkif->media_num_sectors) {
		DPRINTF("%s of [%llu,%llu] extends past end of device %s\n",
			operation == BIO_READ ? "read" : "write",
			req->sector_number,
			req->sector_number + nr_sects, blkif->dev_name); 
		goto fail_flush;
	}

	for (i = 0; i < nseg; i++) {
		struct bio *bio;

		if ((int)seg[i].nsec & ((blkif->sector_size >> 9) - 1)) {
			DPRINTF("Misaligned I/O request from domain %d", blkif->domid);
			goto fail_put_bio;
		}

		bio = biolist[nbio++] = g_new_bio();
		if (unlikely(bio == NULL))
			goto fail_put_bio;

		bio->bio_cmd = operation;
		bio->bio_offset = req->sector_number << blkif->sector_size_shift;
		bio->bio_length = seg[i].nsec << 9;
		bio->bio_bcount = bio->bio_length;
		bio->bio_data = (caddr_t)(vaddr(pending_req, i) | (seg[i].buf & PAGE_MASK));
		bio->bio_done = end_block_io_op;
		bio->bio_caller2 = pending_req;
		bio->bio_dev = blkif->cdev;

		req->sector_number += (seg[i].nsec << 9) >> blkif->sector_size_shift;
#if 0
		printf("new: bio=%x cmd=%d sect=%llu nsect=%u iosize_max=%u @ %08lx\n",
			(unsigned int)bio, req->operation, req->sector_number, seg[i].nsec,
			blkif->cdev->si_iosize_max, seg[i].buf);
#endif
	}

	pending_req->pendcnt = nbio;
	blkif_get(blkif);

	for (i = 0; i < nbio; i++)
		(*blkif->csw->d_strategy)(biolist[i]);

	return;

 fail_put_bio:
	for (i = 0; i < (nbio-1); i++)
		g_destroy_bio(biolist[i]);
 fail_flush:
	fast_flush_area(pending_req);
 fail_response:
	make_response(blkif, req->id, req->operation, BLKIF_RSP_ERROR);
	free_req(pending_req);
}

static void
blk_req_action(void *context, int pending)
{
	blkif_t *blkif;

	DPRINTF("\n");

	while (!STAILQ_EMPTY(&req_sched_list)) {
		blkif_back_ring_t *blk_ring;
		RING_IDX rc, rp;

		blkif = remove_from_req_schedule_list();

		blk_ring = &blkif->ring;
		rc = blk_ring->req_cons;
		rp = blk_ring->sring->req_prod;
		rmb(); /* Ensure we see queued requests up to 'rp'. */

		while ((rc != rp) && !RING_REQUEST_CONS_OVERFLOW(blk_ring, rc)) {
			blkif_request_t *req;
			pending_req_t *pending_req;

			pending_req = alloc_req();
			if (pending_req == NULL)
				goto out_of_preqs;

			req = RING_GET_REQUEST(blk_ring, rc);
			blk_ring->req_cons = ++rc; /* before make_response() */

			switch (req->operation) {
			case BLKIF_OP_READ:
				blkif->st_rd_req++;
				dispatch_rw_block_io(blkif, req, pending_req);
				break;
			case BLKIF_OP_WRITE:
				blkif->st_wr_req++;
				dispatch_rw_block_io(blkif, req, pending_req);
				break;
			default:
				blkif->st_err_req++;
				DPRINTF("error: unknown block io operation [%d]\n",
						req->operation);
				make_response(blkif, req->id, req->operation,
							  BLKIF_RSP_ERROR);
				free_req(pending_req);
				break;
			}
		}

		blkif_put(blkif);
	}

	return;

 out_of_preqs:
	/* We ran out of pending req structs */
	/* Just requeue interface and wait to be rescheduled to run when one is freed */
	add_to_req_schedule_list_tail2(blkif);
	blkif->st_oo_req++;
}

/* Handle interrupt from a frontend */
static void
blkback_intr(void *arg)
{
	blkif_t *blkif = arg;
	DPRINTF("%x\n", (unsigned int)blkif);
	add_to_req_schedule_list_tail(blkif);
}

/* Map grant ref for ring */
static int
map_ring(grant_ref_t ref, domid_t dom, struct ring_ref *ring)
{
	struct gnttab_map_grant_ref op;

	ring->va = kmem_alloc_nofault(kernel_map, PAGE_SIZE);
	if (ring->va == 0)
		return ENOMEM;

	op.host_addr = ring->va;
	op.flags = GNTMAP_host_map;
	op.ref = ref;
	op.dom = dom;
	HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
	if (op.status) {
		WPRINTF("grant table op err=%d\n", op.status);
		kmem_free(kernel_map, ring->va, PAGE_SIZE);
		ring->va = 0;
		return EACCES;
	}

	ring->handle = op.handle;
	ring->bus_addr = op.dev_bus_addr;

	return 0;
}

/* Unmap grant ref for ring */
static void
unmap_ring(struct ring_ref *ring)
{
	struct gnttab_unmap_grant_ref op;

	op.host_addr = ring->va;
	op.dev_bus_addr = ring->bus_addr;
	op.handle = ring->handle;
	HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);
	if (op.status)
		WPRINTF("grant table op err=%d\n", op.status);

	kmem_free(kernel_map, ring->va, PAGE_SIZE);
	ring->va = 0;
}

static int
connect_ring(blkif_t *blkif)
{
	struct xenbus_device *xdev = blkif->xdev;
	blkif_sring_t *ring;
	unsigned long ring_ref;
	evtchn_port_t evtchn;
	evtchn_op_t op = { .cmd = EVTCHNOP_bind_interdomain };
	int err;

	if (blkif->ring_connected)
		return 0;

	// Grab FE data and map his memory
	err = xenbus_gather(NULL, xdev->otherend,
			"ring-ref", "%lu", &ring_ref,
		    "event-channel", "%u", &evtchn, NULL);
	if (err) {
		xenbus_dev_fatal(xdev, err,
			"reading %s/ring-ref and event-channel",
			xdev->otherend);
		return err;
	}

	err = map_ring(ring_ref, blkif->domid, &blkif->rr);
	if (err) {
		xenbus_dev_fatal(xdev, err, "mapping ring");
		return err;
	}
	ring = (blkif_sring_t *)blkif->rr.va;
	BACK_RING_INIT(&blkif->ring, ring, PAGE_SIZE);

	op.u.bind_interdomain.remote_dom = blkif->domid;
	op.u.bind_interdomain.remote_port = evtchn;
	err = HYPERVISOR_event_channel_op(&op);
	if (err) {
		unmap_ring(&blkif->rr);
		xenbus_dev_fatal(xdev, err, "binding event channel");
		return err;
	}
	blkif->evtchn = op.u.bind_interdomain.local_port;

	/* bind evtchn to irq handler */
	blkif->irq =
		bind_evtchn_to_irqhandler(blkif->evtchn, "blkback",
			blkback_intr, blkif, INTR_TYPE_NET|INTR_MPSAFE, &blkif->irq_cookie);

	blkif->ring_connected = 1;

	DPRINTF("%x rings connected! evtchn=%d irq=%d\n",
			(unsigned int)blkif, blkif->evtchn, blkif->irq);

	return 0;
}

static void
disconnect_ring(blkif_t *blkif)
{
	DPRINTF("\n");

	if (blkif->ring_connected) {
		unbind_from_irqhandler(blkif->irq, blkif->irq_cookie);
		blkif->irq = 0;
		unmap_ring(&blkif->rr);
		blkif->ring_connected = 0;
	}
}

static void
connect(blkif_t *blkif)
{
	struct xenbus_transaction *xbt;
	struct xenbus_device *xdev = blkif->xdev;
	int err;

	if (!blkif->ring_connected ||
		blkif->vn == NULL ||
		blkif->state == XenbusStateConnected)
		return;

	DPRINTF("%s\n", xdev->otherend);

	/* Supply the information about the device the frontend needs */
again:
	xbt = xenbus_transaction_start();
	if (IS_ERR(xbt)) {
		xenbus_dev_fatal(xdev, PTR_ERR(xbt),
						 "Error writing configuration for backend "
						 "(start transaction)");
		return;
	}

	err = xenbus_printf(xbt, xdev->nodename, "sectors", "%u",
				blkif->media_num_sectors);
	if (err) {
		xenbus_dev_fatal(xdev, err, "writing %s/sectors",
				 xdev->nodename);
		goto abort;
	}

	err = xenbus_printf(xbt, xdev->nodename, "info", "%u",
				blkif->read_only ? VDISK_READONLY : 0);
	if (err) {
		xenbus_dev_fatal(xdev, err, "writing %s/info",
				 xdev->nodename);
		goto abort;
	}
	err = xenbus_printf(xbt, xdev->nodename, "sector-size", "%u",
			    blkif->sector_size);
	if (err) {
		xenbus_dev_fatal(xdev, err, "writing %s/sector-size",
				 xdev->nodename);
		goto abort;
	}

	err = xenbus_transaction_end(xbt, 0);
	if (err == -EAGAIN)
		goto again;
	if (err)
		xenbus_dev_fatal(xdev, err, "ending transaction");

	err = xenbus_switch_state(xdev, NULL, XenbusStateConnected);
	if (err)
		xenbus_dev_fatal(xdev, err, "switching to Connected state",
				 xdev->nodename);

	blkif->state = XenbusStateConnected;

	return;

 abort:
	xenbus_transaction_end(xbt, 1);
}

static int
blkback_probe(struct xenbus_device *xdev, const struct xenbus_device_id *id)
{
	int err;
	char *p, *mode = NULL, *type = NULL, *params = NULL;
	long handle;

	DPRINTF("node=%s\n", xdev->nodename);

	p = strrchr(xdev->otherend, '/') + 1;
	handle = strtoul(p, NULL, 0);

	mode = xenbus_read(NULL, xdev->nodename, "mode", NULL);
	if (IS_ERR(mode)) {
		xenbus_dev_fatal(xdev, PTR_ERR(mode), "reading mode");
		err = PTR_ERR(mode);
		goto error;
	}
	
	type = xenbus_read(NULL, xdev->nodename, "type", NULL);
	if (IS_ERR(type)) {
		xenbus_dev_fatal(xdev, PTR_ERR(type), "reading type");
		err = PTR_ERR(type);
		goto error;
	}
	
	params = xenbus_read(NULL, xdev->nodename, "params", NULL);
	if (IS_ERR(type)) {
		xenbus_dev_fatal(xdev, PTR_ERR(params), "reading params");
		err = PTR_ERR(params);
		goto error;
	}
	
	err = blkif_create(xdev, handle, mode, type, params);
	if (err) {
		xenbus_dev_fatal(xdev, err, "creating blkif");
		goto error;
	}

	err = vbd_add_dev(xdev);
	if (err) {
		blkif_put((blkif_t *)xdev->data);
		xenbus_dev_fatal(xdev, err, "adding vbd device");
	}

	return err;

 error:
	if (mode)
		free(mode, M_DEVBUF);
	if (type)
		free(type, M_DEVBUF);
	if (params)
		free(params, M_DEVBUF);
	return err;
}

static int
blkback_remove(struct xenbus_device *xdev)
{
	blkif_t *blkif = xdev->data;
	device_t ndev;

	DPRINTF("node=%s\n", xdev->nodename);

	blkif->state = XenbusStateClosing;

	if ((ndev = blkif->ndev)) {
		blkif->ndev = NULL;
		mtx_lock(&Giant);
		device_detach(ndev);
		mtx_unlock(&Giant);
	}

	xdev->data = NULL;
	blkif->xdev = NULL;
	blkif_put(blkif);

	return 0;
}

static int
blkback_resume(struct xenbus_device *xdev)
{
	DPRINTF("node=%s\n", xdev->nodename);
	return 0;
}

static void
frontend_changed(struct xenbus_device *xdev,
				 XenbusState frontend_state)
{
	blkif_t *blkif = xdev->data;

	DPRINTF("state=%d\n", frontend_state);

	blkif->frontend_state = frontend_state;

	switch (frontend_state) {
	case XenbusStateInitialising:
		break;
	case XenbusStateInitialised:
	case XenbusStateConnected:
		connect_ring(blkif);
		connect(blkif);
		break;
	case XenbusStateClosing:
		xenbus_switch_state(xdev, NULL, XenbusStateClosing);
		break;
	case XenbusStateClosed:
		xenbus_remove_device(xdev);
		break;
	case XenbusStateUnknown:
	case XenbusStateInitWait:
		xenbus_dev_fatal(xdev, EINVAL, "saw state %d at frontend",
						 frontend_state);
		break;
	}
}

/* ** Driver registration ** */

static struct xenbus_device_id blkback_ids[] = {
	{ "vbd" },
	{ "" }
};

static struct xenbus_driver blkback = {
	.name = "blkback",
	.ids = blkback_ids,
	.probe = blkback_probe,
	.remove = blkback_remove,
	.resume = blkback_resume,
	.otherend_changed = frontend_changed,
};

static void
blkback_init(void *unused)
{
	int i;

	TASK_INIT(&blk_req_task, 0, blk_req_action, NULL);
	mtx_init(&req_sched_list_lock, "blk_req_sched_lock", "blkback req sched lock", MTX_DEF);

	mtx_init(&pending_free_lock, "blk_pending_req_ock", "blkback pending request lock", MTX_DEF);

	mmap_pages = blkif_reqs * BLKIF_MAX_SEGMENTS_PER_REQUEST;
	pending_reqs = malloc(sizeof(pending_reqs[0]) *
		blkif_reqs, M_DEVBUF, M_ZERO|M_NOWAIT);
	pending_grant_handles = malloc(sizeof(pending_grant_handles[0]) *
		mmap_pages, M_DEVBUF, M_NOWAIT);
	pending_vaddrs = malloc(sizeof(pending_vaddrs[0]) *
		mmap_pages, M_DEVBUF, M_NOWAIT);
	mmap_vstart = alloc_empty_page_range(mmap_pages);
	if (!pending_reqs || !pending_grant_handles || !pending_vaddrs || !mmap_vstart) {
		if (pending_reqs)
			free(pending_reqs, M_DEVBUF);
		if (pending_grant_handles)
			free(pending_grant_handles, M_DEVBUF);
		if (pending_vaddrs)
			free(pending_vaddrs, M_DEVBUF);
		WPRINTF("out of memory\n");
		return;
	}

	for (i = 0; i < mmap_pages; i++) {
		pending_vaddrs[i] = mmap_vstart + (i << PAGE_SHIFT);
		pending_grant_handles[i] = BLKBACK_INVALID_HANDLE;
	}

	for (i = 0; i < blkif_reqs; i++) {
		STAILQ_INSERT_TAIL(&pending_free, &pending_reqs[i], free_list);
	}

	DPRINTF("registering %s\n", blkback.name);
	xenbus_register_backend(&blkback);
}

SYSINIT(xbbedev, SI_SUB_PSEUDO, SI_ORDER_ANY, blkback_init, NULL)

static void
close_device(blkif_t *blkif)
{
	DPRINTF("closing dev=%s\n", blkif->dev_name);
	if (blkif->vn) {
		int flags = FREAD;

		if (!blkif->read_only)
			flags |= FWRITE;

		if (blkif->csw) {
			dev_relthread(blkif->cdev);
			blkif->csw = NULL;
		}

		(void)vn_close(blkif->vn, flags, NOCRED, curthread);
		blkif->vn = NULL;
	}
}

static int
open_device(blkif_t *blkif)
{
	struct nameidata nd;
	struct vattr vattr;
	struct cdev *dev;
	struct cdevsw *devsw;
	int flags = FREAD, err = 0;

	DPRINTF("opening dev=%s\n", blkif->dev_name);

	if (!blkif->read_only)
		flags |= FWRITE;

	if (!curthread->td_proc->p_fd->fd_cdir) {
		curthread->td_proc->p_fd->fd_cdir = rootvnode;
		VREF(rootvnode);
	}
	if (!curthread->td_proc->p_fd->fd_rdir) {
		curthread->td_proc->p_fd->fd_rdir = rootvnode;
		VREF(rootvnode);
	}
	if (!curthread->td_proc->p_fd->fd_jdir) {
		curthread->td_proc->p_fd->fd_jdir = rootvnode;
		VREF(rootvnode);
	}

 again:
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, blkif->dev_name, curthread);
	err = vn_open(&nd, &flags, 0, -1);
	if (err) {
		if (blkif->dev_name[0] != '/') {
			char *dev_path = "/dev/";
			char *dev_name;

			/* Try adding device path at beginning of name */
			dev_name = malloc(strlen(blkif->dev_name) + strlen(dev_path) + 1, M_DEVBUF, M_NOWAIT);
			if (dev_name) {
				sprintf(dev_name, "%s%s", dev_path, blkif->dev_name);
				free(blkif->dev_name, M_DEVBUF);			
				blkif->dev_name = dev_name;
				goto again;
			}
		}
		xenbus_dev_fatal(blkif->xdev, err, "error opening device %s", blkif->dev_name);
		return err;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
		
	blkif->vn = nd.ni_vp;

	/* We only support disks for now */
	if (!vn_isdisk(blkif->vn, &err)) {
		xenbus_dev_fatal(blkif->xdev, err, "device %s is not a disk", blkif->dev_name);
		VOP_UNLOCK(blkif->vn, 0, curthread);
		goto error;
	}

	blkif->cdev = blkif->vn->v_rdev;
	blkif->csw = dev_refthread(blkif->cdev);
	PANIC_IF(blkif->csw == NULL);

	err = VOP_GETATTR(blkif->vn, &vattr, NOCRED, curthread);
	if (err) {
		xenbus_dev_fatal(blkif->xdev, err,
			"error getting vnode attributes for device %s", blkif->dev_name);
		VOP_UNLOCK(blkif->vn, 0, curthread);
		goto error;
	}

	VOP_UNLOCK(blkif->vn, 0, curthread);

	dev = blkif->vn->v_rdev;
	devsw = dev->si_devsw;
	if (!devsw->d_ioctl) {
		err = ENODEV;
		xenbus_dev_fatal(blkif->xdev, err,
			"no d_ioctl for device %s!", blkif->dev_name);
		goto error;
	}

	err = (*devsw->d_ioctl)(dev, DIOCGSECTORSIZE, (caddr_t)&blkif->sector_size, FREAD, curthread);
	if (err) {
		xenbus_dev_fatal(blkif->xdev, err,
			"error calling ioctl DIOCGSECTORSIZE for device %s", blkif->dev_name);
		goto error;
	}
	blkif->sector_size_shift = fls(blkif->sector_size) - 1;

	err = (*devsw->d_ioctl)(dev, DIOCGMEDIASIZE, (caddr_t)&blkif->media_size, FREAD, curthread);
	if (err) {
		xenbus_dev_fatal(blkif->xdev, err,
			"error calling ioctl DIOCGMEDIASIZE for device %s", blkif->dev_name);
		goto error;
	}
	blkif->media_num_sectors = blkif->media_size >> blkif->sector_size_shift;

	blkif->major = umajor(vattr.va_rdev);
	blkif->minor = uminor(vattr.va_rdev);

	DPRINTF("opened dev=%s major=%d minor=%d sector_size=%u media_size=%lld\n",
			blkif->dev_name, blkif->major, blkif->minor, blkif->sector_size, blkif->media_size);

	return 0;

 error:
	close_device(blkif);
	return err;
}

static int
vbd_add_dev(struct xenbus_device *xdev)
{
	blkif_t *blkif = xdev->data;
	device_t nexus, ndev;
	devclass_t dc;
	int err = 0;

	mtx_lock(&Giant);

	/* We will add a vbd device as a child of nexus0 (for now) */
	if (!(dc = devclass_find("nexus")) ||
		!(nexus = devclass_get_device(dc, 0))) {
		WPRINTF("could not find nexus0!\n");
		err = ENOENT;
		goto done;
	}


	/* Create a newbus device representing the vbd */
	ndev = BUS_ADD_CHILD(nexus, 0, "vbd", blkif->handle);
	if (!ndev) {
		WPRINTF("could not create newbus device vbd%d!\n", blkif->handle);
		err = EFAULT;
		goto done;
	}
	
	blkif_get(blkif);
	device_set_ivars(ndev, blkif);
	blkif->ndev = ndev;

	device_probe_and_attach(ndev);

 done:

	mtx_unlock(&Giant);

	return err;
}

enum {
	VBD_SYSCTL_DOMID,
	VBD_SYSCTL_ST_RD_REQ,
	VBD_SYSCTL_ST_WR_REQ,
	VBD_SYSCTL_ST_OO_REQ,
	VBD_SYSCTL_ST_ERR_REQ,
	VBD_SYSCTL_RING,
};

static char *
vbd_sysctl_ring_info(blkif_t *blkif, int cmd)
{
	char *buf = malloc(256, M_DEVBUF, M_WAITOK);
	if (buf) {
		if (!blkif->ring_connected)
			sprintf(buf, "ring not connected\n");
		else {
			blkif_back_ring_t *ring = &blkif->ring;
			sprintf(buf, "nr_ents=%x req_cons=%x"
					" req_prod=%x req_event=%x"
					" rsp_prod=%x rsp_event=%x",
					ring->nr_ents, ring->req_cons,
					ring->sring->req_prod, ring->sring->req_event,
					ring->sring->rsp_prod, ring->sring->rsp_event);
		}
	}
	return buf;
}

static int
vbd_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	blkif_t *blkif = (blkif_t *)device_get_ivars(dev);
	const char *value;
	char *buf = NULL;
	int err;

	switch (arg2) {
	case VBD_SYSCTL_DOMID:
		return sysctl_handle_int(oidp, NULL, blkif->domid, req);
	case VBD_SYSCTL_ST_RD_REQ:
		return sysctl_handle_int(oidp, NULL, blkif->st_rd_req, req);
	case VBD_SYSCTL_ST_WR_REQ:
		return sysctl_handle_int(oidp, NULL, blkif->st_wr_req, req);
	case VBD_SYSCTL_ST_OO_REQ:
		return sysctl_handle_int(oidp, NULL, blkif->st_oo_req, req);
	case VBD_SYSCTL_ST_ERR_REQ:
		return sysctl_handle_int(oidp, NULL, blkif->st_err_req, req);
	case VBD_SYSCTL_RING:
		value = buf = vbd_sysctl_ring_info(blkif, arg2);
		break;
	default:
		return (EINVAL);
	}

	err = SYSCTL_OUT(req, value, strlen(value));
	if (buf != NULL)
		free(buf, M_DEVBUF);

	return err;
}

/* Newbus vbd device driver probe */
static int
vbd_probe(device_t dev)
{
	DPRINTF("vbd%d\n", device_get_unit(dev));
	return 0;
}

/* Newbus vbd device driver attach */
static int
vbd_attach(device_t dev) 
{
	blkif_t *blkif = (blkif_t *)device_get_ivars(dev);

	DPRINTF("%s\n", blkif->dev_name);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "domid", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VBD_SYSCTL_DOMID, vbd_sysctl_handler, "I",
	    "domid of frontend");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rd_reqs", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VBD_SYSCTL_ST_RD_REQ, vbd_sysctl_handler, "I",
	    "number of read reqs");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "wr_reqs", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VBD_SYSCTL_ST_WR_REQ, vbd_sysctl_handler, "I",
	    "number of write reqs");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "oo_reqs", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VBD_SYSCTL_ST_OO_REQ, vbd_sysctl_handler, "I",
	    "number of deferred reqs");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "err_reqs", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VBD_SYSCTL_ST_ERR_REQ, vbd_sysctl_handler, "I",
	    "number of reqs that returned error");
#if XEN_BLKBACK_DEBUG
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "ring", CTLFLAG_RD,
	    dev, VBD_SYSCTL_RING, vbd_sysctl_handler, "A",
	    "req ring info");
#endif

	if (!open_device(blkif))
		connect(blkif);

	return bus_generic_attach(dev);
}

/* Newbus vbd device driver detach */
static int
vbd_detach(device_t dev)
{
	blkif_t *blkif = (blkif_t *)device_get_ivars(dev);

	DPRINTF("%s\n", blkif->dev_name);

	close_device(blkif);

	bus_generic_detach(dev);

	blkif_put(blkif);

	return 0;
}

static device_method_t vbd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vbd_probe),
	DEVMETHOD(device_attach, 	vbd_attach),
	DEVMETHOD(device_detach,	vbd_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{0, 0}
};

static devclass_t vbd_devclass;

static driver_t vbd_driver = {
	"vbd",
	vbd_methods,
	0,
};

DRIVER_MODULE(vbd, nexus, vbd_driver, vbd_devclass, 0, 0);

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
