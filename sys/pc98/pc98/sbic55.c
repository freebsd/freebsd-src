/*
 * Julian SCSI driver for PC-9801 based on aha1542.c
 *
 * Copyright (c) by Yoshio Kimura
 * 05/14/1994
 */

#include <sys/types.h>
#include "sbic.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <machine/clock.h>
#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <pc98/pc98/pc98.h>
#include <i386/isa/isa_device.h>
#include <pc98/pc98/icu.h>
#include <pc98/pc98/ic/i8237.h>
#include <pc98/pc98/scsireg.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/devconf.h>

#include <sys/kernel.h>

/************************** board definitions *******************************/

/*
 * I/O Port Interface
 */

#define SBIC_BASE	sbic->sbic_base
#define SBIC_AUX_REG	(SBIC_BASE + 0)		/* auxiliary status(R) */
#define SBIC_ADR_REG	(SBIC_BASE + 0)		/* address(W) */
#define SBIC_CTL_REG	(SBIC_BASE + 2)		/* control(R/W) */
#define SBIC_STA_REG	(SBIC_BASE + 4)		/* status(R/W) */

/*
 * Register Access Interface
 */

#define SBIC_asr(val)	(val) = inb(SBIC_AUX_REG)
#define GET_SBIC_REG(regno, val) { \
		outb(SBIC_ADR_REG, (regno)); \
		(val) = inb(SBIC_CTL_REG); \
	}
#define SET_SBIC_REG(regno, val) { \
		outb(SBIC_ADR_REG, (regno)); \
		outb(SBIC_CTL_REG, (val)); \
	}
#define SET_SCSI_CMD(cmd, cmdlen) { \
		int n; \
		u_char *cmds = (u_char *)(cmd); \
		SET_SBIC_REG(SBIC_cdbsize, (cmdlen)); \
		for (n = 0; n < (cmdlen); n++) { \
			SET_SBIC_REG(SBIC_cdb1 + n, cmds[n]); \
		} \
	}
#define SET_XFER_LEN(val) { \
		SET_SBIC_REG(SBIC_count_hi, ((val) & 0xff0000) >> 16); \
		SET_SBIC_REG(SBIC_count_med, ((val) & 0x00ff00) >> 8); \
		SET_SBIC_REG(SBIC_count_lo, (val) & 0x0000ff); \
	}
#define SBIC_ENABLE_INT() { \
		int tmp; \
		GET_SBIC_REG(SBIC_bank, tmp); \
		SET_SBIC_REG(SBIC_bank, tmp | 0x04); \
	}
#define SBIC_DISABLE_INT() { \
		int tmp; \
		GET_SBIC_REG(SBIC_bank, tmp); \
		SET_SBIC_REG(SBIC_bank, tmp & 0xfb); \
	}
#define SBIC_DMA_ENABLE() outb(SBIC_STA_REG, 1)

#define INT3	0
#define INT5	1
#define INT6	2
#define INT9	3
#define INT12	4
#define INT13	5

#define SBIC_NSEG	17
#define SBIC_ID		7
#define MAXSIMUL	8
#define SBIC_RESET_TIMEOUT	2000	/* time to wait for reset */

extern short dmapageport[];


struct sbic_ccb {
	u_char	target;
	u_char	lun;
	u_char	status;
	u_char	message;
	int	datalen;
	int	sense_addr;
	int	sense_len;
	u_char	scsi_cmdlen;
	struct scsi_generic scsi_cmd;
	struct scsi_xfer *xfer;
	struct scat_gather {
		int	seg_addr;
		int	seg_len;
	} scat_gath[SBIC_NSEG];
	int	seg;
	int	use_seg;
	int	xs_flags;
	int	flags;
#define CCB_FREE	0x00
#define CCB_ACTIVE	0x01
#define CCB_ABORT	0x40
#define CCB_SENSE	0x80
#define CCB_BOUNCE 0x100
};

struct sbic_config {
	u_char	chan;
	u_char	intr;
	u_char	scsi_dev:3;
	u_char :5;
};
/*********************************** end of board definitions***************/

#define KVTOPHYS(x)	vtophys(x)
#define SBIC_DMA_PAGES	SBIC_NSEG

#define PAGESIZ		4096
#define ALLWAYS_BOUNCE

#ifdef ALLWAYS_BOUNCE
static vm_offset_t sbic_bounce;
#endif

#ifdef SBICDEBUG
static int sbic_debug = 1;
#endif

struct sbic_data {
	short	sbic_base;		/* base port for each board */
	struct sbic_ccb sbic_ccb[MAXSIMUL];
	int	top;			/* ccb queue top */
	int	bottom;			/* ccb queue end */
	int	active;			/* number of active ccb */
	int	free;			/* number of free ccb */
	int	unit;
	int	sbic_int;		/* our irq level */
	int	sbic_dma;		/* our DMA req channel */
	int	sbic_scsi_dev;		/* our scsi bus address */
	struct scsi_link sc_link;	/* prototype for subdevs */
};

struct sbic_data *sbicdata[NSBIC];;

static struct sbic_ccb *sbic_get_ccb(struct sbic_data *, int);
static int	sbicprobe(struct isa_device *);
static void	sbic_done(struct sbic_data *, struct sbic_ccb *);
static int	sbicattach(struct isa_device *);
static int32_t	sbic_scsi_cmd(struct scsi_xfer *xs);
static u_int32_t sbic_adapter_info(int);
static void sbicminphys(struct buf *);
static void sbic_free_ccb(struct sbic_data *, struct sbic_ccb *, int);
static int sbic_init(int);
static int xfer_addr_check(int, struct sbic_ccb *);
static int sbic_poll(struct sbic_data *, struct sbic_ccb *);
static void start_scsi(struct sbic_data *, struct sbic_ccb *);
static void dataphase(struct sbic_data *, struct sbic_ccb *);
static void sbic_request_sense(struct sbic_data *, struct sbic_ccb *);
static void sbic_dmastart(int, int, unsigned, unsigned);
static void sbic_dmadone(int, int, unsigned, unsigned);

static struct scsi_adapter sbic_switch = {
	sbic_scsi_cmd,
	sbicminphys,
	0,
	0,
	sbic_adapter_info,
	"sbic",
	{ 0, 0 }
};

/* the below structure is so we have a default dev struct for out link struct */
static struct scsi_device sbic_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"sbic",
	0,
	{ 0, 0 }
};

struct isa_driver sbicdriver = {
	sbicprobe,
	sbicattach,
	"sbic"
};

static struct kern_devconf kdc_sbic[NSBIC] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"sbic", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start out here in probe */
	"55 compatible SCSI board host adapter",
	DC_CLS_MISC		/* host adapters aren't special */
} };

static inline void
sbic_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_sbic[id->id_unit] = kdc_sbic[0];
	kdc_sbic[id->id_unit].kdc_unit = id->id_unit;
	kdc_sbic[id->id_unit].kdc_parentdata = id;
	dev_attach(&kdc_sbic[id->id_unit]);
}


static int sbicunit = 0;


/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
sbicprobe(struct isa_device *dev)
{
	int unit = sbicunit;
	struct sbic_data *sbic;
	
	/*
	 * find unit and check we have that many defined
	 */
	if (unit >= NSBIC) {
		printf("sbic: unit number (%d) to high\n", unit);
		return(0);
	}
	dev->id_unit = unit;

	/*
	 * a quick safety check so we can be sleazy later
	 */
	if (sizeof(struct sbic_data) > PAGESIZ) {
		printf("sbic struct > pagesize\n");
		return(0);
	}
	/*
	 * Allocate a storage area for us
	 */
	if (sbicdata[unit]) {
		printf("sbic%d: memory already allocated\n", unit);
		return(0);
	}
	sbic = malloc(sizeof(struct sbic_data), M_TEMP, M_NOWAIT);
	if (!sbic) {
		printf("sbic%d: cannot malloc!\n", unit);
		return(0);
	}
	bzero(sbic, sizeof(struct sbic_data));
	sbicdata[unit] = sbic;
	sbic->sbic_base = dev->id_iobase;

#ifdef ALLWAYS_BOUNCE
	/*
	 * allocate bounce buffer for sense data
	 */
#ifdef EPSON_BOUNCEDMA
#define PC98_RAMEND		0xf00000
#else
#define PC98_RAMEND		RAM_END
#endif

	/* try malloc() first. */
	sbic_bounce = (vm_offset_t)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (sbic_bounce != NULL) {
		if (vtophys(sbic_bounce) >= PC98_RAMEND) {
			free(buf, M_DEVBUF);
			sbic_bounce = (vm_offset_t)contigmalloc(PAGE_SIZE, M_DEVBUF,
													M_NOWAIT,
													0ul, PC98_RAMEND, 1ul,
													0x10000ul);
		}
	}
	if (sbic_bounce == NULL)
		panic("Can't allocate bounce buffer.");
#endif

#ifndef DEV_LKM
	sbic_registerdev(dev);
#endif

	/*
	 * Try initialize a unit at this location
	 * sets up dma, loads sbic_init[unit]
	 */
	if (sbic_init(unit) != 0) {
		sbicdata[unit] = NULL;
		free(sbic, M_TEMP);
		return(0);
	}
	/*
	 * If it's there, put in it's interrupt vectors
	 */
	dev->id_irq = (1 << sbic->sbic_int);
	dev->id_drq = sbic->sbic_dma;
	sbicunit++;
	return(5);
}

/*
 * Attach all the sub-devices we can find.
 */
static int
sbicattach(struct isa_device *dev)
{
	int unit = dev->id_unit;
	struct sbic_data *sbic = sbicdata[unit];
	struct scsibus_data *scbus;

	/*
	 * fill in the prototype scsi_link
	 */
	sbic->sc_link.adapter_unit = unit;
	sbic->sc_link.adapter_targ = sbic->sbic_scsi_dev;
	sbic->sc_link.adapter_softc = sbic;
	sbic->sc_link.adapter = &sbic_switch;
	sbic->sc_link.device = &sbic_dev;
	sbic->sc_link.flags = SDEV_BOUNCE;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return 0;
	scbus->adapter_link = &sbic->sc_link;

	/*
	 * ask the adapter what subunits are present
	 */
	kdc_sbic[unit].kdc_state = DC_BUSY; /* host adapters are always busy */
	scsi_attachdevs(scbus);
	
	return 1;
}

/*
 * Return some information to the caller about
 * the adapter and it's capabilities
 */
static u_int32_t
sbic_adapter_info(int unit)
{
	return(1);	/* 1 outstanding request at a time per device */
}

/*
 * Catch an interrupt from the adapter
 */
void
sbicintr(int unit)
{
	struct sbic_ccb *ccb;
	struct sbic_data *sbic = sbicdata[unit];
	u_char asr, host_stat, cmd_phase;
	u_char *cmd;
	int i, seg, phys;
	
#ifdef	SBICDEBUG
	printf("sbicintr");
#endif	/* SBICDEBUG */

	SBIC_asr(asr);
	/* drop spurious interrupts */
	if (!(asr & SBIC_ASR_INT))
		return;
	GET_SBIC_REG(SBIC_csr, host_stat);
	GET_SBIC_REG(SBIC_cmd_phase, cmd_phase);
	
	ccb = &sbic->sbic_ccb[sbic->top];
	seg = ccb->seg;
	
	switch(host_stat) {
	case SBIC_CSR_XFERRED | SBIC_MCI_DATA_OUT:	/* data phase start */
	case SBIC_CSR_MIS_1 | SBIC_MCI_DATA_OUT:	/* data phase continue */
	case SBIC_CSR_XFERRED | SBIC_MCI_DATA_IN:
	case SBIC_CSR_MIS_1 | SBIC_MCI_DATA_IN:
		dataphase(sbic, ccb);
		return;
	case SBIC_CSR_MIS_1 | SBIC_MCI_STATUS:	/* status phase start */
	case SBIC_CSR_XFERRED | SBIC_MCI_STATUS:	/* status phase start */
		SET_XFER_LEN(0);
		SET_SBIC_REG(SBIC_cmd_phase, 0x46);
		SET_SBIC_REG(SBIC_cmd, SBIC_CMD_SEL_XFER);
		return;
	case SBIC_CSR_S_XFERRED:
		phys = KVTOPHYS(ccb->sense_addr);
		if (ccb->flags & CCB_SENSE) {
			sbic_dmadone(B_READ, phys,
					ccb->sense_len, sbic->sbic_dma);
		} else {
			if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_IN)) {
				sbic_dmadone(B_READ,
					     ccb->scat_gath[seg - 1].seg_addr,
					     ccb->scat_gath[seg - 1].seg_len,
					     sbic->sbic_dma);
			}
			if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_OUT)) {
				sbic_dmadone(B_WRITE,
					     ccb->scat_gath[seg - 1].seg_addr,
					     ccb->scat_gath[seg - 1].seg_len,
					     sbic->sbic_dma);
			}
		}
		GET_SBIC_REG(SBIC_tlun, ccb->status);
		ccb->status &= 0x1f;
		switch(ccb->status) {
		case SCSI_CHECK:
			ccb->flags |= CCB_SENSE;
			ccb->xfer->error = XS_SENSE;
			sbic_request_sense(sbic, ccb);
			return;
		case SCSI_BUSY:
			ccb->xfer->error = XS_BUSY;
			break;
		default:
			break;
		}
		sbic_done(sbic, ccb);
		break;

	case SBIC_CSR_SEL_TIMEO:	/* selection timeout */
		ccb->xfer->error = XS_TIMEOUT;
		sbic_done(sbic, ccb);
		break;

	default:
		printf("sbic%d:%d:%d -- ", unit, ccb->target, ccb->lun);
		printf("host: %x interrupt occured\n", host_stat);
		panic("Unsupported interrupts. check intr code\n");
		break;
	}
	return;
}

/*
 * A ccb is put onto the free list.
 */
static void
sbic_free_ccb(struct sbic_data *sbic, struct sbic_ccb *ccb, int flags)
{
	unsigned opri = 0;
	
	if (!(flags & SCSI_NOMASK))
		opri = splbio();
	if (ccb = &sbic->sbic_ccb[sbic->top])
		sbic->top = ++sbic->top % MAXSIMUL;
	else
		sbic->bottom = (--sbic->bottom + MAXSIMUL) % MAXSIMUL;
	sbic->active--;
	sbic->free++;
	ccb->flags = CCB_FREE;
	if (sbic->free)
		wakeup((caddr_t)&sbic->free);
	if (!(flags & SCSI_NOMASK))
		splx(opri);
}

/*
 * Get a free ccb
 */
static struct sbic_ccb *
sbic_get_ccb(struct sbic_data *sbic, int flags)
{
	unsigned opri = 0;
	struct sbic_ccb *rc;
	
	if (!(flags & SCSI_NOMASK))
		opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 */
	while((!sbic->free) && (!(flags & SCSI_NOSLEEP)))
		tsleep((caddr_t)&sbic->free, PRIBIO, "sbicccb", 0);
	if (sbic->free) {
		rc = &sbic->sbic_ccb[sbic->bottom];
		sbic->free--;
		sbic->active++;
		sbic->bottom = ++sbic->bottom % MAXSIMUL;
		rc->flags = CCB_ACTIVE;
	} else
		rc = (struct sbic_ccb *)0;
	if (!(flags & SCSI_NOMASK))
		splx(opri);
	return(rc);
}

/*
 * We have a ccb which has been processed by the
 * adapter, now we lock to see how the operation
 * went. Wake up the owner if waiting
 */
static void
sbic_done(struct sbic_data *sbic, struct sbic_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->xfer;
	unsigned opri;
	
	SC_DEBUG(xs->sc_link, SDEV_DB2, ("sbic_done\n"));
	if (!(xs->flags & INUSE)) {
		printf("sbic%d: exiting but not in use!\n", sbic->unit);
		Debugger("sbic55");
	}
	if ((!xs->error) || (xs->flags & SCSI_ERR_OK))
		xs->resid = 0;
	
	xs->flags |= ITSDONE;
#ifdef ALLWAYS_BOUNCE
	bcopy((char *)ccb->sense_addr, (char *)&xs->sense, ccb->sense_len);
#else
	if(ccb->flags & CCB_BOUNCE)
		bcopy((char *)ccb->sense_addr,
			(char *)&xs->sense, ccb->sense_len);
#endif
	sbic_free_ccb(sbic, ccb, xs->flags);
	if(sbic->active) {
		opri = splbio();
		SBIC_ENABLE_INT();
		start_scsi(sbic, &sbic->sbic_ccb[sbic->top]);
		splx(opri);
	}
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
static int
sbic_init(int unit)
{
	struct sbic_data *sbic = sbicdata[unit];
	struct sbic_config conf;
	int i, asr, csr, tmp;
	
	SBIC_asr(asr);		/* dummy read */
	GET_SBIC_REG(SBIC_csr, csr);
	SBIC_DISABLE_INT();
	
	SET_SBIC_REG(SBIC_myid, SBIC_ID_FS_16_20 | SBIC_ID);
	SET_SBIC_REG(SBIC_cmd, SBIC_CMD_RESET);
	for (i = SBIC_RESET_TIMEOUT; i; i--) {
		SBIC_asr(asr);
		if ((asr != 0xff) && (asr & SBIC_ASR_INT)) {
			GET_SBIC_REG(SBIC_csr, csr);
			if (csr == SBIC_CSR_RESET)
				break;
		}
		DELAY(1);		/* calibrated in msec */
	}
	if (i == 0) {
#ifdef	SBICDEBUG
		if (sbic_debug)
			printf("sbic_init: No answer from sbic board\n");
#endif
		return(ENXIO);
	}
	
	conf.chan = (inb(SBIC_STA_REG) & 0x03);
	GET_SBIC_REG(SBIC_int, tmp);
	conf.intr = (tmp >> 3) & 0x07;
	conf.scsi_dev = tmp & 0x07;
	sbic->sbic_dma = conf.chan;
	sbic->sbic_scsi_dev = conf.scsi_dev;
	switch(conf.intr) {
	case INT3:
		sbic->sbic_int = 3;
		break;
	case INT5:
		sbic->sbic_int = 5;
		break;
	case INT6:
		sbic->sbic_int = 6;
		break;
	case INT9:
		sbic->sbic_int = 9;
		break;
	case INT12:
		sbic->sbic_int = 12;
		break;
	case INT13:
		sbic->sbic_int = 13;
		break;
	default:
		printf("illegal int jumpter setting\n");
		return(EIO);
	}
	
	SET_SBIC_REG(SBIC_rselid, 0);
#if 1
	SET_SBIC_REG(SBIC_timeo, 0x80);
#else
	SET_SBIC_REG(SBIC_timeo, 0xa0);
#endif
	SET_SBIC_REG(SBIC_control, SBIC_CTL_EDI);
	SET_SBIC_REG(SBIC_syn, 0);
	SBIC_ENABLE_INT();
	/*
	 * ccb queue initialize
	 */
	for (i = 0; i < MAXSIMUL; i++) {
		sbic->sbic_ccb[i].flags = CCB_FREE;
	}
	sbic->top = sbic->bottom = sbic->active = 0;
	sbic->free = MAXSIMUL;
	/*
	 * Note that wa are going and return (to probe)
	 */
	return(0);
}

static void
sbicminphys(struct buf *bp)
{
/*	sbic seems to explode with 17 segs (64k may require 17 segs) */
	if (bp->b_bcount > ((SBIC_NSEG - 1) *PAGESIZ))
		bp->b_bcount = ((SBIC_NSEG - 1) * PAGESIZ);
}

/*
 * start a scsi operation given the command and
 * the data address. Also needs the unit, target
 * and lu
 */
static int32_t
sbic_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;
	int	unit = sc_link->adapter_unit;
	struct sbic_data *sbic = sc_link->adapter_softc;
	struct sbic_ccb *ccb;
	int s, i, retval, flags;
	
	SC_DEBUG(xs->sc_link, SDEV_DB2, ("sbic_scsi_cmd\n"));
	flags = xs->flags;
	if (!(ccb = sbic_get_ccb(sbic, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}
	ccb->xfer = xs;
	ccb->target = xs->sc_link->target;
	ccb->lun = xs->sc_link->lun;
	ccb->xs_flags = xs->flags;
	ccb->scsi_cmdlen = xs->cmdlen;
	ccb->datalen = xs->datalen;
	ccb->sense_len = sizeof(xs->sense);
#ifdef ALLWAYS_BOUNCE
	ccb->sense_addr = (int)(sbic_bounce);
#else
	ccb->sense_addr = (int)&xs->sense;
#endif
	ccb->seg = 0;
	
	if (retval = xfer_addr_check(unit, ccb)) {
		xs->error = XS_DRIVER_STUFFUP;
		sbic_free_ccb(sbic, ccb, flags);
		return(retval);
	}
	if (!(flags & SCSI_RESET)) {
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmdlen);
		ccb->scsi_cmd.bytes[0] |= ((ccb->lun << 5) & 0xe0);
	}
	if (!(flags & SCSI_NOMASK)) {
		if (sbic->active == 1) {
			s = splbio();
			SBIC_ENABLE_INT();
			start_scsi(sbic, ccb);
			splx(s);
		}
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("sent\n"));
		return(SUCCESSFULLY_QUEUED);
	}
	SBIC_DISABLE_INT();
	start_scsi(sbic, ccb);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd sent, waiting\n"));
	return(sbic_poll(sbic, ccb));
}

static int
sbic_poll(struct sbic_data *sbic, struct sbic_ccb *ccb)
{
	int count, asr, done = 0;

	count = ccb->xfer->timeout * 1000;
	while(count) {
		SBIC_asr(asr);
		if ((asr != 0xff) && (asr & SBIC_ASR_INT)) {
			sbicintr(sbic->unit);
		}
		if (ccb->xfer->flags & ITSDONE) {
			break;
		}
		DELAY(1);
		count--;
	}
	if (!count) panic("sbic scsi timeout!!");
	if (ccb->xfer->error) return(HAD_ERROR);
	return(COMPLETE);
}

static int
xfer_addr_check(int unit, struct sbic_ccb *ccb)
{
	struct iovec *iovp;
	struct scat_gather *sg;
	struct scsi_xfer *xs;
	int thiskv, thisphys, nextphys;
	int bytes_this_page, bytes_this_seg;
	int datalen, flags;
	
	xs = ccb->xfer;
	flags = ccb->xs_flags;
	if ((xs->datalen) && (!(flags & SCSI_RESET))) {
		sg = ccb->scat_gath;
		ccb->use_seg = 0;
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			while((datalen) && (ccb->use_seg < SBIC_NSEG)) {
				sg->seg_addr = KVTOPHYS((int)iovp->iov_base);
				sg->seg_len = iovp->iov_len;
				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("UIO(0x%x@0x%x)"
					,iovp->iov_len
					,iovp->iov_base));
				sg++;
				iovp++;
				ccb->use_seg++;
				datalen++;
			}
		} else {
			/*
			 * Set up scatter gather block
			 */
			SC_DEBUG(xs->sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int)xs->data;
			thisphys = KVTOPHYS(thiskv);
			while((datalen) && (ccb->use_seg < SBIC_NSEG)) {
				bytes_this_seg = 0;
				sg->seg_addr = thisphys;
				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("0x%x", thisphys));
				nextphys = thisphys;
				while((datalen) && (thisphys == nextphys)) {
					if (thisphys > 0xFFFFFF) {
						printf("sbic%d: DMA beyond"
							" end of PC98\n", unit);
						xs->error = XS_DRIVER_STUFFUP;
						return(HAD_ERROR);
					}
					nextphys = (thisphys & (~(PAGESIZ - 1)))
							+ PAGESIZ;
					bytes_this_page = nextphys - thisphys;
					bytes_this_page = min(bytes_this_page,
								datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;
					thiskv = (thiskv & (~(PAGESIZ - 1)))
							+ PAGESIZ;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				sg->seg_len = bytes_this_seg;
				sg++;
				ccb->use_seg++;
			}
		}
		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			printf("sbic%d: sbic_scsi_cmd, more than %d DMA segs\n",
						unit, SBIC_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			return(HAD_ERROR);
		}
	} else {
		ccb->scat_gath[0].seg_len = 0;
	}
	return(0);
}

static void
start_scsi(struct sbic_data *sbic, struct sbic_ccb *ccb)
{
	if (ccb->xs_flags & SCSI_RESET) {
		SET_SBIC_REG(SBIC_cmd, SBIC_CMD_RESET);
		return;
	}
	
	if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_IN))
		sbic_dmastart(B_READ, ccb->scat_gath[0].seg_addr,
				ccb->scat_gath[0].seg_len, sbic->sbic_dma);
	if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_OUT))
		sbic_dmastart(B_WRITE, ccb->scat_gath[0].seg_addr,
				ccb->scat_gath[0].seg_len, sbic->sbic_dma);
	SET_SCSI_CMD(&ccb->scsi_cmd, ccb->scsi_cmdlen);
	SET_XFER_LEN(ccb->scat_gath[0].seg_len);
	SET_SBIC_REG(SBIC_selid, ccb->target);
	SET_SBIC_REG(SBIC_tlun, ccb->lun);
	SBIC_DMA_ENABLE();
	SET_SBIC_REG(SBIC_control, SBIC_CTL_DMA | SBIC_CTL_EDI);
	SET_SBIC_REG(SBIC_cmd, SBIC_CMD_SEL_XFER);
	ccb->seg++;
	return;
}

static void
dataphase(struct sbic_data *sbic, struct sbic_ccb *ccb)
{
	int seg = ccb->seg, tmp;
	
	if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_IN)) {
		sbic_dmadone(B_READ, ccb->scat_gath[seg - 1].seg_addr,
			     ccb->scat_gath[seg - 1].seg_len, sbic->sbic_dma);
		sbic_dmastart(B_READ, ccb->scat_gath[seg].seg_addr,
				ccb->scat_gath[seg].seg_len, sbic->sbic_dma);
	}
	if ((ccb->datalen) && (ccb->xs_flags & SCSI_DATA_OUT)) {
		sbic_dmadone(B_WRITE, ccb->scat_gath[seg - 1].seg_addr,
			     ccb->scat_gath[seg - 1].seg_len, sbic->sbic_dma);
		sbic_dmastart(B_WRITE, ccb->scat_gath[seg].seg_addr,
				ccb->scat_gath[seg].seg_len, sbic->sbic_dma);
	}
	SBIC_DMA_ENABLE();
	SET_SBIC_REG(SBIC_control, SBIC_CTL_DMA | SBIC_CTL_EDI);
	SET_XFER_LEN(ccb->scat_gath[seg].seg_len);
	SET_SBIC_REG(SBIC_cmd, SBIC_CMD_XFER_INFO);
	ccb->seg++;
	return;
}

static void
sbic_request_sense(struct sbic_data *sbic, struct sbic_ccb *ccb)
{
	int phys, len;
	unsigned chan;
	u_char cmd[6] = { 3, 0, 0, 0, 0, 0 };

	cmd[1] |= ((ccb->lun << 5) & 0xe0);

	chan = sbic->sbic_dma;
	len = ccb->sense_len;
#ifndef ALLWAYS_BOUNCE
	if(isa_dmarangecheck((caddr_t)ccb->sense_addr,len)) {
		ccb->sense_addr = (int)(sbic_bounce);
		ccb->flags |= CCB_BOUNCE;
	}
#endif
	phys = KVTOPHYS(ccb->sense_addr);
	sbic_dmastart(B_READ, phys, len, chan);
	
	SET_SCSI_CMD(cmd, 6);
	SET_XFER_LEN(ccb->sense_len);
	SET_SBIC_REG(SBIC_selid, ccb->target);
	SET_SBIC_REG(SBIC_tlun, ccb->lun);
	SBIC_DMA_ENABLE();
	SET_SBIC_REG(SBIC_control, SBIC_CTL_DMA | SBIC_CTL_EDI);
	SET_SBIC_REG(SBIC_cmd, SBIC_CMD_SEL_XFER);
	return;
}


static void
sbic_dmastart(int flags, int phys, unsigned nbytes, unsigned chan)
{
	int modeport, waport, mskport;
	caddr_t newaddr;
	int	s;

	if (chan > 3 || nbytes > (1<<16))
		panic("sbic_dmastart: impossible request"); 

	s = splbio();	/* mask on */

#ifdef CYRIX_5X86
	asm("wbinvd");	/* wbinvd (WB cache flush) */
#endif

	/* mask channel */
	mskport =  IO_DMA + 0x14;	/* 0x15 */
	outb(mskport, chan & 3 | 0x04);

	/* set dma channel mode, and reset address ff */
	modeport = IO_DMA + 0x16;	/* 0x17 */
	if (flags & B_READ) {
		outb(modeport, DMA37MD_SINGLE|DMA37MD_WRITE|(chan&3));
	} else {
		outb(modeport, DMA37MD_SINGLE|DMA37MD_READ|(chan&3));
	}
	outb(modeport + 1*2, 0);	/* 0x19 (clear byte pointer) */

	/* send start address */
	waport =  IO_DMA + (chan<<2);	/* 0x1, 0x5, 0x9, 0xd */
	outb(waport, phys);
	outb(waport, phys>>8);
	outb(dmapageport[chan], phys>>16);

	/* send count */
	outb(waport + 2, --nbytes);	/* 0x3, 0x7, 0xb, 0xf */
	outb(waport + 2, nbytes>>8);

	/* unmask channel */
	mskport =  IO_DMA + 0x14;	/* 0x15 */
	outb(mskport, chan & 3);

	splx(s);	/* mask off */
}

static void
sbic_dmadone(int flags, int addr, unsigned nbytes, unsigned chan)
{
#if defined(CYRIX_486DLC) || defined(IBM_486SLC)
	if (flags & B_READ) {
		/* cache flush only after reading 92/12/9 by A.Kojima */
		asm("	.byte 0x0f,0x08");	/* invd (cache flush) */
	}
#endif
}
