/*
 * (Mostly) Written by Julian Elischer (julian@tfs.com)
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
 *      $Id: aha1542.c,v 1.77 1998/05/01 18:30:00 bde Exp $
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include "aha.h"
#include "opt_tune_1542.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/clock.h>
#include <machine/stdarg.h>

#include <scsi/scsiconf.h>
#include <scsi/scsi_debug.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>

/************************** board definitions *******************************/

/*
 * I/O Port Interface
 */

#define	AHA_BASE		aha->aha_base
#define	AHA_CTRL_STAT_PORT	(AHA_BASE + 0x0)	/* control & status */
#define	AHA_CMD_DATA_PORT	(AHA_BASE + 0x1)	/* cmds and datas */
#define	AHA_INTR_PORT		(AHA_BASE + 0x2)	/* Intr. stat */

/*
 * AHA_CTRL_STAT bits (write)
 */

#define AHA_HRST		0x80	/* Hardware reset */
#define AHA_SRST		0x40	/* Software reset */
#define AHA_IRST		0x20	/* Interrupt reset */
#define AHA_SCRST		0x10	/* SCSI bus reset */

/*
 * AHA_CTRL_STAT bits (read)
 */

#define AHA_STST		0x80	/* Self test in Progress */
#define AHA_DIAGF		0x40	/* Diagnostic Failure */
#define AHA_INIT		0x20	/* Mbx Init required */
#define AHA_IDLE		0x10	/* Host Adapter Idle */
#define AHA_CDF			0x08	/* cmd/data out port full */
#define AHA_DF			0x04	/* Data in port full */
#define AHA_INVDCMD		0x01	/* Invalid command */

/*
 * AHA_CMD_DATA bits (write)
 */

#define	AHA_NOP			0x00	/* No operation */
#define AHA_MBX_INIT		0x01	/* Mbx initialization */
#define AHA_START_SCSI		0x02	/* start scsi command */
#define AHA_START_BIOS		0x03	/* start bios command */
#define AHA_INQUIRE		0x04	/* Adapter Inquiry */
#define AHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define AHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define AHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define AHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define AHA_SPEED_SET		0x09	/* set transfer speed */
#define AHA_DEV_GET		0x0a	/* return installed devices */
#define AHA_CONF_GET		0x0b	/* return configuration data */
#define AHA_TARGET_EN		0x0c	/* enable target mode */
#define AHA_SETUP_GET		0x0d	/* return setup data */
#define AHA_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define AHA_READ_CH2		0x1b	/* read channel 2 buffer */
#define AHA_WRITE_FIFO		0x1c	/* write fifo buffer */
#define AHA_READ_FIFO		0x1d	/* read fifo buffer */
#define AHA_ECHO		0x1e	/* Echo command data */
#define AHA_EXT_BIOS		0x28	/* return extended bios info */
#define AHA_MBX_ENABLE		0x29	/* enable mail box interface */

struct aha_cmd_buf {
	u_char  byte[16];
};

/*
 * AHA_INTR_PORT bits (read)
 */

#define AHA_ANY_INTR		0x80	/* Any interrupt */
#define AHA_SCRD		0x08	/* SCSI reset detected */
#define AHA_HACC		0x04	/* Command complete */
#define AHA_MBOA		0x02	/* MBX out empty */
#define AHA_MBIF		0x01	/* MBX in full */

/*
 * Mail box defs
 */

#define AHA_MBX_SIZE		16	/* mail box size */

struct aha_mbx {
	struct aha_mbx_out {
		unsigned char cmd;
		unsigned char ccb_addr[3];
	} mbo[AHA_MBX_SIZE];
	struct aha_mbx_in {
		unsigned char stat;
		unsigned char ccb_addr[3];
	} mbi[AHA_MBX_SIZE];
};

/*
 * mbo.cmd values
 */

#define AHA_MBO_FREE	0x0	/* MBO entry is free */
#define AHA_MBO_START	0x1	/* MBO activate entry */
#define AHA_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.stat values
 */

#define AHA_MBI_FREE	0x0	/* MBI entry is free */
#define AHA_MBI_OK	0x1	/* completed without error */
#define AHA_MBI_ABORT	0x2	/* aborted ccb */
#define AHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define AHA_MBI_ERROR	0x4	/* Completed with error */

#define AHA_MBI_TGT_NO_CCB	0x10	/* Target received, no CCB ready */

/* FOR OLD VERSIONS OF THE !%$@ this may have to be 16 (yuk) */
#define	AHA_NSEG	17	/* Number of scatter gather segments <= 16 */
				/* allow 64 K i/o (min) */

struct aha_ccb {
	unsigned char opcode;
	unsigned char lun:3;
	unsigned char data_in:1;	/* must be 0 */
	unsigned char data_out:1;	/* must be 0 */
	unsigned char target:3;
	unsigned char scsi_cmd_length;
	unsigned char req_sense_length;
	unsigned char data_length[3];
	unsigned char data_addr[3];
	unsigned char link_addr[3];
	unsigned char link_id;
	unsigned char host_stat;
	unsigned char target_stat;
	unsigned char reserved[2];
	struct scsi_generic scsi_cmd;
	struct scsi_sense_data scsi_sense;
	struct aha_scat_gath {
		unsigned char seg_len[3];
		unsigned char seg_addr[3];
	} scat_gath[AHA_NSEG];
	struct aha_ccb *next;
	struct scsi_xfer *xfer;	/* the scsi_xfer for this cmd */
	struct aha_mbx_out *mbx;	/* pointer to mail box */
	int     flags;
#define CCB_FREE        0
#define CCB_ACTIVE      1
#define CCB_ABORTED     2
};

/*
 * opcode fields
 */

#define AHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define AHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define AHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scatter gather */
#define AHA_RESET_CCB		0x81	/* SCSI Bus reset */

#define AHA_INIT_RESID_CCB	0x03	/* SCSI Initiator CCB */
#define AHA_INIT_SG_RESID_CCB	0x04	/* SCSI initiator with scatter gather */

/*
 * aha_ccb.host_stat values
 */

#define AHA_OK		0x00	/* cmd ok */
#define AHA_LINK_OK	0x0a	/* Link cmd ok */
#define AHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define AHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define AHA_OVER_UNDER	0x12	/* Data over/under run */
#define AHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define AHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define AHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define AHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define AHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define AHA_INV_TARGET	0x18	/* Invalid target direction */
#define AHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define AHA_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define AHA_ABORTED      42

struct aha_setup {
	u_char  sync_neg:1;
	u_char  parity:1;
	        u_char:6;
	u_char  speed;
	u_char  bus_on;
	u_char  bus_off;
	u_char  num_mbx;
	u_char  mbx[3];
	struct {
		u_char  offset:4;
		u_char  period:3;
		u_char  valid:1;
	} sync[8];
	u_char  disc_sts;
};

struct aha_config {
	u_char  chan;
	u_char  intr;
	u_char  scsi_dev:3;
	        u_char:5;
};

struct	aha_inquire
{
	u_char	boardid;		/* type of board */
					/* 0x20 (' ') = BusLogic 545, but it gets
					   the command wrong, only returns
					   one byte */
					/* 0x31 ('1') = AHA-1540 */
					/* 0x41 ('A') = AHA-1540A/1542A/1542B */
					/* 0x42 ('B') = AHA-1640 */
					/* 0x43 ('C') = AHA-1542C */
					/* 0x44 ('D') = AHA-1542CF */
					/* 0x45 ('E') = AHA-1542CF, BIOS v2.01 */
					/* 0x46 ('F') = AHA-1542CP, "Plug'nPlay" */
	u_char	spec_opts;		/* special options ID */
					/* 0x41 = Board is standard model */
	u_char	revision_1;		/* firmware revision [0-9A-Z] */
	u_char	revision_2;		/* firmware revision [0-9A-Z] */
};

struct	aha_extbios
{
	u_char	flags;			/* Bit 3 == 1 extended bios enabled */
	u_char	mailboxlock;		/* mail box lock code to unlock it */
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80

/*********************************** end of board definitions***************/

#define PHYSTOKV(x)	(((long int)(x)) ^ aha->kv_phys_xor)
#define KVTOPHYS(x)	vtophys(x)
#define	AHA_DMA_PAGES	AHA_NSEG

#define PAGESIZ 	4096

#ifdef	AHADEBUG
int     aha_debug = 1;
#endif /*AHADEBUG */

static struct aha_data {
	int   aha_base;		/* base port for each board */
	/*
	 * xor this with a physaddr to get a kv addr and visa versa
	 * for items in THIS STRUCT only.
	 * Used to get the CCD's physical and kv addresses from each
	 * other.
	 */
	long int kv_phys_xor;
	struct aha_mbx aha_mbx;	/* all the mailboxes */
	struct aha_ccb *aha_ccb_free;	/* the next free ccb */
	struct aha_ccb aha_ccb[AHA_MBX_SIZE];	/* all the CCBs      */
	int     unit;	/* unit number */
	int     aha_int;	/* irq level        */
	int     aha_dma;	/* DMA req channel  */
	int     aha_scsi_dev;	/* scsi bus address  */
	int     flags;

	/* We use different op codes for different revs of the board
	 * if we think residual codes will work.
	 */
	short	init_opcode;	/* Command to use for initiator */
	short	sg_opcode;	/* Command to use for scatter/gather */
	struct scsi_link sc_link;	/* prototype for subdevs */
} *ahadata[NAHA];

static u_int32_t	aha_adapter_info __P((int unit));
static int	ahaattach __P((struct isa_device *dev));
#ifdef	TUNE_1542
static int	aha_bus_speed_check __P((struct aha_data *aha, int speed));
static int	aha_set_bus_speed __P((struct aha_data *aha));
#endif
static int	aha_cmd __P((struct aha_data *aha, int icnt, int ocnt, int wait,
			     u_char *retval, u_char opcode, ...));
static void	aha_done __P((struct aha_data *aha, struct aha_ccb *ccb));
static int	aha_escape __P((struct scsi_xfer *xs, struct aha_ccb *ccb));
static void	aha_free_ccb __P((struct aha_data *aha, struct aha_ccb *ccb,
				  int flags));
static struct aha_ccb *
		aha_get_ccb __P((struct aha_data *aha, int flags));
static int	aha_init __P((struct aha_data *aha));
static void	ahaminphys __P((struct buf *bp));
static int	aha_poll __P((struct aha_data *aha, struct scsi_xfer *xs,
			      struct aha_ccb *ccb));
static int	ahaprobe __P((struct isa_device *dev));
static int32_t	aha_scsi_cmd __P((struct scsi_xfer *xs));
static timeout_t
		aha_timeout;
static char	*board_rev __P((struct aha_data *aha, int type));
static int	physcontig __P((int kv, int len));
static void	put_host_stat __P((int host_stat));

static struct scsi_adapter aha_switch =
{
    aha_scsi_cmd,
    ahaminphys,
    0,
    0,
    aha_adapter_info,
    "aha",
    { 0, 0 }
};

/* the below structure is so we have a default dev struct for out link struct */
static struct scsi_device aha_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "aha",
    0,
    { 0, 0 }
};

struct isa_driver ahadriver =
{
    ahaprobe,
    ahaattach,
    "aha"
};

static int ahaunit = 0;

#define aha_abortmbx(mbx) \
	(mbx)->cmd = AHA_MBO_ABORT; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);
#define aha_startmbx(mbx) \
	(mbx)->cmd = AHA_MBO_START; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);

#define AHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */

/*
 * aha_cmd(struct aha_data *aha,icnt, ocnt,wait, retval, opcode, ...)
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes written after opcode)
 *    ocnt:   number of expected returned bytes
 *    wait:   number of seconds to wait for response
 *    retval: buffer where to place returned bytes
 *    opcode: opcode AHA_NOP, AHA_MBX_INIT, AHA_START_SCSI, etc
 *    ...   : parameters to the command specified by opcode
 *
 * Performs an adapter command through the ports. Not to be confused
 * with a scsi command, which is read in via the dma.  One of the adapter
 * commands tells it to read in a scsi command but that one is done
 * separately.  This is only called during set-up.
 *
 */
static int
#ifdef __STDC__
aha_cmd(struct aha_data *aha, int icnt, int ocnt, int wait, u_char *retval,
	u_char opcode, ... )
#else
aha_cmd(aha, icnt, ocnt, wait, retval, opcode, va_alist)
	struct aha_data *aha,
	int icnt,
	int ocnt,
	int wait,
	u_char *retval,
	u_char opcode,
	va_dcl
#endif
{
	va_list	 ap;
	u_char   oc;
	u_char	 data;
	register int i;
	int      sts;

	/*
	 * multiply the wait argument by a big constant
	 * zero defaults to 1 sec..
	 * all wait loops are in 50uSec cycles
	 */
	if (wait)
		wait *= 20000;
	else
		wait = 20000;
	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != AHA_MBX_INIT && opcode != AHA_START_SCSI) {
		i = 20000;	/*do this for upto about a second */
		while (--i) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_IDLE) {
				break;
			}
			DELAY(50);
		}
		if (!i) {
			printf("aha%d: aha_cmd, host not idle(0x%x)\n",
			    aha->unit, sts);
			return (ENXIO);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((inb(AHA_CTRL_STAT_PORT)) & AHA_DF)
			inb(AHA_CMD_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	va_start(ap, opcode);
	for(data = opcode; icnt >=0; icnt--, data = (u_char)va_arg(ap, int)) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (!(sts & AHA_CDF))
				break;
			DELAY(50);
		}
		if (i == 0) {
			printf("aha%d: aha_cmd, cmd/data port full\n",
				aha->unit);
			outb(AHA_CTRL_STAT_PORT, AHA_SRST);
			return (ENXIO);
		}
		outb(AHA_CMD_DATA_PORT, data);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i = wait; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_DF)
				break;
			DELAY(50);
		}
		if (i == 0) {
			printf("aha%d: aha_cmd, cmd/data port empty %d\n",
			    aha->unit, ocnt);
			return (ENXIO);
		}
		oc = inb(AHA_CMD_DATA_PORT);
		if (retval)
			*retval++ = oc;
	}
	/*
	 * Wait for the board to report a finised instruction
	 */
	i = 20000;
	while (--i) {
		sts = inb(AHA_INTR_PORT);
		if (sts & AHA_HACC) {
			break;
		}
		DELAY(50);
	}
	if (i == 0) {
		printf("aha%d: aha_cmd, host not finished(0x%x)\n",
			aha->unit, sts);
		return (ENXIO);
	}
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	return 0;
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
ahaprobe(dev)
	struct isa_device *dev;
{
	int     unit = ahaunit;
	struct aha_data *aha;

	/*
	 * find unit and check we have that many defined
	 */
	if (unit >= NAHA) {
		printf("aha%d: unit number too high\n", unit);
		return 0;
	}
	dev->id_unit = unit;

	/*
	 * a quick safety check so we can be sleazy later
	 */
	if (sizeof(struct aha_data) > PAGESIZ) {
		printf("aha struct > pagesize\n");
		return 0;
	}
	/*
	 * Allocate a storage area for us
	 */
	if (ahadata[unit]) {
		printf("aha%d: memory already allocated\n", unit);
		return 0;
	}
	aha = malloc(sizeof(struct aha_data), M_TEMP, M_NOWAIT);
	if (!aha) {
		printf("aha%d: cannot malloc!\n", unit);
		return 0;
	}
	bzero(aha, sizeof(struct aha_data));
	ahadata[unit] = aha;
	aha->unit = unit;
	aha->aha_base = dev->id_iobase;

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads aha->aha_int
	 */
	if (aha_init(aha) != 0) {
		ahadata[unit] = NULL;
		free(aha, M_TEMP);
		return 0;
	}
	/*
	 * Calculate the xor product of the aha struct's
	 * physical and virtual address. This allows us
	 * to change addresses within the structure
	 * from physical to virtual easily, as long as
	 * the structure is less than 1 page in size.
	 * This is used to recognise CCBs which are in
	 * this struct and which are refered to by the
	 * hardware using physical addresses.
	 * (assumes malloc returns a chunk that doesn't
	 * span pages)
	 * eventually use the hash table in aha1742.c
	 */
	aha->kv_phys_xor = (long int) aha ^ (KVTOPHYS(aha));

	/*
	 * If it's there, put in it's interrupt vectors
	 */
	dev->id_irq = (1 << aha->aha_int);
	dev->id_drq = aha->aha_dma;
	ahaunit++;
	return 0x4;
}

/*
 * Attach all the sub-devices we can find
 */
static int
ahaattach(dev)
	struct isa_device *dev;
{
	int     unit = dev->id_unit;
	struct aha_data *aha = ahadata[unit];
	struct scsibus_data *scbus;

	/*
	 * fill in the prototype scsi_link.
	 */
	aha->sc_link.adapter_unit = unit;
	aha->sc_link.adapter_targ = aha->aha_scsi_dev;
	aha->sc_link.adapter_softc = aha;
	aha->sc_link.adapter = &aha_switch;
	aha->sc_link.device = &aha_dev;
	aha->sc_link.flags = aha->flags;;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return 0;
	scbus->adapter_link = &aha->sc_link;

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
static u_int32_t
aha_adapter_info(unit)
	int     unit;
{
	return (2);		/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
void
ahaintr(unit)
	int unit;
{
	unsigned char stat;
	register int i;
	struct aha_data *aha = ahadata[unit];

#ifdef	AHADEBUG
	printf("ahaintr ");
#endif /*AHADEBUG */
	/*
	 * First acknowledge the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	stat = inb(AHA_INTR_PORT);
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	if (!(stat & AHA_MBIF))
		return;
#ifdef	AHADEBUG
	printf("mbxin ");
#endif /*AHADEBUG */
	/*
	 * If it IS then process the completed operation
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		struct aha_mbx_in *mbi = aha->aha_mbx.mbi + i;

		if (mbi->stat != AHA_MBI_FREE) {
			struct aha_ccb *ccb =
			 (struct aha_ccb *)PHYSTOKV(scsi_3btou(mbi->ccb_addr));

			stat = mbi->stat;

			switch (stat) {
			case AHA_MBI_OK:
				break;

			case AHA_MBI_ABORT:
#ifdef	AHADEBUG
				if (aha_debug)
					printf("abort");
#endif /*AHADEBUG */
				ccb->host_stat = AHA_ABORTED;
				break;

			case	AHA_MBI_TGT_NO_CCB:
				/* We enabled target mode and received a SEND
				 * or RECEIVE command from the initiator, but
				 * we don't have any CCB registered to handle the command.
				 * At this point it would be nice to wakeup a
				 * process sleeping on this event via an ioctl,
				 * returning whether it is a SEND or RECEIVE and the
				 * required length.
				 * However, I want to look at the CAM documentation before
				 * I start extending the API at all.
				 */
#ifdef NOISE_WHEN_TGT_NO_CDB
				printf("Target received, but no CCB ready.\n");
				printf("Initiator & lun: %02x\n", mbi->ccb_addr[0]);
				printf("Max data length:     %06x\n",
				(mbi->ccb_addr[1] << 16) | (mbi->ccb_addr[2] << 8)
				+ 255);
#endif
#ifdef	AHADEBUG
				if (aha_debug)
					printf("target-no-ccb");
#endif /*AHADEBUG */
				ccb = 0;
				break;

			case AHA_MBI_UNKNOWN:
				ccb = 0;
#ifdef	AHADEBUG
				if (aha_debug)
					printf("unknown ccb for abort ");
#endif /*AHADEBUG */
				/* may have missed it */
				/* no such ccb known for abort */

			case AHA_MBI_ERROR:
				/* XXX ccb is still set up? Driver fails without it? */
				break;

			default:
				panic("Impossible mbxi status");

			}
#ifdef	AHADEBUG
			 if (aha_debug && ccb && stat != AHA_MBI_OK) {
				u_char *cp;
				cp = (u_char *) (&(ccb->scsi_cmd));
				printf("op=%x %x %x %x %x %x\n",
				    cp[0], cp[1], cp[2],
				    cp[3], cp[4], cp[5]);
				printf("stat %x for mbi[%d]\n"
				    ,mbi->stat, i);
				printf("addr = 0x%x\n", ccb);
			}
#endif /*AHADEBUG */
			if (ccb) {
				untimeout(aha_timeout, (caddr_t)ccb,
					  ccb->xfer->timeout_ch);
				aha_done(aha, ccb);
			}
			mbi->stat = AHA_MBI_FREE;
		}
	}
}

/*
 * A ccb (and hence a mbx-out) is put onto the
 * free list.
 */
static void
aha_free_ccb(aha, ccb, flags)
	struct aha_data *aha;
	struct aha_ccb *ccb;
	int flags;
{
	unsigned int opri = 0;

	if (!(flags & SCSI_NOMASK))
		opri = splbio();

	ccb->next = aha->aha_ccb_free;
	aha->aha_ccb_free = ccb;
	ccb->flags = CCB_FREE;
	/*
	 * If there were none, wake anybody waiting for
	 * one to come free, starting with queued entries
	 */
	if (!ccb->next) {
		wakeup((caddr_t)&aha->aha_ccb_free);
	}
	if (!(flags & SCSI_NOMASK))
		splx(opri);
}

/*
 * Get a free ccb (and hence mbox-out entry)
 */
static struct aha_ccb *
aha_get_ccb(aha, flags)
	struct aha_data *aha;
	int flags;
{
	unsigned opri = 0;
	struct aha_ccb *rc;

	if (!(flags & SCSI_NOMASK))
		opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one
	 * to come free
	 */
	while ((!(rc = aha->aha_ccb_free)) && (!(flags & SCSI_NOSLEEP))) {
		tsleep((caddr_t)&aha->aha_ccb_free, PRIBIO, "ahaccb", 0);
	}
	if (rc) {
		aha->aha_ccb_free = aha->aha_ccb_free->next;
		rc->flags = CCB_ACTIVE;
	}
	if (!(flags & SCSI_NOMASK))
		splx(opri);
	return (rc);
}

static void
put_host_stat(int host_stat)
{
	int i;

	struct { int host_stat; char *text; } tab[] = {
		{ AHA_OK, "Cmd ok" },
		{ AHA_LINK_OK, "Link cmd ok" },
		{ AHA_LINK_IT, "Link cmd ok + int" },
		{ AHA_SEL_TIMEOUT, "Selection time out" },
		{ AHA_OVER_UNDER, "Data over/under run" },
		{ AHA_BUS_FREE, "Bus dropped at unexpected time" },
		{ AHA_INV_BUS, "Invalid bus phase/sequence" },
		{ AHA_BAD_MBO, "Incorrect MBO cmd" },
		{ AHA_BAD_CCB, "Incorrect ccb opcode" },
		{ AHA_BAD_LINK, "Not same values of LUN for links" },
		{ AHA_INV_TARGET, "Invalid target direction" },
		{ AHA_CCB_DUP, "Duplicate CCB received" },
		{ AHA_INV_CCB, "Invalid CCB or segment list" },
		{ AHA_ABORTED, "Software abort" },
	};

	for (i = 0; i < (int)(sizeof(tab) / sizeof(tab[0])); i++) {
		if (tab[i].host_stat == host_stat) {
			printf("%s\n", tab[i].text);
			return;
		}
	}

	printf("Unknown host_stat %02x\n", host_stat);
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
static void
aha_done(aha, ccb)
	struct	aha_data *aha;
	struct	aha_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ccb->xfer;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("aha_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if (!(xs->flags & INUSE)) {
		printf("aha%d: exiting but not in use!\n", aha->unit);
#ifdef DIAGNOSTIC
		panic("aha1542 exiting but not in use");
#endif
	}
	xs->status = ccb->target_stat;
	xs->resid = 0;

	if (((ccb->host_stat != AHA_OK) || (ccb->target_stat != SCSI_OK))
	    && ((xs->flags & SCSI_ERR_OK) == 0)) {
		/*
		 * We have an error, that we cannot ignore.
		 */
		s1 = (struct scsi_sense_data *) (((char *) (&ccb->scsi_cmd))
		    + ccb->scsi_cmd_length);
		s2 = &(xs->sense);

		if (ccb->host_stat) {
			SC_DEBUG(xs->sc_link, SDEV_DB3, ("host err 0x%x\n",
				ccb->host_stat));
			switch (ccb->host_stat) {
			case AHA_ABORTED:
				xs->error = XS_TIMEOUT;
				break;
			case AHA_SEL_TIMEOUT:
				xs->error = XS_SELTIMEOUT;
				break;

			case	AHA_OVER_UNDER:		/* Over run / under run */
				switch(ccb->opcode)
				{
					case AHA_TARGET_CCB:
					xs->resid = xs->datalen - scsi_3btoi(ccb->data_length);
					xs->flags |= SCSI_RESID_VALID;
					if (xs->resid <= 0)
						xs->error = XS_LENGTH;
					break;

					case AHA_INIT_RESID_CCB:
					case AHA_INIT_SG_RESID_CCB:
					xs->resid = scsi_3btoi(ccb->data_length);
					xs->flags |= SCSI_RESID_VALID;
					if (xs->resid <= 0)
						xs->error = XS_LENGTH;
					break;

					default:
						xs->error = XS_LENGTH;
				}
				break;

			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				printf("aha%d: ", aha->unit);
				put_host_stat(ccb->host_stat);
			}
		} else {
			SC_DEBUG(xs->sc_link, SDEV_DB3, ("target err 0x%x\n",
				ccb->target_stat));
			switch (ccb->target_stat) {
			case 0x02:
				/* structure copy!!!!! */
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case 0x08:
				xs->error = XS_BUSY;
				break;
			default:
				printf("aha%d:target_stat%x\n",
				    aha->unit, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}

	xs->flags |= ITSDONE;
	aha_free_ccb(aha, ccb, xs->flags);
	scsi_done(xs);
}

/* Macro to determine that a rev is potentially a new valid one
 * so that the driver doesn't keep breaking on new revs as it
 * did for the CF and CP.
 */
#define PROBABLY_NEW_BOARD(REV) (REV > 0x43 && REV < 0x56)

static char *board_rev(struct aha_data *aha, int type)
{
	switch(type)
	{
		case 0x20: return "Buslogic 545?";
		case 0x31: return "AHA-1540";
		case 0x41: return "AHA-154x[AB]";
		case 0x42: return "AHA-1640";
		case 0x43: return "AHA-1542C";
		case 0x44: return "AHA-1542CF";
		case 0x45: return "AHA-1542CF BIOS v2.01";
		case 0x46: return "AHA-1542CP";

		default:


		if (PROBABLY_NEW_BOARD(type))
		{
			printf("aha%d: Assuming type %02x is a new board.\n",
				aha->unit, type);
			return "New Adaptec rev?";
		}

		printf("aha%d: type %02x is an unknown board.\n",
			aha->unit, type);
		return "Unknown board";
	}
}

/*
 * Start the board, ready for normal operation
 */
static int
aha_init(aha)
	struct	aha_data *aha;
{
	char *desc;
	unsigned char ad[3];
	volatile int i, sts;
	struct	aha_config conf;
	struct	aha_inquire inquire;
	struct	aha_extbios extbios;

	/* Assume that residual codes don't work.  If they
	 * do we enable that after we figure out what kind of
	 * board it is.
	 */
	aha->init_opcode = AHA_INITIATOR_CCB;
	aha->sg_opcode = AHA_INIT_SCAT_GATH_CCB;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(AHA_CTRL_STAT_PORT, AHA_HRST | AHA_SRST);

	for (i = AHA_RESET_TIMEOUT; i; i--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		if (sts == (AHA_IDLE | AHA_INIT)) {
			break;
		}
		DELAY(1000);	/* calibrated in msec */
	}
#ifdef	AHADEBUG
	printf("aha_init: AHA_RESET_TIMEOUT went to %d\n", i);
#endif /* AHADEBUG */
	if (i == 0) {
#ifdef	AHADEBUG
		if (aha_debug)
			printf("aha_init: No answer from board\n");
#endif /*AHADEBUG */
		return (ENXIO);
	}

	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is.  If the AHA_INQUIRE
	 * command fails, blatter about it, nuke the boardid so the 1542C
	 * stuff gets skipped over, and reset the board again.
	 */
	if(aha_cmd(aha, 0, sizeof(inquire), 1,
		   (u_char *)&inquire, AHA_INQUIRE)) {
		/*
		 * Blah.. not a real adaptec board!!!
		 * Seems that the Buslogic 545S and the DTC3290 both get
		 * this wrong.
		 */
		printf ("aha%d: not a REAL adaptec board, may cause warnings\n",
			aha->unit);
		inquire.boardid = 0;
		outb(AHA_CTRL_STAT_PORT, AHA_HRST | AHA_SRST);
		for (i = AHA_RESET_TIMEOUT; i; i--) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts == (AHA_IDLE | AHA_INIT)) {
				break;
			}
			DELAY(1000);    /* calibrated in msec */
		}
#ifdef	AHADEBUG
		printf("aha_init2: AHA_RESET_TIMEOUT went to %d\n", i);
#endif /* AHADEBUG */
		if (i == 0) {
#ifdef	AHADEBUG
			if (aha_debug)
				printf("aha_init2: No answer from board\n");
#endif /*AHADEBUG */
		return (ENXIO);
		}
	}
#ifdef	AHADEBUG
	printf("aha%d: inquire %x, %x, %x, %x\n",
		aha->unit,
		inquire.boardid, inquire.spec_opts,
		inquire.revision_1, inquire.revision_2);
#endif	/* AHADEBUG */

	aha->flags = SDEV_BOUNCE;

#define	PRVERBOSE(x) if (bootverbose) printf x

	/*
	 * If we are a new type of 1542 board (anything newer than a 1542C)
	 * then disable the extended bios so that the
	 * mailbox interface is unlocked.
	 * This is also true for the 1542B Version 3.20. First Adaptec
	 * board that supports >1Gb drives.
	 * No need to check the extended bios flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	desc = board_rev(aha, inquire.boardid);

	PRVERBOSE( ("aha%d: Rev %02x (%s) V%c.%c",
		aha->unit, inquire.boardid, desc, inquire.revision_1,
		inquire.revision_2) );

	if (PROBABLY_NEW_BOARD(inquire.boardid) ||
		(inquire.boardid == 0x41
		&& inquire.revision_1 == 0x31 && inquire.revision_2 == 0x34)) {
		aha_cmd(aha, 0, sizeof(extbios), 0,
			(u_char *)&extbios, AHA_EXT_BIOS);
#ifdef	AHADEBUG
		printf("aha%d: extended bios flags %x\n", aha->unit, extbios.flags);
#endif	/* AHADEBUG */

		PRVERBOSE( (", enabling mailbox") );

		aha_cmd(aha, 2, 0, 0, 0, AHA_MBX_ENABLE,
			0, extbios.mailboxlock);
	}

	/* Which boards support residuals?  Some early 1542A's apparently
	 * don't.  The 1542B with V0.5 of the software does, so I've
	 * arbitrarily set that as the earliest rev.
	 */
	if (PROBABLY_NEW_BOARD(inquire.boardid) ||
		(inquire.boardid == 0x41
		&& (inquire.revision_1 > '0' || inquire.revision_2 >= '5'))) {

		PRVERBOSE( (", enabling residuals") );

		aha->init_opcode = AHA_INIT_RESID_CCB;
		aha->sg_opcode = AHA_INIT_SG_RESID_CCB;
	}

	/* Which boards support target operations?  The 1542C completely
	 * locks up the SCSI bus if you enable them.  I'm only sure
	 * about the B, which was sold in the OEM market as a target
	 * board.
	 */
	if (inquire.boardid == 0x41) {
		PRVERBOSE( (", target ops") );
		aha->flags |= SDEV_TARGET_OPS;
	}

	PRVERBOSE( ("\n") );

	/*
	 * setup dma channel from jumpers and save int
	 * level
	 */
	PRVERBOSE(("aha%d: reading board settings, ", aha->unit));

	if (inquire.boardid == 0x20) {
		DELAY(1000);		/* for Bustek 545 */
	}

	aha_cmd(aha, 0, sizeof(conf), 0, (u_char *)&conf, AHA_CONF_GET);
	switch (conf.chan) {
	case CHAN0:
		outb(0x0b, 0x0c);
		outb(0x0a, 0x00);
		aha->aha_dma = 0;
		break;
	case CHAN5:
		outb(0xd6, 0xc1);
		outb(0xd4, 0x01);
		aha->aha_dma = 5;
		break;
	case CHAN6:
		outb(0xd6, 0xc2);
		outb(0xd4, 0x02);
		aha->aha_dma = 6;
		break;
	case CHAN7:
		outb(0xd6, 0xc3);
		outb(0xd4, 0x03);
		aha->aha_dma = 7;
		break;
	default:
		printf("aha%d: illegal dma jumper setting\n", aha->unit);
		return (EIO);
	}

	PRVERBOSE( ("dma=%d ", aha->aha_dma) );

	switch (conf.intr) {
	case INT9:
		aha->aha_int = 9;
		break;
	case INT10:
		aha->aha_int = 10;
		break;
	case INT11:
		aha->aha_int = 11;
		break;
	case INT12:
		aha->aha_int = 12;
		break;
	case INT14:
		aha->aha_int = 14;
		break;
	case INT15:
		aha->aha_int = 15;
		break;
	default:
		printf("aha%d: illegal int jumper setting\n", aha->unit);
		return (EIO);
	}

	PRVERBOSE( ("int=%d ", aha->aha_int) );

	/* who are we on the scsi bus? */
	aha->aha_scsi_dev = conf.scsi_dev;

	PRVERBOSE( ("id=%d ", aha->aha_scsi_dev) );

	/*
	 * Change the bus on/off times to not clash with other dma users.
	 */
	aha_cmd(aha, 1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 7);
	aha_cmd(aha, 1, 0, 0, 0, AHA_BUS_OFF_TIME_SET, 4);

#ifdef TUNE_1542
	/*
	 * Initialize memory transfer speed
	 * Not compiled in by default because it breaks some machines
	 */
	if (!(aha_set_bus_speed(aha))) {
		return (EIO);
	}
#else
	PRVERBOSE( (" (bus speed defaulted)\n") );
#endif	/*TUNE_1542*/
	/*
	 * Initialize mail box
	 */
	scsi_uto3b(KVTOPHYS(&aha->aha_mbx), ad);

	aha_cmd(aha, 4, 0, 0, 0, AHA_MBX_INIT,
	    AHA_MBX_SIZE,
	    ad[0],
	    ad[1],
	    ad[2]);

	/*
	 * link the ccb's with the mbox-out entries and
	 * into a free-list
	 * this is a kludge but it works
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		aha->aha_ccb[i].next = aha->aha_ccb_free;
		aha->aha_ccb_free = &aha->aha_ccb[i];
		aha->aha_ccb_free->flags = CCB_FREE;
		aha->aha_ccb_free->mbx = &aha->aha_mbx.mbo[i];
		scsi_uto3b(KVTOPHYS(aha->aha_ccb_free), aha->aha_mbx.mbo[i].ccb_addr);
	}
	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

static void
ahaminphys(bp)
	struct buf *bp;
{
/*      aha seems to explode with 17 segs (64k may require 17 segs) */
/*      on old boards so use a max of 16 segs if you have problems here */
	if (bp->b_bcount > ((AHA_NSEG - 1) * PAGESIZ)) {
		bp->b_bcount = ((AHA_NSEG - 1) * PAGESIZ);
	}
}

static int
aha_escape(xs, ccb)
	struct scsi_xfer *xs;
	struct aha_ccb *ccb;
{
	int ret = 0;
	int s;

	if (xs->cmd)
	{
		switch(xs->cmd->opcode)
		{
			case SCSI_OP_RESET:
			ccb->opcode	=	AHA_RESET_CCB;
			ret = 0;
			break;

			case SCSI_OP_TARGET:
			s= splbio();
			aha_cmd((struct aha_data *)xs->sc_link->adapter_softc,
				2, 0, 0, 0, AHA_TARGET_EN,
			(int)xs->cmd->bytes[0], (int)1);
			splx(s);
			ret = COMPLETE;
			break;

			default:
			ret = ESCAPE_NOT_SUPPORTED;
			break;
		}
	}
	else
	{
		ccb->opcode	=	AHA_RESET_CCB;
		ret = 0;
	}

	return ret;
}

#define physdb(ARG) (void)(ARG)

/* physcontig: Scan forward from a KV and return length to the
 * end of physically contiguous addresses.  This belongs in
 * i386/.../something_or_other.c
 * XXX: Find the right thing in the kernel.
 */
static int physcontig(int kv, int len)
{
	int len_was = len;
	u_long kvl = (u_long)kv;

	int phys_len;
	u_long phys, prev_phys;

	prev_phys = KVTOPHYS(kvl);

	/* We go at least to the end of this page:
	 */
	phys_len = PAGESIZ - (prev_phys & (PAGESIZ - 1));
	len -= phys_len;
	kvl += phys_len;
	prev_phys &= ~(PAGESIZ - 1);

	while (len > 0)
	{
		phys = KVTOPHYS(kvl);

		if (phys != prev_phys + PAGESIZ)
		{
			physdb(("phys %08x != prev_phys %08x + PAGESIZ\n",
			phys, prev_phys));

			break;
		}

		prev_phys = phys;
		kvl += PAGESIZ;
		len -= PAGESIZ;
	}

	phys_len = (len < 0) ? len_was : (len_was - len);

	physdb(("physcontig(%08x, %d) = %d\n", kv, len_was, phys_len));

	return phys_len;
}
/*
 * start a scsi operation given the command and
 * the data address. Also needs the unit, target
 * and lu
 */
static int32_t
aha_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct aha_data *aha;
	struct aha_ccb *ccb;
	struct aha_scat_gath *sg;
	int     seg;		/* scatter gather seg being worked on */
	int     thiskv;
	int     thisphys, nextphys;
	int     bytes_this_seg, bytes_this_page, datalen, flags;
	int     s;

	aha = (struct aha_data *)sc_link->adapter_softc;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("aha_scsi_cmd\n"));
	/*
	 * get a ccb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (!(ccb = aha_get_ccb(aha, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	if (ccb->mbx->cmd != AHA_MBO_FREE)
		printf("aha%d: MBO %02x and not %02x (free)\n",
			aha->unit, ccb->mbx->cmd, AHA_MBO_FREE);

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	ccb->xfer = xs;
	if (flags & SCSI_RESET) {
		ccb->opcode = AHA_RESET_CCB;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ?
		    aha->sg_opcode
		    : aha->init_opcode);
	}
	ccb->target = sc_link->target;
	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->lun = sc_link->lun;
	ccb->scsi_cmd_length = xs->cmdlen;

	/* Some devices (e.g, Microtek ScanMaker II)
	 * fall on the ground if you ask for anything but
	 * an exact number of sense bytes (wiping out the
	 * sense data)
	 * XXX: This was lost at some point in scsi_ioctl.c.
	 */
	ccb->req_sense_length = (xs->req_sense_length)
		? xs->req_sense_length
		: sizeof(ccb->scsi_sense);

	/* XXX: I propose we move the reset handling into the escape
	 * handling.
	 */
	if (flags & SCSI_RESET) {
		flags |= SCSI_ESCAPE;
		xs->cmd->opcode = SCSI_OP_RESET;
	}

	/* Set up the CCB.  For an escape function, the escape hook may
	 * set it up for us.
	 */

	if (flags & SCSI_ESCAPE) {
		int ret;
		ret = aha_escape(xs, ccb);
		if (ret)
			return ret;
	}
	else  if (flags & SCSI_TARGET)
	{
		ccb->opcode = AHA_TARGET_CCB;

		/* These must be set up for target mode:
		 */
		if (flags & SCSI_DATA_IN)
			ccb->data_in = 1;
		if (flags & SCSI_DATA_OUT)
			ccb->data_out = 1;
	}
	else
	{
		ccb->opcode	=	(xs->datalen?	 /* can't use S/G if zero length */
					AHA_INIT_SCAT_GATH_CCB
					:AHA_INITIATOR_CCB);
	}

	switch(ccb->opcode)
	{
		case AHA_TARGET_CCB:
		if (xs->data)
			scsi_uto3b(KVTOPHYS((int)xs->data), ccb->data_addr);
		else
			scsi_uto3b(0, ccb->data_addr);

		/* For non scatter-gather I/O (and Target mode doesn't do
		 * scatter-gather) we need to truncate the transfer
		 * at the first non consecutive physical address.
		 */
		scsi_uto3b(physcontig((int)xs->data, xs->datalen), ccb->data_length);
		break;

		/* This should be folded in with TARGET_CCB once
		 * physcontig is debugged.
		 */
		case AHA_INITIATOR_CCB:
		case AHA_INIT_RESID_CCB:

		if (xs->data)
			scsi_uto3b(KVTOPHYS((int)xs->data), ccb->data_addr);
		else
			scsi_uto3b(0, ccb->data_addr);

		scsi_uto3b(xs->datalen, ccb->data_length);
		break;

		case AHA_RESET_CCB:
		scsi_uto3b(0, ccb->data_addr);
		scsi_uto3b(0, ccb->data_length);
		break;

		case AHA_INIT_SCAT_GATH_CCB:
		case AHA_INIT_SG_RESID_CCB:
		scsi_uto3b(KVTOPHYS(ccb->scat_gath), ccb->data_addr );
		sg		=	ccb->scat_gath ;
		seg 		=	0;
#ifdef	TFS_ONLY
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			while ((datalen) && (seg < AHA_NSEG)) {
				scsi_uto3b(iovp->iov_base, sg->seg_addr);
				scsi_uto3b(iovp->iov_len, sg->seg_len);
				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("UIO(0x%x@0x%x)"
					,iovp->iov_len
					,iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif /*TFS_ONLY */
		{
			/*
			 * Set up the scatter gather block
			 */

			SC_DEBUG(xs->sc_link, SDEV_DB4,
			    ("%ld @%p:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while ((datalen) && (seg < AHA_NSEG)) {
				bytes_this_seg = 0;

				/* put in the base address */
				scsi_uto3b(thisphys, sg->seg_addr);

				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while ((datalen) && (thisphys == nextphys)) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* check it fits on the ISA bus */
					if (thisphys > 0xFFFFFF)
					{
						printf("aha%d: DMA beyond"
							" end Of ISA: 0x%x\n",
							aha->unit, thisphys);
						xs->error = XS_DRIVER_STUFFUP;
						aha_free_ccb(aha, ccb, flags);
						return (HAD_ERROR);
					}
					/** how far to the end of the page ***/
					nextphys = (thisphys & (~(PAGESIZ - 1)))
					    + PAGESIZ;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page
					    ,datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/**** get more ready for the next page ****/
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
				scsi_uto3b(bytes_this_seg, sg->seg_len);
				sg++;
				seg++;
			}
		}
		scsi_uto3b(seg * sizeof(struct aha_scat_gath), ccb->data_length);
		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));

		if (datalen) {	/* there's still data, must have run out of segs! */
			printf("aha%d: aha_scsi_cmd, more than %d DMA segs\n",
			    aha->unit, AHA_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			aha_free_ccb(aha, ccb, flags);
			return (HAD_ERROR);
		}
		break;

		default:
		printf("aha_scsi_cmd%d: Illegal CCB opcode.\n", aha->unit);
		xs->error = XS_DRIVER_STUFFUP;
		aha_free_ccb(aha,ccb,flags);
		return HAD_ERROR;
	}

	scsi_uto3b(0, ccb->link_addr);
	/*
	 * Put the scsi command in the ccb and start it
	 */
	if (!(flags & SCSI_ESCAPE))
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmd_length);
	if (!(flags & SCSI_NOMASK)) {
		s = splbio();	/* stop instant timeouts */
		xs->timeout_ch = timeout(aha_timeout, (caddr_t)ccb,
					 (xs->timeout * hz) / 1000);
		aha_startmbx(ccb->mbx);
		/*
		 * Usually return SUCCESSFULLY QUEUED
		 */
		splx(s);
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("sent\n"));
		return (SUCCESSFULLY_QUEUED);
	}
	aha_startmbx(ccb->mbx);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd sent, waiting\n"));

	/*
	 * If we can't use interrupts, poll on completion
	 */
	return (aha_poll(aha, xs, ccb));	/* only during boot */
}

/*
 * Poll a particular unit, looking for a particular xs
 */
static int
aha_poll(aha, xs, ccb)
	struct aha_data *aha;
	struct scsi_xfer *xs;
	struct aha_ccb *ccb;
{
	int     count = xs->timeout;
	u_char  stat;

	/*timeouts are in msec, so we loop in 1000uSec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		stat = inb(AHA_INTR_PORT);
		if (stat & AHA_ANY_INTR) {
			ahaintr(aha->unit);
		}
		if (xs->flags & ITSDONE) {
			break;
		}
		DELAY(1000);	/* only happens in boot so ok */
		count--;
	}
	if (count == 0) {
		/*
		 * We timed out, so call the timeout handler
		 * manually, accout  for the fact that the
		 * clock is not running yet by taking out the
		 * clock queue entry it makes
		 */
		aha_timeout((caddr_t)ccb);

		/*
		 * because we are polling,
		 * take out the timeout entry aha_timeout made
		 */
		untimeout(aha_timeout, (caddr_t)ccb, ccb->xfer->timeout_ch);
		count = 2000;
		while (count) {
			/*
			 * Once again, wait for the int bit
			 */
			stat = inb(AHA_INTR_PORT);
			if (stat & AHA_ANY_INTR) {
				ahaintr(aha->unit);
			}
			if (xs->flags & ITSDONE) {
				break;
			}
			DELAY(1000);	/* only happens in boot so ok */
			count--;
		}
		if (count == 0) {
			/*
			 * We timed out again.. this is bad
			 * Notice that this time there is no
			 * clock queue entry to remove
			 */
			aha_timeout((caddr_t)ccb);
		}
	}
	if (xs->error)
		return (HAD_ERROR);
	return (COMPLETE);

}

#ifdef	TUNE_1542
/*
 * Try all the speeds from slowest to fastest.. if it finds a
 * speed that fails, back off one notch from the last working
 * speed (unless there is no other notch).
 * Returns the nSEC value of the time used
 * or 0 if it could get a working speed (or the NEXT speed
 * failed)
 */
static	struct bus_speed
{
	char	arg;
	int	nsecs;
}aha_bus_speeds[] =
{
	{0x88,100},
	{0x99,150},
	{0xaa,200},
	{0xbb,250},
	{0xcc,300},
	{0xdd,350},
	{0xee,400},
	{0xff,450}
};

static int
aha_set_bus_speed(aha)
	struct	aha_data *aha;
{
	int	speed;
	int	lastworking;
	int	retval,retval2;

	lastworking = -1;
	speed = 7;
	while (1) {
		retval = aha_bus_speed_check(aha,speed);
		if(retval != 0) {
			lastworking = speed;
		}
		if((retval == 0) || (speed == 0)) {
			if(lastworking == -1) {
				printf("No working bus speed for aha154X\n");
				return 0;
			}
			printf("%d nSEC ok, using "
				,aha_bus_speeds[lastworking].nsecs);
			if(lastworking == 7) { /* is slowest already */
				printf("marginal ");
			} else {
				lastworking++;
			}
			retval2 = aha_bus_speed_check(aha,lastworking);
			if(retval2 == 0) {
				printf("test retry failed.. aborting.\n");
				return 0;
			}
			printf("%d nSEC\n",retval2);
			return retval2 ;

		}
		speed--;
	}
}

/*
 * Set the DMA speed to the Nth speed and try an xfer. If it
 * fails return 0, if it succeeds return the nSec value selected
 * If there is no such speed return HAD_ERROR.
 */
static char aha_test_string[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz!@";

u_char  aha_scratch_buf[256];

static int
aha_bus_speed_check(aha, speed)
	struct	aha_data *aha;
	int     speed;
{
	int     numspeeds = sizeof(aha_bus_speeds) / sizeof(struct bus_speed);
	int	loopcount;
	u_char  ad[3];

	/*
	 * Check we have such an entry
	 */
	if (speed >= numspeeds)
		return (HAD_ERROR);	/* illegal speed */

	/*
	 * Set the dma-speed
	 */
	aha_cmd(aha, 1, 0, 0, 0, AHA_SPEED_SET, aha_bus_speeds[speed].arg);

	/*
	 * put the test data into the buffer and calculate
	 * its address. Read it onto the board
	 */
	scsi_uto3b(KVTOPHYS(aha_scratch_buf), ad);
	for(loopcount = 2000;loopcount;loopcount--)
	{
		strcpy(aha_scratch_buf, aha_test_string);

		aha_cmd(aha, 3, 0, 0, 0, AHA_WRITE_FIFO, ad[0], ad[1], ad[2]);

		/*
	 	* clear the buffer then copy the contents back from the
	 	* board.
	 	*/
		bzero(aha_scratch_buf, 54);	/* 54 bytes transfered by test */

		aha_cmd(aha, 3, 0, 0, 0, AHA_READ_FIFO, ad[0], ad[1], ad[2]);

		/*
	 	* Compare the original data and the final data and
	 	* return the correct value depending upon the result
	 	*/
		if (strcmp(aha_test_string, aha_scratch_buf))
			return 0; /* failed test */
	}
			/* copy succeded assume speed ok */

	return (aha_bus_speeds[speed].nsecs);

}
#endif	/*TUNE_1542*/

static void
aha_timeout(void *arg1)
{
	struct aha_ccb * ccb = (struct aha_ccb *)arg1;
	int     s = splbio();
	struct aha_data *aha;

	aha = (struct aha_data *)ccb->xfer->sc_link->adapter_softc;
	sc_print_addr(ccb->xfer->sc_link);
	printf("timed out ");

	/*
	 * If The ccb's mbx is not free, then
	 * the board has gone south
	 */
	if (ccb->mbx->cmd != AHA_MBO_FREE) {
		printf("\nadapter not taking commands.. frozen?!\n");
#ifdef DIAGNOSTIC
		panic("aha1542 frozen");
#endif
	}
	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags == CCB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		ccb->xfer->retries = 0;		/* I MEAN IT ! */
		ccb->host_stat = AHA_ABORTED;
		aha_done(aha, ccb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		aha_abortmbx(ccb->mbx);
		/* 4 secs for the abort */
		ccb->xfer->timeout_ch = timeout(aha_timeout,
						(caddr_t)ccb, 4 * hz);
		ccb->flags = CCB_ABORTED;
	} splx(s);
}
