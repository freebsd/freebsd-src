/* $FreeBSD$ */
/*
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
#include <sys/ioccom.h>

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/bus.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
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
#define INLINE __inline

/* Max MPT Reply we are willing to accept (must be power of 2) */
#define MPT_REPLY_SIZE   128

#define MPT_MAX_REQUESTS 256	/* XXX: should be derived from GlobalCredits */
#define MPT_REQUEST_AREA 512
#define MPT_SENSE_SIZE    32	/* included in MPT_REQUEST_SIZE */
#define MPT_REQ_MEM_SIZE	(MPT_MAX_REQUESTS * MPT_REQUEST_AREA)

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
	((void *)(m->reply + ((x << 1) - (u_int32_t)(m->reply_phys))))

#define ccb_mpt_ptr sim_priv.entries[0].ptr
#define ccb_req_ptr sim_priv.entries[1].ptr

enum mpt_req_state {
    REQ_FREE, REQ_IN_PROGRESS, REQ_TIMEOUT, REQ_ON_CHIP, REQ_DONE
};
typedef struct req_entry {
	u_int16_t    index;          /* Index of this entry */
	union ccb   *ccb;            /* Request that generated this command */
	void        *req_vbuf;       /* Virtual Address of Entry */
	void        *sense_vbuf;     /* Virtual Address of sense data */
	u_int32_t    req_pbuf;       /* Physical Address of Entry */
	u_int32_t    sense_pbuf;     /* Physical Address of sense data */
	bus_dmamap_t dmap;           /* DMA map for data buffer */
	SLIST_ENTRY(req_entry) link; /* Pointer to next in list */
	enum mpt_req_state debug;    /* Debuging */
	u_int32_t    sequence;       /* Sequence Number */

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

struct mpt_softc {
	device_t            dev;
	int                 unit;
	struct mtx  	    lock;

	/* Operational flags, set during initialization */
	int		    verbose;	/* print debug messages */

	struct resource    *pci_irq;	/* Interrupt map for chip */
	void               *ih;			/* Interupt handle */

        /* First Memory Region (Device MEM) */
	struct resource    *pci_reg;    /* Register map for chip */
	int                 pci_reg_id; /* Resource ID */
	bus_space_tag_t     pci_st;     /* Bus tag for registers */
	bus_space_handle_t  pci_sh;     /* Bus handle for registers */
	vm_offset_t         pci_pa;     /* Physical Address */

	/* Second Memory Region (Diagnostic memory window) */
	/* (only used for diagnostic purposes) */
	struct resource    *pci_mem;     /* Register map for chip */
	int                 pci_mem_id;  /* Resource ID */
	bus_space_tag_t     pci_mst;     /* Bus tag for registers */
	bus_space_handle_t  pci_msh;     /* Bus handle for registers */

	/* DMA Memory for IOCTLs */
        void            *ioctl_mem_va;   /* Virtual Addr */
        u_int32_t       ioctl_mem_pa;    /* Physical Addr */
        bus_dmamap_t    ioctl_mem_map;   /* DMA map for buffer */
        bus_dma_tag_t   ioctl_mem_tag;   /* DMA tag for memory alloc */
	int             open;            /* only allow one open at a time */

	bus_dma_tag_t       parent_dmat; /* DMA tag for parent PCI bus */

	bus_dma_tag_t       reply_dmat;  /* DMA tag for reply memory */
	bus_dmamap_t        reply_dmap;  /* DMA map for reply memory */
	char               *reply;       /* Virtual address of reply memory */
	u_int32_t           reply_phys;  /* Physical address of reply memory */

	u_int32_t
				: 29,
		disabled	: 1,
		is_fc		: 1,
		bus		: 1;	/* FC929/1030 have two busses */

	u_int32_t           blk_size;    /* Block size transfers to IOC */
	u_int16_t           mpt_global_credits;
	u_int16_t           request_frame_size;

	bus_dma_tag_t       buffer_dmat; /* DMA tag for mapping data buffers */

	bus_dma_tag_t       request_dmat; /* DMA tag for request memroy */
	bus_dmamap_t        request_dmap; /* DMA map for request memroy */
	char               *request;      /* Virtual address of Request memory */
	u_int32_t           request_phys; /* Physical address of Request memory */

	request_t            requests[MPT_MAX_REQUESTS];
	SLIST_HEAD(req_queue, req_entry) request_free_list;

	struct cam_sim      *sim;
	struct cam_path     *path;

	u_int32_t    sequence;       /* Sequence Number */
	u_int32_t    timeouts;       /* timeout count */
	u_int32_t    success;       /* timeout  successes afer timeout */

	/* Opposing port in a 929, or NULL */
	struct mpt_softc *mpt2;

	/* Saved values for the PCI configuration registers */
	struct mpt_pci_cfg pci_cfg;
};

static INLINE void
mpt_write(struct mpt_softc *mpt, size_t offset, u_int32_t val)
{
	bus_space_write_4(mpt->pci_st, mpt->pci_sh, offset, val);
}

static INLINE u_int32_t
mpt_read(struct mpt_softc *mpt, int offset)
{
	return bus_space_read_4(mpt->pci_st, mpt->pci_sh, offset);
}

void mpt_cam_attach(struct mpt_softc *mpt);
void mpt_cam_detach(struct mpt_softc *mpt);
void mpt_done(struct mpt_softc *mpt, u_int32_t reply);
void mpt_notify(struct mpt_softc *mpt, void *vmsg);

/* mpt_pci.c declarations */
void mpt_set_config_regs(struct mpt_softc *mpt);

#endif  /*_KERNEL */
#endif	/* _MPT_FREEBSD_H */
