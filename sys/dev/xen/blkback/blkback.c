/*-
 * Copyright (c) 2009-2010 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * Authors: Justin T. Gibbs     (Spectra Logic Corporation)
 *          Ken Merry           (Spectra Logic Corporation)
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/**
 * \file blkback.c
 *
 * \brief Device driver supporting the vending of block storage from
 *        a FreeBSD domain to other domains.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <geom/geom.h>

#include <machine/_inttypes.h>
#include <machine/xen/xen-os.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <xen/blkif.h>
#include <xen/evtchn.h>
#include <xen/gnttab.h>
#include <xen/xen_intr.h>

#include <xen/interface/event_channel.h>
#include <xen/interface/grant_table.h>

#include <xen/xenbus/xenbusvar.h>

/*--------------------------- Compile-time Tunables --------------------------*/
/**
 * The maximum number of outstanding request blocks (request headers plus
 * additional segment blocks) we will allow in a negotiated block-front/back
 * communication channel.
 */
#define	XBB_MAX_REQUESTS	256

/**
 * \brief Define to force all I/O to be performed on memory owned by the
 *        backend device, with a copy-in/out to the remote domain's memory.
 *
 * \note  This option is currently required when this driver's domain is
 *        operating in HVM mode on a system using an IOMMU.
 *
 * This driver uses Xen's grant table API to gain access to the memory of
 * the remote domains it serves.  When our domain is operating in PV mode,
 * the grant table mechanism directly updates our domain's page table entries
 * to point to the physical pages of the remote domain.  This scheme guarantees
 * that blkback and the backing devices it uses can safely perform DMA
 * operations to satisfy requests.  In HVM mode, Xen may use a HW IOMMU to
 * insure that our domain cannot DMA to pages owned by another domain.  As
 * of Xen 4.0, IOMMU mappings for HVM guests are not updated via the grant
 * table API.  For this reason, in HVM mode, we must bounce all requests into
 * memory that is mapped into our domain at domain startup and thus has
 * valid IOMMU mappings.
 */
#define XBB_USE_BOUNCE_BUFFERS

/**
 * \brief Define to enable rudimentary request logging to the console.
 */
#undef XBB_DEBUG

/*---------------------------------- Macros ----------------------------------*/
/**
 * Custom malloc type for all driver allocations.
 */
MALLOC_DEFINE(M_XENBLOCKBACK, "xbbd", "Xen Block Back Driver Data");

#ifdef XBB_DEBUG
#define DPRINTF(fmt, args...) \
    printf("xbb(%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

/**
 * The maximum mapped region size per request we will allow in a negotiated
 * block-front/back communication channel.
 */
#define	XBB_MAX_REQUEST_SIZE		\
	MIN(MAXPHYS, BLKIF_MAX_SEGMENTS_PER_REQUEST * PAGE_SIZE)

/**
 * The maximum number of segments (within a request header and accompanying
 * segment blocks) per request we will allow in a negotiated block-front/back
 * communication channel.
 */
#define	XBB_MAX_SEGMENTS_PER_REQUEST			\
	(MIN(UIO_MAXIOV,				\
	     MIN(BLKIF_MAX_SEGMENTS_PER_REQUEST,	\
		 (XBB_MAX_REQUEST_SIZE / PAGE_SIZE) + 1)))

/**
 * The maximum number of shared memory ring pages we will allow in a
 * negotiated block-front/back communication channel.  Allow enough
 * ring space for all requests to be XBB_MAX_REQUEST_SIZE'd.
 */
#define	XBB_MAX_RING_PAGES						    \
	BLKIF_RING_PAGES(BLKIF_SEGS_TO_BLOCKS(XBB_MAX_SEGMENTS_PER_REQUEST) \
		       * XBB_MAX_REQUESTS)

/*--------------------------- Forward Declarations ---------------------------*/
struct xbb_softc;

static void xbb_attach_failed(struct xbb_softc *xbb, int err, const char *fmt,
			      ...) __attribute__((format(printf, 3, 4)));
static int  xbb_shutdown(struct xbb_softc *xbb);
static int  xbb_detach(device_t dev);

/*------------------------------ Data Structures -----------------------------*/
/**
 * \brief Object tracking an in-flight I/O from a Xen VBD consumer.
 */
struct xbb_xen_req {
	/**
	 * Linked list links used to aggregate idle request in the
	 * request free pool (xbb->request_free_slist).
	 */
	SLIST_ENTRY(xbb_xen_req) links;

	/**
	 * Back reference to the parent block back instance for this
	 * request.  Used during bio_done handling.
	 */
	struct xbb_softc        *xbb;

	/**
	 * The remote domain's identifier for this I/O request.
	 */
	uint64_t		 id;

	/**
	 * Kernel virtual address space reserved for this request
	 * structure and used to map the remote domain's pages for
	 * this I/O, into our domain's address space.
	 */
	uint8_t			*kva;

#ifdef XBB_USE_BOUNCE_BUFFERS
	/**
	 * Pre-allocated domain local memory used to proxy remote
	 * domain memory during I/O operations.
	 */
	uint8_t			*bounce;
#endif

	/**
	 * Base, psuedo-physical address, corresponding to the start
	 * of this request's kva region.
	 */
	uint64_t	 	 gnt_base;

	/**
	 * The number of pages currently mapped for this request.
	 */
	int			 nr_pages;

	/**
	 * The number of 512 byte sectors comprising this requests.
	 */
	int			 nr_512b_sectors;

	/**
	 * The number of struct bio requests still outstanding for this
	 * request on the backend device.  This field is only used for	
	 * device (rather than file) backed I/O.
	 */
	int			 pendcnt;

	/**
	 * BLKIF_OP code for this request.
	 */
	int			 operation;

	/**
	 * BLKIF_RSP status code for this request.
	 *
	 * This field allows an error status to be recorded even if the
	 * delivery of this status must be deferred.  Deferred reporting
	 * is necessary, for example, when an error is detected during
	 * completion processing of one bio when other bios for this
	 * request are still outstanding.
	 */
	int			 status;

	/**
	 * Device statistics request ordering type (ordered or simple).
	 */
	devstat_tag_type	 ds_tag_type;

	/**
	 * Device statistics request type (read, write, no_data).
	 */
	devstat_trans_flags	 ds_trans_type;

	/**
	 * The start time for this request.
	 */
	struct bintime		 ds_t0;

	/**
	 * Array of grant handles (one per page) used to map this request.
	 */
	grant_handle_t		*gnt_handles;
};
SLIST_HEAD(xbb_xen_req_slist, xbb_xen_req);

/**
 * \brief Configuration data for the shared memory request ring
 *        used to communicate with the front-end client of this
 *        this driver.
 */
struct xbb_ring_config {
	/** KVA address where ring memory is mapped. */
	vm_offset_t	va;

	/** The pseudo-physical address where ring memory is mapped.*/
	uint64_t	gnt_addr;

	/**
	 * Grant table handles, one per-ring page, returned by the
	 * hyperpervisor upon mapping of the ring and required to
	 * unmap it when a connection is torn down.
	 */
	grant_handle_t	handle[XBB_MAX_RING_PAGES];

	/**
	 * The device bus address returned by the hypervisor when
	 * mapping the ring and required to unmap it when a connection
	 * is torn down.
	 */
	uint64_t	bus_addr[XBB_MAX_RING_PAGES];

	/** The number of ring pages mapped for the current connection. */
	u_int		ring_pages;

	/**
	 * The grant references, one per-ring page, supplied by the
	 * front-end, allowing us to reference the ring pages in the
	 * front-end's domain and to map these pages into our own domain.
	 */
	grant_ref_t	ring_ref[XBB_MAX_RING_PAGES];

	/** The interrupt driven even channel used to signal ring events. */
	evtchn_port_t   evtchn;
};

/**
 * Per-instance connection state flags.
 */
typedef enum
{
	/**
	 * The front-end requested a read-only mount of the
	 * back-end device/file.
	 */
	XBBF_READ_ONLY         = 0x01,

	/** Communication with the front-end has been established. */
	XBBF_RING_CONNECTED    = 0x02,

	/**
	 * Front-end requests exist in the ring and are waiting for
	 * xbb_xen_req objects to free up.
	 */
	XBBF_RESOURCE_SHORTAGE = 0x04,

	/** Connection teardown in progress. */
	XBBF_SHUTDOWN          = 0x08
} xbb_flag_t;

/** Backend device type.  */
typedef enum {
	/** Backend type unknown. */
	XBB_TYPE_NONE		= 0x00,

	/**
	 * Backend type disk (access via cdev switch
	 * strategy routine).
	 */
	XBB_TYPE_DISK		= 0x01,

	/** Backend type file (access vnode operations.). */
	XBB_TYPE_FILE		= 0x02
} xbb_type;

/**
 * \brief Structure used to memoize information about a per-request
 *        scatter-gather list.
 *
 * The chief benefit of using this data structure is it avoids having
 * to reparse the possibly discontiguous S/G list in the original
 * request.  Due to the way that the mapping of the memory backing an
 * I/O transaction is handled by Xen, a second pass is unavoidable.
 * At least this way the second walk is a simple array traversal.
 *
 * \note A single Scatter/Gather element in the block interface covers
 *       at most 1 machine page.  In this context a sector (blkif
 *       nomenclature, not what I'd choose) is a 512b aligned unit
 *       of mapping within the machine page referenced by an S/G
 *       element.
 */
struct xbb_sg {
	/** The number of 512b data chunks mapped in this S/G element. */
	int16_t nsect;

	/**
	 * The index (0 based) of the first 512b data chunk mapped
	 * in this S/G element.
	 */
	uint8_t first_sect;

	/**
	 * The index (0 based) of the last 512b data chunk mapped
	 * in this S/G element.
	 */
	uint8_t last_sect;
};

/**
 * Character device backend specific configuration data.
 */
struct xbb_dev_data {
	/** Cdev used for device backend access.  */
	struct cdev   *cdev;

	/** Cdev switch used for device backend access.  */
	struct cdevsw *csw;

	/** Used to hold a reference on opened cdev backend devices. */
	int	       dev_ref;
};

/**
 * File backend specific configuration data.
 */
struct xbb_file_data {
	/** Credentials to use for vnode backed (file based) I/O. */
	struct ucred   *cred;

	/**
	 * \brief Array of io vectors used to process file based I/O.
	 *
	 * Only a single file based request is outstanding per-xbb instance,
	 * so we only need one of these.
	 */
	struct iovec	xiovecs[XBB_MAX_SEGMENTS_PER_REQUEST];
#ifdef XBB_USE_BOUNCE_BUFFERS

	/**
	 * \brief Array of io vectors used to handle bouncing of file reads.
	 *
	 * Vnode operations are free to modify uio data during their
	 * exectuion.  In the case of a read with bounce buffering active,
	 * we need some of the data from the original uio in order to
	 * bounce-out the read data.  This array serves as the temporary
	 * storage for this saved data.
	 */
	struct iovec	saved_xiovecs[XBB_MAX_SEGMENTS_PER_REQUEST];

	/**
	 * \brief Array of memoized bounce buffer kva offsets used
	 *        in the file based backend.
	 *
	 * Due to the way that the mapping of the memory backing an
	 * I/O transaction is handled by Xen, a second pass through
	 * the request sg elements is unavoidable. We memoize the computed
	 * bounce address here to reduce the cost of the second walk.
	 */
	void		*xiovecs_vaddr[XBB_MAX_SEGMENTS_PER_REQUEST];
#endif /* XBB_USE_BOUNCE_BUFFERS */
};

/**
 * Collection of backend type specific data.
 */
union xbb_backend_data {
	struct xbb_dev_data  dev;
	struct xbb_file_data file;
};

/**
 * Function signature of backend specific I/O handlers.
 */
typedef int (*xbb_dispatch_t)(struct xbb_softc *xbb, blkif_request_t *ring_req,
			      struct xbb_xen_req *req, int nseg,
			      int operation, int flags);

/**
 * Per-instance configuration data.
 */
struct xbb_softc {

	/**
	 * Task-queue used to process I/O requests.
	 */
	struct taskqueue	 *io_taskqueue;

	/**
	 * Single "run the request queue" task enqueued
	 * on io_taskqueue.
	 */
	struct task		  io_task;

	/** Device type for this instance. */
	xbb_type		  device_type;

	/** NewBus device corresponding to this instance. */
	device_t		  dev;

	/** Backend specific dispatch routine for this instance. */
	xbb_dispatch_t		  dispatch_io;

	/** The number of requests outstanding on the backend device/file. */
	u_int			  active_request_count;

	/** Free pool of request tracking structures. */
	struct xbb_xen_req_slist  request_free_slist;

	/** Array, sized at connection time, of request tracking structures. */
	struct xbb_xen_req	 *requests;

	/**
	 * Global pool of kva used for mapping remote domain ring
	 * and I/O transaction data.
	 */
	vm_offset_t		  kva;

	/** Psuedo-physical address corresponding to kva. */
	uint64_t		  gnt_base_addr;

	/** The size of the global kva pool. */
	int			  kva_size;

	/**
	 * \brief Cached value of the front-end's domain id.
	 * 
	 * This value is used at once for each mapped page in
	 * a transaction.  We cache it to avoid incuring the
	 * cost of an ivar access every time this is needed.
	 */
	domid_t			  otherend_id;

	/**
	 * \brief The blkif protocol abi in effect.
	 *
	 * There are situations where the back and front ends can
	 * have a different, native abi (e.g. intel x86_64 and
	 * 32bit x86 domains on the same machine).  The back-end
	 * always accomodates the front-end's native abi.  That
	 * value is pulled from the XenStore and recorded here.
	 */
	int			  abi;

	/**
	 * \brief The maximum number of requests allowed to be in
	 *        flight at a time.
	 *
	 * This value is negotiated via the XenStore.
	 */
	uint32_t		  max_requests;

	/**
	 * \brief The maximum number of segments (1 page per segment)
	 *	  that can be mapped by a request.
	 *
	 * This value is negotiated via the XenStore.
	 */
	uint32_t		  max_request_segments;

	/**
	 * The maximum size of any request to this back-end
	 * device.
	 *
	 * This value is negotiated via the XenStore.
	 */
	uint32_t		  max_request_size;

	/** Various configuration and state bit flags. */
	xbb_flag_t		  flags;

	/** Ring mapping and interrupt configuration data. */
	struct xbb_ring_config	  ring_config;

	/** Runtime, cross-abi safe, structures for ring access. */
	blkif_back_rings_t	  rings;

	/** IRQ mapping for the communication ring event channel. */
	int			  irq;

	/**
	 * \brief Backend access mode flags (e.g. write, or read-only).
	 *
	 * This value is passed to us by the front-end via the XenStore.
	 */
	char			 *dev_mode;

	/**
	 * \brief Backend device type (e.g. "disk", "cdrom", "floppy").
	 *
	 * This value is passed to us by the front-end via the XenStore.
	 * Currently unused.
	 */
	char			 *dev_type;

	/**
	 * \brief Backend device/file identifier.
	 *
	 * This value is passed to us by the front-end via the XenStore.
	 * We expect this to be a POSIX path indicating the file or
	 * device to open.
	 */
	char			 *dev_name;

	/**
	 * Vnode corresponding to the backend device node or file
	 * we are acessing.
	 */
	struct vnode		 *vn;

	union xbb_backend_data	  backend;
	/** The native sector size of the backend. */
	u_int			  sector_size;

	/** log2 of sector_size.  */
	u_int			  sector_size_shift;

	/** Size in bytes of the backend device or file.  */
	off_t			  media_size;

	/**
	 * \brief media_size expressed in terms of the backend native
	 *	  sector size.
	 *
	 * (e.g. xbb->media_size >> xbb->sector_size_shift).
	 */
	uint64_t		  media_num_sectors;

	/**
	 * \brief Array of memoized scatter gather data computed during the
	 *	  conversion of blkif ring requests to internal xbb_xen_req
	 *	  structures.
	 *
	 * Ring processing is serialized so we only need one of these.
	 */
	struct xbb_sg		  xbb_sgs[XBB_MAX_SEGMENTS_PER_REQUEST];

	/** Mutex protecting per-instance data. */
	struct mtx		  lock;

#ifdef XENHVM
	/**
	 * Resource representing allocated physical address space
	 * associated with our per-instance kva region.
	 */
	struct resource		 *pseudo_phys_res;

	/** Resource id for allocated physical address space. */
	int			  pseudo_phys_res_id;
#endif

	/** I/O statistics. */
	struct devstat		 *xbb_stats;
};

/*---------------------------- Request Processing ----------------------------*/
/**
 * Allocate an internal transaction tracking structure from the free pool.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * \return  On success, a pointer to the allocated xbb_xen_req structure.
 *          Otherwise NULL.
 */
static inline struct xbb_xen_req *
xbb_get_req(struct xbb_softc *xbb)
{
	struct xbb_xen_req *req;

	req = NULL;
	mtx_lock(&xbb->lock);

	/*
	 * Do not allow new requests to be allocated while we
	 * are shutting down.
	 */
	if ((xbb->flags & XBBF_SHUTDOWN) == 0) {
		if ((req = SLIST_FIRST(&xbb->request_free_slist)) != NULL) {
			SLIST_REMOVE_HEAD(&xbb->request_free_slist, links);
			xbb->active_request_count++;
		} else {
			xbb->flags |= XBBF_RESOURCE_SHORTAGE;
		}
	}
	mtx_unlock(&xbb->lock);
	return (req);
}

/**
 * Return an allocated transaction tracking structure to the free pool.
 *
 * \param xbb  Per-instance xbb configuration structure.
 * \param req  The request structure to free.
 */
static inline void
xbb_release_req(struct xbb_softc *xbb, struct xbb_xen_req *req)
{
	int wake_thread;

	mtx_lock(&xbb->lock);
	wake_thread = xbb->flags & XBBF_RESOURCE_SHORTAGE;
	xbb->flags &= ~XBBF_RESOURCE_SHORTAGE;
	SLIST_INSERT_HEAD(&xbb->request_free_slist, req, links);
	xbb->active_request_count--;

	if ((xbb->flags & XBBF_SHUTDOWN) != 0) {
		/*
		 * Shutdown is in progress.  See if we can
		 * progress further now that one more request
		 * has completed and been returned to the
		 * free pool.
		 */
		xbb_shutdown(xbb);
	}
	mtx_unlock(&xbb->lock);

	if (wake_thread != 0)
		taskqueue_enqueue(xbb->io_taskqueue, &xbb->io_task); 
}

/**
 * Given a page index and 512b sector offset within that page,
 * calculate an offset into a request's kva region.
 *
 * \param req     The request structure whose kva region will be accessed.
 * \param pagenr  The page index used to compute the kva offset.
 * \param sector  The 512b sector index used to compute the page relative
 *                kva offset.
 *
 * \return  The computed global KVA offset.
 */
static inline uint8_t *
xbb_req_vaddr(struct xbb_xen_req *req, int pagenr, int sector)
{
	return (req->kva + (PAGE_SIZE * pagenr) + (sector << 9));
}

#ifdef XBB_USE_BOUNCE_BUFFERS
/**
 * Given a page index and 512b sector offset within that page,
 * calculate an offset into a request's local bounce memory region.
 *
 * \param req     The request structure whose bounce region will be accessed.
 * \param pagenr  The page index used to compute the bounce offset.
 * \param sector  The 512b sector index used to compute the page relative
 *                bounce offset.
 *
 * \return  The computed global bounce buffer address.
 */
static inline uint8_t *
xbb_req_bounce_addr(struct xbb_xen_req *req, int pagenr, int sector)
{
	return (req->bounce + (PAGE_SIZE * pagenr) + (sector << 9));
}
#endif

/**
 * Given a page number and 512b sector offset within that page,
 * calculate an offset into the request's memory region that the
 * underlying backend device/file should use for I/O.
 *
 * \param req     The request structure whose I/O region will be accessed.
 * \param pagenr  The page index used to compute the I/O offset.
 * \param sector  The 512b sector index used to compute the page relative
 *                I/O offset.
 *
 * \return  The computed global I/O address.
 *
 * Depending on configuration, this will either be a local bounce buffer
 * or a pointer to the memory mapped in from the front-end domain for
 * this request.
 */
static inline uint8_t *
xbb_req_ioaddr(struct xbb_xen_req *req, int pagenr, int sector)
{
#ifdef XBB_USE_BOUNCE_BUFFERS
	return (xbb_req_bounce_addr(req, pagenr, sector));
#else
	return (xbb_req_vaddr(req, pagenr, sector));
#endif
}

/**
 * Given a page index and 512b sector offset within that page, calculate
 * an offset into the local psuedo-physical address space used to map a
 * front-end's request data into a request.
 *
 * \param req     The request structure whose pseudo-physical region
 *                will be accessed.
 * \param pagenr  The page index used to compute the pseudo-physical offset.
 * \param sector  The 512b sector index used to compute the page relative
 *                pseudo-physical offset.
 *
 * \return  The computed global pseudo-phsyical address.
 *
 * Depending on configuration, this will either be a local bounce buffer
 * or a pointer to the memory mapped in from the front-end domain for
 * this request.
 */
static inline uintptr_t
xbb_req_gntaddr(struct xbb_xen_req *req, int pagenr, int sector)
{
	return ((uintptr_t)(req->gnt_base
			  + (PAGE_SIZE * pagenr) + (sector << 9)));
}

/**
 * Unmap the front-end pages associated with this I/O request.
 *
 * \param req  The request structure to unmap.
 */
static void
xbb_unmap_req(struct xbb_xen_req *req)
{
	struct gnttab_unmap_grant_ref unmap[XBB_MAX_SEGMENTS_PER_REQUEST];
	u_int			      i;
	u_int			      invcount;
	int			      error;

	invcount = 0;
	for (i = 0; i < req->nr_pages; i++) {

		if (req->gnt_handles[i] == GRANT_REF_INVALID)
			continue;

		unmap[invcount].host_addr    = xbb_req_gntaddr(req, i, 0);
		unmap[invcount].dev_bus_addr = 0;
		unmap[invcount].handle       = req->gnt_handles[i];
		req->gnt_handles[i]	     = GRANT_REF_INVALID;
		invcount++;
	}

	error = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref,
					  unmap, invcount);
	KASSERT(error == 0, ("Grant table operation failed"));
}

/**
 * Create and transmit a response to a blkif request.
 * 
 * \param xbb     Per-instance xbb configuration structure.
 * \param req     The request structure to which to respond.
 * \param status  The status code to report.  See BLKIF_RSP_*
 *                in sys/xen/interface/io/blkif.h.
 */
static void
xbb_send_response(struct xbb_softc *xbb, struct xbb_xen_req *req, int status)
{
	blkif_response_t *resp;
	int		  more_to_do;
	int		  notify;

	more_to_do = 0;

	/*
	 * Place on the response ring for the relevant domain.
	 * For now, only the spacing between entries is different
	 * in the different ABIs, not the response entry layout.
	 */
	mtx_lock(&xbb->lock);
	switch (xbb->abi) {
	case BLKIF_PROTOCOL_NATIVE:
		resp = RING_GET_RESPONSE(&xbb->rings.native,
					 xbb->rings.native.rsp_prod_pvt);
		break;
	case BLKIF_PROTOCOL_X86_32:
		resp = (blkif_response_t *)
		    RING_GET_RESPONSE(&xbb->rings.x86_32,
				      xbb->rings.x86_32.rsp_prod_pvt);
		break;
	case BLKIF_PROTOCOL_X86_64:
		resp = (blkif_response_t *)
		    RING_GET_RESPONSE(&xbb->rings.x86_64,
				      xbb->rings.x86_64.rsp_prod_pvt);
		break;
	default:
		panic("Unexpected blkif protocol ABI.");
	}

	resp->id        = req->id;
	resp->operation = req->operation;
	resp->status    = status;

	xbb->rings.common.rsp_prod_pvt += BLKIF_SEGS_TO_BLOCKS(req->nr_pages);
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&xbb->rings.common, notify);

	if (xbb->rings.common.rsp_prod_pvt == xbb->rings.common.req_cons) {

		/*
		 * Tail check for pending requests. Allows frontend to avoid
		 * notifications if requests are already in flight (lower
		 * overheads and promotes batching).
		 */
		RING_FINAL_CHECK_FOR_REQUESTS(&xbb->rings.common, more_to_do);
	} else if (RING_HAS_UNCONSUMED_REQUESTS(&xbb->rings.common)) {

		more_to_do = 1;
	}

	mtx_unlock(&xbb->lock);

	if (more_to_do)
		taskqueue_enqueue(xbb->io_taskqueue, &xbb->io_task); 

	if (notify)
		notify_remote_via_irq(xbb->irq);
}

/**
 * Completion handler for buffer I/O requests issued by the device
 * backend driver.
 *
 * \param bio  The buffer I/O request on which to perform completion
 *             processing.
 */
static void
xbb_bio_done(struct bio *bio)
{
	struct xbb_softc   *xbb;
	struct xbb_xen_req *req;

	req = bio->bio_caller1;
	xbb = req->xbb;

	/* Only include transferred I/O in stats. */
	req->nr_512b_sectors -= bio->bio_resid >> 9;
	if (bio->bio_error) {
		DPRINTF("BIO returned error %d for operation on device %s\n",
			bio->bio_error, xbb->dev_name);
		req->status = BLKIF_RSP_ERROR;

		if (bio->bio_error == ENXIO
		 && xenbus_get_state(xbb->dev) == XenbusStateConnected) {

			/*
			 * Backend device has disappeared.  Signal the
			 * front-end that we (the device proxy) want to
			 * go away.
			 */
			xenbus_set_state(xbb->dev, XenbusStateClosing);
		}
	}

#ifdef XBB_USE_BOUNCE_BUFFERS
	if (bio->bio_cmd == BIO_READ) {
		vm_offset_t kva_offset;

		kva_offset = (vm_offset_t)bio->bio_data
			   - (vm_offset_t)req->bounce;
		memcpy((uint8_t *)req->kva + kva_offset,
		       bio->bio_data, bio->bio_bcount);
	}
#endif /* XBB_USE_BOUNCE_BUFFERS */

	if (atomic_fetchadd_int(&req->pendcnt, -1) == 1) {
		xbb_unmap_req(req);
		xbb_send_response(xbb, req, req->status);
		devstat_end_transaction(xbb->xbb_stats,
					/*bytes*/req->nr_512b_sectors << 9,
					req->ds_tag_type,
					req->ds_trans_type,
					/*now*/NULL,
					/*then*/&req->ds_t0);
		xbb_release_req(xbb, req);
	}

	g_destroy_bio(bio);
}

/**
 * Parse a blkif request into an internal request structure and send
 * it to the backend for processing.
 *
 * \param xbb           Per-instance xbb configuration structure.
 * \param ring_req      Front-end's I/O request as pulled from the shared
 *                      communication ring.
 * \param req           Allocated internal request structure.
 * \param req_ring_idx  The location of ring_req within the shared
 *                      communication ring.
 *
 * This routine performs the backend common aspects of request parsing
 * including compiling an internal request structure, parsing the S/G
 * list and any secondary ring requests in which they may reside, and
 * the mapping of front-end I/O pages into our domain.
 */
static void
xbb_dispatch_io(struct xbb_softc *xbb, blkif_request_t *ring_req,
		struct xbb_xen_req *req, RING_IDX req_ring_idx)
{
	struct gnttab_map_grant_ref   maps[XBB_MAX_SEGMENTS_PER_REQUEST];
	struct xbb_sg                *xbb_sg;
	struct gnttab_map_grant_ref  *map;
	struct blkif_request_segment *sg;
	struct blkif_request_segment *last_block_sg;
	u_int			      nseg;
	u_int			      seg_idx;
	u_int			      block_segs;
	int			      nr_sects;
	int			      operation;
	uint8_t			      bio_flags;
	int			      error;

	nseg                 = ring_req->nr_segments;
	nr_sects             = 0;
	req->xbb             = xbb;
	req->id              = ring_req->id;
	req->operation       = ring_req->operation;
	req->status          = BLKIF_RSP_OKAY;
	req->ds_tag_type     = DEVSTAT_TAG_SIMPLE;
	req->nr_pages        = nseg;
	req->nr_512b_sectors = 0;
	bio_flags            = 0;
	sg	             = NULL;

	binuptime(&req->ds_t0);
	devstat_start_transaction(xbb->xbb_stats, &req->ds_t0);

	switch (req->operation) {
	case BLKIF_OP_WRITE_BARRIER:
		bio_flags       |= BIO_ORDERED;
		req->ds_tag_type = DEVSTAT_TAG_ORDERED;
		/* FALLTHROUGH */
	case BLKIF_OP_WRITE:
		operation = BIO_WRITE;
		req->ds_trans_type = DEVSTAT_WRITE;
		if ((xbb->flags & XBBF_READ_ONLY) != 0) {
			DPRINTF("Attempt to write to read only device %s\n",
				xbb->dev_name);
			goto fail_send_response;
		}
		break;
	case BLKIF_OP_READ:
		operation = BIO_READ;
		req->ds_trans_type = DEVSTAT_READ;
		break;
	case BLKIF_OP_FLUSH_DISKCACHE:
		operation = BIO_FLUSH;
		req->ds_tag_type = DEVSTAT_TAG_ORDERED;
		req->ds_trans_type = DEVSTAT_NO_DATA;
		goto do_dispatch;
		/*NOTREACHED*/
	default:
		DPRINTF("error: unknown block io operation [%d]\n",
			req->operation);
		goto fail_send_response;
	}

	/* Check that number of segments is sane. */
	if (unlikely(nseg == 0)
	 || unlikely(nseg > xbb->max_request_segments)) {
		DPRINTF("Bad number of segments in request (%d)\n", nseg);
		goto fail_send_response;
	}

	map	      = maps;
	xbb_sg        = xbb->xbb_sgs;
	block_segs    = MIN(req->nr_pages, BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK);
	sg            = ring_req->seg;
	last_block_sg = sg + block_segs;
	seg_idx	      = 0;
	while (1) {

		while (sg < last_block_sg) {
			
			xbb_sg->first_sect = sg->first_sect;
			xbb_sg->last_sect  = sg->last_sect;
			xbb_sg->nsect =
			    (int8_t)(sg->last_sect - sg->first_sect + 1);

			if ((sg->last_sect >= (PAGE_SIZE >> 9))
			 || (xbb_sg->nsect <= 0))
				goto fail_send_response;

			nr_sects += xbb_sg->nsect;
			map->host_addr = xbb_req_gntaddr(req, seg_idx,
							 /*sector*/0);
			map->flags     = GNTMAP_host_map;
			map->ref       = sg->gref;
			map->dom       = xbb->otherend_id;
			if (operation == BIO_WRITE)
				map->flags |= GNTMAP_readonly;
			sg++;
			map++;
			xbb_sg++;
			seg_idx++;
		}

		block_segs = MIN(nseg - seg_idx,
				 BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK);
		if (block_segs == 0)
			break;

		/*
		 * Fetch the next request block full of SG elements.
		 * For now, only the spacing between entries is different
		 * in the different ABIs, not the sg entry layout.
		 */
		req_ring_idx++;
		switch (xbb->abi) {
		case BLKIF_PROTOCOL_NATIVE:
			sg = BLKRING_GET_SG_REQUEST(&xbb->rings.native,
						    req_ring_idx);
			break;
		case BLKIF_PROTOCOL_X86_32:
		{
			sg = BLKRING_GET_SG_REQUEST(&xbb->rings.x86_32,
						    req_ring_idx);
			break;
		}
		case BLKIF_PROTOCOL_X86_64:
		{
			sg = BLKRING_GET_SG_REQUEST(&xbb->rings.x86_64,
						    req_ring_idx);
			break;
		}
		default:
			panic("Unexpected blkif protocol ABI.");
			/* NOTREACHED */
		} 
		last_block_sg = sg + block_segs;
	}

	/* Convert to the disk's sector size */
	req->nr_512b_sectors = nr_sects;
	nr_sects = (nr_sects << 9) >> xbb->sector_size_shift;

	if ((req->nr_512b_sectors & ((xbb->sector_size >> 9) - 1)) != 0) {
		device_printf(xbb->dev, "%s: I/O size (%d) is not a multiple "
			      "of the backing store sector size (%d)\n",
			      __func__, req->nr_512b_sectors << 9,
			      xbb->sector_size);
		goto fail_send_response;
	}

	error = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref,
					  maps, req->nr_pages);
	if (error != 0)
		panic("Grant table operation failed (%d)", error);

	for (seg_idx = 0, map = maps; seg_idx < nseg; seg_idx++, map++) {

		if (unlikely(map->status != 0)) {
			DPRINTF("invalid buffer -- could not remap it (%d)\n",
				map->status);
			DPRINTF("Mapping(%d): Host Addr 0x%lx, flags 0x%x "
				"ref 0x%x, dom %d\n", seg_idx,
				map->host_addr, map->flags, map->ref,
				map->dom);
			goto fail_unmap_req;
		}

		req->gnt_handles[seg_idx] = map->handle;
	}
	if (ring_req->sector_number + nr_sects > xbb->media_num_sectors) {

		DPRINTF("%s of [%" PRIu64 ",%" PRIu64 "] "
			"extends past end of device %s\n",
			operation == BIO_READ ? "read" : "write",
			ring_req->sector_number,
			ring_req->sector_number + nr_sects, xbb->dev_name); 
		goto fail_unmap_req;
	}

do_dispatch:

	error = xbb->dispatch_io(xbb,
				 ring_req,
				 req,
				 nseg,
				 operation,
				 bio_flags);

	if (error != 0) {
		if (operation == BIO_FLUSH)
			goto fail_send_response;
		else
			goto fail_unmap_req;
	}

	return;


fail_unmap_req:
	xbb_unmap_req(req);
	/* FALLTHROUGH */

fail_send_response:
	xbb_send_response(xbb, req, BLKIF_RSP_ERROR);
	xbb_release_req(xbb, req);
	devstat_end_transaction(xbb->xbb_stats,
				/*bytes*/0,
				req->ds_tag_type,
				req->ds_trans_type,
				/*now*/NULL,
				/*then*/&req->ds_t0);
}

/**
 * Process incoming requests from the shared communication ring in response
 * to a signal on the ring's event channel.
 *
 * \param context  Callback argument registerd during task initialization -
 *                 the xbb_softc for this instance.
 * \param pending  The number of taskqueue_enqueue events that have
 *                 occurred since this handler was last run.
 */
static void
xbb_run_queue(void *context, int pending)
{
	struct xbb_softc   *xbb;
	blkif_back_rings_t *rings;
	RING_IDX	    rp;


	xbb   = (struct xbb_softc *)context;
	rings = &xbb->rings;

	/*
	 * Cache req_prod to avoid accessing a cache line shared
	 * with the frontend.
	 */
	rp = rings->common.sring->req_prod;

	/* Ensure we see queued requests up to 'rp'. */
	rmb();

	/**
	 * Run so long as there is work to consume and the generation
	 * of a response will not overflow the ring.
	 *
	 * @note There's a 1 to 1 relationship between requests and responses,
	 *       so an overflow should never occur.  This test is to protect
	 *       our domain from digesting bogus data.  Shouldn't we log this?
	 */
	while (rings->common.req_cons != rp
	    && RING_REQUEST_CONS_OVERFLOW(&rings->common,
					  rings->common.req_cons) == 0) {
		blkif_request_t     ring_req_storage;
		blkif_request_t    *ring_req;
		struct xbb_xen_req *req;
		RING_IDX	    req_ring_idx;

		req = xbb_get_req(xbb);
		if (req == NULL) {
			/*
			 * Resource shortage has been recorded.
			 * We'll be scheduled to run once a request
			 * object frees up due to a completion.
			 */
			break;
		}

		switch (xbb->abi) {
		case BLKIF_PROTOCOL_NATIVE:
			ring_req = RING_GET_REQUEST(&xbb->rings.native,
						    rings->common.req_cons);
			break;
		case BLKIF_PROTOCOL_X86_32:
		{
			struct blkif_x86_32_request *ring_req32;

			ring_req32 = RING_GET_REQUEST(&xbb->rings.x86_32,
						      rings->common.req_cons);
			blkif_get_x86_32_req(&ring_req_storage, ring_req32);
			ring_req = &ring_req_storage;
			break;
		}
		case BLKIF_PROTOCOL_X86_64:
		{
			struct blkif_x86_64_request *ring_req64;

			ring_req64 = RING_GET_REQUEST(&xbb->rings.x86_64,
						      rings->common.req_cons);
			blkif_get_x86_64_req(&ring_req_storage, ring_req64);
			ring_req = &ring_req_storage;
			break;
		}
		default:
			panic("Unexpected blkif protocol ABI.");
			/* NOTREACHED */
		} 

		/*
		 * Signify that	we can overwrite this request with a
		 * response by incrementing our consumer index. The
		 * response won't be generated until after we've already
		 * consumed all necessary data out of the version of the
		 * request in the ring buffer (for native mode).  We
		 * must update the consumer index  before issueing back-end
		 * I/O so there is no possibility that it will complete
		 * and a response be generated before we make room in
		 * the queue for that response.
		 */
		req_ring_idx = xbb->rings.common.req_cons;
		xbb->rings.common.req_cons +=
		    BLKIF_SEGS_TO_BLOCKS(ring_req->nr_segments);

		xbb_dispatch_io(xbb, ring_req, req, req_ring_idx);
	}
}

/**
 * Interrupt handler bound to the shared ring's event channel.
 *
 * \param arg  Callback argument registerd during event channel
 *             binding - the xbb_softc for this instance.
 */
static void
xbb_intr(void *arg)
{
	struct xbb_softc *xbb;

	/* Defer to kernel thread. */
	xbb = (struct xbb_softc *)arg;
	taskqueue_enqueue(xbb->io_taskqueue, &xbb->io_task); 
}

/*----------------------------- Backend Handlers -----------------------------*/
/**
 * Backend handler for character device access.
 *
 * \param xbb        Per-instance xbb configuration structure.
 * \param ring_req   Front-end's I/O request as pulled from the shared
 *                   communication ring.
 * \param req        Allocated internal request structure.
 * \param nseg       The number of valid segments for this request in
 *                   xbb->xbb_sgs.
 * \param operation  BIO_* I/O operation code.
 * \param bio_flags  Additional bio_flag data to pass to any generated
 *                   bios (e.g. BIO_ORDERED)..
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_dispatch_dev(struct xbb_softc *xbb, blkif_request_t *ring_req,
		 struct xbb_xen_req *req, int nseg, int operation,
		 int bio_flags)
{
	struct xbb_dev_data *dev_data;
	struct bio          *bios[XBB_MAX_SEGMENTS_PER_REQUEST];
	off_t                bio_offset;
	struct bio          *bio;
	struct xbb_sg       *xbb_sg;
	u_int	             nbio;
	u_int                bio_idx;
	u_int                seg_idx;
	int                  error;

	dev_data   = &xbb->backend.dev;
	bio_offset = (off_t)ring_req->sector_number
		   << xbb->sector_size_shift;
	error      = 0;
	nbio       = 0;
	bio_idx    = 0;

	if (operation == BIO_FLUSH) {
		bio = g_new_bio();
		if (unlikely(bio == NULL)) {
			DPRINTF("Unable to allocate bio for BIO_FLUSH\n");
			error = ENOMEM;
			return (error);
		}

		bio->bio_cmd	 = BIO_FLUSH;
		bio->bio_flags	|= BIO_ORDERED;
		bio->bio_dev	 = dev_data->cdev;
		bio->bio_offset	 = 0;
		bio->bio_data	 = 0;
		bio->bio_done	 = xbb_bio_done;
		bio->bio_caller1 = req;
		bio->bio_pblkno	 = 0;

		req->pendcnt	 = 1;

		(*dev_data->csw->d_strategy)(bios[bio_idx]);

		return (0);
	}

	for (seg_idx = 0, bio = NULL, xbb_sg = xbb->xbb_sgs;
	     seg_idx < nseg;
	     seg_idx++, xbb_sg++) {

		/*
		 * KVA will not be contiguous, so any additional
		 * I/O will need to be represented in a new bio.
		 */
		if ((bio != NULL)
		 && (xbb_sg->first_sect != 0)) {
			if ((bio->bio_length & (xbb->sector_size - 1)) != 0) {
				printf("%s: Discontiguous I/O request from "
				       "domain %d ends on non-sector "
				       "boundary\n", __func__,
				       xbb->otherend_id);
				error = EINVAL;
				goto fail_free_bios;
			}
			bio = NULL;
		}

		if (bio == NULL) {
			/*
			 * Make sure that the start of this bio is aligned
			 * to a device sector.
			 */
			if ((bio_offset & (xbb->sector_size - 1)) != 0) {
				printf("%s: Misaligned I/O request from "
				       "domain %d\n", __func__,
				       xbb->otherend_id);
				error = EINVAL;
				goto fail_free_bios;
			}

			bio = bios[nbio++] = g_new_bio();
			if (unlikely(bio == NULL)) {
				error = ENOMEM;
				goto fail_free_bios;
			}
			bio->bio_cmd     = operation;
			bio->bio_flags  |= bio_flags;
			bio->bio_dev     = dev_data->cdev;
			bio->bio_offset  = bio_offset;
			bio->bio_data    = xbb_req_ioaddr(req, seg_idx,
							  xbb_sg->first_sect);
			bio->bio_done    = xbb_bio_done;
			bio->bio_caller1 = req;
			bio->bio_pblkno  = bio_offset
				        >> xbb->sector_size_shift;
		}

		bio->bio_length += xbb_sg->nsect << 9;
		bio->bio_bcount  = bio->bio_length;
		bio_offset      += xbb_sg->nsect << 9;

		if (xbb_sg->last_sect != (PAGE_SIZE - 512) >> 9) {

			if ((bio->bio_length & (xbb->sector_size - 1)) != 0) {
				printf("%s: Discontiguous I/O request from "
				       "domain %d ends on non-sector "
				       "boundary\n", __func__,
				       xbb->otherend_id);
				error = EINVAL;
				goto fail_free_bios;
			}
			/*
			 * KVA will not be contiguous, so any additional
			 * I/O will need to be represented in a new bio.
			 */
			bio = NULL;
		}
	}

	req->pendcnt = nbio;

	for (bio_idx = 0; bio_idx < nbio; bio_idx++)
	{
#ifdef XBB_USE_BOUNCE_BUFFERS
		vm_offset_t kva_offset;

		kva_offset = (vm_offset_t)bios[bio_idx]->bio_data
			   - (vm_offset_t)req->bounce;
		if (operation == BIO_WRITE) {
			memcpy(bios[bio_idx]->bio_data,
			       (uint8_t *)req->kva + kva_offset,
			       bios[bio_idx]->bio_bcount);
		}
#endif
		(*dev_data->csw->d_strategy)(bios[bio_idx]);
	}

	return (error);

fail_free_bios:
	for (bio_idx = 0; bio_idx < (nbio-1); bio_idx++)
		g_destroy_bio(bios[bio_idx]);

	return (error);
}

/**
 * Backend handler for file access.
 *
 * \param xbb        Per-instance xbb configuration structure.
 * \param ring_req   Front-end's I/O request as pulled from the shared
 *                   communication ring.
 * \param req        Allocated internal request structure.
 * \param nseg       The number of valid segments for this request in
 *                   xbb->xbb_sgs.
 * \param operation  BIO_* I/O operation code.
 * \param bio_flags  Additional bio_flag data to pass to any generated bios
 *                   (e.g. BIO_ORDERED)..
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_dispatch_file(struct xbb_softc *xbb, blkif_request_t *ring_req,
		  struct xbb_xen_req *req, int nseg, int operation,
		  int flags)
{
	struct xbb_file_data *file_data;
	u_int                 seg_idx;
	struct uio            xuio;
	struct xbb_sg        *xbb_sg;
	struct iovec         *xiovec;
#ifdef XBB_USE_BOUNCE_BUFFERS
	void                **p_vaddr;
	int                   saved_uio_iovcnt;
#endif /* XBB_USE_BOUNCE_BUFFERS */
	int                   vfs_is_locked;
	int                   error;

	file_data = &xbb->backend.file;
	error = 0;
	bzero(&xuio, sizeof(xuio));

	req->pendcnt = 0;

	switch (operation) {
	case BIO_READ:
		xuio.uio_rw = UIO_READ;
		break;
	case BIO_WRITE:
		xuio.uio_rw = UIO_WRITE;
		break;
	case BIO_FLUSH: {
		struct mount *mountpoint;

		vfs_is_locked = VFS_LOCK_GIANT(xbb->vn->v_mount);

		(void) vn_start_write(xbb->vn, &mountpoint, V_WAIT);

		vn_lock(xbb->vn, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_FSYNC(xbb->vn, MNT_WAIT, curthread);
		VOP_UNLOCK(xbb->vn, 0);

		vn_finished_write(mountpoint);

		VFS_UNLOCK_GIANT(vfs_is_locked);

		goto bailout_send_response;
		/* NOTREACHED */
	}
	default:
		panic("invalid operation %d", operation);
		/* NOTREACHED */
	}
	xuio.uio_offset = (vm_offset_t)ring_req->sector_number
			<< xbb->sector_size_shift;

	xuio.uio_segflg = UIO_SYSSPACE;
	xuio.uio_iov = file_data->xiovecs;
	xuio.uio_iovcnt = 0;

	for (seg_idx = 0, xiovec = NULL, xbb_sg = xbb->xbb_sgs;
	     seg_idx < nseg; seg_idx++, xbb_sg++) {

		/*
		 * If the first sector is not 0, the KVA will not be
		 * contiguous and we'll need to go on to another segment.
		 */
		if (xbb_sg->first_sect != 0)
			xiovec = NULL;

		if (xiovec == NULL) {
			xiovec = &file_data->xiovecs[xuio.uio_iovcnt];
			xiovec->iov_base = xbb_req_ioaddr(req, seg_idx,
							  xbb_sg->first_sect);
#ifdef XBB_USE_BOUNCE_BUFFERS
			/*
			 * Store the address of the incoming buffer at this
			 * particular offset as well, so we can do the copy
			 * later without having to do more work to
			 * recalculate this address.
		 	 */
			p_vaddr = &file_data->xiovecs_vaddr[xuio.uio_iovcnt];
			*p_vaddr = xbb_req_vaddr(req, seg_idx,
						 xbb_sg->first_sect);
#endif /* XBB_USE_BOUNCE_BUFFERS */
			xiovec->iov_len = 0;
			xuio.uio_iovcnt++;
		}

		xiovec->iov_len += xbb_sg->nsect << 9;

		xuio.uio_resid += xbb_sg->nsect << 9;

		/*
		 * If the last sector is not the full page size count,
		 * the next segment will not be contiguous in KVA and we
		 * need a new iovec.
		 */
		if (xbb_sg->last_sect != (PAGE_SIZE - 512) >> 9)
			xiovec = NULL;
	}

	xuio.uio_td = curthread;

#ifdef XBB_USE_BOUNCE_BUFFERS
	saved_uio_iovcnt = xuio.uio_iovcnt;

	if (operation == BIO_WRITE) {
		/* Copy the write data to the local buffer. */
		for (seg_idx = 0, p_vaddr = file_data->xiovecs_vaddr,
		     xiovec = xuio.uio_iov; seg_idx < xuio.uio_iovcnt;
		     seg_idx++, xiovec++, p_vaddr++) {

			memcpy(xiovec->iov_base, *p_vaddr, xiovec->iov_len);
		}
	} else {
		/*
		 * We only need to save off the iovecs in the case of a
		 * read, because the copy for the read happens after the
		 * VOP_READ().  (The uio will get modified in that call
		 * sequence.)
		 */
		memcpy(file_data->saved_xiovecs, xuio.uio_iov,
		       xuio.uio_iovcnt * sizeof(xuio.uio_iov[0]));
	}
#endif /* XBB_USE_BOUNCE_BUFFERS */

	vfs_is_locked = VFS_LOCK_GIANT(xbb->vn->v_mount);
	switch (operation) {
	case BIO_READ:

		vn_lock(xbb->vn, LK_EXCLUSIVE | LK_RETRY);

		/*
		 * UFS pays attention to IO_DIRECT for reads.  If the
		 * DIRECTIO option is configured into the kernel, it calls
		 * ffs_rawread().  But that only works for single-segment
		 * uios with user space addresses.  In our case, with a
		 * kernel uio, it still reads into the buffer cache, but it
		 * will just try to release the buffer from the cache later
		 * on in ffs_read().
		 *
		 * ZFS does not pay attention to IO_DIRECT for reads.
		 *
		 * UFS does not pay attention to IO_SYNC for reads.
		 *
		 * ZFS pays attention to IO_SYNC (which translates into the
		 * Solaris define FRSYNC for zfs_read()) for reads.  It
		 * attempts to sync the file before reading.
		 *
		 * So, to attempt to provide some barrier semantics in the
		 * BIO_ORDERED case, set both IO_DIRECT and IO_SYNC.  
		 */
		error = VOP_READ(xbb->vn, &xuio, (flags & BIO_ORDERED) ? 
				 (IO_DIRECT|IO_SYNC) : 0, file_data->cred);

		VOP_UNLOCK(xbb->vn, 0);
		break;
	case BIO_WRITE: {
		struct mount *mountpoint;

		(void)vn_start_write(xbb->vn, &mountpoint, V_WAIT);

		vn_lock(xbb->vn, LK_EXCLUSIVE | LK_RETRY);

		/*
		 * UFS pays attention to IO_DIRECT for writes.  The write
		 * is done asynchronously.  (Normally the write would just
		 * get put into cache.
		 *
		 * UFS pays attention to IO_SYNC for writes.  It will
		 * attempt to write the buffer out synchronously if that
		 * flag is set.
		 *
		 * ZFS does not pay attention to IO_DIRECT for writes.
		 *
		 * ZFS pays attention to IO_SYNC (a.k.a. FSYNC or FRSYNC)
		 * for writes.  It will flush the transaction from the
		 * cache before returning.
		 *
		 * So if we've got the BIO_ORDERED flag set, we want
		 * IO_SYNC in either the UFS or ZFS case.
		 */
		error = VOP_WRITE(xbb->vn, &xuio, (flags & BIO_ORDERED) ?
				  IO_SYNC : 0, file_data->cred);
		VOP_UNLOCK(xbb->vn, 0);

		vn_finished_write(mountpoint);

		break;
	}
	default:
		panic("invalid operation %d", operation);
		/* NOTREACHED */
	}
	VFS_UNLOCK_GIANT(vfs_is_locked);

#ifdef XBB_USE_BOUNCE_BUFFERS
	/* We only need to copy here for read operations */
	if (operation == BIO_READ) {

		for (seg_idx = 0, p_vaddr = file_data->xiovecs_vaddr,
		     xiovec = file_data->saved_xiovecs;
		     seg_idx < saved_uio_iovcnt; seg_idx++,
		     xiovec++, p_vaddr++) {

			/*
			 * Note that we have to use the copy of the 
			 * io vector we made above.  uiomove() modifies
			 * the uio and its referenced vector as uiomove
			 * performs the copy, so we can't rely on any
			 * state from the original uio.
			 */
			memcpy(*p_vaddr, xiovec->iov_base, xiovec->iov_len);
		}
	}
#endif /* XBB_USE_BOUNCE_BUFFERS */

bailout_send_response:

	/*
	 * All I/O is already done, send the response.  A lock is not
	 * necessary here because we're single threaded, and therefore the
	 * only context accessing this request right now.  If that changes,
	 * we may need some locking here.
	 */
	xbb_unmap_req(req);
	xbb_send_response(xbb, req, (error == 0) ? BLKIF_RSP_OKAY :
			  BLKIF_RSP_ERROR);
	devstat_end_transaction(xbb->xbb_stats,
				/*bytes*/error == 0 ? req->nr_512b_sectors << 9
						    : 0,
				req->ds_tag_type,
				req->ds_trans_type,
				/*now*/NULL,
				/*then*/&req->ds_t0);
	xbb_release_req(xbb, req);

	return (0);
}

/*--------------------------- Backend Configuration --------------------------*/
/**
 * Close and cleanup any backend device/file specific state for this
 * block back instance. 
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static void
xbb_close_backend(struct xbb_softc *xbb)
{
	DROP_GIANT();
	DPRINTF("closing dev=%s\n", xbb->dev_name);
	if (xbb->vn) {
		int flags = FREAD;
		int vfs_is_locked = 0;

		if ((xbb->flags & XBBF_READ_ONLY) == 0)
			flags |= FWRITE;

		switch (xbb->device_type) {
		case XBB_TYPE_DISK:
			if (xbb->backend.dev.csw) {
				dev_relthread(xbb->backend.dev.cdev,
					      xbb->backend.dev.dev_ref);
				xbb->backend.dev.csw  = NULL;
				xbb->backend.dev.cdev = NULL;
			}
			break;
		case XBB_TYPE_FILE:
			vfs_is_locked = VFS_LOCK_GIANT(xbb->vn->v_mount);
			break;
		case XBB_TYPE_NONE:
		default:
			panic("Unexpected backend type.");
			break;
		}

		(void)vn_close(xbb->vn, flags, NOCRED, curthread);
		xbb->vn = NULL;

		switch (xbb->device_type) {
		case XBB_TYPE_DISK:
			break;
		case XBB_TYPE_FILE:
			VFS_UNLOCK_GIANT(vfs_is_locked);
			if (xbb->backend.file.cred != NULL) {
				crfree(xbb->backend.file.cred);
				xbb->backend.file.cred = NULL;
			}
			break;
		case XBB_TYPE_NONE:
		default:
			panic("Unexpected backend type.");
			break;
		}
	}
	PICKUP_GIANT();
}

/**
 * Open a character device to be used for backend I/O.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_open_dev(struct xbb_softc *xbb)
{
	struct vattr   vattr;
	struct cdev   *dev;
	struct cdevsw *devsw;
	int	       error;

	xbb->device_type = XBB_TYPE_DISK;
	xbb->dispatch_io = xbb_dispatch_dev;
	xbb->backend.dev.cdev = xbb->vn->v_rdev;
	xbb->backend.dev.csw = dev_refthread(xbb->backend.dev.cdev,
					     &xbb->backend.dev.dev_ref);
	if (xbb->backend.dev.csw == NULL)
		panic("Unable to retrieve device switch");

	error = VOP_GETATTR(xbb->vn, &vattr, NOCRED);
	if (error) {
		xenbus_dev_fatal(xbb->dev, error, "error getting "
				 "vnode attributes for device %s",
				 xbb->dev_name);
		return (error);
	}


	dev = xbb->vn->v_rdev;
	devsw = dev->si_devsw;
	if (!devsw->d_ioctl) {
		xenbus_dev_fatal(xbb->dev, ENODEV, "no d_ioctl for "
				 "device %s!", xbb->dev_name);
		return (ENODEV);
	}

	error = devsw->d_ioctl(dev, DIOCGSECTORSIZE,
			       (caddr_t)&xbb->sector_size, FREAD,
			       curthread);
	if (error) {
		xenbus_dev_fatal(xbb->dev, error,
				 "error calling ioctl DIOCGSECTORSIZE "
				 "for device %s", xbb->dev_name);
		return (error);
	}

	error = devsw->d_ioctl(dev, DIOCGMEDIASIZE,
			       (caddr_t)&xbb->media_size, FREAD,
			       curthread);
	if (error) {
		xenbus_dev_fatal(xbb->dev, error,
				 "error calling ioctl DIOCGMEDIASIZE "
				 "for device %s", xbb->dev_name);
		return (error);
	}

	return (0);
}

/**
 * Open a file to be used for backend I/O.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_open_file(struct xbb_softc *xbb)
{
	struct xbb_file_data *file_data;
	struct vattr          vattr;
	int                   error;

	file_data = &xbb->backend.file;
	xbb->device_type = XBB_TYPE_FILE;
	xbb->dispatch_io = xbb_dispatch_file;
	error = VOP_GETATTR(xbb->vn, &vattr, curthread->td_ucred);
	if (error != 0) {
		xenbus_dev_fatal(xbb->dev, error,
				 "error calling VOP_GETATTR()"
				 "for file %s", xbb->dev_name);
		return (error);
	}

	/*
	 * Verify that we have the ability to upgrade to exclusive
	 * access on this file so we can trap errors at open instead
	 * of reporting them during first access.
	 */
	if (VOP_ISLOCKED(xbb->vn) != LK_EXCLUSIVE) {
		vn_lock(xbb->vn, LK_UPGRADE | LK_RETRY);
		if (xbb->vn->v_iflag & VI_DOOMED) {
			error = EBADF;
			xenbus_dev_fatal(xbb->dev, error,
					 "error locking file %s",
					 xbb->dev_name);

			return (error);
		}
	}

	file_data->cred = crhold(curthread->td_ucred);
	xbb->media_size = vattr.va_size;

	/*
	 * XXX KDM vattr.va_blocksize may be larger than 512 bytes here.
	 * With ZFS, it is 131072 bytes.  Block sizes that large don't work
	 * with disklabel and UFS on FreeBSD at least.  Large block sizes
	 * may not work with other OSes as well.  So just export a sector
	 * size of 512 bytes, which should work with any OS or
	 * application.  Since our backing is a file, any block size will
	 * work fine for the backing store.
	 */
#if 0
	xbb->sector_size = vattr.va_blocksize;
#endif
	xbb->sector_size = 512;

	/*
	 * Sanity check.  The media size has to be at least one
	 * sector long.
	 */
	if (xbb->media_size < xbb->sector_size) {
		error = EINVAL;
		xenbus_dev_fatal(xbb->dev, error,
				 "file %s size %ju < block size %u",
				 xbb->dev_name,
				 (uintmax_t)xbb->media_size,
				 xbb->sector_size);
	}
	return (error);
}

/**
 * Open the backend provider for this connection.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_open_backend(struct xbb_softc *xbb)
{
	struct nameidata nd;
	int		 flags;
	int		 error;
	int		 vfs_is_locked;

	flags = FREAD;
	error = 0;

	DPRINTF("opening dev=%s\n", xbb->dev_name);

	if ((xbb->flags & XBBF_READ_ONLY) == 0)
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
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, xbb->dev_name, curthread);
	error = vn_open(&nd, &flags, 0, NULL);
	if (error) {
		/*
		 * This is the only reasonable guess we can make as far as
		 * path if the user doesn't give us a fully qualified path.
		 * If they want to specify a file, they need to specify the
		 * full path.
		 */
		if (xbb->dev_name[0] != '/') {
			char *dev_path = "/dev/";
			char *dev_name;

			/* Try adding device path at beginning of name */
			dev_name = malloc(strlen(xbb->dev_name)
					+ strlen(dev_path) + 1,
					  M_XENBLOCKBACK, M_NOWAIT);
			if (dev_name) {
				sprintf(dev_name, "%s%s", dev_path,
					xbb->dev_name);
				free(xbb->dev_name, M_XENBLOCKBACK);
				xbb->dev_name = dev_name;
				goto again;
			}
		}
		xenbus_dev_fatal(xbb->dev, error, "error opening device %s",
				 xbb->dev_name);
		return (error);
	}

	vfs_is_locked = NDHASGIANT(&nd);

	NDFREE(&nd, NDF_ONLY_PNBUF);
		
	xbb->vn = nd.ni_vp;

	/* We only support disks and files. */
	if (vn_isdisk(xbb->vn, &error)) {
		error = xbb_open_dev(xbb);
	} else if (xbb->vn->v_type == VREG) {
		error = xbb_open_file(xbb);
	} else {
		error = EINVAL;
		xenbus_dev_fatal(xbb->dev, error, "%s is not a disk "
				 "or file", xbb->dev_name);
	}
	VOP_UNLOCK(xbb->vn, 0);
	VFS_UNLOCK_GIANT(vfs_is_locked);

	if (error != 0) {
		xbb_close_backend(xbb);
		return (error);
	}

	xbb->sector_size_shift = fls(xbb->sector_size) - 1;
	xbb->media_num_sectors = xbb->media_size >> xbb->sector_size_shift;

	DPRINTF("opened %s=%s sector_size=%u media_size=%" PRId64 "\n",
		(xbb->device_type == XBB_TYPE_DISK) ? "dev" : "file",
		xbb->dev_name, xbb->sector_size, xbb->media_size);

	return (0);
}

/*------------------------ Inter-Domain Communication ------------------------*/
/**
 * Cleanup all inter-domain communication mechanisms.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static void
xbb_disconnect(struct xbb_softc *xbb)
{
	struct gnttab_unmap_grant_ref  ops[XBB_MAX_RING_PAGES];
	struct gnttab_unmap_grant_ref *op;
	u_int			       ring_idx;
	int			       error;

	DPRINTF("\n");

	if ((xbb->flags & XBBF_RING_CONNECTED) == 0)
		return;

	if (xbb->irq != 0) {
		unbind_from_irqhandler(xbb->irq);
		xbb->irq = 0;
	}

	for (ring_idx = 0, op = ops;
	     ring_idx < xbb->ring_config.ring_pages;
	     ring_idx++, op++) {

		op->host_addr    = xbb->ring_config.gnt_addr
			         + (ring_idx * PAGE_SIZE);
		op->dev_bus_addr = xbb->ring_config.bus_addr[ring_idx];
		op->handle	 = xbb->ring_config.handle[ring_idx];
	}

	error = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, ops,
					  xbb->ring_config.ring_pages);
	if (error != 0)
		panic("Grant table op failed (%d)", error);

	xbb->flags &= ~XBBF_RING_CONNECTED;
}

/**
 * Map shared memory ring into domain local address space, initialize
 * ring control structures, and bind an interrupt to the event channel
 * used to notify us of ring changes.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static int
xbb_connect_ring(struct xbb_softc *xbb)
{
	struct gnttab_map_grant_ref  gnts[XBB_MAX_RING_PAGES];
	struct gnttab_map_grant_ref *gnt;
	u_int			     ring_idx;
	int			     error;

	if ((xbb->flags & XBBF_RING_CONNECTED) != 0)
		return (0);

	/*
	 * Kva for our ring is at the tail of the region of kva allocated
	 * by xbb_alloc_communication_mem().
	 */
	xbb->ring_config.va = xbb->kva
			    + (xbb->kva_size
			     - (xbb->ring_config.ring_pages * PAGE_SIZE));
	xbb->ring_config.gnt_addr = xbb->gnt_base_addr
				  + (xbb->kva_size
				   - (xbb->ring_config.ring_pages * PAGE_SIZE));

	for (ring_idx = 0, gnt = gnts;
	     ring_idx < xbb->ring_config.ring_pages;
	     ring_idx++, gnt++) {

		gnt->host_addr = xbb->ring_config.gnt_addr
			       + (ring_idx * PAGE_SIZE);
		gnt->flags     = GNTMAP_host_map;
		gnt->ref       = xbb->ring_config.ring_ref[ring_idx];
		gnt->dom       = xbb->otherend_id;
	}

	error = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, gnts,
					  xbb->ring_config.ring_pages);
	if (error)
		panic("blkback: Ring page grant table op failed (%d)", error);

	for (ring_idx = 0, gnt = gnts;
	     ring_idx < xbb->ring_config.ring_pages;
	     ring_idx++, gnt++) {
		if (gnt->status != 0) {
			xbb->ring_config.va = 0;
			xenbus_dev_fatal(xbb->dev, EACCES,
					 "Ring shared page mapping failed. "
					 "Status %d.", gnt->status);
			return (EACCES);
		}
		xbb->ring_config.handle[ring_idx]   = gnt->handle;
		xbb->ring_config.bus_addr[ring_idx] = gnt->dev_bus_addr;
	}

	/* Initialize the ring based on ABI. */
	switch (xbb->abi) {
	case BLKIF_PROTOCOL_NATIVE:
	{
		blkif_sring_t *sring;
		sring = (blkif_sring_t *)xbb->ring_config.va;
		BACK_RING_INIT(&xbb->rings.native, sring,
			       xbb->ring_config.ring_pages * PAGE_SIZE);
		break;
	}
	case BLKIF_PROTOCOL_X86_32:
	{
		blkif_x86_32_sring_t *sring_x86_32;
		sring_x86_32 = (blkif_x86_32_sring_t *)xbb->ring_config.va;
		BACK_RING_INIT(&xbb->rings.x86_32, sring_x86_32,
			       xbb->ring_config.ring_pages * PAGE_SIZE);
		break;
	}
	case BLKIF_PROTOCOL_X86_64:
	{
		blkif_x86_64_sring_t *sring_x86_64;
		sring_x86_64 = (blkif_x86_64_sring_t *)xbb->ring_config.va;
		BACK_RING_INIT(&xbb->rings.x86_64, sring_x86_64,
			       xbb->ring_config.ring_pages * PAGE_SIZE);
		break;
	}
	default:
		panic("Unexpected blkif protocol ABI.");
	}

	xbb->flags |= XBBF_RING_CONNECTED;

	error =
	    bind_interdomain_evtchn_to_irqhandler(xbb->otherend_id,
						  xbb->ring_config.evtchn,
						  device_get_nameunit(xbb->dev),
						  xbb_intr, /*arg*/xbb,
						  INTR_TYPE_BIO | INTR_MPSAFE,
						  &xbb->irq);
	if (error) {
		xbb_disconnect(xbb);
		xenbus_dev_fatal(xbb->dev, error, "binding event channel");
		return (error);
	}

	DPRINTF("rings connected!\n");

	return 0;
}

/**
 * Size KVA and pseudo-physical address allocations based on negotiated
 * values for the size and number of I/O requests, and the size of our
 * communication ring.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * These address spaces are used to dynamically map pages in the
 * front-end's domain into our own.
 */
static int
xbb_alloc_communication_mem(struct xbb_softc *xbb)
{
	xbb->kva_size = (xbb->ring_config.ring_pages
		      +  (xbb->max_requests * xbb->max_request_segments))
		      * PAGE_SIZE;
#ifndef XENHVM
	xbb->kva = kmem_alloc_nofault(kernel_map, xbb->kva_size);
	if (xbb->kva == 0)
		return (ENOMEM);
	xbb->gnt_base_addr = xbb->kva;
#else /* XENHVM */
	/*
	 * Reserve a range of pseudo physical memory that we can map
	 * into kva.  These pages will only be backed by machine
	 * pages ("real memory") during the lifetime of front-end requests
	 * via grant table operations.
	 */
	xbb->pseudo_phys_res_id = 0;
	xbb->pseudo_phys_res = bus_alloc_resource(xbb->dev, SYS_RES_MEMORY,
						  &xbb->pseudo_phys_res_id,
						  0, ~0, xbb->kva_size,
						  RF_ACTIVE);
	if (xbb->pseudo_phys_res == NULL) {
		xbb->kva = 0;
		return (ENOMEM);
	}
	xbb->kva = (vm_offset_t)rman_get_virtual(xbb->pseudo_phys_res);
	xbb->gnt_base_addr = rman_get_start(xbb->pseudo_phys_res);
#endif /* XENHVM */
	return (0);
}

/**
 * Free dynamically allocated KVA or pseudo-physical address allocations.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static void
xbb_free_communication_mem(struct xbb_softc *xbb)
{
	if (xbb->kva != 0) {
#ifndef XENHVM
		kmem_free(kernel_map, xbb->kva, xbb->kva_size);
#else
		if (xbb->pseudo_phys_res != NULL) {
			bus_release_resource(xbb->dev, SYS_RES_MEMORY,
					     xbb->pseudo_phys_res_id,
					     xbb->pseudo_phys_res);
			xbb->pseudo_phys_res = NULL;
		}
#endif
	}
	xbb->kva = 0;
	xbb->gnt_base_addr = 0;
}

/**
 * Collect front-end information from the XenStore.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static int
xbb_collect_frontend_info(struct xbb_softc *xbb)
{
	char	    protocol_abi[64];
	const char *otherend_path;
	int	    error;
	u_int	    ring_idx;

	otherend_path = xenbus_get_otherend_path(xbb->dev);

	/*
	 * Mandatory data (used in all versions of the protocol) first.
	 */
	error = xs_gather(XST_NIL, otherend_path,
			  "ring-ref", "%" PRIu32,
			  &xbb->ring_config.ring_ref[0],
			  "event-channel", "%" PRIu32,
			  &xbb->ring_config.evtchn,
			  NULL);
	if (error != 0) {
		xenbus_dev_fatal(xbb->dev, error,
				 "Unable to retrieve ring information from "
				 "frontend %s.  Unable to connect.",
				 xenbus_get_otherend_path(xbb->dev));
		return (error);
	}

	/*
	 * These fields are initialized to legacy protocol defaults
	 * so we only need to fail if reading the updated value succeeds
	 * and the new value is outside of its allowed range.
	 *
	 * \note xs_gather() returns on the first encountered error, so
	 *       we must use independant calls in order to guarantee
	 *       we don't miss information in a sparsly populated front-end
	 *       tree.
	 */
	(void)xs_scanf(XST_NIL, otherend_path,
		       "ring-pages", NULL, "%" PRIu32,
		       &xbb->ring_config.ring_pages);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-requests", NULL, "%" PRIu32,
		       &xbb->max_requests);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-request-segments", NULL, "%" PRIu32,
		       &xbb->max_request_segments);

	(void)xs_scanf(XST_NIL, otherend_path,
		       "max-request-size", NULL, "%" PRIu32,
		       &xbb->max_request_size);

	if (xbb->ring_config.ring_pages	> XBB_MAX_RING_PAGES) {
		xenbus_dev_fatal(xbb->dev, EINVAL,
				 "Front-end specificed ring-pages of %u "
				 "exceeds backend limit of %zu.  "
				 "Unable to connect.",
				 xbb->ring_config.ring_pages,
				 XBB_MAX_RING_PAGES);
		return (EINVAL);
	} else if (xbb->max_requests > XBB_MAX_REQUESTS) {
		xenbus_dev_fatal(xbb->dev, EINVAL,
				 "Front-end specificed max_requests of %u "
				 "exceeds backend limit of %u.  "
				 "Unable to connect.",
				 xbb->max_requests,
				 XBB_MAX_REQUESTS);
		return (EINVAL);
	} else if (xbb->max_request_segments > XBB_MAX_SEGMENTS_PER_REQUEST) {
		xenbus_dev_fatal(xbb->dev, EINVAL,
				 "Front-end specificed max_requests_segments "
				 "of %u exceeds backend limit of %u.  "
				 "Unable to connect.",
				 xbb->max_request_segments,
				 XBB_MAX_SEGMENTS_PER_REQUEST);
		return (EINVAL);
	} else if (xbb->max_request_size > XBB_MAX_REQUEST_SIZE) {
		xenbus_dev_fatal(xbb->dev, EINVAL,
				 "Front-end specificed max_request_size "
				 "of %u exceeds backend limit of %u.  "
				 "Unable to connect.",
				 xbb->max_request_size,
				 XBB_MAX_REQUEST_SIZE);
		return (EINVAL);
	}

	/* If using a multi-page ring, pull in the remaining references. */
	for (ring_idx = 1; ring_idx < xbb->ring_config.ring_pages; ring_idx++) {
		char ring_ref_name[]= "ring_refXX";

		snprintf(ring_ref_name, sizeof(ring_ref_name),
			 "ring-ref%u", ring_idx);
		error = xs_scanf(XST_NIL, otherend_path,
				 ring_ref_name, NULL, "%" PRIu32,
			         &xbb->ring_config.ring_ref[ring_idx]);
		if (error != 0) {
			xenbus_dev_fatal(xbb->dev, error,
					 "Failed to retriev grant reference "
					 "for page %u of shared ring.  Unable "
					 "to connect.", ring_idx);
			return (error);
		}
	}

	error = xs_gather(XST_NIL, otherend_path,
			  "protocol", "%63s", protocol_abi,
			  NULL); 
	if (error != 0
	 || !strcmp(protocol_abi, XEN_IO_PROTO_ABI_NATIVE)) {
		/*
		 * Assume native if the frontend has not
		 * published ABI data or it has published and
		 * matches our own ABI.
		 */
		xbb->abi = BLKIF_PROTOCOL_NATIVE;
	} else if (!strcmp(protocol_abi, XEN_IO_PROTO_ABI_X86_32)) {

		xbb->abi = BLKIF_PROTOCOL_X86_32;
	} else if (!strcmp(protocol_abi, XEN_IO_PROTO_ABI_X86_64)) {

		xbb->abi = BLKIF_PROTOCOL_X86_64;
	} else {

		xenbus_dev_fatal(xbb->dev, EINVAL,
				 "Unknown protocol ABI (%s) published by "
				 "frontend.  Unable to connect.", protocol_abi);
		return (EINVAL);
	}
	return (0);
}

/**
 * Allocate per-request data structures given request size and number
 * information negotiated with the front-end.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static int
xbb_alloc_requests(struct xbb_softc *xbb)
{
	struct xbb_xen_req *req;
	struct xbb_xen_req *last_req;
	uint8_t		   *req_kva;
	u_long		    gnt_base;

	/*
	 * Allocate request book keeping datastructures.
	 */
	xbb->requests = malloc(xbb->max_requests * sizeof(*xbb->requests),
			       M_XENBLOCKBACK, M_NOWAIT|M_ZERO);
	if (xbb->requests == NULL) {
		xenbus_dev_fatal(xbb->dev, ENOMEM, 
				  "Unable to allocate request structures");
		return (ENOMEM);
	}

	req_kva  = (uint8_t *)xbb->kva;
	gnt_base = xbb->gnt_base_addr;
	req      = xbb->requests;
	last_req = &xbb->requests[xbb->max_requests - 1];
	while (req <= last_req) {
		int seg;

		req->xbb         = xbb;
		req->kva         = req_kva;
		req->gnt_handles = malloc(xbb->max_request_segments
					* sizeof(*req->gnt_handles),
					  M_XENBLOCKBACK, M_NOWAIT|M_ZERO);
		if (req->gnt_handles == NULL) {
			xenbus_dev_fatal(xbb->dev, ENOMEM,
					  "Unable to allocate request "
					  "grant references");
			return (ENOMEM);
		}
#ifdef XBB_USE_BOUNCE_BUFFERS
		req->bounce = malloc(xbb->max_request_size,
				     M_XENBLOCKBACK, M_NOWAIT);
		if (req->bounce == NULL) {
			xenbus_dev_fatal(xbb->dev, ENOMEM, 
					 "Unable to allocate request "
					 "bounce buffers");
			return (ENOMEM);
		}
#endif /* XBB_USE_BOUNCE_BUFFERS */
		req->gnt_base = gnt_base;
		req_kva      += xbb->max_request_segments * PAGE_SIZE;
		gnt_base     += xbb->max_request_segments * PAGE_SIZE;
		SLIST_INSERT_HEAD(&xbb->request_free_slist, req, links);

		for (seg = 0; seg < xbb->max_request_segments; seg++)
			req->gnt_handles[seg] = GRANT_REF_INVALID;

		req++;
	}
	return (0);
}

/**
 * Supply information about the physical device to the frontend
 * via XenBus.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static int
xbb_publish_backend_info(struct xbb_softc *xbb)
{
	struct xs_transaction xst;
	const char	     *our_path;
	const char	     *leaf;
	int		      error;

	our_path = xenbus_get_node(xbb->dev);
	while (1) {
		error = xs_transaction_start(&xst);
		if (error != 0) {
			xenbus_dev_fatal(xbb->dev, error,
					 "Error publishing backend info "
					 "(start transaction)");
			return (error);
		}

		leaf = "sectors";
		error = xs_printf(xst, our_path, leaf,
				  "%"PRIu64, xbb->media_num_sectors);
		if (error != 0)
			break;

		/* XXX Support all VBD attributes here. */
		leaf = "info";
		error = xs_printf(xst, our_path, leaf, "%u",
				  xbb->flags & XBBF_READ_ONLY
				? VDISK_READONLY : 0);
		if (error != 0)
			break;

		leaf = "sector-size";
		error = xs_printf(xst, our_path, leaf, "%u",
				  xbb->sector_size);
		if (error != 0)
			break;

		error = xs_transaction_end(xst, 0);
		if (error == 0) {
			return (0);
		} else if (error != EAGAIN) {
			xenbus_dev_fatal(xbb->dev, error, "ending transaction");
			return (error);
		}
	}

	xenbus_dev_fatal(xbb->dev, error, "writing %s/%s",
			our_path, leaf);
	xs_transaction_end(xst, 1);
	return (error);
}

/**
 * Connect to our blkfront peer now that it has completed publishing
 * its configuration into the XenStore.
 *
 * \param xbb  Per-instance xbb configuration structure.
 */
static void
xbb_connect(struct xbb_softc *xbb)
{
	int		      error;

	if (xenbus_get_state(xbb->dev) == XenbusStateConnected)
		return;

	if (xbb_collect_frontend_info(xbb) != 0)
		return;

	/* Allocate resources whose size depends on front-end configuration. */
	error = xbb_alloc_communication_mem(xbb);
	if (error != 0) {
		xenbus_dev_fatal(xbb->dev, error,
				 "Unable to allocate communication memory");
		return;
	}

	error = xbb_alloc_requests(xbb);
	if (error != 0) {
		/* Specific errors are reported by xbb_alloc_requests(). */
		return;
	}

	/*
	 * Connect communication channel.
	 */
	error = xbb_connect_ring(xbb);
	if (error != 0) {
		/* Specific errors are reported by xbb_connect_ring(). */
		return;
	}
	
	if (xbb_publish_backend_info(xbb) != 0) {
		/*
		 * If we can't publish our data, we cannot participate
		 * in this connection, and waiting for a front-end state
		 * change will not help the situation.
		 */
		xbb_disconnect(xbb);
		return;
	}

	/* Ready for I/O. */
	xenbus_set_state(xbb->dev, XenbusStateConnected);
}

/*-------------------------- Device Teardown Support -------------------------*/
/**
 * Perform device shutdown functions.
 *
 * \param xbb  Per-instance xbb configuration structure.
 *
 * Mark this instance as shutting down, wait for any active I/O on the
 * backend device/file to drain, disconnect from the front-end, and notify
 * any waiters (e.g. a thread invoking our detach method) that detach can
 * now proceed.
 */
static int
xbb_shutdown(struct xbb_softc *xbb)
{
	static int in_shutdown;

	DPRINTF("\n");

	/*
	 * Due to the need to drop our mutex during some
	 * xenbus operations, it is possible for two threads
	 * to attempt to close out shutdown processing at
	 * the same time.  Tell the caller that hits this
	 * race to try back later. 
	 */
	if (in_shutdown != 0)
		return (EAGAIN);

	DPRINTF("\n");

	/* Indicate shutdown is in progress. */
	xbb->flags |= XBBF_SHUTDOWN;

	/* Wait for requests to complete. */
	if (xbb->active_request_count != 0)
		return (EAGAIN);
	
	DPRINTF("\n");

	/* Disconnect from the front-end. */
	xbb_disconnect(xbb);

	in_shutdown = 1;
	mtx_unlock(&xbb->lock);
	xenbus_set_state(xbb->dev, XenbusStateClosed);
	mtx_lock(&xbb->lock);
	in_shutdown = 0;

	/* Indicate to xbb_detach() that is it safe to proceed. */
	wakeup(xbb);

	return (0);
}

/**
 * Report an attach time error to the console and Xen, and cleanup
 * this instance by forcing immediate detach processing.
 *
 * \param xbb  Per-instance xbb configuration structure.
 * \param err  Errno describing the error.
 * \param fmt  Printf style format and arguments
 */
static void
xbb_attach_failed(struct xbb_softc *xbb, int err, const char *fmt, ...)
{
	va_list ap;
	va_list ap_hotplug;

	va_start(ap, fmt);
	va_copy(ap_hotplug, ap);
	xs_vprintf(XST_NIL, xenbus_get_node(xbb->dev),
		  "hotplug-error", fmt, ap_hotplug);
	va_end(ap_hotplug);
	xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
		  "hotplug-status", "error");

	xenbus_dev_vfatal(xbb->dev, err, fmt, ap);
	va_end(ap);

	xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
		  "online", "0");
	xbb_detach(xbb->dev);
}

/*---------------------------- NewBus Entrypoints ----------------------------*/
/**
 * Inspect a XenBus device and claim it if is of the appropriate type.
 * 
 * \param dev  NewBus device object representing a candidate XenBus device.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_probe(device_t dev)
{
 
        if (!strcmp(xenbus_get_type(dev), "vbd")) {
                device_set_desc(dev, "Backend Virtual Block Device");
                device_quiet(dev);
                return (0);
        }

        return (ENXIO);
}

/**
 * Attach to a XenBus device that has been claimed by our probe routine.
 *
 * \param dev  NewBus device object representing this Xen Block Back instance.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_attach(device_t dev)
{
	struct xbb_softc   *xbb;
	int		    error;

	DPRINTF("Attaching to %s\n", xenbus_get_node(dev));

	/*
	 * Basic initialization.
	 * After this block it is safe to call xbb_detach()
	 * to clean up any allocated data for this instance.
	 */
	xbb = device_get_softc(dev);
	xbb->dev = dev;
	xbb->otherend_id = xenbus_get_otherend_id(dev);
	TASK_INIT(&xbb->io_task, /*priority*/0, xbb_run_queue, xbb);
	mtx_init(&xbb->lock, device_get_nameunit(dev), NULL, MTX_DEF);
	SLIST_INIT(&xbb->request_free_slist);

	/*
	 * Protocol defaults valid even if all negotiation fails.
	 */
	xbb->ring_config.ring_pages = 1;
	xbb->max_requests	    = BLKIF_MAX_RING_REQUESTS(PAGE_SIZE);
	xbb->max_request_segments   = BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK;
	xbb->max_request_size	    = xbb->max_request_segments * PAGE_SIZE;

	/*
	 * Publish protocol capabilities for consumption by the
	 * front-end.
	 */
	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "feature-barrier", "1");
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/feature-barrier",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "feature-flush-cache", "1");
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/feature-flush-cache",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "max-ring-pages", "%zu", XBB_MAX_RING_PAGES);
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/max-ring-pages",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "max-requests", "%u", XBB_MAX_REQUESTS);
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/max-requests",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "max-request-segments", "%u",
			  XBB_MAX_SEGMENTS_PER_REQUEST);
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/max-request-segments",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "max-request-size", "%u",
			  XBB_MAX_REQUEST_SIZE);
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/max-request-size",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	/* Collect physical device information. */
	error = xs_gather(XST_NIL, xenbus_get_otherend_path(xbb->dev),
			  "device-type", NULL, &xbb->dev_type,
			  NULL);
	if (error != 0)
		xbb->dev_type = NULL;

	error = xs_gather(XST_NIL, xenbus_get_node(dev),
                          "mode", NULL, &xbb->dev_mode,
			  "params", NULL, &xbb->dev_name,
                          NULL);
	if (error != 0) {
		xbb_attach_failed(xbb, error, "reading backend fields at %s",
				  xenbus_get_node(dev));
                return (ENXIO);
        }

	/* Parse fopen style mode flags. */
	if (strchr(xbb->dev_mode, 'w') == NULL)
		xbb->flags |= XBBF_READ_ONLY;

	/*
	 * Verify the physical device is present and can support
	 * the desired I/O mode.
	 */
	DROP_GIANT();
	error = xbb_open_backend(xbb);
	PICKUP_GIANT();
	if (error != 0) {
		xbb_attach_failed(xbb, error, "Unable to open %s",
				  xbb->dev_name);
		return (ENXIO);
	}

	/* Use devstat(9) for recording statistics. */
	xbb->xbb_stats = devstat_new_entry("xbb", device_get_unit(xbb->dev),
					   xbb->sector_size,
					   DEVSTAT_ALL_SUPPORTED,
					   DEVSTAT_TYPE_DIRECT
					 | DEVSTAT_TYPE_IF_OTHER,
					   DEVSTAT_PRIORITY_OTHER);
	/*
	 * Create a taskqueue for doing work that must occur from a
	 * thread context.
	 */
	xbb->io_taskqueue = taskqueue_create(device_get_nameunit(dev), M_NOWAIT,
					     taskqueue_thread_enqueue,
					     /*context*/&xbb->io_taskqueue);
	if (xbb->io_taskqueue == NULL) {
		xbb_attach_failed(xbb, error, "Unable to create taskqueue");
		return (ENOMEM);
	}

	taskqueue_start_threads(&xbb->io_taskqueue,
				/*num threads*/1,
				/*priority*/PWAIT,
				/*thread name*/
				"%s taskq", device_get_nameunit(dev));

	/* Update hot-plug status to satisfy xend. */
	error = xs_printf(XST_NIL, xenbus_get_node(xbb->dev),
			  "hotplug-status", "connected");
	if (error) {
		xbb_attach_failed(xbb, error, "writing %s/hotplug-status",
				  xenbus_get_node(xbb->dev));
		return (error);
	}

	/* Tell the front end that we are ready to connect. */
	xenbus_set_state(dev, XenbusStateInitWait);

	return (0);
}

/**
 * Detach from a block back device instanced.
 *
 * \param dev  NewBus device object representing this Xen Block Back instance.
 *
 * \return  0 for success, errno codes for failure.
 * 
 * \note A block back device may be detached at any time in its life-cycle,
 *       including part way through the attach process.  For this reason,
 *       initialization order and the intialization state checks in this
 *       routine must be carefully coupled so that attach time failures
 *       are gracefully handled.
 */
static int
xbb_detach(device_t dev)
{
        struct xbb_softc *xbb;

	DPRINTF("\n");

        xbb = device_get_softc(dev);
	mtx_lock(&xbb->lock);
	while (xbb_shutdown(xbb) == EAGAIN) {
		msleep(xbb, &xbb->lock, /*wakeup prio unchanged*/0,
		       "xbb_shutdown", 0);
	}
	mtx_unlock(&xbb->lock);
	mtx_destroy(&xbb->lock);

	DPRINTF("\n");

	if (xbb->io_taskqueue != NULL)
		taskqueue_free(xbb->io_taskqueue);

	if (xbb->xbb_stats != NULL)
		devstat_remove_entry(xbb->xbb_stats);

	xbb_close_backend(xbb);
	xbb_free_communication_mem(xbb);

	if (xbb->dev_mode != NULL) {
		free(xbb->dev_mode, M_XENBUS);
		xbb->dev_mode = NULL;
	}

	if (xbb->dev_type != NULL) {
		free(xbb->dev_type, M_XENBUS);
		xbb->dev_type = NULL;
	}

	if (xbb->dev_name != NULL) {
		free(xbb->dev_name, M_XENBUS);
		xbb->dev_name = NULL;
	}

	if (xbb->requests != NULL) {
		struct xbb_xen_req *req;
		struct xbb_xen_req *last_req;

		req      = xbb->requests;
		last_req = &xbb->requests[xbb->max_requests - 1];
		while (req <= last_req) {
#ifdef XBB_USE_BOUNCE_BUFFERS
			if (req->bounce != NULL) {
				free(req->bounce, M_XENBLOCKBACK);
				req->bounce = NULL;
			}
#endif
			if (req->gnt_handles != NULL) {
				free (req->gnt_handles, M_XENBLOCKBACK);
				req->gnt_handles = NULL;
			}
			req++;
		}
		free(xbb->requests, M_XENBLOCKBACK);
		xbb->requests = NULL;
	}

        return (0);
}

/**
 * Prepare this block back device for suspension of this VM.
 * 
 * \param dev  NewBus device object representing this Xen Block Back instance.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_suspend(device_t dev)
{
#ifdef NOT_YET
        struct xbb_softc *sc = device_get_softc(dev);

        /* Prevent new requests being issued until we fix things up. */
        mtx_lock(&sc->xb_io_lock);
        sc->connected = BLKIF_STATE_SUSPENDED;
        mtx_unlock(&sc->xb_io_lock);
#endif

        return (0);
}

/**
 * Perform any processing required to recover from a suspended state.
 * 
 * \param dev  NewBus device object representing this Xen Block Back instance.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_resume(device_t dev)
{
	return (0);
}

/**
 * Handle state changes expressed via the XenStore by our front-end peer.
 *
 * \param dev             NewBus device object representing this Xen
 *                        Block Back instance.
 * \param frontend_state  The new state of the front-end.
 *
 * \return  0 for success, errno codes for failure.
 */
static int
xbb_frontend_changed(device_t dev, XenbusState frontend_state)
{
	struct xbb_softc *xbb = device_get_softc(dev);

	DPRINTF("state=%s\n", xenbus_strstate(frontend_state));

	switch (frontend_state) {
	case XenbusStateInitialising:
	case XenbusStateClosing:
		break;
	case XenbusStateInitialised:
	case XenbusStateConnected:
		xbb_connect(xbb);
		break;
	case XenbusStateClosed:
	case XenbusStateInitWait:

		mtx_lock(&xbb->lock);
		xbb_shutdown(xbb);
		mtx_unlock(&xbb->lock);
		break;
	default:
		xenbus_dev_fatal(xbb->dev, EINVAL, "saw state %d at frontend",
				 frontend_state);
		break;
	}
	return (0);
}

/*---------------------------- NewBus Registration ---------------------------*/
static device_method_t xbb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xbb_probe),
	DEVMETHOD(device_attach,	xbb_attach),
	DEVMETHOD(device_detach,	xbb_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	xbb_suspend),
	DEVMETHOD(device_resume,	xbb_resume),

	/* Xenbus interface */
	DEVMETHOD(xenbus_otherend_changed, xbb_frontend_changed),

	{ 0, 0 }
};

static driver_t xbb_driver = {
        "xbbd",
        xbb_methods,
        sizeof(struct xbb_softc),
};
devclass_t xbb_devclass;

DRIVER_MODULE(xbbd, xenbusb_back, xbb_driver, xbb_devclass, 0, 0);
