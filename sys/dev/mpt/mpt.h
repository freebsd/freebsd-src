/* $FreeBSD$ */
/*-
 * Generic defines for LSI '909 FC  adapters.
 * FreeBSD Version.
 *
 * Copyright (c)  2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */
/*
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MPT_H_
#define _MPT_H_

/********************************* OS Includes ********************************/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#if __FreeBSD_version < 500000  
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#else
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#endif
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include "opt_ddb.h"

/**************************** Register Definitions ****************************/
#include <dev/mpt/mpt_reg.h>

/******************************* MPI Definitions ******************************/
#include <dev/mpt/mpilib/mpi_type.h>
#include <dev/mpt/mpilib/mpi.h>
#include <dev/mpt/mpilib/mpi_cnfg.h>
#include <dev/mpt/mpilib/mpi_ioc.h>
#include <dev/mpt/mpilib/mpi_raid.h>

/* XXX For mpt_debug.c */
#include <dev/mpt/mpilib/mpi_init.h>

/****************************** Misc Definitions ******************************/
#define MPT_OK (0)
#define MPT_FAIL (0x10000)

#define NUM_ELEMENTS(array) (sizeof(array) / sizeof(*array))

/**************************** Forward Declarations ****************************/
struct mpt_softc;
struct mpt_personality;
typedef struct req_entry request_t;

/************************* Personality Module Support *************************/
typedef int mpt_load_handler_t(struct mpt_personality *);
typedef int mpt_probe_handler_t(struct mpt_softc *);
typedef int mpt_attach_handler_t(struct mpt_softc *);
typedef int mpt_event_handler_t(struct mpt_softc *, request_t *,
				MSG_EVENT_NOTIFY_REPLY *);
typedef void mpt_reset_handler_t(struct mpt_softc *, int /*type*/);
/* XXX Add return value and use for veto? */
typedef void mpt_shutdown_handler_t(struct mpt_softc *);
typedef void mpt_detach_handler_t(struct mpt_softc *);
typedef int mpt_unload_handler_t(struct mpt_personality *);

struct mpt_personality
{
	const char		*name;
	uint32_t		 id;		/* Assigned identifier. */
	u_int			 use_count;	/* Instances using personality*/
	mpt_load_handler_t	*load;		/* configure personailty */
#define MPT_PERS_FIRST_HANDLER(pers) (&(pers)->load)
	mpt_probe_handler_t	*probe;		/* configure personailty */
	mpt_attach_handler_t	*attach;	/* initialize device instance */
	mpt_event_handler_t	*event;		/* Handle MPI event. */
	mpt_reset_handler_t	*reset;		/* Re-init after reset. */
	mpt_shutdown_handler_t	*shutdown;	/* Shutdown instance. */
	mpt_detach_handler_t	*detach;	/* release device instance */
	mpt_unload_handler_t	*unload;	/* Shutdown personality */
#define MPT_PERS_LAST_HANDLER(pers) (&(pers)->unload)
};

int mpt_modevent(module_t, int, void *);

/* Maximum supported number of personalities. */
#define MPT_MAX_PERSONALITIES	(15)

#define MPT_PERSONALITY_DEPEND(name, dep, vmin, vpref, vmax) \
	MODULE_DEPEND(name, dep, vmin, vpref, vmax)

#define DECLARE_MPT_PERSONALITY(name, order)				  \
	static moduledata_t name##_mod = {				  \
		#name, mpt_modevent, &name##_personality		  \
	};								  \
	DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, order);	  \
	MODULE_VERSION(name, 1);					  \
	MPT_PERSONALITY_DEPEND(name, mpt_core, 1, 1, 1)

/******************************* Bus DMA Support ******************************/
/* XXX Need to update bus_dmamap_sync to take a range argument. */
#define bus_dmamap_sync_range(dma_tag, dmamap, offset, len, op)	\
	bus_dmamap_sync(dma_tag, dmamap, op)

#if __FreeBSD_version >= 501102
#define mpt_dma_tag_create(mpt, parent_tag, alignment, boundary,	\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)					\
	bus_dma_tag_create(parent_tag, alignment, boundary,		\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   busdma_lock_mutex, &Giant,			\
			   dma_tagp)
#else
#define mpt_dma_tag_create(mpt, parent_tag, alignment, boundary,	\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)					\
	bus_dma_tag_create(parent_tag, alignment, boundary,		\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)
#endif

struct mpt_map_info {
	struct mpt_softc *mpt;
	int		  error;
	uint32_t	  phys;
};

void mpt_map_rquest(void *, bus_dma_segment_t *, int, int);

/**************************** Kernel Thread Support ***************************/
#if __FreeBSD_version > 500005
#define mpt_kthread_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
	kthread_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#else
#define mpt_kthread_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
	kthread_create(func, farg, proc_ptr, fmtstr, arg)
#endif

/****************************** Timer Facilities ******************************/
#if __FreeBSD_version > 500000
#define mpt_callout_init(c)	callout_init(c, /*mpsafe*/0);
#else
#define mpt_callout_init(c)	callout_init(c);
#endif

/********************************** Endianess *********************************/
static __inline uint64_t
u64toh(U64 s)
{
	uint64_t result;

	result = le32toh(s.Low);
	result |= ((uint64_t)le32toh(s.High)) << 32;
	return (result);
}

/**************************** MPI Transaction State ***************************/
typedef enum {
	REQ_STATE_FREE		= 0x00,
	REQ_STATE_ALLOCATED	= 0x01,
	REQ_STATE_QUEUED	= 0x02,
	REQ_STATE_DONE		= 0x04,
	REQ_STATE_TIMEDOUT	= 0x08,
	REQ_STATE_NEED_WAKEUP	= 0x10,
	REQ_STATE_MASK		= 0xFF
} mpt_req_state_t; 

struct req_entry {
	TAILQ_ENTRY(req_entry) links;	/* Pointer to next in list */
	mpt_req_state_t	state;		/* Request State Information */
	uint16_t	index;		/* Index of this entry */
	uint16_t	IOCStatus;	/* Completion status */
	union ccb      *ccb;		/* CAM request */
	void	       *req_vbuf;	/* Virtual Address of Entry */
	void	       *sense_vbuf;	/* Virtual Address of sense data */
	bus_addr_t	req_pbuf;	/* Physical Address of Entry */
	bus_addr_t	sense_pbuf;	/* Physical Address of sense data */
	bus_dmamap_t	dmap;		/* DMA map for data buffer */
};

/**************************** Handler Registration ****************************/
/*
 * Global table of registered reply handlers.  The
 * handler is indicated by byte 3 of the request
 * index submitted to the IOC.  This allows the
 * driver core to perform generic processing without
 * any knowledge of per-personality behavior.
 *
 * MPT_NUM_REPLY_HANDLERS must be a power of 2
 * to allow the easy generation of a mask.
 *
 * The handler offsets used by the core are hard coded
 * allowing faster code generation when assigning a handler
 * to a request.  All "personalities" must use the
 * the handler registration mechanism.
 *
 * The IOC handlers that are rarely executed are placed
 * at the tail of the table to make it more likely that
 * all commonly executed handlers fit in a single cache
 * line.
 */
#define MPT_NUM_REPLY_HANDLERS		(16)
#define MPT_REPLY_HANDLER_EVENTS	MPT_CBI_TO_HID(0)
#define MPT_REPLY_HANDLER_CONFIG	MPT_CBI_TO_HID(MPT_NUM_REPLY_HANDLERS-1)
#define MPT_REPLY_HANDLER_HANDSHAKE	MPT_CBI_TO_HID(MPT_NUM_REPLY_HANDLERS-2)
typedef int mpt_reply_handler_t(struct mpt_softc *mpt, request_t *request,
				 MSG_DEFAULT_REPLY *reply_frame);
typedef union {
	mpt_reply_handler_t	*reply_handler;
} mpt_handler_t;

typedef enum {
	MPT_HANDLER_REPLY,
	MPT_HANDLER_EVENT,
	MPT_HANDLER_RESET,
	MPT_HANDLER_SHUTDOWN
} mpt_handler_type;

struct mpt_handler_record
{
	LIST_ENTRY(mpt_handler_record)	links;
	mpt_handler_t			handler;
};

LIST_HEAD(mpt_handler_list, mpt_handler_record);

/*
 * The handler_id is currently unused but would contain the
 * handler ID used in the MsgContext field to allow direction
 * of replies to the handler.  Registrations that don't require
 * a handler id can pass in NULL for the handler_id.
 *
 * Deregistrations for handlers without a handler id should
 * pass in MPT_HANDLER_ID_NONE.
 */
#define MPT_HANDLER_ID_NONE		(0xFFFFFFFF)
int mpt_register_handler(struct mpt_softc *, mpt_handler_type,
			 mpt_handler_t, uint32_t *);
int mpt_deregister_handler(struct mpt_softc *, mpt_handler_type,
			   mpt_handler_t, uint32_t);

/******************* Per-Controller Instance Data Structures ******************/
TAILQ_HEAD(req_queue, req_entry);

/* Structure for saving proper values for modifyable PCI config registers */
struct mpt_pci_cfg {
	uint16_t Command;
	uint16_t LatencyTimer_LineSize;
	uint32_t IO_BAR;
	uint32_t Mem0_BAR[2];
	uint32_t Mem1_BAR[2];
	uint32_t ROM_BAR;
	uint8_t  IntLine;
	uint32_t PMCSR;
};

typedef enum {
	MPT_RVF_NONE		= 0x0,
	MPT_RVF_ACTIVE		= 0x1,
	MPT_RVF_ANNOUNCED	= 0x2,
	MPT_RVF_UP2DATE		= 0x4,
	MPT_RVF_REFERENCED	= 0x8,
	MPT_RVF_WCE_CHANGED	= 0x10
} mpt_raid_volume_flags;

struct mpt_raid_volume {
	CONFIG_PAGE_RAID_VOL_0	       *config_page;
	MPI_RAID_VOL_INDICATOR		sync_progress;
	mpt_raid_volume_flags		flags;
	u_int				quieced_disks;
};

typedef enum {
	MPT_RDF_NONE		= 0x00,
	MPT_RDF_ACTIVE		= 0x01,
	MPT_RDF_ANNOUNCED	= 0x02,
	MPT_RDF_UP2DATE		= 0x04,
	MPT_RDF_REFERENCED	= 0x08,
	MPT_RDF_QUIESCING	= 0x10,
	MPT_RDF_QUIESCED	= 0x20
} mpt_raid_disk_flags;

struct mpt_raid_disk {
	CONFIG_PAGE_RAID_PHYS_DISK_0	config_page;
	struct mpt_raid_volume	       *volume;
	u_int				member_number;
	u_int				pass_thru_active;
	mpt_raid_disk_flags		flags;
};

struct mpt_evtf_record {
	MSG_EVENT_NOTIFY_REPLY		reply;
	uint32_t			context;
	LIST_ENTRY(mpt_evtf_record)	links;
};

LIST_HEAD(mpt_evtf_list, mpt_evtf_record);

struct mpt_softc {
	device_t		dev;
#if __FreeBSD_version < 500000  
	int			mpt_splsaved;
	uint32_t		mpt_islocked;	
#else
	struct mtx		mpt_lock;
#endif
	uint32_t		mpt_pers_mask;
	uint32_t		: 15,
		raid_mwce_set	: 1,
		getreqwaiter	: 1,
		shutdwn_raid    : 1,
		shutdwn_recovery: 1,
		unit		: 8,
		outofbeer	: 1,
		mpt_locksetup	: 1,
		disabled	: 1,
		is_fc		: 1,
		bus		: 1;	/* FC929/1030 have two busses */

	u_int			verbose;

	/*
	 * IOC Facts
	 */
	uint16_t	mpt_global_credits;
	uint16_t	request_frame_size;
	uint8_t		mpt_max_devices;
	uint8_t		mpt_max_buses;

	/*
	 * Port Facts
	 * XXX - Add multi-port support!.
	 */
	uint16_t	mpt_ini_id;
	uint16_t	mpt_port_type;
	uint16_t	mpt_proto_flags;

	/*
	 * Device Configuration Information
	 */
	union {
		struct mpt_spi_cfg {
			CONFIG_PAGE_SCSI_PORT_0		_port_page0;
			CONFIG_PAGE_SCSI_PORT_1		_port_page1;
			CONFIG_PAGE_SCSI_PORT_2		_port_page2;
			CONFIG_PAGE_SCSI_DEVICE_0	_dev_page0[16];
			CONFIG_PAGE_SCSI_DEVICE_1	_dev_page1[16];
			uint16_t			_tag_enable;
			uint16_t			_disc_enable;
			uint16_t			_update_params0;
			uint16_t			_update_params1;
		} spi;
#define	mpt_port_page0		cfg.spi._port_page0
#define	mpt_port_page1		cfg.spi._port_page1
#define	mpt_port_page2		cfg.spi._port_page2
#define	mpt_dev_page0		cfg.spi._dev_page0
#define	mpt_dev_page1		cfg.spi._dev_page1
#define	mpt_tag_enable		cfg.spi._tag_enable
#define	mpt_disc_enable		cfg.spi._disc_enable
#define	mpt_update_params0	cfg.spi._update_params0
#define	mpt_update_params1	cfg.spi._update_params1
		struct mpi_fc_cfg {
			uint8_t	nada;
		} fc;
	} cfg;

	/* Controller Info */
	CONFIG_PAGE_IOC_2 *	ioc_page2;
	CONFIG_PAGE_IOC_3 *	ioc_page3;

	/* Raid Data */
	struct mpt_raid_volume* raid_volumes;
	struct mpt_raid_disk*	raid_disks;
	u_int			raid_max_volumes;
	u_int			raid_max_disks;
	u_int			raid_page0_len;
	u_int			raid_wakeup;
	u_int			raid_rescan;
	u_int			raid_resync_rate;
	u_int			raid_mwce_setting;
	u_int			raid_queue_depth;
	u_int			raid_nonopt_volumes;
	struct proc	       *raid_thread;
	struct callout		raid_timer;

	/*
	 * PCI Hardware info
	 */
	struct resource *	pci_irq;	/* Interrupt map for chip */
	void *			ih;		/* Interupt handle */
	struct mpt_pci_cfg	pci_cfg;	/* saved PCI conf registers */

	/*
	 * DMA Mapping Stuff
	 */
	struct resource *	pci_reg;	/* Register map for chip */
	int			pci_mem_rid;	/* Resource ID */
	bus_space_tag_t		pci_st;		/* Bus tag for registers */
	bus_space_handle_t	pci_sh;		/* Bus handle for registers */
	/* PIO versions of above. */
	int			pci_pio_rid;
	struct resource *	pci_pio_reg;
	bus_space_tag_t		pci_pio_st;
	bus_space_handle_t	pci_pio_sh;

	bus_dma_tag_t		parent_dmat;	/* DMA tag for parent PCI bus */
	bus_dma_tag_t		reply_dmat;	/* DMA tag for reply memory */
	bus_dmamap_t		reply_dmap;	/* DMA map for reply memory */
	uint8_t		       *reply;		/* KVA of reply memory */
	bus_addr_t		reply_phys;	/* BusAddr of reply memory */

	bus_dma_tag_t		buffer_dmat;	/* DMA tag for buffers */
	bus_dma_tag_t		request_dmat;	/* DMA tag for request memroy */
	bus_dmamap_t		request_dmap;	/* DMA map for request memroy */
	uint8_t		       *request;	/* KVA of Request memory */
	bus_addr_t		request_phys;	/* BusADdr of request memory */

	u_int			reset_cnt;

	/*
	 * CAM && Software Management
	 */
	request_t	       *request_pool;
	struct req_queue	request_free_list;
	struct req_queue	request_pending_list;
	struct req_queue	request_timeout_list;

	/*
	 * Deferred frame acks due to resource shortage.
	 */
	struct mpt_evtf_list	ack_frames;


	struct cam_sim	       *sim;
	struct cam_path	       *path;

	struct cam_sim	       *phydisk_sim;
	struct cam_path	       *phydisk_path;

	struct proc	       *recovery_thread;
	request_t	       *tmf_req;

	uint32_t		sequence;	/* Sequence Number */
	uint32_t		timeouts;	/* timeout count */
	uint32_t		success;	/* successes afer timeout */

	/* Opposing port in a 929 or 1030, or NULL */
	struct mpt_softc *	mpt2;

	/* FW Image management */
	uint32_t		fw_image_size;
	uint8_t		       *fw_image;
	bus_dma_tag_t		fw_dmat;	/* DMA tag for firmware image */
	bus_dmamap_t		fw_dmap;	/* DMA map for firmware image */
	bus_addr_t		fw_phys;	/* BusAddr of request memory */

	/* Shutdown Event Handler. */
	eventhandler_tag         eh;

	TAILQ_ENTRY(mpt_softc)	links;
};

/***************************** Locking Primatives *****************************/
#if __FreeBSD_version < 500000  
#define	MPT_IFLAGS		INTR_TYPE_CAM
#define	MPT_LOCK(mpt)		mpt_lockspl(mpt)
#define	MPT_UNLOCK(mpt)		mpt_unlockspl(mpt)
#define	MPTLOCK_2_CAMLOCK	MPT_UNLOCK
#define	CAMLOCK_2_MPTLOCK	MPT_LOCK
#define	MPT_LOCK_SETUP(mpt)
#define	MPT_LOCK_DESTROY(mpt)

static __inline void mpt_lockspl(struct mpt_softc *mpt);
static __inline void mpt_unlockspl(struct mpt_softc *mpt);

static __inline void
mpt_lockspl(struct mpt_softc *mpt)
{
       int s;

       s = splcam();
       if (mpt->mpt_islocked++ == 0) {  
               mpt->mpt_splsaved = s;
       } else {
               splx(s);
	       panic("Recursed lock with mask: 0x%x\n", s);
       }
}

static __inline void
mpt_unlockspl(struct mpt_softc *mpt)
{
       if (mpt->mpt_islocked) {
               if (--mpt->mpt_islocked == 0) {
                       splx(mpt->mpt_splsaved);
               }
       } else
	       panic("Negative lock count\n");
}

static __inline int
mpt_sleep(struct mpt_softc *mpt, void *ident, int priority,
	   const char *wmesg, int timo)
{
	int saved_cnt;
	int saved_spl;
	int error;

	KASSERT(mpt->mpt_islocked <= 1, ("Invalid lock count on tsleep"));
	saved_cnt = mpt->mpt_islocked;
	saved_spl = mpt->mpt_splsaved;
	mpt->mpt_islocked = 0;
	error = tsleep(ident, priority, wmesg, timo);
	KASSERT(mpt->mpt_islocked = 0, ("Invalid lock count on wakeup"));
	mpt->mpt_islocked = saved_cnt;
	mpt->mpt_splsaved = saved_spl;
	return (error);
}

#else
#if	LOCKING_WORKED_AS_IT_SHOULD
#error "Shouldn't Be Here!"
#define	MPT_IFLAGS		INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE
#define	MPT_LOCK_SETUP(mpt)						\
		mtx_init(&mpt->mpt_lock, "mpt", NULL, MTX_DEF);		\
		mpt->mpt_locksetup = 1
#define	MPT_LOCK_DESTROY(mpt)						\
	if (mpt->mpt_locksetup) {					\
		mtx_destroy(&mpt->mpt_lock);				\
		mpt->mpt_locksetup = 0;					\
	}

#define	MPT_LOCK(mpt)		mtx_lock(&(mpt)->mpt_lock)
#define	MPT_UNLOCK(mpt)		mtx_unlock(&(mpt)->mpt_lock)
#define	MPTLOCK_2_CAMLOCK(mpt)	\
	mtx_unlock(&(mpt)->mpt_lock); mtx_lock(&Giant)
#define	CAMLOCK_2_MPTLOCK(mpt)	\
	mtx_unlock(&Giant); mtx_lock(&(mpt)->mpt_lock)
#define mpt_sleep(mpt, ident, priority, wmesg, timo) \
	msleep(ident, &(mpt)->mpt_lock, priority, wmesg, timo)
#else
#define	MPT_IFLAGS		INTR_TYPE_CAM | INTR_ENTROPY
#define	MPT_LOCK_SETUP(mpt)	do { } while (0)
#define	MPT_LOCK_DESTROY(mpt)	do { } while (0)
#define	MPT_LOCK(mpt)		do { } while (0)
#define	MPT_UNLOCK(mpt)		do { } while (0)
#define	MPTLOCK_2_CAMLOCK(mpt)	do { } while (0)
#define	CAMLOCK_2_MPTLOCK(mpt)	do { } while (0)
#define mpt_sleep(mpt, ident, priority, wmesg, timo) \
	tsleep(ident, priority, wmesg, timo)
#endif
#endif

/******************************* Register Access ******************************/
static __inline void mpt_write(struct mpt_softc *, size_t, uint32_t);
static __inline uint32_t mpt_read(struct mpt_softc *, int);
static __inline void mpt_pio_write(struct mpt_softc *, size_t, uint32_t);
static __inline uint32_t mpt_pio_read(struct mpt_softc *, int);

static __inline void
mpt_write(struct mpt_softc *mpt, size_t offset, uint32_t val)
{
	bus_space_write_4(mpt->pci_st, mpt->pci_sh, offset, val);
}

static __inline uint32_t
mpt_read(struct mpt_softc *mpt, int offset)
{
	return (bus_space_read_4(mpt->pci_st, mpt->pci_sh, offset));
}

/*
 * Some operations (e.g. diagnostic register writes while the ARM proccessor
 * is disabled), must be performed using "PCI pio" operations.  On non-PCI
 * busses, these operations likely map to normal register accesses.
 */
static __inline void
mpt_pio_write(struct mpt_softc *mpt, size_t offset, uint32_t val)
{
	bus_space_write_4(mpt->pci_pio_st, mpt->pci_pio_sh, offset, val);
}

static __inline uint32_t
mpt_pio_read(struct mpt_softc *mpt, int offset)
{
	return (bus_space_read_4(mpt->pci_pio_st, mpt->pci_pio_sh, offset));
}
/*********************** Reply Frame/Request Management ***********************/
/* Max MPT Reply we are willing to accept (must be power of 2) */
#define MPT_REPLY_SIZE   	128

#define MPT_MAX_REQUESTS(mpt)	((mpt)->is_fc ? 1024 : 256)
#define MPT_REQUEST_AREA 512
#define MPT_SENSE_SIZE    32	/* included in MPT_REQUEST_SIZE */
#define MPT_REQ_MEM_SIZE(mpt)	(MPT_MAX_REQUESTS(mpt) * MPT_REQUEST_AREA)

#define MPT_CONTEXT_CB_SHIFT	(16)
#define MPT_CBI(handle)	(handle >> MPT_CONTEXT_CB_SHIFT)
#define MPT_CBI_TO_HID(cbi)	((cbi) << MPT_CONTEXT_CB_SHIFT)
#define MPT_CONTEXT_TO_CBI(x)	\
    (((x) >> MPT_CONTEXT_CB_SHIFT) & (MPT_NUM_REPLY_HANDLERS - 1))
#define MPT_CONTEXT_REQI_MASK 0xFFFF
#define MPT_CONTEXT_TO_REQI(x)	\
    ((x) & MPT_CONTEXT_REQI_MASK)

/*
 * Convert a 32bit physical address returned from IOC to an
 * offset into our reply frame memory or the kvm address needed
 * to access the data.  The returned address is only the low
 * 32 bits, so mask our base physical address accordingly.
 */
#define MPT_REPLY_BADDR(x)		\
	(x << 1)
#define MPT_REPLY_OTOV(m, i) 		\
	((void *)(&m->reply[i]))

#define	MPT_DUMP_REPLY_FRAME(mpt, reply_frame)		\
do {							\
	if (mpt->verbose >= MPT_PRT_DEBUG)		\
		mpt_dump_reply_frame(mpt, reply_frame);	\
} while(0)

static __inline uint32_t mpt_pop_reply_queue(struct mpt_softc *mpt);
static __inline void mpt_free_reply(struct mpt_softc *mpt, uint32_t ptr);

/*
 * Give the reply buffer back to the IOC after we have
 * finished processing it.
 */
static __inline void
mpt_free_reply(struct mpt_softc *mpt, uint32_t ptr)
{
     mpt_write(mpt, MPT_OFFSET_REPLY_Q, ptr);
}

/* Get a reply from the IOC */
static __inline uint32_t
mpt_pop_reply_queue(struct mpt_softc *mpt)
{
     return mpt_read(mpt, MPT_OFFSET_REPLY_Q);
}

void mpt_complete_request_chain(struct mpt_softc *mpt,
				struct req_queue *chain, u_int iocstatus);
/************************** Scatter Gather Managment **************************/
/*
 * We cannot tell prior to getting IOC facts how big the IOC's request
 * area is. Because of this we cannot tell at compile time how many
 * simple SG elements we can fit within an IOC request prior to having
 * to put in a chain element.
 * 
 * Experimentally we know that the Ultra4 parts have a 96 byte request
 * element size and the Fibre Channel units have a 144 byte request
 * element size. Therefore, if we have 512-32 (== 480) bytes of request
 * area to play with, we have room for between 3 and 5 request sized
 * regions- the first of which is the command  plus a simple SG list,
 * the rest of which are chained continuation SG lists. Given that the
 * normal request we use is 48 bytes w/o the first SG element, we can
 * assume we have 480-48 == 432 bytes to have simple SG elements and/or
 * chain elements. If we assume 32 bit addressing, this works out to
 * 54 SG or chain elements. If we assume 5 chain elements, then we have
 * a maximum of 49 seperate actual SG segments.
 */
#define MPT_SGL_MAX		49

#define	MPT_RQSL(mpt)		(mpt->request_frame_size << 2)
#define	MPT_NSGL(mpt)		(MPT_RQSL(mpt) / sizeof (SGE_SIMPLE32))

#define	MPT_NSGL_FIRST(mpt)				\
	(((mpt->request_frame_size << 2) -		\
	sizeof (MSG_SCSI_IO_REQUEST) -			\
	sizeof (SGE_IO_UNION)) / sizeof (SGE_SIMPLE32))

/***************************** IOC Initialization *****************************/
int mpt_reset(struct mpt_softc *, int /*reinit*/);

/****************************** Debugging/Logging *****************************/
typedef struct mpt_decode_entry {
	char    *name;
	u_int	 value;
	u_int	 mask;
} mpt_decode_entry_t;

int mpt_decode_value(mpt_decode_entry_t *table, u_int num_entries,
		     const char *name, u_int value, u_int *cur_column,
		     u_int wrap_point);

enum {
	MPT_PRT_ALWAYS,
	MPT_PRT_FATAL,
	MPT_PRT_ERROR,
	MPT_PRT_WARN,
	MPT_PRT_INFO,
	MPT_PRT_DEBUG,
	MPT_PRT_TRACE
};

#define mpt_lprt(mpt, level, ...)		\
do {						\
	if (level <= (mpt)->verbose)		\
		mpt_prt(mpt, __VA_ARGS__);	\
} while (0)

#define mpt_lprtc(mpt, level, ...)		 \
do {						 \
	if (level <= (mpt)->debug_level)	 \
		mpt_prtc(mpt, __VA_ARGS__);	 \
} while (0)

void mpt_prt(struct mpt_softc *, const char *, ...);
void mpt_prtc(struct mpt_softc *, const char *, ...);

/**************************** Unclassified Routines ***************************/
void		mpt_send_cmd(struct mpt_softc *mpt, request_t *req);
int		mpt_recv_handshake_reply(struct mpt_softc *mpt,
					 size_t reply_len, void *reply);
int		mpt_wait_req(struct mpt_softc *mpt, request_t *req,
			     mpt_req_state_t state, mpt_req_state_t mask,
			     int sleep_ok, int time_ms);
void		mpt_enable_ints(struct mpt_softc *mpt);
void		mpt_disable_ints(struct mpt_softc *mpt);
int		mpt_attach(struct mpt_softc *mpt);
int		mpt_shutdown(struct mpt_softc *mpt);
int		mpt_detach(struct mpt_softc *mpt);
int		mpt_send_handshake_cmd(struct mpt_softc *mpt,
				       size_t len, void *cmd);
request_t *	mpt_get_request(struct mpt_softc *mpt, int sleep_ok);
void		mpt_free_request(struct mpt_softc *mpt, request_t *req);
void		mpt_intr(void *arg);
void		mpt_check_doorbell(struct mpt_softc *mpt);
void		mpt_dump_reply_frame(struct mpt_softc *mpt,
				     MSG_DEFAULT_REPLY *reply_frame);

void		mpt_set_config_regs(struct mpt_softc *);
int		mpt_issue_cfg_req(struct mpt_softc */*mpt*/, request_t */*req*/,
				  u_int /*Action*/, u_int /*PageVersion*/,
				  u_int /*PageLength*/, u_int /*PageNumber*/,
				  u_int /*PageType*/, uint32_t /*PageAddress*/,
				  bus_addr_t /*addr*/, bus_size_t/*len*/,
				  int /*sleep_ok*/, int /*timeout_ms*/);
int		mpt_read_cfg_header(struct mpt_softc *, int /*PageType*/,
				    int /*PageNumber*/,
				    uint32_t /*PageAddress*/,
				    CONFIG_PAGE_HEADER *,
				    int /*sleep_ok*/, int /*timeout_ms*/);
int		mpt_read_cfg_page(struct mpt_softc *t, int /*Action*/,
				  uint32_t /*PageAddress*/,
				  CONFIG_PAGE_HEADER *, size_t /*len*/,
				  int /*sleep_ok*/, int /*timeout_ms*/);
int		mpt_write_cfg_page(struct mpt_softc *, int /*Action*/,
				   uint32_t /*PageAddress*/,
				   CONFIG_PAGE_HEADER *, size_t /*len*/,
				   int /*sleep_ok*/, int /*timeout_ms*/);
static __inline int
mpt_read_cur_cfg_page(struct mpt_softc *mpt, uint32_t PageAddress,
		      CONFIG_PAGE_HEADER *hdr, size_t len,
		      int sleep_ok, int timeout_ms)
{
	return (mpt_read_cfg_page(mpt, MPI_CONFIG_ACTION_PAGE_READ_CURRENT,
				  PageAddress, hdr, len, sleep_ok, timeout_ms));
}

static __inline int
mpt_write_cur_cfg_page(struct mpt_softc *mpt, uint32_t PageAddress,
		       CONFIG_PAGE_HEADER *hdr, size_t len, int sleep_ok,
		       int timeout_ms)
{
	return (mpt_write_cfg_page(mpt, MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT,
				   PageAddress, hdr, len, sleep_ok,
				   timeout_ms));
}

/* mpt_debug.c functions */
void mpt_print_reply(void *vmsg);
void mpt_print_db(uint32_t mb);
void mpt_print_config_reply(void *vmsg);
char *mpt_ioc_diag(uint32_t diag);
void mpt_req_state(mpt_req_state_t state);
void mpt_print_config_request(void *vmsg);
void mpt_print_request(void *vmsg);
void mpt_print_scsi_io_request(MSG_SCSI_IO_REQUEST *msg);
#endif /* _MPT_H_ */
