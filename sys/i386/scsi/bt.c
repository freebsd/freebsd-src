/*
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
 *      $Id: bt.c,v 1.4 1996/01/07 19:24:36 gibbs Exp $
 */

/*
 * Bulogic/Bustek 32 bit Addressing Mode SCSI driver.
 *
 * NOTE: 1. Some bt5xx card can NOT handle 32 bit addressing mode.
 *       2. OLD bt445s Revision A,B,C,D(nowired) + any firmware version
 *          has broken busmaster for handling 32 bit addressing on H/W bus
 *	    side.
 *
 *       3. Extended probing still needs confirmation from our user base, due
 *	    to several H/W and firmware dependencies. If you have a problem
 *	    with extended probing, please contact 'amurai@spec.co.jp'
 *
 *						amurai@spec.co.jp 94/6/16
 */

#include <sys/param.h>
#include <sys/systm.h> 

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
 
#include <machine/clock.h>
 
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
 
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <i386/scsi/btreg.h>

struct bt_data *btdata[NBT];

/*
 * I/O Port Interface
 */

#define	BT_BASE			bt->bt_base
#define	BT_CTRL_STAT_PORT	(BT_BASE + 0x0)		/* control & status */
/*			      ReadOps WriteOps			*/
#define 	BT_HRST			0x80	/* Hardware reset */
#define 	BT_SRST			0x40	/* Software reset */
#define		BT_IRST			0x20	/* Interrupt reset */
#define		BT_SCRST		0x10	/* SCSI bus reset */
#define		BT_STST		0x80		/* Self test in Progress */
#define		BT_DIAGF	0x40		/* Diagnostic Failure */
#define		BT_INIT		0x20		/* Mbx Init required */
#define		BT_IDLE		0x10		/* Host Adapter Idle */
#define		BT_CDF		0x08		/* cmd/data out port full */
#define		BT_DF		0x04		/* Data in port full */
#define		BT_INVDCMD	0x01		/* Invalid command */

#define	BT_CMD_DATA_PORT	(BT_BASE + 0x1)		/* cmds and datas */
/*			      ReadOps WriteOps			*/
#define		BT_NOP			0x00	/* No operation */
#define		BT_MBX_INIT		0x01	/* Mbx initialization */
#define		BT_START_SCSI		0x02	/* start scsi command */
#define		BT_START_BIOS		0x03	/* start bios command */
#define		BT_INQUIRE		0x04	/* Adapter Inquiry */
#define		BT_MBO_INTR_EN		0x05	/* Enable MBO available intr */
#define		BT_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define		BT_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define		BT_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define		BT_SPEED_SET		0x09	/* set transfer speed */
#define		BT_DEV_GET		0x0a	/* return installed devices */
#define		BT_CONF_GET		0x0b	/* return configuration data */
#define		BT_TARGET_EN		0x0c	/* enable target mode */
#define		BT_SETUP_GET		0x0d	/* return setup data */
#define		BT_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define		BT_READ_CH2		0x1b	/* read channel 2 buffer */
#define		BT_WRITE_FIFO		0x1c	/* write fifo buffer */
#define		BT_READ_FIFO		0x1d	/* read fifo buffer */
#define		BT_ECHO			0x1e	/* Echo command data */
#define		BT_MBX_INIT_EXTENDED	0x81	/* Mbx initialization */
#define		BT_INQUIRE_REV_THIRD	0x84	/* Get FirmWare version #3 */
#define		BT_INQUIRE_REV_FOURTH	0x85	/* Get FirmWare version #4 */
#define		BT_INQUIRE_EXTENDED	0x8D	/* Adapter Setup Inquiry */
/* The following commands appeared at FirmWare 3.31 */
#define		BT_ROUND_ROBIN		0x8f	/* Enable/Disable round robin */
#define		BT_STRICT_ROUND_ROBIN	0x00	/* Parameter for strict mode */
#define		BT_AGRES_ROUND_ROBIN	0x01	/* Parameter for back compat */

#define	BT_INTR_PORT		(BT_BASE + 0x2)		/* Intr. stat */
/*			      ReadOps WriteOps			*/
#define		BT_ANY_INTR	0x80		/* Any interrupt */
#define		BT_SCRD		0x08		/* SCSI reset detected */
#define		BT_HACC		0x04		/* Command complete */
#define		BT_MBOA		0x02		/* MBX out empty */
#define		BT_MBIF		0x01		/* MBX in full */

struct bt_cmd_buf {
	u_char  byte[16];
};


#define	CCB_HASH_SHIFT	9	/* only hash on multiples of 512 */
#define CCB_HASH(x)	((((long int)(x))>>CCB_HASH_SHIFT) % CCB_HASH_SIZE)

#define bt_nextmbx( wmb, mbx, mbio ) \
	if ( (wmb) == &((mbx)->mbio[BT_MBX_SIZE - 1 ]) ) \
		(wmb) = &((mbx)->mbio[0]); \
	else \
		(wmb)++;

struct bt_boardID {
	u_char  board_type;
	u_char  custom_feture;
	char    firm_revision;
	u_char  firm_version;
};

struct bt_setup {
	u_char  sync_neg:1;
	u_char  parity:1;
	u_char	:6;
	u_char  speed;
	u_char  bus_on;
	u_char  bus_off;
	u_char  num_mbx;
	u_char  mbx[3];		/* for backwards compatibility */
	struct {
		u_char  offset:4;
		u_char  period:3;
		u_char  valid:1;
	} sync[8];
	u_char  disc_sts;
};

struct bt_config {
	u_char  chan;
#define		BUSDMA	0x00
#define		CHAN0	0x01
#define		CHAN5	0x20
#define		CHAN6	0x40
#define		CHAN7	0x80
	u_char  intr;
#define		INT9	0x01
#define		INT10	0x02
#define		INT11	0x04
#define		INT12	0x08
#define		INT14	0x20
#define		INT15	0x40
	u_char  scsi_dev:3;	/* XXX What about Wide Controllers? */
	u_char	:5;
};



/*
 * Determine 32bit address/Data firmware functionality from the bus type
 * Note: bt742a/747[s|d]/757/946/445s will return 'E'
 *       bt542b/545s/545d will return 'A'
 *				94/05/18 amurai@spec.co.jp
 */
struct bt_ext_info {
	u_char  bus_type;	/* Host adapter bus type */
#define		BT_BUS_TYPE_24bit 'A' 	/* PC/AT 24 bit address bus type */
#define		BT_BUS_TYPE_32bit 'E' 	/* EISA/VLB/PCI 32 bit address type */
#define		BT_BUS_TYPE_MCA   'M'   /* Micro chanel? */
	u_char  bios_addr;	/* Bios Address-Not used */
	u_short max_seg;	/* Max segment List */
	u_char  num_mbx;	/* Number of mailbox */
	int32	mbx_base;	/* mailbox base address */
	struct	{
		u_char  resv1:1;	/* ??? */
		u_char  force:1;	/* ON: force sync */
		u_char	maxsync:1;	/* ON: 10MB/s , OFF: 5MB/s */
		u_char	resv2:2;	/* ??? */
		u_char	sync:1;		/* ON: Sync,  OFF: async ONLY!! */
		u_char	resv3:2;	/* ??? */
	} s;
	u_char  firmid[3];	/* Firmware ver. & rev. w/o last char */
};

#define BT_GET_BOARD_INFO	0x8b	/* Get H/W ID and Revision */
struct bt_board_info {
	u_char	id[4];		/* i.e bt742a -> '7','4','2','A'  */
	u_char	ver[2];		/* i.e Board Revision 'H' -> 'H', 0x00 */
};

#define BT_GET_SYNC_VALUE	0x8c	/* Get Synchronous Value */
struct bt_sync_value {
	u_char	value[8];	/* Synchrnous value (value * 10 nsec) */
};


#define KVTOPHYS(x)	vtophys(x)
#define PAGESIZ		4096
#define INVALIDATE_CACHE {asm volatile( ".byte	0x0F ;.byte 0x08" ); }

/***********debug values *************/
#define	BT_SHOWCCBS 0x01
#define	BT_SHOWINTS 0x02
#define	BT_SHOWCMDS 0x04
#define	BT_SHOWMISC 0x08
static int		bt_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, bt_debug, CTLFLAG_RW, &bt_debug, 0, "");

static u_int32	bt_adapter_info __P((int unit));
static struct bt_ccb *
		bt_ccb_phys_kv __P((struct bt_data *bt, physaddr ccb_phys));
#ifdef notyet
static int	bt_cmd __P((int unit, int icnt, int ocnt, int wait,
			    u_char *retval, unsigned opcode, ...));
#else
static int	bt_cmd();
#endif
static void	bt_done __P((struct bt_data *bt, struct bt_ccb *ccb));
static void	bt_free_ccb __P((struct bt_data *bt, struct bt_ccb *ccb,
				 int flags));
static struct bt_ccb *
		bt_get_ccb __P((struct bt_data *bt, int flags));
static void	bt_inquire_setup_information __P((struct bt_data *bt,
						  struct bt_ext_info *info));
static void	btminphys __P((struct buf *bp));
static int	bt_poll __P((struct bt_data *bt, struct scsi_xfer *xs,
			     struct bt_ccb *ccb));
#ifdef UTEST
static void	bt_print_active_ccbs __P((int unit));
static void	bt_print_ccb __P((struct bt_ccb *ccb));
#endif
static int32	bt_scsi_cmd __P((struct scsi_xfer *xs));
static BT_MBO *	bt_send_mbo __P((struct bt_data *bt, int flags, int cmd,
				 struct bt_ccb *ccb));
static timeout_t
		bt_timeout;

u_long bt_unit = 0;
static int btprobing = 1;

/*
 * XXX
 * Do our own re-probe protection until a configuration
 * manager can do it for us.  This ensures that we don't
 * reprobe a card already found by the EISA or PCI probes.
 */
struct bt_found
{
	u_long	port;
	char	probed;
};

static struct bt_found found[] =
{
	{ 0x330, 0 },
	{ 0x334, 0 },
	{ 0x230, 0 },
	{ 0x234, 0 },
	{ 0x130, 0 },
	{ 0x134, 0 }
};

static struct scsi_adapter bt_switch =
{
    bt_scsi_cmd,
    btminphys,
    0,
    0,
    bt_adapter_info,
    "bt",
    { 0, 0 }
};

/* the below structure is so we have a default dev struct for out link struct */
static struct scsi_device bt_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "bt",
    0,
    { 0, 0 }
};

#define BT_RESET_TIMEOUT 1000

/*
 * bt_cmd(bt, icnt, ocnt, wait, retval, opcode, args)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes written after opcode)
 *    ocnt:   number of expected returned bytes
 *    wait:   number of seconds to wait for response
 *    retval: buffer where to place returned bytes
 *    opcode: opcode BT_NOP, BT_MBX_INIT, BT_START_SCSI ...
 *    args:   parameters
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the dma; one of the adapter commands
 * tells it to read in a scsi command.
 */
static int
bt_cmd(bt, icnt, ocnt, wait, retval, opcode, args)
	struct bt_data* bt;
	int icnt;
	int ocnt;
	int wait;
	u_char		*retval;
	unsigned	opcode;
	u_char		args;
{
	unsigned	*ic = &opcode;
	u_char		oc;
	register	i;
	int		sts;

	/*
	 * multiply the wait argument by a big constant
	 * zero defaults to 1
	 */
	if (wait)
		wait *= 100000;
	else
		wait = 100000;
	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != BT_MBX_INIT && opcode != BT_START_SCSI) {
		i = 100000;	/* 1 sec? */
		while (--i) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts & BT_IDLE) {
				break;
			}
			DELAY(10);
		}
		if (i == 0) {
			if(!btprobing)
				printf("bt%d: bt_cmd, host not idle(0x%x)\n",
					bt->unit, sts);
			return (ENXIO);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(BT_CTRL_STAT_PORT)) & BT_DF)
			inb(BT_CMD_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	icnt++;
				/* include the command */
	while (icnt--) {
		sts = inb(BT_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (!(sts & BT_CDF))
				break;
			DELAY(10);
		}
		if (i == 0) {
			if(!btprobing)
				printf("bt%d: bt_cmd, cmd/data port full\n",
					bt->unit);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return (ENXIO);
		}
		outb(BT_CMD_DATA_PORT, (u_char) (*ic++));
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		sts = inb(BT_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(BT_CTRL_STAT_PORT);
			if (sts & BT_DF)
				break;
			DELAY(10);
		}
		if (i == 0) {
			if(!btprobing)
				printf("bt%d: bt_cmd, cmd/data port empty %d\n",
				    bt->unit, ocnt);
			return (ENXIO);
		}
		oc = inb(BT_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*
	 * Wait for the board to report a finised instruction
	 */
	i = 100000;	/* 1 sec? */
	while (--i) {
		sts = inb(BT_INTR_PORT);
		if (sts & BT_HACC) {
			break;
		}
		DELAY(10);
	}
	if (i == 0) {
		if(!btprobing)
			printf("bt%d: bt_cmd, host not finished(0x%x)\n",
				bt->unit, sts);
		return (ENXIO);
	}
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return (0);
}

struct bt_data *
bt_alloc(unit, iobase)
	int     unit;
	u_long  iobase;
{
	struct  bt_data *bt;  
	int	i;

	if (unit >= NBT) {     
		printf("bt: unit number (%d) too high\n", unit);
		return NULL;
	}

        /*
	 * Allocate a storage area for us
	 */
	if (btdata[unit]) {    
		printf("bt%d: memory already allocated\n", unit);
		return NULL;    
	}

	/*
	 * Ensure that we haven't already been probed
	 */
	for (i=0; i < sizeof(found)/sizeof(struct bt_found); i++) {
		if (found[i].port == iobase) {
			if (found[i].probed)
				return NULL;
			else {
				found[i].probed = 1;
				break;
			}
		}
	}
	if (i >= sizeof(found)/sizeof(struct bt_found)) {
		printf("bt%d: Invalid base address\n", unit);
		return NULL;    
	}
	
	bt = malloc(sizeof(struct bt_data), M_DEVBUF, M_NOWAIT);
	if (!bt) {
		printf("bt%d: cannot malloc!\n", unit);
		return NULL;    
	}
	bzero(bt, sizeof(struct bt_data));
	btdata[unit] = bt;    
	bt->unit = unit;
	bt->bt_base = iobase;

	return(bt);
}

void
bt_free(bt)
	struct bt_data *bt;
{
	btdata[bt->unit] = NULL;
	free(bt, M_DEVBUF);
	return;  
}


int
bt_attach(bt)
	struct bt_data *bt;
{
	struct	scsibus_data *scbus;

	btprobing = 0;
	/*
	 * fill in the prototype scsi_link.
	 */
	bt->sc_link.adapter_unit = bt->unit;
	bt->sc_link.adapter_targ = bt->bt_scsi_dev;
	bt->sc_link.adapter_softc = bt;
	bt->sc_link.adapter = &bt_switch;
	bt->sc_link.device = &bt_dev;
	bt->sc_link.flags = bt->bt_bounce ? SDEV_BOUNCE : 0;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	/* XXX scbus->magtarg should be adjusted for Wide cards */
	if(!scbus)
		return 0;
	scbus->adapter_link = &bt->sc_link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);
	return 1;
}

/*
 * Return some information to the caller about the adapter and its
 * capabilities.
 */
static u_int32
bt_adapter_info(unit)
	int	unit;
{
	return (2);		/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
void
bt_intr(arg)
	void	*arg;
{
	BT_MBI *wmbi;
	struct bt_mbx *wmbx;
	struct bt_ccb *ccb;
	unsigned char stat;
	int     i, wait;
	int     found = 0;
	struct bt_data *bt;

	bt = (struct bt_data *)arg;

#ifdef UTEST
	printf("bt_intr ");
#endif
	/*
	 * First acknowlege the interrupt, Then if it's
	 * not telling about a completed operation
	 * just return.
	 */
	stat = inb(BT_INTR_PORT);

	/* Mail Box out empty ? */
	if (stat & BT_MBOA) {
		printf("bt%d: Available Free mbo post\n", bt->unit);
		/* Disable MBO available interrupt */
		outb(BT_CMD_DATA_PORT, BT_MBO_INTR_EN);
		wait = 100000;	/* 1 sec enough? */
		for (i = wait; i; i--) {
			if (!(inb(BT_CTRL_STAT_PORT) & BT_CDF))
				break;
			DELAY(10);
		}
		if (i == 0) {
			printf("bt%d: bt_intr, cmd/data port full\n", bt->unit);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return;
		}
		outb(BT_CMD_DATA_PORT, 0x00);	/* Disable */
		wakeup((caddr_t)&bt->bt_mbx);
		outb(BT_CTRL_STAT_PORT, BT_IRST);
		return;
	}
	if (!(stat & BT_MBIF)) {
		outb(BT_CTRL_STAT_PORT, BT_IRST);
		return;
	}
	/*
	 * If it IS then process the competed operation
	 */
	wmbx = &bt->bt_mbx;
	wmbi = wmbx->tmbi;
      AGAIN:
	while (wmbi->stat != BT_MBI_FREE) {
		ccb = bt_ccb_phys_kv(bt, (wmbi->ccb_addr));
		if (!ccb) {
			wmbi->stat = BT_MBI_FREE;
			printf("bt: BAD CCB ADDR!\n");
			continue;
		}
		found++;
		if ((stat = wmbi->stat) != BT_MBI_OK) {
			switch (stat) {
			case BT_MBI_ABORT:
#ifdef UTEST
				if (bt_debug & BT_SHOWMISC)
					printf("abort ");
#endif
				ccb->host_stat = BT_ABORTED;
				break;

			case BT_MBI_UNKNOWN:
				ccb = (struct bt_ccb *) 0;
#ifdef UTEST
				if (bt_debug & BT_SHOWMISC)
					printf("unknown ccb for abort");
#endif
				break;

			case BT_MBI_ERROR:
				break;

			default:
				panic("Impossible mbxi status");

			}
#ifdef UTEST
			 if ((bt_debug & BT_SHOWCMDS) && ccb) {
				u_char *cp;
				cp = ccb->scsi_cmd;
				printf("op=%x %x %x %x %x %x\n",
				    cp[0], cp[1], cp[2],
				    cp[3], cp[4], cp[5]);
				printf("stat %x for mbi addr = 0x%08x\n"
				    ,wmbi->stat, wmbi);
				printf("addr = 0x%x\n", ccb);
			}
#endif
		}
		wmbi->stat = BT_MBI_FREE;
		if (ccb) {
			untimeout(bt_timeout, (caddr_t)ccb);
			bt_done(bt, ccb);
		}
		/* Set the IN mail Box pointer for next */ bt_nextmbx(wmbi, wmbx, mbi);
	}
	if (!found) {
		for (i = 0; i < BT_MBX_SIZE; i++) {
			if (wmbi->stat != BT_MBI_FREE) {
				found++;
				break;
			}
			bt_nextmbx(wmbi, wmbx, mbi);
		}
		if (!found) {
#ifdef DEBUG
			printf("bt%d: mbi at 0x%08x should be found, stat=%02x..resync\n",
			    bt->unit, wmbi, stat);
#endif
		} else {
			found = 0;
			goto AGAIN;
		}
	}
	wmbx->tmbi = wmbi;
	outb(BT_CTRL_STAT_PORT, BT_IRST);
}

/*
 * A ccb is put onto the free list.
 */
static void
bt_free_ccb(bt, ccb, flags)
	struct bt_data *bt;
	struct bt_ccb *ccb;
	int flags;
{
	unsigned int opri;

	opri = splbio();

	ccb->next = bt->bt_ccb_free;
	bt->bt_ccb_free = ccb;
	ccb->flags = CCB_FREE;
	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (!ccb->next) {
		wakeup((caddr_t)&bt->bt_ccb_free);
	}

	splx(opri);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
static struct bt_ccb *
bt_get_ccb(bt, flags)
	struct bt_data *bt;
	int flags;
{
	unsigned opri;
	struct bt_ccb *ccbp;
	int     hashnum;

	opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	while (!(ccbp = bt->bt_ccb_free)) {
		if (bt->numccbs < BT_CCB_MAX) {
			if (ccbp = (struct bt_ccb *) malloc(sizeof(struct bt_ccb),
				M_TEMP,
				M_NOWAIT)) {
				bzero(ccbp, sizeof(struct bt_ccb));
				bt->numccbs++;
				ccbp->flags = CCB_ACTIVE;
				/*
				 * put in the phystokv hash table
				 * Never gets taken out.
				 */
				ccbp->hashkey = KVTOPHYS(ccbp);
				hashnum = CCB_HASH(ccbp->hashkey);
				ccbp->nexthash = bt->ccbhash[hashnum];
				bt->ccbhash[hashnum] = ccbp;
			} else {
				printf("bt%d: Can't malloc CCB\n", bt->unit);
			}
			goto gottit;
		} else {
			if (!(flags & SCSI_NOSLEEP)) {
				tsleep((caddr_t)&bt->bt_ccb_free, PRIBIO,
				       "btccb", 0);
				continue;
			}
			break;
		}
	}
	if (ccbp) {
		/* Get CCB from from free list */
		bt->bt_ccb_free = ccbp->next;
		ccbp->flags = CCB_ACTIVE;
	}
      gottit:
	splx(opri);

	return (ccbp);
}

/*
 * given a physical address, find the ccb that
 * it corresponds to:
 */
static struct bt_ccb *
bt_ccb_phys_kv(bt, ccb_phys)
	struct bt_data *bt;
	physaddr ccb_phys;
{
	int     hashnum = CCB_HASH(ccb_phys);
	struct bt_ccb *ccbp = bt->ccbhash[hashnum];

	while (ccbp) {
		if (ccbp->hashkey == ccb_phys)
			break;
		ccbp = ccbp->nexthash;
	}
	return ccbp;
}

/*
 * Get a MBO and then Send it
 */
static BT_MBO *
bt_send_mbo(bt, flags, cmd, ccb)
	struct	bt_data *bt;
	int	flags;
	int	cmd;
	struct	bt_ccb *ccb;
{
	unsigned opri;
	BT_MBO	*wmbo;		/* Mail Box Out pointer */
	struct	bt_mbx *wmbx;	/* Mail Box pointer specified unit */
	int     i, wait;

	wmbx = &bt->bt_mbx;

	opri = splbio();

	/* Get the Target OUT mail Box pointer and move to Next */
	wmbo = wmbx->tmbo;
	wmbx->tmbo = (wmbo == &(wmbx->mbo[BT_MBX_SIZE - 1]) ?
	    &(wmbx->mbo[0]) : wmbo + 1);

	/*
	 * Check the outmail box is free or not.
	 * Note: Under the normal operation, it shuld NOT happen to wait.
	 */
	while (wmbo->cmd != BT_MBO_FREE) {
		wait = 100000;	/* 1 sec enough? */
		/* Enable MBO available interrupt */
		outb(BT_CMD_DATA_PORT, BT_MBO_INTR_EN);
		for (i = wait; i; i--) {
			if (!(inb(BT_CTRL_STAT_PORT) & BT_CDF))
				break;
			DELAY(10);
		}
		if (i == 0) {
			printf("bt%d: bt_send_mbo, cmd/data port full\n", bt->unit);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return ((BT_MBO *) 0);
		}
		outb(BT_CMD_DATA_PORT, 0x01);	/* Enable */
		tsleep((caddr_t)wmbx, PRIBIO, "btsend", 0);
		/* XXX */ /*can't do this! */
		/* May be servicing an int */
	}
	/* Link CCB to the Mail Box */
	wmbo->ccb_addr = KVTOPHYS(ccb);
	ccb->mbx = wmbo;
	wmbo->cmd = cmd;

	/* Send it! */
	outb(BT_CMD_DATA_PORT, BT_START_SCSI);

	splx(opri);

	return (wmbo);
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
static void
bt_done(bt, ccb)
	struct  bt_data *bt;
	struct	bt_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xfer;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bt_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((ccb->host_stat != BT_OK || ccb->target_stat != SCSI_OK)
	    && (!(xs->flags & SCSI_ERR_OK))) {

		s1 = &(ccb->scsi_sense);
		s2 = &(xs->sense);

		if (ccb->host_stat) {
			switch (ccb->host_stat) {
			case BT_ABORTED:	/* No response */
			case BT_SEL_TIMEOUT:	/* No response */
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("timeout reported back\n"));
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("unexpected host_stat: %x\n",
					ccb->host_stat));
			}
		} else {
			switch (ccb->target_stat) {
			case 0x02:
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case 0x08:
				xs->error = XS_BUSY;
				break;
			default:
				SC_DEBUG(xs->sc_link, SDEV_DB3,
				    ("unexpected target_stat: %x\n",
					ccb->target_stat));
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	} else {		/* All went correctly  OR errors expected */
		xs->resid = 0;
	}
	xs->flags |= ITSDONE;
	bt_free_ccb(bt, ccb, xs->flags);
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
int
bt_init(bt)
	struct bt_data* bt;
{
	unsigned char ad[4];
	volatile int i, sts;
	struct bt_config conf;
	struct bt_ext_info info;
	struct bt_board_info binfo;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(BT_CTRL_STAT_PORT, BT_HRST | BT_SRST);

	DELAY(10000);

	for (i = BT_RESET_TIMEOUT; i; i--) {
		sts = inb(BT_CTRL_STAT_PORT);
		if (sts == (BT_IDLE | BT_INIT))
			break;
		DELAY(1000);
	}
	if (i == 0) {
#ifdef	UTEST
		printf("bt_init: No answer from board\n");
#endif
		return (ENXIO);
	}

	DELAY(10000);

	/*
         * Displaying Board ID and Hardware Revision
         *                                   94/05/18 amurai@spec.co.jp
         */
	i = bt_cmd(bt, 1, sizeof(binfo),0,
		&binfo,BT_GET_BOARD_INFO,sizeof(binfo));
	if(i)
		return i;
	printf("bt%d: Bt%c%c%c%c/%c%d-", bt->unit,
				binfo.id[0],
				binfo.id[1],
				binfo.id[2],
				binfo.id[3],
				binfo.ver[0],
				(unsigned) binfo.ver[1]
				);

	/*
         * Make sure board has a capability of 32bit addressing.
         *   and Firmware also need a capability of 32bit addressing pointer
         *   in Extended mailbox and ccb structure.
         *                                   94/05/18 amurai@spec.co.jp
         */
	bt_cmd(bt, 1, sizeof(info),0,&info, BT_INQUIRE_EXTENDED,sizeof(info));
	switch (info.bus_type) {
		case BT_BUS_TYPE_24bit:		/* PC/AT 24 bit address bus */
			printf("ISA(24bit) bus\n");
			break;
		case BT_BUS_TYPE_32bit:		/* EISA/VLB/PCI 32 bit bus */
			printf("(32bit) bus\n");
			break;
		case BT_BUS_TYPE_MCA:           /* forget it right now */
			printf("MCA bus architecture...");
			printf("giving up\n");
			return (ENXIO);
			break;
		default:
			printf("Unknown state...");
			printf("giving up\n");
			return (ENXIO);
			break;
	}

	if ( binfo.id[0] == '4' && binfo.id[1] == '4' && binfo.id[2] == '5' &&
	     binfo.id[3] == 'S' ) {
		printf("bt%d: Your card cannot DMA above 16MB boundary. Bounce buffering enabled.\n", bt->unit);
		bt->bt_bounce++;
	} else if ( binfo.id[0] == '5' ) {
		printf("bt%d: This driver is designed for using 32 bit addressing\n"
			"bt%d: mode firmware and EISA/PCI/VLB bus architectures\n"
			"bt%d: Bounce-buffering will be used (and is necessary)\n"
			"bt%d: if you have more than 16MBytes memory.\n",
			bt->unit,
			bt->unit,
			bt->unit,
			bt->unit);
		bt->bt_bounce++;
	} else if ( info.bus_type == BT_BUS_TYPE_24bit ) {
		printf("bt%d: Your board should report a 32bit bus architecture type..\n"
			"bt%d: The firmware on your board may have a problem with over\n"
			"bt%d: 16MBytes memory handling with this driver.\n",
			bt->unit,
			bt->unit,
			bt->unit);
		bt->bt_bounce++;
	}

	/*
	 * Assume we have a board at this stage
	 * setup dma channel from jumpers and save int
	 * level
	 */
	printf("bt%d: reading board settings, ", bt->unit);

	bt_cmd(bt, 0, sizeof(conf), 0, &conf, BT_CONF_GET);
	switch (conf.chan) {
	case BUSDMA:
		bt->bt_dma = -1;
		break;
	case CHAN0:
		outb(0x0b, 0x0c);
		outb(0x0a, 0x00);
		bt->bt_dma = 0;
		break;
	case CHAN5:
		outb(0xd6, 0xc1);
		outb(0xd4, 0x01);
		bt->bt_dma = 5;
		break;
	case CHAN6:
		outb(0xd6, 0xc2);
		outb(0xd4, 0x02);
		bt->bt_dma = 6;
		break;
	case CHAN7:
		outb(0xd6, 0xc3);
		outb(0xd4, 0x03);
		bt->bt_dma = 7;
		break;
	default:
		printf("illegal dma setting %x\n", conf.chan);
		return (EIO);
	}
	if (bt->bt_dma == -1)
		printf("busmastering, ");
	else
		printf("dma=%d, ", bt->bt_dma);

	switch (conf.intr) {
	case INT9:
		bt->bt_int = 9;
		break;
	case INT10:
		bt->bt_int = 10;
		break;
	case INT11:
		bt->bt_int = 11;
		break;
	case INT12:
		bt->bt_int = 12;
		break;
	case INT14:
		bt->bt_int = 14;
		break;
	case INT15:
		bt->bt_int = 15;
		break;
	default:
		printf("illegal int setting\n");
		return (EIO);
	}
	printf("int=%d\n", bt->bt_int);

	/* who are we on the scsi bus */
	bt->bt_scsi_dev = conf.scsi_dev;
	/*
	 * Initialize mail box
	 */
	*((physaddr *) ad) = KVTOPHYS(&bt->bt_mbx);
	bt_cmd(bt, 5, 0, 0, 0, BT_MBX_INIT_EXTENDED
	    ,BT_MBX_SIZE
	    ,ad[0]
	    ,ad[1]
	    ,ad[2]
	    ,ad[3]);

	/*
	 * Set Pointer chain null for just in case
	 * Link the ccb's into a free-list W/O mbox
	 * Initialize mail box status to free
	 */
	if (bt->bt_ccb_free != (struct bt_ccb *) 0) {
		printf("bt%d: bt_ccb_free is NOT initialized but init here\n",
		    bt->unit);
		bt->bt_ccb_free = (struct bt_ccb *) 0;
	}
	for (i = 0; i < BT_MBX_SIZE; i++) {
		bt->bt_mbx.mbo[i].cmd = BT_MBO_FREE;
		bt->bt_mbx.mbi[i].stat = BT_MBI_FREE;
	}
	/*
	 * Set up initial mail box for round-robin operation.
	 */
	bt->bt_mbx.tmbo = &bt->bt_mbx.mbo[0];
	bt->bt_mbx.tmbi = &bt->bt_mbx.mbi[0];
	bt_inquire_setup_information(bt, &info);


	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

static void
bt_inquire_setup_information(bt, info)
	struct	bt_data* bt;
	struct  bt_ext_info *info;
{
	struct	bt_setup setup;
	struct	bt_sync_value sync;
	char	dummy[8];
	char	sub_ver[3];
	struct	bt_boardID bID;
	int	i;

	/* Inquire Installed Devices */
	bzero( &dummy[0], sizeof(dummy) );
        bt_cmd(bt, 0, sizeof(dummy), 100, &dummy[0], BT_DEV_GET);

	/*
	 * If board has a capbility of Syncrhonouse mode,
         * Get a SCSI Synchronous value
	 */

 	if (info->s.force) {                 /* Assume fast sync capability */
             info->s.sync    = 1;            /* It's appear at 4.25? version */
             info->s.maxsync = 1;
        }
	if ( info->s.sync ) {
        	bt_cmd(bt, 1, sizeof(sync), 100,
				&sync,BT_GET_SYNC_VALUE,sizeof(sync));
	}

	/*
	 * Inquire Board ID to board for firmware version
	 */
	bt_cmd(bt, 0, sizeof(bID), 0, &bID, BT_INQUIRE);
	bt_cmd(bt, 0, 1, 0, &sub_ver[0], BT_INQUIRE_REV_THIRD );
	i = ((int)(bID.firm_revision-'0')) * 10 + (int)(bID.firm_version-'0');
	if ( i >= 33 ) {
		bt_cmd(bt, 0, 1, 0, &sub_ver[1], BT_INQUIRE_REV_FOURTH );
	} else {
		/*
                 * Below rev 3.3 firmware has a problem for issuing
		 * the BT_INQUIRE_REV_FOURTH command.
 		 */
		sub_ver[1]='\0';
	}
	sub_ver[2]='\0';
	if (sub_ver[1]==' ')
		sub_ver[1]='\0';
	printf("bt%d: version %c.%c%s, ",
	    bt->unit, bID.firm_revision, bID.firm_version, sub_ver );

	/*
	 * Obtain setup information from board.
	 */
	bt_cmd(bt, 1, sizeof(setup), 0, &setup, BT_SETUP_GET, sizeof(setup));

	if (setup.sync_neg && info->s.sync ) {
		if ( info->s.maxsync ) {
			printf("fast sync, ");	/* Max 10MB/s */
		} else {
			printf("sync, ");	/* Max 5MB/s */
		}
	} else {
		if ( info->s.sync ) {
			printf("async, ");	/* Never try by board */
		} else {
			printf("async only, "); /* Doesn't has a capability on board */
		}
	}
	if (setup.parity) {
		printf("parity, ");
	} else {
		printf("no parity, ");
	}
	printf("%d mbxs, %d ccbs\n", setup.num_mbx, BT_CCB_MAX);

	/*
	 * Displayi SCSI negotiation value by each target.
         *   						amurai@spec.co.jp
         */
	for (i = 0; i < 8; i++) {
		if (!setup.sync[i].valid )
			continue;
		if ( (!setup.sync[i].offset && !setup.sync[i].period)
					    || !info->s.sync ) {
			printf("bt%d: targ %d async\n", bt->unit, i);
		} else {
			printf("bt%d: targ %d sync rate=%2d.%02dMB/s(%dns), offset=%02d\n",
		    	    bt->unit, i,
			    100 / sync.value[i],
			    (100 % sync.value[i]) * 100 / sync.value[i],
			    sync.value[i] * 10,
		    	    setup.sync[i].offset );
		}
	}

	/*
         * Enable round-robin scheme - appeared at firmware rev. 3.31
	 *   Below rev 3.XX firmware has a problem for issuing
         *    BT_ROUND_ROBIN command  amurai@spec.co.jp
	 */
	if ( bID.firm_revision >= '3' ) {
		printf("bt%d: Using Strict Round robin scheme\n", bt->unit);
		bt_cmd(bt, 1, 0, 0, 0, BT_ROUND_ROBIN, BT_STRICT_ROUND_ROBIN);
	} else {
		printf("bt%d: Not using Strict Round robin scheme\n", bt->unit);
	}

}

#ifndef	min
#define min(x,y) (x < y ? x : y)
#endif	/* min */

static void
btminphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > ((BT_NSEG - 1) * PAGESIZ)) {
		bp->b_bcount = ((BT_NSEG - 1) * PAGESIZ);
	}
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
static int32
bt_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	bt_ccb *ccb;
	struct	bt_scat_gath *sg;
	int	seg;		/* scatter gather seg being worked on */
	int	thiskv;
	physaddr thisphys, nextphys;
	int	bytes_this_seg, bytes_this_page, datalen, flags;
	struct	bt_data *bt;

	bt = (struct bt_data *)xs->sc_link->adapter_softc;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bt_scsi_cmd\n"));
	/*
	 * get a ccb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (flags & ITSDONE) {
		printf("bt%d: Already done?\n", bt->unit);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("bt%d: Not in use?\n", bt->unit);
		xs->flags |= INUSE;
	}
	if (!(ccb = bt_get_ccb(bt, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	SC_DEBUG(xs->sc_link, SDEV_DB3,
	    ("start ccb(%p)\n", ccb));
	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	ccb->xfer = xs;
	if (flags & SCSI_RESET) {
		ccb->opcode = BT_RESET_CCB;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ?
		    BT_INIT_SCAT_GATH_CCB
		    : BT_INITIATOR_CCB);
	}
	ccb->target = xs->sc_link->target;
	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->lun = xs->sc_link->lun;
	ccb->scsi_cmd_length = xs->cmdlen;
	ccb->sense_ptr = KVTOPHYS(&(ccb->scsi_sense));
	ccb->req_sense_length = sizeof(ccb->scsi_sense);

	if ((xs->datalen) && (!(flags & SCSI_RESET))) {		/* can use S/G only if not zero length */
		ccb->data_addr = KVTOPHYS(ccb->scat_gath);
		sg = ccb->scat_gath;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < BT_NSEG)) {
				sg->seg_addr = (physaddr) iovp->iov_base;
				xs->datalen += sg->seg_len = iovp->iov_len;
				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("(0x%x@0x%x)"
					,iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif	/* TFS */
		{
			/*
			 * Set up the scatter gather block
			 */

			SC_DEBUG(xs->sc_link, SDEV_DB4,
			    ("%ld @%p:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while ((datalen) && (seg < BT_NSEG)) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("0x%lx", thisphys));

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
				sg->seg_len = bytes_this_seg;
				sg++;
				seg++;
			}
		}
		/* end of iov/kv decision */
		ccb->data_length = seg * sizeof(struct bt_scat_gath);
		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("bt%d: bt_scsi_cmd, more than %d DMA segs\n",
			    bt->unit, BT_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			bt_free_ccb(bt, ccb, flags);
			return (HAD_ERROR);
		}
	} else {		/* No data xfer, use non S/G values */
		ccb->data_addr = (physaddr) 0;
		ccb->data_length = 0;
	}
	ccb->link_id = 0;
	ccb->link_addr = (physaddr) 0;
	/*
	 * Put the scsi command in the ccb and start it
	 */
	if (!(flags & SCSI_RESET)) {
		bcopy(xs->cmd, ccb->scsi_cmd, ccb->scsi_cmd_length);
	}
	if (bt_send_mbo(bt, flags, BT_MBO_START, ccb) == (BT_MBO *) 0) {
		xs->error = XS_DRIVER_STUFFUP;
		bt_free_ccb(bt, ccb, flags);
		return (TRY_AGAIN_LATER);
	}
	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
	if (!(flags & SCSI_NOMASK)) {
		timeout(bt_timeout, (caddr_t)ccb, (xs->timeout * hz) / 1000);
		return (SUCCESSFULLY_QUEUED);
	}
	/*
	 * If we can't use interrupts, poll on completion
	 */
	return (bt_poll(bt, xs, ccb));
}

/*
 * Poll a particular unit, looking for a particular xs
 */
static int
bt_poll(bt, xs, ccb)
	struct	bt_data* bt;
	struct	scsi_xfer *xs;
	struct	bt_ccb *ccb;
{
	int	count = xs->timeout;
	u_char	stat;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		stat = inb(BT_INTR_PORT);
		if (stat & BT_ANY_INTR) {
			bt_intr((void *)bt);
		}
		if (xs->flags & ITSDONE) {
			break;
		}
		DELAY(1000);	/* only happens in boot so ok */
		count--;
	}
	if (count == 0) {
		/*
		 * We timed out, so call the timeout handler manually,
		 * accounting for the fact that the clock is not running yet
		 * by taking out the clock queue entry it makes.
		 */
		bt_timeout(ccb);

		/*
		 * because we are polling, take out the timeout entry
		 * bt_timeout made
		 */
		untimeout(bt_timeout, (caddr_t)ccb);
		count = 2000;
		while (count) {
			/*
			 * Once again, wait for the int bit
			 */
			stat = inb(BT_INTR_PORT);
			if (stat & BT_ANY_INTR) {
				bt_intr((void *)bt);
			}
			if (xs->flags & ITSDONE) {
				break;
			}
			DELAY(1000);	/* only happens in boot so ok */
			count--;
		}
		if (count == 0) {
			/*
			 * We timed out again...  This is bad.  Notice that
			 * this time there is no clock queue entry to remove.
			 */
			bt_timeout(ccb);
		}
	}
	if (xs->error)
		return (HAD_ERROR);
	return (COMPLETE);
}

static void
bt_timeout(void *arg1)
{
	struct bt_ccb * ccb = (struct bt_ccb *)arg1;
	int     unit;
	struct bt_data *bt;
	int     s = splbio();

	/*
         * A timeout routine in kernel DONOT unlink
	 * Entry chains when time outed....So infinity Loop..
         *                              94/04/20 amurai@spec.co.jp
         */
	untimeout(bt_timeout, (caddr_t)ccb);

	unit = ccb->xfer->sc_link->adapter_unit;
	bt = btdata[unit];

#ifdef	UTEST
	bt_print_active_ccbs(bt);
#endif

	/*
	 * If the ccb's mbx is not free, then the board has gone Far East?
	 */
	if (bt_ccb_phys_kv(bt, ccb->mbx->ccb_addr) == ccb &&
	    ccb->mbx->cmd != BT_MBO_FREE) {
		printf("bt%d: not taking commands!\n", unit);
		Debugger("bt742a");
	}
	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags == CCB_ABORTED) {
		/*
		 * abort timed out
		 */
		printf("bt%d: Abort Operation has timed out\n", unit);
		ccb->xfer->retries = 0;		/* I MEAN IT ! */
		ccb->host_stat = BT_ABORTED;
		bt_done(bt, ccb);
	} else {
		/* abort the operation that has timed out */
		printf("bt%d: Try to abort\n", unit);
		bt_send_mbo(bt, ~SCSI_NOMASK, BT_MBO_ABORT, ccb);
		/* 2 secs for the abort */
		ccb->flags = CCB_ABORTED;
		timeout(bt_timeout, (caddr_t)ccb, 2 * hz);
	}
	splx(s);
}

#ifdef	UTEST
static void
bt_print_ccb(ccb)
	struct bt_ccb *ccb;
{
	printf("ccb:%x op:%x cmdlen:%d senlen:%d\n"
	    ,ccb
	    ,ccb->opcode
	    ,ccb->scsi_cmd_length
	    ,ccb->req_sense_length);
	printf("	datlen:%d hstat:%x tstat:%x flags:%x\n"
	    ,ccb->data_length
	    ,ccb->host_stat
	    ,ccb->target_stat
	    ,ccb->flags);
}

static void
bt_print_active_ccbs(bt)
	struct bt_data *bt;
{
	struct bt_ccb *ccb;
	int     i = 0;

	while (i < CCB_HASH_SIZE) {
		ccb = bt->ccbhash[i];
		while (ccb) {
			if (ccb->flags != CCB_FREE)
				bt_print_ccb(ccb);
			ccb = ccb->nexthash;
		}
		i++;
	}
}
#endif /*UTEST */
