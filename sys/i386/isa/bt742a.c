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
 *      $Id: bt742a.c,v 1.10 1993/11/18 05:02:17 rgrimes Exp $
 */

/*
 * bt742a SCSI driver
 */

#include <sys/types.h>

#ifdef	KERNEL		/* don't laugh.. it compiles to a program too.. look */
#include <bt.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#endif /* KERNEL */

#include <i386/isa/isa_device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef	KERNEL
#include "ddb.h"
#if	NDDB > 0
int     Debugger();
#else	/* NDDB */
#define	Debugger() panic("should call debugger here (bt742a.c)")
#endif	/* NDDB */
#else /*KERNEL */
#define	NBT 1
#endif /*KERNEL */

extern int hz;
typedef unsigned long int physaddr;

/*
 * I/O Port Interface
 */

#define	BT_BASE			bt->bt_base
#define	BT_CTRL_STAT_PORT	(BT_BASE + 0x0)		/* control & status */
#define	BT_CMD_DATA_PORT	(BT_BASE + 0x1)		/* cmds and datas */
#define	BT_INTR_PORT		(BT_BASE + 0x2)		/* Intr. stat */

/*
 * BT_CTRL_STAT bits (write)
 */

#define BT_HRST		0x80	/* Hardware reset */
#define BT_SRST		0x40	/* Software reset */
#define BT_IRST		0x20	/* Interrupt reset */
#define BT_SCRST	0x10	/* SCSI bus reset */

/*
 * BT_CTRL_STAT bits (read)
 */

#define BT_STST		0x80	/* Self test in Progress */
#define BT_DIAGF	0x40	/* Diagnostic Failure */
#define BT_INIT		0x20	/* Mbx Init required */
#define BT_IDLE		0x10	/* Host Adapter Idle */
#define BT_CDF		0x08	/* cmd/data out port full */
#define BT_DF		0x04	/* Data in port full */
#define BT_INVDCMD	0x01	/* Invalid command */

/*
 * BT_CMD_DATA bits (write)
 */

#define	BT_NOP			0x00	/* No operation */
#define BT_MBX_INIT		0x01	/* Mbx initialization */
#define BT_START_SCSI		0x02	/* start scsi command */
#define BT_START_BIOS		0x03	/* start bios command */
#define BT_INQUIRE		0x04	/* Adapter Inquiry */
#define BT_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define BT_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define BT_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define BT_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define BT_SPEED_SET		0x09	/* set transfer speed */
#define BT_DEV_GET		0x0a	/* return installed devices */
#define BT_CONF_GET		0x0b	/* return configuration data */
#define BT_TARGET_EN		0x0c	/* enable target mode */
#define BT_SETUP_GET		0x0d	/* return setup data */
#define BT_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define BT_READ_CH2		0x1b	/* read channel 2 buffer */
#define BT_WRITE_FIFO		0x1c	/* write fifo buffer */
#define BT_READ_FIFO		0x1d	/* read fifo buffer */
#define BT_ECHO			0x1e	/* Echo command data */
#define BT_MBX_INIT_EXTENDED	0x81	/* Mbx initialization */
#define BT_INQUIRE_EXTENDED	0x8D	/* Adapter Setup Inquiry */

/* Follows command appeared at FirmWare 3.31 */
#define	BT_ROUND_ROBIN	0x8f	/* Enable/Disable(default) round robin */
#define   BT_DISABLE		0x00	/* Parameter value for Disable */
#define   BT_ENABLE		0x01	/* Parameter value for Enable */

struct bt_cmd_buf {
	u_char  byte[16];
};

/*
 * BT_INTR_PORT bits (read)
 */

#define BT_ANY_INTR	0x80	/* Any interrupt */
#define BT_SCRD		0x08	/* SCSI reset detected */
#define BT_HACC		0x04	/* Command complete */
#define BT_MBOA		0x02	/* MBX out empty */
#define BT_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs  etc.
 * these could be bigger but we need the bt_data to fit on a single page..
 */

#define BT_MBX_SIZE	16	/* mail box size  (MAX 255 MBxs) */
				/* don't need that many really */
#define BT_CCB_MAX	32	/* store up to 32CCBs at any one time */
				/* in bt742a H/W ( Not MAX ? ) */
#define	CCB_HASH_SIZE	32	/* when we have a physical addr. for */
				/* a ccb and need to find the ccb in */
				/* space, look it up in the hash table */
#define	CCB_HASH_SHIFT	9	/* only hash on multiples of 512 */
#define CCB_HASH(x)	((((long int)(x))>>CCB_HASH_SHIFT) % CCB_HASH_SIZE)

#define bt_nextmbx( wmb, mbx, mbio ) \
	if ( (wmb) == &((mbx)->mbio[BT_MBX_SIZE - 1 ]) ) \
		(wmb) = &((mbx)->mbio[0]); \
	else \
		(wmb)++;

typedef struct bt_mbx_out {
	physaddr ccb_addr;
	unsigned char dummy[3];
	unsigned char cmd;
} BT_MBO;

typedef struct bt_mbx_in {
	physaddr ccb_addr;
	unsigned char btstat;
	unsigned char sdstat;
	unsigned char dummy;
	unsigned char stat;
} BT_MBI;

struct bt_mbx {
	BT_MBO  mbo[BT_MBX_SIZE];
	BT_MBI  mbi[BT_MBX_SIZE];
	BT_MBO *tmbo;		/* Target Mail Box out */
	BT_MBI *tmbi;		/* Target Mail Box in */
};

/*
 * mbo.cmd values
 */

#define BT_MBO_FREE	0x0	/* MBO entry is free */
#define BT_MBO_START	0x1	/* MBO activate entry */
#define BT_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.stat values
 */

#define BT_MBI_FREE	0x0	/* MBI entry is free */
#define BT_MBI_OK	0x1	/* completed without error */
#define BT_MBI_ABORT	0x2	/* aborted ccb */
#define BT_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define BT_MBI_ERROR	0x4	/* Completed with error */

#if	defined(BIG_DMA)
WARNING...THIS WON'T WORK(won't fit on 1 page)
/* #define      BT_NSEG 2048*/    /* Number of scatter gather segments - to much vm */
#define	BT_NSEG	128
#else
#define	BT_NSEG	33
#endif /* BIG_DMA */

struct bt_scat_gath {
	unsigned long seg_len;
	physaddr seg_addr;
};

struct bt_ccb {
	unsigned char opcode;
	unsigned char:3, data_in:1, data_out:1,:3;
	unsigned char scsi_cmd_length;
	unsigned char req_sense_length;
	/*------------------------------------longword boundary */
	unsigned long data_length;
	/*------------------------------------longword boundary */
	physaddr data_addr;
	/*------------------------------------longword boundary */
	unsigned char dummy[2];
	unsigned char host_stat;
	unsigned char target_stat;
	/*------------------------------------longword boundary */
	unsigned char target;
	unsigned char lun;
	unsigned char scsi_cmd[12];	/* 12 bytes (bytes only) */
	unsigned char dummy2[1];
	unsigned char link_id;
	/*------------------------------------4 longword boundary */
	physaddr link_addr;
	/*------------------------------------longword boundary */
	physaddr sense_ptr;
/*-----end of HW fields-------------------------------longword boundary */
	struct scsi_sense_data scsi_sense;
	/*------------------------------------longword boundary */
	struct bt_scat_gath scat_gath[BT_NSEG];
	/*------------------------------------longword boundary */
	struct bt_ccb *next;
	/*------------------------------------longword boundary */
	struct scsi_xfer *xfer;	/* the scsi_xfer for this cmd */
	/*------------------------------------longword boundary */
	struct bt_mbx_out *mbx;	/* pointer to mail box */
	/*------------------------------------longword boundary */
	int     flags;
#define	CCB_FREE	0
#define CCB_ACTIVE	1
#define	CCB_ABORTED	2
	/*------------------------------------longword boundary */
	struct bt_ccb *nexthash;	/* if two hash the same */
	/*------------------------------------longword boundary */
	physaddr hashkey;	/*physaddr of this ccb */
	/*------------------------------------longword boundary */
};

/*
 * opcode fields
 */

#define BT_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define BT_TARGET_CCB		0x01	/* SCSI Target CCB */
#define BT_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scattter gather */
#define BT_RESET_CCB		0x81	/* SCSI Bus reset */

/*
 * bt_ccb.host_stat values
 */

#define BT_OK		0x00	/* cmd ok */
#define BT_LINK_OK	0x0a	/* Link cmd ok */
#define BT_LINK_IT	0x0b	/* Link cmd ok + int */
#define BT_SEL_TIMEOUT	0x11	/* Selection time out */
#define BT_OVER_UNDER	0x12	/* Data over/under run */
#define BT_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define BT_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define BT_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define BT_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define BT_BAD_LINK	0x17	/* Not same values of LUN for links */
#define BT_INV_TARGET	0x18	/* Invalid target direction */
#define BT_CCB_DUP	0x19	/* Duplicate CCB received */
#define BT_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define BT_ABORTED	42	/* pseudo value from driver */

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
	u_char  mbx[3];		/*XXX */
	/* doesn't make sense with 32bit addresses */
	struct {
		u_char  offset:4;
		u_char  period:3;
		u_char  valid:1;
	} sync[8];
	u_char  disc_sts;
};

struct bt_config {
	u_char  chan;
	u_char  intr;
	u_char  scsi_dev:3;
	u_char	:5;
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define EISADMA	0x00
#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80

#define KVTOPHYS(x)	vtophys(x)
#define PAGESIZ		4096
#define INVALIDATE_CACHE {asm volatile( ".byte	0x0F ;.byte 0x08" ); }

u_char  bt_scratch_buf[256];

struct bt_data {
	short   bt_base;		/* base port for each board */
	struct bt_mbx bt_mbx;		/* all our mailboxes */
	struct bt_ccb *bt_ccb_free;	/* list of free CCBs */
	struct bt_ccb *ccbhash[CCB_HASH_SIZE];	/* phys to kv hash */
	int     bt_int;			/* int. read off board */
	int     bt_dma;			/* DMA channel read of board */
	int     bt_scsi_dev;		/* adapters scsi id */
	int     numccbs;		/* how many we have malloc'd */
	struct scsi_link sc_link;	/* prototype for devs */
}      *btdata[NBT];

/***********debug values *************/
#define	BT_SHOWCCBS 0x01
#define	BT_SHOWINTS 0x02
#define	BT_SHOWCMDS 0x04
#define	BT_SHOWMISC 0x08
int     bt_debug = 0;

#ifdef	KERNEL
int     btprobe();
int     btattach();
int     btintr();
int32   bt_scsi_cmd();
void	bt_timeout(caddr_t, int);
void	bt_inquire_setup_information();
void    bt_done();
void    btminphys();
u_int32 bt_adapter_info();
struct bt_ccb *bt_get_ccb();
struct bt_ccb *bt_ccb_phys_kv();

static int btunit = 0;

struct isa_driver btdriver =
{
    btprobe,
    btattach,
    "bt"
};

struct scsi_adapter bt_switch =
{
    bt_scsi_cmd,
    btminphys,
    0,
    0,
    bt_adapter_info,
    "bt",
    0, 0
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device bt_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "bt",
    0,
    0, 0
};

#endif /*KERNEL */

#define BT_RESET_TIMEOUT 1000
#ifndef	KERNEL
main()
{
	printf("bt_data is %d bytes\n", sizeof(struct bt_data));
	printf("bt_ccb is %d bytes\n", sizeof(struct bt_ccb));
	printf("bt_mbx is %d bytes\n", sizeof(struct bt_mbx));
}

#else /*KERNEL */

/*
 * bt_cmd(unit,icnt, ocnt,wait, retval, opcode, args)
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
int
bt_cmd(unit, icnt, ocnt, wait, retval, opcode, args)
	int unit;
	int icnt;
	int ocnt;
	int wait;
	u_char		*retval;
	unsigned	opcode;
	u_char		args;
{
	struct		bt_data *bt = btdata[unit];
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
			printf("bt%d: bt_cmd, host not idle(0x%x)\n", unit, sts);
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
			printf("bt%d: bt_cmd, cmd/data port full\n", unit);
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
			printf("bt%d: bt_cmd, cmd/data port empty %d\n",
			    unit, ocnt);
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
		printf("bt%d: bt_cmd, host not finished(0x%x)\n", unit, sts);
		return (ENXIO);
	}
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return (0);
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
int
btprobe(dev)
	struct isa_device *dev;
{
	/*
	 * find unit and check we have that many defined
	 */
	int     unit = btunit;
	struct bt_data *bt;

	if (unit >= NBT) {
		printf("bt%d: unit number too high\n", unit);
		return 0;
	}
	/*
	 * Allocate a storage area for us
	 */
	if (btdata[unit]) {
		printf("bt%d: memory already allocated\n", unit);
		return 0;
	}
	bt = malloc(sizeof(struct bt_data), M_TEMP, M_NOWAIT);
	if (!bt) {
		printf("bt%d: cannot malloc!\n", unit);
		return 0;
	}
	bzero(bt, sizeof(struct bt_data));
	btdata[unit] = bt;
	bt->bt_base = dev->id_iobase;

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads bt->bt_int
	 */
	if (bt_init(unit) != 0) {
		btdata[unit] = NULL;
		free(bt, M_TEMP);
		return 0;
	}
	/*
	 * If it's there, put in it's interrupt vectors
	 */ dev->id_unit = unit;
	dev->id_irq = (1 << bt->bt_int);
	dev->id_drq = bt->bt_dma;

	btunit++;
	return 1;
}

/*
 * Attach all the sub-devices we can find
 */
int
btattach(dev)
	struct isa_device *dev;
{
	int	unit = dev->id_unit;
	struct	bt_data *bt = btdata[unit];

	/*
	 * fill in the prototype scsi_link.
	 */
	bt->sc_link.adapter_unit = unit;
	bt->sc_link.adapter_targ = bt->bt_scsi_dev;
	bt->sc_link.adapter = &bt_switch;
	bt->sc_link.device = &bt_dev;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(&(bt->sc_link));
	return 1;
}

/*
 * Return some information to the caller about the adapter and its
 * capabilities.
 */
u_int32 
bt_adapter_info(unit)
	int	unit;
{
	return (2);		/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
int
btintr(unit)
	int	unit;
{
	struct bt_data *bt = btdata[unit];
	BT_MBI *wmbi;
	struct bt_mbx *wmbx;
	struct bt_ccb *ccb;
	unsigned char stat;
	int     i, wait;
	int     found = 0;

#ifdef UTEST
	printf("btintr ");
#endif
	/*
	 * First acknowlege the interrupt, Then if it's
	 * not telling about a completed operation
	 * just return. 
	 */
	stat = inb(BT_INTR_PORT);

	/* Mail Box out empty ? */
	if (stat & BT_MBOA) {
		printf("bt%d: Available Free mbo post\n", unit);
		/* Disable MBO available interrupt */
		outb(BT_CMD_DATA_PORT, BT_MBO_INTR_EN);
		wait = 100000;	/* 1 sec enough? */
		for (i = wait; i; i--) {
			if (!(inb(BT_CTRL_STAT_PORT) & BT_CDF))
				break;
			DELAY(10);
		}
		if (i == 0) {
			printf("bt%d: bt_intr, cmd/data port full\n", unit);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return 1;
		}
		outb(BT_CMD_DATA_PORT, 0x00);	/* Disable */
		wakeup((caddr_t)&bt->bt_mbx);
		outb(BT_CTRL_STAT_PORT, BT_IRST);
		return 1;
	}
	if (!(stat & BT_MBIF)) {
		outb(BT_CTRL_STAT_PORT, BT_IRST);
		return 1;
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
			untimeout(bt_timeout, ccb);
			bt_done(unit, ccb);
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
			printf("bt%d: mbi at 0x%08x should be found, stat=%02x..resync\n",
			    unit, wmbi, stat);
		} else {
			found = 0;
			goto AGAIN;
		}
	}
	wmbx->tmbi = wmbi;
	outb(BT_CTRL_STAT_PORT, BT_IRST);
	return 1;
}

/*
 * A ccb is put onto the free list.
 */
void
bt_free_ccb(unit, ccb, flags)
	int unit;
	struct bt_ccb *ccb;
	int flags;
{
	struct bt_data *bt = btdata[unit];
	unsigned int opri = 0;

	if (!(flags & SCSI_NOMASK))
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

	if (!(flags & SCSI_NOMASK))
		splx(opri);
}

/*
 * Get a free ccb 
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct bt_ccb *
bt_get_ccb(unit, flags)
	int unit;
	int flags;
{
	struct bt_data *bt = btdata[unit];
	unsigned opri = 0;
	struct bt_ccb *ccbp;
	struct bt_mbx *wmbx;	/* Mail Box pointer specified unit */
	BT_MBO *wmbo;		/* Out Mail Box pointer */
	int     hashnum;

	if (!(flags & SCSI_NOMASK))
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
				printf("bt%d: Can't malloc CCB\n", unit);
			}
			goto gottit;
		} else {
			if (!(flags & SCSI_NOSLEEP)) {
				sleep(&bt->bt_ccb_free, PRIBIO);
			}
		}
	}
	if (ccbp) {
		/* Get CCB from from free list */
		bt->bt_ccb_free = ccbp->next;
		ccbp->flags = CCB_ACTIVE;
	}
      gottit:
      	if (!(flags & SCSI_NOMASK))
		splx(opri);

	return (ccbp);
}

/*
 * given a physical address, find the ccb that
 * it corresponds to:
 */
struct bt_ccb *
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
BT_MBO *
bt_send_mbo(int unit, int flags, int cmd, struct bt_ccb *ccb)
{
	struct	bt_data *bt = btdata[unit];
	unsigned opri = 0;
	BT_MBO	*wmbo;		/* Mail Box Out pointer */
	struct	bt_mbx *wmbx;	/* Mail Box pointer specified unit */
	int     i, wait;

	wmbx = &bt->bt_mbx;

	if (!(flags & SCSI_NOMASK))
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
			printf("bt%d: bt_send_mbo, cmd/data port full\n", unit);
			outb(BT_CTRL_STAT_PORT, BT_SRST);
			return ((BT_MBO *) 0);
		}
		outb(BT_CMD_DATA_PORT, 0x01);	/* Enable */
		sleep(wmbx, PRIBIO);	/*XXX *//*can't do this! */
		/* May be servicing an int */
	}
	/* Link CCB to the Mail Box */
	wmbo->ccb_addr = KVTOPHYS(ccb);
	ccb->mbx = wmbo;
	wmbo->cmd = cmd;

	/* Send it! */
	outb(BT_CMD_DATA_PORT, BT_START_SCSI);

	if (!(flags & SCSI_NOMASK))
		splx(opri);

	return (wmbo);
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
bt_done(unit, ccb)
	int	unit;
	struct	bt_ccb *ccb;
{
	struct bt_data *bt = btdata[unit];
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
	bt_free_ccb(unit, ccb, xs->flags);
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
int
bt_init(unit)
	int     unit;
{
	struct bt_data *bt = btdata[unit];
	unsigned char ad[4];
	volatile int i, sts;
	struct bt_config conf;

	/*
	 * reset board, If it doesn't respond, assume 
	 * that it's not there.. good for the probe
	 */

	outb(BT_CTRL_STAT_PORT, BT_HRST | BT_SRST);

	for (i = BT_RESET_TIMEOUT; i; i--) {
		sts = inb(BT_CTRL_STAT_PORT);
		if (sts == (BT_IDLE | BT_INIT))
			break;
		DELAY(1000);
	}
	if (i == 0) {
#ifdef	UTEST
		printf("bt_init: No answer from bt742a board\n");
#endif
		return (ENXIO);
	}
	/*
	 * Assume we have a board at this stage
	 * setup dma channel from jumpers and save int
	 * level
	 */
	printf("bt%d: reading board settings, ", unit);

	bt_cmd(unit, 0, sizeof(conf), 0, &conf, BT_CONF_GET);
	switch (conf.chan) {
	case EISADMA:
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
		printf("eisa dma, ");
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
	bt_cmd(unit, 5, 0, 0, 0, BT_MBX_INIT_EXTENDED
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
		    unit);
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
	bt_inquire_setup_information(unit);

	/* Enable round-robin scheme - appeared at firmware rev. 3.31 */
	bt_cmd(unit, 1, 0, 0, 0, BT_ROUND_ROBIN, BT_ENABLE);

	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

void
bt_inquire_setup_information(unit)
	int     unit;
{
	struct	bt_data *bt = btdata[unit];
	struct	bt_setup setup;
	struct	bt_boardID bID;
	int	i;

	/* Inquire Board ID to Bt742 for firmware version */
	bt_cmd(unit, 0, sizeof(bID), 0, &bID, BT_INQUIRE);
	printf("bt%d: version %c.%c, ",
	    unit, bID.firm_revision, bID.firm_version);

	/* Obtain setup information from Bt742. */
	bt_cmd(unit, 1, sizeof(setup), 0, &setup, BT_SETUP_GET, sizeof(setup));

	if (setup.sync_neg) {
		printf("sync, ");
	} else {
		printf("async, ");
	}
	if (setup.parity) {
		printf("parity, ");
	} else {
		printf("no parity, ");
	}
	printf("%d mbxs, %d ccbs\n", setup.num_mbx, bt->numccbs);

	for (i = 0; i < 8; i++) {
		if (!setup.sync[i].offset &&
		    !setup.sync[i].period &&
		    !setup.sync[i].valid)
			continue;

		printf("bt%d: dev%02d Offset=%d,Transfer period=%d, Synchronous? %s",
		    unit, i,
		    setup.sync[i].offset, setup.sync[i].period,
		    setup.sync[i].valid ? "Yes" : "No");
	}
}

#ifndef	min
#define min(x,y) (x < y ? x : y)
#endif	/* min */

void 
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
int32 
bt_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	scsi_sense_data *s1, *s2;
	struct	bt_ccb *ccb;
	struct	bt_scat_gath *sg;
	int	seg;		/* scatter gather seg being worked on */
	int	i = 0;
	int	c = 0;
	int	thiskv;
	physaddr thisphys, nextphys;
	int	unit = xs->sc_link->adapter_unit;
	int	bytes_this_seg, bytes_this_page, datalen, flags;
	struct	iovec *iovp;
	struct	bt_data *bt = btdata[unit];
	BT_MBO	*mbo;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("bt_scsi_cmd\n"));
	/*
	 * get a ccb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (xs->bp)
		flags |= (SCSI_NOSLEEP);	/* just to be sure */
	if (flags & ITSDONE) {
		printf("bt%d: Already done?\n", unit);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("bt%d: Not in use?\n", unit);
		xs->flags |= INUSE;
	}
	if (!(ccb = bt_get_ccb(unit, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	SC_DEBUG(xs->sc_link, SDEV_DB3,
	    ("start ccb(%x)\n", ccb));
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
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while ((datalen) && (seg < BT_NSEG)) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("0x%x", thisphys));

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
			    unit, BT_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			bt_free_ccb(unit, ccb, flags);
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
	if (bt_send_mbo(unit, flags, BT_MBO_START, ccb) == (BT_MBO *) 0) {
		xs->error = XS_DRIVER_STUFFUP;
		bt_free_ccb(unit, ccb, flags);
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
	return (bt_poll(unit, xs, ccb));
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int 
bt_poll(unit, xs, ccb)
	int	unit;
	struct	scsi_xfer *xs;
	struct	bt_ccb *ccb;
{
	struct	bt_data *bt = btdata[unit];
	int	done = 0;
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
			btintr(unit);
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
		bt_timeout((caddr_t)ccb, 0);

		/*
		 * because we are polling, take out the timeout entry
		 * bt_timeout made
		 */
		untimeout(bt_timeout, ccb);
		count = 2000;
		while (count) {
			/*
			 * Once again, wait for the int bit
			 */
			stat = inb(BT_INTR_PORT);
			if (stat & BT_ANY_INTR) {
				btintr(unit);
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
			bt_timeout((caddr_t)ccb, 0);
		}
	}
	if (xs->error)
		return (HAD_ERROR);
	return (COMPLETE);
}

void
bt_timeout(caddr_t arg1, int arg2)
{
	struct bt_ccb * ccb = (struct bt_ccb *)arg1;
	int     unit;
	struct bt_data *bt;
	int     s = splbio();

	unit = ccb->xfer->sc_link->adapter_unit;
	bt = btdata[unit];
	printf("bt%d:%d:%d (%s%d) timed out ", unit
	    ,ccb->xfer->sc_link->target
	    ,ccb->xfer->sc_link->lun
	    ,ccb->xfer->sc_link->device->name
	    ,ccb->xfer->sc_link->dev_unit);

#ifdef	UTEST
	bt_print_active_ccbs(unit);
#endif

	/*
	 * If the ccb's mbx is not free, then the board has gone Far East?
	 */
	if (bt_ccb_phys_kv(bt, ccb->mbx->ccb_addr) == ccb &&
	    ccb->mbx->cmd != BT_MBO_FREE) {
		printf("bt%d: not taking commands!\n", unit);
		Debugger();
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
		bt_done(unit, ccb);
	} else {		/* abort the operation that has timed out */
		printf("bt%d: Try to abort\n", unit);
		bt_send_mbo(unit, ~SCSI_NOMASK,
		    BT_MBO_ABORT, ccb);
		/* 2 secs for the abort */
		timeout(bt_timeout, (caddr_t)ccb, 2 * hz);
		ccb->flags = CCB_ABORTED;
	}
	splx(s);
}

#ifdef	UTEST
void
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

void
bt_print_active_ccbs(int unit)
{
	struct bt_data *bt = btdata[unit];
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
#endif /*KERNEL */
