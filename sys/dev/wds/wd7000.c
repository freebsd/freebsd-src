/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * Copyright (c) 2000 Sergey A. Babkin
 * All rights reserved.
 *
 * Written by Olof Johansson (offe@ludd.luth.se) 1995.
 * Based on code written by Theo de Raadt (deraadt@fsa.ca).
 * Resurrected, ported to CAM and generally cleaned up by Sergey Babkin
 * <babkin@bellatlantic.net> or <babkin@users.sourceforge.net>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}
 *     and by the FreeBSD project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* All bugs are subject to removal without further notice */

/*
 * offe 01/07/95
 * 
 * This version of the driver _still_ doesn't implement scatter/gather for the
 * WD7000-FASST2. This is due to the fact that my controller doesn't seem to
 * support it. That, and the lack of documentation makes it impossible for me
 * to implement it. What I've done instead is allocated a local buffer,
 * contiguous buffer big enough to handle the requests. I haven't seen any
 * read/write bigger than 64k, so I allocate a buffer of 64+16k. The data
 * that needs to be DMA'd to/from the controller is copied to/from that
 * buffer before/after the command is sent to the card.
 * 
 * SB 03/30/00
 * 
 * An intermediate buffer is needed anyway to make sure that the buffer is
 * located under 16MB, otherwise it's out of reach of ISA cards. I've added
 * optimizations to allocate space in buffer in fragments.
 */

/*
 * Jumpers: (see The Ref(TM) for more info)
 * W1/W2 - interrupt selection:
 *  W1 (1-2) IRQ3, (3-4) IRQ4, (5-6) IRQ5, (7-8) IRQ7, (9-10) IRQ9
 *  W2 (21-22) IRQ10, (19-20) IRQ11, (17-18) IRQ12, (15-16) IRQ14, (13-14) IRQ15
 *
 * W2 - DRQ/DACK selection, DRQ and DACK must be the same:
 *  (5-6) DRQ5 (11-12) DACK5
 *  (3-4) DRQ6 (9-10) DACK6
 *  (1-2) DRQ7 (7-8) DACK7
 *
 * W3 - I/O address selection: open pair of pins (OFF) means 1, jumpered (ON) means 0
 *  pair (1-2) is bit 3, ..., pair (9-10) is bit 7. All the other bits are equal
 *  to the value 0x300. In bitwise representation that would be:
 *   0 0 1 1 (9-10) (7-8) (5-6) (3-4) (1-2) 0 0 0
 *  For example, address 0x3C0, bitwise 1111000000 will be represented as:
 *   (9-10) OFF, (7-8) OFF, (5-6) ON, (3-4) ON, (1-2) ON
 * 
 * W4 - BIOS address: open pair of pins (OFF) means 1, jumpered (ON) means 0
 *  pair (1-2) is bit 13, ..., pair (7-8) is bit 16. All the other bits are
 *  equal to the value 0xC0000. In bitwise representation that would be:
 *   1 1 0 (7-8) (5-6) (3-4) (1-2) 0 0000 0000 0000
 *  For example, address 0xD8000 will be represented as:
 *   (7-8) OFF, (5-6) OFF, (3-4) ON, (1-2) ON
 *
 * W98 (on newer cards) - BIOS enabled; on older cards just remove the BIOS
 * chip to disable it
 * W99 (on newer cards) - ROM size (1-2) OFF, (3-4) ON
 *
 * W5 - terminator power
 *  ON - host supplies term. power
 *  OFF - target supplies term. power
 *
 * W6, W9 - floppy support (a bit cryptic):
 *  W6 ON, W9 ON - disabled
 *  W6 OFF, W9 ON - enabled with HardCard only
 *  W6 OFF, W9 OFF - enabled with no hardCard or Combo
 *
 * Default: I/O 0x350, IRQ15, DMA6
 */

/*
 * debugging levels: 
 * 0 - disabled 
 * 1 - print debugging messages 
 * 2 - collect  debugging messages in an internal log buffer which can be 
 *     printed later by calling wds_printlog from DDB 
 *
 * Both kind of logs are heavy and interact significantly with the timing 
 * of commands, so the observed problems may become invisible if debug 
 * logging is enabled.
 * 
 * The light-weight logging facility may be enabled by defining
 * WDS_ENABLE_SMALLOG as 1. It has very little overhead and allows observing 
 * the traces of various race conditions without affectiong them but the log is
 * quite terse. The small log can be printer from DDB by calling
 * wds_printsmallog.
 */
#ifndef WDS_DEBUG
#define WDS_DEBUG 0
#endif

#ifndef WDS_ENABLE_SMALLOG 
#define WDS_ENABLE_SMALLOG 0
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/assym.h>

#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/disklabel.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <isa/pnpvar.h>

#define WDSTOPHYS(wp, a)	( ((u_long)a) - ((u_long)wp->dx) + ((u_long)wp->dx_p) )
#define WDSTOVIRT(wp, a)	( ((char *)a) - ((char*)wp->dx_p) + ((char *)wp->dx) )

/* 0x10000 (64k) should be enough. But just to be sure... */
#define BUFSIZ 		0x12000
/* buffer fragment size, no more than 32 frags per buffer */
#define FRAGSIZ		0x1000


/* WD7000 registers */
#define WDS_STAT		0	/* read */
#define WDS_IRQSTAT		1	/* read */

#define WDS_CMD			0	/* write */
#define WDS_IRQACK		1	/* write */
#define WDS_HCR			2	/* write */

#define WDS_NPORTS		4 /* number of ports used */

/* WDS_STAT (read) defs */
#define WDS_IRQ			0x80
#define WDS_RDY			0x40
#define WDS_REJ			0x20
#define WDS_INIT		0x10

/* WDS_IRQSTAT (read) defs */
#define WDSI_MASK		0xc0
#define WDSI_ERR		0x00
#define WDSI_MFREE		0x80
#define WDSI_MSVC		0xc0

/* WDS_CMD (write) defs */
#define WDSC_NOOP		0x00
#define WDSC_INIT		0x01
#define WDSC_DISUNSOL		0x02 /* disable unsolicited ints */
#define WDSC_ENAUNSOL		0x03 /* enable unsolicited ints */
#define WDSC_IRQMFREE		0x04 /* interrupt on free RQM */
#define WDSC_SCSIRESETSOFT	0x05 /* soft reset */
#define WDSC_SCSIRESETHARD	0x06 /* hard reset ack */
#define WDSC_MSTART(m)		(0x80 + (m)) /* start mailbox */
#define WDSC_MMSTART(m)		(0xc0 + (m)) /* start all mailboxes */

/* WDS_HCR (write) defs */
#define WDSH_IRQEN		0x08
#define WDSH_DRQEN		0x04
#define WDSH_SCSIRESET		0x02
#define WDSH_ASCRESET		0x01

struct wds_cmd {
	u_int8_t	cmd;
	u_int8_t	targ;
	u_int8_t	scb[12];
	u_int8_t	stat;
	u_int8_t	venderr;
	u_int8_t	len[3];
	u_int8_t	data[3];
	u_int8_t	next[3];
	u_int8_t	write;
	u_int8_t	xx[6];
};

struct wds_req {
	struct	   wds_cmd cmd;
	union	   ccb *ccb;
	enum {
		WR_DONE = 0x01,
		WR_SENSE = 0x02
	} flags;
	u_int8_t  *buf;		/* address of linear data buffer */
	u_int32_t  mask;	/* mask of allocated fragments */
	u_int8_t	ombn;
	u_int8_t	id;	/* number of request */
};

#define WDSX_SCSICMD		0x00
#define WDSX_OPEN_RCVBUF	0x80
#define WDSX_RCV_CMD		0x81
#define WDSX_RCV_DATA		0x82
#define WDSX_RCV_DATASTAT	0x83
#define WDSX_SND_DATA		0x84
#define WDSX_SND_DATASTAT	0x85
#define WDSX_SND_CMDSTAT	0x86
#define WDSX_READINIT		0x88
#define WDSX_READSCSIID		0x89
#define WDSX_SETUNSOLIRQMASK	0x8a
#define WDSX_GETUNSOLIRQMASK	0x8b
#define WDSX_GETFIRMREV		0x8c
#define WDSX_EXECDIAG		0x8d
#define WDSX_SETEXECPARM	0x8e
#define WDSX_GETEXECPARM	0x8f

struct wds_mb {
	u_int8_t	stat;
	u_int8_t	addr[3];
};
/* ICMB status value */
#define ICMB_OK			0x01
#define ICMB_OKERR		0x02
#define ICMB_ETIME		0x04
#define ICMB_ERESET		0x05
#define ICMB_ETARCMD		0x06
#define ICMB_ERESEL		0x80
#define ICMB_ESEL		0x81
#define ICMB_EABORT		0x82
#define ICMB_ESRESET		0x83
#define ICMB_EHRESET		0x84

struct wds_setup {
	u_int8_t	cmd;
	u_int8_t	scsi_id;
	u_int8_t	buson_t;
	u_int8_t	busoff_t;
	u_int8_t	xx;
	u_int8_t	mbaddr[3];
	u_int8_t	nomb;
	u_int8_t	nimb;
};

/* the code depends on equality of these parameters */
#define MAXSIMUL	8
#define WDS_NOMB	MAXSIMUL
#define WDS_NIMB	MAXSIMUL

static int	fragsiz;
static int	nfrags;

/* structure for data exchange with controller */

struct wdsdx {
	struct wds_req	req[MAXSIMUL];
	struct wds_mb	ombs[MAXSIMUL];
	struct wds_mb	imbs[MAXSIMUL];
	u_int8_t	data[BUFSIZ];
};

/* structure softc */

struct wds {
	device_t	 dev;
	int		 unit;
	int		 addr;
	int		 drq;
	struct cam_sim	*sim;	/* SIM descriptor for this card */
	struct cam_path	*path;	/* wildcard path for this card */
	char		 want_wdsr;	/* resource shortage flag */
	u_int32_t	 data_free;
	u_int32_t	 wdsr_free;
	struct wdsdx	*dx;
	struct wdsdx	*dx_p; /* physical address */
	struct resource	*port_r;
	int		 port_rid;
	struct resource	*drq_r;
	int		 drq_rid;
	struct resource *intr_r;
	int		 intr_rid;
	void		*intr_cookie;
	bus_dma_tag_t	 bustag;
	bus_dmamap_t	 busmap;
};

#define ccb_wdsr	spriv_ptr1	/* for wds request */

static int      wds_probe(device_t dev);
static int      wds_attach(device_t dev);
static void     wds_intr(struct wds *wp);

static void     wds_action(struct cam_sim * sim, union ccb * ccb);
static void     wds_poll(struct cam_sim * sim);

static int      wds_preinit(struct wds *wp);
static int      wds_init(struct wds *wp);

static void     wds_alloc_callback(void *arg, bus_dma_segment_t *seg,  
	 int nseg, int error);
static void     wds_free_resources(struct wds *wp);

static struct wds_req *wdsr_alloc(struct wds *wp);

static void     wds_scsi_io(struct cam_sim * sim, struct ccb_scsiio * csio);
static void     wdsr_ccb_done(struct wds *wp, struct wds_req *r, 
			      union ccb *ccb, u_int32_t status);

static void     wds_done(struct wds *wp, struct wds_req *r, u_int8_t stat);
static int      wds_runsense(struct wds *wp, struct wds_req *r);
static int      wds_getvers(struct wds *wp);

static int      wds_cmd(int base, u_int8_t * p, int l);
static void     wds_wait(int reg, int mask, int val);

static struct wds_req *cmdtovirt(struct wds *wp, u_int32_t phys);

static u_int32_t frag_alloc(struct wds *wp, int size, u_int8_t **res, 
			    u_int32_t *maskp);
static void     frag_free(struct wds *wp, u_int32_t mask);

void            wds_print(void);

#if WDS_ENABLE_SMALLOG==1
static __inline void   smallog(char c);
void 	wds_printsmallog(void);
#endif /* SMALLOG */

/* SCSI ID of the adapter itself */
#ifndef WDS_HBA_ID
#define WDS_HBA_ID 7
#endif

#if WDS_DEBUG == 2
#define LOGLINESIZ	81
#define NLOGLINES	300
#define DBX	wds_nextlog(), LOGLINESIZ,
#define DBG	snprintf

static char     wds_log[NLOGLINES][LOGLINESIZ];
static int      logwrite = 0, logread = 0;
static char    *wds_nextlog(void);
void            wds_printlog(void);

#elif WDS_DEBUG != 0
#define DBX
#define DBG	printf
#else
#define DBX
#define DBG	if(0) printf
#endif

/* the table of supported bus methods */
static device_method_t wds_isa_methods[] = {
	DEVMETHOD(device_probe,		wds_probe),
	DEVMETHOD(device_attach,	wds_attach),
	{ 0, 0 }
};

static driver_t wds_isa_driver = {
	"wds",
	wds_isa_methods,
	sizeof(struct wds),
};

static devclass_t wds_devclass;

DRIVER_MODULE(wds, isa, wds_isa_driver, wds_devclass, 0, 0);

#if WDS_ENABLE_SMALLOG==1
#define SMALLOGSIZ	512
static char	 wds_smallog[SMALLOGSIZ];
static char	*wds_smallogp = wds_smallog;
static char	 wds_smallogover = 0;

static __inline void
smallog(char c)
{
	*wds_smallogp = c;
	if (++wds_smallogp == &wds_smallog[SMALLOGSIZ]) {
		wds_smallogp = wds_smallog;
		wds_smallogover = 1;
	}
}

#define smallog2(a, b)	(smallog(a), smallog(b))
#define smallog3(a, b, c)	(smallog(a), smallog(b), smallog(c))
#define smallog4(a, b, c, d)	(smallog(a),smallog(b),smallog(c),smallog(d))

void 
wds_printsmallog(void)
{
	int	 i;
	char	*p;

	printf("wds: ");
	p = wds_smallogover ? wds_smallogp : wds_smallog;
	i = 0;
	do {
		printf("%c", *p);
		if (++p == &wds_smallog[SMALLOGSIZ])
			p = wds_smallog;
		if (++i == 70) {
			i = 0;
			printf("\nwds: ");
		}
	} while (p != wds_smallogp);
	printf("\n");
}
#else
#define smallog(a)
#define smallog2(a, b)
#define smallog3(a, b, c)
#define smallog4(a, b, c, d)
#endif				/* SMALLOG */

static int
wds_probe(device_t dev)
{
	struct	wds *wp;
	int	error = 0;
	int	irq;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	wp = (struct wds *) device_get_softc(dev);
	wp->unit = device_get_unit(dev);
	wp->dev = dev;

	wp->addr = bus_get_resource_start(dev, SYS_RES_IOPORT, 0 /*rid*/);
	if (wp->addr == 0 || wp->addr <0x300
	 || wp->addr > 0x3f8 || wp->addr & 0x7) {
		device_printf(dev, "invalid port address 0x%x\n", wp->addr);
		return (ENXIO);
	}

	if (bus_set_resource(dev, SYS_RES_IOPORT, 0, wp->addr, WDS_NPORTS) < 0)
		return (ENXIO);

	/* get the DRQ */
	wp->drq = bus_get_resource_start(dev, SYS_RES_DRQ, 0 /*rid*/);
	if (wp->drq < 5 || wp->drq > 7) {
		device_printf(dev, "invalid DRQ %d\n", wp->drq);
		return (ENXIO);
	}

	/* get the IRQ */
	irq = bus_get_resource_start(dev, SYS_RES_IRQ, 0 /*rid*/);
	if (irq < 3) {
		device_printf(dev, "invalid IRQ %d\n", irq);
		return (ENXIO);
	}

	wp->port_rid = 0;
	wp->port_r = bus_alloc_resource(dev, SYS_RES_IOPORT,  &wp->port_rid,
				        /*start*/ 0, /*end*/ ~0,
					/*count*/ 0, RF_ACTIVE);
	if (wp->port_r == NULL)
		return (ENXIO);

	error = wds_preinit(wp);

	/*
	 * We cannot hold resources between probe and
	 * attach as we may never be attached.
	 */
	wds_free_resources(wp);

	return (error);
}

static int
wds_attach(device_t dev)
{
	struct	wds *wp;
	struct	cam_devq *devq;
	struct	cam_sim *sim;
	struct	cam_path *pathp;
	int	i;
	int	error = 0;

	wp = (struct wds *)device_get_softc(dev);

	wp->port_rid = 0;
	wp->port_r = bus_alloc_resource(dev, SYS_RES_IOPORT,  &wp->port_rid,
					/*start*/ 0, /*end*/ ~0,
					/*count*/ 0, RF_ACTIVE);
	if (wp->port_r == NULL)
		return (ENXIO);

	/* We must now release resources on error. */

	wp->drq_rid = 0;
	wp->drq_r = bus_alloc_resource(dev, SYS_RES_DRQ,  &wp->drq_rid,
				       /*start*/ 0, /*end*/ ~0,
				       /*count*/ 0, RF_ACTIVE);
	if (wp->drq_r == NULL)
		goto bad;

	wp->intr_rid = 0;
	wp->intr_r = bus_alloc_resource(dev, SYS_RES_IRQ,  &wp->intr_rid,
					/*start*/ 0, /*end*/ ~0,
					/*count*/ 0, RF_ACTIVE);
	if (wp->intr_r == NULL)
		goto bad;
	error = bus_setup_intr(dev, wp->intr_r, INTR_TYPE_CAM | INTR_ENTROPY,
			       (driver_intr_t *)wds_intr, (void *)wp,
			       &wp->intr_cookie);
	if (error)
		goto bad;

	/* now create the memory buffer */
	error = bus_dma_tag_create(NULL, /*alignment*/4,
				   /*boundary*/0,
				   /*lowaddr*/BUS_SPACE_MAXADDR_24BIT,
				   /*highaddr*/ BUS_SPACE_MAXADDR,
				   /*filter*/ NULL, /*filterarg*/ NULL,
				   /*maxsize*/ sizeof(* wp->dx),
				   /*nsegments*/ 1,
				   /*maxsegsz*/ sizeof(* wp->dx), /*flags*/ 0,
				   &wp->bustag);
	if (error)
		goto bad;

	error = bus_dmamem_alloc(wp->bustag, (void **)&wp->dx,
				 /*flags*/ 0, &wp->busmap);
	if (error)
		goto bad;
            
	bus_dmamap_load(wp->bustag, wp->busmap, (void *)wp->dx,
			sizeof(* wp->dx), wds_alloc_callback,
			(void *)&wp->dx_p, /*flags*/0);

	/* initialize the wds_req structures on this unit */
	for(i=0; i<MAXSIMUL; i++)  {
		wp->dx->req[i].id = i;
		wp->wdsr_free |= 1<<i;
	}

	/* initialize the memory buffer allocation for this unit */
	if (BUFSIZ / FRAGSIZ > 32) {
		fragsiz = (BUFSIZ / 32) & ~0x01; /* keep it word-aligned */
		device_printf(dev, "data buffer fragment size too small.  "
			      "BUFSIZE / FRAGSIZE must be <= 32\n");
	} else
		fragsiz = FRAGSIZ & ~0x01; /* keep it word-aligned */

	wp->data_free = 0;
	nfrags = 0;
	for (i = fragsiz; i <= BUFSIZ; i += fragsiz) {
		nfrags++;
		wp->data_free = (wp->data_free << 1) | 1;
	}

	/* complete the hardware initialization */
	if (wds_init(wp) != 0)
		goto bad;

	if (wds_getvers(wp) == -1)
		device_printf(dev, "getvers failed\n");
	device_printf(dev, "using %d bytes / %d frags for dma buffer\n",
		      BUFSIZ, nfrags);

	devq = cam_simq_alloc(MAXSIMUL);
	if (devq == NULL)
		goto bad;

	sim = cam_sim_alloc(wds_action, wds_poll, "wds", (void *) wp,
			    wp->unit, 1, 1, devq);
	if (sim == NULL) {
		cam_simq_free(devq);
		goto bad;
	}
	wp->sim = sim;

	if (xpt_bus_register(sim, 0) != CAM_SUCCESS) {
		cam_sim_free(sim, /* free_devq */ TRUE);
		goto bad;
	}
	if (xpt_create_path(&pathp, /* periph */ NULL,
			    cam_sim_path(sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, /* free_devq */ TRUE);
		goto bad;
	}
	wp->path = pathp;

	return (0);

bad:
	wds_free_resources(wp);
	if (error)  
		return (error);
	else /* exact error is unknown */
		return (ENXIO);
}

/* callback to save the physical address */
static void     
wds_alloc_callback(void *arg, bus_dma_segment_t *seg,  int nseg, int error)
{
	*(bus_addr_t *)arg = seg[0].ds_addr;
}

static void     
wds_free_resources(struct wds *wp)
{
	/* check every resource and free if not zero */
            
	/* interrupt handler */
	if (wp->intr_r) {
		bus_teardown_intr(wp->dev, wp->intr_r, wp->intr_cookie);
		bus_release_resource(wp->dev, SYS_RES_IRQ, wp->intr_rid,
				     wp->intr_r);
		wp->intr_r = 0;
	}

	/* all kinds of memory maps we could have allocated */
	if (wp->dx_p) {
		bus_dmamap_unload(wp->bustag, wp->busmap);
		wp->dx_p = 0;
	}
	if (wp->dx) { /* wp->busmap may be legitimately equal to 0 */
		/* the map will also be freed */
		bus_dmamem_free(wp->bustag, wp->dx, wp->busmap);
		wp->dx = 0;
	}
	if (wp->bustag) {
		bus_dma_tag_destroy(wp->bustag);
		wp->bustag = 0;
	}
	/* release all the bus resources */
	if (wp->drq_r) {
		bus_release_resource(wp->dev, SYS_RES_DRQ,
				     wp->drq_rid, wp->drq_r);
		wp->drq_r = 0;
	}
	if (wp->port_r) {
		bus_release_resource(wp->dev, SYS_RES_IOPORT,
				     wp->port_rid, wp->port_r);
		wp->port_r = 0;
	}
}

/* allocate contiguous fragments from the buffer */
static u_int32_t
frag_alloc(struct wds *wp, int size, u_int8_t **res, u_int32_t *maskp)
{
	int	i;
	u_int32_t	mask;
	u_int32_t	free;

	if (size > fragsiz * nfrags)
		return (CAM_REQ_TOO_BIG);

	mask = 1;		/* always allocate at least 1 fragment */
	for (i = fragsiz; i < size; i += fragsiz)
		mask = (mask << 1) | 1;

	free = wp->data_free;
	if(free != 0) {
		i = ffs(free)-1; /* ffs counts bits from 1 */
		for (mask <<= i; i < nfrags; i++) {
			if ((free & mask) == mask) {
				wp->data_free &= ~mask;	/* mark frags as busy */
				*maskp = mask;
				*res = &wp->dx->data[fragsiz * i];
				DBG(DBX "wds%d: allocated buffer mask=0x%x\n",
					wp->unit, mask);
				return (CAM_REQ_CMP);
			}
			if (mask & 0x80000000)
				break;

			mask <<= 1;
		}
	}
	return (CAM_REQUEUE_REQ);	/* no free memory now, try later */
}

static void
frag_free(struct wds *wp, u_int32_t mask)
{
	wp->data_free |= mask;	/* mark frags as free */
	DBG(DBX "wds%d: freed buffer mask=0x%x\n", wp->unit, mask);
}

static struct wds_req *
wdsr_alloc(struct wds *wp)
{
	struct	wds_req *r;
	int	x;
	int	i;

	r = NULL;
	x = splcam();

	/* anyway most of the time only 1 or 2 commands will
	 * be active because SCSI disconnect is not supported
	 * by hardware, so the search should be fast enough
	 */
	i = ffs(wp->wdsr_free) - 1;
	if(i < 0) {
		splx(x);
		return (NULL);
	}
	wp->wdsr_free &= ~ (1<<i);
	r = &wp->dx->req[i];
	r->flags = 0;	/* reset all flags */
	r->ombn = i;		/* luckily we have one omb per wdsr */
	wp->dx->ombs[i].stat = 1;

	r->mask = 0;
	splx(x);
	smallog3('r', i + '0', r->ombn + '0');
	return (r);
}

static void
wds_intr(struct wds *wp)
{
	struct	 wds_req *rp;
	struct	 wds_mb *in;
	u_int8_t stat;
	u_int8_t c;
	int	 addr = wp->addr;

	DBG(DBX "wds%d: interrupt [\n", wp->unit);
	smallog('[');

	if (inb(addr + WDS_STAT) & WDS_IRQ) {
		c = inb(addr + WDS_IRQSTAT);
		if ((c & WDSI_MASK) == WDSI_MSVC) {
			c = c & ~WDSI_MASK;
			in = &wp->dx->imbs[c];

			rp = cmdtovirt(wp, scsi_3btoul(in->addr));
			stat = in->stat;

			if (rp != NULL)
				wds_done(wp, rp, stat);
			else
				device_printf(wp->dev,
					      "got weird command address %p"
					      "from controller\n", rp);

			in->stat = 0;
		} else
			device_printf(wp->dev,
				      "weird interrupt, irqstat=0x%x\n", c);
		outb(addr + WDS_IRQACK, 0);
	} else {
		smallog('?');
	}
	smallog(']');
	DBG(DBX "wds%d: ]\n", wp->unit);
}

static void
wds_done(struct wds *wp, struct wds_req *r, u_int8_t stat)
{
	struct	ccb_hdr *ccb_h;
	struct	ccb_scsiio *csio;
	int	status;

	smallog('d');

	if (r->flags & WR_DONE) {
		device_printf(wp->dev,
				"request %d reported done twice\n", r->id);
		smallog2('x', r->id + '0');
		return;
	}

	smallog(r->id + '0');
	ccb_h = &r->ccb->ccb_h;
	csio = &r->ccb->csio;
	status = CAM_REQ_CMP_ERR;

	DBG(DBX "wds%d: %s stat=0x%x c->stat=0x%x c->venderr=0x%x\n", wp->unit,
	    r->flags & WR_SENSE ? "(sense)" : "", 
		stat, r->cmd.stat, r->cmd.venderr);

	if (r->flags & WR_SENSE) {
		if (stat == ICMB_OK || (stat == ICMB_OKERR && r->cmd.stat == 0)) {
			DBG(DBX "wds%d: sense 0x%x\n", wp->unit, r->buf[0]);
			/* it has the same size now but for future */
			bcopy(r->buf, &csio->sense_data,
			      sizeof(struct scsi_sense_data) > csio->sense_len ?
			      csio->sense_len : sizeof(struct scsi_sense_data));
			if (sizeof(struct scsi_sense_data) >= csio->sense_len)
				csio->sense_resid = 0;
			else
				csio->sense_resid =
					csio->sense_len
				      - sizeof(struct scsi_sense_data);
			status = CAM_AUTOSNS_VALID | CAM_SCSI_STATUS_ERROR;
		} else {
			status = CAM_AUTOSENSE_FAIL;
		}
	} else {
		switch (stat) {
		case ICMB_OK:
			if (ccb_h) {
				csio->resid = 0;
				csio->scsi_status = r->cmd.stat;
				status = CAM_REQ_CMP;
			}
			break;
		case ICMB_OKERR:
			if (ccb_h) {
				csio->scsi_status = r->cmd.stat;
				if (r->cmd.stat) {
					if (ccb_h->flags & CAM_DIS_AUTOSENSE)
						status = CAM_SCSI_STATUS_ERROR;
					else {
						if ( wds_runsense(wp, r) == CAM_REQ_CMP )
							return;
						/* in case of error continue with freeing of CCB */
					}
				} else {
					csio->resid = 0;
					status = CAM_REQ_CMP;
				}
			}
			break;
		case ICMB_ETIME:
			if (ccb_h)
				status = CAM_SEL_TIMEOUT;
			break;
		case ICMB_ERESET:
		case ICMB_ETARCMD:
		case ICMB_ERESEL:
		case ICMB_ESEL:
		case ICMB_EABORT:
		case ICMB_ESRESET:
		case ICMB_EHRESET:
			if (ccb_h)
				status = CAM_REQ_CMP_ERR;
			break;
		}

		if (ccb_h && (ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			/* we accept only virtual addresses in wds_action() */
			bcopy(r->buf, csio->data_ptr, csio->dxfer_len);
		}
	}

	r->flags |= WR_DONE;
	wp->dx->ombs[r->ombn].stat = 0;

	if (ccb_h) {
		wdsr_ccb_done(wp, r, r->ccb, status);
		smallog3('-', ccb_h->target_id + '0', ccb_h->target_lun + '0');
	} else {
		frag_free(wp, r->mask);
		if (wp->want_wdsr) {
			wp->want_wdsr = 0;
			xpt_release_simq(wp->sim, /* run queue */ 1);
		}
		wp->wdsr_free |= (1 << r->id);
	}

	DBG(DBX "wds%d: request %p done\n", wp->unit, r);
}

/* command returned bad status, request sense */

static int
wds_runsense(struct wds *wp, struct wds_req *r)
{
	u_int8_t          c;
	struct	ccb_hdr *ccb_h;

	ccb_h = &r->ccb->ccb_h;

	r->flags |= WR_SENSE;
	scsi_ulto3b(WDSTOPHYS(wp, &r->cmd),
	 wp->dx->ombs[r->ombn].addr);
	bzero(&r->cmd, sizeof r->cmd);
	r->cmd.cmd = WDSX_SCSICMD;
	r->cmd.targ = (ccb_h->target_id << 5) |
		ccb_h->target_lun;

	scsi_ulto3b(0, r->cmd.next);

	r->cmd.scb[0] = REQUEST_SENSE;
	r->cmd.scb[1] = ccb_h->target_lun << 5;
	r->cmd.scb[4] = sizeof(struct scsi_sense_data);
	r->cmd.scb[5] = 0;
	scsi_ulto3b(WDSTOPHYS(wp, r->buf), r->cmd.data);
	scsi_ulto3b(sizeof(struct scsi_sense_data), r->cmd.len);
	r->cmd.write = 0x80;

	outb(wp->addr + WDS_HCR, WDSH_IRQEN | WDSH_DRQEN);

	wp->dx->ombs[r->ombn].stat = 1;
	c = WDSC_MSTART(r->ombn);

	if (wds_cmd(wp->addr, &c, sizeof c) != 0) {
		device_printf(wp->dev, "unable to start outgoing sense mbox\n");
		wp->dx->ombs[r->ombn].stat = 0;
		wdsr_ccb_done(wp, r, r->ccb, CAM_AUTOSENSE_FAIL);
		return CAM_AUTOSENSE_FAIL;
	} else {
		DBG(DBX "wds%d: enqueued status cmd 0x%x, r=%p\n",
			wp->unit, r->cmd.scb[0] & 0xFF, r);
		/* don't free CCB yet */
		smallog3('*', ccb_h->target_id + '0',
			 ccb_h->target_lun + '0');
		return CAM_REQ_CMP;
	}
}

static int
wds_getvers(struct wds *wp)
{
	struct	 wds_req *r;
	int	 base;
	u_int8_t c;
	int	 i;

	base = wp->addr;

	r = wdsr_alloc(wp);
	if (!r) {
		device_printf(wp->dev, "no request slot available!\n");
		return (-1);
	}
	r->flags &= ~WR_DONE;

	r->ccb = NULL;

	scsi_ulto3b(WDSTOPHYS(wp, &r->cmd), wp->dx->ombs[r->ombn].addr);

	bzero(&r->cmd, sizeof r->cmd);
	r->cmd.cmd = WDSX_GETFIRMREV;

	outb(base + WDS_HCR, WDSH_DRQEN);

	c = WDSC_MSTART(r->ombn);
	if (wds_cmd(base, (u_int8_t *) & c, sizeof c)) {
		device_printf(wp->dev, "version request failed\n");
		wp->wdsr_free |= (1 << r->id);
		wp->dx->ombs[r->ombn].stat = 0;
		return (-1);
	}
	while (1) {
		i = 0;
		while ((inb(base + WDS_STAT) & WDS_IRQ) == 0) {
			DELAY(9000);
			if (++i == 100) {
				device_printf(wp->dev, "getvers timeout\n");
				return (-1);
			}
		}
		wds_intr(wp);
		if (r->flags & WR_DONE) {
			device_printf(wp->dev, "firmware version %d.%02d\n",
			       r->cmd.targ, r->cmd.scb[0]);
			wp->wdsr_free |= (1 << r->id);
			return (0);
		}
	}
}

static void
wdsr_ccb_done(struct wds *wp, struct wds_req *r,
	      union ccb *ccb, u_int32_t status)
{
	ccb->ccb_h.ccb_wdsr = 0;

	if (r != NULL) {
		/* To implement timeouts we would need to know how to abort the
		 * command on controller, and this is a great mystery.
		 * So for now we just pass the responsibility for timeouts
		 * to the controlles itself, it does that reasonably good.
		 */
		/* untimeout(_timeout, (caddr_t) hcb, ccb->ccb_h.timeout_ch); */
		/* we're about to free a hcb, so the shortage has ended */
		frag_free(wp, r->mask);
		if (wp->want_wdsr && status != CAM_REQUEUE_REQ) {
			wp->want_wdsr = 0;
			status |= CAM_RELEASE_SIMQ;
			smallog('R');
		}
		wp->wdsr_free |= (1 << r->id);
	}
	ccb->ccb_h.status =
	    status | (ccb->ccb_h.status & ~(CAM_STATUS_MASK | CAM_SIM_QUEUED));
	xpt_done(ccb);
}

static void
wds_scsi_io(struct cam_sim * sim, struct ccb_scsiio * csio)
{
	int	 unit = cam_sim_unit(sim);
	struct	 wds *wp;
	struct	 ccb_hdr *ccb_h;
	struct	 wds_req *r;
	int	 base;
	u_int8_t c;
	int	 error;
	int	 n;

	wp = (struct wds *)cam_sim_softc(sim);
	ccb_h = &csio->ccb_h;

	DBG(DBX "wds%d: cmd TARG=%d LUN=%d\n", unit, ccb_h->target_id,
	    ccb_h->target_lun);

	if (ccb_h->target_id > 7 || ccb_h->target_id == WDS_HBA_ID) {
		ccb_h->status = CAM_TID_INVALID;
		xpt_done((union ccb *) csio);
		return;
	}
	if (ccb_h->target_lun > 7) {
		ccb_h->status = CAM_LUN_INVALID;
		xpt_done((union ccb *) csio);
		return;
	}
	if (csio->dxfer_len > BUFSIZ) {
		ccb_h->status = CAM_REQ_TOO_BIG;
		xpt_done((union ccb *) csio);
		return;
	}
	if (ccb_h->flags & (CAM_CDB_PHYS | CAM_SCATTER_VALID | CAM_DATA_PHYS)) {
		/* don't support these */
		ccb_h->status = CAM_REQ_INVALID;
		xpt_done((union ccb *) csio);
		return;
	}
	base = wp->addr;

	/*
	 * this check is mostly for debugging purposes,
	 * "can't happen" normally.
	 */
	if(wp->want_wdsr) {
		DBG(DBX "wds%d: someone already waits for buffer\n", unit);
		smallog('b');
		n = xpt_freeze_simq(sim, /* count */ 1);
		smallog('0'+n);
		ccb_h->status = CAM_REQUEUE_REQ;
		xpt_done((union ccb *) csio);
		return;
	}

	r = wdsr_alloc(wp);
	if (r == NULL) {
		device_printf(wp->dev, "no request slot available!\n");
		wp->want_wdsr = 1;
		n = xpt_freeze_simq(sim, /* count */ 1);
		smallog2('f', '0'+n);
		ccb_h->status = CAM_REQUEUE_REQ;
		xpt_done((union ccb *) csio);
		return;
	}

	ccb_h->ccb_wdsr = (void *) r;
	r->ccb = (union ccb *) csio;

	switch (error = frag_alloc(wp, csio->dxfer_len, &r->buf, &r->mask)) {
	case CAM_REQ_CMP:
		break;
	case CAM_REQUEUE_REQ:
		DBG(DBX "wds%d: no data buffer available\n", unit);
		wp->want_wdsr = 1;
		n = xpt_freeze_simq(sim, /* count */ 1);
		smallog2('f', '0'+n);
		wdsr_ccb_done(wp, r, r->ccb, CAM_REQUEUE_REQ);
		return;
	default:
		DBG(DBX "wds%d: request is too big\n", unit);
		wdsr_ccb_done(wp, r, r->ccb, error);
		break;
	}

	ccb_h->status |= CAM_SIM_QUEUED;
	r->flags &= ~WR_DONE;

	scsi_ulto3b(WDSTOPHYS(wp, &r->cmd), wp->dx->ombs[r->ombn].addr);

	bzero(&r->cmd, sizeof r->cmd);
	r->cmd.cmd = WDSX_SCSICMD;
	r->cmd.targ = (ccb_h->target_id << 5) | ccb_h->target_lun;

	if (ccb_h->flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, &r->cmd.scb,
		      csio->cdb_len < 12 ? csio->cdb_len : 12);
	else
		bcopy(csio->cdb_io.cdb_bytes, &r->cmd.scb,
		      csio->cdb_len < 12 ? csio->cdb_len : 12);

	scsi_ulto3b(csio->dxfer_len, r->cmd.len);

	if (csio->dxfer_len > 0
	 && (ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
		/* we already rejected physical or scattered addresses */
		bcopy(csio->data_ptr, r->buf, csio->dxfer_len);
	}
	scsi_ulto3b(csio->dxfer_len ? WDSTOPHYS(wp, r->buf) : 0, r->cmd.data);

	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN)
		r->cmd.write = 0x80;
	else
		r->cmd.write = 0x00;

	scsi_ulto3b(0, r->cmd.next);

	outb(base + WDS_HCR, WDSH_IRQEN | WDSH_DRQEN);

	c = WDSC_MSTART(r->ombn);

	if (wds_cmd(base, &c, sizeof c) != 0) {
		device_printf(wp->dev, "unable to start outgoing mbox\n");
		wp->dx->ombs[r->ombn].stat = 0;
		wdsr_ccb_done(wp, r, r->ccb, CAM_RESRC_UNAVAIL);
		return;
	}
	DBG(DBX "wds%d: enqueued cmd 0x%x, r=%p\n", unit,
	    r->cmd.scb[0] & 0xFF, r);

	smallog3('+', ccb_h->target_id + '0', ccb_h->target_lun + '0');
}

static void
wds_action(struct cam_sim * sim, union ccb * ccb)
{
	int	unit = cam_sim_unit(sim);
	int	s;

	DBG(DBX "wds%d: action 0x%x\n", unit, ccb->ccb_h.func_code);
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
		s = splcam();
		DBG(DBX "wds%d: SCSI IO entered\n", unit);
		wds_scsi_io(sim, &ccb->csio);
		DBG(DBX "wds%d: SCSI IO returned\n", unit);
		splx(s);
		break;
	case XPT_RESET_BUS:
		/* how to do it right ? */
		printf("wds%d: reset\n", unit);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_ABORT:
		ccb->ccb_h.status = CAM_UA_ABORT;
		xpt_done(ccb);
		break;
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

		ccg->heads = 64;
		ccg->secs_per_track = 16;
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:	/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;	/* XXX??? */
		cpi->hba_inquiry = 0;	/* nothing fancy */
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7;
		cpi->max_lun = 7;
		cpi->initiator_id = WDS_HBA_ID;
		cpi->hba_misc = 0;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "WD/FDC", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
wds_poll(struct cam_sim * sim)
{
	wds_intr((struct wds *)cam_sim_softc(sim));
}

/* part of initialization done in probe() */
/* returns 0 if OK, ENXIO if bad */

static int
wds_preinit(struct wds *wp)
{
	int	base;
	int	i;

	base = wp->addr;

	/*
	 * Sending a command causes the CMDRDY bit to clear.
	 */
	outb(base + WDS_CMD, WDSC_NOOP);
	if (inb(base + WDS_STAT) & WDS_RDY)
		return (ENXIO);

	/*
	 * the controller exists. reset and init.
	 */
	outb(base + WDS_HCR, WDSH_ASCRESET | WDSH_SCSIRESET);
	DELAY(30);
	outb(base + WDS_HCR, 0);

	if ((inb(base + WDS_STAT) & (WDS_RDY)) != WDS_RDY) {
		for (i = 0; i < 10; i++) {
			if ((inb(base + WDS_STAT) & (WDS_RDY)) == WDS_RDY)
				break;
			DELAY(40000);
		}
		if ((inb(base + WDS_STAT) & (WDS_RDY)) != WDS_RDY)
			/* probe timeout */
			return (ENXIO);
	}

	return (0);
}

/* part of initialization done in attach() */
/* returns 0 if OK, 1 if bad */

static int
wds_init(struct wds *wp)
{
	struct	wds_setup init;
	int	base;
	int	i;
	struct	wds_cmd  wc;

	base = wp->addr;

	outb(base + WDS_HCR, WDSH_DRQEN);

	isa_dmacascade(wp->drq);

	if ((inb(base + WDS_STAT) & (WDS_RDY)) != WDS_RDY) {
		for (i = 0; i < 10; i++) {
			if ((inb(base + WDS_STAT) & (WDS_RDY)) == WDS_RDY)
				break;
			DELAY(40000);
		}
		if ((inb(base + WDS_STAT) & (WDS_RDY)) != WDS_RDY)
			/* probe timeout */
			return (1);
	}
	bzero(&init, sizeof init);
	init.cmd = WDSC_INIT;
	init.scsi_id = WDS_HBA_ID;
	init.buson_t = 24;
	init.busoff_t = 48;
	scsi_ulto3b(WDSTOPHYS(wp, &wp->dx->ombs), init.mbaddr); 
	init.xx = 0;
	init.nomb = WDS_NOMB;
	init.nimb = WDS_NIMB;

	wds_wait(base + WDS_STAT, WDS_RDY, WDS_RDY);
	if (wds_cmd(base, (u_int8_t *) & init, sizeof init) != 0) {
		device_printf(wp->dev, "wds_cmd init failed\n");
		return (1);
	}
	wds_wait(base + WDS_STAT, WDS_INIT, WDS_INIT);

	wds_wait(base + WDS_STAT, WDS_RDY, WDS_RDY);

	bzero(&wc, sizeof wc);
	wc.cmd = WDSC_DISUNSOL;
	if (wds_cmd(base, (char *) &wc, sizeof wc) != 0) {
		device_printf(wp->dev, "wds_cmd init2 failed\n");
		return (1);
	}
	return (0);
}

static int
wds_cmd(int base, u_int8_t * p, int l)
{
	int	s = splcam();

	while (l--) {
		do {
			outb(base + WDS_CMD, *p);
			wds_wait(base + WDS_STAT, WDS_RDY, WDS_RDY);
		} while (inb(base + WDS_STAT) & WDS_REJ);
		p++;
	}

	wds_wait(base + WDS_STAT, WDS_RDY, WDS_RDY);

	splx(s);

	return (0);
}

static void
wds_wait(int reg, int mask, int val)
{
	while ((inb(reg) & mask) != val)
		;
}

static struct wds_req *
cmdtovirt(struct wds *wp, u_int32_t phys)
{
	char	*a;

	a = WDSTOVIRT(wp, (uintptr_t)phys);
	if( a < (char *)&wp->dx->req[0] || a>= (char *)&wp->dx->req[MAXSIMUL]) {
		device_printf(wp->dev, "weird phys address 0x%x\n", phys);
		return (NULL);
	}
	a -= (int)offsetof(struct wds_req, cmd); /* convert cmd to request */
	return ((struct wds_req *)a);
}

/* for debugging, print out all the data about the status of devices */
void
wds_print(void)
{
	int	unit;
	int	i;
	struct	wds_req *r;
	struct	wds     *wp;

	for (unit = 0; unit < devclass_get_maxunit(wds_devclass); unit++) {
		wp = (struct wds *) devclass_get_device(wds_devclass, unit);
		if (wp == NULL)
			continue;
		printf("wds%d: want_wdsr=0x%x stat=0x%x irq=%s irqstat=0x%x\n",
		       unit, wp->want_wdsr, inb(wp->addr + WDS_STAT) & 0xff,
		       (inb(wp->addr + WDS_STAT) & WDS_IRQ) ? "ready" : "no",
		       inb(wp->addr + WDS_IRQSTAT) & 0xff);
		for (i = 0; i < MAXSIMUL; i++) {
			r = &wp->dx->req[i];
			if( wp->wdsr_free & (1 << r->id) ) {
				printf("req=%d flg=0x%x ombn=%d ombstat=%d "
				       "mask=0x%x targ=%d lun=%d cmd=0x%x\n",
				       i, r->flags, r->ombn,
				       wp->dx->ombs[r->ombn].stat,
				       r->mask, r->cmd.targ >> 5,
				       r->cmd.targ & 7, r->cmd.scb[0]);
			}
		}
	}
}

#if WDS_DEBUG == 2
/* create circular log buffer */
static char    *
wds_nextlog(void)
{
	int	n = logwrite;

	if (++logwrite >= NLOGLINES)
		logwrite = 0;
	if (logread == logwrite)
		if (++logread >= NLOGLINES)
			logread = 0;
	return (wds_log[n]);
}

void
wds_printlog(void)
{
	/* print the circular buffer */
	int	i;

	for (i = logread; i != logwrite;) {
		printf("%s", wds_log[i]);
		if (i == NLOGLINES)
			i = 0;
		else
			i++;
	}
}
#endif /* WDS_DEBUG */
