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
 * today: Fri Jun  2 17:21:03 EST 1994
 * added 24F support  ++sg
 *
 *      $Id: ultra14f.c,v 1.44 1996/01/07 19:22:39 gibbs Exp $
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
#include <sys/devconf.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>
#endif /*KERNEL */
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/* */

#ifndef	KERNEL
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
struct uha_reg
{
	int id;			/* product id reg		*/
	int type;		/* product type reg		*/
	int ectl;		/* EISA expansion control bits	*/
	int config;		/* configuration bits 		*/
	int lmask;		/* local doorbell mask reg	*/
	int lint;		/* local doorbell int/stat reg	*/
	int smask;		/* system doorbell mask reg	*/
	int sint;		/* system doorbell int/stat reg	*/
	int ogmcmd;		/* outgoing mail command	*/
	int ogmptr;		/* outgoing mail ptr		*/
	int icmcmd;		/* incoming mail command	*/
	int icmptr;		/* incoming mail ptr		*/
};

struct uha_bits
{
	/* uha_lint (read) */
	unsigned char ldip;

	/* uha_lint (write) */
	unsigned char adrst;
	unsigned char sbrst;
	unsigned char asrst;
	unsigned char abort;
	unsigned char ogmint;

	/* uha_sint (read) */
	unsigned char sintp;
	unsigned char abort_succ;
	unsigned char abort_fail;

	/* uha_sint (write) */
	unsigned char abort_ack;
	unsigned char icm_ack;
};


/*
 * UHA_LINT bits (read)
 */

#define UHA_LDIP                0x80	/* local doorbell int pending */
#define U24_LDIP		0x02

/*
 * UHA_LINT bits (write)
 */

#define UHA_ADRST               0x40	/* adapter soft reset */
#define UHA_SBRST               0x20	/* scsi bus reset */
#define UHA_ASRST               0x60	/* adapter and scsi reset */
#define UHA_ABORT               0x10	/* abort MSCP */
#define UHA_OGMINT              0x01	/* tell adapter to get mail */
#define U24_SBRST		0x40	/* scsi bus reset */
#define U24_ADRST		0x80	/* adapter soft reset */
#define U24_ASRST		0xc0	/* adapter and scsi reset */
#define U24_ABORT		0x10	/* same? */
#define U24_OGMINT		0x02	/* enable OGM interrupt */

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
#define U24_SINTP		0x02	/* system doorbell int pending */
#define U24_ABORT_SUCC		0x10	/* same? */
#define U24_ABORT_FAIL		0x18	/* same? */

/*
 * UHA_SINT bits (write)
 */

#define UHA_ABORT_ACK           0x18	/* acknowledge status and clear */
#define UHA_ICM_ACK             0x01	/* acknowledge ICM and clear */
#define U24_ABORT_ACK		0x18	/* same */
#define U24_ICM_ACK		0x02	/* 24F acknowledge ICM and clear */

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

#define EISA_CONFIG		0x0c80	/* Configuration base port */
#define EISA_DISABLE		0x01	/* EISA disable bit */

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

static struct uha_data {
	int	flags;
#define UHA_INIT        0x01
#define UHA_24F		0x02
	int	baseport;
	struct	mscp *mscphash[MSCP_HASH_SIZE];
	struct	mscp *free_mscp;
	int	unit;
	int	our_id;		/* our scsi id */
	int	vect;
	int	dma;
	int	nummscps;
	struct	scsi_link sc_link;
	struct	uha_reg *ur;
	struct	uha_bits *ub;
} *uhadata[NUHA];

static int	uha_abort __P((struct uha_data *uha, struct mscp *mscp));
static u_int32	uha_adapter_info __P((int unit));
static int	uha_attach __P((struct isa_device *dev));
static void	uha_done __P((struct uha_data *uha, struct mscp *mscp));
static void	uha_free_mscp __P((struct uha_data *uha, struct mscp *mscp,
				   int flags));
static struct mscp *
		uha_get_mscp __P((struct uha_data *uha, int flags));
static int	uha_init __P((struct uha_data *uha));
static int	uha24_init __P((struct uha_data *uha));
static void	uhaminphys __P((struct buf *bp));
static struct mscp *
		uha_mscp_phys_kv __P((struct uha_data *uha, long mscp_phys));
static int	uha_poll __P((struct uha_data *uha, int wait));
#ifdef UHADEBUG
static void	uha_print_active_mscp __P((struct uha_data *uha));
static void	uha_print_mscp __P((struct mscp *mscp));
#endif
static int	uhaprobe __P((struct isa_device *dev));
static int32	uha_scsi_cmd __P((struct scsi_xfer *xs));
static void	uha_send_mbox __P((struct uha_data *uha, struct mscp *mscp));
static timeout_t
		uha_timeout;

static	struct mscp *cheat;
static unsigned long int scratch;
#define	EISA_MAX_SLOTS	16	/* XXX This should go into a comon header */
static	uha_slot = 0;		/* slot last board was found in */
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

static struct scsi_adapter uha_switch =
{
	uha_scsi_cmd,
	uhaminphys,
	0,
	0,
	uha_adapter_info,
	"uha",
	{ 0, 0 }
};

/* the below structure is so we have a default dev struct for out link struct */
static struct scsi_device uha_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "uha",
    0,
    { 0, 0 }
};

static struct kern_devconf kdc_uha[NUHA] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"uha", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* state */
	"UltraStore 14F or 34F SCSI host adapter",
	DC_CLS_MISC		/* host adapters aren't special */
} };

static inline void
uha_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_uha[id->id_unit] = kdc_uha[0];
	kdc_uha[id->id_unit].kdc_unit = id->id_unit;
	kdc_uha[id->id_unit].kdc_parentdata = id;
	dev_attach(&kdc_uha[id->id_unit]);
}

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
static void
uha_send_mbox(struct uha_data *uha, struct mscp *mscp)
{
	int     spincount = 100000;	/* 1s should be enough */
	struct uha_reg *ur = uha->ur;
	struct uha_bits *ub = uha->ub;
	int     s = splbio();

	while (--spincount) {
		if ((inb(ur->lint) & ub->ldip) == 0)
			break;
		DELAY(100);
	}
	if (spincount == 0) {
		printf("uha%d: uha_send_mbox, board not responding\n",
			uha->unit);
		Debugger("ultra14f");
	}
	outl(ur->ogmptr, KVTOPHYS(mscp));
	if (uha->flags & UHA_24F) outb(ur->ogmcmd, 1);
	outb(ur->lint, ub->ogmint);
	splx(s);
}

/*
 * Function to send abort to 14f
 */
int
uha_abort(struct uha_data *uha, struct mscp *mscp)
{
	int	spincount = 100;	/* 1 mSec */
	int	abortcount = 200000;	/*2 secs */
	struct	uha_reg *ur = uha->ur;
	struct	uha_bits *ub = uha->ub;
	int     s = splbio();

	while (--spincount) {
		if ((inb(ur->lint) & ub->ldip) == 0)
			break;
		DELAY(10);
	}
	if (spincount == 0) {
		printf("uha%d: uha_abort, board not responding\n", uha->unit);
		Debugger("ultra14f");
	}
	outl(ur->ogmptr,KVTOPHYS(mscp));
	if (uha->flags & UHA_24F) outb(ur->ogmcmd, 1);
	outb(ur->lint, ub->abort);

	while (--abortcount) {
		if (inb(ur->sint) & ub->abort_fail)
			break;
		DELAY(10);
	}
	if (abortcount == 0) {
		printf("uha%d: uha_abort, board not responding\n", uha->unit);
		Debugger("ultra14f");
	}
	if ((inb(ur->sint) & 0x10) != 0) {
		outb(ur->sint, ub->abort_ack);
		splx(s);
		return (1);
	} else {
		outb(ur->sint, ub->abort_ack);
		splx(s);
		return (0);
	}
}

/*
 * Function to poll for command completion when in poll mode.
 *
 *	wait = timeout in msec
 */
static int
uha_poll(struct uha_data *uha, int wait)
{
	struct	uha_reg *ur = uha->ur;
	struct	uha_bits *ub = uha->ub;
	int	stport = ur->sint;

	while (--wait) {
		if (inb(stport) & ub->sintp)
			break;
		DELAY(1000);	/* 1 mSec per loop */
	}
	if (wait == 0) {
		printf("uha%d: uha_poll, board not responding\n", uha->unit);
		return (EIO);
	}
	uhaintr(uha->unit);
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

	dev->id_unit = unit;	/* XXX */

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

	uha->ur = malloc(sizeof(struct uha_reg), M_TEMP, M_NOWAIT);
	if (!uha->ur) {
		printf("uha%d: cannot malloc!\n", unit);
		return 0;
	}
	bzero(uha->ur, sizeof(struct uha_reg));

	uha->ub = malloc(sizeof(struct uha_bits), M_TEMP, M_NOWAIT);
	if (!uha->ub) {
		printf("uha%d: cannot malloc!\n", unit);
		return 0;
	}
	bzero(uha->ub, sizeof(struct uha_bits));

	uha_registerdev(dev);

	uhadata[unit] = uha;
	uha->unit = unit;
	uha->baseport = dev->id_iobase;
	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads uha->vect
	 */
	if (uha_init(uha) != 0 && uha24_init(uha) != 0) {
		uhadata[unit] = NULL;
		free(uha->ur, M_TEMP);
		free(uha->ub, M_TEMP);
		free(uha, M_TEMP);
		return (0);
	}
	/* if it's there put in its interrupt and DRQ vectors */
	dev->id_irq = (1 << uha->vect);
	dev->id_drq = uha->dma;
	dev->id_iobase = uha->baseport;

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
	struct scsibus_data *scbus;

	/*
	 * fill in the prototype scsi_link.
	 */
	uha->sc_link.adapter_unit = unit;
	uha->sc_link.adapter_targ = uha->our_id;
	uha->sc_link.adapter_softc = uha;
	uha->sc_link.adapter = &uha_switch;
	uha->sc_link.device = &uha_dev;
	uha->sc_link.flags = SDEV_BOUNCE;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
                return 0;
	scbus->adapter_link = &uha->sc_link;

	kdc_uha[unit].kdc_state = DC_BUSY;
	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);

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
void
uhaintr(unit)
	int unit;
{
	struct uha_data *uha = uhadata[unit];
	struct mscp *mscp;
	u_char  uhastat;
	unsigned long int mboxval;
	struct uha_reg *ur;
	struct uha_bits *ub;
	int     port;

	ur = uha->ur;
	ub = uha->ub;
	port = uha->baseport;

#ifdef	UHADEBUG
	printf("uhaintr ");
#endif /*UHADEBUG */

	while ((uhastat = inb(ur->sint)) & ub->sintp) {
		/*
		 * First get all the information and then
		 * acknowledge the interrupt
		 */
		mboxval = inl(ur->icmptr);
		outb(ur->sint, ub->icm_ack);
		if (uha->flags & UHA_24F) outb(ur->icmcmd, 0);

#ifdef	UHADEBUG
		printf("status = 0x%x ", uhastat);
#endif /*UHADEBUG*/
		/*
		 * Process the completed operation
		 */

		mscp = uha_mscp_phys_kv(uha, mboxval);
		if (!mscp) {
			printf("uha: BAD MSCP RETURNED\n");
			return;		/* whatever it was, it'll timeout */
		}
		untimeout(uha_timeout, (caddr_t)mscp);

		uha_done(uha, mscp);
	}
}

/*
 * We have a mscp which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
void
uha_done(uha, mscp)
	struct	uha_data *uha;
	struct	mscp *mscp;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = mscp->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("uha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if (((mscp->ha_status != UHA_NO_ERR) || (mscp->targ_status != SCSI_OK))
	 && ((xs->flags & SCSI_ERR_OK) == 0)) {

		s1 = &(mscp->mscp_sense);
		s2 = &(xs->sense);

		if (mscp->ha_status != UHA_NO_ERR) {
			switch (mscp->ha_status) {
			case UHA_SBUS_ABORT_ERR:
			case UHA_SBUS_TIMEOUT:		/* No sel response */
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("abort or timeout; ha_status 0x%x\n",
					mscp->ha_status));
				xs->error = XS_TIMEOUT;
				break;
			case UHA_SBUS_OVER_UNDER:
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("scsi bus xfer over/underrun\n"));
				xs->error = XS_DRIVER_STUFFUP;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				printf("uha%d: unexpected ha_status 0x%x (target status 0x%x)\n",
					uha->unit, mscp->ha_status,
					mscp->targ_status);
				break;
			}
		} else {
			/* Target status problem */
			SC_DEBUG(xs->sc_link, SDEV_DB3, ("target err 0x%x\n",
				mscp->targ_status));
			switch (mscp->targ_status) {
			case 0x02:
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case 0x08:
				xs->error = XS_BUSY;
				break;
			default:
				printf("uha%d: unexpected targ_status 0x%x\n",
					uha->unit, mscp->targ_status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		}
	}
	else {
		/* All went correctly  OR  errors expected */
		xs->resid = 0;
		xs->error = 0;
	}
	xs->flags |= ITSDONE;
	uha_free_mscp(uha, mscp, xs->flags);
	scsi_done(xs);
}

/*
 * A mscp (and hence a mbx-out) is put onto the free list.
 */
void
uha_free_mscp(uha, mscp, flags)
	struct uha_data *uha;
	struct mscp *mscp;
	int flags;
{
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
static struct mscp *
uha_get_mscp(uha, flags)
	struct	uha_data *uha;
	int	flags;
{
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
				printf("uha%d: Can't malloc MSCP\n", uha->unit);
			}
			goto gottit;
		} else {
			if (!(flags & SCSI_NOSLEEP)) {
				tsleep((caddr_t)&uha->free_mscp, PRIBIO,
				       "uhamscp", 0);
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
static struct mscp *
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
uha_init(uha)
	struct	uha_data *uha;
{
	volatile unsigned char model;
	volatile unsigned char submodel;
	unsigned char config_reg1;
	unsigned char config_reg2;
	unsigned char dma_ch;
	unsigned char irq_ch;
	unsigned char uha_id;
	int     port = uha->baseport;
	int     resetcount = 4000;	/* 4 secs? */
	struct uha_reg *ur = uha->ur;
	struct uha_bits *ub = uha->ub;

	/*
	 *  Prepare to use a 14/34F.
	 */
	ur->id		= port + 0x04;
	ur->type	= port + 0x00;		/* 24F only */
	ur->ectl	= port + 0x00;		/* 24F only */
	ur->config	= port + 0x06;		/* 0-1 for 14F */
	ur->lmask	= port + 0x00;
	ur->lint	= port + 0x01;
	ur->smask	= port + 0x02;
	ur->sint	= port + 0x03;
	ur->ogmcmd	= port + 0x00;		/* 24F only */
	ur->ogmptr	= port + 0x08;
	ur->icmcmd	= port + 0x00;		/* 24F only */
	ur->icmptr	= port + 0x0c;

	ub->ldip	= UHA_LDIP;
	ub->adrst	= UHA_ADRST;
	ub->sbrst	= UHA_SBRST;
	ub->asrst	= UHA_ASRST;
	ub->abort	= UHA_ABORT;
	ub->ogmint	= UHA_OGMINT;
	ub->sintp	= UHA_SINTP;
	ub->abort_succ	= UHA_ABORT_SUCC;
	ub->abort_fail	= UHA_ABORT_FAIL;
	ub->abort_ack	= UHA_ABORT_ACK;
	ub->icm_ack	= UHA_ICM_ACK;

	model = inb(ur->id);
	submodel = inb(ur->id + 1);
	if ((model != 0x56) & (submodel != 0x40)) return(ENXIO);
	printf("uha%d: reading board settings, ", uha->unit);

	config_reg1 = inb(ur->config);
	config_reg2 = inb(ur->config + 1);
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
	outb(ur->lint, ub->asrst);
	while (--resetcount) {
		if (inb(ur->lint))
			break;
		DELAY(1000);	/* 1 mSec per loop */
	}
	if (resetcount == 0) {
		printf("uha%d: board timed out during reset\n", uha->unit);
		return (ENXIO);
	}
	outb(ur->smask, 0x81);		/* make sure interrupts are enabled */
	uha->flags |= UHA_INIT;
	return (0);
}


/*
 *  Initialize an Ultrastor 24F
 */
int
uha24_init(uha)
	struct uha_data *uha;
{
  unsigned char p0, p1, p2, p3, p5, p7;
  unsigned char id[7], rev, haid;
  int port = 0, irq;
  int resetcount = 4000;
  struct uha_reg *ur = uha->ur;
  struct uha_bits *ub = uha->ub;

  /* Search for the 24F's product ID */
  uha_slot++;
  while (uha_slot < EISA_MAX_SLOTS) {
	/*
	 *  Prepare to use a 24F.
	 */
	port = EISA_CONFIG | (uha_slot << 12);
	ur->id		= port + 0x00;
	ur->type	= port + 0x02;
	ur->ectl	= port + 0x04;
	ur->config	= port + 0x05;		/* 0-2 for 24F */
	ur->lmask	= port + 0x0c;
	ur->lint	= port + 0x0d;
	ur->smask	= port + 0x0e;
	ur->sint	= port + 0x0f;
	ur->ogmcmd	= port + 0x16;
	ur->ogmptr	= port + 0x17;
	ur->icmcmd	= port + 0x1b;
	ur->icmptr	= port + 0x1c;

	ub->ldip	= U24_LDIP;
	ub->adrst	= U24_ADRST;
	ub->sbrst	= U24_SBRST;
	ub->asrst	= U24_ASRST;
	ub->abort	= U24_ABORT;
	ub->ogmint	= U24_OGMINT;
	ub->sintp	= U24_SINTP;
	ub->abort_succ	= U24_ABORT_SUCC;
	ub->abort_fail	= U24_ABORT_FAIL;
	ub->abort_ack	= U24_ABORT_ACK;
	ub->icm_ack	= U24_ICM_ACK;

	/*
	 * Make sure an EISA card is installed in this slot,
	 * and if it is make sure that the card is enabled.
	 */
	outb(ur->id, 0xff);
	p0 = inb(ur->id);
	if ((p0 == 0xff) ||
	    ((p0 & 0x80) != 0) ||
            ((inb(ur->ectl) & EISA_DISABLE) == 0)) {
		uha_slot++;
		continue;
	}

	/* Found an enabled card.  Grab the product ID. */
	p1 = inb(ur->id+1);
	p2 = inb(ur->type);
	p3 = inb(ur->type+1);
	id[0] = 0x40 + ((p0 >> 2) & 0x1f);
	id[1] = 0x40 + (((p0 & 0x03) << 3) | ((p1 >> 5) & 0x07));
	id[2] = 0x40 + (p1 & 0x1f);
	id[3] = hex2ascii((p2 >> 4) & 0x0f);
	id[4] = hex2ascii(p2 & 0x0f);
	id[5] = hex2ascii((p3 >> 4) & 0x0f);
	id[6] = '\0';
	rev = p3 & 0xf;

	/* We only want the 24F product ID. */
	if (!strcmp(id, "USC024")) break;
	uha_slot++;
  }
  if (uha_slot == EISA_MAX_SLOTS) return(ENODEV);

  /* We have the card!  Grab remaining config. */
  p5 = inb(ur->config);
  p7 = inb(ur->config+2);

  switch (p5 & 0xf0) {
     case 0x10: irq = 15; break;
     case 0x20: irq = 14; break;
     case 0x40: irq = 11; break;
     case 0x80: irq = 10; break;
     default:
	printf("uha%d: bad 24F irq\n", uha->unit);
	return(ENXIO);
  }

  haid = (p7 & 0x07);
  printf("uha%d: UltraStor 24F int=%d id=%d\n", uha->unit, irq, haid);

  /* Issue SCSI and adapter reset */
  outb(ur->lint, ub->asrst);
  while (--resetcount) {
	if (inb(ur->lint))
		break;
	DELAY(1000);	/* 1 mSec per loop */
  }
  if (resetcount == 0) {
	printf("uha%d: board timed out during reset\n", uha->unit);
	return (ENXIO);
  }
  outb(ur->smask, 0xc2);	/* make sure interrupts are enabled */
  uha->flags |= (UHA_INIT | UHA_24F);
  uha->baseport = port;
  uha->our_id = haid;
  uha->vect = irq;
  uha->dma = -1;
  return(0);
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
static int32
uha_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	mscp *mscp;
	struct	uha_dma_seg *sg;
	int	seg;		/* scatter gather seg being worked on */
	int     thiskv;
	unsigned long int thisphys, nextphys;
	int     bytes_this_seg, bytes_this_page, datalen, flags;
	struct	uha_data *uha;
	int     s;
	unsigned long int templen;

	uha = (struct uha_data *)xs->sc_link->adapter_softc;
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
		printf("uha%d: Already done?", uha->unit);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("uha%d: Not in use?", uha->unit);
		xs->flags |= INUSE;
	}
	if (!(mscp = uha_get_mscp(uha, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	cheat = mscp;
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start mscp(%p)\n", mscp));
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
		uha_free_mscp(uha, mscp, flags);
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
			    ("%ld @%p:- ", xs->datalen, xs->data));
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

				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%lx",
					thisphys));

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
			    uha->unit, UHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			uha_free_mscp(uha, mscp, flags);
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
		uha_send_mbox(uha, mscp);
		timeout(uha_timeout, (caddr_t)mscp, (xs->timeout * hz) / 1000);
		splx(s);
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
		return (SUCCESSFULLY_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll on completion
	 */
	uha_send_mbox(uha, mscp);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_wait\n"));
	do {
		if (uha_poll(uha, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("uha%d: cmd fail\n", uha->unit);
			if (!(uha_abort(uha, mscp))) {
				printf("uha%d: abort failed in wait\n",
					uha->unit);
				uha_free_mscp(uha, mscp, flags);
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

static void
uha_timeout(arg1)
	void	*arg1;
{
	struct mscp *mscp = (struct mscp *)arg1;
	struct uha_data *uha;
	int     s = splbio();

	uha = (struct uha_data *)mscp->xs->sc_link->adapter_softc;
	printf("uha%d:%d:%d (%s%d) timed out ", uha->unit
	    ,mscp->xs->sc_link->target
	    ,mscp->xs->sc_link->lun
	    ,mscp->xs->sc_link->device->name
	    ,mscp->xs->sc_link->dev_unit);

#ifdef	UHADEBUG
	uha_print_active_mscp(uha);
#endif /*UHADEBUG */

	if ((uha_abort(uha, mscp) != 1) || (mscp->flags = MSCP_ABORTED)) {
		printf("AGAIN");
		mscp->xs->retries = 0;	/* I MEAN IT ! */
		uha_done(uha, mscp);
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
uha_print_active_mscp(struct uha_data *uha)
{
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
