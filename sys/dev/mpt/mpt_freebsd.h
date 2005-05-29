/* $FreeBSD$ */
/*-
 * LSI MPT Host Adapter FreeBSD Wrapper Definitions (CAM version)
 *
 * Copyright (c) 2000, 2001 by Greg Ansley, Adam Prewett
 *
 * Partially derived from Matty Jacobs ISP driver.
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

#ifndef  _MPT_FREEBSD_H_
#define  _MPT_FREEBSD_H_

/* #define RELENG_4	1 */

#include <sys/param.h>
#include <sys/systm.h>
#ifdef	RELENG_4
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#else
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#endif
#include <sys/proc.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpu.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include "opt_ddb.h"

#include "dev/mpt/mpilib/mpi_type.h"
#include "dev/mpt/mpilib/mpi.h"
#include "dev/mpt/mpilib/mpi_cnfg.h"
#include "dev/mpt/mpilib/mpi_fc.h"
#include "dev/mpt/mpilib/mpi_init.h"
#include "dev/mpt/mpilib/mpi_ioc.h"
#include "dev/mpt/mpilib/mpi_lan.h"
#include "dev/mpt/mpilib/mpi_targ.h"


#define INLINE __inline

#ifdef	RELENG_4
#define	MPT_IFLAGS		INTR_TYPE_CAM
#define	MPT_LOCK(mpt)		mpt_lockspl(mpt)
#define	MPT_UNLOCK(mpt)		mpt_unlockspl(mpt)
#define	MPTLOCK_2_CAMLOCK	MPT_UNLOCK
#define	CAMLOCK_2_MPTLOCK	MPT_LOCK
#define	MPT_LOCK_SETUP(mpt)
#define	MPT_LOCK_DESTROY(mpt)
#else
#if	LOCKING_WORKED_AS_IT_SHOULD
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
#else
#define	MPT_IFLAGS		INTR_TYPE_CAM | INTR_ENTROPY
#define	MPT_LOCK_SETUP(mpt)	do { } while (0)
#define	MPT_LOCK_DESTROY(mpt)	do { } while (0)
#define	MPT_LOCK(mpt)		do { } while (0)
#define	MPT_UNLOCK(mpt)		do { } while (0)
#define	MPTLOCK_2_CAMLOCK(mpt)	do { } while (0)
#define	CAMLOCK_2_MPTLOCK(mpt)	do { } while (0)
#endif
#endif
	


/* Max MPT Reply we are willing to accept (must be power of 2) */
#define MPT_REPLY_SIZE   	128

#define MPT_MAX_REQUESTS(mpt)	((mpt)->is_fc? 1024 : 256)
#define MPT_REQUEST_AREA 512
#define MPT_SENSE_SIZE    32	/* included in MPT_REQUEST_SIZE */
#define MPT_REQ_MEM_SIZE(mpt)	(MPT_MAX_REQUESTS(mpt) * MPT_REQUEST_AREA)

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

/*
 * Convert a physical address returned from IOC to kvm address
 * needed to access the data.
 */
#define MPT_REPLY_PTOV(m, x) 		\
	((void *)(&m->reply[((x << 1) - m->reply_phys)]))

#define ccb_mpt_ptr sim_priv.entries[0].ptr
#define ccb_req_ptr sim_priv.entries[1].ptr

enum mpt_req_state {
    REQ_FREE, REQ_IN_PROGRESS, REQ_TIMEOUT, REQ_ON_CHIP, REQ_DONE
};
typedef struct req_entry {
	u_int16_t	index;		/* Index of this entry */
	union ccb *	ccb;		/* CAM request */
	void *		req_vbuf;	/* Virtual Address of Entry */
	void *		sense_vbuf;	/* Virtual Address of sense data */
	bus_addr_t	req_pbuf;	/* Physical Address of Entry */
	bus_addr_t	sense_pbuf;	/* Physical Address of sense data */
	bus_dmamap_t	dmap;		/* DMA map for data buffer */
	SLIST_ENTRY(req_entry) link;	/* Pointer to next in list */
	enum mpt_req_state debug;	/* Debugging */
	u_int32_t	sequence;	/* Sequence Number */
} request_t;


/* Structure for saving proper values for modifyable PCI configuration registers */
struct mpt_pci_cfg {
	u_int16_t Command;
	u_int16_t LatencyTimer_LineSize;
	u_int32_t IO_BAR;
	u_int32_t Mem0_BAR[2];
	u_int32_t Mem1_BAR[2];
	u_int32_t ROM_BAR;
	u_int8_t  IntLine;
	u_int32_t PMCSR;
};

typedef struct mpt_softc {
	device_t		dev;
#ifdef	RELENG_4
	int			mpt_splsaved;
	u_int32_t		mpt_islocked;	
#else
	struct mtx		mpt_lock;
#endif
	u_int32_t		: 16,
		unit		: 8,
		verbose		: 3,
		outofbeer	: 1,
		mpt_locksetup	: 1,
		disabled	: 1,
		is_fc		: 1,
		bus		: 1;	/* FC929/1030 have two busses */

	/*
	 * IOC Facts
	 */
	u_int16_t		mpt_global_credits;
	u_int16_t		request_frame_size;
	u_int8_t		mpt_max_devices;
	u_int8_t		mpt_max_buses;

	/*
	 * Port Facts
	 */
	u_int16_t		mpt_ini_id;


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
			u_int8_t	nada;
		} fc;
	} cfg;

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
	int			pci_reg_id;	/* Resource ID */
	bus_space_tag_t		pci_st;		/* Bus tag for registers */
	bus_space_handle_t	pci_sh;		/* Bus handle for registers */
	vm_offset_t		pci_pa;		/* Physical Address */

	bus_dma_tag_t		parent_dmat;	/* DMA tag for parent PCI bus */
	bus_dma_tag_t		reply_dmat;	/* DMA tag for reply memory */
	bus_dmamap_t		reply_dmap;	/* DMA map for reply memory */
	char *			reply;		/* KVA of reply memory */
	bus_addr_t		reply_phys;	/* BusAddr of reply memory */


	bus_dma_tag_t		buffer_dmat;	/* DMA tag for buffers */
	bus_dma_tag_t		request_dmat;	/* DMA tag for request memroy */
	bus_dmamap_t		request_dmap;	/* DMA map for request memroy */
	char *			request;	/* KVA of Request memory */
	bus_addr_t		request_phys;	/* BusADdr of request memory */

	/*
	 * CAM && Software Management
	 */

	request_t *		request_pool;
	SLIST_HEAD(req_queue, req_entry) request_free_list;

	struct cam_sim *	sim;
	struct cam_path *	path;

	u_int32_t		sequence;	/* Sequence Number */
	u_int32_t		timeouts;	/* timeout count */
	u_int32_t		success;	/* successes afer timeout */

	/* Opposing port in a 929 or 1030, or NULL */
	struct mpt_softc *	mpt2;

} mpt_softc_t;

#include <dev/mpt/mpt.h>


static INLINE void mpt_write(mpt_softc_t *, size_t, u_int32_t);
static INLINE u_int32_t mpt_read(mpt_softc_t *, int);

static INLINE void
mpt_write(mpt_softc_t *mpt, size_t offset, u_int32_t val)
{
	bus_space_write_4(mpt->pci_st, mpt->pci_sh, offset, val);
}

static INLINE u_int32_t
mpt_read(mpt_softc_t *mpt, int offset)
{
	return (bus_space_read_4(mpt->pci_st, mpt->pci_sh, offset));
}

void mpt_cam_attach(mpt_softc_t *);
void mpt_cam_detach(mpt_softc_t *);
void mpt_done(mpt_softc_t *, u_int32_t);
void mpt_prt(mpt_softc_t *, const char *, ...);
void mpt_set_config_regs(mpt_softc_t *);

#ifdef	RELENG_4
static INLINE void mpt_lockspl(mpt_softc_t *);
static INLINE void mpt_unlockspl(mpt_softc_t *);

static INLINE void
mpt_lockspl(mpt_softc_t *mpt)
{
       int s = splcam();
       if (mpt->mpt_islocked++ == 0) {  
               mpt->mpt_splsaved = s;
       } else {
               splx(s);
       }
}

static INLINE void
mpt_unlockspl(mpt_softc_t *mpt)
{
       if (mpt->mpt_islocked) {
               if (--mpt->mpt_islocked == 0) {
                       splx(mpt->mpt_splsaved);
               }
       }
}
#endif

#endif	/* _MPT_FREEBSD_H */
