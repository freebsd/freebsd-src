/*
 * Ported for use with the UltraStor 14f by Gary Close (gclose@wvnvms.wvnet.edu)
 * Slight fixes to timeouts to run with the 34F
 * Thanks to Julian Elischer for advice and help with this port.
 *
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 * slight mod to make work with 34F as well: Wed Jun  2 18:05:48 WST 1993
 *
 *      $Id: ultra14f.c,v 1.12 1993/11/18 05:02:20 rgrimes Exp $
 */

#include <sys/types.h>

#ifdef	KERNEL			/* don't laugh.. this compiles to a program too.. look */
#include <uha.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <i386/include/pio.h>
#include <i386/isa/isa_device.h>
#endif /*KERNEL */
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/* */

#ifdef	KERNEL
#include "ddb.h"
#if     NDDB > 0
int     Debugger();
#else	/* NDDB */
#define Debugger() panic("should call debugger here")
#endif	/* NDDB */
#else /*KERNEL */
#define NUHA 1
#endif /*KERNEL */

typedef struct {
	unsigned char addr[4];
} physaddr;
typedef struct {
	unsigned char len[4];
} physlen;

#define KVTOPHYS(x)   vtophys(x)

#define UHA_MSCP_MAX	32	/* store up to 32MSCPs at any one time
				 * MAX = ?
				 */
#define	MSCP_HASH_SIZE	32	/* when we have a physical addr. for
				 * a mscp and need to find the mscp in
				 * space, look it up in the hash table
				 */
#define	MSCP_HASH_SHIFT	9	/* only hash on multiples of 512 */
#define MSCP_HASH(x)	((((long int)(x))>>MSCP_HASH_SHIFT) % MSCP_HASH_SIZE)

extern int hz;
#define UHA_NSEG        33	/* number of dma segments supported */

/************************** board definitions *******************************/
/*
 * I/O Port Interface
 */
#define UHA_LMASK              (0x000)	/* local doorbell mask reg */
#define UHA_LINT               (0x001)	/* local doorbell int/stat reg */
#define UHA_SMASK              (0x002)	/* system doorbell mask reg */
#define UHA_SINT               (0x003)	/* system doorbell int/stat reg */
#define UHA_ID0                (0x004)	/* product id reg 0 */
#define UHA_ID1                (0x005)	/* product id reg 1 */
#define UHA_CONF1              (0x006)	/* config reg 1 */
#define UHA_CONF2              (0x007)	/* config reg 2 */
#define UHA_OGM0               (0x008)	/* outgoing mail ptr 0 least sig */
#define UHA_OGM1               (0x009)	/* outgoing mail ptr 1 least mid */
#define UHA_OGM2               (0x00a)	/* outgoing mail ptr 2 most mid  */
#define UHA_OGM3               (0x00b)	/* outgoing mail ptr 3 most sig  */
#define UHA_ICM0               (0x00c)	/* incoming mail ptr 0 */
#define UHA_ICM1               (0x00d)	/* incoming mail ptr 1 */
#define UHA_ICM2               (0x00e)	/* incoming mail ptr 2 */
#define UHA_ICM3               (0x00f)	/* incoming mail ptr 3 */

/*
 * UHA_LMASK bits (read only) 
 */

#define UHA_LDIE                0x80	/* local doorbell int enabled */
#define UHA_SRSTE               0x40	/* soft reset enabled */
#define UHA_ABORTEN             0x10	/* abort MSCP enabled */
#define UHA_OGMINTEN            0x01	/* outgoing mail interrupt enabled */

/*
 * UHA_LINT bits (read)
 */

#define UHA_LDIP                0x80	/* local doorbell int pending */

/*
 * UHA_LINT bits (write)
 */

#define UHA_ADRST               0x40	/* adapter soft reset */
#define UHA_SBRST               0x20	/* scsi bus reset */
#define UHA_ASRST               0x60	/* adapter and scsi reset */
#define UHA_ABORT               0x10	/* abort MSCP */
#define UHA_OGMINT              0x01	/* tell adapter to get mail */

/*
 * UHA_SMASK bits (read)
 */

#define UHA_SINTEN              0x80	/* system doorbell interupt Enabled */
#define UHA_ABORT_COMPLETE_EN   0x10	/* abort MSCP command complete int Enabled */
#define UHA_ICM_ENABLED         0x01	/* ICM interrupt enabled */

/*
 * UHA_SMASK bits (write)
 */

#define UHA_ENSINT              0x80	/* enable system doorbell interrupt */
#define UHA_EN_ABORT_COMPLETE   0x10	/* enable abort MSCP complete int */
#define UHA_ENICM               0x01	/* enable ICM interrupt */

/*
 * UHA_SINT bits (read)
 */

#define UHA_SINTP               0x80	/* system doorbell int pending */
#define UHA_ABORT_SUCC          0x10	/* abort MSCP successful */
#define UHA_ABORT_FAIL          0x18	/* abort MSCP failed */

/*
 * UHA_SINT bits (write)
 */

#define UHA_ABORT_ACK           0x18	/* acknowledge status and clear */
#define UHA_ICM_ACK             0x01	/* acknowledge ICM and clear */

/* 
 * UHA_CONF1 bits (read only)
 */

#define UHA_DMA_CH5             0x00	/* DMA channel 5 */
#define UHA_DMA_CH6             0x40	/* 6 */
#define UHA_DMA_CH7             0x80	/* 7 */
#define UHA_IRQ15               0x00	/* IRQ 15 */
#define UHA_IRQ14               0x10	/* 14 */
#define UHA_IRQ11               0x20	/* 11 */
#define UHA_IRQ10               0x30	/* 10 */

/*
 * ha_status error codes
 */

#define UHA_NO_ERR		0x00	/* No error supposedly */
#define UHA_SBUS_ABORT_ERR	0x84	/* scsi bus abort error */
#define UHA_SBUS_TIMEOUT	0x91	/* scsi bus selection timeout */
#define UHA_SBUS_OVER_UNDER	0x92	/* scsi bus over/underrun */
#define UHA_BAD_SCSI_CMD	0x96	/* illegal scsi command */
#define UHA_AUTO_SENSE_ERR	0x9b	/* auto request sense err */
#define UHA_SBUS_RES_ERR	0xa3	/* scsi bus reset error */
#define UHA_BAD_SG_LIST		0xff	/* invalid scatter gath list */

struct uha_dma_seg {
	physaddr addr;
	physlen len;
};

struct mscp {
	unsigned char opcode:3;
#define U14_HAC		0x01	/* host adapter command */
#define U14_TSP		0x02	/* target scsi pass through command */
#define U14_SDR		0x04	/* scsi device reset */
	unsigned char xdir:2;	/* xfer direction */
#define U14_SDET	0x00	/* determined by scsi command */
#define U14_SDIN	0x01	/* scsi data in */
#define U14_SDOUT	0x02	/* scsi data out */
#define U14_NODATA	0x03	/* no data xfer */
	unsigned char dcn:1;	/* disable disconnect for this command */
	unsigned char ca:1;	/* cache control */
	unsigned char sgth:1;	/* scatter gather flag */
	unsigned char target:3;
	unsigned char chan:2;	/* scsi channel (always 0 for 14f) */
	unsigned char lun:3;
	physaddr data;
	physlen datalen;
	physaddr link;
	unsigned char link_id;
	unsigned char sg_num;	/*number of scat gath segs */
	/*in s-g list if sg flag is */
	/*set. starts at 1, 8bytes per */
	unsigned char senselen;
	unsigned char cdblen;
	unsigned char cdb[12];
	unsigned char ha_status;
	unsigned char targ_status;
	physaddr sense;		/* if 0 no auto sense */
	/*-----------------end of hardware supported fields----------------*/
	struct mscp *next;	/* in free list */
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int     flags;
#define MSCP_FREE        0
#define MSCP_ACTIVE      1
#define MSCP_ABORTED     2
	struct uha_dma_seg uha_dma[UHA_NSEG];
	struct scsi_sense_data mscp_sense;
	struct mscp *nexthash;
	long int hashkey;
};

struct uha_data {
	int     flags;
#define UHA_INIT        0x01;
	int     baseport;
	struct mscp *mscphash[MSCP_HASH_SIZE];
	struct mscp *free_mscp;
	int     our_id;		/* our scsi id */
	int     vect;
	int     dma;
	int     nummscps;
	struct scsi_link sc_link;
} *uhadata[NUHA];

int     uhaprobe();
int     uha_attach();
int     uhaintr();
int32   uha_scsi_cmd();
void    uha_timeout(caddr_t, int);
void	uha_free_mscp();
int     uha_abort();
void    uhaminphys();
void    uha_done();
u_int32 uha_adapter_info();
struct mscp *uha_mscp_phys_kv();

struct mscp *cheat;
unsigned long int scratch;
static  uha_unit = 0;
#define UHA_SHOWMSCPS 0x01
#define UHA_SHOWINTS 0x02
#define UHA_SHOWCMDS 0x04
#define UHA_SHOWMISC 0x08
#define FAIL    1
#define SUCCESS 0
#define PAGESIZ 4096

#ifdef	KERNEL
struct isa_driver uhadriver =
{
	uhaprobe,
	uha_attach,
	"uha"
};

struct scsi_adapter uha_switch =
{
	uha_scsi_cmd,
	uhaminphys,
	0,
	0,
	uha_adapter_info,
	"uha",
	0, 0
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device uha_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "uha",
    0,
    0, 0
};

#endif /*KERNEL */

#ifndef	KERNEL
main()
{
	printf("uha_data is %d bytes\n", sizeof(struct uha_data));
	printf("mscp is %d bytes\n", sizeof(struct mscp));
}

#else /*KERNEL*/
/*
 * Function to send a command out through a mailbox
 */
void
uha_send_mbox(int unit, struct mscp *mscp)
{
	struct uha_data *uha = uhadata[unit];
	int     port = uha->baseport;
	int     spincount = 100000;	/* 1s should be enough */
	int     s = splbio();

	while (--spincount) {
		if ((inb(port + UHA_LINT) & UHA_LDIP) == 0)
			break;
		DELAY(100);
	}
	if (spincount == 0) {
		printf("uha%d: uha_send_mbox, board not responding\n", unit);
		Debugger();
	}
	outl(port + UHA_OGM0, KVTOPHYS(mscp));
	outb(port + UHA_LINT, (UHA_OGMINT));
	splx(s);
}

/*
 * Function to send abort to 14f
 */
int
uha_abort(int unit, struct mscp *mscp)
{
	struct uha_data *uha = uhadata[unit];
	int     port = uha->baseport;
	int     spincount = 100;	/* 1 mSec */
	int     abortcount = 200000;	/*2 secs */
	int     s = splbio();

	while (--spincount) {
		if ((inb(port + UHA_LINT) & UHA_LDIP) == 0)
			break;
		DELAY(10);
	}
	if (spincount == 0);
	{
		printf("uha%d: uha_abort, board not responding\n", unit);
		Debugger();
	}
	outl(port + UHA_OGM0, KVTOPHYS(mscp));
	outb(port + UHA_LINT, UHA_ABORT);

	while (--abortcount) {
		if (inb(port + UHA_SINT) & UHA_ABORT_FAIL)
			break;
		DELAY(10);
	}
	if (abortcount == 0) {
		printf("uha%d: uha_abort, board not responding\n", unit);
		Debugger();
	}
	if ((inb(port + UHA_SINT) & 0x10) != 0) {
		outb(port + UHA_SINT, UHA_ABORT_ACK);
		return (1);
	} else {
		outb(port + UHA_SINT, UHA_ABORT_ACK);
		return (0);
	}
}

/*
 * Function to poll for command completion when in poll mode.
 *
 *	wait = timeout in msec
 */
int
uha_poll(int unit, int wait)
{
	struct	uha_data *uha = uhadata[unit];
	int	port = uha->baseport;
	int	stport = port + UHA_SINT;

      retry:
	while (--wait) {
		if (inb(stport) & UHA_SINTP)
			break;
		DELAY(1000);	/* 1 mSec per loop */
	}
	if (wait == 0) {
		printf("uha%d: uha_poll, board not responding\n", unit);
		return (EIO);
	}
	uhaintr(unit);
	return (0);
}

/*
 * Check if the device can be found at the port given and if so, set it up
 * ready for further work as an argument, takes the isa_device structure
 * from autoconf.c
 */
int
uhaprobe(dev)
	struct isa_device *dev;
{
	int     unit = uha_unit;
	struct uha_data *uha;

	dev->id_unit = unit;

	/*
	 * find unit and check we have that many defined
	 */
	if (unit >= NUHA) {
		printf("uha: unit number (%d) too high\n", unit);
		return (0);
	}
	dev->id_unit = unit;

	/*
	 * Allocate a storage area for us
	 */
	if (uhadata[unit]) {
		printf("uha%d: memory already allocated\n", unit);
		return 0;
	}
	uha = malloc(sizeof(struct uha_data), M_TEMP, M_NOWAIT);
	if (!uha) {
		printf("uha%d: cannot malloc!\n", unit);
		return 0;
	}
	bzero(uha, sizeof(struct uha_data));
	uhadata[unit] = uha;
	uha->baseport = dev->id_iobase;
	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads uha->vect
	 */
	if (uha_init(unit) != 0) {
		uhadata[unit] = NULL;
		free(uha, M_TEMP);
		return (0);
	}
	/* if it's there put in its interrupt and DRQ vectors */
	dev->id_irq = (1 << uha->vect);
	dev->id_drq = uha->dma;

	uha_unit++;
	return (16);
}

/*
 * Attach all the sub-devices we can find
 */
int
uha_attach(dev)
	struct isa_device *dev;
{
	int     unit = dev->id_unit;
	struct uha_data *uha = uhadata[unit];

	/*
	 * fill in the prototype scsi_link.
	 */
	uha->sc_link.adapter_unit = unit;
	uha->sc_link.adapter_targ = uha->our_id;
	uha->sc_link.adapter = &uha_switch;
	uha->sc_link.device = &uha_dev;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(&(uha->sc_link));

	return 1;
}

/*
 * Return some information to the caller about
 * the adapter and it's capabilities
 */
u_int32 
uha_adapter_info(unit)
	int     unit;
{
	return (2);		/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
int
uhaintr(unit)
	int unit;
{
	struct uha_data *uha = uhadata[unit];
	struct mscp *mscp;
	u_char  uhastat;
	unsigned long int mboxval;

	int     port = uha->baseport;

#ifdef	UHADEBUG
	printf("uhaintr ");
#endif /*UHADEBUG */

	while (inb(port + UHA_SINT) & UHA_SINTP) {
		/*
		 * First get all the information and then
		 * acknowledge the interrupt
		 */
		uhastat = inb(port + UHA_SINT);
		mboxval = inl(port + UHA_ICM0);
		outb(port + UHA_SINT, UHA_ICM_ACK);

#ifdef	UHADEBUG
		printf("status = 0x%x ", uhastat);
#endif /*UHADEBUG*/
		/*
		 * Process the completed operation
		 */

		mscp = uha_mscp_phys_kv(uha, mboxval);
		if (!mscp) {
			printf("uha: BAD MSCP RETURNED\n");
			return (0);	/* whatever it was, it'll timeout */
		}
		untimeout(uha_timeout, mscp);

		uha_done(unit, mscp);
	}
	return (1);
}

/*
 * We have a mscp which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
void
uha_done(unit, mscp)
	int	unit;
	struct	mscp *mscp;
{
	struct uha_data *uha = uhadata[unit];
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = mscp->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("uha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((mscp->ha_status == UHA_NO_ERR) || (xs->flags & SCSI_ERR_OK)) {	/* All went correctly  OR errors expected */
		xs->resid = 0;
		xs->error = 0;
	} else {

		s1 = &(mscp->mscp_sense);
		s2 = &(xs->sense);

		if (mscp->ha_status != UHA_NO_ERR) {
			switch (mscp->ha_status) {
			case UHA_SBUS_TIMEOUT:		/* No response */
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("timeout reported back\n"));
				xs->error = XS_TIMEOUT;
				break;
			case UHA_SBUS_OVER_UNDER:
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("scsi bus xfer over/underrun\n"));
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case UHA_BAD_SG_LIST:
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("bad sg list reported back\n"));
				xs->error = XS_DRIVER_STUFFUP;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("unexpected ha_status: %x\n",
					mscp->ha_status));
			}
		} else {

			if (mscp->targ_status != 0)
/*
 * I have no information for any possible value of target status field
 * other than 0 means no error!! So I guess any error is unexpected in that
 * event!!
 */

			{
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("unexpected targ_status: %x\n",
					mscp->targ_status));
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
      }
    done:
	xs->flags |= ITSDONE;
	uha_free_mscp(unit, mscp, xs->flags);
	scsi_done(xs);
}

/*
 * A mscp (and hence a mbx-out) is put onto the free list.
 */
void
uha_free_mscp(unit, mscp, flags)
	int unit;
	struct mscp *mscp;
	int flags;
{
	struct uha_data *uha = uhadata[unit];
	unsigned int opri = 0;

	if (!(flags & SCSI_NOMASK))
		opri = splbio();

	mscp->next = uha->free_mscp;
	uha->free_mscp = mscp;
	mscp->flags = MSCP_FREE;
	/*
	 * If there were none, wake abybody waiting for
	 * one to come free, starting with queued entries
	 */
	if (!mscp->next) {
		wakeup((caddr_t)&uha->free_mscp);
	}
	if (!(flags & SCSI_NOMASK))
		splx(opri);
}

/*
 * Get a free mscp
 *
 * If there are none, see if we can allocate a new one.  If so, put it in the
 * hash table too otherwise either return an error or sleep.
 */
struct mscp *
uha_get_mscp(unit, flags)
	int	unit, flags;
{
	struct uha_data *uha = uhadata[unit];
	unsigned opri = 0;
	struct mscp *mscpp;
	int     hashnum;

	if (!(flags & SCSI_NOMASK))
		opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one
	 */
	while (!(mscpp = uha->free_mscp)) {
		if (uha->nummscps < UHA_MSCP_MAX) {
			if (mscpp = (struct mscp *)malloc(sizeof(struct mscp),
			    M_TEMP,
			    M_NOWAIT)) {
				bzero(mscpp, sizeof(struct mscp));
				uha->nummscps++;
				mscpp->flags = MSCP_ACTIVE;
				/*
				 * put in the phystokv hash table
				 * Never gets taken out.
				 */
				mscpp->hashkey = KVTOPHYS(mscpp);
				hashnum = MSCP_HASH(mscpp->hashkey);
				mscpp->nexthash = uha->mscphash[hashnum];
				uha->mscphash[hashnum] = mscpp;
			} else {
				printf("uha%d: Can't malloc MSCP\n", unit);
			}
			goto gottit;
		} else {
			if (!(flags & SCSI_NOSLEEP)) {
				sleep(&uha->free_mscp, PRIBIO);
			}
		}
	}
	if (mscpp) {
		/* Get MSCP from from free list */
		uha->free_mscp = mscpp->next;
		mscpp->flags = MSCP_ACTIVE;
	}
      gottit:
      	if (!(flags & SCSI_NOMASK))
		splx(opri);

	return (mscpp);
}

/*
 * given a physical address, find the mscp that it corresponds to.
 */
struct mscp *
uha_mscp_phys_kv(uha, mscp_phys)
	struct	uha_data *uha;
	long	int mscp_phys;
{
	int     hashnum = MSCP_HASH(mscp_phys);
	struct mscp *mscpp = uha->mscphash[hashnum];

	while (mscpp) {
		if (mscpp->hashkey == mscp_phys)
			break;
		mscpp = mscpp->nexthash;
	}
	return mscpp;
}

/*
 * Start the board, ready for normal operation
 */
int
uha_init(unit)
	int     unit;
{
	struct uha_data *uha = uhadata[unit];
	unsigned char ad[4];
	volatile unsigned char model;
	volatile unsigned char submodel;
	unsigned char config_reg1;
	unsigned char config_reg2;
	unsigned char dma_ch;
	unsigned char irq_ch;
	unsigned char uha_id;
	int     port = uha->baseport;
	int     i;
	int     resetcount = 4000;	/* 4 secs? */

	model = inb(port + UHA_ID0);
	submodel = inb(port + UHA_ID1);
	if ((model != 0x56) & (submodel != 0x40)) {
		printf("uha%d: uha_init, board not responding\n", unit);
		return (ENXIO);
	}
	printf("uha%d: reading board settings, ", unit);

	config_reg1 = inb(port + UHA_CONF1);
	config_reg2 = inb(port + UHA_CONF2);
	dma_ch = (config_reg1 & 0xc0);
	irq_ch = (config_reg1 & 0x30);
	uha_id = (config_reg2 & 0x07);

	switch (dma_ch) {
	case UHA_DMA_CH5:
		uha->dma = 5;
		printf("dma=5 ");
		break;
	case UHA_DMA_CH6:
		uha->dma = 6;
		printf("dma=6 ");
		break;
	case UHA_DMA_CH7:
		uha->dma = 7;
		printf("dma=7 ");
		break;
	default:
		printf("illegal dma jumper setting\n");
		return (EIO);
	}
	switch (irq_ch) {
	case UHA_IRQ10:
		uha->vect = 10;
		printf("int=10 ");
		break;
	case UHA_IRQ11:
		uha->vect = 11;
		printf("int=11 ");
		break;
	case UHA_IRQ14:
		uha->vect = 14;
		printf("int=14 ");
		break;
	case UHA_IRQ15:
		uha->vect = 15;
		printf("int=15 ");
		break;
	default:
		printf("illegal int jumper setting\n");
		return (EIO);
	}

	/* who are we on the scsi bus */
	printf("id=%x\n", uha_id);
	uha->our_id = uha_id;

	/*
	 * Note that we are going and return (to probe)
	 */
	outb(port + UHA_LINT, UHA_ASRST);
	while (--resetcount) {
		if (inb(port + UHA_LINT));
		break;
		DELAY(1000);	/* 1 mSec per loop */
	}
	if (resetcount == 0) {
		printf("uha%d: board timed out during reset\n", unit);
		return (ENXIO);
	}
	outb(port + UHA_SMASK, 0x81);	/* make sure interrupts are enabled */
	uha->flags |= UHA_INIT;
	return (0);
}

#ifndef min
#define min(x,y) (x < y ? x : y)
#endif	/* min */

void
uhaminphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > ((UHA_NSEG - 1) * PAGESIZ)) {
		bp->b_bcount = ((UHA_NSEG - 1) * PAGESIZ);
	}
}

/*
 * start a scsi operation given the command and the data address.  Also
 * needs the unit, target and lu.
 */
int32 
uha_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	scsi_sense_data *s1, *s2;
	struct	mscp *mscp;
	struct	uha_dma_seg *sg;
	int	seg;		/* scatter gather seg being worked on */
	int	i = 0;
	int     rc = 0;
	int     thiskv;
	unsigned long int thisphys, nextphys;
	int     unit = xs->sc_link->adapter_unit;
	int     bytes_this_seg, bytes_this_page, datalen, flags;
	struct iovec *iovp;
	struct uha_data *uha = uhadata[unit];
	int     s;
	unsigned int stat;
	int     port = uha->baseport;
	unsigned long int templen;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("uha_scsi_cmd\n"));
	/*
	 * get a mscp (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (xs->bp)
		flags |= (SCSI_NOSLEEP);	/* just to be sure */
	if (flags & ITSDONE) {
		printf("uha%d: Already done?", unit);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("uha%d: Not in use?", unit);
		xs->flags |= INUSE;
	}
	if (!(mscp = uha_get_mscp(unit, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	cheat = mscp;
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start mscp(%x)\n", mscp));
	mscp->xs = xs;

	/*
	 * Put all the arguments for the xfer in the mscp
	 */
	if (flags & SCSI_RESET) {
		mscp->opcode = 0x04;
		mscp->ca = 0x01;
	} else {
		mscp->opcode = 0x02;
		mscp->ca = 0x01;
	}
	if (flags & SCSI_DATA_IN) {
		mscp->xdir = 0x01;
	}
	if (flags & SCSI_DATA_OUT) {
		mscp->xdir = 0x02;
	}
#ifdef	GOTTABEJOKING
	if (xs->sc_link->lun != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		uha_free_mscp(unit, mscp, flags);
		return (HAD_ERROR);
	}
#endif
	mscp->dcn = 0x00;
	mscp->chan = 0x00;
	mscp->target = xs->sc_link->target;
	mscp->lun = xs->sc_link->lun;
	mscp->link.addr[0] = 0x00;
	mscp->link.addr[1] = 0x00;
	mscp->link.addr[2] = 0x00;
	mscp->link.addr[3] = 0x00;
	mscp->link_id = 0x00;
	mscp->cdblen = xs->cmdlen;
	scratch = KVTOPHYS(&(mscp->mscp_sense));
	mscp->sense.addr[0] = (scratch & 0xff);
	mscp->sense.addr[1] = ((scratch >> 8) & 0xff);
	mscp->sense.addr[2] = ((scratch >> 16) & 0xff);
	mscp->sense.addr[3] = ((scratch >> 24) & 0xff);
	mscp->senselen = sizeof(mscp->mscp_sense);
	mscp->ha_status = 0x00;
	mscp->targ_status = 0x00;

	if (xs->datalen) {	/* should use S/G only if not zero length */
		scratch = KVTOPHYS(mscp->uha_dma);
		mscp->data.addr[0] = (scratch & 0xff);
		mscp->data.addr[1] = ((scratch >> 8) & 0xff);
		mscp->data.addr[2] = ((scratch >> 16) & 0xff);
		mscp->data.addr[3] = ((scratch >> 24) & 0xff);
		sg = mscp->uha_dma;
		seg = 0;
		mscp->sgth = 0x01;

#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < UHA_NSEG)) {
				scratch = (unsigned long) iovp->iov_base;
				sg->addr.addr[0] = (scratch & 0xff);
				sg->addr.addr[1] = ((scratch >> 8) & 0xff);
				sg->addr.addr[2] = ((scratch >> 16) & 0xff);
				sg->addr.addr[3] = ((scratch >> 24) & 0xff);
				xs->datalen += *(unsigned long *) sg->len.len = iovp->iov_len;
				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("(0x%x@0x%x)",
					iovp->iov_len,
					iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif /*TFS */
		{
			/*
			 * Set up the scatter gather block
			 */

			SC_DEBUG(xs->sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);
			templen = 0;

			while ((datalen) && (seg < UHA_NSEG)) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->addr.addr[0] = (thisphys & 0xff);
				sg->addr.addr[1] = ((thisphys >> 8) & 0xff);
				sg->addr.addr[2] = ((thisphys >> 16) & 0xff);
				sg->addr.addr[3] = ((thisphys >> 24) & 0xff);

				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while ((datalen) && (thisphys == nextphys))
					/*
					 * This page is contiguous (physically) with 
					 * the the last, just extend the length 
					 */
				{
					/* how far to the end of the page */
					nextphys = (thisphys & (~(PAGESIZ - 1)))
					    + PAGESIZ;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page
					    ,datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & (~(PAGESIZ - 1)))
					    + PAGESIZ;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				sg->len.len[0] = (bytes_this_seg & 0xff);
				sg->len.len[1] = ((bytes_this_seg >> 8) & 0xff);
				sg->len.len[2] = ((bytes_this_seg >> 16) & 0xff);
				sg->len.len[3] = ((bytes_this_seg >> 24) & 0xff);
				templen += bytes_this_seg;
				sg++;
				seg++;
			}
		}

		/* end of iov/kv decision */
		mscp->datalen.len[0] = (templen & 0xff);
		mscp->datalen.len[1] = ((templen >> 8) & 0xff);
		mscp->datalen.len[2] = ((templen >> 16) & 0xff);
		mscp->datalen.len[3] = ((templen >> 24) & 0xff);
		mscp->sg_num = seg;

		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
		if (datalen) {	/* there's still data, must have run out of segs! */
			printf("uha%d: uha_scsi_cmd, more than %d DMA segs\n",
			    unit, UHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			uha_free_mscp(unit, mscp, flags);
			return (HAD_ERROR);
		}
	} else {		/* No data xfer, use non S/G values */
		mscp->data.addr[0] = 0x00;
		mscp->data.addr[1] = 0x00;
		mscp->data.addr[2] = 0x00;
		mscp->data.addr[3] = 0x00;
		mscp->datalen.len[0] = 0x00;
		mscp->datalen.len[1] = 0x00;
		mscp->datalen.len[2] = 0x00;
		mscp->datalen.len[3] = 0x00;
		mscp->xdir = 0x03;
		mscp->sgth = 0x00;
		mscp->sg_num = 0x00;
	}

	/*
	 * Put the scsi command in the mscp and start it
	 */
	bcopy(xs->cmd, mscp->cdb, xs->cmdlen);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if (!(flags & SCSI_NOMASK)) {
		s = splbio();
		uha_send_mbox(unit, mscp);
		timeout(uha_timeout, (caddr_t)mscp, (xs->timeout * hz) / 1000);
		splx(s);
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
		return (SUCCESSFULLY_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll on completion
	 */
	uha_send_mbox(unit, mscp);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_wait\n"));
	do {
		if (uha_poll(unit, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("uha%d: cmd fail\n", unit);
			if (!(uha_abort(unit, mscp))) {
				printf("uha%d: abort failed in wait\n", unit);
				uha_free_mscp(unit, mscp, flags);
			}
			xs->error = XS_DRIVER_STUFFUP;
			return (HAD_ERROR);
		}
	}
	while (!(xs->flags & ITSDONE));	/* something (?) else finished */
	if (xs->error) {
		return (HAD_ERROR);
	}
	return (COMPLETE);
}

void
uha_timeout(caddr_t arg1, int arg2)
{
	struct mscp *mscp = (struct mscp *)arg1;
	int     unit;
	struct uha_data *uha;
	int     s = splbio();
	/*int   port = uha->baseport; */

	unit = mscp->xs->sc_link->adapter_unit;
	uha = uhadata[unit];
	printf("uha%d:%d:%d (%s%d) timed out ", unit
	    ,mscp->xs->sc_link->target
	    ,mscp->xs->sc_link->lun
	    ,mscp->xs->sc_link->device->name
	    ,mscp->xs->sc_link->dev_unit);

#ifdef	UHADEBUG
	uha_print_active_mscp(unit);
#endif /*UHADEBUG */

	if ((uha_abort(unit, mscp) != 1) || (mscp->flags = MSCP_ABORTED)) {
		printf("AGAIN");
		mscp->xs->retries = 0;	/* I MEAN IT ! */
		uha_done(unit, mscp, FAIL);
	} else {		/* abort the operation that has timed out */
		printf("\n");
		timeout(uha_timeout, (caddr_t)mscp, 2 * hz);
		mscp->flags = MSCP_ABORTED;
	}
	splx(s);
}

#ifdef	UHADEBUG
void
uha_print_mscp(mscp)
	struct mscp *mscp;
{
	printf("mscp:%x op:%x cmdlen:%d senlen:%d\n"
	    ,mscp
	    ,mscp->opcode
	    ,mscp->cdblen
	    ,mscp->senselen);
	printf("	sg:%d sgnum:%x datlen:%d hstat:%x tstat:%x flags:%x\n"
	    ,mscp->sgth
	    ,mscp->sg_num
	    ,mscp->datalen
	    ,mscp->ha_status
	    ,mscp->targ_status
	    ,mscp->flags);
	show_scsi_cmd(mscp->xs);
}

void
uha_print_active_mscp(int unit)
{
	struct uha_data *uha = uhadata[unit];
	struct mscp *mscp;
	int     i = 0;

	while (i < MSCP_HASH_SIZE) {
		mscp = uha->mscphash[i];
		while (mscp) {
			if (mscp->flags != MSCP_FREE) {
				uha_print_mscp(mscp);
			}
			mscp = mscp->nexthash;
		}
	i++;
	}
}
#endif /*UHADEBUG */
#endif /*KERNEL */
